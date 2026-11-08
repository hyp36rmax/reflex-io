#pragma once

#include <cstring>
#include "../../core/controller_state.h"
#include "../../core/controller_settings_state.h"

// One USB identity covers every HID interface. A digital Saturn pad on the
// other port supplies full-press axes while this profile is active.
template <typename FrameAt>
inline bool saturn3dIdentityEligible(FrameAt frameAt, uint8_t count,
                                     uint8_t triggerMode, bool mixed) {
  if (mixed || (triggerMode != TRIGGER_MODE_ANALOG &&
                triggerMode != TRIGGER_MODE_RSTICK &&
                triggerMode != TRIGGER_MODE_BOTH)) {
    return false;
  }
  bool found = false;
  for (uint8_t i = 0; i < count; ++i) {
    const controller_state_t& frame = frameAt(i);
    if (!frame.connected) {
      continue;
    }
    if (std::strcmp(frame.controller_type_name, "Saturn") == 0 &&
        !frame.HAS_ANALOG_TRIGGERS) {
      continue;
    }
    if (std::strcmp(frame.controller_type_name, "3D Pad") != 0 ||
        !frame.HAS_ANALOG_TRIGGERS) {
      return false;
    }
    found = true;
  }
  return found;
}
