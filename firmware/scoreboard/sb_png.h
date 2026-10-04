// PNG file in memory -> RGBA pixels (4 bytes each), using PNGdec.
#pragma once
#include <stdint.h>
#include <stddef.h>

// Returns an sbAlloc'd buffer (free with sbFree) or null.
uint8_t* decodePngRGBA(const uint8_t* data, size_t len, int& w, int& h, int* err = nullptr);
