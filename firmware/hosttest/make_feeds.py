"""Synthetic ESPN feeds built from a real saved event, and what the
preview's own parser (scoreboard_sim.parse_live) makes of each."""
import json, copy, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'preview'))
import scoreboard_sim as SB
base = json.load(open(os.path.join(HERE, 'feeds', 'scoreboard_last_event.json')))
os.makedirs('/tmp/feeds', exist_ok=True)

def ev(eid, away, home, state, sport_period=1, date="2026-10-03T23:05Z", short="10/3 - 7:05 PM EDT",
       ascore="0", hscore="0", sit=None, season=(2, "regular-season"), series=None, notes=None, ranks=(99, 99),
       detail=None, clock="12:07"):
    e = copy.deepcopy(base)
    e["id"] = eid; e["date"] = date
    e["season"] = {"year": 2027, "type": season[0], "slug": season[1]}
    c = e["competitions"][0]
    c["id"] = eid
    h = next(x for x in c["competitors"] if x["homeAway"] == "home")
    a = next(x for x in c["competitors"] if x["homeAway"] == "away")
    for side, (ab, nm, col, tid) in ((a, away), (h, home)):
        side["team"].update({"abbreviation": ab, "displayName": nm, "location": nm.split()[0],
                             "shortDisplayName": nm, "color": col, "id": tid,
                             "logo": f"https://a.espncdn.com/i/teamlogos/x/500/scoreboard/{ab.lower()}.png"})
    a["score"], h["score"] = ascore, hscore
    a["curatedRank"] = {"current": ranks[0]}; h["curatedRank"] = {"current": ranks[1]}
    c["status"] = {"displayClock": clock, "period": sport_period,
                   "type": {"state": state, "shortDetail": detail or (short if state == "pre" else "x"), "detail": detail or "x"}}
    if sit is not None: c["situation"] = sit
    else: c.pop("situation", None)
    if series is not None: c["series"] = series
    if notes is not None: c["notes"] = notes
    return e

NYR = ("NYR", "New York Rangers", "0056ae", "13"); DET = ("DET", "Detroit Red Wings", "e30526", "5")
BOS = ("BOS", "Boston Bruins", "fdb71a", "1"); TOR = ("TOR", "Toronto Maple Leafs", "003e7e", "21")
NYG = ("NYG", "New York Giants", "003c7f", "19"); DAL = ("DAL", "Dallas Cowboys", "002a5c", "6")
PHI = ("PHI", "Philadelphia Eagles", "06424d", "21"); WSH = ("WSH", "Washington Commanders", "5a1414", "28")
NYY = ("NYY", "New York Yankees", "132448", "10"); TB = ("TB", "Tampa Bay Rays", "092c5c", "30")
FLA = ("FLA", "Florida Gators", "0021a5", "57"); TENN = ("TENN", "Tennessee Volunteers", "ff8200", "2633")
UGA = ("UGA", "Georgia Bulldogs", "ba0c2f", "61"); MISS = ("MISS", "Ole Miss Rebels", "13294b", "145")

feeds = {}
feeds["hockey_live"] = ("NYR", "hockey", False, {"events": [
    ev("1", NYR, DET, "in", 2, ascore="2", hscore="1", detail="8:14 - 2nd", clock="8:14"),
    ev("2", BOS, TOR, "post", 3, ascore="3", hscore="2", date="2026-10-03T23:00Z"),
    ev("3", TOR, BOS, "pre", 0, date="2026-10-04T23:00Z", short="10/4 - 7:00 PM EDT")]})
feeds["hockey_int"] = ("NYR", "hockey", True, {"events": [
    ev("1", DET, NYR, "in", 4, ascore="2", hscore="2", detail="End of 3rd", season=(3, "post-season"),
       series={"summary": "NYR leads series 2-1", "totalCompetitions": 7, "title": "x",
               "competitors": [{"id": "13", "wins": 2}, {"id": "5", "wins": 1}]},
       notes=[{"headline": "East 1st Round - Game 4"}])]})
