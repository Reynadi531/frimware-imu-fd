#include <Arduino.h>
#include <MPU6500_WE.h>
#include "globals.h"

MPU6500_WE MPU = MPU6500_WE(MPU_ADDR);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ OLED_SCL, /* data=*/ OLED_SDA);

SdFat32 sd;
SdFile currentLogFile;
String currentLogFileName = "";
volatile bool g_sd_available = false;
volatile bool g_logging_enabled = false;
volatile uint32_t g_log_records_written = 0;
volatile uint32_t g_log_write_errors = 0;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60 * 60 * 1000);

WebServer server(80);
WebServer discoveryServer(5681);

volatile bool g_stream_enabled = false;
volatile uint32_t imu_delay_ms = 50;
volatile bool g_calibrating = false;

uint8_t g_peer_mac[6] = {0,0,0,0,0,0};
uint8_t g_peer_channel = 0;
volatile uint32_t g_send_ok = 0;
volatile uint32_t g_send_fail = 0;
volatile esp_now_send_status_t g_last_send_status = ESP_NOW_SEND_FAIL;

uint32_t pkt_seq = 0;

#if defined(ESP32)
portMUX_TYPE g_timeMux = portMUX_INITIALIZER_UNLOCKED;
#endif

volatile int64_t  g_ntp_offset_ms_ewma = 0;
volatile uint32_t g_loop_dt_avg_ms = 50;
volatile uint32_t g_loop_jitter_avg_ms = 0;
const float NTP_EWMA_ALPHA  = 0.1f;
const float LOOP_EWMA_ALPHA = 0.2f;

volatile bool g_display_update_needed = true;
volatile uint32_t g_last_display_update = 0;
const uint32_t DISPLAY_UPDATE_INTERVAL_MS = 1000;

SemaphoreHandle_t g_i2cMutex = nullptr;
SemaphoreHandle_t g_sdMutex  = nullptr;
SemaphoreHandle_t g_netMutex = nullptr;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);

  g_i2cMutex = xSemaphoreCreateMutex();
  g_sdMutex  = xSemaphoreCreateMutex();
  g_netMutex = xSemaphoreCreateMutex();
  configASSERT(g_i2cMutex);
  configASSERT(g_sdMutex);
  configASSERT(g_netMutex);

  initDisplay();

  if (!MPU.init()) Serial.println("MPU6500 no response");
  else Serial.println("MPU6500 connected");

  u8g2.clearBuffer(); u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(15, 25, "Calibrating IMU"); u8g2.drawStr(25, 40, "Keep Flat!"); u8g2.sendBuffer();
  delay(800);
  MPU.autoOffsets();
  MPU.enableGyrDLPF();
  MPU.setGyrDLPF(MPU6500_DLPF_6);
  MPU.setSampleRateDivider(5);
  MPU.setGyrRange(MPU6500_GYRO_RANGE_250);
  MPU.setAccRange(MPU6500_ACC_RANGE_2G);
  MPU.enableAccDLPF(true);
  MPU.setAccDLPF(MPU6500_DLPF_6);
  delay(100);

  connectWiFi();

  u8g2.clearBuffer(); u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(20, 25, "Init SD Card"); u8g2.sendBuffer();
  if (!initSDCard()) { Serial.println("SD NG"); u8g2.clearBuffer(); u8g2.drawStr(20,25,"SD Card Failed"); u8g2.sendBuffer(); delay(800); }
  else Serial.println("SD OK");

  EEPROM.begin(EEPROM_SIZE);
  loadPeerFromEEPROM();

  if (!initEspNow())
    Serial.println("ESP-NOW ready; set peer via /peer/set?mac=AA:BB:CC:DD:EE:FF");
  else
    Serial.printf("ESP-NOW peer %s ch=%u\n", macToString(g_peer_mac).c_str(),
                  (g_peer_channel==0)?WiFi.channel():g_peer_channel);

  registerHttpRoutes();
  server.begin();
  discoveryServer.begin();
  Serial.println("HTTP ready");

  timeClient.begin();
  if (timeClient.update()) setTimebase(timeClient.getEpochTime(), millis());
  else setTimebase(0, 0);

  xTaskCreatePinnedToCore(ntpTask,     "ntpTask",     4096, nullptr, 2, nullptr, CORE_TIME);
  xTaskCreatePinnedToCore(imuTask,     "imuTask",     8192, nullptr, 1, nullptr, CORE_TIME);
  xTaskCreatePinnedToCore(httpTask,    "httpTask",    4096, nullptr, 1, nullptr, (CORE_TIME == 0) ? 1 : 0);
  xTaskCreatePinnedToCore(displayTask, "displayTask", 4096, nullptr, 1, nullptr, (CORE_TIME == 0) ? 1 : 0);

  triggerDisplayUpdate();
  Serial.println("All tasks started");
}

void loop() {
}