#pragma once

#include "rotary_policy.h"
#include "controller_settings_state.h"
#include "../menu/menu_runtime_state.h"
#include "../output/output_runtime_state.h"

inline RotaryTuning currentRotaryTuning(RotaryInputKind kind, uint8_t jogconMode = 0) {
  return resolveRotaryTuning(kind, get_effective_output_mode(),
      kind == RotaryInputKind::JogCon ? jogcon_rotary_profile : rotary_profile,
      paddle_divisor, rotary_reverse, spinner_speed, menu_wheel_sensitivity, jogconMode);
}
