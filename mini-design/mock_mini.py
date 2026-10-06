"""Mini scoreboard mock-ups, 480x320 wide, using the real ESPN logos from the
Scoreboard repo (assets/logos). Needs Pillow and the Inter font.

    python3 mock_mini.py        -> mini_mockups.png (the six main screens)
    python3 mock_mini.py rz     -> mini_redzone.png (our team in the red zone)
    python3 mock_mini.py opp    -> mini_redzone_opp.png (red zone pop-ups, ours vs theirs)

LOGOS points at Scoreboard/assets/logos (default: a Scoreboard checkout next
to this repo). FONTS points at the folder holding Inter-Bold.otf etc.
"""
import os, sys, math
from PIL import Image, ImageDraw, ImageFont, ImageFilter
OUT = os.path.dirname(os.path.abspath(__file__))
LOGOS = os.environ.get("LOGOS", os.path.join(OUT, "../../Scoreboard/assets/logos"))
FONTS = os.environ.get("FONTS", "/usr/share/fonts/opentype/inter")
S = 2                      # draw at 2x, shrink for smooth edges
W, H = 480, 320
FB = os.path.join(FONTS, "Inter-Bold.otf")
FS = os.path.join(FONTS, "Inter-SemiBold.otf")
FR = os.path.join(FONTS, "Inter-Medium.otf")
_fc = {}
def font(path, size):
    k = (path, size)
    if k not in _fc: _fc[k] = ImageFont.truetype(path, size * S)
    return _fc[k]

BG = (12, 14, 20); TILE = (28, 32, 42); TILE_HI = (38, 44, 58); EDGE = (52, 58, 74)
WHITE = (240, 242, 246); GREY = (150, 156, 170); DIM = (100, 106, 120)
RED = (226, 40, 46); GREEN = (40, 190, 90); YELLOW = (250, 210, 40)

def logo(path, size):
    im = Image.open(os.path.join(LOGOS, path)).convert("RGBA")
    bb = im.getbbox()
    if bb: im = im.crop(bb)
    w, h = im.size; sc = size * S / max(w, h)
    return im.resize((max(1, round(w * sc)), max(1, round(h * sc))), Image.LANCZOS)

def paste_logo(img, path, cx, cy, size, maxw=None):
    l = logo(path, size)
    if maxw and l.width > maxw * S:
        l = l.resize((round(maxw * S), max(1, round(l.height * maxw * S / l.width))), Image.LANCZOS)
    img.alpha_composite(l, (round(cx * S - l.width / 2), round(cy * S - l.height / 2)))

def new():
    img = Image.new("RGBA", (W * S, H * S), BG + (255,))
    return img, ImageDraw.Draw(img)

def R(x0, y0, x1, y1): return [x0 * S, y0 * S, x1 * S, y1 * S]

def rbox(d, x0, y0, x1, y1, fill, r=10, outline=None, width=1):
    d.rounded_rectangle(R(x0, y0, x1, y1), r * S, fill=fill, outline=outline, width=width * S)

def text(d, x, y, s, f, fill, anchor="la"):
    d.text((x * S, y * S), s, font=f, fill=fill, anchor=anchor)

def pill(d, x, y, s, bg, fg=WHITE, size=12, anchor="l"):
    f = font(FB, size); tw = d.textlength(s, font=f) / S
    pw, ph = tw + 14, size + 8
    if anchor == "r": x -= pw
    rbox(d, x, y, x + pw, y + ph, bg, r=ph / 2)
    text(d, x + pw / 2, y + ph / 2, s, f, fg, "mm")
    return pw

def football(d, cx, cy, w=22, h=13):
    d.ellipse(R(cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2), fill=(140, 80, 40))
    d.line(R(cx - w * .22, cy, cx + w * .22, cy), fill=WHITE, width=2 * S)
    for i in range(-2, 3):
        x = cx + i * w * .09
        d.line(R(x, cy - 2.5, x, cy + 2.5), fill=WHITE, width=1 * S)

