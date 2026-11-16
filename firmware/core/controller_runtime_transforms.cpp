#include "../product_config.h"
#include "n64_assignment_runtime.h"
#include "controller_runtime_internal.h"

#include <Arduino.h>

#include <cmath>
#include <cstdint>
#include <cstring>

#include "button_remap.h"
#include "button_chord_remap.h"
#include "controller_button_pressure.h"
#include "classic_dual_merge_config.h"
#include "controller_delivery_state.h"
#include "controller_frame_state.h"
#include "controller_axis_roles.h"
#include "controller_output_cache_state.h"
#include "rotary_motion_state.h"
#include "rotary_output_transform.h"
#include "rotary_runtime_policy.h"
#include "controller_settings_state.h"
#include "device_runtime_state.h"
#include "hotkey_combo.h"
#include "negcon_input_modes.h"
#include "../input/jaguar/input_jaguar_runtime_state.h"
#if defined(ADAPT_FEATURE_JAGUAR_TRACE)
#include "../features/jaguar_trace/jaguar_trace.h"
#endif
#include "../input/shared/input_button_bits.h"
#include "../menu/menu_runtime_state.h"
#include "../menu/menu_ui_state.h"
#include "../platform/rgb_led.h"
#include "../output/output_runtime_state.h"
#include "stick_center.h"
#include "turbo.h"
#include "../input/runtime/input_frame_runtime.h"
#include "controller_runtime_product_transforms.h"

namespace {

uint8_t dualMergePrimarySource = 0xFF;
uint8_t dualMergeSecondarySource = 0xFF;

struct Vector2i16 {
  int16_t x;
  int16_t y;
};

Vector2i16 __not_in_flash_func(hardRadialDeadzone)(int16_t rawX, int16_t rawY, uint16_t deadZone) {
  const float dx = static_cast<float>(rawX);
  const float dy = static_cast<float>(rawY);

  float magnitude = std::sqrt(dx * dx + dy * dy);

  if (magnitude <= static_cast<float>(deadZone)) {
    return { 0, 0 };
  }

  return { rawX, rawY };
}

int16_t __not_in_flash_func(stickPrecisionFullScale)(analog_stick_precision precision) {
  switch (precision) {
    case ANALOG_STICK_PRECISION_12:
      return INT16_MAX >> 4;
    case ANALOG_STICK_PRECISION_16:
      return INT16_MAX;
    case ANALOG_STICK_PRECISION_8:
    default:
      return INT8_MAX;
  }
}

int16_t __not_in_flash_func(scaleStickThresholdFrom8Bit)(analog_stick_precision precision, uint8_t threshold8) {
  const int32_t scaled =
      ((int32_t)threshold8 * stickPrecisionFullScale(precision)) / INT8_MAX;
  return (int16_t)(scaled < 1 ? 1 : scaled);
}

#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
bool __not_in_flash_func(remapRuntimeStateChanged)() {
  static uint8_t last_runtime_remaps[REMAP_MAX_BUTTONS] = {};
  static bool initialized = false;

  if (initialized &&
      std::memcmp(last_runtime_remaps, active_remaps, REMAP_MAX_BUTTONS) == 0) {
    return false;
  }

  std::memcpy(last_runtime_remaps, active_remaps, REMAP_MAX_BUTTONS);
  initialized = true;
  return true;
}
#endif

void __not_in_flash_func(apply_stick_deadzone)(controller_state_t& frame) {
  if (controllerProductBypassesStickDeadzone()) {
    return;
  }

  if (deadzone_percent == 0) {
    return;
  }

  uint16_t maxvalue = 0;
  uint16_t deadzone = 0;

  switch (frame.sticks_precision_bits) {
    case ANALOG_STICK_PRECISION_8:
      maxvalue = static_cast<uint16_t>(INT8_MAX);
      break;
    case ANALOG_STICK_PRECISION_12:
      maxvalue = static_cast<uint16_t>(INT16_MAX >> 4);
      break;
    case ANALOG_STICK_PRECISION_16:
      maxvalue = static_cast<uint16_t>(INT16_MAX);
      break;
  }
  deadzone = (maxvalue * deadzone_percent) / 100UL;

  if (frame.LX || frame.LY) {
    auto lstick = hardRadialDeadzone(frame.LX, frame.LY, deadzone);
    frame.LX = lstick.x;
    frame.LY = lstick.y;
  }
  if (!controllerHasReelSpeedAxis(frame) && (frame.RX || frame.RY)) {
    auto rstick = hardRadialDeadzone(frame.RX, frame.RY, deadzone);
    frame.RX = rstick.x;
    frame.RY = rstick.y;
  }
}

bool __not_in_flash_func(wiiClassicAnalogTriggersStayAnalog)(const controller_state_t& frame) {
  return std::strcmp(frame.controller_type_name, "Classic") == 0;
}

void __not_in_flash_func(apply_deadzone)(uint8_t i) {
  controller_state_t& frame = controllerFrame(i);
  apply_stick_deadzone(frame);

  const bool forceDigitalTriggerSources =
      controllerProductRequiresDigitalTriggerSources();
  const uint8_t triggerMode = triggerModeForFrame(frame);
  if (frame.HAS_ANALOG_TRIGGERS &&
      (forceDigitalTriggerSources ||
       !wiiClassicAnalogTriggersStayAnalog(frame)) &&
      (forceDigitalTriggerSources ||
       triggerMode == TRIGGER_MODE_DIGITAL || triggerMode == TRIGGER_MODE_BOTH)) {
    // DB15 is digital-only: trigger travel past the product threshold is a
    // press. Other USB outputs retain the shared Digital/Both threshold.
    frame.L2 |= forceDigitalTriggerSources
        ? controllerProductDigitalTriggerPressed(frame.ANALOG_L2)
        : frame.ANALOG_L2 > TRIGGER_DIGITAL_THRESHOLD;
    frame.R2 |= forceDigitalTriggerSources
        ? controllerProductDigitalTriggerPressed(frame.ANALOG_R2)
        : frame.ANALOG_R2 > TRIGGER_DIGITAL_THRESHOLD;
  }

  if (triggerMode == TRIGGER_MODE_DIGITAL) {
    frame.ANALOG_L2 = 0;
    frame.ANALOG_R2 = 0;
  }
}

}  // namespace

