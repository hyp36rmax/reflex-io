#include <string.h>

#include "button_remap.h"
#include "button_chord_remap.h"
#include "classic_dual_merge_config.h"
#include "configuration_persistence.h"
#include "device_runtime_state.h"
#include "eeprom_helper.h"
#include "firmware_build_stamp.h"
#include "hid_mapping_profiles.h"
#include "hotkey_combo.h"
#include "negcon_input_modes.h"
#include "psx_poll_timing.h"
#include "remap_pair_store.h"
#include "remap_pairs.h"
#include "settings_layout.h"
#include "settings_registry.h"
#include "settings_persistence_deferred.h"
#include "settings_store.h"
#include "boot/boot_storage_product.h"
#include "../output/output_runtime_state.h"

#ifdef ENABLE_BLUETOOTH_PAIRING
#include "bluetooth_bond_storage.h"
#endif
#ifdef ENABLE_INPUT_ESP32_SPI
#include "../input/esp32_spi/Input_Esp32Spi.h"
#endif
#if ADAPT_POLICY_RESERVED_FEEDBACK_STORAGE
#include "modern2usb_analog_profiles.h"
#endif
#if defined(ADAPT_CAP_CONTROLLER_HOTKEY_PROFILES)
#include "modern2usb_hotkey_profiles.h"
#endif
#ifdef ENABLE_INPUT_JVS
#include "../input/jvs/jvs_analog_config.h"
#include "../input/jvs/jvs_panel_map.h"
#endif

namespace {

bool configuredPs2RemoteOutput = false;

GlobalSettingsRecord buildDefaultGlobalSettings() {
  GlobalSettingsRecord settings = {};
  for (uint8_t i = 0; i < static_cast<uint8_t>(SettingId::Count); ++i) {
    const SettingId id = (SettingId)i;
    if (!settingIsGlobal(id)) {
      continue;
    }
    writeRecordSettingValue(settings, id, defaultSettingValue(id, RZORD_NONE));
  }
  sanitizeGlobalSettings(settings);
  if (firstBootSetupSupportedOnProduct()) {
    settings.menu_ok_back = markFirstBootSetupPending(settings.menu_ok_back);
  }
  return settings;
}

void writeValidatedGlobalSettings(const GlobalSettingsRecord& settings) {
  if (settingsPersistenceShouldStageWrites()) {
    settingsPersistenceStageGlobal(settings);
    return;
  }
  writePersistedRecordAB(
    SETTINGS_GLOBAL_RECORD_BASE,
    GLOBAL_SETTINGS_RECORD_SIZE,
    PERSISTED_BLOCK_MAGIC_GLOBAL,
    settings
  );
}

DeviceEnum effectiveClassicDualMergeSettingsMode(DeviceEnum mode) {
  if (isConcreteModeForPerSettings(mode)) {
    return mode;
  }
  if (isConcreteModeForPerSettings(deviceMode)) {
    return deviceMode;
  }
  for (uint8_t rawMode = 0; rawMode < (uint8_t)RZORD_LAST; ++rawMode) {
    const DeviceEnum candidate = (DeviceEnum)rawMode;
    if (isConcreteModeForPerSettings(candidate)) {
      return candidate;
    }
  }
  return RZORD_NONE;
}

}  // namespace

void sanitizeGlobalSettings(GlobalSettingsRecord& settings) {
  for (uint8_t i = 0; i < static_cast<uint8_t>(SettingId::Count); ++i) {
    const SettingId id = (SettingId)i;
    if (!settingIsGlobal(id)) {
      continue;
    }
    const int32_t sanitized = sanitizeSettingValue(id, RZORD_NONE, readRecordSettingValue(settings, id));
    writeRecordSettingValue(settings, id, sanitized);
  }
}

