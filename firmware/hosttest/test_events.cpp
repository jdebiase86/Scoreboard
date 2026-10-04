// Host test: which animation each change in a game sets off
#include <stdio.h>
#include <stdlib.h>
#include "../scoreboard/sb_events.h"
void* sbAlloc(size_t n){return malloc(n);} void sbFree(void*p){free(p);} void sbBreathe(){}
static int bad = 0;
static Game mk(Sport sp, GState st, int mine, int other) {
  Game g; g.valid = true; g.sport = sp; g.state = st; scopy(g.eventId, "401");
  scopy(g.home.abbr, "NYG"); g.home.hasColor = true; g.home.color = 0x0B2265; g.home.hasScore = st != ST_PRE; g.home.score = mine;
  scopy(g.away.abbr, "DAL"); g.away.hasColor = true; g.away.color = 0x002A5C; g.away.hasScore = st != ST_PRE; g.away.score = other;
  g.pinnedHome = true; g.period = 1; return g;
}
static FxSpec f;
static void expect(const char* what, const Game& a, const Game& b, FxKind k, const char* label = nullptr) {
  FxKind got = detectEvent(a, b, f);
  bool ok = got == k && (!label || !strcmp(f.label, label));
  if (!ok) bad++;
  printf("%s %-34s kind %d (want %d) label '%s'\n", ok ? "ok " : "BAD", what, got, k, f.label);
}
int main() {
  Game a = mk(FOOTBALL, ST_IN, 7, 3), b = a;
  b.home.score = 14; expect("touchdown", a, b, FX_TOUCHDOWN, "NYG");
  b = a; b.home.score = 10; expect("field goal", a, b, FX_FIELDGOAL);
  b = a; b.home.score = 8; expect("extra point: nothing", a, b, FX_NONE);
  b = a; b.away.score = 10; expect("their touchdown: nothing", a, b, FX_NONE);
  Game p = mk(FOOTBALL, ST_PRE, 0, 0); b = mk(FOOTBALL, ST_IN, 0, 0); expect("kickoff", p, b, FX_KICKOFF);
  b = a; b.period = 2; expect("end 1st", a, b, FX_QUARTER, "END 1ST");
  Game a2 = a; a2.period = 2; b = a; b.period = 3; expect("halftime", a2, b, FX_QUARTER, "HALFTIME");
  b = a; scopy(b.lastPlay, "PENALTY on DAL-J.Smith, Defensive Pass Interference, 15 yards, enforced at NYG 40");
  expect("flag", a, b, FX_FLAG);
  printf("    flag team '%s' colour %06X\n", f.team, f.teamColor);
  Game c = a; c.possession = 1; scopy(c.downDistance, "3rd & 4 at NYG 30"); b = c; scopy(b.downDistance, "1st & 10 at NYG 36");
  expect("first down", c, b, FX_FIRSTDOWN);
  Game d = c; d.possession = 2; Game e = d; scopy(e.downDistance, "1st & 10 at DAL 20");
  expect("their first down: nothing", d, e, FX_NONE);
  b = a; scopy(b.eventId, "402"); b.home.score = 14; expect("different game: nothing", a, b, FX_NONE);
  Game h = mk(HOCKEY, ST_IN, 1, 0), h2 = h; h2.home.score = 2; expect("goal", h, h2, FX_GOAL);
  h2 = h; h2.intermission = true; scopy(h2.intLines[0], "1ST"); scopy(h2.intLines[1], "INTERMISSION");
  expect("intermission", h, h2, FX_QUARTER, "INTERMISSION");
  Game m = mk(BASEBALL, ST_IN, 2, 1), m2 = m; m2.home.score = 4; expect("run x2", m, m2, FX_RUN, "NYG");
  printf("    n=%d\n", f.n);
  scopy(m2.lastPlay, "Judge homers (40) on a fly ball to center field."); expect("home run", m, m2, FX_HOMERUN);
  m2.home.score = 6; scopy(m2.lastPlay, "Soto hits a grand slam (2) to right field."); expect("grand slam", m, m2, FX_HOMERUN);
  printf("    grand=%d\n", f.grand);
  Game k = mk(BASKETBALL, ST_IN, 50, 48), k2 = k; k2.home.score = 53; expect("three (no text)", k, k2, FX_THREE);
  k2 = k; k2.home.score = 52; expect("two: nothing", k, k2, FX_NONE);
  k2 = k; k2.home.score = 53; scopy(k2.lastPlay, "Brunson makes 26-foot three point jumper"); expect("three (text)", k, k2, FX_THREE);
  scopy(k2.lastPlay, "Brunson makes driving layup (and one)"); expect("and-one: nothing", k, k2, FX_NONE);
  k2 = k; k2.period = 2; expect("end 1st (hoops)", k, k2, FX_QUARTER, "END 1ST");
  const char* names[] = {"touchdown","fieldgoal","kickoff","quarter","halftime","flag","firstdown","goal","intermission","run","homerun","grandslam","three"};
  for (const char* n : names) { bool ok = testEvent(n, nullptr, f); if (!ok || f.kind == FX_NONE) bad++; printf("test %-12s kind %d label '%s' grand %d\n", n, f.kind, f.label, f.grand); }
  if (testEvent("nonsense", nullptr, f)) bad++;
  printf("%s\n", bad ? "EVENTS: FAILURES" : "events: all ok");
  return bad != 0;
}
