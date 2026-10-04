#include "sb_log.h"
#include <stdarg.h>
#include <esp_heap_caps.h>

static const int LINES = 80, LEN = 160;
static char (*ring)[LEN] = nullptr;   // in PSRAM: internal RAM is kept for Wi-Fi and TLS
static int head = 0, count = 0;
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

void sbLog(const char* fmt, ...) {
  char line[LEN];
  time_t now = time(nullptr);
  int n = 0;
  if (now > 1700000000) {
    struct tm lt;
    localtime_r(&now, &lt);
    n = strftime(line, sizeof(line), "%H:%M:%S ", &lt);
  } else {
    n = snprintf(line, sizeof(line), "+%lus ", (unsigned long)(millis() / 1000));
  }
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(line + n, sizeof(line) - n, fmt, ap);
  va_end(ap);
  Serial.println(line);
  if (!ring) ring = (char (*)[LEN])heap_caps_calloc(LINES, LEN, MALLOC_CAP_SPIRAM);
  if (!ring) return;
  portENTER_CRITICAL(&mux);
  memcpy(ring[head], line, LEN);
  head = (head + 1) % LINES;
  if (count < LINES) count++;
  portEXIT_CRITICAL(&mux);
}

String sbLogText() {
  String out;
  if (!ring) return out;
  char (*copy)[LEN] = (char (*)[LEN])heap_caps_malloc(LINES * LEN, MALLOC_CAP_SPIRAM);
  if (!copy) return out;
  int h, c;
  portENTER_CRITICAL(&mux);
  memcpy(copy, ring, LINES * LEN);
  h = head; c = count;
  portEXIT_CRITICAL(&mux);
  for (int i = 0; i < c; i++) {
    out += copy[(h - c + i + LINES) % LINES];
    out += "\n";
  }
  free(copy);
  return out;
}
