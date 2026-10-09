#include "sb_render.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <initializer_list>

// Row positions (LAY in the preview)
static const int L_STATUS = 1, L_ROW1 = 7, L_ROW2 = 22, L_DIVIDER = 37,
                 L_TICKA = 39, L_TICKDIV = 51, L_TICKB = 53;

static const char* const BALL[] = {"0011100", "0111110", "1112111", "0111110", "0011100"};
static const char* const ARROW_UP[] = {"00100", "01110", "11111"};
static const char* const ARROW_DN[] = {"11111", "01110", "00100"};
// 9x9 baseball for the team at bat
static const char* const AT_BAT[] = {"000333000", "003111300", "042111240", "312111213", "342111243",
                                     "312111213", "042111240", "003111300", "000333000"};

static int ROW_INSET = 0;   // 1 while the playoff / preseason frame is up
static void scoreText(Frame& fb, int x, int y, const char* s, RGB c, Font f, int sc);
static RGB scoreTop = WHITE, scoreBot = WHITE;   // score glow / flash (renderScoreColors)
static bool renderCloseGame = false, closeGameNow = false;   // renderCloseGameFlag
void renderCloseGameFlag(bool on) { renderCloseGame = on; }
void renderScoreColors(RGB top, RGB bot) { scoreTop = top; scoreBot = bot; }

static void fullAbbr(char (&out)[5], const char* a) {
  strncpy(out, a ? a : "", 4);
  out[4] = 0;
}

static void spriteAtBat(Frame& fb, int x, int y) {
  for (int ry = 0; ry < 9; ry++)
    for (int rx = 0; rx < 9; rx++) {
      char k = AT_BAT[ry][rx];
      RGB c;
      switch (k) {
        case '1': c = rgb(250, 250, 245); break;
        case '2': c = rgb(220, 30, 30); break;
        case '3': c = rgb(150, 150, 150); break;
        case '4': c = rgb(150, 20, 20); break;
        default: continue;
      }
      fb.put(x + rx, y + ry, c);
    }
}

// hasBall: 0 none, 1 football possession, 2 baseball at bat
static void teamRow(Frame& fb, int y, const char* abbrIn, bool hasScore, int score, RGB color,
                    int hasBall, const char* record, int rank, bool condensed, bool big = false,
                    RGB scoreC = WHITE) {
  char abbr[5];
  fullAbbr(abbr, abbrIn);
  const int rowH = big ? 21 : 14;
  Font af = condensed ? F3 : F5;
  int afh = condensed ? 10 : 14;
  int ax = 1 + ROW_INSET, aw = tw(abbr, af, 2), RX = W - 1 - ROW_INSET;
  text(fb, ax, y + ((rowH - afh) >> 1), abbr, color, af, 2);

  bool pre = !hasScore;
  char txt[16];
  Font f;
  int sc, h;
  if (pre) {
    scopy(txt, record ? record : "");
    f = F5; sc = 1; h = 7;
  } else {
    snprintf(txt, sizeof(txt), "%d", score);
    f = F5; sc = 2; h = 14;
    // no-ticker mode: the score a size bigger when it fits beside the name
    // (and the ball, so it never covers the letters)
    if (big && ax + aw + 12 + tw(txt, F5, 3) <= RX) { sc = 3; h = 21; }
    else if (ax + aw + 2 + tw(txt, F5, 2) > RX) { f = F3; h = 10; }
  }
  char rs[6] = "";
  if (rank) snprintf(rs, sizeof(rs), "%d", rank);
  int rx = ax + aw + 2, rEnd = rs[0] ? rx + tw(rs, F3) + 2 : rx;
  if (pre && txt[0] && RX - tw(txt, f, sc) < rEnd) { f = F3; h = 5; }
  int sw = tw(txt, f, sc);
  int sx = RX - sw;
  if (txt[0]) {
    if (pre) text(fb, sx, y + ((rowH - h) >> 1), txt, WHITE, f, sc);
    else scoreText(fb, sx, y + ((rowH - h) >> 1), txt, scoreC, f, sc);
  }
  if (rs[0]) text(fb, rx, y + rowH - 5, rs, RANKC, F3);
  if (hasBall == 2) spriteAtBat(fb, sx - 12, y + ((rowH - 9) >> 1));
  else if (hasBall == 1) sprite(fb, sx - 9, y + ((rowH - 5) >> 1), BALL, 5, BROWN);
}

// a solid status chip (PP / PK / BONUS), letters cut out of it
static void chip(Frame& fb, int xr, const char* label, RGB c) {
  int w = tw(label, F3) + 2, xl = xr - w + 1;
  for (int y = L_STATUS - 1; y <= L_STATUS + 5; y++)
    for (int x = xl; x <= xr; x++) fb.put(x, y, c);
  text(fb, xl + 1, L_STATUS, label, BLACK, F3);
}

// The infield as a thin green outline (Joe's pick B, 1.11) with the bases on
// it: a runner = solid gold base, white centre; empty = light grey; home
// plate white. (Same as the preview's drawBaseDiamond.)
static void drawBaseDiamond(Frame& fb, int x, int y, const bool* bases) {
  int cx = x + 5, cy = y + 3;
  for (int dy = -4; dy <= 4; dy++)
    for (int dx = -6; dx <= 6; dx++) {
      int d = abs(dx) * 2 + abs(dy) * 3;
      if (d >= 10 && d <= 12) fb.put(cx + dx, cy + dy, rgb(40, 140, 60));
    }
  auto base = [&](int bx, int by, bool on) {
    RGB c = on ? rgb(255, 200, 0) : rgb(170, 170, 170);
    fb.put(bx, by - 1, c); fb.put(bx - 1, by, c); fb.put(bx + 1, by, c); fb.put(bx, by + 1, c);
    fb.put(bx, by, on ? WHITE : c);
  };
  base(cx, cy - 3, bases[1]);       // 2nd
  base(cx - 5, cy, bases[2]);       // 3rd
  base(cx + 5, cy, bases[0]);       // 1st
  fb.put(cx, cy + 4, WHITE);        // home plate
}

static int drawInning(Frame& fb, int x, int y, bool top, const char* num, RGB c) {
  sprite(fb, x, y + 1, top ? ARROW_UP : ARROW_DN, 3, c);
  text(fb, x + 6, y, num, c, F3);
  return x + 6 + tw(num, F3);
}

