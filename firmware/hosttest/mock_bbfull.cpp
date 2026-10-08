// Mock-ups: baseball full-game screen, upcoming-game full screen, green diamond.
#include "logo_dir.h"
#include "../scoreboard/sb_render.h"
#include "../scoreboard/sb_png.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void* sbAlloc(size_t n){return malloc(n);} void sbFree(void*p){free(p);} void sbBreathe(){}
static void ppm(const char* path, const Frame& fb){ FILE* f=fopen(path,"wb"); fprintf(f,"P6 %d %d 255\n",W*6,H*6);
 for(int y=0;y<H*6;y++)for(int x=0;x<W*6;x++){RGB c=fb.px[(y/6)*W+x/6];bool d=(x%6)&&(y%6)&&(x%6<5)&&(y%6<5);
 unsigned char p[3]={(unsigned char)(c>>16),(unsigned char)(c>>8),(unsigned char)c}; if(!d)p[0]=p[1]=p[2]=0; else if(!fb.lit[(y/6)*W+x/6])p[0]=p[1]=p[2]=18; fwrite(p,1,3,f);} fclose(f);}
static const char* const UP[] = {"00100", "01110", "11111"};
static const char* const DN[] = {"11111", "01110", "00100"};

static void logoBox(Frame& fb, const Logo& l, int x, int y, int bw, int bh, const char* abbr, RGB c) {
  if (l.n) { drawLogo(fb, l, x + ((bw - l.w) >> 1), y + ((bh - l.h) >> 1), 1); return; }
  int w = tw(abbr, F5); text(fb, x + ((bw - w) >> 1), y + ((bh - 7) >> 1), abbr, c, F5);
}
// small diamond for the normal screen, green version: grass inside, dirt base paths
static void greenDiamond(Frame& fb, int x, int y, const bool* b) {
  RGB grass = rgb(10, 70, 25), dirt = rgb(110, 65, 30);
  int cx = x + 5, cy = y + 4;
  for (int dy = -3; dy <= 3; dy++) for (int dx = -5; dx <= 5; dx++) {
    int d = abs(dx) * 3 / 5 + abs(dy);   // squashed diamond
    if (d <= 3) fb.put(cx + dx, cy + dy, d == 3 ? dirt : grass);
  }
  auto base = [&](int bx, int by, bool on) {
    RGB c = on ? rgb(255, 200, 0) : rgb(150, 150, 150);
    fb.put(bx, by - 1, c); fb.put(bx - 1, by, c); fb.put(bx + 1, by, c); fb.put(bx, by + 1, c);
    fb.put(bx, by, on ? WHITE : c);
  };
  base(x + 5, y + 1, b[1]); base(x + 1, y + 4, b[2]); base(x + 9, y + 4, b[0]);
  fb.put(x + 5, y + 7, WHITE);
}
// big diamond for the full screen, between the logos
static void bigDiamond(Frame& fb, int cx, int cy, const bool* b) {
  RGB grass = rgb(10, 75, 25), dirt = rgb(120, 70, 30);
  for (int dy = -5; dy <= 5; dy++) for (int dx = -5; dx <= 5; dx++) {
    int d = abs(dx) + abs(dy);
    if (d <= 5) fb.put(cx + dx, cy + dy, d >= 4 ? dirt : grass);
  }
  auto base = [&](int bx, int by, bool on) {
    RGB c = on ? rgb(255, 200, 0) : rgb(160, 160, 160);
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) if (abs(dx) + abs(dy) <= 1) fb.put(bx + dx, by + dy, c);
    if (on) fb.put(bx, by, WHITE);
  };
  base(cx, cy - 4, b[1]); base(cx - 4, cy, b[2]); base(cx + 4, cy, b[0]);
  fb.put(cx, cy + 5, WHITE); fb.put(cx, cy + 4, WHITE);   // home plate
  fb.put(cx, cy, rgb(140, 85, 40));   // mound
}
static void dots(Frame& fb, int x, int y, const char* lab, int n, int on, RGB c) {
  text(fb, x, y, lab, DATEC, F3);
  int sx = x + tw(lab, F3) + 2;
  for (int i = 0; i < n; i++) for (int dx = 0; dx < 3; dx++) for (int dy = 0; dy < 3; dy++)
    if (!((dx == 0 || dx == 2) && (dy == 0 || dy == 2))) fb.put(sx + i * 5 + dx, y + 1 + dy, i < on ? c : rgb(45, 45, 45));
}

struct BB { const char *a, *h; int as, hs; bool top; int inn, balls, strikes, outs; bool bases[3]; bool homeYours; };

static void baseballFull(Frame& fb, const BB& s, const Logo& la, const Logo& lh, RGB ca, RGB ch) {
  fb.clear();
  // top line: inning arrow + number on the left, outs on the right
  char num[8]; snprintf(num, 8, "%d", s.inn);
  sprite(fb, 2, 2, s.top ? UP : DN, 3, CLOCK);
  text(fb, 8, 1, num, CLOCK, F3);
  char ord[10]; snprintf(ord, 10, "%s", s.top ? "TOP" : "BOT");
  text(fb, 8 + tw(num, F3) + 3, 1, ord, DATEC, F3);
  logoBox(fb, la, 0, 8, 26, 24, s.a, ca);
  logoBox(fb, lh, 38, 8, 26, 24, s.h, ch);
  bigDiamond(fb, 31, 20, s.bases);
  char sa[6], sh[6]; snprintf(sa, 6, "%d", s.as); snprintf(sh, 6, "%d", s.hs);
  text(fb, 13 - (tw(sa, F5, 2) >> 1), 33, sa, s.homeYours ? CLOCK : WHITE, F5, 2);
  text(fb, 51 - (tw(sh, F5, 2) >> 1), 33, sh, WHITE, F5, 2);
  // count and outs across the bottom, stadium style
  for (int x = 2; x < 62; x++) fb.put(x, 50, LINE);
  dots(fb, 3, 55, "B", 3, s.balls, GREEN);
  dots(fb, 24, 55, "S", 2, s.strikes, GOLD);
  dots(fb, 40, 55, "OUT", 2, s.outs, RED);
  if (s.outs >= 0) {}
}

