#include "../product_config.h"
#include "controller_runtime_internal.h"

#include <Arduino.h>

#include "analog_calibration_state.h"
#include "classic_analog_range.h"
#include "controller_button_pressure.h"
#include "controller_frame_state.h"
#include "controller_output_cache_state.h"
#include "controller_settings_state.h"
#include "rotary_motion_state.h"
#include "rotary_runtime_policy.h"
#include "rotary_output_transform.h"
#include "controller_runtime_product_transforms.h"
#include "device_runtime_state.h"
#include "button_map_mode.h"
#include "dpad_mode.h"
#include "negcon_input_modes.h"
#include "trigger_button_policy.h"
#include "../input/shared/input_button_bits.h"
#include "../input/jaguar/input_jaguar_runtime_state.h"
#include "../menu/menu_ui_state.h"
#include "../output/output_runtime_state.h"
#include "../platform/latency_service_mask.h"
#include "../platform/runtime/platform_input_transform_runtime.h"
#include "n64_output_helpers.h"
#include "n64_assignment_runtime.h"

#include <cstring>

namespace controller_runtime_internal {

namespace {

controller_state_t menuOutputRestoreFrames[MAX_USB_OUT];
bool menuOutputRestoreFrameValid[MAX_USB_OUT] = {};
bool menuOutputRestoreValid = false;
controller_state_t outputFinalizeRestoreFrames[MAX_USB_OUT];
bool outputFinalizeRestoreFrameValid[MAX_USB_OUT] = {};
bool outputFinalizeRestoreActive = false;
uint32_t processedButtons[MAX_USB_OUT] = {};
bool processedButtonsValid[MAX_USB_OUT] = {};

#ifdef ENABLE_INPUT_PSX
NegconOutputLayout negconLayoutForFrame(const controller_state_t& frame);
#endif

void neutralizeControllerFrameForMenu(controller_state_t& frame) {
  frame.digital_buttons = 0;
  frame.extended_buttons = 0;
  frame.analog_sticks = 0;
  frame.analog_buttons = 0;
  frame.analog_pad = 0;
  frame.paddle = 128;
  frame.spinner = 0;
  frame.mouse_x = 0;
  frame.mouse_y = 0;
  frame.mouse_wheel_x = 0;
  frame.mouse_wheel_y = 0;
#ifdef ENABLE_INPUT_PSX
  if (frameIsNegcon(frame)) {
    neutralizeNegconAxes(frame, negconLayoutForFrame(frame));
  }
#endif
}

bool shouldApplyAnalogCalibrationToCurrentMode() {
#if !ADAPT_POLICY_LEGACY_ANALOG_CALIBRATION
  return false;
#endif
  if (!analogCalibration.enabled) {
    return false;
  }

  if (platformShouldBypassAnalogCalibrationForInputMode(deviceMode)) {
    return false;
  }

  switch (deviceMode) {
    #ifdef ENABLE_INPUT_N64
    case RZORD_N64:
      return false;
    #endif
    #ifdef ENABLE_INPUT_GAMECUBE
    case RZORD_GAMECUBE:
      return wii_analog_range == CLASSIC_ANALOG_RANGE_CALIBRATED;
    #endif
    #ifdef ENABLE_INPUT_WII
    case RZORD_WII:
      return wii_analog_range == CLASSIC_ANALOG_RANGE_CALIBRATED;
    #endif
    default:
      return true;
  }
}

bool outputCacheInputHasIndependentDigitalTriggers() {
  #ifdef ENABLE_INPUT_PSX
  if (deviceMode == RZORD_PSX) {
    return true;
  }
  #endif
  return false;
}

bool outputCacheAnalogTriggersActive(const controller_state_t& frame) {
  const uint8_t triggerMode = triggerModeForFrame(frame);
  return frame.HAS_ANALOG_TRIGGERS &&
         (triggerMode == TRIGGER_MODE_ANALOG ||
          triggerMode == TRIGGER_MODE_RSTICK ||
          triggerMode == TRIGGER_MODE_BOTH);
}

bool wiiClassicAnalogTriggersStayAnalog(const controller_state_t& frame) {
  return std::strcmp(frame.controller_type_name, "Classic") == 0;
}

bool zOutputPolicyAppliesToCurrentMode() {
#ifdef ENABLE_INPUT_N64
  if (deviceMode == RZORD_N64) {
    return true;
  }
#endif
#ifdef ENABLE_INPUT_GAMECUBE
  if (deviceMode == RZORD_GAMECUBE) {
    return true;
  }
#endif
  return false;
}

void applyTriggerModeToCachedButtons(const controller_state_t& frame, uint32_t& buttons) {
  // Digital-only products promote physical triggers BEFORE remapping. At this
  // stage R2 may be HK assigned to LB, not the physical trigger. Do not erase
  // that assignment (Analog mode), or re-add an unassigned trigger (Both).
  if (controllerProductRequiresDigitalTriggerSources()) {
    return;
  }
  const outputMode_t output = canonicalizeOutputMode(get_effective_output_mode());
  // These reports already carry the prepared, remapped trigger sources.
  if (output == OUTPUT_HID || output == OUTPUT_MISTER) {
    return;
  }
  if (!frame.HAS_ANALOG_TRIGGERS) {
    return;
  }
  const uint8_t triggerMode = triggerModeForFrame(frame);
  if (!wiiClassicAnalogTriggersStayAnalog(frame) &&
      (triggerMode == TRIGGER_MODE_DIGITAL || triggerMode == TRIGGER_MODE_BOTH)) {
    if (frame.ANALOG_L2 > TRIGGER_DIGITAL_THRESHOLD) buttons |= INPUT_L2;
    if (frame.ANALOG_R2 > TRIGGER_DIGITAL_THRESHOLD) buttons |= INPUT_R2;
  }
  if (triggerMode == TRIGGER_MODE_ANALOG &&
      !outputCacheInputHasIndependentDigitalTriggers()) {
    buttons &= ~(INPUT_L2 | INPUT_R2);
  }
}

#ifdef ENABLE_INPUT_PSX
// The decoder carries I/II on the trigger channels exactly when this output
// has analog triggers, and reports that in HAS_ANALOG_TRIGGERS.
NegconOutputLayout negconLayoutForFrame(const controller_state_t& frame) {
  const outputMode_t output = canonicalizeOutputMode(get_effective_output_mode());
  return negconOutputLayout(output, frame.HAS_ANALOG_TRIGGERS);
}

// Runs from flash, like the rotary tuning, to keep the RAM rebuild small.
__attribute__((noinline)) void applyNegconInputModesToOutputFrame(controller_state_t& frame,
                                                                  uint32_t heldThroughMenu) {
  if (!frameIsNegcon(frame)) {
    return;
  }
  // An input still held from a menu sends what a released Digital input sends.
  uint8_t digital = negcon_digital_inputs;
  if (heldThroughMenu & INPUT_A) digital |= NEGCON_INPUT_I;
  if (heldThroughMenu & INPUT_X) digital |= NEGCON_INPUT_II;
  if (heldThroughMenu & INPUT_L1) digital |= NEGCON_INPUT_L;
  const NegconOutputLayout layout = negconLayoutForFrame(frame);
  applyNegconInputModes(frame, digital, layout);
  applyNegconAnalogGain(frame, digital, negcon_gain, layout);
}
#endif

}  // namespace

#ifdef ENABLE_INPUT_PSX
__attribute__((noinline)) void applyNegconInputPressesToProcessedFrames() {
  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    controller_state_t& frame = controllerFrame(i);
    if (frame.connected && frameIsNegcon(frame)) {
      applyNegconInputPresses(frame, negcon_digital_inputs, negconLayoutForFrame(frame));
    }
  }
}
#endif

