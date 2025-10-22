#include "globals.h"

BluetoothSerial SerialBT;
bool g_bt_initialized = false;

void initBluetooth() {
  if (!g_bt_initialized) {
    if (!SerialBT.begin("ESP32-IMU")) {
      Serial.println("BT init failed!");
      return;
    }
    g_bt_initialized = true;
    Serial.println("BT: ESP32-IMU");
    SerialBT.println(F("SET_WIFI:<SSID>:<PASS>"));
    SerialBT.println(F("GET_WIFI | RESTART"));
  }
}

void stopBluetooth() {
  if (g_bt_initialized) {
    SerialBT.end();
    g_bt_initialized = false;
    Serial.println("BT: Stopped");
  }
}

void handleBluetoothCommands() {
  if (!g_bt_initialized || !SerialBT.available()) return;

  String cmd = SerialBT.readStringUntil('\n');
  cmd.trim();
  
  Serial.printf("BT cmd: %s\n", cmd.c_str());

  if (cmd.startsWith("SET_WIFI:")) {
    int c1 = cmd.indexOf(':', 0);
    int c2 = cmd.indexOf(':', c1 + 1);
    
    if (c1 != -1 && c2 != -1) {
      String ssid = cmd.substring(c1 + 1, c2);
      String pass = cmd.substring(c2 + 1);
      
      Serial.printf("SSID len: %d, Pass len: %d\n", ssid.length(), pass.length());
      
      if (ssid.length() > 0 && ssid.length() < 64 && pass.length() < 64) {
        if (saveWiFiToEEPROM(ssid.c_str(), pass.c_str())) {
          SerialBT.print(F("OK: ")); SerialBT.println(ssid);
          Serial.printf("Saved: %s\n", ssid.c_str());
        } else {
          SerialBT.println(F("ERR: Save failed"));
        }
      } else {
        SerialBT.println(F("ERR: Length"));
      }
    } else {
      SerialBT.println(F("ERR: Format"));
    }
  } 
  else if (cmd == "GET_WIFI") {
    char ssid[64], pass[64];
    loadWiFiFromEEPROM(ssid, pass);
    SerialBT.print(F("SSID: ")); SerialBT.println(ssid);
    SerialBT.print(F("Pass: ")); SerialBT.println(pass);
    if (WiFi.status() == WL_CONNECTED) {
      SerialBT.print(F("IP: ")); SerialBT.println(WiFi.localIP());
    } else {
      SerialBT.println(F("Disconnected"));
    }
  }
  else if (cmd == "RESTART") {
    SerialBT.println(F("Restarting..."));
    delay(500);
    ESP.restart();
  }
}
