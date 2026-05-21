# Sentinel ESP32 MQTT Topic Map

This document maps the MQTT topics and JSON structures used by the Sentinel ESP32 Node for communication with Home Assistant.

## Quick Start

Use this section when commissioning a node and you only need the most common topics.

### Core state topics

- `sentinel/<device_id>/status` - online/offline LWT state
- `sentinel/<device_id>/environment` - BME280 telemetry
- `sentinel/<device_id>/motion` - PIR motion state
- `sentinel/<device_id>/sound` - sound intrusion state and peak-to-peak ADC data
- `sentinel/<device_id>/door` - reed switch state
- `sentinel/<device_id>/ble_watchlist` - iBeacon watchlist presence summary and per-slot state
- `sentinel/<device_id>/diagnostics` - uptime, heap, RSSI, reconnect counters, IP, firmware

### Most-used runtime config topics

- `sentinel/<device_id>/config/pir_detect_stable_ms/set`
- `sentinel/<device_id>/config/pir_clear_stable_ms/set`
- `sentinel/<device_id>/config/pir_hold_ms/set`
- `sentinel/<device_id>/config/sound_threshold_adc/set`
- `sentinel/<device_id>/config/sound_sample_window_ms/set`
- `sentinel/<device_id>/config/sound_hold_ms/set`
- `sentinel/<device_id>/config/ble_beacon1_uuid/set`
- `sentinel/<device_id>/config/ble_beacon1_major/set`
- `sentinel/<device_id>/config/ble_beacon1_minor/set`

Each config key also has a matching `/state` topic.

### Home Assistant discovery pattern

- `homeassistant/<component>/<device_id>/<object_id>/config`

Discovery is published at boot and when runtime config discovery is refreshed.

## Topic Structure
All topics follow the base format: `sentinel/<device_id>/<subtopic>`
- `<device_id>` is the unique 12-character MAC address in lowercase (e.g., `4c11ae0d7604`).

## Sensors & Telemetry

### 1. Environment
**Topic:** `sentinel/<device_id>/environment`
- **Payload:** JSON
- **Frequency:** Every 30 seconds
- **Fields:**
  - `temperature_f`: float (Temperature in Fahrenheit)
  - `humidity`: float (Relative humidity percentage)
  - `pressure_hpa`: float (Atmospheric pressure in hPa)
  - `comfort_index`: float (Calculated comfort score 0-100)
  - `comfort_label`: string (e.g., "Excellent", "Good")
  - `iaq_score`: int (Indoor Air Quality score 0-100)
  - `iaq_label`: string (e.g., "Excellent", "Moderate")

### 2. Motion (PIR)
**Topic:** `sentinel/<device_id>/motion`
- **Payload:** JSON
- **Fields:**
  - `motion`: string ("detected" or "clear")

### 3. Sound (Intrusion)
**Topic:** `sentinel/<device_id>/sound`
- **Payload:** JSON
- **Fields:**
  - `intrusion`: string (`"detected"` or `"clear"`)
  - `peak_adc`: int (Peak-to-peak ADC span for the sample window)
  - `min_adc`: int (Lowest ADC sample seen in the window)
  - `max_adc`: int (Highest ADC sample seen in the window)

### 4. Door (Reed Switch)
**Topic:** `sentinel/<device_id>/door`
- **Payload:** JSON
- **Fields:**
  - `state`: string ("OPEN" or "CLOSED")
  - `raw`: int (0 for CLOSED, 1 for OPEN — logic adjusted for NC switch)

### 5. BLE Watchlist Presence
**Topic:** `sentinel/<device_id>/ble_watchlist`
- **Payload:** JSON
- **Frequency:** Every 2 minutes
- **Fields:**
  - `configured_count`: int (Configured watchlist slots)
  - `present_count`: int (Configured slots currently present)
  - `beacon1_name` ... `beacon4_name`
  - `beacon1_uuid` ... `beacon4_uuid`
  - `beacon1_major` ... `beacon4_major`
  - `beacon1_minor` ... `beacon4_minor`
  - `beacon1_present` ... `beacon4_present` (`home`, `away`, or `unconfigured`)
  - `beacon1_rssi` ... `beacon4_rssi`

## System & Diagnostics

### Status (LWT)
**Topic:** `sentinel/<device_id>/status`
- **Payload:** string ("online" or "offline")
- **Note:** Utilizes MQTT Last Will and Testament for disconnect detection.

### Diagnostics
**Topic:** `sentinel/<device_id>/diagnostics`
- **Payload:** JSON
- **Frequency:** Every 60 seconds
- **Fields:**
  - `uptime_sec`: int (Seconds since boot)
  - `uptime_fmt`: string (Human-readable uptime)
  - `free_heap`: int (Available RAM in bytes)
  - `min_free_heap`: int (Lowest observed free heap)
  - `rssi`: int (WiFi signal strength in dBm)
  - `wifi_reconnects`: int
  - `mqtt_reconnects`: int
  - `ip_address`: string
  - `mac`: string
  - `bme280_ok`: bool
  - `firmware`: string

## Runtime Config Topics

Runtime-tunable settings are exposed under:

- `sentinel/<device_id>/config/<key>/state`
- `sentinel/<device_id>/config/<key>/set`

Examples:

- `sentinel/<device_id>/config/pir_detect_stable_ms/state`
- `sentinel/<device_id>/config/sound_threshold_adc/set`
- `sentinel/<device_id>/config/ble_beacon1_uuid/state`
- `sentinel/<device_id>/config/ble_beacon1_major/set`
- `sentinel/<device_id>/config/ble_beacon1_minor/set`

## Home Assistant Auto-Discovery
**Topics:** `homeassistant/<component>/<device_id>/<object_id>/config`
- These topics are published at boot and whenever runtime config discovery is refreshed.