void cacheProcessedControllerOutputStateInternal() {
  for (uint8_t i = 0; i < max_devices; ++i) {
    const controller_state_t& frame = controllerFrameConst(i);
    uint32_t buttons = applyNintendoPositionButtonMap(
      applyZButtonOutputMode(frame.digital_buttons, frame),
      effectivePositionButtonMap(
        deviceMode,
        get_effective_output_mode(),
        is_nso_special_active(),
        button_map_mode));

    int16_t lx = frame.LX;
    int16_t ly = frame.LY;
    int16_t rx = frame.RX;
    int16_t ry = frame.RY;

    const uint8_t effectiveDpadMode = effective_dpad_mode_for_sticks(
      std::strcmp(frame.controller_type_name, "Fishing") == 0 ? uint8_t(0) : dpad_mode,
      frame.HAS_ANALOG_STICK_MAIN,
      frame.HAS_ANALOG_STICK_AUX);
    if (effectiveDpadMode == DPAD_MODE_LEFT_STICK ||
        effectiveDpadMode == DPAD_MODE_RIGHT_STICK) {
      int16_t dpad_x = 0;
      int16_t dpad_y = 0;
      if (frame.PAD_L) dpad_x = -127;
      if (frame.PAD_R) dpad_x = 127;
      if (frame.PAD_U) dpad_y = -127;
      if (frame.PAD_D) dpad_y = 127;

      if (effectiveDpadMode == DPAD_MODE_LEFT_STICK) {
        lx = dpad_x;
        ly = dpad_y;
      } else {
        rx = dpad_x;
        ry = dpad_y;
      }
      buttons &= ~(INPUT_PAD_U | INPUT_PAD_D | INPUT_PAD_L | INPUT_PAD_R);
    }

    applyTriggerModeToCachedButtons(frame, buttons);
    post_remap_buttons[i] = buttons;

    if (stick_invert & 0x01) ly = -ly;
    if (stick_invert & 0x02) ry = -ry;

    post_output_lx[i] = lx;
    post_output_ly[i] = ly;
    post_output_rx[i] = rx;
    post_output_ry[i] = ry;
    post_output_l2[i] = outputCacheAnalogTriggersActive(frame) ? frame.ANALOG_L2 : 0;
    post_output_r2[i] = outputCacheAnalogTriggersActive(frame) ? frame.ANALOG_R2 : 0;
  }
}

