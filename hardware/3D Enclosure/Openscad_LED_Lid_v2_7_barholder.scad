// Sentinel ESP32 Node Enclosure v2.7 Horizontal Bar Holder Variant
// Purpose:
// - visibly match one intact 3-LED horizontal strip
// - keep the LED bar away from the PIR hole by lowering it
// - keep wall-mount features
// - keep the original screw holes, PIR port, mic port, vents, and BME divider intact

$fn = 64;
wall = 2.0;
length = 95;
width = 65;
height = 35;
screw_d = 3.2;
pillar_size = 8;

// Preview helpers
show_led_preview_labels = false;
label_height = 0.8;
label_size = 4;

// LED bar layout
// Intact LED strip: 30mm x 15mm with wiring on both left and right ends.
// Lowered to maintain clearer separation from the PIR opening.
led_bar_center = [39, 16];
led_strip_w = 30;
led_strip_h = 15;

// Visible windows for left / center / right LEDs
led_window_w = 5;
led_window_h = 5;
led_window_r = 1;
led_window_centers = [
    [29, 16],
    [39, 16],
    [49, 16]
];

// Holder geometry
led_backing_w = 30;
led_backing_h = 15;
led_backing_depth = 0.8;
led_bottom_ledge_h = 2.0;
led_bottom_ledge_depth = 1.5;
led_side_stop_w = 1.5;
led_side_stop_h = 9;
led_side_stop_depth = 1.3;
led_top_tab_w = 3.5;
led_top_tab_h = 1.5;
led_top_tab_depth = 1.3;
led_top_tab_offsets = [-11, 11];
wire_notch_w = 4;
wire_notch_h = 6;

// Baffles and front bezel
led_baffle_thickness = 2.5;
led_baffle_depth = 1.0;
led_bezel_w = 36;
led_bezel_h = 12;
led_bezel_r = 2;
led_bezel_depth = 0.6;

// Wall mount keyholes in the base back panel.
mount_keyhole_head_d = 7;
mount_keyhole_neck_w = 4;
mount_keyhole_slot_len = 8;
mount_keyhole_x = [24, 68];
mount_keyhole_y = width / 2;

module rounded_rect_2d(w, h, r) {
    hull() {
        translate([-(w/2-r), -(h/2-r)]) circle(r=r);
        translate([ (w/2-r), -(h/2-r)]) circle(r=r);
        translate([-(w/2-r),  (h/2-r)]) circle(r=r);
        translate([ (w/2-r),  (h/2-r)]) circle(r=r);
    }
}

module keyhole_2d(head_d, neck_w, slot_len) {
    union() {
        translate([0, slot_len/2]) circle(d=head_d);
        translate([-neck_w/2, -slot_len/2]) square([neck_w, slot_len]);
    }
}

module led_window_cutouts() {
    for (center = led_window_centers) {
        translate([center[0], center[1], -1])
            linear_extrude(height=wall + 2)
                rounded_rect_2d(led_window_w, led_window_h, led_window_r);
    }
}

module led_diffusers() {
    translate([0, width + 20, 0]) {
        for (center = led_window_centers) {
            translate([center[0], center[1], 0])
                linear_extrude(height=wall)
                    rounded_rect_2d(led_window_w, led_window_h, led_window_r);
        }
    }
}

module led_preview_labels() {
    if (show_led_preview_labels) {
        translate([0, width + 20, 0]) {
            color([0.1, 0.7, 1.0, 0.8]) {
                translate([29, 22, wall])
                    linear_extrude(height=label_height)
                        text("L", size=label_size, halign="center", valign="center");
                translate([39, 22, wall])
                    linear_extrude(height=label_height)
                        text("C", size=label_size, halign="center", valign="center");
                translate([49, 22, wall])
                    linear_extrude(height=label_height)
                        text("R", size=label_size, halign="center", valign="center");
            }
        }
    }
}

