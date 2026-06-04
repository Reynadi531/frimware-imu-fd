# Repository Walkthrough

This guide maps the repository to the runtime behavior of the device.

## Top-level structure

```text
.
├── docs/
│   ├── README.md
│   ├── api-reference.md
│   ├── data-and-persistence.md
│   ├── openapi.json
│   ├── repository-walkthrough.md
│   └── setup-and-operation.md
├── gcs-src/
│   └── main.ino
├── include/
│   ├── globals.h
│   └── README
├── src/
│   ├── calibration.cpp
│   ├── calibration.h
│   ├── display.cpp
│   ├── eeprom_cfg.cpp
│   ├── espnow.cpp
│   ├── http_handlers.cpp
│   ├── imu.cpp
│   ├── main.cpp
│   ├── sd_log.cpp
│   ├── target_secret.h
│   ├── timebase.cpp
│   ├── util.cpp
│   ├── wifi.cpp
│   └── wifi_secret.h
├── platformio.ini
└── README.md
```

## How the firmware is organized

### `platformio.ini`
Defines the main PlatformIO environment:

- board: `esp32doit-devkit-v1`
- framework: Arduino on `espressif32`
- libraries: MPU driver, SdFat, NTPClient, Adafruit GFX, Adafruit SSD1306
- partition table: `huge_app.csv`

This file is the build entrypoint for the main sender firmware.

### `include/globals.h`
This is the central interface header. It contains:

- global singleton declarations for the IMU, display, SD card, HTTP servers, NTP client, and shared runtime flags
- hardware pin definitions
- EEPROM constants and Wi-Fi/peer persistence constants
- task and subsystem function declarations
- shared utility helpers like MAC formatting and time conversion

When changing subsystem boundaries, this header is the first place to check for coupling.

### `src/main.cpp`
Owns boot sequencing and task creation.

`setup()` does the following in order:

1. Starts serial at `115200`.
2. Starts the default I2C bus with `Wire.begin()` for the IMU.
3. Starts `Wire1` on the OLED pins for the SSD1306 display.
4. Creates mutexes for SD and network access.
5. Initializes the OLED boot screen.
6. Initializes and configures the MPU6500.
7. Starts EEPROM and loads persisted ESP-NOW peer state.
8. Loads saved calibration if present.
9. Connects to Wi-Fi, with Bluetooth Serial fallback if connection fails.
10. Initializes the microSD card and creates `/IMU_DATA` if needed.
11. Initializes ESP-NOW and registers the send callback.
12. Registers HTTP routes, then starts the main server and discovery server.
13. Initializes NTP-backed timebase.
14. Creates four FreeRTOS tasks: NTP, IMU, HTTP, and display.
15. Triggers an initial display refresh.

`loop()` is empty; the firmware is task-driven after setup.

### `src/imu.cpp`
Owns the main acquisition and transmission loop.

For each iteration, the IMU task:

1. Checks calibration state.
   - `CALIB_COLLECTING`: samples acceleration only and stores calibration samples.
   - `CALIB_FITTING`: runs the ellipsoid fit and saves calibration to EEPROM on success.
   - `CALIB_FAILED`: idles until another calibration request.
2. Reads acceleration, gyroscope, and temperature from the MPU6500.
3. Applies calibration to acceleration if persisted parameters are valid.
4. Builds a CSV payload:
   - local ISO-8601 timestamp with milliseconds
   - tick count in milliseconds
   - packet sequence number
   - calibrated accelerometer XYZ
   - gyroscope XYZ
   - temperature
5. If streaming is enabled:
   - logs the payload to SD when logging is enabled
   - sends the payload over ESP-NOW when a peer MAC is configured
6. Updates rolling timing statistics (`g_loop_dt_avg_ms`, `g_loop_jitter_avg_ms`).
7. Delays to maintain the requested sampling interval.

The sender transmits **CSV text**, not a packed binary struct. The receiver example accepts both its own struct format and the CSV the sender currently emits.

### `src/http_handlers.cpp`
Owns all HTTP control and status routes.

Subsystem responsibilities:

- register endpoints on the main `WebServer` at port `80`
- register a discovery endpoint on a second `WebServer` at port `5681`
- mutate shared runtime flags for streaming, logging, calibration, peer management, display state, and timing
- expose operational counters and status JSON

See [API reference](./api-reference.md) for the route-by-route breakdown.

### `src/wifi.cpp`
Owns network bring-up and fallback provisioning.

Behavior summary:

- tries EEPROM Wi-Fi credentials first
- falls back to `src/wifi_secret.h` credentials when EEPROM is empty
- attempts a normal STA connection with hostname `esp32-imu`
- if connection fails, enters Bluetooth Serial provisioning mode with device name `esp32-imu-config`
- after successful Wi-Fi association, starts mDNS, disables power save, disables Wi-Fi sleep, and sets transmit power to `WIFI_POWER_19_5dBm`

Bluetooth provisioning commands are line-oriented:

- `SSID:<ssid>`
- `PASS:<password>`
- `CONNECT`
- `STATUS`
- `RESET`

`RESET` clears only the persisted Wi-Fi credentials region in EEPROM.

### `src/eeprom_cfg.cpp`
Owns persisted network configuration.

Stored items:

- ESP-NOW peer MAC + channel in a `PersistPeer` struct starting at EEPROM offset `sizeof(PersistPeer)`
- Wi-Fi credentials at EEPROM base `80`

If no persisted peer exists, the firmware falls back to `TARGET_MAC` from `src/target_secret.h` when that macro is defined.

