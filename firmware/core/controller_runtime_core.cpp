#include "../product_config.h"
#include "directional_hotkeys.h"
#include "../menu/menu_ui_state.h"
#include "controller_runtime_core.h"

#include <Arduino.h>

#include "controller_runtime_internal.h"
#include "analog_calibration_state.h"
#include "button_chord_remap.h"
#include "button_remap.h"
#include "controller_button_pressure.h"
#include "controller_output_cache_state.h"
#include "controller_settings_state.h"
#include "device_runtime_state.h"
#include "firmware_hotkeys.h"
#include "turbo.h"
#include "trigger_button_policy.h"
#include "../platform/latency_service_mask.h"
#include "../output/output_runtime_state.h"

namespace {

enum ControllerRuntimeFeature : uint16_t {
  RUNTIME_FEATURE_BOOTLOADER_HOTKEY = 1u << 0,
  RUNTIME_FEATURE_DPAD_HOTKEY       = 1u << 1,
  RUNTIME_FEATURE_VIRTUAL_HOTKEYS   = 1u << 2,
  RUNTIME_FEATURE_BUTTON_REMAP      = 1u << 3,
  RUNTIME_FEATURE_CHORD_REMAP       = 1u << 4,
  RUNTIME_FEATURE_TURBO             = 1u << 5,
  RUNTIME_FEATURE_CLASSIC_MERGE     = 1u << 6,
  RUNTIME_FEATURE_ANALOG_CALIBRATION = 1u << 7,
  RUNTIME_FEATURE_FINAL_DEADZONE    = 1u << 8,
  RUNTIME_FEATURE_OUTPUT_BUTTON_MAP = 1u << 9,
  RUNTIME_FEATURE_JAGUAR_ROTARY     = 1u << 10,
};

uint16_t runtimeFeatureMask = 0;
bool runtimeFeatureMaskInitialized = false;
DeviceEnum runtimeFeatureMaskMode = RZORD_AUTODETECT;

uint16_t __not_in_flash_func(refreshRuntimeFeatureMask)(bool updated) {
  if (runtimeFeatureMaskInitialized && !updated &&
      runtimeFeatureMaskMode == deviceMode) {
    return runtimeFeatureMask;
  }

  uint16_t mask = 0;
  if (hotkey_bootsel != (uint32_t)-1) {
    mask |= RUNTIME_FEATURE_BOOTLOADER_HOTKEY;
  }
  if (hotkey_dpad_as_buttons != (uint32_t)-1) {
    mask |= RUNTIME_FEATURE_DPAD_HOTKEY;
  }
  if (controller_runtime_internal::virtualControllerHotkeysRequireService()) {
    mask |= RUNTIME_FEATURE_VIRTUAL_HOTKEYS;
  }
  if (hasActiveRemap()) {
    mask |= RUNTIME_FEATURE_BUTTON_REMAP;
  }
  if (hasActiveButtonChordRemap()) {
    mask |= RUNTIME_FEATURE_CHORD_REMAP;
  }
  if (turbo.hasAnyEnabled()) {
    mask |= RUNTIME_FEATURE_TURBO;
  }
  if (classic_dual_merge_enabled != 0) {
    mask |= RUNTIME_FEATURE_CLASSIC_MERGE;
  }
  if (analogCalibration.enabled) {
    mask |= RUNTIME_FEATURE_ANALOG_CALIBRATION;
  }
  if (deadzone_percent != 0) {
    mask |= RUNTIME_FEATURE_FINAL_DEADZONE;
  }
#ifdef ENABLE_INPUT_N64
  if (deviceMode == RZORD_N64) {
    mask |= RUNTIME_FEATURE_OUTPUT_BUTTON_MAP;
  }
#endif
#ifdef ENABLE_INPUT_GAMECUBE
  if (deviceMode == RZORD_GAMECUBE) {
    mask |= RUNTIME_FEATURE_OUTPUT_BUTTON_MAP;
  }
#endif
#ifdef ENABLE_INPUT_JAGUAR
  if (deviceMode == RZORD_JAGUAR) {
    mask |= RUNTIME_FEATURE_JAGUAR_ROTARY;
  }
#endif
  runtimeFeatureMask = mask;
  runtimeFeatureMaskInitialized = true;
  runtimeFeatureMaskMode = deviceMode;
  return mask;
}

bool __not_in_flash_func(runtimeFeatureEnabled)(ControllerRuntimeFeature feature) {
  return (runtimeFeatureMask & feature) != 0;
}

}  // namespace

