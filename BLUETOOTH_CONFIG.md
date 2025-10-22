# Bluetooth WiFi Configuration

This firmware now supports configuring WiFi credentials via Bluetooth Serial, eliminating the need to recompile and flash the firmware when changing WiFi networks.

## Features

- Set WiFi credentials wirelessly via Bluetooth
- Credentials are stored in EEPROM
- Fallback to default credentials from `wifi_secret.h` if no stored credentials exist
- Easy-to-use command interface
- Automatic connection timeout with helpful error messages

## Setup Instructions

### 1. Connect via Bluetooth

1. Power on the ESP32
2. On your computer/phone, search for Bluetooth devices
3. Connect to **"ESP32-IMU"**

### 2. Available Commands

Once connected via Bluetooth Serial, you can use the following commands:

#### SET_WIFI
Configure WiFi credentials:
```
SET_WIFI:<SSID>:<PASSWORD>
```

**Example:**
```
SET_WIFI:MyHomeNetwork:MyPassword123
```

**Response:**
```
OK: WiFi credentials saved
SSID: MyHomeNetwork
Restart device to apply changes
```

#### GET_WIFI
View current WiFi configuration:
```
GET_WIFI
```

**Response:**
```
Current WiFi Configuration:
SSID: MyHomeNetwork
Password: MyPassword123
Status: Connected
IP: 192.168.1.100
```

#### RESTART
Restart the ESP32 to apply new WiFi settings:
```
RESTART
```

#### HELP or ?
Display help message with all available commands:
```
HELP
```
or
```
?
```

## Usage Examples

### Setting WiFi Credentials

1. Connect to "ESP32-IMU-Config" via Bluetooth
2. Send command: `SET_WIFI:MyNetwork:MyPassword`
3. Wait for confirmation
4. Send command: `RESTART` (or manually reset the device)
5. Device will connect to the new WiFi network

### Troubleshooting Connection Issues

If WiFi connection fails:
1. Connect via Bluetooth
2. Send `GET_WIFI` to verify the stored credentials
3. Update credentials with `SET_WIFI:<SSID>:<PASSWORD>`
4. Restart with `RESTART`

## Technical Details

### EEPROM Storage

- WiFi credentials are stored in EEPROM at offset `sizeof(PersistPeer) * 2`
- Maximum SSID length: 63 characters
- Maximum password length: 63 characters
- Magic number: `0x57494649` ("WIFI")

### Connection Timeout

- WiFi connection timeout: 10 seconds (20 attempts × 500ms)
- On timeout, helpful Bluetooth configuration instructions are displayed

### Default Credentials

If no credentials are stored in EEPROM, the firmware falls back to the hardcoded values in `src/wifi_secret.h`:
```cpp
const static char* WIFI_SSID = "YourDefaultSSID";
const static char* WIFI_PASSWORD = "YourDefaultPassword";
```

## Implementation Files

- `src/bluetooth.cpp` - Bluetooth serial handling and command parsing
- `src/eeprom_cfg.cpp` - WiFi credential storage/retrieval functions
- `src/wifi.cpp` - WiFi connection with EEPROM credential loading
- `include/globals.h` - Global declarations and structures

## Security Note

⚠️ **Warning:** Bluetooth Classic is not encrypted by default. WiFi credentials are transmitted in plain text over Bluetooth. Only use this feature in trusted environments. For production use, consider implementing Bluetooth pairing and encryption.
