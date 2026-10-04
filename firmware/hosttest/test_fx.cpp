// Host test: every animation, frame by frame, against the browser preview.
#include <ArduinoJson.h>
#include <fstream>
#include <math.h>
#include <map>
#include <string>
#include <stdlib.h>
#include "../scoreboard/sb_fx.h"
#include "../scoreboard/sb_types.h"
void* sbAlloc(size_t n){return malloc(n);} void sbFree(void*p){free(p);} void sbBreathe(){}

static RGB parseRgb(const char* s) { int r,g,b; if (sscanf(s,"rgb(%d,%d,%d)",&r,&g,&b)==3) return rgb(r,g,b); return 0xFFFFFFFF; }
static void ppm(const char* path, const Frame& fb){ FILE* f=fopen(path,"wb"); fprintf(f,"P6 %d %d 255\n",W*6,H*6);
 for(int y=0;y<H*6;y++)for(int x=0;x<W*6;x++){RGB c=fb.px[(y/6)*W+x/6];bool d=(x%6)&&(y%6)&&(x%6<5)&&(y%6<5);
 unsigned char p[3]={(unsigned char)(d?c>>16:0),(unsigned char)(d?c>>8:0),(unsigned char)(d?c:0)}; if(d&&!fb.lit[(y/6)*W+x/6])p[0]=p[1]=p[2]=20; fwrite(p,1,3,f);} fclose(f);}

static void setPal(FxSpec& s, JsonArrayConst a) {
  s.npal = 0;
  for (JsonArrayConst c : a) { if (s.npal >= FX_MAXPAL) break; for (int i=0;i<3;i++) s.pal[s.npal][i] = c[i]; s.npal++; }
}
static void side(FxSide& d, const char* ab, int sc, const char* hex) { scopy(d.abbr, ab); d.hasScore = true; d.score = sc; d.hasColor = hexRgb(hex, d.color); }

