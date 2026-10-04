/*
  Scoreboard - Seengreat "RGB Matrix HUB75 S3" + 64x64 P3 panel

  The browser preview (scoreboard_live.py), running on the board itself:
  it fetches scores straight from ESPN over Wi-Fi and draws them exactly
  the way the preview does (the drawing code is checked pixel-for-pixel
  against it on a computer before every build).

  First power-up: the board makes a Wi-Fi network called Scoreboard-XXXX.
  Join it on a phone, a setup page pops up: home Wi-Fi + teams. Done.
  Later changes: http://scoreboard.local on the home Wi-Fi.
  Changed routers? Hold the BOOT button for 5 seconds: back to setup.

  Build: ESP32S3 Dev Module, USB CDC on boot, 16MB flash, OPI PSRAM,
  partitions "16M Flash (3MB APP/9.9MB FATFS)", C++17,
  -DPNG_MAX_BUFFERED_PIXELS=8194 (ESPN's logos are 500 px wide).
*/
#include <WiFi.h>
#include <new>
#include "sb_settings.h"
#include "sb_panel.h"
#include "sb_render.h"
#include "sb_net.h"
#include "sb_portal.h"
#include "sb_log.h"
#include "sb_wheel.h"
#include <Preferences.h>

// A fresh over-the-air update starts "on trial": the core is told not to
// approve it at start-up; sb_net approves it once Wi-Fi and a download have
// worked. If it crashes or loses power first, the chip rolls back by itself.
extern "C" bool verifyRollbackLater() { return true; }

#include "sb_version.h"
#define BOOT_PIN 0

enum Mode { M_SETUP, M_CONNECTING, M_FALLBACK, M_CONNECTED_MSG, M_RUNNING };
static Mode mode;
static uint32_t modeAt = 0, lastTry = 0, lastPairAt = 0;
static String apName;
static bool netStarted = false;
static Frame* fb;   // in PSRAM (internal RAM is kept for Wi-Fi and TLS)
static Shown* shown;
static uint32_t shownVersion = 0;
static int pairN = 0;
static bool dirty = true;

// Score glow: after your team scores its number stays gold for a minute;
// after the other team scores theirs flashes in their colour a few times.
// Remembered per game, so taking turns between two games keeps each one's.
struct ScoreMemo { char eventId[16] = ""; bool pinnedHome = true; int mine = -1, other = -1;
                   uint32_t glowAt = 0, flashAt = 0, seenAt = 0; };
static ScoreMemo memos[4];
static const uint32_t GLOW_MS = 60000, FLASH_MS = 4000;

static ScoreMemo* memoFor(const Game& g) {
  for (auto& m : memos) if (!strcmp(m.eventId, g.eventId)) return &m;
  ScoreMemo* o = &memos[0];
  for (auto& m : memos) if (m.eventId[0] == 0 || m.glowAt < o->glowAt) o = &m;
  *o = ScoreMemo();
  scopy(o->eventId, g.eventId);
  o->pinnedHome = g.pinnedHome;
  return o;
}

static void noteScores(const Game& g) {
  if (!g.valid || !g.eventId[0]) return;
  ScoreMemo* m = memoFor(g);
  const Side& me = g.pinned();
  const Side& them = g.other();
  if (m->pinnedHome != g.pinnedHome) { m->pinnedHome = g.pinnedHome; m->mine = m->other = -1; }
  // back to a game after a long while (it was off the board): just catch up
  if (m->seenAt && millis() - m->seenAt > 150000UL) m->mine = m->other = -1;
  m->seenAt = millis();
  if (me.hasScore) {
    if (m->mine >= 0 && me.score > m->mine && g.state == ST_IN) m->glowAt = millis() | 1;
    m->mine = me.score;
  }
  if (them.hasScore) {
    if (m->other >= 0 && them.score > m->other && g.state == ST_IN) m->flashAt = millis() | 1;
    m->other = them.score;
  }
}

