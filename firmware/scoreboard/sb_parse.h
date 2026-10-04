// ESPN JSON -> Game. A port of scoreboard_sim.parse_live and friends.
// Works on an already-parsed (filtered) ArduinoJson document, so it can be
// tested on a computer against saved ESPN data.
#pragma once
#include <ArduinoJson.h>
#include <time.h>
#include "sb_types.h"
#include "sb_teams.h"

// Only the fields the board uses - keeps a whole Saturday's college feed
// down to a few KB of memory.
void buildScoreboardFilter(JsonDocument& f);
void buildNhlFilter(JsonDocument& f);
void buildNbaFilter(JsonDocument& f);

// "Today" for the board: local date, rolling over at 5am, as YYYYMMDD
void boardDate(time_t now, char (&out)[9]);

// The event for this team in a scoreboard feed, or a null object.
JsonObjectConst findEvent(JsonObjectConst feed, const TeamDef& t);
GState eventState(JsonObjectConst ev);

// Fills g from the feed. false if the team isn't in it.
bool parseGame(JsonObjectConst feed, const TeamDef& t, bool top25, time_t now, Game& g);

// Rebuilds g's ticker from a feed (college: all ranked games, not just the
// conference feed the main game came from)
void buildTicker(JsonObjectConst feed, const char* excludeId, Sport sport, bool top25, time_t now, Game& g);

// A whole football feed at a glance: is anything live, when's the next
// kickoff, and every game as a ticker (college: ranked games only)
void footballSummary(JsonObjectConst feed, bool college, time_t now, bool& live, time_t& nextKick, Game& tick);
// Put football tickers a and b (either may be null) under the game on top,
// leaving out that game itself
void mergeTicker(Game& g, const Game* a, const Game* b, bool ranked);

// Extras from the leagues' own feeds (power play / intermission, bonus)
void enrichHockey(JsonObjectConst nhl, Game& g);
void enrichBasketball(JsonObjectConst nba, Game& g);

// Software versions: "1.10" is newer than "1.9". <0, 0, >0 like strcmp.
int versionCompare(const char* a, const char* b);
// "scoreboard-1.4.bin" -> "1.4"; false if the name isn't an update file
bool versionFromAsset(const char* name, char (&out)[16]);

// exposed for tests
std::string shortRound(const char* txt);
std::string shortSummary(const char* txt, int total);
time_t parseIso(const char* iso);
