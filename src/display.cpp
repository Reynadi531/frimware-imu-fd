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

void calibrationInstructionPage(uint8_t pos, uint16_t sample_idx)
{
  display.clearDisplay();
  drawHeader();
  display.setTextColor(SSD1306_WHITE);
  
  const char* arrows[6] = { "^", "v", "<", ">", ".", "o" };
  const char* desc[6] = { "X+ UP", "X+ DN", "Y+ UP", "Y+ DN", "FLAT", "FLIP" };
  
  if (pos < 6) {
    display.setTextSize(2);
    display.setCursor(50, 16);
    display.print(arrows[pos]);
    
    display.setTextSize(1);
    display.setCursor(30, 36);
    display.print("Step ");
    display.print(pos + 1);
    display.print(":");
    display.setCursor(0, 46);
    display.print(desc[pos]);
  }
  
  display.setTextSize(1);
  uint8_t bar_len = 100;
  uint8_t filled = (sample_idx * bar_len) / 128;
  display.setCursor(14, 56);
  display.print("[");
  for (uint8_t i = 0; i < bar_len; i++) {
    if (i < filled) display.print("=");
    else if (i == filled) display.print(">");
    else display.print(" ");
  }
  display.print("]");
  
  display.display();
}

void calibrationFittingPage()
{
  display.clearDisplay();
  drawHeader();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(30, 26);
  display.print("FITTING");
  display.setTextSize(1);
  display.setCursor(20, 48);
  display.print("Computing...");
  display.display();
}

void calibrationSuccessPage()
{
  display.clearDisplay();
  drawHeader();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(24, 24);
  display.print("CALIB");
  display.setCursor(24, 44);
  display.print("DONE!");
  display.setTextSize(1);
  display.setCursor(16, 56);
  display.print("Saved to EEPROM");
  display.display();
  delay(3000);
}

void calibrationFailedPage()
{
  display.clearDisplay();
  drawHeader();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(24, 24);
  display.print("FAILED");
  display.setTextSize(1);
  display.setCursor(16, 48);
  display.print("Retry:/imu/recalib");
  display.display();
}

void triggerDisplayUpdate() { g_display_update_needed = true; }

void displayTask(void *pv)
{
  for (;;)
  {
    if (g_display_enabled)
    {
      CalibState calib_state = g_calib_state;
      if (calib_state == CALIB_COLLECTING) {
        static uint8_t last_pos = 255;
        static uint16_t last_idx = 0xFFFF;
        if (last_pos != g_calib_position || last_idx != g_calib_sample_idx) {
          calibrationInstructionPage(g_calib_position, g_calib_sample_idx);
          last_pos = g_calib_position;
          last_idx = g_calib_sample_idx;
        }
      } else if (calib_state == CALIB_FITTING) {
        calibrationFittingPage();
      } else if (calib_state == CALIB_SUCCESS) {
        calibrationSuccessPage();
        g_calib_state = CALIB_IDLE;
        triggerDisplayUpdate();
      } else if (calib_state == CALIB_FAILED) {
        calibrationFailedPage();
      } else {
        uint32_t current_time = millis();
        if (g_display_update_needed || (current_time - g_last_display_update > DISPLAY_UPDATE_INTERVAL_MS))
        {
          updateDisplay();
          g_display_update_needed = false;
          g_last_display_update = current_time;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
