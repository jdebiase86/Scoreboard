#include "sb_net.h"
#include "sb_parse.h"
#include "sb_png.h"
#include "sb_settings.h"
#include "sb_log.h"
#include "sb_events.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
#include "sb_version.h"
#include <esp_heap_caps.h>
#include <new>

// ------------------------------------------------------------------ memory
void* sbAlloc(size_t n) {
  void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return p ? p : malloc(n);
}
void sbFree(void* p) { free(p); }   // heap_caps memory is freed by free() too

// The watchdog expects the idle task to run every few seconds; a long
// parse or logo shrink must step aside now and then.
void sbBreathe() {
  static uint32_t last = 0;
  if (millis() - last > 100) { vTaskDelay(1); last = millis(); }
}

struct SpiRamAllocator : ArduinoJson::Allocator {
  void* allocate(size_t n) override { return sbAlloc(n); }
  void deallocate(void* p) override { sbFree(p); }
  void* reallocate(void* p, size_t n) override {
    void* q = heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return q ? q : realloc(p, n);
  }
};
static SpiRamAllocator spiAlloc;

// ---------------------------------------------------------------- download
// Reads the HTTPS response in 4 KB gulps: byte-at-a-time reads through TLS
// make a big college Saturday feed take many times longer.
struct NetReader {
  WiFiClient* c;
  long remaining;
  uint8_t* buf;
  int pos = 0, len = 0;
  size_t total = 0;
  bool fill() {
    if (remaining == 0) return false;
    sbBreathe();
    uint32_t t0 = millis();
    for (;;) {
      int a = c->available();
      if (a > 0) {
        int want = a < 4096 ? a : 4096;
        if (remaining > 0 && want > remaining) want = remaining;
        int n = c->read(buf, want);
        if (n > 0) {
          pos = 0; len = n; total += n;
          if (remaining > 0) remaining -= n;
          return true;
        }
      }
      if (!c->connected() && c->available() <= 0) return false;
      if (millis() - t0 > 15000) return false;
      delay(2);
    }
  }
  int read() {
    if (pos >= len && !fill()) return -1;
    return buf[pos++];
  }
  size_t readBytes(char* b, size_t n) {
    size_t k = 0;
    while (k < n) {
      if (pos >= len && !fill()) break;
      size_t m = len - pos;
      if (m > n - k) m = n - k;
      memcpy(b + k, buf + pos, m);
      pos += m; k += m;
    }
    return k;
  }
};

static uint8_t* rxbuf = nullptr;
static void confirmApp();

static bool httpGet(const String& url, HTTPClient& http, WiFiClientSecure& client) {
  client.setInsecure();   // scores aren't secret; skips certificate upkeep
  http.useHTTP10(true);   // no chunked encoding - the body streams straight in
  http.setReuse(false);   // and say "Connection: close" to match
  http.setConnectTimeout(10000);
  http.setTimeout(15000);
  if (!http.begin(client, url)) return false;
  http.setUserAgent("Mozilla/5.0 (Scoreboard)");
  int code = http.GET();
  if (code != 200) {
    sbLog("HTTP %d  %s", code, url.c_str());
    http.end();
    return false;
  }
  return true;
}

static bool fetchJson(const String& url, JsonDocument& doc, JsonDocument& filter) {
  uint32_t t0 = millis();
  WiFiClientSecure client;
  HTTPClient http;
  if (!httpGet(url, http, client)) return false;
  WiFiClient* stream = http.getStreamPtr();
  if (!stream) { http.end(); return false; }
  NetReader r{stream, (long)http.getSize(), rxbuf};
  DeserializationError e = deserializeJson(doc, r, DeserializationOption::Filter(filter),
                                           DeserializationOption::NestingLimit(40));
  http.end();
  sbLog("%s %uKB %lums %s", e ? "FAIL" : "ok", (unsigned)(r.total / 1024),
        (unsigned long)(millis() - t0), url.c_str() + 8);
  if (e) sbLog("json: %s", e.c_str());
  if (!e) confirmApp();
  return !e;
}

static uint8_t* fetchBytes(const String& url, size_t& n) {
  n = 0;
  WiFiClientSecure client;
  HTTPClient http;
  if (!httpGet(url, http, client)) return nullptr;
  long size = http.getSize();
  size_t cap = size > 0 ? (size_t)size : 400000;
  if (cap > 400000) { http.end(); return nullptr; }
  WiFiClient* stream = http.getStreamPtr();
  if (!stream) { http.end(); return nullptr; }
  uint8_t* out = (uint8_t*)sbAlloc(cap);
  if (!out) { http.end(); return nullptr; }
  NetReader r{stream, size, rxbuf};
  n = r.readBytes((char*)out, cap);
  http.end();
  if (size > 0 && n != (size_t)size) { sbFree(out); n = 0; return nullptr; }
  return out;
}

// -------------------------------------------------------------------- feeds
static const char* const PATHS[L_COUNT] = {"football/nfl", "football/college-football", "baseball/mlb",
                                           "hockey/nhl", "basketball/nba"};
static const char* NHL_LIVE = "https://api-web.nhle.com/v1/score/now";
static const char* NBA_LIVE = "https://cdn.nba.com/static/json/liveData/scoreboard/todaysScoreboard_00.json";

static bool daily(League l) { return l == L_MLB || l == L_NHL || l == L_NBA; }

static String feedUrl(League l, const char* day, int group) {
  String u = String("https://site.api.espn.com/apis/site/v2/sports/") + PATHS[l] + "/scoreboard";
  String q;
  if (l == L_CFB) q += String("groups=") + (group ? group : 80) + "&limit=300";
  if (day) { if (q.length()) q += "&"; q += String("dates=") + day; }
  if (q.length()) u += "?" + q;
  return u;
}

static void dayOffset(time_t now, int d, char (&out)[9]) {
  boardDate(now + (time_t)d * 86400, out);
}

// ------------------------------------------------------------------- state
static JsonDocument* doc;
static JsonDocument* filterSb;
static JsonDocument* filterNhl;
static JsonDocument* filterNba;
static Game* work;       // scratch while choosing
static Game* best;       // auto mode's best candidate so far
static Game* cur;        // what's on the board
// Football tickers. Whenever football is being played anywhere, the ticker
// along the bottom shows it, whatever game is on top (the Yankees on top,
// college scores underneath). College: ranked games from the all-FBS feed.
struct FootFeed {
  League league;
  Game* tick = nullptr;      // its games, as a ticker
  uint32_t at = 0;           // millis of the last download (0 = never)
  uint32_t tried = 0;        // millis of the last attempt, good or bad
  bool live = false;         // any game in it in progress
  time_t nextKick = 0;       // earliest game still to start (0 = none)
};
static FootFeed footNfl{L_NFL}, footCfb{L_CFB};
static Shown* shared;
static SemaphoreHandle_t lock;
static volatile uint32_t sharedVersion = 0;
static volatile bool kicked = false;
static volatile bool saveWanted = false;   // the wheel changed settings.pin
bool netTakeSave() { bool w = saveWanted; saveWanted = false; return w; }

// ----------------------------------------------------------- the wheel's pick
// The wheel can put any game on the board for a while: one of your teams,
// or another live football game from the ticker. Not saved: a restart, the
// game ending, or an hour on a game that isn't live puts Auto back.
// Wheel requests (one int, see netSetStop): -2 nothing asked, -1 Auto,
// 0.. another live game (TEAMS index, temporary), STOP_TEAM + i = one of
// your teams' mode (saved, like "Always <team>"), STOP_ALL + 1/2 = the full
// NFL / college ticker.
static volatile int manualReq = -2;
static volatile int manualTeam = -1;
static volatile int tickerMode = 0;      // 0 off, 1 NFL, 2 college
static uint32_t manualAt = 0;
static GState manualLastSt = ST_PRE;     // to spot the picked game finishing
static int manualLastTeam = -1;

