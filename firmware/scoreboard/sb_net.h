// Everything that talks to the internet: ESPN scoreboards, the NHL and NBA
// live feeds, team logos. Runs as its own background task so a slow
// download never freezes the panel.
#pragma once
#include <Arduino.h>
#include "sb_types.h"
#include "sb_logo.h"
#include "sb_render.h"
#include "sb_fx.h"

enum NetStatus : uint8_t { NS_LOADING = 0, NS_OK, NS_NOGAME, NS_OFFLINE };

struct Shown {
  Game game;
  int team = -1;                  // TEAMS index of the game on the board
  NetStatus status = NS_LOADING;
  Logo away, home;                // 26x24 logos (pregame, live football); point into the arrays below
  LogoPix awayPix[MATCHUP_W * MATCHUP_H];
  LogoPix homePix[MATCHUP_W * MATCHUP_H];
};

// The full ticker (wheel: ALL NFL / ALL COLLEGE): every game, live first
struct FullTicker {
  int mode = 0;                   // 0 off, 1 NFL, 2 college
  int n = 0;
  FullGame g[MAX_TICK];
};
// The wheel changed a saved setting: save it (call from loop)
bool netTakeSave();
bool netFullTicker(FullTicker& out, uint32_t& version);

void netStart();
// Copies the latest result into out if it changed since `version`.
bool netSnapshot(Shown& out, uint32_t& version);
// Settings changed: forget cached lookups and refresh straight away.
void netKick();

// Over-the-air updates from GitHub. The board checks a few minutes after it
// starts, every night around 4am, and when the settings page asks.
extern volatile int otaPercent;       // -1 = not updating, else 0-100
extern char otaNewVersion[16];        // what it's installing
void otaRequest();                    // "Check for updates now"
String otaStatus();                   // one line for the settings page

// Animations: the net task spots touchdowns, goals, ... and queues them up
// (logo already loaded); the panel loop takes them one at a time.
bool fxTake(FxSpec& out);
// The settings page's test buttons ("touchdown", "halftime", ...)
bool fxTest(const char* name);

// The wheel: put one game on the board (TEAMS index) or -1 = back to Auto.
// Takes effect within a moment; the board drops back to Auto by itself when
// that game ends.
static const int MAX_CHANNELS = 24;
static const int STOP_TEAM = 10000, STOP_ALL = 20000;
void netSetStop(int code);             // see sb_net.cpp: -1 Auto, team, STOP_TEAM+i, STOP_ALL+1/2
int netStop();                         // where the wheel is now
int netTickerMode();                   // 0 off, 1 all NFL, 2 all college
// What the wheel can flip through (TEAMS indexes): your teams, then other
// live football games. Returns how many.
int netChannels(int* out, int max);
// That game's card for the wheel's pop-up (logos as loaded so far)
bool netCard(int team, ChanCard& out);
