# API Reference

This guide documents the HTTP interfaces implemented in `src/http_handlers.cpp`.

## Important behavior before the route list

### Actual implementation vs OpenAPI verb declarations

Every route in `src/http_handlers.cpp` is registered with `HTTP_ANY`.

That means the firmware currently accepts any HTTP method for these paths, even where `docs/openapi.json` documents a narrower method such as `GET` or `POST`.

This guide follows the code as implemented. Example calls use the conventional verb for readability.

### Base addresses

Main control server:

- `http://<device-ip>/`
- `http://esp32-imu.local/` when mDNS resolution works on the network

Discovery server:

- `http://<device-ip>:5681/`
- `http://esp32-imu.local:5681/`

### Data plane vs control plane

- The HTTP API is the **control plane**.
- IMU payload streaming itself goes over **ESP-NOW** as CSV text.

## Endpoint inventory

### `GET|POST|ANY /status`

Returns current runtime state in one JSON document.

Example:

```json
{
  "ip": "192.168.1.100",
  "stream": "on",
  "ntp_synced": true,
  "imu_delay_ms": 50,
  "transport": "espnow",
  "peer_mac": "14:08:08:A5:08:1C",
  "peer_channel": 1,
  "send_ok": 1234,
  "send_fail": 3,
  "last_send_status": 0,
  "calibrating": false,
  "latency_ms": 123,
  "loop_dt_avg_ms": 50,
  "jitter_avg_ms": 1,
  "sd_available": true,
  "logging_enabled": true,
  "log_records": 200,
  "log_errors": 0,
  "log_file": "/IMU_DATA/IMU_20251022_143022.csv"
}
```

Behavior notes:

- `peer_channel` resolves channel `0` to the current Wi-Fi channel before returning it.
- `calibrating` is `true` for any state except `CALIB_IDLE`; it does not distinguish collecting vs fitting vs failed.
- `latency_ms` is taken from `g_ntp_offset_ms_ewma`, which the code updates directly from the latest NTP offset sample.

### `POST|ANY /stream/start`

Turns streaming on.

Response:

```json
{"stream":"on"}
```

Behavior notes:

- sets `g_stream_enabled = true`
- if the SD card is available and logging is not already active, it starts a new log file automatically
- does not validate that a peer is configured before enabling streaming

### `POST|ANY /stream/stop`

Turns streaming off.

Response:

```json
{"stream":"off"}
```

Behavior notes:

- sets `g_stream_enabled = false`
- if logging is active, stops logging and closes the SD file

### `POST|ANY /stream/toggle`

Toggles streaming state.

Response examples:

```json
{"stream":"on"}
```

```json
{"stream":"off"}
```

Behavior notes:

- on transition to `on`, it auto-starts logging when SD is available
- on transition to `off`, it auto-stops logging when logging is active

### `GET|POST|ANY /imu/delay`

Gets the configured IMU loop delay only when called with parameters that make the handler succeed, and otherwise acts as the delay-setting endpoint.

Supported query parameters for writes:

- `ms=<integer>`: target interval in milliseconds
- `hz=<number>`: target frequency in hertz

Set by milliseconds:

```text
/imu/delay?ms=20
```

Set by frequency:

```text
/imu/delay?hz=50
```

Success response:

```json
{"imu_delay_ms":20}
```

Error responses:

```json
{"error":"missing 'ms' or 'hz'"}
```

```json
{"error":"hz must be > 0"}
```

Behavior notes:

- the implementation does not provide a dedicated read-only response shape; any successful call returns the current `imu_delay_ms`
- `ms` is clamped to `[5, 1000]`
- `hz` is converted to milliseconds and then clamped to `[5, 1000]`
- if both `hz` and `ms` are present, `hz` wins because the code checks it first

### `POST|ANY /imu/recalibrate`

Starts a new calibration run.

Response example:

```json
{"calibration":"started","state":"collecting"}
```

Behavior notes:

- resets calibration state to collecting position `0`, sample index `0`
- clears the current `valid` flag before new samples are collected
- actual sample collection and fitting happen later in `imuTask`

### `GET|POST|ANY /imu/calib/status`

Returns detailed calibration progress.

Response example:

```json
{
  "state":"collecting",
  "position":2,
  "sample_idx":45,
  "valid":true
}
```

Fields:

- `state`: one of `idle`, `collecting`, `fitting`, `success`, `failed`, `unknown`
- `position`: current position index `0..5`
- `sample_idx`: sample index within the current position
- `valid`: whether the currently stored calibration parameters are valid

Behavior notes:

