#include <math.h>
#include "sb_parse.h"
#include "sb_gfx.h"
#include <string>
#include <vector>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

// ------------------------------------------------------------------ filters
void buildScoreboardFilter(JsonDocument& f) {
  f.clear();
  JsonObject ev = f["events"].add<JsonObject>();
  ev["id"] = true;
  ev["date"] = true;
  ev["season"]["type"] = true;
  ev["season"]["slug"] = true;
  JsonObject c = ev["competitions"].add<JsonObject>();
  c["timeValid"] = true;
  JsonObject cp = c["competitors"].add<JsonObject>();
  cp["homeAway"] = true;
  cp["score"] = true;
  cp["record"] = true;
  cp["curatedRank"]["current"] = true;
  JsonObject rec = cp["records"].add<JsonObject>();
  rec["type"] = true;
  rec["summary"] = true;
  JsonObject t = cp["team"].to<JsonObject>();
  for (const char* k : {"id", "abbreviation", "location", "shortDisplayName", "displayName", "color",
                        "alternateColor", "logo"})
    t[k] = true;
  JsonObject lg = t["logos"].add<JsonObject>();
  lg["href"] = true;
  lg["rel"] = true;
  JsonObject st = c["status"].to<JsonObject>();
  st["displayClock"] = true;
  st["period"] = true;
  st["type"]["state"] = true;
  st["type"]["shortDetail"] = true;
  st["type"]["detail"] = true;
  JsonObject s = c["situation"].to<JsonObject>();
  for (const char* k : {"downDistanceText", "shortDownDistanceText", "isRedZone", "possession", "balls", "strikes",
                        "outs", "onFirst", "onSecond", "onThird", "down", "distance", "yardLine", "homeTimeouts",
                        "awayTimeouts"})
    s[k] = true;
  s["lastPlay"]["text"] = true;
  s["lastPlay"]["scoreValue"] = true;
  s["lastPlay"]["probability"]["homeWinPercentage"] = true;
  JsonObject ser = c["series"].to<JsonObject>();
  ser["summary"] = true;
  ser["title"] = true;
  ser["totalCompetitions"] = true;
  JsonObject sc = ser["competitors"].add<JsonObject>();
  sc["id"] = true;
  sc["wins"] = true;
  c["notes"].add<JsonObject>()["headline"] = true;
}

void buildNhlFilter(JsonDocument& f) {
  f.clear();
  JsonObject gm = f["games"].add<JsonObject>();
  gm["homeTeam"]["abbrev"] = true;
  gm["awayTeam"]["abbrev"] = true;
  gm["clock"] = true;
  JsonObject s = gm["situation"].to<JsonObject>();
  s["situationCode"] = true;
  s["timeRemaining"] = true;
  for (const char* side : {"homeTeam", "awayTeam"}) {
    s[side]["strength"] = true;
    s[side]["situationDescriptions"] = true;
  }
}

void buildNbaFilter(JsonDocument& f) {
  f.clear();
  JsonObject gm = f["scoreboard"]["games"].add<JsonObject>();
  for (const char* side : {"homeTeam", "awayTeam"}) {
    gm[side]["teamTricode"] = true;
    gm[side]["inBonus"] = true;
  }
}

// -------------------------------------------------------------------- time
static long daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long)doe - 719468;
}