namespace controller_runtime_internal {

namespace {


bool __not_in_flash_func(frameHasAnalogStickStateForCentering)(const controller_state_t& frame) {
  return frame.HAS_ANALOG_STICK_MAIN || frame.HAS_ANALOG_STICK_AUX ||
         frame.LX != 0 || frame.LY != 0 || frame.RX != 0 || frame.RY != 0;
}

}  // namespace

void __not_in_flash_func(updateStickCenteringFromPoll)(bool updated) {
  (void)updated;
  // Input pollers present decoded analog values every completed poll, rebuilt
  // or restored (decoded_sticks.h). The updated flag only means the stable
  // state changed, so centering must still be re-applied while the controller
  // is resting at the same raw offset.

  #ifdef ENABLE_INPUT_JVS
  // JVS analog channels are cabinet-defined absolute inputs. They may be
  // centered sticks, pedals, guns, or other controls, so learning an idle
  // offset here can destroy valid absolute position data. Preserve the raw
  // JVS midpoint/range and let the explicit deadzone/invert settings operate
  // later in the normal neutral-frame pipeline.
  if (deviceMode == RZORD_JVS) {
    for (uint8_t i = 0; i < MAX_USB_OUT; ++i) {
      stickCenter.update(i, false, 0, 0, 0, 0);
    }
    return;
  }
  #endif

  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    controller_state_t& frame = controllerFrame(i);
    if (controllerProductHandlesStickCentering(i, frame)) {
      continue;
    }
    // A wheel or paddle rests wherever it was left, so its first position is
    // not stick drift. JogCon also rewrites its frame only when the state
    // changes, so a learned offset would be subtracted again on every poll and
    // walk the axis to an endpoint while the wheel sits still.
    if (rotaryInputKindForFrame(frame) != RotaryInputKind::None) {
      stickCenter.update(i, false, 0, 0, 0, 0);
      continue;
    }
    if (!frame.connected || !frameHasAnalogStickStateForCentering(frame)) {
      stickCenter.update(i, false, 0, 0, 0, 0);
      continue;
    }

    const int16_t autoCenterThreshold =
        scaleStickThresholdFrom8Bit(frame.sticks_precision_bits, 10);
    const int16_t restSnapThreshold =
        scaleStickThresholdFrom8Bit(frame.sticks_precision_bits, 2);
    int16_t sc_lx = frame.LX;
    int16_t sc_ly = frame.LY;
    const bool reelSpeedAxis = controllerHasReelSpeedAxis(frame);
    int16_t sc_rx = reelSpeedAxis ? 0 : frame.RX;
    int16_t sc_ry = reelSpeedAxis ? 0 : frame.RY;

    stickCenter.update(i, frame.connected, sc_lx, sc_ly, sc_rx, sc_ry,
                       autoCenterThreshold);
    if (frame.connected) {
      stickCenter.apply(i, sc_lx, sc_ly, sc_rx, sc_ry, restSnapThreshold);
      frame.LX = sc_lx;
      frame.LY = sc_ly;
      if (!reelSpeedAxis) {
        frame.RX = sc_rx;
        frame.RY = sc_ry;
      }
    }
  }
}

