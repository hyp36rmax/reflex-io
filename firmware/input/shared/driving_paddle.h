#pragma once

#include "../../core/rotary_motion_state.h"

// Absolute position synthesized from an endless encoder. Dividing movement
// before integration preserves full travel; fractions survive slow turns.
inline int16_t advanceDrivingPaddle(int16_t position, int16_t delta,
                                    RotaryDivider& divider, uint8_t divisor,
                                    bool wrap) {
  const int32_t next = position + divider.apply(delta, divisor);
  if (wrap) return static_cast<uint8_t>(next);
  if (next < 0) { divider.reset(); return 0; }
  if (next > 255) { divider.reset(); return 255; }
  return static_cast<int16_t>(next);
}
