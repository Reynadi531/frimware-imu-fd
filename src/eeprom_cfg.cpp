#include "globals.h"

void loadPeerFromEEPROM() {
  PersistPeer p{};
  EEPROM.get(sizeof(PersistPeer), p);
  if (p.magic == EEPROM_MAGIC_PEER) {
    memcpy(g_peer_mac, p.mac, 6);
    g_peer_channel = p.channel;
  } else {
    #ifdef TARGET_MAC
    if (!parseMac(TARGET_MAC, g_peer_mac)) memset(g_peer_mac, 0, 6);
    g_peer_channel = 0;
    #else
    memset(g_peer_mac, 0, 6);
    g_peer_channel = 0;
    #endif
  }
}
bool savePeerToEEPROM(const uint8_t mac[6], uint8_t channel) {
  PersistPeer p{};
  p.magic = EEPROM_MAGIC_PEER;
  memcpy(p.mac, mac, 6);
  p.channel = channel;
  EEPROM.put(sizeof(PersistPeer), p);
  return EEPROM.commit();
}

void loadWiFiFromEEPROM(char* ssid, char* password) {
  PersistWiFi w{};
  EEPROM.get(sizeof(PersistPeer) * 2, w);
  if (w.magic == EEPROM_MAGIC_WIFI) {
    strncpy(ssid, w.ssid, 64);
    strncpy(password, w.password, 64);
    ssid[63] = '\0';
    password[63] = '\0';
    Serial.println("WiFi: Loaded from EEPROM");
  } else {
    // No stored credentials, use defaults from wifi_secret.h
    strncpy(ssid, WIFI_SSID, 64);
    strncpy(password, WIFI_PASSWORD, 64);
    Serial.println("WiFi: Using defaults");
  }
}

bool saveWiFiToEEPROM(const char* ssid, const char* password) {
  PersistWiFi w{};
  w.magic = EEPROM_MAGIC_WIFI;
  strncpy(w.ssid, ssid, 63);
  strncpy(w.password, password, 63);
  w.ssid[63] = '\0';
  w.password[63] = '\0';
  EEPROM.put(sizeof(PersistPeer) * 2, w);
  bool result = EEPROM.commit();
  Serial.printf("WiFi save to EEPROM: %s\n", result ? "OK" : "FAIL");
  return result;
}
