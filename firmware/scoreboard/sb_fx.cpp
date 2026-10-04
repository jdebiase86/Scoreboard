#include "sb_fx.h"
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <string>
#include <algorithm>

static const fr PI_ = 3.14159265358979323846;
static const fr TICK = 1000.0 / 60.0;

// JavaScript's Math.round
static inline int jr(fr x) { return (int)floor(x + 0.5); }
static inline RGB col(const int* c, fr b) { return rgb(jr(c[0] * b), jr(c[1] * b), jr(c[2] * b)); }
static inline RGB col3(int r, int g, int b_, fr b) { const int c[3] = {r, g, b_}; return col(c, b); }
static const int C_WHITE[3] = {255, 255, 255};

// the preview's liftFx: dark team colours lifted so they light up
static void liftFx(const int* in, int* out) {
  int m = std::max(in[0], std::max(in[1], in[2]));
  if (m == 0) { out[0] = out[1] = out[2] = 255; return; }
  if (m < 150) {
    fr f = 200.0 / m;
    for (int i = 0; i < 3; i++) out[i] = std::min(255, (int)(in[i] * f));
    return;
  }
  for (int i = 0; i < 3; i++) out[i] = in[i];
}

static void shortAbbr(char (&o)[4], const char* a) { strncpy(o, a ? a : "", 3); o[3] = 0; }
static void fullAbbr(char (&o)[5], const char* a) { strncpy(o, a ? a : "", 4); o[4] = 0; }

// ------------------------------------------------------------------ sprites
static const char* const BALL[] = {"0011100", "0111110", "1112111", "0111110", "0011100"};
static const char* const FLAG_FLY[4][7] = {
  {"110000000000", "011100000000", "001111100220", "000011112222", "001111112222", "011100000220", "110000000000"},
  {"000000000000", "111110000000", "011111110220", "000111112222", "000011112222", "111111100220", "000000000000"},
  {"011100000000", "001111000000", "000011110220", "111111112222", "000001112222", "000011100220", "000111000000"},
  {"000111000000", "000011100000", "000001110220", "111111112222", "000011112222", "001111000220", "011100000000"}};
static const char* const FLAG_LANDED[] = {"0110000000110", "0111100011110", "0011112111100", "0001122211000",
                                          "0011112111100", "0111100011110", "0110000000110"};
static const char* const LAMP[] = {"0001111000", "0011111100", "0111111110", "0111111110", "2222222222"};
static const char* const BASEBALL[] = {"0011100", "0111110", "1211121", "1111111", "1211121", "0111110", "0011100"};
static const char* const HR_BALL[] = {"010", "111", "010"};
static const char* const HOOP_BALL[] = {"01110", "12121", "11211", "12121", "01110"};

// knot leads, flop, knot flips to the other end, flop again
static char mirrored[3][7][13];
static const char* tumble[6][7];
static bool tumbleReady = false;
static void buildTumble() {
  if (tumbleReady) return;
  auto mirror = [](int f, int slot) {
    for (int r = 0; r < 7; r++) {
      int n = strlen(FLAG_FLY[f][r]);
      for (int i = 0; i < n; i++) mirrored[slot][r][i] = FLAG_FLY[f][r][n - 1 - i];
      mirrored[slot][r][n] = 0;
    }
  };
  mirror(2, 0); mirror(0, 1); mirror(1, 2);
  for (int r = 0; r < 7; r++) {
    tumble[0][r] = FLAG_FLY[0][r];
    tumble[1][r] = FLAG_FLY[1][r];
    tumble[2][r] = mirrored[0][r];
    tumble[3][r] = mirrored[1][r];
    tumble[4][r] = mirrored[2][r];
    tumble[5][r] = FLAG_FLY[3][r];
  }
  tumbleReady = true;
}

// ------------------------------------------------------------------- engine
static const int DUR = 5000;
int FxPlayer::durationMs() const { return DUR; }

// mulberry32 - small, fast, and the test harness uses the same one in the
// browser code, so the sparks land in the same places
fr FxPlayer::rnd() {
  uint32_t a = rng_ += 0x6D2B79F5u;
  uint32_t t = (a ^ (a >> 15)) * (1u | a);
  t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
  return (fr)(t ^ (t >> 14)) / 4294967296.0;
}

void FxPlayer::blast(fr cx, fr cy, fr power) {
  const fr n[3] = {30 * power, 22 * power, 14 * power}, spd[3] = {1.9, 1.15, 0.55};
  for (int r = 0; r < 3; r++)
    for (int i = 0; i < n[r]; i++) {
      fr ang = rnd() * PI_ * 2;
      fr s = spd[r] * (0.6 + rnd() * 0.8);
      Spark p;
      p.x = cx; p.y = cy;
      p.vx = cos(ang) * s; p.vy = sin(ang) * s;
      p.life = 1;
      p.decay = 0.008 + rnd() * 0.007;
      const int* c = pal_[(int)(rnd() * npal_)];
      p.c[0] = c[0]; p.c[1] = c[1]; p.c[2] = c[2];
      parts_.push_back(p);
    }
}

static void dropDead(std::vector<Spark, FxAlloc<Spark>>& v) {
  v.erase(std::remove_if(v.begin(), v.end(), [](const Spark& p) { return p.life <= 0; }), v.end());
}

void FxPlayer::start(const FxSpec* spec, uint32_t seed) {
  buildTumble();
  spec_ = spec;
  rng_ = seed;
  ticks_ = 0;
  parts_.clear();
  waves_.clear();
  npal_ = std::min(spec->npal, FX_MAXPAL);
  if (!npal_) { npal_ = 1; pal_[0][0] = pal_[0][1] = pal_[0][2] = 255; }
  else for (int i = 0; i < npal_; i++) liftFx(spec->pal[i], pal_[i]);
  if (spec->kind == FX_TOUCHDOWN) {
    blast(32, 30, 1.6);
    const int* c1 = npal_ > 1 ? pal_[1] : pal_[0];
    waves_.push_back({32, 30, 0, {pal_[0][0], pal_[0][1], pal_[0][2]}, 0.055, 56});
    waves_.push_back({32, 30, 110, {c1[0], c1[1], c1[2]}, 0.042, 50});
    waves_.push_back({32, 30, 230, {255, 255, 255}, 0.032, 44});
    nextBlast_ = 620;
  } else if (spec->kind == FX_HOMERUN) {
    nextBlast_ = 1850;
  } else if (spec->kind == FX_THREE) {
    nextBlast_ = 1900;
  }
  qHalf_ = spec->kind == FX_QUARTER && !strcmp(spec->label, "HALFTIME");
}