bool readGlobalSettings(GlobalSettingsRecord& settings) {
  if (settingsPersistenceReadPendingGlobal(settings)) {
    sanitizeGlobalSettings(settings);
    configuredPs2RemoteOutput = ps2RemoteOutputEnabledFromRaw(settings.psx_periph);
    return true;
  }
  uint8_t slot = 0;
  uint16_t generation = 0;
  if (!readLatestPersistedRecordAB(
        SETTINGS_GLOBAL_RECORD_BASE,
        GLOBAL_SETTINGS_RECORD_SIZE,
        PERSISTED_BLOCK_MAGIC_GLOBAL,
        slot,
        generation,
        settings)) {
    settings = buildDefaultGlobalSettings();
    configuredPs2RemoteOutput = false;
    return false;
  }

  sanitizeGlobalSettings(settings);
  configuredPs2RemoteOutput = ps2RemoteOutputEnabledFromRaw(settings.psx_periph);
  return true;
}

void writeGlobalSettings(const GlobalSettingsRecord& settings) {
  GlobalSettingsRecord sanitized = settings;
  sanitizeGlobalSettings(sanitized);
  configuredPs2RemoteOutput = ps2RemoteOutputEnabledFromRaw(sanitized.psx_periph);
  writeValidatedGlobalSettings(sanitized);
}

bool firstBootSetupPending() {
  if (!firstBootSetupSupportedOnProduct()) {
    return false;
  }
  GlobalSettingsRecord settings{};
  (void)readGlobalSettings(settings);
  return firstBootSetupPendingFromRaw(settings.menu_ok_back);
}

bool saveFirstBootSetupSettings(const GlobalSettingsRecord& staged) {
  if (!firstBootSetupSupportedOnProduct()) {
    return false;
  }
  MenuModeVisibilityRecord visibility{};
  (void)readMenuModeVisibility(visibility);
  return saveFirstBootSetupSettings(staged, visibility);
}

int32_t loadSettingValue(SettingId id, DeviceEnum mode) {
  if (id == SettingId::ClassicDualMerge) {
    if (!classicDualMergeSupportedOnProduct()) {
      return 0;
    }
    const DeviceEnum effectiveMode = effectiveClassicDualMergeSettingsMode(mode);
    return classicDualMergeEnabledForMode(effectiveMode) ? 1 : 0;
  }

  if (settingIsGlobal(id)) {
    GlobalSettingsRecord globalSettings{};
    readGlobalSettings(globalSettings);
    return readRecordSettingValue(globalSettings, id);
  }

  PerModeSettingsRecord perModeSettings{};
  readPerModeSettings(mode, perModeSettings);
  return readRecordSettingValue(perModeSettings, id);
}

bool get_configured_ps2_remote_output() {
  // The runtime checks this on every input/output pass. Global-record reads
  // and writes refresh this cache, so this must not perform an EEPROM read.
  return configuredPs2RemoteOutput;
}

void saveSettingValue(SettingId id, int32_t value, DeviceEnum mode) {
  if (id == SettingId::ClassicDualMerge) {
    if (!classicDualMergeSupportedOnProduct()) {
      return;
    }
    const DeviceEnum effectiveMode = effectiveClassicDualMergeSettingsMode(mode);
    const int32_t sanitized = sanitizeSettingValue(id, effectiveMode, value);
    saveClassicDualMergeEnabledForMode(effectiveMode, sanitized != 0);
    return;
  }

  if (settingIsGlobal(id)) {
    GlobalSettingsRecord globalSettings{};
    readGlobalSettings(globalSettings);
    writeRecordSettingValue(
      globalSettings,
      id,
      sanitizeSettingValue(id, RZORD_NONE, value)
    );
    writeGlobalSettings(globalSettings);
    return;
  }

  if (!isConcreteModeForPerSettings(mode)) {
    return;
  }
  // The PSX record keeps its poll timing in the GameCube L byte.
  if (id == SettingId::GameCubeLSwitchMode && psx_poll::storedInRecord(mode)) {
    return;
  }
  // The PSX record keeps its neGcon I/II/L choice in the NES Device byte.
  if (id == SettingId::PowerpadMode && negconInputsStoredInRecord(mode)) {
    return;
  }

  PerModeSettingsRecord perModeSettings{};
  readPerModeSettings(mode, perModeSettings);
  writeRecordSettingValue(
    perModeSettings,
    id,
    sanitizeSettingValue(id, mode, value)
  );
  sanitizePerModeSettings(mode, perModeSettings);
  writePerModeSettings(mode, perModeSettings);
}