// sets the score colours for this frame; true while they're changing
static bool scoreColors(const Game& g) {
  RGB top = WHITE, bot = WHITE;
  bool moving = false;
  if (g.valid) {
    ScoreMemo* m = nullptr;
    for (auto& x : memos) if (!strcmp(x.eventId, g.eventId)) m = &x;
    if (m && m->glowAt && millis() - m->glowAt < GLOW_MS) { top = GOLD; moving = true; }
    if (m && m->flashAt && millis() - m->flashAt < FLASH_MS) {
      const Side& them = g.other();
      if (((millis() - m->flashAt) / 300) % 2 == 0) bot = ledColor(them.hasColor, them.color);
      moving = true;
    }
  }
  renderScoreColors(top, bot);
  return moving;
}
static FxSpec* fxSpec;   // the animation playing (PSRAM)
static FxPlayer player;
static uint32_t fxStart = 0;

// ------------------------------------------------------------------- wheel
// The little thumbwheel on the controller (it sits on an I2C expander, see
// sb_wheel): flick UP (K1) / DOWN (K3) = change game - Auto, your teams,
// then other live football games; it settles on your choice a moment after
// the last flick. PUSH (K2) = no-ticker mode on/off. Hold PUSH 3 s = back to
// normal (Auto, ticker on).
static bool bigMode = false;
static uint32_t popAt = 0, popFor = 0;      // a wheel message owns the screen until popAt + popFor
static bool pending = false;                // a flick not yet sent
static int pendTeam = -1;                   // ...and the stop it picked (netSetStop code)
static uint32_t pendAt = 0;
static int lastKeys = 0;
static uint32_t pushAt = 0;
static bool pushFired = false;
static char popBuf[3][24];
static ChanCard* card;   // the game card being shown (PSRAM)
static FullTicker* fullTick;   // the full ticker's games (PSRAM)
static uint32_t fullTickVersion = 0;

static void wheelBegin() {
  uint32_t t0 = millis();
  bool ok = wheelStart();
  sbLog("wheel: I2C bus has [%s] (%lu ms)", wheelBusList(), (unsigned long)(millis() - t0));
  if (ok) sbLog("wheel: expander found at 0x%02X", wheelAddress());
  else sbLog("wheel: no expander found");
}

static void popup(const Line* l, int n, uint32_t ms) {
  drawMessage(*fb, l, n);
  panelShow(*fb);
  popAt = millis();
  popFor = ms;
}

// The wheel's stops, in order: AUTO, a mode for each of your teams (stays
// on that team until changed, saved like "Always <team>"), ALL NFL, ALL
// COLLEGE, then any other live games (temporary). Codes as netSetStop.
static int buildStops(int* out, int max) {
  int n = 0;
  out[n++] = -1;
  for (int i = 0; i < settings.npicks && n < max; i++) out[n++] = STOP_TEAM + settings.picks[i];
  if (n < max) out[n++] = STOP_ALL + 1;
  if (n < max) out[n++] = STOP_ALL + 2;
  int ch[MAX_CHANNELS];
  int nc = netChannels(ch, MAX_CHANNELS);
  for (int i = 0; i < nc && n < max; i++) {
    bool mine = false;
    for (int k = 0; k < settings.npicks; k++) if (settings.picks[k] == ch[i]) mine = true;
    if (!mine) out[n++] = ch[i];
  }
  return n;
}

static void showChoice(int code, int pos, int count) {
  if (code == -1) {
    Line l[] = {{"AUTO", GOLD, true}, {"YOUR TEAMS", DATEC, false}};
    popup(l, 2, 1800);
    return;
  }
  if (code >= STOP_ALL) {
    Line l[] = {{code == STOP_ALL + 1 ? "ALL NFL" : "ALL", GOLD, true},
                {code == STOP_ALL + 1 ? "EVERY GAME" : "COLLEGE", code == STOP_ALL + 1 ? DATEC : GOLD, code != STOP_ALL + 1},
                {code == STOP_ALL + 1 ? "" : "TOP 25 + SEC", DATEC, false}};
    popup(l, code == STOP_ALL + 1 ? 2 : 3, 1800);
    return;
  }
  int team = code >= STOP_TEAM ? code - STOP_TEAM : code;
  if (!netCard(team, *card)) {
    new (card) ChanCard();
    scopy(card->home, TEAMS[team].abbr);
  }
  card->yours = code >= STOP_TEAM;
  renderCard(*fb, *card, pos, count);
  if (code >= STOP_TEAM) {
    // a team's mode: say so along the bottom
    char m[20];
    snprintf(m, sizeof(m), "%s MODE", TEAMS[team].abbr);
    for (int y = 56; y < 63; y++) for (int x = 0; x < W; x++) fb->unput(x, y);
    text(*fb, (W - tw(m, F3)) >> 1, 57, m, GOLD, F3);
  }
  panelShow(*fb);
  popAt = millis();
  popFor = 1800;
}

