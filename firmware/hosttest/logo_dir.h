// Where the mock-ups and logo tests find real ESPN logos: assets/logos in
// the repo (downloaded by assets/get_logos.py). Paths are relative to this
// folder, so run the programs from firmware/hosttest.
#pragma once
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <string>

#define LOGO_DIR "../../assets/logos/"

// league "nfl", "ncaa", "mlb", "nhl" or "nba" + ESPN's abbreviation ->
// the logo's path, or "" if there's no logo for that team
static inline std::string findLogo(const char* league, const char* abbr) {
  FILE* f = fopen(LOGO_DIR "index.txt", "r");
  if (!f) return "";
  char line[400], lg[16], ab[16], id[16], file[128];
  std::string out;
  while (fgets(line, sizeof line, f))
    if (sscanf(line, "%15s %15s %15s %127s", lg, ab, id, file) == 4 && !strcmp(lg, league) && !strcasecmp(ab, abbr)) {
      out = std::string(LOGO_DIR) + file;
      break;
    }
  fclose(f);
  return out;
}

// A team's logo shrunk to bw x bh the way the board does it, with Joe's
// per-team picks applied (sb_logofix.h): the regular team-colour file,
// the clean outline, the 76ers' crop, or no logo (letters). Small sizes
// only get the picks, as on the board.
#include "../scoreboard/sb_logo.h"
#include "../scoreboard/sb_png.h"
#include "../scoreboard/sb_logofix.h"
#include <stdlib.h>
#include <vector>
static inline bool loadTeamLogo(const char* league, const char* abbr, int bw, int bh, Logo& L) {
  L = Logo();
  std::string path = findLogo(league, abbr);
  if (path.empty()) return false;
  bool small = bw < 40 && bh < 40;
  std::string url = "/teamlogos/" + std::string(league) + "/500/" + path.substr(path.rfind('/') + 1);
  uint8_t fix = small ? logoFix(url.c_str()) : 0;
  if (fix & FIX_LETTERS) return false;
  if (fix & FIX_LIGHT) {
    std::string lp = path.substr(0, path.size() - 4) + "-light.png";
    if (FILE* t = fopen(lp.c_str(), "rb")) { fclose(t); path = lp; }
  }
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) return false;
  fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  std::vector<uint8_t> b(n);
  if (fread(b.data(), 1, n, f) != (size_t)n) { fclose(f); return false; }
  fclose(f);
  int w, h, e;
  uint8_t* rgba = decodePngRGBA(b.data(), n, w, h, &e);
  if (!rgba) return false;
  if (fix & FIX_CROP_TOP)
    for (int y = 0; y < h * 2 / 5; y++) for (int x = 0; x < w; x++) rgba[((size_t)y * w + x) * 4 + 3] = 0;
  bool ok = shrinkLogo(rgba, w, h, bw, bh, L, fix & FIX_KEYLINE);
  free(rgba);
  return ok;
}
