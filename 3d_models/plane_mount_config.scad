// ===================================================================
// Plane mounting plate - tunable dimensions.
// This is a MOCKUP to show component layout, not a flight-ready part:
// no fuselage attachment, wiring channels, or weight optimization.
// Edit values here; plane_mount_plate.scad only reads these parameters.
// ===================================================================

// ---- Main PCB (flight controller) ----
pcb_main_w = 50;   // X
pcb_main_l = 70;   // Y
pcb_main_h = 20;   // Z, tallest component on the board (for standoff clearance only)

// ---- Daughter board 1 ----
pcb_d1_w = 20;  // X
pcb_d1_l = 80;  // Y
pcb_d1_h = 20;  // Z

// ---- Daughter board 2 ----
pcb_d2_w = 40;  // X
pcb_d2_l = 60;  // Y
pcb_d2_h = 20;  // Z

// ---- Plate ----
plate_thickness = 3;
edge_margin     = 5;   // border from outermost feature to plate edge
board_gap       = 5;   // gap between adjacent boards in the row
section_gap     = 10;  // gap between the board row and the servo row

// ---- PCB standoffs (self-tapping screw bosses, same style as the base station case) ----
standoff_enable = true;
standoff_height = 6.9;
standoff_od     = 8;
standoff_hole_d = 4;    // pilot hole for an M2.5 self-tap screw
standoff_inset  = 1.8;  // distance of standoff center from each board corner

// ---- Servos ----
// The two mounting holes sit on the tab plane; the servo case hangs
// servo_body_h below that plane. The plate is cut through under each
// servo so the case has room to pass through it (or the servo can just
// stand on top with the case in open air below - either way the plate
// itself must not block that space).
servo_count        = 4;
servo_hole_spacing = 27.5;  // distance between a servo's two mounting holes (front-to-back)
servo_hole_d       = 2.2;   // pilot hole for an M2 self-tap screw
servo_body_w       = 20;    // full footprint, X (across the tabs)
servo_body_l       = 40;    // full footprint, Y (tab-to-tab, along the hole spacing)
servo_body_h       = 17;    // case height below the mounting-hole/tab plane (clearance only, not modeled in Z)
servo_tab_l        = 8;     // length of solid plate left under each tab (must clear the hole, which sits (body_l - hole_spacing)/2 in from the end)
servo_side_margin  = 1.5;   // solid plate rim left on each side of the body cutout, for print strength
servo_cable_w      = 6;     // width (along Y) of the cable-exit notch on the side of the body cutout
servo_cable_ext    = 4;     // how far the cable notch extends past the body cutout, through the side rim
servo_cable_side   = -1;    // which side the cable exits: -1 = -X edge, +1 = +X edge (same for all servos)
servo_row_gap      = 10;    // gap between adjacent servos

// ---- Derived servo cutout (the body opening, centered in the footprint) ----
servo_cutout_w = servo_body_w - 2 * servo_side_margin;
servo_cutout_l = servo_body_l - 2 * servo_tab_l;

// ---- Derived board row (main + 2 daughter boards, left to right) ----
board_row_w = pcb_main_w + board_gap + pcb_d1_w + board_gap + pcb_d2_w;
board_row_l = max(pcb_main_l, pcb_d1_l, pcb_d2_l);

// ---- Derived servo row ----
servo_row_w = servo_count * servo_body_w + (servo_count - 1) * servo_row_gap;
servo_row_l = servo_body_l;

// ---- Derived plate footprint ----
plate_w = max(board_row_w, servo_row_w) + 2 * edge_margin;
plate_l = board_row_l + section_gap + servo_row_l + 2 * edge_margin;

// ---- Board placement: lower-left corner of each board's footprint, in plate coordinates ----
// Board row is centered along X, sits above the servo row.
board_row_x0  = (plate_w - board_row_w) / 2;
board_row_y0  = edge_margin + servo_row_l + section_gap;

main_origin_x = board_row_x0;
main_origin_y = board_row_y0;
d1_origin_x   = main_origin_x + pcb_main_w + board_gap;
d1_origin_y   = board_row_y0;
d2_origin_x   = d1_origin_x + pcb_d1_w + board_gap;
d2_origin_y   = board_row_y0;

// ---- Servo placement: lower-left corner of each servo's footprint, in plate coordinates ----
// Servo row is centered along X, sits along the bottom edge.
servo_row_x0 = (plate_w - servo_row_w) / 2;
servo_row_y0 = edge_margin;

function servo_origin_x(i) = servo_row_x0 + i * (servo_body_w + servo_row_gap);
function servo_origin_y(i) = servo_row_y0;
