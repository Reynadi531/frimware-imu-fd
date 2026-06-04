# IMU-FD-NOW Documentation

This documentation set explains the ESP32 firmware in this repository from two angles:

- **Maintainers**: where the code lives, how the runtime is composed, and which files own which behavior.
- **Operators / bring-up engineers**: how to wire the device, build and flash it, provision Wi-Fi, control it over HTTP, and interpret what it stores or transmits.

The repository contains two firmware artifacts:

- `src/`: the main ESP32 IMU sender firmware.
- `gcs-src/main.ino`: a separate ground/control-station example that receives the sender's ESP-NOW payloads.

## Start here

- Need a repository map: [Repository walkthrough](./repository-walkthrough.md)
- Need build, flash, provisioning, and day-1 operation steps: [Setup and operation](./setup-and-operation.md)
- Need endpoint-by-endpoint control details: [API reference](./api-reference.md)
- Need persistence, CSV layout, and runtime counters: [Data and persistence](./data-and-persistence.md)
- Need the machine-readable control contract: [`openapi.json`](./openapi.json)

## Firmware summary

The main firmware does all of the following:

1. Initializes the MPU6500 IMU and SSD1306 OLED.
2. Loads persisted peer, Wi-Fi, and calibration state from EEPROM.
3. Connects to Wi-Fi, falling back to Bluetooth Serial provisioning when connection fails.
4. Brings up ESP-NOW for IMU payload transmission.
5. Starts HTTP control on port `80` and discovery on port `5681`.
6. Optionally logs streamed payloads to `/IMU_DATA/*.csv` on microSD.
7. Runs FreeRTOS tasks for NTP refresh, IMU sampling/transmission, HTTP servicing, and display refresh.

## Which document answers which question?

### I need to bring up hardware for the first time
Read [Setup and operation](./setup-and-operation.md). It covers pin mapping, required secret headers, PlatformIO commands, first boot behavior, and Wi-Fi provisioning.

### I need to understand the code before changing it
Read [Repository walkthrough](./repository-walkthrough.md). It maps each file to a subsystem and explains the boot/runtime data flow.

### I need to integrate a controller or dashboard
Read [API reference](./api-reference.md) and then check [`openapi.json`](./openapi.json). The Markdown guide matches the code in `src/http_handlers.cpp`; the OpenAPI file remains the integration artifact.

### I need to understand what is stored, logged, or transmitted
Read [Data and persistence](./data-and-persistence.md). It documents EEPROM layout, calibration persistence, log file naming, CSV fields, and operational counters.

## Scope notes

- The prose in this docs set is derived from the current firmware implementation, especially `src/main.cpp`, `src/http_handlers.cpp`, `src/wifi.cpp`, `src/imu.cpp`, `src/sd_log.cpp`, `src/calibration.cpp`, and `src/eeprom_cfg.cpp`.
- `docs/openapi.json` is intentionally preserved as the machine-readable API contract.
- The ground-station sketch under `gcs-src/` is a companion example, not part of the main firmware image built by PlatformIO.
