"""
The stacked layout, drawn on a real pixel grid.

  MAIN GAME (top 40px) - your team, always. Status line, then the two
  teams stacked one per row with the score right-aligned, then one line
  of live detail underneath (down & distance).

  TICKER (bottom 23px) - two other games at a time, same stacked shape,
  with the quarter and clock on the right. Vertical bars at the panel
  edges flag a game: RED = someone's in the red zone, GREEN = one of
  your fantasy players is in that game.

Output is rendered LED-style (a grid of lit and unlit dots) so previews
actually look like the panel instead of like a screenshot.
"""
from PIL import Image, ImageDraw
import pixfont as PF

W, H = 64, 64
MAIN_H = 40

BLACK = (0, 0, 0)
WHITE = (255, 255, 255)
DIM = (120, 120, 120)
GRAY = (90, 90, 90)
GOLD = (255, 190, 0)
CLOCK = (235, 235, 235)   # the time itself - reads first
DATE = (140, 140, 140)    # the date / labels sitting behind it
RED = (255, 40, 40)
GREEN = (0, 230, 80)
BROWN = (190, 95, 30)   # football


def _led_color(rgb):
    """Navy, forest green and similar near-black team colors are
    invisible on a panel like this. Lift anything too dark until it
    actually lights up, while keeping its hue."""
    if rgb is None:
        return WHITE
    m = max(rgb)
    if m == 0:
        return WHITE
    if m < 140:
        f = 190.0 / m
        return tuple(min(255, int(c * f)) for c in rgb)
    return rgb


def _fit_abbr_scale(abbr, score_w, avail=63):
    """Use the big 2x font when the row has room, drop to 1x when the
    abbreviation is long (college teams like TENN) so nothing collides."""
    big_w = PF.text_width(abbr, PF.FONT5X7, scale=2)
    if big_w + 9 + score_w <= avail:
        return 2
    return 1


def short_abbr(abbr):
    """Both team rows must render at the same size or the board looks
    lopsided. A four-letter abbreviation (TENN, MISS) can't fit beside a
    two-digit score at the large size, so it gets clipped to three -
    which is what the NFL feed sends anyway, and why only college looked
    uneven."""
    return (abbr or "")[:3]


def draw_team_row(px, y, abbr, score, color, has_ball, row_h=14,
                  record=None, rank=None):
    abbr = short_abbr(abbr)
    """One stacked team row: ABBR on the left in team color, score
    right-aligned in white, football tucked in between if they have
    the ball.

    Before kickoff there's no score, so the slot carries the team's
    win-loss record instead - real information in the space the score
    will take, rather than a placeholder dash."""
    if score is None:
        # Record if we have it, ranking if that's all there is, otherwise
        # leave it empty - a dash says nothing the kickoff time doesn't.
        slot_txt = str(record) if record else (f"#{rank}" if rank else "")
        slot_font, slot_scale = PF.FONT5X7, 1
    else:
        slot_txt, slot_font, slot_scale = str(score), PF.FONT5X7, 2

    slot_w = PF.text_width(slot_txt, slot_font, scale=slot_scale)
    # The abbreviation sizing still has to clear a full-size score, or the
    # layout would jump when the game kicks off.
    scale = _fit_abbr_scale(abbr, PF.text_width("00", PF.FONT5X7, scale=2))

    glyph_h = 7 * scale
    y_abbr = y + (row_h - glyph_h) // 2
    PF.draw_text(px, 1, y_abbr, abbr, color, PF.FONT5X7, scale=scale, W=W, H=H)

    slot_x = W - 1 - slot_w
    y_slot = y + (row_h - 7 * slot_scale) // 2
    PF.draw_text(px, slot_x, y_slot, slot_txt, WHITE, slot_font,
                 scale=slot_scale, W=W, H=H)

    if has_ball:
        fb_x = slot_x - 9
        fb_y = y + (row_h - len(PF.FOOTBALL)) // 2
        PF.draw_sprite(px, fb_x, fb_y, PF.FOOTBALL, BROWN, W=W, H=H)


