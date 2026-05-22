# PCB Package

This directory holds PCB fabrication package assets.

## Current source
- `Sentinel_Passive_Carrier.kicad_pcb`: generated KiCad board source for a passive carrier board
- `generate_passive_carrier.py`: reproducible pcbnew generator for the current board source
- `Sentinel_Passive_Carrier_drc.rpt`: latest KiCad DRC report

## Current design scope
This is a passive carrier board derived from the current wiring, schematic, and enclosure documentation.

It assumes:
- ESP32 DevKit remains off-board and connects through a wire harness header rather than a direct socket footprint
- BME280, AM312, MAX9814, reed switch, and LED strip land on generic header / terminal positions
- board outline is sized conservatively to fit within the current 95 mm x 65 mm enclosure envelope

## Fabrication package
The `gerbers/` folder now contains real KiCad exports for the passive carrier board, including:
- copper layers
- solder mask layers
- silkscreen layers
- edge cuts
- Excellon drill file
- drill map and drill report
- KiCad gerber job file

## Validation status
The fabrication package is generated from a real board source, but the current carrier layout is not yet fully DRC-clean. The latest DRC report should be reviewed before fabrication.
