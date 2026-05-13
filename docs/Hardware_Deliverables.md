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

| Component | ESP32 GPIO | Firmware Variable | Electrical Logic |
| :--- | :--- | :--- | :--- |
| **BME280 SDA** | 21 | `BME_SDA` | I2C Data |
| **BME280 SCL** | 22 | `BME_SCL` | I2C Clock |
| **AM312 PIR** | 27 | `PIN_PIR` | Digital Input (HIGH = Motion) |
| **MC-38 Reed** | 32 | `PIN_DOOR` | Input Pull-up (LOW = Closed / HIGH = Open) |
| **MAX9814 Out** | 34 | `PIN_SOUND` | Analog Input (ADC 0-4095) |
| **Onboard LED** | 2 | `PIN_LED` | Digital Output (Status Blinks) |

## Firmware Logic Adjustment (Normally Closed)
The MC-38 (Normally Closed) means that when the magnet is near the sensor (door closed), the circuit is **completed**. 
- **Wiring**: Connect one wire to GPIO 32 and the other to Ground (GND).
- **Logic**: With `INPUT_PULLUP` enabled in the code:
    - **Door Closed**: Magnet present -> Switch closed -> GPIO 32 pulled to GND (**LOW**).
    - **Door Open**: Magnet removed -> Switch open -> GPIO 32 pulled to VCC (**HIGH**).

## Design & Fabrication Assets
- **Schematic Notes**: Detailed in `hardware/schematics/`.
- **PCB Layout**: Source files and notes in `hardware/pcb/`.
- **Manufacturing Files**: Gerber stack-up files are located in `hardware/pcb/gerbers/`.

*Note: The `SOUND_THRESHOLD` in `Sentinel_ESP32_Node.ino` (currently set to 2000) should be calibrated based on the specific gain settings of the MAX9814 module.*
