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