void __not_in_flash_func(captureOutputFinalizeRestoreFrames)() {
  outputFinalizeRestoreActive = true;
  for (uint8_t i = 0; i < MAX_USB_OUT; ++i) {
    outputFinalizeRestoreFrameValid[i] = false;
  }

  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    const controller_state_t& frame = controllerFrameConst(i);
    if (frame.connected) {
      outputFinalizeRestoreFrames[i] = frame;
      outputFinalizeRestoreFrameValid[i] = true;
    }
  }
}

void __not_in_flash_func(retainOutputFinalizeRestoreFrame)(uint8_t index) {
  outputFinalizeRestoreFrames[index] = controllerFrameConst(index);
  outputFinalizeRestoreFrameValid[index] = true;
}

// A button pressed in a menu, usually the one that closed it, is not a new
// press for the host: it stays out of the output until released. Streaming
// outputs would otherwise send it on the next report. D-pad bits stay live,
// because Konami pads report them held in every reply.
uint32_t __not_in_flash_func(updateHeldAcrossMenuClose)(uint8_t port, bool menuOwned) {
  constexpr uint32_t kDirections =
      INPUT_PAD_U | INPUT_PAD_D | INPUT_PAD_L | INPUT_PAD_R;
  held_across_menu_close[port] =
      pre_remap_buttons[port] & (menuOwned ? ~kDirections : held_across_menu_close[port]);
  return held_across_menu_close[port];
}

