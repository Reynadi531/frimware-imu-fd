#include "globals.h"

void imuTask(void *pv) {
  TickType_t last_wake = xTaskGetTickCount();
  uint32_t loop_dt_ms_last = imu_delay_ms;

  for (;;) {
    if (g_calibrating) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }

    xyzFloat a, g;
    float tC;

    a  = MPU.getGValues();
    g  = MPU.getGyrValues();
    tC = MPU.getTemperature();

    TickType_t ticks = xTaskGetTickCount();
    uint32_t tick_ms = (uint32_t)(ticks * portTICK_PERIOD_MS);
    uint64_t now_ms  = epochMillisNow();
    String iso       = iso8601_local_ms(now_ms);

    char payload[220];
    int n = snprintf(
      payload, sizeof(payload),
      "%s,%lu,%lu,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.2f",
      iso.c_str(),
      (unsigned long)tick_ms,
      (unsigned long)pkt_seq++,
      a.x, a.y, a.z,
      g.x, g.y, g.z,
      tC
    );

    if (g_stream_enabled && n > 0) {
      if (g_logging_enabled) {
        if (xSemaphoreTake(g_sdMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
          if (!logIMUData(String(payload))) {
            if (g_log_write_errors > 100) { stopLogging(); triggerDisplayUpdate(); }
          }
          xSemaphoreGive(g_sdMutex);
        }
      }
      if (memcmp(g_peer_mac, "\x00\x00\x00\x00\x00\x00", 6) != 0) {
        if (xSemaphoreTake(g_netMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
          esp_err_t err = esp_now_send(g_peer_mac, (const uint8_t*)payload, (size_t)n);
          xSemaphoreGive(g_netMutex);
          if (err != ESP_OK) { g_send_fail++; g_last_send_status = ESP_NOW_SEND_FAIL; }
        }
      }
    }

    TickType_t prev_wake = last_wake;
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(imu_delay_ms));
  }
}
