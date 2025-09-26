#include "globals.h"

bool addOrUpdatePeer() {
  if (memcmp(g_peer_mac, "\x00\x00\x00\x00\x00\x00", 6) == 0) return false;
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, g_peer_mac, 6);
  peer.channel = (g_peer_channel == 0) ? (uint8_t)WiFi.channel() : g_peer_channel;
  peer.encrypt = false;
  esp_now_del_peer(peer.peer_addr);
  return esp_now_add_peer(&peer) == ESP_OK;
}
void onEspNowSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
  g_last_send_status = status;
  if (status == ESP_NOW_SEND_SUCCESS) g_send_ok++;
  else g_send_fail++;
  static uint32_t last_total = 0;
  uint32_t current_total = g_send_ok + g_send_fail;
  if (current_total != last_total) { triggerDisplayUpdate(); last_total = current_total; }
}
bool initEspNow() {
  if (esp_now_init() != ESP_OK) { Serial.println("ESP-NOW init failed"); return false; }
  esp_now_register_send_cb(onEspNowSent);
  return addOrUpdatePeer();
}