void __not_in_flash_func(processPolledInputFrame)(bool updated) {
  refreshRuntimeFeatureMask(updated);
  captureRawAnalogInputSnapshots(deviceMode);
  controller_runtime_internal::updateStickCenteringFromPoll(updated);
  controller_runtime_internal::snapshotRawInputButtonsBeforeTransforms();
  controller_runtime_internal::applyDeadzoneAndTriggerTransforms(updated);
  if (runtimeFeatureEnabled(RUNTIME_FEATURE_CLASSIC_MERGE)) {
    controller_runtime_internal::captureDualInputMergeSourceSlots();
  }
  controller_runtime_internal::snapshotPhysicalButtonsBeforeTransforms();
  if (runtimeFeatureEnabled(RUNTIME_FEATURE_BOOTLOADER_HOTKEY)) {
    controller_runtime_internal::handleHeldBootloaderHotkey();
  }
  if (runtimeFeatureEnabled(RUNTIME_FEATURE_DPAD_HOTKEY)) {
    controller_runtime_internal::handleHeldDpadModeHotkey();
  }
  if (runtimeFeatureEnabled(RUNTIME_FEATURE_VIRTUAL_HOTKEYS) &&
      !latencyServiceDisabled(LATENCY_SERVICE_MENU_HOTKEYS)) {
    controller_runtime_internal::updateHeldControllerHotkeysBeforeTransforms();
  }
#ifdef ENABLE_INPUT_PSX
  // Turbo and remap must see the same neGcon presses here as on output.
  if (deviceMode == RZORD_PSX) {
    controller_runtime_internal::applyNegconInputPressesToProcessedFrames();
  }
#endif
  // Keep the physical snapshot available to hotkeys, then filter only the
  // trigger sources that turbo/remap and the USB output will consume.
  const outputMode_t triggerOutput = get_effective_output_mode();
  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    controller_state_t& frame = controllerFrame(i);
    if (frame.connected) {
      applyAnalogOnlyTriggerButtons(frame, trigger_mode, triggerOutput);
    }
  }
#if !defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
  if (runtimeFeatureEnabled(RUNTIME_FEATURE_TURBO) &&
      !latencyServiceDisabled(LATENCY_SERVICE_TURBO_TRANSFORM)) {
    controller_runtime_internal::applyTurboTransforms();
  }
#endif
  if (runtimeFeatureEnabled(RUNTIME_FEATURE_BUTTON_REMAP) &&
      !latencyServiceDisabled(LATENCY_SERVICE_BUTTON_REMAP)) {
    controller_runtime_internal::applyButtonRemapTransforms();
  }
  if (runtimeFeatureEnabled(RUNTIME_FEATURE_CHORD_REMAP) &&
      !latencyServiceDisabled(LATENCY_SERVICE_CHORD_REMAP)) {
    controller_runtime_internal::applyButtonChordRemapTransforms();
  }
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
  if (runtimeFeatureEnabled(RUNTIME_FEATURE_TURBO) &&
      !latencyServiceDisabled(LATENCY_SERVICE_TURBO_TRANSFORM)) {
    controller_runtime_internal::applyTurboTransforms();
  }
#endif
  // A lone P2 merges only at output, so slot 0 stays empty between sends.
  if (runtimeFeatureEnabled(RUNTIME_FEATURE_CLASSIC_MERGE) &&
      controller_runtime_internal::dualMergePrimaryIsFirstSlot()) {
    controller_runtime_internal::applyClassicDualControllerMergeTransform();
  }
  if (runtimeFeatureEnabled(RUNTIME_FEATURE_VIRTUAL_HOTKEYS) &&
      !latencyServiceDisabled(LATENCY_SERVICE_VIRTUAL_HOTKEYS)) {
    controller_runtime_internal::applyVirtualControllerHotkeyOutputs();
  }

  if (runtimeFeatureEnabled(RUNTIME_FEATURE_JAGUAR_ROTARY)) {
    controller_runtime_internal::updateJaguarRotaryStateFromInputs();
  }
  if (!latencyServiceDisabled(LATENCY_SERVICE_SOCD)) {
    controller_runtime_internal::applySocdCleaningToInputs();
  }
  controller_runtime_internal::noteProcessedButtonsAfterTransforms();
}

