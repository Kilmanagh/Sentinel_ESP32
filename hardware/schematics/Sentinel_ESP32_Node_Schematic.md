# Sentinel ESP32 Node Schematic Reference

This document describes the current schematic intent for the active Sentinel ESP32 node hardware.

## 1. Core Blocks
- ESP32 development board / module
- 3.3V sensor rail for BME280, AM312, and MAX9814
- 5V rail for the external 3-pixel WS2812 status LED chain
- Shared ground rail across ESP32, sensors, reed switch, and LED chain
- I2C environmental sensing on GPIO21 / GPIO22
- Digital inputs for PIR and reed switch
- Analog input for MAX9814 on GPIO34
- Onboard status LED on GPIO2
- External addressable LED data output on GPIO25

## 2. Required Outputs
Export final schematic as:
- PDF for review
- Source project file from the chosen EDA tool

## 3. Power Architecture
- Main power input: USB 5V through the ESP32 development board
- Sensor rail: regulated 3.3V from the ESP32 `3V3` pin
- LED rail: ESP32 board `5V` or `VIN` to the external WS2812 chain
- Grounding: all modules must share common ground

This is no longer a single-rail all-3.3V design. The external status LEDs are the main exception and must stay on the 5V rail.

## 4. Signal And Power Map

| Component | Pin / Connection | ESP32 GPIO or Rail | Notes |
| :--- | :--- | :--- | :--- |
| **BME280** | SDA | **GPIO 21** | I2C data |
| **BME280** | SCL | **GPIO 22** | I2C clock |
| **BME280** | VCC | **3V3** | Breakout board supply |
| **BME280** | GND | **GND** | Shared ground |
| **AM312 PIR** | OUT | **GPIO 27** | Digital motion input |
| **AM312 PIR** | VCC | **3V3** | Sensor supply |
| **AM312 PIR** | GND | **GND** | Shared ground |
| **MAX9814** | OUT | **GPIO 34** | ADC1 analog input |
| **MAX9814** | VCC | **3V3** | Sensor supply |
| **MAX9814** | GND | **GND** | Shared ground |
| **MC-38 Reed Switch** | Signal | **GPIO 32** | Uses internal pull-up |
| **MC-38 Reed Switch** | Return | **GND** | Closed switch pulls low |
| **Onboard LED** | Internal LED | **GPIO 2** | ESP32 board LED |
| **WS2812 Chain** | DIN (LED 1) | **GPIO 25** | External 3-pixel data input |
| **WS2812 Chain** | VCC | **5V / VIN** | External LED power |
| **WS2812 Chain** | GND | **GND** | Must share ESP32 ground |

## 5. External LED Chain Requirements
- LED chain size: 3 x WS2812
- Physical intent: one intact 3-LED strip used for left / center / right status indication
- Data path: `GPIO25 -> series resistor -> DIN LED 1 -> DOUT LED 1 -> DIN LED 2 -> DOUT LED 2 -> DIN LED 3`
- Recommended series resistor on data line: `330-470 ohm`
- Recommended bulk capacitor across LED power input: `470-1000 uF` between `5V` and `GND`
- Expected firmware color order: `GRB`

## 6. Functional Notes By Circuit

### BME280
- Uses the shared I2C bus on GPIO21 / GPIO22
- Typical pull-ups are usually present on the breakout board

### AM312 PIR
- Powered from 3.3V
- Output is read as a digital input on GPIO27

### MAX9814
- Output feeds GPIO34, which is input-only and part of ADC1
- Gain pin can remain floating for default gain, or be tied low if reduced gain is preferred

### MC-38 Reed Switch
- One side to GPIO32, the other side to ground
- Logic is normally closed, so the closed door state pulls the input low when the magnet is present

### Status Indicators
- GPIO2 remains the ESP32 onboard status LED path
- GPIO25 drives the external 3-pixel WS2812 chain used by the enclosure window group

## 7. Layout Notes
- Keep the BME280 thermally separated from the ESP32 module
- Keep the microphone close to the enclosure acoustic port
- Keep the WS2812 data resistor close to LED 1 input if possible
- Place the LED bulk capacitor close to the 5V entry point for the LED chain
- Keep the LED chain ground tied solidly to the ESP32 ground reference

## 8. Source Files In This Folder
- `Sentinel_ESP32_Node.kicad_sch`: KiCad source scaffold for the current hardware revision
- `Sentinel_KiCad_Netlist.net`: machine-style connectivity reference derived from the same hardware intent
- `Sentinel_ESP32_Node_Schematic.md`: human-readable schematic reference
- `Sentinel_Schematic_BOM.md`: build-oriented reference designator, connector, and harness table

## 9. Scope
This file is the human-readable schematic reference for the current hardware. The KiCad source scaffold lives in `Sentinel_ESP32_Node.kicad_sch`, the machine-style net connectivity reference lives in `Sentinel_KiCad_Netlist.net`, and the build-oriented connector/BOM table lives in `Sentinel_Schematic_BOM.md`.