def button(d, x0, y0, x1, y1, label, sub=None, hi=False, icon=None):
    rbox(d, x0, y0, x1, y1, TILE_HI if hi else TILE, r=12, outline=(255, 255, 255) if hi else EDGE, width=2 if hi else 1)
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    if icon == "auto":
        auto_icon(d, x0 + 30, cy, 13); cx += 14
    if icon == "home":
        home_icon(d, x0 + 30, cy, 13); cx += 14
    if sub:
        text(d, cx, cy - 8, label, font(FB, 18), WHITE, "mm")
        text(d, cx, cy + 12, sub, font(FR, 11), GREY, "mm")
    else:
        text(d, cx, cy, label, font(FB, 18), WHITE, "mm")

def auto_icon(d, cx, cy, r, col=WHITE, w=3):
    d.arc(R(cx - r, cy - r, cx + r, cy + r), 200, 340, fill=col, width=w * S)
    d.arc(R(cx - r, cy - r, cx + r, cy + r), 20, 160, fill=col, width=w * S)
    for ang, sgn in ((340, 1), (160, 1)):
        a = math.radians(ang); px, py = cx + r * math.cos(a), cy + r * math.sin(a)
        t = math.radians(ang + 90)
        tip = (px + 6 * math.cos(t), py + 6 * math.sin(t))
        n = math.radians(ang)
        p1 = (px + 6 * math.cos(n), py + 6 * math.sin(n)); p2 = (px - 6 * math.cos(n), py - 6 * math.sin(n))
        d.polygon([(tip[0] * S, tip[1] * S), (p1[0] * S, p1[1] * S), (p2[0] * S, p2[1] * S)], fill=col)

def home_icon(d, cx, cy, r, col=WHITE):
    d.polygon([((cx - r) * S, (cy - 1) * S), (cx * S, (cy - r) * S), ((cx + r) * S, (cy - 1) * S)], fill=col)
    d.rectangle(R(cx - r * .65, cy - 2, cx + r * .65, cy + r * .8), fill=col)
    d.rectangle(R(cx - 3, cy + 3, cx + 3, cy + r * .8), fill=TILE)

def remote_icon(d, cx, cy, col=WHITE):
    rbox(d, cx - 5, cy - 9, cx + 5, cy + 9, None, r=3, outline=col, width=2)
    d.ellipse(R(cx - 2, cy - 5, cx + 2, cy - 1), fill=col)

def finish(img, name):
    out = img.convert("RGB").resize((W, H), Image.LANCZOS)
    return out

# ---------------------------------------------------------------- home
def home(rz=False):
    img, d = new()
    text(d, 12, 16, "My Teams", font(FB, 16), WHITE, "lm")
    text(d, 240, 16, "Tue 7:42 PM", font(FS, 13), GREY, "mm")
    # Big board found on Wi-Fi -> small Board button
    rbox(d, 376, 3, 474, 29, TILE, r=13, outline=EDGE)
    remote_icon(d, 394, 16)
    text(d, 432, 16, "BOARD", font(FB, 12), WHITE, "mm")
    tiles = [
        dict(logo="nfl/nyg.png", live=True, big="21 - 17", small="Q3 4:12  vs PHI") if not rz else
        dict(logo="nfl/nyg.png", live=True, big="21 - 17", small="RED ZONE  1st & Goal", rz=True),
        dict(logo="nhl/nyr.png", live=True, big="2 - 1", small="2nd 8:31  vs PIT"),
        dict(logo="ncaa/57.png", big="Final 31-24", small="Win vs LSU", win=True),
        dict(logo="mlb/nyy.png", big="Wed 7:08 PM", small="vs Boston", smallbig=True),
        dict(logo="nba/ny.png", big="Fri 7:30 PM", small="vs Boston", smallbig=True),
        dict(auto=True),
    ]
    for i, t in enumerate(tiles):
        col, row = i % 3, i // 3
        x0, y0 = 6 + col * 158, 34 + row * 143
        x1, y1 = x0 + 152, y0 + 137
        cx = (x0 + x1) / 2
        if t.get("auto"):
            rbox(d, x0, y0, x1, y1, (24, 40, 70), r=14, outline=(70, 110, 190), width=2)
            auto_icon(d, cx, y0 + 50, 26, w=5)
            text(d, cx, y0 + 98, "AUTO", font(FB, 24), WHITE, "mm")
            text(d, cx, y0 + 120, "rotate my teams", font(FR, 12), (170, 190, 230), "mm")
            continue
        live = t.get("live")
        rbox(d, x0, y0, x1, y1, TILE, r=14, outline=RED if live else EDGE, width=2 if live else 1)
        paste_logo(img, t["logo"], cx, y0 + 46, 74)
        if live: pill(d, x0 + 8, y0 + 8, "LIVE", RED, size=11)
        if t.get("smallbig"):
            text(d, cx, y0 + 104, t["big"], font(FB, 19), WHITE, "mm")
        else:
            text(d, cx, y0 + 104, t["big"], font(FB, 24 if live else 21), WHITE, "mm")
        if t.get("rz"):
            rbox(d, x0 + 10, y0 + 116, x1 - 10, y0 + 133, RED, r=8)
            text(d, cx, y0 + 125, t["small"], font(FB, 11), WHITE, "mm")
        else:
            text(d, cx, y0 + 125, t["small"], font(FS, 12), GREEN if t.get("win") else GREY, "mm")
    return finish(img, "1_home" + ("_rz" if rz else ""))