// "1st & Goal at MIA 4" -> "1ST&G", trimmed to fit beside the clock
static void shortDown(char (&out)[32], const char* dd, int leftW) {
  out[0] = 0;
  if (!dd || !*dd) return;
  const char* at = strstr(dd, " at ");
  size_t n = at ? (size_t)(at - dd) : strlen(dd);
  char tmp[64]; size_t k = 0;
  for (size_t i = 0; i < n && k < sizeof(tmp) - 1; i++)
    if (dd[i] != ' ') tmp[k++] = (char)toupper((unsigned char)dd[i]);
  tmp[k] = 0;
  char* g = strstr(tmp, "GOAL");
  if (g) memmove(g + 1, g + 4, strlen(g + 4) + 1);   // GOAL -> G (first only)
  scopy(out, tmp);
  // keep a clear gap after the clock: "1ST&10", then "1&10", else nothing
  auto fits = [&](const char* t) { return leftW + tw(t, F3) + 2 + 4 + 2 <= W; };
  if (fits(out)) return;
  char compact[32]; size_t k2 = 0;
  for (size_t i = 0; out[i] && k2 < sizeof(compact) - 1; i++) {
    if (isdigit((unsigned char)out[i]) && isalpha((unsigned char)out[i + 1]) && isalpha((unsigned char)out[i + 2])) {
      compact[k2++] = out[i]; i += 2; continue;            // 1ST -> 1
    }
    compact[k2++] = out[i];
  }
  compact[k2] = 0;
  if (fits(compact)) { scopy(out, compact); return; }
  out[0] = 0;
}

// Scores: the font's own digits (Joe preferred the plain 1 to the chunkier
// stadium-style one that 1.7 had).
static void scoreText(Frame& fb, int x, int y, const char* s, RGB c, Font f, int sc) {
  text(fb, x, y, s, c, f, sc);
}

// one of several messages, changing with the ticker; empties skipped
static const char* cycle(const char* const* lines, int n, int pair) {
  const char* l[6]; int m = 0;
  for (int i = 0; i < n; i++) if (lines[i] && lines[i][0]) l[m++] = lines[i];
  return m ? l[pair % m] : "";
}

static void winsText(char (&out)[12], int n) {
  snprintf(out, sizeof(out), "%d%s", n, n == 1 ? " WIN" : " WINS");
}

static void drawMatchup(Frame& fb, const Logo& la, const Logo& lh, const Side& away, const Side& home,
                        const char* recA, const char* recH) {
  drawLogo(fb, la, 14 - (la.w >> 1), 7 + ((MATCHUP_H - la.h) >> 1), 1);
  drawLogo(fb, lh, 50 - (lh.w >> 1), 7 + ((MATCHUP_H - lh.h) >> 1), 1);
  text(fb, (W - tw("AT", F3)) >> 1, 17, "AT", DATEC, F3);
  auto under = [&](const Side& s, const char* rec, int cx) {
    char rk[6] = "";
    if (s.rank) snprintf(rk, sizeof(rk), "%d", s.rank);
    const char* r = rec ? rec : "";
    int wr = tw(r, F3), wk = rk[0] ? tw(rk, F3) + 3 : 0, x0 = cx - ((wk + wr) >> 1);
    if (rk[0]) text(fb, x0, 32, rk, RANKC, F3);
    if (r[0]) text(fb, x0 + wk, 32, r, WHITE, F3);
  };
  under(away, recA, 14);
  under(home, recH, 50);
}

static void drawMain(Frame& fb, const Game& g, int pair, const Logo* la, const Logo* lh, bool big = false) {
  const Playoff& po = g.po;
  ROW_INSET = (po.on || g.preseason) ? 1 : 0;
  bool pinnedHome = g.pinnedHome;
  const Side& top = pinnedHome ? g.home : g.away;
  const Side& bot = pinnedHome ? g.away : g.home;
  int topBall = 0, botBall = 0;
  if (g.sport == FOOTBALL && g.possession) {
    bool t = (g.possession == 1) == pinnedHome;
    topBall = t ? 1 : 0;
    botBall = t ? 0 : 1;
  }
  bool atBatHalf = !strcmp(g.half, "TOP") || !strcmp(g.half, "BOT");
  if (g.sport == BASEBALL && g.state == ST_IN && atBatHalf) {
    bool homeBats = !strcmp(g.half, "BOT");
    topBall = (homeBats == pinnedHome) ? 2 : 0;
    botBall = topBall ? 0 : 2;
  }
  const bool condensed = true;

  const char* extras[3] = {po.on ? po.round : nullptr, po.on ? po.summary : nullptr,
                           g.preseason ? "PRESEASON" : nullptr};
  RGB extraColor = po.on ? PLAYOFF_GOLD : PRESEASON_SILVER;

  if (g.state == ST_POST) {
    const char* lines[3] = {g.finalLabel[0] ? g.finalLabel : "FINAL", po.on ? po.summary : nullptr,
                            g.preseason ? "PRESEASON" : nullptr};
    const char* fl = cycle(lines, 3, pair);
    text(fb, (W - tw(fl, F3)) >> 1, L_STATUS, fl, GOLD, F3);
  } else if (g.state == ST_PRE) {
    // "10/4 3:30P" -> date top-left, time top-right
    char words[4][12]; int k = 0;
    const char* p = g.kickoff;
    while (*p && k < 4) {
      while (*p == ' ') p++;
      if (!*p) break;
      int i = 0;
      while (*p && *p != ' ') { if (i < 11) words[k][i++] = *p; p++; }
      words[k][i] = 0; k++;
    }
    const char* date = k > 1 ? words[0] : "";
    const char* time = k > 1 ? words[k - 1] : (k ? words[0] : "");
    const char* msgs[4]; int m = 0;
    msgs[m++] = nullptr;   // the date/time slot
    for (auto e : extras) if (e && e[0]) msgs[m++] = e;
    const char* msg = msgs[pair % m];
    if (!msg) {
      if (date[0]) text(fb, 2, L_STATUS, date, DATEC, F3);
      text(fb, W - 2 - tw(time, F3), L_STATUS, time, CLOCK, F3);
    } else {
      text(fb, (W - tw(msg, F3)) >> 1, L_STATUS, msg, extraColor, F3);
    }
  } else if (g.sport == BASEBALL) {
    if (atBatHalf) {
      char num[8];
      const char* is = g.inningShort;
      if (is[0] >= 'A' && is[0] <= 'Z') is++;
      if (*is) scopy(num, is);
      else if (g.period) snprintf(num, sizeof(num), "%d", g.period);
      else num[0] = 0;
      drawInning(fb, 1, L_STATUS, !strcmp(g.half, "TOP"), num, CLOCK);
      if (g.balls >= 0 && g.strikes >= 0) {
        char c[24]; snprintf(c, sizeof(c), "%d-%d", g.balls, g.strikes);
        text(fb, 17, L_STATUS, c, DATEC, F3);
      }
      drawBaseDiamond(fb, 30, L_STATUS, g.bases);
      if (g.outs >= 0) {
        char o[16]; snprintf(o, sizeof(o), "%d OUT", g.outs);
        text(fb, W - 2 - tw(o, F3), L_STATUS, o, CLOCK, F3);
      }
    } else {
      text(fb, 2, L_STATUS, g.inningText[0] ? g.inningText : g.periodLabel, CLOCK, F3);
    }
  } else if (g.sport == HOCKEY && g.intermission) {
    char left[12]; snprintf(left, sizeof(left), "%s INT", g.periodLabel);
    text(fb, 2, L_STATUS, left, CLOCK, F3);
    if (g.intermissionLeft[0])
      text(fb, W - 2 - tw(g.intermissionLeft, F3), L_STATUS, g.intermissionLeft, DATEC, F3);
  } else {
    char left[20];
    snprintf(left, sizeof(left), "%s %s", g.periodLabel[0] ? g.periodLabel : "OT", g.clock);
    text(fb, 2, L_STATUS, left, CLOCK, F3);
    char r[32] = ""; RGB rc = GRAY;
    if (g.sport == FOOTBALL) {
      shortDown(r, g.downDistance, tw(left, F3));
      rc = g.redzone ? RED : GRAY;
    } else if (g.sport == HOCKEY && g.pp) {
      bool mine = g.ppHome == g.pinnedHome;
      RGB c = mine ? GREEN : RED;
      const char* tag = mine ? "PP" : "PK";
      int leftEnd = 2 + tw(left, F3);
      int timeX = W - 2 - tw(g.ppTime, F3);
      int chipW = tw(tag, F3) + 2;
      int xr = W - 3;
      if (g.ppTime[0] && (timeX - 3) - chipW + 1 >= leftEnd + 3) {
        text(fb, timeX, L_STATUS, g.ppTime, c, F3);
        xr = timeX - 3;
      }
      chip(fb, xr, tag, c);
    } else if (g.sport == BASKETBALL && g.hasBonus && (g.pinnedHome ? g.bonusHome : g.bonusAway)) {
      chip(fb, W - 3, "BONUS", GREEN);
    }
    if (r[0]) text(fb, W - 2 - tw(r, F3), L_STATUS, r, rc, F3);
  }

  // Before a playoff game, series wins replace the season records
  // (but not before game 1: 0 WINS vs 0 WINS says nothing)
  char topRecW[12], botRecW[12];
  const char* topRec = top.record;
  const char* botRec = bot.record;
  if (po.on && po.hasWins && g.state == ST_PRE && (po.winsHome + po.winsAway) > 0) {
    winsText(topRecW, pinnedHome ? po.winsHome : po.winsAway);
    winsText(botRecW, pinnedHome ? po.winsAway : po.winsHome);
    topRec = topRecW; botRec = botRecW;
  }
  if (g.state == ST_PRE && la && lh && la->n && lh->n) {
    const char* recA = pinnedHome ? botRec : topRec;
    const char* recH = pinnedHome ? topRec : botRec;
    drawMatchup(fb, *la, *lh, g.away, g.home, recA, recH);
    return;
  }
  teamRow(fb, big ? 9 : L_ROW1, top.abbr, top.hasScore, top.score, ledColor(top.hasColor, top.color), topBall,
          topRec, top.rank, condensed, big, scoreTop);
  teamRow(fb, big ? 32 : L_ROW2, bot.abbr, bot.hasScore, bot.score, ledColor(bot.hasColor, bot.color), botBall,
          botRec, bot.rank, condensed, big, scoreBot);
}