void __not_in_flash_func(applyDeadzoneAndTriggerTransforms)(bool updated) {
  (void)updated;
  // Input pollers rebuild raw frame values even when the stable controller
  // state is unchanged, so these transforms must run on every completed poll.
  // Digital-only products promote analog triggers even at default settings.
  const bool hasActiveFrameTransforms =
      deadzone_percent != 0 || stick_invert != 0 || trigger_mode != 0 ||
      controllerProductRequiresDigitalTriggerSources();
  if (!hasActiveFrameTransforms) {
    return;
  }

  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    if (controllerFrameConst(i).connected) {
      apply_deadzone(i);
    }
  }
}

void __not_in_flash_func(applyFinalStickDeadzoneToConnectedInputs)() {
  if (deadzone_percent == 0) {
    return;
  }

  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    if (controllerFrameConst(i).connected) {
      apply_stick_deadzone(controllerFrame(i));
    }
  }
}

void __not_in_flash_func(snapshotRawInputButtonsBeforeTransforms)() {
  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    const controller_state_t& frame = controllerFrameConst(i);
    raw_input_buttons[i] = frame.connected ? frame.digital_buttons : 0;
  }
}

void __not_in_flash_func(snapshotPhysicalButtonsBeforeTransforms)() {
  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    const controller_state_t& frame = controllerFrameConst(i);
    if (frame.connected) {
      pre_remap_buttons[i] = frame.digital_buttons;
      pre_transform_hotkey_buttons[i] = hotkeyButtonsForControllerFrame(frame);
    } else {
      pre_remap_buttons[i] = 0;
      pre_transform_hotkey_buttons[i] = 0;
    }
  }
}

void __not_in_flash_func(applyButtonRemapTransforms)() {
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
  // Cabinet turbo is intentionally attached to virtual arcade actions.
  if (remapRuntimeStateChanged()) turbo.resetRuntimeState();
#endif
  // Turbo belongs to physical inputs. Editing a destination must not reset
  // its phase/latch or alter rates. N64 assignments run once at finalization.

  if (!hasActiveRemap() || menuOwnsControllerInput()) {
    return;
  }

  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    controller_state_t& frame = controllerFrame(i);
    if (frame.connected) {
      if (n64AssignmentActive() && controllerFrameTypeNameIsN64(frame)) continue;
      ButtonRemapMenu::applyRemap(frame, active_remaps);
    }
  }
}

