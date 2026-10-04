"""What goes in the score slot before kickoff."""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
from PIL import Image, ImageDraw, ImageFont
import mock_data as M
import led_render as L
import pixfont as PF

W, H = 64, 64
CLOCK = (235,235,235)   # white time  \ the combination
DATE  = (140,140,140)   # grey date   / Joe picked
GREY  = (140,140,140)

def base(game, slot_mode, pair=0):
    px = {}
    old = L.DIM
    L.DIM = CLOCK
    L.draw_main(px, game, M.TEAM_COLORS)
    L.DIM = old

    # wipe the two score slots and redraw them per the mode
    pinned_home = game["pinned_side"] == "home"
    top = game["home"] if pinned_home else game["away"]
    bot = game["away"] if pinned_home else game["home"]
    for y in (L.LAYOUT["row1"], L.LAYOUT["row2"]):
        for xx in range(34, W):
            for yy in range(y, y+14):
                px.pop((xx, yy), None)

    for side, y in ((top, L.LAYOUT["row1"]), (bot, L.LAYOUT["row2"])):
        rec = side.get("record") or ""
        if slot_mode == "record_white" and rec:
            w = PF.text_width(rec, PF.FONT5X7, scale=1)
            PF.draw_text(px, W-1-w, y+4, rec, (255,255,255), PF.FONT5X7, scale=1, W=W, H=H)
        elif slot_mode == "record_grey" and rec:
            w = PF.text_width(rec, PF.FONT5X7, scale=1)
            PF.draw_text(px, W-1-w, y+4, rec, GREY, PF.FONT5X7, scale=1, W=W, H=H)
        elif slot_mode == "record_big" and rec:
            w = PF.text_width(rec, PF.FONT3X5, scale=2)
            PF.draw_text(px, W-1-w, y+2, rec, GREY, PF.FONT3X5, scale=2, W=W, H=H)
        elif slot_mode == "dim_dash":
            w = PF.text_width("-", PF.FONT5X7, scale=2)
            PF.draw_text(px, W-1-w, y, "-", (80,80,80), PF.FONT5X7, scale=2, W=W, H=H)
        # "blank" draws nothing

    for x in range(W):
        px[(x, L.LAYOUT["divider"])] = (105,105,105)
    games = game.get("ticker_games", [])
    i = (pair*2) % len(games)
    for slot, yy in ((0, L.LAYOUT["tick_a"]), (1, L.LAYOUT["tick_b"])):
        g = games[(i+slot) % len(games)]
        L.DIM = CLOCK
        L.draw_ticker_block(px, yy, g, M.TEAM_COLORS)
        L.DIM = old
        if not g.get("score") and g.get("kick_date"):
            rb = g["kick_date"]; rw = PF.text_width(rb, PF.FONT3X5)
            for x2 in range(W-3-rw, W-2):
                for y2 in range(yy+6, yy+11):
                    px.pop((x2, y2), None)
            PF.draw_text(px, W-3-rw, yy+6, rb, DATE, PF.FONT3X5, W=W, H=H)
    for x in range(2, 39):
        px[(x, L.LAYOUT["tick_div"])] = (105,105,105)
    return L.to_led_image(px, scale=9)

MODES = [
    ("Records, white",  "record_white"),
    ("Records, grey",   "record_grey"),
    ("Records, bigger", "record_big"),
    ("Dimmed dash",     "dim_dash"),
    ("Nothing at all",  "blank"),
]

game = M.nfl_pregame_giants()
game["kickoff_local"] = "9/27 1:00P"

scale=9; cell=64*scale; lh=26; pad=12; cols=3
rows=(len(MODES)+cols-1)//cols
sheet=Image.new("RGB",(cols*(cell+pad)+pad, rows*(cell+lh+pad)+pad),(18,18,18))
d=ImageDraw.Draw(sheet)
try: f=ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",17)
except Exception: f=ImageFont.load_default()
for i,(name,mode) in enumerate(MODES):
    img=base(game,mode)
    r,c=divmod(i,cols); x=pad+c*(cell+pad); y=pad+r*(cell+lh+pad)
    sheet.paste(img,(x,y+lh)); d.text((x+2,y+4),name,font=f,fill=(235,235,235))
p=os.path.join(os.path.dirname(__file__),"..","out","pregame_slot.png")
sheet.save(p); print("wrote",p)
