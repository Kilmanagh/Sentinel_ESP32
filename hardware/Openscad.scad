// Sentinel ESP32 Node Enclosure
// Features: BME280 Isolation, PIR Cutout, Mic Port, NC Reed Notch

$fn = 64;
wall = 2.0;
length = 85;
width = 55;
height = 30;

module base() {
    difference() {
        // Main Body
        cube([length, width, height]);
        translate([wall, wall, wall]) 
            cube([length-wall*2, width-wall*2, height]);
        
        // USB Cutout
        translate([-1, width/2-6, wall+2]) cube([wall+2, 12, 8]);
        
        // Reed Switch Wire Notch
        translate([length-wall-1, 10, height-5]) cube([wall+2, 4, 6]);
        
        // BME280 Cooling Vents
        for(i=[0:3]) {
            translate([length-15, width-wall-1, 5 + i*5]) cube([10, wall+2, 2]);
        }
    }
    // Internal Baffle Wall (Thermal Isolation)
    translate([length-20, wall, wall]) cube([1.5, width-wall*2, height-wall-5]);
}

module lid() {
    translate([0, width + 10, 0]) {
        difference() {
            cube([length, width, wall]);
            
            // PIR Lens Cutout (12.5mm)
            translate([25, width/2, -1]) cylinder(h=wall+2, d=12.5);
            
            // Microphone Port (4mm)
            translate([50, width/2, -1]) cylinder(h=wall+2, d=4);
        }
    }
}

base();
lid();
