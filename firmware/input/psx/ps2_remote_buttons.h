#pragma once

#include <stdint.h>

// Called only with a validated native IR code. Buttons 1..16 follow PSX button
// word order; DVD controls occupy 17..49. This bank never changes Gamepad actions.
inline uint64_t ps2RemoteButtons(uint32_t code) {
  if ((code & 0xFFFF0u) == 0xDAD50u) {
    return uint64_t(1) << (code & 0xFu);
  }
  switch (code) {
    case 0x49D79: return uint64_t(1) << 4; // Up aliases the controller key.
    case 0x49D7C: return uint64_t(1) << 5;
    case 0x49D7A: return uint64_t(1) << 6;
    case 0x49D7B: return uint64_t(1) << 7;
    default: break;
  }
  static constexpr uint32_t dvdCodes[] = {
    0x49D0B, 0x49D54, 0x49D1A, 0x49D1B, 0x49D0E, // Enter, Display, Title, Menu, Return
    0x49D32, 0x49D39, 0x49D38, 0x49D30, 0x49D31, // Play, Pause, Stop, Previous, Next
    0x49D33, 0x49D34, 0x49D60, 0x49D61,           // Scan reverse/forward, Slow reverse/forward
    0x49D64, 0x49D63, 0x49D65, 0x49D28, 0x49D0F, // Audio, Subtitle, Angle, Time, Clear
    0x49D1F, 0x49D35, 0x49D2C, 0x49D2A,           // Program, Shuffle, Repeat, A-B
    0x49D00, 0x49D01, 0x49D02, 0x49D03, 0x49D04, // 1..5
    0x49D05, 0x49D06, 0x49D07, 0x49D08, 0x49D09, // 6..9, 0
  };
  for (uint8_t i = 0; i < sizeof(dvdCodes) / sizeof(dvdCodes[0]); ++i) {
    if (code == dvdCodes[i]) return uint64_t(1) << (16u + i);
  }
  return 0;
}
