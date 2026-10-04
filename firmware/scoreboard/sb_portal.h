// The setup / settings web page. On first power-up the board makes its own
// Wi-Fi network ("Scoreboard-XXXX"); joining it on a phone pops this page
// up. Once the board is on the home Wi-Fi, the same page (minus the Wi-Fi
// part, unless asked) is at http://scoreboard.local.
#pragma once
#include <Arduino.h>

void portalStartAP(const String& apName);   // setup network + captive page
void portalStopAP();
void portalStartHome();                      // settings page on the home network
void portalLoop();
bool portalAPRunning();
// set when the page saved new Wi-Fi details (the board restarts to use them)
extern volatile bool portalWifiSaved;
extern volatile uint32_t portalSavedAt;
// last time someone loaded or saved the page (0 = never)
extern volatile uint32_t portalUsedAt;
