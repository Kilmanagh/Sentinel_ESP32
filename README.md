# Sentinel ESP32

Sentinel is an ESP32-based multi-sensor node built around ESPHome and Home Assistant.

The active implementation lives in `esphome/sentinel.yaml`. The old PlatformIO project in `pio-ARCHIVE/` is deprecated and is no longer the source of truth.

## Current firmware

- Active config: `esphome/sentinel.yaml`
- Secrets file: `esphome/secrets.yaml`
- Board target: `esp32dev`
- Framework: ESPHome on Arduino
- Home Assistant integration: native ESPHome API
- Device services: Wi-Fi, OTA, web server, local fallback AP, and Home Assistant entity-based runtime tuning

## Features

- BME280 environment telemetry
- AM312 PIR motion detection with warmup, stable-detect, stable-clear, and hold timing
- MAX9814 sound intrusion detection using peak-to-peak sampling
- Reed switch door state reporting
- Sound-level calibration helpers and approximate dB display
- Smoke alarm tone and cadence detection
- Device-health diagnostics and fault indicators
- Three-pixel WS2812 status LED strip plus onboard status LED
- Runtime tuning through Home Assistant entities exposed by ESPHome

## Primary docs

- `docs/Adjustments.MD` - active ESPHome tuning, calibration, and sensor-behavior guide
- `docs/Hardware_Deliverables.md` - current hardware, pin mapping, and LED wiring summary
- `docs/Sentinel_Home_Assistant_Helpers.md` - optional Home Assistant helper package for counters, mode helpers, and last-event tracking
- `docs/Sentinel_Lovelace_Dashboard.md` - dashboard layout, setup, and troubleshooting view
- `docs/Sentinel_Lovelace_Dashboard.yaml` - Lovelace YAML for the dashboard itself

## Deploy

Use ESPHome to validate and install `esphome/sentinel.yaml`.

Typical workflow:

1. Fill in `esphome/secrets.yaml` with your Wi-Fi credentials.
2. Open `esphome/sentinel.yaml` in ESPHome or the Home Assistant ESPHome add-on.
3. Validate the config.
4. Install over USB for first flash, then use OTA afterward if desired.

## Notes

- The active docs no longer use the old MQTT, serial `cfg`, or BLE watchlist workflow.
- If you need historical PlatformIO material, keep it isolated under `pio-ARCHIVE/`.
