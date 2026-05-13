# Sentinel ESP32 Project: Complete Documentation Package

This document contains all the necessary technical specifications, topic maps, and calibration guides for the Sentinel ESP32 Multi-Sensor Node.

---

## 1. Hardware Deliverables
This section details the physical components and pin mapping.

### Bill of Materials (BOM)
- **Microcontroller**: ESP-WROOM-32 / ESP-32S Development Board.
- **Environmental Sensor**: BME280 (Supports 3.3V, I2C interface).
- **Motion Sensor**: AM312 Mini PIR Sensor (Supports 2.7V - 12V; powered via 3.3V).
- **Acoustic Sensor**: MAX9814 Microphone Amplifier (Analog output connected to GPIO 34).
- **Security Sensor**: MC-38 Magnetic Reed Switch (**Normally Closed**, connected to GPIO 32).
- **Status Indicator**: Onboard LED (GPIO 2).

### Hardware Pin Mapping
| Hardware Component | Component Pin | ESP32 GPIO | Firmware Variable | Pin Type |
| :--- | :--- | :--- | :--- | :--- |
| **BME280 Sensor** | SDA | **GPIO 21** | `BME_SDA` | I2C Data |
| **BME280 Sensor** | SCL | **GPIO 22** | `BME_SCL` | I2C Clock |
| **AM312 PIR** | Out | **GPIO 27** | `PIN_PIR` | Digital Input |
| **MC-38 Reed Switch (NC)**| Signal | **GPIO 32** | `PIN_DOOR` | Digital Input (Internal Pull-up) |
| **MAX9814 Microphone** | Out | **GPIO 34** | `PIN_SOUND` | Analog Input |
| **Status LED** | Onboard | **GPIO 2** | `PIN_LED` | Digital Output |

---

## 2. MQTT Topic Map
This section details the communication protocols for Home Assistant.

### Purpose: Discovery (Home Assistant)
**Topic:** `homeassistant/<component>/<device_id>/<object_id>/config`
- Published once at boot to automatically configure entities.

### Purpose: Telemetry & State (Operational Data)
All state topics follow the format: `sentinel/<device_id>/<subtopic>`

| Purpose | Subtopic | Payload Fields |
| :--- | :--- | :--- |
| **Environmental** | `/environment` | `temperature_f`, `humidity`, `pressure_hpa`, `iaq_score` |
| **Security: Motion** | `/motion` | `motion` ("detected" or "clear") |
| **Security: Sound** | `/sound` | `intrusion` ("detected"), `peak_adc` (int) |
| **Security: Door** | `/door` | `state` ("OPEN" or "CLOSED"), `raw` (0 or 1) |
| **System Status** | `/status` | "online" or "offline" (LWT) |
| **Presence** | `/ble` | `device_count` (int) |
| **Diagnostics** | `/diagnostics` | `uptime_sec`, `free_heap`, `rssi` |

---

## 3. Calibration Guide
Technical instructions for tuning the sensors.

### Acoustic Sensor (MAX9814)
- **Configuration**: `#define SOUND_THRESHOLD 1500`
- **Steps**: Monitor peak ADC in Serial Monitor (115200). Set threshold 20% above quiet room noise.

### Motion Sensor (AM312 PIR)
- **Configuration**: `#define PIR_COOLDOWN 10000`
- **Steps**: Increase cooldown to prevent redundant triggers. Use physical masking (tape) on the lens to narrow field of view.

### Environmental Offset (BME280)
- **Steps**: If ESP32 heat causes temperature drift, add a math correction:
  - `float tempF = ((bme.readTemperature() * 9.0 / 5.0) + 32.0) - offset;`

---

## 4. Electrical Schematic (Netlist Style)
For use in PCB design software (KiCad/EasyEDA).

- **Net: 3.3V** -> ESP32(3V3), BME280(VCC), AM312(VCC), MAX9814(VCC)
- **Net: GND** -> ESP32(GND), BME280(GND), AM312(GND), MAX9814(GND), MC-38(Pin 2)
- **Net: SDA** -> ESP32(GPIO 21), BME280(SDA)
- **Net: SCL** -> ESP32(GPIO 22), BME280(SCL)
- **Net: PIR** -> ESP32(GPIO 27), AM312(OUT)
- **Net: SOUND** -> ESP32(GPIO 34), MAX9814(OUT)
- **Net: DOOR** -> ESP32(GPIO 32), MC-38(Pin 1)

---
*End of Documentation Package.*