void __not_in_flash_func(rebuildDigitalButtonsForOutputFrame)(bool applyRemap,
                                                              bool applyChordRemap,
                                                              bool applyTurbo,
                                                              bool applyVirtualHotkeys) {
  const bool menuOwned = menuOwnsControllerInput();
  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    controller_state_t& frame = controllerFrame(i);
    const uint32_t heldThroughMenu = updateHeldAcrossMenuClose(i, menuOwned);
    if (frame.connected) {
      frame.digital_buttons = pre_remap_buttons[i] & ~heldThroughMenu;
      if (heldThroughMenu != 0) setControllerPressureForMask(frame, heldThroughMenu, 0);
      if (applyVirtualHotkeys) {
        frame.digital_buttons &= ~suppressedControllerHotkeySourceButtons(i);
      }
      applyAnalogOnlyTriggerButtons(frame, trigger_mode, get_effective_output_mode());
#ifdef ENABLE_INPUT_PSX
      // Before turbo and remap, so they see only what the host will get.
      if (deviceMode == RZORD_PSX) applyNegconInputModesToOutputFrame(frame, heldThroughMenu);
#endif
    }
  }

#if !defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
  if (applyTurbo && !latencyServiceDisabled(LATENCY_SERVICE_TURBO_TRANSFORM)) {
    applyTurboTransforms();
  }
#endif
  if (applyRemap && !latencyServiceDisabled(LATENCY_SERVICE_BUTTON_REMAP)) {
    applyButtonRemapTransforms();
  }
  if (applyChordRemap && !latencyServiceDisabled(LATENCY_SERVICE_CHORD_REMAP)) {
    applyButtonChordRemapTransforms();
  }
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
  if (applyTurbo && !latencyServiceDisabled(LATENCY_SERVICE_TURBO_TRANSFORM)) {
    applyTurboTransforms();
  }
#endif
}

void __not_in_flash_func(applyAnalogCalibrationToConnectedInputs)() {
  if (!shouldApplyAnalogCalibrationToCurrentMode()) {
    return;
  }
  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    controller_state_t& frame = controllerFrame(i);
    if (frame.connected) {
      applyAnalogCalibrationToReport(frame);
    }
  }
}

void applyRotaryTuningToOutputFrames() {
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  const outputMode_t output = get_effective_output_mode();
  for (uint8_t port = 0; port < max_devices && port < MAX_USB_OUT; ++port) {
    auto& frame = controllerFrame(port);
    bool jaguarActive = false;
#ifdef ENABLE_INPUT_JAGUAR
    jaguarActive = deviceMode == RZORD_JAGUAR && jaguarRotaryActivePorts[port];
#endif
    const RotaryInputKind kind = rotaryInputKindForFrame(frame, jaguarActive);
    if (kind == RotaryInputKind::None) continue;
    const uint8_t jogconMode = std::strcmp(frame.controller_type_name, "JogCon-S") == 0 ? 0 :
        std::strcmp(frame.controller_type_name, "JogCon-Fake") == 0 ? 3 : 1;
    applyRotaryOutputTuning(frame, kind, output, currentRotaryTuning(kind, jogconMode),
                           menu_jogcon_wheel_axis);
  }
#endif
}

void __not_in_flash_func(applyOutputButtonModeMappingsToConnectedInputs)() {
  if (n64AssignmentActive()) {
    for (uint8_t i=0; i<max_devices && i<MAX_USB_OUT; ++i) {
      auto& frame = controllerFrame(i);
      if (!frame.connected) continue;
      if (controllerFrameTypeNameIsN64(frame)) applyN64OutputAssignments(frame);
      else frame.digital_buttons = applyZButtonOutputMode(frame.digital_buttons, frame);
    }
    return;
  }
  if (!zOutputPolicyAppliesToCurrentMode()) {
    return;
  }
  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    controller_state_t& frame = controllerFrame(i);
    if (frame.connected) {
      frame.digital_buttons = applyZButtonOutputMode(frame.digital_buttons, frame);
    }
  }
}

