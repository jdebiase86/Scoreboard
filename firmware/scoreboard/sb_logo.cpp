#include "sb_logo.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>

static const int SS = 8;   // source samples per LED, each way

// Box-filter the crop (cx0,cy0,cw,ch) of the source to ow x oh, weighting
// colour by alpha so transparent pixels don't bleed dark into the edges.
static void resample(const uint8_t* src, int iw, int cx0, int cy0, int cw, int ch,
                     uint8_t* dst, int ow, int oh) {
  double sx = (double)cw / ow, sy = (double)ch / oh;
  for (int y = 0; y < oh; y++) {
    if ((y & 15) == 0) sbBreathe();
    int y0 = cy0 + (int)floor(y * sy), y1 = cy0 + (int)floor((y + 1) * sy);
    if (y1 <= y0) y1 = y0 + 1;
    for (int x = 0; x < ow; x++) {
      int x0 = cx0 + (int)floor(x * sx), x1 = cx0 + (int)floor((x + 1) * sx);
      if (x1 <= x0) x1 = x0 + 1;
      double r = 0, g = 0, b = 0, a = 0; int n = 0;
      for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++) {
          const uint8_t* p = src + ((size_t)yy * iw + xx) * 4;
          double al = p[3];
          r += p[0] * al; g += p[1] * al; b += p[2] * al; a += al; n++;
        }
      uint8_t* o = dst + ((size_t)y * ow + x) * 4;
      if (a > 0) { o[0] = (uint8_t)(r / a + .5); o[1] = (uint8_t)(g / a + .5); o[2] = (uint8_t)(b / a + .5); }
      else { o[0] = o[1] = o[2] = 0; }
      o[3] = (uint8_t)(a / n + .5);
    }
  }
}

struct Bucket { int key, n; double r, g, b; };

static inline int keyOf(const uint8_t* p) { return ((p[0] >> 5) << 10) | ((p[1] >> 5) << 5) | (p[2] >> 5); }

// The colours that make up at least 2% of the mark, near-duplicates merged
static std::vector<std::vector<double>> logoPalette(const uint8_t* bd, size_t npx) {
  std::vector<Bucket> cnt;
  int16_t* idx = (int16_t*)sbAlloc(32768 * sizeof(int16_t));
  if (!idx) return {};
  memset(idx, 0xff, 32768 * sizeof(int16_t));
  long total = 0;
  for (size_t i = 0; i < npx; i++) {
    const uint8_t* p = bd + i * 4;
    if (p[3] < 140) continue;
    total++;
    int k = keyOf(p);
    if (idx[k] < 0) { idx[k] = (int16_t)cnt.size(); cnt.push_back({k, 0, 0, 0, 0}); }
    Bucket& e = cnt[idx[k]];
    e.n++; e.r += p[0]; e.g += p[1]; e.b += p[2];
  }
  sbFree(idx);
  std::stable_sort(cnt.begin(), cnt.end(), [](const Bucket& a, const Bucket& b) { return a.n > b.n; });
  struct P { long n; double c[3]; };
  std::vector<P> pal;
  for (auto& e : cnt) {
    if (e.n < total * 0.02) break;
    double c[3] = {e.r / e.n, e.g / e.n, e.b / e.n};
    P* near = nullptr;
    for (auto& q : pal)
      if (sqrt((q.c[0]-c[0])*(q.c[0]-c[0]) + (q.c[1]-c[1])*(q.c[1]-c[1]) + (q.c[2]-c[2])*(q.c[2]-c[2])) < 70) { near = &q; break; }
    if (near) near->n += e.n;
    else pal.push_back({e.n, {c[0], c[1], c[2]}});
  }
  std::vector<std::vector<double>> out;
  for (auto& q : pal) out.push_back({q.c[0], q.c[1], q.c[2]});
  return out;
}