// What the wheel shows for each game: the matchup, score / start, logo
// addresses. Your teams' come from the last time each was looked up.
struct ChanInfo {
  int team = -1;
  bool yours = false;
  League league = L_NFL;
  char away[8] = "", home[8] = "";
  bool hasAwayColor = false, hasHomeColor = false;
  uint32_t awayColor = 0, homeColor = 0;
  bool hasScore = false;
  int awayScore = 0, homeScore = 0;
  char top[16] = "";
  char awayLogo[128] = "", homeLogo[128] = "";
};
static ChanInfo* chanInfo;             // MAX_CHANNELS, net task's own
static ChanCard* cards;                // MAX_CHANNELS, shared with the panel (under lock)
static volatile int nchans = 0;
struct TeamLook { int team = -1; ChanInfo info; };
static TeamLook* looks;                // your teams' latest games
static const int NLOOKS = MAX_PICKS + 2;

static const Logo* peekLogo(const char* url, int size);
static bool logoFailedLately(const char* url, int size);
static bool logoRoom();
static bool lettersOnly(const char* url);
static const Logo* cachedLogo(const char* url, int size);

void netSetStop(int code) { manualReq = code; }
int netStop() {
  int q = manualReq;
  if (q != -2) return q;
  if (tickerMode) return STOP_ALL + tickerMode;
  if (manualTeam >= 0) return manualTeam;
  if (settings.pin >= 0) return STOP_TEAM + settings.pin;
  return -1;
}
int netTickerMode() { return tickerMode; }

int netChannels(int* out, int max) {
  if (!lock) return 0;
  xSemaphoreTake(lock, portMAX_DELAY);
  int n = nchans < max ? nchans : max;
  for (int i = 0; i < n; i++) out[i] = cards[i].team;
  xSemaphoreGive(lock);
  return n;
}

bool netCard(int team, ChanCard& out) {
  if (!lock) return false;
  bool got = false;
  xSemaphoreTake(lock, portMAX_DELAY);
  for (int i = 0; i < nchans; i++)
    if (cards[i].team == team) {
      memcpy((void*)&out, (const void*)&cards[i], sizeof(ChanCard));
      out.la.pix = out.laPix;
      out.lh.pix = out.lhPix;
      got = true;
      break;
    }
  xSemaphoreGive(lock);
  return got;
}

static int findTeam(League l, const char* abbr) {
  for (int i = 0; i < NTEAMS; i++)
    if (TEAMS[i].league == l && !strcasecmp(TEAMS[i].abbr, abbr)) return i;
  return -1;
}

static void infoFromGame(int team, const Game& g, ChanInfo& c) {
  c = ChanInfo();
  c.team = team;
  c.league = TEAMS[team].league;
  scopy(c.away, g.away.abbr); scopy(c.home, g.home.abbr);
  c.hasAwayColor = g.away.hasColor; c.awayColor = g.away.color;
  c.hasHomeColor = g.home.hasColor; c.homeColor = g.home.color;
  scopy(c.awayLogo, g.away.logo); scopy(c.homeLogo, g.home.logo);
  c.hasScore = g.state != ST_PRE && g.away.hasScore && g.home.hasScore;
  c.awayScore = g.away.score; c.homeScore = g.home.score;
  if (g.state == ST_POST) scopy(c.top, "FINAL");
  else if (g.state == ST_PRE) scopy(c.top, g.kickoff);
  else if (g.sport == BASEBALL) scopy(c.top, g.inningText[0] ? g.inningText : g.periodLabel);
  else snprintf(c.top, sizeof(c.top), "%s %s", g.periodLabel, g.clock);
}

// after every successful lookup of a team: remember its game for the wheel
static void noteLook(int team, const Game& g) {
  TeamLook* t = nullptr;
  for (int i = 0; i < NLOOKS; i++) if (looks[i].team == team) t = &looks[i];
  if (!t) for (int i = 0; i < NLOOKS; i++) if (looks[i].team < 0) { t = &looks[i]; break; }
  if (!t) t = &looks[NLOOKS - 1];
  t->team = team;
  infoFromGame(team, g, t->info);
}

static void logoUrl(League l, const char* abbr, const char* id, char (&out)[128]) {
  if (l == L_CFB) {
    if (id[0]) snprintf(out, sizeof(out), "https://a.espncdn.com/i/teamlogos/ncaa/500/%s.png", id);
    else out[0] = 0;
    return;
  }
  char lo[8]; int i = 0;
  for (; abbr[i] && i < 7; i++) lo[i] = tolower((unsigned char)abbr[i]);
  lo[i] = 0;
  snprintf(out, sizeof(out), "https://a.espncdn.com/i/teamlogos/nfl/500/%s.png", lo);
}

// your teams first, then every live football game we know about (each game
// once, named by its home team if that team is in the catalog)
static void buildChannels(int shownTeam) {
  int n = 0;
  auto has = [&](const char* home) {
    for (int i = 0; i < n; i++) if (!strcasecmp(chanInfo[i].home, home)) return true;
    return false;
  };
  for (int i = 0; i < settings.npicks && n < MAX_CHANNELS; i++) {
    int ti = settings.picks[i];
    ChanInfo& c = chanInfo[n];
    c = ChanInfo();
    bool found = false;
    for (int k = 0; k < NLOOKS; k++) if (looks[k].team == ti) { c = looks[k].info; found = true; }
    if (!found) { c.team = ti; c.league = TEAMS[ti].league; scopy(c.home, TEAMS[ti].abbr); }
    c.yours = true;
    n++;
  }
  auto fromTicker = [&](const Game* g, League l) {
    if (!g) return;
    for (int i = 0; i < g->nticker && n < MAX_CHANNELS; i++) {
      const Tick& t = g->ticker[i];
      if (!t.score[0] || !strcmp(t.status, "F")) continue;   // live games only
      if (has(t.home)) continue;
      int ti = findTeam(l, t.home);
      if (ti < 0) ti = findTeam(l, t.away);
      if (ti < 0) continue;
      bool dup = false;
      for (int k = 0; k < n; k++) if (chanInfo[k].team == ti) dup = true;
      if (dup) continue;
      ChanInfo& c = chanInfo[n++];
      c = ChanInfo();
      c.team = ti; c.league = l;
      scopy(c.away, t.away); scopy(c.home, t.home);
      c.hasAwayColor = t.hasAwayColor; c.awayColor = t.awayColor;
      c.hasHomeColor = t.hasHomeColor; c.homeColor = t.homeColor;
      const char* d = strchr(t.score, '-');
      if (d) { c.hasScore = true; c.awayScore = atoi(t.score); c.homeScore = atoi(d + 1); }
      snprintf(c.top, sizeof(c.top), "%s %s", t.status, t.clock);
      logoUrl(l, t.away, t.awayId, c.awayLogo);
      logoUrl(l, t.home, t.homeId, c.homeLogo);
    }
  };
  bool shownFootball = cur->valid && cur->sport == FOOTBALL && shownTeam >= 0;
  if (shownFootball && !(TEAMS[shownTeam].league == L_CFB && footCfb.at)) fromTicker(cur, TEAMS[shownTeam].league);
  // a feed not downloaded lately (it isn't refreshed while its own league
  // is on the board) would offer games with old scores
  if (footNfl.at && millis() - footNfl.at < 600000UL) fromTicker(footNfl.tick, L_NFL);
  if (footCfb.at && millis() - footCfb.at < 600000UL) fromTicker(footCfb.tick, L_CFB);
  // the game on now stays on the list
  if (manualTeam >= 0 && n < MAX_CHANNELS) {
    bool dup = false;
    for (int k = 0; k < n; k++) if (chanInfo[k].team == manualTeam) dup = true;
    if (!dup && cur->valid) infoFromGame(manualTeam, *cur, chanInfo[n++]);
  }
  // the cards the panel shows: copy in whichever logos are already loaded
  xSemaphoreTake(lock, portMAX_DELAY);
  for (int i = 0; i < n; i++) {
    const ChanInfo& c = chanInfo[i];
    ChanCard& k = cards[i];
    k.team = c.team; k.yours = c.yours;
    scopy(k.away, c.away); scopy(k.home, c.home);
    k.hasAwayColor = c.hasAwayColor; k.awayColor = c.awayColor;
    k.hasHomeColor = c.hasHomeColor; k.homeColor = c.homeColor;
    k.hasScore = c.hasScore; k.awayScore = c.awayScore; k.homeScore = c.homeScore;
    scopy(k.top, c.top);
    auto put = [](const Logo* src, Logo& dst, LogoPix* store) {
      dst = Logo();
      if (!src || !src->n) return;
      int m = src->n < MATCHUP_W * MATCHUP_H ? src->n : MATCHUP_W * MATCHUP_H;
      memcpy(store, src->pix, m * sizeof(LogoPix));
      dst.w = src->w; dst.h = src->h; dst.n = m; dst.pix = store;
    };
    put(peekLogo(c.awayLogo, 0), k.la, k.laPix);
    put(peekLogo(c.homeLogo, 0), k.lh, k.lhPix);
  }
  nchans = n;
  xSemaphoreGive(lock);
}