void __not_in_flash_func(clearJaguarRotaryDpadBeforeUsbSend)() {
  #ifdef ENABLE_INPUT_JAGUAR
  if (deviceMode == RZORD_JAGUAR && jaguarRotaryActive) {
    for (uint8_t i = 0; i < MAX_USB_OUT; ++i) {
      if (!jaguarRotaryActivePorts[i]) {
        continue;
      }
      controller_state_t& frame = controllerFrame(i);
      frame.PAD_L = 0;
      frame.PAD_R = 0;
      frame.digital_buttons &= ~((1UL << 30) | (1UL << 31));
    }
  }
  #endif
}

void __not_in_flash_func(consumeMenuCapturedControllerOutput)() {
  if (!menuOwnsControllerInput()) {
    menuOutputRestoreValid = false;
    for (uint8_t i = 0; i < MAX_USB_OUT; ++i) {
      menuOutputRestoreFrameValid[i] = false;
    }
    return;
  }

  // Movement used while navigating a menu must not replay when it closes.
  resetAllRotaryMotion();
  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    controller_state_t& frame = controllerFrame(i);
    if (frame.connected) {
      menuOutputRestoreFrames[i] = frame;
      menuOutputRestoreFrameValid[i] = true;
      neutralizeControllerFrameForMenu(frame);
    } else {
      menuOutputRestoreFrameValid[i] = false;
    }
  }
  menuOutputRestoreValid = true;
}

void __not_in_flash_func(restorePhysicalButtonsAfterUsbSend)() {
  if (outputFinalizeRestoreActive) {
    for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
      if (outputFinalizeRestoreFrameValid[i]) {
        controllerFrame(i) = outputFinalizeRestoreFrames[i];
        // Keep held-button transforms idempotent between polls so remap/turbo never feeds back into itself.
        // Swapped remaps such as A<->B must always start from physical buttons, not the last remapped USB frame.
        controllerFrame(i).digital_buttons = pre_remap_buttons[i];
        outputFinalizeRestoreFrameValid[i] = false;
      }
    }
    outputFinalizeRestoreActive = false;
    menuOutputRestoreValid = false;
    for (uint8_t i = 0; i < MAX_USB_OUT; ++i) {
      menuOutputRestoreFrameValid[i] = false;
    }
    return;
  }

  if (menuOutputRestoreValid) {
    for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
      if (menuOutputRestoreFrameValid[i]) {
        controllerFrame(i) = menuOutputRestoreFrames[i];
        menuOutputRestoreFrameValid[i] = false;
      }
    }
    menuOutputRestoreValid = false;
    return;
  }

  for (uint8_t i = 0; i < max_devices; ++i) {
    controller_state_t& frame = controllerFrame(i);
    if (frame.connected) {
      frame.digital_buttons = pre_remap_buttons[i];
    }
  }
}

void __not_in_flash_func(noteProcessedButtonsAfterTransforms)() {
  for (uint8_t i = 0; i < MAX_USB_OUT; ++i) {
    processedButtonsValid[i] = i < max_devices && controllerFrameConst(i).connected;
    if (processedButtonsValid[i]) {
      processedButtons[i] = controllerFrameConst(i).digital_buttons;
    }
  }
}

void __not_in_flash_func(restorePhysicalButtonsBeforeInputPoll)() {
  // Input modules rewrite a frame only when the controller state changes, and
  // an unchanged poll sends no report, so nothing restores the frame after
  // its hotkey chords and remaps were consumed in place. Put the physical
  // buttons back before the module runs; otherwise the next snapshot reads the
  // transformed frame and a held shortcut releases itself after two polls.
  for (uint8_t i = 0; i < MAX_USB_OUT; ++i) {
    if (!processedButtonsValid[i]) {
      continue;
    }
    processedButtonsValid[i] = false;
    if (i >= max_devices) {
      continue;
    }
    controller_state_t& frame = controllerFrame(i);
    // Anything else written since that poll is newer than the snapshot.
    if (frame.connected && frame.digital_buttons == processedButtons[i]) {
      frame.digital_buttons = pre_remap_buttons[i];
    }
  }
}

}  // namespace controller_runtime_internal