static void logoPixels(const uint8_t* bd, int dw, int dh, bool snap, Logo& out) {
  std::vector<LogoPix> pix;
  const int need = (int)ceil(SS * SS * 0.34);
  std::vector<std::vector<double>> pal;
  if (snap) pal = logoPalette(bd, (size_t)dw * SS * dh * SS);
  for (int y = 0; y < dh; y++)
    for (int x = 0; x < dw; x++) {
      if (!x) sbBreathe();
      int opaque = 0; double sr = 0, sg = 0, sb = 0;
      Bucket bucket[64]; int nb = 0;
      for (int by = 0; by < SS; by++)
        for (int bx = 0; bx < SS; bx++) {
          const uint8_t* p = bd + (((size_t)(y * SS + by) * dw * SS) + (x * SS + bx)) * 4;
          if (p[3] < 140) continue;
          opaque++;
          if (snap) { sr += p[0]; sg += p[1]; sb += p[2]; continue; }
          int k = keyOf(p), j = 0;
          while (j < nb && bucket[j].key != k) j++;
          if (j == nb) bucket[nb++] = {k, 0, 0, 0, 0};
          bucket[j].n++; bucket[j].r += p[0]; bucket[j].g += p[1]; bucket[j].b += p[2];
        }
      if (opaque < need) continue;
      double r, gg, bb;
      if (snap && !pal.empty()) {
        double avg[3] = {sr / opaque, sg / opaque, sb / opaque};
        const std::vector<double>* best = &pal[0]; double bd2 = 1e18;
        for (auto& c : pal) {
          double d = sqrt((c[0]-avg[0])*(c[0]-avg[0]) + (c[1]-avg[1])*(c[1]-avg[1]) + (c[2]-avg[2])*(c[2]-avg[2]));
          if (d < bd2) { bd2 = d; best = &c; }
        }
        r = (*best)[0]; gg = (*best)[1]; bb = (*best)[2];
      } else if (snap) {
        continue;
      } else {
        int bi = -1;
        for (int j = 0; j < nb; j++) if (bi < 0 || bucket[j].n > bucket[bi].n) bi = j;
        if (bi < 0) continue;
        r = bucket[bi].r / bucket[bi].n; gg = bucket[bi].g / bucket[bi].n; bb = bucket[bi].b / bucket[bi].n;
      }
      if (r + gg + bb < 70) continue;                 // near-black, drop it
      double mx = std::max(r, std::max(gg, bb)), mn = std::min(r, std::min(gg, bb));
      if (mx - mn > 10) {                              // it has a hue: deepen it
        double avg = (r + gg + bb) / 3;
        r = std::max(0.0, std::min(255.0, avg + (r - avg) * 1.7));
        gg = std::max(0.0, std::min(255.0, avg + (gg - avg) * 1.7));
        bb = std::max(0.0, std::min(255.0, avg + (bb - avg) * 1.7));
      }
      double m2 = std::max(r, std::max(gg, bb));
      if (m2 > 0 && m2 < 175) { double f = 175 / m2; r *= f; gg *= f; bb *= f; }
      pix.push_back({(uint8_t)x, (uint8_t)y, (uint8_t)std::min(255, (int)r),
                     (uint8_t)std::min(255, (int)gg), (uint8_t)std::min(255, (int)bb)});
    }
  // a lit dot with fewer than two lit neighbours is noise
  std::vector<uint8_t> litm((size_t)dw * dh, 0);
  for (auto& q : pix) litm[q.y * dw + q.x] = 1;
  std::vector<LogoPix> keep;
  for (auto& q : pix) {
    int n = 0;
    for (int dy = -1; dy <= 1; dy++)
      for (int dx = -1; dx <= 1; dx++) {
        if (!dx && !dy) continue;
        int xx = q.x + dx, yy = q.y + dy;
        if (xx >= 0 && xx < dw && yy >= 0 && yy < dh && litm[yy * dw + xx]) n++;
      }
    if (n >= 2) keep.push_back(q);
  }
  out.w = dw; out.h = dh; out.n = (int)keep.size();
  out.pix = out.n ? (LogoPix*)sbAlloc(out.n * sizeof(LogoPix)) : nullptr;
  if (out.n && !out.pix) { out.n = 0; return; }
  if (out.n) memcpy(out.pix, keep.data(), out.n * sizeof(LogoPix));
}


// Small logos (pregame matchup, wheel cards, full ticker): every LED takes
// the logo's own colour that covers most of its patch, lit from 25% cover.
// Then: one-dot gaps in thin bright lines are bridged (outline logos stay
// joined up); a mostly-black mark (Panthers, Raiders) shows its dark parts
// as dim grey instead of vanishing, minus the grey's outer edge so dark
// outlines don't become halos; lone dots are dropped.
static bool liftColor(double r, double gg, double bb, uint8_t (&o)[3]) {
  if (r + gg + bb < 70) return false;
  double mx = std::max(r, std::max(gg, bb)), mn = std::min(r, std::min(gg, bb));
  if (mx - mn > 10) {
    double avg = (r + gg + bb) / 3;
    r = std::max(0.0, std::min(255.0, avg + (r - avg) * 1.7));
    gg = std::max(0.0, std::min(255.0, avg + (gg - avg) * 1.7));
    bb = std::max(0.0, std::min(255.0, avg + (bb - avg) * 1.7));
  }
  double m2 = std::max(r, std::max(gg, bb));
  if (m2 > 0 && m2 < 175) { double f = 175 / m2; r *= f; gg *= f; bb *= f; }
  o[0] = (uint8_t)std::min(255, (int)r); o[1] = (uint8_t)std::min(255, (int)gg); o[2] = (uint8_t)std::min(255, (int)bb);
  return true;
}

