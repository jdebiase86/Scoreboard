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
  scopy(k2.lastPlay, "Brunson makes free throw 1 of 1"); k2.playScore = 1; expect("and-one: nothing", k, k2, FX_NONE);
  // 1.13: ESPN's last play has moved on by the time the board looks
  k2 = k; k2.home.score = 53; scopy(k2.lastPlay, "Hart defensive rebound"); k2.playScore = 0; expect("three (last play moved on)", k, k2, FX_THREE);
  k2 = k; k2.home.score = 55; scopy(k2.lastPlay, "Bridges makes 25-foot jumper"); k2.playScore = 3; expect("three (worth 3, more scored)", k, k2, FX_THREE);
  k2 = k; k2.home.score = 55; scopy(k2.lastPlay, "Towns makes layup"); k2.playScore = 2; expect("five, last a layup: nothing", k, k2, FX_NONE);
  k2 = k; k2.period = 2; expect("end 1st (hoops)", k, k2, FX_QUARTER, "END 1ST");
  const char* names[] = {"touchdown","fieldgoal","kickoff","quarter","halftime","flag","firstdown","goal","intermission","run","homerun","grandslam","three"};
  for (const char* n : names) { bool ok = testEvent(n, nullptr, f); if (!ok || f.kind == FX_NONE) bad++; printf("test %-12s kind %d label '%s' grand %d\n", n, f.kind, f.label, f.grand); }
  if (testEvent("nonsense", nullptr, f)) bad++;
  // sounds (1.11): which sound goes with each animation, close games, they scored, game start
  auto snd = [&](const char* what, SoundId got, SoundId want) {
    printf("%-34s sound %d %s\n", what, got, got == want ? "ok" : "WRONG");
    if (got != want) bad++;
  };
  testEvent("touchdown", nullptr, f); snd("touchdown sound", fxSound(f), SND_TOUCHDOWN);
  testEvent("grandslam", nullptr, f); snd("grand slam sound", fxSound(f), SND_GRANDSLAM);
  testEvent("homerun", nullptr, f); snd("home run sound", fxSound(f), SND_HOMERUN);
  testEvent("firstdown", nullptr, f); snd("first down: no sound", fxSound(f), SND_NONE);
  testEvent("kickoff", nullptr, f); scopy(f.label, "NYG"); snd("kickoff sound: whistle", fxSound(f), SND_WHISTLE);
  scopy(f.label, "FLA"); snd("Gators kickoff: fight song", fxSound(f), SND_GATORS);
  Game t1 = mk(FOOTBALL, ST_IN, 14, 10), t2 = t1; t2.away.score = 17;   // NYG (home) is yours
  snd("they scored (football)", soundEvent(t1, t2), SND_THEYSCORED);
  Game b1 = mk(BASKETBALL, ST_IN, 50, 48), b2 = b1; b2.away.score = 51;
  snd("they scored (basketball): nothing", soundEvent(b1, b2), SND_NONE);
  Game s1 = mk(HOCKEY, ST_PRE, 0, 0), s2 = mk(HOCKEY, ST_IN, 0, 0);
  snd("game starting (hockey)", soundEvent(s1, s2), SND_GAMESTART);
  Game f1 = mk(FOOTBALL, ST_PRE, 0, 0), f2 = mk(FOOTBALL, ST_IN, 0, 0);
  snd("game starting (football): kickoff", soundEvent(f1, f2), SND_NONE);
  Game cg = mk(FOOTBALL, ST_IN, 21, 17); cg.period = 4; scopy(cg.clock, "1:45");
  bool c1 = closeGame(cg); cg.home.score = 31; bool c2 = closeGame(cg); cg.home.score = 21; scopy(cg.clock, "2:30"); bool c3 = closeGame(cg);
  Game ch = mk(HOCKEY, ST_IN, 2, 2); ch.period = 3; scopy(ch.clock, "0:58"); bool c4 = closeGame(ch);
  printf("close game: 4th 1:45 by 4 %d, by 14 %d, 2:30 %d, hockey tied 0:58 %d\n", c1, c2, c3, c4);
  if (!c1 || c2 || c3 || !c4) bad++;
  // 1.13: basketball - last 3 minutes within three possessions, beating faster
  Game cb = mk(BASKETBALL, ST_IN, 102, 108); cb.period = 4; scopy(cb.clock, "1:32");
  bool b6 = closeGame(cb); cb.home.score = 98; bool b10 = closeGame(cb); cb.home.score = 102; scopy(cb.clock, "2:50"); bool b250 = closeGame(cb);
  scopy(cb.clock, "4:00"); bool b400 = closeGame(cb);
  scopy(cb.clock, "2:30"); uint32_t g1 = heartbeatGap(cb); scopy(cb.clock, "1:30"); uint32_t g2 = heartbeatGap(cb); scopy(cb.clock, "0:40"); uint32_t g3 = heartbeatGap(cb);
  printf("hoops close: down 6 at 1:32 %d, down 10 %d, 2:50 %d, 4:00 %d; beats every %u/%u/%u ms\n", b6, b10, b250, b400, g1, g2, g3);
  if (!b6 || b10 || !b250 || b400 || !(g1 > g2 && g2 > g3)) bad++;
  Game e1 = mk(BASKETBALL, ST_IN, 102, 108), e2 = mk(BASKETBALL, ST_POST, 102, 108);
  snd("final buzzer (basketball)", soundEvent(e1, e2), SND_BUZZER);
  Game e3 = mk(BASEBALL, ST_IN, 2, 3), e4 = mk(BASEBALL, ST_POST, 2, 3);
  snd("game over (baseball): no buzzer", soundEvent(e3, e4), SND_NONE);
  printf("%s\n", bad ? "EVENTS: FAILURES" : "events: all ok");
  return bad != 0;
}
