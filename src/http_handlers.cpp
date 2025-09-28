#include "globals.h"

static void handleStart();
static void handleStop();
static void handleToggle();
static void handleStatus();
static void handleTargetGet();
static void handleTargetSet();
static void handleTargetReset();
static void handlePeerGet();
static void handlePeerSet();
static void handlePeerReset();
static void handleRecalibrate();
static void handleNetTune();
static void handleSDStatus();
static void handleSDStart();
static void handleSDStop();
static void handleSetDelay();
static void handleDiscovery();

void httpTask(void *pv) {
  for (;;) { server.handleClient(); vTaskDelay(pdMS_TO_TICKS(5)); }
}

void registerHttpRoutes() {
  server.on("/stream/start",    HTTP_ANY, handleStart);
  server.on("/stream/stop",     HTTP_ANY, handleStop);
  server.on("/stream/toggle",   HTTP_ANY, handleToggle);
  server.on("/status",          HTTP_ANY, handleStatus);
  server.on("/imu/recalibrate", HTTP_ANY, handleRecalibrate);
  server.on("/imu/delay",       HTTP_ANY, handleSetDelay);
  server.on("/peer/get",        HTTP_ANY, handlePeerGet);
  server.on("/peer/set",        HTTP_ANY, handlePeerSet);  
  server.on("/peer/reset",      HTTP_ANY, handlePeerReset);
  server.on("/net/tune",        HTTP_ANY, handleNetTune);
  server.on("/sd/status",       HTTP_ANY, handleSDStatus);
  server.on("/sd/start",        HTTP_ANY, handleSDStart);
  server.on("/sd/stop",         HTTP_ANY, handleSDStop);
  discoveryServer.on("/", HTTP_ANY, handleDiscovery);
}

static void handleStart() {
  g_stream_enabled = true;
  if (g_sd_available && !g_logging_enabled) {
    if (xSemaphoreTake(g_sdMutex, pdMS_TO_TICKS(250)) == pdTRUE) { startLogging(); xSemaphoreGive(g_sdMutex); }
  }
  triggerDisplayUpdate();
  server.send(200, "application/json", "{\"stream\":\"on\"}");
}
static void handleStop() {
  g_stream_enabled = false;
  if (g_logging_enabled) {
    if (xSemaphoreTake(g_sdMutex, pdMS_TO_TICKS(250)) == pdTRUE) { stopLogging(); xSemaphoreGive(g_sdMutex); }
  }
  triggerDisplayUpdate();
  server.send(200, "application/json", "{\"stream\":\"off\"}");
}
static void handleToggle() {
  g_stream_enabled = !g_stream_enabled;
  if (g_stream_enabled) {
    if (g_sd_available && !g_logging_enabled) {
      if (xSemaphoreTake(g_sdMutex, pdMS_TO_TICKS(250)) == pdTRUE) { startLogging(); xSemaphoreGive(g_sdMutex); }
    }
  } else {
    if (g_logging_enabled) {
      if (xSemaphoreTake(g_sdMutex, pdMS_TO_TICKS(250)) == pdTRUE) { stopLogging(); xSemaphoreGive(g_sdMutex); }
    }
  }
  triggerDisplayUpdate();
  server.send(200, "application/json", g_stream_enabled ? "{\"stream\":\"on\"}" : "{\"stream\":\"off\"}");
}
static void handleStatus() {
  String synced = getTimebase().synced ? "true" : "false";
  String ip = WiFi.localIP().toString();
  long latency_ms = (long)g_ntp_offset_ms_ewma;
  uint8_t ch = (g_peer_channel == 0) ? (uint8_t)WiFi.channel() : g_peer_channel;
  String body = String("{\"ip\":\"") + ip +
                "\",\"stream\":\"" + (g_stream_enabled ? "on" : "off") +
                "\",\"ntp_synced\":" + synced +
                ",\"imu_delay_ms\":" + String(imu_delay_ms) +
                ",\"transport\":\"espnow\"" +
                ",\"peer_mac\":\"" + macToString(g_peer_mac) + "\"" +
                ",\"peer_channel\":" + String(ch) +
                ",\"send_ok\":" + String(g_send_ok) +
                ",\"send_fail\":" + String(g_send_fail) +
                ",\"last_send_status\":" + String((int)g_last_send_status) +
                ",\"calibrating\":" + String(g_calibrating ? "true" : "false") +
                ",\"latency_ms\":" + String(latency_ms) +
                ",\"loop_dt_avg_ms\":" + String(g_loop_dt_avg_ms) +
                ",\"jitter_avg_ms\":" + String(g_loop_jitter_avg_ms) +
                ",\"sd_available\":" + String(g_sd_available ? "true" : "false") +
                ",\"logging_enabled\":" + String(g_logging_enabled ? "true" : "false") +
                ",\"log_records\":" + String(g_log_records_written) +
                ",\"log_errors\":" + String(g_log_write_errors) +
                ",\"log_file\":\"" + currentLogFileName + "\"" +
                "}";
  server.send(200, "application/json", body);
}

