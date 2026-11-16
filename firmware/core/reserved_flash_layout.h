#pragma once

#include <stdint.h>

#if defined(ARDUINO_ARCH_RP2040)
#include <hardware/flash.h>

#include "rp2040_update_rollback_layout.h"

extern "C" uint8_t _EEPROM_start;

// Reflex normally reserves three erase sectors immediately below
// Arduino-Pico's EEPROM sector. MODERN2USB's on-device updater needs that
// contiguous range for LittleFS, so the RP2040 build moves the three raw
// stores into the otherwise-unmapped final 12 KiB of its physical 2 MiB flash.
// The legacy helpers remain available for one-time, format-before-write
// migration.
constexpr uint8_t REFLEX_RESERVED_FLASH_SECTOR_COUNT = 3;
constexpr uintptr_t REFLEX_RP2040_PHYSICAL_FLASH_END =
    XIP_BASE + (2u * 1024u * 1024u);
#if ADAPT_POLICY_FIXED_END_RESERVED_FLASH_LAYOUT
static_assert(
    PICO_FLASH_SIZE_BYTES >= (2u * 1024u * 1024u),
    "MODERN2USB raw stores require an RP2040 SDK configured for the full 2 MiB flash");
#endif

inline uintptr_t reflexEepromSectorAddress() {
  return reinterpret_cast<uintptr_t>(&_EEPROM_start);
}

inline uintptr_t reflexAuthFlashSectorAddress() {
#if ADAPT_POLICY_FIXED_END_RESERVED_FLASH_LAYOUT
  return REFLEX_RP2040_PHYSICAL_FLASH_END - FLASH_SECTOR_SIZE;
#else
  return reflexEepromSectorAddress() - FLASH_SECTOR_SIZE;
#endif
}

inline uintptr_t reflexBluetoothBondFlashSectorAddress() {
#if ADAPT_POLICY_FIXED_END_RESERVED_FLASH_LAYOUT
  return REFLEX_RP2040_PHYSICAL_FLASH_END - (2u * FLASH_SECTOR_SIZE);
#else
  return reflexEepromSectorAddress() - (2u * FLASH_SECTOR_SIZE);
#endif
}

inline uintptr_t reflexHidMappingFlashSectorAddress() {
#if ADAPT_POLICY_FIXED_END_RESERVED_FLASH_LAYOUT
  return REFLEX_RP2040_PHYSICAL_FLASH_END - (3u * FLASH_SECTOR_SIZE);
#else
  return reflexEepromSectorAddress() - (3u * FLASH_SECTOR_SIZE);
#endif
}

// A fourth sector, taken only by products that cache exportable mapping lines.
// It sits below the existing three, so auth, Bluetooth bonds and learned
// mappings keep the addresses they already have on a device in the field.
#if defined(ADAPT_CAP_MAPPING_EXPORT_STORE)
inline uintptr_t reflexMappingExportFlashSectorAddress() {
#if ADAPT_POLICY_FIXED_END_RESERVED_FLASH_LAYOUT
  return REFLEX_RP2040_PHYSICAL_FLASH_END - (4u * FLASH_SECTOR_SIZE);
#else
  return reflexEepromSectorAddress() - (4u * FLASH_SECTOR_SIZE);
#endif
}
#endif

// Two sectors for the settings journal. The standard layout takes them below
// the existing three, so stored data keeps its address on a device in the field.
#if defined(ADAPT_CAP_SETTINGS_JOURNAL)
#if defined(ADAPT_CAP_MAPPING_EXPORT_STORE)
#error "The settings journal and the mapping export store want the same sector"
#endif
#if ADAPT_POLICY_FIXED_END_RESERVED_FLASH_LAYOUT
// MODERN2USB's flash below its stores is the updater's LittleFS, so its
// journal takes the first two sectors of the unused 64 KiB between the
// rollback recovery records (end 0x144000) and LittleFS (0x154000). The
// stage-3 linker caps the app at 0x0A0000. A build without stage 3 must still
// end below them, so the journal checks the image end before using them.
inline uintptr_t reflexSettingsJournalDataSectorAddress() {
  return XIP_BASE + MODERN2USB_RP2040_SETTINGS_JOURNAL_DATA_OFFSET;
}

inline uintptr_t reflexSettingsJournalControlSectorAddress() {
  return XIP_BASE + MODERN2USB_RP2040_SETTINGS_JOURNAL_CONTROL_OFFSET;
}
#else
inline uintptr_t reflexSettingsJournalDataSectorAddress() {
  return reflexEepromSectorAddress() - (4u * FLASH_SECTOR_SIZE);
}

inline uintptr_t reflexSettingsJournalControlSectorAddress() {
  return reflexEepromSectorAddress() - (5u * FLASH_SECTOR_SIZE);
}
#endif
#endif

inline uintptr_t reflexLegacyAuthFlashSectorAddress() {
  return reflexEepromSectorAddress() - FLASH_SECTOR_SIZE;
}

inline uintptr_t reflexLegacyBluetoothBondFlashSectorAddress() {
  return reflexEepromSectorAddress() - (2u * FLASH_SECTOR_SIZE);
}

inline uintptr_t reflexLegacyHidMappingFlashSectorAddress() {
  return reflexEepromSectorAddress() - (3u * FLASH_SECTOR_SIZE);
}

inline uint32_t reflexFlashOffset(uintptr_t address) {
  return static_cast<uint32_t>(address - XIP_BASE);
}
#endif