- `valid` reports the global calibration parameter validity, not whether the in-progress run has already completed
- after the display task processes `CALIB_SUCCESS`, runtime state returns to `CALIB_IDLE`

### `GET|POST|ANY /peer/get`

Returns the currently configured ESP-NOW peer.

Response example:

```json
{
  "peer_mac":"14:08:08:A5:08:1C",
  "peer_channel":1
}
```

Behavior notes:

- channel `0` is resolved to the current Wi-Fi channel before returning it
- an unset peer appears as `00:00:00:00:00:00`

### `POST|ANY /peer/set`

Sets or replaces the ESP-NOW peer.

Required query parameters:

- `mac=<AA:BB:CC:DD:EE:FF>`

Optional query parameters:

- `ch=<0..14>`

Example:

```text
/peer/set?mac=14:08:08:A5:08:1C&ch=1
```

Success response example:

```json
{
  "peer_added":true,
  "saved":true,
  "peer_mac":"14:08:08:A5:08:1C",
  "peer_channel":1
}
```

Error responses:

```json
{"error":"missing mac"}
```

```json
{"error":"invalid mac"}
```

```json
{"error":"invalid channel"}
```

Behavior notes:

- `peer_added` reflects whether `esp_now_add_peer()` succeeded after deleting any previous peer
- `saved` reflects whether EEPROM commit succeeded
- channel `0` is allowed and means "follow current Wi-Fi channel"

### `POST|ANY /peer/reset`

Clears the configured peer and reinitializes ESP-NOW.

Response example:

```json
{"peer_reset":true,"saved":true}
```

Behavior notes:

- zeroes `g_peer_mac`
- resets `g_peer_channel` to `0`
- calls `esp_now_deinit()`, `esp_now_init()`, and re-registers the send callback
- persists the cleared peer state to EEPROM

### `GET|POST|ANY /sd/status`

Returns SD/logging state.

Response example:

```json
{
  "sd_available":true,
  "logging_enabled":true,
  "current_file":"/IMU_DATA/IMU_20251022_143022.csv",
  "records_written":201,
  "write_errors":0
}
```

Behavior notes:

- `records_written` includes the CSV header line because the logger increments the counter after writing the header
- `current_file` is an empty string when no log file is open

### `POST|ANY /sd/start`

Starts a new SD log file.

Success response example:

```json
{
  "logging_started":true,
  "file":"/IMU_DATA/IMU_20251022_143022.csv"
}
```

Error when SD is unavailable:

```json
{"error":"SD not available"}
```

Behavior notes:

- returns HTTP `503` when no SD card is available
- if logging is already active, `startLogging()` first stops the current file and then opens a new one
- writes the CSV header immediately after opening the file

### `POST|ANY /sd/stop`

Stops the current SD log file.

Response example:

```json
{
  "logging_stopped":true,
  "final_records":201,
  "write_errors":0
}
```

Behavior notes:

- currently returns HTTP `200` even if there was no active log file
- `final_records` is the current record counter value at stop time

### `POST|ANY /display/toggle`

Toggles display updates.

Response examples:

```json
{"display_enabled":true}
```

```json
{"display_enabled":false}
```

Behavior notes:

- flips only the runtime flag `g_display_enabled`
- does not power the display hardware down explicitly

### `POST|ANY /net/tune`

Reapplies the Wi-Fi tuning used during startup.

Response:

```json
{"wifi":"tuned"}
```

Behavior notes:

- disables Wi-Fi power save
- disables Wi-Fi sleep
- sets Wi-Fi TX power to `WIFI_POWER_19_5dBm`

### `GET|POST|ANY /` on port `5681`

Discovery endpoint served by `discoveryServer`.

Example URL:

```text
http://esp32-imu.local:5681/
```

Response example:

```json
{
  "device":"imu-fd-now",
  "ip":"192.168.1.100"
}
```

Behavior notes:

- this endpoint is implemented in code but is not currently represented in `docs/openapi.json`
- it is useful for network discovery or simple device presence checks

## Cross-reference to `docs/openapi.json`

Use the OpenAPI file when you need:

- code generation
- schema import into tooling
- a machine-readable contract for the main port-80 API

Known code/spec gaps to keep in mind:

1. The code accepts `HTTP_ANY` for all routes, while the OpenAPI file advertises narrower verbs.
2. The discovery endpoint on port `5681` is implemented but absent from the OpenAPI file.
3. The OpenAPI file describes `GET /imu/delay` as a read operation, but the implementation is a single handler that mainly acts as a setter and errors when no `ms` or `hz` parameter is supplied.
4. The OpenAPI file describes `/sd/status` as `GET`; the implementation accepts any method.
