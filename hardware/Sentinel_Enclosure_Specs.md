# Sentinel ESP32 Enclosure Specifications

This document outlines the mechanical requirements for the 3D-printed enclosure of the Sentinel ESP32 Node.

## 1. Physical Dimensions
- **Footprint**: 85mm (L) x 55mm (W) x 35mm (H)
- **Wall Thickness**: 2.0mm (Standard for PLA/PETG)
- **Internal Chamber**: Divided into two sections via a 1.5mm baffle wall.

## 2. Sensor Integration Requirements
### PIR Sensor (AM312)
- **Cutout**: 12.5mm circular hole on the front face.
- **Mounting**: Press-fit or glue from the interior. Lens must protrude through the wall.

### Acoustic Sensor (MAX9814)
- **Cutout**: 4.0mm circular hole.
- **Positioning**: Align directly over the microphone diaphragm.
- **Tip**: Apply a thin gasket of closed-cell foam around the mic element to reduce internal echo.

### Environmental Sensor (BME280)
- **Vents**: Horizontal or vertical slots (minimum 4 slots, 10mm x 2mm each).
- **Isolation**: Must be placed in the isolated chamber away from the ESP32 chip to prevent heat contamination.

### Power & Connectivity
- **USB Port**: 12mm x 8mm rectangular cutout on the side.
- **Reed Switch Exit**: 4mm circular or U-shaped notch for external MC-38 wiring.

## 3. Recommended Print Settings (Flashforge Creator 5 Pro)
- **Material**: PETG (Preferred for heat resistance) or PLA.
- **Infill**: 20% Gyroid (for structural rigidity).
- **Supports**: Required for the USB and sensor cutouts.
- **Orientation**: Print with the main face down for best finish on sensor ports.

---
*Created May 2026.*
