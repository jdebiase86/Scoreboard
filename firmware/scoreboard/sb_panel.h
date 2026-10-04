// The LED panel itself (Seengreat HUB75 S3 board + 64x64 P3 panel), and
// the plain-text screens shown while setting up.
#pragma once
#include "sb_gfx.h"

// clockOnFalling: clock data in on the falling edge. This panel needs it - on
// the rising edge everything lands one dot to the left (found 2026-10-03).
bool panelBegin(uint8_t brightness, bool clockOnFalling = true);
void panelBrightness(uint8_t b);
void panelShow(const Frame& fb);

struct Line { const char* text; RGB color; bool big; };
// centred lines of text, top to bottom
void drawMessage(Frame& fb, const Line* lines, int n, int top = -1);
void showMessage(const Line* lines, int n);
