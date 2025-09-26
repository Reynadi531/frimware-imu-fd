#include "globals.h"

void imuTask(void *pv) {
  TickType_t last_wake = xTaskGetTickCount();
  uint32_t loop_dt_ms_last = imu_delay_ms;

  for (;;) {
    if (g_calibrating) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }

    xyzFloat a, g;
    float tC;

    if (xSemaphoreTake(g_i2cMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      a  = MPU.getGValues();
      g  = MPU.getGyrValues();
      tC = MPU.getTemperature();
      xSemaphoreGive(g_i2cMutex);
    } else {
      vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(imu_delay_ms));
      continue;
    }

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
      if (memcmp(g_peer_mac, "\x00\x00\x00\x00\x00\x00", 6) != 0) {
        if (xSemaphoreTake(g_netMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
          esp_err_t err = esp_now_send(g_peer_mac, (const uint8_t*)payload, (size_t)n);
          xSemaphoreGive(g_netMutex);
          if (err != ESP_OK) { g_send_fail++; g_last_send_status = ESP_NOW_SEND_FAIL; }
        }
      }
      if (g_logging_enabled) {
        if (xSemaphoreTake(g_sdMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
          if (!logIMUData(String(payload))) {
            if (g_log_write_errors > 100) { stopLogging(); triggerDisplayUpdate(); }
          }
          xSemaphoreGive(g_sdMutex);
        }
      }
    }

    TickType_t prev_wake = last_wake;
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(imu_delay_ms));
    TickType_t now_ticks = xTaskGetTickCount();
    uint32_t loop_dt_ms = (uint32_t)((now_ticks - prev_wake) * portTICK_PERIOD_MS);
    int32_t jitter_ms = (int32_t)loop_dt_ms - (int32_t)imu_delay_ms;
    if (jitter_ms < 0) jitter_ms = -jitter_ms;
    g_loop_dt_avg_ms = loop_dt_ms;
    g_loop_jitter_avg_ms = jitter_ms;
  }
}
