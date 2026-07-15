include <plane_mount_config.scad>
$fn = 48;

// Mockup mounting plate for the plane: shows where the main PCB, the two
// daughter boards, and the four control-surface servos would sit relative
// to each other. Not a functional flight part.

plate();

module plate() {
    difference() {
        union() {
            base_plate();
            if (standoff_enable) board_standoffs();
        }
        board_standoff_holes();
        servo_holes();
        servo_cutouts();
    }
}

module base_plate() {
    cube([plate_w, plate_l, plate_thickness]);
}

// ===================================================================
// PCB standoffs
// ===================================================================

module standoff(x, y) {
    translate([x, y, plate_thickness])
    difference() {
        cylinder(d = standoff_od, h = standoff_height);
        translate([0, 0, -0.5])
            cylinder(d = standoff_hole_d, h = standoff_height + 1);
    }
}

function board_corners(ox, oy, w, l) = [
    [ox + standoff_inset, oy + standoff_inset],
    [ox + w - standoff_inset, oy + standoff_inset],
    [ox + standoff_inset, oy + l - standoff_inset],
    [ox + w - standoff_inset, oy + l - standoff_inset],
];

module board_standoffs() {
    for (p = board_corners(main_origin_x, main_origin_y, pcb_main_w, pcb_main_l))
        standoff(p[0], p[1]);
    for (p = board_corners(d1_origin_x, d1_origin_y, pcb_d1_w, pcb_d1_l))
        standoff(p[0], p[1]);
    for (p = board_corners(d2_origin_x, d2_origin_y, pcb_d2_w, pcb_d2_l))
        standoff(p[0], p[1]);
}

// Pilot holes through the plate itself, under each standoff, so a screw
// can pass through if the standoff is used as a real mounting boss.
module board_standoff_holes() {
    for (p = concat(
        board_corners(main_origin_x, main_origin_y, pcb_main_w, pcb_main_l),
        board_corners(d1_origin_x, d1_origin_y, pcb_d1_w, pcb_d1_l),
        board_corners(d2_origin_x, d2_origin_y, pcb_d2_w, pcb_d2_l)
    ))
        translate([p[0], p[1], -0.5])
            cylinder(d = standoff_hole_d, h = plate_thickness + 1);
}

// ===================================================================
// Servos
// ===================================================================

// Two mounting holes per servo, centered in the footprint and spaced
// servo_hole_spacing apart along Y.
module servo_holes() {
    for (i = [0 : servo_count - 1]) {
        cx = servo_origin_x(i) + servo_body_w / 2;
        cy = servo_origin_y(i) + servo_body_l / 2;
        for (dy = [-servo_hole_spacing / 2, servo_hole_spacing / 2])
            translate([cx, cy + dy, -0.5])
                cylinder(d = servo_hole_d, h = plate_thickness + 1);
    }
}

// Through-opening under each servo so the case (servo_body_h tall below the
// tab plane) has clearance to pass through the plate, plus a side notch for
// the cable exiting the case. The tabs themselves rest on the solid rim
// left around the cutout (servo_tab_l / servo_side_margin).
module servo_cutouts() {
    for (i = [0 : servo_count - 1]) {
        ox = servo_origin_x(i) + (servo_body_w - servo_cutout_w) / 2;
        oy = servo_origin_y(i) + servo_tab_l;
        translate([ox, oy, -0.5])
        union() {
            cube([servo_cutout_w, servo_cutout_l, plate_thickness + 1]);

            cable_x = servo_cable_side < 0 ? -servo_cable_ext : servo_cutout_w;
            translate([cable_x, (servo_cutout_l - servo_cable_w) / 2, 0])
                cube([servo_cable_ext, servo_cable_w, plate_thickness + 1]);
        }
    }
}
