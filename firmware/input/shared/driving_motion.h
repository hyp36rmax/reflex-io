#pragma once

#include "../../core/rotary_motion_state.h"
#include "../../core/rotary_policy.h"

// A relative output (rotary_policy.h) gets each encoder step of an active
// Driving controller as queued motion at the paddle's baseline of eight
// units, gained and reversed once here. Turning the output off, or changing
// gain or reverse, drops the remainder and anything still queued, so old
// motion is never replayed.
class DrivingMotion {
 public:
  void sample(uint8_t port, int8_t direction, bool active, outputMode_t output,
              uint8_t gain, bool reverse) {
    const bool enabled =
      active && rotaryOutputQueuesMotion(RotaryInputKind::Driving, output);
    if (enabled != enabled_ || gain != gain_ || reverse != reverse_) {
      divider_.reset();
      resetRotaryMotion(port);
      enabled_ = enabled;
      gain_ = gain;
      reverse_ = reverse;
    }
    if (!enabled_ || direction == 0) return;
    int32_t scaled = static_cast<int32_t>(direction) * 8 * rotaryGainNumerator(gain_);
    if (reverse_) scaled = -scaled;
    addRotaryMotion(port, divider_.apply(scaled, rotaryGainDenominator(gain_)));
  }

 private:
  RotaryDivider divider_;
  uint8_t gain_ = 1;
  bool reverse_ = false;
  bool enabled_ = false;
};
