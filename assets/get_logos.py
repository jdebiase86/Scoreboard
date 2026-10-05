"""Downloads every team logo the board can show, for the mock-up programs
and logo tests in firmware/hosttest. The board itself fetches its logos
from ESPN; these are copies of exactly the same files.

For each team it keeps the first file the board would use (see
logoCandidates in sb_net.cpp): 500-dark/scoreboard, 500-dark, 500/scoreboard,
then 500, skipping any over 1024 px wide or tall as the board does. Saved as assets/logos/<league>/<file>.png, with index.txt listing
"league ABBR id file name" one team per line.

Teams Joe picked ESPN's regular team-colour logo for (FIX_LIGHT in
firmware/scoreboard/sb_logofix.h) also get <file>-light.png, the file the
board uses for them at small sizes.

Run from anywhere: python3 assets/get_logos.py
"""
import json, os, re, sys, urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "logos")
LEAGUES = [  # (folder, ESPN sport/league, extra query)
    ("nfl", "football/nfl", ""),
    ("ncaa", "football/college-football", "?groups=80&limit=1000"),
    ("mlb", "baseball/mlb", ""),
    ("nhl", "hockey/nhl", ""),
    ("nba", "basketball/nba", ""),
]


def get(url):
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(req, timeout=30) as r:
        return r.read()


def candidates(url):
    out = []
    if "/500/" in url:
        dark = url.replace("/500/", "/500-dark/")
        out.append(dark)
        if "/scoreboard/" in dark:
            out.append(dark.replace("/scoreboard/", "/"))
    out.append(url)
    if "/scoreboard/" in url:
        out.append(url.replace("/scoreboard/", "/"))
    return out


def board_url(folder, team):
    # what the scoreboard feed gives the board as the team's logo
    tail = f"{team['id']}.png" if folder == "ncaa" else f"scoreboard/{team['abbreviation'].lower()}.png"
    for l in team.get("logos", []):
        h = l.get("href", "")
        if f"/teamlogos/{folder}/500/" in h:
            name = h.rsplit("/", 1)[1]
            tail = name if folder == "ncaa" else f"scoreboard/{name}"
            break
    return f"https://a.espncdn.com/i/teamlogos/{folder}/500/{tail}"


def main():
    lines, missing = [], []
    for folder, path, q in LEAGUES:
        os.makedirs(os.path.join(OUT, folder), exist_ok=True)
        data = json.loads(get(f"https://site.api.espn.com/apis/site/v2/sports/{path}/teams{q}"))
        teams = [t["team"] for t in data["sports"][0]["leagues"][0]["teams"]]
        for t in teams:
            url = board_url(folder, t)
            fname = url.rsplit("/", 1)[1]
            dest = os.path.join(OUT, folder, fname)
            ok = os.path.exists(dest)
            for c in ([] if ok else candidates(url)):
                try:
                    png = get(c)
                except Exception:
                    continue
                # the board can't decode images over 1024 px (sb_png.cpp) and
                # moves on to the next candidate
                w, h = int.from_bytes(png[16:20], "big"), int.from_bytes(png[20:24], "big")
                if png[:4] == b"\x89PNG" and w <= 1024 and h <= 1024:
                    open(dest, "wb").write(png)
                    ok = True
                    break
            name = t.get("displayName") if folder != "ncaa" else t.get("location", t.get("displayName"))
            if ok:
                lines.append(f"{folder} {t['abbreviation']} {t['id']} {folder}/{fname} {name}")
            else:
                missing.append(f"{folder} {t['abbreviation']}")
        print(f"{folder}: {len(teams)} teams")
    # the regular logo for the teams picked to use it (sb_logofix.h)
    fix = open(os.path.join(HERE, "..", "firmware", "scoreboard", "sb_logofix.h")).read()
    for folder, stem in re.findall(r'\{"/(\w+)/", "(\w+)", FIX_LIGHT', fix):
        dest = os.path.join(OUT, folder, stem + "-light.png")
        if os.path.exists(dest):
            continue
        tails = [f"{stem}.png"] if folder == "ncaa" else [f"scoreboard/{stem}.png", f"{stem}.png"]
        for t in tails:
            try:
                png = get(f"https://a.espncdn.com/i/teamlogos/{folder}/500/{t}")
            except Exception:
                continue
            w, h = int.from_bytes(png[16:20], "big"), int.from_bytes(png[20:24], "big")
            if png[:4] == b"\x89PNG" and w <= 1024 and h <= 1024:
                open(dest, "wb").write(png)
                break
    lines.sort()
    open(os.path.join(OUT, "index.txt"), "w").write("\n".join(lines) + "\n")
    print(f"{len(lines)} logos; no logo for {len(missing)}: {' '.join(missing)}")


if __name__ == "__main__":
    sys.exit(main())
