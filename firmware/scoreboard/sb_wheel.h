// The little 3-way thumbwheel (K1, K2, K3 on the board). It isn't wired to
// the ESP32 directly: it sits on a PCA9557 I/O expander on the I2C bus
// (SDA IO1, SCL IO2) - Seengreat's wiki: K1 = expander IO1, K2 = IO3,
// K3 = IO2.
#pragma once
#include <stdint.h>

enum WheelKey : uint8_t { WK_NONE = 0, WK_K1 = 1, WK_K2 = 2, WK_K3 = 4 };

bool wheelStart();          // finds the expander; false if it isn't there
int wheelAddress();         // its I2C address (for the log), -1 = none
const char* wheelBusList();  // every address that answered on the bus
// Keys held right now (WK_ bits), or -1 if the expander didn't answer
int wheelKeys();
