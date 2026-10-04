import sys, os
sys.path.insert(0, os.path.dirname(__file__))
from PIL import Image, ImageDraw, ImageFont
import mock_data as M, led_render as L

OUT = os.path.join(os.path.dirname(__file__), "..", "out")
os.makedirs(OUT, exist_ok=True)

live = M.nfl_live_giants()

pre = M.nfl_pregame_giants()
pre["kickoff_local"] = "SUN 1:00"
pre["ticker_games"] = M.nfl_sunday_slate()

final = M.nfl_final_giants()
final["ticker_games"] = M.nfl_sunday_slate()

cfb = M.cfb_live_gators()

jobs = [
    ("Live - Giants have the ball", live, 0),
    ("Live - ticker rotated on", live, 1),
    ("Pregame", pre, 0),
    ("Final", final, 0),
    ("College Saturday - Gators", cfb, 0),
]

paths = []
for name, g, pair in jobs:
    img = L.render(g, M.TEAM_COLORS, ticker_pair=pair, scale=8)
    fn = name.lower().replace(" ", "_").replace("-", "").replace("'", "")
    p = os.path.join(OUT, f"led_{fn}.png")
    img.save(p); paths.append((name, p))
    print("wrote", p)

cell = 64 * 8
label_h = 26
pad = 12
cols = 3
rows = (len(paths) + cols - 1) // cols
sheet = Image.new("RGB", (cols * (cell + pad) + pad, rows * (cell + label_h + pad) + pad), (18, 18, 18))
d = ImageDraw.Draw(sheet)
try:
    f = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 17)
except Exception:
    f = ImageFont.load_default()
for i, (name, p) in enumerate(paths):
    r, c = divmod(i, cols)
    x = pad + c * (cell + pad); y = pad + r * (cell + label_h + pad)
    sheet.paste(Image.open(p), (x, y + label_h))
    d.text((x + 2, y + 4), name, font=f, fill=(235, 235, 235))
sp = os.path.join(OUT, "led_contact_sheet.png")
sheet.save(sp); print("wrote", sp)
