# Sentinel ESP32: Complete Technical Wiring & Hardware Reference

This document provides the definitive, pin-to-pin wiring guide for the Sentinel Multi-Sensor Node. It accounts for all shared power/ground rails and the specific Normally Closed (NC) logic for security sensors.

---

## 1. The Common Rail Logic (Power & Ground)
The ESP32 DevKit has limited GND and 3.3V pins. To connect all five sensors, you must create a **Shared Rail** (Bus).
- **VCC Rail (3.3V)**: Connect the ESP32 `3V3` pin to a common rail. All sensor VCC pins must jump from this rail.
- **Common Ground (GND)**: Connect one ESP32 `GND` pin to a common rail. All sensor GND pins must jump from this rail.

---

## 2. Component Pin Mapping

### A. Environmental Sensor (BME280)
- **VCC**: Shared 3.3V Rail
- **GND**: Shared GND Rail
- **SDA**: **GPIO 21**
- **SCL**: **GPIO 22**

### B. Motion Detector (AM312 Mini PIR)
- **VCC**: Shared 3.3V Rail
- **GND**: Shared GND Rail
- **OUT**: **GPIO 27** (Center Pin on most AM312 modules)

### C. Acoustic Sensor (MAX9814 Microphone)
- **VCC**: Shared 3.3V Rail
- **GND**: Shared GND Rail
- **OUT**: **GPIO 34** (Analog Input)
- **GAIN**: Leave floating for 60dB or tie to GND for 50dB.

### D. Security Sensor (MC-38 Reed Switch - Normally Closed)
- **Wire 1**: **GPIO 32**
- **Wire 2**: Shared GND Rail
- *Note: Firmware uses `INPUT_PULLUP`. Circuit is CLOSED (LOW) when magnet is present.*

### E. Status Indicator
- **Onboard LED**: Internal to **GPIO 2**

---

## 3. Detailed Wiring Master Table

| Hardware Component | Component Pin | ESP32 GPIO / Pin | Signal Type |
| :--- | :--- | :--- | :--- |
| **BME280** | VCC | 3V3 Rail | Power |
| **BME280** | GND | GND Rail | Ground |
| **BME280** | SDA | **GPIO 21** | I2C Data |
| **BME280** | SCL | **GPIO 22** | I2C Clock |
| **AM312 PIR** | VCC | 3V3 Rail | Power |
| **AM312 PIR** | GND | GND Rail | Ground |
| **AM312 PIR** | OUT | **GPIO 27** | Digital Input |
| **MAX9814** | VCC | 3V3 Rail | Power |
| **MAX9814** | GND | GND Rail | Ground |
| **MAX9814** | OUT | **GPIO 34** | Analog Input (ADC1) |
| **MC-38 Switch** | Terminal 1 | **GPIO 32** | Digital (Pull-up) |
| **MC-38 Switch** | Terminal 2 | GND Rail | Ground |

---

## 4. Hardware Logic Summary
- **I2C Bus**: Standard hardware pins for ESP32.
- **Sound**: Uses ADC1 (GPIO 34) because ADC2 cannot be used while WiFi is active.
- **NC Logic**: The door reports "CLOSED" when the switch is LOW (magnet present).

---
*Created for Hartmann Studio Creations Project Sentinel - May 2026*