static void tickerBlock(Frame& fb, int y, const Tick& t, bool ranked) {
  char aS[8] = "", hS[8] = "";
  bool hasS = t.score[0] != 0;
  if (hasS) {
    const char* d = strchr(t.score, '-');
    if (d) {
      size_t n = d - t.score; if (n > 7) n = 7;
      memcpy(aS, t.score, n); aS[n] = 0;
      scopy(hS, d + 1);
    } else scopy(aS, t.score);
  }
  const char *rt, *rb;
  bool fin = !strcmp(t.status, "F");
  if (fin) { rt = "FINAL"; rb = ""; }
  else if (!hasS) { rt = t.kickTime; rb = t.kickDate; }
  else { rt = t.status; rb = t.clock; }

  for (int i = 0; i < 2; i++) {
    bool isHome = i == 1;
    char abbr[5]; fullAbbr(abbr, isHome ? t.home : t.away);
    const char* sc = isHome ? hS : aS;
    RGB col = isHome ? ledColor(t.hasHomeColor, t.homeColor) : ledColor(t.hasAwayColor, t.awayColor);
    int rk = isHome ? t.homeRank : t.awayRank;
    int ry = y + i * 6;
    bool ball = t.possession && ((t.possession == 1) == isHome);
    if (ranked) {
      if (rk) { char rs[6]; snprintf(rs, sizeof(rs), "%d", rk); text(fb, 8 - tw(rs, F3), ry, rs, DATEC, F3); }
      text(fb, 10, ry, abbr, col, F3);
      if (hasS) text(fb, 34 - tw(sc, F3), ry, sc, WHITE, F3);
      if (ball) sprite(fb, 35, ry, BALL, 5, BROWN);
    } else {
      text(fb, 3, ry, abbr, col, F3);
      if (hasS) text(fb, 21, ry, sc, WHITE, F3);
      if (ball) sprite(fb, 31, ry, BALL, 5, BROWN);
    }
  }
  bool sched = !hasS && !fin;
  // baseball "T5"/"B5" -> arrow + inning
  bool inn = (rt[0] == 'T' || rt[0] == 'B') && rt[1];
  for (const char* q = rt + 1; inn && *q; q++) if (*q < '0' || *q > '9') inn = false;
  if (inn) {
    const char* n = rt + 1;
    int w = 6 + tw(n, F3);
    drawInning(fb, W - 3 - w, y, rt[0] == 'T', n, DATEC);
  } else if (rt[0]) {
    text(fb, W - 3 - tw(rt, F3), y, rt, !strcmp(rt, "FINAL") ? GREEN : (sched ? CLOCK : DATEC), F3);
  }
  if (rb[0]) text(fb, W - 3 - tw(rb, F3), y + 6, rb, sched ? DATEC : CLOCK, F3);
  if (t.redzone)
    for (int yy = y; yy < y + 11; yy++) { fb.put(0, yy, RED); fb.put(W - 1, yy, RED); }
}

