# Data and Persistence

This guide documents what the firmware stores, what it transmits, what it logs, and how runtime counters should be interpreted.

## Persistence overview

The firmware persists three classes of data in EEPROM:

1. ESP-NOW peer configuration
2. Wi-Fi credentials
3. IMU calibration parameters

It also persists session data to microSD when logging is enabled.

## EEPROM usage

EEPROM is initialized with:

- total size: `256` bytes

Relevant constants from `include/globals.h` and `src/calibration.cpp`:

- calibration base: `24`
- Wi-Fi base: `80`
- Wi-Fi magic: `EEPROM_MAGIC_WIFI`
- peer magic: `EEPROM_MAGIC_PEER`
- calibration magic: `0x43414C42`

## ESP-NOW peer persistence

Peer persistence is implemented in `src/eeprom_cfg.cpp` using this structure from `include/globals.h`:

```cpp
struct PersistPeer {
  uint32_t magic;
  uint8_t  mac[6];
  uint8_t  channel;
  uint8_t  _pad;
};
```

Storage behavior:

- the struct is stored at EEPROM offset `sizeof(PersistPeer)`
- on load, if `magic == EEPROM_MAGIC_PEER`, the stored MAC and channel become the active peer
- if the magic does not match, the firmware falls back to `TARGET_MAC` from `src/target_secret.h` when available
- otherwise the peer is cleared to all zeros

Operational meaning:

- a MAC of `00:00:00:00:00:00` means "no active peer configured"
- a channel of `0` means "follow the current Wi-Fi channel"

## Wi-Fi credential persistence

Wi-Fi credentials are stored starting at EEPROM offset `80`.

Layout:

- `+0..3`: magic value `EEPROM_MAGIC_WIFI`
- `+4..36`: SSID buffer (`33` bytes including terminator handling)
- `+37..101`: password buffer (`65` bytes including terminator handling)

Load behavior:

- if the Wi-Fi magic is missing, load fails
- if the stored SSID is empty, load fails
- on failure, the firmware falls back to hardcoded credentials in `src/wifi_secret.h`

Save behavior:

- input SSID is truncated to `32` characters
- input password is truncated to `64` characters
- buffers are zero-filled before commit

Clear behavior:

- Bluetooth command `RESET` clears only the Wi-Fi credential region
- it does not erase peer configuration or calibration data

## Calibration persistence

Calibration persistence lives in `src/calibration.cpp`.

Stored data:

- calibration magic at EEPROM offset `24`
- `offset[3]` float array
- `correction[9]` float array
- a one-byte validity flag

Saved model:

- hard-iron style offset vector (`offset[3]`)
- `3x3` correction matrix (`correction[9]`)
- boolean validity flag

Load behavior:

- if the calibration magic does not match, calibration is considered invalid
- on successful load, the IMU task applies calibration to acceleration before transmission/logging

Write behavior:

- after a successful six-position fit, the firmware commits the model to EEPROM
- the main boot sequence loads it automatically on the next restart

## Calibration workflow data

Calibration runtime state is not fully persisted; only the fitted result is.

Transient runtime fields:

- `g_calib_state`
- `g_calib_position`
- `g_calib_sample_idx`
- in-memory sample buffer for `768` raw acceleration samples

Persistent output:

- `g_calib_params.offset`
- `g_calib_params.correction`
- `g_calib_params.valid`

That means a power loss during calibration loses the in-progress sample set; only a completed successful fit survives reboot.

## SD logging behavior

SD logging is implemented in `src/sd_log.cpp` and fed by `src/imu.cpp`.

### Directory and file placement

Log files are stored in:

```text
/IMU_DATA/
```

If the directory does not exist, the firmware creates it during SD initialization.

### File naming

When NTP-backed time is available, file names use UTC calendar time:

```text
/IMU_DATA/IMU_YYYYMMDD_HHMMSS.csv
```

Example:

```text
/IMU_DATA/IMU_20251022_143022.csv
```

When synchronized epoch time is not yet available, file naming falls back to device uptime milliseconds:

```text
/IMU_DATA/IMU_<millis>.csv
```

This fallback matters during early boot or when NTP has not succeeded.

### File lifecycle

On log start:

1. if logging is already active, the current file is stopped first
2. a new file name is generated
3. the file is opened with create/write/append flags
4. the CSV header is written immediately
5. logging state and counters are reset

On log stop:

1. the current file is flushed
2. the file is closed
3. `g_logging_enabled` becomes `false`
4. `currentLogFileName` becomes an empty string

### CSV header

The firmware writes this exact header:

```text
timestamp,tick_ms,seq,ax,ay,az,gx,gy,gz,temp
```

### CSV row schema

Each streamed/logged payload contains these fields in order:

1. `timestamp`
2. `tick_ms`
3. `seq`
4. `ax`
5. `ay`
6. `az`
7. `gx`
8. `gy`
9. `gz`
10. `temp`