void __not_in_flash_func(applyButtonChordRemapTransforms)() {
#if ADAPT_POLICY_TOURNAMENT_KIOSK
  // Chord remaps can synthesize a multi-button macro. Tournament mode retains
  // ordinary one-to-one remaps but must bypass those expansions at runtime.
  if (menu_kiosk_mode != 0) {
    return;
  }
#endif
  if (!hasActiveButtonChordRemap()) {
    return;
  }

  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    controller_state_t& frame = controllerFrame(i);
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
    // Fixed Start+direction shortcuts belong exclusively to the per-mode
    // action picker, including when disabled. Legacy multi-output chords
    // must not provide a second hidden assignment for the same shortcut.
    const uint32_t physical = pre_transform_hotkey_buttons[i];
    if (physical == (INPUT_START|INPUT_PAD_U) || physical == (INPUT_START|INPUT_PAD_D) ||
        physical == (INPUT_START|INPUT_PAD_L) || physical == (INPUT_START|INPUT_PAD_R)) continue;
#endif
    applyButtonChordRemapsToFrame(frame);
  }
}

void __not_in_flash_func(applyTurboTransforms)() {
#if ADAPT_POLICY_TOURNAMENT_KIOSK
  static bool tournamentWasActive = false;
  const bool tournamentActive = menu_kiosk_mode != 0;
  if (tournamentActive != tournamentWasActive) {
    turbo.resetRuntimeState();
    tournamentWasActive = tournamentActive;
  }
  // Tournament mode is a runtime lockout. Preserve each player's saved turbo
  // configuration so leaving the venue does not destroy their preferences.
  if (tournamentActive) {
    return;
  }
#endif
  if (!turbo.hasAnyEnabled() || menuOwnsControllerInput()) {
    return;
  }

  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    controller_state_t& frame = controllerFrame(i);
    if (!frame.connected) {
      continue;
    }

    // This runs in the process stage on the physical frame and again in the
    // output stage, which drops buttons still held from a menu. Both calls
    // must give the press tracker the same view, or every pass reads as a
    // new press and a latched rate flips each pass until the release.
    turbo.updateRawButtons(
        i, frame.digital_buttons & ~(held_across_menu_close[i] & pre_remap_buttons[i]));
    const uint32_t beforeTurbo = frame.digital_buttons;
    frame.digital_buttons = turbo.applyTurbo(i, frame.digital_buttons);
    setControllerPressureForMask(frame, beforeTurbo & ~frame.digital_buttons, 0);
    setControllerPressureForMask(frame, frame.digital_buttons & ~beforeTurbo, 255);
    if (turbo.isHoldingTurboButton(i)) {
      requestControllerFrameDelivery(i);
    }
  }
}

void __not_in_flash_func(captureDualInputMergeSourceSlots)() {
  dualMergePrimarySource = 0xFF;
  dualMergeSecondarySource = 0xFF;
  const uint8_t count = min(inputPortCount(), (uint8_t)MAX_USB_OUT);
  for (uint8_t index = 0; index < count; ++index) {
    if (!controllerFrameConst(index).connected) continue;
    if (dualMergePrimarySource == 0xFF) {
      dualMergePrimarySource = index;
    } else {
      dualMergeSecondarySource = index;
      break;
    }
  }
}

bool __not_in_flash_func(dualMergePrimaryIsFirstSlot)() {
  return dualMergePrimarySource == 0;
}