// a couple of the wheel's missing logos per refresh, so they're ready
// before anyone flicks (never more, so scores aren't held up)
static void prefetchCardLogos() {
  int fetched = 0;
  for (int i = 0; i < nchans && fetched < 2; i++)
    for (const char* u : {chanInfo[i].awayLogo, chanInfo[i].homeLogo}) {
      if (fetched >= 2 || !*u || lettersOnly(u) || peekLogo(u, 0) || logoFailedLately(u, 0)) continue;
      if (!logoRoom()) return;
      cachedLogo(u, 0);
      fetched++;
    }
}

// next/last game lookups for the day-by-day sports, so an off day doesn't
// re-scan three weeks of feeds on every refresh
struct TeamCache { int team = -1; char day[9] = ""; bool none = false; uint32_t until = 0; uint32_t allUntil = 0; };
static TeamCache tcache[MAX_PICKS + 1];

static TeamCache& cacheFor(int team) {
  for (auto& c : tcache) if (c.team == team) return c;
  for (auto& c : tcache) if (c.team < 0) { c.team = team; return c; }
  tcache[0] = TeamCache(); tcache[0].team = team; return tcache[0];
}

// Refresh a football feed when it's worth it: every 5 min while games are
// on, 10 min before the next kickoff, and otherwise every 3 hours (schedule
// changes). Between those, the last download is reused.
static void updateFoot(FootFeed& f, time_t now, bool onScreen = false) {
  uint32_t age = millis() - f.at;
  bool due = !f.at || (f.live && age > (onScreen ? 20000UL : 5 * 60000UL)) || age > (onScreen ? 10 * 60000UL : 3 * 3600000UL) ||
             (f.nextKick && now >= f.nextKick - 600 && age > 5 * 60000UL);
  if (!due) return;
  if (f.tried && millis() - f.tried < 60000UL) return;   // failed a moment ago: don't hammer
  f.tried = millis();
  if (!fetchJson(feedUrl(f.league, nullptr, 80), *doc, *filterSb)) return;
  f.at = millis();
  footballSummary(doc->as<JsonObjectConst>(), f.league == L_CFB, now, f.live, f.nextKick, *f.tick);
  sbLog("football %s: %s, %d games in ticker", f.league == L_NFL ? "NFL" : "college",
        f.live ? "live" : "nothing live", f.tick->nticker);
}

// 1 = got a game, 0 = no game in range, -1 = couldn't reach ESPN
static int teamGame(int ti, Game& g) {
  const TeamDef& t = TEAMS[ti];
  bool college = t.league == L_CFB;
  time_t now = time(nullptr);
  char today[9];
  boardDate(now, today);
  TeamCache& c = cacheFor(ti);
  // a bye week (or the off-season) was confirmed recently: don't re-download
  if (!daily(t.league) && c.none && millis() < c.until) return 0;
  // college: the conference's feed is small; a non-conference game is only
  // in the all-FBS one, and once that's known, go straight there for a while
  int group = college && millis() >= c.allUntil ? t.group : 0;
  if (!fetchJson(feedUrl(t.league, daily(t.league) ? today : nullptr, group), *doc, *filterSb))
    return -1;
  if (parseGame(doc->as<JsonObjectConst>(), t, college, now, g)) return 1;
  if (college && group) {
    if (fetchJson(feedUrl(t.league, nullptr, 80), *doc, *filterSb) &&
        parseGame(doc->as<JsonObjectConst>(), t, college, now, g)) {
      c.allUntil = millis() + 2 * 3600000UL;
      return 1;
    }
  }
  if (!daily(t.league)) {            // football feeds already cover the week
    c.none = true;
    c.until = millis() + 45 * 60000UL;
    return 0;
  }

  if (millis() < c.until) {
    if (c.none) return 0;
    char day[9]; scopy(day, c.day);
    if (fetchJson(feedUrl(t.league, day, 0), *doc, *filterSb) &&
        parseGame(doc->as<JsonObjectConst>(), t, false, now, g)) return 1;
  }
  int failed = 0;
  for (int pass = 0; pass < 2; pass++) {
    int span = pass == 0 ? 10 : 21;
    for (int d = 1; d <= span; d++) {
      if (kicked || manualReq != -2) return -1;
      char day[9];
      dayOffset(now, pass == 0 ? d : -d, day);
      if (!fetchJson(feedUrl(t.league, day, 0), *doc, *filterSb)) { failed++; continue; }
      if (parseGame(doc->as<JsonObjectConst>(), t, false, now, g)) {
        scopy(c.day, day); c.none = false;
        c.until = millis() + (pass == 0 ? 30 : 60) * 60000UL;
        return 1;
      }
    }
  }
  if (failed) return -1;   // couldn't check every day - don't conclude "no games"
  c.none = true;
  c.until = millis() + 60 * 60000UL;
  return 0;
}

// What the last full check learned about each team's game, so the board
// knows when to look harder (just before a start time) and when it can relax.
struct Sched { int team; time_t start; GState st; };
static Sched sched[MAX_PICKS + 1];
static int nsched = 0;

static void noteSchedule(int team, const Game& g) {
  for (int i = 0; i < nsched; i++)
    if (sched[i].team == team) { sched[i].start = parseIso(g.eventDate); sched[i].st = g.state; return; }
  if (nsched < MAX_PICKS + 1) sched[nsched++] = {team, parseIso(g.eventDate), g.state};
}

// From 10 minutes before a known start until 30 minutes after (games start
// late), check every minute so kickoff / first pitch / puck drop shows up
// right away.
static bool nearStart(time_t now) {
  for (int i = 0; i < nsched; i++)
    if (sched[i].st == ST_PRE && sched[i].start && now >= sched[i].start - 600 && now <= sched[i].start + 1800)
      return true;
  return false;
}

// Live games of your teams right now, all in the same sport (the highest
// one live: football first). Two or more and the board takes turns.
static const int MAX_LIVE = 4;
static int liveTeam[MAX_LIVE];
static Game* liveGame[MAX_LIVE];
static int liveScore[MAX_LIVE];   // your team's score at the last look (-1 none)
static int nlive = 0;
static int rotIdx = 0;
static uint32_t rotSince = 0;

// None of your teams live: the board takes turns between all of them,
// each one's latest final or next game (Joe: "cycle through my teams").
static int upTies[MAX_PICKS + 1];
static int nUpTies = 0;
static uint32_t tieSince = 0;

static int pinnedScore(const Game& g) {
  const Side& s = g.pinnedHome ? g.home : g.away;
  return s.hasScore ? s.score : -1;
}

// Football always wins if it's live; then the first sport with a live game
// in pick order (every live game of that sport, to take turns); else
// whoever plays next; else the most recent result.
static int autoPick(Game& out, int& chosen) {
  bool haveUp = false, haveRecent = false, anyOk = false;
  int upTeam = -1, recTeam = -1;
  Game* recent = work;   // reuse: work holds each lookup, best holds the upcoming pick
  static Game* rec = nullptr;
  if (!rec) rec = new (sbAlloc(sizeof(Game))) Game();
  int prevShown = nlive ? liveTeam[rotIdx] : -1;
  nsched = 0;
  nlive = 0;
  nUpTies = 0;
  int liveSport = -1;
  for (int i = 0; i < settings.npicks; i++) {
    if (kicked || manualReq != -2) return -1;
    int ti = settings.picks[i];
    int sp = leagueSport(TEAMS[ti].league);
    if (liveSport >= 0 && sp != liveSport) break;   // picks are in sport order
    int r = teamGame(ti, *recent);
    if (r >= 0) anyOk = true;
    if (r != 1) continue;
    noteSchedule(ti, *recent);
    noteLook(ti, *recent);
    if (nUpTies < MAX_PICKS + 1) upTies[nUpTies++] = ti;
    if (recent->state == ST_IN) {
      liveSport = sp;
      if (nlive < MAX_LIVE) {
        *liveGame[nlive] = *recent;
        liveTeam[nlive] = ti;
        liveScore[nlive] = pinnedScore(*recent);
        nlive++;
      }
      continue;
    }
    if (recent->state == ST_PRE) {
      if (!haveUp || strcmp(recent->eventDate, best->eventDate) < 0) { *best = *recent; upTeam = ti; haveUp = true; }
    } else {
      if (!haveRecent || strcmp(recent->eventDate, rec->eventDate) > 0) { *rec = *recent; recTeam = ti; haveRecent = true; }
    }
  }
  if (nlive) {
    // keep showing the same game if it's still live
    int keep = 0;
    for (int i = 0; i < nlive; i++) if (liveTeam[i] == prevShown) keep = i;
    if (liveTeam[keep] != prevShown) rotSince = millis();
    rotIdx = keep;
    out = *liveGame[rotIdx];
    chosen = liveTeam[rotIdx];
    return 1;
  }
  if (haveUp) { out = *best; chosen = upTeam; return 1; }
  if (haveRecent) { out = *rec; chosen = recTeam; return 1; }
  return anyOk ? 0 : -1;
}

