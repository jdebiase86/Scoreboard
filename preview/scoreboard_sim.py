#!/usr/bin/env python3
"""
Scoreboard panel simulator - runs right in your Terminal.

    python3 scoreboard_sim.py              demo game (fake Giants game)
    python3 scoreboard_sim.py --live       real live data from ESPN
    python3 scoreboard_sim.py --team DAL   your father-in-law's board
    python3 scoreboard_sim.py --college    college Saturday mode

Press Control-C to quit.

Every pixel you see here is exactly what the 64x64 LED panel will show -
same layout, same fonts, same colors. Nothing here needs installing;
it only uses what comes with Python.
"""
import re
import sys
import time
import json
import argparse
import urllib.request

W, H = 64, 64

# ---------------------------------------------------------------- colors
WHITE = (255, 255, 255)
DIM = (120, 120, 120)
GRAY = (90, 90, 90)
GOLD = (255, 190, 0)
CLOCK = (235, 235, 235)   # the time itself - reads first
DATE = (140, 140, 140)    # the date / period label sitting behind it
RED = (255, 40, 40)
GREEN = (0, 230, 80)
BROWN = (190, 95, 30)
OFF = (26, 26, 26)

TEAM_COLORS = {
    # NFL
    "ARI": (151, 35, 63),   "ATL": (167, 25, 48),  "BAL": (26, 25, 95),
    "BUF": (0, 51, 141),    "CAR": (0, 133, 202),  "CHI": (11, 22, 42),
    "CIN": (251, 79, 20),   "CLE": (255, 60, 0),   "DAL": (0, 34, 68),
    "DEN": (251, 79, 20),   "DET": (0, 118, 182),  "GB": (24, 48, 40),
    "HOU": (3, 32, 47),     "IND": (0, 44, 95),    "JAX": (0, 103, 120),
    "KC": (227, 24, 55),    "LAC": (0, 128, 198),  "LAR": (0, 53, 148),
    "LV": (165, 172, 175),  "MIA": (0, 142, 151),  "MIN": (79, 38, 131),
    "NE": (0, 34, 68),      "NO": (211, 188, 141), "NYG": (11, 35, 79),
    "NYJ": (18, 87, 64),    "PHI": (0, 76, 84),    "PIT": (255, 182, 18),
    "SEA": (0, 34, 68),     "SF": (170, 0, 0),     "TB": (213, 10, 10),
    "TEN": (12, 35, 64),    "WSH": (90, 20, 20),
    # College
    "UF": (0, 33, 165),     "LSU": (70, 29, 124),  "TENN": (255, 130, 0),
    "ALA": (155, 12, 26),   "UGA": (186, 12, 47),  "GA": (186, 12, 47),
    "TEX": (191, 87, 0),    "OSU": (187, 0, 0),    "MICH": (0, 39, 76),
    "AUB": (12, 35, 64),    "OLE": (20, 0, 58),    "ND": (12, 35, 64),
    "USC": (153, 27, 30),   "ORE": (0, 79, 57),    "PSU": (4, 30, 66),
    "OU": (132, 22, 23),    "CLEM": (245, 102, 0), "MISS": (20, 0, 58),
}


def led_color(rgb, hexcolor=None):
    """Navy and forest-green team colors are invisible on a real panel.
    Lift anything too dark until it actually lights up.

    ESPN ships a color with every team, so in live mode all 130-odd FBS
    schools get their real colors instead of falling back to white."""
    if hexcolor:
        rgb = hex_rgb(hexcolor) or rgb
    if not rgb:
        return WHITE
    m = max(rgb)
    if m == 0:
        return WHITE
    if m < 140:
        f = 190.0 / m
        return tuple(min(255, int(c * f)) for c in rgb)
    return rgb


# ------------------------------------------------------------- the fonts
F5 = {
    "0": ["01110", "10001", "10011", "10101", "11001", "10001", "01110"],
    "1": ["00100", "01100", "00100", "00100", "00100", "00100", "01110"],
    "2": ["01110", "10001", "00001", "00010", "00100", "01000", "11111"],
    "3": ["11111", "00010", "00100", "00010", "00001", "10001", "01110"],
    "4": ["00010", "00110", "01010", "10010", "11111", "00010", "00010"],
    "5": ["11111", "10000", "11110", "00001", "00001", "10001", "01110"],
    "6": ["00110", "01000", "10000", "11110", "10001", "10001", "01110"],
    "7": ["11111", "00001", "00010", "00100", "01000", "01000", "01000"],
    "8": ["01110", "10001", "10001", "01110", "10001", "10001", "01110"],
    "9": ["01110", "10001", "10001", "01111", "00001", "00010", "01100"],
    "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    "B": ["11110", "10001", "10001", "11110", "10001", "10001", "11110"],
    "C": ["01110", "10001", "10000", "10000", "10000", "10001", "01110"],
    "D": ["11100", "10010", "10001", "10001", "10001", "10010", "11100"],
    "E": ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
    "F": ["11111", "10000", "10000", "11110", "10000", "10000", "10000"],
    "G": ["01110", "10001", "10000", "10111", "10001", "10001", "01111"],
    "H": ["10001", "10001", "10001", "11111", "10001", "10001", "10001"],
    "I": ["01110", "00100", "00100", "00100", "00100", "00100", "01110"],
    "J": ["00111", "00010", "00010", "00010", "00010", "10010", "01100"],
    "K": ["10001", "10010", "10100", "11000", "10100", "10010", "10001"],
    "L": ["10000", "10000", "10000", "10000", "10000", "10000", "11111"],
    "M": ["10001", "11011", "10101", "10101", "10001", "10001", "10001"],
    "N": ["10001", "10001", "11001", "10101", "10011", "10001", "10001"],
    "O": ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
    "P": ["11110", "10001", "10001", "11110", "10000", "10000", "10000"],
    "Q": ["01110", "10001", "10001", "10001", "10101", "10010", "01101"],
    "R": ["11110", "10001", "10001", "11110", "10100", "10010", "10001"],
    "S": ["01111", "10000", "10000", "01110", "00001", "00001", "11110"],
    "T": ["11111", "00100", "00100", "00100", "00100", "00100", "00100"],
    "U": ["10001", "10001", "10001", "10001", "10001", "10001", "01110"],
    "V": ["10001", "10001", "10001", "10001", "10001", "01010", "00100"],
    "W": ["10001", "10001", "10001", "10101", "10101", "11011", "10001"],
    "X": ["10001", "10001", "01010", "00100", "01010", "10001", "10001"],
    "Y": ["10001", "10001", "01010", "00100", "00100", "00100", "00100"],
    "Z": ["11111", "00001", "00010", "00100", "01000", "10000", "11111"],
    " ": ["00000"] * 7,
    "/": ["00001", "00010", "00010", "00100", "01000", "01000", "10000"],
    "&": ["01100", "10010", "10100", "01000", "10101", "10010", "01101"],
    ":": ["00000", "00100", "00100", "00000", "00100", "00100", "00000"],
    "#": ["01010", "01010", "11111", "01010", "11111", "01010", "01010"],
    "-": ["00000", "00000", "00000", "11111", "00000", "00000", "00000"],
    "@": ["01110", "10001", "10111", "10101", "10111", "10000", "01110"],
    ".": ["00000", "00000", "00000", "00000", "00000", "00100", "00100"],
}

