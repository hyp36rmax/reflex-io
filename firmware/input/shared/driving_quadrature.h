#pragma once

#include <stdint.h>

// Quadrature step for (last state << 2) | current state: +1 or -1 between
// neighbouring states, 0 for no change or a two-line jump.
inline constexpr int8_t driving_quad_table[16] = {
   0,  1, -1,  0,
  -1,  0,  0,  1,
   1,  0,  0, -1,
   0, -1,  1,  0
};

// Auto's evidence for a Driving controller: two steps in a row turning the
// same way. One toggling line steps forward and back, so a serial line, a D-pad
// tap or contact bounce never qualifies.
class DrivingProbeSteps {
 public:
  bool sample(uint8_t lastState, uint8_t currState) {
    const int8_t direction = driving_quad_table[((lastState & 3u) << 2) | (currState & 3u)];
    const bool confirmed = direction != 0 && direction == lastDirection_;
    lastDirection_ = direction;
    return confirmed;
  }

 private:
  int8_t lastDirection_ = 0;
};