// One step of the sparks, at time el
void FxPlayer::physics(fr el) {
  FxKind k = spec_->kind;
  if (k == FX_TOUCHDOWN) {
    if (el > nextBlast_ && el < DUR - 800) {
      fr cx = 6 + rnd() * 52, cy = 6 + rnd() * 46;
      blast(cx, cy, 1.05);
      const int* c = pal_[(int)(rnd() * npal_)];
      waves_.push_back({cx, cy, el, {c[0], c[1], c[2]}, 0.045, 30});
      dropDead(parts_);
      nextBlast_ = el + 240 + rnd() * 200;
    }
    for (auto& p : parts_) {
      if (p.life <= 0) continue;
      p.x += p.vx; p.y += p.vy;
      p.vy += 0.022;
      p.vx *= 0.985;
      p.life -= p.decay;
    }
  } else if ((k == FX_HOMERUN && el > 1800 && el < 3500) || (k == FX_THREE && el >= 1850 && el < 3500)) {
    bool hr = k == FX_HOMERUN;
    if (el > nextBlast_ && el < (hr ? 3000 : 3200)) {
      dropDead(parts_);
      if (hr) { fr x = 10 + rnd() * 44, y = 6 + rnd() * 10; blast(x, y, 0.55); nextBlast_ = el + 260 + rnd() * 180; }
      else { fr x = 8 + rnd() * 48, y = 8 + rnd() * 44; blast(x, y, 0.5); nextBlast_ = el + 220 + rnd() * 160; }
    }
    for (auto& p : parts_) {
      if (p.life <= 0) continue;
      p.x += p.vx * 0.7; p.y += p.vy * 0.7; p.vy += 0.02; p.life -= p.decay * 1.4;
    }
  }
}

bool FxPlayer::frame(Frame& fb, fr el) {
  if (!spec_) return false;
  long target = (long)floor((el + 1e-6) / TICK);
  while (ticks_ < target) {
    ticks_++;
    physics(ticks_ == target ? el : ticks_ * TICK);
  }
  fb.clear();
  switch (spec_->kind) {
    case FX_TOUCHDOWN: drawTouchdown(fb, el); break;
    case FX_KICKOFF: drawKickoff(fb, el); break;
    case FX_QUARTER: drawQuarter(fb, el); break;
    case FX_FIELDGOAL: drawFieldGoal(fb, el); break;
    case FX_FLAG: drawFlag(fb, el); break;
    case FX_FIRSTDOWN: drawFirstDown(fb, el); break;
    case FX_GOAL: drawGoal(fb, el); break;
    case FX_RUN: drawRun(fb, el); break;
    case FX_HOMERUN: drawHomeRun(fb, el); break;
    case FX_THREE: drawThree(fb, el); break;
    default: break;
  }
  if (el < DUR) return true;
  spec_ = nullptr;
  return false;
}

static void drawRing(Frame& fb, fr cx, fr cy, fr r, const int* c, fr bright) {
  if (r < 1) return;
  int steps = std::max(10, jr(r * 7));
  RGB k = col(c, bright);
  for (int i = 0; i < steps; i++) {
    fr a = (PI_ * 2 * i) / steps;
    fb.put(jr(cx + cos(a) * r), jr(cy + sin(a) * r), k);
  }
}

static void blackBox(Frame& fb, int x0, int x1, int y0, int y1) {
  for (int x = x0; x <= x1; x++) for (int y = y0; y <= y1; y++) fb.put(x, y, BLACK);
}

// ---------------------------------------------------------------- touchdown
void FxPlayer::drawTouchdown(Frame& fb, fr el) {
  const FxSpec& s = *spec_;
  if (el < 130) {
    RGB c = col(C_WHITE, 1 - el / 130);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) fb.put(x, y, c);
  }
  for (auto& w : waves_) {
    fr t = el - w.t0;
    if (t <= 0) continue;
    fr r = t * w.speed;
    if (r > w.max) continue;
    drawRing(fb, w.cx, w.cy, r, w.c, std::max<fr>(0, 1 - r / w.max));
    drawRing(fb, w.cx, w.cy, r - 1, w.c, std::max<fr>(0, 0.45 - r / w.max));
  }
  for (auto& p : parts_) {
    if (p.life <= 0) continue;
    fr b = std::max<fr>(0, std::min<fr>(1, p.life));
    fb.put(jr(p.x), jr(p.y), col(p.c, b));
    if (b > 0.4) fb.put(jr(p.x - p.vx), jr(p.y - p.vy), col(p.c, b * 0.4));
    if (b > 0.7) fb.put(jr(p.x - p.vx * 2), jr(p.y - p.vy * 2), col(p.c, b * 0.18));
  }
  if (el > 620) {
    const int* cyc = pal_[(long)floor(el / 130) % npal_];
    if (s.logo.n) {
      fr b = std::min<fr>(1, (el - 620) / 320);
      int ly = std::max(0, (54 - s.logo.h) >> 1);
      drawLogo(fb, s.logo, (W - s.logo.w) >> 1, ly, b);
    } else {
      int lw = tw(s.label, F5, 2);
      if (lw <= W - 2) text(fb, (W - lw) >> 1, 20, s.label, col(cyc, 1), F5, 2);
      else text(fb, (W - tw(s.label, F5, 1)) >> 1, 24, s.label, col(cyc, 1), F5, 1);
    }
    int sw = tw(s.banner, F3);
    int ty = s.logo.n ? 58 : 42;
    blackBox(fb, ((W - sw) >> 1) - 1, ((W - sw) >> 1) + sw, ty - 1, ty + 5);
    text(fb, (W - sw) >> 1, ty, s.banner, ((long)floor(el / 130) % 2) ? rgb(255, 255, 255) : rgb(255, 190, 0), F3);
  }
}

