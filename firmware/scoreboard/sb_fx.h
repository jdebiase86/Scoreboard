// The animations - a port of the browser preview's (scoreboard_live.py):
// touchdown, kickoff, end of quarter / halftime / intermission, field goal,
// flag, first down, goal light, run, home run, three-pointer. Checked frame
// by frame against the preview on a computer before every build.
//
// Everything except the flying sparks is a pure function of the time since
// the animation started. The sparks move one step per 1/60 s, as they do in
// the browser, so the board catches up step by step if a frame runs late.
#pragma once
#include <stdint.h>
#include <vector>
#include "sb_gfx.h"
#include "sb_logo.h"

typedef double fr;   // spark maths; double keeps it identical to the browser

enum FxKind : uint8_t {
  FX_NONE = 0, FX_TOUCHDOWN, FX_KICKOFF, FX_QUARTER, FX_FIELDGOAL, FX_FLAG,
  FX_FIRSTDOWN, FX_GOAL, FX_RUN, FX_HOMERUN, FX_THREE, FX_WIN
};

struct FxSide { char abbr[8] = ""; bool hasScore = false; int score = 0; bool hasColor = false; uint32_t color = 0; };

static const int FX_MAXPAL = 6;
static const int FX_LOGO_MAX = 54 * 54;

// Everything an animation needs, filled in when the event is detected
struct FxSpec {
  FxKind kind = FX_NONE;
  int pal[FX_MAXPAL][3];       // team palette (before lifting)
  int npal = 0;
  char label[20] = "";         // team abbreviation, foul, ...
  char banner[16] = "TOUCHDOWN";
  // quarter break
  bool hasLines = false;
  char lines[2][14] = {"", ""};
  FxSide away, home;
  // flag
  char team[8] = "";
  RGB teamColor = WHITE;
  // run / home run
  int n = 1;
  bool grand = false;
  // win card: "GIANTS WIN", "24-17"
  char winText[20] = "";
  char scoreText[12] = "";
  // celebration logo (54 px), may be empty
  Logo logo;
  LogoPix logoPix[FX_LOGO_MAX];
  // kickoff's ending: both teams' small logos, may be empty
  Logo awayLogo, homeLogo;
  LogoPix awayPix[26 * 24], homePix[26 * 24];
  void fixPointers() { logo.pix = logoPix; awayLogo.pix = awayPix; homeLogo.pix = homePix; }
};

// sparks live in PSRAM on the board: internal memory is kept for Wi-Fi/TLS
template <class T> struct FxAlloc {
  typedef T value_type;
  FxAlloc() = default;
  template <class U> FxAlloc(const FxAlloc<U>&) {}
  T* allocate(size_t n) { return (T*)sbAlloc(n * sizeof(T)); }
  void deallocate(T* p, size_t) { sbFree(p); }
  template <class U> bool operator==(const FxAlloc<U>&) const { return true; }
  template <class U> bool operator!=(const FxAlloc<U>&) const { return false; }
};

struct Spark { fr x, y, vx, vy, life, decay; int c[3]; };
struct Wave { fr cx, cy, t0; int c[3]; fr speed, max; };

class FxPlayer {
 public:
  // spec must stay alive while playing. seed: any number (sparks' randomness)
  void start(const FxSpec* spec, uint32_t seed);
  // Draws the frame el ms in; false once it's over. The animation itself,
  // exactly as the preview has it (5 s) - what the host tests compare.
  bool frame(Frame& fb, fr el);
  // What the board plays: the animation (a touch slower for the big ones)
  // and then a closing card - the team logo with the word under it, or
  // both logos after a kickoff. ms since it started; false once it's over.
  bool show(Frame& fb, uint32_t ms);
  int showMs() const;
  bool active() const { return spec_ != nullptr; }
  void stop() { spec_ = nullptr; }
  size_t sparks() const { return parts_.size(); }
  int durationMs() const;

 private:
  const FxSpec* spec_ = nullptr;
  int pal_[FX_MAXPAL][3];
  int npal_ = 0;
  std::vector<Spark, FxAlloc<Spark>> parts_;
  std::vector<Wave, FxAlloc<Wave>> waves_;
  fr nextBlast_ = 0;
  long ticks_ = 0;      // sparks steps taken
  uint32_t rng_ = 1;
  bool qHalf_ = false;

  fr rnd();
  void blast(fr cx, fr cy, fr power);
  void physics(fr el);   // one 1/60 s step at time el
  void drawTouchdown(Frame& fb, fr el);
  void drawKickoff(Frame& fb, fr el);
  void drawQuarter(Frame& fb, fr el);
  void drawFieldGoal(Frame& fb, fr el);
  void drawFlag(Frame& fb, fr el);
  void drawFirstDown(Frame& fb, fr el);
  void drawGoal(Frame& fb, fr el);
  void drawRun(Frame& fb, fr el);
  void drawHomeRun(Frame& fb, fr el);
  void drawThree(Frame& fb, fr el);
  void drawCard(Frame& fb, uint32_t ms);
  void plan(fr& speed, uint32_t& cut, uint32_t& total) const;
};

// "PENALTY on DAL-J.Smith, Defensive Pass Interference, 15 yards" ->
// team "DAL", foul "PASS INTERF" (15 characters at most)
void parsePenalty(const char* txt, char (&team)[8], char (&foul)[20]);
