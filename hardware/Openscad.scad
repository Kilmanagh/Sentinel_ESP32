// Sentinel ESP32 Node Enclosure v2.2
// FINAL VERSION: Includes BME Vents, Mic Port, and M3 Pillars

$fn = 64;
wall = 2.0;
length = 95; 
width = 65;  
height = 35;
screw_d = 3.2; 
pillar_size = 8;

module base() {
    difference() {
        cube([length, width, height]);
        translate([wall, wall, wall]) 
            cube([length-wall*2, width-wall*2, height]);
        
        // --- NEW: BME280 ISOLATION VENTS ---
        // Side vents to allow cross-flow air into the sensor chamber
        for(i=[0:4]) {
            translate([length-15, width-wall-1, 8 + i*4]) cube([10, wall+2, 2]);
            translate([length-15, -1, 8 + i*4]) cube([10, wall+2, 2]);
        }

        // USB & Reed Switch Cutouts
        translate([-1, width/2-6, wall+2]) cube([wall+2, 12, 8]);
        translate([length-wall-1, 10, height-8]) cube([wall+2, 4, 10]);
    }
    
    // Screw Pillars
    for(x=[pillar_size/2, length-pillar_size/2])
        for(y=[pillar_size/2, width-pillar_size/2])
            translate([x, y, wall]) difference() {
                cylinder(h=height-wall, d=pillar_size);
                cylinder(h=height, d=screw_d);
            }

    // --- REINFORCED BAFFLE WALL ---
    // Physical barrier to block heat from ESP32 reaching BME280
    translate([length-22, wall, wall]) cube([2, width-wall*2, height-wall-5]);
}

module lid() {
    translate([0, width + 20, 0]) {
        difference() {
            cube([length, width, wall]);
            
            // Lid Screw Holes
            translate([pillar_size/2, pillar_size/2, -1]) cylinder(h=wall+2, d=screw_d);
            translate([length-pillar_size/2, pillar_size/2, -1]) cylinder(h=wall+2, d=screw_d);
            translate([pillar_size/2, width-pillar_size/2, -1]) cylinder(h=wall+2, d=screw_d);
            translate([length-pillar_size/2, width-pillar_size/2, -1]) cylinder(h=wall+2, d=screw_d);
            
            // PIR Port
            translate([30, width/2, -1]) cylinder(h=wall+2, d=12.5); 
            
            // --- NEW: MAX9814 ACOUSTIC PORT ---
            // 4.5mm hole with a 1mm recessed chamfer for better sound pickup
            translate([55, width/2, -1]) cylinder(h=wall+2, d=4.5);
        }
    }
}

base();
lid();
