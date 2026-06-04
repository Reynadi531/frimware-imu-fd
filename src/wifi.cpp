#include "globals.h"

BluetoothSerial SerialBT;

static char g_wifi_ssid[33] = {0};
static char g_wifi_pass[65] = {0};

static bool tryConnectWiFi(const char *ssid, const char *pass, uint32_t timeout_ms) {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(IMU_HOSTNAME);
  WiFi.begin(ssid, pass);
  Serial.printf("Connecting to WiFi \"%s\"", ssid);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > timeout_ms) {
      Serial.println("\nWiFi connection timeout");
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      return false;
    }
    delay(500);
    connectingWifiPage(true, false);
    Serial.print(".");
  }
  return true;
}

static void processBtCommand(const String &cmd, char *ssid, char *pass, bool &doConnect, bool &doReset) {
  doConnect = false;
  doReset = false;
  String trimmed = cmd;
  trimmed.trim();
  if (trimmed.startsWith("SSID:")) {
    String val = trimmed.substring(5);
    val.trim();
    strncpy(ssid, val.c_str(), 32);
    ssid[32] = '\0';
    SerialBT.printf("{\"status\":\"ssid_set\",\"ssid\":\"%s\"}\n", ssid);
  } else if (trimmed.startsWith("PASS:")) {
    String val = trimmed.substring(5);
    val.trim();
    strncpy(pass, val.c_str(), 64);
    pass[64] = '\0';
    SerialBT.println("{\"status\":\"pass_set\"}");
  } else if (trimmed == "CONNECT") {
    if (ssid[0] == '\0') {
      SerialBT.println("{\"status\":\"error\",\"msg\":\"SSID not set\"}");
    } else {
      doConnect = true;
    }
  } else if (trimmed == "STATUS") {
    wl_status_t s = WiFi.status();
    SerialBT.printf("{\"status\":\"wifi\",\"state\":%d,\"ssid\":\"%s\"}\n", s, ssid);
  } else if (trimmed == "RESET") {
    doReset = true;
  } else {
    SerialBT.println("{\"status\":\"error\",\"msg\":\"unknown command\"}");
    SerialBT.println("Commands: SSID:<ssid>, PASS:<pass>, CONNECT, STATUS, RESET");
  }
}

static void enterBtConfigMode() {
  SerialBT.begin("esp32-imu-config");
  btConfigPage();
  Serial.println("Entered BT config mode: esp32-imu-config");
  SerialBT.println("{\"status\":\"bt_config\",\"msg\":\"Send SSID:<ssid>, PASS:<pass>, CONNECT\"}");

  char bt_ssid[33] = {0};
  char bt_pass[65] = {0};
  String bt_buf = "";

  while (true) {
    while (SerialBT.available()) {
      char c = SerialBT.read();
      if (c == '\n' || c == '\r') {
        if (bt_buf.length() > 0) {
          bool doConnect = false;
          bool doReset = false;
          processBtCommand(bt_buf, bt_ssid, bt_pass, doConnect, doReset);
          bt_buf = "";

          if (doReset) {
            clearWiFiEEPROM();
            SerialBT.println("{\"status\":\"reset\",\"msg\":\"WiFi EEPROM cleared, will use hardcoded creds on reboot\"}");
            bt_ssid[0] = '\0';
            bt_pass[0] = '\0';
          }

          if (doConnect) {
            SerialBT.printf("{\"status\":\"connecting\",\"ssid\":\"%s\"}\n", bt_ssid);
            if (tryConnectWiFi(bt_ssid, bt_pass, 15000)) {
              saveWiFiToEEPROM(bt_ssid, bt_pass);
              SerialBT.printf("{\"status\":\"connected\",\"ip\":\"%s\"}\n", WiFi.localIP().toString().c_str());
              SerialBT.println("{\"status\":\"saved\",\"msg\":\"Credentials saved to EEPROM\"}");
              delay(1000);
              SerialBT.end();
              connectingWifiPage(false, true);
              return;
            } else {
              SerialBT.println("{\"status\":\"error\",\"msg\":\"Connection failed, try again\"}");
            }
          }
        }
      } else {
        bt_buf += c;
      }
    }
    delay(10);
  }
}

void connectWiFi() {
  bool eeprom_ok = loadWiFiFromEEPROM(g_wifi_ssid, g_wifi_pass);
  if (eeprom_ok) {
    Serial.printf("Using WiFi creds from EEPROM: \"%s\"\n", g_wifi_ssid);
  } else {
    strncpy(g_wifi_ssid, WIFI_SSID, 32);
    strncpy(g_wifi_pass, WIFI_PASSWORD, 64);
    Serial.printf("Using hardcoded WiFi creds: \"%s\"\n", g_wifi_ssid);
  }

  if (tryConnectWiFi(g_wifi_ssid, g_wifi_pass, 10000)) {
    connectingWifiPage(false, true);
  } else {
    Serial.println("WiFi connection failed, entering BT config mode...");
    enterBtConfigMode();
  }

  Serial.println();
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  if (!MDNS.begin(IMU_HOSTNAME)) {
    Serial.println("Error setting up MDNS responder!");
    while (1) { delay(1000); }
  }
  Serial.println("mDNS responder started");

  triggerDisplayUpdate();
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
}