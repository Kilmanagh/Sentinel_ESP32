# Sentinel ESP32 Hardware Deliverables

This document outlines the current hardware components and wiring configuration for the active ESPHome-based Sentinel ESP32 node.

## Bill of Materials (BOM)

- **Microcontroller**: AITRIP ESP-WROOM-32 / ESP-32S Type-C CH340C development board, 30-pin ESP32 DevKit V1 style header layout
- **Environmental Sensor**: BME280, 3.3V I2C
- **Motion Sensor**: AM312 mini PIR, powered from 3.3V
- **Acoustic Sensor**: MAX9814 microphone amplifier, analog output to GPIO 34
- **Security Sensor**: MC-38 magnetic reed switch, normally closed, signal to GPIO 32
- **Onboard Status LED**: ESP32 onboard LED on GPIO 2
- **External Status LEDs**: 3 x WS2812 addressable LEDs chained on GPIO 25 data
- **Recommended LED protection**: 330-470 ohm series resistor on the GPIO 25 data line and 470-1000 uF capacitor across LED 5V and GND

## Hardware Configuration And Pin Mapping

The active ESPHome build uses a mixed power layout:

- Sensors use the ESP32 regulated 3.3V rail
- The external WS2812 status LED chain uses the board 5V or VIN rail
- All devices must share a common ground

### Confirmed Dev Board Header Layout

The actual board shown by the user is the common 30-pin ESP32 DevKit V1 / ESP-32S style board, not the 38-pin Espressif DevKitC layout.

With the module antenna on the left and the USB Type-C connector on the right, the header labels are:

- Top row, left to right: `3V3`, `GND`, `D15`, `D2`, `D4`, `RX2`, `TX2`, `D5`, `D18`, `D19`, `D21`, `RX0`, `TX0`, `D22`, `D23`
- Bottom row, left to right: `EN`, `VP`, `VN`, `D34`, `D35`, `D32`, `D33`, `D25`, `D26`, `D27`, `D14`, `D12`, `D13`, `GND`, `VIN`

For Sentinel, the important physical header locations are:

- `3V3`: top row, pin 1 from the left
- `GND`: top row, pin 2 from the left or bottom row, pin 14 from the left
- `VIN` / 5V: bottom row, pin 15 from the left
- `GPIO34`: bottom row, pin 4 from the left (`D34`)
- `GPIO32`: bottom row, pin 6 from the left (`D32`)
- `GPIO25`: bottom row, pin 8 from the left (`D25`)
- `GPIO27`: bottom row, pin 10 from the left (`D27`)
- `GPIO21`: top row, pin 11 from the left (`D21`)
- `GPIO22`: top row, pin 14 from the left (`D22`)
- `GPIO2`: top row, pin 4 from the left (`D2`)

| Hardware Component | Component Pin | ESP32 GPIO / Pin | Active ESPHome Use | Pin Type |
| :--- | :--- | :--- | :--- | :--- |
| **BME280** | SDA | **GPIO 21** | `i2c.sda` | I2C Data |
| **BME280** | SCL | **GPIO 22** | `i2c.scl` | I2C Clock |
| **AM312 PIR** | OUT | **GPIO 27** | `pir_raw` | Digital Input |
| **MC-38 Reed Switch (NC)** | Signal | **GPIO 32** | `Door` | Digital Input with Pull-up |
| **MAX9814** | OUT | **GPIO 34** | sound sampling path | Analog Input |
| **Onboard LED** | Internal LED | **GPIO 2** | `status_led` | Digital Status Output |
| **WS2812 LED Chain** | DIN (LED 1) | **GPIO 25** | `Status LEDs` | Addressable LED Data |

## Wiring Connection Notes

- **Sensor Power**: Connect the BME280, AM312 PIR, and MAX9814 to the **3.3V** rail and **GND**.
- **Reed Switch**: Connect one MC-38 wire to **GPIO 32** and the other to **GND**. The active ESPHome config enables the internal pull-up, so the closed switch reads low when the magnet is present.
- **Analog Input**: GPIO 34 is input-only on the ESP32 and is appropriate for the MAX9814 analog output.
- **WS2812 Status LEDs**: The external 3-pixel LED chain is not powered from 3.3V. Use the ESP32 board **5V** or **VIN** rail for LED power, connect LED ground to the shared ground rail, and feed LED 1 data from **GPIO 25**.
- **LED Data Chain**: `GPIO25 -> DIN LED 1 -> DOUT LED 1 -> DIN LED 2 -> DOUT LED 2 -> DIN LED 3`.
- **LED Color Order**: The current ESPHome build expects `GRB` for the WS2812 chain.
- **Onboard LED vs External LEDs**: The node uses both the onboard GPIO 2 LED and the external 3-pixel WS2812 chain. The onboard LED is the basic ESPHome status LED, while the three external LEDs provide the main left, center, and right status display described elsewhere in the project docs.

### Final Expected Connections

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

For the baseplate / expansion kit build, keep the baseplate `V` jumper set to `3.3V` so the shared `V` row remains safe for the BME280, PIR, and MAX9814. Do not power the WS2812 strip from that shared `V` row.

## Current Firmware-Relevant Hardware Notes

- The active build exposes a `Door` binary sensor on GPIO 32.
- The active build drives `Status LEDs` as a 3-pixel WS2812 strip on GPIO 25.
- The active build still uses the onboard GPIO 2 LED through the ESPHome `status_led` component.
- Sound detection is no longer described by a single fixed firmware constant in documentation. Thresholds such as `Sound Threshold ADC` are runtime-adjustable through ESPHome and Home Assistant.

## Design And Fabrication Assets

- **Wiring Reference**: `hardware/Sentinel_Wiring.md`
- **Baseplate Build Guide**: `hardware/Sentinel_Baseplate_Kit_Wiring.md`
- **Enclosure Notes**: `hardware/3D Enclosure/Sentinel_Enclosure.md` and `hardware/3D Enclosure/Sentinel_Enclosure_Specs.md`
- **Schematic Notes**: `hardware/schematics/`
- **PCB Layout Notes**: `hardware/pcb/`
- **Manufacturing Files**: `hardware/pcb/gerbers/`

The active physical build reference is the baseplate / Dupont path in `hardware/Sentinel_Baseplate_Kit_Wiring.md`. The PCB layout and gerber outputs remain experimental reference assets until the passive carrier board is fully DRC-clean.
