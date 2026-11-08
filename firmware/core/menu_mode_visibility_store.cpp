#include "../product_config.h"

#include "settings_store.h"

#include <string.h>

#include "eeprom_helper.h"
#include "settings_layout.h"
#include "settings_persistence_deferred.h"

namespace {

constexpr MenuModeVisibilityRecord defaultVisibility() {
  MenuModeVisibilityRecord visibility{};
#if defined(ADAPT_FEATURE_FIRST_BOOT_SETUP) && ADAPT_POLICY_HIDE_UNADAPTED_KEYPAD_INPUTS
  // Keep adapter-dependent protocols available to owners who enable them.
#ifdef ENABLE_INPUT_INTV
  visibility.hidden_input_modes |= 1UL << (uint8_t)RZORD_INTV;
#endif
#ifdef ENABLE_INPUT_COLECOVISION
  visibility.hidden_input_modes |= 1UL << (uint8_t)RZORD_COLECOVISION;
#endif
#ifdef ENABLE_INPUT_CDI
  visibility.hidden_input_modes |= 1UL << (uint8_t)RZORD_CDI;
#endif
#endif
  return visibility;
}

MenuModeVisibilityRecord activeVisibility = defaultVisibility();
bool visibilityStorageAvailable = false;

#if defined(ADAPT_FEATURE_FIRST_BOOT_SETUP)
constexpr uint32_t maskForCount(uint8_t count) {
  return count >= 32u ? UINT32_MAX : ((1UL << count) - 1UL);
}

MenuModeVisibilityRecord sanitizeVisibility(MenuModeVisibilityRecord visibility) {
  visibility.hidden_input_modes &= maskForCount((uint8_t)RZORD_LAST);
  visibility.hidden_output_modes &= maskForCount((uint8_t)OUTPUT_LAST);
  visibility.hidden_input_modes &= ~(1UL << (uint8_t)RZORD_NONE);
#if defined(ENABLE_INPUT_AUTODETECT)
  visibility.hidden_input_modes &= ~(1UL << (uint8_t)RZORD_AUTODETECT);
#endif
  visibility.hidden_output_modes &= ~(1UL << (uint8_t)OUTPUT_AUTO);
  return visibility;
}
#endif

bool readStoredVisibility(MenuModeVisibilityRecord& visibility) {
#if defined(ADAPT_FEATURE_FIRST_BOOT_SETUP)
  if (!visibilityStorageAvailable) {
    visibility = defaultVisibility();
    return false;
  }
  uint8_t slot = 0;
  uint16_t generation = 0;
  if (!readLatestPersistedRecordAB(
        menuModeVisibilityRecordBase(),
        MENU_MODE_VISIBILITY_RECORD_SIZE,
        PERSISTED_BLOCK_MAGIC_MENU_MODE_VISIBILITY,
        slot,
        generation,
        visibility)) {
    visibility = defaultVisibility();
    return false;
  }
  visibility = sanitizeVisibility(visibility);
  return true;
#else
  visibility = defaultVisibility();
  return false;
#endif
}

#if defined(ADAPT_FEATURE_FIRST_BOOT_SETUP)
uint16_t targetRecordAddress(
    uint16_t recordBase,
    uint16_t recordSize,
    uint16_t magic,
    const GlobalSettingsRecord&) {
  uint8_t currentSlot = 0;
  uint16_t currentGeneration = 0;
  GlobalSettingsRecord current{};
  const bool hasCurrent = readLatestPersistedRecordAB(
      recordBase, recordSize, magic, currentSlot, currentGeneration, current);
  const uint8_t targetSlot = hasCurrent ? (uint8_t)(currentSlot ^ 0x01u) : 0;
  return recordBase + targetSlot * recordSize;
}

uint16_t targetRecordAddress(
    uint16_t recordBase,
    uint16_t recordSize,
    uint16_t magic,
    const MenuModeVisibilityRecord&) {
  uint8_t currentSlot = 0;
  uint16_t currentGeneration = 0;
  MenuModeVisibilityRecord current{};
  const bool hasCurrent = readLatestPersistedRecordAB(
      recordBase, recordSize, magic, currentSlot, currentGeneration, current);
  const uint8_t targetSlot = hasCurrent ? (uint8_t)(currentSlot ^ 0x01u) : 0;
  return recordBase + targetSlot * recordSize;
}

template <size_t N>
void captureBytes(uint16_t address, uint8_t (&bytes)[N]) {
  for (size_t i = 0; i < N; ++i) {
    bytes[i] = EEPROM.read(address + i);
  }
}

template <size_t N>
void restoreBytes(uint16_t address, const uint8_t (&bytes)[N]) {
  for (size_t i = 0; i < N; ++i) {
    EEPROM.write(address + i, bytes[i]);
  }
}

bool saveVisibilityOnly(const MenuModeVisibilityRecord& requested) {
  if (!visibilityStorageAvailable) {
    return false;
  }

  const MenuModeVisibilityRecord visibility = sanitizeVisibility(requested);
  MenuModeVisibilityRecord current{};
  (void)readStoredVisibility(current);
  if (memcmp(&current, &visibility, sizeof(visibility)) == 0) {
    activeVisibility = visibility;
    return true;
  }
  if (settingsPersistenceHasPending()) {
    return false;
  }
  const uint16_t targetAddress = targetRecordAddress(
      menuModeVisibilityRecordBase(),
      MENU_MODE_VISIBILITY_RECORD_SIZE,
      PERSISTED_BLOCK_MAGIC_MENU_MODE_VISIBILITY,
      visibility);
  uint8_t priorTarget[MENU_MODE_VISIBILITY_RECORD_SIZE]{};
  captureBytes(targetAddress, priorTarget);

  const bool retryWasPending = eeprom_commit_retry_pending();
  EepromTransaction transaction;
  (void)stagePersistedRecordAB(
      transaction,
      menuModeVisibilityRecordBase(),
      MENU_MODE_VISIBILITY_RECORD_SIZE,
      PERSISTED_BLOCK_MAGIC_MENU_MODE_VISIBILITY,
      visibility);
  if (!transaction.commit()) {
    restoreBytes(targetAddress, priorTarget);
    eeprom_commit_retry_pending() = retryWasPending;
    return false;
  }

  MenuModeVisibilityRecord verify{};
  if (!readStoredVisibility(verify) ||
      memcmp(&verify, &visibility, sizeof(visibility)) != 0) {
    return false;
  }
  activeVisibility = visibility;
  return true;
}

bool saveGlobalAndVisibility(
    const GlobalSettingsRecord& requestedGlobal,
    const MenuModeVisibilityRecord& requestedVisibility) {
  if (!visibilityStorageAvailable || settingsPersistenceHasPending()) {
    return false;
  }

  GlobalSettingsRecord global = requestedGlobal;
  sanitizeGlobalSettings(global);
  const MenuModeVisibilityRecord visibility =
      sanitizeVisibility(requestedVisibility);

  const uint16_t globalTargetAddress = targetRecordAddress(
      SETTINGS_GLOBAL_RECORD_BASE,
      GLOBAL_SETTINGS_RECORD_SIZE,
      PERSISTED_BLOCK_MAGIC_GLOBAL,
      global);
  const uint16_t visibilityTargetAddress = targetRecordAddress(
      menuModeVisibilityRecordBase(),
      MENU_MODE_VISIBILITY_RECORD_SIZE,
      PERSISTED_BLOCK_MAGIC_MENU_MODE_VISIBILITY,
      visibility);
  uint8_t priorGlobalTarget[GLOBAL_SETTINGS_RECORD_SIZE]{};
  uint8_t priorVisibilityTarget[MENU_MODE_VISIBILITY_RECORD_SIZE]{};
  captureBytes(globalTargetAddress, priorGlobalTarget);
  captureBytes(visibilityTargetAddress, priorVisibilityTarget);

  const bool retryWasPending = eeprom_commit_retry_pending();
  EepromTransaction transaction;
  (void)stagePersistedRecordAB(
      transaction,
      SETTINGS_GLOBAL_RECORD_BASE,
      GLOBAL_SETTINGS_RECORD_SIZE,
      PERSISTED_BLOCK_MAGIC_GLOBAL,
      global);
  (void)stagePersistedRecordAB(
      transaction,
      menuModeVisibilityRecordBase(),
      MENU_MODE_VISIBILITY_RECORD_SIZE,
      PERSISTED_BLOCK_MAGIC_MENU_MODE_VISIBILITY,
      visibility);
  if (!transaction.commit()) {
    restoreBytes(globalTargetAddress, priorGlobalTarget);
    restoreBytes(visibilityTargetAddress, priorVisibilityTarget);
    eeprom_commit_retry_pending() = retryWasPending;
    return false;
  }

  uint8_t globalSlot = 0;
  uint16_t globalGeneration = 0;
  GlobalSettingsRecord verifyGlobal{};
  MenuModeVisibilityRecord verifyVisibility{};
  if (!readLatestPersistedRecordAB(
        SETTINGS_GLOBAL_RECORD_BASE,
        GLOBAL_SETTINGS_RECORD_SIZE,
        PERSISTED_BLOCK_MAGIC_GLOBAL,
        globalSlot,
        globalGeneration,
        verifyGlobal) ||
      memcmp(&verifyGlobal, &global, sizeof(global)) != 0 ||
      !readStoredVisibility(verifyVisibility) ||
      memcmp(&verifyVisibility, &visibility, sizeof(visibility)) != 0) {
    return false;
  }

  activeVisibility = visibility;
  return true;
}
#endif

}  // namespace

