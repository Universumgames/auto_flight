// ===================================================================
// Base station case - tunable dimensions.
// Edit values here; case.scad only reads these parameters.
// ===================================================================

// ---- Main PCB (Heltec LoRa32 V3 + GPS module) ----
pcb_main_w = 50;   // X
pcb_main_l = 70;   // Y
pcb_main_h = 26;   // Z, tallest component on the board

// ---- Daughter board (barometer), connected to main PCB by a flying cable ----
pcb_daughter_w = 20;  // X
pcb_daughter_l = 80;  // Y
pcb_daughter_h = 26;  // Z

// ---- Shell thickness ----
wall_thickness  = 1.5;
floor_thickness = 2;
lid_thickness   = 1.5;

// ---- Internal clearances ----
board_clearance_xy = 1.5;  // air gap between a board's edge and the inner wall
board_gap           = 3;   // gap between main PCB and daughter board
top_clearance        = 5;   // clearance above components before the lid underside

// ---- PCB standoffs (self-tapping screw bosses) ----
standoff_enable  = true;
standoff_height  = 6.9;
standoff_od      = 8;
standoff_hole_d  = 4;   // pilot hole for an M2.5 self-tap screw
standoff_inset   = 1.8;     // distance of standoff center from each board corner

// ---- Derived case interior / exterior footprint ----
case_inner_w = pcb_main_w + board_gap + pcb_daughter_w + 2 * board_clearance_xy;
case_inner_l = max(pcb_main_l, pcb_daughter_l) + 2 * board_clearance_xy;
case_inner_h = max(pcb_main_h, pcb_daughter_h) + standoff_height + top_clearance;

case_outer_w = case_inner_w + 2 * wall_thickness;
case_outer_l = case_inner_l + 2 * wall_thickness;

// Board placement: lower-left corner of each board's footprint, in case coordinates.
// Main PCB sits against the x=0 wall (where the LoRa antenna exits); daughter board
// sits alongside it towards +x.
main_origin_x     = wall_thickness + board_clearance_xy;
main_origin_y     = wall_thickness + board_clearance_xy;
daughter_origin_x = main_origin_x + pcb_main_w + board_gap;
daughter_origin_y = wall_thickness + board_clearance_xy;

// ---- LoRa antenna bulkhead mount (SMA-bulkhead style, on the x=0 wall next to the main PCB) ----
lora_thread_d       = 6.2;  // antenna connector thread diameter (given)
lora_hole_clearance = 0.3;  // extra hole diameter over the thread for a clean fit
lora_hole_d         = lora_thread_d + lora_hole_clearance;
lora_boss_od        = 11;   // outer diameter of the printed boss (nut bearing surface)
lora_boss_extra_len = 1;    // boss protrusion beyond the outer wall face (thread purchase for the nut)
lora_mount_y        = main_origin_y + pcb_main_l / 2;                     // centered along the main PCB
lora_mount_z        = floor_thickness + standoff_height + pcb_main_h / 2; // centered on the main PCB height

// ---- Barometer pressure-equalization vent (in the lid, above the daughter board) ----
vent_hole_d = 2.5;
vent_x = daughter_origin_x + pcb_daughter_w / 2;
vent_y = daughter_origin_y + pcb_daughter_l / 2;

// ---- Snap-fit lid retention ----
snap_tab_w              = 4;    // width of each bump
snap_tab_h              = 1.4;  // height of each bump
snap_tab_depth          = 0.8;  // how far the bump reaches into the groove
tab_fit_clearance       = 0.5; // gap so the lid skirt slides over the base
lid_skirt_height        = 6;    // depth of the lid skirt around the base
groove_offset_from_top  = 3;  // distance from the base's top edge down to the groove
snap_tabs_long_side     = 2;    // bumps on each long (Y) skirt wall
snap_tabs_short_side    = 1;    // bumps on each short (X) skirt wall

// ---- Derived lid footprint (skirt sits just outside the base, plus its own wall) ----
lid_offset_x = -tab_fit_clearance - wall_thickness;
lid_offset_y = -tab_fit_clearance - wall_thickness;
lid_inner_w  = case_outer_w + 2 * tab_fit_clearance;
lid_inner_l  = case_outer_l + 2 * tab_fit_clearance;
lid_outer_w  = lid_inner_w + 2 * wall_thickness;
lid_outer_l  = lid_inner_l + 2 * wall_thickness;
