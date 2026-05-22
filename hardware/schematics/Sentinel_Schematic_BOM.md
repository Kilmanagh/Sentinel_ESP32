# Sentinel Schematic BOM And Connector Plan

This file is the build-oriented companion to `Sentinel_ESP32_Node.kicad_sch` and `Sentinel_KiCad_Netlist.net`.

## 1. Reference Designator BOM

| RefDes | Item | Electrical Role | Suggested Footprint / Form |
| :--- | :--- | :--- | :--- |
| **U1** | ESP32 development board / ESP-WROOM-32 host | Main controller, power source, GPIO host | ESP32 DevKit style module or equivalent host board |
| **U2** | BME280 breakout | I2C environmental sensor | 4-pin I2C breakout |
| **U3** | AM312 PIR module | Digital motion sensor | 3-pin module header |
| **U4** | MAX9814 microphone module | Analog sound input | 5-pin module header |
| **S1** | MC-38 reed switch | Door / magnetic contact input | 2-wire external switch |
| **D1** | WS2812 LED, left | External status LED 1 | WS2812B package or intact strip position 1 |
| **D2** | WS2812 LED, center | External status LED 2 | WS2812B package or intact strip position 2 |
| **D3** | WS2812 LED, right | External status LED 3 | WS2812B package or intact strip position 3 |
| **R1** | 330-470 ohm resistor | Series resistor on GPIO25 LED data line | 0805 or leaded equivalent |
| **C1** | 470-1000 uF capacitor | Bulk capacitor across LED 5V and GND | Radial electrolytic or equivalent |

## 2. Connection Landing Table

| RefDes | Pin / Terminal | Connects To | Net / Function |
| :--- | :--- | :--- | :--- |
| **U2** | VCC | U1 `3V3` | `3.3V` |
| **U2** | GND | U1 `GND` | `GND` |
| **U2** | SDA | U1 `GPIO21` | `I2C_SDA` |
| **U2** | SCL | U1 `GPIO22` | `I2C_SCL` |
| **U3** | VCC | U1 `3V3` | `3.3V` |
| **U3** | GND | U1 `GND` | `GND` |
| **U3** | OUT | U1 `GPIO27` | `PIR_DATA` |
| **U4** | VCC | U1 `3V3` | `3.3V` |
| **U4** | GND | U1 `GND` | `GND` |
| **U4** | OUT | U1 `GPIO34` | `MIC_ANALOG` |
| **S1** | Terminal 1 | U1 `GPIO32` | `REED_SIGNAL` |
| **S1** | Terminal 2 | U1 `GND` | `GND` |
| **R1** | Pin 1 | U1 `GPIO25` | `LED_DATA` |
| **R1** | Pin 2 | D1 `DIN` | `LED_DATA_IN` |
| **D1** | VDD | U1 `5V/VIN` | `5V_LED` |
| **D1** | GND | U1 `GND` | `GND` |
| **D1** | DOUT | D2 `DIN` | `LED1_TO_LED2` |
| **D2** | VDD | U1 `5V/VIN` | `5V_LED` |
| **D2** | GND | U1 `GND` | `GND` |
| **D2** | DOUT | D3 `DIN` | `LED2_TO_LED3` |
| **D3** | VDD | U1 `5V/VIN` | `5V_LED` |
| **D3** | GND | U1 `GND` | `GND` |
| **C1** | Positive | U1 `5V/VIN` | `5V_LED` |
| **C1** | Negative | U1 `GND` | `GND` |

## 3. Harness And Build Notes

- The external status indicator is one intact horizontal 3-LED strip in the enclosure `v2_7` layout.
- Preserve left / center / right LED order as `D1`, `D2`, `D3` to match the enclosure window order.
- Keep `R1` physically close to LED 1 data input.
- Keep `C1` physically close to the LED power entry.
- Sensors remain on `3V3`; the WS2812 chain remains on `5V/VIN`.
- All grounds must be common.

## 4. Firmware-Relevant Mapping

- `GPIO21` / `GPIO22`: BME280 I2C
- `GPIO27`: PIR digital output
- `GPIO34`: MAX9814 analog output
- `GPIO32`: reed switch input
- `GPIO25`: external 3-pixel status LED data
- `GPIO2`: onboard ESP32 status LED
