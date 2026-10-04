#pragma once
#include <ArduinoJson.h>
#include "../scoreboard/sb_render.h"

static void side(JsonObjectConst o, Side& s) {
  scopy(s.abbr, o["abbr"] | "");
  s.hasScore = o["score"].is<int>();
  s.score = o["score"] | 0;
  s.hasColor = hexRgb(o["color"] | "", s.color);
  s.hasColor2 = hexRgb(o["color2"] | "", s.color2);
  scopy(s.record, o["record"] | "");
  s.rank = o["rank"] | 0;
}

static Sport sportOf(const char* s) {
  if (!strcmp(s, "baseball")) return BASEBALL;
  if (!strcmp(s, "hockey")) return HOCKEY;
  if (!strcmp(s, "basketball")) return BASKETBALL;
  return FOOTBALL;
}

void fromPreview(JsonObjectConst o, Game& g) {
  g = Game();
  g.valid = true;
  g.sport = sportOf(o["sport"] | "football");
  const char* st = o["state"] | "pre";
  g.state = !strcmp(st, "in") ? ST_IN : !strcmp(st, "post") ? ST_POST : ST_PRE;
  side(o["home"], g.home); side(o["away"], g.away);
  g.pinnedHome = !strcmp(o["pinned_side"] | "home", "home");
  scopy(g.kickoff, o["kickoff_local"] | "");
  scopy(g.clock, o["clock"] | "");
  g.period = o["period"] | 0;
  if (o["period_label"].is<const char*>()) scopy(g.periodLabel, o["period_label"]);
  else if (o["period_label"].is<int>()) snprintf(g.periodLabel, sizeof(g.periodLabel), "%d", (int)o["period_label"]);
  if (o["playoff"].is<JsonObjectConst>()) {
    g.po.on = true;
    scopy(g.po.round, o["playoff"]["round"] | "");
    scopy(g.po.summary, o["playoff"]["summary"] | "");
    if (o["playoff"]["wins"].is<JsonObjectConst>()) {
      g.po.hasWins = true;
      g.po.winsHome = o["playoff"]["wins"]["home"] | 0;
      g.po.winsAway = o["playoff"]["wins"]["away"] | 0;
    }
  }
  g.preseason = o["preseason"] | false;
  scopy(g.finalLabel, o["final_label"] | "");
  scopy(g.downDistance, o["down_distance"] | "");
  g.redzone = o["redzone"] | false;
  const char* pos = o["possession"] | "";
  g.possession = !strcmp(pos, "home") ? 1 : !strcmp(pos, "away") ? 2 : 0;
  scopy(g.half, o["half"] | "");
  scopy(g.inningText, o["inning_text"] | "");
  scopy(g.inningShort, o["inning_short"] | "");
  g.balls = o["balls"].is<int>() ? (int)o["balls"] : -1;
  g.strikes = o["strikes"].is<int>() ? (int)o["strikes"] : -1;
  g.outs = o["outs"].is<int>() ? (int)o["outs"] : -1;
  for (int i = 0; i < 3; i++) g.bases[i] = o["bases"][i] | false;
  g.intermission = o["intermission"] | false;
  scopy(g.intermissionLeft, o["intermission_left"] | "");
  if (o["pp"].is<JsonObjectConst>()) {
    g.pp = true; g.ppHome = !strcmp(o["pp"]["side"] | "", "home");
    scopy(g.ppTime, o["pp"]["time"] | "");
  }
  if (o["bonus"].is<JsonObjectConst>()) {
    g.hasBonus = true; g.bonusHome = o["bonus"]["home"] | false; g.bonusAway = o["bonus"]["away"] | false;
  }
  g.ranked = o["ranked"] | false;
  for (JsonObjectConst t : o["ticker_games"].as<JsonArrayConst>()) {
    if (g.nticker >= MAX_TICK) break;
    Tick& k = g.ticker[g.nticker++];
    scopy(k.home, t["home"] | ""); scopy(k.away, t["away"] | "");
    k.hasHomeColor = hexRgb(t["home_color"] | "", k.homeColor);
    k.hasAwayColor = hexRgb(t["away_color"] | "", k.awayColor);
    k.homeRank = t["home_rank"] | 0; k.awayRank = t["away_rank"] | 0;
    scopy(k.status, t["status"] | ""); scopy(k.clock, t["clock"] | "");
    scopy(k.score, t["score"] | "");
    scopy(k.kickDate, t["kick_date"] | ""); scopy(k.kickTime, t["kick_time"] | "");
    k.redzone = t["redzone"] | false;
    const char* p = t["possession"] | "";
    k.possession = !strcmp(p, "home") ? 1 : !strcmp(p, "away") ? 2 : 0;
  }
}

