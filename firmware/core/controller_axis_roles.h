#pragma once

#include <cstring>
#include "controller_state.h"

// Fishing RX is a unipolar reel-speed axis (rest -128, full 127), not a
// centered second stick. Keep stick centering/deadzones off that axis.
// Gamepad report builders also keep D-pad mode at normal (ID 0) for Fishing,
// preserving both analog controls despite a saved PSX D-pad-to-stick setting.
inline bool controllerHasReelSpeedAxis(const controller_state_t& frame) {
  return frame.connected && std::strcmp(frame.controller_type_name, "Fishing") == 0;
}
