#pragma once
#include <stdint.h>

struct SaturnMouseReport {
  int16_t x = 0, y = 0;
  uint8_t flags = 0, buttons = 0, rawX = 0, rawY = 0;
};

// ST-TECH-44 (1996) supersedes the magnitude wording in the earlier SMPC
// manual: the sign extends the eight movement bits. Overflow is undefined.
inline int16_t saturnMouseAxis(uint8_t value, bool negative) {
  return static_cast<int16_t>(value) - (negative ? 256 : 0);
}

// Bus supplies bounded TL waits and restores the previous TR direction.
// Sega SMPC manual table 3.29: 0/B, two F handshake nibbles, six data.
template <typename Bus>
bool readSaturnMouseReport(Bus& bus, SaturnMouseReport& report) {
  report = {};
  bus.begin();
  bool valid = false;
  do {
    bus.th(true); bus.waitUs(4);
    if (bus.nibble() != 0) break;
    bus.th(false); bus.waitUs(4);
    if (bus.nibble() != 0xB) break;
    uint8_t data[8] = {};
    bool complete = true;
    for (uint8_t i = 0; i < 8; ++i) {
      const bool tr = (i & 1) != 0;
      bus.tr(tr);
      if (!bus.waitTl(tr)) { complete = false; break; }
      bus.waitUs(2);
      data[i] = bus.nibble();
    }
    if (!complete || data[0] != 0xF || data[1] != 0xF) break;
    report.flags = data[2]; report.buttons = data[3];
    report.rawX = static_cast<uint8_t>((data[4] << 4) | data[5]);
    report.rawY = static_cast<uint8_t>((data[6] << 4) | data[7]);
    report.x = (report.flags & 4) ? 0 : saturnMouseAxis(report.rawX, report.flags & 1);
    report.y = (report.flags & 8) ? 0 : saturnMouseAxis(report.rawY, report.flags & 2);
    valid = true;
  } while (false);
  bus.end();
  return valid;
}