static void flick(int dir) {
  int stops[40];
  int n = buildStops(stops, 40);
  int at = pending ? pendTeam : netStop();
  int pos = -1;
  for (int i = 0; i < n; i++) if (stops[i] == at) pos = i;
  if (pos < 0 && at != -1 && n < 40) {   // a saved team no longer in your picks: keep it as a stop
    for (int i = n; i > 1; i--) stops[i] = stops[i - 1];
    stops[1] = at;
    n++;
    pos = 1;
  }
  if (pos < 0) pos = 0;
  pos = (pos + dir + n) % n;
  pending = true;
  pendTeam = stops[pos];
  pendAt = millis();
  showChoice(stops[pos], pos, n - 1);
}

// true while a wheel message owns the screen
static bool wheelPoll() {
  static uint32_t last = 0;
  if (millis() - last >= 25) {
    last = millis();
    int k = wheelKeys();
    if (k >= 0 && k != lastKeys) {
      delay(5);
      if (wheelKeys() == k) {
        int pressed = k & ~lastKeys, released = lastKeys & ~k;
        lastKeys = k;
        if (pressed & WK_K1) flick(+1);
        if (pressed & WK_K3) flick(-1);
        if (pressed & WK_K2) { pushAt = millis(); pushFired = false; }
        if ((released & WK_K2) && !pushFired && millis() - pushAt < 1500) {
          bigMode = !bigMode;
          sbLog("wheel: %s", bigMode ? "no-ticker mode" : "ticker back");
          bool fullGame = shown->game.valid && shown->game.sport == FOOTBALL && shown->game.state == ST_IN;
          Line l[] = {{bigMode ? (fullGame ? "FULL" : "BIG") : "TICKER", GOLD, true},
                      {bigMode ? (fullGame ? "GAME" : "NO TICKER") : "BACK ON", DATEC, false}};
          popup(l, 2, 1000);
        }
      }
    }
    // held in: back to normal
    if ((lastKeys & WK_K2) && !pushFired && millis() - pushAt >= 3000) {
      pushFired = true;
      bigMode = false;
      pending = false;
      netSetStop(-1);
      Line l[] = {{"AUTO", GOLD, true}, {"TICKER ON", DATEC, false}};
      popup(l, 2, 1500);
    }
  }
  // a moment after the last flick, go to that game
  if (pending && millis() - pendAt >= 1200) {
    netSetStop(pendTeam);
    pending = false;
  }
  if (popAt && millis() - popAt < popFor) return true;
  if (popAt) { popAt = 0; dirty = true; }
  return false;
}

static void setMode(Mode m) { mode = m; modeAt = millis(); dirty = true; }

static const RGB CYAN = 0x00C8FF;

static void setupScreen() {
  Line l[] = {{"SETUP", GOLD, true}, {"ON YOUR PHONE", DATEC, false}, {"JOIN WIFI", DATEC, false},
              {apName.c_str(), CYAN, false}, {"THEN GO TO", DATEC, false}, {"192.168.4.1", CYAN, false}};
  showMessage(l, 6);
}

static void fallbackScreen() {
  String s = settings.ssid.substring(0, 16);
  s.toUpperCase();
  Line l[] = {{"CAN'T JOIN", RED, false}, {s.c_str(), WHITE, false}, {"ON YOUR PHONE", DATEC, false},
              {"JOIN WIFI", DATEC, false}, {apName.c_str(), CYAN, false}, {"THEN GO TO", DATEC, false},
              {"192.168.4.1", CYAN, false}};
  showMessage(l, 7);
}

static void connectingScreen() {
  String s = settings.ssid.substring(0, 16);
  s.toUpperCase();
  static const char* dots[] = {"", ".", "..", "..."};
  String c = String("JOINING") + dots[(millis() / 500) % 4];
  Line l[] = {{"WIFI", WHITE, true}, {c.c_str(), DATEC, false}, {s.c_str(), CYAN, false}};
  showMessage(l, 3);
}