static void smallPixels(const uint8_t* bd, int dw, int dh, Logo& out) {
  std::vector<std::vector<double>> pal = logoPalette(bd, (size_t)dw * SS * dh * SS);
  int np = (int)pal.size();
  if (!np) return;
  if (np > 16) np = 16;
  std::vector<bool> dark(np);
  for (int j = 0; j < np; j++) dark[j] = pal[j][0] + pal[j][1] + pal[j][2] < 90;
  size_t cells = (size_t)dw * dh;
  uint16_t* vote = (uint16_t*)sbAlloc(cells * np * sizeof(uint16_t));
  uint8_t* state = (uint8_t*)sbAlloc(cells);        // 0 off, 1 lit, 2 dark cell, 3 grey
  uint8_t* col = (uint8_t*)sbAlloc(cells * 3);
  if (!vote || !state || !col) { if (vote) sbFree(vote); if (state) sbFree(state); if (col) sbFree(col); return; }
  memset(vote, 0, cells * np * sizeof(uint16_t));
  memset(state, 0, cells);
  const int need = (int)ceil(SS * SS * 0.25);
  for (int y = 0; y < dh; y++)
    for (int x = 0; x < dw; x++) {
      if (!x) sbBreathe();
      int opaque = 0;
      uint16_t* v = vote + ((size_t)y * dw + x) * np;
      for (int by = 0; by < SS; by++)
        for (int bx = 0; bx < SS; bx++) {
          const uint8_t* p = bd + (((size_t)(y * SS + by) * dw * SS) + (x * SS + bx)) * 4;
          if (p[3] < 140) continue;
          opaque++;
          int best = 0; double bd2 = 1e18;
          for (int j = 0; j < np; j++) {
            double d = (pal[j][0]-p[0])*(pal[j][0]-p[0]) + (pal[j][1]-p[1])*(pal[j][1]-p[1]) + (pal[j][2]-p[2])*(pal[j][2]-p[2]);
            if (d < bd2) { bd2 = d; best = j; }
          }
          v[best]++;
        }
      if (opaque < need) continue;
      int w = 0;
      for (int j = 1; j < np; j++) if (v[j] > v[w]) w = j;
      size_t c = (size_t)y * dw + x;
      if (dark[w]) { state[c] = 2; continue; }
      uint8_t o[3];
      if (liftColor(pal[w][0], pal[w][1], pal[w][2], o)) { state[c] = 1; memcpy(col + c * 3, o, 3); }
    }
  auto lit = [&](int x, int y) { return x >= 0 && y >= 0 && x < dw && y < dh && state[(size_t)y * dw + x] == 1; };
  // bridge one-dot gaps in bright thin lines
  std::vector<std::pair<size_t, int>> bridge;
  static const int PAIRS[4][4] = {{-1, 0, 1, 0}, {0, -1, 0, 1}, {-1, -1, 1, 1}, {-1, 1, 1, -1}};
  for (int y = 0; y < dh; y++)
    for (int x = 0; x < dw; x++) {
      size_t c = (size_t)y * dw + x;
      if (state[c] == 1) continue;
      uint16_t* v = vote + c * np;
      int w = -1;
      for (int j = 0; j < np; j++) if (!dark[j] && v[j] >= SS * SS / 10 && (w < 0 || v[j] > v[w])) w = j;
      if (w < 0) continue;
      bool join = false;
      for (auto& pr : PAIRS) if (lit(x + pr[0], y + pr[1]) && lit(x + pr[2], y + pr[3])) join = true;
      if (join) bridge.push_back({c, w});
    }
  for (auto& b : bridge) {
    uint8_t o[3];
    if (liftColor(pal[b.second][0], pal[b.second][1], pal[b.second][2], o)) { state[b.first] = 1; memcpy(col + b.first * 3, o, 3); }
  }
  // a mostly-black mark: its dark parts in dim grey
  int nd = 0, nl = 0;
  for (size_t c = 0; c < cells; c++) { if (state[c] == 2) nd++; else if (state[c] == 1) nl++; }
  if (nd > 0.3 * (nd + nl))
    for (size_t c = 0; c < cells; c++) if (state[c] == 2) { state[c] = 3; col[c * 3] = 70; col[c * 3 + 1] = 70; col[c * 3 + 2] = 78; }
  // lone dots go (fewer than two lit neighbours)
  auto on = [&](int x, int y) { return x >= 0 && y >= 0 && x < dw && y < dh && (state[(size_t)y * dw + x] == 1 || state[(size_t)y * dw + x] == 3); };
  std::vector<size_t> drop;
  for (int y = 0; y < dh; y++)
    for (int x = 0; x < dw; x++) {
      if (!on(x, y)) continue;
      int n = 0;
      for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) if ((dx || dy) && on(x + dx, y + dy)) n++;
      if (n < 2) drop.push_back((size_t)y * dw + x);
    }
  for (size_t c : drop) state[c] = 0;
  // peel the grey's outer edge (dark outlines would be halos)
  drop.clear();
  for (int y = 0; y < dh; y++)
    for (int x = 0; x < dw; x++)
      if (state[(size_t)y * dw + x] == 3 && (!on(x + 1, y) || !on(x - 1, y) || !on(x, y + 1) || !on(x, y - 1)))
        drop.push_back((size_t)y * dw + x);
  for (size_t c : drop) state[c] = 0;
  int n = 0;
  for (size_t c = 0; c < cells; c++) if (state[c] == 1 || state[c] == 3) n++;
  out.w = dw; out.h = dh; out.n = n;
  out.pix = n ? (LogoPix*)sbAlloc(n * sizeof(LogoPix)) : nullptr;
  if (n && !out.pix) out.n = 0;
  else {
    int k = 0;
    for (int y = 0; y < dh; y++)
      for (int x = 0; x < dw; x++) {
        size_t c = (size_t)y * dw + x;
        if (state[c] == 1 || state[c] == 3) out.pix[k++] = {(uint8_t)x, (uint8_t)y, col[c * 3], col[c * 3 + 1], col[c * 3 + 2]};
      }
  }
  sbFree(vote); sbFree(state); sbFree(col);
}

