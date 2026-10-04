// The game as the board draws it - the same fields the browser preview's
// "game" object carries (scoreboard_sim.parse_live), as fixed-size C structs
// so nothing allocates while the panel is running.
#pragma once
#include <stdint.h>
#include <string.h>

enum Sport : uint8_t { FOOTBALL = 0, BASEBALL, HOCKEY, BASKETBALL };
enum GState : uint8_t { ST_PRE = 0, ST_IN, ST_POST };

// Fixed-size copy that always terminates.
template <size_t N>
inline void scopy(char (&dst)[N], const char* src) {
  if (!src) { dst[0] = 0; return; }
  strncpy(dst, src, N - 1);
  dst[N - 1] = 0;
}

struct Side {
  char abbr[8] = "";
  bool hasScore = false;
  int score = 0;
  bool hasColor = false;     // ESPN sent a usable hex colour
  uint32_t color = 0;        // 0xRRGGBB
  bool hasColor2 = false;
  uint32_t color2 = 0;
  char logo[128] = "";       // ESPN logo URL
  char record[12] = "";      // "10-2", or "" if none
  int rank = 0;              // 1-25, 0 = unranked
  char id[12] = "";          // ESPN team id
};

struct Playoff {
  bool on = false;
  char round[16] = "";
  char summary[16] = "";
  bool hasWins = false;
  int winsHome = 0, winsAway = 0;
};

struct Tick {
  char home[8] = "", away[8] = "";
  char homeId[8] = "", awayId[8] = "";   // ESPN team ids (college logos are named by id)
  bool hasHomeColor = false, hasAwayColor = false;
  uint32_t homeColor = 0, awayColor = 0;
  int homeRank = 0, awayRank = 0;
  char status[8] = "";       // "F", "3RD", "T5", "SCHED"
  char clock[10] = "";
  char score[12] = "";       // "21-17", "" = not started
  char kickDate[8] = "", kickTime[8] = "";
  bool redzone = false;
  int8_t possession = 0;     // 0 none, 1 home, 2 away
};

static const int MAX_TICK = 40;

struct Game {
  bool valid = false;
  Sport sport = FOOTBALL;
  GState state = ST_PRE;
  Side home, away;
  bool pinnedHome = true;
  char kickoff[20] = "";     // "10/4 3:30P"
  char clock[10] = "";
  int period = 0;
  char periodLabel[6] = "";
  bool postseason = false;
  Playoff po;
  bool preseason = false;
  char gameDate[8] = "";
  char finalLabel[14] = "";
  char eventDate[24] = "";   // ESPN's ISO time, for sorting
  char eventId[16] = "";
  // football
  char downDistance[32] = "";
  bool redzone = false;
  int8_t possession = 0;     // 0 none, 1 home, 2 away
  char lastPlay[200] = "";
  // football, live: for the full-game screen
  int8_t down = 0;           // 1-4, 0 = unknown
  int distance = -1;
  int yardLine = -1;         // 0 = home goal line ... 100 = away goal line
  int8_t toHome = -1, toAway = -1;   // timeouts left
  int8_t winHome = -1;       // home win chance %, -1 unknown
  int8_t playScore = 0;      // points the last play scored
  char shortDD[16] = "";     // "1st & 10"
  // baseball
  char half[4] = "";         // TOP BOT MID END
  char inningText[10] = "";
  char inningShort[6] = "";
  int balls = -1, strikes = -1, outs = -1;
  bool bases[3] = {false, false, false};
  // hockey
  bool intermission = false;
  char intermissionLeft[8] = "";
  char intLines[2][14] = {"", ""};
  bool pp = false, ppHome = false;
  char ppTime[8] = "";
  // basketball
  bool hasBonus = false, bonusHome = false, bonusAway = false;
  bool ranked = false;       // college: ticker shows ranks
  Tick ticker[MAX_TICK];
  int nticker = 0;

  Side& pinned() { return pinnedHome ? home : away; }
  Side& other() { return pinnedHome ? away : home; }
  const Side& pinned() const { return pinnedHome ? home : away; }
  const Side& other() const { return pinnedHome ? away : home; }
};
