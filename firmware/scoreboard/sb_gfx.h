// A 64x64 frame of RGB pixels, and the drawing primitives the preview uses:
// put(), text() with the two pixel fonts, sprite(). Plain C++ so the same
// code can be compiled on a computer and checked against the browser preview.
#pragma once
#include <stdint.h>

static const int W = 64, H = 64;

typedef uint32_t RGB;   // 0xRRGGBB; 0 = LED off
static inline RGB rgb(int r, int g, int b) {
  if (r < 0) r = 0; if (r > 255) r = 255;
  if (g < 0) g = 0; if (g > 255) g = 255;
  if (b < 0) b = 0; if (b > 255) b = 255;
  return ((RGB)r << 16) | ((RGB)g << 8) | (RGB)b;
}

static const RGB WHITE = 0xFFFFFF, DIM = 0x787878, GRAY = 0x5A5A5A,
  GOLD = 0xFFBE00, RED = 0xFF2828, GREEN = 0x00E650, BROWN = 0xBE5F1E,
  LINE = 0x696969, CLOCK = 0xEBEBEB, DATEC = 0x8C8C8C, BLACK = 0x000000,
  RANKC = 0xBEBEBE, PLAYOFF_GOLD = 0xE6AA00, PRESEASON_SILVER = 0xA5AAB9;

struct Frame {
  RGB px[W * H];
  bool lit[W * H];   // drawn at all (a black logo halo still counts)
  void clear();
  inline void put(int x, int y, RGB c) {
    if (x >= 0 && x < W && y >= 0 && y < H) { px[y * W + x] = c; lit[y * W + x] = true; }
  }
  inline void unput(int x, int y) {
    if (x >= 0 && x < W && y >= 0 && y < H) { px[y * W + x] = 0; lit[y * W + x] = false; }
  }
};

enum Font : uint8_t { F3 = 0, F5 = 1 };

int tw(const char* s, Font f, int sc = 1);
void text(Frame& fb, int x, int y, const char* s, RGB c, Font f, int sc = 1);
// rows of '0' (off), '1' (c1), anything else (c2)
void sprite(Frame& fb, int x, int y, const char* const* rows, int n, RGB c1, RGB c2 = WHITE);

// Team colours: navy and forest green read as black on a panel, so lift them
// until they light. No colour at all -> white.
RGB ledColor(bool has, uint32_t hex);
bool hexRgb(const char* s, uint32_t& out);