feeds["nfl_live"] = ("NYG", "football", False, {"events": [
    ev("1", DAL, NYG, "in", 3, ascore="17", hscore="21", clock="4:12",
       sit={"downDistanceText": "1st & Goal at DAL 4", "isRedZone": True, "possession": "19",
            "lastPlay": {"text": "PENALTY on DAL-J.Smith, Holding"}}),
    ev("2", PHI, WSH, "in", 2, ascore="7", hscore="3", clock="0:42",
       sit={"downDistanceText": "3rd & 2 at WSH 30", "isRedZone": False, "possession": "21"}),
    ev("3", WSH, PHI, "pre", 0, date="2026-10-05T17:00Z", short="10/5 - 1:00 PM EDT")]})
feeds["nfl_pre"] = ("NYG", "football", False, {"events": [
    ev("1", NYG, DAL, "pre", 0, date="2026-10-05T20:25Z", short="10/5 - 4:25 PM EDT"),
    ev("2", PHI, WSH, "post", 4, ascore="27", hscore="30", date="2026-09-28T17:00Z")]})
feeds["mlb_live"] = ("NYY", "baseball", False, {"events": [
    ev("1", TB, NYY, "in", 7, ascore="3", hscore="4", detail="Bot 7th",
       sit={"balls": 2, "strikes": 1, "outs": 1, "onFirst": True, "onSecond": False, "onThird": True},
       season=(3, "post-season"), notes=[{"headline": "American League Division Series - Game 2"}],
       series={"summary": "TB leads series 1-0", "totalCompetitions": 5,
               "competitors": [{"id": "10", "wins": 0}, {"id": "30", "wins": 1}]}),
    ev("2", BOS, TOR, "in", 5, ascore="1", hscore="1", detail="Top 5th", sit={"outs": 2})]})
feeds["mlb_mid"] = ("NYY", "baseball", False, {"events": [
    ev("1", NYY, TB, "in", 7, ascore="3", hscore="4", detail="Middle 7th", sit={})]})
feeds["mlb_post_old"] = ("NYY", "baseball", False, {"events": [
    ev("1", NYY, TB, "post", 9, ascore="5", hscore="4", date="2026-09-28T17:05Z", detail="Final")]})
feeds["cfb_ranked"] = ("FLA", "football", True, {"events": [
    ev("1", TENN, FLA, "in", 3, ascore="17", hscore="21", ranks=(8, 12), clock="4:12",
       sit={"downDistanceText": "3rd & 7 at TENN 34", "isRedZone": False, "possession": "57"}),
    ev("2", MISS, UGA, "in", 2, ascore="14", hscore="10", ranks=(14, 3), clock="9:40",
       sit={"possession": "145"}),
    ev("3", NYG, DAL, "post", 4, ascore="1", hscore="2"),
    ev("4", UGA, MISS, "pre", 0, ranks=(3, 99), date="2026-10-04T23:30Z", short="10/4 - 7:30 PM EDT")]})
feeds["cfb_bowl_pre"] = ("FLA", "football", True, {"events": [
    ev("1", FLA, UGA, "pre", 0, ranks=(12, 3), season=(3, "post-season"), date="2027-01-01T20:00Z",
       short="1/1 - 3:00 PM EST", notes=[{"headline": "Capital One Orange Bowl presented by Capital One"}])]})

out = {}
for name, (team, sport, top25, feed) in feeds.items():
    json.dump(feed, open(f"/tmp/feeds/{name}.json", "w"))
    g = SB.parse_live(feed, team, sport=sport, top25=top25)
    out[name] = {"team": team, "sport": sport, "top25": top25, "game": g}
json.dump(out, open("/tmp/feeds/expected.json", "w"), indent=1)
print(len(out), "feeds")