module base() {
    difference() {
        cube([length, width, height]);
        translate([wall, wall, wall])
            cube([length-wall*2, width-wall*2, height]);

        for (i = [0:4]) {
            translate([length-15, width-wall-1, 8 + i*4]) cube([10, wall+2, 2]);
            translate([length-15, -1, 8 + i*4]) cube([10, wall+2, 2]);
        }

        translate([-1, width/2-6, wall+2]) cube([wall+2, 12, 8]);
        translate([length-wall-1, 10, height-8]) cube([wall+2, 4, 10]);

        for (x = mount_keyhole_x) {
            translate([x, mount_keyhole_y, -1])
                linear_extrude(height=wall + 2)
                    keyhole_2d(mount_keyhole_head_d, mount_keyhole_neck_w, mount_keyhole_slot_len);
        }
    }

    for (x = [pillar_size/2, length-pillar_size/2])
        for (y = [pillar_size/2, width-pillar_size/2])
            translate([x, y, wall]) difference() {
                cylinder(h=height-wall, d=pillar_size);
                cylinder(h=height, d=screw_d);
            }

    translate([length-22, wall, wall]) cube([2, width-wall*2, height-wall-5]);
}

module led_retainer() {
    translate([led_bar_center[0], led_bar_center[1], 0]) {
        // Thin backing shelf for the full strip.
        translate([-(led_backing_w/2), -(led_backing_h/2), -led_backing_depth])
            cube([led_backing_w, led_backing_h, led_backing_depth]);

        // Bottom ledge supports the strip from below.
        translate([-(led_backing_w/2), -(led_backing_h/2) - led_bottom_ledge_h, -led_bottom_ledge_depth])
            cube([led_backing_w, led_bottom_ledge_h, led_bottom_ledge_depth]);

        // Slim side stops centered vertically, leaving top corners visually open.
        translate([-(led_backing_w/2), -(led_side_stop_h/2), -led_side_stop_depth])
            cube([led_side_stop_w, led_side_stop_h, led_side_stop_depth]);
        translate([(led_backing_w/2) - led_side_stop_w, -(led_side_stop_h/2), -led_side_stop_depth])
            cube([led_side_stop_w, led_side_stop_h, led_side_stop_depth]);

        // Small top tabs near the ends hold the strip in place without a full top rail.
        for (x_off = led_top_tab_offsets) {
            translate([x_off - led_top_tab_w/2, (led_backing_h/2) - led_top_tab_h, -led_top_tab_depth])
                cube([led_top_tab_w, led_top_tab_h, led_top_tab_depth]);
        }

        // Wire relief notches at left and right ends of the bar.
        translate([-(led_backing_w/2) - 0.1, -(wire_notch_h/2), -led_side_stop_depth - 0.1])
            cube([wire_notch_w + 0.2, wire_notch_h, led_side_stop_depth + 0.2]);
        translate([(led_backing_w/2) - wire_notch_w + 0.1, -(wire_notch_h/2), -led_side_stop_depth - 0.1])
            cube([wire_notch_w + 0.2, wire_notch_h, led_side_stop_depth + 0.2]);

        // Light baffles between left/center/right windows.
        for (divider_x = [34, 44]) {
            translate([divider_x - led_bar_center[0] - led_baffle_thickness/2, -(led_backing_h/2), -led_baffle_depth])
                cube([led_baffle_thickness, led_backing_h, led_baffle_depth]);
        }
    }
}

module lid_body() {
    translate([0, width + 20, 0]) {
        union() {
            difference() {
                cube([length, width, wall]);

                translate([pillar_size/2, pillar_size/2, -1]) cylinder(h=wall+2, d=screw_d);
                translate([length-pillar_size/2, pillar_size/2, -1]) cylinder(h=wall+2, d=screw_d);
                translate([pillar_size/2, width-pillar_size/2, -1]) cylinder(h=wall+2, d=screw_d);
                translate([length-pillar_size/2, width-pillar_size/2, -1]) cylinder(h=wall+2, d=screw_d);

                translate([30, width/2, -1]) cylinder(h=wall+2, d=12.5);
                translate([55, width/2, -1]) cylinder(h=wall+2, d=4.5);

                // Horizontal bar bezel for the 3-LED strip.
                translate([led_bar_center[0], led_bar_center[1], wall - led_bezel_depth])
                    linear_extrude(height=led_bezel_depth + 0.1)
                        rounded_rect_2d(led_bezel_w, led_bezel_h, led_bezel_r);

                led_window_cutouts();
            }

            led_retainer();
        }
    }
}

base();
lid_body();
led_diffusers();
led_preview_labels();