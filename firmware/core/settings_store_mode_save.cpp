#include "analog_calibration_state.h"
#include "button_remap.h"
#include "controller_settings_state.h"
#include "deferred_mode_saves.h"
#include "controller_frame_state.h"
#include "device_runtime_state.h"
#include "firmware_support.h"
#include "settings_store_internal.h"
#include "../input/autodetect/input_autodetect_runtime.h"
#include "../input/autodetect/input_autodetect_runtime_state.h"
#include "../input/runtime/input_boot_runtime.h"
#include "../input/runtime/input_frame_runtime.h"
#include "../output/output_capabilities.h"
#include "../menu/menu_mode_state.h"
#include "../menu/menu_runtime_state.h"
#include "../menu/menu_working_state.h"
#include "../output/runtime/input_usb_identity_runtime.h"
#include "../output/runtime/output_boot_runtime.h"
#include "../output/output_runtime_state.h"
#include "../platform/webhid_runtime.h"
#include "settings_store.h"
#include "turbo.h"

#include <string.h>

namespace {

bool isPersistedAutoDetectMode(DeviceEnum mode) {
#ifdef ENABLE_INPUT_AUTODETECT
  return mode == RZORD_AUTODETECT;
#else
  (void)mode;
  return false;
#endif
}

bool isManualInputMode(DeviceEnum mode) {
  return mode > RZORD_NONE &&
         mode < RZORD_LAST &&
         !isPersistedAutoDetectMode(mode);
}

DeviceEnum getPersistedInputModeForSave(DeviceEnum selectedMode) {
  if (selectedMode == deviceMode && isPersistedAutoDetectMode(savedDeviceMode)) {
    return savedDeviceMode;
  }

  if (inputAutoDetectModeActive() &&
      selectedMode == deviceMode &&
      savedDeviceMode > RZORD_NONE &&
      savedDeviceMode < RZORD_LAST) {
    return savedDeviceMode;
  }
  return selectedMode;
}

struct QuickConfigSaveSelection {
  DeviceEnum newInputMode;
  DeviceEnum persistedInputMode;
  outputMode_t newOutputMode;
  bool needsReboot;
  PerModeSettingsRecord selectedSettings;
};

void captureRuntimeTurboRates(uint8_t* rates) {
  for (uint8_t i = 0; i < TURBO_BTN_COUNT; ++i) {
    rates[i] = (uint8_t)turbo.getButtonRate((TurboButton)i);
  }
}

GlobalSettingsRecord loadCurrentGlobalSettings() {
  GlobalSettingsRecord settings{};
  readGlobalSettings(settings);
  return settings;
}

QuickConfigSaveSelection collectModeSaveSelection(
    DeviceEnum newInputMode,
    outputMode_t newOutputMode,
    const uint8_t* tempRates,
    const PerModeSettingsRecord& selectedSettings
) {
  QuickConfigSaveSelection selection{};
  selection.newInputMode = newInputMode;
  selection.persistedInputMode = getPersistedInputModeForSave(newInputMode);
  // Both menu selections are staged together. An unchanged output is already
  // the current configured choice; an input change must not discard a new one.
  selection.newOutputMode = sanitizeConfiguredOutputMode(newOutputMode);
  selection.needsReboot =
    (selection.newInputMode != deviceMode) ||
    (selection.newOutputMode != configuredOutputMode);
#if defined(ADAPT_OUTPUT_USB_DEVICE)
  selection.needsReboot = selection.needsReboot || (menu_usb_descriptor_reboot_required != 0);
#endif

  selection.selectedSettings = selectedSettings;
  for (uint8_t i = 0; i < TURBO_BTN_COUNT; ++i) {
    selection.selectedSettings.turbo_rates[i] =
      (tempRates[i] < TURBO_RATE_LAST) ? tempRates[i] : (uint8_t)TURBO_OFF;
  }
  sanitizePerModeSettings(selection.newInputMode, selection.selectedSettings);
  return selection;
}

void persistSelection(const QuickConfigSaveSelection& selection) {
#if ADAPT_POLICY_PS5_DIAGNOSTIC_RUNTIME && !defined(PS5_DIAG_USE_PERSISTED_SETTINGS)
  (void)selection;
  return;
#else
  GlobalSettingsRecord globalSettings = loadCurrentGlobalSettings();
  globalSettings.persisted_input_mode =
      persistedInputModeId(selection.persistedInputMode);
  globalSettings.configured_output_mode = (uint8_t)selection.newOutputMode;
  sanitizeGlobalSettings(globalSettings);
  writeGlobalSettings(globalSettings);

  if (isConcreteModeForPerSettings(selection.newInputMode)) {
    // Remaps are saved per input+output pair by the caller; the record keeps
    // whatever legacy copy it already had.
    PerModeSettingsRecord record = selection.selectedSettings;
    PerModeSettingsRecord stored{};
    readPerModeSettings(selection.newInputMode, stored);
    for (uint8_t i = 0; i < PER_MODE_REMAP_SIZE; ++i) {
      record.remaps[i] = stored.remaps[i];
    }
    writePerModeSettings(selection.newInputMode, record);
  }
#endif
}

void applySavedModeRuntimeState(
    const QuickConfigSaveSelection& selection,
    bool preserveAutoOutput
) {
  const bool preserveLiveOutputInSession =
    !selection.needsReboot &&
    selection.newOutputMode == configuredOutputMode;
  const outputMode_t resolvedOutputMode = outputMode;
  const AutoDetectState resolvedAutoDetectState = autoDetectState;
  const AutoDetectProbeStage resolvedProbeStage = autoDetectProbeStage;

  savedDeviceMode = selection.persistedInputMode;
  setInputAutoDetectModeActive(isPersistedAutoDetectMode(savedDeviceMode));
  configuredOutputMode = selection.newOutputMode;
  if (preserveLiveOutputInSession) {
    outputMode = resolvedOutputMode;
    autoDetectState = resolvedAutoDetectState;
    autoDetectProbeStage = resolvedProbeStage;
  } else {
    outputMode = sanitizeRuntimeOutputMode(configuredOutputMode);
    autoDetectState = AUTO_STATE_IDLE;
  }
  if (isManualInputMode(selection.persistedInputMode) &&
      preserveAutoOutput &&
      !preserveLiveOutputInSession) {
    auto_detect_clear_input_scratch_state();
  }
  if (!preserveAutoOutput && !preserveLiveOutputInSession) {
    auto_detect_clear_scratch_state();
  }
#ifdef ENABLE_INPUT_AUTODETECT
  // Descriptor-only changes (Pro <-> NSO) must not discard the controller
  // and player count which selected that descriptor. This is a one-boot
  // hint, not a change from the user's saved Auto input preference.
  if (selection.needsReboot && selection.newInputMode == deviceMode &&
      isPersistedAutoDetectMode(selection.persistedInputMode)) {
    preserveDetectedInputModeForReboot(deviceMode);
  }
#endif
}

bool shouldPreserveAutoOutputForInputReboot(const QuickConfigSaveSelection& selection) {
  return selection.needsReboot &&
         selection.newOutputMode == configuredOutputMode &&
         auto_detect_preserve_known_runtime_mode_for_input_reboot();
}

#ifndef REFLEX_HOST_TEST_EEPROM_AUTH
bool isManualInputOnlyModeChange(const QuickConfigSaveSelection& selection) {
  if (selection.newInputMode == deviceMode ||
      selection.newOutputMode != configuredOutputMode ||
      !isManualInputMode(selection.persistedInputMode)) {
    return false;
  }
#if defined(ADAPT_OUTPUT_USB_DEVICE)
  if (output_is_generic_mister_hid_mode(get_effective_output_mode()) &&
      inputModeNeedsBcdDeviceReenumeration(selection.newInputMode)) {
    // MiSTer GameControllerDB keys off bcdDevice, so a live input-family
    // switch would keep the previous mapping until USB re-enumerates.
    return false;
  }
#endif
#if defined(ADAPT_OUTPUT_USB_DEVICE)
  if (menu_usb_descriptor_reboot_required != 0) {
    return false;
  }
#endif
  return true;
}

uint8_t expectedUsbSlotCountForCurrentRuntime() {
  uint8_t expected = bootUsbSlotCountForModeAndInput(
    get_effective_output_mode(),
    deviceMode,
    inputPortCount(),
    connectedInputFrameCount(inputPortCount()));
  if (expected == 0) {
    expected = 1;
  }
  return expected;
}

bool applyManualInputModeLive(QuickConfigSaveSelection& selection) {
  if (!isManualInputOnlyModeChange(selection)) {
    return false;
  }

  const outputMode_t resolvedOutputMode = outputMode;
  const AutoDetectState resolvedAutoDetectState = autoDetectState;
  const AutoDetectProbeStage resolvedProbeStage = autoDetectProbeStage;

  savedDeviceMode = selection.persistedInputMode;
  deviceMode = selection.newInputMode;
  setInputAutoDetectModeActive(false);
  clearAutoDetectResult();
  clearAutoInputResolvedGrace();
  clearInputHotSwapNextDetectAllowedAt();
  configuredOutputMode = selection.newOutputMode;
  outputMode = resolvedOutputMode;
  autoDetectState = resolvedAutoDetectState;
  autoDetectProbeStage = resolvedProbeStage;
  auto_detect_clear_input_scratch_state();

  const switchpro_mode_enum switchProfileBefore = SwitchCommon::switchpro_mode;
  initializeInputModuleForRuntimeModeChange();
  latch_nso_special_connected_players(
    deviceMode, connectedInputFrameCount(inputPortCount()));
  output_apply_automatic_switch_profile_for_input_mode(deviceMode);
  webhid_update_device_mode(deviceMode);

  // The NSO persona selects the USB PID and report layout. A slot count
  // that happens to match cannot excuse the host from re-enumerating when
  // the persona moved, so mirror the hot-swap descriptor check here.
  if (output_runtime_is_switchpro_mode() &&
      switchProfileBefore != SwitchCommon::switchpro_mode) {
    selection.needsReboot = true;
    return false;
  }
  return expectedUsbSlotCountForCurrentRuntime() == max_devices;
}
#else
bool applyManualInputModeLive(QuickConfigSaveSelection& selection) {
  (void)selection;
  return false;
}
#endif

void armPs5DiagBootIfNeeded(const QuickConfigSaveSelection& selection) {
#if ADAPT_POLICY_PS5_DIAGNOSTIC_RUNTIME
  if (selection.needsReboot && selection.newOutputMode == OUTPUT_PS5) {
    ps5_diag_arm_next_boot();
  }
#else
  (void)selection;
#endif
}

}  // namespace