void renderGame(Frame& fb, const Game& g, int pair, const Logo* la, const Logo* lh, bool big, uint32_t ms) {
  fb.clear();
  if (!g.valid) return;
  if (big) { renderFull(fb, g, la, lh, ms); return; }
  drawMain(fb, g, pair, la, lh);
  RGB frame = g.po.on ? PLAYOFF_GOLD : (g.preseason ? PRESEASON_SILVER : 0);
  bool hasFrame = g.po.on || g.preseason;
  bool dotted = !g.po.on && g.preseason;
  auto on = [&](int i) { return !dotted || i % 2 == 0; };
  for (int x = 0; x < W; x++) fb.put(x, L_DIVIDER, hasFrame && on(x) ? frame : LINE);
  if (hasFrame) {
    for (int x = 0; x < W; x++) if (on(x)) fb.put(x, 0, frame);
    for (int y = 0; y < L_DIVIDER; y++) if (on(y)) { fb.put(0, y, frame); fb.put(W - 1, y, frame); }
  }
  int n = g.nticker;
  if (n) {
    int i = (pair * 2) % n;
    tickerBlock(fb, L_TICKA, g.ticker[i % n], g.ranked);
    for (int x = 2; x < 39; x++) fb.put(x, L_TICKDIV, LINE);
    tickerBlock(fb, L_TICKB, g.ticker[(i + 1) % n], g.ranked);
  }
}


void renderCard(Frame& fb, const ChanCard& c, int pos, int count) {
  fb.clear();
  if (c.top[0]) text(fb, (W - tw(c.top, F3)) >> 1, 1, c.top, strcmp(c.top, "FINAL") ? CLOCK : GOLD, F3);
  RGB ca = ledColor(c.hasAwayColor, c.awayColor), ch = ledColor(c.hasHomeColor, c.homeColor);
  if (c.la.n && c.lh.n) {
    drawLogo(fb, c.la, 14 - (c.la.w >> 1), 7 + ((MATCHUP_H - c.la.h) >> 1), 1);
    drawLogo(fb, c.lh, 50 - (c.lh.w >> 1), 7 + ((MATCHUP_H - c.lh.h) >> 1), 1);
    text(fb, (W - tw("AT", F3)) >> 1, 17, "AT", DATEC, F3);
  } else {
    // no logos (yet): the names, big, in team colours
    char a[5], h[5];
    fullAbbr(a, c.away); fullAbbr(h, c.home);
    text(fb, 14 - (tw(a, F3, 2) >> 1), 14, a, ca, F3, 2);
    text(fb, 50 - (tw(h, F3, 2) >> 1), 14, h, ch, F3, 2);
    text(fb, (W - tw("AT", F3)) >> 1, 17, "AT", DATEC, F3);
  }
  if (c.hasScore) {
    char sa[6], sh[6];
    snprintf(sa, sizeof(sa), "%d", c.awayScore);
    snprintf(sh, sizeof(sh), "%d", c.homeScore);
    Font f = F5; int sc = 2;
    if (tw(sa, F5, 2) > 28 || tw(sh, F5, 2) > 28) sc = 1;
    scoreText(fb, 14 - (tw(sa, f, sc) >> 1), 34, sa, WHITE, f, sc);
    scoreText(fb, 50 - (tw(sh, f, sc) >> 1), 34, sh, WHITE, f, sc);
  } else {
    char a[5], h[5];
    fullAbbr(a, c.away); fullAbbr(h, c.home);
    text(fb, 14 - (tw(a, F3) >> 1), 34, a, ca, F3);
    text(fb, 50 - (tw(h, F3) >> 1), 34, h, ch, F3);
  }
  char n[16];
  snprintf(n, sizeof(n), "%d OF %d", pos, count);
  text(fb, (W - tw(n, F3)) >> 1, 57, n, c.yours ? GOLD : GRAY, F3);
}

// ------------------------------------------------------------ full ticker
static void logoOrLetters(Frame& fb, const Logo& l, int x, int y, const char* abbr, bool hasC, uint32_t c) {
  if (l.n) { drawLogo(fb, l, x + ((SMALL_LOGO - l.w) >> 1), y + ((SMALL_LOGO - l.h) >> 1), 1); return; }
  char a[5]; fullAbbr(a, abbr);
  int w = tw(a, F3);
  text(fb, x + ((SMALL_LOGO - w) >> 1), y + ((SMALL_LOGO - 5) >> 1), a, ledColor(hasC, c), F3);
}

// Two games a page, 32 rows each: both logos big along the top (away left,
// home right), each score small under its logo, the clock / FINAL / start
// time in the bottom middle with the quarter (or date) just above it in dim
// grey. Between the logos: the ball on the side that has it, or AT before
// the game. Red zone: the quarter and clock go red.
void renderFullTicker(Frame& fb, const FullGame* games, int n, int page, const char* title) {
  fb.clear();
  if (n <= 0) {
    text(fb, (W - tw(title, F5)) >> 1, 20, title, GOLD, F5);
    text(fb, (W - tw("NO GAMES", F3)) >> 1, 34, "NO GAMES", DATEC, F3);
    return;
  }
  const int L = SMALL_LOGO;
  int pages = (n + 1) / 2;
  int p = page % pages;
  for (int k = 0; k < 2; k++) {
    int i = p * 2 + k;
    if (i >= n) break;
    const FullGame& g = games[i];
    const Tick& t = g.t;
    int y = k * 32;
    logoOrLetters(fb, g.la, 0, y + 1, t.away, t.hasAwayColor, t.awayColor);
    logoOrLetters(fb, g.lh, W - L, y + 1, t.home, t.hasHomeColor, t.homeColor);
    bool fin = !strcmp(t.status, "F"), sched = !t.score[0];
    const char *top = "", *bot;
    RGB tc = DATEC, bc = CLOCK;
    if (fin) { bot = "FINAL"; bc = GREEN; }
    else if (sched) { top = t.kickDate; bot = t.kickTime; }
    else { top = t.status; bot = t.clock; if (t.redzone) tc = bc = RED; }
    if (top[0]) {
      int w = tw(top, F3), x0 = (W - w) >> 1;
      // a long date ("10/12") reaches under the logos' bottom corners: a dark gap round it
      if (w > W - 2 * L - 2)
        for (int yy = y + 19; yy <= y + 24; yy++) for (int x = x0 - 1; x <= x0 + w; x++) fb.put(x, yy, BLACK);
      text(fb, x0, y + 20, top, tc, F3);
    }
    text(fb, (W - tw(bot, F3)) >> 1, y + 26, bot, bc, F3);
    if (!sched) {
      char a[8] = "", h[8] = "";
      const char* d = strchr(t.score, '-');
      if (d) { size_t m = d - t.score; if (m > 7) m = 7; memcpy(a, t.score, m); a[m] = 0; scopy(h, d + 1); }
      text(fb, L / 2 - (tw(a, F3) >> 1), y + 26, a, WHITE, F3);
      text(fb, W - L / 2 - (tw(h, F3) >> 1), y + 26, h, WHITE, F3);
      if (t.possession) {   // a small football between the logos, on the side with the ball
        int cx = t.possession == 2 ? 28 : 35;
        RGB br = rgb(200, 100, 35);
        for (int dx = -1; dx <= 1; dx++) { fb.put(cx + dx, y + 10, br); fb.put(cx + dx, y + 12, br); }
        for (int dx = -2; dx <= 2; dx++) fb.put(cx + dx, y + 11, br);
        fb.put(cx, y + 11, WHITE);
      }
    } else {
      text(fb, (W - tw("AT", F3)) >> 1, y + 9, "AT", DATEC, F3);
    }
    if (k == 0) for (int x = 2; x < W - 2; x++) fb.put(x, 31, rgb(50, 50, 50));
  }
}

