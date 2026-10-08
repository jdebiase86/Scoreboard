// Draws the full screens with the real firmware code (renderFull) to check
// them against the approved mock-ups (mock_full5.cpp).
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
static Logo L(const char* lg, const char* ab) { Logo l; loadTeamLogo(lg, ab, 26, 24, l); return l; }
static void side(Side& s, const char* ab, uint32_t c, int score, const char* rec) {
  scopy(s.abbr, ab); s.hasColor = true; s.color = c; if (score >= 0) { s.hasScore = true; s.score = score; } scopy(s.record, rec);
}
int main() {
  static Frame fb; static Game g;
  // football live
  g = Game(); g.valid = true; g.sport = FOOTBALL; g.state = ST_IN; scopy(g.periodLabel, "3RD"); scopy(g.clock, "4:12"); g.period = 3;
  side(g.away, "PHI", 0x004C54, 17, "3-1"); side(g.home, "NYG", 0x0B2265, 21, "3-1"); g.pinnedHome = true;
  g.possession = 2; scopy(g.downDistance, "2nd & 6 at PHI 34"); scopy(g.shortDD, "2nd & 6"); g.yardLine = 66; g.distance = 6;
  g.toAway = 2; g.toHome = 3; g.winHome = 62;
  Logo phi = L("nfl", "PHI"), nyg = L("nfl", "NYG");
  renderScoreColors(WHITE, WHITE); renderFull(fb, g, &phi, &nyg, 0); ppm("/tmp/r_foot.ppm", fb);
  g.redzone = true; g.yardLine = 96; scopy(g.downDistance, "1st & Goal at NYG 4"); scopy(g.shortDD, "1st & Goal"); g.distance = 4; g.possession = 2;
  renderFull(fb, g, &phi, &nyg, 0); ppm("/tmp/r_foot_rz.ppm", fb);
  // baseball
  g = Game(); g.valid = true; g.sport = BASEBALL; g.state = ST_IN; scopy(g.half, "TOP"); scopy(g.inningShort, "T7"); g.period = 7;
  g.balls = 2; g.strikes = 1; g.outs = 1; g.bases[0] = true; g.bases[2] = true;
  side(g.away, "TB", 0x092C5C, 3, "80-75"); side(g.home, "NYY", 0x132448, 5, "88-67"); g.pinnedHome = true;
  Logo tb = L("mlb", "TB"), nyy = L("mlb", "NYY");
  renderScoreColors(GOLD, WHITE); renderFull(fb, g, &tb, &nyy, 0); ppm("/tmp/r_base.ppm", fb);
  // hockey, PP ours, PK
  g = Game(); g.valid = true; g.sport = HOCKEY; g.state = ST_IN; scopy(g.periodLabel, "2ND"); scopy(g.clock, "12:34"); g.period = 2;
  side(g.away, "BOS", 0xFFB81C, 2, "8-3-1"); side(g.home, "NYR", 0x0038A8, 3, "9-2-1"); g.pinnedHome = true;
  Logo bos = L("nhl", "BOS"), nyr = L("nhl", "NYR");
  renderScoreColors(WHITE, WHITE); renderFull(fb, g, &bos, &nyr, 0); ppm("/tmp/r_hockey.ppm", fb);
  g.pp = true; g.ppHome = true; scopy(g.ppTime, "1:32"); renderFull(fb, g, &bos, &nyr, 0); ppm("/tmp/r_hockey_pp.ppm", fb);
  g.ppHome = false; renderFull(fb, g, &bos, &nyr, 0); ppm("/tmp/r_hockey_pk.ppm", fb);
  // basketball (close game)
  g = Game(); g.valid = true; g.sport = BASKETBALL; g.state = ST_IN; scopy(g.periodLabel, "3RD"); scopy(g.clock, "4:51"); g.period = 3;
  side(g.away, "BOS", 0x007A33, 88, "3-1"); side(g.home, "NY", 0x006BB6, 94, "4-0"); g.pinnedHome = true; g.hasBonus = true; g.bonusHome = true;
  Logo bc = L("nba", "BOS"), ny = L("nba", "NY");
  renderFull(fb, g, &bc, &ny, 0); ppm("/tmp/r_bball.ppm", fb);
  // final
  g = Game(); g.valid = true; g.sport = BASEBALL; g.state = ST_POST; scopy(g.day, "SAT"); 
  side(g.away, "TB", 0x092C5C, 3, "80-75"); side(g.home, "NYY", 0x132448, 5, "88-67"); g.pinnedHome = true; scopy(g.nextText, "TUE 7:05P");
  renderScoreColors(WHITE, WHITE); renderFull(fb, g, &tb, &nyy, 0); ppm("/tmp/r_final.ppm", fb);
  // upcoming playoff
  g = Game(); g.valid = true; g.sport = BASEBALL; g.state = ST_PRE; scopy(g.day, "TOMORROW"); scopy(g.startTime, "7:08P"); scopy(g.gameDate, "10/8");
  side(g.away, "BOS", 0xBD3039, -1, "92-70"); side(g.home, "NYY", 0x132448, -1, "96-66"); g.po.on = true; scopy(g.po.round, "ALDS GAME 2"); scopy(g.po.summary, "NYY lead 1-0");
  Logo bsx = L("mlb", "BOS");
  renderFull(fb, g, &bsx, &nyy, 0); ppm("/tmp/r_pre.ppm", fb);
}
