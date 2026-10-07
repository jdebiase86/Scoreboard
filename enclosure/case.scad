// Scoreboard case - 2 inch deep box, the screen is the front.
// Prints open side up, no supports. Needs 4x M3 x 30 mm screws (screen, its
// brass inserts are M3) and 4x M2 x 4 mm heat-set inserts + 4x M2 x 6 mm screws (board).
// part = "case" (default), "test_screen" or "test_board" - see the bottom of the file.
// Coordinates as seen from the FRONT: x right, y up, z from the back (0) to the front.
// Screen held upright with its wires at the bottom.
panel = 191.9; panel_t = 14.4; clr = 0.4; wall = 2.5;
inner = panel + 2*clr; outer = inner + 2*wall;
depth = 50.8; floor_t = 3; recess = 1.0; corner_r = 4;
panel_back_z = depth - recess - panel_t;   // posts stop the screen here

// four corner brass inserts, panel coordinates (from photos, +/- 2 mm)
ins = [[10, 22], [panel - 10, 22], [10, panel - 22], [panel - 10, panel - 22]];
post_d = 11; screw_hole = 4.2;   // oversize M3 clearance for photo error
head_d = 8; screw_len = 30; bite = 6;   // M3 x 30 mm, ~6 mm into the insert
pocket_h = panel_back_z - (screw_len - bite);

// Seengreat board, USB/wheel edge against the right wall
// bw = depth into the case, bl = along the right wall (the USB/wheel edge is a short side).
// Hole spacing measured on the board: 60 mm along the long side, 51.65 mm along the short side.
bw = 64.75; bl = 57.5; hx = 60; hy = 51.65;
by0 = 34; standoff_h = 11; sd = 7.5;
// M2 x 4 mm heat-set inserts (kit: 3.0 mm top, 2.7 mm bottom): 2.9 mm hole, 6 mm deep
insert_d = 2.9; insert_depth = 6;
// The board's USB/wheel edge sits edge_in mm into a pocket in the wall, so the
// wheel sticks out further (was 0.8 mm short of the wall).
edge_in = 0.7; pocket = 1.0;
bx0 = outer - wall + edge_in - bw;
board_z = floor_t + standoff_h;
$fn = 48;

function P(p) = [p[0] + wall + clr, p[1] + wall + clr];
module rsq(s, r) offset(r) offset(-r) square(s);

module shell() difference() {
  linear_extrude(depth) rsq([outer, outer], corner_r);
  translate([wall, wall, floor_t]) cube([inner, inner, depth]);
}
module posts() for (p = ins) translate(concat(P(p), 0)) cylinder(d = post_d, h = panel_back_z);
module post_holes() for (p = ins) translate(concat(P(p), -1)) {
  cylinder(d = head_d, h = pocket_h + 1);
  cylinder(d = screw_hole, h = depth);
}
function bholes() = [for (i = [-1, 1], j = [-1, 1]) [bx0 + bw/2 + i*hx/2, by0 + bl/2 + j*hy/2]];
module standoffs() for (h = bholes()) translate(concat(h, 0)) difference() {
  cylinder(d = sd, h = board_z);
  translate([0, 0, board_z - insert_depth]) cylinder(d = insert_d, h = insert_depth + 1);
}
module side_slot() {  // wheel + USB-C: two openings with a bar between the wheel and the first USB-C
  cy = by0 + bl/2;
  translate([outer - wall - 0.01, cy - slot_len/2, slot_z0]) cube([wall + 1, slot_len/2 + bar_y - bar_w/2, slot_z1 - slot_z0]);
  translate([outer - wall - 0.01, cy + bar_y + bar_w/2, slot_z0]) cube([wall + 1, slot_len/2 - bar_y - bar_w/2, slot_z1 - slot_z0]);
  // pocket on the inside of the wall for the board's edge
  translate([outer - wall - 0.01, by0 - 1, board_z]) cube([pocket + 0.01, bl + 2, 6.75]);
}
// Fitted on test print 3: opening from 1.35 to 5.75 mm above the board's underside, 48 mm long,
// with an 8 mm bar centred 2.75 mm below the board's middle (wheel side), filling the gap
// between the wheel and the first USB-C port.
slot_len = 48; slot_z0 = board_z + 1.35; slot_z1 = board_z + 5.75; bar_w = 8; bar_y = -2.75;
module vents() {
  r = 5; g = 2.4; dx = 2*r*cos(30) + g; dy = 1.5*r + g*cos(30); c = [outer/2 - 22, outer/2 + 10];
  intersection() {
    translate(concat(c, -1)) cylinder(r = 42, h = floor_t + 2);
    for (i = [-8:8], j = [-8:8]) translate([c[0] + i*dx + (j % 2)*dx/2, c[1] + j*dy, -1])
      rotate(30) cylinder(r = r, h = floor_t + 2, $fn = 6);
  }
}
module keyhole() translate([0, 0, -1]) {
  cylinder(d = 9, h = floor_t + 2);
  translate([-2.25, 0, 0]) cube([4.5, 9, floor_t + 2]);
  translate([0, 9, 0]) cylinder(d = 4.5, h = floor_t + 2);
}
module box() difference() {
  union() { shell(); posts(); standoffs(); }
  post_holes(); side_slot(); vents();
  for (x = [outer/2 - 55, outer/2 + 55]) translate([x, outer - 30, 0]) keyhole();
}

