#pragma once

#include <stdint.h>
#include "../firmware_platform_config.h"

// Driver-confirmed relative mice, indexed by physical input slot.
inline uint32_t& classicMouseInputMaskStorage() {
  static uint32_t mask = 0;
  return mask;
}

inline void setClassicMouseInput(uint8_t port, bool mouse) {
  if (port >= MAX_USB_OUT || port >= 32) return;
  const uint32_t bit = uint32_t(1) << port;
  if (mouse) classicMouseInputMaskStorage() |= bit;
  else classicMouseInputMaskStorage() &= ~bit;
}

inline uint32_t classicMouseInputMask() { return classicMouseInputMaskStorage(); }
inline void resetClassicMouseInputs() { classicMouseInputMaskStorage() = 0; }
