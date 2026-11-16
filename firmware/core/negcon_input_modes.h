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
// record. Bit 7 tags a stored choice, bits 0-2 set I, II and L to Digital,
// bits 3-4 hold the Analog gain (2026-10-19) and bit 5 opts L into the right
// stick on PC DInput and XInput (2026-11-09). NES values (0-3) and every
// record written before these settings are untagged and read as all Analog at
// 1x, without the opt-in. Older firmware resets a tagged byte to 0.
constexpr uint8_t NEGCON_INPUT_I = 0x01;
constexpr uint8_t NEGCON_INPUT_II = 0x02;
constexpr uint8_t NEGCON_INPUT_L = 0x04;
constexpr uint8_t NEGCON_INPUT_DIGITAL_MASK =
  NEGCON_INPUT_I | NEGCON_INPUT_II | NEGCON_INPUT_L;
// L was always a press on PC DInput and XInput until 2026-11-09, and Analog
// drops the press, so L sends its pressure on the right stick there only once
// the Quick Menu has set it to Analog on one of them.
constexpr uint8_t NEGCON_INPUT_L_STICK = 0x20;
constexpr uint8_t NEGCON_INPUT_STORED_MASK =
  NEGCON_INPUT_DIGITAL_MASK | NEGCON_INPUT_L_STICK;
constexpr uint8_t NEGCON_INPUT_TAG = 0x80;
constexpr uint8_t NEGCON_INPUT_PRESS_THRESHOLD = 128;

// Analog gain for I, II and L, so a shorter squeeze reaches full scale (tester
// request, 2026-10-06): 1x, 1.25x, 1.5x or 2x, in quarters.
constexpr uint8_t NEGCON_GAIN_SHIFT = 3;
constexpr uint8_t NEGCON_GAIN_MASK = 0x18;
constexpr uint8_t NEGCON_GAIN_COUNT = 4;
constexpr uint8_t kNegconGainQuarters[NEGCON_GAIN_COUNT] = {4, 5, 6, 8};

// The Digital bits plus the L right-stick opt-in.
inline constexpr uint8_t negconDigitalInputsFromRaw(uint8_t raw) {
  return (raw & NEGCON_INPUT_TAG) != 0
    ? static_cast<uint8_t>(raw & NEGCON_INPUT_STORED_MASK)
    : 0;
}

inline constexpr uint8_t negconGainFromRaw(uint8_t raw) {
  return (raw & NEGCON_INPUT_TAG) != 0
    ? static_cast<uint8_t>((raw & NEGCON_GAIN_MASK) >> NEGCON_GAIN_SHIFT)
    : 0;
}

inline constexpr uint8_t encodeNegconInputs(uint8_t digital, uint8_t gain) {
  return ((digital & NEGCON_INPUT_STORED_MASK) != 0 || gain % NEGCON_GAIN_COUNT != 0)
    ? static_cast<uint8_t>(NEGCON_INPUT_TAG |
                           ((gain % NEGCON_GAIN_COUNT) << NEGCON_GAIN_SHIFT) |
                           (digital & NEGCON_INPUT_STORED_MASK))
    : 0;
}