void __not_in_flash_func(applyClassicDualControllerMergeTransform)() {
#if defined(ADAPT_CAP_DUAL_INPUT_MERGE)
  static uint32_t lastMergedPlayer2Actions = 0;
  static uint8_t lastMergedPlayer2AnalogL2 = 0;
  static uint8_t lastMergedPlayer2AnalogR2 = 0;
  static int16_t lastMergedPlayer2Rx = 0;
  static int16_t lastMergedPlayer2Ry = 0;
  static uint32_t lastMergedMask = 0;
  static uint8_t lastPrimarySource = 0xFF;
  static uint8_t lastSecondarySource = 0xFF;

  // Mouse players retain physical-port identity even if this input mode's
  // gamepad profile has 2P Merge enabled.
  if (!classic_dual_merge_enabled || MAX_USB_OUT < 2 ||
      get_effective_output_mode() == OUTPUT_ROTARY_MOUSE ||
      get_effective_output_mode() == OUTPUT_REMOTE) {
    lastMergedPlayer2Actions = 0;
    lastMergedPlayer2AnalogL2 = 0;
    lastMergedPlayer2AnalogR2 = 0;
    lastMergedPlayer2Rx = 0;
    lastMergedPlayer2Ry = 0;
    lastMergedMask = 0;
    lastPrimarySource = 0xFF;
    lastSecondarySource = 0xFF;
    return;
  }

  if (dualMergePrimarySource == 0xFF) {
    // The last controller left: send the empty slot so nothing stays held.
    if (lastPrimarySource != 0xFF) {
      requestControllerFrameDelivery(0);
    }
    lastPrimarySource = 0xFF;
    lastSecondarySource = 0xFF;
    return;
  }
  const controller_state_t primarySnapshot =
      controllerFrameConst(dualMergePrimarySource);
  const bool secondaryConnected = dualMergeSecondarySource != 0xFF;
  const controller_state_t secondarySnapshot = secondaryConnected
      ? controllerFrameConst(dualMergeSecondarySource)
      : controller_state_t{};
  if (dualMergePrimarySource != 0) {
    // Slot 0 is empty. Fill it for this send only: a copy left behind would
    // look like a real P1 on the next pass and demote P2 to the secondary.
    retainOutputFinalizeRestoreFrame(0);
    controllerFrame(0) = primarySnapshot;
  }
  controller_state_t& player1 = controllerFrame(0);
  if (!secondaryConnected) {
    // Only slot 0 reports under merge, so a lone P2's changes go out there.
    const bool loneP2Changed = dualMergePrimarySource != 0 &&
        controllerFrameNeedsDelivery(dualMergePrimarySource);
    if (loneP2Changed) {
      completeControllerFrameDelivery(dualMergePrimarySource);
    }
    if (loneP2Changed || lastPrimarySource != dualMergePrimarySource ||
        lastSecondarySource != 0xFF) {
      requestControllerFrameDelivery(0);
    }
    lastMergedPlayer2Actions = 0;
    lastMergedPlayer2AnalogL2 = 0;
    lastMergedPlayer2AnalogR2 = 0;
    lastMergedPlayer2Rx = 0;
    lastMergedPlayer2Ry = 0;
    lastMergedMask = 0;
    lastPrimarySource = dualMergePrimarySource;
    lastSecondarySource = 0xFF;
    return;
  }
  if (deviceMode == RZORD_PSX &&
      std::strcmp(player1.controller_type_name, "neGcon") == 0) {
    lastMergedPlayer2Actions = 0;
    lastMergedPlayer2AnalogL2 = 0;
    lastMergedPlayer2AnalogR2 = 0;
    lastMergedPlayer2Rx = 0;
    lastMergedPlayer2Ry = 0;
    lastMergedMask = 0;
    return;
  }
  if (!player1.connected) {
    return;
  }

  const uint32_t actionButtonMask =
    sanitizeClassicDualMergeP2Mask(deviceMode, classic_dual_merge_p2_mask);
  const uint32_t digitalButtonMask = actionButtonMask &
      ~(DUAL_INPUT_MERGE_LEFT_STICK | DUAL_INPUT_MERGE_RIGHT_STICK);
  const bool n64Merge = (deviceMode == RZORD_N64);
  const uint32_t n64CButtonMask = INPUT_X | INPUT_Y | INPUT_L3 | INPUT_R3;

  const uint32_t player2Actions = secondaryConnected
    ? (secondarySnapshot.digital_buttons & digitalButtonMask)
    : 0;
  const uint8_t player2AnalogL2 = secondaryConnected ? secondarySnapshot.ANALOG_L2 : 0;
  const uint8_t player2AnalogR2 = secondaryConnected ? secondarySnapshot.ANALOG_R2 : 0;
  const int16_t player2Rx = (secondaryConnected && n64Merge) ? secondarySnapshot.RX : 0;
  const int16_t player2Ry = (secondaryConnected && n64Merge) ? secondarySnapshot.RY : 0;

  player1.digital_buttons = (player1.digital_buttons & ~digitalButtonMask) | player2Actions;
  if (secondaryConnected && (digitalButtonMask & (INPUT_L2 | INPUT_R2)) != 0) {
    player1.HAS_ANALOG_TRIGGERS |= secondarySnapshot.HAS_ANALOG_TRIGGERS;
    if ((actionButtonMask & INPUT_L2) != 0) {
      player1.ANALOG_L2 = player2AnalogL2;
    }
    if ((actionButtonMask & INPUT_R2) != 0) {
      player1.ANALOG_R2 = player2AnalogR2;
    }
  } else if ((digitalButtonMask & (INPUT_L2 | INPUT_R2)) != 0) {
    if ((actionButtonMask & INPUT_L2) != 0) {
      player1.ANALOG_L2 = 0;
    }
    if ((actionButtonMask & INPUT_R2) != 0) {
      player1.ANALOG_R2 = 0;
    }
  }
  if ((actionButtonMask & DUAL_INPUT_MERGE_LEFT_STICK) != 0) {
    player1.LX = secondaryConnected ? secondarySnapshot.LX : 0;
    player1.LY = secondaryConnected ? secondarySnapshot.LY : 0;
    if (secondaryConnected) {
      player1.HAS_ANALOG_STICK_MAIN |= secondarySnapshot.HAS_ANALOG_STICK_MAIN;
    }
  }
  if ((actionButtonMask & DUAL_INPUT_MERGE_RIGHT_STICK) != 0) {
    player1.RX = secondaryConnected ? secondarySnapshot.RX : 0;
    player1.RY = secondaryConnected ? secondarySnapshot.RY : 0;
    if (secondaryConnected) {
      player1.HAS_ANALOG_STICK_AUX |= secondarySnapshot.HAS_ANALOG_STICK_AUX;
    }
  }
  if (n64Merge && (actionButtonMask & n64CButtonMask) != 0) {
    player1.RX = player2Rx;
    player1.RY = player2Ry;
    if (secondaryConnected) {
      player1.HAS_ANALOG_STICK_AUX |= secondarySnapshot.HAS_ANALOG_STICK_AUX;
    }
  }
  if (lastMergedPlayer2Actions != player2Actions ||
      lastMergedPlayer2AnalogL2 != player2AnalogL2 ||
      lastMergedPlayer2AnalogR2 != player2AnalogR2 ||
      lastMergedPlayer2Rx != player2Rx ||
      lastMergedPlayer2Ry != player2Ry ||
      lastMergedMask != actionButtonMask ||
      lastPrimarySource != dualMergePrimarySource ||
      lastSecondarySource != dualMergeSecondarySource) {
    requestControllerFrameDelivery(0);
  }
  lastMergedPlayer2Actions = player2Actions;
  lastMergedPlayer2AnalogL2 = player2AnalogL2;
  lastMergedPlayer2AnalogR2 = player2AnalogR2;
  lastMergedPlayer2Rx = player2Rx;
  lastMergedPlayer2Ry = player2Ry;
  lastMergedMask = actionButtonMask;
  lastPrimarySource = dualMergePrimarySource;
  lastSecondarySource = dualMergeSecondarySource;
#endif
}