// ------------------------------------------------------------- full screens
// The wheel's full screen and Auto (Mini-Scoreboard style, Joe's pick, 1.11):
// the period and clock along the top, both logos big (away left, home
// right) with the score under each, then a strip for the sport along the
// bottom - football a thin field + win chance + ball spot, baseball a field
// with the diamond and the count, hockey records + a period tracker (power
// play banner / penalty kill tape), basketball bonus + a quarter tracker. Finals
// and upcoming games get a coloured tag (FINAL grey, TODAY / TOMORROW blue).

// a logo centred in a box, or the team's letters if there's no logo
static void logoBox(Frame& fb, const Logo* l, int x, int y, int bw, int bh, const char* abbr, bool hasC, uint32_t c) {
  if (l && l->n) { drawLogo(fb, *l, x + ((bw - l->w) >> 1), y + ((bh - l->h) >> 1), 1); return; }
  char a[5]; fullAbbr(a, abbr);
  int w = tw(a, F5);
  text(fb, x + ((bw - w) >> 1), y + ((bh - 7) >> 1), a, ledColor(hasC, c), F5);
}

// a logo at half size (the power play banner)
static void logoHalf(Frame& fb, const Logo* l, int x, int y) {
  if (!l || !l->n) return;
  for (int i = 0; i < l->n; i++) {
    const LogoPix& p = l->pix[i];
    if ((p.x & 1) || (p.y & 1)) continue;
    fb.put(x + p.x / 2, y + p.y / 2, rgb(p.r, p.g, p.b));
  }
}

static int colorGap(RGB a, RGB b) {
  return abs((int)((a >> 16) & 255) - (int)((b >> 16) & 255)) + abs((int)((a >> 8) & 255) - (int)((b >> 8) & 255)) +
         abs((int)(a & 255) - (int)(b & 255));
}

static void fillRect(Frame& fb, int x0, int y0, int x1, int y1, RGB c) {
  for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) fb.put(x, y, c);
}
static RGB dimRgb(RGB c, int pct) {
  return rgb(((c >> 16) & 255) * pct / 100, ((c >> 8) & 255) * pct / 100, (c & 255) * pct / 100);
}
static void centerText(Frame& fb, int y, const char* s, RGB c, Font f = F3) { text(fb, (W - tw(s, f)) >> 1, y, s, c, f); }

// a filled tag with the corners knocked off (FINAL, TODAY, TOMORROW)
static int tag(Frame& fb, int x, const char* s, RGB bg) {
  int w = tw(s, F3) + 3;
  fillRect(fb, x, 0, x + w, 6, bg);
  for (int yy : {0, 6}) for (int xx : {x, x + w}) fb.unput(xx, yy);
  text(fb, x + 2, 1, s, WHITE, F3);
  return x + w + 3;
}

// The mini field, TV style: solid grass, a faint midfield line, end zones in
// team colours (away on the left, home on the right, matching the logos),
// the ball on its spot and the yellow line to gain. In the red zone only the
// 20 yards in front of the goal being attacked turn red, outlined in red.
// ESPN's yard line 0 is the home goal line, so the home team attacks left.
static void drawField(Frame& fb, const Game& g, int FY, int FH) {
  const int X0 = 6, LEN = 52;
  auto X = [&](int yd) { return X0 + (100 - yd) * LEN / 100; };   // yard 0 (home goal) at x=58
  RGB grass = rgb(8, 52, 18), rz = rgb(85, 12, 12);
  for (int x = X0; x <= X0 + LEN; x++) for (int yy = FY; yy < FY + FH; yy++) fb.put(x, yy, grass);
  bool homeBall = g.possession == 1;
  if (g.redzone && g.possession) {
    int from = homeBall ? 100 : 20, to = homeBall ? 80 : 0;
    for (int x = X(from); x <= X(to); x++) {
      for (int yy = FY; yy < FY + FH; yy++) fb.put(x, yy, rz);
      fb.put(x, FY - 1, RED); fb.put(x, FY + FH, RED);
    }
  }
  for (int yy = FY; yy < FY + FH; yy++) fb.put(X0 + LEN / 2, yy, rgb(45, 90, 55));   // midfield
  auto ez = [&](int x0, int x1, bool has, uint32_t c) {
    RGB l = ledColor(has, c);
    for (int x = x0; x <= x1; x++) for (int yy = FY; yy < FY + FH; yy++) fb.put(x, yy, l);
  };
  ez(0, X0 - 1, g.away.hasColor, g.away.color);
  ez(X0 + LEN + 1, 63, g.home.hasColor, g.home.color);
  if (g.yardLine < 0 || g.yardLine > 100 || !g.possession) return;
  int dir = homeBall ? -1 : 1;   // on screen: home drives left, away drives right
  int bx = X(g.yardLine);
  if (g.distance > 0) {
    int fd = g.yardLine + (homeBall ? g.distance : -g.distance);
    if (fd >= 0 && fd <= 100) {
      int fx = X(fd);
      for (int yy = FY; yy < FY + FH; yy++) fb.put(fx, yy, rgb(255, 215, 0));
    }
  }
  // the ball: a little football pointing the way they're going
  int cy = FY + (FH >> 1);
  RGB br = rgb(200, 100, 35);
  for (int dx = -1; dx <= 1; dx++) for (int dy = -1; dy <= 1; dy++) fb.put(bx + dx, cy + dy, br);
  fb.put(bx + 2 * dir, cy, br);
  fb.put(bx - 2 * dir, cy, br);
  fb.put(bx, cy, WHITE);   // the laces
}

// win chance: a thin bar, away share from the left (like the field); if the
// two colours look alike the away side goes white so the split still shows
static void drawWinBar(Frame& fb, const Game& g, int y) {
  if (g.winHome < 0) return;
  int split = 1 + (100 - g.winHome) * 62 / 100;
  RGB ch = ledColor(g.home.hasColor, g.home.color), ca = ledColor(g.away.hasColor, g.away.color);
  if (colorGap(ch, ca) < 120) ca = rgb(200, 200, 200);
  for (int x = 1; x <= 62; x++) fb.put(x, y, x < split ? ca : ch);
}