def game_bottom(d, auto_on=False, mid=None):
    button(d, 8, 266, 168, 314, "HOME", icon="home")
    button(d, 312, 266, 472, 314, "AUTO", icon="auto", hi=auto_on)
    if mid: text(d, 240, 290, mid, font(FS, 12), GREY, "mm")

def teams_row(d, img, away, home_, ascore, hscore, a_dim=False, h_dim=False):
    paste_logo(img, away["logo"], 70, 98, 110, maxw=98)
    paste_logo(img, home_["logo"], 410, 98, 110, maxw=98)
    text(d, 70, 166, away["name"], font(FS, 13), GREY, "mm")
    text(d, 410, 166, home_["name"], font(FS, 13), GREY, "mm")
    if ascore is not None:
        text(d, 168, 96, ascore, font(FB, 62), DIM if a_dim else WHITE, "mm")
        text(d, 312, 96, hscore, font(FB, 62), DIM if h_dim else WHITE, "mm")

# ---------------------------------------------------------- live game
def live(rz=False, alert=False, opp=False, bad=None):
    img, d = new()
    pill(d, 10, 7, "LIVE", RED, size=12)
    text(d, 62, 17, "NFL  Week 5", font(FS, 13), GREY, "lm")
    if not rz: text(d, 470, 17, "FOX", font(FB, 13), GREY, "rm")
    teams_row(d, img, dict(logo="nfl/phi.png", name="Eagles 3-1"), dict(logo="nfl/nyg.png", name="Giants 3-1"), "17", "21")
    if rz: pill(d, 470, 7, "RED ZONE", RED, size=12, anchor="r")
    text(d, 240, 70, "3RD", font(FS, 13), RED if rz else GREY, "mm")
    text(d, 240, 94, "4:12", font(FB, 28), RED if rz else WHITE, "mm")
    for side, n in ((168, 2), (312, 3)):
        for k in range(3):
            d.rounded_rectangle(R(side - 22 + k * 16, 130, side - 10 + k * 16, 134), 2 * S, fill=YELLOW if k < n else DIM)
    football(d, 168 if opp else 312, 150)   # who has the ball
    text(d, 240, 122, "1st & Goal" if rz else "2nd & 6", font(FB, 16), RED if rz else YELLOW, "mm")
    text(d, 240, 142, ("at NYG 8" if opp else "at PHI 8") if rz else "at PHI 34", font(FS, 12), GREY, "mm")
    # field: away (PHI) end zone left, home (NYG) right; Giants attack left
    fx0, fx1, fy0, fy1 = 12, 468, 184, 222
    ez = 30
    gx0, gx1 = fx0 + ez, fx1 - ez
    yd = (gx1 - gx0) / 100
    d.rectangle(R(gx0, fy0, gx1, fy1), fill=(38, 120, 52))
    if opp: d.rectangle(R(gx1 - 20 * yd, fy0, gx1, fy1), fill=(200, 40, 40))
    else: d.rectangle(R(gx0, fy0, gx0 + 20 * yd, fy1), fill=(200, 40, 40) if rz else (130, 60, 50))      # red zone tint
    for y in range(10, 100, 10):
        x = gx0 + y * yd
        d.line(R(x, fy0, x, fy1), fill=(90, 170, 100) if y != 50 else (160, 210, 165), width=S)
    d.rounded_rectangle(R(fx0, fy0, gx0, fy1), 6 * S, fill=(0, 76, 84))
    d.rectangle(R(gx0 - 6, fy0, gx0, fy1), fill=(0, 76, 84))
    d.rounded_rectangle(R(gx1, fy0, fx1, fy1), 6 * S, fill=(11, 34, 101))
    d.rectangle(R(gx1, fy0, gx1 + 6, fy1), fill=(11, 34, 101))
    text(d, (fx0 + gx0) / 2, (fy0 + fy1) / 2, "PHI", font(FB, 11), WHITE, "mm")
    text(d, (gx1 + fx1) / 2, (fy0 + fy1) / 2, "NYG", font(FB, 11), WHITE, "mm")
    if rz: rbox(d, fx0 - 3, fy0 - 3, fx1 + 3, fy1 + 3, None, r=8, outline=RED, width=2)
    lx = gx0 + 28 * yd
    if not rz: d.line(R(lx, fy0, lx, fy1), fill=YELLOW, width=2 * S)
    bx = gx1 - 8 * yd if opp else gx0 + (8 if rz else 34) * yd
    d.line(R(bx, fy0, bx, fy1), fill=(80, 150, 255), width=2 * S)
    football(d, bx, (fy0 + fy1) / 2, 16, 10)
    text(d, 240, 240, ("Eagles pass for 19 yards to the NYG 8" if opp else "Pass complete for 26 yards to the PHI 8") if rz else "Run up the middle for 4 yards", font(FR, 12), GREY, "mm")
    game_bottom(d)
    if alert:
        ov = Image.new("RGBA", img.size, (0, 0, 0, 0)); od = ImageDraw.Draw(ov)
        od.rectangle(R(0, 0, W, H), fill=(0, 0, 0, 120))
        img.alpha_composite(ov)
        for k in range(10):
            x = -40 + k * 56
            d.polygon([(x * S, 108 * S), ((x + 28) * S, 108 * S), ((x + 58) * S, 212 * S), ((x + 30) * S, 212 * S)], fill=(180, 20, 28))
        d.rectangle(R(0, 108, W, 212), outline=None)
        band = Image.new("RGBA", img.size, (0, 0, 0, 0)); bd = ImageDraw.Draw(band)
        bd.rectangle(R(0, 108, W, 212), fill=(226, 40, 46, 150))
        img.alpha_composite(band)
        d = ImageDraw.Draw(img)
        d.rectangle(R(0, 104, W, 108), fill=WHITE); d.rectangle(R(0, 212, W, 216), fill=WHITE)
        paste_logo(img, "nfl/nyg.png", 70, 160, 80)
        text(d, 283, 147, "RED ZONE", font(FB, 50), (0, 0, 0), "mm")
        text(d, 280, 144, "RED ZONE", font(FB, 50), WHITE, "mm")
        text(d, 280, 188, "Giants 1st & Goal at the PHI 8", font(FB, 15), WHITE, "mm")
    if bad:
        ov = Image.new("RGBA", img.size, (0, 0, 0, 0)); od = ImageDraw.Draw(ov)
        od.rectangle(R(0, 0, W, H), fill=(0, 0, 0, 150))
        img.alpha_composite(ov); d = ImageDraw.Draw(img)
        if bad == "defense":
            d.rectangle(R(0, 108, W, 212), fill=(16, 16, 18))
            for y0 in (100, 212):           # hazard tape top and bottom
                d.rectangle(R(0, y0, W, y0 + 10), fill=(250, 200, 20))
                for k in range(-1, 26):
                    x = k * 20
                    d.polygon([(x * S, (y0 + 10) * S), ((x + 10) * S, y0 * S), ((x + 18) * S, y0 * S), ((x + 8) * S, (y0 + 10) * S)], fill=(16, 16, 18))
            # warning triangle
            tx, ty = 62, 160
            d.polygon([(tx * S, (ty - 34) * S), ((tx + 38) * S, (ty + 30) * S), ((tx - 38) * S, (ty + 30) * S)], fill=(250, 200, 20))
            text(d, tx, ty + 8, "!", font(FB, 40), (16, 16, 18), "mm")
            text(d, 290, 146, "DEFENSE!", font(FB, 50), (250, 200, 20), "mm")
            paste_logo(img, "nfl/phi.png", 160, 189, 22)
            text(d, 300, 189, "Eagles 1st & Goal at the NYG 8", font(FB, 15), WHITE, "mm")
        else:
            d.rectangle(R(0, 108, W, 212), fill=(30, 30, 34))
            d.rectangle(R(0, 104, W, 108), fill=(90, 20, 24)); d.rectangle(R(0, 212, W, 216), fill=(90, 20, 24))
            paste_logo(img, "nfl/phi.png", 70, 160, 80)
            text(d, 283, 147, "UH OH...", font(FB, 50), (0, 0, 0), "mm")
            text(d, 280, 144, "UH OH...", font(FB, 50), (200, 60, 60), "mm")
            text(d, 280, 188, "Eagles in the red zone, NYG 8", font(FB, 15), GREY, "mm")
    return finish(img, "2_live" + ("_rz" if rz else "") + ("_alert" if alert else "") + ("_opp" if opp else "") + ("_" + bad if bad else ""))

