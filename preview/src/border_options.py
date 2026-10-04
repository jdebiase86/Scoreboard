"""
Compare ways of drawing the two divider lines, given that very dark
greys may not survive the panel's limited color depth.

The two lines in question:
  y=38  - under the main game, above the ticker
  y=51  - between the two ticker games
"""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
from PIL import Image, ImageDraw, ImageFont
import mock_data as M
import led_render as L

W = 64
MAIN_Y = 38
TICK_Y = 51


def clear_rows(px):
    for y in (MAIN_Y, TICK_Y):
        for x in range(W):
            px.pop((x, y), None)


def style_solid_dim(px):
    for x in range(W):
        px[(x, MAIN_Y)] = (48, 48, 48)
    for x in range(5, W - 5):
        px[(x, TICK_Y)] = (48, 48, 48)


def style_solid_bright(px):
    for x in range(W):
        px[(x, MAIN_Y)] = (105, 105, 105)
    for x in range(5, W - 5):
        px[(x, TICK_Y)] = (105, 105, 105)


def style_dotted(px):
    for x in range(W):
        if x % 2 == 0:
            px[(x, MAIN_Y)] = (150, 150, 150)
    for x in range(5, W - 5):
        if x % 2 == 0:
            px[(x, TICK_Y)] = (150, 150, 150)


def style_dashed(px):
    for x in range(W):
        if (x % 5) < 3:
            px[(x, MAIN_Y)] = (135, 135, 135)
    for x in range(5, W - 5):
        if (x % 5) < 3:
            px[(x, TICK_Y)] = (135, 135, 135)


def style_none(px):
    pass  # rely on the blank rows alone


def style_blue(px):
    for x in range(W):
        px[(x, MAIN_Y)] = (30, 55, 150)
    for x in range(5, W - 5):
        px[(x, TICK_Y)] = (30, 55, 150)


def style_tick_marks(px):
    """No line at all - just short marks at each end, like a bracket."""
    for x in list(range(0, 8)) + list(range(W - 8, W)):
        px[(x, MAIN_Y)] = (140, 140, 140)
    for x in list(range(5, 12)) + list(range(W - 12, W - 5)):
        px[(x, TICK_Y)] = (120, 120, 120)


STYLES = [
    ("Current (grey 48)", style_solid_dim),
    ("Solid grey 105", style_solid_bright),
    ("Dotted 150", style_dotted),
    ("Dashed 135", style_dashed),
    ("End ticks only", style_tick_marks),
    ("No line (gap only)", style_none),
]


def main():
    out = os.path.join(os.path.dirname(__file__), "..", "out")
    game = M.nfl_live_giants()
    scale = 7
    cell = 64 * scale
    label_h = 26
    pad = 12
    cols = 3
    rows = (len(STYLES) + cols - 1) // cols
    sheet = Image.new("RGB",
                      (cols * (cell + pad) + pad,
                       rows * (cell + label_h + pad) + pad), (18, 18, 18))
    d = ImageDraw.Draw(sheet)
    try:
        f = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 17)
    except Exception:
        f = ImageFont.load_default()

    for i, (name, fn) in enumerate(STYLES):
        px = L.render_pixels(game, M.TEAM_COLORS, ticker_pair=0)
        clear_rows(px)
        fn(px)
        img = L.to_led_image(px, scale=scale)
        r, c = divmod(i, cols)
        x = pad + c * (cell + pad)
        y = pad + r * (cell + label_h + pad)
        sheet.paste(img, (x, y + label_h))
        d.text((x + 2, y + 4), name, font=f, fill=(235, 235, 235))

    p = os.path.join(out, "border_options.png")
    sheet.save(p)
    print("wrote", p)


if __name__ == "__main__":
    main()
