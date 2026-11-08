#include "../product_config.h"

#include "settings_persistence_deferred.h"

#if defined(ADAPT_CAP_DEFERRED_SETTINGS_PERSISTENCE) || defined(REFLEX_HOST_TEST_DEFERRED_SETTINGS)

#if defined(ADAPT_CAP_DEFERRED_SETTINGS_PERSISTENCE) && defined(ADAPT_OUTPUT_USB_DEVICE)
#include <Adafruit_TinyUSB.h>
#endif
#include <Arduino.h>
#include <string.h>

#include "eeprom_helper.h"
#include "settings_layout.h"
#include "../output/output_runtime_state.h"

namespace {

struct DeferredSettingsState {
  GlobalSettingsRecord global{};
  PerModeSettingsRecord per_mode[MAX_INPUT_MODES]{};
  bool global_pending = false;
  bool per_mode_pending[MAX_INPUT_MODES]{};
  uint8_t immediate_depth = 0;
  SettingsPersistenceSafeWindow safe_window{};
};

DeferredSettingsState deferredSettings{};

#if defined(REFLEX_HOST_TEST_DEFERRED_SETTINGS)
bool hostProtectedSessionActive = false;
#endif

bool rawProtectedConsoleSessionActive() {
#if defined(REFLEX_HOST_TEST_DEFERRED_SETTINGS)
  return hostProtectedSessionActive;
#elif defined(ADAPT_OUTPUT_USB_DEVICE)
  const outputMode_t mode = canonicalizeOutputMode(get_effective_output_mode());
  return (mode == OUTPUT_PS4 || mode == OUTPUT_PS5) &&
         tud_mounted() && !tud_suspended();
#else
  return false;
#endif
}

bool perModeIndexValid(DeviceEnum mode) {
  return isConcreteModeForPerSettings(mode) &&
         static_cast<uint8_t>(mode) < MAX_INPUT_MODES;
}

void clearPendingRecords() {
  deferredSettings.global_pending = false;
  memset(deferredSettings.per_mode_pending, 0,
         sizeof(deferredSettings.per_mode_pending));
  deferredSettings.safe_window.reset();
}

}  // namespace

#if defined(REFLEX_HOST_TEST_DEFERRED_SETTINGS)
void settingsPersistenceHostSetProtected(bool protectedSessionActive) {
  hostProtectedSessionActive = protectedSessionActive;
}
#endif

bool settingsPersistenceHasPending() {
  if (eeprom_commit_retry_pending()) {
    return true;
  }
  if (deferredSettings.global_pending) {
    return true;
  }
  for (uint8_t mode = 0; mode < MAX_INPUT_MODES; ++mode) {
    if (deferredSettings.per_mode_pending[mode]) {
      return true;
    }
  }
  return false;
}

bool settingsPersistenceProtectedSessionActive() {
  return deferredSettings.immediate_depth == 0 &&
         rawProtectedConsoleSessionActive();
}

bool settingsPersistenceShouldStageWrites() {
  return deferredSettings.immediate_depth == 0 &&
         (rawProtectedConsoleSessionActive() || settingsPersistenceHasPending());
}

bool settingsPersistenceReadPendingGlobal(GlobalSettingsRecord& settings) {
  if (!deferredSettings.global_pending) {
    return false;
  }
  settings = deferredSettings.global;
  return true;
}

bool settingsPersistenceReadPendingPerMode(
    DeviceEnum mode, PerModeSettingsRecord& settings) {
  if (!perModeIndexValid(mode) ||
      !deferredSettings.per_mode_pending[static_cast<uint8_t>(mode)]) {
    return false;
  }
  settings = deferredSettings.per_mode[static_cast<uint8_t>(mode)];
  return true;
}

void settingsPersistenceStageGlobal(const GlobalSettingsRecord& settings) {
  deferredSettings.global = settings;
  deferredSettings.global_pending = true;
  deferredSettings.safe_window.reset();
}

void settingsPersistenceStagePerMode(
    DeviceEnum mode, const PerModeSettingsRecord& settings) {
  if (!perModeIndexValid(mode)) {
    return;
  }
  const uint8_t index = static_cast<uint8_t>(mode);
  deferredSettings.per_mode[index] = settings;
  deferredSettings.per_mode_pending[index] = true;
  deferredSettings.safe_window.reset();
}

bool settingsPersistenceFlushNow() {
  if (!settingsPersistenceHasPending()) {
    deferredSettings.safe_window.reset();
    return true;
  }

  // Stage every latest value into its inactive A/B slot, then publish the
  // entire EEPROM image once. This avoids one flash erase/program cycle per
  // changed setting and retains the existing record generation/CRC scheme.
  EepromTransaction transaction;
  if (deferredSettings.global_pending) {
    stagePersistedRecordAB(
      transaction,
      SETTINGS_GLOBAL_RECORD_BASE,
      GLOBAL_SETTINGS_RECORD_SIZE,
      PERSISTED_BLOCK_MAGIC_GLOBAL,
      deferredSettings.global
    );
  }
  for (uint8_t mode = 0; mode < MAX_INPUT_MODES; ++mode) {
    if (!deferredSettings.per_mode_pending[mode]) {
      continue;
    }
    stagePersistedRecordAB(
      transaction,
      getPerModeSettingsRecordBase(static_cast<DeviceEnum>(mode)),
      PER_MODE_SETTINGS_RECORD_SIZE,
      PERSISTED_BLOCK_MAGIC_PER_MODE,
      deferredSettings.per_mode[mode]
    );
  }

  if (!transaction.commit()) {
    // Keep the latest RAM overlay for readback and a later bounded retry.
    deferredSettings.safe_window.reset();
    return false;
  }

  clearPendingRecords();
  return true;
}

void settingsPersistenceService() {
  const uint32_t nowMs = millis();
  if (!deferredSettings.safe_window.shouldFlush(
        settingsPersistenceHasPending(),
        settingsPersistenceProtectedSessionActive(),
        nowMs)) {
    return;
  }
  (void)settingsPersistenceFlushNow();
}

void settingsPersistenceDiscardDeferred() {
  clearPendingRecords();
}

void settingsPersistenceBeginImmediate() {
  if (deferredSettings.immediate_depth != 0xFFu) {
    ++deferredSettings.immediate_depth;
  }
}

void settingsPersistenceEndImmediate() {
  if (deferredSettings.immediate_depth != 0) {
    --deferredSettings.immediate_depth;
  }
}

#else

bool settingsPersistenceProtectedSessionActive() { return false; }
bool settingsPersistenceShouldStageWrites() { return false; }
bool settingsPersistenceReadPendingGlobal(GlobalSettingsRecord&) { return false; }
bool settingsPersistenceReadPendingPerMode(DeviceEnum, PerModeSettingsRecord&) { return false; }
void settingsPersistenceStageGlobal(const GlobalSettingsRecord&) {}
void settingsPersistenceStagePerMode(DeviceEnum, const PerModeSettingsRecord&) {}
void settingsPersistenceService() {}
bool settingsPersistenceFlushNow() { return true; }
void settingsPersistenceDiscardDeferred() {}
void settingsPersistenceBeginImmediate() {}
void settingsPersistenceEndImmediate() {}
bool settingsPersistenceHasPending() { return false; }

#endif
