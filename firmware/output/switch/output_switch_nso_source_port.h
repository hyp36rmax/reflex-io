#pragma once

#include <cstring>

#include "../../core/controller_frame_state.h"
#include "../../core/device_runtime_state.h"
#include "../output_capabilities.h"
#include "../output_runtime_state.h"

// Native NSO identities are one-player. With USB or JVS input, the Switch
// packer and USB current-state delivery both send the official Genesis
// controller if one is live, otherwise the earliest live neutral frame.
// Classic inputs keep their own port.
inline __attribute__((noinline)) uint8_t switchpro_output_source_port(uint8_t outputPort) {
#if defined(ENABLE_INPUT_USB) || defined(ENABLE_INPUT_JVS)
  bool genericSwitchInput = false;
  #ifdef ENABLE_INPUT_USB
  genericSwitchInput = genericSwitchInput || deviceMode == RZORD_USB;
  #endif
  #ifdef ENABLE_INPUT_JVS
  genericSwitchInput = genericSwitchInput || deviceMode == RZORD_JVS;
  #endif
  if (genericSwitchInput && is_nso_special_active()) {
    if (output_uses_switch_genesis_profile()) {
      for (uint8_t i = 0; i < MAX_USB_OUT; ++i) {
        const controller_state_t& candidate = controllerFrameConst(i);
        if (candidate.connected &&
            std::strcmp(candidate.controller_type_name, "Genesis NSO") == 0) {
          return i;
        }
      }
    }
    for (uint8_t i = 0; i < MAX_USB_OUT; ++i) {
      if (controllerFrameConst(i).connected) return i;
    }
  }
#endif
  return outputPort;
}
