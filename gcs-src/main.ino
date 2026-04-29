#include <esp_now.h>
#include <WiFi.h>
#include <HardwareSerial.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SERIAL_BAUD 115200

#define WIFI_SSID "WIFI"
#define WIFI_PASSWORD "WIFI"

#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_ADDR 0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

#define DISPLAY_UPDATE_MS 100

uint8_t g_peer_mac[] = {0x14, 0x08, 0x08, 0xa5, 0x08, 0x1c};
uint8_t g_espnow_channel = 1;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

typedef struct {
  char timestamp[32];
  uint32_t tick_ms;
  uint32_t seq;
  float ax;
  float ay;
  float az;
  float gx;
  float gy;
  float gz;
  float temp;
} imu_data_t;

imu_data_t g_latest;
bool g_data_valid = false;
volatile uint32_t g_pkt_rx_ok = 0;
volatile uint32_t g_pkt_rx_fail = 0;
int8_t g_last_rssi = 0;

void draw_header() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(4, 4);
  display.print("ESP32 GCS");
  display.drawLine(4, 13, 123, 13, SSD1306_WHITE);
}

void update_display() {
  display.clearDisplay();
  draw_header();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  char short_mac[10];
  snprintf(short_mac, sizeof(short_mac), "%02X:%02X:%02X",
           g_peer_mac[3], g_peer_mac[4], g_peer_mac[5]);
  display.setCursor(0, 16);
  display.print("Peer:");
  display.print(short_mac);

  display.setCursor(70, 16);
  display.print("CH:");
  display.print(g_espnow_channel);

  display.setCursor(0, 26);
  display.print("RX:");
  display.print(g_pkt_rx_ok);
  display.print("/");
  display.print(g_pkt_rx_fail);

  display.setCursor(70, 26);
  display.print("RSSI:");
  display.print(g_last_rssi);
  display.print(" dBm");

  display.drawLine(0, 35, 127, 35, SSD1306_WHITE);

  if (g_data_valid) {
    display.setCursor(0, 38);
    display.print("AX:");
    display.print(g_latest.ax, 3);

    display.setCursor(70, 38);
    display.print("AY:");
    display.print(g_latest.ay, 3);

    display.setCursor(0, 47);
    display.print("AZ:");
    display.print(g_latest.az, 3);

    display.setCursor(70, 47);
    display.print("TMP:");
    display.print(g_latest.temp, 1);

    display.setCursor(0, 56);
    display.print("GX:");
    display.print(g_latest.gx, 3);

    display.setCursor(70, 56);
    display.print("GY:");
    display.print(g_latest.gy, 3);
  } else {
    display.setCursor(0, 47);
    display.print("Waiting for data...");
  }

  display.display();
}

void on_data_recv(const esp_now_recv_info* info, const uint8_t* payload, int len) {
  if (info == nullptr || payload == nullptr || len < 10) {
    g_pkt_rx_fail++;
    return;
  }

  const uint8_t* mac = info->src_addr;
  g_last_rssi = WiFi.RSSI();

  if (len == sizeof(imu_data_t)) {
    memcpy(&g_latest, payload, sizeof(imu_data_t));
    g_data_valid = true;
    g_pkt_rx_ok++;
  } else {
    g_latest.timestamp[0] = '\0';
    g_latest.tick_ms = 0;
    g_latest.seq = 0;
    g_latest.ax = 0;
    g_latest.ay = 0;
    g_latest.az = 0;
    g_latest.gx = 0;
    g_latest.gy = 0;
    g_latest.gz = 0;
    g_latest.temp = 0;

    char* ptr = (char*)payload;
    char* end;
    int field = 0;
    char* field_start = ptr;

    for (int i = 0; i < len && field < 10; i++) {
      if (ptr[i] == ',' || i == len - 1) {
        char save = ptr[i];
        ptr[i] = '\0';
        char* tok = field_start;

        if (field == 0) strncpy(g_latest.timestamp, tok, sizeof(g_latest.timestamp) - 1);
        else if (field == 1) g_latest.tick_ms = atoi(tok);
        else if (field == 2) g_latest.seq = atoi(tok);
        else if (field == 3) g_latest.ax = atof(tok);
        else if (field == 4) g_latest.ay = atof(tok);
        else if (field == 5) g_latest.az = atof(tok);
        else if (field == 6) g_latest.gx = atof(tok);
        else if (field == 7) g_latest.gy = atof(tok);
        else if (field == 8) g_latest.gz = atof(tok);
        else if (field == 9) g_latest.temp = atof(tok);

        field++;
        if (i < len - 1) field_start = &ptr[i + 1];
        ptr[i] = save;
      }
    }

    if (field >= 3) {
      g_data_valid = true;
      g_pkt_rx_ok++;
    } else {
      g_pkt_rx_fail++;
    }
  }
}

void print_json() {
  if (!g_data_valid) return;

  char json_buf[512];
  int len = snprintf(json_buf, sizeof(json_buf),
    "{\"timestamp\":\"%s\",\"tick_ms\":%u,\"seq\":%u,"
    "\"accel\":{\"x\":%.6f,\"y\":%.6f,\"z\":%.6f},"
    "\"gyro\":{\"x\":%.6f,\"y\":%.6f,\"z\":%.6f},"
    "\"temp\":%.2f}",
    g_latest.timestamp,
    g_latest.tick_ms,
    g_latest.seq,
    g_latest.ax, g_latest.ay, g_latest.az,
    g_latest.gx, g_latest.gy, g_latest.gz,
    g_latest.temp
  );

  if (len > 0 && len < sizeof(json_buf)) {
    Serial.println(json_buf);
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(100);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("{\"error\":\"wifi_connect_failed\"}");
  }

  g_espnow_channel = (uint8_t)WiFi.channel();

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("{\"error\":\"oled_init_failed\"}");
  } else {
    display.clearDisplay();
    draw_header();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(31, 42);
    display.print("STARTING....");
    display.display();
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("{\"error\":\"esp_now_init_failed\"}");
    return;
  }

  esp_now_peer_info_t peer;
  memset(&peer, 0, sizeof(peer));
  memcpy(peer.peer_addr, g_peer_mac, 6);
  peer.channel = g_espnow_channel;
  peer.encrypt = false;

  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("{\"error\":\"peer_add_failed\"}");
    return;
  }

  esp_now_register_recv_cb(on_data_recv);

  char ready_json[256];
  snprintf(ready_json, sizeof(ready_json),
    "{\"status\":\"ground_station_ready\",\"channel\":%d,\"peer_mac\":\"14:08:08:a5:08:1c\"}",
    g_espnow_channel);
  Serial.println(ready_json);

  update_display();
}

void loop() {
  static unsigned long last_display = 0;
  static unsigned long last_json = 0;
  unsigned long now = millis();

  if (now - last_display >= DISPLAY_UPDATE_MS) {
    update_display();
    last_display = now;
  }

  if (g_data_valid && now - last_json >= 50) {
    print_json();
    last_json = now;
    g_data_valid = false;
  }
}