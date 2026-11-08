#pragma once
#include <stdint.h>

// maple_in shifts the first wire bit into the high side of its DMA word;
// readPacket byte-swaps that word, putting the first wire byte in bits 7:0.
// The final PIO flush pads the checksum byte with 24 zero bits.
inline bool mapleMouseChecksumValid(const uint32_t* words, uint32_t received,
                                    bool ended) {
  if (!ended || received != 8 || (words[0] & 0xff) != 6) return false;
  uint8_t checksum = 0;
  for (uint8_t i = 0; i < 7; ++i) {
    const uint32_t word = words[i];
    checksum ^= uint8_t(word) ^ uint8_t(word >> 8) ^
                uint8_t(word >> 16) ^ uint8_t(word >> 24);
  }
  return (words[7] & 0xffffff00u) == 0 && uint8_t(words[7]) == checksum;
}

inline bool decodeMapleMouseCondition(const uint32_t* words, uint8_t len,
                                      uint8_t sender, DreamcastMouseCondition& out) {
  if (len != 7 || (words[0] >> 24) != MAPLE_CMD_DATA_TRANSFER ||
      (words[0] & 0xff) != 6 || ((words[0] >> 16) & 0xff) != 0 ||
      ((words[0] >> 8) & 0xff) != sender) return false;
  // Function identifiers have project and KOS semantic representations.
  // Their normalization does not change the physical payload byte order.
  if (words[1] != MAPLE_FUNC_MOUSE &&
      __builtin_bswap32(words[1]) != MAPLE_FUNC_MOUSE) return false;
  const uint32_t b = words[2];
  const uint32_t xy = words[3];
  const uint32_t z = words[4];
  const uint16_t x = uint16_t(xy);
  const uint16_t y = uint16_t(xy >> 16);
  const uint16_t wheel = uint16_t(z);
  if (x > 1023 || y > 1023 || wheel > 1023) return false;
  out.buttons = uint16_t(b);
  out.x = int16_t(x) - 512;
  out.y = int16_t(y) - 512;
  out.wheel = int16_t(wheel) - 512;
  return true;
}
