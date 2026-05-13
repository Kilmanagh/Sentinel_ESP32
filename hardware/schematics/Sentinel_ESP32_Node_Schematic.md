# Sentinel ESP32 Node Schematic Notes

## Core blocks
- ESP32 module
- 3.3V regulation and decoupling
- Analog sensing input to GPIO34
- Status LED on GPIO2
- Programming/UART header

## Required outputs
Export final schematic as:
- PDF for review
- Source project file from chosen EDA tool

# Sentinel ESP32 Schematic Reference

This document provides a detailed map of the electrical connections and component specifications for the Sentinel ESP32 Node.

## 1. System Architecture Overview
The Sentinel Node is built on the ESP-WROOM-32 (ESP-32S) platform. All peripherals are powered via the regulated 3.3V rail from the ESP32 development board to ensure signal stability and common ground reference.

## 2. Power Distribution
- **Main Power In**: USB 5V (via Micro-USB or USB-C port on the DevKit).
- **Peripheral Rail**: 3.3V (Out from ESP32 `3V3` pin).
- **Common Reference**: All `GND` pins must be tied to a single ground plane.

## 3. Component Interconnect Map

| Component | Function | Module Pin | ESP32 GPIO | Logic/Signal Type |
| :--- | :--- | :--- | :--- | :--- |
| **BME280** | Env. Sensing | SDA | **GPIO 21** | I2C Data |
| **BME280** | Env. Sensing | SCL | **GPIO 22** | I2C Clock |
| **AM312 PIR** | Motion Detection| OUT | **GPIO 27** | Digital (3.3V Active High) |
| **MAX9814** | Sound Analysis | OUT | **GPIO 34** | Analog (0 - 3.3V) |
| **MC-38 Switch**| Security | Signal | **GPIO 32** | Digital (Internal Pull-Up) |
| **Onboard LED** | Status | LED | **GPIO 2** | Digital (Sink/Source) |

## 4. Hardware Wiring Details

### Environmental Sensor (BME280)
- **VCC**: 3.3V
- **GND**: GND
- **SDA**: Connect to GPIO 21.
- **SCL**: Connect to GPIO 22.
- *Note: I2C pull-up resistors are typically included on BME280 breakout boards.*

### Motion Sensor (AM312 PIR)
- **VCC**: 3.3V
- **GND**: GND
- **OUT**: Connect to GPIO 27.
- *Note: The AM312 is highly efficient and operates reliably on the 3.3V rail.*

### Acoustic Sensor (MAX9814 Microphone)
- **VCC**: 3.3V
- **GND**: GND
- **OUT**: Connect to GPIO 34.
- **GAIN**: Floating (default 60dB) or tied to GND (50dB) for lower sensitivity.
- *Note: GPIO 34 is part of ADC1, ensuring it is available even when WiFi is active.*

### Magnetic Reed Switch (MC-38 - Normally Closed)
- **Terminal 1**: Connect to GPIO 32.
- **Terminal 2**: Connect to GND.
- **Firmware Requirement**: `INPUT_PULLUP` enabled in setup.
- **Electrical Logic**: Circuit is **Closed** (LOW) when the magnet is present (door shut).

## 5. Physical Layout Considerations
- **Heat Isolation**: Mount the BME280 away from the ESP32 chip to avoid "thermal bloom" affecting temperature accuracy.
- **Acoustic Port**: Ensure the MAX9814 microphone has a clear path to the outside of the enclosure.
- **Zigbee/WiFi Interference**: Keep sensor signal wires as short as possible to minimize EMI.

---
*Reference: Created for the Sentinel ESP32 Project, May 2026.*