// Whether I or II sends its pressure, which gives a neGcon on the generic HID
// its own USB identity (input/psx/negcon_usb_identity.h).
inline constexpr bool negconPadPressureSelected(uint8_t digital) {
  return (digital & (NEGCON_INPUT_I | NEGCON_INPUT_II)) !=
         (NEGCON_INPUT_I | NEGCON_INPUT_II);
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
// MiSTer neGcon also has an L pedal axis. PC DInput and XInput can send L on
// the right stick X, which a neGcon has no use for, unless D-pad -> Right
// Stick takes that stick (rightStickFree). L is a press everywhere else; on
// MiSTer DInput, where the PSX core would read the stick as a DualShock's, the
// decoder's value there is held at rest.
struct NegconOutputLayout {
  bool facePressure;
  bool triggerAxes;
  bool invertedTriggers;
  bool lAxisWritten;
  bool lPedal;
  bool lStick;
};

inline NegconOutputLayout negconOutputLayout(outputMode_t output,
                                             bool outputHasAnalogTriggers,
                                             bool rightStickFree) {
  NegconOutputLayout layout{};
  layout.facePressure = output == OUTPUT_PS3 || output == OUTPUT_XID;
  layout.triggerAxes = outputHasAnalogTriggers && !layout.facePressure;
  layout.invertedTriggers = output == OUTPUT_MISTER_NEGCON;
  layout.lAxisWritten = outputHasAnalogTriggers &&
    (output == OUTPUT_MISTER || output == OUTPUT_MISTER_NEGCON);
  layout.lPedal = outputHasAnalogTriggers && output == OUTPUT_MISTER_NEGCON;
  layout.lStick = outputHasAnalogTriggers && rightStickFree &&
    (output == OUTPUT_HID || output == OUTPUT_XINPUT2P);
  return layout;
}

// Whether L sends a press on this output: its Digital bit, or a right-stick
// output it has not been opted into (NEGCON_INPUT_L_STICK).
inline constexpr bool negconLDigital(uint8_t digital, const NegconOutputLayout& layout) {
  return (digital & NEGCON_INPUT_L) != 0 ||
         (layout.lStick && (digital & NEGCON_INPUT_L_STICK) == 0);
}

inline constexpr bool negconInputDigital(uint8_t digital, uint8_t input,
                                         const NegconOutputLayout& layout) {
  return input == NEGCON_INPUT_L ? negconLDigital(digital, layout)
                                 : (digital & input) != 0;
}

// Flips what the input's Quick Menu row shows. On a right-stick output, Analog
// for L clears its Digital bit and opts it into the stick; Digital there drops
// only the opt-in, so L stays Analog on MiSTer neGcon, PS3 and Xbox.
inline constexpr uint8_t toggleNegconInput(uint8_t digital, uint8_t input,
                                           const NegconOutputLayout& layout) {
  return input == NEGCON_INPUT_L && layout.lStick
    ? (negconLDigital(digital, layout)
         ? static_cast<uint8_t>((digital & ~NEGCON_INPUT_L) | NEGCON_INPUT_L_STICK)
         : static_cast<uint8_t>(digital & ~NEGCON_INPUT_L_STICK))
    : static_cast<uint8_t>(digital ^ input);
}

// L's pressure as the right stick X: centered at rest and moving right with
// the squeeze, so anything reading it as a stick sees no deflection at rest.
inline constexpr int16_t negconStickValue(uint8_t pressure) {
  return static_cast<int16_t>(pressure >> 1);
}

// Where L's axis rests. MiSTer neGcon sends L as a pedal released at 255. MiSTer
// DInput has no L axis: RX there is the right stick X, which the PSX core reads
// as a DualShock's, so it rests centred (0). Holding it at the pedal's 255 sent
// the right stick fully right (owner's bench, 2026-11-14).
inline constexpr int16_t negconLAxisRest(const NegconOutputLayout& layout) {
  return layout.lPedal ? negconAxisValue(0, true) : 0;
}

// The axes of a neutral neGcon frame, which rest at 255 where they are inverted.
inline void neutralizeNegconAxes(controller_state_t& frame, const NegconOutputLayout& layout) {
  if (layout.triggerAxes) {
    frame.ANALOG_R2 = frame.ANALOG_L2 = negconAxisValue(0, layout.invertedTriggers);
  }
  if (layout.lAxisWritten) {
    frame.RX = negconLAxisRest(layout);
  }
}

// Whether this output can send the input's pressure. Without one the input
// is always a press, and the Quick Menu hides its row.
inline bool negconInputHasPressure(const NegconOutputLayout& layout,
                                   uint8_t input) {
  if (layout.facePressure) return true;
  return input == NEGCON_INPUT_L ? layout.lPedal || layout.lStick : layout.triggerAxes;
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
  if ((layout.lPedal || layout.lStick) && !negconLDigital(digital, layout)) frame.L1 = 0;
}

// Applies the choice to a neGcon output frame before turbo and remap. Expects
// the decoder's frame: the I/II/L presses in A/X/L1, their pressure in
// ANALOG_A/X/L1, the I/II trigger copies in ANALOG_R2/L2 and, on MiSTer
// outputs, the L axis in RX.
inline void applyNegconInputModes(controller_state_t& frame, uint8_t digital,
                                  const NegconOutputLayout& layout) {
  const bool iDigital = (digital & NEGCON_INPUT_I) != 0;
  const bool iiDigital = (digital & NEGCON_INPUT_II) != 0;
  const bool lDigital = negconLDigital(digital, layout);

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
    frame.RX = layout.lPedal && !lDigital ? negconAxisValue(lPressure, true)
                                          : negconLAxisRest(layout);
  }
  // A Digital L leaves the right stick alone, so a 2P Merge right stick on
  // this frame still gets through.
  if (layout.lStick && !lDigital) {
    frame.RX = negconStickValue(lPressure);
  }
}

// Scales each Analog input's pressure after applyNegconInputModes(), on the
// channel that carries it: face pressure on PS3/Xbox, the I/II trigger axes
// (released at 255 on MiSTer neGcon), the MiSTer neGcon L pedal and the L
// right stick. Digital inputs and the half-travel press are unchanged.
inline void applyNegconAnalogGain(controller_state_t& frame, uint8_t digital,
                                  uint8_t gain, const NegconOutputLayout& layout) {
  if (gain % NEGCON_GAIN_COUNT == 0) return;
  const bool iAnalog = (digital & NEGCON_INPUT_I) == 0;
  const bool iiAnalog = (digital & NEGCON_INPUT_II) == 0;
  const bool lAnalog = !negconLDigital(digital, layout);
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
  if (layout.lStick && lAnalog) {
    const uint8_t stick = negconGainPressure(static_cast<uint8_t>(frame.RX), gain);
    frame.RX = stick > 127 ? 127 : stick;
  }
}

}  // extern "C++"