### `src/espnow.cpp`
Owns ESP-NOW peer registration and send accounting.

Important behaviors:

- `addOrUpdatePeer()` removes any existing peer entry and adds the current configured peer
- channel `0` means "use the current Wi-Fi channel"
- send callback increments `g_send_ok` or `g_send_fail`
- display updates are triggered when send totals change
- `initEspNow()` initializes ESP-NOW, registers the send callback, and immediately tries to add the configured peer

### `src/sd_log.cpp`
Owns microSD initialization and logging.

Responsibilities:

- starts SPI on the configured SD pins
- mounts the SD card through `SdFat32`
- creates `/IMU_DATA` if missing
- creates a new CSV file when logging starts
- writes the CSV header
- appends one line per IMU payload
- flushes every 20 appended records and on stop

The SD logger is coupled to streaming: `/stream/start` auto-starts logging when SD is available, and `/stream/stop` auto-stops logging when logging is active.

### `src/timebase.cpp` and `src/util.cpp`
Own shared time utilities.

`src/timebase.cpp`:

- maintains the synchronized epoch/base-millis pair
- computes current epoch milliseconds from that pair
- refreshes the timebase from NTP every 10 seconds
- exposes `g_ntp_offset_ms_ewma`, which is actually the latest observed NTP offset, not a smoothed average

`src/util.cpp`:

- formats UTC and local ISO-8601 timestamps
- converts MAC addresses to/from strings

Local timestamps use a fixed `GMT_OFFSET_SEC` in `include/globals.h`, currently `GMT+7`.

### `src/calibration.h` and `src/calibration.cpp`
Own IMU calibration state and math.

Calibration model:

- six operator-guided positions
- `128` samples per position
- `768` total samples
- ellipsoid fit to estimate offset and `3x3` correction matrix
- persistence to EEPROM starting at offset `24`

Runtime states:

- `CALIB_IDLE`
- `CALIB_COLLECTING`
- `CALIB_FITTING`
- `CALIB_SUCCESS`
- `CALIB_FAILED`

The display task renders dedicated calibration instruction and result pages based on this state machine.

### `src/display.cpp`
Owns OLED rendering.

The display task renders one of two modes:

- **normal status mode**: IP, peer, stream/log state, ESP-NOW send counters, and SD readiness/log record count
- **workflow pages**: startup, Wi-Fi connection, SD init, Bluetooth config, calibration instructions, calibration fitting, calibration success, calibration failure

The normal status page is refreshed on demand or at least once per second.

### `src/wifi_secret.h`
Contains hardcoded Wi-Fi credentials used when EEPROM does not already hold credentials.

This file is sensitive configuration, not documentation. Treat it as local secret material.

### `src/target_secret.h`
Contains the default ESP-NOW peer MAC used when EEPROM does not already hold a saved peer.

This is also sensitive deployment configuration.

### `gcs-src/main.ino`
A companion receiver example, not part of the PlatformIO sender build.

What it does:

- connects to Wi-Fi as a station
- initializes an SSD1306 display on a separate pin mapping (`SDA 21`, `SCL 22`)
- initializes ESP-NOW reception
- registers a hardcoded peer MAC
- parses incoming IMU payloads
- prints parsed JSON to serial
- shows peer/channel/RSSI and the latest IMU data on the OLED

Use it as a lab reference for receiver-side bring-up or packet inspection.

### `docs/openapi.json`
Machine-readable contract for the main control API.

Important note: the source of truth for implemented behavior is `src/http_handlers.cpp`. When the OpenAPI file and code diverge, maintainers should update the spec to match the code, not the other way around.

## Runtime task model

The main firmware creates four pinned FreeRTOS tasks:

- `ntpTask`: refreshes the timebase every 10 seconds
- `imuTask`: reads sensor data, performs calibration work, logs, and transmits
- `httpTask`: services HTTP clients every 5 ms
- `displayTask`: refreshes the OLED based on state changes

Shared resources protected by mutexes:

- `g_sdMutex`: guards log file operations from concurrent access
- `g_netMutex`: guards ESP-NOW send operations

## End-to-end data flow

### Normal streaming path

1. `setup()` initializes hardware and networking.
2. An HTTP caller turns streaming on with `/stream/start` or `/stream/toggle`.
3. `imuTask` reads IMU acceleration, gyro, and temperature.
4. If calibration is valid, acceleration is corrected.
5. The task formats a CSV record.
6. If logging is active, the CSV line is appended to the current SD file.
7. If a peer is configured, the same CSV line is sent by ESP-NOW.
8. Send counters and display state are updated asynchronously.

### Calibration path

1. An HTTP caller triggers `/imu/recalibrate`.
2. Calibration state switches to `CALIB_COLLECTING`.
3. `displayTask` guides the operator through six positions.
4. `imuTask` stores 128 acceleration samples per position.
5. After the sixth position, state switches to `CALIB_FITTING`.
6. `imuTask` computes offset and correction matrix.
7. On success, parameters are saved to EEPROM and state becomes `CALIB_SUCCESS`.
8. `displayTask` shows the success page and returns to normal mode.

## Maintainer notes

- The current top-level `README.md` previously described stale `/target/*` routes that are not implemented in `src/http_handlers.cpp`.
- The previous README also used OLED pin values `21/22`, but the main firmware uses `Wire1` on `25/26`; `21/22` belong to the receiver example in `gcs-src/main.ino`.
- The sender code uses `HTTP_ANY` for all registered routes, even where the OpenAPI file advertises specific verbs.
- The discovery endpoint on port `5681` is implemented in code but is not described in `docs/openapi.json`.
