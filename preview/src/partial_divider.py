import sys, os
sys.path.insert(0, os.path.dirname(__file__))
from PIL import Image, ImageDraw, ImageFont
import mock_data as M
import led_render as L

OUT = os.path.join(os.path.dirname(__file__), "..", "out")

# Ticker content x-ranges, for reference:
#   abbr   x=3..17   (3-4 chars)
#   score  x=21..27
#   ball   x=31..37
#   right column (FINAL / 3RD / clock) is right-aligned, starts ~x=42

# Layout B: one row borrowed from between the team rows, one from under
# the scores, so the ticker divider gets a blank row above and below.
B = dict(status=1, row1=7, row2=22, divider=37, tick_a=39, tick_div=51, tick_b=53)
# Layout D: nothing moved, divider squeezed into the existing 1px gap
D = dict(status=1, row1=7, row2=23, divider=38, tick_a=40, tick_div=51, tick_b=52)

VARIANTS = [
    ("1 - thru football (x2-38)", B, (2, 39)),
    ("2 - teams+score (x2-28)",   B, (2, 29)),
    ("3 - no layout change",      D, (2, 39)),
]

game = M.nfl_live_giants()
scale = 9
cell = 64 * scale
label_h = 26
pad = 12
sheet = Image.new("RGB", (3 * (cell + pad) + pad, cell + label_h + 2 * pad), (18, 18, 18))
d = ImageDraw.Draw(sheet)
try:
    f = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 17)
except Exception:
    f = ImageFont.load_default()

for i, (name, lay, span) in enumerate(VARIANTS):
    px = L.render_pixels(game, M.TEAM_COLORS, 0, lay)
    y = lay["tick_div"]
    for x in range(64):
        px.pop((x, y), None)
    for x in range(span[0], span[1]):
        px[(x, y)] = (105, 105, 105)
    img = L.to_led_image(px, scale=scale)
    x0 = pad + i * (cell + pad)
    sheet.paste(img, (x0, pad + label_h))
    d.text((x0 + 2, pad + 4), name, font=f, fill=(235, 235, 235))

p = os.path.join(OUT, "partial_divider.png")
sheet.save(p)
print("wrote", p)