void __not_in_flash_func(updateJaguarRotaryStateFromInputs)() {
  #ifdef ENABLE_INPUT_JAGUAR
  static uint8_t lastState[MAX_USB_OUT] = {0};
  static int32_t counter[MAX_USB_OUT] = {0};

  if (deviceMode != RZORD_JAGUAR) {
    // Re-entering Jaguar while the spinner output remains selected must still
    // baseline quadrature state rather than comparing it with a prior mode.
    jaguarSpinnerOutputWasActive = false;
    return;
  }

  static const int8_t quadTable[16] = {
     0, +1, -1,  0,
    -1,  0,  0, +1,
    +1,  0,  0, -1,
     0, -1, +1,  0
  };

  // The dedicated MiSTer Spinner and Mouse outputs explicitly opt Jaguar into
  // the relative-motion path.  Other outputs retain the legacy L+R-latched
  // absolute paddle path below.
  const bool spinnerOutputActive =
    rotaryOutputQueuesMotion(RotaryInputKind::Jaguar, get_effective_output_mode());
  const RotaryTuning rotaryTuning = currentRotaryTuning(RotaryInputKind::Jaguar);
  if (spinnerOutputActive != jaguarSpinnerOutputWasActive) {
    resetJaguarRotaryRuntimeState();
    for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
      const controller_state_t& frame = controllerFrameConst(i);
      const bool left = frame.ANALOG_PAD_L > 0;
      const bool right = frame.ANALOG_PAD_R > 0;
      lastState[i] = (left << 1) | right;
    }
    jaguarSpinnerOutputWasActive = spinnerOutputActive;
  }
  if (!spinnerOutputActive) {
    // Preserve the legacy Jaguar rotary mode for all existing outputs.  It
    // latches when Left+Right is held and provides its historic absolute
    // paddle/LX values using the existing Spin Speed setting.
    for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
      controller_state_t& frame = controllerFrame(i);
      const bool curL = frame.ANALOG_PAD_L > 0;
      const bool curR = frame.ANALOG_PAD_R > 0;
      const uint8_t curState = (curL << 1) | curR;

      if (curL && curR) {
        jaguarRotaryActivePorts[i] = true;
      }

      int8_t delta = 0;
      if (curState != lastState[i]) {
        const uint8_t tableIndex = (lastState[i] << 2) | curState;
        delta = quadTable[tableIndex];
      }

      if (jaguarRotaryActivePorts[i] && delta != 0) {
        // Jaguar's historic absolute path is baseline 4. Apply its policy
        // gain once and leave reversal to the shared absolute finalizer.
        const int32_t scaledDelta = -static_cast<int32_t>(delta) * 4 *
            rotaryGainNumerator(rotaryTuning.gain);
        const int32_t emitted = jaguarRotaryDivider[i].apply(
            scaledDelta, rotaryGainDenominator(rotaryTuning.gain));
        counter[i] = constrain(counter[i] + emitted, 0, 255);
        jaguarRotaryDisplayPhase[i] = static_cast<uint8_t>(
            jaguarRotaryDisplayPhase[i] + emitted);
      }

      if (jaguarRotaryActivePorts[i]) {
        const int16_t rotaryPosition = constrain(counter[i], 0, 255);
        frame.LX = rotaryPosition - 128;
        frame.paddle = rotaryPosition;
      }
      lastState[i] = curState;
    }

    bool anyJaguarRotary = false;
    for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
      if (jaguarRotaryActivePorts[i]) {
        anyJaguarRotary = true;
        break;
      }
    }
    jaguarRotaryActive = anyJaguarRotary;
    return;
  }

  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    controller_state_t& frame = controllerFrame(i);
    if (!frame.connected) {
      jaguarRotaryActivePorts[i] = false;
      jaguarRotaryDisplayPhase[i] = 0;
      jaguarRotaryDivider[i].reset();
      resetRotaryMotion(i);
      jaguarRotaryQuadrature[i].sample(false, false, false);
      lastState[i] = 0;
      continue;
    }
    bool curL = frame.ANALOG_PAD_L > 0;
    bool curR = frame.ANALOG_PAD_R > 0;
    jaguarRotaryActivePorts[i] = true;
    const int8_t delta = jaguarRotaryQuadrature[i].sample(true, curL, curR);