static void handlePeerGet() {
  uint8_t ch = (g_peer_channel == 0) ? (uint8_t)WiFi.channel() : g_peer_channel;
  String body = String("{\"peer_mac\":\"") + macToString(g_peer_mac) + "\",\"peer_channel\":" + String(ch) + "}";
  server.send(200, "application/json", body);
}
static void handlePeerSet() {
  if (!server.hasArg("mac")) { server.send(400, "application/json", "{\"error\":\"missing mac\"}"); return; }
  uint8_t mac[6];
  if (!parseMac(server.arg("mac").c_str(), mac)) { server.send(400, "application/json", "{\"error\":\"invalid mac\"}"); return; }
  uint8_t ch = g_peer_channel;
  if (server.hasArg("ch")) {
    int c = server.arg("ch").toInt();
    if (c < 0 || c > 14) { server.send(400, "application/json", "{\"error\":\"invalid channel\"}"); return; }
    ch = (uint8_t)c;
  }
  memcpy(g_peer_mac, mac, 6);
  g_peer_channel = ch;
  bool ok1 = addOrUpdatePeer();
  bool ok2 = savePeerToEEPROM(g_peer_mac, g_peer_channel);
  triggerDisplayUpdate();
  String body = String("{\"peer_added\":") + (ok1 ? "true" : "false") +
                ",\"saved\":" + (ok2 ? "true" : "false") +
                ",\"peer_mac\":\"" + macToString(g_peer_mac) + "\"" +
                ",\"peer_channel\":" + String((g_peer_channel==0)?WiFi.channel():g_peer_channel) + "}";
  server.send(200, "application/json", body);
}
static void handlePeerReset() {
  memset(g_peer_mac, 0, 6);
  g_peer_channel = 0;
  esp_now_deinit();
  esp_now_init();
  esp_now_register_send_cb(onEspNowSent);
  bool ok = savePeerToEEPROM(g_peer_mac, g_peer_channel);
  triggerDisplayUpdate();
  server.send(200, "application/json", String("{\"peer_reset\":true,\"saved\":") + (ok?"true":"false") + "}");
}
static void handleRecalibrate() {
  bool was_streaming = g_stream_enabled;
  g_stream_enabled = false;
  g_calibrating = true;
  triggerDisplayUpdate();
  delay(800);
  if (xSemaphoreTake(g_i2cMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
    MPU.autoOffsets();
    xSemaphoreGive(g_i2cMutex);
  } else {
    MPU.autoOffsets();
  }
  g_calibrating = false;
  g_stream_enabled = was_streaming;
  triggerDisplayUpdate();
  server.send(200, "application/json", "{\"recalibrated\":true}");
}
static void handleNetTune() {
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  server.send(200, "application/json", "{\"wifi\":\"tuned\"}");
}
static void handleSDStatus() {
  String body = String("{\"sd_available\":") + (g_sd_available ? "true" : "false") +
                ",\"logging_enabled\":" + (g_logging_enabled ? "true" : "false") +
                ",\"current_file\":\"" + currentLogFileName + "\"" +
                ",\"records_written\":" + String(g_log_records_written) +
                ",\"write_errors\":" + String(g_log_write_errors) + "}";
  server.send(200, "application/json", body);
}
static void handleSDStart() {
  if (!g_sd_available) { server.send(503, "application/json", "{\"error\":\"SD not available\"}"); return; }
  bool success = false;
  if (xSemaphoreTake(g_sdMutex, pdMS_TO_TICKS(250)) == pdTRUE) { success = startLogging(); xSemaphoreGive(g_sdMutex); }
  String body = String("{\"logging_started\":") + (success ? "true" : "false") +
                ",\"file\":\"" + currentLogFileName + "\"}";
  server.send(success ? 200 : 500, "application/json", body);
  if (success) triggerDisplayUpdate();
}
static void handleSDStop() {
  bool success = false;
  if (xSemaphoreTake(g_sdMutex, pdMS_TO_TICKS(250)) == pdTRUE) { success = stopLogging(); xSemaphoreGive(g_sdMutex); }
  String body = String("{\"logging_stopped\":") + (success ? "true" : "false") +
                ",\"final_records\":" + String(g_log_records_written) +
                ",\"write_errors\":" + String(g_log_write_errors) + "}";
  server.send(200, "application/json", body);
  triggerDisplayUpdate();
}
// IMU delay setter (kept)
static void handleSetDelay() {
  uint32_t new_delay_ms = imu_delay_ms;
  if (server.hasArg("hz")) {
    double hz = server.arg("hz").toDouble();
    if (hz <= 0.0) { server.send(400, "application/json", "{\"error\":\"hz must be > 0\"}"); return; }
    double ms = 1000.0 / hz;
    if (ms < 5.0)   ms = 5.0;
    if (ms > 1000.) ms = 1000.0;
    new_delay_ms = (uint32_t)ms;
  } else if (server.hasArg("ms")) {
    long ms = server.arg("ms").toInt();
    if (ms < 5) ms = 5;
    if (ms > 1000) ms = 1000;
    new_delay_ms = (uint32_t)ms;
  } else {
    server.send(400, "application/json", "{\"error\":\"missing 'ms' or 'hz'\"}");
    return;
  }
  imu_delay_ms = new_delay_ms;
  triggerDisplayUpdate();
  String body = String("{\"imu_delay_ms\":") + String(imu_delay_ms) + "}";
  server.send(200, "application/json", body);
}

static void handleDiscovery() {
  String body = String("{\"device\":\"imu-fd-now\",\"ip\":\"") + WiFi.localIP().toString() + "\"}";
  discoveryServer.send(200, "application/json", body);
}