// Two or more live: refresh each, drop any that ended, and pick which one
// is on top - a team that just scored jumps straight up, otherwise they
// take turns every few seconds (the setting).
static int refreshLive(Game& out, int& chosen) {
  int anyOk = 0;
  for (int i = 0; i < nlive; i++) {
    if (kicked || manualReq != -2) return -1;
    int r = teamGame(liveTeam[i], *work);   // a miss mustn't wipe the game we have
    if (r >= 0) anyOk = 1;
    if (r == 1) { *liveGame[i] = *work; noteSchedule(liveTeam[i], *liveGame[i]); noteLook(liveTeam[i], *work); }
  }
  if (!anyOk) return -1;
  uint32_t turn = ROTATE_SECS[settings.rot] * 1000UL;
  int jump = -1;
  for (int i = 0; i < nlive; i++) {
    int sc = pinnedScore(*liveGame[i]);
    if (i != rotIdx && sc > liveScore[i] && liveScore[i] >= 0) jump = i;
    liveScore[i] = sc;
  }
  int shownTeam = liveTeam[rotIdx];
  if (jump >= 0) {
    rotIdx = jump;
    rotSince = millis();
    sbLog("switching to %s: they scored", TEAMS[liveTeam[rotIdx]].abbr);
  } else if (millis() - rotSince >= turn) {
    // next game that's still live
    for (int k = 1; k <= nlive; k++) {
      int j = (rotIdx + k) % nlive;
      if (liveGame[j]->state == ST_IN) { rotIdx = j; break; }
    }
    rotSince = millis();
  }
  // games that have finished drop out of the rotation (the one on top stays
  // until its turn is up, so a final score isn't yanked away mid-glance)
  int w = 0, newIdx = 0;
  for (int i = 0; i < nlive; i++) {
    bool keep = liveGame[i]->state == ST_IN || i == rotIdx;
    if (!keep) continue;
    if (i == rotIdx) newIdx = w;
    if (w != i) {
      Game* t = liveGame[w]; liveGame[w] = liveGame[i]; liveGame[i] = t;
      liveTeam[w] = liveTeam[i]; liveScore[w] = liveScore[i];
    }
    w++;
  }
  nlive = w;
  rotIdx = newIdx;
  if (liveTeam[rotIdx] != shownTeam) sbLog("now showing %s", TEAMS[liveTeam[rotIdx]].abbr);
  out = *liveGame[rotIdx];
  chosen = liveTeam[rotIdx];
  return 1;
}

// ------------------------------------------------------------------- logos
struct CachedLogo { String url; int size = 0; Logo logo; uint32_t used = 0, failedAt = 0; };
static CachedLogo logos[80];   // matchups, celebrations, wheel cards, full ticker

static void logoCandidates(const String& url, String* out, int& n) {
  n = 0;
  if (url.indexOf("/500/") >= 0) {
    String dark = url; dark.replace("/500/", "/500-dark/");
    out[n++] = dark;
    if (dark.indexOf("/scoreboard/") >= 0) { String d2 = dark; d2.replace("/scoreboard/", "/"); out[n++] = d2; }
  }
  out[n++] = url;
  if (url.indexOf("/scoreboard/") >= 0) { String u2 = url; u2.replace("/scoreboard/", "/"); out[n++] = u2; }
}

// Already loaded? (no download) - null if not
static bool lettersOnly(const char* url);
static const Logo* peekLogo(const char* url, int size) {
  if (!url || !*url) return nullptr;
  for (auto& c : logos) if (c.size == size && c.logo.n && c.url == url) { c.used = millis(); return &c.logo; }
  return nullptr;
}
// a slot nobody has used for 10 minutes (or never) - so prefetching never
// pushes out a logo that's on a card or on the board right now
static bool logoRoom() {
  for (auto& c : logos) if (!c.url.length() || millis() - c.used > 600000UL) return true;
  return false;
}
static bool logoFailedLately(const char* url, int size) {
  for (auto& c : logos)
    if (c.size == size && !c.logo.n && c.failedAt && millis() - c.failedAt < 600000UL && c.url == url) return true;
  return false;
}

// Teams whose logo is mostly lettering or script and can't be made out at
// 26 dots: the board shows their letters instead (Joe's call - a rough logo
// that still reads is kept; only the hopeless ones go). League folder +
// file name in ESPN's logo address; college logos are named by team id.
static bool lettersOnly(const char* url) {
  static const char* const LIST[][2] = {
      {"/nba/", "bkn"}, {"/nba/", "lal"}, {"/nba/", "lac"}, {"/nba/", "det"}, {"/nba/", "dal"}, {"/nba/", "no"},
      {"/nba/", "okc"}, {"/mlb/", "det"}, {"/mlb/", "mia"}, {"/ncaa/", "344"}, {"/ncaa/", "145"}, {"/ncaa/", "96"},
      {"/nhl/", "la"},  {"/nhl/", "wsh"}};
  const char* f = strrchr(url, '/');
  if (!f) return false;
  f++;
  const char* dot = strchr(f, '.');
  size_t n = dot ? (size_t)(dot - f) : strlen(f);
  for (auto& e : LIST)
    if (strstr(url, e[0]) && strlen(e[1]) == n && !strncasecmp(f, e[1], n)) return true;
  return false;
}

// size 0: the pregame matchup (26x24); 54: celebrations
static const Logo* cachedLogo(const char* url, int size) {
  if (!url || !*url) return nullptr;
  if (size == 0 && lettersOnly(url)) return nullptr;   // letters instead
  CachedLogo* slot = nullptr;
  for (auto& c : logos)
    if (c.url == url && c.size == size) {
      c.used = millis();
      if (c.logo.n) return &c.logo;
      if (millis() - c.failedAt < 600000UL) return nullptr;   // failed lately; retry after 10 min
      slot = &c;
      break;
    }
  if (!slot) {
    slot = &logos[0];
    for (auto& c : logos) if (c.used < slot->used) slot = &c;
  }
  freeLogo(slot->logo);
  slot->url = url;
  slot->size = size;
  slot->used = millis();
  String cands[4]; int nc;
  logoCandidates(String(url), cands, nc);
  for (int i = 0; i < nc; i++) {
    size_t n;
    uint8_t* png = fetchBytes(cands[i], n);
    if (!png) { sbLog("logo %s: download failed", cands[i].c_str() + 8); continue; }
    int w, h, err = 0;
    uint8_t* rgba = decodePngRGBA(png, n, w, h, &err);
    sbFree(png);
    if (!rgba) { sbLog("logo %s: can't decode (%u bytes, error %d)", cands[i].c_str() + 8, (unsigned)n, err); continue; }
    bool ok = size ? shrinkLogo(rgba, w, h, size, size, slot->logo)
                   : shrinkLogo(rgba, w, h, MATCHUP_W, MATCHUP_H, slot->logo);
    sbFree(rgba);
    sbLog("logo %s: %s (%d px)", cands[i].c_str() + 8, ok ? "ok" : "empty", slot->logo.n);
    if (ok) return &slot->logo;
  }
  slot->failedAt = millis();
  return nullptr;
}

