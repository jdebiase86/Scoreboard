// Mock-ups: hockey / basketball / final full screens, small diamond options.
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
static const char* const PUCK[] = {"0111110", "1111111", "0111110"};
static const char* const BBALL[] = {"0011100", "0121210", "1112111", "0121210", "0011100"};
static void logoBox(Frame& fb, const Logo& l, int x, int y, int bw, int bh, const char* abbr, RGB c) {
  if (l.n) { drawLogo(fb, l, x + ((bw - l.w) >> 1), y + ((bh - l.h) >> 1), 1); return; }
  int w = tw(abbr, F5); text(fb, x + ((bw - w) >> 1), y + ((bh - 7) >> 1), abbr, c, F5);
}
static void top2(Frame& fb, const Logo& la, const Logo& lh, const char* a, const char* h, int as, int hs, RGB ca, RGB ch, RGB sa, RGB sh) {
  logoBox(fb, la, 0, 8, 26, 24, a, ca);
  logoBox(fb, lh, 38, 8, 26, 24, h, ch);
  char s1[6], s2[6]; snprintf(s1, 6, "%d", as); snprintf(s2, 6, "%d", hs);
  text(fb, 13 - (tw(s1, F5, 2) >> 1), 33, s1, sa, F5, 2);
  text(fb, 51 - (tw(s2, F5, 2) >> 1), 33, s2, sh, F5, 2);
}
static void hline(Frame& fb, int y) { for (int x = 2; x < 62; x++) fb.put(x, y, LINE); }
static void center(Frame& fb, int y, const char* s, RGB c, Font f) { text(fb, 32 - (tw(s, f) >> 1), y, s, c, f); }

static void bottomClock(Frame& fb, const char* per, const char* clk) {
  int w = tw(per, F3) + 4 + tw(clk, F5);
  int x = 32 - w / 2;
  text(fb, x, 56, per, DATEC, F3);
  text(fb, x + tw(per, F3) + 4, 54, clk, CLOCK, F5);
}
static void hockey(Frame& fb, const Logo& la, const Logo& lh, bool pp) {
  fb.clear();
  if (pp) { text(fb, 2, 1, "POWER PLAY", GOLD, F3); text(fb, 62 - tw("1:32", F3), 1, "1:32", GOLD, F3); for (int x = 40; x < 62; x++) fb.put(x, 32, GOLD); }
  top2(fb, la, lh, "BOS", "NYR", 2, 3, ledColor(true, 0xFFB81C), ledColor(true, 0x0038A8), WHITE, GOLD);
  sprite(fb, 28, 19, PUCK, 3, WHITE, WHITE);
  hline(fb, 50);
  bottomClock(fb, "2ND", "12:34");
}
static void basketball(Frame& fb, const Logo& la, const Logo& lh) {
  fb.clear();
  text(fb, 51 - (tw("BONUS", F3) >> 1), 1, "BONUS", GOLD, F3);
  top2(fb, la, lh, "BOS", "NYK", 88, 94, ledColor(true, 0x007A33), ledColor(true, 0x006BB6), WHITE, GOLD);
  sprite(fb, 28, 19, BBALL, 5, BROWN, rgb(40, 20, 5));
  hline(fb, 50);
  bottomClock(fb, "3RD", "4:51");
}
static void finalScreen(Frame& fb, const Logo& la, const Logo& lh) {
  fb.clear();
  center(fb, 1, "FINAL", GOLD, F3);
  top2(fb, la, lh, "TB", "NYY", 3, 5, ledColor(true, 0x092C5C), ledColor(true, 0x132448), DATEC, WHITE);
  hline(fb, 50);
  center(fb, 55, "NEXT: TUE 7:05P", DATEC, F3);
}
// small diamond options for the normal screen (13 x 9 area at x 29..41, y 0..8)
static void dia(Frame& fb, int x, int y, const bool* b, int style) {
  for (int yy = 0; yy <= 8; yy++) for (int xx = 29; xx <= 41; xx++) { fb.px[yy * W + xx] = 0; fb.lit[yy * W + xx] = false; }
  int cx = x + 6, cy = y + 4;
  RGB grass = rgb(20, 110, 40), dirt = rgb(150, 90, 40);
  for (int dy = -4; dy <= 4; dy++) for (int dx = -6; dx <= 6; dx++) {
    int d = abs(dx) * 2 + abs(dy) * 3;   // wide diamond
    if (style == 0 && d <= 12) fb.put(cx + dx, cy + dy, d >= 10 ? dirt : grass);
    if (style == 1 && d <= 12 && d >= 10) fb.put(cx + dx, cy + dy, rgb(40, 140, 60));   // green outline only
  }
  auto base = [&](int bx, int by, bool on) {
    RGB c = on ? rgb(255, 200, 0) : rgb(170, 170, 170);
    fb.put(bx, by - 1, c); fb.put(bx - 1, by, c); fb.put(bx + 1, by, c); fb.put(bx, by + 1, c);
    fb.put(bx, by, on ? WHITE : c);
  };
  base(cx, cy - 3, b[1]); base(cx - 5, cy, b[2]); base(cx + 5, cy, b[0]);
  fb.put(cx, cy + 4, WHITE);
}
static Logo L(const char* lg, const char* ab) { Logo l; loadTeamLogo(lg, ab, 26, 24, l); return l; }
int main() {
  static Frame fb;
  Logo bos = L("nhl", "BOS"), nyr = L("nhl", "NYR"), bosn = L("nba", "BOS"), nyk = L("nba", "NY"), tb = L("mlb", "TB"), nyy = L("mlb", "NYY");
  hockey(fb, bos, nyr, true); ppm("/tmp/f_hockey_pp.ppm", fb);
  hockey(fb, bos, nyr, false); ppm("/tmp/f_hockey.ppm", fb);
  basketball(fb, bosn, nyk); ppm("/tmp/f_bball.ppm", fb);
  finalScreen(fb, tb, nyy); ppm("/tmp/f_final.ppm", fb);
  static Game g; g.valid = true; g.sport = BASEBALL; g.state = ST_IN; scopy(g.half, "TOP"); scopy(g.inningShort, "T7"); g.period = 7;
  g.balls = 2; g.strikes = 1; g.outs = 1; g.bases[0] = true; g.bases[2] = true;
  scopy(g.away.abbr, "TB"); g.away.hasScore = true; g.away.score = 3; g.away.hasColor = true; g.away.color = 0x092C5C;
  scopy(g.home.abbr, "NYY"); g.home.hasScore = true; g.home.score = 5; g.home.hasColor = true; g.home.color = 0x132448;
  for (int s = 0; s < 2; s++) { renderGame(fb, g, 0, nullptr, nullptr, false, 0); dia(fb, 29, 0, g.bases, s); char n[40]; snprintf(n, 40, "/tmp/f_dia%d.ppm", s); ppm(n, fb); }
  printf("%d %d %d %d %d %d\n", bos.n, nyr.n, bosn.n, nyk.n, tb.n, nyy.n);
}
