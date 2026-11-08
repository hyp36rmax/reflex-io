#pragma once

#include <stdint.h>

#include "../../core/rotary_motion_state.h"
#include "../../firmware_platform_config.h"

extern bool jaguarRotaryActive;
extern bool jaguarRotaryActivePorts[MAX_USB_OUT];
// OLED-only wrapping phase. USB joystick position remains independently clamped.
extern uint8_t jaguarRotaryDisplayPhase[MAX_USB_OUT];
extern RotaryDivider jaguarRotaryDivider[MAX_USB_OUT];
extern bool jaguarSpinnerOutputWasActive;

class JaguarRotaryQuadrature {
 public:
  int8_t sample(bool connected, bool left, bool right) {
    if (!connected) {
      needsBaseline_ = true;
      return 0;
    }
    const uint8_t current = static_cast<uint8_t>((left << 1) | right);
    if (needsBaseline_) {
      lastState_ = current;
      needsBaseline_ = false;
      return 0;
    }
    static constexpr int8_t table[16] = {
       0, +1, -1,  0,
      -1,  0,  0, +1,
      +1,  0,  0, -1,
       0, -1, +1,  0
    };
    const int8_t delta = table[(lastState_ << 2) | current];
    lastState_ = current;
    return delta;
  }

  void reset() { needsBaseline_ = true; lastState_ = 0; }

 private:
  uint8_t lastState_ = 0;
  bool needsBaseline_ = true;
};

extern JaguarRotaryQuadrature jaguarRotaryQuadrature[MAX_USB_OUT];

void resetJaguarRotaryRuntimeState();
