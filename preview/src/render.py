"""
Draws a game-state dict (see mock_data.py / fetch.py for the shape) onto a
64x64 image exactly the way the panel layout is meant to look:

  - Top ~40px: ONE scoreboard for the followed team, not two side-by-side
    boxes — a single row of "AWAY  score - score  HOME" with each
    abbreviation in its real team color, a possession arrow next to
    whichever team has the ball, then one line of context underneath
    (kickoff time before the game, clock/quarter and down & distance
    while live, "FINAL" after).
  - Bottom ~24px: scrolling ticker. Stripped down to just what was asked
    for — one other game at a time, team abbreviations in their real
    colors, the score, and a possession arrow. No down/distance, no
    venue, no odds clutter down there; that detail stays reserved for
    the main game up top.

This only depends on Pillow, no matplotlib, since we're literally trying
to preview what a 4096-LED panel would show pixel-for-pixel.
"""
from PIL import Image, ImageDraw, ImageFont
import os

W, H = 64, 64
TOP_H = 40      # main game section
TICKER_H = 24   # scrolling ticker lane

BG = (0, 0, 0)
WHITE = (255, 255, 255)
GRAY = (140, 140, 140)
GOLD = (255, 200, 0)
RED = (255, 60, 60)
GREEN = (60, 220, 100)

FONT_DIR = os.path.join(os.path.dirname(__file__), "..", "fonts")


def _font(size):
    # DejaVu Sans Mono ships with Pillow's test fonts on most systems;
    # fall back to Pillow's built-in bitmap font if unavailable. On the
    # real panel this is replaced by the hand-authored bitmap font the
    # firmware draws with, so exact glyphs here are just a stand-in.
    for candidate in [
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    ]:
        if os.path.exists(candidate):
            try:
                return ImageFont.truetype(candidate, size)
            except Exception:
                pass
    return ImageFont.load_default()


F_SCORE = _font(15)
F_ABBR = _font(9)
F_SMALL = _font(7)
F_TINY = _font(6)


def _team_color(abbr, mock_colors):
    c = mock_colors.get(abbr)
    if c:
        return c["primary"]
    return WHITE


def _tw(draw, text, font):
    bbox = draw.textbbox((0, 0), text, font=font)
    return bbox[2] - bbox[0]


def _score_str(v):
    return "-" if v is None else str(v)


def _arrow(draw, x, y, pointing_right, color):
    # small possession triangle, ~4px, pointing at the team that has it
    if pointing_right:
        draw.polygon([(x, y), (x, y + 5), (x + 4, y + 2)], fill=color)
    else:
        draw.polygon([(x + 4, y), (x + 4, y + 5), (x, y + 2)], fill=color)


