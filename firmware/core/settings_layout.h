#pragma once

#include <stddef.h>
#include <stdint.h>

#include "settings_store.h"

constexpr uint16_t GLOBAL_SETTINGS_RECORD_SIZE =
  PERSISTED_BLOCK_HEADER_SIZE + sizeof(GlobalSettingsRecord);
constexpr uint16_t PER_MODE_SETTINGS_RECORD_SIZE =
  PERSISTED_BLOCK_HEADER_SIZE + sizeof(PerModeSettingsRecord);

constexpr uint16_t SETTINGS_STORAGE_REGION_BASE = 0;
constexpr uint16_t SETTINGS_GLOBAL_RECORD_BASE = SETTINGS_STORAGE_REGION_BASE;
constexpr uint16_t SETTINGS_PER_MODE_RECORD_BASE =
  SETTINGS_GLOBAL_RECORD_BASE + (PERSISTED_AB_SLOT_COUNT * GLOBAL_SETTINGS_RECORD_SIZE);
// The auth keys and the hotkey, chord and dual-merge records follow this
// region, so its size must never change for an existing mode.
constexpr uint16_t SETTINGS_STORAGE_REGION_SIZE =
  SETTINGS_PER_MODE_RECORD_BASE +
  (PERSISTED_LAYOUT_INPUT_MODES * PERSISTED_AB_SLOT_COUNT * PER_MODE_SETTINGS_RECORD_SIZE);

// Arduino-Pico emulates EEPROM in one 4 KiB flash sector: EEPROM.begin()
// clamps larger sizes and silently ignores accesses past the sector, and every
// commit erases the whole sector. No persisted record may end beyond it.
constexpr uint16_t ADAPT_EEPROM_EMULATION_SIZE = 4096;

// RZORD_NONE never owns a per-mode record (isConcreteModeForPerSettings), so the
// A/B pair reserved for ordinal 0 at the start of the per-mode region is free.
// A record whose historical chained address would cross the EEPROM end lives
// here instead. Layouts that already fit keep their historical address, so no
// stored data moves on those products (Classic2USB is byte-identical).
constexpr uint16_t SETTINGS_SPARE_RECORD_REGION_BASE =
  SETTINGS_PER_MODE_RECORD_BASE;
// The legacy 72-byte per-mode layout (settings_store_per_mode_quick.cpp) put
// ordinal 1 at base + 144, so only those first 144 bytes are free in both.
constexpr uint16_t SETTINGS_SPARE_RECORD_REGION_SIZE = 144;
static_assert(SETTINGS_SPARE_RECORD_REGION_SIZE <=
                  PERSISTED_AB_SLOT_COUNT * PER_MODE_SETTINGS_RECORD_SIZE,
              "spare record region exceeds the RZORD_NONE per-mode slot");
static_assert(RZORD_NONE == 0,
              "the spare record region is the unused RZORD_NONE per-mode slot");
// Fixed offsets inside the spare region, one per relocatable record.
constexpr uint16_t SETTINGS_SPARE_BUTTON_CHORD_OFFSET = 0;
constexpr uint16_t SETTINGS_SPARE_DUAL_MERGE_OFFSET = 84;
// The firmware build stamp's 18 bytes at the end of the region, used when the
// chained stores leave it no room before the next fixed record
// (firmware_build_stamp.cpp). Only a relocated dual-merge record can reach it.
constexpr uint16_t SETTINGS_SPARE_BUILD_STAMP_OFFSET = 126;

inline constexpr bool eepromRangeFits(uint32_t base, uint32_t length) {
  return base + length <= ADAPT_EEPROM_EMULATION_SIZE;
}

// Keeps the historical chained address when the A/B pair fits the 4 KiB
// window, otherwise returns the record's fixed spare-region address.
inline constexpr uint16_t eepromChainedOrSpareRecordBase(uint16_t chainedBase,
                                                         uint16_t abLength,
                                                         uint16_t spareOffset) {
  // A record too large for its spare slot keeps the chained address; the
  // per-product layout test (tools/test_eeprom_layout.py) then fails.
  return (eepromRangeFits(chainedBase, abLength) ||
          spareOffset + abLength > SETTINGS_SPARE_RECORD_REGION_SIZE)
             ? chainedBase
             : (uint16_t)(SETTINGS_SPARE_RECORD_REGION_BASE + spareOffset);
}

// Per-mode records of the appended modes follow every other EEPROM store, so
// they cannot overlap one. The base is the end of that chain
// (boot_storage_runtime.cpp).
constexpr uint16_t SETTINGS_APPENDED_MODE_REGION_SIZE =
  RZORD_APPENDED_MODE_COUNT * PERSISTED_AB_SLOT_COUNT * PER_MODE_SETTINGS_RECORD_SIZE;
uint16_t settingsAppendedModeRecordBase();

// First-boot menu visibility owns a fixed A/B record at the end of RP2040's
// 4 KiB EEPROM emulation window. Keeping the address fixed prevents appended
// input-mode records from relocating user choices on a firmware update.
constexpr uint16_t MENU_MODE_VISIBILITY_EEPROM_SIZE = ADAPT_EEPROM_EMULATION_SIZE;
constexpr uint16_t MENU_MODE_VISIBILITY_RECORD_SIZE =
  PERSISTED_BLOCK_HEADER_SIZE + sizeof(MenuModeVisibilityRecord);