// ------------------------------------------------------------- animations
// A few waiting at most; the panel loop takes them in order.
static const int FXQ = 3;
static FxSpec* fxq[FXQ];
static int fxHead = 0, fxCount = 0;
static FxSpec* fxBuild;          // net task's scratch
static volatile bool fxTestReq = false;
static char fxTestName[16] = "";

static void fxQueue(FxSpec& f, const Game& g) {
  f.logo = Logo(); f.awayLogo = Logo(); f.homeLogo = Logo();
  f.fixPointers();
  bool net = WiFi.status() == WL_CONNECTED;
  auto copy = [](const Logo* l, Logo& dst, LogoPix* store, int max) {
    if (!l || !l->n) return;
    int n = l->n < max ? l->n : max;
    memcpy(store, l->pix, n * sizeof(LogoPix));
    dst.w = l->w; dst.h = l->h; dst.n = n; dst.pix = store;
  };
  const char* mine = g.valid ? g.pinned().logo : "";
  if (fxUsesLogo(f.kind) && *mine && net) copy(cachedLogo(mine, 54), f.logo, f.logoPix, FX_LOGO_MAX);
  if (f.kind == FX_KICKOFF && g.valid && net) {
    copy(cachedLogo(g.away.logo, 0), f.awayLogo, f.awayPix, 26 * 24);
    copy(cachedLogo(g.home.logo, 0), f.homeLogo, f.homePix, 26 * 24);
  }
  xSemaphoreTake(lock, portMAX_DELAY);
  if (fxCount < FXQ) {
    FxSpec* d = fxq[(fxHead + fxCount) % FXQ];
    memcpy((void*)d, (const void*)&f, sizeof(FxSpec));
    d->fixPointers();
    fxCount++;
  }
  xSemaphoreGive(lock);
}

bool fxTake(FxSpec& out) {
  if (!lock || !fxCount) return false;
  xSemaphoreTake(lock, portMAX_DELAY);
  bool got = fxCount > 0;
  if (got) {
    memcpy((void*)&out, (const void*)fxq[fxHead], sizeof(FxSpec));
    out.fixPointers();
    fxHead = (fxHead + 1) % FXQ;
    fxCount--;
  }
  xSemaphoreGive(lock);
  return got;
}

bool fxTest(const char* name) {
  if (!name || !*name || strlen(name) >= sizeof(fxTestName)) return false;
  scopy(fxTestName, name);
  fxTestReq = true;
  return true;
}

static void serviceFxTest() {
  if (!fxTestReq) return;
  fxTestReq = false;
  char name[16];
  scopy(name, fxTestName);
  bool have = cur->valid;
  if (!testEvent(name, have ? cur : nullptr, *fxBuild)) { sbLog("test animation '%s': unknown", name); return; }
  sbLog("test animation: %s", name);
  if (fxBuild->kind == FX_WIN) {
    int team = -1;
    xSemaphoreTake(lock, portMAX_DELAY); team = shared->team; xSemaphoreGive(lock);
    if (have && team >= 0) winWords(TEAMS[team].name, TEAMS[team].league == L_CFB, fxBuild->winText);
    else scopy(fxBuild->winText, "GIANTS WIN");
  }
  static Game* none = nullptr;
  if (!none) none = new (sbAlloc(sizeof(Game))) Game();
  fxQueue(*fxBuild, have ? *cur : *none);
}

// What each team's game looked like at the last look, to spot what changed.
// Only a recent look counts: comparing with one from hours (or a long
// rotation) ago would set off a touchdown for points scored long since.
struct Seen { int team = -1; Game* g = nullptr; uint32_t used = 0; };
static Seen seen[6];

static void checkEvents(int team, const Game& g) {
  Seen* s = nullptr;
  for (auto& e : seen) if (e.team == team) s = &e;
  uint32_t fresh = 150000UL + (uint32_t)nlive * ROTATE_SECS[settings.rot] * 1000UL;
  if (s && millis() - s->used < fresh) {
    FxKind k = detectEvent(*s->g, g, *fxBuild);
    if (k != FX_NONE) {
      static const char* const NAMES[] = {"", "touchdown", "kickoff", "quarter break", "field goal", "flag",
                                          "first down", "goal", "run", "home run", "three", "win"};
      sbLog("%s: %s", TEAMS[team].abbr, NAMES[k]);
      fxQueue(*fxBuild, g);
    }
    // the game just ended and your team won: the win card (after any
    // last-second touchdown that came with it)
    if (detectWin(*s->g, g, *fxBuild)) {
      winWords(TEAMS[team].name, TEAMS[team].league == L_CFB, fxBuild->winText);
      sbLog("%s: win", TEAMS[team].abbr);
      fxQueue(*fxBuild, g);
    }
  }
  if (!s) {
    s = &seen[0];
    for (auto& e : seen) if (e.used < s->used) s = &e;
    s->team = team;
  }
  s->used = millis();
  // ESPN sometimes drops the last play for a poll; keep the one we had so
  // the same penalty coming back doesn't count as a new one
  char keep[sizeof(g.lastPlay)];
  bool hold = !g.lastPlay[0] && !strcmp(s->g->eventId, g.eventId);
  if (hold) scopy(keep, s->g->lastPlay);
  *s->g = g;
  if (hold) scopy(s->g->lastPlay, keep);
}

static void forgetEvents() {
  for (auto& e : seen) { e.team = -1; e.used = 0; }
  xSemaphoreTake(lock, portMAX_DELAY);
  fxCount = 0;
  xSemaphoreGive(lock);
}

// ------------------------------------------------------------ full ticker
static FullTicker* ftShared;
static volatile uint32_t ftVersion = 0;
static volatile bool ftBusy = false;

bool netFullTicker(FullTicker& out, uint32_t& version) {
  if (!lock || version == ftVersion || ftBusy) return false;
  xSemaphoreTake(lock, portMAX_DELAY);
  out.mode = ftShared->mode;
  out.n = ftShared->n;
  for (int i = 0; i < out.n; i++) {
    FullGame& d = out.g[i];
    const FullGame& s = ftShared->g[i];
    d.t = s.t;
    memcpy(d.pa, s.pa, s.la.n * sizeof(LogoPix));
    memcpy(d.ph, s.ph, s.lh.n * sizeof(LogoPix));
    d.la = s.la; d.la.pix = d.pa;
    d.lh = s.lh; d.lh.pix = d.ph;
  }
  version = ftVersion;
  xSemaphoreGive(lock);
  return true;
}

// The feed for the mode, refreshed often while it's on screen; games live
// first, then finished, then still to come; two or three new logos a pass.
static void updateFullTicker(time_t now) {
  int mode = tickerMode;
  FootFeed* f = mode == 1 ? &footNfl : mode == 2 ? &footCfb : nullptr;
  if (f) updateFoot(*f, now, true);
  League l = mode == 1 ? L_NFL : L_CFB;
  int order[MAX_TICK], n = 0;
  if (f && f->at) {
    const Game& t = *f->tick;
    for (int pass = 0; pass < 3; pass++)
      for (int i = 0; i < t.nticker && n < MAX_TICK; i++) {
        const Tick& k = t.ticker[i];
        bool fin = !strcmp(k.status, "F"), sched = !k.score[0];
        int kind = fin ? 1 : sched ? 2 : 0;
        if (kind == pass) order[n++] = i;
      }
  }
  int fetched = 0;
  ftBusy = true;   // the panel waits for the finished list
  for (int k = 0; k < n; k++) {
    const Tick& t = f->tick->ticker[order[k]];
    char ua[128], uh[128];
    logoUrl(l, t.away, t.awayId, ua);
    logoUrl(l, t.home, t.homeId, uh);
    const Logo* la = lettersOnly(ua) ? nullptr : peekLogo(ua, SMALL_LOGO);
    const Logo* lh = lettersOnly(uh) ? nullptr : peekLogo(uh, SMALL_LOGO);
    if (!la && fetched < 3 && *ua && !lettersOnly(ua) && !logoFailedLately(ua, SMALL_LOGO) && logoRoom()) { la = cachedLogo(ua, SMALL_LOGO); fetched++; }
    if (!lh && fetched < 3 && *uh && !lettersOnly(uh) && !logoFailedLately(uh, SMALL_LOGO) && logoRoom()) { lh = cachedLogo(uh, SMALL_LOGO); fetched++; }
    xSemaphoreTake(lock, portMAX_DELAY);
    FullGame& d = ftShared->g[k];
    d.t = t;
    auto put = [](const Logo* src, Logo& dst, LogoPix* store) {
      dst = Logo();
      if (!src || !src->n) return;
      int m = src->n < SMALL_LOGO * SMALL_LOGO ? src->n : SMALL_LOGO * SMALL_LOGO;
      memcpy(store, src->pix, m * sizeof(LogoPix));
      dst.w = src->w; dst.h = src->h; dst.n = m; dst.pix = store;
    };
    put(la, d.la, d.pa);
    put(lh, d.lh, d.ph);
    xSemaphoreGive(lock);
  }
  xSemaphoreTake(lock, portMAX_DELAY);
  ftShared->mode = mode;
  ftShared->n = n;
  ftBusy = false;
  ftVersion++;
  xSemaphoreGive(lock);
}