// timeouts left: three gold dashes under a score (grey once used)
static void timeoutDashes(Frame& fb, int cx, int y, int left) {
  if (left < 0) return;
  for (int i = 0; i < 3; i++)
    for (int xx = 0; xx < 3; xx++) fb.put(cx - 7 + i * 5 + xx, y, i < left ? GOLD : rgb(50, 50, 50));
}

// one block per period: done grey, now gold, still to come dark
static void periodTracker(Frame& fb, int y, int n, int cur) {
  int w = (60 - (n - 1) * 2) / n;
  for (int i = 0; i < n; i++) {
    RGB c = i + 1 < cur ? rgb(110, 110, 110) : i + 1 == cur ? GOLD : rgb(35, 35, 35);
    fillRect(fb, 2 + i * (w + 2), y, 2 + i * (w + 2) + w - 1, y + 1, c);
  }
}

// A full-screen score centred on cx: the big double-size digits, or for 100
// and up a narrower 4-wide set at the same size so three digits fit under the
// logo (Joe's pick B; 108-102 ran together in a live Knicks game).
static void bigScore(Frame& fb, int cx, int y, const char* s, RGB c) {
  int n = (int)strlen(s);
  if (n < 3) { scoreText(fb, cx - (tw(s, F5, 2) >> 1), y, s, c, F5, 2); return; }
  static const uint8_t N4[10][7] = {   // bit 3 = left column
    {6, 9, 9, 9, 9, 9, 6}, {2, 6, 2, 2, 2, 2, 7}, {6, 9, 1, 2, 4, 8, 15}, {14, 1, 1, 6, 1, 1, 14},
    {9, 9, 9, 15, 1, 1, 1}, {15, 8, 14, 1, 1, 9, 6}, {6, 8, 8, 14, 9, 9, 6}, {15, 1, 2, 2, 4, 4, 4},
    {6, 9, 9, 6, 9, 9, 6}, {6, 9, 9, 7, 1, 1, 6}};
  int x = cx - ((n * 9 - 1) >> 1);
  for (; *s; s++, x += 9) {
    if (*s < '0' || *s > '9') continue;
    const uint8_t* g = N4[*s - '0'];
    for (int r = 0; r < 7; r++)
      for (int q = 0; q < 4; q++)
        if (g[r] & (8 >> q)) fillRect(fb, x + q * 2, y + r * 2, x + q * 2 + 1, y + r * 2 + 1, c);
  }
}

// both logos, each score under its logo (your team's in scoreTop: gold after
// it scores; theirs in scoreBot)
static void teamsTop(Frame& fb, const Game& g, const Logo* la, const Logo* lh, int ax = 0, int hx = 38, RGB lose = 0) {
  const Side* sides[2] = {&g.away, &g.home};
  const Logo* lg[2] = {la, lh};
  for (int k = 0; k < 2; k++) {
    const Side& sd = *sides[k];
    bool homeSide = k == 1;
    int x = homeSide ? hx : ax;
    logoBox(fb, lg[k], x, 8, MATCHUP_W, MATCHUP_H, sd.abbr, sd.hasColor, sd.color);
    if (!sd.hasScore) continue;
    char s[6]; snprintf(s, sizeof(s), "%d", sd.score);
    RGB sc = homeSide == g.pinnedHome ? scoreTop : scoreBot;
    if (lose) {   // a final: the loser's score dimmed
      const Side& o = homeSide ? g.away : g.home;
      if (o.hasScore && o.score > sd.score) sc = lose;
    }
    bigScore(fb, x + 13, 32, s, sc);
  }
}

static void underScore(Frame& fb, bool homeSide, const char* s, RGB c, int ax = 0, int hx = 38) {
  int x = (homeSide ? hx : ax) + 13;
  text(fb, x - (tw(s, F3) >> 1), 47, s, c, F3);
}

static void upperCopy(char* out, size_t n, const char* in) {
  size_t i = 0;
  for (; in[i] && i < n - 1; i++) out[i] = toupper((unsigned char)in[i]);
  out[i] = 0;
}

// "2nd & 6 at PHI 34" -> "AT PHI 34"
static void ballSpot(char (&out)[16], const char* dd) {
  out[0] = 0;
  const char* at = nullptr;
  for (const char* p = dd; *p; p++) if (tolower((unsigned char)p[0]) == 'a' && tolower((unsigned char)p[1]) == 't' && p[2] == ' ' && (p == dd || p[-1] == ' ')) at = p;
  if (at) upperCopy(out, sizeof(out), at);
}

static void footballFull(Frame& fb, const Game& g, const Logo* la, const Logo* lh, uint32_t ms) {
  bool close = closeGameNow;
  RGB clk = g.redzone ? RED : close ? GOLD : CLOCK;
  char left[20];
  snprintf(left, sizeof(left), "%s %s", g.periodLabel[0] ? g.periodLabel : "", g.clock);
  text(fb, 2, 1, left, clk, F3);
  char r[32];
  shortDown(r, g.shortDD[0] ? g.shortDD : g.downDistance, tw(left, F3));
  if (r[0]) text(fb, W - 2 - tw(r, F3), 1, r, g.redzone ? RED : GOLD, F3);
  teamsTop(fb, g, la, lh);
  if (g.possession) sprite(fb, g.possession == 2 ? 27 : 31, 18, BALL, 5, BROWN);
  timeoutDashes(fb, 13, 47, g.toAway);
  timeoutDashes(fb, 51, 47, g.toHome);
  drawField(fb, g, 50, 5);
  drawWinBar(fb, g, 57);
  char spot[16];
  ballSpot(spot, g.downDistance);
  if (close && (ms / 2000) % 2) centerText(fb, 59, "CLOSE GAME", GOLD);
  else if (spot[0]) centerText(fb, 59, spot, g.redzone ? RED : DATEC);
}

