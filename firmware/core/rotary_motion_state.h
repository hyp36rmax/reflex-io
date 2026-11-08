#pragma once

#include <stdint.h>

#include "../firmware_platform_config.h"

// Divides signed rotary increments while retaining fractional motion. A
// divisor change deliberately drops the old fraction because it has a
// different physical scale.
class RotaryDivider {
 public:
  int32_t apply(int32_t delta, uint8_t divisor) {
    if (divisor < 1) divisor = 1;
    if (divisor > 64) divisor = 64;
    if (divisor != divisor_) {
      remainder_ = 0;
      divisor_ = divisor;
    }
    const int64_t total = static_cast<int64_t>(remainder_) + delta;
    const int64_t output = total / divisor_;
    remainder_ = static_cast<int32_t>(total % divisor_);
    return static_cast<int32_t>(output);
  }

  void reset() {
    remainder_ = 0;
    divisor_ = 1;
  }

 private:
  int32_t remainder_ = 0;
  uint8_t divisor_ = 1;
};

// Relative rotary movement is transport-neutral queued state.  An output must
// consume only the exact amount that it successfully submits.
void addRotaryMotion(uint8_t port, int32_t delta);
int8_t peekRotaryMotion(uint8_t port);
void consumeRotaryMotion(uint8_t port, int8_t sent);
void resetRotaryMotion(uint8_t port);
void resetAllRotaryMotion();