part = "case";
show_box = true; show_board = false; show_panel = false; cut = false;
module all() {
  if (show_box) color("#3a3d42") box();
  if (show_board) {
    color("darkgreen") translate([bx0, by0, board_z]) cube([bw, bl, 1.6]);
    color("dimgray") translate([bx0 + 10, by0 + 12, board_z - 9.2]) cube([30, 40, 9.2]);   // speaker side
    color("silver") translate([bx0 + 8, by0 + 30, board_z + 1.6]) cube([22, 20, 9]);       // ribbon plug
    color("orange") translate([outer - wall + 1.5, by0 + 45, board_z + 5]) rotate([0, 90, 0]) cylinder(d = 12, h = 3, center = true);
    color("gold") translate([outer - wall - 7, by0 + 22, board_z + 1.6]) cube([7.5, 9, 3.3]);
  }
  if (show_panel) translate([wall + clr, wall + clr, panel_back_z]) {
    color("#222") cube([panel, panel, panel_t - 1.2]);
    color("#555", 0.9) translate([0, 0, panel_t - 1.2]) cube([panel, panel, 1.2]);
  }
}
// Test piece 1: a thin frame that drops over the back of the screen; check that
// all four screw holes land on the screen's brass inserts.
module test_screen() {
  t = 2; lip = 5; band = 30;
  difference() {
    union() {
      difference() {   // edge band plus a lip that wraps the screen's edge
        linear_extrude(t + lip) rsq([outer, outer], corner_r);
        translate([wall, wall, t]) cube([inner, inner, lip + 1]);
        translate([band, band, -1]) cube([outer - 2*band, outer - 2*band, t + lip + 2]);
      }
      for (p = ins) translate(concat(P(p), 0)) cylinder(d = post_d + 4, h = t);
    }
    for (p = ins) translate(concat(P(p), -1)) cylinder(d = screw_hole, h = t + 2);
  }
}
// Test piece 2: the lower right corner of the case with the board posts and the
// wheel/USB slot; press in the inserts, screw the board on, try the wheel and cable.
module test_board() difference() {
  intersection() {   // just the board's footprint, its posts and the slot wall
    box();
    translate([bx0 - 3, by0 - 3, -1]) cube([outer, bl + 6, board_z + 6.75 + 3 + 1]);
  }
  // open up the middle of the floor to save plastic
  translate([bx0 + 8, by0 + 8, -1]) cube([bw - 18, bl - 16, floor_t + 2]);
}
if (part == "test_screen") test_screen();
else if (part == "test_board") test_board();
else if (cut) difference() { all(); translate([-1, -1, -1]) cube([outer + 2, outer / 2 + 1, 100]); } else all();
explode = 0;
if (explode > 0) translate([0, 0, explode]) translate([wall + clr, wall + clr, panel_back_z]) {
  color("#222") cube([panel, panel, panel_t - 1.2]);
  color("#555") translate([0, 0, panel_t - 1.2]) cube([panel, panel, 1.2]);
}