void saveSystemSettingByte(SettingId id, uint8_t value) {
  saveSettingValue(id, value, deviceMode);
}

void saveSystemSettingWord(SettingId id, uint16_t value) {
  saveSettingValue(id, value, deviceMode);
}

void saveSoundSettings(uint8_t buzzerModeValue, uint16_t soundEvents) {
  GlobalSettingsRecord globalSettings{};
  readGlobalSettings(globalSettings);
  globalSettings.buzzer_mode = (uint8_t)sanitizeSettingValue(SettingId::BuzzerMode, RZORD_NONE, buzzerModeValue);
  globalSettings.sound_events = (uint16_t)sanitizeSettingValue(SettingId::SoundEvents, RZORD_NONE, soundEvents);
  writeGlobalSettings(globalSettings);
}

void saveSoundEventSettings(uint16_t soundEvents) {
  GlobalSettingsRecord globalSettings{};
  readGlobalSettings(globalSettings);
  globalSettings.sound_events = (uint16_t)sanitizeSettingValue(SettingId::SoundEvents, RZORD_NONE, soundEvents);
  writeGlobalSettings(globalSettings);
}

void saveOledSettings(uint8_t oledInactiveValue, uint8_t oledWakeValue, uint8_t screensaverModeValue,
                      uint8_t screensaverAnimationValue) {
  GlobalSettingsRecord globalSettings{};
  readGlobalSettings(globalSettings);
  writeRecordSettingValue(
    globalSettings, SettingId::OledInactive,
    sanitizeSettingValue(SettingId::OledInactive, RZORD_NONE, oledInactiveValue));
  writeRecordSettingValue(
    globalSettings, SettingId::OledWake,
    sanitizeSettingValue(SettingId::OledWake, RZORD_NONE, oledWakeValue));
  writeRecordSettingValue(
    globalSettings, SettingId::ScreensaverMode,
    sanitizeSettingValue(SettingId::ScreensaverMode, RZORD_NONE, screensaverModeValue));
  globalSettings.screensaver_animation =
    (uint8_t)sanitizeSettingValue(SettingId::ScreensaverAnimation, RZORD_NONE, screensaverAnimationValue);
  writeGlobalSettings(globalSettings);
}

void saveGunconOffsets(int8_t offsetX, int8_t offsetY) {
  if (!isConcreteModeForPerSettings(deviceMode)) {
    return;
  }

  PerModeSettingsRecord settings{};
  readPerModeSettings(deviceMode, settings);
  settings.guncon_offset_x = normalizeGunconAlignmentOffset(offsetX);
  settings.guncon_offset_y = normalizeGunconAlignmentOffset(offsetY);
  sanitizePerModeSettings(deviceMode, settings);
  writePerModeSettings(deviceMode, settings);
}

bool saveButtonRemaps() {
  // Remaps belong to the current input+output pair.
  return writeRemapsForPair(deviceMode, configuredOutputMode, active_remaps);
}

