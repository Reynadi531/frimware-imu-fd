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

bool loadWiFiFromEEPROM(char *ssid, char *pass) {
  uint32_t magic = 0;
  EEPROM.get(EEPROM_WIFI_BASE, magic);
  if (magic != EEPROM_MAGIC_WIFI) return false;
  EEPROM.get(EEPROM_WIFI_BASE + 4, ssid);
  EEPROM.get(EEPROM_WIFI_BASE + 4 + 33, pass);
  ssid[32] = '\0';
  pass[64] = '\0';
  if (ssid[0] == '\0') return false;
  return true;
}

bool saveWiFiToEEPROM(const char *ssid, const char *pass) {
  uint32_t magic = EEPROM_MAGIC_WIFI;
  char ssid_buf[33] = {0};
  char pass_buf[65] = {0};
  strncpy(ssid_buf, ssid, 32);
  strncpy(pass_buf, pass, 64);
  EEPROM.put(EEPROM_WIFI_BASE, magic);
  EEPROM.put(EEPROM_WIFI_BASE + 4, ssid_buf);
  EEPROM.put(EEPROM_WIFI_BASE + 4 + 33, pass_buf);
  return EEPROM.commit();
}

void clearWiFiEEPROM() {
  for (int i = 0; i < 4 + 33 + 65; i++) {
    EEPROM.write(EEPROM_WIFI_BASE + i, 0);
  }
  EEPROM.commit();
}
