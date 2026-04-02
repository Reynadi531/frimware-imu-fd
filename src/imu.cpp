#include "globals.h"
#include <esp_timer.h>

#define USE_ESP_TIMER 1

void imuTask(void *pv) {
  TickType_t last_wake = xTaskGetTickCount();
  int64_t last_us = 0;
  uint32_t loop_dt_ms_acc = 0;
  uint32_t loop_jitter_acc = 0;
  uint8_t loop_sample_count = 0;

#if USE_ESP_TIMER
  int64_t target_us = imu_delay_ms * 1000LL;
  int64_t last_trig_us = esp_timer_get_time();
#else
  (void)last_us;
#endif

  for (;;) {
    if (g_calibrating) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }

    TickType_t work_start = xTaskGetTickCount();

    xyzFloat a, g;
    float tC;

    a  = MPU.getGValues();
    g  = MPU.getGyrValues();
    tC = MPU.getTemperature();

#if USE_ESP_TIMER
    int64_t now_us = esp_timer_get_time();
    uint64_t now_ms = (uint64_t)(now_us / 1000ULL);
    String iso = iso8601_local_ms(now_ms);
    uint32_t tick_ms = (uint32_t)(now_us / 1000ULL);
#else
    TickType_t ticks = xTaskGetTickCount();
    uint32_t tick_ms = (uint32_t)(ticks * portTICK_PERIOD_MS);
    uint64_t now_ms  = epochMillisNow();
    String iso       = iso8601_local_ms(now_ms);
#endif

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
        if (xSemaphoreTake(g_sdMutex, pdMS_TO_TICKS(2)) == pdTRUE) {
          if (!logIMUData(payload)) {
            if (g_log_write_errors > 100) { stopLogging(); triggerDisplayUpdate(); }
          }
          xSemaphoreGive(g_sdMutex);
        }
      }
      if (memcmp(g_peer_mac, "\x00\x00\x00\x00\x00\x00", 6) != 0) {
        if (xSemaphoreTake(g_netMutex, pdMS_TO_TICKS(2)) == pdTRUE) {
          esp_err_t err = esp_now_send(g_peer_mac, (const uint8_t*)payload, (size_t)n);
          xSemaphoreGive(g_netMutex);
          if (err != ESP_OK) { g_send_fail++; g_last_send_status = ESP_NOW_SEND_FAIL; }
        }
      }
    }

#if USE_ESP_TIMER
    int64_t elapsed_us = esp_timer_get_time() - work_start;
    int64_t interval_us = esp_timer_get_time() - last_trig_us;
    int64_t jitter_us = interval_us - target_us;
    if (jitter_us < 0) jitter_us = -jitter_us;

    loop_dt_ms_acc += (uint32_t)(interval_us / 1000ULL);
    loop_jitter_acc += (uint32_t)(jitter_us / 1000ULL);
    loop_sample_count++;

    if (loop_sample_count >= 8) {
      g_loop_dt_avg_ms = loop_dt_ms_acc / loop_sample_count;
      g_loop_jitter_avg_ms = loop_jitter_acc / loop_sample_count;
      loop_dt_ms_acc = 0;
      loop_jitter_acc = 0;
      loop_sample_count = 0;
    }

    int64_t sleep_us = target_us - elapsed_us;
    if (sleep_us > 0) {
      ets_delay_us((uint32_t)sleep_us);
    }
    last_trig_us += target_us;
    if (esp_timer_get_time() - last_trig_us > target_us) {
      last_trig_us = esp_timer_get_time();
    }
#else
    TickType_t ticks_now = xTaskGetTickCount();
    TickType_t ticks_elapsed = ticks_now - work_start;
    TickType_t ticks_target = pdMS_TO_TICKS(imu_delay_ms);
    TickType_t ticks_delta = ticks_now - last_wake;

    loop_dt_ms_acc += (uint32_t)(ticks_delta * portTICK_PERIOD_MS);
    loop_jitter_acc += (ticks_delta > ticks_target) ? (uint32_t)((ticks_delta - ticks_target) * portTICK_PERIOD_MS) : (uint32_t)((ticks_target - ticks_delta) * portTICK_PERIOD_MS);
    loop_sample_count++;

    if (loop_sample_count >= 8) {
      g_loop_dt_avg_ms = loop_dt_ms_acc / loop_sample_count;
      g_loop_jitter_avg_ms = loop_jitter_acc / loop_sample_count;
      loop_dt_ms_acc = 0;
      loop_jitter_acc = 0;
      loop_sample_count = 0;
    }

    if (ticks_elapsed < ticks_target) {
      vTaskDelay(ticks_target - ticks_elapsed);
      last_wake = work_start + ticks_target;
    } else {
      last_wake = ticks_now;
    }
#endif
  }
}
