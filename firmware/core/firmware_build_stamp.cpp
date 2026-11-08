#include "firmware_build_stamp.h"

#include <string.h>

#include <EEPROM.h>

#if defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED)
#include "hardware/watchdog.h"
#endif

#include "../firmware_platform_config.h"
#include "boot/boot_storage_runtime.h"
#include "classic_dual_merge_config.h"
#include "eeprom_helper.h"
#include "settings_layout.h"

namespace {

bool buildChangedThisBoot = false;
bool updateNoticeThisBoot = false;

// Every hand-off slot of firmware_platform_config.h: passive input (0/1),
// Auto output (2/3), Auto input hint (4/5), probe stage/aux (6/7). All read
// zero as "no hand-off".
void clearAutoHandOffScratchRegisters() {
#if defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED)
  watchdog_hw->scratch[PASSIVE_MODE_SCRATCH_MAGIC] = 0;
  watchdog_hw->scratch[PASSIVE_MODE_SCRATCH_MODE] = 0;
  watchdog_hw->scratch[AUTO_DETECT_SCRATCH_MAGIC] = 0;
  watchdog_hw->scratch[AUTO_DETECT_SCRATCH_STATE] = 0;
  // watchdog_enable() writes the SDK's own reboot marker to scratch[4], and
  // crash telemetry reads it; anything but the hint's magic is not a hand-off.
  if (watchdog_hw->scratch[AUTO_INPUT_MODE_SCRATCH_MAGIC] == AUTO_INPUT_MODE_MAGIC) {
    watchdog_hw->scratch[AUTO_INPUT_MODE_SCRATCH_MAGIC] = 0;
  }
  watchdog_hw->scratch[AUTO_INPUT_MODE_SCRATCH_MODE] = 0;
  watchdog_hw->scratch[AUTO_PROBE_SCRATCH_STAGE] = 0;
  watchdog_hw->scratch[AUTO_PROBE_SCRATCH_AUX] = 0;
#endif
}

bool updateNoticeSupported() {
#if defined(ADAPT_FEATURE_FIRST_BOOT_SETUP)
  return true;
#else
  return false;
#endif
}

static_assert(SETTINGS_SPARE_BUILD_STAMP_OFFSET + FIRMWARE_BUILD_STAMP_RECORD_SIZE <=
                  SETTINGS_SPARE_RECORD_REGION_SIZE,
              "build stamp spare slot exceeds the spare record region");
static_assert(SETTINGS_SPARE_BUTTON_CHORD_OFFSET < SETTINGS_SPARE_DUAL_MERGE_OFFSET &&
                  SETTINGS_SPARE_DUAL_MERGE_OFFSET < SETTINGS_SPARE_BUILD_STAMP_OFFSET,
              "only a relocated dual-merge record can reach the stamp's spare slot");

constexpr uint16_t kSpareStampBase =
    SETTINGS_SPARE_RECORD_REGION_BASE + SETTINGS_SPARE_BUILD_STAMP_OFFSET;

bool chainedStampFits() {
  return (uint32_t)persistentStorageOccupiedEnd() + FIRMWARE_BUILD_STAMP_RECORD_SIZE <=
         firmwareBuildStampStorageLimit();
}

bool spareStampSlotFree() {
#if defined(ADAPT_CAP_DUAL_INPUT_MERGE)
  // A relocated dual-merge record ends inside the spare region; a chained one
  // ends far past it.
  const uint32_t dualMergeEnd = classicDualMergeStorageRequiredEnd();
  if (dualMergeEnd > kSpareStampBase &&
      dualMergeEnd <= SETTINGS_SPARE_RECORD_REGION_BASE + SETTINGS_SPARE_RECORD_REGION_SIZE) {
    return false;
  }
#endif
  return true;
}

}  // namespace

// After the last chained store when it fits there. Classic2USB's appended
// input modes fill the gap before its menu visibility pair, so its stamp sits
// in the spare region instead (settings_layout.h). Moving it only makes the
// stamp read as missing once.
uint16_t firmwareBuildStampRecordBase() {
  if (chainedStampFits() || !spareStampSlotFree()) {
    return persistentStorageOccupiedEnd();
  }
  return kSpareStampBase;
}

