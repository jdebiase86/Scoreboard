import sys, os
sys.path.insert(0, os.path.dirname(__file__))
from PIL import Image, ImageDraw, ImageFont
import mock_data as M
import led_render as L

OUT = os.path.join(os.path.dirname(__file__), "..", "out")

# A: what we have now - no divider, 1 blank row between ticker games
A = dict(status=1, row1=7, row2=23, divider=38, tick_a=40, tick_div=None, tick_b=52)

# B: squeeze 1 row out of the gap between the two main team rows,
#    which buys a blank/line/blank sandwich in the ticker
B = dict(status=1, row1=7, row2=22, divider=37, tick_a=39, tick_div=51, tick_b=53)

# C: leave the main rows alone, close the gap under the status line
#    instead, and spend the row on the ticker divider
C = dict(status=1, row1=6, row2=22, divider=37, tick_a=39, tick_div=51, tick_b=53)

# D: no layout change at all - just accept the tight 1px gap and put a
#    short centered line in it
D = dict(status=1, row1=7, row2=23, divider=38, tick_a=40, tick_div=51, tick_b=52)

VARIANTS = [
    ("A - now (no divider)", A, None),
    ("B - tighter team rows", B, None),
    ("C - tighter under clock", C, None),
    ("D - line in the 1px gap", D, "short"),
]

game = M.nfl_live_giants()
scale = 8
cell = 64 * scale
label_h = 26
pad = 12
cols = 4
sheet = Image.new("RGB", (cols * (cell + pad) + pad, cell + label_h + 2 * pad), (18, 18, 18))
d = ImageDraw.Draw(sheet)
try:
    f = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 16)
except Exception:
    f = ImageFont.load_default()

for i, (name, lay, special) in enumerate(VARIANTS):
    px = L.render_pixels(game, M.TEAM_COLORS, 0, lay)
    if special == "short":
        for x in range(4, 64 - 4):
            px.pop((x, lay["tick_div"]), None)
        for x in range(18, 46):
            px[(x, lay["tick_div"])] = (105, 105, 105)
    img = L.to_led_image(px, scale=scale)
    x = pad + i * (cell + pad)
    sheet.paste(img, (x, pad + label_h))
    d.text((x + 2, pad + 4), name, font=f, fill=(235, 235, 235))

p = os.path.join(OUT, "ticker_divider_options.png")
sheet.save(p)
print("wrote", p)
