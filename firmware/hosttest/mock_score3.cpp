// Mock-up: three-digit scores on the full screens (108 and 102 overflowed in
// a live Knicks game). A = tall and thin (normal width, double height),
// B = the same big digits packed tight with a narrow 1, C = a narrower 4-wide
// digit set at double size (3 digits = 26 dots, the logo's width).
#include "logo_dir.h"
#include "../scoreboard/sb_render.cpp"
#include "../scoreboard/sb_png.h"
#include "../scoreboard/sb_fonts.h"
#include <stdio.h>
#include <stdlib.h>
void* sbAlloc(size_t n){return malloc(n);} void sbFree(void*p){free(p);} void sbBreathe(){}
static void ppm(const char* path, const Frame& fb){ FILE* f=fopen(path,"wb"); fprintf(f,"P6 %d %d 255\n",W*6,H*6);
 for(int y=0;y<H*6;y++)for(int x=0;x<W*6;x++){RGB c=fb.px[(y/6)*W+x/6];bool d=(x%6)&&(y%6)&&(x%6<5)&&(y%6<5);
 unsigned char p[3]={(unsigned char)(c>>16),(unsigned char)(c>>8),(unsigned char)c}; if(!d)p[0]=p[1]=p[2]=0; else if(!fb.lit[(y/6)*W+x/6])p[0]=p[1]=p[2]=18; fwrite(p,1,3,f);} fclose(f);}
static Logo L(const char* lg, const char* ab) { Logo l; loadTeamLogo(lg, ab, 26, 24, l); return l; }
static const char* const N4[10][7] = {
  {".##.","#..#","#..#","#..#","#..#","#..#",".##."}, {"..#.",".##.","..#.","..#.","..#.","..#.",".###"},
  {".##.","#..#","...#","..#.",".#..","#...","####"}, {"###.","...#","...#",".##.","...#","...#","###."},
  {"#..#","#..#","#..#","####","...#","...#","...#"}, {"####","#...","###.","...#","...#","#..#",".##."},
  {".##.","#...","#...","###.","#..#","#..#",".##."}, {"####","...#","..#.","..#.",".#..",".#..",".#.."},
  {".##.","#..#","#..#",".##.","#..#","#..#",".##."}, {".##.","#..#","#..#",".###","...#","...#",".##."}};
static void putBlock(Frame& fb, int x, int y, int sx, int sy, RGB c) { for (int a = 0; a < sx; a++) for (int b = 0; b < sy; b++) fb.put(x + a, y + b, c); }
// F5 glyph columns actually used (for packing)
static void f5cols(char ch, int& c0, int& c1) {
  const uint8_t* g = F5_GLYPHS[strchr(F5_CHARS, ch) - F5_CHARS]; c0 = 5; c1 = -1;
  for (int r = 0; r < 7; r++) for (int x = 0; x < 5; x++) if (g[r] & (1 << (4 - x))) { if (x < c0) c0 = x; if (x > c1) c1 = x; }
}
static int drawScore(Frame& fb, int cx, int y, const char* s, RGB c, int mode, bool measure) {
  int w = 0;
  if (mode == 0) { w = tw(s, F5, 2); if (!measure) text(fb, cx - w / 2, y, s, c, F5, 2); return w; }
  if (mode == 1) {   // A: tall and thin
    w = tw(s, F5, 1); if (measure) return w;
    int x = cx - w / 2;
    for (const char* p = s; *p; p++, x += 6) {
      const uint8_t* g = F5_GLYPHS[strchr(F5_CHARS, *p) - F5_CHARS];
      for (int r = 0; r < 7; r++) for (int q = 0; q < 5; q++) if (g[r] & (1 << (4 - q))) putBlock(fb, x + q, y + r * 2, 1, 2, c);
    }
    return w;
  }
  if (mode == 2) {   // B: big, packed, narrow 1
    for (const char* p = s; *p; p++) { int a, b; f5cols(*p, a, b); w += (b - a + 1) * 2 + (p[1] ? 2 : 0); }
    if (measure) return w;
    int x = cx - w / 2;
    for (const char* p = s; *p; p++) {
      int a, b; f5cols(*p, a, b); const uint8_t* g = F5_GLYPHS[strchr(F5_CHARS, *p) - F5_CHARS];
      for (int r = 0; r < 7; r++) for (int q = a; q <= b; q++) if (g[r] & (1 << (4 - q))) putBlock(fb, x + (q - a) * 2, y + r * 2, 2, 2, c);
      x += (b - a + 1) * 2 + 2;
    }
    return w;
  }
  // C: 4-wide digits at double size
  w = (int)strlen(s) * 9 - 1; if (measure) return w;
  int x = cx - w / 2;
  for (const char* p = s; *p; p++, x += 9)
    for (int r = 0; r < 7; r++) for (int q = 0; q < 4; q++) if (N4[*p - '0'][r][q] == '#') putBlock(fb, x + q * 2, y + r * 2, 2, 2, c);
  return w;
}
int main() {
  static Frame fb; static Game g;
  g.valid = true; g.sport = BASKETBALL; g.state = ST_IN; scopy(g.periodLabel, "4TH"); scopy(g.clock, "1:32"); g.period = 4;
  scopy(g.away.abbr, "WSH"); g.away.hasColor = true; g.away.color = 0x002B5C; g.away.hasScore = true; g.away.score = 108; scopy(g.away.record, "0-0");
  scopy(g.home.abbr, "NY"); g.home.hasColor = true; g.home.color = 0x006BB6; g.home.hasScore = true; g.home.score = 102; scopy(g.home.record, "0-1");
  g.pinnedHome = true;
  Logo la = L("nba", "WSH"), lh = L("nba", "NY");
  char p[300];
  for (int mode = 0; mode < 4; mode++) for (int v = 0; v < 2; v++) {
    if (v) { g.away.score = 98; g.home.score = 101; }
    else { g.away.score = 108; g.home.score = 102; }
    fb.clear(); renderScoreColors(GOLD, WHITE); renderFull(fb, g, &la, &lh, 0);
    for (int y = 32; y < 47; y++) for (int x = 0; x < W; x++) fb.unput(x, y);
    char a[6], h[6]; snprintf(a, 6, "%d", g.away.score); snprintf(h, 6, "%d", g.home.score);
    int y = mode == 1 ? 32 : 32;
    if (mode == 0) { // today: as the board draws it (overflowing)
      text(fb, 13 - (tw(a, F5, 2) >> 1), 32, a, WHITE, F5, 2); text(fb, 51 - (tw(h, F5, 2) >> 1), 32, h, GOLD, F5, 2);
    } else {
      // C only changes three-digit scores; two digits stay the big normal ones
      int ma = (mode == 3 && strlen(a) < 3) ? 0 : mode, mh = (mode == 3 && strlen(h) < 3) ? 0 : mode;
      drawScore(fb, 13, y, a, WHITE, ma, false); drawScore(fb, 51, y, h, GOLD, mh, false);
    }
    snprintf(p, sizeof(p), "%s/s3_%d%d.ppm", getenv("OUT"), mode, v); ppm(p, fb);
  }
}