void persistConfiguredInputMode(DeviceEnum mode) {
#if ADAPT_POLICY_PS5_DIAGNOSTIC_RUNTIME && !defined(PS5_DIAG_USE_PERSISTED_SETTINGS)
  (void)mode;
  return;
#else
  GlobalSettingsRecord globalSettings = loadCurrentGlobalSettings();
  globalSettings.persisted_input_mode = persistedInputModeId(mode);
  globalSettings.configured_output_mode =
      (uint8_t)sanitizeConfiguredOutputMode(configuredOutputMode);
  sanitizeGlobalSettings(globalSettings);
  writeGlobalSettings(globalSettings);
#endif
}

void saveModeSettings(DeviceEnum newInputMode, outputMode_t newOutputMode, const uint8_t* tempRates,
                      const PerModeSettingsRecord& selectedSettings) {
  deferredModeSavesFlushNow();
  QuickConfigSaveSelection selection = collectModeSaveSelection(
    newInputMode,
    newOutputMode,
    tempRates,
    selectedSettings
  );

  persistSelection(selection);
  if (applyManualInputModeLive(selection)) {
    return;
  }

  const bool preserveAutoOutput =
    shouldPreserveAutoOutputForInputReboot(selection);
  applySavedModeRuntimeState(selection, preserveAutoOutput);
  armPs5DiagBootIfNeeded(selection);

  if (selection.needsReboot) {
    delay(100);
    reboot();
  }
}

bool saveMenuModeSettings() {
  deferredModeSavesFlushNow();
  uint8_t turboRates[TURBO_BTN_COUNT] = {};
  PerModeSettingsRecord selectedSettings{};

  if (menu_input == deviceMode) {
    captureRuntimeTurboRates(turboRates);
  } else {
    loadStoredTurboRatesForMode(menu_input, turboRates);
  }
  captureMenuPerModeSettings(menu_input, selectedSettings);

  QuickConfigSaveSelection selection = collectModeSaveSelection(
    menu_input,
    menu_output,
    turboRates,
    selectedSettings
  );
  const bool preserveAutoOutput =
    shouldPreserveAutoOutputForInputReboot(selection);
  persistSelection(selection);
  applySavedModeRuntimeState(selection, preserveAutoOutput);
  armPs5DiagBootIfNeeded(selection);
  return selection.needsReboot;
}