static void startStation() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname("scoreboard");
  WiFi.begin(settings.ssid.c_str(), settings.pass.c_str());
  lastTry = millis();
  setMode(M_CONNECTING);
}

static void onConnected() {
  sbLog("Wi-Fi up on %s: %s", settings.ssid.c_str(), WiFi.localIP().toString().c_str());
  if (portalAPRunning()) portalStopAP();
  WiFi.setAutoReconnect(true);
  configTzTime(TZS[settings.tz].posix, "pool.ntp.org", "time.nist.gov", "time.google.com");
  portalStartHome();
  if (!netStarted) { netStart(); netStarted = true; }
  setMode(M_CONNECTED_MSG);
}

// What to draw when there's no game to draw
static void statusScreen(NetStatus st) {
  switch (st) {
    case NS_NOGAME: {
      if (!settings.npicks) {
        String ip = WiFi.localIP().toString();
        Line l[] = {{"NO TEAMS", WHITE, true}, {"PICK THEM AT", DATEC, false}, {"SCOREBOARD.LOCAL", CYAN, false},
                    {"OR", DATEC, false}, {ip.c_str(), CYAN, false}};
        drawMessage(*fb, l, 5);
      } else {
        int stop = netStop();
        if (stop >= STOP_TEAM && stop < STOP_ALL) {   // a team's mode, and that team has no game
          Line l[] = {{TEAMS[stop - STOP_TEAM].abbr, GOLD, true}, {"NO GAME", WHITE, false}, {"THIS WEEK", DATEC, false}};
          drawMessage(*fb, l, 3);
        } else {
          Line l[] = {{"NO GAMES", WHITE, true}, {"COMING UP", DATEC, false}};
          drawMessage(*fb, l, 2);
        }
      }
      break;
    }
    case NS_OFFLINE: {
      Line l[] = {{"NO SCORES", WHITE, true}, {"CAN'T REACH ESPN", DATEC, false},
                  {WiFi.status() == WL_CONNECTED ? "TRYING AGAIN" : "WIFI IS DOWN", RED, false}};
      drawMessage(*fb, l, 3);
      break;
    }
    default: {
      static const char* dots[] = {"", ".", "..", "..."};
      String c = String("SCORES") + dots[(millis() / 500) % 4];
      Line l[] = {{"LOADING", WHITE, true}, {c.c_str(), DATEC, false}};
      drawMessage(*fb, l, 2);
    }
  }
}

