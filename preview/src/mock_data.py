"""
Mock game-state data, shaped exactly like the parsed output our real ESPN
fetch layer (fetch.py) produces. Used to preview the panel layout in
states that aren't happening live today (Wednesday, no games in progress).

Every dict here matches the schema documented at the top of fetch.py, so
swapping a mock dict for a real fetched one is a no-op for the renderer.
"""

# Team color/abbreviation reference (subset needed for previews)
TEAM_COLORS = {
    "NYG": {"primary": (11, 35, 79), "secondary": (163, 13, 45)},   # Giants blue/red
    "DAL": {"primary": (0, 34, 68), "secondary": (134, 147, 151)},  # Cowboys navy/silver
    "PHI": {"primary": (0, 76, 84), "secondary": (165, 172, 175)},
    "WSH": {"primary": (90, 20, 20), "secondary": (255, 182, 18)},
    "UF":  {"primary": (0, 33, 165), "secondary": (250, 70, 22)},   # Florida Gators
    "LSU": {"primary": (70, 29, 124), "secondary": (253, 208, 35)}, # LSU purple/gold
    "ALA": {"primary": (155, 12, 26), "secondary": (255, 255, 255)},
    "GA":  {"primary": (186, 12, 47), "secondary": (0, 0, 0)},
    "TEX": {"primary": (191, 87, 0), "secondary": (255, 255, 255)},
    "OSU": {"primary": (187, 0, 0), "secondary": (102, 102, 102)},
    # Rest of the NFL, for the ticker
    "CAR": {"primary": (0, 133, 202), "secondary": (16, 24, 32)},
    "CLE": {"primary": (255, 60, 0), "secondary": (49, 29, 0)},
    "LAC": {"primary": (0, 128, 198), "secondary": (255, 194, 14)},
    "BUF": {"primary": (0, 51, 141), "secondary": (198, 12, 48)},
    "NYJ": {"primary": (18, 87, 64), "secondary": (255, 255, 255)},
    "DET": {"primary": (0, 118, 182), "secondary": (176, 183, 188)},
    "HOU": {"primary": (3, 32, 47), "secondary": (167, 25, 48)},
    "IND": {"primary": (0, 44, 95), "secondary": (255, 255, 255)},
    "GB":  {"primary": (24, 48, 40), "secondary": (255, 184, 28)},
    "ATL": {"primary": (167, 25, 48), "secondary": (16, 24, 32)},
    "MIN": {"primary": (79, 38, 131), "secondary": (255, 198, 47)},
    "NO":  {"primary": (211, 188, 141), "secondary": (16, 24, 32)},
    "SEA": {"primary": (0, 34, 68), "secondary": (105, 190, 40)},
    "ARI": {"primary": (151, 35, 63), "secondary": (255, 255, 255)},
    "SF":  {"primary": (170, 0, 0), "secondary": (173, 153, 93)},
    "TEN": {"primary": (12, 35, 64), "secondary": (75, 146, 219)},
    "TENN": {"primary": (255, 130, 0), "secondary": (255, 255, 255)},
    "MICH": {"primary": (0, 39, 76), "secondary": (255, 203, 5)},
    "AUB": {"primary": (12, 35, 64), "secondary": (232, 119, 34)},
    "OLE": {"primary": (20, 0, 58), "secondary": (206, 17, 65)},
}


def nfl_sunday_slate():
    """A realistic full Sunday slate for the ticker - mix of finals,
    live games and later kickoffs, the way the panel would actually see
    it around 4pm on a Sunday."""
    return [
        {"away": "CAR", "home": "CLE", "status": "F", "score": "13-27"},
        {"away": "LAC", "home": "BUF", "status": "3RD", "clock": "8:41",
         "score": "10-14", "possession": "home", "redzone": True},
        {"away": "NYJ", "home": "DET", "status": "2ND", "clock": "2:00",
         "score": "7-13", "possession": "away", "fantasy": True},
        {"away": "HOU", "home": "IND", "status": "F", "score": "24-21"},
        {"away": "ATL", "home": "GB", "status": "4TH", "clock": "1:12",
         "score": "17-20", "possession": "away", "fantasy": True},
        {"away": "MIN", "home": "NO", "status": "1ST", "clock": "11:36",
         "score": "3-0", "possession": "home"},
        {"away": "SEA", "home": "ARI", "status": "SCHED", "score": None,
         "kick_time": "4:25P", "kick_date": "9/27"},
        {"away": "SF", "home": "TEN", "status": "SCHED", "score": None,
         "kick_time": "4:25P", "kick_date": "9/27"},
    ]