// ------------------------------------------------------------------ kickoff
void FxPlayer::drawKickoff(Frame& fb, fr el) {
  const FxSpec& s = *spec_;
  const int* A = pal_[0];
  const int* B = npal_ > 1 ? pal_[1] : pal_[0];
  if (el < 1000) {
    fr t = el / 1000;
    int reach = jr(33 * t);
    for (int x = 0; x < reach; x++) {
      fr fade = 0.30 + 0.70 * (1 - x / 33.0);
      RGB ca = col(A, fade * 0.55), cb = col(B, fade * 0.55);
      for (int y = 0; y < H; y++) { fb.put(x, y, ca); fb.put(W - 1 - x, y, cb); }
    }
    char aA[4], hA[4];
    shortAbbr(aA, s.away.abbr); shortAbbr(hA, s.home.abbr);
    int aw = tw(aA, F5), hw = tw(hA, F5);
    text(fb, std::max(1, reach - aw - 2), 28, aA, WHITE, F5);
    text(fb, std::min(W - hw - 1, W - reach + 2), 28, hA, WHITE, F5);
  } else if (el < 1350) {
    fr t = (el - 1000) / 350;
    int top = jr(H * t);
    for (int x = 0; x < W; x++)
      for (int y = top; y < H; y++) fb.put(x, y, col(x < 32 ? A : B, 0.55 * (1 - t)));
    for (int gx = 0; gx < W; gx++) fb.put(gx, 58, rgb(20, 70, 25));
  } else if (el < 3100) {
    fr t = (el - 1350) / 1750;
    for (int gx = 0; gx < W; gx++) fb.put(gx, 58, rgb(20, 70, 25));
    for (int k = 7; k >= 1; k--) {
      fr tt = t - k * 0.030;
      if (tt < 0) continue;
      fr bx = 4 + tt * 55, by = 54 - sin(PI_ * tt) * 44;
      fb.put(jr(bx), jr(by), col3(190, 95, 30, 0.5 - k * 0.055));
    }
    fr x = 4 + t * 55, y = 54 - sin(PI_ * t) * 44;
    sprite(fb, jr(x) - 3, jr(y) - 2, BALL, 5, BROWN);
  } else {
    fr t = (el - 3100) / (DUR - 3100);
    fr pulse = 0.25 + 0.20 * sin(el / 90);
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) fb.put(x, y, col(x < 32 ? A : B, pulse * 0.35 * (1 - t * 0.5)));
    int w1 = tw("KICK", F5, 2), w2 = tw("OFF", F5, 2);
    fr show = std::min<fr>(1, t * 3);
    static const int GOLDC[3] = {255, 190, 0};
    const int* accent = npal_ > 1 ? pal_[1] : GOLDC;
    for (int bx = 0; bx < W; bx++) for (int by = 16; by < 48; by++) fb.put(bx, by, BLACK);
    text(fb, (W - w1) >> 1, 18, "KICK", col(C_WHITE, show), F5, 2);
    text(fb, (W - w2) >> 1, 34, "OFF", col(accent, show), F5, 2);
  }
}

// ------------------------------------------------------------ quarter break
void FxPlayer::drawQuarter(Frame& fb, fr el) {
  const FxSpec& s = *spec_;
  const int* C = pal_[0];
  const int IN = 420, OUT = DUR - 420;
  if (el < IN) {
    int edge = jr(H * (el / IN));
    for (int y = 0; y < edge; y++)
      for (int x = 0; x < W; x++) fb.put(x, y, col(C, y > edge - 3 ? 0.9 : 0.16));
    return;
  }
  if (el > OUT) {
    fr t = (el - OUT) / 420;
    int edge = jr(H * t);
    for (int y = edge; y < H; y++)
      for (int x = 0; x < W; x++) fb.put(x, y, col(C, y < edge + 3 ? 0.9 : 0.16));
    return;
  }
  RGB bg = col(C, 0.16);
  for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) fb.put(x, y, bg);
  RGB rule = col(C, 0.95);
  for (int x = 6; x < W - 6; x++) { fb.put(x, 14, rule); fb.put(x, 30, rule); }

  const char* l1 = s.hasLines ? s.lines[0] : (qHalf_ ? "HALF" : nullptr);
  const char* l2 = s.hasLines ? s.lines[1] : (qHalf_ ? "TIME" : nullptr);
  bool big = l1 != nullptr;
  if (big) {
    static const int GOLDC[3] = {255, 190, 0};
    RGB accent = col(npal_ > 1 ? pal_[1] : GOLDC, 1);
    if (tw(l1, F5, 2) <= W - 4) text(fb, (W - tw(l1, F5, 2)) >> 1, 16, l1, WHITE, F5, 2);
    else text(fb, (W - tw(l1, F3, 2)) >> 1, 18, l1, WHITE, F3, 2);
    if (tw(l2, F5, 2) <= W - 4) text(fb, (W - tw(l2, F5, 2)) >> 1, 33, l2, accent, F5, 2);
    else if (tw(l2, F5, 1) <= W - 4) text(fb, (W - tw(l2, F5, 1)) >> 1, 36, l2, accent, F5, 1);
    else text(fb, (W - tw(l2, F3, 1)) >> 1, 37, l2, accent, F3, 1);
    for (int x = 6; x < W - 6; x++) fb.unput(x, 30);
  } else {
    int lw = tw(s.label, F5, 2);
    if (lw <= W - 4) text(fb, (W - lw) >> 1, 17, s.label, WHITE, F5, 2);
    else text(fb, (W - tw(s.label, F5, 1)) >> 1, 20, s.label, WHITE, F5, 1);
  }
  auto scoreStr = [](const FxSide& sd, char (&o)[8]) {
    if (sd.hasScore) snprintf(o, sizeof(o), "%d", sd.score); else strcpy(o, "-");
  };
  if (!big) {
    const FxSide* rows[2] = {&s.away, &s.home};
    for (int i = 0; i < 2; i++) {
      int y = 36 + i * 11;
      char ab[4]; shortAbbr(ab, rows[i]->abbr);
      text(fb, 2, y + 2, ab, ledColor(rows[i]->hasColor, rows[i]->color), F3);
      char sc[8]; scoreStr(*rows[i], sc);
      text(fb, W - 1 - tw(sc, F5), y, sc, WHITE, F5);
    }
  } else {
    for (int bx = 0; bx < W; bx++) for (int by = 48; by < 58; by++) fb.put(bx, by, BLACK);
    char aA[4], hA[4], aS[8], hS[8];
    shortAbbr(aA, s.away.abbr); shortAbbr(hA, s.home.abbr);
    scoreStr(s.away, aS); scoreStr(s.home, hS);
    char aSp[16], hSp[16];
    snprintf(aSp, sizeof(aSp), " %s   ", aS);
    snprintf(hSp, sizeof(hSp), " %s", hS);
    int wA = tw(aA, F3), wAS = tw(aSp, F3), wH = tw(hA, F3), wHS = tw(hSp, F3);
    int x = (W - (wA + wAS + wH + wHS)) >> 1;
    text(fb, x, 50, aA, ledColor(s.away.hasColor, s.away.color), F3); x += wA;
    text(fb, x, 50, aSp, WHITE, F3); x += wAS;
    text(fb, x, 50, hA, ledColor(s.home.hasColor, s.home.color), F3); x += wH;
    text(fb, x, 50, hSp, WHITE, F3);
  }
}

// --------------------------------------------------------------- field goal
static const int POST_X = 54, CROSS_Y = 30, ARM_X = 6, TOP_Y = 9, GROUND_Y = 52;
static void drawGoalpost(Frame& fb, fr b) {
  RGB c = col3(255, 205, 40, b);
  for (int y = CROSS_Y; y <= GROUND_Y; y++) fb.put(POST_X, y, c);
  for (int x = POST_X - ARM_X; x <= POST_X + ARM_X; x++) fb.put(x, CROSS_Y, c);
  for (int y = TOP_Y; y <= CROSS_Y; y++) { fb.put(POST_X - ARM_X, y, c); fb.put(POST_X + ARM_X, y, c); }
}

