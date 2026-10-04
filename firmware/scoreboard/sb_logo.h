// Team logos shrunk to a handful of LEDs - the same method as the preview:
// crop the empty margin, then for each LED either "snap" to the logo's own
// palette (big celebration logos) or take the dominant colour of its patch
// (small pregame logos), drop near-black and lonely pixels.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "sb_gfx.h"

struct LogoPix { uint8_t x, y, r, g, b; };

struct Logo {
  int w = 0, h = 0, n = 0;
  LogoPix* pix = nullptr;   // n entries
};

// rgba: iw x ih, 4 bytes per pixel. Fits the mark in a boxW x boxH box.
// Returns false (and an empty logo) if nothing usable was left.
bool shrinkLogo(const uint8_t* rgba, int iw, int ih, int boxW, int boxH, Logo& out);
void freeLogo(Logo& l);
void drawLogo(Frame& fb, const Logo& l, int ox, int oy, float bright);

// where shrinkLogo gets its working memory (PSRAM on the board)
void* sbAlloc(size_t n);
void sbFree(void* p);
// lets the rest of the system run during long jobs (no-op on a computer)
void sbBreathe();
