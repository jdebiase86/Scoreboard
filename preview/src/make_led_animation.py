import sys, os
sys.path.insert(0, os.path.dirname(__file__))
import copy
import mock_data as M, led_render as L

OUT = os.path.join(os.path.dirname(__file__), "..", "out")

def tick(clock, seconds):
    m, s = clock.split(":")
    total = int(m) * 60 + int(s) - seconds
    if total < 0:
        total = 0
    return f"{total//60}:{total%60:02d}"

base = M.nfl_live_giants()
slate = base["ticker_games"]
pairs = (len(slate) + 1) // 2

FPS = 1
HOLD_S = 4          # seconds each pair of ticker games stays up
frames = []
sec = 0
for p in range(pairs):
    for f in range(FPS * HOLD_S):
        g = copy.deepcopy(base)
        g["clock"] = tick(base["clock"], sec // FPS)
        frames.append(L.render(g, M.TEAM_COLORS, ticker_pair=p, scale=7))
        sec += 1

path = os.path.join(OUT, "joe_board_led.gif")
frames[0].save(path, save_all=True, append_images=frames[1:],
               duration=int(1000 / FPS), loop=0, optimize=True)
print("wrote", path, os.path.getsize(path)//1024, "KB", len(frames), "frames")
