#pragma once

#include "button_remap.h"
#include "button_map_mode.h"
#include "../input/shared/input_button_bits.h"
#include "../output/output_capabilities.h"

inline n64_assignment::Family remapOutputFamily(outputMode_t mode, bool nativeN64 = false) {
  using F = n64_assignment::Family;
  switch (canonicalizeOutputMode(mode)) {
    case OUTPUT_XID: return F::XboxClassic;
    case OUTPUT_XINPUT: case OUTPUT_XINPUTW: case OUTPUT_XINPUT2P: case OUTPUT_XBOXONE: return F::Xbox;
    case OUTPUT_PS3: case OUTPUT_PS4: case OUTPUT_PS5: return F::PlayStation;
    case OUTPUT_SWITCH: case OUTPUT_SWITCHPRO: return nativeN64 ? F::NsoN64 : F::Switch;
    case OUTPUT_SWITCH2PRO: return F::Switch;
    default: return F::DInput;
  }
}
inline bool n64AssignmentSupported(DeviceEnum mode, outputMode_t output) {
#ifdef ENABLE_INPUT_N64
  if (mode != RZORD_N64) return false;
  switch (canonicalizeOutputMode(output)) {
    case OUTPUT_HID: case OUTPUT_MISTER: case OUTPUT_XID:
    case OUTPUT_XINPUT: case OUTPUT_XINPUTW: case OUTPUT_XINPUT2P:
    case OUTPUT_PS3: case OUTPUT_PS4: case OUTPUT_SWITCH: case OUTPUT_SWITCHPRO:
    case OUTPUT_SWITCH2PRO:
      return true;
    default: return false;
  }
#else
  (void)mode; (void)output;
  return false;
#endif
}
inline bool n64AssignmentActive() {
  return n64AssignmentSupported(deviceMode, get_effective_output_mode());
}
inline const char* remapOutputButtonName(outputMode_t output, uint8_t slot) {
  if (canonicalizeOutputMode(output) == OUTPUT_PS5 && slot == 11) return "Create";
  if (slot >= 14 && slot < 19) {
    static const char* const extras[] = {"B16", "B19", "B15", "B17", "B18"};
    return remapOutputFamily(output) == n64_assignment::Family::DInput ? extras[slot-14] : "--";
  }
  if (canonicalizeOutputMode(output) == OUTPUT_PS3) {
    if (slot == 10) return "Start";
    if (slot == 11) return "Select";
    if (slot == 13) return "--";
  }
  if ((output == OUTPUT_XINPUT || output == OUTPUT_XINPUTW || output == OUTPUT_XINPUT2P) && slot == 13) return "--";
  return n64_assignment::outputName(remapOutputFamily(output), slot);
}
inline const char* n64AssignmentName(uint8_t index, const uint8_t* remaps,
    outputMode_t output, uint8_t cMode, bool nativeN64) {
  if (index >= n64_assignment::count) return "--";
  if (cMode == N64CSTICK_AS_RS && !nativeN64 && index>=5 && index<=8) {
    static const char* const axes[] = {"RY-", "RY+", "RX-", "RX+"};
    return axes[index-5];
  }
  uint8_t targets[n64_assignment::count];
  const auto family = remapOutputFamily(output, nativeN64);
  n64_assignment::resolve(family, cMode != N64CSTICK_AS_RS, remaps, targets);
  return nativeN64 ? n64_assignment::outputName(family, targets[index])
                   : remapOutputButtonName(output, targets[index]);
}
inline void cycleN64Assignment(uint8_t index, uint8_t* remaps,
    outputMode_t output, uint8_t cMode, bool nativeN64, bool forward = true) {
  using namespace n64_assignment;
  if (index >= count || (cMode == N64CSTICK_AS_RS && !nativeN64 && index>=5 && index<=8)) return;
  const auto family = remapOutputFamily(output, nativeN64);
  const uint8_t src = sources[index];
  // Auto, supported output buttons, Off. Multiple inputs may share an output.
  int option = remaps[src] == off ? 15 : tagged(remaps[src]) ? (remaps[src]&15)+1 : 0;
  for (uint8_t attempts=0; attempts<16; ++attempts) {
    option = (option+(forward ? 1 : 15))%16;
    if (option == 15) { remaps[src] = off; return; }
    if (option == 0) { remaps[src] = src; return; }
    const uint8_t candidate = option-1;
    // XInput has no Capture; Xbox Classic has neither Home nor Capture.
    if ((canonicalizeOutputMode(output) == OUTPUT_PS3 && candidate == 13) ||
        (family == Family::Xbox && candidate == 13) ||
        (family == Family::XboxClassic && candidate >= 12) ||
        (family == Family::NsoN64 && (candidate == 8 || candidate == 9))) continue;
    remaps[src] = encode(candidate);
    return;
  }
}
// N64 D-pad rows (remap slots 19-22): a direction stays itself or becomes an
// output button, with the same tagged values as the other N64 assignments.
inline const char* n64DpadAssignmentName(uint8_t value, outputMode_t output, bool nativeN64) {
  if (!n64_assignment::tagged(value)) return "--";
  const auto family = remapOutputFamily(output, nativeN64);
  return nativeN64 ? n64_assignment::outputName(family, value & 0x0f)
                   : remapOutputButtonName(output, value & 0x0f);
}
inline void cycleN64DpadAssignment(uint8_t slot, uint8_t* remaps, outputMode_t output,
                                   bool nativeN64, bool forward = true) {
  using namespace n64_assignment;
  if (!remapSlotIsDpad(slot)) return;
  const auto family = remapOutputFamily(output, nativeN64);
  // Itself, then the supported output buttons. A direction is never Off.
  int option = tagged(remaps[slot]) ? (remaps[slot] & 15) + 1 : 0;
  for (uint8_t attempts=0; attempts<15; ++attempts) {
    option = (option+(forward ? 1 : 14))%15;
    if (option == 0) { remaps[slot] = slot; return; }
    const uint8_t candidate = option-1;
    if ((canonicalizeOutputMode(output) == OUTPUT_PS3 && candidate == 13) ||
        (family == Family::Xbox && candidate == 13) ||
        (family == Family::XboxClassic && candidate >= 12) ||
        (family == Family::NsoN64 && (candidate == 8 || candidate == 9))) continue;
    remaps[slot] = encode(candidate);
    return;
  }
}
inline uint32_t applyN64DpadAssignments(uint32_t buttons, const uint8_t* remaps) {
  static constexpr uint32_t pads[] = {INPUT_PAD_U, INPUT_PAD_D, INPUT_PAD_L, INPUT_PAD_R};
  uint32_t cleared = 0, added = 0;
  for (uint8_t d = 0; d < REMAP_DPAD_SLOTS && d < 4; ++d) {
    const uint8_t value = remaps[REMAP_DPAD_FIRST_SLOT + d];
    if (!n64_assignment::tagged(value)) continue;
    cleared |= pads[d];
    if (buttons & pads[d]) added |= uint32_t(1) << (value & 0x0f);
  }
  return (buttons & ~cleared) | added;
}
inline void applyN64OutputAssignments(controller_state_t& frame) {
  if (!n64AssignmentActive() || !controllerFrameTypeNameIsN64(frame) || frame.n64_output_assigned) return;
  uint8_t targets[n64_assignment::count];
  n64_assignment::resolve(remapOutputFamily(get_effective_output_mode(), output_uses_switch_n64_profile()),
      output_maps_n64_c_buttons_to_face_buttons(), active_remaps, targets);
  frame.digital_buttons = n64_assignment::project(frame.digital_buttons, targets);
  // Output slots are frame bits here, so assigned directions land after the
  // projection, which leaves the D-pad bits untouched.
  frame.digital_buttons = applyN64DpadAssignments(frame.digital_buttons, active_remaps);
  frame.n64_output_assigned = true;
}
