# Sentinel ESP32

Sentinel is an ESP32-based multi-sensor node for Home Assistant with MQTT auto-discovery, runtime-tunable detection logic, and a lightweight BLE presence watchlist.

## Current firmware

- Firmware source: `src/main.cpp`
- Build system: PlatformIO (`esp32dev`)
- MQTT base: `sentinel/<device_id>/...`
- Runtime config storage: ESP32 NVS (`sentinel_cfg` namespace)

## Features

- BME280 environment telemetry over MQTT
- AM312 PIR motion detection with warmup, debounce, and hold timing
- MAX9814 sound intrusion detection using peak-to-peak sampling
- Reed switch door state reporting
- Home Assistant MQTT auto-discovery
- Runtime configuration through Home Assistant MQTT entities and serial `cfg` commands
- BLE watchlist presence tracking using iBeacon `UUID + major + minor`
- Serial iBeacon enrollment logging with `ble enroll on`

## Primary docs

- `docs/Sentinel_ESP32_Node.md` - firmware overview, build/flash, serial config, BLE enrollment
- `docs/MQTT_Topic_Map.md` - MQTT state, config, and discovery topics
- `docs/Adjustments.MD` - PIR, sound, and BLE watchlist tuning guide
- `docs/Sentinel_Full_Project_Reference.md` - high-level hardware, firmware, and integration reference
- `docs/Hardware_Deliverables.md` - hardware package guidance

## Build and flash

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -d "d:\Source Code\ESP32\Sent" -e esp32dev
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -d "d:\Source Code\ESP32\Sent" -e esp32dev -t upload --upload-port COM6
```

Adjust `COM6` to the active ESP32 port if needed.