void FxPlayer::drawFieldGoal(Frame& fb, fr el) {
  const FxSpec& s = *spec_;
  const int* A = pal_[0];
  drawGoalpost(fb, 1);
  if (el < 1200) {
    fr t = el / 1200;
    fr arc = sin(PI_ * t) * 10;
    for (int k = 6; k >= 1; k--) {
      fr tt = t - k * 0.035;
      if (tt < 0) continue;
      fr bx = 6 + tt * 48, by = GROUND_Y - tt * 34 - sin(PI_ * tt) * 10;
      fb.put(jr(bx), jr(by), col3(190, 95, 30, 0.5 - k * 0.06));
    }
    fr bx = 6 + t * 48, by = GROUND_Y - t * 34 - arc;
    sprite(fb, jr(bx) - 3, jr(by) - 2, BALL, 5, BROWN);
  } else if (el < 1450) {
    RGB c = col(C_WHITE, 1 - (el - 1200) / 250);
    for (int y = TOP_Y; y < CROSS_Y; y++)
      for (int x = POST_X - ARM_X; x < POST_X + ARM_X; x++) fb.put(x, y, c);
  } else {
    fr t = (el - 1450) / (DUR - 1450);
    fr show = std::min<fr>(1, t * 3);
    int w1 = tw("FIELD", F5), w2 = tw("GOAL", F5);
    text(fb, (W - w1) >> 1, 20, "FIELD", col(A, show), F5);
    text(fb, (W - w2) >> 1, 29, "GOAL", col(A, show), F5);
    if (s.label[0]) {
      int lw = tw(s.label, F3);
      text(fb, (W - lw) >> 1, 42, s.label, col(C_WHITE, show), F3);
    }
  }
}

// --------------------------------------------------------------------- flag
static const int FLIGHT_MS = 1000, TURF_Y = 53;
void FxPlayer::drawFlag(Frame& fb, fr el) {
  const FxSpec& s = *spec_;
  RGB bg = col3(255, 190, 0, 0.08 + 0.05 * sin(el / 140));
  for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) fb.put(x, y, bg);
  for (int x = 0; x < W; x++) fb.put(x, TURF_Y, rgb(20, 90, 30));
  const RGB CLOTH = rgb(255, 215, 0), KNOT = rgb(200, 120, 0);
  auto path = [](fr t, fr& x, fr& y) { x = 4 + t * 22; y = 46 - sin(PI_ * t) * 18; };
  if (el < FLIGHT_MS) {
    fr t = el / FLIGHT_MS;
    fr x0, y0;
    path(t, x0, y0);
    long fi = (long)floor(el / 75);
    if (t > 0.1) {
      fr px0, py0;
      path(t - 0.1, px0, py0);
      sprite(fb, jr(px0), jr(py0), tumble[(fi + 6 - 1) % 6], 7, rgb(110, 90, 0), rgb(80, 50, 0));
    }
    sprite(fb, jr(x0), jr(y0), tumble[fi % 6], 7, CLOTH, KNOT);
  } else {
    fr st = el - FLIGHT_MS;
    int hop = st < 180 ? jr(sin(PI_ * st / 180) * 2) : 0;
    sprite(fb, 25, TURF_Y - 7 - hop, FLAG_LANDED, 7, CLOTH, KNOT);
  }
  bool blink = (long)floor(el / 260) % 2 == 0;
  int w = tw("FLAG", F5, 2);
  text(fb, (W - w) >> 1, 2, "FLAG", blink ? WHITE : GOLD, F5, 2);
  if (s.team[0]) {
    char ab[5]; fullAbbr(ab, s.team);
    int onW = tw("ON ", F5), abW = tw(ab, F5);
    int x0 = (W - onW - abW) >> 1;
    text(fb, x0, 19, "ON", WHITE, F5);
    text(fb, x0 + onW, 19, ab, s.teamColor, F5);
  }
  if (s.label[0]) {
    int lw = tw(s.label, F3);
    if (lw <= W - 4) text(fb, (W - lw) >> 1, 56, s.label, WHITE, F3);
  }
}

// --------------------------------------------------------------- first down
void FxPlayer::drawFirstDown(Frame& fb, fr el) {
  const FxSpec& s = *spec_;
  const int* A = pal_[0];
  const int IN = 200, OUT = DUR - 300;
  fr b = 1;
  if (el < IN) b = el / IN;
  else if (el > OUT) b = std::max<fr>(0, 1 - (el - OUT) / 300);
  RGB g = col3(0, 230, 80, 0.9 * b);
  for (int x = 4; x < W - 4; x++) { fb.put(x, 22, g); fb.put(x, 46, g); }
  const char* txt = "1ST DOWN";
  int w2 = tw(txt, F3, 2);
  if (w2 <= W - 4) text(fb, (W - w2) >> 1, 28, txt, col(A, b), F3, 2);
  else text(fb, (W - tw(txt, F3)) >> 1, 30, txt, col(A, b), F3);
  if (s.label[0]) {
    int lw = tw(s.label, F3);
    text(fb, (W - lw) >> 1, 44, s.label, col(C_WHITE, b), F3);
  }
}