int main(int argc, char** argv) {
  std::ifstream in("/tmp/fx_frames.json");
  JsonDocument doc;
  if (deserializeJson(doc, in)) { puts("bad json"); return 2; }
  static FxSpec logoSpec;   // just to own the logo pixels
  Logo L; L.w = doc["logo"]["w"]; L.h = doc["logo"]["h"]; L.n = 0; L.pix = logoSpec.logoPix;
  for (JsonArrayConst p : doc["logo"]["pix"].as<JsonArrayConst>())
    logoSpec.logoPix[L.n++] = {(uint8_t)(int)p[0], (uint8_t)(int)p[1], (uint8_t)(int)p[2], (uint8_t)(int)p[3], (uint8_t)(int)p[4]};

  // penalty text parsing
  int penBad = 0;
  for (JsonArrayConst pc : doc["penalties"].as<JsonArrayConst>()) {
    char team[8], foul[20];
    parsePenalty(pc[0], team, foul);
    bool ok = !strcmp(team, pc[1]["team"] | "") && !strcmp(foul, pc[1]["foul"] | "");
    if (!ok) { penBad++; printf("penalty MISMATCH: %s -> [%s|%s] want [%s|%s]\n", (const char*)pc[0], team, foul,
                               (const char*)(pc[1]["team"] | ""), (const char*)(pc[1]["foul"] | "")); }
  }
  printf("penalty parsing: %d mismatches\n", penBad);

  int totalBad = 0;
  static FxSpec spec; static Frame fb;
  for (JsonPairConst kv : doc["cases"].as<JsonObjectConst>()) {
    std::string name = kv.key().c_str();
    spec = FxSpec();
    bool pal2 = name == "td_text" || name == "three_text";
    setPal(spec, doc[pal2 ? "pal2" : "pal"]);
    bool withLogo = name.find("logo") != std::string::npos;
    if (withLogo) { spec.logo = L; }
    if (name == "td_logo") { spec.kind = FX_TOUCHDOWN; scopy(spec.label, "NYG"); }
    else if (name == "td_text") { spec.kind = FX_TOUCHDOWN; scopy(spec.label, "TENN"); }
    else if (name == "kick") { spec.kind = FX_KICKOFF; scopy(spec.away.abbr, "DAL"); scopy(spec.home.abbr, "NYG"); }
    else if (name == "qtr" || name == "half") { spec.kind = FX_QUARTER; scopy(spec.label, name == "qtr" ? "END 1ST" : "HALFTIME");
      side(spec.away, "DAL", 7, "002a5c"); side(spec.home, "NYG", 10, "003c7f"); }
    else if (name == "int1" || name == "intreg") { spec.kind = FX_QUARTER; scopy(spec.label, "INTERMISSION"); spec.hasLines = true;
      scopy(spec.lines[0], name == "int1" ? "1ST" : "END OF"); scopy(spec.lines[1], name == "int1" ? "INTERMISSION" : "REGULATION");
      side(spec.away, "NJ", name == "int1" ? 1 : 2, "ce1126"); side(spec.home, "NYR", 2, "0038a8"); }
    else if (name == "fg") { spec.kind = FX_FIELDGOAL; scopy(spec.label, "NYG"); }
    else if (name == "flag") { spec.kind = FX_FLAG; char t[8], f[20];
      parsePenalty("PENALTY on DAL-J.Smith, Defensive Pass Interference, 15 yards, enforced at NYG 40.", t, f);
      scopy(spec.label, f); scopy(spec.team, t); uint32_t h; hexRgb("002a5c", h); spec.teamColor = ledColor(true, h); }
    else if (name == "first") { spec.kind = FX_FIRSTDOWN; scopy(spec.label, "NYG"); }
    else if (name.rfind("goal", 0) == 0) { spec.kind = FX_GOAL; scopy(spec.label, "NYR"); }
    else if (name == "run1" || name == "run3") { spec.kind = FX_RUN; scopy(spec.label, "NYY"); spec.n = name == "run3" ? 3 : 1; }
    else if (name == "hr_logo" || name == "gs_text") { spec.kind = FX_HOMERUN; scopy(spec.label, "NYY"); spec.grand = name == "gs_text"; }
    else if (name.rfind("three", 0) == 0) { spec.kind = FX_THREE; scopy(spec.label, "NY"); }
    FxPlayer pl;
    pl.start(&spec, 12345);
    JsonArrayConst frames = kv.value();
    size_t fi = 0; int badFrames = 0, worst = 0; double worstEl = 0; size_t maxSparks = 0;
    for (int k = 1; k <= 300 && fi < frames.size(); k++) {
      double now = 1000 + (k * 1000) / 60.0, el = now - 1000;
      if (el >= 5000) break;
      bool skip = getenv("SKIP") && !(fi < frames.size() && fabs((double)frames[fi]["el"] - el) < 1e-9);
      if (!skip) pl.frame(fb, el);
      if (pl.sparks() > maxSparks) maxSparks = pl.sparks();
      JsonObjectConst fr = frames[fi];
      if (fabs((double)fr["el"] - el) > 1e-9) continue;
      fi++;
      JsonArrayConst px = fr["px"];
      int diff = 0;
      for (int i = 0; i < W * H; i++) {
        bool wl = !px[i].isNull(); RGB want = wl ? parseRgb(px[i]) : 0;
        RGB have = fb.lit[i] ? fb.px[i] : 0;
        if (wl != fb.lit[i] || want != have) diff++;
      }
      if (diff) { badFrames++; if (diff > worst) { worst = diff; worstEl = el; } }
      if (argc > 1 && strstr(argv[1], name.c_str()) && (fi % 10 == 0)) { char p[80]; snprintf(p, 80, "/tmp/fx_%s_%04d.ppm", name.c_str(), (int)el); ppm(p, fb); }
    }
    totalBad += badFrames;
    printf("%-11s %3zu frames compared, %3d differ (worst %d px at %.0f ms), max sparks %zu\n", name.c_str(), fi, badFrames, worst, worstEl, maxSparks);
  }
  printf("TOTAL differing frames: %d\n", totalBad);
  return totalBad || penBad;
}