constexpr uint16_t MENU_MODE_VISIBILITY_REGION_SIZE =
  PERSISTED_AB_SLOT_COUNT * MENU_MODE_VISIBILITY_RECORD_SIZE;
constexpr uint16_t MENU_MODE_VISIBILITY_RECORD_BASE =
  MENU_MODE_VISIBILITY_EEPROM_SIZE - MENU_MODE_VISIBILITY_REGION_SIZE;

inline constexpr uint16_t menuModeVisibilityRecordBase() {
#if defined(REFLEX_HOST_TEST_EEPROM_AUTH) || \
    defined(REFLEX_HOST_TEST_EEPROM_BONDS)
  // The durability harness maps production raw-flash stores into fake EEPROM.
  // Keep its synthetic regions separate from the real 4 KiB EEPROM contract.
  return 16384;
#else
  return MENU_MODE_VISIBILITY_RECORD_BASE;
#endif
}

inline constexpr uint16_t menuModeVisibilityStorageRequiredEnd() {
  return menuModeVisibilityRecordBase() + MENU_MODE_VISIBILITY_REGION_SIZE;
}

#if defined(ADAPT_FEATURE_FIRST_BOOT_SETUP)
static_assert(RZORD_LAST <= 32,
              "menu input visibility mask needs a wider persisted record");
static_assert(OUTPUT_LAST <= 32,
              "menu output visibility mask needs a wider persisted record");
static_assert(SETTINGS_STORAGE_REGION_SIZE <= MENU_MODE_VISIBILITY_RECORD_BASE,
              "fixed settings region overlaps menu visibility storage");
#endif

inline constexpr uint16_t getBootSettingsRecordAddress(uint8_t slot) {
  return SETTINGS_GLOBAL_RECORD_BASE + ((slot & 0x01u) * GLOBAL_SETTINGS_RECORD_SIZE);
}

inline constexpr uint16_t getGlobalSettingsPayloadAddress(uint8_t slot = 0) {
  return getBootSettingsRecordAddress(slot) + PERSISTED_BLOCK_HEADER_SIZE;
}

// Records are addressed by the stable persisted input-mode ID, never by the
// build-dependent DeviceEnum ordinal (persisted_input_mode.h).
inline uint16_t getPerModeSettingsRecordBase(DeviceEnum mode) {
  constexpr uint16_t stride = PERSISTED_AB_SLOT_COUNT * PER_MODE_SETTINGS_RECORD_SIZE;
  const uint8_t id = persistedInputModeId(mode);
  if constexpr (RZORD_APPENDED_MODE_COUNT > 0) {
    if (id >= PERSISTED_LAYOUT_INPUT_MODES) {
      return settingsAppendedModeRecordBase() +
             (uint16_t)(id - PERSISTED_LAYOUT_INPUT_MODES) * stride;
    }
  }
  return SETTINGS_PER_MODE_RECORD_BASE + (uint16_t)id * stride;
}

inline uint16_t getPerModeSettingsRecordAddress(DeviceEnum mode, uint8_t slot) {
  return getPerModeSettingsRecordBase(mode) + ((slot & 0x01u) * PER_MODE_SETTINGS_RECORD_SIZE);
}

inline uint16_t getPerModeSettingsPayloadAddress(DeviceEnum mode, uint8_t slot) {
  return getPerModeSettingsRecordAddress(mode, slot) + PERSISTED_BLOCK_HEADER_SIZE;
}

inline uint16_t getPerModeQuickSettingsAddress(DeviceEnum mode) {
  return getPerModeSettingsPayloadAddress(mode, 0);
}

inline uint16_t getPerModeQuickSettingsHeaderAddress(DeviceEnum mode) {
  return getPerModeSettingsRecordAddress(mode, 0);
}

inline uint16_t getPerModeQuickSettingsRecordBase(DeviceEnum mode) {
  return getPerModeSettingsRecordBase(mode);
}

inline uint16_t getPerModeQuickSettingsRecordAddress(DeviceEnum mode, uint8_t slot) {
  return getPerModeSettingsRecordAddress(mode, slot);
}

inline uint16_t getPerModeTurboAddress(DeviceEnum mode) {
  return getPerModeSettingsPayloadAddress(mode, 0) +
         (uint16_t)offsetof(PerModeSettingsRecord, turbo_rates);
}

inline uint16_t getPerModeTurboHeaderAddress(DeviceEnum mode) {
  return getPerModeSettingsRecordAddress(mode, 0);
}

inline uint16_t getPerModeTurboRecordBase(DeviceEnum mode) {
  return getPerModeSettingsRecordBase(mode);
}

inline uint16_t getPerModeTurboRecordAddress(DeviceEnum mode, uint8_t slot) {
  return getPerModeSettingsRecordAddress(mode, slot);
}

inline uint16_t getPerModeRemapAddress(DeviceEnum mode) {
  return getPerModeSettingsPayloadAddress(mode, 0) +
         (uint16_t)offsetof(PerModeSettingsRecord, remaps);
}

inline uint16_t getPerModeRemapHeaderAddress(DeviceEnum mode) {
  return getPerModeSettingsRecordAddress(mode, 0);
}

inline uint16_t getPerModeRemapRecordBase(DeviceEnum mode) {
  return getPerModeSettingsRecordBase(mode);
}

inline uint16_t getPerModeRemapRecordAddress(DeviceEnum mode, uint8_t slot) {
  return getPerModeSettingsRecordAddress(mode, slot);
}
