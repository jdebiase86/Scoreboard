#include "sb_png.h"
#include "sb_logo.h"
#include <PNGdec.h>
#include <new>
#include <string.h>

// PNGdec's default line buffer only fits images 320 pixels wide; ESPN's logos
// are 500. The build sets PNG_MAX_BUFFERED_PIXELS for 1024 (see the .ino).
#if PNG_MAX_BUFFERED_PIXELS < ((1024 * 4 + 1) * 2)
#error "Build with -DPNG_MAX_BUFFERED_PIXELS=8194 - ESPN logos are 500 px wide"
#endif

struct Out { uint8_t* buf; int w, h; };

static int onLine(PNGDRAW* d) {
  Out* o = (Out*)d->pUser;
  if (d->y < 0 || d->y >= o->h) return 1;
  uint8_t* dst = o->buf + (size_t)d->y * o->w * 4;
  const uint8_t* s = d->pPixels;
  int n = d->iWidth < o->w ? d->iWidth : o->w;
  switch (d->iPixelType) {
    case PNG_PIXEL_TRUECOLOR_ALPHA:
      if (d->iBpp == 8) memcpy(dst, s, (size_t)n * 4);
      break;
    case PNG_PIXEL_TRUECOLOR:
      if (d->iBpp == 8)
        for (int x = 0; x < n; x++) { dst[x*4] = s[x*3]; dst[x*4+1] = s[x*3+1]; dst[x*4+2] = s[x*3+2]; dst[x*4+3] = 255; }
      break;
    case PNG_PIXEL_GRAY_ALPHA:
      if (d->iBpp == 8)
        for (int x = 0; x < n; x++) { dst[x*4] = dst[x*4+1] = dst[x*4+2] = s[x*2]; dst[x*4+3] = s[x*2+1]; }
      break;
    case PNG_PIXEL_GRAYSCALE:
      if (d->iBpp == 8)
        for (int x = 0; x < n; x++) { dst[x*4] = dst[x*4+1] = dst[x*4+2] = s[x]; dst[x*4+3] = 255; }
      break;
    case PNG_PIXEL_INDEXED: {
      int bpp = d->iBpp, per = 8 / bpp, mask = (1 << bpp) - 1;
      for (int x = 0; x < n; x++) {
        int idx = (s[x / per] >> ((per - 1 - x % per) * bpp)) & mask;
        const uint8_t* p = d->pPalette + idx * 3;
        dst[x*4] = p[0]; dst[x*4+1] = p[1]; dst[x*4+2] = p[2];
        dst[x*4+3] = d->iHasAlpha ? d->pPalette[768 + idx] : 255;
      }
      break;
    }
  }
  return 1;
}

uint8_t* decodePngRGBA(const uint8_t* data, size_t len, int& w, int& h, int* err) {
  w = h = 0;
  if (err) *err = -1;
  void* mem = sbAlloc(sizeof(PNG));
  if (!mem) return nullptr;
  PNG* png = new (mem) PNG();
  uint8_t* buf = nullptr;
  int rc = png->openRAM((uint8_t*)data, (int)len, onLine);
  if (err) *err = rc;
  if (rc == PNG_SUCCESS) {
    int iw = png->getWidth(), ih = png->getHeight();
    if (iw > 0 && ih > 0 && iw <= 1024 && ih <= 1024) {
      buf = (uint8_t*)sbAlloc((size_t)iw * ih * 4);
      if (buf) {
        memset(buf, 0, (size_t)iw * ih * 4);
        Out o{buf, iw, ih};
        rc = png->decode(&o, 0);
        if (err) *err = rc;
        if (rc == PNG_SUCCESS) { w = iw; h = ih; }
        else { sbFree(buf); buf = nullptr; }
      }
    }
    png->close();
  }
  png->~PNG();
  sbFree(mem);
  return buf;
}
