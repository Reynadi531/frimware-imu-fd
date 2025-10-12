# ESP32 IMU → ESP-NOW Sender

This project streams IMU data from an ESP32 over **ESP-NOW**, shows status on an **SSD1306 128×64** OLED, and (optionally) logs data to **microSD** using **SdFat**. It also exposes simple **HTTP endpoints** to start/stop streaming, set peer MAC, recalibrate, and adjust IMU rate.

## Features

- **IMU (MPU6500)** sampling at a configurable rate (default 50 ms)
- **ESP-NOW** unicast payloads (CSV)
- **CSV logging to microSD (SdFat)** with one file per session
- **SSD1306 (Adafruit GFX)** status display
- **HTTP control** (on port 80):
  - `/stream/start`, `/stream/stop`, `/stream/toggle`
  - `/status` – JSON device status
  - `/imu/recalibrate`
  - `/imu/delay?ms=50` or `?hz=20`
  - `/peer/get`, `/peer/set?mac=AA:BB:CC:DD:EE:FF[&ch=1]`, `/peer/reset`
  - `/sd/status`, `/sd/start`, `/sd/stop`
  - `/target/*` for legacy UDP (optional)
- **Discovery Server** at port 5681 with HTTP return

---

## Hardware

- **ESP32** dev board (ESP-WROOM-32, ESP32-S3, etc.)
- **MPU6500** IMU (I²C `0x68`)
- **SSD1306 128×64** OLED (I²C)  
  - SDA = **21**, SCL = **22**
- **microSD** via SPI (SdFat v2)  
  - CS = 5, MOSI = 23, MISO = 19, SCK = 18

---

## Software / Libraries

- **PlatformIO** (Arduino framework)
- Arduino core for ESP32
- Libraries:
  - [`MPU6500_WE`](https://github.com/wollewald/MPU6500_WE)
  - [`Adafruit GFX Library`](https://github.com/adafruit/Adafruit-GFX-Library)
  - [`Adafruit SSD1306`](https://github.com/adafruit/Adafruit_SSD1306)
  - [`SdFat`](https://github.com/greiman/SdFat) 
  - [`NTPClient`](https://github.com/arduino-libraries/NTPClient)

## Project Structure
```bash
.
├── README.md
├── include
│   ├── README
│   └── globals.h
├── lib
├── platformio.ini
├── src
│   ├── display.cpp
│   ├── eeprom_cfg.cpp
│   ├── espnow.cpp
│   ├── http_handlers.cpp
│   ├── imu.cpp
│   ├── main.cpp
│   ├── sd_log.cpp
│   ├── target_secret.h
│   ├── timebase.cpp
│   ├── util.cpp
│   ├── wifi.cpp
│   └── wifi_secret.h
└── test
    └── README

4 directories, 17 files
```

## Build & Flash
1. Set Wi-Fi credentials in /include/wifi_secret.h or /src/wifi_secret.h :
```cpp
#define WIFI_SSID "your-ssid"
#define WIFI_PASSWORD "your-pass"
```

Add /include/target_secret.h or /src/target_secret.h:
```cpp
#define TARGET_MAC "your-peer-esp-now-reciever-mac"
```

2. Flash and monitor:
```bash
pio run -t upload
pio device monitor -b 115200
```

3. On boot you should see:

```bash
MPU connected & calibrated

Wi-Fi STA connected

ESP-NOW init OK

HTTP server started

SD initialized
```

## LICENSE
On going license decisions. Under private license

2025 @ Team Hardware Fall Detection Research, Telkom University
