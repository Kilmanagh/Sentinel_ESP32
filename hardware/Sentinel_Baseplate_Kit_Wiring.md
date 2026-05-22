# Sentinel ESP32 Baseplate Kit Wiring Guide

This guide covers the AITRIP ESP32 30-pin baseplate / expansion board shown in the user-provided photo.

For the current Sentinel build, this baseplate-plus-Dupont approach is the recommended hardware path over the unfinished custom carrier PCB.

## 1. Confirmed ESP32 Board On This Baseplate

The ESP32 board plugged into this kit is the common 30-pin ESP32 DevKit V1 / ESP-32S style board with these header labels:

- Top row, left to right: `3V3`, `GND`, `D15`, `D2`, `D4`, `RX2`, `TX2`, `D5`, `D18`, `D19`, `D21`, `RX0`, `TX0`, `D22`, `D23`
- Bottom row, left to right: `EN`, `VP`, `VN`, `D34`, `D35`, `D32`, `D33`, `D25`, `D26`, `D27`, `D14`, `D12`, `D13`, `GND`, `VIN`

Sentinel uses:

- `D21` for BME280 SDA
- `D22` for BME280 SCL
- `D27` for PIR OUT
- `D34` for MAX9814 OUT
- `D32` for reed switch signal
- `D25` for WS2812 DIN
- `D2` for onboard status LED
- `3V3`, `VIN`, and `GND` for power distribution

### Sentinel-Only Pin Map

Use this as the fast reference when you do not need the full board header listing.

| Sentinel use | ESP32 label | Purpose |
| :--- | :--- | :--- |
| BME280 SDA | `D21` | I2C data |
| BME280 SCL | `D22` | I2C clock |
| PIR output | `D27` | motion input |
| MAX9814 output | `D34` | analog microphone input |
| Reed switch | `D32` | dry-contact input |
| WS2812 data | `D25` | addressable LED data |
| Onboard LED | `D2` | ESP32 status LED |
| Sensor power | `3V3` | regulated 3.3V rail |
| LED power | `VIN` or dedicated `5V` | 5V LED supply |
| Common ground | `GND` | shared return path |

## 2. How The Baseplate Rows Work

The baseplate breaks each ESP32 header pin out to a 3-pin group.

Those 3-pin groups are labeled with:

- `S` = signal
- `V` = supply rail
- `G` = ground

Important detail from the photo:

- The upper bank is silk-labeled `S V G`
- The lower bank is silk-labeled `G V S`

So the row order is mirrored between the two sides. Follow the printed silk on the board itself. Do not assume the same top-to-bottom order on both header banks.

## 3. The Voltage Selector Jumper

The baseplate has a jumper block labeled around:

- `3.3V`
- `V`
- `5V`
- `JUMP`

This jumper selects what voltage appears on the baseplate's `V` row.

### Recommended setting for Sentinel

Set the jumper to `3.3V`.

Reason:

- BME280 should be powered from `3.3V`
- AM312 PIR should be powered from `3.3V`
- MAX9814 should be powered from `3.3V`
- the reed switch does not need a supply rail
- only the external WS2812 strip should use `5V`

If you set the baseplate `V` rail to `5V`, the normal sensor `V` pins on the Dupont rows would also become `5V`, which is not the intended Sentinel wiring.

## 4. Power Inputs And What They Mean

The baseplate photo shows multiple power entry points.

### A. ESP32 USB-C on the dev board

This is the simplest and safest primary power path for Sentinel.

Use it to:

- flash the ESP32
- power the ESP32 itself
- provide normal development power

### B. Barrel jack marked `DC 6.5-16V`

This belongs to the baseplate, not the ESP32 module itself.

Use this only if you deliberately want to power the baseplate from an external DC supply. For the current Sentinel build, this is not the recommended starting path.

### C. Baseplate micro-USB marked `USB5V`

This is another baseplate-side power input. It is also not the recommended starting path for Sentinel when the ESP32 USB-C is already available.

### D. Separate `5V` / `3.3V` breakout headers

The baseplate photo also shows dedicated power breakout headers labeled `5V`, `3.3V`, and `GND` near the lower-right area.

These are the cleanest places to tap dedicated rails when you need a voltage different from the selected `V` rail.

For Sentinel, this matters because:

- sensors want `3.3V`
- WS2812 wants `5V`

## 5. Recommended Power Strategy For Sentinel

Use this power plan:

1. Power the ESP32 through its own USB-C port.
2. Set the baseplate `V` jumper to `3.3V`.
3. Use the normal `S/V/G` rows for the 3.3V sensors.
4. Do not power the WS2812 strip from the baseplate `V` row.
5. Power the WS2812 strip from the dedicated `5V` breakout or from the ESP32 `VIN` pin breakout.
6. Tie WS2812 ground to the same common ground used by the ESP32 and sensors.

Avoid powering the setup from multiple sources at once until the final power plan is intentionally designed.

## 6. Exact Sentinel Wiring On The Baseplate

### BME280

- `SDA` -> `D21` signal pin
- `SCL` -> `D22` signal pin
- `VCC` -> a `V` pin on the baseplate with jumper set to `3.3V`
- `GND` -> a `G` pin on the baseplate

### AM312 PIR

- `OUT` -> `D27` signal pin
- `VCC` -> `V` pin with jumper set to `3.3V`
- `GND` -> `G` pin

### MAX9814

- `OUT` -> `D34` signal pin
- `VCC` -> `V` pin with jumper set to `3.3V`
- `GND` -> `G` pin
- `GAIN` -> leave floating for default gain, or tie to ground only if you intentionally want lower gain
- `AR` / attack-release pin -> leave as the module default unless you are deliberately tuning the analog hardware behavior

