#include "../product_config.h"

#include "configuration_persistence.h"

#include <Arduino.h>

#include "classic_dual_merge_config.h"
#include "deferred_mode_saves.h"
#include "remap_pair_store.h"
#include "remap_pairs.h"
#include "settings_persistence_deferred.h"

#if ADAPT_POLICY_RESERVED_FEEDBACK_STORAGE
#include "hid_mapping_profiles.h"
#include "modern2usb_analog_profiles.h"
#endif
#if defined(ADAPT_CAP_CONTROLLER_HOTKEY_PROFILES)
#include "modern2usb_hotkey_profiles.h"
#endif
#if defined(ADAPT_CAP_DEFERRED_SETTINGS_PERSISTENCE) || defined(REFLEX_HOST_TEST_DEFERRED_SETTINGS)
#include "button_chord_remap.h"
#include "hotkey_combo.h"
#endif
#if defined(ENABLE_INPUT_JVS)
#include "../input/jvs/jvs_analog_config.h"
#include "../input/jvs/jvs_panel_map.h"
#endif

void configurationPersistenceService() {
  settingsPersistenceService();
  deferredModeSavesService();
#if ADAPT_REMAPS_PER_OUTPUT
  remapPairPersistenceService();
#endif
#if ADAPT_POLICY_RESERVED_FEEDBACK_STORAGE
  hidMappingProfilePersistenceService();
  modern2usbAnalogProfilesPersistenceService();
#endif
#if defined(ADAPT_CAP_CONTROLLER_HOTKEY_PROFILES)
  modern2usbHotkeyProfilesPersistenceService();
#endif
#if defined(ADAPT_CAP_DEFERRED_SETTINGS_PERSISTENCE) || defined(REFLEX_HOST_TEST_DEFERRED_SETTINGS)
  hotkeyPersistenceService();
  buttonChordPersistenceService();
#endif
#if defined(ADAPT_CAP_DUAL_INPUT_MERGE)
  classicDualMergePersistenceService();
#endif
#if defined(ENABLE_INPUT_JVS)
  jvsAnalogPersistenceService();
  jvsPanelPersistenceService();
#endif
}

bool configurationPersistenceFlushNow() {
  bool success = settingsPersistenceFlushNow();
  if (!deferredModeSavesFlushNow()) success = false;
#if ADAPT_REMAPS_PER_OUTPUT
  if (!remapPairPersistenceFlushNow()) success = false;
#endif
#if ADAPT_POLICY_RESERVED_FEEDBACK_STORAGE
  if (!hidMappingProfilePersistenceFlushNow()) success = false;
  if (!modern2usbAnalogProfilesPersistenceFlushNow()) success = false;
#endif
#if defined(ADAPT_CAP_CONTROLLER_HOTKEY_PROFILES)
  if (!modern2usbHotkeyProfilesPersistenceFlushNow()) success = false;
#endif
#if defined(ADAPT_CAP_DEFERRED_SETTINGS_PERSISTENCE) || defined(REFLEX_HOST_TEST_DEFERRED_SETTINGS)
  if (!hotkeyPersistenceFlushNow()) success = false;
  if (!buttonChordPersistenceFlushNow()) success = false;
#endif
#if defined(ADAPT_CAP_DUAL_INPUT_MERGE)
  if (!classicDualMergePersistenceFlushNow()) success = false;
#endif
#if defined(ENABLE_INPUT_JVS)
  if (!jvsAnalogPersistenceFlushNow()) success = false;
  if (!jvsPanelPersistenceFlushNow()) success = false;
#endif
  return success;
}

bool configurationPersistenceFlushBounded(uint8_t attempts,
                                          uint16_t retryDelayMs) {
  if (attempts == 0) {
    return !configurationPersistenceHasPending();
  }
  for (uint8_t attempt = 0; attempt < attempts; ++attempt) {
    if (configurationPersistenceFlushNow()) {
      return true;
    }
    if (retryDelayMs != 0 && attempt + 1u < attempts) {
      delay(retryDelayMs);
    }
  }
  return false;
}

bool configurationPersistencePrepareForReset() {
  constexpr uint8_t kPersistenceFlushAttempts = 3;
  constexpr uint16_t kPersistenceRetryDelayMs = 2;
  return configurationPersistenceFlushBounded(
      kPersistenceFlushAttempts, kPersistenceRetryDelayMs);
}

void configurationPersistenceDiscardDeferred() {
  settingsPersistenceDiscardDeferred();
  deferredModeSavesDiscard();
#if ADAPT_REMAPS_PER_OUTPUT
  remapPairPersistenceDiscardDeferred();
#endif
#if ADAPT_POLICY_RESERVED_FEEDBACK_STORAGE
  hidMappingProfilePersistenceDiscardDeferred();
  modern2usbAnalogProfilesPersistenceDiscardDeferred();
#endif
#if defined(ADAPT_CAP_CONTROLLER_HOTKEY_PROFILES)
  modern2usbHotkeyProfilesPersistenceDiscardDeferred();
#endif
#if defined(ADAPT_CAP_DEFERRED_SETTINGS_PERSISTENCE) || defined(REFLEX_HOST_TEST_DEFERRED_SETTINGS)
  hotkeyPersistenceDiscardDeferred();
  buttonChordPersistenceDiscardDeferred();
#endif
#if defined(ADAPT_CAP_DUAL_INPUT_MERGE)
  classicDualMergePersistenceDiscardDeferred();
#endif
#if defined(ENABLE_INPUT_JVS)
  jvsAnalogPersistenceDiscardDeferred();
  jvsPanelPersistenceDiscardDeferred();
#endif
}

void configurationPersistenceBeginImmediate() {
  settingsPersistenceBeginImmediate();
}

void configurationPersistenceEndImmediate() {
  settingsPersistenceEndImmediate();
}

bool configurationPersistenceHasPending() {
  bool pending = settingsPersistenceHasPending();
  pending = pending || deferredModeSavesHasPending();
#if ADAPT_REMAPS_PER_OUTPUT
  pending = pending || remapPairPersistenceHasPending();
#endif
#if ADAPT_POLICY_RESERVED_FEEDBACK_STORAGE
  pending = pending || hidMappingProfilePersistenceHasPending();
  pending = pending || modern2usbAnalogProfilesPersistenceHasPending();
#endif
#if defined(ADAPT_CAP_CONTROLLER_HOTKEY_PROFILES)
  pending = pending || modern2usbHotkeyProfilesPersistenceHasPending();
#endif
#if defined(ADAPT_CAP_DEFERRED_SETTINGS_PERSISTENCE) || defined(REFLEX_HOST_TEST_DEFERRED_SETTINGS)
  pending = pending || hotkeyPersistenceHasPending();
  pending = pending || buttonChordPersistenceHasPending();
#endif
#if defined(ADAPT_CAP_DUAL_INPUT_MERGE)
  pending = pending || classicDualMergePersistenceHasPending();
#endif
#if defined(ENABLE_INPUT_JVS)
  pending = pending || jvsAnalogPersistenceHasPending();
  pending = pending || jvsPanelPersistenceHasPending();
#endif
  return pending;
}
