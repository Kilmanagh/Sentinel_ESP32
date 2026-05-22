# Sentinel ESP32 Enclosure Specifications

This document captures the current mechanical design represented by the `v2_7` enclosure files in this folder.

## 1. Current Design Files
- Primary lid/base model: `Openscad_LED_Lid_v2_7_barholder.scad`
- Preview image: `Openscad_LED_Lid_v2_7.png`
- Exported print mesh: `Openscad_LED_Lid_v2_7_barholder.stl`
- Reference guide mesh: `Sentinel_Enclosure_Guide.stl`

## 2. Overall Envelope
- External size: 95 mm (L) x 65 mm (W) x 35 mm (H)
- Nominal wall thickness: 2.0 mm
- Screw pillar diameter: 8.0 mm
- Screw clearance hole diameter: 3.2 mm
- Internal divider wall: 2.0 mm thick

## 3. Base Features
- Four screw pillars at the corners for a screw-fastened lid
- Rear/side USB cutout sized by the SCAD slot at `12 mm x 8 mm`
- Reed-switch cable exit notch on the opposite side wall
- BME280 isolation chamber formed by the internal divider near the right side of the enclosure
- Vent slots on both right-side walls for the isolated environmental chamber
- Two wall-mount keyholes in the back panel

## 4. Lid Features
### PIR opening
- 12.5 mm round opening
- Centered at `x = 30`, `y = 32.5`

### Microphone opening
- 4.5 mm round opening
- Centered at `x = 55`, `y = 32.5`

### Status LED window group
- Design intent: one intact 3-LED horizontal bar holder
- LED bar center: `x = 39`, `y = 16`
- Nominal LED strip footprint: `30 mm x 15 mm`
- Three visible windows for left / center / right status indication
- Window centers:
	- Left: `x = 29`, `y = 16`
	- Center: `x = 39`, `y = 16`
	- Right: `x = 49`, `y = 16`
- Individual window size: `5 mm x 5 mm` with rounded corners
- Front bezel size: `36 mm x 12 mm`

## 5. Internal LED Retainer Intent
- Thin backing shelf for the full strip
- Bottom ledge supporting the strip from below
- Left and right side stops
- Two top tabs near the strip ends
- Left and right wire-relief notches
- Two internal light baffles between the three windows

## 6. Print Notes
- Recommended material: PETG or PLA
- Suggested infill: about 20%
- Add supports only where your slicer needs help around the side cutouts and keyholes
- Keep the lid face orientation consistent with the desired finish on the PIR, mic, and LED window side

## 7. Scope
This file is the mechanical reference for dimensions, openings, and enclosure features. Assembly and service steps belong in `Sentinel_Enclosure.md`.
