import sys, os
sys.path.insert(0, os.path.dirname(__file__))
from PIL import Image
import mock_data as M
import render as R

OUT = os.path.join(os.path.dirname(__file__), "..", "out")
os.makedirs(OUT, exist_ok=True)

jobs = [
    ("joe_nfl_pregame", M.nfl_pregame_giants()),
    ("joe_nfl_live",    M.nfl_live_giants()),
    ("joe_nfl_final",   M.nfl_final_giants()),
    ("joe_cfb_live",    M.cfb_live_gators()),
    ("fil_nfl_live",    M.nfl_live_cowboys()),
    ("fil_cfb_live",    M.cfb_live_lsu()),
]

paths = []
for name, game in jobs:
    img = R.render_scaled(game, M.TEAM_COLORS, ticker_offset=0, scale=8)
    p = os.path.join(OUT, f"{name}.png")
    img.save(p)
    paths.append(p)
    print("wrote", p)

# Contact sheet: 3 columns
cols = 3
rows = (len(paths) + cols - 1) // cols
cell_w, cell_h = 64 * 8, 64 * 8
label_h = 22
pad = 10
sheet = Image.new("RGB", (cols * (cell_w + pad) + pad, rows * (cell_h + label_h + pad) + pad), (20, 20, 20))
from PIL import ImageDraw, ImageFont
d = ImageDraw.Draw(sheet)
try:
    lf = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 16)
except Exception:
    lf = ImageFont.load_default()

for i, ((name, _), p) in enumerate(zip(jobs, paths)):
    r, c = divmod(i, cols)
    x = pad + c * (cell_w + pad)
    y = pad + r * (cell_h + label_h + pad)
    im = Image.open(p)
    sheet.paste(im, (x, y + label_h))
    d.text((x, y), name, font=lf, fill=(255, 255, 255))

sheet_path = os.path.join(OUT, "contact_sheet.png")
sheet.save(sheet_path)
print("wrote", sheet_path)