// baseball: the field along the bottom - a big diamond with the runners in
// gold, balls and strikes on the left, outs on the right
static void baseballField(Frame& fb, const Game& g, bool count) {
  RGB grass = rgb(15, 75, 25), dirt = rgb(120, 72, 32), line = rgb(220, 220, 220);
  fillRect(fb, 0, 46, 63, 63, grass);
  int cx = 32, cy = 55;
  for (int dy = -8; dy <= 8; dy++) for (int dx = -9; dx <= 9; dx++) {
    int d = abs(dx) * 8 / 9 + abs(dy);
    if (d <= 8) fb.put(cx + dx, cy + dy, d >= 6 ? dirt : rgb(25, 105, 35));
  }
  auto base = [&](int x, int y, bool on) {
    fillRect(fb, x - 1, y - 1, x + 1, y + 1, on ? rgb(255, 200, 0) : line);
    if (on) fb.put(x, y, WHITE);
  };
  base(cx, cy - 7, g.bases[1]); base(cx - 8, cy, g.bases[2]); base(cx + 8, cy, g.bases[0]);
  fillRect(fb, cx - 1, cy + 7, cx + 1, cy + 7, WHITE); fb.put(cx, cy + 8, WHITE);   // home plate
  fillRect(fb, cx - 1, cy - 1, cx + 1, cy + 1, dirt);   // the mound
  if (!count) return;
  RGB off = rgb(10, 40, 15);
  auto dots = [&](int x, int y, const char* lab, int n, int on, RGB c) {
    text(fb, x, y, lab, CLOCK, F3);
    int sx = x + tw(lab, F3) + 2;
    for (int i = 0; i < n; i++) fillRect(fb, sx + i * 4, y + 1, sx + i * 4 + 2, y + 3, i < on ? c : off);
  };
  dots(2, 49, "B", 3, g.balls, GREEN);
  dots(2, 56, "S", 2, g.strikes, GOLD);
  text(fb, 62 - tw("OUT", F3), 49, "OUT", CLOCK, F3);
  for (int i = 0; i < 2; i++) fillRect(fb, 50 + i * 5, 56, 52 + i * 5, 58, i < g.outs ? RED : off);
}

static void baseballFull(Frame& fb, const Game& g, const Logo* la, const Logo* lh) {
  bool atBat = !strcmp(g.half, "TOP") || !strcmp(g.half, "BOT");
  if (atBat) {
    char num[8];
    const char* is = g.inningShort;
    if (is[0] >= 'A' && is[0] <= 'Z') is++;
    if (*is) scopy(num, is);
    else if (g.period) snprintf(num, sizeof(num), "%d", g.period);
    else num[0] = 0;
    sprite(fb, 2, 2, !strcmp(g.half, "TOP") ? ARROW_UP : ARROW_DN, 3, CLOCK);
    char inn[12];
    int n = atoi(num);
    const char* suf = (n % 100 >= 11 && n % 100 <= 13) ? "TH" : n % 10 == 1 ? "ST" : n % 10 == 2 ? "ND" : n % 10 == 3 ? "RD" : "TH";
    snprintf(inn, sizeof(inn), "%s%s", num, n ? suf : "");
    text(fb, 8, 1, inn, CLOCK, F3);
    if (g.balls >= 0 && g.strikes >= 0) {
      char c[12]; snprintf(c, sizeof(c), "%d-%d", g.balls, g.strikes);
      text(fb, W - 2 - tw(c, F3), 1, c, DATEC, F3);
    }
  } else {
    text(fb, 2, 1, g.inningText[0] ? g.inningText : g.periodLabel, CLOCK, F3);
  }
  // team-colour stripes down the edges, logos a little further in
  fillRect(fb, 0, 7, 0, 44, ledColor(g.away.hasColor, g.away.color));
  fillRect(fb, 63, 7, 63, 44, ledColor(g.home.hasColor, g.home.color));
  teamsTop(fb, g, la, lh, 2, 36);
  baseballField(fb, g, atBat);
}

// hockey: your power play = a banner in your colours with the time left
// draining; theirs = a PENALTY KILL warning on caution tape
static void hockeyPowerPlay(Frame& fb, const Game& g, const Logo* la, const Logo* lh) {
  const Side& pp = g.ppHome ? g.home : g.away;
  const Logo* lg = g.ppHome ? lh : la;
  bool ours = g.ppHome == g.pinnedHome;
  int secs = -1;
  if (g.ppTime[0]) { const char* c = strchr(g.ppTime, ':'); secs = c ? atoi(g.ppTime) * 60 + atoi(c + 1) : atoi(g.ppTime); }
  if (ours) {
    RGB bg = ledColor(pp.hasColor, pp.color);
    fillRect(fb, 0, 46, 63, 63, bg);
    if (lg && lg->n) logoHalf(fb, lg, 2, 48);
    else { char a[5]; fullAbbr(a, pp.abbr); text(fb, 2, 50, a, WHITE, F3); }
    text(fb, 18, 47, "POWER", WHITE, F3);
    text(fb, 18, 53, "PLAY", WHITE, F3);
    if (g.ppTime[0]) text(fb, 62 - tw(g.ppTime, F5), 48, g.ppTime, GOLD, F5);
    if (secs >= 0) {
      int full = secs > 120 ? 300 : 120;
      fillRect(fb, 2, 60, 61, 61, dimRgb(bg, 40));
      fillRect(fb, 2, 60, 2 + 59 * secs / full, 61, GOLD);
    }
  } else {
    for (int y : {46, 47, 62, 63})
      for (int x = 0; x < W; x++) fb.put(x, y, ((x + y) / 3) % 2 ? rgb(230, 190, 0) : rgb(10, 10, 10));
    centerText(fb, 49, "PENALTY KILL", GOLD);
    char who[12]; char a[5]; fullAbbr(a, pp.abbr);
    snprintf(who, sizeof(who), "%s PP", a);
    text(fb, 2, 55, who, DATEC, F3);
    if (g.ppTime[0]) text(fb, 62 - tw(g.ppTime, F3), 55, g.ppTime, CLOCK, F3);
  }
}

static void hockeyFull(Frame& fb, const Game& g, const Logo* la, const Logo* lh) {
  bool close = closeGameNow;
  if (g.intermission) {
    char l[12]; snprintf(l, sizeof(l), "%s INT", g.periodLabel);
    text(fb, 2, 1, l, CLOCK, F3);
    if (g.intermissionLeft[0]) text(fb, W - 2 - tw(g.intermissionLeft, F3), 1, g.intermissionLeft, DATEC, F3);
  } else {
    char l[20]; snprintf(l, sizeof(l), "%s %s", g.periodLabel, g.clock);
    text(fb, 2, 1, l, close ? GOLD : CLOCK, F3);
  }
  teamsTop(fb, g, la, lh);
  static const char* const PUCK[] = {"0111110", "1111111", "0111110"};
  sprite(fb, 28, 19, PUCK, 3, WHITE);
  if (g.pp && !g.intermission) { hockeyPowerPlay(fb, g, la, lh); return; }
  if (g.away.record[0]) underScore(fb, false, g.away.record, DATEC);
  if (g.home.record[0]) underScore(fb, true, g.home.record, DATEC);
  periodTracker(fb, 54, g.period > 3 ? 4 : 3, g.period);
  const char* lab;
  char buf[16];
  if (close) lab = "CLOSE GAME";
  else if (g.intermission) lab = "INTERMISSION";
  else if (g.period > 4 || !strcmp(g.periodLabel, "SO")) lab = "SHOOTOUT";
  else if (g.period == 4) lab = "OVERTIME";
  else { snprintf(buf, sizeof(buf), "%s PERIOD", g.periodLabel); lab = buf; }
  centerText(fb, 57, lab, close ? GOLD : DATEC);
}