# ---------------------------------------------------------- final
def final():
    img, d = new()
    pill(d, 10, 7, "FINAL", (70, 76, 92), size=12)
    text(d, 72, 17, "College Football  Sat Oct 3", font(FS, 13), GREY, "lm")
    text(d, 470, 17, "SEC", font(FB, 13), GREY, "rm")
    teams_row(d, img, dict(logo="ncaa/99.png", name="LSU 4-1"), dict(logo="ncaa/57.png", name="Florida 4-1"), "24", "31", a_dim=True)
    text(d, 240, 96, "FINAL", font(FB, 22), WHITE, "mm")
    text(d, 312, 136, "WIN", font(FB, 13), GREEN, "mm")
    rbox(d, 40, 190, 440, 244, TILE, r=12)
    text(d, 60, 206, "NEXT GAME", font(FB, 11), GREY, "lm")
    text(d, 60, 228, "Sat Oct 10, 3:30 PM  at Tennessee", font(FS, 15), WHITE, "lm")
    paste_logo(img, "ncaa/2633.png", 410, 217, 38)
    game_bottom(d, auto_on=True, mid="Auto: next in 14s")
    return finish(img, "3_final")

# ---------------------------------------------------------- upcoming
def upcoming():
    img, d = new()
    pill(d, 10, 7, "TOMORROW", (40, 90, 170), size=12)
    text(d, 104, 17, "AL Division Series  Game 2", font(FS, 13), GREY, "lm")
    text(d, 470, 17, "TBS", font(FB, 13), GREY, "rm")
    teams_row(d, img, dict(logo="mlb/bos.png", name="Red Sox 92-70"), dict(logo="mlb/nyy.png", name="Yankees 96-66"), None, None)
    text(d, 240, 70, "WED OCT 7", font(FB, 15), GREY, "mm")
    text(d, 240, 102, "7:08 PM", font(FB, 40), WHITE, "mm")
    text(d, 240, 136, "Yankee Stadium", font(FS, 13), GREY, "mm")
    rbox(d, 40, 190, 440, 244, TILE, r=12)
    text(d, 240, 206, "SERIES", font(FB, 11), GREY, "mm")
    text(d, 240, 228, "Yankees lead 1-0", font(FS, 16), WHITE, "mm")
    game_bottom(d)
    return finish(img, "4_upcoming")

