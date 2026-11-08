#include <EEPROM.h>

#include "device_runtime_state.h"
#include "classic_dual_merge_config.h"
#include "remap_pairs.h"
#include "settings_store.h"
#include "settings_persistence_deferred.h"
#include "eeprom_helper.h"
#include "settings_store_per_mode_internal.h"
#include "turbo.h"

void refreshRemapsForOutputChange() {
  // Remaps belong to the input+output pair. Auto resolving in place or a live
  // output apply changes the pair without reloading the input's settings.
  if (!activeRemapPairIsStale(deviceMode)) return;
  if (loadRemapsForMode(deviceMode) && !settingsPersistenceShouldStageWrites()) {
    persistentEepromCommit();
  }
}

void loadPerModeSettings() {
#if ADAPT_POLICY_PS5_DIAGNOSTIC_RUNTIME && !defined(PS5_DIAG_USE_PERSISTED_SETTINGS)
  uint8_t turbo_rates[TURBO_BTN_COUNT] = {};
  sanitizeTurboRatesForMode(deviceMode, turbo_rates);
  turbo.setAllRates(turbo_rates);
  turbo.setInputMode(getTurboInputModeForDeviceMode(deviceMode));
  return;
#else
  bool commit_needed = false;
  selectPerModeRemapSlot(deviceMode);
  commit_needed |= loadAndApplyPerModeSettingsForMode(deviceMode);

  uint8_t turbo_rates[TURBO_BTN_COUNT];
  commit_needed |= loadTurboRatesForMode(deviceMode, turbo_rates);
  turbo.setAllRates(turbo_rates);

  turbo.setInputMode(getTurboInputModeForDeviceMode(deviceMode));
  commit_needed |= loadRemapsForMode(deviceMode);

#if defined(ADAPT_CAP_DUAL_INPUT_MERGE)
  loadClassicDualMergeConfigForMode(deviceMode);
#endif

  if (commit_needed) {
    // Schema-backed writes commit immediately unless the product has staged
    // them for publication in a later safe window.
    if (!settingsPersistenceShouldStageWrites()) {
      persistentEepromCommit();
    }
  }
#endif
}