static void pregameFull(Frame& fb, const char* a, const char* h, const char* ra, const char* rh, const char* day, const char* tm,
                        const Logo& la, const Logo& lh, RGB ca, RGB ch) {
  fb.clear();
  logoBox(fb, la, 0, 1, 30, 30, a, ca);
  logoBox(fb, lh, 34, 1, 30, 30, h, ch);
  text(fb, 30 - (tw("@", F3) >> 1) + 2, 13, "@", DATEC, F3);
  text(fb, 15 - (tw(a, F3) >> 1), 33, a, CLOCK, F3);
  text(fb, 49 - (tw(h, F3) >> 1), 33, h, CLOCK, F3);
  text(fb, 15 - (tw(ra, F3) >> 1), 40, ra, DATEC, F3);
  text(fb, 49 - (tw(rh, F3) >> 1), 40, rh, DATEC, F3);
  for (int x = 4; x < 60; x++) fb.put(x, 47, LINE);
  text(fb, 32 - (tw(day, F3) >> 1), 50, day, DATEC, F3);
  text(fb, 32 - (tw(tm, F5) >> 1), 56, tm, CLOCK, F5);
}

static Logo L(const char* lg, const char* ab, int w, int h) { Logo l; loadTeamLogo(lg, ab, w, h, l); return l; }

int main() {
  static Frame fb;
  static Game g;
  // --- now: normal live baseball screen (grey diamond) and the full screen with the play line
  g = Game(); g.valid = true; g.sport = BASEBALL; g.state = ST_IN; scopy(g.half, "TOP"); scopy(g.inningShort, "T7"); g.period = 7;
  g.balls = 2; g.strikes = 1; g.outs = 1; g.bases[0] = true; g.bases[2] = true;
  scopy(g.away.abbr, "TB"); g.away.hasScore = true; g.away.score = 3; g.away.hasColor = true; g.away.color = 0x092C5C;
  scopy(g.home.abbr, "NYY"); g.home.hasScore = true; g.home.score = 5; g.home.hasColor = true; g.home.color = 0x132448; g.pinnedHome = true;
  scopy(g.lastPlay, "Arozarena singled to left, Diaz to third, Lowe scored");
  renderGame(fb, g, 0, nullptr, nullptr, false, 0); ppm("/tmp/bb_now_normal.ppm", fb);
  greenDiamond(fb, 30, 1, g.bases);
  // clear the old grey diamond first
  { static Frame f2; renderGame(f2, g, 0, nullptr, nullptr, false, 0);
    for (int y = 0; y <= 8; y++) for (int x = 29; x <= 41; x++) { f2.px[y * W + x] = 0; f2.lit[y * W + x] = false; }
    greenDiamond(f2, 30, 0, g.bases); ppm("/tmp/bb_new_normal.ppm", f2); }
  renderGame(fb, g, 0, nullptr, nullptr, true, 1500); ppm("/tmp/bb_now_full.ppm", fb);
  // --- proposed baseball full screen
  Logo tb = L("mlb", "TB", 26, 24), nyy = L("mlb", "NYY", 26, 24);
  BB s{"TB", "NYY", 3, 5, true, 7, 2, 1, 1, {true, false, true}, true};
  baseballFull(fb, s, tb, nyy, ledColor(true, 0x092C5C), ledColor(true, 0x132448)); ppm("/tmp/bb_new_full.ppm", fb);
  // --- upcoming game full screen: now vs proposed
  g = Game(); g.valid = true; g.sport = FOOTBALL; g.state = ST_PRE; scopy(g.kickoff, "SUN 1:00P");
  scopy(g.away.abbr, "JAX"); scopy(g.away.record, "3-2"); g.away.hasColor = true; g.away.color = 0x006778;
  scopy(g.home.abbr, "SEA"); scopy(g.home.record, "4-1"); g.home.hasColor = true; g.home.color = 0x002244; g.pinnedHome = false;
  Logo jax = L("nfl", "JAX", 26, 24), sea = L("nfl", "SEA", 26, 24);
  renderGame(fb, g, 0, &jax, &sea, true, 0); ppm("/tmp/pre_now.ppm", fb);
  Logo jax30 = L("nfl", "JAX", 30, 30), sea30 = L("nfl", "SEA", 30, 30);
  pregameFull(fb, "JAX", "SEA", "3-2", "4-1", "SUNDAY 10/12", "1:00P", jax30, sea30, ledColor(true, 0x006778), ledColor(true, 0x002244));
  ppm("/tmp/pre_new.ppm", fb);
  printf("logos: tb %d nyy %d jax %d sea %d jax30 %d\n", tb.n, nyy.n, jax.n, sea.n, jax30.n);
}
