#pragma once
#include <stdint.h>

// Pressed-bit order: bit 0 is the first serial bit (SnesLib digitalRaw).
// VB bit 14 is its marker; 12/13 are B/A and 15 is battery status. Do not
// treat those variable bits as a fixed ID. Neither physical D-pad can report
// both directions on one axis. A damaged NES trailing byte can otherwise
// satisfy the marker alone, even on repeated reads.
inline constexpr bool snesCredibleVirtualBoyReport(uint8_t id, uint16_t pressed) {
  return (id & 0x4) != 0 &&
      (pressed & 0x0030) != 0x0030 &&  // left Up + Down
      (pressed & 0x00c0) != 0x00c0 &&  // left Left + Right
      (pressed & 0x0201) != 0x0201 &&  // right Up + Down
      (pressed & 0x0102) != 0x0102;    // right Left + Right
}

inline constexpr uint16_t snesReverseSerialWord(uint16_t word) {
  uint16_t reversed = 0;
  for (uint8_t bit = 0; bit < 16; ++bit) {
    reversed = uint16_t((reversed << 1) | (word & 1));
    word >>= 1;
  }
  return reversed;
}