// ---------------------------------------------------------- hockey goal light
void FxPlayer::drawGoal(Frame& fb, fr el) {
  const FxSpec& s = *spec_;
  const int STROBE = 2000, CUT = 2200;
  static const int RED_[3] = {255, 20, 20};
  if (el < STROBE) {
    fr ph = fmod(el, 250) / 250;
    fr wash = std::max<fr>(0, 1 - fabs(ph - 0.5) * 2.4);
    RGB bg = col(RED_, 0.08 + 0.45 * wash);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) fb.put(x, y, bg);
    fr cx = 32, cy = 5, a0 = (el / 250) * PI_ * 2;
    for (int k = 0; k < 2; k++) {
      fr a = a0 + k * PI_;
      for (int r = 6; r < 64; r++) {
        int bx = jr(cx + cos(a) * r);
        int by = jr(cy + fabs(sin(a)) * r * 0.9);
        fb.put(bx, by, col3(255, 90, 60, std::max<fr>(0, 0.9 - r / 70.0)));
      }
    }
    sprite(fb, 27, 1, LAMP, 5, col(RED_, 0.55 + 0.45 * wash), rgb(110, 110, 110));
    int w = tw("GOAL", F5, 2);
    for (int bx = -2; bx < w + 2; bx++)
      for (int by = -2; by < 16; by++) fb.put(((W - w) >> 1) + bx, 30 + by, BLACK);
    text(fb, (W - w) >> 1, 30, "GOAL", wash > 0.35 ? WHITE : col(RED_, 1), F5, 2);
  } else if (el < CUT) {
    RGB c = col(C_WHITE, 1 - (el - STROBE) / (CUT - STROBE));
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) fb.put(x, y, c);
  } else {
    fr b = std::min<fr>(1, (el - CUT) / 300);
    fr pulse = 0.4 + 0.6 * ((long)floor(el / 250) % 2);
    RGB edge = col(RED_, pulse);
    for (int y = 0; y < H; y++) { fb.put(0, y, edge); fb.put(W - 1, y, edge); }
    RGB gc = ((long)floor(el / 250) % 2) ? WHITE : col(RED_, 1);
    if (s.logo.n) {
      int ly = std::max(1, (54 - s.logo.h) >> 1);
      drawLogo(fb, s.logo, (W - s.logo.w) >> 1, ly, b);
      int sw = tw("GOAL", F3);
      blackBox(fb, ((W - sw) >> 1) - 1, ((W - sw) >> 1) + sw, 57, 63);
      text(fb, (W - sw) >> 1, 58, "GOAL", gc, F3);
    } else {
      char ab[4]; shortAbbr(ab, s.label);
      int lw = tw(ab, F5, 2);
      text(fb, (W - lw) >> 1, 12, ab, col(pal_[0], b), F5, 2);
      int gw = tw("GOAL", F5, 2);
      text(fb, (W - gw) >> 1, 36, "GOAL", gc, F5, 2);
    }
  }
}

// ---------------------------------------------------------------------- run
void FxPlayer::drawRun(Frame& fb, fr el) {
  const FxSpec& s = *spec_;
  const int* A = pal_[0];
  const int OUT = DUR - 300;
  fr b = 1;
  if (el < 200) b = el / 200;
  else if (el > OUT) b = std::max<fr>(0, 1 - (el - OUT) / 300);
  RGB ln = col(A, 0.9 * b);
  for (int x = 4; x < W - 4; x++) { fb.put(x, 14, ln); fb.put(x, 48, ln); }
  fr t = std::min<fr>(1, el / 600);
  int bx = jr(-7 + t * 35);
  sprite(fb, bx, 4, BASEBALL, 7, col(C_WHITE, b), col3(220, 30, 30, b));
  int n = s.n < 1 ? 1 : s.n;
  const char* word = n > 1 ? "RUNS" : "RUN";
  int ww = tw(word, F5, 2);
  text(fb, (W - ww) >> 1, 20, word, col(A, b), F5, 2);
  char sub[32];
  if (n > 1) snprintf(sub, sizeof(sub), "%s +%d", s.label, n); else snprintf(sub, sizeof(sub), "%s", s.label);
  int sw = tw(sub, F3);
  text(fb, (W - sw) >> 1, 39, sub, col(C_WHITE, b), F3);
}

// ----------------------------------------------------------------- home run
// the preview's hash2: stable per-pixel "random" for the crowd, with
// JavaScript's 32-bit integer behaviour reproduced exactly
static inline int32_t toInt32(double d) { return (int32_t)(uint32_t)(int64_t)d; }
static fr hash2(int x, int y) {
  int32_t h = toInt32((double)x * 374761393.0 + (double)y * 668265263.0);
  int32_t m = h ^ (int32_t)((uint32_t)h >> 13);
  h = toInt32((double)m * 1274126177.0);
  uint32_t r = (uint32_t)(h ^ (int32_t)((uint32_t)h >> 16));
  return (fr)r / 4294967295.0;
}

static void drawPark(Frame& fb, fr el, fr b, fr cheer) {
  RGB sky = col3(8, 14, 40, b);
  for (int y = 0; y < 18; y++) for (int x = 0; x < W; x++) fb.put(x, y, sky);
  RGB tower = col3(255, 250, 220, b);
  for (int lx : {6, 57}) for (int dx = -1; dx <= 1; dx++) fb.put(lx + dx, 2, tower);
  static const int crowd[5][3] = {{200, 60, 60}, {60, 90, 200}, {220, 220, 220}, {200, 160, 60}, {60, 160, 90}};
  int shift = (int)(el / 120);
  for (int y = 18; y < 31; y++)
    for (int x = 0; x < W; x++) {
      fr r = hash2(x, y);
      if (r < 0.45) continue;
      const int* c = crowd[(int)(r * 997) % 5];
      bool flash = cheer > 0 && hash2(x + shift, y) > 1 - 0.35 * cheer;
      fb.put(x, y, col(c, b * (flash ? 1 : 0.28)));
    }
  RGB yel = col3(255, 205, 40, b), wall = col3(0, 70, 35, b);
  for (int x = 0; x < W; x++) {
    fb.put(x, 31, yel);
    for (int y = 32; y < 36; y++) fb.put(x, y, wall);
  }
  for (int y = 12; y < 36; y++) { fb.put(3, y, yel); fb.put(60, y, yel); }
  RGB g1 = col3(20, 105, 40, b), g2 = col3(14, 84, 30, b);
  for (int y = 36; y < H; y++) for (int x = 0; x < W; x++) fb.put(x, y, ((y - 36) >> 2) % 2 ? g1 : g2);
  RGB DIRT = col3(150, 92, 48, b), WH = col3(255, 255, 255, b);
  const int HOME[2] = {32, 61}, FIRST[2] = {43, 51}, SECOND[2] = {32, 41}, THIRD[2] = {21, 51};
  auto line = [&](const int* a, const int* c) {
    for (int i = 0; i <= 40; i++) {
      int x = jr(a[0] + (c[0] - a[0]) * i / 40.0), y = jr(a[1] + (c[1] - a[1]) * i / 40.0);
      fb.put(x, y, DIRT); fb.put(x, y + 1, DIRT);
    }
  };
  line(HOME, FIRST); line(FIRST, SECOND); line(SECOND, THIRD); line(THIRD, HOME);
  for (int y = 57; y < H; y++)
    for (int x = 27; x <= 37; x++)
      if ((x - 32) * (x - 32) + (y - 61) * (y - 61) * 1.6 <= 26) fb.put(x, y, DIRT);
  static const int M[5][2] = {{0, -1}, {-1, 0}, {0, 0}, {1, 0}, {0, 1}};
  for (auto& d : M) fb.put(32 + d[0], 51 + d[1], DIRT);
  RGB CHALK = col3(235, 235, 235, b * 0.9);
  const int poles[2][2] = {{3, 36}, {60, 36}};
  for (auto& p : poles)
    for (int i = 0; i <= 60; i++)
      fb.put(jr(HOME[0] + (p[0] - HOME[0]) * i / 60.0), jr(HOME[1] + (p[1] - HOME[1]) * i / 60.0), CHALK);
  const int* bases[3] = {FIRST, SECOND, THIRD};
  for (auto bs : bases) {
    int bx = bs[0], by = bs[1];
    fb.put(bx, by, WH); fb.put(bx + 1, by, WH); fb.put(bx, by - 1, WH); fb.put(bx + 1, by - 1, WH);
  }
  for (int x = 31; x <= 33; x++) fb.put(x, 61, WH);
  fb.put(32, 62, WH);
}

