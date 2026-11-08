#pragma once

#include <stdint.h>

struct SnesMouseReport {
  uint16_t digital = 0;
  uint16_t extended = 0;
  int8_t x = 0;
  int8_t y = 0;
  uint8_t buttons = 0;
  uint8_t sensitivity = 0;
};

inline int8_t snesMouseSignedMagnitude(uint8_t value) {
  const int8_t magnitude = value & 0x7f;
  return value & 0x80 ? -magnitude : magnitude;
}

inline uint16_t snesMouseSerialWord(uint16_t value) {
  uint16_t reversed = 0;
  for (uint8_t bit = 0; bit < 16; ++bit) {
    reversed = static_cast<uint16_t>((reversed << 1) | (value & 1));
    value >>= 1;
  }
  return reversed;
}

// Logical bytes are MSB-first; DAT is active low. Hyperkin mice require
// >=170 SNES master cycles between samples and >=336 at the header/motion
// boundary. Use 12 us per bit and 20 us at that boundary (NTSC is faster).
// https://snes.nesdev.org/wiki/Mouse
template <typename Bus>
bool readSnesMouseReport(Bus& bus, SnesMouseReport& report) {
  report = {};
  bus.clock(false);
  bus.latch(false);
  bus.waitUs(12);
  bus.latch(true);
  bus.waitUs(12);
  bus.latch(false);
  bus.waitUs(6);

  uint16_t header = bus.data() ? 0 : 1;
  auto nextBit = [&bus](bool movementBoundary) {
    bus.clock(true);
    bus.waitUs(6);
    bus.clock(false);
    bus.waitUs(6);
    if (movementBoundary) bus.waitUs(8);
    return bus.data() ? 0 : 1;
  };
  for (uint8_t bit = 1; bit < 16; ++bit) {
    header = static_cast<uint16_t>((header << 1) | nextBit(false));
  }
  // The reserved first byte and fixed signature must both match. Do not
  // accept a pad, an unplugged bus or partial/noisy IDs as a mouse.
  if ((header & 0xff0f) != 0x0001) {
    bus.clock(true);
    return false;
  }
  uint16_t motion = 0;
  for (uint8_t bit = 0; bit < 16; ++bit) {
    motion = static_cast<uint16_t>((motion << 1) | nextBit(bit == 0));
  }
  bus.clock(true);

  report.digital = snesMouseSerialWord(header);
  report.extended = snesMouseSerialWord(motion);
  report.x = snesMouseSignedMagnitude(static_cast<uint8_t>(motion));
  report.y = snesMouseSignedMagnitude(static_cast<uint8_t>(motion >> 8));
  report.buttons = static_cast<uint8_t>((header >> 6) & 3);
  report.sensitivity = static_cast<uint8_t>((header >> 4) & 3);
  return true;
}
