#include "sb_wheel.h"
#include <Arduino.h>
#include <Wire.h>

static int addr = -1;
static uint8_t idle = 0xFF;   // input register with nothing pressed

static int readInputs() {
  Wire.beginTransmission((uint8_t)addr);
  Wire.write((uint8_t)0x00);                  // input port register
  if (Wire.endTransmission(false) != 0) return -1;
  if (Wire.requestFrom((uint8_t)addr, (uint8_t)1) != 1) return -1;
  return Wire.read();
}

// A chip left half-way through a transfer (a restart doesn't power it down)
// can hold the data line low and jam the bus. Clock it out: up to 9 pulses,
// then a STOP.
static void unjam() {
  pinMode(1, INPUT_PULLUP);
  pinMode(2, OUTPUT_OPEN_DRAIN);
  digitalWrite(2, HIGH);
  delayMicroseconds(10);
  for (int i = 0; i < 9 && digitalRead(1) == LOW; i++) {
    digitalWrite(2, LOW);  delayMicroseconds(10);
    digitalWrite(2, HIGH); delayMicroseconds(10);
  }
  pinMode(1, OUTPUT_OPEN_DRAIN);
  digitalWrite(1, LOW);  delayMicroseconds(10);
  digitalWrite(1, HIGH); delayMicroseconds(10);   // STOP: SDA rises while SCL is high
  pinMode(1, INPUT_PULLUP);
  pinMode(2, INPUT_PULLUP);
}

static char found[64] = "";
const char* wheelBusList() { return found; }

bool wheelStart() {
  unjam();
  Wire.begin(1, 2, 100000);
  Wire.setTimeOut(20);
  // everyone on the bus, for the log (the audio chip should be there too)
  int n = 0;
  found[0] = 0;
  static bool present[128];
  for (int a = 0x08; a <= 0x77; a++) {
    Wire.beginTransmission((uint8_t)a);
    uint8_t e = Wire.endTransmission();
    present[a] = e == 0;
    if (e == 5 || e == 4) {   // timeout / bus error: the bus is jammed, don't wait on every address
      snprintf(found + n, sizeof(found) - n, "bus stuck (error %d at 0x%02X)", e, a);
      return false;
    }
    if (present[a]) {
      n += snprintf(found + n, sizeof(found) - n, "0x%02X ", a);
      if (n >= (int)sizeof(found) - 6) break;
    }
  }
  // the expander: PCA9557 0x18-0x1F (the audio chip may also sit at 0x18),
  // or a PCA9554/TCA9554 0x20-0x27 / 0x38-0x3F on other board runs. Take
  // the first one whose config register reads back like an expander's.
  static const int CANDS[] = {0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x18, 0x20, 0x21, 0x22, 0x23,
                              0x24, 0x25, 0x26, 0x27, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F};
  for (int a : CANDS) {
    if (!present[a]) continue;
    Wire.beginTransmission((uint8_t)a);
    Wire.write((uint8_t)0x03);
    if (Wire.endTransmission(false) != 0) continue;
    if (Wire.requestFrom((uint8_t)a, (uint8_t)1) != 1) continue;
    int cfg = Wire.read();
    if (cfg == 0xFF) { addr = a; break; }   // power-on: all inputs
  }
  if (addr < 0) return false;
  // all eight lines inputs (the power-on default; set it anyway)
  Wire.beginTransmission((uint8_t)addr);
  Wire.write((uint8_t)0x03);
  Wire.write((uint8_t)0xFF);
  Wire.endTransmission();
  int v = readInputs();
  if (v >= 0) idle = (uint8_t)v;
  return true;
}

int wheelAddress() { return addr; }

int wheelKeys() {
  if (addr < 0) return -1;
  int v = readInputs();
  if (v < 0) return -1;
  // a key counts as held when its line differs from how it sat at start-up
  // (works whichever way the switch is wired)
  uint8_t ch = (uint8_t)v ^ idle;
  int k = 0;
  if (ch & (1 << 1)) k |= WK_K1;
  if (ch & (1 << 3)) k |= WK_K2;
  if (ch & (1 << 2)) k |= WK_K3;
  return k;
}
