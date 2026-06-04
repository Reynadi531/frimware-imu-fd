# Setup and Operation

This guide is for first bring-up, reflashing, and day-to-day operation of the main ESP32 IMU sender firmware.

## What you are bringing up

The main firmware runs on an ESP32 and combines these subsystems:

- MPU6500 IMU over I2C
- SSD1306 OLED over a second I2C bus (`Wire1`)
- Wi-Fi station mode with mDNS hostname `esp32-imu`
- ESP-NOW transmission of IMU payloads
- HTTP control API on port `80`
- discovery endpoint on port `5681`
- optional microSD logging through `SdFat`
- Bluetooth Serial fallback provisioning when Wi-Fi cannot connect

## Hardware prerequisites

Minimum hardware for the main firmware:

- ESP32 board compatible with the PlatformIO target `esp32doit-devkit-v1`
- MPU6500 IMU at I2C address `0x68`
- SSD1306 `128x64` OLED at I2C address `0x3C`
- optional microSD card wired for SPI logging
- Wi-Fi network reachable by the ESP32
- an ESP-NOW receiver, or at least a known peer MAC, if streaming off-device is required

## Pin mapping used by the main firmware

Pin assignments come from `include/globals.h` and `src/main.cpp`.

### IMU bus

The firmware calls `Wire.begin()` with no explicit pins, so the IMU uses the board's default I2C pins for the selected ESP32 board definition.

- I2C address: `0x68`
- bus object: `Wire`

Because the pins are not hardcoded in this repository, confirm the board's default SDA/SCL pins in your hardware setup.

### OLED bus

The OLED uses a separate I2C bus:

- SDA: `25`
- SCL: `26`
- I2C address: `0x3C`
- bus object: `Wire1`
- resolution: `128x64`

### microSD SPI bus

- CS: `5`
- MOSI: `23`
- MISO: `19`
- SCK: `18`

## Required local configuration files

The firmware includes two local configuration headers:

- `src/wifi_secret.h`
- `src/target_secret.h`

### `src/wifi_secret.h`
Provides fallback Wi-Fi credentials used only when EEPROM does not already contain saved credentials.

Expected shape:

```cpp
static char WIFI_SSID[32] = "your-ssid";
static char WIFI_PASSWORD[64] = "your-password";
```

### `src/target_secret.h`
Provides the default ESP-NOW peer MAC used when EEPROM does not already contain a saved peer.

Expected shape:

```cpp
#define TARGET_MAC "AA:BB:CC:DD:EE:FF"
```

## Build environment

PlatformIO configuration is in `platformio.ini`.

Main environment:

- environment: `esp32doit-devkit-v1`
- platform: `espressif32`
- framework: Arduino

Library dependencies declared by the project:

- `MPU9250_WE` / `MPU6500_WE` family driver dependency
- `SdFat - Adafruit Fork`
- `NTPClient`
- `Adafruit GFX Library`
- `Adafruit SSD1306`

## Build, flash, and monitor

From the repository root:

```bash
pio run
pio run -t upload
pio device monitor -b 115200
```

If you need to target the named environment explicitly:

```bash
pio run -e esp32doit-devkit-v1
pio run -e esp32doit-devkit-v1 -t upload
```

## Boot sequence and what to expect

The main boot order is fixed in `src/main.cpp`:

1. Serial starts at `115200`.
2. I2C buses come up.
3. The OLED shows a startup page.
4. The MPU6500 is initialized and configured.
5. EEPROM is opened.
6. Persisted peer information is loaded.
7. Saved calibration is loaded if present.
8. Wi-Fi connection is attempted.
9. microSD is initialized.
10. ESP-NOW is initialized.
11. HTTP routes are registered and both servers start.
12. NTP timebase is initialized.
13. FreeRTOS tasks are created.

Typical serial messages include combinations of:

- `MPU6500 connected`
- `Calibration loaded from EEPROM`
- `No calibration data - use /imu/recalibrate`
- `Using WiFi creds from EEPROM: "..."`
- `Using hardcoded WiFi creds: "..."`
- `Connecting to WiFi "..."`
- `WiFi connection timeout`
- `Entered BT config mode: esp32-imu-config`
- `IP: ...`
- `mDNS responder started`
- `ESP-NOW peer ... ch=...`
- `ESP-NOW ready; set peer via /peer/set?mac=AA:BB:CC:DD:EE:FF`
- `HTTP ready`
- `All tasks started`

## Wi-Fi provisioning paths

The device has three ways to obtain Wi-Fi credentials.

### 1. EEPROM-backed credentials

Preferred path after first successful field provisioning.

- `connectWiFi()` first calls `loadWiFiFromEEPROM()`.
- If EEPROM contains valid credentials, they are used immediately.
- This is the path used after Bluetooth provisioning succeeds and saves credentials.

### 2. Hardcoded credentials in `src/wifi_secret.h`

Fallback path when EEPROM does not contain valid credentials.

- On first boot in a lab environment, this is usually the fastest path.
- These credentials remain in source unless you remove or replace them locally.

### 3. Bluetooth Serial fallback provisioning

Used when the selected credentials fail to connect within the timeout.

Behavior:

- Bluetooth device name: `esp32-imu-config`
- OLED enters `BT Config Mode`
- serial prints `Entered BT config mode: esp32-imu-config`
- Bluetooth client receives JSON-like status lines

