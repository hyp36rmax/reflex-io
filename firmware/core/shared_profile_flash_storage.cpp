#include "../product_config.h"
#include "shared_profile_flash_storage.h"

#include <Arduino.h>
#include <string.h>

#if defined(REFLEX_HOST_TEST_SHARED_PROFILE_FLASH) || \
    defined(ARDUINO_ARCH_RP2040)
#include <pico/mutex.h>
#endif

#if !defined(REFLEX_HOST_TEST_SHARED_PROFILE_FLASH) && \
    defined(ARDUINO_ARCH_RP2040)
#include <hardware/flash.h>
#include <hardware/sync.h>
#include "reserved_flash_layout.h"
#include "settings_journal.h"
#endif

namespace {

#if defined(REFLEX_HOST_TEST_SHARED_PROFILE_FLASH) || \
    defined(ARDUINO_ARCH_RP2040)
alignas(4) uint8_t sectorScratch[SHARED_PROFILE_FLASH_SECTOR_SIZE];
bool editActive = false;
auto_init_mutex(editMutex);
#endif

#if defined(REFLEX_HOST_TEST_SHARED_PROFILE_FLASH)
alignas(4) uint8_t hostSector[SHARED_PROFILE_FLASH_SECTOR_SIZE];
uint8_t hostFailedPrograms = 0;
uint32_t hostProgramCount = 0;
#endif

void releaseEdit() {
#if defined(REFLEX_HOST_TEST_SHARED_PROFILE_FLASH) || \
    defined(ARDUINO_ARCH_RP2040)
  editActive = false;
  mutex_exit(&editMutex);
#endif
}

}  // namespace

const uint8_t* sharedProfileFlashData() {
#if defined(REFLEX_HOST_TEST_SHARED_PROFILE_FLASH)
  return hostSector;
#elif defined(ARDUINO_ARCH_RP2040)
  return reinterpret_cast<const uint8_t*>(
      reflexBluetoothBondFlashSectorAddress());
#else
  return nullptr;
#endif
}

bool sharedProfileFlashBeginEdit(uint8_t*& sectorImage) {
#if !defined(REFLEX_HOST_TEST_SHARED_PROFILE_FLASH) && \
    !defined(ARDUINO_ARCH_RP2040)
  sectorImage = nullptr;
  return false;
#else
  // The lock spans copy, edit, erase, program, and verification. A try-lock
  // avoids deadlocking if an interrupt or the other core attempts a write.
  if (!mutex_try_enter(&editMutex, nullptr)) {
    sectorImage = nullptr;
    return false;
  }
  editActive = true;
  memcpy(sectorScratch, sharedProfileFlashData(), sizeof(sectorScratch));
  sectorImage = sectorScratch;
  return true;
#endif
}

bool sharedProfileFlashCommitEdit() {
#if !defined(REFLEX_HOST_TEST_SHARED_PROFILE_FLASH) && \
    !defined(ARDUINO_ARCH_RP2040)
  return false;
#else
  if (!editActive) return false;
  const uint8_t* current = sharedProfileFlashData();
  if (memcmp(current, sectorScratch, sizeof(sectorScratch)) == 0) {
    releaseEdit();
    return true;
  }

#if defined(REFLEX_HOST_TEST_SHARED_PROFILE_FLASH)
  if (hostFailedPrograms != 0) {
    --hostFailedPrograms;
    releaseEdit();
    return false;
  }
  memcpy(hostSector, sectorScratch, sizeof(hostSector));
  ++hostProgramCount;
#elif defined(ADAPT_SETTINGS_JOURNAL_ACTIVE)
  (void)settingsJournalWriteSector(SettingsJournalTarget::kSharedProfiles,
                                   sectorScratch, sizeof(sectorScratch));
#else
  const uint32_t irqState = save_and_disable_interrupts();
  rp2040.idleOtherCore();
  const uint32_t offset = reflexFlashOffset(
      reflexBluetoothBondFlashSectorAddress());
  flash_range_erase(offset, SHARED_PROFILE_FLASH_SECTOR_SIZE);
  flash_range_program(offset, sectorScratch,
                      SHARED_PROFILE_FLASH_SECTOR_SIZE);
  rp2040.resumeOtherCore();
  restore_interrupts(irqState);
#endif

  const bool verified = memcmp(sharedProfileFlashData(), sectorScratch,
                               sizeof(sectorScratch)) == 0;
  releaseEdit();
  return verified;
#endif
}

#if defined(REFLEX_HOST_TEST_SHARED_PROFILE_FLASH)
void sharedProfileFlashHostReset(uint8_t fill) {
  memset(hostSector, fill, sizeof(hostSector));
  memset(sectorScratch, fill, sizeof(sectorScratch));
  hostFailedPrograms = 0;
  hostProgramCount = 0;
  editActive = false;
  mutex_init(&editMutex);
}

void sharedProfileFlashHostFailNextPrograms(uint8_t count) {
  hostFailedPrograms = count;
}

uint32_t sharedProfileFlashHostProgramCount() {
  return hostProgramCount;
}

void sharedProfileFlashHostClearProgramCount() {
  hostProgramCount = 0;
}

uint8_t* sharedProfileFlashHostData() {
  return hostSector;
}
#endif
