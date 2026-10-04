#include "../scoreboard/sb_parse.h"
#include <stdlib.h>
void* sbAlloc(size_t n) { return malloc(n); }
void sbFree(void* p) { free(p); }
void sbBreathe() {}
int main() {
  Game g; g.valid = true; g.state = ST_IN; g.sport = HOCKEY; g.pinnedHome = true; scopy(g.home.abbr, "NJ");
  JsonDocument f; buildNhlFilter(f);
  JsonDocument d;
  deserializeJson(d, R"({"games":[{"homeTeam":{"abbrev":"NJD","x":1},"awayTeam":{"abbrev":"NYR"},
    "clock":{"timeRemaining":"08:14","running":true,"inIntermission":false},
    "situation":{"homeTeam":{"abbrev":"NJD","strength":4},"awayTeam":{"abbrev":"NYR","strength":5,"situationDescriptions":["PP"]},
    "situationCode":"1541","timeRemaining":"01:23"}}]})", DeserializationOption::Filter(f));
  enrichHockey(d.as<JsonObjectConst>(), g);
  printf("pp=%d ppHome=%d time=%s int=%d\n", g.pp, g.ppHome, g.ppTime, g.intermission);
  deserializeJson(d, R"({"games":[{"homeTeam":{"abbrev":"NJD"},"awayTeam":{"abbrev":"NYR"},
    "clock":{"timeRemaining":"14:32","running":true,"inIntermission":true}}]})", DeserializationOption::Filter(f));
  enrichHockey(d.as<JsonObjectConst>(), g);
  printf("int=%d left=%s pp=%d\n", g.intermission, g.intermissionLeft, g.pp);
  Game b; b.state = ST_IN; b.sport = BASKETBALL; b.pinnedHome = false; scopy(b.away.abbr, "NY");
  buildNbaFilter(f);
  deserializeJson(d, R"({"meta":{},"scoreboard":{"games":[{"homeTeam":{"teamTricode":"BOS","inBonus":"0"},"awayTeam":{"teamTricode":"NYK","inBonus":"1"}}]}})", DeserializationOption::Filter(f));
  enrichBasketball(d.as<JsonObjectConst>(), b);
  printf("bonus has=%d home=%d away=%d\n", b.hasBonus, b.bonusHome, b.bonusAway);
}
