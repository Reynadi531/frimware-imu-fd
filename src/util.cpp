#include "globals.h"

String iso8601_utc_ms(uint64_t epoch_ms) {
  if (epoch_ms == 0ULL) return String("1970-01-01T00:00:00.000Z");
  time_t secs = epoch_ms / 1000ULL;
  uint16_t ms = (uint16_t)(epoch_ms % 1000ULL);
  struct tm tm_utc;
  gmtime_r(&secs, &tm_utc);
  char buf[32];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03uZ",
           tm_utc.tm_year + 1900, tm_utc.tm_mon + 1, tm_utc.tm_mday,
           tm_utc.tm_hour, tm_utc.tm_min, tm_utc.tm_sec, ms);
  return String(buf);
}

String iso8601_local_ms(uint64_t epoch_ms) {
  if (epoch_ms == 0ULL) return String("1970-01-01T00:00:00.000Z");
  
  time_t secs = (epoch_ms / 1000ULL) + GMT_OFFSET_SEC;
  uint16_t ms = (uint16_t)(epoch_ms % 1000ULL);

  struct tm tm_local;
  gmtime_r(&secs, &tm_local);

  char buf[40];
  snprintf(buf, sizeof(buf),
           "%04d-%02d-%02dT%02d:%02d:%02d.%03u%+03d:00",
           tm_local.tm_year + 1900, tm_local.tm_mon + 1, tm_local.tm_mday,
           tm_local.tm_hour, tm_local.tm_min, tm_local.tm_sec,
           ms,
           GMT_OFFSET_SEC / 3600);  

  return String(buf);
}


String macToString(const uint8_t mac[6]) {
  char b[18];
  snprintf(b, sizeof(b), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(b);
}
bool parseMac(const char* s, uint8_t out[6]) {
  int v[6];
  if (!s) return false;
  if (sscanf(s, "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) return false;
  for (int i = 0; i < 6; i++) out[i] = (uint8_t)v[i];
  return true;
}