void initializeMenuModeVisibilityFromStorage(uint16_t occupiedStorageEnd) {
#if defined(ADAPT_FEATURE_FIRST_BOOT_SETUP)
#if defined(REFLEX_HOST_TEST_EEPROM_AUTH) || \
    defined(REFLEX_HOST_TEST_EEPROM_BONDS)
  // The host durability suite deliberately relocates the production raw-flash
  // bond store into fake EEPROM, beyond the real hardware layout. Individual
  // cases reset that image, so keep the fixed-tail record testable there.
  (void)occupiedStorageEnd;
  visibilityStorageAvailable =
      EEPROM.length() >= menuModeVisibilityStorageRequiredEnd();
#else
  visibilityStorageAvailable =
      occupiedStorageEnd <= MENU_MODE_VISIBILITY_RECORD_BASE &&
      EEPROM.length() >= MENU_MODE_VISIBILITY_EEPROM_SIZE;
#endif
  MenuModeVisibilityRecord stored{};
  (void)readMenuModeVisibility(stored);
#else
  (void)occupiedStorageEnd;
  activeVisibility = defaultVisibility();
  visibilityStorageAvailable = false;
#endif
}

bool readMenuModeVisibility(MenuModeVisibilityRecord& visibility) {
  const bool valid = readStoredVisibility(visibility);
  activeVisibility = visibility;
  return valid;
}