static void drawBatter(Frame& fb, fr el, const int* color) {
  RGB c = col(color, 1), wood = rgb(215, 165, 105);
  fb.put(28, 54, rgb(230, 190, 160));
  for (int y = 55; y <= 58; y++) fb.put(28, y, c);
  fb.put(27, 59, c); fb.put(29, 59, c); fb.put(27, 60, c); fb.put(29, 60, c);
  static const int B1[4][2] = {{29, 55}, {28, 53}, {27, 52}, {26, 51}};
  static const int B2[5][2] = {{29, 56}, {30, 56}, {31, 56}, {32, 56}, {33, 56}};
  static const int B3[4][2] = {{29, 55}, {30, 54}, {31, 53}, {32, 52}};
  if (el < 120) for (auto& p : B1) fb.put(p[0], p[1], wood);
  else if (el < 320) for (auto& p : B2) fb.put(p[0], p[1], wood);
  else for (auto& p : B3) fb.put(p[0], p[1], wood);
}

void FxPlayer::drawHomeRun(Frame& fb, fr el) {
  const FxSpec& s = *spec_;
  const int LOGO_AT = 3500;
  auto ballPos = [](fr t, fr& x, fr& y) { x = 33 + 11 * t; y = 56 - 61 * t + 10 * t * t; };
  if (el < LOGO_AT) {
    bool gone = el > 1800;
    fr cheer = el < 1100 ? 0 : std::min<fr>(1, (el - 1100) / 500);
    drawPark(fb, el, gone ? 0.55 : 1, cheer);
    if (!gone) drawBatter(fb, el, pal_[0]);
    if (el < 120) {
      fr t = el / 120;
      fb.put(32 + jr(t), 51 + jr(5 * t), WHITE);
    }
    if (el >= 120 && el < 420) {
      fr k = 1 - (el - 120) / 300;
      for (int r = 1; r <= 4; r++) {
        RGB c = col(C_WHITE, k * (1 - r / 5.0));
        fb.put(33 + r, 56, c); fb.put(33 - r, 56, c); fb.put(33, 56 - r, c);
        fb.put(33 + r, 56 - r, c); fb.put(33 - r, 56 - r, c);
      }
    }
    if (el >= 120 && el < 1800) {
      fr t = std::min<fr>(1, (el - 120) / 1680);
      for (int k = 6; k >= 1; k--) {
        fr tt = t - k * 0.03;
        if (tt < 0) continue;
        fr tx, ty;
        ballPos(tt, tx, ty);
        fb.put(jr(tx), jr(ty), col(C_WHITE, 0.85 - k * 0.11));
      }
      fr bx, by;
      ballPos(t, bx, by);
      int rx = jr(bx), ry = jr(by);
      if (ry < 33)
        for (int dy = -2; dy <= 2; dy++)
          for (int dx = -2; dx <= 2; dx++)
            if (abs(dx) + abs(dy) <= 3) fb.put(rx + dx, ry + dy, BLACK);
      if (t < 0.5) sprite(fb, rx - 1, ry - 1, HR_BALL, 3, WHITE);
      else { fb.put(rx, ry, WHITE); fb.put(rx + 1, ry, WHITE); fb.put(rx, ry + 1, WHITE); fb.put(rx + 1, ry + 1, WHITE); }
    }
    if (gone) {
      for (auto& p : parts_)
        if (p.life > 0) fb.put(jr(p.x), jr(p.y), col(p.c, std::min<fr>(1, p.life)));
      const char* l1 = s.grand ? "GRAND" : "HOME";
      const char* l2 = s.grand ? "SLAM" : "RUN";
      fr show = std::min<fr>(1, (el - 1800) / 250);
      RGB c = ((long)floor(el / 200) % 2) ? WHITE : col(pal_[0], 1);
      int w1 = tw(l1, F5, 2), w2 = tw(l2, F5, 2);
      for (int y = 21; y < 57; y++) for (int x = 2; x < W - 2; x++) fb.put(x, y, BLACK);
      text(fb, (W - w1) >> 1, 23, l1, show >= 1 ? c : col(C_WHITE, show), F5, 2);
      text(fb, (W - w2) >> 1, 40, l2, show >= 1 ? c : col(C_WHITE, show), F5, 2);
    }
  } else {
    fr b = std::min<fr>(1, (el - LOGO_AT) / 300);
    const char* banner = s.grand ? "GRAND SLAM" : "HOME RUN";
    RGB bc = ((long)floor(el / 200) % 2) ? WHITE : col(pal_[0], 1);
    if (s.logo.n) {
      drawLogo(fb, s.logo, (W - s.logo.w) >> 1, std::max(1, (54 - s.logo.h) >> 1), b);
      int sw = tw(banner, F3);
      blackBox(fb, ((W - sw) >> 1) - 1, ((W - sw) >> 1) + sw, 57, 63);
      text(fb, (W - sw) >> 1, 58, banner, bc, F3);
    } else {
      char ab[4]; shortAbbr(ab, s.label);
      int lw = tw(ab, F5, 2);
      text(fb, (W - lw) >> 1, 14, ab, col(pal_[0], b), F5, 2);
      int sw = tw(banner, F3);
      text(fb, (W - sw) >> 1, 38, banner, bc, F3);
    }
  }
}

// --------------------------------------------------------------- 3-pointer
static void drawCourt(Frame& fb, fr el, const int* color) {
  for (int y = 58; y < H; y++) for (int x = 0; x < W; x++) fb.put(x, y, rgb(120, 72, 32));
  for (int x = 0; x < W; x++) fb.put(x, 58, rgb(170, 110, 55));
  for (int y = 9; y < 58; y++) { fb.put(61, y, rgb(110, 110, 110)); fb.put(62, y, rgb(110, 110, 110)); }
  for (int x = 58; x < 61; x++) fb.put(x, 12, rgb(110, 110, 110));
  for (int y = 4; y < 22; y++) fb.put(57, y, WHITE);
  for (int x = 47; x <= 56; x++) fb.put(x, 18, rgb(255, 110, 20));
  fr kick = el > 1300 && el < 1900 ? sin((el - 1300) / 60) * 1.5 : 0;
  for (int i = 0; i <= 3; i++)
    for (int y = 19; y <= 26; y++) {
      fr t = (y - 19) / 7.0;
      int xl = jr(47 + i * 1 + t * 2 + kick * t);
      int xr = jr(56 - i * 1 - t * 2 + kick * t);
      if ((y + i) % 2 == 0) { fb.put(xl, y, rgb(230, 230, 230)); fb.put(xr, y, rgb(230, 230, 230)); }
    }
  RGB c = col(color, 1);
  fb.put(6, 43, rgb(230, 190, 160));
  for (int y = 44; y <= 50; y++) fb.put(6, y, c);
  fb.put(5, 51, c); fb.put(7, 51, c); fb.put(5, 52, c); fb.put(7, 52, c); fb.put(5, 53, c); fb.put(7, 53, c);
  for (int y = 54; y < 58; y++) { fb.put(5, y, c); fb.put(7, y, c); }
  if (el < 400) { fb.put(7, 42, c); fb.put(8, 41, c); fb.put(5, 42, c); }
  else { fb.put(7, 43, c); fb.put(8, 44, c); fb.put(5, 44, c); }
}

