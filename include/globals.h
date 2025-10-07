#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include <wifi_secret.h>
#include <MPU6500_WE.h>
#include <Wire.h>
#include <NTPClient.h>
#include <time.h>
#include <EEPROM.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <U8g2lib.h>
#include <SdFat.h>
#include <target_secret.h>
#include <ESPmDNS.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define IMU_HOSTNAME "esp32-imu"

#define MPU_ADDR 0x68
#define EEPROM_SIZE 64
#define EEPROM_MAGIC_TARGET 0x54524754u 
#define EEPROM_MAGIC_PEER   0x454E4F57u 

#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_ADDR 0x3C

#define SD_CS_PIN   5
#define SD_MOSI_PIN 23
#define SD_MISO_PIN 19
#define SD_SCK_PIN  18

extern MPU6500_WE MPU;

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

extern SdFat32 sd;
extern SdFile currentLogFile;
extern String currentLogFileName;
extern volatile bool g_sd_available;
extern volatile bool g_logging_enabled;
extern volatile uint32_t g_log_records_written;
extern volatile uint32_t g_log_write_errors;

extern WiFiUDP ntpUDP;
extern NTPClient timeClient;

extern WebServer server;
extern WebServer discoveryServer;

extern volatile bool g_stream_enabled;
extern volatile uint32_t imu_delay_ms;
extern volatile bool g_calibrating;

extern uint8_t g_peer_mac[6];
extern uint8_t g_peer_channel;
extern volatile uint32_t g_send_ok;
extern volatile uint32_t g_send_fail;
extern volatile esp_now_send_status_t g_last_send_status;
extern uint32_t pkt_seq;

static uint16_t GMT_OFFSET_SEC = 7 * 3600; // GMT + 7

extern volatile int64_t  g_ntp_offset_ms_ewma;
extern volatile uint32_t g_loop_dt_avg_ms;
extern volatile uint32_t g_loop_jitter_avg_ms;
extern const float NTP_EWMA_ALPHA;
extern const float LOOP_EWMA_ALPHA;

extern volatile bool g_display_update_needed;
extern volatile uint32_t g_last_display_update;
extern const uint32_t DISPLAY_UPDATE_INTERVAL_MS;

extern SemaphoreHandle_t g_i2cMutex; 
extern SemaphoreHandle_t g_sdMutex;  
extern SemaphoreHandle_t g_netMutex; 

struct PersistPeer {
  uint32_t magic;
  uint8_t  mac[6];
  uint8_t  channel; 
  uint8_t  _pad;
};

struct Timebase {
  time_t   epoch_s;
  uint32_t base_ms;
  bool     synced;
};

void setTimebase(time_t epoch_s, uint32_t base_ms);
Timebase getTimebase();
uint64_t epochMillisNow();
String iso8601_utc_ms(uint64_t epoch_ms);
String iso8601_local_ms(uint64_t epoch_ms);
String macToString(const uint8_t mac[6]);
bool parseMac(const char* s, uint8_t out[6]);

bool initSDCard();
String generateLogFileName();
bool startLogging();
bool stopLogging();
bool logIMUData(const String& jsonData);

void initDisplay();
void updateDisplay();
void triggerDisplayUpdate();
void displayTask(void *pv);
void drawHeader();
void calibratingPage(bool in_progress, bool success);
void connectingWifiPage(bool in_progress, bool success);
void sdCardInitPage(bool in_progress, bool success);

bool addOrUpdatePeer();
void onEspNowSent(const uint8_t* mac_addr, esp_now_send_status_t status);
bool initEspNow();

void loadLegacyTargetFromEEPROM();
bool saveLegacyTargetToEEPROM(const IPAddress& ip, uint16_t port);
void loadPeerFromEEPROM();
bool savePeerToEEPROM(const uint8_t mac[6], uint8_t channel);

void httpTask(void *pv);
void registerHttpRoutes();

void connectWiFi();

/// @brief 
/// @param pv 
void ntpTask(void *pv);

void imuTask(void *pv);

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S3
  #define CORE_TIME 0
#else
  #define CORE_TIME 0
#endif

#if defined(ESP32)
  extern portMUX_TYPE g_timeMux;
  #define LOCK_TIME()   portENTER_CRITICAL(&g_timeMux)
  #define UNLOCK_TIME() portEXIT_CRITICAL(&g_timeMux)
#else
  #define LOCK_TIME()
  #define UNLOCK_TIME()
#endif