F3 = {
    "0": ["111", "101", "101", "101", "111"],
    "1": ["010", "110", "010", "010", "111"],
    "2": ["111", "001", "111", "100", "111"],
    "3": ["111", "001", "111", "001", "111"],
    "4": ["101", "101", "111", "001", "001"],
    "5": ["111", "100", "111", "001", "111"],
    "6": ["111", "100", "111", "101", "111"],
    "7": ["111", "001", "001", "001", "001"],
    "8": ["111", "101", "111", "101", "111"],
    "9": ["111", "101", "111", "001", "111"],
    "A": ["111", "101", "111", "101", "101"],
    "B": ["110", "101", "110", "101", "110"],
    "C": ["111", "100", "100", "100", "111"],
    "D": ["110", "101", "101", "101", "110"],
    "E": ["111", "100", "111", "100", "111"],
    "F": ["111", "100", "111", "100", "100"],
    "G": ["111", "100", "101", "101", "111"],
    "H": ["101", "101", "111", "101", "101"],
    "I": ["111", "010", "010", "010", "111"],
    "J": ["001", "001", "001", "101", "111"],
    "K": ["101", "101", "110", "101", "101"],
    "L": ["100", "100", "100", "100", "111"],
    "M": ["101", "111", "111", "101", "101"],
    "N": ["101", "111", "111", "111", "101"],
    "O": ["111", "101", "101", "101", "111"],
    "P": ["111", "101", "111", "100", "100"],
    "Q": ["111", "101", "101", "111", "001"],
    "R": ["111", "101", "111", "110", "101"],
    "S": ["111", "100", "111", "001", "111"],
    "T": ["111", "010", "010", "010", "010"],
    "U": ["101", "101", "101", "101", "111"],
    "V": ["101", "101", "101", "101", "010"],
    "W": ["101", "101", "111", "111", "101"],
    "X": ["101", "101", "010", "101", "101"],
    "Y": ["101", "101", "010", "010", "010"],
    "Z": ["111", "001", "010", "100", "111"],
    " ": ["000"] * 5,
    "/": ["001", "001", "010", "100", "100"],
    "&": ["010", "101", "010", "101", "011"],
    ":": ["000", "010", "000", "010", "000"],
    "#": ["101", "111", "101", "111", "101"],
    "-": ["000", "000", "111", "000", "000"],
    "@": ["111", "101", "111", "100", "111"],
    ".": ["000", "000", "000", "000", "010"],
}

FOOTBALL = ["0011100", "0111110", "1112111", "0111110", "0011100"]


def tw(text, font, scale=1, spacing=1):
    if not text:
        return 0
    gw = len(next(iter(font.values()))[0])
    return len(text) * (gw * scale + spacing) - spacing


def text(px, x, y, s, color, font, scale=1, spacing=1):
    gw = len(next(iter(font.values()))[0])
    for ch in str(s).upper():
        g = font.get(ch)
        if g is None:
            x += gw * scale + spacing
            continue
        for ry, row in enumerate(g):
            for rx, bit in enumerate(row):
                if bit != "1":
                    continue
                for sy in range(scale):
                    for sx in range(scale):
                        px[(x + rx * scale + sx, y + ry * scale + sy)] = color
        x += gw * scale + spacing
    return x


def sprite(px, x, y, rows, color, color2=WHITE):
    for ry, row in enumerate(rows):
        for rx, bit in enumerate(row):
            if bit == "0":
                continue
            px[(x + rx, y + ry)] = color if bit == "1" else color2


# ------------------------------------------------------------ the layout
def short_down(dd, left_w):
    """Squeeze down & distance into whatever room is left on the top line.
    "1st & Goal" spelled out collides with the clock, so Goal becomes G."""
    if not dd:
        return ""
    s = dd.split(" at ")[0].upper().replace(" ", "").replace("GOAL", "G")
    while s and left_w + 2 + tw(s, F3) > W:
        s = s[:-1]
    return s


def fit_scale(abbr, score_w):
    return 2 if tw(abbr, F5, 2) + 9 + score_w <= 63 else 1


def short_abbr(abbr):
    """Both team rows must render at the same size or the board looks
    lopsided. A four-letter abbreviation (TENN, MISS) can't fit beside a
    two-digit score at the large size, so it gets clipped to three."""
    return (abbr or "")[:3]


