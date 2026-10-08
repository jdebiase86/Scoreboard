// Mock-up: roomier upcoming-game screens (bigger day tag at the top, logos
// lower, start time and date on one line). Draws three layouts x three games.
#include "logo_dir.h"
#include "../scoreboard/sb_render.cpp"
#include "../scoreboard/sb_png.h"
#include <stdio.h>
#include <stdlib.h>
void* sbAlloc(size_t n){return malloc(n);} void sbFree(void*p){free(p);} void sbBreathe(){}
static void ppm(const char* path, const Frame& fb){ FILE* f=fopen(path,"wb"); fprintf(f,"P6 %d %d 255\n",W*6,H*6);
 for(int y=0;y<H*6;y++)for(int x=0;x<W*6;x++){RGB c=fb.px[(y/6)*W+x/6];bool d=(x%6)&&(y%6)&&(x%6<5)&&(y%6<5);
 unsigned char p[3]={(unsigned char)(c>>16),(unsigned char)(c>>8),(unsigned char)c}; if(!d)p[0]=p[1]=p[2]=0; else if(!fb.lit[(y/6)*W+x/6])p[0]=p[1]=p[2]=18; fwrite(p,1,3,f);} fclose(f);}
static Logo L(const char* lg, const char* ab) { Logo l; loadTeamLogo(lg, ab, 26, 24, l); return l; }
static void side(Side& s, const char* ab, uint32_t c, const char* rec) { scopy(s.abbr, ab); s.hasColor = true; s.color = c; scopy(s.record, rec); }
static const RGB BLUE = rgb(32, 80, 192);
static const char* bigDay(const Game& g) {
  static const struct { const char* a; const char* b; } D[] = {{"SUN","SUNDAY"},{"MON","MONDAY"},{"TUE","TUESDAY"},{"WED","WEDNSDAY"},{"THU","THURSDAY"},{"FRI","FRIDAY"},{"SAT","SATURDAY"}};
  for (auto& d : D) if (!strcmp(g.day, d.a)) return tw(d.b, F5) <= 60 ? d.b : d.a;
  return g.day;
}
static void logos(Frame& fb, const Game& g, const Logo* la, const Logo* lh, int y) {
  logoBox(fb, la, 0, y, MATCHUP_W, MATCHUP_H, g.away.abbr, g.away.hasColor, g.away.color);
  logoBox(fb, lh, 38, y, MATCHUP_W, MATCHUP_H, g.home.abbr, g.home.hasColor, g.home.color);
  text(fb, 32 - (tw("AT", F3) >> 1), y + 9, "AT", DATEC, F3);
}
static void recs(Frame& fb, const Game& g, int y) {
  if (g.away.record[0]) text(fb, 13 - (tw(g.away.record, F3) >> 1), y, g.away.record, DATEC, F3);
  if (g.home.record[0]) text(fb, 51 - (tw(g.home.record, F3) >> 1), y, g.home.record, DATEC, F3);
}
// time (big) and date (small, grey) side by side, centred
static void timeDate(Frame& fb, const Game& g, int y) {
  const char* t = g.startTime; const char* d = g.gameDate;
  int w = tw(t, F5) + 4 + tw(d, F3), x = (W - w) >> 1;
  text(fb, x, y, t, CLOCK, F5);
  text(fb, x + tw(t, F5) + 4, y + 2, d, DATEC, F3);
}
static void series(Frame& fb, const Game& g, int y) {
  if (!g.po.on || !g.po.summary[0]) return;
  char up[24]; upperCopy(up, sizeof(up), g.po.summary); centerText(fb, y, up, GOLD);
}
// A: full-width day bar, logos lower, time + date on one line at the bottom
static void layA(Frame& fb, const Game& g, const Logo* la, const Logo* lh) {
  fillRect(fb, 0, 0, 63, 9, BLUE); centerText(fb, 1, bigDay(g), WHITE, F5);
  logos(fb, g, la, lh, 12); recs(fb, g, 38);
  timeDate(fb, g, g.po.on ? 46 : 51);
  series(fb, g, 56);
}
// B: big day tag on the left, date top right, big time alone at the bottom
static void layB(Frame& fb, const Game& g, const Logo* la, const Logo* lh) {
  const char* d = bigDay(g); int w = tw(d, F5) + 4;
  bool date = w + tw(g.gameDate, F3) + 4 <= 62;   // no room next to TOMORROW: tomorrow is clear enough
  fillRect(fb, 1, 0, w, 8, BLUE); fb.unput(1, 0); fb.unput(w, 0); fb.unput(1, 8); fb.unput(w, 8);
  text(fb, 3, 1, d, WHITE, F5);
  if (date) text(fb, 62 - tw(g.gameDate, F3), 2, g.gameDate, DATEC, F3);
  logos(fb, g, la, lh, 12); recs(fb, g, 38);
  centerText(fb, g.po.on ? 46 : 50, g.startTime, CLOCK, F5);
  series(fb, g, 56);
}
// C: day bar at the top, logos lower, a dark strip along the bottom holding time + date
static void layC(Frame& fb, const Game& g, const Logo* la, const Logo* lh) {
  fillRect(fb, 0, 0, 63, 9, BLUE); centerText(fb, 1, bigDay(g), WHITE, F5);
  logos(fb, g, la, lh, 11); recs(fb, g, 37);
  if (g.po.on) { series(fb, g, 44); }
  fillRect(fb, 0, 51, 63, 63, rgb(25, 25, 32));
  timeDate(fb, g, 54);
}
// The final screen in option A's style: a full-width grey FINAL bar (the day
// after it if it wasn't today), logos and big scores lower, WIN, NEXT strip.
static void finalA(Frame& fb, const Game& g, const Logo* la, const Logo* lh) {
  char top[20]; snprintf(top, sizeof(top), g.day[0] && strcmp(g.day, "TODAY") ? "FINAL %s" : "FINAL", g.day);
  fillRect(fb, 0, 0, 63, 9, rgb(80, 80, 90)); centerText(fb, 1, top, WHITE, F5);
  const Side* sides[2] = {&g.away, &g.home}; const Logo* lg[2] = {la, lh};
  for (int k = 0; k < 2; k++) {
    const Side& sd = *sides[k]; const Side& o = *sides[1 - k]; int x = k ? 38 : 0;
    logoBox(fb, lg[k], x, 11, MATCHUP_W, MATCHUP_H, sd.abbr, sd.hasColor, sd.color);
    char sc[6]; snprintf(sc, sizeof(sc), "%d", sd.score);
    scoreText(fb, x + 13 - (tw(sc, F5, 2) >> 1), 36, sc, o.score > sd.score ? DATEC : WHITE, F5, 2);
  }
  text(fb, 32 - (tw("AT", F3) >> 1), 20, "AT", DATEC, F3);
  const Side& m = g.pinned(); const Side& o = g.other();
  if (m.score > o.score) { int x = (g.pinnedHome ? 38 : 0) + 13; text(fb, x - (tw("WIN", F3) >> 1), 51, "WIN", GREEN, F3); }
  fillRect(fb, 0, 57, 63, 63, rgb(25, 25, 32));
  text(fb, 3, 58, "NEXT", DATEC, F3); text(fb, 61 - tw(g.nextText, F3), 58, g.nextText, CLOCK, F3);
}
// Header styles for the upcoming screen's day bar (drawn over renderFull's bar)
static void textShadow(Frame& fb, int y, const char* s, RGB c, RGB sh) {
  int x = (W - tw(s, F5)) >> 1; text(fb, x + 1, y + 1, s, sh, F5); text(fb, x, y, s, c, F5);
}
static void textOutline(Frame& fb, int y, const char* s, RGB c, RGB o) {
  int x = (W - tw(s, F5)) >> 1;
  for (int dx = -1; dx <= 1; dx++) for (int dy = -1; dy <= 1; dy++) if (dx || dy) text(fb, x + dx, y + dy, s, o, F5);
  text(fb, x, y, s, c, F5);
}
static void header(Frame& fb, const Game& g, int style, const char* d) {
  fillRect(fb, 0, 0, W - 1, 10, 0); for (int x = 0; x < W; x++) for (int y = 0; y <= 10; y++) fb.unput(x, y);
  const Side& m = g.pinned(); RGB tc = ledColor(m.hasColor, m.color);
  bool today = !strcmp(g.day, "TODAY");
  switch (style) {
    case 0: fillRect(fb, 0, 0, W - 1, 9, rgb(32, 80, 192)); centerText(fb, 1, d, WHITE, F5); break;     // blue (now)
    case 1: fillRect(fb, 0, 0, W - 1, 9, tc); centerText(fb, 1, d, WHITE, F5); break;                   // your team's colour
    case 2: fillRect(fb, 0, 0, W - 1, 9, rgb(30, 30, 38)); centerText(fb, 1, d, WHITE, F5); break;      // dark grey
    case 3: fillRect(fb, 0, 0, W - 1, 9, today ? rgb(20, 120, 50) : rgb(32, 80, 192)); centerText(fb, 1, d, WHITE, F5); break; // green today
    case 4: textShadow(fb, 1, d, GOLD, rgb(120, 60, 0)); for (int x = 4; x < 60; x++) fb.put(x, 10, rgb(60, 60, 70)); break;  // gold, 3D shadow, no bar
    case 5: textOutline(fb, 1, d, WHITE, tc); break;                                                     // white with team-colour outline
  }
}
int main() {
  static Frame fb; static Game g[3]; static Logo la[3], lh[3];
  g[0].valid = true; g[0].sport = BASKETBALL; g[0].state = ST_PRE; scopy(g[0].day, "TODAY"); scopy(g[0].startTime, "7:30P"); scopy(g[0].gameDate, "10/8");
  side(g[0].away, "WSH", 0x002B5C, "0-0"); side(g[0].home, "NY", 0x006BB6, "0-1"); g[0].pinnedHome = true; la[0] = L("nba", "WSH"); lh[0] = L("nba", "NY");
  g[1].valid = true; g[1].sport = FOOTBALL; g[1].state = ST_PRE; scopy(g[1].day, "SAT"); scopy(g[1].startTime, "1:00P"); scopy(g[1].gameDate, "10/11");
  side(g[1].away, "PHI", 0x004C54, "4-1"); side(g[1].home, "NYG", 0x0B2265, "2-3"); g[1].pinnedHome = true; la[1] = L("nfl", "PHI"); lh[1] = L("nfl", "NYG");
  g[2].valid = true; g[2].sport = BASEBALL; g[2].state = ST_PRE; scopy(g[2].day, "TOMORROW"); scopy(g[2].startTime, "7:08P"); scopy(g[2].gameDate, "10/9");
  side(g[2].away, "BOS", 0xBD3039, "92-70"); side(g[2].home, "NYY", 0x132448, "96-66"); g[2].po.on = true; scopy(g[2].po.summary, "NYY lead 1-0");
  la[2] = L("mlb", "BOS"); lh[2] = L("mlb", "NYY");
  void (*lay[3])(Frame&, const Game&, const Logo*, const Logo*) = {layA, layB, layC};
  char p[256];
  for (int k = 0; k < 3; k++) for (int i = 0; i < 3; i++) {
    fb.clear(); lay[k](fb, g[i], &la[i], &lh[i]);
    if (g[i].po.on) for (int j = 0; j < W; j++) { fb.put(j, 0, PLAYOFF_GOLD); fb.put(j, H - 1, PLAYOFF_GOLD); fb.put(0, j, PLAYOFF_GOLD); fb.put(W - 1, j, PLAYOFF_GOLD); }
    snprintf(p, sizeof(p), "%s/pre_%c%d.ppm", getenv("OUT"), 'a' + k, i); ppm(p, fb);
  }
  static Game f[2]; static Logo fa[2], fh[2];
  f[0].valid = true; f[0].sport = BASEBALL; f[0].state = ST_POST; scopy(f[0].day, "TODAY"); side(f[0].away, "TB", 0x092C5C, "80-75"); side(f[0].home, "NYY", 0x132448, "88-67");
  f[0].away.hasScore = f[0].home.hasScore = true; f[0].away.score = 3; f[0].home.score = 5; f[0].pinnedHome = true; scopy(f[0].nextText, "TUE 7:05P"); fa[0] = L("mlb", "TB"); fh[0] = L("mlb", "NYY");
  f[1].valid = true; f[1].sport = FOOTBALL; f[1].state = ST_POST; scopy(f[1].day, "SUN"); side(f[1].away, "PHI", 0x004C54, "4-2"); side(f[1].home, "NYG", 0x0B2265, "2-4");
  f[1].away.hasScore = f[1].home.hasScore = true; f[1].away.score = 24; f[1].home.score = 17; f[1].pinnedHome = true; scopy(f[1].nextText, "SUN 1:00P"); fa[1] = L("nfl", "PHI"); fh[1] = L("nfl", "NYG");
  for (int i = 0; i < 2; i++) {
    fb.clear(); renderScoreColors(WHITE, WHITE); renderFull(fb, f[i], &fa[i], &fh[i], 0); snprintf(p, sizeof(p), "%s/fin_now%d.ppm", getenv("OUT"), i); ppm(p, fb);
    fb.clear(); finalA(fb, f[i], &fa[i], &fh[i]); snprintf(p, sizeof(p), "%s/fin_a%d.ppm", getenv("OUT"), i); ppm(p, fb);
  }
  static const char* dn[2] = {"TODAY", "SATURDAY"};
  for (int st = 0; st < 6; st++) for (int i = 0; i < 2; i++) {
    fb.clear(); renderFull(fb, g[i], &la[i], &lh[i], 0); header(fb, g[i], st, dn[i]);
    snprintf(p, sizeof(p), "%s/hdr_%d%d.ppm", getenv("OUT"), st, i); ppm(p, fb);
  }
  for (int i = 0; i < 3; i++) { fb.clear(); renderFull(fb, g[i], &la[i], &lh[i], 0); snprintf(p, sizeof(p), "%s/pre_now%d.ppm", getenv("OUT"), i); ppm(p, fb); }
}
