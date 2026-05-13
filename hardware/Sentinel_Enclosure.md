## 1. Assembly Instructions
To ensure maximum sensor accuracy and structural integrity, follow these steps:

1. **Board Mounting**: Secure the ESP32 DevKit in the main (larger) chamber. Align the Micro-USB/USB-C port with the side cutout.
2. **Sensor Isolation**: Place the BME280 sensor in the small chamber behind the internal baffle wall. This is critical for preventing ESP32 heat from skewing temperature data.
3. **Sensor Alignment**:
    - Push the **AM312 PIR** lens through the 12.5mm circular hole from the inside.
    - Align the **MAX9814 Mic** element directly behind the 4mm hole. Use a small piece of double-sided tape or foam to hold it flush against the wall.
4. **Reed Switch**: Run the two signal wires for the **MC-38** through the U-shaped rear notch.
5. **Closure**: Snap the lid onto the base. If a tighter fit is required, use a small amount of hot glue or M3 screws (if holes are added).

## 2. Disassembly & Maintenance (Destruction)
If maintenance or sensor recalibration is required:

1. **Power Down**: Always disconnect the USB power cable before opening.
2. **Pry Points**: Use a flat tool (spudger or flat-head screwdriver) at the USB port cutout. Gently pry upward to release the lid.
3. **Internal Wires**: Lift the lid slowly. The PIR and Mic may be snug; pulling too fast could disconnect the jumper wires from the ESP32 pins.
4. **BME280 Chamber**: If debris accumulates in the vents, use compressed air to clear the environmental chamber.

Feature,Dimension,Requirement
Main Footprint,85mm x 55mm,Fits ESP32 DevKit V1
PIR Aperture,12.5mm Circle,Front-facing for AM312
Acoustic Port,4.0mm Circle,Centered over MAX9814
Baffle Wall,1.5mm Thickness,Isolates BME280 from ESP32 heat
Ventilation,4x Slotted Grills,Ensure natural convection for IAQ accuracy

Assembly Instructions
Board Mounting: Place the ESP32 in the larger chamber. Align the USB port with the rectangular side cutout.

Environmental Isolation: Slide the BME280 into the smaller vented chamber. Ensure no wires are pinched by the baffle wall.

Sensor Seating:

Press the AM312 PIR lens through the 12.5mm hole in the lid.

Secure the MAX9814 Mic flush against the 4mm hole using a small dab of adhesive or a foam gasket.

Wiring Exit: Thread the MC-38 Reed Switch wires through the side notch.

Closure: Snap the lid onto the base. If a tighter fit is required, use a drop of cyanoacrylate (Super Glue) on the corners.

4. Deconstruction (Repair) Instructions
Power Safety: Always disconnect the USB power source before opening.

Case Opening: Insert a thin flat-head tool (or guitar pick) into the seam near the USB port. Gently pry upward.

Sensor Protection: When lifting the lid, do not pull abruptly. The PIR and Mic are mounted to the lid; ensure you do not tension the jumper wires connected to the ESP32.

Component Removal: To remove the ESP32, lift from the side opposite the USB port to avoid damaging the connector.
