// What the board remembers between power-ups (ESP32 "Preferences" flash):
// Wi-Fi, the teams, time zone, brightness, and Auto or one team.
#pragma once
#include <Arduino.h>
#include "sb_teams.h"

static const int MAX_PICKS = 8;

struct TzDef { const char* label; const char* posix; };
static const TzDef TZS[] = {
  {"Eastern", "EST5EDT,M3.2.0,M11.1.0"},
  {"Central", "CST6CDT,M3.2.0,M11.1.0"},
  {"Mountain", "MST7MDT,M3.2.0,M11.1.0"},
  {"Arizona", "MST7"},
  {"Pacific", "PST8PDT,M3.2.0,M11.1.0"},
};
static const int NTZ = sizeof(TZS) / sizeof(TZS[0]);

struct BrightDef { const char* label; uint8_t level; };
static const BrightDef BRIGHTS[] = {{"Low", 35}, {"Medium", 70}, {"High", 120}, {"Max", 180}};
static const int NBRIGHT = sizeof(BRIGHTS) / sizeof(BRIGHTS[0]);

// Two of your teams live at once (same sport): seconds each gets on top
static const int ROTATE_SECS[] = {30, 60, 120};
static const char* const ROTATE_LABELS[] = {"30 seconds", "1 minute", "2 minutes"};
static const int NROTATE = 3;

struct Settings {
  String ssid, pass;
  int picks[MAX_PICKS];   // indexes into TEAMS, in priority order
  int npicks = 0;
  int pin = -1;           // -1 = Auto, else an index into TEAMS
  int tz = 0;
  int bright = 1;
  int rot = 0;             // index into ROTATE_SECS
  bool clockA = true;    // panel timing: true = falling edge (this panel), false = the library default

  void load();
  void save();
  void forgetWifi();
  bool hasWifi() const { return ssid.length() > 0; }
  String picksString() const;          // "NFL:NYG,CFB:FLA"
  void setPicksFromString(const String& s);
  void sortPicks();                    // football, baseball, hockey, basketball
};

int findTeam(const char* key);          // "NFL:NYG" -> index, or -1
String teamKey(int idx);
extern Settings settings;