bool firmwareBuildStampStorageAvailable() {
  // The limit is inside the 4 KiB window on a device, so a stamp that fits
  // also means every record before it does. A spare-slot stamp still needs
  // the chained stores to end within the limit.
  if (firmwareBuildStampRecordBase() == kSpareStampBase) {
    return persistentStorageOccupiedEnd() <= firmwareBuildStampStorageLimit();
  }
  return (uint32_t)firmwareBuildStampRecordBase() + FIRMWARE_BUILD_STAMP_RECORD_SIZE <=
         firmwareBuildStampStorageLimit();
}

uint16_t firmwareBuildStampStorageRequiredEnd() {
  const uint16_t base = firmwareBuildStampRecordBase();
  return firmwareBuildStampStorageAvailable()
             ? (uint16_t)(base + FIRMWARE_BUILD_STAMP_RECORD_SIZE)
             : base;
}

bool readFirmwareBuildStamp(FirmwareBuildStampRecord& record) {
  if (!firmwareBuildStampStorageAvailable() ||
      EEPROM.length() < firmwareBuildStampStorageRequiredEnd()) {
    return false;
  }
  return readPersistedRecord(firmwareBuildStampRecordBase(),
                             PERSISTED_BLOCK_MAGIC_FIRMWARE_BUILD_STAMP, record);
}

void stageSettingsRegionResetKeepingBuildStamp(EepromTransaction& txn) {
  const uint16_t regionEnd = SETTINGS_STORAGE_REGION_BASE + SETTINGS_STORAGE_REGION_SIZE;
  const uint16_t stampBase = firmwareBuildStampRecordBase();
  const uint16_t stampEnd = stampBase + FIRMWARE_BUILD_STAMP_RECORD_SIZE;
  if (!firmwareBuildStampStorageAvailable() || stampBase < SETTINGS_STORAGE_REGION_BASE ||
      stampEnd > regionEnd) {
    stageEepromRangeFill(txn, SETTINGS_STORAGE_REGION_BASE, SETTINGS_STORAGE_REGION_SIZE, 0xFF);
    return;
  }
  stageEepromRangeFill(txn, SETTINGS_STORAGE_REGION_BASE,
                       stampBase - SETTINGS_STORAGE_REGION_BASE, 0xFF);
  stageEepromRangeFill(txn, stampEnd, regionEnd - stampEnd, 0xFF);
}

bool writeFirmwareBuildStamp(const FirmwareBuildStampRecord& record) {
  if (!firmwareBuildStampStorageAvailable() ||
      EEPROM.length() < firmwareBuildStampStorageRequiredEnd()) {
    return false;
  }
  EepromTransaction transaction;
  stagePersistedRecord(transaction, firmwareBuildStampRecordBase(),
                       PERSISTED_BLOCK_MAGIC_FIRMWARE_BUILD_STAMP, record, 1);
  if (!transaction.commit()) {
    return false;
  }
  FirmwareBuildStampRecord verify{};
  return readFirmwareBuildStamp(verify) &&
         memcmp(&verify, &record, sizeof(record)) == 0;
}

void checkFirmwareBuildStampAtBoot() {
  buildChangedThisBoot = false;
  updateNoticeThisBoot = false;
#if defined(ADAPT_FEATURE_PS2_IR_CAPTURE)
  // A temporary capture image must not replace the standard image's update
  // stamp merely by booting.
  return;
#elif ADAPT_POLICY_PS5_DIAGNOSTIC_RUNTIME && !defined(PS5_DIAG_USE_PERSISTED_SETTINGS)
  // This image keeps no persisted settings and maps EEPROM only up to them.
  return;
#else
  if (!firmwareBuildStampStorageAvailable()) {
    return;
  }
  FirmwareBuildStampRecord stored{};
  const bool valid = readFirmwareBuildStamp(stored);
  const FirmwareBuildStampRecord current = firmware_build_stamp::current();
  if (valid && firmware_build_stamp::sameBuild(stored, current)) {
    return;
  }
  clearAutoHandOffScratchRegisters();
  buildChangedThisBoot = true;
  // A unit still awaiting first-boot setup (fresh, or factory reset) gets
  // the setup guide instead.
  updateNoticeThisBoot = updateNoticeSupported() && !firstBootSetupPending();
  (void)writeFirmwareBuildStamp(current);
#endif
}

bool firmwareBuildChangedThisBoot() {
  return buildChangedThisBoot;
}

bool firmwareUpdateNoticeThisBoot() {
  return updateNoticeThisBoot;
}
