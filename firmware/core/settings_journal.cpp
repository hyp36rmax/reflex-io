#include "settings_journal.h"

#if defined(REFLEX_HOST_TEST_SETTINGS_JOURNAL) || defined(ADAPT_SETTINGS_JOURNAL_ACTIVE)

#include <string.h>

#if !defined(REFLEX_HOST_TEST_SETTINGS_JOURNAL)
#include <Arduino.h>
#include <EEPROM.h>
#include <hardware/flash.h>
#include <hardware/sync.h>

#include "reserved_flash_layout.h"
#endif

namespace {

constexpr size_t kSectorSize = 4096;
constexpr size_t kPageSize = 256;
constexpr uint32_t kControlMagic = 0x314E4A52u;  // "RJN1"

struct JournalControl {
  uint32_t magic;
  uint32_t target;
  uint32_t length;
  uint32_t image_hash;
  uint32_t control_hash;
};

#if defined(REFLEX_HOST_TEST_SETTINGS_JOURNAL)
const uint8_t* targetBytes(uint32_t target) {
  return settingsJournalHostTargetBytes(static_cast<SettingsJournalTarget>(target));
}
const uint8_t* dataBytes() { return settingsJournalHostDataBytes(); }
const uint8_t* controlBytes() { return settingsJournalHostControlBytes(); }
uint32_t openFlashWindow() { return 0; }
void closeFlashWindow(uint32_t) { settingsJournalHostWindowClosed(); }
void eraseSector(const uint8_t* sector) { settingsJournalHostErase(sector); }
void programPages(const uint8_t* address, const uint8_t* data, size_t length) {
  settingsJournalHostProgram(address, data, length);
}
#else
const uint8_t* targetBytes(uint32_t target) {
  switch (static_cast<SettingsJournalTarget>(target)) {
    case SettingsJournalTarget::kEeprom:
      return reinterpret_cast<const uint8_t*>(reflexEepromSectorAddress());
    case SettingsJournalTarget::kSharedProfiles:
      return reinterpret_cast<const uint8_t*>(reflexBluetoothBondFlashSectorAddress());
  }
  return nullptr;
}
const uint8_t* dataBytes() {
  return reinterpret_cast<const uint8_t*>(reflexSettingsJournalDataSectorAddress());
}
const uint8_t* controlBytes() {
  return reinterpret_cast<const uint8_t*>(reflexSettingsJournalControlSectorAddress());
}
// A save from the USB soft IRQ can only land between windows. Copy, seal and
// rewrite share one window, so it can never see or change a save half done.
uint32_t openFlashWindow() {
  const uint32_t irqState = save_and_disable_interrupts();
  rp2040.idleOtherCore();
  return irqState;
}
void closeFlashWindow(uint32_t irqState) {
  rp2040.resumeOtherCore();
  restore_interrupts(irqState);
}
void eraseSector(const uint8_t* sector) {
  flash_range_erase(reflexFlashOffset(reinterpret_cast<uintptr_t>(sector)), kSectorSize);
}
void programPages(const uint8_t* address, const uint8_t* data, size_t length) {
  flash_range_program(reflexFlashOffset(reinterpret_cast<uintptr_t>(address)), data, length);
}
#endif

bool validImageLength(size_t length) {
  return length != 0 && length <= kSectorSize && length % kPageSize == 0;
}

// FNV-1a. It only has to tell a sealed copy from anything else in the sector.
uint32_t hashBytes(const uint8_t* bytes, size_t length) {
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < length; ++i) {
    hash = (hash ^ bytes[i]) * 16777619u;
  }
  return hash;
}

uint32_t hashControl(const JournalControl& control) {
  return hashBytes(reinterpret_cast<const uint8_t*>(&control),
                   offsetof(JournalControl, control_hash));
}

bool sectorErased(const uint8_t* sector) {
  for (size_t i = 0; i < kSectorSize; ++i) {
    if (sector[i] != 0xFF) return false;
  }
  return true;
}

void clearSector(const uint8_t* sector) {
  if (sectorErased(sector)) return;
  const uint32_t window = openFlashWindow();
  eraseSector(sector);
  closeFlashWindow(window);
}

// Between saves both journal sectors stay erased, so the window that does the
// work only adds the time to program them.
void clearJournal() {
  clearSector(controlBytes());
  clearSector(dataBytes());
}

// Returns the sector a sealed copy belongs to, or nullptr if there is none.
const uint8_t* sealedTarget(JournalControl& control) {
  memcpy(&control, controlBytes(), sizeof(control));
  if (control.magic != kControlMagic || control.control_hash != hashControl(control) ||
      !validImageLength(control.length) ||
      control.image_hash != hashBytes(dataBytes(), control.length)) {
    return nullptr;
  }
  return targetBytes(control.target);
}

// False only if a sealed save could not be finished. Its record stays then,
// because that copy is the only good one of the sector.
bool finishSealedSave() {
  JournalControl control{};
  const uint8_t* target = sealedTarget(control);
  if (target == nullptr || memcmp(target, dataBytes(), control.length) == 0) return true;
  alignas(4) uint8_t page[kPageSize];
  const uint32_t window = openFlashWindow();
  eraseSector(target);
  for (size_t offset = 0; offset < control.length; offset += kPageSize) {
    memcpy(page, dataBytes() + offset, kPageSize);
    programPages(target + offset, page, kPageSize);
  }
  closeFlashWindow(window);
  return memcmp(target, dataBytes(), control.length) == 0;
}

bool sealAndRewrite(SettingsJournalTarget target, const uint8_t* targetSector,
                    const uint8_t* image, size_t length) {
  programPages(dataBytes(), image, length);
  if (memcmp(dataBytes(), image, length) != 0) return false;

  JournalControl control = {};
  control.magic = kControlMagic;
  control.target = static_cast<uint32_t>(target);
  control.length = static_cast<uint32_t>(length);
  control.image_hash = hashBytes(dataBytes(), length);
  control.control_hash = hashControl(control);
  alignas(4) uint8_t page[kPageSize];
  memset(page, 0xFF, sizeof(page));
  memcpy(page, &control, sizeof(control));
  programPages(controlBytes(), page, sizeof(page));
  if (memcmp(controlBytes(), page, sizeof(page)) != 0) return false;

  eraseSector(targetSector);
  programPages(targetSector, image, length);
  return memcmp(targetSector, image, length) == 0;
}

}  // namespace

void settingsJournalRecover() {
  if (finishSealedSave()) clearJournal();
}

bool settingsJournalWriteSector(SettingsJournalTarget target, const uint8_t* image,
                                size_t length) {
  const uint8_t* targetSector = targetBytes(static_cast<uint32_t>(target));
  if (targetSector == nullptr || !validImageLength(length)) return false;
  if (!finishSealedSave()) return false;
  if (memcmp(targetSector, image, length) == 0) return true;

  clearJournal();
  const uint32_t window = openFlashWindow();
  const bool saved = sealAndRewrite(target, targetSector, image, length);
  closeFlashWindow(window);
  if (saved) clearJournal();
  return saved;
}

#if !defined(REFLEX_HOST_TEST_SETTINGS_JOURNAL)
bool settingsJournalCommitEeprom() {
  return settingsJournalWriteSector(SettingsJournalTarget::kEeprom,
                                    EEPROM.getConstDataPtr(), EEPROM.length());
}
#endif

#endif
