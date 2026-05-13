# Sentinel ESP32 MQTT Topic Map

This document maps the MQTT topics and JSON structures used by the Sentinel ESP32 Node for communication with Home Assistant.

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
  - `intrusion`: string ("detected")
  - `peak_adc`: int (Raw ADC value 0-4095)

### 4. Door (Reed Switch)
**Topic:** `sentinel/<device_id>/door`
- **Payload:** JSON
- **Fields:**
  - `state`: string ("OPEN" or "CLOSED")
  - `raw`: int (0 for CLOSED, 1 for OPEN — logic adjusted for NC switch)

### 5. BLE Presence
**Topic:** `sentinel/<device_id>/ble`
- **Payload:** JSON
- **Frequency:** Every 2 minutes
- **Fields:**
  - `device_count`: int (Number of local BLE devices found)

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
  - `free_heap`: int (Available RAM in bytes)
  - `rssi`: int (WiFi signal strength in dBm)

## Home Assistant Auto-Discovery
**Topics:** `homeassistant/<component>/<device_id>/<object_id>/config`
- These topics are published once at boot to automatically configure entities in Home Assistant.
