#include "sb_render.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

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

// A runner: solid gold base, white centre. Empty: faint grey outline.
// (Same as the preview's drawBaseDiamond.)
static void drawBaseDiamond(Frame& fb, int x, int y, const bool* bases) {
  auto base = [&](int cx, int cy, bool on) {
    RGB c = on ? rgb(255, 200, 0) : rgb(70, 70, 70);
    fb.put(cx, cy - 1, c); fb.put(cx - 1, cy, c); fb.put(cx + 1, cy, c); fb.put(cx, cy + 1, c);
    if (on) fb.put(cx, cy, WHITE);
  };
  base(x + 5, y + 1, bases[1]);   // 2nd
  base(x + 1, y + 4, bases[2]);   // 3rd
  base(x + 9, y + 4, bases[0]);   // 1st
  fb.put(x + 5, y + 6, rgb(90, 90, 90));   // home plate
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

// Score digits: the font's 1 is a thin stroke, so a 14 looks lighter than a
// 6. Scores use a chunkier, stadium-style 1 (same width as the other digits).
static const uint8_t BOLD_ONE[7] = {0b00110, 0b01110, 0b00110, 0b00110, 0b00110, 0b00110, 0b01111};
static void scoreText(Frame& fb, int x, int y, const char* s, RGB c, Font f, int sc) {
  if (f != F5) { text(fb, x, y, s, c, f, sc); return; }
  char one[2] = {0, 0};
  for (; *s; s++) {
    if (*s == '1') {
      for (int ry = 0; ry < 7; ry++)
        for (int rx = 0; rx < 5; rx++)
          if (BOLD_ONE[ry] & (1 << (4 - rx)))
            for (int sy = 0; sy < sc; sy++)
              for (int sx = 0; sx < sc; sx++) fb.put(x + rx * sc + sx, y + ry * sc + sy, c);
    } else {
      one[0] = *s;
      text(fb, x, y, one, c, f, sc);
    }
    x += 5 * sc + 1;
  }
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

// No-ticker mode (the wheel's push): the game fills the panel - bigger
// scores, and underneath the last play scrolling past (or, before the game,
// when it starts).
static const int B_DIVIDER = 55, B_LINE = 58;

static void renderBig(Frame& fb, const Game& g, int pair, const Logo* la, const Logo* lh, uint32_t ms) {
  bool matchup = g.state == ST_PRE && la && lh && la->n && lh->n;
  drawMain(fb, g, pair, la, lh, !matchup);
  RGB frame = g.po.on ? PLAYOFF_GOLD : (g.preseason ? PRESEASON_SILVER : 0);
  bool hasFrame = g.po.on || g.preseason;
  bool dotted = !g.po.on && g.preseason;
  auto on = [&](int i) { return !dotted || i % 2 == 0; };
  for (int x = 0; x < W; x++) fb.put(x, B_DIVIDER, hasFrame && on(x) ? frame : LINE);
  if (hasFrame) {
    for (int x = 0; x < W; x++) if (on(x)) fb.put(x, 0, frame);
    for (int y = 0; y < B_DIVIDER; y++) if (on(y)) { fb.put(0, y, frame); fb.put(W - 1, y, frame); }
  }
  if (matchup && g.kickoff[0]) {
    // date and time big under the logos
    text(fb, (W - tw(g.kickoff, F3)) >> 1, 44, g.kickoff, CLOCK, F3);
  }
  const char* line = "";
  RGB lc = DATEC;
  if (g.state == ST_PRE) { if (!matchup) { line = g.kickoff; lc = CLOCK; } }
  else if (g.state == ST_IN) line = g.lastPlay;
  if (!line[0]) return;
  char up[200];
  int i = 0;
  for (; line[i] && i < (int)sizeof(up) - 1; i++) up[i] = toupper((unsigned char)line[i]);
  up[i] = 0;
  int w = tw(up, F3);
  if (w <= W - 4) { text(fb, (W - w) >> 1, B_LINE, up, lc, F3); return; }
  // scroll right to left, about 25 dots a second, with a gap before it repeats
  int span = w + W;
  int x = W - (int)((ms / 40) % (uint32_t)span);
  text(fb, x, B_LINE, up, lc, F3);
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
  if (big) { renderBig(fb, g, pair, la, lh, ms); return; }
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

bool renderBigScrolls(const Game& g) {
  return g.valid && g.state == ST_IN && g.lastPlay[0] && tw(g.lastPlay, F3) > W - 4;
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
  text(fb, x + ((SMALL_LOGO - w) >> 1), y + 5, a, ledColor(hasC, c), F3);
}

void renderFullTicker(Frame& fb, const FullGame* games, int n, int page, const char* title) {
  fb.clear();
  if (n <= 0) {
    text(fb, (W - tw(title, F5)) >> 1, 20, title, GOLD, F5);
    text(fb, (W - tw("NO GAMES", F3)) >> 1, 34, "NO GAMES", DATEC, F3);
    return;
  }
  int pages = (n + 1) / 2;
  int p = page % pages;
  for (int k = 0; k < 2; k++) {
    int i = p * 2 + k;
    if (i >= n) break;
    const FullGame& g = games[i];
    const Tick& t = g.t;
    int y = k * 32;
    logoOrLetters(fb, g.la, 1, y + 2, t.away, t.hasAwayColor, t.awayColor);
    logoOrLetters(fb, g.lh, W - 1 - SMALL_LOGO, y + 2, t.home, t.hasHomeColor, t.homeColor);
    bool fin = !strcmp(t.status, "F"), sched = !t.score[0];
    if (!sched) {
      char a[8] = "", h[8] = "";
      const char* d = strchr(t.score, '-');
      if (d) { size_t m = d - t.score; if (m > 7) m = 7; memcpy(a, t.score, m); a[m] = 0; scopy(h, d + 1); }
      int wa = tw(a, F5), wh = tw(h, F5);
      int mid = W / 2;
      scoreText(fb, mid - 3 - wa, y + 6, a, WHITE, F5, 1);
      for (int x = mid - 1; x <= mid; x++) fb.put(x, y + 9, GRAY);
      scoreText(fb, mid + 3, y + 6, h, WHITE, F5, 1);
      if (t.possession) sprite(fb, t.possession == 2 ? mid - 3 - wa : mid + 3 + wh - 7, y + 15, BALL, 5, BROWN);
      (void)wh;
    } else {
      text(fb, (W - tw("AT", F3)) >> 1, y + 8, "AT", DATEC, F3);
    }
    char st[24];
    RGB sc = CLOCK;
    if (fin) { scopy(st, "FINAL"); sc = GREEN; }
    else if (sched) snprintf(st, sizeof(st), "%s %s", t.kickDate, t.kickTime);
    else snprintf(st, sizeof(st), "%s %s", t.status, t.clock);
    text(fb, (W - tw(st, F3)) >> 1, y + 22, st, sc, F3);
    if (t.redzone)
      for (int yy = y + 1; yy < y + 30; yy++) { fb.put(0, yy, RED); fb.put(W - 1, yy, RED); }
    if (k == 0) for (int x = 2; x < W - 2; x++) fb.put(x, 31, rgb(50, 50, 50));
  }
}

// --------------------------------------------------- football full-game
void renderFootballFull(Frame& fb, const Game& g, const Logo* la16, const Logo* lh16, uint32_t ms) {
  fb.clear();
  char left[20];
  snprintf(left, sizeof(left), "%s %s", g.periodLabel[0] ? g.periodLabel : "", g.clock);
  text(fb, 2, 1, left, CLOCK, F3);
  char r[32];
  shortDown(r, g.shortDD[0] ? g.shortDD : g.downDistance, tw(left, F3));
  if (r[0]) text(fb, W - 2 - tw(r, F3), 1, r, g.redzone ? RED : DATEC, F3);
  // your team on top, like the main screen
  for (int k = 0; k < 2; k++) {
    bool homeRow = (k == 0) == g.pinnedHome;
    const Side& sd = homeRow ? g.home : g.away;
    const Logo* lg = homeRow ? lh16 : la16;
    int y = 8 + k * 18;
    static const Logo none;
    logoOrLetters(fb, lg ? *lg : none, 1, y, sd.abbr, sd.hasColor, sd.color);
    char s[6]; snprintf(s, sizeof(s), "%d", sd.score);
    RGB scol = WHITE;
    if (k == 0) scol = scoreTop; else scol = scoreBot;
    scoreText(fb, W - 2 - tw(s, F5, 2), y + 1, s, scol, F5, 2);
    bool ball = g.possession && ((g.possession == 1) == homeRow);
    if (ball) sprite(fb, 22, y + 4, BALL, 5, BROWN);
    int to = homeRow ? g.toHome : g.toAway;
    if (to >= 0)
      for (int i = 0; i < 3; i++)
        for (int xx = 0; xx < 3; xx++) fb.put(21 + i * 4 + xx, y + 13, i < to ? rgb(255, 200, 0) : rgb(50, 50, 50));
  }
  // mini field: home goal on the left (yard line 0), away on the right
  const int FY = 45, X0 = 6, LEN = 52;
  RGB grass = g.redzone ? rgb(80, 12, 12) : rgb(10, 60, 20);
  RGB mark = g.redzone ? rgb(140, 40, 40) : rgb(40, 110, 50);
  for (int x = 2; x <= 61; x++) for (int yy = FY; yy < FY + 5; yy++) fb.put(x, yy, grass);
  auto dimC = [](bool has, uint32_t c) { RGB l = ledColor(has, c); return rgb(((l >> 16) & 255) * 7 / 10, ((l >> 8) & 255) * 7 / 10, (l & 255) * 7 / 10); };
  for (int x = 2; x < X0; x++) for (int yy = FY; yy < FY + 5; yy++) fb.put(x, yy, dimC(g.home.hasColor, g.home.color));
  for (int x = X0 + LEN; x <= 61; x++) for (int yy = FY; yy < FY + 5; yy++) fb.put(x, yy, dimC(g.away.hasColor, g.away.color));
  for (int yd = 10; yd < 100; yd += 10) { int x = X0 + yd * LEN / 100; fb.put(x, FY, mark); fb.put(x, FY + 4, mark); }
  if (g.yardLine >= 0 && g.yardLine <= 100 && g.possession) {
    bool homeBall = g.possession == 1;          // home drives toward the away goal (right)
    int dir = homeBall ? 1 : -1;
    int bx = X0 + g.yardLine * LEN / 100;
    if (g.distance > 0) {
      int fd = g.yardLine + dir * g.distance;
      if (fd >= 0 && fd <= 100) { int fx = X0 + fd * LEN / 100; for (int yy = FY; yy < FY + 5; yy++) fb.put(fx, yy, rgb(255, 215, 0)); }
    }
    for (int yy = FY + 1; yy < FY + 4; yy++) { fb.put(bx, yy, rgb(230, 120, 40)); fb.put(bx - dir, yy, rgb(230, 120, 40)); }
    fb.put(bx + dir, FY + 2, WHITE); fb.put(bx + 2 * dir, FY + 2, WHITE); fb.put(bx + dir, FY + 1, WHITE); fb.put(bx + dir, FY + 3, WHITE);
  }
  // win chance: home share from the left, in team colours
  if (g.winHome >= 0) {
    int split = 2 + g.winHome * 60 / 100;
    RGB ch = ledColor(g.home.hasColor, g.home.color), ca = ledColor(g.away.hasColor, g.away.color);
    for (int x = 2; x <= 61; x++) fb.put(x, 52, x < split ? ch : ca);
  }
  // last play, gold when it scored
  if (g.lastPlay[0]) {
    char up[200]; int i = 0;
    for (; g.lastPlay[i] && i < (int)sizeof(up) - 1; i++) up[i] = toupper((unsigned char)g.lastPlay[i]);
    up[i] = 0;
    const char* s = up;
    while (*s == ' ') s++;
    RGB c = g.playScore > 0 ? GOLD : DATEC;
    int w = tw(s, F3);
    if (w <= W - 4) text(fb, (W - w) >> 1, 57, s, c, F3);
    else text(fb, W - (int)((ms / 40) % (uint32_t)(w + W)), 57, s, c, F3);
  }
}