time_t parseIso(const char* iso) {
  int Y, M, D, h = 0, m = 0, s = 0;
  if (!iso || sscanf(iso, "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &s) < 5) return 0;
  return (time_t)(daysFromCivil(Y, M, D) * 86400L + h * 3600L + m * 60L + s);
}

void boardDate(time_t now, char (&out)[9]) {
  time_t t = now - 5 * 3600;
  struct tm lt;
  localtime_r(&t, &lt);
  snprintf(out, sizeof(out), "%04d%02d%02d", lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
}

// "10/4" and "3:30P" in local time; is it today (5am rollover)?
static bool localWhen(const char* iso, time_t now, char (&md)[8], char (&hm)[8]) {
  md[0] = hm[0] = 0;
  time_t t = parseIso(iso);
  if (!t) return true;
  struct tm lt;
  localtime_r(&t, &lt);
  snprintf(md, sizeof(md), "%d/%d", lt.tm_mon + 1, lt.tm_mday);
  int h12 = lt.tm_hour % 12; if (!h12) h12 = 12;
  snprintf(hm, sizeof(hm), "%d:%02d%c", h12, lt.tm_min, lt.tm_hour < 12 ? 'A' : 'P');
  char a[9], b[9];
  boardDate(t, a);
  boardDate(now, b);
  return !strcmp(a, b);
}

// "TODAY", "TOMORROW", the weekday within a week ("WED"), else "10/14"
void dayWord(const char* iso, time_t now, char (&out)[10]) {
  out[0] = 0;
  time_t t = parseIso(iso);
  if (!t) return;
  struct tm a, b;
  localtime_r(&t, &a);
  localtime_r(&now, &b);
  // whole days between the two dates (noon to noon, so clock changes don't matter)
  struct tm na = a, nb = b;
  na.tm_hour = nb.tm_hour = 12; na.tm_min = nb.tm_min = na.tm_sec = nb.tm_sec = 0;
  long days = lround(difftime(mktime(&na), mktime(&nb)) / 86400.0);
  static const char* const WD[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  if (days == 0) scopy(out, "TODAY");
  else if (days == 1) scopy(out, "TOMORROW");
  else if (days > 1 && days < 7) scopy(out, WD[a.tm_wday]);
  else snprintf(out, sizeof(out), "%d/%d", a.tm_mon + 1, a.tm_mday);
}

// ------------------------------------------------------------------ helpers
static std::string upper(const char* s) {
  std::string o;
  for (; s && *s; s++) o += (char)toupper((unsigned char)*s);
  return o;
}

static bool sideMatches(JsonObjectConst comp, const TeamDef& t, bool byName) {
  JsonObjectConst tm = comp["team"];
  if (!byName) return upper(tm["abbreviation"] | "") == upper(t.abbr);
  std::string want = upper(t.name);
  for (const char* k : {"location", "shortDisplayName", "displayName"})
    if (upper(tm[k] | "") == want) return true;
  return false;
}

static void sides(JsonObjectConst ev, JsonObjectConst& c, JsonObjectConst& h, JsonObjectConst& a) {
  c = ev["competitions"][0];
  for (JsonObjectConst x : c["competitors"].as<JsonArrayConst>()) {
    if (!strcmp(x["homeAway"] | "", "home")) h = x;
    else if (!strcmp(x["homeAway"] | "", "away")) a = x;
  }
}

JsonObjectConst findEvent(JsonObjectConst feed, const TeamDef& t) {
  for (int pass = 0; pass < 2; pass++)
    for (JsonObjectConst ev : feed["events"].as<JsonArrayConst>()) {
      JsonObjectConst c, h, a;
      sides(ev, c, h, a);
      if (h.isNull() || a.isNull()) continue;
      if (sideMatches(h, t, pass) || sideMatches(a, t, pass)) return ev;
    }
  return JsonObjectConst();
}

GState eventState(JsonObjectConst ev) {
  const char* s = ev["competitions"][0]["status"]["type"]["state"] | "pre";
  return !strcmp(s, "in") ? ST_IN : !strcmp(s, "post") ? ST_POST : ST_PRE;
}

static bool scoreOf(JsonObjectConst comp, GState st, int& out) {
  if (st == ST_PRE) return false;
  JsonVariantConst v = comp["score"];
  if (v.is<int>()) { out = v.as<int>(); return true; }
  const char* s = v | "";
  if (!*s) return false;
  for (const char* p = s; *p; p++) if (!isdigit((unsigned char)*p)) return false;
  out = atoi(s);
  return true;
}

static const char* okRec(const char* v) {
  while (v && *v == ' ') v++;
  return (v && isdigit((unsigned char)*v) && strchr(v, '-')) ? v : nullptr;
}

static void recordOf(JsonObjectConst comp, char (&out)[12]) {
  out[0] = 0;
  JsonArrayConst recs = comp["records"];
  if (!recs.isNull()) {
    for (const char* want : {"total", "overall", "ytd"})
      for (JsonObjectConst r : recs)
        if (upper(r["type"] | "") == upper(want) && okRec(r["summary"] | "")) { scopy(out, okRec(r["summary"])); return; }
    for (JsonObjectConst r : recs)
      if (okRec(r["summary"] | "")) { scopy(out, okRec(r["summary"])); return; }
  }
  if (okRec(comp["record"] | "")) scopy(out, okRec(comp["record"]));
}

static void logoOf(JsonObjectConst comp, char (&out)[128]) {
  JsonObjectConst t = comp["team"];
  out[0] = 0;
  for (JsonObjectConst l : t["logos"].as<JsonArrayConst>()) {
    bool dark = false;
    for (JsonVariantConst r : l["rel"].as<JsonArrayConst>()) if (!strcmp(r | "", "dark")) dark = true;
    if (dark && l["href"].is<const char*>()) { scopy(out, l["href"]); return; }
  }
  for (JsonObjectConst l : t["logos"].as<JsonArrayConst>())
    if (l["href"].is<const char*>()) { scopy(out, l["href"]); return; }
  scopy(out, t["logo"] | "");
}

static int rankOf(JsonObjectConst comp) {
  int r = comp["curatedRank"]["current"] | 0;
  return (r >= 1 && r <= 25) ? r : 0;
}

static int tickRank(JsonObjectConst comp) {
  JsonVariantConst v = comp["curatedRank"]["current"];
  if (!v.is<int>()) return 0;
  int r = v.as<int>();
  return r == 99 ? 0 : r;
}

static void fillSide(JsonObjectConst comp, GState st, Side& s) {
  JsonObjectConst t = comp["team"];
  scopy(s.abbr, t["abbreviation"] | "");
  s.hasScore = scoreOf(comp, st, s.score);
  s.hasColor = hexRgb(t["color"] | "", s.color);
  s.hasColor2 = hexRgb(t["alternateColor"] | "", s.color2);
  logoOf(comp, s.logo);
  recordOf(comp, s.record);
  s.rank = rankOf(comp);
  scopy(s.id, t["id"] | "");
}

static void periodLabel(Sport sp, int period, bool post, char (&out)[6]) {
  out[0] = 0;
  if (!period) return;
  static const char* const ORD[] = {"", "1ST", "2ND", "3RD", "4TH"};
  if (sp == HOCKEY) {
    if (period <= 3) { scopy(out, ORD[period]); return; }
    if (!post) { scopy(out, period == 4 ? "OT" : "SO"); return; }
    if (period == 4) scopy(out, "OT"); else snprintf(out, sizeof(out), "%dOT", period - 3);
    return;
  }
  if (sp == BASEBALL) { snprintf(out, sizeof(out), "%d", period); return; }
  if (period <= 4) { scopy(out, ORD[period]); return; }
  if (period == 5) scopy(out, "OT"); else snprintf(out, sizeof(out), "%dOT", period - 4);
}

static bool isPostseason(JsonObjectConst ev) {
  int st = ev["season"]["type"] | 0;
  return st == 3 || strstr(ev["season"]["slug"] | "", "post");
}

static bool icontains(const char* hay, const char* needle) {
  return upper(hay).find(upper(needle)) != std::string::npos;
}

static int numOf(JsonVariantConst v) {
  if (v.is<int>()) return v.as<int>();
  if (v.is<float>()) return (int)v.as<float>();
  const char* s = v | "";
  if (*s && isdigit((unsigned char)*s)) return atoi(s);
  return -1;
}

struct Bb { char half[4] = ""; char text[10] = ""; char shrt[6] = ""; int balls = -1, strikes = -1, outs = -1; bool bases[3] = {}; };

static void baseballState(JsonObjectConst status, JsonObjectConst sit, Bb& out) {
  const char* det = status["type"]["shortDetail"] | "";
  if (!*det) det = status["type"]["detail"] | "";
  std::string d = upper(det);
  // BOTTOM -> BOT, MIDDLE -> MID
  size_t p;
  while ((p = d.find("BOTTOM")) != std::string::npos) d.replace(p, 6, "BOT");
  while ((p = d.find("MIDDLE")) != std::string::npos) d.replace(p, 6, "MID");
  char w0[16] = "", w1[16] = "";
  sscanf(d.c_str(), "%15s %15s", w0, w1);
  if (!strcmp(w0, "TOP") || !strcmp(w0, "BOT") || !strcmp(w0, "MID") || !strcmp(w0, "END")) {
    scopy(out.half, w0);
    char num[8] = ""; int k = 0;
    for (char* q = w1; *q && k < 7; q++) if (isdigit((unsigned char)*q)) num[k++] = *q;
    num[k] = 0;
    snprintf(out.text, sizeof(out.text), "%s %s", w0, w1);
    if (!w1[0]) scopy(out.text, w0);
    if (num[0]) snprintf(out.shrt, sizeof(out.shrt), "%c%s", w0[0], num); else scopy(out.shrt, w0);
  } else {
    int n = status["period"] | 0;
    if (n) { snprintf(out.text, sizeof(out.text), "%d", n); snprintf(out.shrt, sizeof(out.shrt), "%d", n); }
  }
  out.balls = numOf(sit["balls"]);
  out.strikes = numOf(sit["strikes"]);
  out.outs = numOf(sit["outs"]);
  out.bases[0] = sit["onFirst"] | false;
  out.bases[1] = sit["onSecond"] | false;
  out.bases[2] = sit["onThird"] | false;
}

static void replaceAll(std::string& t, const char* a, const char* b) {
  size_t la = strlen(a), lb = strlen(b), p = 0;
  while ((p = t.find(a, p)) != std::string::npos) { t.replace(p, la, b); p += lb; }
}

static std::string squash(const char* txt) {
  std::string t;
  bool sp = false;
  for (const char* s = txt ? txt : ""; *s; s++) {
    if (isspace((unsigned char)*s)) { sp = true; continue; }
    if (sp && !t.empty()) t += ' ';
    sp = false;
    t += (char)toupper((unsigned char)*s);
  }
  return t;
}

static std::string trimWords(std::string t) {
  while (t.size() > 15 && t.find(' ') != std::string::npos) t = t.substr(0, t.rfind(' '));
  return t.substr(0, 15);
}

std::string shortRound(const char* txt) {
  std::string t = squash(txt);
  if (t.find("PLAYOFF") != std::string::npos &&
      (t.find("COLLEGE") != std::string::npos || t.find("CFP") != std::string::npos)) {
    static const char* const M[][2] = {{"NATIONAL CHAMPIONSHIP", "CFP TITLE GAME"}, {"SEMIFINAL", "CFP SEMIFINAL"},
                                       {"QUARTERFINAL", "CFP QUARTERS"}, {"FIRST ROUND", "CFP 1ST ROUND"}};
    for (auto& m : M) if (t.find(m[0]) != std::string::npos) return m[1];
  }
  if (t.find("NATIONAL CHAMPIONSHIP") != std::string::npos) return "NATL TITLE GAME";
  if (t.find("BOWL") != std::string::npos && t.find("SUPER BOWL") == std::string::npos) {
    std::string head = t;
    size_t pb = head.find("PRESENTED BY");
    if (pb != std::string::npos) head = head.substr(0, pb);
    std::vector<std::string> words;
    size_t i = 0;
    while (i < head.size()) {
      size_t j = head.find(' ', i);
      if (j == std::string::npos) j = head.size();
      if (j > i) words.push_back(head.substr(i, j - i));
      i = j + 1;
    }
    int bi = -1;
    for (int k = 0; k < (int)words.size(); k++) if (words[k] == "BOWL") bi = k;
    if (bi >= 0) return bi > 0 ? words[bi - 1] + " BOWL" : std::string("BOWL");
  }
  for (const char* conf : {"AFC", "NFC"}) {
    for (const char* r : {"WILD CARD", "DIVISIONAL"})
      for (const char* pl : {" PLAYOFFS", " PLAYOFF"}) {
        std::string a = std::string(conf) + " " + r + pl, b = std::string(conf) + " " + r;
        replaceAll(t, a.c_str(), b.c_str());
      }
    std::string a = std::string(conf) + " CHAMPIONSHIP", b = std::string(conf) + " TITLE GAME";
    replaceAll(t, a.c_str(), b.c_str());
  }
  static const char* const R[][2] = {
    {"AMERICAN LEAGUE ", "AL"}, {"NATIONAL LEAGUE ", "NL"},
    {"AL DIVISION SERIES", "ALDS"}, {"NL DIVISION SERIES", "NLDS"},
    {"AL CHAMPIONSHIP SERIES", "ALCS"}, {"NL CHAMPIONSHIP SERIES", "NLCS"},
    {"ALDIVISION SERIES", "ALDS"}, {"NLDIVISION SERIES", "NLDS"},
    {"ALCHAMPIONSHIP SERIES", "ALCS"}, {"NLCHAMPIONSHIP SERIES", "NLCS"},
    {"WILD CARD SERIES", "WILD CARD"},
    {"EASTERN CONFERENCE", "EAST"}, {"WESTERN CONFERENCE", "WEST"},
    {"FIRST ROUND", "RD1"}, {"SECOND ROUND", "RD2"},
    {"CONFERENCE FINALS", "CONF FINAL"}, {"CONFERENCE FINAL", "CONF FINAL"},
    {"STANLEY CUP FINAL", "CUP FINAL"}, {" - ", " "}};
  for (auto& r : R) replaceAll(t, r[0], r[1]);
  if (t.size() > 15) {
    replaceAll(t, "GAME ", "G");
    replaceAll(t, "ROUND", "RD");
    replaceAll(t, "CONFERENCE", "CONF");
  }
  return trimWords(t);
}

std::string shortSummary(const char* txt, int total) {
  std::string t = squash(txt);
  if (t.rfind("SERIES STARTS", 0) == 0 || t.rfind("SERIES BEGINS", 0) == 0)
    return total ? "BEST OF " + std::to_string(total) : std::string();
  if (t.rfind("SERIES", 0) != 0) replaceAll(t, " SERIES ", " ");
  return trimWords(t);
}

static void playoffOf(JsonObjectConst ev, JsonObjectConst c, JsonObjectConst h, JsonObjectConst a, Playoff& po) {
  po = Playoff();
  JsonObjectConst ser = c["series"];
  if (!isPostseason(ev) && ser.isNull()) return;
  po.on = true;
  const char* head = "";
  for (JsonObjectConst n : c["notes"].as<JsonArrayConst>())
    if (*(n["headline"] | "")) { head = n["headline"]; break; }
  if (!*head) head = ser["title"] | "";
  if (!*head) head = "PLAYOFFS";
  scopy(po.round, shortRound(head).c_str());
  scopy(po.summary, shortSummary(ser["summary"] | "", ser["totalCompetitions"] | 0).c_str());
  bool gh = false, ga = false;
  for (JsonObjectConst comp : ser["competitors"].as<JsonArrayConst>()) {
    std::string cid = comp["id"].is<int>() ? std::to_string(comp["id"].as<int>()) : std::string(comp["id"] | "");
    if (cid == (const char*)(h["team"]["id"] | "~")) { po.winsHome = comp["wins"] | 0; gh = true; }
    else if (cid == (const char*)(a["team"]["id"] | "~")) { po.winsAway = comp["wins"] | 0; ga = true; }
  }
  po.hasWins = gh && ga;
}

static bool isBreakText(const char* det) {
  return icontains(det, "intermission") || icontains(det, "end of");
}


// the ticker: everyone else's games
static bool isSec(const char* ab) {
  static const char* const SEC[] = {"ALA", "ARK", "AUB", "FLA", "UGA", "UK", "LSU", "MISS",
                                    "MSST", "MIZ", "OU", "SC", "TENN", "TEX", "TA&M", "VAN"};
  for (const char* s : SEC) if (!strcmp(s, ab)) return true;
  return false;
}

void buildTicker(JsonObjectConst feed, const char* excludeId, Sport sport, bool top25, time_t now, Game& g) {
  g.nticker = 0;
  for (JsonObjectConst ev : feed["events"].as<JsonArrayConst>()) {
    if (g.nticker >= MAX_TICK) break;
    if (!strcmp(ev["id"] | "", excludeId)) continue;
    JsonObjectConst c2, h2, a2;
    sides(ev, c2, h2, a2);
    if (h2.isNull() || a2.isNull()) continue;
    int hr = tickRank(h2), ar = tickRank(a2);
    // college: ranked games, and every SEC game
    if (top25 && !hr && !ar && !isSec(h2["team"]["abbreviation"] | "") && !isSec(a2["team"]["abbreviation"] | ""))
      continue;
    Tick& e = g.ticker[g.nticker++];
    scopy(e.home, h2["team"]["abbreviation"] | "");
    scopy(e.away, a2["team"]["abbreviation"] | "");
    scopy(e.homeId, h2["team"]["id"] | "");
    scopy(e.awayId, a2["team"]["id"] | "");
    e.hasHomeColor = hexRgb(h2["team"]["color"] | "", e.homeColor);
    e.hasAwayColor = hexRgb(a2["team"]["color"] | "", e.awayColor);
    e.homeRank = hr; e.awayRank = ar;
    JsonObjectConst s2 = c2["status"];
    GState st2 = eventState(ev);
    char score[12];
    snprintf(score, sizeof(score), "%s-%s", (const char*)(a2["score"] | ""), (const char*)(h2["score"] | ""));
    if (st2 == ST_POST) {
      scopy(e.status, "F");
      scopy(e.score, score);
    } else if (st2 == ST_IN) {
      char pl[6];
      periodLabel(sport, s2["period"] | 0, isPostseason(ev), pl);
      scopy(e.status, pl);
      scopy(e.clock, s2["displayClock"] | "");
      if (sport == HOCKEY && isBreakText(s2["type"]["shortDetail"] | "")) scopy(e.clock, "INT");
      scopy(e.score, score);
      JsonObjectConst sit = c2["situation"];
      if (sport == BASEBALL) {
        Bb b;
        baseballState(s2, sit, b);
        if (b.shrt[0]) scopy(e.status, b.shrt);
        if (b.outs >= 0 && (!strcmp(b.half, "TOP") || !strcmp(b.half, "BOT")))
          snprintf(e.clock, sizeof(e.clock), "%d OUT", b.outs);
        else e.clock[0] = 0;
      }
      if (sport == FOOTBALL) {
        e.redzone = sit["isRedZone"] | false;
        const char* p2 = sit["possession"] | "";
        if (*p2 && !strcmp(p2, h2["team"]["id"] | "~")) e.possession = 1;
        else if (*p2 && !strcmp(p2, a2["team"]["id"] | "~")) e.possession = 2;
      }
    } else {
      scopy(e.status, "SCHED");
      char md2[8], hm2[8];
      localWhen(ev["date"] | "", now, md2, hm2);
      scopy(e.kickDate, md2);
      scopy(e.kickTime, (c2["timeValid"] | true) ? hm2 : "TBD");
    }
  }
}

// ------------------------------------------------------------------- parse
bool parseGame(JsonObjectConst feed, const TeamDef& team, bool top25, time_t now, Game& g) {
  g = Game();
  JsonObjectConst mine = findEvent(feed, team);
  if (mine.isNull()) return false;
  Sport sport = leagueSport(team.league);
  JsonObjectConst c, h, a;
  sides(mine, c, h, a);
  JsonObjectConst stt = c["status"];
  GState state = eventState(mine);
  bool post = isPostseason(mine);
  g.valid = true;
  g.sport = sport;
  g.state = state;
  fillSide(h, state, g.home);
  fillSide(a, state, g.away);
  bool homeIsMine = sideMatches(h, team, false) || (!sideMatches(a, team, false) && sideMatches(h, team, true));
  g.pinnedHome = homeIsMine;
  char md[8], hm[8];
  bool today = localWhen(mine["date"] | "", now, md, hm);
  bool timeValid = c["timeValid"] | true;
  snprintf(g.kickoff, sizeof(g.kickoff), "%s %s", md, timeValid ? hm : "TBD");
  if (timeValid) scopy(g.startTime, hm);
  dayWord(mine["date"] | "", now, g.day);
  scopy(g.clock, stt["displayClock"] | "");
  g.period = stt["period"] | 0;
  periodLabel(sport, g.period, post, g.periodLabel);
  g.postseason = post;
  playoffOf(mine, c, h, a, g.po);
  int stype = mine["season"]["type"] | 0;
  g.preseason = stype == 1 || strstr(mine["season"]["slug"] | "", "pre");
  scopy(g.gameDate, md);
  scopy(g.eventDate, mine["date"] | "");
  if (state == ST_POST) {
    if (today || !md[0]) scopy(g.finalLabel, "FINAL");
    else snprintf(g.finalLabel, sizeof(g.finalLabel), "FINAL %s", md);
  }
  JsonObjectConst s = c["situation"];
  if (state == ST_IN) {
    scopy(g.lastPlay, s["lastPlay"]["text"] | "");
    if (sport == FOOTBALL) {
      scopy(g.downDistance, s["downDistanceText"] | "");
      scopy(g.shortDD, s["shortDownDistanceText"] | "");
      g.redzone = s["isRedZone"] | false;
      g.down = s["down"] | 0;
      g.distance = s["distance"] | -1;
      g.yardLine = s["yardLine"] | -1;
      g.toHome = s["homeTimeouts"] | -1;
      g.toAway = s["awayTimeouts"] | -1;
      g.playScore = s["lastPlay"]["scoreValue"] | 0;
      JsonVariantConst wp = s["lastPlay"]["probability"]["homeWinPercentage"];
      g.winHome = wp.isNull() ? -1 : (int8_t)lround((wp.as<double>()) * 100);
      const char* p = s["possession"] | "";
      if (*p && !strcmp(p, h["team"]["id"] | "~")) g.possession = 1;
      else if (*p && !strcmp(p, a["team"]["id"] | "~")) g.possession = 2;
    } else if (sport == BASEBALL) {
      Bb b;
      baseballState(stt, s, b);
      scopy(g.half, b.half); scopy(g.inningText, b.text); scopy(g.inningShort, b.shrt);
      g.balls = b.balls; g.strikes = b.strikes; g.outs = b.outs;
      for (int i = 0; i < 3; i++) g.bases[i] = b.bases[i];
    } else if (sport == HOCKEY) {
      const char* det = stt["type"]["shortDetail"] | "";
      if (!*det) det = stt["type"]["detail"] | "";
      g.intermission = isBreakText(det);
      int p = g.period ? g.period : 1;
      if (p == 1) { scopy(g.intLines[0], "1ST"); scopy(g.intLines[1], "INTERMISSION"); }
      else if (p == 2) { scopy(g.intLines[0], "2ND"); scopy(g.intLines[1], "INTERMISSION"); }
      else if (p == 3) { scopy(g.intLines[0], "END OF"); scopy(g.intLines[1], "REGULATION"); }
      else { scopy(g.intLines[0], "END OF"); char pl[6]; periodLabel(HOCKEY, p, post, pl); scopy(g.intLines[1], pl); }
    }
  }
  g.ranked = top25;

  scopy(g.eventId, mine["id"] | "");
  buildTicker(feed, g.eventId, sport, top25, now, g);
  return true;
}

void footballSummary(JsonObjectConst feed, bool college, time_t now, bool& live, time_t& nextKick, Game& tick) {
  live = false;
  nextKick = 0;
  for (JsonObjectConst ev : feed["events"].as<JsonArrayConst>()) {
    GState st = eventState(ev);
    if (st == ST_IN) live = true;
    if (st == ST_PRE) {
      time_t k = parseIso(ev["date"] | "");
      if (k && k > now - 3600 && (!nextKick || k < nextKick)) nextKick = k;
    }
  }
  buildTicker(feed, "", FOOTBALL, college, now, tick);   // resets its ticker
}

void mergeTicker(Game& g, const Game* a, const Game* b, bool ranked) {
  g.nticker = 0;
  for (const Game* src : {a, b}) {
    if (!src) continue;
    for (int i = 0; i < src->nticker && g.nticker < MAX_TICK; i++) {
      const Tick& k = src->ticker[i];
      if (!strcmp(k.home, g.home.abbr) && !strcmp(k.away, g.away.abbr)) continue;
      g.ticker[g.nticker++] = k;
    }
  }
  g.ranked = ranked;
}

// ------------------------------------------------------------------ extras
static std::string strOf(JsonVariantConst v) {
  if (v.is<const char*>()) return v.as<const char*>();
  if (v.is<int>()) return std::to_string(v.as<int>());
  return "";
}

void enrichHockey(JsonObjectConst data, Game& g) {
  g.pp = false;
  if (g.state != ST_IN) return;
  std::string want = upper(nhlCode(g.pinned().abbr));
  for (JsonObjectConst gm : data["games"].as<JsonArrayConst>()) {
    std::string ha = upper(gm["homeTeam"]["abbrev"] | ""), aa = upper(gm["awayTeam"]["abbrev"] | "");
    if (ha != want && aa != want) continue;
    JsonObjectConst clk = gm["clock"];
    if (clk["inIntermission"] | false) {
      g.intermission = true;
      std::string t = strOf(clk["timeRemaining"]);
      if (t.size() == 5 && t[0] == '0') t = t.substr(1);
      scopy(g.intermissionLeft, t.c_str());
    } else if (!clk["running"].isNull() || !clk["timeRemaining"].isNull()) {
      g.intermission = false;
      g.intermissionLeft[0] = 0;
    }
    if (g.intermission) return;
    JsonObjectConst sit = gm["situation"];
    if (sit.isNull()) return;
    int side = 0;   // 1 home, 2 away
    auto hasPP = [](JsonArrayConst d) {
      for (JsonVariantConst x : d) if (!strcmp(x | "", "PP")) return true;
      return false;
    };
    if (hasPP(sit["homeTeam"]["situationDescriptions"])) side = 1;
    else if (hasPP(sit["awayTeam"]["situationDescriptions"])) side = 2;
    else {
      int hn = sit["homeTeam"]["strength"].is<int>() ? sit["homeTeam"]["strength"].as<int>() : -1;
      int an = sit["awayTeam"]["strength"].is<int>() ? sit["awayTeam"]["strength"].as<int>() : -1;
      std::string code = strOf(sit["situationCode"]);
      if ((hn < 0 || an < 0) && code.size() == 4 && isdigit((unsigned char)code[1]) && isdigit((unsigned char)code[2])) {
        an = code[1] - '0'; hn = code[2] - '0';
      }
      if (hn >= 0 && an >= 0 && hn != an) side = hn > an ? 1 : 2;
    }
    if (!side) return;
    std::string t = strOf(sit["timeRemaining"]);
    if (t.size() == 5 && t[0] == '0') t = t.substr(1);
    g.pp = true;
    g.ppHome = side == 1;
    scopy(g.ppTime, t.c_str());
    return;
  }
}

void enrichBasketball(JsonObjectConst data, Game& g) {
  g.hasBonus = false;
  if (g.state != ST_IN) return;
  std::string want = upper(nbaCode(g.pinned().abbr));
  auto on = [](JsonVariantConst v) {
    if (v.is<bool>()) return v.as<bool>();
    std::string s = strOf(v);
    return s == "1" || s == "true" || s == "True";
  };
  for (JsonObjectConst gm : data["scoreboard"]["games"].as<JsonArrayConst>()) {
    if (upper(gm["homeTeam"]["teamTricode"] | "") != want && upper(gm["awayTeam"]["teamTricode"] | "") != want) continue;
    g.hasBonus = true;
    g.bonusHome = on(gm["homeTeam"]["inBonus"]);
    g.bonusAway = on(gm["awayTeam"]["inBonus"]);
    return;
  }
}

// ----------------------------------------------------------------- updates
int versionCompare(const char* a, const char* b) {
  while (*a || *b) {
    long x = strtol(a, (char**)&a, 10), y = strtol(b, (char**)&b, 10);
    if (x != y) return x < y ? -1 : 1;
    if (*a == '.') a++;
    if (*b == '.') b++;
    if (!isdigit((unsigned char)*a) && !isdigit((unsigned char)*b)) break;
  }
  return 0;
}

bool versionFromAsset(const char* name, char (&out)[16]) {
  const char* pre = "scoreboard-";
  size_t lp = strlen(pre), n = strlen(name);
  if (n <= lp + 4 || strncasecmp(name, pre, lp) || strcasecmp(name + n - 4, ".bin")) return false;
  size_t len = n - lp - 4;
  if (len == 0 || len >= sizeof(out)) return false;
  for (size_t i = 0; i < len; i++) {
    char c = name[lp + i];
    if (!isdigit((unsigned char)c) && c != '.') return false;
  }
  memcpy(out, name + lp, len);
  out[len] = 0;
  return isdigit((unsigned char)out[0]) != 0;
}