// Hold BOOT for 5 seconds: forget the Wi-Fi and go back to setup.
// true while it's being held (the countdown owns the screen)
static bool checkBootButton() {
  static uint32_t downAt = 0;
  if (digitalRead(BOOT_PIN) == LOW) {
    if (!downAt) downAt = millis();
    uint32_t held = millis() - downAt;
    if (held > 800) {
      int left = 5 - (int)(held / 1000);
      if (left <= 0) {
        Line l[] = {{"WIFI", WHITE, true}, {"FORGOTTEN", GOLD, true}, {"RESTARTING", DATEC, false}};
        showMessage(l, 3);
        settings.forgetWifi();
        delay(1500);
        ESP.restart();
      }
      String n = String(left);
      Line l[] = {{"KEEP HOLDING", WHITE, false}, {"TO RESET WIFI", WHITE, false}, {n.c_str(), GOLD, true}};
      showMessage(l, 3);
      return true;
    }
    return false;
  }
  if (downAt) { downAt = 0; dirty = true; }   // let go early: put the screen back
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  sbLog("Scoreboard " FW_VERSION " starting");
  pinMode(BOOT_PIN, INPUT_PULLUP);
  settings.load();
  setenv("TZ", TZS[settings.tz].posix, 1);
  tzset();
  if (!panelBegin(BRIGHTS[settings.bright].level, settings.clockA)) sbLog("panel failed to start");
  wheelBegin();
  shown = new (sbAlloc(sizeof(Shown))) Shown();
  fb = new (sbAlloc(sizeof(Frame))) Frame();
  fxSpec = new (sbAlloc(sizeof(FxSpec))) FxSpec();
  card = new (sbAlloc(sizeof(ChanCard))) ChanCard();
  fullTick = new (sbAlloc(sizeof(FullTicker))) FullTicker();

  uint8_t mac[6];
  WiFi.macAddress(mac);
  char nm[24];
  snprintf(nm, sizeof(nm), "Scoreboard-%02X%02X", mac[4], mac[5]);
  apName = nm;

  // start-up screen with a frame round the very edge: both sides should
  // show (a missing left or right edge = panel timing is off)
  {
    Line l[] = {{"SCOREBOARD", WHITE, true}, {"V" FW_VERSION, DATEC, false}};
    drawMessage(*fb, l, 2);
    for (int i = 0; i < 64; i++) {
      fb->put(i, 0, LINE); fb->put(i, 63, LINE); fb->put(0, i, LINE); fb->put(63, i, LINE);
    }
    panelShow(*fb);
    delay(2000);
  }

  // just updated over the air: say so
  {
    Preferences p;
    p.begin("sb", false);
    String v = p.getString("updated", "");
    if (v.length()) {
      p.remove("updated");
      if (v == FW_VERSION) {
        p.remove("bad");
        String l2 = "TO V" + v;
        Line l[] = {{"UPDATED", GREEN, true}, {l2.c_str(), WHITE, false}};
        showMessage(l, 2);
        sbLog("updated to %s", v.c_str());
      } else {
        // the new version didn't make it and the chip went back to this one:
        // remember, so it isn't installed again every night
        p.putString("bad", v);
        String l2 = "V" + v + " DIDN'T WORK";
        String l3 = "BACK ON V" FW_VERSION;
        Line l[] = {{"UPDATE", RED, true}, {l2.c_str(), WHITE, false}, {l3.c_str(), DATEC, false}};
        showMessage(l, 3);
        sbLog("update to %s failed, back on %s", v.c_str(), FW_VERSION);
      }
      delay(3000);
    }
    p.end();
  }

  if (!settings.hasWifi()) {
    portalStartAP(apName);
    setMode(M_SETUP);
  } else {
    startStation();
  }
}

