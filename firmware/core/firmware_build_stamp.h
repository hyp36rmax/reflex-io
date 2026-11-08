#pragma once

// Which firmware build last ran on these settings.
//
// Settings deliberately survive a UF2 update, but Auto's reboot hand-off in
// the watchdog scratch registers (firmware_platform_config.h) only means
// something to the build that wrote it. The stamp lets a new build discard a
// hand-off it did not write, and lets Classic2USB say "Firmware updated" once
// (platform/boot/boot_ui_runtime.cpp).

#include <stdint.h>

#include "../product_config.h"
#include "../firmware_build_info.h"
#include "eeprom_helper.h"
#include "settings_store.h"

// The source hash tools/pio_hooks.py injects into every build; the fallback
// matches management_identity.h.
#ifndef ADAPT_IDENTITY_BUILD
#define ADAPT_IDENTITY_BUILD "unknown"
#endif

constexpr uint16_t PERSISTED_BLOCK_MAGIC_FIRMWARE_BUILD_STAMP = 0x4653;  // FS

struct FirmwareBuildStampRecord {
  // FNV-1a of FIRMWARE_VERSION_STRING, a NUL byte and ADAPT_IDENTITY_BUILD.
  uint32_t build_hash;
  uint8_t version_major;
  uint8_t version_minor;
  uint8_t version_patch;
  // Written as 0 and not checked on read.
  uint8_t reserved;
};

static_assert(sizeof(FirmwareBuildStampRecord) == 8,
              "FirmwareBuildStampRecord size changed unexpectedly");

// One slot, not A/B: a torn write reads as missing, which only means "the
// build changed" (the hand-off is cleared and the record written again).
constexpr uint16_t FIRMWARE_BUILD_STAMP_RECORD_SIZE =
    PERSISTED_BLOCK_HEADER_SIZE + sizeof(FirmwareBuildStampRecord);

namespace firmware_build_stamp {

constexpr uint32_t kFnvOffset = 2166136261u;
constexpr uint32_t kFnvPrime = 16777619u;

constexpr uint32_t fnv1aByte(uint32_t hash, uint8_t value) {
  return (hash ^ value) * kFnvPrime;
}

constexpr uint32_t fnv1a(uint32_t hash, const char* text) {
  while (*text) {
    hash = fnv1aByte(hash, static_cast<uint8_t>(*text++));
  }
  return hash;
}

constexpr uint32_t buildHash(const char* version, const char* build) {
  return fnv1a(fnv1aByte(fnv1a(kFnvOffset, version), 0), build);
}

constexpr FirmwareBuildStampRecord current() {
  return FirmwareBuildStampRecord{
      buildHash(FIRMWARE_VERSION_STRING, ADAPT_IDENTITY_BUILD),
      static_cast<uint8_t>(FIRMWARE_VERSION_MAJOR),
      static_cast<uint8_t>(FIRMWARE_VERSION_MINOR),
      static_cast<uint8_t>(FIRMWARE_VERSION_PATCH),
      0};
}

constexpr bool sameBuild(const FirmwareBuildStampRecord& a,
                         const FirmwareBuildStampRecord& b) {
  return a.build_hash == b.build_hash && a.version_major == b.version_major &&
         a.version_minor == b.version_minor && a.version_patch == b.version_patch;
}

}  // namespace firmware_build_stamp

// The record sits right after the product's last chained EEPROM store
// (persistentStorageOccupiedEnd()), the rule appended-mode records follow,
// and must end at or before the next fixed record: Classic2USB's menu
// visibility pair, otherwise the end of the 4 KiB EEPROM window. When the
// chained stores leave it no room there (Classic2USB since the CD-i mode), it
// takes its slot in the spare record region (settings_layout.h). Checked at
// boot; tools/test_eeprom_layout.py checks it per product.
inline constexpr uint16_t firmwareBuildStampStorageLimit() {
#if defined(REFLEX_HOST_TEST_EEPROM_AUTH) || \
    defined(REFLEX_HOST_TEST_EEPROM_BONDS)
  // The durability harness maps raw-flash stores into fake EEPROM past 4 KiB
  // and moves the menu visibility pair beyond them (settings_layout.h).
  return menuModeVisibilityRecordBase();
#elif defined(ADAPT_FEATURE_FIRST_BOOT_SETUP)
  return MENU_MODE_VISIBILITY_RECORD_BASE;
#else
  return ADAPT_EEPROM_EMULATION_SIZE;
#endif
}

uint16_t firmwareBuildStampRecordBase();
// End of the record, or the occupied end when the record does not fit.
uint16_t firmwareBuildStampStorageRequiredEnd();
bool firmwareBuildStampStorageAvailable();

bool readFirmwareBuildStamp(FirmwareBuildStampRecord& record);
// Factory reset: erases the settings region (global and per-mode records)
// except the stamp when it sits in the spare slot there.
void stageSettingsRegionResetKeepingBuildStamp(EepromTransaction& txn);
// Commits and reads back. False when storage is unavailable or the write did
// not verify.
bool writeFirmwareBuildStamp(const FirmwareBuildStampRecord& record);

// Boot: after beginPersistentStorage() and before the settings load and
// anything reads Auto's reboot hand-off (loadPersistedBootConfiguration(),
// restoreInputModeFromScratchRegisters(), restoreAutoOutputBootState()).
// When the stored stamp is missing, invalid or from another build, clears the
// hand-off so Auto input and output detection start fresh. On first-boot
// setup products with setup complete, it also arms the "Firmware updated"
// notice. Then records this build. Saved settings are left alone, so a manual
// Input or Output survives an update.
void checkFirmwareBuildStampAtBoot();
bool firmwareBuildChangedThisBoot();
// Show "Firmware updated to vX.Y.Z" on this boot's splash.
bool firmwareUpdateNoticeThisBoot();