def draw_main_game(draw, game, mock_colors):
    home, away = game["home"], game["away"]
    pinned_is_home = game["pinned_side"] == "home"
    left, right = (home, away) if pinned_is_home else (away, home)
    left_is_home = pinned_is_home
    left_color = _team_color(left["abbr"], mock_colors)
    right_color = _team_color(right["abbr"], mock_colors)

    # --- Row 1: abbreviations, flush to the outside edges ---
    y_abbr = 1
    left_w = _tw(draw, left["abbr"], F_ABBR)
    draw.text((2, y_abbr), left["abbr"], font=F_ABBR, fill=left_color)
    right_w = _tw(draw, right["abbr"], F_ABBR)
    draw.text((W - 2 - right_w, y_abbr), right["abbr"], font=F_ABBR, fill=right_color)

    # Possession arrow lives up here next to the abbreviation, where
    # there's actually room - down by the scores it gets clipped off
    # the edge, since two big numbers nearly fill all 64px.
    poss = game.get("possession")
    if poss:
        poss_is_left = (poss == "home") == left_is_home
        if poss_is_left:
            _arrow(draw, 2 + left_w + 2, y_abbr + 2, False, GOLD)
        else:
            _arrow(draw, W - 2 - right_w - 7, y_abbr + 2, True, GOLD)

    # --- Row 2: one shared score line, "20 - 17", centered ---
    y_score = 11
    l_s = _score_str(left.get("score"))
    r_s = _score_str(right.get("score"))
    mid = " - "
    l_w = _tw(draw, l_s, F_SCORE)
    m_w = _tw(draw, mid, F_SCORE)
    r_w = _tw(draw, r_s, F_SCORE)
    total = l_w + m_w + r_w
    x = (W - total) // 2
    draw.text((x, y_score), l_s, font=F_SCORE, fill=WHITE)
    x += l_w
    draw.text((x, y_score), mid, font=F_SCORE, fill=GRAY)
    x += m_w
    draw.text((x, y_score), r_s, font=F_SCORE, fill=WHITE)

    # --- Row 3: context line ---
    y_ctx = 29
    state = game["state"]
    if state == "pre":
        draw.text((2, y_ctx), game.get("kickoff_local", "")[:18], font=F_TINY, fill=GRAY)
        return
    if state == "post":
        w = _tw(draw, "FINAL", F_SMALL)
        draw.text(((W - w) // 2, y_ctx - 1), "FINAL", font=F_SMALL, fill=GOLD)
        return

    period = game.get("period", 1)
    q_label = {1: "1st", 2: "2nd", 3: "3rd", 4: "4th"}.get(period, f"OT{period - 4}" if period > 4 else str(period))
    clock_line = f"{q_label}  {game.get('clock', '')}"
    w = _tw(draw, clock_line, F_SMALL)
    draw.text(((W - w) // 2, y_ctx - 2), clock_line, font=F_SMALL, fill=WHITE)

    dd = game.get("down_distance")
    if dd:
        # Down & distance only, no field position - that's what fits
        # cleanly at this size: "3rd & 6 at PHI 34" -> "3rd & 6"
        short = dd.split(" at ")[0]
        dd_color = RED if game.get("redzone") else GRAY
        w = _tw(draw, short, F_TINY)
        draw.text(((W - w) // 2, 33), short[:14], font=F_TINY, fill=dd_color)


def draw_ticker(draw, game, mock_colors, ticker_offset=0):
    y0 = TOP_H
    draw.line([(0, y0), (W, y0)], fill=(50, 50, 50))
    games = game.get("ticker_games", [])
    if not games:
        return
    g = games[ticker_offset % len(games)]

    rank_a = g.get("rank_away")
    rank_h = g.get("rank_home")
    a_label = (f"#{rank_a} " if rank_a else "") + g["away"]
    h_label = (f"#{rank_h} " if rank_h else "") + g["home"]
    a_color = _team_color(g["away"], mock_colors)
    h_color = _team_color(g["home"], mock_colors)

    is_live_or_final = "score" in g
    y_names = y0 + 4

    if is_live_or_final:
        away_s, home_s = g["score"].split("-")
        # "AWAY 20  -  17 HOME" single centered line, each half its team's color
        l_w = _tw(draw, a_label, F_SMALL)
        s_w = _tw(draw, f" {away_s}", F_SMALL)
        mid_w = _tw(draw, " - ", F_SMALL)
        s2_w = _tw(draw, f"{home_s} ", F_SMALL)
        r_w = _tw(draw, h_label, F_SMALL)
        total = l_w + s_w + mid_w + s2_w + r_w
        x = (W - total) // 2
        draw.text((x, y_names), a_label, font=F_SMALL, fill=a_color); x += l_w
        draw.text((x, y_names), f" {away_s}", font=F_SMALL, fill=WHITE); x += s_w
        draw.text((x, y_names), " - ", font=F_SMALL, fill=GRAY); x += mid_w
        draw.text((x, y_names), f"{home_s} ", font=F_SMALL, fill=WHITE); x += s2_w
        draw.text((x, y_names), h_label, font=F_SMALL, fill=h_color)

        poss = g.get("possession")
        if poss:
            if poss == "away":
                _arrow(draw, max(0, (W - total) // 2 - 7), y_names + 5, True, GOLD)
            else:
                _arrow(draw, (W - total) // 2 + total + 3, y_names + 5, False, GOLD)
        if g.get("status") == "F":
            w = _tw(draw, "FINAL", F_TINY)
            draw.text(((W - w) // 2, y0 + 15), "FINAL", font=F_TINY, fill=GREEN)
        else:
            w = _tw(draw, g.get("status", ""), F_TINY)
            draw.text(((W - w) // 2, y0 + 15), g.get("status", ""), font=F_TINY, fill=GRAY)
    else:
        line = f"{a_label} @ {h_label}"
        w = _tw(draw, line, F_SMALL)
        draw.text(((W - w) // 2, y_names), a_label, font=F_SMALL, fill=a_color)
        aw = _tw(draw, a_label + " @ ", F_SMALL)
        start_x = (W - w) // 2
        draw.text((start_x, y_names), a_label, font=F_SMALL, fill=a_color)
        draw.text((start_x + _tw(draw, a_label, F_SMALL), y_names), " @ ", font=F_SMALL, fill=GRAY)
        draw.text((start_x + _tw(draw, a_label + " @ ", F_SMALL), y_names), h_label, font=F_SMALL, fill=h_color)
        status = g.get("status", "")
        sw = _tw(draw, status, F_TINY)
        draw.text(((W - sw) // 2, y0 + 15), status, font=F_TINY, fill=GRAY)


def build_ticker_strip(games, mock_colors, gap=14):
    """Render every ticker game side by side into one long horizontal
    strip. The panel then shows a 64px-wide window onto this strip and
    slides that window left, which is what produces the scroll.

    This is how the real firmware will do it too - render once, scroll a
    pointer - rather than re-laying-out text every frame."""
    # First pass: measure
    probe = Image.new("RGB", (1, 1))
    pd = ImageDraw.Draw(probe)

    entries = []
    for g in games:
        rank_a = g.get("rank_away")
        rank_h = g.get("rank_home")
        a_label = (f"#{rank_a} " if rank_a else "") + g["away"]
        h_label = (f"#{rank_h} " if rank_h else "") + g["home"]
        if g.get("score"):
            away_s, home_s = g["score"].split("-")
            parts = [
                (a_label, _team_color(g["away"], mock_colors)),
                (f" {away_s}", WHITE),
                (" - ", GRAY),
                (f"{home_s} ", WHITE),
                (h_label, _team_color(g["home"], mock_colors)),
            ]
        else:
            parts = [
                (a_label, _team_color(g["away"], mock_colors)),
                (" @ ", GRAY),
                (h_label, _team_color(g["home"], mock_colors)),
            ]
        width = sum(_tw(pd, t, F_SMALL) for t, _ in parts)
        entries.append({"parts": parts, "width": width, "game": g})

    # Leading/trailing 64px of blank so the scroll wraps cleanly
    total_w = W + sum(e["width"] + gap for e in entries) + W
    strip = Image.new("RGB", (total_w, TICKER_H), BG)
    sd = ImageDraw.Draw(strip)

    x = W
    for e in entries:
        g = e["game"]
        start_x = x
        y = 3
        for text, color in e["parts"]:
            sd.text((x, y), text, font=F_SMALL, fill=color)
            x += _tw(sd, text, F_SMALL)

        # possession arrow, tucked in under the team that has the ball
        poss = g.get("possession")
        if poss:
            ax = start_x if poss == "away" else x - 6
            _arrow(sd, ax, y + 10, poss == "away", GOLD)

        # status under the score: FINAL in green, otherwise the quarter
        status = g.get("status", "")
        if status:
            color = GREEN if status == "F" else GRAY
            label = "FINAL" if status == "F" else status
            sw = _tw(sd, label, F_TINY)
            sd.text((start_x + (e["width"] - sw) // 2, y + 10), label, font=F_TINY, fill=color)

        x += gap

    return strip


def render_scroll_frame(game, strip, scroll_x, mock_colors):
    """One animation frame: static main game on top, a 64px window onto
    the ticker strip at the bottom."""
    img = Image.new("RGB", (W, H), BG)
    draw = ImageDraw.Draw(img)
    draw_main_game(draw, game, mock_colors)
    draw.line([(0, TOP_H), (W, TOP_H)], fill=(50, 50, 50))
    window = strip.crop((scroll_x, 0, scroll_x + W, TICKER_H))
    img.paste(window, (0, TOP_H))
    return img


def render_animation(game, mock_colors=None, scale=5, step=2):
    """Full scroll cycle as a list of scaled frames, ready for a GIF."""
    mock_colors = mock_colors or {}
    strip = build_ticker_strip(game.get("ticker_games", []), mock_colors)
    frames = []
    max_x = strip.width - W
    x = 0
    while x < max_x:
        f = render_scroll_frame(game, strip, x, mock_colors)
        frames.append(f.resize((W * scale, H * scale), Image.NEAREST))
        x += step
    return frames


def render_frame(game, mock_colors=None, ticker_offset=0):
    """Render one static 64x64 frame. ticker_offset selects which ticker
    entry leads (0 = first game), simulating one step of the scroll."""
    mock_colors = mock_colors or {}
    img = Image.new("RGB", (W, H), BG)
    draw = ImageDraw.Draw(img)
    draw_main_game(draw, game, mock_colors)
    draw_ticker(draw, game, mock_colors, ticker_offset)
    return img


def render_scaled(game, mock_colors=None, ticker_offset=0, scale=8):
    """Convenience: render at native 64x64 then nearest-neighbor scale up
    so it's actually viewable as a preview image (a 64px PNG is tiny)."""
    img = render_frame(game, mock_colors, ticker_offset)
    return img.resize((W * scale, H * scale), Image.NEAREST)
