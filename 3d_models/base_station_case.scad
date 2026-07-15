include <base_station_config.scad>
$fn = 48;

// "base"       -> base tray only, print-ready
// "lid"        -> lid only, print-ready (flat face up as modeled; rotate 180 degrees
//                 in your slicer so the skirt points down onto the bed)
// "both"       -> base + lid laid out side by side, for a quick visual check
// "assembled"  -> lid placed on top of the base, for a fit check (not for printing)
part = "both";

if (part == "base") {
    case_base();
} else if (part == "lid") {
    case_lid();
} else if (part == "assembled") {
    case_base();
    translate([0, 0, floor_thickness + case_inner_h])
        case_lid();
} else {
    case_base();
    translate([case_outer_w + 10, 0, 0])
        case_lid();
}

// ===================================================================
// Base tray
// ===================================================================

module case_base() {
    difference() {
        union() {
            case_shell();
            lora_boss();
            if (standoff_enable) standoffs();
        }
        lora_bore();
        snap_groove_ring();
    }
}

module case_shell() {
    outer_h = floor_thickness + case_inner_h;
    difference() {
        cube([case_outer_w, case_outer_l, outer_h]);
        translate([wall_thickness, wall_thickness, floor_thickness])
            cube([case_inner_w, case_inner_l, case_inner_h + 1]);
    }
}

// Continuous groove cut into the outside of the base wall, just below the top edge,
// that the lid's snap bumps click into.
module snap_groove_ring() {
    z0 = floor_thickness + case_inner_h - groove_offset_from_top - tab_fit_clearance;
    groove_h = snap_tab_h + tab_fit_clearance * 1.1;
    translate([0, 0, z0])
    difference() {
        cube([case_outer_w, case_outer_l, groove_h]);
        translate([snap_tab_depth, snap_tab_depth, -0.5])
            cube([case_outer_w - 2 * snap_tab_depth, case_outer_l - 2 * snap_tab_depth, groove_h + 1]);
    }
}

module standoff(x, y) {
    translate([x, y, floor_thickness])
    difference() {
        cylinder(d = standoff_od, h = standoff_height);
        translate([0, 0, -0.5])
            cylinder(d = standoff_hole_d, h = standoff_height + 1);
    }
}

module standoffs() {
    for (p = [
        [main_origin_x + standoff_inset, main_origin_y + standoff_inset],
        [main_origin_x + pcb_main_w - standoff_inset, main_origin_y + standoff_inset],
        [main_origin_x + standoff_inset, main_origin_y + pcb_main_l - standoff_inset],
        [main_origin_x + pcb_main_w - standoff_inset, main_origin_y + pcb_main_l - standoff_inset],
    ]) standoff(p[0], p[1]);

    for (p = [
        [daughter_origin_x + standoff_inset, daughter_origin_y + standoff_inset],
        [daughter_origin_x + pcb_daughter_w - standoff_inset, daughter_origin_y + standoff_inset],
        [daughter_origin_x + standoff_inset, daughter_origin_y + pcb_daughter_l - standoff_inset],
        [daughter_origin_x + pcb_daughter_w - standoff_inset, daughter_origin_y + pcb_daughter_l - standoff_inset],
    ]) standoff(p[0], p[1]);
}

// Boss protrudes outward through the x=0 wall so the LoRa antenna's threaded
// connector can be secured from outside with its nut.
module lora_boss() {
    translate([-lora_boss_extra_len, lora_mount_y, lora_mount_z])
        rotate([0, 90, 0])
            cylinder(d = lora_boss_od, h = lora_boss_extra_len + wall_thickness);
}

module lora_bore() {
    translate([-lora_boss_extra_len - 1, lora_mount_y, lora_mount_z])
        rotate([0, 90, 0])
            cylinder(d = lora_hole_d, h = lora_boss_extra_len + wall_thickness + 2);
}

// ===================================================================
// Lid
// ===================================================================

module case_lid() {
    difference() {
        union() {
            lid_plate();
            lid_skirt();
            snap_bumps();
        }
        translate([vent_x, vent_y, -1])
            cylinder(d = vent_hole_d, h = lid_thickness + 2, $fn = 24);
    }
}

module lid_plate() {
    translate([lid_offset_x, lid_offset_y, 0])
        cube([lid_outer_w, lid_outer_l, lid_thickness]);
}

module lid_skirt() {
    cx0 = -tab_fit_clearance;
    cx1 = case_outer_w + tab_fit_clearance;
    cy0 = -tab_fit_clearance;
    cy1 = case_outer_l + tab_fit_clearance;
    translate([0, 0, -lid_skirt_height])
    difference() {
        translate([lid_offset_x, lid_offset_y, 0])
            cube([lid_outer_w, lid_outer_l, lid_skirt_height + lid_thickness]);
        translate([cx0, cy0, -1])
            cube([cx1 - cx0, cy1 - cy0, lid_skirt_height + lid_thickness + 2]);
    }
}

// A small tapered nub that protrudes in +X from the x=0 plane; instantiated on
// each wall below with the right translate/rotate to point inward.
module snap_bump() {
    d = snap_tab_depth + tab_fit_clearance;
    hull() {
        cube([0.01, snap_tab_w, snap_tab_h], center = true);
        translate([d, 0, 0])
            cube([0.01, snap_tab_w * 0.4, snap_tab_h * 0.4], center = true);
    }
}

function spaced(count, length, margin) =
    [ for (i = [0 : count - 1]) margin + (length - 2 * margin) * (count <= 1 ? 0.5 : i / (count - 1)) ];

module snap_bumps() {
    margin = 8;
    cx0 = -tab_fit_clearance;
    cx1 = case_outer_w + tab_fit_clearance;
    cy0 = -tab_fit_clearance;
    cy1 = case_outer_l + tab_fit_clearance;
    bump_z = -groove_offset_from_top + (snap_tab_h + tab_fit_clearance) / 2;

    for (x = spaced(snap_tabs_long_side, case_outer_w, margin)) {
        translate([x, cy0, bump_z]) rotate([0, 0, 90]) snap_bump();
        translate([x, cy1, bump_z]) rotate([0, 0, -90]) snap_bump();
    }
    for (y = spaced(snap_tabs_short_side, case_outer_l, margin)) {
        translate([cx0, y, bump_z]) snap_bump();
        translate([cx1, y, bump_z]) rotate([0, 0, 180]) snap_bump();
    }
}
