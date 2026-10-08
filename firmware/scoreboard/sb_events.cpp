#include "sb_events.h"
#include "sb_logo.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <new>

static bool icontains(const char* hay, const char* needle) {
  size_t n = strlen(needle);
  for (const char* p = hay; *p; p++) {
    size_t i = 0;
    while (i < n && p[i] && tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) i++;
    if (i == n) return true;
  }
  return false;
}

// "1st & 10 at ..." -> 1; anything else -> -1 (the preview's firstDownNum)
static int downNum(const char* dd) {
  while (dd && isspace((unsigned char)*dd)) dd++;
  if (!dd || !isdigit((unsigned char)*dd)) return -1;
  int n = 0;
  while (isdigit((unsigned char)*dd)) n = n * 10 + (*dd++ - '0');
  char a = tolower((unsigned char)dd[0]), b = tolower((unsigned char)dd[1]);
  bool ord = (a == 's' && b == 't') || (a == 'n' && b == 'd') || (a == 'r' && b == 'd') || (a == 't' && b == 'h');
  return ord ? n : -1;
}

// team colours first, then gold / white / ember so it stays colourful
static void palette(const Side& s, FxSpec& f) {
  f.npal = 0;
  auto add = [&](int r, int g, int b) { f.pal[f.npal][0] = r; f.pal[f.npal][1] = g; f.pal[f.npal][2] = b; f.npal++; };
  if (s.hasColor) add((s.color >> 16) & 255, (s.color >> 8) & 255, s.color & 255);
  if (s.hasColor2) add((s.color2 >> 16) & 255, (s.color2 >> 8) & 255, s.color2 & 255);
  add(255, 215, 0); add(255, 255, 255); add(255, 110, 30);
}

static void fxSide(const Side& s, FxSide& d) {
  scopy(d.abbr, s.abbr);
  d.hasScore = s.hasScore; d.score = s.score;
  d.hasColor = s.hasColor; d.color = s.color;
}

bool fxUsesLogo(FxKind k) {
  return k == FX_TOUCHDOWN || k == FX_GOAL || k == FX_HOMERUN || k == FX_THREE || k == FX_FIELDGOAL ||
         k == FX_FIRSTDOWN || k == FX_RUN || k == FX_WIN;
}

void winWords(const char* name, bool college, char (&out)[20]) {
  // pros: the nickname (last word); college: the school
  const char* w = name;
  if (!college) { const char* sp = strrchr(name, ' '); if (sp) w = sp + 1; }
  char up[20]; int i = 0;
  for (; w[i] && i < 13; i++) up[i] = toupper((unsigned char)w[i]);
  up[i] = 0;
  bool plural = i > 0 && up[i - 1] == 'S';
  snprintf(out, sizeof(out), "%s %s", up, plural ? "WIN" : "WINS");
  if (tw(out, F3) > W - 2) snprintf(out, sizeof(out), "%s", plural ? "WIN!" : "WINS!");
}

static void base(const Game& g, FxKind k, FxSpec& f) {
  new (&f) FxSpec();   // in place: a temporary would put 15 KB on the stack
  f.kind = k;
  palette(g.pinned(), f);
  scopy(f.label, g.pinned().abbr);
  fxSide(g.away, f.away);
  fxSide(g.home, f.home);
}

static void quarterLabel(int done, char (&o)[20]) {
  scopy(o, done == 1 ? "END 1ST" : done == 2 ? "HALFTIME" : done == 3 ? "END 3RD" : done == 4 ? "END REG" : "END OT");
}

