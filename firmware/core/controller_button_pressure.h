#pragma once

#include "controller_state.h"

// Neutral slots 0..7 are A/B/X/Y/L1/R1/L2/R2. A pressure-capable
// group supplies every channel; mixed inputs synthesize 0/255 for their
// digital-only buttons. This keeps output encoders independent of input type.
inline bool controllerButtonHasPressure(const controller_state_t& frame, uint8_t slot) {
  return slot < 6 ? frame.HAS_ANALOG_MAIN_BUTTONS :
         slot < 8 ? frame.HAS_ANALOG_TRIGGERS : false;
}

inline uint8_t controllerButtonPressure(const controller_state_t& frame, uint8_t slot) {
  if (!controllerButtonHasPressure(frame, slot)) {
    return slot < 14 && (frame.digital_buttons & (1u << slot)) ? 255 : 0;
  }
  switch (slot) {
    case 0: return frame.ANALOG_A;
    case 1: return frame.ANALOG_B;
    case 2: return frame.ANALOG_X;
    case 3: return frame.ANALOG_Y;
    case 4: return frame.ANALOG_L1;
    case 5: return frame.ANALOG_R1;
    case 6: return frame.ANALOG_L2;
    case 7: return frame.ANALOG_R2;
    default: return 0;
  }
}

inline void setControllerButtonPressure(controller_state_t& frame, uint8_t slot, uint8_t value) {
  switch (slot) {
    case 0: frame.ANALOG_A = value; break;
    case 1: frame.ANALOG_B = value; break;
    case 2: frame.ANALOG_X = value; break;
    case 3: frame.ANALOG_Y = value; break;
    case 4: frame.ANALOG_L1 = value; break;
    case 5: frame.ANALOG_R1 = value; break;
    case 6: frame.ANALOG_L2 = value; break;
    case 7: frame.ANALOG_R2 = value; break;
    default: break;
  }
}

inline void setControllerPressureForMask(controller_state_t& frame, uint32_t mask, uint8_t value) {
  for (uint8_t slot = 0; slot < 8; ++slot) {
    if (mask & (1u << slot)) setControllerButtonPressure(frame, slot, value);
  }
  if (mask & (1u << 28)) frame.ANALOG_PAD_U = value;
  if (mask & (1u << 29)) frame.ANALOG_PAD_D = value;
  if (mask & (1u << 30)) frame.ANALOG_PAD_L = value;
  if (mask & (1u << 31)) frame.ANALOG_PAD_R = value;
}