void FxPlayer::drawThree(Frame& fb, fr el) {
  const FxSpec& s = *spec_;
  auto pos = [](fr t, fr& x, fr& y) { x = 8 + 43.5 * t; y = (1 - t) * 40 + t * 16 - 23 * sin(PI_ * t); };
  const RGB ORANGE = rgb(235, 120, 30), SEAM = rgb(70, 35, 10);
  if (el < 1850) {
    drawCourt(fb, el, pal_[0]);
    if (el < 1300) {
      fr t = el / 1300;
      for (int k = 6; k >= 1; k--) {
        fr tt = t - k * 0.03;
        if (tt < 0) continue;
        fr tx, ty;
        pos(tt, tx, ty);
        fb.put(jr(tx), jr(ty), col3(255, 140, 40, 0.55 - k * 0.07));
      }
      fr bx, by;
      pos(t, bx, by);
      sprite(fb, jr(bx) - 2, jr(by) - 2, HOOP_BALL, 5, ORANGE, SEAM);
    } else if (el < 1650) {
      fr t = (el - 1300) / 350;
      sprite(fb, 50, jr(14 + t * 14), HOOP_BALL, 5, ORANGE, SEAM);
      for (int x = 46; x <= 56; x++) fb.put(x, 18, rgb(255, 110, 20));
    } else {
      RGB c = col(C_WHITE, 1 - (el - 1650) / 200);
      for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) fb.put(x, y, c);
    }
  } else if (el < 3500) {
    for (auto& p : parts_)
      if (p.life > 0) fb.put(jr(p.x), jr(p.y), col(p.c, std::min<fr>(1, p.life) * 0.7));
    RGB c1 = ((long)floor(el / 200) % 2) ? WHITE : col(pal_[0], 1);
    RGB c2 = col(npal_ > 1 ? pal_[1] : C_WHITE, 1);
    int w1 = tw("THREE", F5, 2), w2 = tw("POINTER", F5);
    for (int y = 10; y < 42; y++) for (int x = 3; x < W - 3; x++) fb.put(x, y, BLACK);
    text(fb, (W - w1) >> 1, 12, "THREE", c1, F5, 2);
    text(fb, (W - w2) >> 1, 32, "POINTER", c2, F5);
  } else {
    fr b = std::min<fr>(1, (el - 3500) / 300);
    const char* banner = "3 POINTER";
    RGB bc = ((long)floor(el / 200) % 2) ? WHITE : col(pal_[0], 1);
    if (s.logo.n) {
      drawLogo(fb, s.logo, (W - s.logo.w) >> 1, std::max(1, (54 - s.logo.h) >> 1), b);
      int sw = tw(banner, F3);
      blackBox(fb, ((W - sw) >> 1) - 1, ((W - sw) >> 1) + sw, 57, 63);
      text(fb, (W - sw) >> 1, 58, banner, bc, F3);
    } else {
      char ab[4]; shortAbbr(ab, s.label);
      int lw = tw(ab, F5, 2);
      text(fb, (W - lw) >> 1, 14, ab, col(pal_[0], b), F5, 2);
      int sw = tw(banner, F3);
      text(fb, (W - sw) >> 1, 38, banner, bc, F3);
    }
  }
}

// ------------------------------------------------------------ penalty text
static std::string up(const std::string& s) {
  std::string o(s);
  for (auto& c : o) c = (char)toupper((unsigned char)c);
  return o;
}

void parsePenalty(const char* txt, char (&team)[8], char (&foul)[20]) {
  std::string t = txt ? txt : "", U = up(t);
  team[0] = 0;
  const std::string key = "PENALTY ON ";
  // who: "PENALTY on XXX-" - 2 to 4 letters, then a dash or a space
  for (size_t p = U.find(key); p != std::string::npos; p = U.find(key, p + 1)) {
    size_t a = p + key.size(), b = a;
    while (b < U.size() && isalpha((unsigned char)U[b])) b++;
    size_t L = b - a;
    if (L >= 2 && L <= 4 && b < U.size() && (U[b] == '-' || isspace((unsigned char)U[b]))) {
      std::string w = U.substr(a, L);
      strncpy(team, w.c_str(), 7); team[7] = 0;
      break;
    }
  }
  // what: the part after the first comma following "PENALTY on ..."
  std::string f;
  bool got = false;
  for (size_t p = U.find(key); p != std::string::npos && !got; p = U.find(key, p + 1)) {
    size_t c = t.find(',', p + key.size());
    if (c == std::string::npos) continue;
    size_t a = c + 1;
    while (a < t.size() && isspace((unsigned char)t[a])) a++;
    size_t e = t.find(',', a);
    std::string cap = t.substr(a, e == std::string::npos ? std::string::npos : e - a);
    if (cap.empty()) continue;
    f = cap; got = true;
  }
  if (!got) {
    std::string line = t.substr(0, t.find('\n'));
    size_t p = up(line).rfind("PENALTY");
    std::string rest = t;
    if (p != std::string::npos) {
      size_t a = p + 7;
      while (a < t.size() && (t[a] == ',' || t[a] == ' ')) a++;
      rest = t.substr(a);
    }
    f = rest.substr(0, rest.find(','));
  }
  // upper case, single spaces, trimmed
  std::string g;
  bool sp = false;
  for (char ch : f) {
    if (isspace((unsigned char)ch)) { sp = true; continue; }
    if (sp && !g.empty()) g += ' ';
    sp = false;
    g += (char)toupper((unsigned char)ch);
  }
  if (g.rfind("OFFENSIVE ", 0) == 0) g = g.substr(10);
  else if (g.rfind("DEFENSIVE ", 0) == 0) g = g.substr(10);
  static const char* const SHORT[][2] = {
    {"PASS INTERFERENCE", "PASS INTERF"}, {"UNNECESSARY ROUGHNESS", "UNNEC ROUGH"},
    {"ROUGHING THE PASSER", "RUFF PASSER"}, {"ROUGHING THE KICKER", "RUFF KICKER"},
    {"UNSPORTSMANLIKE CONDUCT", "UNSPORTSMANLIKE"}, {"NEUTRAL ZONE INFRACTION", "NEUTRAL ZONE"},
    {"ILLEGAL BLOCK IN THE BACK", "BLOCK IN BACK"}, {"INTENTIONAL GROUNDING", "GROUNDING"},
    {"TOO MANY MEN ON THE FIELD", "TOO MANY MEN"}, {"TOO MANY MEN ON FIELD", "TOO MANY MEN"},
    {"ILLEGAL FORMATION", "ILLEGAL FORM"}, {"ILLEGAL USE OF HANDS", "ILLEGAL HANDS"},
    {"HORSE COLLAR TACKLE", "HORSE COLLAR"}, {"DELAY OF GAME", "DELAY OF GAME"},
    {"ILLEGAL CONTACT", "ILLEGAL CONTACT"}, {"LOWERING THE HEAD TO INITIATE CONTACT", "LOWER HEAD"}};
  for (auto& m : SHORT) if (g == m[0]) { g = m[1]; break; }
  while (g.size() > 15 && g.find(' ') != std::string::npos) g = g.substr(0, g.rfind(' '));
  if (g.size() > 15) g = g.substr(0, 15);
  strncpy(foul, g.c_str(), sizeof(foul) - 1);
  foul[sizeof(foul) - 1] = 0;
}

