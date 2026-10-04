#include "sb_panel.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <new>
#include "sb_logo.h"   // sbAlloc

// Seengreat RGB Matrix HUB75 S3 -> HUB75 (from Seengreat's wiki; proven
// by the hardware test)
#define R1_PIN 5
#define G1_PIN 4
#define B1_PIN 6
#define R2_PIN 15
#define G2_PIN 7
#define B2_PIN 17
#define A_PIN 8
#define B_PIN 18
#define C_PIN 10
#define D_PIN 9
#define E_PIN 16
#define LAT_PIN 11
#define OE_PIN 13
#define CLK_PIN 12

static MatrixPanel_I2S_DMA* panel = nullptr;
static Frame* scratch = nullptr;   // PSRAM

bool panelBegin(uint8_t brightness, bool clockOnFalling) {
  HUB75_I2S_CFG::i2s_pins pins = {R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN,
                                  B_PIN,  C_PIN,  D_PIN,  E_PIN,  LAT_PIN, OE_PIN, CLK_PIN};
  HUB75_I2S_CFG cfg(64, 64, 1, pins);
  cfg.double_buff = true;   // draw the next frame off-screen, then swap: no tearing
  cfg.clkphase = !clockOnFalling;
  panel = new MatrixPanel_I2S_DMA(cfg);
  if (!panel->begin()) return false;
  panel->setBrightness8(brightness);
  panel->clearScreen();
  return true;
}

void panelBrightness(uint8_t b) {
  if (panel) panel->setBrightness8(b);
}

void panelShow(const Frame& fb) {
  if (!panel) return;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      RGB c = fb.px[y * W + x];
      panel->drawPixelRGB888(x, y, (c >> 16) & 255, (c >> 8) & 255, c & 255);
    }
  panel->flipDMABuffer();
}

// Centred the same way the scoreboard screens are
void drawMessage(Frame& fb, const Line* lines, int n, int top) {
  fb.clear();
  int h = 0;
  for (int i = 0; i < n; i++) h += (lines[i].big ? 7 : 5) + (i ? 3 : 0);
  int y = top >= 0 ? top : (H - h) / 2;
  for (int i = 0; i < n; i++) {
    Font f = lines[i].big ? F5 : F3;
    if (lines[i].text && *lines[i].text) text(fb, (W - tw(lines[i].text, f)) / 2, y, lines[i].text, lines[i].color, f);
    y += (lines[i].big ? 7 : 5) + 3;
  }
}

void showMessage(const Line* lines, int n) {
  if (!scratch) scratch = new (sbAlloc(sizeof(Frame))) Frame();
  drawMessage(*scratch, lines, n);
  panelShow(*scratch);
}