def team_row(px, y, abbr, score, color, has_ball, row_h=14,
             record=None, rank=None):
    abbr = short_abbr(abbr)
    """Before kickoff the score slot carries the team's win-loss record,
    or its ranking if that's all we have. If ESPN gives us neither, the
    slot stays empty - better than a dash that says nothing."""
    if score is None:
        if record:
            txt, scale = str(record), 1
        elif rank:
            txt, scale = f"#{rank}", 1
        else:
            txt, scale = "", 1
    else:
        txt, scale = str(score), 2
    sw = tw(txt, F5, scale)
    # size the abbreviation against a full score, so the layout doesn't
    # jump when the game kicks off
    sc = fit_scale(abbr, tw("00", F5, 2))
    text(px, 1, y + (row_h - 7 * sc) // 2, abbr, color, F5, sc)
    sx = W - 1 - sw
    text(px, sx, y + (row_h - 7 * scale) // 2, txt, WHITE, F5, scale)
    if has_ball:
        sprite(px, sx - 9, y + (row_h - 5) // 2, FOOTBALL, BROWN)


def draw_main(px, g):
    pinned_home = g["pinned_side"] == "home"
    top, bot = (g["home"], g["away"]) if pinned_home else (g["away"], g["home"])
    poss = g.get("possession")
    top_ball = poss is not None and ((poss == "home") == pinned_home)
    bot_ball = poss is not None and not top_ball
    st = g["state"]

    if st == "post":
        t = "FINAL"
        text(px, (W - tw(t, F3)) // 2, 1, t, GOLD, F3)
    elif st == "pre":
        text(px, 2, 1, g.get("kickoff_local", ""), CLOCK, F3)
    else:
        q = {1: "1ST", 2: "2ND", 3: "3RD", 4: "4TH"}.get(
            g.get("period", 1), "OT")
        left_txt = f"{q} {g.get('clock','')}"
        text(px, 2, 1, left_txt, CLOCK, F3)
        r = short_down(g.get("down_distance"), tw(left_txt, F3))
        if r:
            text(px, W - 2 - tw(r, F3), 1, r,
                 RED if g.get("redzone") else GRAY, F3)

    team_row(px, 7, top["abbr"], top.get("score"),
             led_color(TEAM_COLORS.get(top["abbr"]), top.get("color")),
             top_ball, record=top.get("record"), rank=top.get("rank"))
    team_row(px, 22, bot["abbr"], bot.get("score"),
             led_color(TEAM_COLORS.get(bot["abbr"]), bot.get("color")),
             bot_ball, record=bot.get("record"), rank=bot.get("rank"))


def draw_ticker_block(px, y, g):
    a_s = h_s = None
    if g.get("score"):
        a_s, h_s = g["score"].split("-")
    stat = g.get("status", "")
    if stat == "F":
        rt, rb = "FINAL", ""
    elif g.get("score") is None:
        # Not started: kickoff time on the top line, date underneath
        rt, rb = g.get("kick_time", ""), g.get("kick_date", "")
    else:
        rt, rb = stat, g.get("clock", "")

    for i, (abbr, sc, is_home, hexc) in enumerate(
            [(g["away"], a_s, False, g.get("away_color")),
             (g["home"], h_s, True, g.get("home_color"))]):
        ry = y + i * 6
        text(px, 3, ry, abbr, led_color(TEAM_COLORS.get(abbr), hexc), F3)
        if sc is not None:
            text(px, 21, ry, sc, WHITE, F3)
        p = g.get("possession")
        if p and ((p == "home") == is_home):
            sprite(px, 31, ry, FOOTBALL, BROWN)

    # A time reads bright; a date or period label sits behind it.
    scheduled = g.get("score") is None and g.get("status") != "F"
    if rt:
        c = GREEN if rt == "FINAL" else (CLOCK if scheduled else DATE)
        text(px, W - 3 - tw(rt, F3), y, rt, c, F3)
    if rb:
        text(px, W - 3 - tw(rb, F3), y + 6, rb, DATE if scheduled else CLOCK, F3)

    bar = RED if g.get("redzone") else (GREEN if g.get("fantasy") else None)
    if bar:
        for yy in range(y, y + 11):
            px[(0, yy)] = bar
            px[(W - 1, yy)] = bar


def render(g, pair=0):
    px = {}
    draw_main(px, g)
    # Main divider, your game vs the ticker. 105 grey rather than a
    # near-black value: low PWM levels are where these panels get
    # unreliable and start to flicker.
    for x in range(W):
        px[(x, 37)] = (105, 105, 105)
    games = g.get("ticker_games", [])
    if games:
        i = (pair * 2) % len(games)
        draw_ticker_block(px, 39, games[i % len(games)])
        # Divider between the two ticker games, running under the teams,
        # scores and football only - it stops short of the clock column
        # so it isn't cutting across the whole board.
        for x in range(2, 39):
            px[(x, 51)] = (105, 105, 105)
        draw_ticker_block(px, 53, games[(i + 1) % len(games)])
    return px


# ------------------------------------------------------- terminal output
def to_terminal_blocks(px):
    """One LED per two spaces, colored with the BACKGROUND color.

    Nothing is drawn with a font glyph here, so there are no seams: a
    terminal fills a cell's background across the full line height,
    including the leading between lines. Needs a 128-column window.
    """
    out = []
    for y in range(H):
        line = []
        prev = None
        for x in range(W):
            c = px.get((x, y), OFF)
            if c != prev:
                line.append(f"\x1b[48;2;{c[0]};{c[1]};{c[2]}m")
                prev = c
            line.append("  ")
        out.append("".join(line) + "\x1b[0m")
    return "\n".join(out)


def to_terminal_half(px):
    """Compact fallback: two panel rows per line using a half-block.
    Fits a 64-column window, but some terminals leave a hairline gap
    between rows because of line spacing."""
    out = []
    for y in range(0, H, 2):
        line = []
        for x in range(W):
            t = px.get((x, y), OFF)
            b = px.get((x, y + 1), OFF)
            line.append(f"\x1b[38;2;{t[0]};{t[1]};{t[2]}m"
                        f"\x1b[48;2;{b[0]};{b[1]};{b[2]}m▀")
        out.append("".join(line) + "\x1b[0m")
    return "\n".join(out)


def to_terminal(px, mode="auto"):
    import shutil
    if mode == "half":
        return to_terminal_half(px)
    if mode == "blocks":
        return to_terminal_blocks(px)
    cols, rows = shutil.get_terminal_size((80, 24))
    if cols >= 129 and rows >= 66:
        return to_terminal_blocks(px)
    return to_terminal_half(px)


def size_hint():
    """If the window is too small for the clean rendering, say so."""
    import shutil
    cols, rows = shutil.get_terminal_size((80, 24))
    if cols >= 129 and rows >= 66:
        return None
    return (f"  Window is {cols}x{rows}. For the sharp version, press "
            f"Command and the minus key a few times (need 129x66).")


# ----------------------------------------------------------- demo data
def demo_slate():
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
        {"away": "SEA", "home": "ARI", "status": "SCHED",
         "kick_time": "4:25P", "kick_date": "9/27"},
        {"away": "SF", "home": "TEN", "status": "SCHED",
         "kick_time": "4:25P", "kick_date": "9/27"},
    ]


def demo_game(team="NYG"):
    opp = {"NYG": "PHI", "DAL": "WSH", "UF": "TENN", "LSU": "TEX"}.get(team, "PHI")
    return {
        "state": "in",
        "home": {"abbr": opp, "score": 17},
        "away": {"abbr": team, "score": 20},
        "pinned_side": "away",
        "clock": "4:12",
        "period": 4,
        "down_distance": "3rd & 6 at PHI 34",
        "possession": "away",
        "redzone": True,
        "ticker_games": demo_slate(),
    }


# ------------------------------------------------------------ live data
NFL_URL = "https://site.api.espn.com/apis/site/v2/sports/football/nfl/scoreboard"
CFB_URL = ("https://site.api.espn.com/apis/site/v2/sports/football/"
           "college-football/scoreboard?groups=80&limit=300")
MLB_URL = "https://site.api.espn.com/apis/site/v2/sports/baseball/mlb/scoreboard"
NHL_URL = "https://site.api.espn.com/apis/site/v2/sports/hockey/nhl/scoreboard"
NBA_URL = "https://site.api.espn.com/apis/site/v2/sports/basketball/nba/scoreboard"
# ESPN's hockey and basketball feeds don't say who's on a power play or
# who's in the bonus. The leagues' own live feeds do, so those two get a
# second, small lookup while the pinned game is live.
NHL_LIVE_URL = "https://api-web.nhle.com/v1/score/now"
NBA_LIVE_URL = ("https://cdn.nba.com/static/json/liveData/scoreboard/"
                "todaysScoreboard_00.json")

# Priority order for "auto" mode: football always wins when it's live,
# then baseball, then hockey, then basketball. Within a tier, whichever
# entry is live wins; across tiers, the earliest live tier wins. When
# nothing's live, whichever of these starts soonest gets shown instead.
AUTO_PRIORITY = [
    ("football", NFL_URL, "NYG", False),
    ("football", CFB_URL, "UF", True),
    ("baseball", MLB_URL, "NYY", False),
    ("hockey", NHL_URL, "NYR", False),
    ("basketball", NBA_URL, "NYK", False),
]


def board_date():
    """Today's date as ESPN wants it (YYYYMMDD), in New York time, rolling
    over at 5am instead of midnight so a West Coast game that runs past
    midnight still belongs to the day it started."""
    import datetime as _dt
    try:
        from zoneinfo import ZoneInfo
        now = _dt.datetime.now(ZoneInfo("America/New_York"))
    except Exception:
        now = _dt.datetime.now()
    return (now - _dt.timedelta(hours=5)).strftime("%Y%m%d")


def dated(url):
    """ESPN's baseball, hockey and basketball feeds don't reliably default
    to today - in the 2026 MLB postseason the undated feed was still
    serving the last regular-season day, so a live Yankees playoff game
    never showed. Ask for today explicitly. Football is left alone: its
    feed is organised by week, and the undated one already gets that right."""
    if any(s in url for s in ("/baseball/", "/hockey/", "/basketball/")) \
            and "dates=" not in url:
        return url + ("&" if "?" in url else "?") + "dates=" + board_date()
    return url


def fetch(url):
    url = dated(url)
    req = urllib.request.Request(url, headers={"User-Agent": "curl/8.0"})
    with urllib.request.urlopen(req, timeout=15) as r:
        return json.loads(r.read().decode())


def _score_of(competitor, state):
    """A score only counts once the game is live or done."""
    if state == "pre":
        return None
    v = competitor.get("score")
    if v in (None, ""):
        return None
    try:
        return int(v)
    except (TypeError, ValueError):
        return None


def _record_of(competitor):
    """Dig out a W-L record.

    ESPN is not consistent about this between the NFL and college feeds:
    sometimes it's records[] with a "total"/"overall" entry, sometimes a
    bare "record", sometimes it's on the team object, and sometimes it
    just isn't there. Try each, and accept only something that actually
    looks like a record."""
    def ok(v):
        v = (v or "").strip()
        return v if v and v[0].isdigit() and "-" in v else None

    recs = competitor.get("records")
    if isinstance(recs, list):
        for want in ("total", "overall", "ytd"):
            for r in recs:
                if isinstance(r, dict) and str(r.get("type", "")).lower() == want:
                    if ok(r.get("summary")):
                        return ok(r.get("summary"))
        for r in recs:
            if isinstance(r, dict) and ok(r.get("summary")):
                return ok(r.get("summary"))
    if isinstance(recs, dict) and ok(recs.get("summary")):
        return ok(recs.get("summary"))
    if ok(competitor.get("record")):
        return ok(competitor.get("record"))
    t = competitor.get("team") or {}
    tr = t.get("record")
    if isinstance(tr, dict):
        items = tr.get("items") or []
        if items and ok(items[0].get("summary")):
            return ok(items[0].get("summary"))
    if ok(tr):
        return ok(tr)
    return None


def _logo_of(competitor):
    """ESPN's logo image for this team.

    The scoreboard feed gives a light-background logo; there's a matching
    dark-background version at the same path with "-dark" on the size
    folder, which is what you want on a black panel. The server tries
    that first and falls back."""
    t = competitor.get("team") or {}
    logos = t.get("logos")
    if isinstance(logos, list):
        for l in logos:
            if isinstance(l, dict) and "dark" in (l.get("rel") or []) and l.get("href"):
                return l["href"]
        for l in logos:
            if isinstance(l, dict) and l.get("href"):
                return l["href"]
    return t.get("logo")


def _rank_of(competitor):
    """College teams carry an AP/CFP rank; 99 means unranked."""
    r = (competitor.get("curatedRank") or {}).get("current")
    if isinstance(r, int) and 1 <= r <= 25:
        return r
    return None


def hex_rgb(h):
    h = (h or "").lstrip("#")
    if len(h) != 6:
        return None
    try:
        return tuple(int(h[i:i+2], 16) for i in (0, 2, 4))
    except ValueError:
        return None


def split_kickoff(short_detail):
    """ESPN gives scheduled games as '9/27 - 1:00 PM EDT'.
    Return ('9/27', '1:00P') - short enough to fit the ticker."""
    if " - " not in short_detail:
        return "", short_detail[:5]
    date_s, rest = short_detail.split(" - ", 1)
    parts = rest.split()
    t = parts[0] if parts else ""
    ampm = parts[1][:1] if len(parts) > 1 else ""
    return date_s.strip(), (t + ampm)


# ESPN's abbreviations don't always match what a fan would type. Florida
# is FLA in their data, not UF. Rather than guess, match loosely against
# every name ESPN gives for a team.
# Only real identifiers - never mascot names. "Tigers" as an alias for
# LSU matched Auburn, whose team name is also Tigers, and the board
# happily showed Auburn's logo.
ALIASES = {
    "UF": ["FLA", "FLORIDA"],
    "FLA": ["UF", "FLORIDA"],
    "LSU": ["LOUISIANA STATE"],
    "NYG": ["NEW YORK GIANTS"],
    "DAL": ["DALLAS COWBOYS"],
    "NYY": ["NEW YORK YANKEES", "YANKEES"],
    "NYR": ["NEW YORK RANGERS", "RANGERS"],
    # ESPN's own abbreviation for the Knicks is just "NY"; "Knicks" is
    # unique enough as a mascot name (unlike "Tigers") to match on safely.
    "NYK": ["NEW YORK KNICKS", "KNICKS", "NY"],
}


def team_matches(competitor, want):
    """True if this competitor is the team the user asked for, matching
    on abbreviation or any of ESPN's name fields."""
    t = competitor["team"]
    want = want.upper()
    # "name" is the mascot (Tigers, Gators, Giants) and is shared by many
    # schools, so it is deliberately not matched on.
    names = {str(t.get(k, "")).upper() for k in
             ("abbreviation", "location", "shortDisplayName", "displayName")}
    names.discard("")
    if want in names:
        return True
    for alt in ALIASES.get(want, []):
        if alt.upper() in names:
            return True
    return False


# Football and basketball call it a quarter, hockey a period, baseball
# an inning - and the overtime word changes too. Doesn't attempt
# top/bottom-of-the-inning for baseball; that's a known simplification.
def period_label(sport, period, postseason=False):
    """Each sport's own words. Hockey: periods, then OT; a regular-season
    tie goes to a shootout (SO), but playoff hockey never does - it keeps
    playing overtimes (OT, 2OT, 3OT...). Football and basketball: quarters,
    then OT, 2OT... Baseball: just the inning number."""
    if not period:
        return ""
    if sport == "hockey":
        if period <= 3:
            return {1: "1ST", 2: "2ND", 3: "3RD"}[period]
        if not postseason:
            return "OT" if period == 4 else "SO"
        return "OT" if period == 4 else f"{period - 3}OT"
    if sport == "baseball":
        return f"{period}"
    if period <= 4:
        return {1: "1ST", 2: "2ND", 3: "3RD", 4: "4TH"}[period]
    return "OT" if period == 5 else f"{period - 4}OT"


def intermission_lines(period, postseason=False):
    """Title for the hockey break card after a period ends. Hockey has
    intermissions, not halftime: two of them in regulation, then the break
    before overtime."""
    if period == 1:
        return ["1ST", "INTERMISSION"]
    if period == 2:
        return ["2ND", "INTERMISSION"]
    if period == 3:
        return ["END OF", "REGULATION"]
    return ["END OF", period_label("hockey", period, postseason)]


def _baseball_state(status, sit):
    """Inning, half, count, outs and runners for a live baseball game.

    ESPN spells the inning out in the status text ("Top 5th", "Bot 5th",
    "Mid 5th", "End 5th"), which is more reliable than rebuilding it from
    a period number. Count, outs and runners come from the situation
    block, which only exists mid-game - every read here tolerates it
    being missing."""
    out = {}
    detail = ((status.get("type") or {}).get("shortDetail")
              or (status.get("type") or {}).get("detail") or "")
    words = detail.upper().replace("BOTTOM", "BOT").replace("MIDDLE", "MID").split()
    if words and words[0] in ("TOP", "BOT", "MID", "END"):
        out["half"] = words[0]
        num = "".join(ch for ch in (words[1] if len(words) > 1 else "") if ch.isdigit())
        ordinal = words[1] if len(words) > 1 else ""
        out["inning_text"] = f"{words[0]} {ordinal}".strip()
        out["inning_short"] = f"{words[0][0]}{num}" if num else words[0]
    else:
        n = status.get("period")
        out["inning_text"] = str(n) if n else ""
        out["inning_short"] = str(n) if n else ""

    def num_of(*keys):
        for k in keys:
            v = sit.get(k)
            if isinstance(v, (int, float)):
                return int(v)
            if isinstance(v, str) and v.isdigit():
                return int(v)
        return None

    out["balls"] = num_of("balls")
    out["strikes"] = num_of("strikes")
    out["outs"] = num_of("outs")
    out["bases"] = [bool(sit.get("onFirst")), bool(sit.get("onSecond")),
                    bool(sit.get("onThird"))]
    return out


def enrich_hockey(g, fetch_fn, team):
    """Power play / penalty kill and intermissions for the pinned hockey
    game, from the NHL's own live feed. Sets g["pp"] = {"side":
    "home"|"away", "time": "1:23"} only while one side has the man
    advantage, and g["intermission"] / g["intermission_left"] during a break.

    The NHL feed adds a "situation" block to a game only during special
    teams, with each side's skater strength and a situationCode of four
    digits: away goalie, away skaters, home skaters, home goalie."""
    g["pp"] = None
    if g.get("state") != "in":
        return
    try:
        data = fetch_fn(NHL_LIVE_URL)
    except Exception:
        return
    mine_abbr = g["home"]["abbr"] if g["pinned_side"] == "home" else g["away"]["abbr"]
    want = {team.upper(), str(mine_abbr).upper()}
    for gm in data.get("games", []):
        ha = str((gm.get("homeTeam") or {}).get("abbrev", "")).upper()
        aa = str((gm.get("awayTeam") or {}).get("abbrev", "")).upper()
        if ha not in want and aa not in want:
            continue
        # Intermission: the NHL's clock says so outright, and counts down
        # the break. ESPN's status text is the fallback (set in parse_live).
        clk = gm.get("clock") or {}
        if clk.get("inIntermission"):
            g["intermission"] = True
            t = str(clk.get("timeRemaining") or "")
            g["intermission_left"] = t[1:] if t.startswith("0") and len(t) == 5 else t
        elif "running" in clk or "timeRemaining" in clk:
            # the NHL feed is authoritative when it has the game
            g["intermission"] = False
            g["intermission_left"] = ""
        if g.get("intermission"):
            return                      # no power-play chip during a break
        sit = gm.get("situation") or {}
        if not sit:
            return
        side = None
        hs = (sit.get("homeTeam") or {})
        as_ = (sit.get("awayTeam") or {})
        if "PP" in (hs.get("situationDescriptions") or []):
            side = "home"
        elif "PP" in (as_.get("situationDescriptions") or []):
            side = "away"
        else:
            h_n, a_n = hs.get("strength"), as_.get("strength")
            code = str(sit.get("situationCode") or "")
            if (h_n is None or a_n is None) and len(code) == 4 and code.isdigit():
                a_n, h_n = int(code[1]), int(code[2])
            if isinstance(h_n, int) and isinstance(a_n, int) and h_n != a_n:
                side = "home" if h_n > a_n else "away"
        if side is None:
            return
        # The NHL's home/away and ESPN's home/away are the same game, so
        # the sides line up directly.
        t = str(sit.get("timeRemaining") or "")
        if t.startswith("0") and len(t) == 5:
            t = t[1:]
        g["pp"] = {"side": side, "time": t}
        return


def enrich_basketball(g, fetch_fn, team):
    """Bonus state for the pinned basketball game, from the NBA's own live
    feed. Sets g["bonus"] = {"home": bool, "away": bool}."""
    g["bonus"] = None
    if g.get("state") != "in":
        return
    try:
        data = fetch_fn(NBA_LIVE_URL)
    except Exception:
        return
    mine_abbr = g["home"]["abbr"] if g["pinned_side"] == "home" else g["away"]["abbr"]
    want = {team.upper(), str(mine_abbr).upper()}
    games = ((data.get("scoreboard") or {}).get("games")) or data.get("games") or []
    for gm in games:
        ht, at = gm.get("homeTeam") or {}, gm.get("awayTeam") or {}
        if str(ht.get("teamTricode", "")).upper() not in want and \
           str(at.get("teamTricode", "")).upper() not in want:
            continue
        def on(v):
            return str(v).strip() in ("1", "true", "True")
        g["bonus"] = {"home": on(ht.get("inBonus")), "away": on(at.get("inBonus"))}
        return


def enrich(g, fetch_fn, team):
    """Extra live detail that ESPN's feed doesn't carry. Never raises -
    a failed lookup just means the badge doesn't show."""
    try:
        if g.get("sport") == "hockey":
            enrich_hockey(g, fetch_fn, team)
        elif g.get("sport") == "basketball":
            enrich_basketball(g, fetch_fn, team)
    except Exception:
        pass
    return g


def _is_postseason(ev):
    st = (ev.get("season") or {}).get("type")
    slug = str((ev.get("season") or {}).get("slug") or "")
    return st == 3 or "post" in slug


def _local_md(iso):
    """ESPN's UTC game time -> ("10/1", is_today) in New York time, using
    the same 5am rollover as board_date()."""
    if not iso:
        return "", True
    import datetime as _dt
    try:
        # ESPN sends both "2026-10-01T23:05Z" and "...T23:05:00Z"
        t = _dt.datetime.fromisoformat(iso.replace("Z", "+00:00"))
        try:
            from zoneinfo import ZoneInfo
            t = t.astimezone(ZoneInfo("America/New_York"))
        except Exception:
            t = t - _dt.timedelta(hours=4)
        day = (t - _dt.timedelta(hours=5)).strftime("%Y%m%d")
        return f"{t.month}/{t.day}", day == board_date()
    except Exception:
        return "", True


def _short_round(txt):
    """ESPN's round text, squeezed into the 15 characters the status line
    holds: "American League Division Series - Game 2" -> "ALDS GAME 2"."""
    t = re.sub(r"\s+", " ", str(txt or "")).upper().strip()
    # Football has single games, not series: NFL rounds and college bowls /
    # CFP rounds need their own short names
    if "PLAYOFF" in t and ("COLLEGE" in t or "CFP" in t):
        for k, v in (("NATIONAL CHAMPIONSHIP", "CFP TITLE GAME"), ("SEMIFINAL", "CFP SEMIFINAL"),
                     ("QUARTERFINAL", "CFP QUARTERS"), ("FIRST ROUND", "CFP 1ST ROUND")):
            if k in t:
                return v
    if "NATIONAL CHAMPIONSHIP" in t:
        return "NATL TITLE GAME"
    if "BOWL" in t and "SUPER BOWL" not in t:
        words = t.replace("PRESENTED BY", "|").split("|")[0].split()
        i = max(j for j, w in enumerate(words) if w == "BOWL")
        return " ".join(words[max(0, i - 1):i + 1])          # "ORANGE BOWL"
    t = re.sub(r"\b(AFC|NFC) (WILD CARD|DIVISIONAL) PLAYOFFS?\b", r"\1 \2", t)
    t = re.sub(r"\b(AFC|NFC) CHAMPIONSHIP\b", r"\1 TITLE GAME", t)
    for a, b in (("AMERICAN LEAGUE ", "AL"), ("NATIONAL LEAGUE ", "NL"),
                 ("AL DIVISION SERIES", "ALDS"), ("NL DIVISION SERIES", "NLDS"),
                 ("AL CHAMPIONSHIP SERIES", "ALCS"), ("NL CHAMPIONSHIP SERIES", "NLCS"),
                 ("ALDIVISION SERIES", "ALDS"), ("NLDIVISION SERIES", "NLDS"),
                 ("ALCHAMPIONSHIP SERIES", "ALCS"), ("NLCHAMPIONSHIP SERIES", "NLCS"),
                 ("WILD CARD SERIES", "WILD CARD"),
                 ("EASTERN CONFERENCE", "EAST"), ("WESTERN CONFERENCE", "WEST"),
                 ("FIRST ROUND", "RD1"), ("SECOND ROUND", "RD2"),
                 ("CONFERENCE FINALS", "CONF FINAL"), ("CONFERENCE FINAL", "CONF FINAL"),
                 ("STANLEY CUP FINAL", "CUP FINAL"), (" - ", " ")):
        t = t.replace(a, b)
    if len(t) > 15:
        t = t.replace("GAME ", "G").replace("ROUND", "RD").replace("CONFERENCE", "CONF")
    while len(t) > 15 and " " in t:
        t = t[:t.rfind(" ")]
    return t[:15]


def _short_summary(txt, total=None):
    """ "TB leads series 1-0" -> "TB LEADS 1-0"; "Series tied 1-1" stays.
    Before game 1 ESPN says "Series starts 10/3" - the date's already on the
    board, so say how long the series is instead: "BEST OF 5"."""
    t = re.sub(r"\s+", " ", str(txt or "")).upper().strip()
    if t.startswith("SERIES STARTS") or t.startswith("SERIES BEGINS"):
        return f"BEST OF {total}" if total else ""
    if not t.startswith("SERIES"):
        t = t.replace(" SERIES ", " ")
    while len(t) > 15 and " " in t:
        t = t[:t.rfind(" ")]
    return t[:15]


def _playoff(ev, c, h, a):
    """Round, series standing and each side's series wins for a postseason
    game. Reads ESPN's competitions[0].series and notes; anything missing
    just leaves that piece out."""
    if not _is_postseason(ev) and not c.get("series"):
        return None
    ser = c.get("series") or {}
    notes = c.get("notes") or []
    head = next((n.get("headline") for n in notes if isinstance(n, dict) and n.get("headline")), "")
    rnd = _short_round(head or ser.get("title") or "PLAYOFFS")
    wins = {}
    for comp in ser.get("competitors") or []:
        if not isinstance(comp, dict):
            continue
        cid = str(comp.get("id"))
        if cid == str(h["team"].get("id")):
            wins["home"] = comp.get("wins")
        elif cid == str(a["team"].get("id")):
            wins["away"] = comp.get("wins")
    return {"round": rnd, "summary": _short_summary(ser.get("summary"), ser.get("totalCompetitions")),
            "wins": wins if len(wins) == 2 else None}


def parse_live(data, team, sport="football", top25=False):
    events = data.get("events", [])

    def sides(ev):
        c = ev["competitions"][0]
        h = next(x for x in c["competitors"] if x["homeAway"] == "home")
        a = next(x for x in c["competitors"] if x["homeAway"] == "away")
        return c, h, a

    mine = None
    for ev in events:
        c, h, a = sides(ev)
        if team_matches(h, team) or team_matches(a, team):
            mine = ev
            break
    if mine is None:
        return None

    c, h, a = sides(mine)
    stt = c["status"]
    state = stt["type"]["state"]
    post = _is_postseason(mine)
    g = {
        "state": state,
        "sport": sport,
        "home": {"abbr": h["team"]["abbreviation"],
                 "score": _score_of(h, state),
                 "color": h["team"].get("color"),
                 "color2": h["team"].get("alternateColor"),
                 "logo": _logo_of(h),
                 "record": _record_of(h),
                 "rank": _rank_of(h)},
        "away": {"abbr": a["team"]["abbreviation"],
                 "score": _score_of(a, state),
                 "color": a["team"].get("color"),
                 "color2": a["team"].get("alternateColor"),
                 "logo": _logo_of(a),
                 "record": _record_of(a),
                 "rank": _rank_of(a)},
        "pinned_side": "home" if team_matches(h, team) else "away",
        "kickoff_local": " ".join(
            x for x in split_kickoff(stt["type"].get("shortDetail", "")) if x),
        "clock": stt.get("displayClock"),
        "period": stt.get("period"),
        "period_label": period_label(sport, stt.get("period"), post),
        "postseason": post,
    }
    # Not today's game (an off day shows the next game, or the last one):
    # say which day, so "FINAL" isn't mistaken for tonight's result.
    g["playoff"] = _playoff(mine, c, h, a)
    # Preseason / exhibition: ESPN season type 1 ("preseason")
    sea = mine.get("season") or {}
    g["preseason"] = sea.get("type") == 1 or "pre" in str(sea.get("slug") or "")
    md, is_today = _local_md(mine.get("date"))
    g["game_date"] = md
    if state == "post":
        g["final_label"] = "FINAL" if is_today or not md else f"FINAL {md}"
    if state == "in" and sport == "football":
        s = c.get("situation", {})
        g["down_distance"] = s.get("downDistanceText")
        g["redzone"] = s.get("isRedZone", False)
        p = s.get("possession")
        if p == h["team"]["id"]:
            g["possession"] = "home"
        elif p == a["team"]["id"]:
            g["possession"] = "away"
        # Only present on some ESPN feeds (seen on the summary endpoint,
        # not always on the plain scoreboard one) - the flag animation
        # is a no-op if this never shows up as non-empty, so it's safe
        # to read speculatively rather than block on it.
        lp = s.get("lastPlay") or {}
        g["last_play_text"] = lp.get("text") or ""
    elif state == "in":
        # Other sports: no down/possession, but the same speculative
        # lastPlay read powers the home-run / goal detection client-side.
        s = c.get("situation", {})
        lp = s.get("lastPlay") or {}
        g["last_play_text"] = lp.get("text") or ""
        if sport == "baseball":
            g.update(_baseball_state(stt, s))
        if sport == "hockey":
            # ESPN's status text during a break reads like "End of 1st" or
            # "1st Intermission"; the NHL's own clock (enrich_hockey)
            # overrides this when it has the game.
            det = str(stt["type"].get("shortDetail") or stt["type"].get("detail") or "")
            g["intermission"] = bool(re.search(r"intermission|end of", det, re.I))
            g["intermission_left"] = ""
            g["intermission_lines"] = intermission_lines(stt.get("period") or 1, post)
    g["ranked"] = bool(top25)

    ticker = []
    for ev in events:
        if ev["id"] == mine["id"]:
            continue
        c2, h2, a2 = sides(ev)
        hr = (h2.get("curatedRank") or {}).get("current")
        ar = (a2.get("curatedRank") or {}).get("current")
        hr = None if hr in (None, 99) else hr
        ar = None if ar in (None, 99) else ar
        if top25 and hr is None and ar is None:
            continue
        s2 = c2["status"]
        st2 = s2["type"]["state"]
        e = {"home": h2["team"]["abbreviation"],
             "away": a2["team"]["abbreviation"],
             "home_color": h2["team"].get("color"),
             "away_color": a2["team"].get("color"),
             "home_rank": hr,
             "away_rank": ar}
        if st2 == "post":
            e["status"] = "F"
            e["score"] = f"{a2.get('score','')}-{h2.get('score','')}"
        elif st2 == "in":
            e["status"] = period_label(sport, s2.get("period"), _is_postseason(ev))
            e["clock"] = s2.get("displayClock")
            if sport == "hockey" and re.search(r"intermission|end of",
                    str(s2["type"].get("shortDetail") or ""), re.I):
                e["clock"] = "INT"          # "2ND" over "INT" in the ticker
            e["score"] = f"{a2.get('score','')}-{h2.get('score','')}"
            if sport == "baseball":
                # No clock in baseball: "T5" up top, outs underneath
                b = _baseball_state(s2, c2.get("situation", {}))
                e["status"] = b.get("inning_short") or e["status"]
                e["clock"] = (f"{b['outs']} OUT" if b.get("outs") is not None
                              and b.get("half") in ("TOP", "BOT") else "")
            if sport == "football":
                sit = c2.get("situation", {})
                e["redzone"] = sit.get("isRedZone", False)
                p2 = sit.get("possession")
                if p2 == h2["team"]["id"]:
                    e["possession"] = "home"
                elif p2 == a2["team"]["id"]:
                    e["possession"] = "away"
        else:
            d_s, t_s = split_kickoff(s2["type"].get("shortDetail", ""))
            e["status"] = "SCHED"
            e["kick_date"], e["kick_time"] = d_s, t_s
        ticker.append(e)
    g["ticker_games"] = ticker
    return g


def find_event_state(data, team):
    """Just enough to tell if this team's game exists and what state
    it's in, without the full parse_live work - used by auto_pick to
    scan five feeds quickly."""
    for ev in data.get("events", []):
        c = ev["competitions"][0]
        h = next(x for x in c["competitors"] if x["homeAway"] == "home")
        a = next(x for x in c["competitors"] if x["homeAway"] == "away")
        if team_matches(h, team) or team_matches(a, team):
            return ev, c["status"]["type"]["state"]
    return None


_FEED_CACHE = {}


def _fetch_cached(fetch_fn, url, ttl):
    """Days other than today barely change, so ask ESPN about them at most
    once every `ttl` seconds instead of on every 20-second refresh."""
    now = time.time()
    hit = _FEED_CACHE.get(url)
    if hit and now - hit[0] < ttl:
        return hit[1]
    data = fetch_fn(url)
    _FEED_CACHE[url] = (now, data)
    return data


def _with_date(url, yyyymmdd):
    return url + ("&" if "?" in url else "?") + "dates=" + yyyymmdd


AHEAD_DAYS, BACK_DAYS = 10, 21


def find_team_feed(fetch_fn, url, team, sport):
    """The feed that holds this team's most relevant game.

    Today's, if they play today. Otherwise - for the day-by-day sports
    (baseball, hockey, basketball) - their next game in the coming 10 days,
    and failing that their most recent result in the last 3 weeks (season
    over, eliminated, All-Star break). Football feeds already cover the
    whole week, so they're used as they come.

    Returns (data, when) with when = "today" | "next" | "last", or
    (today's data, None) if the team has nothing in range."""
    today = fetch_fn(url)
    if find_event_state(today, team) or sport == "football":
        return today, ("today" if find_event_state(today, team) else None)
    import datetime as _dt
    base = _dt.datetime.strptime(board_date(), "%Y%m%d")
    for d in range(1, AHEAD_DAYS + 1):
        day = (base + _dt.timedelta(days=d)).strftime("%Y%m%d")
        try:
            data = _fetch_cached(fetch_fn, _with_date(url, day), 1800)
        except Exception:
            continue
        if find_event_state(data, team):
            return data, "next"
    for d in range(1, BACK_DAYS + 1):
        day = (base - _dt.timedelta(days=d)).strftime("%Y%m%d")
        try:
            data = _fetch_cached(fetch_fn, _with_date(url, day), 3600)
        except Exception:
            continue
        if find_event_state(data, team):
            return data, "last"
    return today, None


def auto_pick(fetch_fn):
    """Which tracked team's game to show right now.

    Football always wins if it's live (Giants, then Gators). Otherwise
    the first other sport that's currently live, in priority order
    (baseball, hockey, basketball). If nothing's live, whichever tracked
    team plays next (today or in the coming days); if nobody has anything
    coming up, the most recent result. Returns (sport, data, team, top25) -
    data is the already-fetched feed, so the caller doesn't fetch twice.
    """
    upcoming, recent = [], []
    for sport, url, team, top25 in AUTO_PRIORITY:
        try:
            data, _ = find_team_feed(fetch_fn, url, team, sport)
        except Exception:
            continue
        found = find_event_state(data, team)
        if not found:
            continue
        ev, state = found
        if state == "in":
            return (sport, data, team, top25)
        if state == "pre":
            upcoming.append((ev.get("date", "9999"), sport, data, team, top25))
        else:
            recent.append((ev.get("date", ""), sport, data, team, top25))
    if upcoming:
        upcoming.sort(key=lambda x: x[0])
        _, sport, data, team, top25 = upcoming[0]
        return (sport, data, team, top25)
    if recent:
        recent.sort(key=lambda x: x[0], reverse=True)
        _, sport, data, team, top25 = recent[0]
        return (sport, data, team, top25)
    # Nothing anywhere (every feed empty/unreachable) - fall back to the
    # Giants board so the panel shows *something*.
    sport, url, team, top25 = AUTO_PRIORITY[0]
    return (sport, fetch_fn(url), team, top25)


# ----------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--team", default=None, help="team abbreviation, e.g. NYG or DAL")
    ap.add_argument("--live", action="store_true", help="pull real data from ESPN")
    ap.add_argument("--college", action="store_true", help="college football mode")
    ap.add_argument("--half", action="store_true",
                    help="compact mode for a small window")
    args = ap.parse_args()
    mode = "half" if args.half else "auto"

    team = args.team or ("UF" if args.college else "NYG")
    hold = 4          # seconds each pair of ticker games stays up
    refresh = 20      # seconds between ESPN refreshes in --live mode

    game = None
    last_fetch = 0
    if args.live:
        print("Fetching from ESPN...")
        try:
            url = CFB_URL if args.college else NFL_URL
            game = parse_live(fetch(url), team, top25=args.college)
            last_fetch = time.time()
        except Exception as e:
            print(f"Couldn't reach ESPN ({e}); showing the demo game instead.")
            time.sleep(2)
        if game is None and args.live:
            print(f"No game found for {team} this week; showing the demo game.")
            time.sleep(2)
    if game is None:
        game = demo_game(team)

    pairs = max(1, (len(game.get("ticker_games", [])) + 1) // 2)
    sys.stdout.write("\x1b[?25l")          # hide cursor
    t0 = time.time()
    try:
        while True:
            now = time.time()
            if args.live and now - last_fetch > refresh:
                try:
                    url = CFB_URL if args.college else NFL_URL
                    fresh = parse_live(fetch(url), team, top25=args.college)
                    if fresh:
                        game = fresh
                        pairs = max(1, (len(game.get("ticker_games", [])) + 1) // 2)
                except Exception:
                    pass
                last_fetch = now

            pair = int((now - t0) // hold) % pairs
            frame = to_terminal(render(game, pair), mode)
            sys.stdout.write("\x1b[H\x1b[2J" + frame + "\n")
            label = "LIVE" if args.live else "DEMO"
            sys.stdout.write(f"\x1b[0m  {team} board - {label} - Control-C to quit\n")
            hint = None if args.half else size_hint()
            if hint:
                sys.stdout.write(hint + "\n")
            sys.stdout.flush()
            time.sleep(0.5)
    except KeyboardInterrupt:
        pass
    finally:
        sys.stdout.write("\x1b[?25h\x1b[0m\n")   # show cursor again


if __name__ == "__main__":
    main()