// ----------------------------------------------------------- board timing
// speed: animation ms per real ms; cut: real ms when the closing card takes
// over; total: real ms it all takes.
void FxPlayer::plan(fr& speed, uint32_t& cut, uint32_t& total) const {
  switch (spec_->kind) {
    case FX_TOUCHDOWN: case FX_HOMERUN: case FX_GOAL: case FX_THREE:
      speed = 0.85; cut = 5880; total = 8000; break;      // the whole animation, then hold the logo
    case FX_KICKOFF:
      speed = 0.85; cut = 5060; total = 8000; break;      // through KICK OFF, then both logos
    case FX_FIELDGOAL:
      speed = 1; cut = 3000; total = 6000; break;         // the kick and FIELD GOAL, then the logo
    case FX_FIRSTDOWN: case FX_RUN:
      speed = 1; cut = 2500; total = 6000; break;
    case FX_WIN:
      speed = 1; cut = 0; total = 6000; break;
    default:                                              // flag, quarter: the same, a bit slower
      speed = 5.0 / 6.0; cut = 6000; total = 6000; break;
  }
}

int FxPlayer::showMs() const {
  if (!spec_) return 0;
  fr sp; uint32_t c, t;
  plan(sp, c, t);
  return (int)t;
}

bool FxPlayer::show(Frame& fb, uint32_t ms) {
  if (!spec_) return false;
  fr speed; uint32_t cut, total;
  plan(speed, cut, total);
  if (ms >= total) { spec_ = nullptr; return false; }
  if (ms < cut) {
    fr el = std::min<fr>(ms * speed, DUR - 1);
    const FxSpec* keep = spec_;
    frame(fb, el);
    spec_ = keep;   // frame() lets go at the very end; the card still needs it
    return true;
  }
  drawCard(fb, ms - cut);
  return true;
}

void FxPlayer::drawCard(Frame& fb, uint32_t t) {
  const FxSpec& s = *spec_;
  fb.clear();
  fr b = std::min<fr>(1, t / 250.0);   // a quick fade in
  RGB blink = ((t / 130) % 2) ? rgb(255, 255, 255) : rgb(255, 190, 0);
  auto banner = [&](const char* word, int y, RGB c) {
    int sw = tw(word, F3);
    blackBox(fb, ((W - sw) >> 1) - 1, ((W - sw) >> 1) + sw, y - 1, y + 5);
    text(fb, (W - sw) >> 1, y, word, c, F3);
  };
  if (s.kind == FX_KICKOFF) {
    RGB ca = ledColor(s.away.hasColor, s.away.color), ch = ledColor(s.home.hasColor, s.home.color);
    if (s.awayLogo.n && s.homeLogo.n) {
      drawLogo(fb, s.awayLogo, 14 - (s.awayLogo.w >> 1), 12 + ((24 - s.awayLogo.h) >> 1), b);
      drawLogo(fb, s.homeLogo, 50 - (s.homeLogo.w >> 1), 12 + ((24 - s.homeLogo.h) >> 1), b);
    } else {
      text(fb, 14 - (tw(s.away.abbr, F3, 2) >> 1), 19, s.away.abbr, ca, F3, 2);
      text(fb, 50 - (tw(s.home.abbr, F3, 2) >> 1), 19, s.home.abbr, ch, F3, 2);
    }
    text(fb, (W - tw("AT", F3)) >> 1, 22, "AT", rgb(140, 140, 140), F3);
    int kw = tw("KICKOFF", F5);
    text(fb, (W - kw) >> 1, 47, "KICKOFF", (t / 400) % 2 ? rgb(255, 255, 255) : rgb(255, 190, 0), F5);
    return;
  }
  const char* word = s.kind == FX_TOUCHDOWN ? s.banner
                   : s.kind == FX_HOMERUN ? (s.grand ? "GRAND SLAM" : "HOME RUN")
                   : s.kind == FX_GOAL ? "GOAL"
                   : s.kind == FX_THREE ? "3 POINTER"
                   : s.kind == FX_FIELDGOAL ? "FIELD GOAL"
                   : s.kind == FX_FIRSTDOWN ? "1ST DOWN"
                   : s.kind == FX_WIN ? s.winText : "";
  char runWord[12];
  if (s.kind == FX_RUN) { snprintf(runWord, sizeof(runWord), s.n == 1 ? "RUN SCORES" : "%d RUNS", s.n); word = runWord; }
  const int* tc = npal_ ? pal_[0] : C_WHITE;
  if (s.logo.n) {
    drawLogo(fb, s.logo, (W - s.logo.w) >> 1, s.kind == FX_WIN ? 0 : std::max(1, (54 - s.logo.h) >> 1), b);
  } else {
    char ab[8]; strncpy(ab, s.label, 7); ab[7] = 0;
    int lw = tw(ab, F5, 2);
    text(fb, (W - lw) >> 1, 16, ab, col(tc, b), F5, 2);
  }
  if (s.kind == FX_WIN) {
    banner(word, 51, rgb(255, 190, 0));
    banner(s.scoreText, 58, rgb(255, 255, 255));
  } else {
    banner(word, s.logo.n ? 58 : 40, blink);
  }
}