# ---------------------------------------------------------- animation
def anim():
    img, d = new()
    cx, cy = 240, 120
    cols = [(11, 34, 101), (167, 25, 48)]
    for k in range(24):
        a0 = k * 15; c = cols[k % 2]
        d.pieslice(R(cx - 420, cy - 420, cx + 420, cy + 420), a0, a0 + 15, fill=c)
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0)); gd = ImageDraw.Draw(glow)
    gd.ellipse(R(cx - 95, cy - 95, cx + 95, cy + 95), fill=(255, 255, 255, 120))
    glow = glow.filter(ImageFilter.GaussianBlur(18 * S))
    img.alpha_composite(glow)
    paste_logo(img, "nfl/nyg.png", cx, cy, 150)
    f = font(FB, 52)
    text(d, 243, 243, "TOUCHDOWN", f, (0, 0, 0), "mm")
    text(d, 240, 240, "TOUCHDOWN", f, WHITE, "mm")
    rbox(d, 130, 278, 350, 310, (0, 0, 0, 255), r=16)
    text(d, 240, 294, "GIANTS 28   EAGLES 17", font(FB, 15), YELLOW, "mm")
    return finish(img, "5_touchdown")

# ---------------------------------------------------------- remote
def remote():
    img, d = new()
    text(d, 12, 17, "Big Board Remote", font(FB, 16), WHITE, "lm")
    d.ellipse(R(370, 12, 380, 22), fill=GREEN)
    text(d, 386, 17, "Connected", font(FS, 13), GREY, "lm")
    teams = [("nfl/nyg.png", "GIANTS"), ("ncaa/57.png", "GATORS"), ("mlb/nyy.png", "YANKEES"),
             ("nhl/nyr.png", "RANGERS"), ("nba/ny.png", "KNICKS")]
    for i, (lg, nm) in enumerate(teams):
        x0 = 8 + i * 94; x1 = x0 + 88
        rbox(d, x0, 36, x1, 150, TILE, r=12, outline=EDGE)
        paste_logo(img, lg, (x0 + x1) / 2, 82, 64)
        text(d, (x0 + x1) / 2, 134, nm, font(FB, 12), WHITE, "mm")
    modes = [("AUTO", "auto", True), ("ALL NFL", None, False), ("ALL COLLEGE", None, False), ("FULL GAME", None, False)]
    for i, (lb, ic, hi) in enumerate(modes):
        x0 = 8 + i * 117; x1 = x0 + 113
        rbox(d, x0, 158, x1, 252, TILE_HI if hi else TILE, r=12, outline=WHITE if hi else EDGE, width=2 if hi else 1)
        cx = (x0 + x1) / 2
        if ic:
            auto_icon(d, cx, 192, 13, w=3)
            text(d, cx, 226, lb, font(FB, 15), WHITE, "mm")
        else:
            text(d, cx, 205, lb, font(FB, 15), WHITE, "mm")
    button(d, 8, 266, 168, 314, "BACK")
    text(d, 184, 280, "Board is showing", font(FS, 11), GREY, "lm")
    text(d, 184, 299, "AUTO  Giants 21-17 Eagles", font(FB, 14), WHITE, "lm")
    return finish(img, "6_remote")

