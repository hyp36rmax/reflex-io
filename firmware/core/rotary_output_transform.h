#pragma once

#include <cstring>
#include "controller_state.h"
#include "rotary_policy.h"

inline RotaryInputKind rotaryInputKindForFrame(const controller_state_t& frame,
                                               bool jaguarRotaryActive = false) {
  if (!frame.connected) return RotaryInputKind::None;
  if (std::strcmp(frame.controller_type_name, "neGcon") == 0) return RotaryInputKind::NeGcon;
  if (std::strncmp(frame.controller_type_name, "JogCon", 6) == 0) return RotaryInputKind::JogCon;
  if (std::strncmp(frame.controller_type_name, "Vaus", 4) == 0) return RotaryInputKind::Vaus;
  if (std::strncmp(frame.controller_type_name, "Driving", 7) == 0) return RotaryInputKind::Driving;
  return jaguarRotaryActive ? RotaryInputKind::Jaguar : RotaryInputKind::None;
}

// Mouse output: a connected controller with an analog stick moves the
// pointer by rate. Rotary sources are excluded; their stick fields carry the
// rotation instead (JogCon, neGcon, Driving).
inline bool rotaryMouseStickFrame(const controller_state_t& frame) {
  return frame.connected && rotaryInputKindForFrame(frame) == RotaryInputKind::None &&
         (frame.HAS_ANALOG_STICK_MAIN || frame.HAS_ANALOG_STICK_AUX);
}

inline int16_t applyRotaryAxisGain(int16_t value, analog_stick_precision precision,
                                  uint8_t gain, bool reverse) {
  const uint8_t bits = precision == ANALOG_STICK_PRECISION_16 ? 16 :
                       precision == ANALOG_STICK_PRECISION_12 ? 12 : 8;
  const int32_t midpoint = 1L << (bits - 1);
  int32_t scaled = static_cast<int32_t>(value) * rotaryGainNumerator(gain) /
                   rotaryGainDenominator(gain);
  if (scaled < -midpoint) scaled = -midpoint;
  if (scaled >= midpoint) scaled = midpoint - 1;
  return static_cast<int16_t>(reverse ? -1 - scaled : scaled);
}

// Called on the temporary output frame, after calibration. The physical frame
// is restored after sending, so a stationary held axis is never scaled twice.
inline void applyRotaryOutputTuning(controller_state_t& frame, RotaryInputKind kind,
                                    outputMode_t output, RotaryTuning tuning,
                                    uint8_t jogconAxis) {
  if (!frame.connected || !rotaryOutputSupportsInput(kind, output)) return;
  if (kind == RotaryInputKind::JogCon &&
      (std::strcmp(frame.controller_type_name, "JogCon-S") == 0 ||
       std::strcmp(frame.controller_type_name, "JogCon-Fake") == 0)) return;
  if (rotaryOutputQueuesMotion(kind, output)) return;

  // Driving/Jaguar integrate gained encoder movement at the source. Only
  // reverse their absolute output here; relative motion was already reversed.
  const uint8_t gain = kind == RotaryInputKind::Driving || kind == RotaryInputKind::Jaguar
      ? 1 : tuning.gain;
  if (kind == RotaryInputKind::JogCon && jogconAxis != 0) {
    frame.LY = applyRotaryAxisGain(frame.LY, frame.sticks_precision_bits, gain, tuning.reverse);
  } else {
    frame.LX = applyRotaryAxisGain(frame.LX, frame.sticks_precision_bits, gain, tuning.reverse);
  }
  frame.paddle = applyRotaryPositionGain(frame.paddle, gain, tuning.reverse);
}