bool shrinkLogo(const uint8_t* rgba, int iw, int ih, int S, int SH, Logo& out) {
  out = Logo();
  // crop to what's actually drawn
  int x0 = iw, y0 = ih, x1 = -1, y1 = -1;
  for (int y = 0; y < ih; y++) {
    if (!(y & 63)) sbBreathe();
    for (int x = 0; x < iw; x++)
      if (rgba[((size_t)y * iw + x) * 4 + 3] > 40) {
        if (x < x0) x0 = x; if (x > x1) x1 = x;
        if (y < y0) y0 = y; if (y > y1) y1 = y;
      }
  }
  if (x1 < 0) { x0 = 0; y0 = 0; x1 = iw - 1; y1 = ih - 1; }
  int cw = x1 - x0 + 1, ch = y1 - y0 + 1;
  double fit = std::min((double)S / cw, (double)SH / ch);
  int dw = std::max(1, (int)lround(cw * fit)), dh = std::max(1, (int)lround(ch * fit));
  size_t bytes = (size_t)dw * SS * dh * SS * 4;
  uint8_t* big = (uint8_t*)sbAlloc(bytes);
  if (!big) return false;
  resample(rgba, iw, x0, y0, cw, ch, big, dw * SS, dh * SS);
  if (std::max(S, SH) >= 40) logoPixels(big, dw, dh, true, out);
  else smallPixels(big, dw, dh, out);
  sbFree(big);
  return out.n > 0;
}

void freeLogo(Logo& l) {
  if (l.pix) sbFree(l.pix);
  l = Logo();
}

void drawLogo(Frame& fb, const Logo& l, int ox, int oy, float bright) {
  if (!l.n) return;
  // blank the footprint first, so sparks flying past don't read as the mark
  for (int i = 0; i < l.n; i++)
    for (int dy = -1; dy <= 1; dy++)
      for (int dx = -1; dx <= 1; dx++) fb.put(ox + l.pix[i].x + dx, oy + l.pix[i].y + dy, BLACK);
  for (int i = 0; i < l.n; i++)
    fb.put(ox + l.pix[i].x, oy + l.pix[i].y,
           rgb((int)lround(l.pix[i].r * bright), (int)lround(l.pix[i].g * bright), (int)lround(l.pix[i].b * bright)));
}
