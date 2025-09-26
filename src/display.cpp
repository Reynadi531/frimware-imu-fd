#include "globals.h"

void updateDisplay() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "ESP32 IMU Sensor");
  u8g2.drawHLine(0, 12, 128);
  u8g2.setFont(u8g2_font_5x8_tf);
  String ip_str = "IP: " + WiFi.localIP().toString();
  u8g2.drawStr(0, 23, ip_str.c_str());
  String peer_mac_str = "Peer: ";
  bool peer_set = (memcmp(g_peer_mac, "\x00\x00\x00\x00\x00\x00", 6) != 0);
  if (peer_set) {
    char short_mac[10];
    snprintf(short_mac, sizeof(short_mac), "%02X:%02X:%02X", g_peer_mac[3], g_peer_mac[4], g_peer_mac[5]);
    peer_mac_str += String(short_mac);
  } else peer_mac_str += "None";
  u8g2.drawStr(0, 33, peer_mac_str.c_str());
  String status_line = String("Stream:") + (g_stream_enabled ? "ON " : "OFF ");
  status_line += g_sd_available ? (g_logging_enabled ? "Log:ON" : "Log:RDY") : "SD:ERR";
  u8g2.drawStr(0, 43, status_line.c_str());
  if (peer_set) {
    String stats = "TX:" + String(g_send_ok) + "/" + String(g_send_fail);
    u8g2.drawStr(0, 53, stats.c_str());
  }
  if (g_logging_enabled) {
    String log_stats = "Rec:" + String(g_log_records_written);
    u8g2.drawStr(0, 63, log_stats.c_str());
  } else if (g_sd_available) u8g2.drawStr(0, 63, "SD Ready");
  u8g2.sendBuffer();
}

void initDisplay() {
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(20, 20, "ESP32 IMU");
  u8g2.drawStr(25, 35, "Starting...");
  u8g2.sendBuffer();
  delay(1200);
}

void triggerDisplayUpdate() { g_display_update_needed = true; }

void displayTask(void *pv) {
  for (;;) {
    uint32_t current_time = millis();
    if (g_display_update_needed || (current_time - g_last_display_update > DISPLAY_UPDATE_INTERVAL_MS)) {
      if (xSemaphoreTake(g_i2cMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        updateDisplay();
        xSemaphoreGive(g_i2cMutex);
      }
      g_display_update_needed = false;
      g_last_display_update = current_time;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
