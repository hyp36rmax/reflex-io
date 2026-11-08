#pragma once

#include "rotary_settings.h"
#include "rotary_profile.h"
#include "../output/output_mode.h"

enum class RotaryInputKind : uint8_t { None, NeGcon, JogCon, Vaus, Driving, Jaguar };

struct RotaryTuning {
  uint8_t gain;
  bool reverse;
};

inline bool rotaryOutputSupportsInput(RotaryInputKind kind, outputMode_t output,
                                     uint8_t jogconMode = 0) {
  if (kind == RotaryInputKind::None) return false;
  switch (output) {
    case OUTPUT_HID: case OUTPUT_MISTER:
    case OUTPUT_XINPUT: case OUTPUT_XINPUT2P: case OUTPUT_XINPUTW:
    case OUTPUT_XID: case OUTPUT_PS3: case OUTPUT_PS4: case OUTPUT_PS5:
    case OUTPUT_SWITCH: case OUTPUT_SWITCHPRO: case OUTPUT_SWITCH2PRO: case OUTPUT_XBOXONE:
    case OUTPUT_PANTHERLORD: case OUTPUT_GCWIIU:
      return true;
    case OUTPUT_MISTER_NEGCON: return kind == RotaryInputKind::NeGcon;
    case OUTPUT_MISTER_JOGCON: return kind == RotaryInputKind::JogCon;
    case OUTPUT_KEYBOARD: case OUTPUT_MDMINI:
      return kind == RotaryInputKind::JogCon && jogconMode == 3;
    case OUTPUT_MISTER_PADDLE:
      return kind == RotaryInputKind::NeGcon || kind == RotaryInputKind::JogCon ||
             kind == RotaryInputKind::Vaus || kind == RotaryInputKind::Driving;
    case OUTPUT_MISTER_SPINNER:
      return kind == RotaryInputKind::NeGcon || kind == RotaryInputKind::JogCon ||
             kind == RotaryInputKind::Jaguar || kind == RotaryInputKind::Driving;
    case OUTPUT_ROTARY_MOUSE:
      return kind == RotaryInputKind::Jaguar || kind == RotaryInputKind::Driving ||
             kind == RotaryInputKind::JogCon || kind == RotaryInputKind::NeGcon;
    default: return false;
  }
}

// Outputs that report rotation as queued relative motion
// (rotary_motion_state.h) rather than as an absolute position.
inline bool rotaryOutputQueuesMotion(RotaryInputKind kind, outputMode_t output) {
  return (output == OUTPUT_MISTER_SPINNER || output == OUTPUT_ROTARY_MOUSE) &&
         rotaryOutputSupportsInput(kind, output);
}

// Until a canonical profile is edited, preserve each old path's saved tuning.
// An explicit edit makes that controller's gain independent of USB personality.
inline RotaryTuning resolveRotaryTuning(
    RotaryInputKind kind, outputMode_t output, uint8_t profile,
    uint8_t legacyDivisor, bool legacyReverse, uint8_t spinnerSpeed,
    uint8_t wheelSensitivity, uint8_t jogconMode) {
  if (!rotaryOutputSupportsInput(kind, output, jogconMode)) return {1, false};
  if (rotaryProfileConfigured(profile)) {
    return {rotaryProfileGain(profile), rotaryProfileReverse(profile)};
  }
  if (output == OUTPUT_MISTER_PADDLE || output == OUTPUT_MISTER_SPINNER ||
      output == OUTPUT_ROTARY_MOUSE ||
      (kind == RotaryInputKind::NeGcon &&
       (output == OUTPUT_HID || output == OUTPUT_MISTER))) {
    return {legacyDivisor >= 1 && legacyDivisor <= 16 ? legacyDivisor : uint8_t(1),
            legacyReverse};
  }
  if (kind == RotaryInputKind::JogCon) {
    return {jogconMode == 0 || jogconMode == 3 ?
      rotaryGainFromSpinnerSpeed(spinnerSpeed) :
      rotaryGainFromWheelSensitivity(wheelSensitivity), false};
  }
  if (kind == RotaryInputKind::Driving) {
    return {rotaryGainFromDrivingSpeed(spinnerSpeed), false};
  }
  if (kind == RotaryInputKind::Jaguar) {
    return {rotaryGainFromSpinnerSpeed(spinnerSpeed), false};
  }
  return {1, false};
}
