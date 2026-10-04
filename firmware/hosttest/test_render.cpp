// Host test: renders the preview's sample games with the firmware renderer
// and compares every pixel with what the browser preview drew.
#include <ArduinoJson.h>
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include "../scoreboard/sb_render.h"

void* sbAlloc(size_t n) { return malloc(n); }
void sbFree(void* p) { free(p); }
void sbBreathe() {}

#include "preview_json.h"

static RGB parseRgb(const char* s) {
  int r, g, b;
  if (sscanf(s, "rgb(%d,%d,%d)", &r, &g, &b) == 3) return rgb(r, g, b);
  return 0xFFFFFFFF;
}

static void writePpm(const char* path, const Frame& fb) {
  FILE* f = fopen(path, "wb");
  fprintf(f, "P6 %d %d 255\n", W * 8, H * 8);
  for (int y = 0; y < H * 8; y++)
    for (int x = 0; x < W * 8; x++) {
      RGB c = fb.px[(y / 8) * W + x / 8];
      bool dot = (x % 8) && (y % 8) && (x % 8 < 7) && (y % 8 < 7);
      unsigned char p[3] = {(unsigned char)(dot ? (c >> 16) : 0), (unsigned char)(dot ? (c >> 8) : 0),
                            (unsigned char)(dot ? c : 0)};
      if (dot && !fb.lit[(y / 8) * W + x / 8]) p[0] = p[1] = p[2] = 22;
      fwrite(p, 1, 3, f);
    }
  fclose(f);
}

int main(int argc, char** argv) {
  std::ifstream in(argc > 1 ? argv[1] : "/tmp/js_frames.json");
  JsonDocument doc;
  if (deserializeJson(doc, in)) { std::cerr << "bad json\n"; return 2; }
  static Frame fb;
  static Game g;
  int bad = 0, cases = 0;
  for (JsonPairConst kv : doc.as<JsonObjectConst>()) {
    cases++;
    fromPreview(kv.value()["game"], g);
    int pair = kv.value()["pair"];
    renderGame(fb, g, pair, nullptr, nullptr);
    JsonArrayConst px = kv.value()["px"];
    int diff = 0, first = -1;
    for (int i = 0; i < W * H; i++) {
      RGB want = px[i].isNull() ? 0 : parseRgb(px[i]);
      bool wantLit = !px[i].isNull();
      RGB have = fb.lit[i] ? fb.px[i] : 0;
      if (want != have || wantLit != fb.lit[i]) { if (first < 0) first = i; diff++; }
    }
    if (diff) {
      bad++;
      printf("%-22s %4d px differ (first at x=%d y=%d: want %s have %06X)\n", kv.key().c_str(), diff,
             first % W, first / W, px[first].isNull() ? "off" : px[first].as<const char*>(), fb.px[first]);
    }
    if (pair == 0) {
      std::string p = std::string("/tmp/cpp_") + kv.key().c_str() + ".ppm";
      for (auto& ch : p) if (ch == '@') ch = '_';
      writePpm(p.c_str(), fb);
    }
  }
  printf("%d cases, %d differ\n", cases, bad);
  return bad ? 1 : 0;
}
