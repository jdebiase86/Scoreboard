"""
Board configuration.

This is the ONLY thing that differs between your board and your
father-in-law's. Same code, same flashing procedure - you change these
two lines before flashing each board and that's it.

In the real firmware this becomes a few #define lines at the top of the
main .ino/.cpp file, so when you flash board #2 you edit two words,
hit upload, done. (Later we can move this to a WiFi settings page so
it's changeable without reflashing, but that's a nice-to-have, not
needed for v1.)
"""

BOARDS = {
    "joe": {
        "nfl_team": "NYG",   # Giants - shown at the top every NFL gameday
        "cfb_team": "UF",    # Florida - shown at the top on college Saturdays
    },
    "father_in_law": {
        "nfl_team": "DAL",   # Cowboys
        "cfb_team": "LSU",   # LSU
    },
}

# ---- Set this one line per board before flashing ----
ACTIVE_BOARD = "joe"
# -----------------------------------------------------

NFL_TEAM = BOARDS[ACTIVE_BOARD]["nfl_team"]
CFB_TEAM = BOARDS[ACTIVE_BOARD]["cfb_team"]

# Ticker contents:
#   NFL gamedays  -> every other NFL game happening that day
#   College Sats  -> top 25 games only
CFB_TICKER_TOP25_ONLY = True