void loop() {
  portalLoop();
  if (netTakeSave()) settings.save();   // a team mode picked on the wheel
  if (checkBootButton()) { delay(20); return; }
  if (mode == M_RUNNING && otaPercent < 0 && wheelPoll()) { delay(10); return; }

  // the setup page saved new Wi-Fi details: restart and use them
  if (portalWifiSaved && millis() - portalSavedAt > 2500) {
    Line l[] = {{"SAVED", GREEN, true}, {"RESTARTING", DATEC, false}};
    showMessage(l, 2);
    delay(800);
    ESP.restart();
  }

  switch (mode) {
    case M_SETUP:
      if (dirty) { setupScreen(); dirty = false; }
      break;

    case M_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) { onConnected(); break; }
      connectingScreen();
      if (millis() - modeAt > 45000) {   // not happening: offer the setup page, keep trying
        WiFi.setAutoReconnect(false);
        portalStartAP(apName);
        setMode(M_FALLBACK);
      }
      break;

    case M_FALLBACK:
      if (WiFi.status() == WL_CONNECTED) { onConnected(); break; }
      if (dirty) { fallbackScreen(); dirty = false; }
      // retry now and then (a router that was still booting), but not while
      // someone's using the setup page - retrying hops the radio's channel
      if (millis() - lastTry > 120000 && WiFi.softAPgetStationNum() == 0 &&
          (!portalUsedAt || millis() - portalUsedAt > 180000)) {
        sbLog("retrying %s", settings.ssid.c_str());
        WiFi.begin(settings.ssid.c_str(), settings.pass.c_str());
        lastTry = millis();
      }
      break;

    case M_CONNECTED_MSG: {
      if (dirty) {
        String ip = WiFi.localIP().toString();
        Line l[] = {{"CONNECTED", GREEN, true}, {"SETTINGS:", DATEC, false}, {"SCOREBOARD.LOCAL", CYAN, false},
                    {"OR", DATEC, false}, {ip.c_str(), CYAN, false}};
        showMessage(l, 5);
        dirty = false;
      }
      if (millis() - modeAt > 8000) setMode(M_RUNNING);
      break;
    }

    case M_RUNNING: {
      // installing an update: the progress owns the screen
      static int shownPct = -2;
      int pct = otaPercent;
      if (pct >= 0) {
        player.stop();
        if (pct != shownPct) {
          String v = String("TO V") + otaNewVersion;
          Line l[] = {{"UPDATING", GOLD, true}, {v.c_str(), WHITE, false}, {"", 0, false}, {"", 0, false},
                      {"DON'T UNPLUG", DATEC, false}};
          drawMessage(*fb, l, 5);
          // progress bar where the blank lines are
          for (int x = 8; x < 56; x++)
            for (int y = 34; y < 38; y++) fb->put(x, y, x - 8 < pct * 48 / 100 ? CYAN : rgb(40, 40, 40));
          panelShow(*fb);
          shownPct = pct;
        }
        break;
      }
      if (shownPct != -2) { shownPct = -2; dirty = true; }
      // an animation (touchdown, goal, ...) owns the screen while it plays
      if (!player.active() && fxTake(*fxSpec)) {
        player.start(fxSpec, esp_random());
        fxStart = millis();
        sbLog("animation starts");
      }
      if (player.active()) {
        if (player.show(*fb, millis() - fxStart)) {
          panelShow(*fb);
          delay(4);
          return;
        }
        dirty = true;   // done: back to the scores
      }
      if (netSnapshot(*shown, shownVersion)) { dirty = true; noteScores(shown->game); }
      if (netFullTicker(*fullTick, fullTickVersion)) dirty = true;
      static uint32_t lastPage = 0;
      if (netTickerMode() && millis() / 5000 != lastPage) { lastPage = millis() / 5000; dirty = true; }
      // the full-game screen scrolls its last play too
      static uint32_t lastFullScroll = 0;
      if (bigMode && shown->game.sport == FOOTBALL && shown->game.state == ST_IN && shown->game.lastPlay[0] &&
          millis() - lastFullScroll >= 40) { lastFullScroll = millis(); dirty = true; }
      if (millis() - lastPairAt >= 4000) { lastPairAt = millis(); pairN++; dirty = true; }
      static uint32_t downSince = 0;
      if (WiFi.status() == WL_CONNECTED) downSince = 0;
      else if (!downSince) { downSince = millis(); sbLog("Wi-Fi dropped"); }
      bool wifiDown = downSince && millis() - downSince > 120000;
      static uint32_t lastKick = 0;
      if (downSince && millis() - downSince > 30000 && millis() - lastKick > 45000) {
        lastKick = millis();
        sbLog("Wi-Fi still down, reconnecting");
        WiFi.disconnect();
        WiFi.begin(settings.ssid.c_str(), settings.pass.c_str());
      }
      bool animatedStatus = !shown->game.valid && shown->status == NS_LOADING;
      // a flashing score needs redrawing; a glow ending needs one more
      static bool wasMoving = false;
      static uint32_t lastFlash = 0;
      bool moving = scoreColors(shown->game);
      if ((moving && millis() - lastFlash >= 150) || (wasMoving && !moving)) { lastFlash = millis(); dirty = true; }
      wasMoving = moving;
      // no-ticker mode scrolls the last play: redraw about 25 times a second
      static uint32_t lastScroll = 0;
      if (bigMode && !wifiDown && renderBigScrolls(shown->game) && millis() - lastScroll >= 40) {
        lastScroll = millis();
        dirty = true;
      }
      if (dirty || animatedStatus) {
        if (wifiDown) statusScreen(NS_OFFLINE);
        else if (netTickerMode()) {
          int tm = netTickerMode();
          if (fullTick->mode == tm)
            renderFullTicker(*fb, fullTick->g, fullTick->n, (int)(millis() / 5000), tm == 1 ? "ALL NFL" : "COLLEGE");
          else {
            Line l[] = {{tm == 1 ? "ALL NFL" : "COLLEGE", GOLD, true}, {"LOADING", DATEC, false}};
            drawMessage(*fb, l, 2);
          }
        } else if (shown->game.valid && bigMode && shown->game.sport == FOOTBALL && shown->game.state == ST_IN)
          renderFootballFull(*fb, shown->game, &shown->away16, &shown->home16, millis());
        else if (shown->game.valid)
          renderGame(*fb, shown->game, pairN, &shown->away, &shown->home, bigMode, millis());
        else statusScreen(shown->status);
        panelShow(*fb);
        dirty = false;
      }
      break;
    }
  }
  delay(20);
}