bool saveMenuModeVisibility(const MenuModeVisibilityRecord& visibility) {
#if defined(ADAPT_FEATURE_FIRST_BOOT_SETUP)
  return saveVisibilityOnly(visibility);
#else
  (void)visibility;
  return false;
#endif
}

bool clearMenuModeVisibility() {
#if defined(ADAPT_FEATURE_FIRST_BOOT_SETUP)
  if (!visibilityStorageAvailable) {
    activeVisibility = defaultVisibility();
    return false;
  }
  uint8_t priorRegion[MENU_MODE_VISIBILITY_REGION_SIZE]{};
  captureBytes(menuModeVisibilityRecordBase(), priorRegion);
  const bool retryWasPending = eeprom_commit_retry_pending();
  EepromTransaction transaction;
  stageEepromRangeFill(
      transaction,
      menuModeVisibilityRecordBase(),
      MENU_MODE_VISIBILITY_REGION_SIZE,
      0xFF);
  if (!transaction.commit()) {
    restoreBytes(menuModeVisibilityRecordBase(), priorRegion);
    eeprom_commit_retry_pending() = retryWasPending;
    return false;
  }
  MenuModeVisibilityRecord empty{};
  const bool unexpectedlyValid = readStoredVisibility(empty);
  activeVisibility = defaultVisibility();
  return !unexpectedlyValid;
#else
  activeVisibility = defaultVisibility();
  return true;
#endif
}

bool saveGlobalSettingsWithMenuModeVisibility(
    const GlobalSettingsRecord& settings,
    const MenuModeVisibilityRecord& visibility) {
#if defined(ADAPT_FEATURE_FIRST_BOOT_SETUP)
  return saveGlobalAndVisibility(settings, visibility);
#else
  (void)settings;
  (void)visibility;
  return false;
#endif
}

bool saveFirstBootSetupSettings(
    const GlobalSettingsRecord& staged,
    const MenuModeVisibilityRecord& visibility) {
#if defined(ADAPT_FEATURE_FIRST_BOOT_SETUP)
  GlobalSettingsRecord completed = staged;
  sanitizeGlobalSettings(completed);
  completed.menu_ok_back = markFirstBootSetupComplete(completed.menu_ok_back);
  return saveGlobalAndVisibility(completed, visibility) &&
         firstBootSetupCompleteTagFromRaw(completed.menu_ok_back);
#else
  (void)staged;
  (void)visibility;
  return false;
#endif
}

bool menuModeVisibilityInputHidden(uint8_t mode) {
#if defined(ADAPT_FEATURE_FIRST_BOOT_SETUP)
  if (mode >= (uint8_t)RZORD_LAST || mode >= 32u ||
      mode == (uint8_t)RZORD_NONE) {
    return false;
  }
#if defined(ENABLE_INPUT_AUTODETECT)
  if (mode == (uint8_t)RZORD_AUTODETECT) {
    return false;
  }
#endif
  return (activeVisibility.hidden_input_modes & (1UL << mode)) != 0;
#else
  (void)mode;
  return false;
#endif
}

bool menuModeVisibilityOutputHidden(uint8_t mode) {
#if defined(ADAPT_FEATURE_FIRST_BOOT_SETUP)
  if (mode >= (uint8_t)OUTPUT_LAST || mode >= 32u ||
      mode == (uint8_t)OUTPUT_AUTO) {
    return false;
  }
  return (activeVisibility.hidden_output_modes & (1UL << mode)) != 0;
#else
  (void)mode;
  return false;
#endif
}