if len(sys.argv) > 1 and sys.argv[1] == "rz":
    shots = [("1. Home (Giants in red zone)", home(rz=True)), ("2. Red zone alert (3 sec)", live(rz=True, alert=True)),
             ("3. Game screen in red zone", live(rz=True))]
    name = "mini_redzone.png"
elif len(sys.argv) > 1 and sys.argv[1] == "opp":
    shots = [("Ours: RED ZONE (good)", live(rz=True, alert=True)), ("Theirs, option A: DEFENSE!", live(rz=True, opp=True, bad="defense")),
             ("Theirs, option B: UH OH...", live(rz=True, opp=True, bad="uhoh"))]
    name = "mini_redzone_opp.png"
else:
    shots = [("1. Home", home()), ("2. Live game", live()), ("3. Final (Auto on)", final()),
             ("4. Upcoming game", upcoming()), ("5. Touchdown animation", anim()), ("6. Big board remote", remote())]
    name = "mini_mockups.png"
gap, lab = 24, 34
rows = (len(shots) + 2) // 3
sheet = Image.new("RGB", (3 * W + 4 * gap, rows * (H + lab + gap) + gap), (235, 236, 240))
sd = ImageDraw.Draw(sheet); lf = ImageFont.truetype(FB, 20)
for i, (nm, im) in enumerate(shots):
    c, r = i % 3, i // 3
    x = gap + c * (W + gap); y = gap + r * (H + lab + gap)
    sd.text((x, y), nm, font=lf, fill=(30, 30, 40))
    sheet.paste(im, (x, y + lab))
sheet.save(os.path.join(OUT, name))
print(sheet.size)
