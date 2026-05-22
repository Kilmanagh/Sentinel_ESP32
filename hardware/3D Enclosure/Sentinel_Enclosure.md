# Sentinel ESP32 Enclosure Guide

This file covers assembly, installation, and service notes for the current `v2_7` enclosure layout.

## 1. What This Guide Covers
- Base and lid assembly order
- Sensor placement behind the existing lid openings
- LED strip placement behind the 3-window status bar
- Service considerations when reopening the case

For dimensions and exact feature locations, use `Sentinel_Enclosure_Specs.md`.

## 2. Assembly Order
1. Mount the ESP32 DevKit in the main chamber and confirm the USB connector aligns with the side cutout.
2. Place the BME280 in the isolated vented chamber on the right side of the enclosure so it stays separated from ESP32 heat.
3. Route reed-switch wiring through the side notch before final closure.
4. Seat the PIR module so the AM312 lens aligns cleanly with the 12.5 mm lid opening.
5. Seat the MAX9814 microphone so the mic element is centered behind the 4.5 mm acoustic opening.
6. Fit the 3-LED status strip into the `v2_7` horizontal retainer behind the three LED windows.
7. Verify that LED wiring sits in the side wire-relief areas and does not push the strip out of the retainer.
8. Close the enclosure with the screw-fastened lid.

## 3. LED Bar Notes For v2_7
- The `v2_7` lid is designed around one intact horizontal 3-LED strip, not three separate single-LED holders.
- The visible order is left / center / right across the front bezel.
- The LED bar is intentionally placed low on the lid to preserve clearance from the PIR opening.
- Internal baffles separate the three LED windows to reduce light bleed.

## 4. Sensor Placement Notes
- PIR: keep the lens square to the lid opening and avoid forcing the board at an angle.
- Mic: a thin foam gasket or tape spacer can help keep the microphone aligned and reduce internal echo.
- BME280: keep the vent path open and avoid bunching cable slack inside the isolated chamber.

## 5. Wall Mount Use
- The base includes two back-panel keyholes for wall mounting.
- Test screw-head fit against the printed keyholes before final installation.
- Keep cable exits oriented so the enclosure can sit flat against the wall.

## 6. Service / Reopening
1. Disconnect USB power before opening the case.
2. Remove the lid screws before lifting the lid.
3. Lift slowly so the PIR, microphone, and LED wiring are not strained.
4. Clear dust from the vented BME chamber with air only; avoid pushing debris deeper into the slots.

## 7. Scope
This file is worth keeping only as the practical guide. It should not duplicate raw dimensions or SCAD geometry values from `Sentinel_Enclosure_Specs.md`.
