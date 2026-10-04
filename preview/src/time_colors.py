"""Compare treatments for the clock/time/date text."""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
from PIL import Image, ImageDraw, ImageFont
import mock_data as M
import led_render as L
import pixfont as PF

W = 64

# (name, clock/time color, date color, quarter-label color)
VARIANTS = [
    ("Now - grey 120",      (120,120,120), (120,120,120), (120,120,120)),
    ("Amber time, grey date",(255,176,0),   (150,150,150), (255,176,0)),
    ("Amber time + date",   (255,176,0),   (190,132,20),  (255,176,0)),
    ("White time, grey date",(235,235,235), (140,140,140), (235,235,235)),
]

def render(game, clock_c, date_c, label_c, pair=0):
    """Re-render with the time-related colors swapped in."""
    old_dim = L.DIM
    px = {}
    L.DIM = clock_c
    L.draw_main(px, game, M.TEAM_COLORS)
    for x in range(W):
        px[(x, L.LAYOUT["divider"])] = (105,105,105)
    games = game.get("ticker_games", [])
    i = (pair*2) % len(games)
    for slot, y in ((0, L.LAYOUT["tick_a"]), (1, L.LAYOUT["tick_b"])):
        g = games[(i+slot) % len(games)]
        L.DIM = clock_c
        L.draw_ticker_block(px, y, g, M.TEAM_COLORS)
        # repaint the date line underneath in its own color
        if not g.get("score") and g.get("kick_date"):
            rb = g["kick_date"]
            rw = PF.text_width(rb, PF.FONT3X5)
            for xx in range(W-3-rw, W-2):
                for yy in range(y+6, y+11):
                    px.pop((xx, yy), None)
            PF.draw_text(px, W-3-rw, y+6, rb, date_c, PF.FONT3X5, W=W, H=64)
    for x in range(2, 39):
        px[(x, L.LAYOUT["tick_div"])] = (105,105,105)
    L.DIM = old_dim
    return L.to_led_image(px, scale=9)

game = M.nfl_pregame_giants()
game["kickoff_local"] = "9/27 1:00P"

scale = 9; cell = 64*scale; label_h = 26; pad = 12; cols = 2
rows = 2
sheet = Image.new("RGB", (cols*(cell+pad)+pad, rows*(cell+label_h+pad)+pad), (18,18,18))
d = ImageDraw.Draw(sheet)
try: f = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 17)
except Exception: f = ImageFont.load_default()

for i,(name, c, dt, lb) in enumerate(VARIANTS):
    img = render(game, c, dt, lb)
    r, cc = divmod(i, cols)
    x = pad + cc*(cell+pad); y = pad + r*(cell+label_h+pad)
    sheet.paste(img, (x, y+label_h))
    d.text((x+2, y+4), name, font=f, fill=(235,235,235))
p = os.path.join(os.path.dirname(__file__), "..", "out", "time_colors.png")
sheet.save(p); print("wrote", p)
