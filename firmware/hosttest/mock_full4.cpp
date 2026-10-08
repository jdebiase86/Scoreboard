// Mock-ups v4: full screens with a playing surface along the bottom (no empty space).
#include "logo_dir.h"
#include "../scoreboard/sb_render.h"
#include "../scoreboard/sb_png.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
void* sbAlloc(size_t n){return malloc(n);} void sbFree(void*p){free(p);} void sbBreathe(){}
static void ppm(const char* path, const Frame& fb){ FILE* f=fopen(path,"wb"); fprintf(f,"P6 %d %d 255\n",W*6,H*6);
 for(int y=0;y<H*6;y++)for(int x=0;x<W*6;x++){RGB c=fb.px[(y/6)*W+x/6];bool d=(x%6)&&(y%6)&&(x%6<5)&&(y%6<5);
 unsigned char p[3]={(unsigned char)(c>>16),(unsigned char)(c>>8),(unsigned char)c}; if(!d)p[0]=p[1]=p[2]=0; else if(!fb.lit[(y/6)*W+x/6])p[0]=p[1]=p[2]=18; fwrite(p,1,3,f);} fclose(f);}
static const char* const UP[] = {"00100", "01110", "11111"};
static const char* const DN[] = {"11111", "01110", "00100"};
static void rect(Frame& fb, int x0, int y0, int x1, int y1, RGB c) { for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) fb.put(x, y, c); }
static RGB dimc(RGB c, int pct) { return rgb(((c >> 16) & 255) * pct / 100, ((c >> 8) & 255) * pct / 100, (c & 255) * pct / 100); }
static void logoBox(Frame& fb, const Logo& l, int x, int y, int bw, int bh, const char* abbr, RGB c) {
  if (l.n) { drawLogo(fb, l, x + ((bw - l.w) >> 1), y + ((bh - l.h) >> 1), 1); return; }
  int w = tw(abbr, F5); text(fb, x + ((bw - w) >> 1), y + ((bh - 7) >> 1), abbr, c, F5);
}
// header 0-6, logos 7-30, scores 31-44, surface 46-63
struct T { const char* ab; const char* lg; RGB c; int score; Logo L; };
static void teams(Frame& fb, T& a, T& h, RGB sa, RGB sh) {
  // team-colour edge stripes down each side, full height of the top part
  rect(fb, 0, 7, 0, 44, a.c); rect(fb, 63, 7, 63, 44, h.c);
  logoBox(fb, a.L, 2, 7, 26, 24, a.ab, a.c);
  logoBox(fb, h.L, 36, 7, 26, 24, h.ab, h.c);
  char s1[6], s2[6]; snprintf(s1, 6, "%d", a.score); snprintf(s2, 6, "%d", h.score);
  text(fb, 15 - (tw(s1, F5, 2) >> 1), 31, s1, sa, F5, 2);
  text(fb, 49 - (tw(s2, F5, 2) >> 1), 31, s2, sh, F5, 2);
}
static void header(Frame& fb, const char* l, RGB lc, const char* r, RGB rc) {
  rect(fb, 0, 0, 63, 6, rgb(18, 18, 22));
  text(fb, 2, 1, l, lc, F3);
  if (r) text(fb, 62 - tw(r, F3), 1, r, rc, F3);
}
// --- hockey rink
static void rink(Frame& fb, RGB ca, RGB ch, bool pp, bool ppHome) {
  RGB ice = rgb(55, 65, 80), board = rgb(170, 170, 170);
  rect(fb, 1, 47, 62, 62, ice);
  for (int x = 2; x < 62; x++) { fb.put(x, 46, board); fb.put(x, 63, board); }
  for (int y = 47; y < 63; y++) { fb.put(0, y, board); fb.put(63, y, board); }
  rect(fb, 31, 47, 32, 62, rgb(160, 30, 30));                 // red line
  rect(fb, 21, 47, 21, 62, rgb(40, 70, 180)); rect(fb, 42, 47, 42, 62, rgb(40, 70, 180));   // blue lines
  for (int a = 0; a < 360; a += 12) { int x = 32 + (int)lround(5 * cos(a * M_PI / 180)), y = 55 + (int)lround(5 * sin(a * M_PI / 180)); fb.put(x, y, rgb(40, 70, 180)); }
  rect(fb, 6, 47, 6, 62, rgb(140, 30, 30)); rect(fb, 57, 47, 57, 62, rgb(140, 30, 30));   // goal lines
  rect(fb, 3, 52, 5, 58, ca); rect(fb, 58, 52, 60, 58, ch);   // nets in team colours
  for (int dx : {13, 50}) for (int dy : {51, 59}) fb.put(dx, dy, rgb(160, 30, 30));      // faceoff dots
  if (pp) { int x0 = ppHome ? 43 : 7, x1 = ppHome ? 56 : 20; for (int x = x0; x <= x1; x++) { fb.put(x, 47, GOLD); fb.put(x, 62, GOLD); } }
}
// --- basketball court
static void court(Frame& fb, RGB ca, RGB ch) {
  RGB wood = rgb(95, 58, 25), line = rgb(200, 200, 200);
  rect(fb, 0, 46, 63, 63, wood);
  for (int x = 0; x < 64; x++) { fb.put(x, 46, line); fb.put(x, 63, line); }
  rect(fb, 31, 46, 32, 63, line);
  for (int a = 0; a < 360; a += 10) fb.put(32 + (int)lround(5.5 * cos(a * M_PI / 180)), 55 + (int)lround(5.5 * sin(a * M_PI / 180)), line);
  rect(fb, 0, 51, 9, 59, ca); rect(fb, 54, 51, 63, 59, ch);   // keys in team colours
  for (int a = -70; a <= 70; a += 8) { fb.put(9 + (int)lround(4 * cos(a * M_PI / 180)), 55 + (int)lround(4 * sin(a * M_PI / 180)), line);
                                        fb.put(54 - (int)lround(4 * cos(a * M_PI / 180)), 55 + (int)lround(4 * sin(a * M_PI / 180)), line); }
  for (int a = -80; a <= 80; a += 6) { fb.put(1 + (int)lround(15 * cos(a * M_PI / 180)), 55 + (int)lround(8.5 * sin(a * M_PI / 180)), line);
                                        fb.put(62 - (int)lround(15 * cos(a * M_PI / 180)), 55 + (int)lround(8.5 * sin(a * M_PI / 180)), line); }
  rect(fb, 2, 54, 3, 56, rgb(255, 120, 20)); rect(fb, 60, 54, 61, 56, rgb(255, 120, 20));   // hoops
}
// --- baseball field: big diamond with runners, count and outs on the grass
static void field(Frame& fb, const bool* b, int balls, int strikes, int outs, bool count = true) {
  RGB grass = rgb(15, 75, 25), dirt = rgb(120, 72, 32), line = rgb(220, 220, 220);
  rect(fb, 0, 46, 63, 63, grass);
  int cx = 32, cy = 55;
  for (int dy = -8; dy <= 8; dy++) for (int dx = -9; dx <= 9; dx++) {
    int d = abs(dx) * 8 / 9 + abs(dy);
    if (d <= 8) fb.put(cx + dx, cy + dy, d >= 6 ? dirt : rgb(25, 105, 35));
  }
  auto base = [&](int x, int y, bool on) {
    RGB c = on ? rgb(255, 200, 0) : line;
    rect(fb, x - 1, y - 1, x + 1, y + 1, c); if (on) fb.put(x, y, WHITE);
  };
  base(cx, cy - 7, b[1]); base(cx - 8, cy, b[2]); base(cx + 8, cy, b[0]);
  rect(fb, cx - 1, cy + 7, cx + 1, cy + 7, WHITE); fb.put(cx, cy + 8, WHITE);   // home plate
  rect(fb, cx - 1, cy - 1, cx + 1, cy + 1, dirt);   // mound
  auto dots = [&](int x, int y, const char* lab, int n, int on, RGB c) {
    text(fb, x, y, lab, CLOCK, F3);
    for (int i = 0; i < n; i++) rect(fb, x + tw(lab, F3) + 2 + i * 4, y + 1, x + tw(lab, F3) + 4 + i * 4, y + 3, i < on ? c : rgb(10, 40, 15));
  };
  if (!count) return;
  dots(2, 49, "B", 3, balls, GREEN);
  dots(2, 56, "S", 2, strikes, GOLD);
  text(fb, 62 - tw("OUT", F3), 49, "OUT", CLOCK, F3);
  for (int i = 0; i < 2; i++) rect(fb, 50 + i * 5, 56, 52 + i * 5, 58, i < outs ? RED : rgb(10, 40, 15));
}
// --- football: the existing Option B field, redrawn bigger at the bottom
static void gridiron(Frame& fb, RGB ca, RGB ch, int ballYd) {
  RGB grass = rgb(10, 62, 22);
  rect(fb, 0, 46, 63, 63, grass);
  rect(fb, 0, 46, 5, 63, ca); rect(fb, 58, 46, 63, 63, ch);
  for (int i = 1; i < 10; i++) { int x = 6 + i * 52 / 10; for (int y = 46; y < 64; y += 1) if (y % 2 == 0) fb.put(x, y, rgb(60, 110, 70)); }
  rect(fb, 32, 46, 32, 63, rgb(120, 170, 130));
  int bx = 6 + (100 - ballYd) * 52 / 100;
  rect(fb, bx + 6, 46, bx + 6, 63, rgb(255, 215, 0));   // line to gain
  RGB br = rgb(200, 100, 35); rect(fb, bx - 2, 53, bx + 2, 56, br); fb.put(bx - 3, 54, br); fb.put(bx + 3, 54, br); fb.put(bx - 3, 55, br); fb.put(bx + 3, 55, br);
  rect(fb, bx - 1, 54, bx + 1, 55, WHITE);
}
static Logo L(const char* lg, const char* ab) { Logo l; loadTeamLogo(lg, ab, 26, 24, l); return l; }
static void sheet(const char* n, Frame& fb) { ppm(n, fb); }
int main() {
  static Frame fb;
  T bos{"BOS", "nhl", ledColor(true, 0xFFB81C), 2, L("nhl", "BOS")}, nyr{"NYR", "nhl", ledColor(true, 0x0038A8), 3, L("nhl", "NYR")};
  fb.clear(); header(fb, "2ND 12:34", CLOCK, "PP", GOLD); teams(fb, bos, nyr, WHITE, GOLD); rink(fb, bos.c, nyr.c, true, true); sheet("/tmp/g_hockey.ppm", fb);
  T bc{"BOS", "nba", ledColor(true, 0x007A33), 88, L("nba", "BOS")}, ny{"NYK", "nba", ledColor(true, 0x006BB6), 94, L("nba", "NY")};
  fb.clear(); header(fb, "3RD 4:51", CLOCK, "BONUS", GOLD); teams(fb, bc, ny, WHITE, GOLD); court(fb, bc.c, ny.c); sheet("/tmp/g_bball.ppm", fb);
  T tb{"TB", "mlb", ledColor(true, 0x092C5C), 3, L("mlb", "TB")}, nyy{"NYY", "mlb", ledColor(true, 0x132448), 5, L("mlb", "NYY")};
  bool bases[3] = {true, false, true};
  fb.clear(); header(fb, "", CLOCK, "1-2", DATEC); sprite(fb, 2, 2, UP, 3, CLOCK); text(fb, 8, 1, "7TH", CLOCK, F3);
  teams(fb, tb, nyy, WHITE, GOLD); field(fb, bases, 2, 1, 1); sheet("/tmp/g_base.ppm", fb);
  T jax{"JAX", "nfl", ledColor(true, 0x006778), 17, L("nfl", "JAX")}, sea{"SEA", "nfl", ledColor(true, 0x002244), 10, L("nfl", "SEA")};
  fb.clear(); header(fb, "3RD 8:42", CLOCK, "2&7", DATEC); teams(fb, jax, sea, GOLD, WHITE); gridiron(fb, jax.c, sea.c, 66); sheet("/tmp/g_foot.ppm", fb);
  // final: surface dimmed, FINAL over it
  fb.clear(); header(fb, "NEXT", DATEC, "TUE 7:05P", CLOCK); teams(fb, tb, nyy, DATEC, WHITE);
  bool none[3] = {false, false, false}; field(fb, none, 0, 0, 0, false);
  for (int y = 46; y < 64; y++) for (int x = 0; x < 64; x++) fb.px[y * W + x] = dimc(fb.px[y * W + x], 40);
  text(fb, 32 - (tw("FINAL", F5) >> 1), 52, "FINAL", GOLD, F5); sheet("/tmp/g_final.ppm", fb);
  // upcoming: surface with the start time big on it
  jax.score = 0; sea.score = 0;
  fb.clear(); header(fb, "SUNDAY 10/12", CLOCK, nullptr, 0);
  rect(fb, 0, 7, 0, 44, jax.c); rect(fb, 63, 7, 63, 44, sea.c);
  logoBox(fb, jax.L, 2, 7, 26, 24, "JAX", jax.c); logoBox(fb, sea.L, 36, 7, 26, 24, "SEA", sea.c);
  text(fb, 32 - (tw("@", F3) >> 1), 17, "@", DATEC, F3);
  text(fb, 15 - (tw("JAX", F3) >> 1), 32, "JAX", CLOCK, F3); text(fb, 49 - (tw("SEA", F3) >> 1), 32, "SEA", CLOCK, F3);
  text(fb, 15 - (tw("3-2", F3) >> 1), 38, "3-2", DATEC, F3); text(fb, 49 - (tw("4-1", F3) >> 1), 38, "4-1", DATEC, F3);
  gridiron(fb, jax.c, sea.c, 50);
  for (int y = 46; y < 64; y++) for (int x = 0; x < 64; x++) fb.px[y * W + x] = dimc(fb.px[y * W + x], 35);
  rect(fb, 14, 50, 49, 60, rgb(0, 0, 0));
  text(fb, 32 - (tw("1:00P", F5) >> 1), 52, "1:00P", CLOCK, F5); sheet("/tmp/g_pre.ppm", fb);
}