bool factoryResetSettings(bool eraseRemaps) {
  // A factory reset intentionally publishes immediately and reboots. Drop
  // every staged pre-reset value first so it cannot be flushed afterward and
  // resurrect user configuration over the freshly written defaults.
  configurationPersistenceDiscardDeferred();
  configurationPersistenceBeginImmediate();

  bool externalEraseComplete = true;
  bool localEraseComplete = productClearPersistentStorage();
#if ADAPT_POLICY_RESERVED_FEEDBACK_STORAGE
  (void)hidMappingProfileClear();
  (void)modern2usbAnalogProfilesClear();
#endif
#if defined(ADAPT_CAP_CONTROLLER_HOTKEY_PROFILES)
  (void)modern2usbHotkeyProfilesClear();
#endif
#if defined(ENABLE_INPUT_JVS) && \
    !(ADAPT_POLICY_PS5_DIAGNOSTIC_RUNTIME && !defined(PS5_DIAG_USE_PERSISTED_SETTINGS))
  jvsAnalogClearConfig();
  jvsPanelClearMap();
#endif
#ifdef ENABLE_BLUETOOTH_PAIRING
  localEraseComplete = bluetoothBondClear();
#endif
#ifdef ENABLE_INPUT_ESP32_SPI
  // Bluetooth bonds and home-network credentials live on the ESP32 rather
  // than in RP2040 settings flash. The combined bond clear has a tokenized
  // terminal result; pre-SPI repair still leaves its broad erase deferred.
  const bool bondsErased =
      esp32SpiForgetAllBondsDurableBlocking(5000);
  const bool wifiErased = esp32SpiForgetWifiBlocking(5000);
  externalEraseComplete = bondsErased && wifiErased;
#endif

  // Kept remaps are read before the wipe and restored with the defaults.
  uint8_t keptRemaps[MAX_INPUT_MODES][PER_MODE_REMAP_SIZE];
  if (!eraseRemaps) {
    for (uint8_t modeValue = 0; modeValue < MAX_INPUT_MODES; ++modeValue) {
      PerModeSettingsRecord settings{};
      readPerModeSettings((DeviceEnum)modeValue, settings);
      memcpy(keptRemaps[modeValue], settings.remaps, PER_MODE_REMAP_SIZE);
    }
  }

  EepromTransaction txn;
  stageSettingsRegionResetKeepingBuildStamp(txn);
  if constexpr (RZORD_APPENDED_MODE_COUNT > 0) {
    stageEepromRangeFill(txn, settingsAppendedModeRecordBase(),
                         SETTINGS_APPENDED_MODE_REGION_SIZE, 0xFF);
  }
#if defined(ADAPT_FEATURE_FIRST_BOOT_SETUP)
  stageEepromRangeFill(txn, menuModeVisibilityRecordBase(),
                       MENU_MODE_VISIBILITY_REGION_SIZE, 0xFF);
#endif
  txn.commit();

  writeGlobalSettings(buildDefaultGlobalSettings());
  HotkeyBindingSet hotkeys{};
  setDefaultHotkeyBindings(hotkeys);
  writeHotkeyBindings(hotkeys);
  clearButtonChordRemaps();
  #if defined(ADAPT_CAP_DUAL_INPUT_MERGE)
  clearClassicDualMergeConfig();
  #endif
#if ADAPT_REMAPS_PER_OUTPUT
  if (eraseRemaps) {
    // Per input+output remaps live outside the settings region.
    remapPairEraseAll();
  }
#endif

  for (uint8_t modeValue = 0; modeValue < MAX_INPUT_MODES; ++modeValue) {
    const DeviceEnum mode = (DeviceEnum)modeValue;
    if (!isConcreteModeForPerSettings(mode)) {
      continue;
    }
    PerModeSettingsRecord settings{};
    readPerModeSettings(mode, settings);
    if (!eraseRemaps) {
      memcpy(settings.remaps, keptRemaps[modeValue], PER_MODE_REMAP_SIZE);
    }
    sanitizePerModeSettings(mode, settings);
    writePerModeSettings(mode, settings);
  }
  const bool localPersistenceComplete =
      configurationPersistenceFlushBounded(3, 2);
#if defined(ADAPT_FEATURE_FIRST_BOOT_SETUP)
  MenuModeVisibilityRecord clearedVisibility{};
  (void)readMenuModeVisibility(clearedVisibility);
#endif
  configurationPersistenceEndImmediate();
  return externalEraseComplete && localEraseComplete &&
         localPersistenceComplete;
}
