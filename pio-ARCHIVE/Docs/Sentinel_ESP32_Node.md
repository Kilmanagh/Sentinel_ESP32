# Sentinel ESP32 Node Firmware

## Source and build

- Firmware source: `src/main.cpp`
- PlatformIO environment: `esp32dev`
- Board target: ESP32 Dev Module
- Serial monitor speed: `115200`

## Runtime behavior

The current firmware provides:

- BME280 environmental telemetry
- PIR motion detection on GPIO 27
- Reed switch door monitoring on GPIO 32
- MAX9814 sound intrusion detection on GPIO 34
- MQTT auto-discovery for Home Assistant
- NVS-backed runtime configuration
- BLE watchlist presence tracking using iBeacon `UUID + major + minor`

## Build

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -d "d:\Source Code\ESP32\Sent" -e esp32dev
```

## Flash

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -d "d:\Source Code\ESP32\Sent" -e esp32dev -t upload --upload-port COM6
```

## Serial config commands

Use the serial monitor at `115200` and send:

```text
cfg help
cfg show
cfg set <key> <value>
cfg save
cfg apply
cfg reset
```

Key runtime groups include:

- Wi-Fi, static IP, DNS, NTP, and MQTT settings
- PIR timing settings
- Sound detection settings
- `ble_beaconX_name`, `ble_beaconX_uuid`, `ble_beaconX_major`, `ble_beaconX_minor`

## BLE enrollment commands

If you need to discover a beacon identity before adding it to the watchlist:

```text
ble help
ble enroll on
ble enroll status
ble enroll off
```

While enrollment logging is enabled, the firmware prints entries like:

```text
[BLE][ENROLL] UUID:12345678-1234-1234-1234-1234567890AB Major:100 Minor:7 RSSI:-61
```

## Related docs

- `docs/Adjustments.MD` for tuning and BLE enrollment workflow
- `docs/MQTT_Topic_Map.md` for published topics and payloads
