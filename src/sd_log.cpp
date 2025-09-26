#include "globals.h"

bool initSDCard() {
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  if (!sd.begin(SD_CS_PIN, SD_SCK_MHZ(25))) {
    Serial.println("SD init failed");
    g_sd_available = false;
    return false;
  }
  g_sd_available = true;
  if (!sd.exists("/IMU_DATA")) sd.mkdir("/IMU_DATA");
  return true;
}

String generateLogFileName() {
  uint64_t now_ms = epochMillisNow();
  if (now_ms == 0ULL) return String("/IMU_DATA/IMU_") + String(millis()) + ".csv";
  time_t secs = now_ms / 1000ULL;
  struct tm tm_utc;
  gmtime_r(&secs, &tm_utc);
  char filename[64];
  snprintf(filename, sizeof(filename), "/IMU_DATA/IMU_%04d%02d%02d_%02d%02d%02d.csv",
           tm_utc.tm_year + 1900, tm_utc.tm_mon + 1, tm_utc.tm_mday,
           tm_utc.tm_hour, tm_utc.tm_min, tm_utc.tm_sec);
  return String(filename);
}

bool startLogging() {
  if (!g_sd_available) return false;
  if (g_logging_enabled) stopLogging();

  currentLogFileName = generateLogFileName();
  if (!currentLogFile.open(currentLogFileName.c_str(), O_CREAT | O_WRITE | O_APPEND)) {
    Serial.println("open fail: " + currentLogFileName);
    return false;
  }

  g_logging_enabled = true;
  g_log_records_written = 0;
  g_log_write_errors = 0;

  const char *header =
    "timestamp,tick_ms,seq,ax,ay,az,gx,gy,gz,temp\n";
  if (currentLogFile.print(header)) {
    currentLogFile.flush();
    g_log_records_written++;   
  } else {
    g_log_write_errors++;
  }

  Serial.println("Logging (CSV): " + currentLogFileName);
  return true;
}

bool stopLogging() {
  if (!g_logging_enabled) return true;

  if (currentLogFile.isOpen()) {
    currentLogFile.flush();
    currentLogFile.close();
  }
  g_logging_enabled = false;
  currentLogFileName = "";
  return true;
}

bool logIMUData(const String& data) {
  if (!g_logging_enabled || !currentLogFile.isOpen()) return false;

  if (currentLogFile.println(data)) {
    if (g_log_records_written % 10 == 0) currentLogFile.flush();
    g_log_records_written++;
    return true;
  } else {
    g_log_write_errors++;
    return false;
  }
}