# Where each piece sits vertically. Pulled out into one place because
# fitting a divider between the two ticker games means finding spare
# rows, and the only spare rows are the gaps in here.
LAYOUT = {
    "status": 1,        # top line (clock / down & distance)
    "row1": 7,          # your team
    "row2": 22,         # opponent
    "divider": 37,      # main game vs ticker
    "tick_a": 39,       # first ticker game
    "tick_div": 51,     # divider between the two ticker games
    "tick_b": 53,       # second ticker game
}

# The ticker divider runs under the teams, scores and football only,
# stopping short of the clock / FINAL column on the right.
TICK_DIV_SPAN = (2, 39)


def short_down(dd, left_w):
    """Squeeze down & distance into whatever room is left on the top line.

    "1st & Goal at MISS 8" is the long case - spelled out it collides
    with the clock on the left, so Goal becomes G."""
    if not dd:
        return ""
    s = dd.split(" at ")[0].upper().replace(" ", "").replace("GOAL", "G")
    while s and left_w + 2 + PF.text_width(s, PF.FONT3X5) > W:
        s = s[:-1]
    return s


def draw_main(px, game, colors, lay=None):
    lay = lay or LAYOUT
    home, away = game["home"], game["away"]
    pinned_is_home = game["pinned_side"] == "home"
    # Your team always on the TOP row, regardless of home/away
    top, bottom = (home, away) if pinned_is_home else (away, home)
    top_is_home = pinned_is_home

    poss = game.get("possession")
    top_has_ball = poss is not None and ((poss == "home") == top_is_home)
    bottom_has_ball = poss is not None and not top_has_ball

    state = game["state"]

    # --- top line: clock on the left, situation on the right ---
    # Both live on one line so the two team rows below get real breathing
    # room. 64px only stretches so far.
    if state == "pre":
        left_txt, right_txt = game.get("kickoff_local", ""), ""
        right_color = GRAY
    elif state == "post":
        left_txt, right_txt = "FINAL", ""
        right_color = GRAY
    else:
        period = game.get("period", 1)
        q = {1: "1ST", 2: "2ND", 3: "3RD", 4: "4TH"}.get(
            period, f"OT{period-4}" if period > 4 else str(period))
        left_txt = f"{q} {game.get('clock','')}"
        right_txt = short_down(game.get("down_distance"),
                               PF.text_width(left_txt, PF.FONT3X5))
        right_color = RED if game.get("redzone") else GRAY

    ys = lay["status"]
    if state == "post":
        lw = PF.text_width(left_txt, PF.FONT3X5)
        PF.draw_text(px, (W - lw) // 2, ys, left_txt, GOLD, PF.FONT3X5, W=W, H=H)
    else:
        PF.draw_text(px, 2, ys, left_txt, CLOCK, PF.FONT3X5, W=W, H=H)
        if right_txt:
            rw = PF.text_width(right_txt, PF.FONT3X5)
            PF.draw_text(px, W - 2 - rw, ys, right_txt, right_color, PF.FONT3X5, W=W, H=H)

    # --- the two teams, stacked, with room to breathe ---
    draw_team_row(px, lay["row1"], top["abbr"], top.get("score"),
                  _led_color(colors.get(top["abbr"], {}).get("primary")),
                  top_has_ball, record=top.get("record"), rank=top.get("rank"))
    draw_team_row(px, lay["row2"], bottom["abbr"], bottom.get("score"),
                  _led_color(colors.get(bottom["abbr"], {}).get("primary")),
                  bottom_has_ball, record=bottom.get("record"), rank=bottom.get("rank"))


def draw_ticker_block(px, y, g, colors):
    """One ticker game: two stacked rows, quarter/clock on the right,
    plus the edge bars if it's flagged."""
    away_s = home_s = None
    if g.get("score"):
        away_s, home_s = g["score"].split("-")

    # right-hand column: quarter on top row, clock underneath
    status = g.get("status", "")
    if status == "F":
        right_top, right_bot = "FINAL", ""
    elif g.get("score") is None:
        # Not started: kickoff time on the top line, date underneath
        right_top, right_bot = g.get("kick_time", ""), g.get("kick_date", "")
    else:
        right_top, right_bot = status, g.get("clock", "")

    for i, (abbr, score, is_home) in enumerate(
        [(g["away"], away_s, False), (g["home"], home_s, True)]
    ):
        ry = y + i * 6
        color = _led_color(colors.get(abbr, {}).get("primary"))
        PF.draw_text(px, 3, ry, abbr, color, PF.FONT3X5, W=W, H=H)
        if score is not None:
            PF.draw_text(px, 21, ry, str(score), WHITE, PF.FONT3X5, W=W, H=H)
        poss = g.get("possession")
        if poss and ((poss == "home") == is_home):
            PF.draw_sprite(px, 31, ry, PF.FOOTBALL, BROWN, W=W, H=H)

    # A time reads bright; a date or a period label sits behind it.
    scheduled = g.get("score") is None and g.get("status") != "F"
    if right_top:
        rt_w = PF.text_width(right_top, PF.FONT3X5)
        c = GREEN if right_top == "FINAL" else (CLOCK if scheduled else DATE)
        PF.draw_text(px, W - 3 - rt_w, y, right_top, c, PF.FONT3X5, W=W, H=H)
    if right_bot:
        rb_w = PF.text_width(right_bot, PF.FONT3X5)
        c = DATE if scheduled else CLOCK
        PF.draw_text(px, W - 3 - rb_w, y + 6, right_bot, c, PF.FONT3X5, W=W, H=H)

    # edge bars - red beats green if both apply
    bar = None
    if g.get("redzone"):
        bar = RED
    elif g.get("fantasy"):
        bar = GREEN
    if bar:
        for yy in range(y, y + 11):
            if 0 <= yy < H:
                px[(0, yy)] = bar
                px[(W - 1, yy)] = bar


def render_pixels(game, colors, ticker_pair=0, lay=None):
    """Build the 64x64 pixel map. ticker_pair selects which two ticker
    games are currently showing (the panel rotates through them)."""
    lay = lay or LAYOUT
    px = {}
    draw_main(px, game, colors, lay)

    # Main divider, splitting your game from the ticker. 105 grey rather
    # than a near-black value: low PWM levels are where these panels get
    # unreliable and start to flicker.
    for x in range(W):
        px[(x, lay["divider"])] = (105, 105, 105)

    games = game.get("ticker_games", [])
    if games:
        i = (ticker_pair * 2) % len(games)
        pair = [games[i % len(games)], games[(i + 1) % len(games)]]
        draw_ticker_block(px, lay["tick_a"], pair[0], colors)
        if lay.get("tick_div") is not None:
            for x in range(*TICK_DIV_SPAN):
                px[(x, lay["tick_div"])] = (105, 105, 105)
        draw_ticker_block(px, lay["tick_b"], pair[1], colors)
    return px


def to_led_image(px, scale=10, dot_ratio=0.78):
    """Render the pixel map as an LED panel: lit dots on a dark grid,
    with the unlit LEDs faintly visible the way they are in person."""
    img = Image.new("RGB", (W * scale, H * scale), (14, 14, 14))
    d = ImageDraw.Draw(img)
    r = (scale * dot_ratio) / 2.0
    off = (30, 30, 30)
    for y in range(H):
        for x in range(W):
            cx = x * scale + scale / 2
            cy = y * scale + scale / 2
            color = px.get((x, y), off)
            d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=color)
    return img


def render(game, colors, ticker_pair=0, scale=10, lay=None):
    return to_led_image(render_pixels(game, colors, ticker_pair, lay), scale=scale)
