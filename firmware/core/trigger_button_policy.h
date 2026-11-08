#pragma once

#include <cstring>
#include "../product_config.h"
#include "controller_state.h"
#include "controller_settings_state.h"
#include "../output/output_mode.h"

// Filter physical sources before remapping. Independently assigned output
// buttons and virtual hotkeys must still reach their requested destinations.
inline void applyAnalogOnlyTriggerButtons(controller_state_t& frame,
                                           uint8_t triggerMode,
                                           outputMode_t output) {
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
  (void)frame;
  (void)triggerMode;
  (void)output;
#else
  if (!frame.HAS_ANALOG_TRIGGERS || triggerMode != TRIGGER_MODE_ANALOG ||
      (output != OUTPUT_HID && output != OUTPUT_MISTER) ||
      std::strcmp(frame.controller_type_name, "Classic") == 0 ||
      std::strcmp(frame.controller_type_name, "neGcon") == 0) {
    return;
  }
  frame.L2 = 0;
  frame.R2 = 0;
#endif
}
