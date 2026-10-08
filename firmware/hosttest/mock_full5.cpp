// Mock-ups v5: full screens in the Mini-Scoreboard style (status pill, WIN tag,
// thin football field + win chance, period tracker, power play banner).
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
static const char* const BALL[] = {"0011100", "0111110", "1112111", "0111110", "0011100"};
static const char* const PUCK[] = {"0111110", "1111111", "0111110"};
static void rect(Frame& fb, int x0, int y0, int x1, int y1, RGB c) { for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) fb.put(x, y, c); }
static RGB dimc(RGB c, int pct) { return rgb(((c >> 16) & 255) * pct / 100, ((c >> 8) & 255) * pct / 100, (c & 255) * pct / 100); }
static const RGB BLUEP = 0x2050C0, GREYP = 0x50505A, REDP = 0xD02020;
// a filled tag with dark text, like the mini's LIVE / FINAL / TOMORROW pills
static int pill(Frame& fb, int x, const char* s, RGB bg, RGB fg = WHITE) {
  int w = tw(s, F3) + 3;
  rect(fb, x, 0, x + w, 6, bg);
  fb.px[x] = 0; fb.lit[x] = false; fb.px[x + w] = 0; fb.lit[x + w] = false;   // rounded corners
  fb.px[6 * W + x] = 0; fb.lit[6 * W + x] = false; fb.px[6 * W + x + w] = 0; fb.lit[6 * W + x + w] = false;
  text(fb, x + 2, 1, s, fg, F3);
  return x + w + 3;
}
static void logoBox(Frame& fb, const Logo& l, int x, int y, int bw, int bh, const char* abbr, RGB c) {
  if (l.n) { drawLogo(fb, l, x + ((bw - l.w) >> 1), y + ((bh - l.h) >> 1), 1); return; }
  int w = tw(abbr, F5); text(fb, x + ((bw - w) >> 1), y + ((bh - 7) >> 1), abbr, c, F5);
}
struct T { const char* ab; RGB c; int score; Logo L; const char* rec; };
static void teams(Frame& fb, T& a, T& h, RGB sa, RGB sh, int y = 8) {
  logoBox(fb, a.L, 0, y, 26, 24, a.ab, a.c);
  logoBox(fb, h.L, 38, y, 26, 24, h.ab, h.c);
  char s1[6], s2[6]; snprintf(s1, 6, "%d", a.score); snprintf(s2, 6, "%d", h.score);
  text(fb, 13 - (tw(s1, F5, 2) >> 1), y + 24, s1, sa, F5, 2);
  text(fb, 51 - (tw(s2, F5, 2) >> 1), y + 24, s2, sh, F5, 2);
}
static void dashes(Frame& fb, int cx, int y, int left) {
  for (int i = 0; i < 3; i++) rect(fb, cx - 7 + i * 5, y, cx - 5 + i * 5, y, i < left ? GOLD : rgb(50, 50, 50));
}
static void center(Frame& fb, int y, const char* s, RGB c, Font f = F3) { text(fb, 32 - (tw(s, f) >> 1), y, s, c, f); }
// thin field (5 rows), end zones in team colours, ball, line to gain
static void thinField(Frame& fb, int y, RGB ca, RGB ch, int ballX, int gainX, bool redzone) {
  rect(fb, 0, y, 63, y + 4, rgb(10, 62, 22));
  if (redzone) rect(fb, 46, y, 57, y + 4, rgb(90, 15, 15));
  rect(fb, 0, y, 5, y + 4, ca); rect(fb, 58, y, 63, y + 4, ch);
  for (int x = 11; x < 58; x += 5) fb.put(x, y + 2, rgb(45, 95, 55));
  rect(fb, gainX, y, gainX, y + 4, rgb(255, 215, 0));
  RGB br = rgb(200, 100, 35); rect(fb, ballX - 1, y + 1, ballX + 1, y + 3, br); fb.put(ballX - 2, y + 2, br); fb.put(ballX + 2, y + 2, br); fb.put(ballX, y + 2, WHITE);
}
static void winBar(Frame& fb, int y, RGB ca, RGB ch, int awayPct) {
  int split = 1 + awayPct * 62 / 100;
  for (int x = 1; x <= 62; x++) fb.put(x, y, x < split ? ca : ch);
}
// period tracker: one block per period, done = team-neutral grey, current = bright
static void tracker(Frame& fb, int y, int n, int cur) {
  int w = (60 - (n - 1) * 2) / n;
  for (int i = 0; i < n; i++) rect(fb, 2 + i * (w + 2), y, 2 + i * (w + 2) + w - 1, y + 1, i + 1 < cur ? rgb(110, 110, 110) : i + 1 == cur ? GOLD : rgb(35, 35, 35));
}
static void caution(Frame& fb, int y0, int y1) {
  for (int y = y0; y <= y1; y++) for (int x = 0; x < 64; x++) fb.put(x, y, ((x + y) / 3) % 2 ? rgb(230, 190, 0) : rgb(10, 10, 10));
}
static Logo L(const char* lg, const char* ab, int w = 26, int h = 24) { Logo l; loadTeamLogo(lg, ab, w, h, l); return l; }
int main() {
  static Frame fb;
  // 1 football live
  T phi{"PHI", ledColor(true, 0x004C54), 17, L("nfl", "PHI"), "3-1"}, nyg{"NYG", ledColor(true, 0x0B2265), 21, L("nfl", "NYG"), "3-1"};
  fb.clear(); int x = pill(fb, 1, "3RD", REDP); text(fb, x, 1, "4:12", CLOCK, F3);
  text(fb, 62 - tw("2&6", F3), 1, "2&6", GOLD, F3);
  teams(fb, phi, nyg, WHITE, GOLD);
  sprite(fb, 29, 18, BALL, 5, BROWN);   // ball between the logos on the side with it
  dashes(fb, 13, 47, 2); dashes(fb, 51, 47, 3);
  thinField(fb, 50, phi.c, nyg.c, 22, 28, false);
  winBar(fb, 56, phi.c, nyg.c, 38);
  center(fb, 58, "AT PHI 34", DATEC);
  ppm("/tmp/h_foot.ppm", fb);
  // 1b red zone
  fb.clear(); x = pill(fb, 1, "3RD", REDP); text(fb, x, 1, "4:12", RED, F3);
  text(fb, 62 - tw("1&G", F3), 1, "1&G", RED, F3);
  teams(fb, phi, nyg, WHITE, GOLD); sprite(fb, 29, 18, BALL, 5, BROWN);
  dashes(fb, 13, 47, 2); dashes(fb, 51, 47, 3);
  thinField(fb, 50, phi.c, nyg.c, 54, 58, true); for (int xx = 45; xx < 58; xx++) { fb.put(xx, 49, RED); fb.put(xx, 55, RED); }
  winBar(fb, 56, phi.c, nyg.c, 38);
  center(fb, 58, "AT PHI 4", RED);
  ppm("/tmp/h_foot_rz.ppm", fb);
  // 2 hockey normal: period tracker + big clock
  T bos{"BOS", ledColor(true, 0xFFB81C), 2, L("nhl", "BOS"), "8-3-1"}, nyr{"NYR", ledColor(true, 0x0038A8), 3, L("nhl", "NYR"), "9-2-1"};
  fb.clear(); x = pill(fb, 1, "2ND", REDP); text(fb, x, 1, "12:34", CLOCK, F3);
  teams(fb, bos, nyr, WHITE, GOLD); sprite(fb, 28, 19, PUCK, 3, WHITE, WHITE);
  text(fb, 13 - (tw(bos.rec, F3) >> 1), 47, bos.rec, DATEC, F3); text(fb, 51 - (tw(nyr.rec, F3) >> 1), 47, nyr.rec, DATEC, F3);
  tracker(fb, 54, 3, 2);
  center(fb, 57, "2ND PERIOD", DATEC);
  ppm("/tmp/h_hockey.ppm", fb);
  // 3 hockey: our power play banner with a draining time bar
  fb.clear(); x = pill(fb, 1, "2ND", REDP); text(fb, x, 1, "12:34", CLOCK, F3);
  teams(fb, bos, nyr, WHITE, GOLD); sprite(fb, 28, 19, PUCK, 3, WHITE, WHITE);
  rect(fb, 0, 46, 63, 63, nyr.c);
  Logo nyrS = L("nhl", "NYR", 14, 12); drawLogo(fb, nyrS, 2, 48, 1);
  text(fb, 18, 47, "POWER", WHITE, F3); text(fb, 18, 53, "PLAY", WHITE, F3);
  text(fb, 62 - tw("1:32", F5), 48, "1:32", GOLD, F5);
  rect(fb, 2, 60, 61, 61, dimc(nyr.c, 40)); rect(fb, 2, 60, 2 + 60 * 92 / 120, 61, GOLD);   // time left, drains to the left
  ppm("/tmp/h_hockey_pp.ppm", fb);
  // 4 hockey: their power play = penalty kill, caution tape
  fb.clear(); x = pill(fb, 1, "2ND", REDP); text(fb, x, 1, "12:34", CLOCK, F3);
  teams(fb, bos, nyr, WHITE, GOLD); sprite(fb, 28, 19, PUCK, 3, WHITE, WHITE);
  caution(fb, 46, 47); caution(fb, 62, 63);
  rect(fb, 0, 48, 63, 61, rgb(10, 10, 10));
  center(fb, 49, "PENALTY KILL", GOLD);
  text(fb, 2, 55, "BOS PP", DATEC, F3); text(fb, 62 - tw("1:32", F3), 55, "1:32", CLOCK, F3);
  ppm("/tmp/h_hockey_pk.ppm", fb);
  // 5 basketball: quarter tracker, bonus tags, records
  T bc{"BOS", ledColor(true, 0x007A33), 88, L("nba", "BOS"), "3-1"}, ny{"NYK", ledColor(true, 0x006BB6), 94, L("nba", "NY"), "4-0"};
  fb.clear(); x = pill(fb, 1, "3RD", REDP); text(fb, x, 1, "4:51", CLOCK, F3);
  teams(fb, bc, ny, WHITE, GOLD);
  text(fb, 51 - (tw("BONUS", F3) >> 1), 47, "BONUS", GOLD, F3);
  text(fb, 13 - (tw(bc.rec, F3) >> 1), 47, bc.rec, DATEC, F3);
  tracker(fb, 54, 4, 3);
  center(fb, 57, "KNICKS BY 6", DATEC);
  ppm("/tmp/h_bball.ppm", fb);
  // 6 final with WIN tag and next game
  T tb{"TB", ledColor(true, 0x092C5C), 3, L("mlb", "TB"), "80-75"}, nyy{"NYY", ledColor(true, 0x132448), 5, L("mlb", "NYY"), "88-67"};
  fb.clear(); x = pill(fb, 1, "FINAL", GREYP); text(fb, x, 1, "SAT 10/3", DATEC, F3);
  teams(fb, tb, nyy, DATEC, WHITE);
  text(fb, 51 - (tw("WIN", F3) >> 1), 47, "WIN", GREEN, F3);
  rect(fb, 2, 53, 61, 63, rgb(25, 25, 32));
  text(fb, 5, 55, "NEXT", DATEC, F3); text(fb, 61 - tw("TUE 7:05P", F3), 55, "TUE 7:05P", CLOCK, F3);
  ppm("/tmp/h_final.ppm", fb);
  // 7 upcoming with pill, records and series card
  T bsx{"BOS", ledColor(true, 0xBD3039), 0, L("mlb", "BOS"), "92-70"};
  fb.clear(); x = pill(fb, 1, "TOMORROW", BLUEP); text(fb, 62 - tw("ALDS", F3), 1, "ALDS", DATEC, F3);
  logoBox(fb, bsx.L, 0, 8, 26, 24, "BOS", bsx.c); logoBox(fb, nyy.L, 38, 8, 26, 24, "NYY", nyy.c);
  text(fb, 32 - (tw("WED", F3) >> 1), 14, "WED", DATEC, F3);
  text(fb, 13 - (tw("92-70", F3) >> 1), 33, "92-70", DATEC, F3); text(fb, 51 - (tw("96-66", F3) >> 1), 33, "96-66", DATEC, F3);
  center(fb, 40, "7:08P", CLOCK, F5);
  rect(fb, 2, 52, 61, 63, rgb(25, 25, 32));
  center(fb, 55, "NYY LEAD 1-0", CLOCK);
  ppm("/tmp/h_pre.ppm", fb);
}
