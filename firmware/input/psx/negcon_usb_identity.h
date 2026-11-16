#pragma once

#include <cstring>
#include "../../core/controller_state.h"

// The generic HID drops an Analog neGcon I/II press and sends the pressure on
// axes 5/4 instead, which the plain PSX identity's maps never read. While I or
// II is Analog (negconPadPressureSelected) a neGcon therefore takes its own
// identity, whose maps read Cross from axis 5 and Square from axis 4; every
// other pad supplies 0/255 there (hid_output_negcon_pad_axes).
template <typename FrameAt>
inline bool negconPadIdentityEligible(FrameAt frameAt, uint8_t count,
                                      bool pressureSelected, bool mixed) {
  if (mixed || !pressureSelected) {
    return false;
  }
  for (uint8_t i = 0; i < count; ++i) {
    const controller_state_t& frame = frameAt(i);
    if (frame.connected && frame.HAS_ANALOG_TRIGGERS &&
        std::strcmp(frame.controller_type_name, "neGcon") == 0) {
      return true;
    }
  }
  return false;
}
