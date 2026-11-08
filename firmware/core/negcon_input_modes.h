#pragma once

#include <stdint.h>
#include <string.h>

#include "controller_settings_state.h"
#include "controller_state.h"
#include "device_mode.h"
#include "device_runtime_state.h"
#include "../output/output_mode.h"

// C++ linkage even where out_usb.h includes this inside extern "C".
extern "C++" {

// neGcon I, II and L each send either their pressure only (Analog, the
// default) or a button press only (Digital), never both. The press is the
// decoder's own bit: Cross for I, Square for II and L1 for L, each at half
// travel.
//
// Stored in the PSX mode record in the byte that holds NES Device in the NES
// record. Bit 7 tags a stored choice, bits 0-2 set I, II and L to Digital and
// bits 3-4 hold the Analog gain (2026-10-19). NES values (0-3) and every
// record written before these settings are untagged and read as all Analog at
// 1x. Older firmware resets a tagged byte to 0.
constexpr uint8_t NEGCON_INPUT_I = 0x01;
constexpr uint8_t NEGCON_INPUT_II = 0x02;
constexpr uint8_t NEGCON_INPUT_L = 0x04;
constexpr uint8_t NEGCON_INPUT_DIGITAL_MASK =
  NEGCON_INPUT_I | NEGCON_INPUT_II | NEGCON_INPUT_L;
constexpr uint8_t NEGCON_INPUT_TAG = 0x80;
constexpr uint8_t NEGCON_INPUT_PRESS_THRESHOLD = 128;

// Analog gain for I, II and L, so a shorter squeeze reaches full scale (tester
// request, 2026-10-06): 1x, 1.25x, 1.5x or 2x, in quarters.
constexpr uint8_t NEGCON_GAIN_SHIFT = 3;
constexpr uint8_t NEGCON_GAIN_MASK = 0x18;
constexpr uint8_t NEGCON_GAIN_COUNT = 4;
constexpr uint8_t kNegconGainQuarters[NEGCON_GAIN_COUNT] = {4, 5, 6, 8};

inline constexpr uint8_t negconDigitalInputsFromRaw(uint8_t raw) {
  return (raw & NEGCON_INPUT_TAG) != 0
    ? static_cast<uint8_t>(raw & NEGCON_INPUT_DIGITAL_MASK)
    : 0;
}

inline constexpr uint8_t negconGainFromRaw(uint8_t raw) {
  return (raw & NEGCON_INPUT_TAG) != 0
    ? static_cast<uint8_t>((raw & NEGCON_GAIN_MASK) >> NEGCON_GAIN_SHIFT)
    : 0;
}

inline constexpr uint8_t encodeNegconInputs(uint8_t digital, uint8_t gain) {
  return ((digital & NEGCON_INPUT_DIGITAL_MASK) != 0 || gain % NEGCON_GAIN_COUNT != 0)
    ? static_cast<uint8_t>(NEGCON_INPUT_TAG |
                           ((gain % NEGCON_GAIN_COUNT) << NEGCON_GAIN_SHIFT) |
                           (digital & NEGCON_INPUT_DIGITAL_MASK))
    : 0;
}

inline constexpr uint8_t encodeNegconDigitalInputs(uint8_t digital) {
  return encodeNegconInputs(digital, 0);
}

inline constexpr uint8_t sanitizeNegconDigitalInputsRaw(uint8_t raw) {
  return encodeNegconInputs(negconDigitalInputsFromRaw(raw), negconGainFromRaw(raw));
}

inline const char* negconGainName(uint8_t gain) {
  static const char* const names[NEGCON_GAIN_COUNT] = {"1x", "1.25x", "1.5x", "2x"};
  return names[gain % NEGCON_GAIN_COUNT];
}

inline constexpr uint8_t negconGainPressure(uint8_t pressure, uint8_t gain) {
  return static_cast<uint8_t>(
      (uint16_t)pressure * kNegconGainQuarters[gain % NEGCON_GAIN_COUNT] / 4 > 255
          ? 255
          : (uint16_t)pressure * kNegconGainQuarters[gain % NEGCON_GAIN_COUNT] / 4);
}

// Whether this mode's record keeps the choice in the shared byte.
inline constexpr bool negconInputsStoredInRecord(DeviceEnum mode) {
#ifdef ENABLE_INPUT_PSX
  return mode == RZORD_PSX;
#else
  (void)mode;
  return false;
#endif
}

inline bool frameIsNegcon(const controller_state_t& frame) {
  return strcmp(frame.controller_type_name, "neGcon") == 0;
}

// A neGcon's I/II/L rows replace the Triggers setting, which still applies to
// the other PlayStation pads, including one on the other port.
inline uint8_t triggerModeForModeAndFrame(DeviceEnum mode, uint8_t triggerMode,
                                          const controller_state_t& frame) {
  return negconInputsStoredInRecord(mode) && frameIsNegcon(frame)
    ? TRIGGER_MODE_ANALOG
    : triggerMode;
}

#ifdef ENABLE_INPUT_PSX
// One RAM copy shared by the trigger transform and the USB mappers.
uint8_t triggerModeForFrame(const controller_state_t& frame);
#else
inline uint8_t triggerModeForFrame(const controller_state_t&) {
  return trigger_mode;
}
#endif

// MiSTer neGcon reports its I/II axes released at 255, like a wheel pedal;
// every other output reports them released at 0. The L axis is always
// reported that way round.
inline constexpr uint8_t negconAxisValue(uint8_t pressure, bool inverted) {
  return inverted ? static_cast<uint8_t>(~pressure) : pressure;
}

// Where an output can carry neGcon pressure. PS3 and the original Xbox have
// pressure-sensitive face buttons, so I, II and L use Cross, Square and L1
// there. Other outputs with analog triggers carry I on R2 and II on L2.
// MiSTer neGcon also has an L pedal axis. L is a press everywhere else; on
// MiSTer DInput its Z axis shares the right stick with D-pad -> Right Stick,
// so the decoder's value there is held at rest.
struct NegconOutputLayout {
  bool facePressure;
  bool triggerAxes;
  bool invertedTriggers;
  bool lAxisWritten;
  bool lPedal;
};

inline NegconOutputLayout negconOutputLayout(outputMode_t output,
                                             bool outputHasAnalogTriggers) {
  NegconOutputLayout layout{};
  layout.facePressure = output == OUTPUT_PS3 || output == OUTPUT_XID;
  layout.triggerAxes = outputHasAnalogTriggers && !layout.facePressure;
  layout.invertedTriggers = output == OUTPUT_MISTER_NEGCON;
  layout.lAxisWritten = outputHasAnalogTriggers &&
    (output == OUTPUT_MISTER || output == OUTPUT_MISTER_NEGCON);
  layout.lPedal = outputHasAnalogTriggers && output == OUTPUT_MISTER_NEGCON;
  return layout;
}

// The axes of a neutral neGcon frame, which rest at 255 where they are inverted.
inline void neutralizeNegconAxes(controller_state_t& frame, const NegconOutputLayout& layout) {
  if (layout.triggerAxes) {
    frame.ANALOG_R2 = frame.ANALOG_L2 = negconAxisValue(0, layout.invertedTriggers);
  }
  if (layout.lAxisWritten) {
    frame.RX = negconAxisValue(0, true);
  }
}

// Whether this output can send the input's pressure. Without one the input
// is always a press, and the Quick Menu hides its row.
inline bool negconInputHasPressure(const NegconOutputLayout& layout,
                                   uint8_t input) {
  if (layout.facePressure) return true;
  return input == NEGCON_INPUT_L ? layout.lPedal : layout.triggerAxes;
}

// Clears the press of each Analog input that sends its pressure instead.
// Runs in both the process and the output stages, so turbo and remap see the
// same buttons in each; a latched turbo would otherwise toggle every poll.
inline void applyNegconInputPresses(controller_state_t& frame, uint8_t digital,
                                    const NegconOutputLayout& layout) {
  if (layout.facePressure) return;
  if (layout.triggerAxes) {
    if ((digital & NEGCON_INPUT_I) == 0) frame.A = 0;
    if ((digital & NEGCON_INPUT_II) == 0) frame.X = 0;
  }
  if (layout.lPedal && (digital & NEGCON_INPUT_L) == 0) frame.L1 = 0;
}

// Applies the choice to a neGcon output frame before turbo and remap. Expects
// the decoder's frame: the I/II/L presses in A/X/L1, their pressure in
// ANALOG_A/X/L1, the I/II trigger copies in ANALOG_R2/L2 and, on MiSTer
// outputs, the L axis in RX.
inline void applyNegconInputModes(controller_state_t& frame, uint8_t digital,
                                  const NegconOutputLayout& layout) {
  const bool iDigital = (digital & NEGCON_INPUT_I) != 0;
  const bool iiDigital = (digital & NEGCON_INPUT_II) != 0;
  const bool lDigital = (digital & NEGCON_INPUT_L) != 0;

  if (layout.facePressure) {
    // A pressure button always carries its own press, as on a DualShock 3.
    // Digital sends it at full pressure; neither copies I/II to the triggers.
    if (iDigital) frame.ANALOG_A = frame.A ? 255 : 0;
    if (iiDigital) frame.ANALOG_X = frame.X ? 255 : 0;
    if (lDigital) frame.ANALOG_L1 = frame.L1 ? 255 : 0;
    frame.ANALOG_R2 = 0;
    frame.ANALOG_L2 = 0;
    return;
  }

  const uint8_t lPressure = frame.ANALOG_L1;
  applyNegconInputPresses(frame, digital, layout);
  // Cross/Square/L1 carry exactly the press, so a remap cannot move raw
  // pressure onto another button.
  frame.ANALOG_A = frame.A ? 255 : 0;
  frame.ANALOG_X = frame.X ? 255 : 0;
  frame.ANALOG_L1 = frame.L1 ? 255 : 0;
  if (layout.triggerAxes) {
    const uint8_t released = negconAxisValue(0, layout.invertedTriggers);
    if (iDigital) frame.ANALOG_R2 = released;
    if (iiDigital) frame.ANALOG_L2 = released;
  }
  // The L axis is rebuilt from the raw pressure, so a stick deadzone applied
  // earlier never reaches the pedal.
  if (layout.lAxisWritten) {
    frame.RX = negconAxisValue(layout.lPedal && !lDigital ? lPressure : 0, true);
  }
}

// Scales each Analog input's pressure after applyNegconInputModes(), on the
// channel that carries it: face pressure on PS3/Xbox, the I/II trigger axes
// (released at 255 on MiSTer neGcon) and the MiSTer neGcon L pedal. Digital
// inputs and the half-travel press are unchanged.
inline void applyNegconAnalogGain(controller_state_t& frame, uint8_t digital,
                                  uint8_t gain, const NegconOutputLayout& layout) {
  if (gain % NEGCON_GAIN_COUNT == 0) return;
  const bool iAnalog = (digital & NEGCON_INPUT_I) == 0;
  const bool iiAnalog = (digital & NEGCON_INPUT_II) == 0;
  const bool lAnalog = (digital & NEGCON_INPUT_L) == 0;
  if (layout.facePressure) {
    if (iAnalog) frame.ANALOG_A = negconGainPressure(frame.ANALOG_A, gain);
    if (iiAnalog) frame.ANALOG_X = negconGainPressure(frame.ANALOG_X, gain);
    if (lAnalog) frame.ANALOG_L1 = negconGainPressure(frame.ANALOG_L1, gain);
    return;
  }
  if (layout.triggerAxes) {
    const bool inverted = layout.invertedTriggers;
    if (iAnalog) {
      frame.ANALOG_R2 = negconAxisValue(
          negconGainPressure(negconAxisValue(frame.ANALOG_R2, inverted), gain), inverted);
    }
    if (iiAnalog) {
      frame.ANALOG_L2 = negconAxisValue(
          negconGainPressure(negconAxisValue(frame.ANALOG_L2, inverted), gain), inverted);
    }
  }
  if (layout.lPedal && lAnalog) {
    frame.RX = negconAxisValue(
        negconGainPressure(negconAxisValue(static_cast<uint8_t>(frame.RX), true), gain), true);
  }
}

}  // extern "C++"
