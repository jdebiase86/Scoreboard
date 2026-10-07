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

// four corner brass inserts, panel coordinates: 11 mm in from the left/right sides,
// 22 mm from the top/bottom (wires side = bottom). Checked on test frame 1 (was 10).
ins = [[11, 22], [panel - 11, 22], [11, panel - 22], [panel - 11, panel - 22]];
post_d = 11; screw_hole = 4.2;   // oversize M3 clearance for photo error
head_d = 8; screw_len = 30; bite = 6;   // M3 x 30 mm, ~6 mm into the insert
pocket_h = panel_back_z - (screw_len - bite);

// Seengreat board, USB/wheel edge against the right wall
// bw = depth into the case, bl = along the right wall (the USB/wheel edge is a short side).
// Hole spacing measured on the board: 60 mm along the long side, 51.65 mm along the short side.
bw = 64.75; bl = 57.5; hx = 60; hy = 51.65;
by0 = 34; standoff_h = 11; sd = 7.5;
// Everything about the board is drawn against the RIGHT wall and then turned 180 degrees
// onto the LEFT wall (bottom left seen from the front) when board_left is true.
board_left = true;
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
module place() if (board_left)
    translate([outer/2, by0 + bl/2, 0]) rotate(180) translate([-outer/2, -(by0 + bl/2), 0]) children();
  else children();
module side_slot() {  // wheel + USB-C: two openings with a bar between the wheel and the first USB-C
  cy = by0 + bl/2;
  translate([outer - wall - 0.01, cy - slot_len/2, slot_z0]) cube([wall + 1, slot_len/2 + bar_y - bar_w/2, slot_z1 - slot_z0]);
  translate([outer - wall - 0.01, cy + bar_y + bar_w/2, slot_z0]) cube([wall + 1, slot_len/2 - bar_y - bar_w/2, slot_z1 - slot_z0]);
  // pocket on the inside of the wall for the board's edge
  translate([outer - wall - 0.01, by0 - 1, board_z]) cube([pocket + 0.01, bl + 2, 6.75]);
}
// Fitted on test print 3: opening from 1.35 to 5.75 mm above the board's underside, 48 mm long,
// with an 8 mm bar centred 4.2 mm below the board's middle (wheel side), filling the gap
// between the wheel and the first USB-C port.
slot_len = 48; slot_z0 = board_z + 1.35; slot_z1 = board_z + 5.75; bar_w = 8; bar_y = -4.2;
module vents() {
  r = 5; g = 2.4; dx = 2*r*cos(30) + g; dy = 1.5*r + g*cos(30); c = [outer/2 + (board_left ? 22 : -22), outer/2 + 10];
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
  union() { shell(); posts(); place() standoffs(); }
  post_holes(); place() side_slot(); vents();
  for (x = [outer/2 - 55, outer/2 + 55]) translate([x, outer - 30, 0]) keyhole();
}

part = "case";
show_box = true; show_board = false; show_panel = false; cut = false;
module board_model() {   // rough stand-in for pictures only
  cy = by0 + bl/2; ex = bx0 + bw; top = board_z + 1.6;
  color("#24324a") translate([bx0, by0, board_z]) cube([bw, bl, 1.6]);
  color("#111") translate([bx0 + 18, cy - 10, top]) cube([26, 34, 9]);                  // speaker
  color("#222") translate([bx0 + 1, cy - 25, top]) cube([9, 26, 9]);                    // HUB75 socket
  color("silver") translate([bx0 + 30, by0 + 4, top]) cube([14, 15, 2]);                // microSD
  color("#c0c0c0") for (dy = [7.5, 19.3]) translate([ex - 7.3, cy + dy - 4.5, top]) cube([7.5, 9, 3.2]); // USB-C x2
  color("#e07b00") translate([ex - 1.4, cy - 15, top + 1.8]) cylinder(d = 14, h = 3.4, center = true, $fn = 64); // wheel
}
module all() {
  if (show_box) color("#3a3d42") box();
  if (show_board) place() board_model();
  if (show_panel) translate([wall + clr, wall + clr, panel_back_z]) {
    color("#222") cube([panel, panel, panel_t - 1.2]);
    color("#555", 0.9) translate([0, 0, panel_t - 1.2]) cube([panel, panel, 1.2]);
  }
}
// Test piece 1: a thin frame that drops over the back of the screen; check that
// all four screw holes land on the screen's brass inserts.
module test_screen() {
  t = 1.2; ring = 5.5; lip = panel_t; leg = 22; pad = 15;   // corner lips reach the LED face, like the case walls
  difference() {
    union() {
      difference() {   // thin outer ring
        linear_extrude(t) rsq([outer, outer], corner_r);
        translate([ring, ring, -1]) cube([outer - 2*ring, outer - 2*ring, t + 2]);
      }
      for (p = ins) let(q = P(p)) {   // pads round the holes, tied to the side of the ring
        translate(concat(q, 0)) cylinder(d = pad, h = t);
        translate([q[0] < outer/2 ? 0 : q[0], q[1] - 4, 0]) cube([q[0] < outer/2 ? q[0] : outer - q[0], 8, t]);
      }
      // L-shaped lips at the corners so the screen drops in square
      for (cx = [0, 1], cy = [0, 1]) translate([cx*outer, cy*outer, 0]) mirror([cx, 0, 0]) mirror([0, cy, 0])
        linear_extrude(t + lip) difference() {
          intersection() { square([leg, leg]); translate([0, 0]) offset(corner_r) offset(-corner_r) square([2*leg, 2*leg]); }
          translate([wall, wall]) square([leg, leg]);
        }
    }
    for (p = ins) translate(concat(P(p), -1)) cylinder(d = screw_hole, h = t + 2);
    // marks the screen's wires side, so the frame can't be put on turned round
    translate([outer/2, 2.75, t - 0.6]) linear_extrude(1) text("WIRES SIDE", size = 3.6, font = "DejaVu Sans:style=Bold", halign = "center", valign = "center");
  }
}
// Test piece 2: the lower right corner of the case with the board posts and the
// wheel/USB slot; press in the inserts, screw the board on, try the wheel and cable.
module test_board() difference() {
  intersection() {   // just the board's footprint, its posts and the slot wall
    box();
    place() translate([bx0 - 3, by0 - 3, -1]) cube([outer, bl + 6, board_z + 6.75 + 3 + 1]);
  }
  // open up the middle of the floor to save plastic
  place() translate([bx0 + 8, by0 + 8, -1]) cube([bw - 18, bl - 16, floor_t + 2]);
}
if (part == "test_screen") test_screen();
else if (part == "test_board") test_board();
else if (cut) difference() { all(); translate([-1, -1, -1]) cube([outer + 2, outer / 2 + 1, 100]); } else all();
explode = 0;
if (explode > 0) translate([0, 0, explode]) translate([wall + clr, wall + clr, panel_back_z]) {
  color("#222") cube([panel, panel, panel_t - 1.2]);
  color("#555") translate([0, 0, panel_t - 1.2]) cube([panel, panel, 1.2]);
}
