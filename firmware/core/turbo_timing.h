#pragma once

#include <stdint.h>

// USB2DB15 describes full on/off pulses.  This returns the number of state
// transitions per second, so the phase calculation stays exact at 15 Hz too.
inline constexpr uint16_t turboToggleRateHz(uint8_t baseRate) {
  switch (baseRate) {
    case 1: return 25;   // 12.5 Hz full pulse
    case 2: return 30;   // 15 Hz full pulse
    case 3: return 40;   // 20 Hz full pulse
    case 4: return 60;   // 30 Hz full pulse
    case 5: return 120;  // 60 Hz full pulse
    default: return 0;
  }
}

inline uint32_t turboToggleCountForElapsedUs(uint32_t elapsedUs,
                                               uint16_t togglesPerSecond) {
  return static_cast<uint32_t>(
      (static_cast<uint64_t>(elapsedUs) * togglesPerSecond) / 1000000ULL);
}
