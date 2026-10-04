// What just happened in a game, worked out by comparing two looks at it -
// the same rules the browser preview uses (scoreboard_live.py, load()):
//   football   6+ points = touchdown, exactly 3 = field goal, the game
//              starting = kickoff, a quarter ending = quarter card /
//              halftime, a new penalty in the last play = flag, your team
//              moving the chains = first down
//   hockey     a period ending = intermission card, any goal = goal light
//   baseball   a run: home run (or grand slam) if the play says so,
//              otherwise the run notice
//   basketball a quarter ending = quarter card, a three = 3-pointer
#pragma once
#include "sb_types.h"
#include "sb_fx.h"

// Fills spec (all but the logo) and returns its kind, or FX_NONE
FxKind detectEvent(const Game& prev, const Game& now, FxSpec& spec);
// Builds a test animation from a settings-page button name ("touchdown",
// "halftime", "grandslam", ...) using a game (or made-up teams). false = unknown
bool testEvent(const char* name, const Game* g, FxSpec& spec);
// Your team's game just ended and they won: the win card (all but the
// team's name, which the caller adds - see winWords)
bool detectWin(const Game& prev, const Game& now, FxSpec& spec);
// "New York Giants" -> "GIANTS WIN", "Florida" -> "FLORIDA WINS"
void winWords(const char* teamName, bool college, char (&out)[20]);
// Does this animation show the team's logo?
bool fxUsesLogo(FxKind k);
