# IMU-FD-NOW

ESP32 firmware for IMU acquisition, ESP-NOW streaming, HTTP control, OLED status display, Wi-Fi provisioning, and optional microSD logging.

## What this repository contains

- `src/`: the main sender firmware built with PlatformIO and Arduino
- `gcs-src/main.ino`: a separate receiver / ground-station example sketch
- `docs/openapi.json`: machine-readable control API contract
- `docs/*.md`: maintainer and bring-up documentation for the current implementation

## Start with the docs

- [Documentation index](./docs/README.md)
- [Repository walkthrough](./docs/repository-walkthrough.md)
- [Setup and operation](./docs/setup-and-operation.md)
- [API reference](./docs/api-reference.md)
- [Data and persistence](./docs/data-and-persistence.md)
- [OpenAPI spec](./docs/openapi.json)

## Quick project summary

The main firmware:

- reads an MPU6500 IMU
- shows status on an SSD1306 OLED
- connects to Wi-Fi with Bluetooth Serial fallback provisioning
- advertises HTTP control on port `80`
- serves discovery on port `5681`
- streams CSV IMU payloads over ESP-NOW
- can log the same payloads to `/IMU_DATA/*.csv` on microSD
- supports six-position calibration persisted to EEPROM

## Build entrypoint

PlatformIO configuration is in `platformio.ini`.

Typical commands:

```bash
pio run
pio run -t upload
pio device monitor -b 115200
```

## Current implementation notes

Use the docs under `docs/` instead of older summary material. The current firmware differs from stale earlier descriptions in a few important ways:

- there are no implemented `/target/*` HTTP routes in `src/http_handlers.cpp`
- the main sender firmware OLED uses `Wire1` on pins `25/26`, not `21/22`
- all implemented HTTP routes are registered as `HTTP_ANY`
- the discovery endpoint on port `5681` exists in code even though it is outside the current OpenAPI file

## License

License is not finalized in this repository.