static void basketballFull(Frame& fb, const Game& g, const Logo* la, const Logo* lh) {
  bool close = closeGameNow;
  char l[20]; snprintf(l, sizeof(l), "%s %s", g.periodLabel, g.clock);
  text(fb, 2, 1, l, close ? GOLD : CLOCK, F3);
  teamsTop(fb, g, la, lh);
  for (int k = 0; k < 2; k++) {
    bool home = k == 1;
    bool bonus = g.hasBonus && (home ? g.bonusHome : g.bonusAway);
    const Side& sd = home ? g.home : g.away;
    if (bonus) underScore(fb, home, "BONUS", GOLD);
    else if (sd.record[0]) underScore(fb, home, sd.record, DATEC);
  }
  periodTracker(fb, 54, g.period > 4 ? 5 : 4, g.period);
  static const char* const Q[] = {"", "1ST QUARTER", "2ND QUARTER", "3RD QUARTER", "4TH QUARTER"};
  const char* lab = close ? "CLOSE GAME" : g.period >= 1 && g.period <= 4 ? Q[g.period] : "OVERTIME";
  centerText(fb, 57, lab, close ? GOLD : DATEC);
}

static void finalFull(Frame& fb, const Game& g, const Logo* la, const Logo* lh) {
  int x = tag(fb, 1, "FINAL", rgb(80, 80, 90));
  const char* when = g.day[0] && strcmp(g.day, "TODAY") ? g.day : "";
  if (when[0]) text(fb, x, 1, when, DATEC, F3);
  bool base = g.sport == BASEBALL;
  if (base) {
    fillRect(fb, 0, 7, 0, 44, ledColor(g.away.hasColor, g.away.color));
    fillRect(fb, 63, 7, 63, 44, ledColor(g.home.hasColor, g.home.color));
  }
  int ax = base ? 2 : 0, hx = base ? 36 : 38;
  teamsTop(fb, g, la, lh, ax, hx, DATEC);
  const Side& m = g.pinned();
  const Side& o = g.other();
  if (m.hasScore && o.hasScore && m.score > o.score) underScore(fb, g.pinnedHome, "WIN", GREEN, ax, hx);
  fillRect(fb, 2, 53, 61, 63, rgb(25, 25, 32));
  if (g.nextText[0]) {
    text(fb, 5, 56, "NEXT", DATEC, F3);
    text(fb, 60 - tw(g.nextText, F3), 56, g.nextText, CLOCK, F3);
  } else if (g.po.on && g.po.summary[0]) {
    char up[20]; upperCopy(up, sizeof(up), g.po.summary);
    centerText(fb, 56, up, CLOCK);
  } else {
    char recs[28] = "";
    if (g.away.record[0] && g.home.record[0]) snprintf(recs, sizeof(recs), "%s  %s", g.away.record, g.home.record);
    centerText(fb, 56, recs, DATEC);
  }
}

// Upcoming game (Joe's pick A, 1.12): the day in big gold letters along the
// top (TODAY, TOMORROW, SATURDAY...), the logos with their records,
// and the start time big with the date small beside it along the bottom.
// Without a playoff series line underneath, everything sits a little lower.
static void upcomingFull(Frame& fb, const Game& g, const Logo* la, const Logo* lh) {
  static const char* const DAYS[][2] = {{"SUN", "SUNDAY"}, {"MON", "MONDAY"}, {"TUE", "TUESDAY"}, {"WED", "WEDNESDAY"},
                                        {"THU", "THURSDAY"}, {"FRI", "FRIDAY"}, {"SAT", "SATURDAY"}};
  const char* d = g.day[0] ? g.day : "NEXT";
  for (auto& n : DAYS)
    if (!strcmp(d, n[0])) { d = n[1]; break; }
  // the day in gold with a dark orange shadow (a little 3D), a thin grey line under it (Joe's style 5)
  int dx = (W - tw(d, F5)) >> 1;
  text(fb, dx + 1, 2, d, rgb(120, 60, 0), F5);
  text(fb, dx, 1, d, GOLD, F5);
  for (int x = 4; x < W - 4; x++) fb.put(x, 10, rgb(60, 60, 70));
  bool series = g.po.on && g.po.summary[0];
  int ly = series ? 12 : 14;
  logoBox(fb, la, 0, ly, MATCHUP_W, MATCHUP_H, g.away.abbr, g.away.hasColor, g.away.color);
  logoBox(fb, lh, 38, ly, MATCHUP_W, MATCHUP_H, g.home.abbr, g.home.hasColor, g.home.color);
  text(fb, 32 - (tw("AT", F3) >> 1), ly + 9, "AT", DATEC, F3);
  if (g.away.record[0]) text(fb, 13 - (tw(g.away.record, F3) >> 1), ly + 26, g.away.record, DATEC, F3);
  if (g.home.record[0]) text(fb, 51 - (tw(g.home.record, F3) >> 1), ly + 26, g.home.record, DATEC, F3);
  // the time big, the date small and grey beside it (no date when the bar already shows one)
  const char* t = g.startTime[0] ? g.startTime : "TBD";
  const char* date = strchr(d, '/') ? "" : g.gameDate;
  int w = tw(t, F5) + (date[0] ? 4 + tw(date, F3) : 0), x = (W - w) >> 1, ty = series ? 46 : 53;
  text(fb, x, ty, t, CLOCK, F5);
  if (date[0]) text(fb, x + tw(t, F5) + 4, ty + 2, date, DATEC, F3);
  if (series) { char up[24]; upperCopy(up, sizeof(up), g.po.summary); centerText(fb, 56, up, GOLD); }
}

void renderFull(Frame& fb, const Game& g, const Logo* la, const Logo* lh, uint32_t ms) {
  fb.clear();
  if (!g.valid) return;
  closeGameNow = renderCloseGame;
  if (g.state == ST_PRE) upcomingFull(fb, g, la, lh);
  else if (g.state == ST_POST) finalFull(fb, g, la, lh);
  else if (g.sport == FOOTBALL) footballFull(fb, g, la, lh, ms);
  else if (g.sport == BASEBALL) baseballFull(fb, g, la, lh);
  else if (g.sport == HOCKEY) hockeyFull(fb, g, la, lh);
  else basketballFull(fb, g, la, lh);
  // playoff games: a thin gold frame round the whole screen
  if (g.po.on) {
    for (int i = 0; i < W; i++) { fb.put(i, 0, PLAYOFF_GOLD); fb.put(i, H - 1, PLAYOFF_GOLD); fb.put(0, i, PLAYOFF_GOLD); fb.put(W - 1, i, PLAYOFF_GOLD); }
  }
}
