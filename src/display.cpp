#include "globals.h"

void updateDisplay()
{
  display.clearDisplay();
  drawHeader();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  String ip_str = "IP: " + WiFi.localIP().toString();
  display.setCursor(0, 18);
  display.print(ip_str);
  
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
  display.setCursor(0, 27);
  display.print(peer_mac_str);
  
  String status_line = String("Stream:") + (g_stream_enabled ? "ON " : "OFF ");
  status_line += g_sd_available ? (g_logging_enabled ? "Log:ON" : "Log:RDY") : "SD:ERR";
  display.setCursor(0, 36);
  display.print(status_line);
  
  if (peer_set)
  {
    String stats = "TX:" + String(g_send_ok) + "/" + String(g_send_fail);
    display.setCursor(0, 45);
    display.print(stats);
  }
  if (g_logging_enabled)
  {
    String log_stats = "R:" + String(g_log_records_written);
    display.setCursor(0, 54);
    display.print(log_stats);
  }
  else if (g_sd_available)
  {
    display.setCursor(0, 54);
    display.print("SD Ready");
  }
  display.display();
}

void drawHeader()
{
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(4, 4);
  display.print("ESP32 IMU Instrument");
  display.drawLine(4, 15, 123, 15, SSD1306_WHITE);
}

void initDisplay()
{
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  drawHeader();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(31, 42);
  display.print("STARTING....");
  display.display();
  delay(1200);
}

void calibratingPage(bool in_progress, bool success)
{
  display.clearDisplay();
  drawHeader();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  if (in_progress)
  {
    display.setCursor(19, 35);
    display.print("IMU CALIBRATING");
    display.setCursor(31, 48);
    display.print("DO NOT MOVE");
  }
  else if (success)
  {
    display.setCursor(19, 35);
    display.print("IMU CALIBRATING");
    display.setCursor(46, 48);
    display.print("SUCCESS");
    display.display();
    delay(1000);
  }
  else
  {
    display.setCursor(19, 35);
    display.print("IMU CALIBRATING");
    display.setCursor(46, 48);
    display.print("FAILED");
  }
  display.display();
}

void connectingWifiPage(bool in_progress, bool success)
{
  display.clearDisplay();
  drawHeader();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  if (in_progress)
  {
    display.setCursor(34, 35);
    display.print("CONNECTING");
    display.setCursor(52, 49);
    display.print("WIFI");
  }
  else if (success)
  {
    display.setCursor(13, 35);
    display.print("SUCCESS CONNECTING");
    display.setCursor(52, 49);
    display.print("WIFI");
    display.display();
    delay(2000);
  }
  else
  {
    display.setCursor(13, 35);
    display.print("FAILED CONNECTING");
    display.setCursor(52, 49);
    display.print("WIFI");
    display.display();
    delay(2000);
  }
  display.display();
}

void sdCardInitPage(bool in_progress, bool success)
{
  display.clearDisplay();
  drawHeader();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  if (in_progress)
  {
    display.setCursor(31, 35);
    display.print("INITIALIZING");
    display.setCursor(43, 49);
    display.print("MICROSD"); 
  }
  else if (success)
  {
    display.setCursor(43, 35);
    display.print("SUCCESS");
    display.setCursor(43, 49);
    display.print("MICROSD");
    display.display();
    delay(2000);
  }
  else
  {
    display.setCursor(46, 35);
    display.print("FAILED");
    display.setCursor(43, 49);
    display.print("MICROSD");
    display.display();
    delay(2000);
  }
  display.display();
}

void triggerDisplayUpdate() { g_display_update_needed = true; }

void displayTask(void *pv)
{
  for (;;)
  {
    if (g_display_enabled)
    {
      uint32_t current_time = millis();
      if (g_display_update_needed || (current_time - g_last_display_update > DISPLAY_UPDATE_INTERVAL_MS))
      {
        updateDisplay();
        g_display_update_needed = false;
        g_last_display_update = current_time;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
