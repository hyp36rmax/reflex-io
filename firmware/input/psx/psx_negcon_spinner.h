#pragma once

#include "../../core/rotary_motion_state.h"
#include "../../core/rotary_settings.h"

// Namco Volume and neGcon report a bounded position, not a wrapping counter.
// Relative output starts from the first valid sample and scales movement once.
class NeGconSpinner {
 public:
  void reset(uint8_t port) {
    valid_ = false;
    divider_.reset();
    resetRotaryMotion(port);
  }

  void sample(uint8_t port, uint8_t position, bool enabled,
              uint8_t gain, bool reverse) {
    if (!enabled) {
      reset(port);
      return;
    }
    if (!valid_ || gain != gain_ || reverse != reverse_) {
      reset(port);
      previous_ = position;
      gain_ = gain;
      reverse_ = reverse;
      valid_ = true;
      return;
    }

    int32_t delta = static_cast<int16_t>(position) - previous_;
    previous_ = position;
    if (reverse_) delta = -delta;
    addRotaryMotion(port, divider_.apply(delta * rotaryGainNumerator(gain_),
                                       rotaryGainDenominator(gain_)));
  }

 private:
  RotaryDivider divider_;
  uint8_t previous_ = 0;
  uint8_t gain_ = 1;
  bool reverse_ = false;
  bool valid_ = false;
};