FxKind detectEvent(const Game& prev, const Game& now, FxSpec& f) {
  if (!prev.valid || !now.valid || strcmp(prev.eventId, now.eventId) || prev.pinnedHome != now.pinnedHome ||
      prev.sport != now.sport)
    return FX_NONE;
  const Side& was = prev.pinned();
  const Side& mine = now.pinned();
  bool both = was.hasScore && mine.hasScore;
  int diff = both ? mine.score - was.score : 0;
  Sport sp = now.sport;
  bool liveBoth = now.state == ST_IN && prev.state == ST_IN;
  bool freshPlay = now.lastPlay[0] && strcmp(now.lastPlay, prev.lastPlay);

  if (sp == FOOTBALL && both && diff >= 6) { base(now, FX_TOUCHDOWN, f); return f.kind; }
  if (sp == FOOTBALL && both && diff == 3) { base(now, FX_FIELDGOAL, f); return f.kind; }
  if (sp == FOOTBALL && prev.state == ST_PRE && now.state == ST_IN) { base(now, FX_KICKOFF, f); return f.kind; }
  if ((sp == FOOTBALL || sp == BASKETBALL) && liveBoth && prev.period && now.period > prev.period) {
    base(now, FX_QUARTER, f);
    quarterLabel(prev.period, f.label);
    return f.kind;
  }
  if (sp == FOOTBALL && liveBoth && freshPlay && icontains(now.lastPlay, "penalty")) {
    base(now, FX_FLAG, f);
    char team[8], foul[20];
    parsePenalty(now.lastPlay, team, foul);
    scopy(f.label, foul);
    scopy(f.team, team);
    f.teamColor = WHITE;
    for (const Side* s : {&now.home, &now.away}) {
      char ab[8]; int i = 0;
      for (; s->abbr[i] && i < 7; i++) ab[i] = toupper((unsigned char)s->abbr[i]);
      ab[i] = 0;
      if (team[0] && !strcmp(ab, team)) f.teamColor = ledColor(s->hasColor, s->color);
    }
    return f.kind;
  }
  int mineSide = now.pinnedHome ? 1 : 2;
  if (sp == FOOTBALL && liveBoth && now.possession && now.possession == prev.possession &&
      now.possession == mineSide && downNum(now.downDistance) == 1 && downNum(prev.downDistance) > 1) {
    base(now, FX_FIRSTDOWN, f);
    return f.kind;
  }
  if (sp == HOCKEY && now.state == ST_IN && now.intermission && !prev.intermission) {
    base(now, FX_QUARTER, f);
    scopy(f.label, "INTERMISSION");
    f.hasLines = true;
    if (now.intLines[0][0]) { scopy(f.lines[0], now.intLines[0]); scopy(f.lines[1], now.intLines[1]); }
    else { scopy(f.lines[0], "END OF"); scopy(f.lines[1], now.periodLabel); }
    return f.kind;
  }
  if (sp == HOCKEY && now.state == ST_IN && both && diff > 0) { base(now, FX_GOAL, f); return f.kind; }
  if (sp == BASEBALL && now.state == ST_IN && both && diff > 0) {
    bool hr = freshPlay && (icontains(now.lastPlay, "home run") || icontains(now.lastPlay, "homers") ||
                            icontains(now.lastPlay, "grand slam"));
    if (hr) {
      base(now, FX_HOMERUN, f);
      f.grand = icontains(now.lastPlay, "grand slam");
    } else {
      base(now, FX_RUN, f);
      f.label[3] = 0;   // three letters, like the preview
      f.n = diff;
    }
    return f.kind;
  }
  if (sp == BASKETBALL && now.state == ST_IN && both && diff > 0) {
    bool three = freshPlay ? ((icontains(now.lastPlay, "three point") || icontains(now.lastPlay, "three-point") ||
                               icontains(now.lastPlay, "3-pt") || icontains(now.lastPlay, "3pt")) &&
                              !icontains(now.lastPlay, "miss"))
                           : diff == 3;
    if (three) { base(now, FX_THREE, f); return f.kind; }
  }
  return FX_NONE;
}

bool detectWin(const Game& prev, const Game& now, FxSpec& f) {
  if (!prev.valid || !now.valid || strcmp(prev.eventId, now.eventId) || prev.pinnedHome != now.pinnedHome) return false;
  if (!(prev.state == ST_IN && now.state == ST_POST)) return false;
  const Side& m = now.pinned();
  const Side& o = now.other();
  if (!m.hasScore || !o.hasScore || m.score <= o.score) return false;
  base(now, FX_WIN, f);
  snprintf(f.scoreText, sizeof(f.scoreText), "%d-%d", m.score, o.score);
  return true;
}

