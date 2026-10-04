import sys, os
sys.path.insert(0, os.path.dirname(__file__))
import mock_data as M
import render as R

OUT = os.path.join(os.path.dirname(__file__), "..", "out")
os.makedirs(OUT, exist_ok=True)

# Joe's board: Giants game pinned on top, full NFL slate scrolling below
game = M.nfl_live_giants()
frames = R.render_animation(game, M.TEAM_COLORS, scale=5, step=2)
print("frames:", len(frames))

path = os.path.join(OUT, "joe_board.gif")
frames[0].save(
    path,
    save_all=True,
    append_images=frames[1:],
    duration=50,     # 20fps
    loop=0,
    optimize=True,
)
print("wrote", path, os.path.getsize(path) // 1024, "KB")

# A single still too, for a close look at the top section
still = R.render_scaled(game, M.TEAM_COLORS, ticker_offset=0, scale=10)
still_path = os.path.join(OUT, "joe_board_still.png")
still.save(still_path)
print("wrote", still_path)
