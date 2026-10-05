#include "logo_dir.h"
#include "../scoreboard/sb_png.h"
#include "../scoreboard/sb_render.h"
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
void* sbAlloc(size_t n) { return malloc(n); }
void sbFree(void* p) { free(p); }
void sbBreathe() {}
static std::vector<uint8_t> slurp(const char* p) { FILE* f = fopen(p, "rb"); std::vector<uint8_t> v; int c; while ((c = fgetc(f)) != EOF) v.push_back(c); fclose(f); return v; }
static void ppm(const char* path, const Frame& fb) {
  FILE* f = fopen(path, "wb"); fprintf(f, "P6 %d %d 255\n", W * 8, H * 8);
  for (int y = 0; y < H * 8; y++) for (int x = 0; x < W * 8; x++) {
    RGB c = fb.px[(y / 8) * W + x / 8]; bool dot = (x % 8) && (y % 8) && (x % 8 < 7) && (y % 8 < 7);
    unsigned char p[3] = {(unsigned char)(dot ? c >> 16 : 0), (unsigned char)(dot ? c >> 8 : 0), (unsigned char)(dot ? c : 0)};
    if (dot && !c) p[0] = p[1] = p[2] = 22;
    fwrite(p, 1, 3, f); }
  fclose(f);
}
int main(int argc, char** argv) {
  const char* dir = LOGO_DIR;
  const char* files[] = {"mlb/tb.png", "mlb/nyy.png", "nfl/dal.png",
    "nfl/nyg.png", "ncaa/99.png", "ncaa/57.png", "nba/ny.png"};
  Logo small[7], big[7];
  for (int i = 0; i < 7; i++) {
    auto d = slurp((std::string(dir) + files[i]).c_str());
    int w, h; uint8_t* rgba = decodePngRGBA(d.data(), d.size(), w, h);
    if (!rgba) { printf("decode failed %s\n", files[i]); return 1; }
    shrinkLogo(rgba, w, h, MATCHUP_W, MATCHUP_H, small[i]);
    shrinkLogo(rgba, w, h, 54, 54, big[i]);
    printf("%-34s %dx%d -> small %dx%d (%d px), big %dx%d (%d px)\n", files[i], w, h, small[i].w, small[i].h, small[i].n, big[i].w, big[i].h, big[i].n);
    sbFree(rgba);
  }
  static Frame fb; Game g; g.valid = true; g.sport = BASEBALL; g.state = ST_PRE; g.pinnedHome = true;
  scopy(g.kickoff, "10/3 6:30P"); scopy(g.home.abbr, "NYY"); scopy(g.away.abbr, "TB");
  scopy(g.home.record, "93-69"); scopy(g.away.record, "98-64");
  const char* pairs[][2] = {{"TB","NYY"},{"DAL","NYG"},{"LSU","FLA"}};
  for (int k = 0; k < 3; k++) {
    renderGame(fb, g, 0, &small[k*2], &small[k*2+1]);
    char p[64]; snprintf(p, sizeof p, "/tmp/logo_m%d.ppm", k); ppm(p, fb);
  }
  renderGame(fb, g, 0, &small[0], &small[6]); ppm("/tmp/logo_m3.ppm", fb);
  for (int k = 0; k < 7; k++) { fb.clear(); drawLogo(fb, big[k], (W - big[k].w) / 2, (H - big[k].h) / 2, 1); char p[64]; snprintf(p, sizeof p, "/tmp/logo_b%d.ppm", k); ppm(p, fb); }
}