bool testEvent(const char* name, const Game* g, FxSpec& f) {
  struct N { const char* n; FxKind k; };
  static const N names[] = {
    {"touchdown", FX_TOUCHDOWN}, {"fieldgoal", FX_FIELDGOAL}, {"kickoff", FX_KICKOFF},
    {"quarter", FX_QUARTER}, {"halftime", FX_QUARTER}, {"intermission", FX_QUARTER},
    {"flag", FX_FLAG}, {"firstdown", FX_FIRSTDOWN}, {"goal", FX_GOAL}, {"run", FX_RUN},
    {"homerun", FX_HOMERUN}, {"grandslam", FX_HOMERUN}, {"three", FX_THREE}, {"win", FX_WIN}};
  FxKind kind = FX_NONE;
  for (const N& n : names) if (!strcmp(n.n, name)) kind = n.k;
  if (kind == FX_NONE) return false;
  static Game* demoP = nullptr;   // PSRAM: internal RAM is kept for Wi-Fi
  if (!demoP) demoP = (Game*)sbAlloc(sizeof(Game));
  Game& demo = *demoP;
  if (!g || !g->valid) {
    new (&demo) Game();
    demo.valid = true;
    scopy(demo.home.abbr, "NYG"); demo.home.hasColor = true; demo.home.color = 0x0B2265;
    demo.home.hasColor2 = true; demo.home.color2 = 0xA71930; demo.home.hasScore = true; demo.home.score = 21;
    scopy(demo.away.abbr, "DAL"); demo.away.hasColor = true; demo.away.color = 0x002A5C;
    demo.away.hasScore = true; demo.away.score = 17;
    demo.pinnedHome = true;
    g = &demo;
  }
  base(*g, kind, f);
  if (!strcmp(name, "quarter")) scopy(f.label, "END 1ST");
  else if (!strcmp(name, "halftime")) scopy(f.label, "HALFTIME");
  else if (!strcmp(name, "intermission")) {
    scopy(f.label, "INTERMISSION");
    f.hasLines = true;
    scopy(f.lines[0], "1ST"); scopy(f.lines[1], "INTERMISSION");
  } else if (kind == FX_FLAG) {
    const Side& o = g->other();
    char txt[96];
    snprintf(txt, sizeof(txt), "PENALTY on %s-J.Smith, Defensive Pass Interference, 15 yards", o.abbr);
    char team[8], foul[20];
    parsePenalty(txt, team, foul);
    scopy(f.label, foul); scopy(f.team, team);
    f.teamColor = ledColor(o.hasColor, o.color);
  } else if (kind == FX_RUN) { f.label[3] = 0; f.n = 2; }
  else if (!strcmp(name, "grandslam")) f.grand = true;
  else if (kind == FX_WIN) {
    const Side& m = g->pinned();
    const Side& o = g->other();
    int ms = m.hasScore ? m.score : 24, os = o.hasScore ? o.score : 17;
    if (ms <= os) ms = os + 3;   // a test win card is a win
    snprintf(f.scoreText, sizeof(f.scoreText), "%d-%d", ms, os);
  }
  return true;
}

SoundId fxSound(const FxSpec& f) {
  switch (f.kind) {
    case FX_TOUCHDOWN: return SND_TOUCHDOWN;
    case FX_FIELDGOAL: return SND_FIELDGOAL;
    // kickoff: the Gators get their fight song (Orange and Blue), everyone else the whistle
    case FX_KICKOFF: return strcmp(f.label, "FLA") ? SND_WHISTLE : SND_GATORS;   // no NFL team is FLA
    case FX_FLAG: return SND_WHISTLE;
    case FX_QUARTER: return SND_BUZZER;
    case FX_GOAL: return SND_GOALHORN;
    case FX_HOMERUN: return f.grand ? SND_GRANDSLAM : SND_HOMERUN;
    case FX_THREE: return SND_SWISH;
    case FX_WIN: return SND_WIN;
    default: return SND_NONE;
  }
}

// "1:32" -> 92, "45.2" -> 45, "" -> -1
static int clockSecs(const char* c) {
  if (!c || !*c) return -1;
  const char* colon = strchr(c, ':');
  if (colon) return atoi(c) * 60 + atoi(colon + 1);
  return atoi(c);
}

bool closeGame(const Game& g) {
  if (!g.valid || g.state != ST_IN || !g.home.hasScore || !g.away.hasScore) return false;
  int lastPeriod, margin;
  if (g.sport == FOOTBALL) { lastPeriod = 4; margin = 8; }
  else if (g.sport == BASKETBALL) { lastPeriod = 4; margin = 3; }
  else if (g.sport == HOCKEY) { lastPeriod = 3; margin = 1; }
  else return false;
  if (g.period < lastPeriod) return false;
  if (g.sport == HOCKEY && g.intermission) return false;
  int s = clockSecs(g.clock);
  return s >= 0 && s <= 120 && abs(g.home.score - g.away.score) <= margin;
}

SoundId soundEvent(const Game& prev, const Game& now) {
  if (!prev.valid || !now.valid || strcmp(prev.eventId, now.eventId) || prev.pinnedHome != now.pinnedHome) return SND_NONE;
  if (prev.state == ST_PRE && now.state == ST_IN && now.sport != FOOTBALL) return SND_GAMESTART;
  const Side& wasO = prev.other();
  const Side& o = now.other();
  const Side& wasM = prev.pinned();
  const Side& m = now.pinned();
  bool live = prev.state == ST_IN && now.state == ST_IN;
  bool theyScored = live && wasO.hasScore && o.hasScore && o.score > wasO.score;
  bool weScored = wasM.hasScore && m.hasScore && m.score > wasM.score;
  if (theyScored && !weScored && now.sport != BASKETBALL) return SND_THEYSCORED;
  return SND_NONE;
}
