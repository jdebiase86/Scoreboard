/*
  Scoreboard hardware test - Seengreat "RGB Matrix HUB75 S3" + 64x64 P3 panel

  Proves the panel, the controller and the power all work before any real
  firmware goes on. It loops through five checks, about 3 seconds each, and
  prints what you should be seeing to the Serial Monitor (115200 baud).

    1. CORNERS   - black screen, grey border all the way round, a coloured
                   square in each corner: RED top-left, GREEN top-right,
                   BLUE bottom-left, WHITE bottom-right.
                   Checks orientation and that every edge row/column lights.
    2. COLOURS   - whole panel red, then green, then blue (kept dim).
                   Checks every LED in every colour.
    3. BANDS     - eight horizontal stripes, 8 rows each, all different
                   colours. Checks the row-select lines A-E. If the bottom
                   half just repeats the top half, the E line isn't working.
    4. TEXT      - "SCORE" / "BOARD" / "OK!" in three colours.
    5. MOTION    - a ball bouncing round the panel. Should be smooth, no
                   flicker or tearing.

  Pins are from Seengreat's own wiki: seengreat.com/wiki/214/rgb-matrix-hub75-s3

  Needs (Arduino IDE -> Tools -> Manage Libraries):
    "ESP32 HUB75 LED MATRIX PANEL DMA Display" by mrcodetastic
    "Adafruit GFX Library" (it asks to install this with the one above)

  Board settings (Tools menu):
    Board:            ESP32S3 Dev Module
    USB CDC On Boot:  Enabled
    Flash Size:       16MB
    PSRAM:            OPI PSRAM
*/

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#define PANEL_W 64
#define PANEL_H 64
#define CHAIN   1

// Seengreat RGB Matrix HUB75 S3 -> HUB75 signals
#define R1_PIN   5
#define G1_PIN   4
#define B1_PIN   6
#define R2_PIN  15
#define G2_PIN   7
#define B2_PIN  17
#define A_PIN    8
#define B_PIN   18
#define C_PIN   10
#define D_PIN    9
#define E_PIN   16   // 64x64 panels need the E line
#define LAT_PIN 11
#define OE_PIN  13
#define CLK_PIN 12

// Brightness 0-255. Kept modest for testing: a full-white 64x64 panel at
// full brightness can pull more than the 4A supply gives.
#define BRIGHTNESS 70

MatrixPanel_I2S_DMA *panel = nullptr;

uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return panel->color565(r, g, b); }

void say(const char *msg) { Serial.println(msg); }

void testCorners() {
  say("1. CORNERS: grey border; RED top-left, GREEN top-right, BLUE bottom-left, WHITE bottom-right");
  panel->clearScreen();
  panel->drawRect(0, 0, PANEL_W, PANEL_H, rgb(90, 90, 90));
  panel->fillRect(1, 1, 6, 6, rgb(255, 0, 0));
  panel->fillRect(PANEL_W - 7, 1, 6, 6, rgb(0, 255, 0));
  panel->fillRect(1, PANEL_H - 7, 6, 6, rgb(0, 0, 255));
  panel->fillRect(PANEL_W - 7, PANEL_H - 7, 6, 6, rgb(255, 255, 255));
  delay(4000);
}

void testColours() {
  say("2. COLOURS: whole panel red, then green, then blue");
  panel->fillScreenRGB888(160, 0, 0);  delay(1500);
  panel->fillScreenRGB888(0, 160, 0);  delay(1500);
  panel->fillScreenRGB888(0, 0, 160);  delay(1500);
}

void testBands() {
  say("3. BANDS: 8 stripes, all different colours, top to bottom. Bottom half must NOT repeat the top half.");
  const uint8_t c[8][3] = {{255,0,0},{255,120,0},{255,255,0},{0,255,0},
                           {0,255,255},{0,0,255},{170,0,255},{255,255,255}};
  panel->clearScreen();
  for (int i = 0; i < 8; i++)
    panel->fillRect(0, i * 8, PANEL_W, 8, rgb(c[i][0] / 2, c[i][1] / 2, c[i][2] / 2));
  delay(4000);
}

void testText() {
  say("4. TEXT: SCORE / BOARD / OK!");
  panel->clearScreen();
  panel->setTextWrap(false);
  panel->setTextSize(1);
  panel->setTextColor(rgb(255, 255, 255)); panel->setCursor(17, 12); panel->print("SCORE");
  panel->setTextColor(rgb(0, 120, 255));   panel->setCursor(17, 28); panel->print("BOARD");
  panel->setTextColor(rgb(0, 230, 80));    panel->setCursor(23, 44); panel->print("OK!");
  delay(4000);
}

void testMotion() {
  say("5. MOTION: a ball bouncing round the panel - should be smooth");
  float x = 10, y = 20, vx = 1.3, vy = 0.9;
  for (int f = 0; f < 240; f++) {
    panel->fillCircle((int)x, (int)y, 3, 0);
    x += vx; y += vy;
    if (x < 3 || x > PANEL_W - 4) vx = -vx;
    if (y < 3 || y > PANEL_H - 4) vy = -vy;
    panel->fillCircle((int)x, (int)y, 3, rgb(235, 120, 30));
    delay(16);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  say("Scoreboard hardware test starting");

  HUB75_I2S_CFG::i2s_pins pins = {R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN,
                                  A_PIN, B_PIN, C_PIN, D_PIN, E_PIN,
                                  LAT_PIN, OE_PIN, CLK_PIN};
  HUB75_I2S_CFG cfg(PANEL_W, PANEL_H, CHAIN, pins);
  // If the panel stays black or shows garbage, the panel may use a
  // different driver chip: uncomment this one line and upload again.
  // cfg.driver = HUB75_I2S_CFG::FM6126A;

  panel = new MatrixPanel_I2S_DMA(cfg);
  if (!panel->begin()) {
    say("Panel driver failed to start - check the board selection and PSRAM setting");
    while (true) delay(1000);
  }
  panel->setBrightness8(BRIGHTNESS);
  panel->clearScreen();
  say("Panel started. Running the 5 checks on a loop.");
}

void loop() {
  testCorners();
  testColours();
  testBands();
  testText();
  testMotion();
}