// ------------------------------------------------------------------ publish
static const Logo* pub16a = nullptr;
static const Logo* pub16h = nullptr;
static void publish(const Game* g, int team, NetStatus st, const Logo* la, const Logo* lh) {
  xSemaphoreTake(lock, portMAX_DELAY);
  if (g) shared->game = *g;
  else shared->game.valid = false;
  shared->team = team;
  shared->status = st;
  auto copyLogo = [](const Logo* src, Logo& dst, LogoPix* store) {
    dst = Logo();
    if (!src || !src->n) return;
    int n = src->n < MATCHUP_W * MATCHUP_H ? src->n : MATCHUP_W * MATCHUP_H;
    memcpy(store, src->pix, n * sizeof(LogoPix));
    dst.w = src->w; dst.h = src->h; dst.n = n; dst.pix = store;
  };
  copyLogo(la, shared->away, shared->awayPix);
  copyLogo(lh, shared->home, shared->homePix);
  auto copy16 = [](const Logo* src, Logo& dst, LogoPix* store) {
    dst = Logo();
    if (!src || !src->n) return;
    int n = src->n < SMALL_LOGO * SMALL_LOGO ? src->n : SMALL_LOGO * SMALL_LOGO;
    memcpy(store, src->pix, n * sizeof(LogoPix));
    dst.w = src->w; dst.h = src->h; dst.n = n; dst.pix = store;
  };
  copy16(g ? pub16a : nullptr, shared->away16, shared->away16Pix);
  copy16(g ? pub16h : nullptr, shared->home16, shared->home16Pix);
  sharedVersion++;
  xSemaphoreGive(lock);
}

bool netSnapshot(Shown& out, uint32_t& version) {
  if (!lock || version == sharedVersion) return false;
  xSemaphoreTake(lock, portMAX_DELAY);
  out.game = shared->game;
  out.team = shared->team;
  out.status = shared->status;
  memcpy(out.awayPix, shared->awayPix, sizeof(out.awayPix));
  memcpy(out.homePix, shared->homePix, sizeof(out.homePix));
  out.away = shared->away; out.away.pix = out.awayPix;
  out.home = shared->home; out.home.pix = out.homePix;
  memcpy(out.away16Pix, shared->away16Pix, sizeof(out.away16Pix));
  memcpy(out.home16Pix, shared->home16Pix, sizeof(out.home16Pix));
  out.away16 = shared->away16; out.away16.pix = out.away16Pix;
  out.home16 = shared->home16; out.home16.pix = out.home16Pix;
  version = sharedVersion;
  xSemaphoreGive(lock);
  return true;
}

void netKick() { kicked = true; }


// ------------------------------------------------------------------ updates
volatile int otaPercent = -1;
char otaNewVersion[16] = "";
static volatile bool otaAsked = false;
static char otaLine[96] = "Not checked yet";
static JsonDocument* filterRel;

void otaRequest() { otaAsked = true; }
String otaStatus() { return String(otaLine); }

static void otaNote(const char* fmt, const char* a = "", const char* b = "") {
  char when[16] = "";
  time_t now = time(nullptr);
  if (now > 1700000000) {
    struct tm lt;
    localtime_r(&now, &lt);
    strftime(when, sizeof(when), "%-I:%M %p", &lt);
  }
  char msg[80];
  snprintf(msg, sizeof(msg), fmt, a, b);
  snprintf(otaLine, sizeof(otaLine), "%s%s%s", when, when[0] ? ": " : "", msg);
  sbLog("update: %s", msg);
}

// Download and install one firmware file. Follows GitHub's redirect to its
// file server by hand. Only a complete, valid image is ever switched to -
// anything less and the board keeps running what it has.
static bool otaInstall(String url, const char* version, long apiSize) {
  for (int hop = 0; hop < 5; hop++) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.useHTTP10(true);
    http.setReuse(false);
    http.setConnectTimeout(10000);
    http.setTimeout(20000);
    const char* keys[] = {"Location"};
    http.collectHeaders(keys, 1);
    if (!http.begin(client, url)) { otaNote("couldn't open the download"); return false; }
    http.setUserAgent("Mozilla/5.0 (Scoreboard)");
    int code = http.GET();
    if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
      url = http.header("Location");
      http.end();
      if (!url.length()) { otaNote("download redirect went nowhere"); return false; }
      continue;
    }
    if (code != 200) {
      char c[8]; snprintf(c, sizeof(c), "%d", code);
      otaNote("download failed (HTTP %s)", c);
      http.end();
      return false;
    }
    long size = http.getSize();
    if (size <= 0) size = apiSize;                        // GitHub's own figure
    else if (apiSize > 0 && size != apiSize) { otaNote("download size doesn't match, skipped"); http.end(); return false; }
    WiFiClient* s = http.getStreamPtr();
    if (!s || size < 100000 || size > 3 * 1024 * 1024) { otaNote("download looks wrong, skipped"); http.end(); return false; }
    if (!Update.begin(size)) { otaNote("no room to install (%s)", Update.errorString()); http.end(); return false; }
    scopy(otaNewVersion, version);
    otaPercent = 0;
    long got = 0;
    uint32_t last = millis();
    while (got < size) {
      int a = s->available();
      if (a > 0) {
        int want = a < 4096 ? a : 4096;
        if (want > size - got) want = size - got;
        int n = s->read(rxbuf, want);
        if (n > 0 && got == 0) {
          // before anything touches flash: is this really an ESP32-S3 app?
          // (byte 0 = 0xE9, chip id 9 at 12-13, app-description magic at 32)
          while (n < 48 && millis() - last < 20000) {
            int m = s->read(rxbuf + n, 48 - n);
            if (m > 0) n += m; else delay(5);
          }
          bool ok = n >= 48 && rxbuf[0] == 0xE9 && rxbuf[12] == 9 && rxbuf[13] == 0 &&
                    rxbuf[32] == 0x32 && rxbuf[33] == 0x54 && rxbuf[34] == 0xCD && rxbuf[35] == 0xAB;
          if (!ok) {
            Update.abort();
            otaPercent = -1;
            http.end();
            otaNote("that file isn't a scoreboard program - skipped");
            return false;
          }
        }
        if (n > 0) {
          if (Update.write(rxbuf, n) != (size_t)n) break;
          got += n;
          otaPercent = (int)(got * 100 / size);
          last = millis();
        }
      } else {
        if (!s->connected() || millis() - last > 20000) break;
        delay(5);
      }
      sbBreathe();
    }
    http.end();
    if (got != size || !Update.end(true)) {
      if (Update.isRunning()) Update.abort();
      otaPercent = -1;
      otaNote("install stopped part way (%s) - still on the old version", Update.errorString());
      return false;
    }
    Preferences p;
    p.begin("sb", false);
    p.putString("updated", version);
    p.end();
    otaNote("installed %s, restarting", version);
    otaPercent = 100;
    delay(1500);
    ESP.restart();
    return true;
  }
  otaNote("too many download redirects");
  return false;
}

// Set once this program has proved it works (Wi-Fi up, a download done).
// Until then a fresh update is "on trial": if it crashes or loses power,
// the chip goes back to the previous version by itself.
static bool appConfirmed = false;
static void confirmApp() {
  if (appConfirmed) return;
  appConfirmed = true;
  esp_ota_mark_app_valid_cancel_rollback();
  sbLog("this version (%s) is working - kept", FW_VERSION);
}

