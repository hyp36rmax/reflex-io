#pragma once

#include <stdint.h>

// Fits between the D-pad and face buttons, below Start/Select. Position is
// the controller's raw low position byte, not its scaled USB joystick axis.
namespace jogcon_dial {
constexpr uint8_t kCol = 18;
constexpr uint8_t kRow = 1;
constexpr uint8_t kSize = 24;
constexpr uint8_t kPages = kSize / 8;

inline bool pixel(uint8_t x, uint8_t y, uint8_t position) {
  constexpr int8_t orbit[32][2] = {
    {0,-7}, {1,-7}, {3,-6}, {4,-6}, {5,-5}, {6,-4}, {6,-3}, {7,-1},
    {7,0}, {7,1}, {6,3}, {6,4}, {5,5}, {4,6}, {3,6}, {1,7},
    {0,7}, {-1,7}, {-3,6}, {-4,6}, {-5,5}, {-6,4}, {-6,3}, {-7,1},
    {-7,0}, {-7,-1}, {-6,-3}, {-6,-4}, {-5,-5}, {-4,-6}, {-3,-6}, {-1,-7}
  };
  const int16_t dx = int16_t(x) - 11;
  const int16_t dy = int16_t(y) - 11;
  const int16_t radiusSquared = dx * dx + dy * dy;
  const uint8_t phase = position >> 3;
  const int16_t markerX = dx - orbit[phase][0];
  const int16_t markerY = dy - orbit[phase][1];
  const int16_t markerSquared = markerX * markerX + markerY * markerY;
  return (radiusSquared >= 90 && radiusSquared <= 110) ||
         (markerSquared >= 2 && markerSquared <= 5);
}
}  // namespace jogcon_dial