def nfl_pregame_giants():
    return {
        "league": "nfl",
        "state": "pre",
        "home": {"abbr": "GB", "score": None, "record": "2-0"},
        "away": {"abbr": "NYG", "score": None, "record": "1-1"},
        "pinned_side": "away",
        "kickoff_local": "Thu 8:15 PM",
        "odds": "GB -3.5  O/U 44.5",
        "venue": "Lambeau Field",
        "ticker_games": [
            {"away": "CAR", "home": "CLE", "status": "SCHED", "kick_time": "1:00P", "kick_date": "9/27"},
            {"away": "LAC", "home": "BUF", "status": "SCHED", "kick_time": "1:00P", "kick_date": "9/27"},
            {"away": "NYJ", "home": "DET", "status": "SCHED", "kick_time": "4:25P", "kick_date": "9/27"},
            {"away": "HOU", "home": "IND", "status": "SCHED", "kick_time": "8:20P", "kick_date": "9/27"},
        ],
    }

def nfl_live_giants():
    return {
        "league": "nfl",
        "state": "in",
        "home": {"abbr": "PHI", "score": 17, "record": "2-0", "timeouts": 2},
        "away": {"abbr": "NYG", "score": 20, "record": "1-1", "timeouts": 3},
        "pinned_side": "away",
        "clock": "4:12",
        "period": 4,
        "down_distance": "3rd & 6 at PHI 34",
        "possession": "away",
        "redzone": True,
        "last_play": "NYG - Jaxson Dart pass complete to Malik Nabers for 11 yards",
        "ticker_games": nfl_sunday_slate(),
    }

def nfl_final_giants():
    d = nfl_live_giants()
    d["state"] = "post"
    d["home"]["score"] = 20
    d["away"]["score"] = 27
    d["clock"] = "0:00"
    d["period"] = 4
    d.pop("down_distance", None)
    d.pop("redzone", None)
    d["last_play"] = "FINAL"
    return d

def nfl_live_cowboys():
    return {
        "league": "nfl",
        "state": "in",
        "home": {"abbr": "DAL", "score": 24, "record": "2-1", "timeouts": 1},
        "away": {"abbr": "WSH", "score": 21, "record": "1-2", "timeouts": 2},
        "pinned_side": "home",
        "clock": "1:47",
        "period": 4,
        "down_distance": "2nd & 3 at WSH 18",
        "possession": "home",
        "redzone": True,
        "last_play": "DAL - Dak Prescott pass complete to CeeDee Lamb for 14 yards",
        "ticker_games": [
            {"away": "CAR", "home": "CLE", "status": "F", "score": "13-27"},
            {"away": "LAC", "home": "BUF", "status": "Q3", "score": "10-14"},
            {"away": "NYJ", "home": "DET", "status": "Q2", "score": "7-13"},
        ],
    }

def cfb_live_gators():
    return {
        "league": "cfb",
        "state": "in",
        "home": {"abbr": "UF", "score": 24, "rank": 14, "record": "3-0"},
        "away": {"abbr": "TENN", "score": 20, "rank": 9, "record": "3-0"},
        "pinned_side": "home",
        "clock": "8:55",
        "period": 3,
        "down_distance": "1st & 10 at TENN 42",
        "possession": "home",
        "redzone": False,
        "last_play": "UF - DJ Lagway pass complete to Eugene Wilson III for 22 yards",
        "ticker_games": [
            {"rank_away": 1, "away": "OSU", "rank_home": None, "home": "MICH", "status": "2ND", "clock": "6:21", "score": "14-10", "possession": "away", "fantasy": False},
            {"rank_away": 4, "away": "ALA", "rank_home": None, "home": "AUB", "status": "F", "score": "31-17"},
            {"rank_away": None, "away": "LSU", "rank_home": 6, "home": "TEX", "status": "3RD", "clock": "4:02", "score": "17-20", "possession": "home", "redzone": True},
            {"rank_away": None, "away": "GA", "rank_home": 8, "home": "OLE", "status": "SCHED", "kick_time": "7:30P", "kick_date": "9/26"},
        ],
    }

def cfb_live_lsu():
    return {
        "league": "cfb",
        "state": "in",
        "home": {"abbr": "TEX", "score": 20, "rank": 6, "record": "3-0"},
        "away": {"abbr": "LSU", "score": 17, "rank": None, "record": "2-1"},
        "pinned_side": "away",
        "clock": "8:55",
        "period": 3,
        "down_distance": "3rd & 4 at LSU 45",
        "possession": "away",
        "redzone": False,
        "last_play": "LSU - Garrett Nussmeier pass complete to Aaron Anderson for 9 yards",
        "ticker_games": [
            {"rank_away": 1, "away": "OSU", "rank_home": None, "home": "MICH", "status": "Q2", "score": "14-10"},
            {"rank_away": 4, "away": "ALA", "rank_home": None, "home": "AUB", "status": "F", "score": "31-17"},
            {"rank_away": None, "away": "UF", "rank_home": 14, "home": "TENN", "status": "Q3", "score": "24-20"},
        ],
    }