#if defined(ADAPT_FEATURE_JAGUAR_TRACE)
    jaguarTraceDecoded(i, curL, curR, delta);
#endif

    if (delta != 0) {
      // Dedicated relative spinner movement is baseline 4 and applies its
      // gain once before the divider. Unlike absolute paths, its reverse is
      // consumed here because the queued report is already relative.
      int32_t scaledDelta = -static_cast<int32_t>(delta) * 4 *
          rotaryGainNumerator(rotaryTuning.gain);
      if (rotaryTuning.reverse) {
        scaledDelta = -scaledDelta;
      }
      const int32_t emitted = jaguarRotaryDivider[i].apply(
          scaledDelta, rotaryGainDenominator(rotaryTuning.gain));
      jaguarRotaryDisplayPhase[i] = static_cast<uint8_t>(
        jaguarRotaryDisplayPhase[i] + scaledDelta);
      if (emitted != 0) {
#if defined(ADAPT_FEATURE_JAGUAR_TRACE)
        jaguarTraceMotion(i, emitted, peekRotaryMotion(i));
#endif
        addRotaryMotion(i, emitted);
        markInputFrameUpdated(i);
      }
    }
  }

  bool anyJaguarRotary = false;
  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    if (jaguarRotaryActivePorts[i]) {
      anyJaguarRotary = true;
      break;
    }
  }
  jaguarRotaryActive = anyJaguarRotary;
  #endif
}

