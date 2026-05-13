# Sentinel ESP32 Node Firmware

## Source file
- `Sentinel_ESP32_Node.ino`

## Features
- Reads analog telemetry from GPIO34 every 2 seconds.
- Emits JSON telemetry over serial at 115200 baud.
- Drives status LED (GPIO2) when sensor crosses alert threshold.
- Emits a heartbeat line every 30 seconds.

## Build/flash (Arduino IDE)
1. Open `Sentinel_ESP32_Node.ino`.
2. Select **ESP32 Dev Module** (or equivalent ESP32 board profile).
3. Select the correct serial port.
4. Build and upload.

## Serial output example
```json
{"node_id":"sentinel-node-01","sensor_raw":1234,"sensor_voltage":0.994,"alert":false}
```
