#include "globals.h"

void connectWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("esp32-imu");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    connectingWifiPage(true, false);
    Serial.print(".");
  }

  connectingWifiPage(false, true);

  Serial.println();
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  if (!MDNS.begin(IMU_HOSTNAME)) {  
    Serial.println("Error setting up MDNS responder!");
    while(1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");

  triggerDisplayUpdate();
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
}
