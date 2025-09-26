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