Supported Bluetooth commands:

- `SSID:<ssid>`
- `PASS:<password>`
- `CONNECT`
- `STATUS`
- `RESET`

Notes:

- `CONNECT` is rejected until an SSID is set.
- On successful Bluetooth provisioning, the firmware saves the credentials to EEPROM and exits Bluetooth mode.
- `RESET` clears the persisted Wi-Fi credentials region so the next boot falls back to hardcoded credentials unless new values are provisioned.

## Network identity after bring-up

After Wi-Fi connects successfully, the firmware:

- keeps the ESP32 in station mode
- sets hostname `esp32-imu`
- starts mDNS with the same hostname
- disables Wi-Fi power save
- disables Wi-Fi sleep
- sets transmit power to `WIFI_POWER_19_5dBm`

In a network that supports mDNS, the device should be reachable as:

```text
http://esp32-imu.local/
```

The discovery endpoint uses the same network identity on port `5681`.

## First operational checks

After boot, verify these in order.

### Check status

```bash
curl http://esp32-imu.local/status
```

or by IP:

```bash
curl http://<device-ip>/status
```

Confirm:

- `ip` is populated
- `transport` is `espnow`
- `imu_delay_ms` matches expectation
- `sd_available` reflects whether a card is inserted and mounted
- `peer_mac` is either the configured peer or `00:00:00:00:00:00`

### Check peer configuration

```bash
curl http://esp32-imu.local/peer/get
```

If the peer is wrong or unset:

```bash
curl -X POST "http://esp32-imu.local/peer/set?mac=AA:BB:CC:DD:EE:FF&ch=1"
```

Channel `0` or omitted channel means "use current Wi-Fi channel" behavior in the firmware.

### Start streaming

```bash
curl -X POST http://esp32-imu.local/stream/start
```

Effects:

- sets `g_stream_enabled = true`
- if SD is available and logging is not already active, starts a new log file automatically
- IMU task begins sending CSV payloads to the configured ESP-NOW peer

### Stop streaming

```bash
curl -X POST http://esp32-imu.local/stream/stop
```

Effects:

- clears `g_stream_enabled`
- stops SD logging if it is currently active

## Calibration workflow

Trigger calibration:

```bash
curl -X POST http://esp32-imu.local/imu/recalibrate
```

Then monitor progress:

```bash
curl http://esp32-imu.local/imu/calib/status
```

What the operator should expect:

- the OLED switches from normal status to calibration instruction pages
- the firmware collects `128` accelerometer samples for each of `6` positions
- after all positions are captured, the display shows a fitting page
- on success, calibration parameters are saved to EEPROM and later loads will apply them automatically

The display uses these step descriptions during collection:

- `X+ UP`
- `X+ DN`
- `Y+ UP`
- `Y+ DN`
- `FLAT`
- `FLIP`

If calibration fails, the OLED shows a failure page and the HTTP status route still reports the calibration state.

## SD logging workflow

### Automatic path

`/stream/start` will automatically start SD logging when the SD card is mounted and logging is not already active.

### Manual path

Start logging directly:

```bash
curl -X POST http://esp32-imu.local/sd/start
```

Stop logging directly:

```bash
curl -X POST http://esp32-imu.local/sd/stop
```

Check logging status:

```bash
curl http://esp32-imu.local/sd/status
```

Log files are created under `/IMU_DATA/`.

## OLED behavior during operation

### Normal status page

The default page shows:

- assigned IP address
- shortened peer MAC or `None`
- stream state
- SD/log state
- ESP-NOW transmit success/failure totals when a peer is set
- log record count while logging is active

### State pages

The OLED temporarily switches to dedicated pages for:

- startup
- Wi-Fi connecting / success / failure
- SD init success / failure
- Bluetooth config mode
- calibration instructions
- calibration fitting
- calibration success / failure

### Display toggle behavior

`/display/toggle` only flips the `g_display_enabled` runtime flag. It stops periodic UI updates, but it does not deinitialize the OLED or advertise a deeper power-management mode.

## Serial behavior during operation

Serial is primarily a bring-up and diagnostics channel.

You will see:

- connection and timeout messages during Wi-Fi association
- ESP-NOW initialization status
- SD open failures if logging cannot start
- task startup confirmation

The main sender firmware does not stream IMU samples to serial in normal operation.

## Ground station / receiver example

`gcs-src/main.ino` is a separate example sketch for a receiver/display node.

Use it when you need to:

- confirm packets are arriving over ESP-NOW
- inspect parsed values on a second OLED
- print the received payload as JSON over serial

Do not treat it as part of the sender firmware image built by `platformio.ini`.

## Bring-up checklist

1. Verify IMU wiring on the board's default I2C pins.
2. Verify OLED wiring on `SDA 25`, `SCL 26`.
3. Verify optional SD wiring on `CS 5`, `MOSI 23`, `MISO 19`, `SCK 18`.
4. Set `src/wifi_secret.h` and `src/target_secret.h` for your lab if EEPROM is blank.
5. Flash and open serial monitor at `115200`.
6. Confirm Wi-Fi association and note the device IP.
7. Call `/status` and `/peer/get`.
8. Set the peer if needed.
9. Start streaming.
10. Confirm the receiver or ground-station sketch sees packets.
11. If calibration is required, run `/imu/recalibrate` before collecting field data.
