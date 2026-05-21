# Sentinel ESP32 Project Reference

This is the high-level reference for the current Sentinel ESP32 firmware. Use it as the entry point for hardware, MQTT, and runtime configuration, then follow the linked docs for detail.

## Firmware summary

- Source file: `src/main.cpp`
- Build system: PlatformIO
- Board target: `esp32dev`
- Home Assistant integration: MQTT auto-discovery
- Runtime configuration: ESP32 NVS + MQTT config topics + serial `cfg` commands
- BLE presence model: fixed 4-slot iBeacon watchlist using `UUID + major + minor`

## Hardware summary

### Bill of materials

- ESP-WROOM-32 / ESP32 dev board
- BME280 environmental sensor
- AM312 PIR motion sensor
- MAX9814 microphone amplifier
- MC-38 reed switch
- Onboard status LED

### Pin mapping

| Hardware Component | ESP32 GPIO | Firmware Variable | Notes |
| :--- | :--- | :--- | :--- |
| BME280 SDA | 21 | `BME_SDA` | I2C data |
| BME280 SCL | 22 | `BME_SCL` | I2C clock |
| AM312 PIR OUT | 27 | `PIN_PIR` | Digital input |
| MC-38 Reed Signal | 32 | `PIN_DOOR` | NC switch with internal pull-up |
| MAX9814 OUT | 34 | `PIN_SOUND` | Analog input |
| Status LED | 2 | `PIN_LED` | Digital output |

## MQTT summary

State topics use:

- `sentinel/<device_id>/environment`
- `sentinel/<device_id>/motion`
- `sentinel/<device_id>/sound`
- `sentinel/<device_id>/door`
- `sentinel/<device_id>/ble_watchlist`
- `sentinel/<device_id>/diagnostics`
- `sentinel/<device_id>/status`

Runtime config topics use:

- `sentinel/<device_id>/config/<key>/state`
- `sentinel/<device_id>/config/<key>/set`

See `docs/MQTT_Topic_Map.md` for payload details.

## Runtime adjustment summary

Runtime-tunable groups include:

- Wi-Fi, static IP, DNS, NTP, and MQTT defaults
- PIR warmup, detect debounce, clear debounce, and hold time
- Sound sample window, hold time, and threshold
- BLE beacon slot name, UUID, major, and minor

Adjustment methods:

- Home Assistant MQTT config entities
- Serial `cfg` commands
- Serial BLE enrollment commands: `ble help`, `ble enroll on`, `ble enroll off`

See `docs/Adjustments.MD` for operator workflow.

## Related docs

- `docs/Sentinel_ESP32_Node.md` for firmware build/flash and serial usage
- `docs/MQTT_Topic_Map.md` for MQTT payloads and config topics
- `docs/Adjustments.MD` for tuning and beacon enrollment
- `docs/Sentinel_Home_Assistant_Helpers.md` for HA-side counters, mode helpers, and last-event tracking
- `docs/Hardware_Deliverables.md` for hardware deliverables
