"""
Real ESPN scoreboard fetch + parse layer.

Endpoints (public, no key required, unofficial/undocumented):
  NFL: https://site.api.espn.com/apis/site/v2/sports/football/nfl/scoreboard
  CFB: https://site.api.espn.com/apis/site/v2/sports/football/college-football/scoreboard?groups=80&limit=300
       (groups=80 = FBS. groups=1 is the ACC, not FBS - that was a bug.
       Top-25 filtering is done client-side below since
       ESPN doesn't offer a "ranked only" query param)

Both return every game for the current week in one response. This module
parses that into the same flat dict shape used by mock_data.py, so the
renderer never has to know whether the data is real or a mock.

This will not run inside the sandbox shell (ESPN isn't on the egress
allowlist there) but works fine from the user's own computer or from
firmware, which is where it's meant to run anyway.
"""
import requests

NFL_URL = "https://site.api.espn.com/apis/site/v2/sports/football/nfl/scoreboard"
CFB_URL = "https://site.api.espn.com/apis/site/v2/sports/football/college-football/scoreboard?groups=80&limit=300"

HEADERS = {"User-Agent": "curl/8.0"}  # ESPN's edge rejects unfamiliar UAs


def _team_side(competitor):
    rank = competitor.get("curatedRank", {}).get("current")
    if rank == 99:
        rank = None
    return {
        "abbr": competitor["team"]["abbreviation"],
        "score": int(competitor["score"]) if competitor.get("score") not in (None, "") else None,
        "record": (competitor.get("records") or [{}])[0].get("summary"),
        "rank": rank,
        "timeouts": competitor.get("timeoutsUsed"),
    }


def _parse_event(event, pinned_abbr):
    comp = event["competitions"][0]
    competitors = comp["competitors"]
    home = next(c for c in competitors if c["homeAway"] == "home")
    away = next(c for c in competitors if c["homeAway"] == "away")

    status = comp["status"]
    state = status["type"]["state"]  # "pre" | "in" | "post"

    out = {
        "league": None,  # filled by caller
        "state": state,
        "home": _team_side(home),
        "away": _team_side(away),
        "pinned_side": "home" if home["team"]["abbreviation"] == pinned_abbr else "away",
        "kickoff_local": event["date"],
        "venue": comp.get("venue", {}).get("fullName"),
        "odds": None,
    }

    if comp.get("odds"):
        o = comp["odds"][0]
        out["odds"] = f"{o.get('details', '')}  O/U {o.get('overUnder', '')}"

    if state == "in":
        situation = comp.get("situation", {})
        out["clock"] = status.get("displayClock")
        out["period"] = status.get("period")
        out["down_distance"] = situation.get("downDistanceText")
        out["redzone"] = situation.get("isRedZone", False)
        possessor = situation.get("possession")
        if possessor == home["team"]["id"]:
            out["possession"] = "home"
        elif possessor == away["team"]["id"]:
            out["possession"] = "away"
        out["last_play"] = situation.get("lastPlay", {}).get("text")

    return out


def _find_team_game(events, abbr):
    for event in events:
        comp = event["competitions"][0]
        for c in comp["competitors"]:
            if c["team"]["abbreviation"] == abbr:
                return event
    return None


def _ticker_games(events, exclude_event_id, top25_only=False):
    games = []
    for event in events:
        if event["id"] == exclude_event_id:
            continue
        comp = event["competitions"][0]
        home = next(c for c in comp["competitors"] if c["homeAway"] == "home")
        away = next(c for c in comp["competitors"] if c["homeAway"] == "away")
        h_rank = home.get("curatedRank", {}).get("current")
        a_rank = away.get("curatedRank", {}).get("current")
        h_rank = None if h_rank in (None, 99) else h_rank
        a_rank = None if a_rank in (None, 99) else a_rank
        if top25_only and h_rank is None and a_rank is None:
            continue
        status = comp["status"]
        entry = {
            "home": home["team"]["abbreviation"],
            "away": away["team"]["abbreviation"],
            "rank_home": h_rank,
            "rank_away": a_rank,
            "status": status["type"]["shortDetail"],
        }
        if status["type"]["state"] in ("in", "post"):
            entry["score"] = f"{away.get('score', '')}-{home.get('score', '')}"
        if status["type"]["state"] == "in":
            possessor = comp.get("situation", {}).get("possession")
            if possessor == home["team"]["id"]:
                entry["possession"] = "home"
            elif possessor == away["team"]["id"]:
                entry["possession"] = "away"
        games.append(entry)
    return games


def fetch_nfl(team_abbr):
    """Fetch the current week's NFL scoreboard and return this team's
    game plus a ticker of the rest of the week's games."""
    r = requests.get(NFL_URL, headers=HEADERS, timeout=10)
    r.raise_for_status()
    data = r.json()
    events = data.get("events", [])
    event = _find_team_game(events, team_abbr)
    if event is None:
        return None
    game = _parse_event(event, team_abbr)
    game["league"] = "nfl"
    game["ticker_games"] = _ticker_games(events, event["id"])
    return game


def fetch_cfb(team_abbr):
    """Fetch the current week's college scoreboard and return this team's
    game plus a top-25-only ticker of the rest of the week's games."""
    r = requests.get(CFB_URL, headers=HEADERS, timeout=10)
    r.raise_for_status()
    data = r.json()
    events = data.get("events", [])
    event = _find_team_game(events, team_abbr)
    if event is None:
        return None
    game = _parse_event(event, team_abbr)
    game["league"] = "cfb"
    game["ticker_games"] = _ticker_games(events, event["id"], top25_only=True)
    return game