static void otaCheck(bool userAsked) {
  if (!appConfirmed) return;   // never install on top of an unproven version
  char bad[16] = "";
  {
    Preferences p;
    p.begin("sb", true);
    scopy(bad, p.getString("bad", "").c_str());
    p.end();
  }
  String api = String("https://api.github.com/repos/") + SB_REPO + "/releases/latest";
  WiFiClientSecure client;
  HTTPClient http;
  client.setInsecure();
  http.useHTTP10(true);
  http.setReuse(false);
  http.setConnectTimeout(10000);
  http.setTimeout(15000);
  if (!http.begin(client, api)) { otaNote("couldn't reach GitHub"); return; }
  http.setUserAgent("Mozilla/5.0 (Scoreboard)");
  http.addHeader("Accept", "application/vnd.github+json");
  int code = http.GET();
  if (code == 404) { http.end(); otaNote("no updates published yet (on %s)", FW_VERSION); return; }
  if (code != 200) {
    char c[8]; snprintf(c, sizeof(c), "%d", code);
    http.end();
    otaNote("GitHub answered %s - will try later", c);
    return;
  }
  WiFiClient* s = http.getStreamPtr();
  if (!s) { http.end(); otaNote("GitHub hung up - will try later"); return; }
  NetReader r{s, (long)http.getSize(), rxbuf};
  DeserializationError e = deserializeJson(*doc, r, DeserializationOption::Filter(*filterRel),
                                           DeserializationOption::NestingLimit(20));
  http.end();
  if (e) { otaNote("couldn't read GitHub's answer (%s)", e.c_str()); return; }
  char bestV[16] = "";
  String bestUrl;
  long bestSize = 0;
  for (JsonObjectConst a : (*doc)["assets"].as<JsonArrayConst>()) {
    char v[16];
    if (!versionFromAsset(a["name"] | "", v)) continue;
    if (!bestV[0] || versionCompare(v, bestV) > 0) {
      scopy(bestV, v);
      bestUrl = a["browser_download_url"] | "";
      bestSize = a["size"] | 0L;
    }
  }
  if (!bestV[0]) { otaNote("latest release has no scoreboard-x.y.bin file (on %s)", FW_VERSION); return; }
  if (versionCompare(bestV, FW_VERSION) <= 0) { otaNote("up to date (%s)", FW_VERSION); return; }
  if (!userAsked && !strcmp(bestV, bad)) {
    otaNote("%s didn't work on this board last time - press Check for updates to try again", bestV);
    return;
  }
  otaNote("found %s, installing", bestV);
  otaInstall(bestUrl, bestV, bestSize);
}

// --------------------------------------------------------------- main loop
// How often it asks ESPN:
//   a game on the board is live ........ that game every 5 s
//   within 10 min of a known start ..... everyone every minute
//   otherwise .......................... the game on the board every 2 min,
//                                         everyone every 15 min
// and after a failed request it backs off: 10 s, 20 s, 40 s ... up to 5 min.
static const uint32_t LIVE_MS = 5000, NEAR_MS = 60000, IDLE_MS = 120000, FULL_MS = 15 * 60000UL;

