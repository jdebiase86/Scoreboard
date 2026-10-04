#include "sb_dns.h"
#include <string.h>

size_t dnsBuildReply(const uint8_t* q, size_t n, uint32_t ip, uint8_t* out, size_t cap) {
  if (n < 12 || cap < 512) return 0;
  uint16_t flags = (q[2] << 8) | q[3];
  if (flags & 0x8000) return 0;                  // a reply, not a query
  if ((flags >> 11) & 0xF) return 0;             // only standard queries
  uint16_t qd = (q[4] << 8) | q[5];
  if (qd < 1) return 0;
  // first question: name labels, then type and class
  size_t p = 12;
  while (p < n && q[p]) {
    if (q[p] & 0xC0) return 0;                   // no compression in a question
    p += q[p] + 1;
    if (p > 12 + 255) return 0;
  }
  if (p + 5 > n) return 0;
  size_t qend = p + 5;                           // zero byte + type + class
  uint16_t qtype = (q[p + 1] << 8) | q[p + 2];
  size_t qlen = qend - 12;
  if (12 + qlen + 16 > cap) return 0;

  memcpy(out, q, 2);                             // same ID
  uint16_t rf = 0x8000 | 0x0400 | (flags & 0x0100) | 0x0080;   // reply, authoritative, RD echoed, RA
  out[2] = rf >> 8; out[3] = rf & 0xFF;
  bool answer = qtype == 1 || qtype == 255;      // A or ANY
  out[4] = 0; out[5] = 1;                        // one question
  out[6] = 0; out[7] = answer ? 1 : 0;           // answers
  out[8] = out[9] = out[10] = out[11] = 0;       // no authority / additional (EDNS not echoed)
  memcpy(out + 12, q + 12, qlen);
  size_t o = 12 + qlen;
  if (answer) {
    const uint8_t a[] = {0xC0, 0x0C, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x3C, 0x00, 0x04};
    memcpy(out + o, a, sizeof(a));               // name -> the question, type A, class IN, TTL 60 s
    o += sizeof(a);
    memcpy(out + o, &ip, 4);                     // already in network order
    o += 4;
  }
  return o;
}

#ifdef ARDUINO
void CaptiveDns::begin(uint32_t ipNetworkOrder) {
  ip = ipNetworkOrder;
  on = udp.begin(53);
}

void CaptiveDns::stop() {
  if (on) udp.stop();
  on = false;
}

void CaptiveDns::process() {
  if (!on) return;
  for (int i = 0; i < 8; i++) {                  // a few per loop pass
    int n = udp.parsePacket();
    if (n <= 0) return;
    uint8_t q[512], r[600];
    if (n > (int)sizeof(q)) { udp.flush(); continue; }
    udp.read(q, n);
    size_t len = dnsBuildReply(q, n, ip, r, sizeof(r));
    if (!len) continue;
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.write(r, len);
    udp.endPacket();
  }
}
#endif
