// Host test: firmware parser vs the preview's Python parser, on the same feeds.
#include <ArduinoJson.h>
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include "../scoreboard/sb_parse.h"
#include "preview_json.h"

void* sbAlloc(size_t n) { return malloc(n); }
void sbFree(void* p) { free(p); }
void sbBreathe() {}

int main() {
  setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);
  tzset();
  std::ifstream ein("/tmp/feeds/expected.json");
  JsonDocument exp;
  deserializeJson(exp, ein);
  JsonDocument filter;
  buildScoreboardFilter(filter);
  static Frame a, b;
  static Game ga, gb;
  int bad = 0;
  for (JsonPairConst kv : exp.as<JsonObjectConst>()) {
    std::string name = kv.key().c_str();
    std::ifstream fin("/tmp/feeds/" + name + ".json");
    JsonDocument feed;
    auto err = deserializeJson(feed, fin, DeserializationOption::Filter(filter), DeserializationOption::NestingLimit(40));
    if (err) { printf("%s: %s\n", name.c_str(), err.c_str()); bad++; continue; }
    const char* team = kv.value()["team"];
    const char* sport = kv.value()["sport"];
    bool top25 = kv.value()["top25"];
    League lg = !strcmp(sport, "baseball") ? L_MLB : !strcmp(sport, "hockey") ? L_NHL
              : !strcmp(sport, "basketball") ? L_NBA : top25 ? L_CFB : L_NFL;
    const TeamDef* td = nullptr;
    for (auto& t : TEAMS) if (t.league == lg && !strcmp(t.abbr, team)) td = &t;
    if (!td) { printf("%s: no team %s\n", name.c_str(), team); bad++; continue; }
    bool ok = parseGame(feed.as<JsonObjectConst>(), *td, top25, time(nullptr), gb);
    fromPreview(kv.value()["game"], ga);
    // fields the renderer doesn't show directly
    std::string issues;
    if (!ok) issues += " not-found";
    if (strcmp(ga.home.logo, "") && strcmp(gb.home.logo, kv.value()["game"]["home"]["logo"] | "")) issues += " logo";
    if (strcmp(gb.lastPlay, kv.value()["game"]["last_play_text"] | "")) issues += " lastplay";
    if (gb.intermission != (kv.value()["game"]["intermission"] | false)) issues += " intermission";
    if (strcmp(gb.intLines[1], kv.value()["game"]["intermission_lines"][1] | "")) issues += " intlines";
    if (gb.nticker != ga.nticker) issues += " nticker";
    int diffpx = 0;
    for (int pair = 0; pair < 4; pair++) {
      renderGame(a, ga, pair, nullptr, nullptr);
      renderGame(b, gb, pair, nullptr, nullptr);
      for (int i = 0; i < W * H; i++) if (a.px[i] != b.px[i] || a.lit[i] != b.lit[i]) diffpx++;
    }
    if (diffpx) issues += " pixels:" + std::to_string(diffpx);
    printf("%-14s %s  [%s %s v %s | po:%s/%s | %s | fin:%s]\n", name.c_str(), issues.empty() ? "OK " : ("DIFF" + issues).c_str(),
           gb.kickoff, gb.away.abbr, gb.home.abbr, gb.po.round, gb.po.summary, gb.periodLabel, gb.finalLabel);
    if (!issues.empty()) {
      bad++;
      printf("   sim: kickoff=%s round=%s summary=%s final=%s label=%s\n", ga.kickoff, ga.po.round, ga.po.summary, ga.finalLabel, ga.periodLabel);
    }
  }
  printf("%d bad\n", bad);
  // spot checks of the text shorteners
  for (const char* t : {"American League Division Series - Game 2", "AFC Wild Card Playoffs", "NFC Championship",
                        "College Football Playoff Semifinal at the Fiesta Bowl", "Capital One Orange Bowl presented by Capital One",
                        "Eastern Conference Finals - Game 7", "Stanley Cup Final - Game 1", "Super Bowl LXI"})
    printf("  round  %-55s -> %s\n", t, shortRound(t).c_str());
  for (const char* t : {"NYY leads series 2-1", "Series tied 1-1", "Series starts 10/3"})
    printf("  summ   %-55s -> %s\n", t, shortSummary(t, 5).c_str());
  return bad ? 1 : 0;
}