void __not_in_flash_func(restoreControllerFramesBeforeInputPoll)() {
  controller_runtime_internal::restorePhysicalButtonsBeforeInputPoll();
}

bool __not_in_flash_func(processedControllerFrameNeedsFinalize)() {
#if defined(ADAPT_CAP_DIRECTIONAL_HOTKEYS)
  // Finalization is the only step that applies a Start shortcut's output.
  for (uint8_t i = 0; i < MAX_USB_OUT; ++i) {
    if (directionalHotkeySuppression(i) != 0) {
      return true;
    }
  }
#endif
  return false;
}

void cacheProcessedControllerOutputState() {
  controller_runtime_internal::cacheProcessedControllerOutputStateInternal();
}

void __not_in_flash_func(finalizeControllerFrameForOutput)() {
  // Output finalization may add host-only buttons or calibration. Restore the
  // captured frame after send, but keep digital buttons at the physical
  // pre-remap snapshot so remap/turbo never feeds back into itself.
  controller_runtime_internal::captureOutputFinalizeRestoreFrames();
  controller_runtime_internal::rebuildDigitalButtonsForOutputFrame(
      runtimeFeatureEnabled(RUNTIME_FEATURE_BUTTON_REMAP),
      runtimeFeatureEnabled(RUNTIME_FEATURE_CHORD_REMAP),
      runtimeFeatureEnabled(RUNTIME_FEATURE_TURBO),
      runtimeFeatureEnabled(RUNTIME_FEATURE_VIRTUAL_HOTKEYS));
  // Rebuilding restores raw D-pad bits too, so clean the frame the host gets
  // (and DB15's parallel frame, even when a held report has not changed).
#if !ADAPT_POLICY_SOCD_FIXED_OFF
  controller_runtime_internal::applySocdCleaningToInputs();
#endif
  if (runtimeFeatureEnabled(RUNTIME_FEATURE_CLASSIC_MERGE)) {
    controller_runtime_internal::applyClassicDualControllerMergeTransform();
  }
  if (runtimeFeatureEnabled(RUNTIME_FEATURE_VIRTUAL_HOTKEYS) &&
      !latencyServiceDisabled(LATENCY_SERVICE_VIRTUAL_HOTKEYS)) {
    controller_runtime_internal::applyVirtualControllerHotkeyOutputs();
    // Consume pressure alongside a shortcut's digital sources only on the
    // captured output frame. Restore preserves physical pressure between polls.
    for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
      controller_state_t& frame = controllerFrame(i);
      if (frame.connected) {
        setControllerPressureForMask(frame,
            controller_runtime_internal::suppressedControllerHotkeySourceButtons(i), 0);
      }
    }
  }
  if (runtimeFeatureEnabled(RUNTIME_FEATURE_ANALOG_CALIBRATION)) {
    controller_runtime_internal::applyAnalogCalibrationToConnectedInputs();
  }
  if (runtimeFeatureEnabled(RUNTIME_FEATURE_FINAL_DEADZONE)) {
    controller_runtime_internal::applyFinalStickDeadzoneToConnectedInputs();
  }
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  controller_runtime_internal::applyRotaryTuningToOutputFrames();
#endif
  if (runtimeFeatureEnabled(RUNTIME_FEATURE_OUTPUT_BUTTON_MAP)) {
    controller_runtime_internal::applyOutputButtonModeMappingsToConnectedInputs();
  }
  if (runtimeFeatureEnabled(RUNTIME_FEATURE_JAGUAR_ROTARY)) {
    controller_runtime_internal::clearJaguarRotaryDpadBeforeUsbSend();
  }
  controller_runtime_internal::consumeMenuCapturedControllerOutput();
#if defined(ADAPT_CAP_DIRECTIONAL_HOTKEYS)
  applyDirectionalHotkeysToOutput(!menuOwnsControllerInput() && runtimeFeatureEnabled(RUNTIME_FEATURE_VIRTUAL_HOTKEYS) &&
      !latencyServiceDisabled(LATENCY_SERVICE_VIRTUAL_HOTKEYS));
#endif
}

void __not_in_flash_func(restoreControllerFrameAfterOutputSend)() {
  controller_runtime_internal::restorePhysicalButtonsAfterUsbSend();
}
