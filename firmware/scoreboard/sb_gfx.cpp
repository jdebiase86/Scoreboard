#include "sb_gfx.h"
#include "sb_fonts.h"
#include <string.h>
#include <ctype.h>

void Frame::clear() {
  memset(px, 0, sizeof(px));
  memset(lit, 0, sizeof(lit));
}

int tw(const char* s, Font f, int sc) {
  if (!s || !*s) return 0;
  int gw = (f == F5) ? 5 : 3;
  return (int)strlen(s) * (gw * sc + 1) - 1;
}

static const uint8_t* glyph(Font f, char ch) {
  const char* set = (f == F5) ? F5_CHARS : F3_CHARS;
  const char* p = strchr(set, ch);
  if (!p || !ch) return nullptr;
  int i = p - set;
  return (f == F5) ? F5_GLYPHS[i] : F3_GLYPHS[i];
}

void text(Frame& fb, int x, int y, const char* s, RGB c, Font f, int sc) {
  if (!s) return;
  int gw = (f == F5) ? 5 : 3, gh = (f == F5) ? 7 : 5;
  for (; *s; s++) {
    const uint8_t* g = glyph(f, (char)toupper((unsigned char)*s));
    if (g) {
      for (int ry = 0; ry < gh; ry++)
        for (int rx = 0; rx < gw; rx++)
          if (g[ry] & (1 << (gw - 1 - rx)))
            for (int sy = 0; sy < sc; sy++)
              for (int sx = 0; sx < sc; sx++)
                fb.put(x + rx * sc + sx, y + ry * sc + sy, c);
    }
    x += gw * sc + 1;
  }
}

void sprite(Frame& fb, int x, int y, const char* const* rows, int n, RGB c1, RGB c2) {
  for (int ry = 0; ry < n; ry++)
    for (int rx = 0; rows[ry][rx]; rx++) {
      char b = rows[ry][rx];
      if (b != '0') fb.put(x + rx, y + ry, b == '1' ? c1 : c2);
    }
}

bool hexRgb(const char* s, uint32_t& out) {
  if (!s) return false;
  if (*s == '#') s++;
  if (strlen(s) != 6) return false;
  uint32_t v = 0;
  for (int i = 0; i < 6; i++) {
    char ch = s[i];
    int d;
    if (ch >= '0' && ch <= '9') d = ch - '0';
    else if (ch >= 'a' && ch <= 'f') d = ch - 'a' + 10;
    else if (ch >= 'A' && ch <= 'F') d = ch - 'A' + 10;
    else return false;
    v = (v << 4) | d;
  }
  out = v;
  return true;
}

RGB ledColor(bool has, uint32_t hex) {
  if (!has) return WHITE;
  int r = (hex >> 16) & 255, g = (hex >> 8) & 255, b = hex & 255;
  int m = r > g ? (r > b ? r : b) : (g > b ? g : b);
  if (m == 0) return WHITE;
  if (m < 140) {
    double f = 190.0 / m;
    int rr = (int)(r * f), gg = (int)(g * f), bb = (int)(b * f);
    return rgb(rr > 255 ? 255 : rr, gg > 255 ? 255 : gg, bb > 255 ? 255 : bb);
  }
  return rgb(r, g, b);
}
