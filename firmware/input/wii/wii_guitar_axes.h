#pragma once

#include <stdint.h>

// The guitar's stick is 6-bit (0..63, centered on 32) and its whammy bar 5-bit,
// resting at or below 15. Frames carry 8-bit axes.
inline int16_t wiiGuitarStickAxis(uint8_t raw) {
  const int16_t centered = static_cast<int16_t>(raw) - 0x20;
  return centered < 0 ? centered * 4 : centered * 127 / 31;
}

inline int16_t wiiGuitarWhammyAxis(uint8_t raw) {
  const int16_t travel = raw < 15 ? 0 : (raw > 31 ? 16 : raw - 15);
  return -128 + travel * 255 / 16;
}
