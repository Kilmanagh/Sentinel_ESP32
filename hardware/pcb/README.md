# PCB Package

This directory holds PCB fabrication package assets.

## Current recommendation

For the current Sentinel hardware build, do not treat this folder as the primary assembly path.

The recommended real-world build is:

- the existing AITRIP ESP32 baseplate / expansion kit
- Dupont wiring
- the practical wiring guide in `hardware/Sentinel_Baseplate_Kit_Wiring.md`

This `hardware/pcb/` folder should currently be treated as an experimental custom-board branch and documentation artifact, not the default build path.

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

It is useful for:

- preserving the custom PCB experiment
- future routing / layout work
- exporting fabrication files for review while the design is still being corrected

It is not yet the recommended path for:

- immediate hardware assembly
- ordering production boards
- using as the single source of truth for the active build

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

The fabrication package is generated from a real board source, but the current carrier layout is not yet fully DRC-clean.

Practical meaning:

- the files are real KiCad exports
- the board is still not ready to be treated as fabrication-safe
- the DRC report must be resolved before any board order is placed

Until that happens, the active hardware reference remains the baseplate / Dupont path documented in `hardware/Sentinel_Baseplate_Kit_Wiring.md`.