### MC-38 Reed Switch

- one wire -> `D32` signal pin
- other wire -> `G` pin

The reed switch does not use the `V` rail.

### WS2812 3-LED Strip

- `DIN` -> `D25` signal pin through a `330-470 ohm` series resistor
- `5V` -> dedicated `5V` breakout on the baseplate or `VIN` breakout, not the shared `V` row when the jumper is set to `3.3V`
- `GND` -> common `GND`

Also add:

- `470-1000 uF` capacitor across LED `5V` and `GND`, physically near the LED power feed

## 7. Quick Map

| Sentinel Function | ESP32 Label On Board | Baseplate Use |
| :--- | :--- | :--- |
| BME280 SDA | `D21` | `S` pin for `D21` |
| BME280 SCL | `D22` | `S` pin for `D22` |
| PIR OUT | `D27` | `S` pin for `D27` |
| MAX9814 OUT | `D34` | `S` pin for `D34` |
| Reed signal | `D32` | `S` pin for `D32` |
| WS2812 DIN | `D25` | `S` pin for `D25` via series resistor |
| Sensor power | `3.3V` selected on jumper | `V` row |
| Sensor ground | `GND` | `G` row |
| LED power | `VIN` or dedicated `5V` breakout | direct 5V feed |
| LED ground | `GND` | common ground |

## 8. Physical Wiring Checklist

Work in this order so power stays simple while you build:

1. Plug the 30-pin ESP32 into the baseplate with the printed pin labels matching the baseplate breakout labels.
2. Set the baseplate jumper to `3.3V` before attaching any sensor power leads.
3. Wire all 3.3V sensors first.
4. Wire the reed switch.
5. Wire the WS2812 strip last, including its resistor and capacitor.
6. Power the build from the ESP32 USB-C only for the first test.

| Check | From | To | Notes |
| :--- | :--- | :--- | :--- |
| ESP32 mounted | ESP32 dev board | baseplate socket headers | Confirm header labels line up with the printed baseplate labels |
| Jumper set | `JUMP` block | `3.3V` selected | Do this before sensor power wiring |
| BME280 data 1 | BME280 `SDA` | `D21` `S` pin | I2C data |
| BME280 data 2 | BME280 `SCL` | `D22` `S` pin | I2C clock |
| BME280 power | BME280 `VCC` and `GND` | baseplate `V` and `G` | Safe because jumper is set to `3.3V` |
| PIR signal | AM312 `OUT` | `D27` `S` pin | Digital motion signal |
| PIR power | AM312 `VCC` and `GND` | baseplate `V` and `G` | Uses the shared 3.3V sensor rail |
| Mic signal | MAX9814 `OUT` | `D34` `S` pin | Analog input |
| Mic power | MAX9814 `VCC` and `GND` | baseplate `V` and `G` | Leave `GAIN` floating unless intentionally changing gain |
| Reed lead 1 | MC-38 wire 1 | `D32` `S` pin | Dry contact input |
| Reed lead 2 | MC-38 wire 2 | baseplate `G` | No `V` connection used |
| LED data | WS2812 `DIN` | `D25` `S` pin | Add `330-470 ohm` series resistor in line |
| LED power | WS2812 `5V` and `GND` | dedicated `5V` breakout and common `GND` | Do not use the shared `V` row for this |
| LED capacitor | `470-1000 uF` capacitor | across LED `5V` and `GND` | Place near the LED power entry |
| First power test | USB cable | ESP32 USB-C | Avoid barrel jack and baseplate `USB5V` for first bring-up |

## 9. Recommended Build Notes

- Keep the `V` jumper on `3.3V` for the normal sensor rows.
- Keep the LED data resistor close to the LED strip input, not back near the ESP32 if possible.
- Keep the LED bulk capacitor close to the LED power feed.
- Do not mix up the mirrored `S/V/G` row orientation between the upper and lower header banks.
- If a Dupont lead feels loose, replace it instead of trusting it inside the enclosure.

## 10. Scope

This guide is for the baseplate / Dupont implementation path using the existing AITRIP kit. It does not replace the schematic reference, but it is the practical build reference for the current hardware.

## 11. Bring-Up Checklist

Use this after the wiring is complete.

1. Verify the baseplate jumper is still on `3.3V`.
2. Confirm the WS2812 strip is powered from `5V` or `VIN`, not the shared sensor `V` row.
3. Check that the LED strip ground, sensor ground, and ESP32 ground are common.
4. Inspect the upper and lower baseplate rows against the silk so no `S/V/G` pins are mirrored by mistake.
5. Power the board from ESP32 USB-C only.
6. Confirm the ESP32 boots and is detected normally over USB.
7. Verify I2C sensor discovery for the BME280.
8. Check PIR state changes.
9. Check the reed switch open/closed state.
10. Check microphone readings move with nearby sound.
11. Test the 3-pixel WS2812 chain before closing the enclosure.

### Fast Fault Isolation

| Symptom | Most likely first check |
| :--- | :--- |
| BME280 missing | recheck `D21` / `D22` and `3.3V` power |
| PIR always stuck | confirm `OUT` is on `D27`, not a `V` or `G` pin |
| Reed always open/closed | confirm one side is on `D32` and the other is on `GND` only |
| Mic flatlines | recheck `D34` signal lead and `3.3V` supply |
| LEDs dark | recheck LED `5V`, common `GND`, and `D25` data path |
| LEDs glitch | move the resistor closer to `DIN` and confirm the bulk capacitor is installed |
