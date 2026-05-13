# Sentinel ESP32 Hardware Deliverables

This document outlines the specific hardware components and wiring configurations for the Sentinel ESP32 Node.

## Bill of Materials (BOM)
- **Microcontroller**: ESP-WROOM-32 / ESP-32S Development Board.
- **Environmental Sensor**: BME280 (Supports 3.3V, I2C interface).
- **Motion Sensor**: AM312 Mini PIR Sensor (Supports 2.7V - 12V; powered via 3.3V).
- **Acoustic Sensor**: MAX9814 Microphone Amplifier (Analog output connected to GPIO 34).
- **Security Sensor**: MC-38 Magnetic Reed Switch (**Normally Closed**, connected to GPIO 32).
- **Status Indicator**: Onboard LED (GPIO 2).

## Hardware Configuration & Pin Mapping
The firmware is configured to use a unified 3.3V power rail for all sensors, driven by the ESP32 regulated output.

| Hardware Component | Component Pin | ESP32 GPIO | Firmware Variable | Pin Type |
| :--- | :--- | :--- | :--- | :--- |
| **BME280 Sensor** | SDA | **GPIO 21** | `BME_SDA` | I2C Data |
| **BME280 Sensor** | SCL | **GPIO 22** | `BME_SCL` | I2C Clock |
| **AM312 PIR** | Out | **GPIO 27** | `PIN_PIR` | Digital Input |
| **MC-38 Reed Switch (NC)**| Signal | **GPIO 32** | `PIN_DOOR` | Digital Input (Internal Pull-up) |
| **MAX9814 Microphone** | Out | **GPIO 34** | `PIN_SOUND` | Analog Input |
| **Status LED** | Onboard | **GPIO 2** | `PIN_LED` | Digital Output |

## Wiring Connection Notes
- **Power Supply**: Connect all sensors to the **3.3V** rail and **GND**.
- **Normally Closed (NC) Reed Switch**: Connect one wire of the MC-38 to **GPIO 32** and the other to **GND**. The internal pull-up resistor is enabled in the code (`INPUT_PULLUP`) to handle the logic where **LOW** (magnet present) represents **CLOSED**.
- **Analog Input**: GPIO 34 is an input-only pin on the ESP32, dedicated to the MAX9814 analog signal.

## Design & Fabrication Assets
- **Schematic Notes**: Detailed in `hardware/schematics/`.
- **PCB Layout**: Source files and notes in `hardware/pcb/`.
- **Manufacturing Files**: Gerber stack-up files are located in `hardware/pcb/gerbers/`.

*Note: The `SOUND_THRESHOLD` in the firmware (currently set to 1500) should be calibrated based on the specific environment and gain settings of the MAX9814 module.*