Field meanings:

- `timestamp`: ISO-8601 local timestamp string from `iso8601_local_ms(epochMillisNow())`
- `tick_ms`: FreeRTOS tick count converted to milliseconds
- `seq`: monotonic packet counter `pkt_seq++`
- `ax/ay/az`: accelerometer values, calibrated when calibration is valid
- `gx/gy/gz`: gyroscope values
- `temp`: IMU temperature in Celsius

Example row shape:

```text
2025-10-22T21:30:22.123+07:00,123456,42,0.012345,-0.998877,0.045678,0.001234,0.005678,-0.002345,32.75
```

### Timestamp behavior inside payloads

Payload timestamps and file names do not use identical logic.

Payload timestamp behavior:

- generated with `iso8601_local_ms()`
- uses fixed offset `GMT_OFFSET_SEC`, currently `GMT+7`
- if no synchronized timebase exists, returns `1970-01-01T00:00:00.000Z`

File name timestamp behavior:

- generated from `epochMillisNow()` using `gmtime_r()`
- therefore uses UTC calendar naming when synchronized time exists
- falls back to `/IMU_DATA/IMU_<millis>.csv` when synchronized time does not exist

So the payload timestamp is **local-offset formatted**, while the file name is **UTC-based** when NTP is available.

### Write cadence and flush behavior

- each payload is written as one line via `println()`
- the file is flushed every `20` successful appended records
- the file is also flushed on explicit stop

### Error handling

- if opening the file fails, `startLogging()` returns `false`
- if line append fails, `g_log_write_errors` increments
- if the IMU task sees more than `100` write errors while streaming, it stops logging automatically and requests a display update

## Streamed transport payload

The ESP-NOW sender transmits the same CSV payload that can also be written to SD.

Important for receiver authors:

- the main sender sends text CSV, not a packed struct
- `gcs-src/main.ino` can parse CSV payloads even though it also contains a struct path for fixed-size payloads
- transport send is attempted only when a non-zero peer MAC is configured

## Runtime counters and status fields

These counters are surfaced through `/status` and `/sd/status`.

### Streaming / network counters

- `g_send_ok`: incremented by the ESP-NOW send callback on `ESP_NOW_SEND_SUCCESS`
- `g_send_fail`: incremented by the send callback on failure, and also incremented immediately when `esp_now_send()` itself returns an error
- `g_last_send_status`: most recent send callback status, or forced to `ESP_NOW_SEND_FAIL` when immediate send submission fails

Interpretation note:

- `send_fail` can reflect either delivery callback failures or local send submission failures

### Logging counters

- `g_log_records_written`: incremented once for the CSV header if that header write succeeds, and once per successful payload line after that
- `g_log_write_errors`: incremented on header write failure or data line write failure

Interpretation note:

- `log_records` / `records_written` are **not just sample rows**; they include the header row

### Loop timing counters

Updated by `imuTask` every 8 iterations:

- `g_loop_dt_avg_ms`: average observed loop interval
- `g_loop_jitter_avg_ms`: average absolute distance from the target loop interval

These help distinguish nominal rate from scheduling jitter.

### Time sync status

- `getTimebase().synced` drives `ntp_synced` in `/status`
- `g_ntp_offset_ms_ewma` is exposed as `latency_ms`

Interpretation note:

- despite the `_ewma` name, the current implementation assigns the latest offset directly each time NTP updates; it is not currently smoothed with `NTP_EWMA_ALPHA`

## Status field glossary

Common `/status` fields and what they mean:

- `ip`: current Wi-Fi station IP address
- `stream`: `on` or `off`
- `ntp_synced`: whether epoch time is available
- `imu_delay_ms`: current target loop interval
- `transport`: constant string `espnow`
- `peer_mac`: configured peer MAC or all-zero MAC if unset
- `peer_channel`: explicit peer channel or resolved current Wi-Fi channel
- `send_ok` / `send_fail`: ESP-NOW transmission counters
- `last_send_status`: numeric `esp_now_send_status_t`
- `calibrating`: `true` whenever calibration state is not `CALIB_IDLE`
- `latency_ms`: current reported NTP offset sample
- `loop_dt_avg_ms`: recent average loop period
- `jitter_avg_ms`: recent average timing error relative to target interval
- `sd_available`: whether the SD card initialized successfully
- `logging_enabled`: whether a file is currently open for logging
- `log_records`: current written-record counter, including header
- `log_errors`: current write error counter
- `log_file`: current open log file path, or empty string when stopped

## Persistence and operations checklist

When debugging field issues, check these in order:

1. `/peer/get` to confirm persisted peer MAC/channel
2. `/status` to confirm `ntp_synced`, `sd_available`, and `log_file`
3. `/imu/calib/status` to confirm calibration validity and state
4. serial logs for SD open failures or Wi-Fi provisioning fallbacks
5. SD card contents under `/IMU_DATA/` to confirm file creation and header shape
