#include "globals.h"

void connectWiFi()
{
  char ssid[64] = {0};
  char password[64] = {0};
  
  loadWiFiFromEEPROM(ssid, password);
  Serial.printf("WiFi: %s\n", ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("esp32-imu");
  
  // CRITICAL: Enable WiFi modem sleep when Bluetooth is active
  // This is required by ESP32 hardware when both WiFi and BT are enabled
  WiFi.setSleep(true);
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  const int maxAttempts = 20;
  
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts)
  {
    delay(500);
    connectingWifiPage(true, false);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("\nWiFi Failed!"));
    Serial.println(F("BT active for config"));
    connectingWifiPage(false, false);
    // Keep Bluetooth running for configuration
    return;
  }

  // WiFi connected successfully!
  connectingWifiPage(false, true);
  Serial.print(F("\nIP: "));
  Serial.println(WiFi.localIP());
  
  // Now that WiFi is connected, optimize for performance
  Serial.println(F("Optimizing WiFi..."));
  
  // Stop Bluetooth - no longer needed
  stopBluetooth();
  
  // Disable WiFi sleep for maximum performance
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  
  Serial.println(F("WiFi: Full performance mode"));
  
  if (!MDNS.begin(IMU_HOSTNAME)) {  
    Serial.println(F("mDNS fail!"));
    while(1) delay(1000);
  }

  triggerDisplayUpdate();
}
