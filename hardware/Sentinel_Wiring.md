# Sentinel ESP32: Complete Technical Wiring & Hardware Reference

For the current hardware build, use [hardware/Sentinel_Baseplate_Kit_Wiring.md](d:/Source%20Code/ESP32/Sent/hardware/Sentinel_Baseplate_Kit_Wiring.md) as the practical assembly guide when you are wiring the AITRIP baseplate / expansion kit.

This document provides the definitive, pin-to-pin wiring guide for the Sentinel Multi-Sensor Node. It accounts for all shared power/ground rails and the specific Normally Closed (NC) logic for security sensors.

---

## 1. The Common Rail Logic (Power & Ground)

The ESP32 board uses shared rails for the sensor and LED wiring.

- **VCC Rail (3.3V)**: Connect the ESP32 `3V3` pin to a common rail. All sensor VCC pins must jump from this rail.
- **Common Ground (GND)**: Connect one ESP32 `GND` pin to a common rail. All sensor GND pins must jump from this rail.
- **Status LED Rail (5V)**: The external WS2812 status LEDs should use the ESP32 board `5V`/`VIN` rail, not the `3V3` rail.
- **Shared Ground Rule**: The external WS2812 LED chain must share GND with the ESP32 and the sensor ground rail.

When using the photographed baseplate kit, the simplest safe setup is:

- set the baseplate `V` jumper to `3.3V`
- use the baseplate `V` row for the 3.3V sensors only
- use the dedicated `5V` breakout or `VIN` for the WS2812 strip

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
- **External Status LEDs**: `3 x WS2812` chained on **GPIO 25** data
- **LED Power**: `5V` rail
- **LED Ground**: Shared GND rail
- **LED Data Chain**: `GPIO25 -> DIN LED 1 -> DOUT LED 1 -> DIN LED 2 -> DOUT LED 2 -> DIN LED 3`
- **Recommended Protection**: `330-470 ohm` resistor in series with the data line near LED 1, plus `470-1000 uF` capacitor across LED `5V` and `GND`
- **Color Order**: Default firmware assumes `GRB`

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
| **WS2812 LED Chain** | VCC | 5V Rail | Power |
| **WS2812 LED Chain** | GND | GND Rail | Ground |
| **WS2812 LED Chain** | DIN (LED 1) | **GPIO 25** | Addressable Data |
| **WS2812 LED Chain** | DOUT (LED 1) | DIN (LED 2) | Data Pass-through |
| **WS2812 LED Chain** | DOUT (LED 2) | DIN (LED 3) | Data Pass-through |
| **Onboard LED** | Internal LED | **GPIO 2** | Digital Status |

---

## 4. Hardware Logic Summary

- **I2C Bus**: Standard hardware pins for ESP32.
- **Sound**: Uses ADC1 (GPIO 34) because ADC2 cannot be used while WiFi is active.
- **NC Logic**: The door reports "CLOSED" when the switch is LOW (magnet present).
- **Addressable LEDs**: All 3 external status LEDs share one GPIO data pin because WS2812 LEDs are daisy-chained and individually addressed in sequence.
- **Power Separation**: Sensors stay on `3V3`; the external WS2812 chain uses `5V`.
- **Grounding Requirement**: The LED chain will not work reliably unless the LED ground and ESP32 ground are tied together.

---

## 5. Final Expected Connections

| Sentinel function | Connect to | Power source |
| :--- | :--- | :--- |
| BME280 SDA | `D21` | `3.3V` |
| BME280 SCL | `D22` | `3.3V` |
| AM312 PIR OUT | `D27` | `3.3V` |
| MAX9814 OUT | `D34` | `3.3V` |
| MC-38 reed wire 1 | `D32` | none |
| MC-38 reed wire 2 | `GND` | none |
| WS2812 DIN | `D25` through `330-470 ohm` series resistor | n/a |
| WS2812 power | dedicated `5V` breakout or `VIN` | `5V` |
| WS2812 ground | `GND` | common ground |

When using the photographed AITRIP baseplate / expansion kit, keep the baseplate `V` jumper set to `3.3V` so the shared `V` row remains safe for the BME280, PIR, and MAX9814. Do not power the WS2812 strip from that shared `V` row.

---
Created for Hartmann Studio Creations Project Sentinel - May 2026