static void netTask(void*) {
  int follow = -1;           // auto mode: the team on the board
  uint32_t lastFull = 0;     // last time every team was checked (0 = never)
  int failures = 0;
  for (;;) {
    if (kicked) {
      kicked = false;
      for (auto& c : tcache) c = TeamCache();
      follow = -1; lastFull = 0; nsched = 0; nlive = 0; rotIdx = 0;
      manualTeam = -1;
      footNfl.at = footCfb.at = footNfl.tried = footCfb.tried = 0;
      cur->valid = false;
      forgetEvents();
      publish(nullptr, -1, NS_LOADING, nullptr, nullptr);
    }
    serviceFxTest();
    if (WiFi.status() != WL_CONNECTED || time(nullptr) < 1700000000) { delay(500); continue; }
    // updates: a few minutes after start-up, nightly between 4 and 5am (not
    // during a live game), and whenever the settings page asks
    {
      static uint32_t bootCheckAt = millis() + 180000UL;
      static int checkedDay = -1;
      time_t nowt = time(nullptr);
      struct tm lt;
      localtime_r(&nowt, &lt);
      bool liveNow = cur->valid && cur->state == ST_IN;
      bool nightly = lt.tm_hour == 4 && lt.tm_yday != checkedDay && !liveNow;
      if (appConfirmed && (otaAsked || (bootCheckAt && millis() > bootCheckAt) || nightly)) {
        bool asked = otaAsked;
        otaAsked = false;
        bootCheckAt = 0;
        checkedDay = lt.tm_yday;
        otaCheck(asked);
      }
    }
    int m = __atomic_exchange_n((int*)&manualReq, -2, __ATOMIC_SEQ_CST);
    if (m != -2) {
      int newManual = -1, newPin = settings.pin, newTicker = 0;
      if (m == -1) newPin = -1;                                     // Auto
      else if (m >= STOP_ALL) { newTicker = m - STOP_ALL; newManual = manualTeam; }
      else if (m >= STOP_TEAM) newPin = m - STOP_TEAM;              // a team's mode (saved)
      else newManual = m;                                           // another live game
      if (newTicker != tickerMode) sbLog("wheel: %s", newTicker == 1 ? "all NFL" : newTicker == 2 ? "all college" : "ticker mode off");
      tickerMode = newTicker;
      if (newPin != settings.pin) {
        sbLog("wheel: %s", newPin >= 0 ? TEAMS[newPin].name : "Auto");
        settings.pin = newPin;
        saveWanted = true;   // saved by the loop, on the settings page's core
      }
      if (newManual != manualTeam) {
        if (newManual >= 0) sbLog("wheel: %s", TEAMS[newManual].name);
        manualTeam = newManual;
        manualAt = millis();
        manualLastTeam = -1;
      }
      if (m < STOP_ALL) { lastFull = 0; follow = -1; }   // look again straight away
      if (tickerMode) updateFullTicker(time(nullptr));
    }
    int pin = manualTeam >= 0 ? manualTeam : settings.pin;
    if (settings.npicks == 0 && pin < 0) {
      publish(nullptr, -1, NS_NOGAME, nullptr, nullptr);
      if (tickerMode) updateFullTicker(time(nullptr));
      for (int i = 0; i < (tickerMode ? 150 : 10) && manualReq == -2 && !kicked; i++) delay(100);
      continue;
    }

    uint32_t t0 = millis();
    time_t now = time(nullptr);
    bool near = nearStart(now);
    bool full = !lastFull || millis() - lastFull >= FULL_MS || (near && millis() - lastFull >= NEAR_MS - 2000);
    int team = -1, r;
    const char* kind;
    if (pin >= 0) {
      kind = "one team";
      team = pin;
      r = teamGame(team, *work);
      if (r == 1) { *cur = *work; nsched = 0; noteSchedule(team, *cur); }
    } else if (full || follow < 0 || !cur->valid) {
      kind = "all teams";
      int was = follow;
      r = autoPick(*work, team);
      if (r >= 0) lastFull = millis();
      // taking turns between your teams: stay on the one showing until its turn is up
      if (r == 1 && nUpTies >= 2 && nlive == 0 && was >= 0 && was != team) {
        bool tied = false;
        for (int i = 0; i < nUpTies; i++) if (upTies[i] == was) tied = true;
        if (tied && millis() - tieSince < ROTATE_SECS[settings.rot] * 1000UL && teamGame(was, *best) == 1) { *work = *best; team = was; }
        else tieSince = millis();
      }
      if (r == 1) { *cur = *work; follow = team; }
    } else if (nlive >= 2) {
      kind = "taking turns";
      r = refreshLive(*work, team);
      if (r == 1) { *cur = *work; follow = team; }
      if (nlive < 2 && cur->state != ST_IN) lastFull = 0;   // all done: look at everyone
    } else {
      kind = "board game";
      // nothing of yours live: take turns between all your teams
      if (nUpTies >= 2 && nlive == 0 && millis() - tieSince >= ROTATE_SECS[settings.rot] * 1000UL) {
        int at = 0;
        for (int i = 0; i < nUpTies; i++) if (upTies[i] == follow) at = i;
        follow = upTies[(at + 1) % nUpTies];
        tieSince = millis();
      }
      team = follow;
      r = teamGame(team, *work);
      if (r == 1) { *cur = *work; noteSchedule(team, *cur); }
      if (r == 0) lastFull = 0;   // its game vanished: look at everyone next time
    }
    if (kicked || manualReq != -2) continue;
    // a picked team with no game (bye week, off-season) or no answer: Auto
    // again after an hour, same as a game that isn't live
    if (manualTeam >= 0 && r != 1 && millis() - manualAt > 3600000UL) {
      sbLog("wheel: nothing for an hour, back to %s", settings.pin >= 0 ? "your team" : "Auto");
      manualTeam = -1;
      lastFull = 0;
    }

    if (r == 1) {
      failures = 0;
      const TeamDef& td = TEAMS[team];
      // The ticker. College on top: every ranked game in the country, not
      // just the conference feed the game came from. Anything else on top:
      // if football is being played anywhere, football scores underneath.
      if (td.league == L_CFB) {
        updateFoot(footCfb, now);
        if (footCfb.at) mergeTicker(*cur, footCfb.tick, nullptr, true);
      } else if (td.league != L_NFL) {
        updateFoot(footCfb, now);
        updateFoot(footNfl, now);
        bool nfl = footNfl.at && footNfl.live, cfb = footCfb.at && footCfb.live;
        if (nfl || cfb) mergeTicker(*cur, nfl ? footNfl.tick : nullptr, cfb ? footCfb.tick : nullptr, cfb);
      }
      if (cur->state == ST_IN && cur->sport == HOCKEY && fetchJson(NHL_LIVE, *doc, *filterNhl))
        enrichHockey(doc->as<JsonObjectConst>(), *cur);
      if (cur->state == ST_IN && cur->sport == BASKETBALL && fetchJson(NBA_LIVE, *doc, *filterNba))
        enrichBasketball(doc->as<JsonObjectConst>(), *cur);
      const Logo *la = nullptr, *lh = nullptr;
      if (cur->state == ST_PRE) {
        la = cachedLogo(cur->away.logo, 0);
        lh = cachedLogo(cur->home.logo, 0);
      }
      // a live game's celebration logo, loaded ahead so a touchdown isn't
      // held up downloading it
      if (cur->state == ST_IN) cachedLogo(cur->pinned().logo, 54);
      // animations for your own teams only, not a game you're just looking in on
      bool yours = false;
      for (int i = 0; i < settings.npicks; i++) if (settings.picks[i] == team) yours = true;
      if (yours) checkEvents(team, *cur);
      // a picked game that just finished, or an hour on one that isn't live: Auto again
      if (manualTeam >= 0) {
        bool ended = manualLastTeam == team && manualLastSt == ST_IN && cur->state == ST_POST;
        manualLastSt = cur->state; manualLastTeam = team;
        if (ended || (cur->state != ST_IN && millis() - manualAt > 3600000UL)) {
          sbLog("wheel: %s, back to %s", ended ? "game over" : "an hour on a game that isn't live", settings.pin >= 0 ? "your team" : "Auto");
          manualTeam = -1;
          lastFull = 0;
        }
      }
      // 16-dot logos for the full-game screen (once a game is under way)
      pub16a = cur->state != ST_PRE ? cachedLogo(cur->away.logo, SMALL_LOGO) : nullptr;
      pub16h = cur->state != ST_PRE ? cachedLogo(cur->home.logo, SMALL_LOGO) : nullptr;
      publish(cur, team, NS_OK, la, lh);
      noteLook(team, *cur);
      buildChannels(team);
      prefetchCardLogos();
      buildChannels(team);   // with any logos that just arrived
    } else if (r == 0) {
      failures = 0;
      cur->valid = false;
      publish(nullptr, pin, NS_NOGAME, nullptr, nullptr);
    } else {
      // keep showing the last good game through a short outage
      failures++;
      if (failures >= 3 && !cur->valid) publish(nullptr, -1, NS_OFFLINE, nullptr, nullptr);
      else if (failures >= 12) { cur->valid = false; publish(nullptr, -1, NS_OFFLINE, nullptr, nullptr); }
    }

    bool live = cur->valid && cur->state == ST_IN;
    uint32_t wait = live ? LIVE_MS : (nearStart(time(nullptr)) ? NEAR_MS : IDLE_MS);
    if (tickerMode) {                                   // full ticker on screen: keep it fresh
      updateFullTicker(time(nullptr));
      if (wait > 15000UL) wait = 15000UL;
    }
    // your teams taking turns: come back when the turn is up
    if (!live && nUpTies >= 2 && nlive == 0 && settings.pin < 0 && manualTeam < 0) {
      uint32_t turn = ROTATE_SECS[settings.rot] * 1000UL;
      if (wait > turn) wait = turn;
    }
    if (r < 0) {
      wait = 10000UL << (failures > 5 ? 5 : failures - 1);
      if (wait > 300000UL) wait = 300000UL;
    }
    if (tickerMode && wait > 15000UL) wait = 15000UL;   // the full ticker stays fresh even if your team's feed is down
    sbLog("%s (%s): %s %s v %s, %s; took %lums; next in %lus; heap %uK", r == 1 ? "showing" : r == 0 ? "no game" : "offline",
          kind, r == 1 ? TEAMS[team].abbr : "", r == 1 ? cur->away.abbr : "", r == 1 ? cur->home.abbr : "",
          r == 1 ? (cur->state == ST_IN ? "live" : cur->state == ST_PRE ? "upcoming" : "final") : "",
          (unsigned long)(millis() - t0), (unsigned long)(wait / 1000), ESP.getFreeHeap() / 1024);
    uint32_t t1 = millis();
    while (millis() - t1 < wait && !kicked && !otaAsked && manualReq == -2) {
      serviceFxTest();
      delay(100);
    }
  }
}

void netStart() {
  rxbuf = (uint8_t*)heap_caps_malloc(4096, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  doc = new JsonDocument(&spiAlloc);
  filterSb = new JsonDocument(&spiAlloc);
  filterNhl = new JsonDocument(&spiAlloc);
  filterNba = new JsonDocument(&spiAlloc);
  buildScoreboardFilter(*filterSb);
  buildNhlFilter(*filterNhl);
  buildNbaFilter(*filterNba);
  filterRel = new JsonDocument(&spiAlloc);
  (*filterRel)["tag_name"] = true;
  JsonObject fa = (*filterRel)["assets"].add<JsonObject>();
  fa["name"] = true;
  fa["browser_download_url"] = true;
  fa["size"] = true;
  work = new (sbAlloc(sizeof(Game))) Game();
  for (auto& g : liveGame) g = new (sbAlloc(sizeof(Game))) Game();
  best = new (sbAlloc(sizeof(Game))) Game();
  cur = new (sbAlloc(sizeof(Game))) Game();
  footNfl.tick = new (sbAlloc(sizeof(Game))) Game();
  footCfb.tick = new (sbAlloc(sizeof(Game))) Game();
  shared = new (sbAlloc(sizeof(Shown))) Shown();
  for (auto& q : fxq) q = new (sbAlloc(sizeof(FxSpec))) FxSpec();
  fxBuild = new (sbAlloc(sizeof(FxSpec))) FxSpec();
  for (auto& e : seen) e.g = new (sbAlloc(sizeof(Game))) Game();
  ftShared = new (sbAlloc(sizeof(FullTicker))) FullTicker();
  chanInfo = (ChanInfo*)sbAlloc(sizeof(ChanInfo) * MAX_CHANNELS);
  for (int i = 0; i < MAX_CHANNELS; i++) new (&chanInfo[i]) ChanInfo();
  cards = (ChanCard*)sbAlloc(sizeof(ChanCard) * MAX_CHANNELS);
  for (int i = 0; i < MAX_CHANNELS; i++) new (&cards[i]) ChanCard();
  looks = (TeamLook*)sbAlloc(sizeof(TeamLook) * NLOOKS);
  for (int i = 0; i < NLOOKS; i++) new (&looks[i]) TeamLook();
  lock = xSemaphoreCreateMutex();
  // core 0, away from the panel loop; TLS needs a deep stack
  xTaskCreatePinnedToCore(netTask, "net", 20480, nullptr, 1, nullptr, 0);
}
