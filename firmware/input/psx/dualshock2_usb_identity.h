#pragma once

#include <cstring>
#include "../../core/controller_state.h"
#include "../../core/controller_settings_state.h"

// The shared USB profile provides axes for every port. Digital PSX pads use
// full-press axes while a pressure-capable DS2 supplies its measured values.
template <typename FrameAt>
inline bool dualShock2IdentityEligible(FrameAt frameAt, uint8_t count,
                                       uint8_t triggerMode, bool mixed) {
  if (mixed || (triggerMode != TRIGGER_MODE_ANALOG &&
                triggerMode != TRIGGER_MODE_RSTICK &&
                triggerMode != TRIGGER_MODE_BOTH)) {
    return false;
  }
  for (uint8_t i = 0; i < count; ++i) {
    const controller_state_t& frame = frameAt(i);
    if (frame.connected && frame.HAS_ANALOG_TRIGGERS &&
        frame.HAS_ANALOG_MAIN_BUTTONS &&
        std::strcmp(frame.controller_type_name, "DualShock2") == 0) {
      return true;
    }
  }
  return false;
}
