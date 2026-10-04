// The setup network's name server: every name points at the board, so a
// phone's "is there a sign-in page?" check lands on the setup page.
//
// Written because the core's DNSServer only answers queries that carry no
// extra records - and modern iPhones add one (EDNS) to almost every query.
// Those got an empty reply, so the iPhone took 1-2 minutes to give up and
// fall back before the setup page popped up (Joe, 2026-10-03).
#pragma once
#include <stdint.h>
#include <stddef.h>

// Builds the reply to one DNS query. A (and ANY) questions are answered with
// ip; anything else (AAAA, HTTPS...) gets a quick "no such record" so the
// phone moves on instead of waiting. Returns the reply length, 0 = ignore.
size_t dnsBuildReply(const uint8_t* q, size_t n, uint32_t ipNetworkOrder, uint8_t* out, size_t cap);

#ifdef ARDUINO
#include <WiFiUdp.h>
class CaptiveDns {
 public:
  void begin(uint32_t ipNetworkOrder);
  void stop();
  void process();   // call often from loop()
 private:
  WiFiUDP udp;
  uint32_t ip = 0;
  bool on = false;
};
#endif
