#include "globals.h"

static Timebase g_timebase = {0, 0, false};

void setTimebase(time_t epoch_s, uint32_t base_ms) {
  LOCK_TIME();
  g_timebase.epoch_s = epoch_s;
  g_timebase.base_ms = base_ms;
  g_timebase.synced  = (epoch_s != 0);
  UNLOCK_TIME();
}
Timebase getTimebase() {
  Timebase tb;
  LOCK_TIME();
  tb = g_timebase;
  UNLOCK_TIME();
  return tb;
}
uint64_t epochMillisNow() {
  Timebase tb = getTimebase();
  if (!tb.synced) return 0ULL;
  uint32_t delta = millis() - tb.base_ms;
  return (uint64_t)tb.epoch_s * 1000ULL + (uint64_t)delta;
}

void ntpTask(void *pv) {
  for (;;) {
    if (timeClient.update()) {
      setTimebase(timeClient.getEpochTime(), millis());
      int64_t now_ms = (int64_t)millis();
      int64_t ntp_ms = (int64_t)timeClient.getEpochTime() * 1000LL;
      int64_t offset_ms = ntp_ms - now_ms;
      g_ntp_offset_ms_ewma = offset_ms;
    }
    vTaskDelay(pdMS_TO_TICKS(10 * 1000));
  }
}
