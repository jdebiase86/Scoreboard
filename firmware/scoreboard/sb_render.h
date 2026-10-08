// The scoreboard screen itself - a line-for-line port of the browser
// preview's renderPixels() / drawMain() / tickerBlock() (scoreboard_live.py).
#pragma once
#include "sb_gfx.h"
#include "sb_types.h"
#include "sb_logo.h"

static const int MATCHUP_W = 26, MATCHUP_H = 24;

// logoAway / logoHome: pregame matchup logos (26x24), or null if not loaded.
// pair: advances every 4 s - pages the ticker and the status-line messages.
// big: the full screen instead (renderFull)
void renderGame(Frame& fb, const Game& g, int pair, const Logo* logoAway, const Logo* logoHome,
                bool big = false, uint32_t ms = 0);
// Colours for the two scores on the next renderGame (your team's on top):
// gold after your team scores, their colour flashing after they score
void renderScoreColors(RGB top, RGB bot);

// The wheel's pop-up for one game: both logos like the pregame screen (or
// the names if a logo isn't loaded), the score or start time under them,
// the clock / FINAL on top, and "3 OF 9" at the bottom.
struct ChanCard {
  int team = -1;              // TEAMS index (-1 = Auto)
  bool yours = false;
  char away[8] = "", home[8] = "";
  bool hasAwayColor = false, hasHomeColor = false;
  uint32_t awayColor = 0, homeColor = 0;
  bool hasScore = false;
  int awayScore = 0, homeScore = 0;
  char top[16] = "";          // "3RD 8:42", "FINAL", "SUN 1:00P"
  Logo la, lh;                // point into the arrays below (n = 0: not loaded)
  LogoPix laPix[MATCHUP_W * MATCHUP_H], lhPix[MATCHUP_W * MATCHUP_H];
};
void renderCard(Frame& fb, const ChanCard& c, int pos, int count);

// Full ticker (wheel: ALL NFL / ALL COLLEGE): two games a page, both teams'
// 24-dot logos (or their letters), each score under its logo, the clock /
// FINAL / start time between them. games: array of n; page counts up every
// few seconds.
static const int SMALL_LOGO = 24;   // logos on the full ticker
struct FullGame {
  Tick t;
  Logo la, lh;
  LogoPix pa[SMALL_LOGO * SMALL_LOGO], ph[SMALL_LOGO * SMALL_LOGO];
};
void renderFullTicker(Frame& fb, const FullGame* games, int n, int page, const char* title);

// The full screens (the wheel's push, and Auto): the period and clock on
// top, both logos big with the scores under them, and a strip for the sport
// along the bottom; finals and upcoming games their own layout (see
// sb_render.cpp). la/lh: away/home 26x24 logos (may be empty: letters).
void renderFull(Frame& fb, const Game& g, const Logo* la, const Logo* lh, uint32_t ms);
// The close-game look (gold clock, CLOSE GAME) on the next renderFull
// (closeGame() in sb_events decides; sb_render doesn't know the rules)
void renderCloseGameFlag(bool on);
