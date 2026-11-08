#pragma once

#include "../../core/controller_state.h"

// I/II/L retain their real pressure. Printed A/B and R are digital-only,
// but must still populate the complete neutral pressure group.
inline void syncNegconDigitalButtonPressure(controller_state_t& frame) {
  frame.ANALOG_B = frame.B ? 255 : 0;
  frame.ANALOG_Y = frame.Y ? 255 : 0;
  frame.ANALOG_R1 = frame.R1 ? 255 : 0;
}
