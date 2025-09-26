#include "globals.h"

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("esp32-imu");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println(); Serial.print("IP: "); Serial.println(WiFi.localIP());
  triggerDisplayUpdate();
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
}