void __not_in_flash_func(applySocdCleaningToInputs)() {
#if ADAPT_POLICY_SOCD_FIXED_OFF
  return;
#endif
  socdMode_t effectiveSocdMode = socdMode;
#if ADAPT_POLICY_TOURNAMENT_KIOSK
  // Neutral is the safest generic tournament policy. It is a no-op for a
  // mechanical lever unless contradictory directions are actually reported.
  if (menu_kiosk_mode != 0) effectiveSocdMode = SOCD_NEUTRAL;
#endif
  if (effectiveSocdMode == SOCD_OFF) {
    return;
  }

  static struct __attribute((packed, aligned(1))) {
    uint8_t PAD_U : 1;
    uint8_t PAD_D : 1;
    uint8_t PAD_L : 1;
    uint8_t PAD_R : 1;
    uint8_t : 4;
  } last_direction_state[MAX_USB_OUT] { 0 };

  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    controller_state_t& frame = controllerFrame(i);
    bool isSpecialLR = false;
    bool isSpecialUD = false;
    #ifdef ENABLE_INPUT_PSX
    if (deviceMode == RZORD_PSX) {
      isSpecialLR = frame.PAD_L && frame.PAD_R;
      isSpecialUD = frame.PAD_U && frame.PAD_D && !isSpecialLR;
    }
    #endif
    #ifdef ENABLE_INPUT_JAGUAR
    if (deviceMode == RZORD_JAGUAR) {
      isSpecialLR = frame.PAD_L && frame.PAD_R;
    }
    #endif
    #ifdef ENABLE_INPUT_JPC
    if (deviceMode == RZORD_JPC) {
      isSpecialLR = true;
      isSpecialUD = true;
    }
    #endif

    if (frame.PAD_U && frame.PAD_D && !isSpecialUD) {
      if (effectiveSocdMode == SOCD_NEUTRAL) {
        frame.PAD_U = 0;
        frame.PAD_D = 0;
      } else if (effectiveSocdMode == SOCD_SECOND && (last_direction_state[i].PAD_U || last_direction_state[i].PAD_D)) {
        frame.PAD_U = last_direction_state[i].PAD_D;
        frame.PAD_D = last_direction_state[i].PAD_U;
      } else if (effectiveSocdMode == SOCD_FIRST && (last_direction_state[i].PAD_U || last_direction_state[i].PAD_D)) {
        frame.PAD_U = last_direction_state[i].PAD_U;
        frame.PAD_D = last_direction_state[i].PAD_D;
      } else {
        last_direction_state[i].PAD_U = 0;
        last_direction_state[i].PAD_D = 0;
      }
    } else {
      last_direction_state[i].PAD_U = frame.PAD_U;
      last_direction_state[i].PAD_D = frame.PAD_D;
    }

    if (frame.PAD_L && frame.PAD_R && !isSpecialLR) {
      if (effectiveSocdMode == SOCD_NEUTRAL) {
        frame.PAD_L = 0;
        frame.PAD_R = 0;
      } else if (effectiveSocdMode == SOCD_SECOND && (last_direction_state[i].PAD_L || last_direction_state[i].PAD_R)) {
        frame.PAD_L = last_direction_state[i].PAD_R;
        frame.PAD_R = last_direction_state[i].PAD_L;
      } else if (effectiveSocdMode == SOCD_FIRST && (last_direction_state[i].PAD_L || last_direction_state[i].PAD_R)) {
        frame.PAD_L = last_direction_state[i].PAD_L;
        frame.PAD_R = last_direction_state[i].PAD_R;
      } else {
        last_direction_state[i].PAD_L = 0;
        last_direction_state[i].PAD_R = 0;
      }
    } else {
      last_direction_state[i].PAD_L = frame.PAD_L;
      last_direction_state[i].PAD_R = frame.PAD_R;
    }
  }
}

}  // namespace controller_runtime_internal
