#include "globals.h"

void updateDisplay()
{
  u8g2.clearBuffer();
  drawHeader();
  u8g2.setFont(u8g2_font_5x8_tf);
  String ip_str = "IP: " + WiFi.localIP().toString();
  u8g2.drawStr(0, 23, ip_str.c_str());
  String peer_mac_str = "Peer: ";
  bool peer_set = (memcmp(g_peer_mac, "\x00\x00\x00\x00\x00\x00", 6) != 0);
  if (peer_set)
  {
    char short_mac[10];
    snprintf(short_mac, sizeof(short_mac), "%02X:%02X:%02X", g_peer_mac[3], g_peer_mac[4], g_peer_mac[5]);
    peer_mac_str += String(short_mac);
  }
  else
    peer_mac_str += "None";
  u8g2.drawStr(0, 33, peer_mac_str.c_str());
  String status_line = String("Stream:") + (g_stream_enabled ? "ON " : "OFF ");
  status_line += g_sd_available ? (g_logging_enabled ? "Log:ON" : "Log:RDY") : "SD:ERR";
  u8g2.drawStr(0, 43, status_line.c_str());
  if (peer_set)
  {
    String stats = "TX:" + String(g_send_ok) + "/" + String(g_send_fail);
    u8g2.drawStr(0, 53, stats.c_str());
  }
  if (g_logging_enabled)
  {
    String log_stats = "Rec:" + String(g_log_records_written);
    u8g2.drawStr(0, 63, log_stats.c_str());
  }
  else if (g_sd_available)
    u8g2.drawStr(0, 63, "SD Ready");
  u8g2.sendBuffer();
}

void drawHeader()
{
  u8g2.setFont(u8g2_font_t0_11b_tr);
  u8g2.drawStr(4, 12, "ESP32 IMU Instrument");
  u8g2.drawLine(4, 15, 123, 15);
}

void initDisplay()
{
  u8g2.begin();
  u8g2.clearBuffer();
  drawHeader();
  u8g2.setFont(u8g2_font_helvB08_tr);
  u8g2.drawStr(31, 42, "STARTING....");
  u8g2.sendBuffer();
  delay(1200);
}

void calibratingPage(bool in_progress, bool success)
{
  u8g2.clearBuffer();
  drawHeader();
  if (in_progress)
  {
    u8g2.setFont(u8g2_font_6x13_tr);
    u8g2.drawStr(19, 35, "IMU CALIBRATING");
    u8g2.drawStr(31, 48, "DO NOT MOVE");
  }
  else if (success)
  {
    u8g2.setFont(u8g2_font_6x13_tr);
    u8g2.drawStr(19, 35, "IMU CALIBRATING");
    u8g2.drawStr(46, 48, "SUCCESS");
    delay(1000);
  }
  else
  {
    u8g2.setFont(u8g2_font_6x13_tr);
    u8g2.drawStr(19, 35, "IMU CALIBRATING");
    u8g2.drawStr(46, 48, "FAILED");
  }
  u8g2.sendBuffer();
}

void connectingWifiPage(bool in_progress, bool success)
{
  u8g2.clearBuffer();
  drawHeader();
  if (in_progress)
  {
    u8g2.setFont(u8g2_font_6x13_tr);
    u8g2.drawStr(34, 35, "CONNECTING");
    u8g2.drawStr(52, 49, "WIFI");
  }
  else if (success)
  {
    u8g2.setFont(u8g2_font_6x13_tr);
    u8g2.drawStr(13, 35, "SUCCESS CONNECTING");
    u8g2.drawStr(52, 49, "WIFI");
    delay(2000);
  }
  else
  {
    u8g2.setFont(u8g2_font_6x13_tr);
    u8g2.drawStr(13, 35, "FAILED CONNECTING");
    u8g2.drawStr(52, 49, "WIFI");
    delay(2000);
  }
  u8g2.sendBuffer();
}

void sdCardInitPage(bool in_progress, bool success)
{
  u8g2.clearBuffer();
  drawHeader();
  if (in_progress)
  {
    u8g2.setFont(u8g2_font_6x13_tr);
    u8g2.drawStr(31, 35, "INITIALIZING");
    u8g2.drawStr(43, 49, "MICROSD"); 
  }
  else if (success)
  {
    u8g2.setFont(u8g2_font_6x13_tr);
    u8g2.drawStr(43, 35, "SUCCESS");
    u8g2.drawStr(43, 49, "MICROSD");
    delay(2000);
  }
  else
  {
    u8g2.setFont(u8g2_font_6x13_tr);
    u8g2.drawStr(46, 35, "FAILED");
    u8g2.drawStr(43, 49, "MICROSD");
    delay(2000);
  }
  u8g2.sendBuffer();
}

void triggerDisplayUpdate() { g_display_update_needed = true; }

void displayTask(void *pv)
{
  for (;;)
  {
    uint32_t current_time = millis();
    if (g_display_update_needed || (current_time - g_last_display_update > DISPLAY_UPDATE_INTERVAL_MS))
    {
      if (xSemaphoreTake(g_i2cMutex, pdMS_TO_TICKS(10)) == pdTRUE)
      {
        updateDisplay();
        xSemaphoreGive(g_i2cMutex);
      }
      g_display_update_needed = false;
      g_last_display_update = current_time;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
