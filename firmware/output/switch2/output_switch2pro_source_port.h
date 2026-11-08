#pragma once

#include "../../core/controller_frame_state.h"
#include "../../firmware_platform_config.h"

// A Switch 2 Pro Controller is one player: the earliest connected port drives
// it, so a pad in either Classic2USB port works alone.
inline uint8_t switch2pro_output_source_port() {
  for (uint8_t i = 0; i < MAX_USB_OUT; ++i) {
    if (controllerFrameConst(i).connected) return i;
  }
  return 0;
}
