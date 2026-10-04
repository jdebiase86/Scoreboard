// Log lines go to the USB serial port and into a small ring that the
// settings page shows at http://scoreboard.local/log - so a problem can be
// seen from a phone, no cable or Terminal needed.
#pragma once
#include <Arduino.h>
void sbLog(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
String sbLogText();
