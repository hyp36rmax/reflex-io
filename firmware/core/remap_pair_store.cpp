#include "../product_config.h"
#include "remap_pair_store.h"

#include <Arduino.h>
#include <string.h>

#include "eeprom_helper.h"
#include "settings_persistence_deferred.h"
#include "settings_persistence_policy.h"
#include "shared_profile_flash_storage.h"

static_assert(REMAP_PAIR_REGION_END <= SHARED_PROFILE_FLASH_SECTOR_SIZE,
              "remap pairs exceed the shared raw-flash sector");
static_assert(REMAP_PAIR_CAPACITY == (REMAP_PAIR_LAYOUT == 2 ? 52 : 61),
              "remap pair capacity changed");

namespace {

constexpr uint8_t kEmpty = 0xFF;
// The layout 1 entry: input, output, 19 remap bytes, CRC16.
constexpr uint8_t kLayout1Buttons = 19;
constexpr uint8_t kLayout1EntrySize = 2 + kLayout1Buttons + 2;
constexpr uint8_t kLayout1Capacity =
    (REMAP_PAIR_REGION_END - REMAP_PAIR_REGION_OFFSET) / kLayout1EntrySize;

struct Entry {
  uint8_t input;
  uint8_t output;
  uint8_t remaps[REMAP_PAIR_BUTTONS];
};

Entry table[REMAP_PAIR_CAPACITY];
bool loaded = false;
bool dirty = false;
SettingsPersistenceSafeWindow persistenceWindow{};

// Seeded with the layout so erased, foreign or other-layout bytes never
// validate as a pair.
uint16_t entryCrc(const Entry& entry, uint8_t layout, uint8_t buttons) {
  uint8_t bytes[3 + 2 + REMAP_PAIR_BUTTONS];
  bytes[0] = 'R';
  bytes[1] = 'P';
  bytes[2] = layout;
  bytes[3] = entry.input;
  bytes[4] = entry.output;
  memcpy(bytes + 5, entry.remaps, buttons);
  return calculateEepromBlockCrc(bytes, 5 + buttons);
}

// Fills the table from one layout; true when any entry validated. Entries
// narrower than the table keep their directions as themselves.
bool loadLayout(const uint8_t* sector, uint8_t layout, uint8_t buttons, uint8_t capacity) {
  const uint8_t entrySize = 2 + buttons + 2;
  bool found = false;
  for (uint8_t slot = 0; slot < REMAP_PAIR_CAPACITY; ++slot) table[slot].input = kEmpty;
  for (uint8_t slot = 0, next = 0; slot < capacity && next < REMAP_PAIR_CAPACITY; ++slot) {
    const uint8_t* raw = sector + REMAP_PAIR_REGION_OFFSET + slot * entrySize;
    Entry candidate{};
    candidate.input = raw[0];
    candidate.output = raw[1];
    for (uint8_t i = 0; i < REMAP_PAIR_BUTTONS; ++i) candidate.remaps[i] = i;
    memcpy(candidate.remaps, raw + 2, buttons);
    const uint16_t stored = (uint16_t)raw[2 + buttons] | ((uint16_t)raw[3 + buttons] << 8);
    if (candidate.input != kEmpty && stored == entryCrc(candidate, layout, buttons)) {
      table[next++] = candidate;
      found = true;
    }
  }
  return found;
}

bool isIdentity(const uint8_t* remaps) {
  for (uint8_t i = 0; i < REMAP_PAIR_BUTTONS; ++i) {
    if (remaps[i] != i) return false;
  }
  return true;
}

void ensureLoaded() {
  if (loaded) return;
  loaded = true;
  for (Entry& entry : table) entry.input = kEmpty;
  const uint8_t* sector = sharedProfileFlashData();
  if (sector == nullptr) return;
  if (loadLayout(sector, REMAP_PAIR_LAYOUT, REMAP_PAIR_BUTTONS, REMAP_PAIR_CAPACITY)) return;
  if (REMAP_PAIR_LAYOUT != 1) {
    // A layout 1 region from older firmware. Layout 1 holds more pairs than
    // layout 2; pairs beyond the table's capacity are not loaded.
    (void)loadLayout(sector, 1, kLayout1Buttons, kLayout1Capacity);
  }
}

int findSlot(uint8_t input, uint8_t output) {
  for (uint8_t slot = 0; slot < REMAP_PAIR_CAPACITY; ++slot) {
    if (table[slot].input == input && table[slot].output == output) return slot;
  }
  return -1;
}

void markDirty() {
  dirty = true;
  persistenceWindow.reset();
}

}  // namespace

bool remapPairRead(uint8_t input, uint8_t output, uint8_t* remaps) {
  ensureLoaded();
  const int slot = findSlot(input, output);
  if (slot < 0) return false;
  memcpy(remaps, table[slot].remaps, REMAP_PAIR_BUTTONS);
  return true;
}

bool remapPairWrite(uint8_t input, uint8_t output, const uint8_t* remaps) {
  if (input == kEmpty) return false;
  ensureLoaded();
  int slot = findSlot(input, output);
  if (isIdentity(remaps)) {
    if (slot >= 0) {
      table[slot].input = kEmpty;
      markDirty();
    }
    return true;
  }
  if (slot < 0) {
    for (uint8_t candidate = 0; candidate < REMAP_PAIR_CAPACITY; ++candidate) {
      if (table[candidate].input == kEmpty) {
        slot = candidate;
        break;
      }
    }
    if (slot < 0) return false;
  } else if (memcmp(table[slot].remaps, remaps, REMAP_PAIR_BUTTONS) == 0) {
    return true;
  }
  table[slot].input = input;
  table[slot].output = output;
  memcpy(table[slot].remaps, remaps, REMAP_PAIR_BUTTONS);
  markDirty();
  return true;
}

bool remapPairCanWrite(uint8_t input, uint8_t output, const uint8_t* remaps) {
  if (input == kEmpty) return false;
  ensureLoaded();
  if (isIdentity(remaps) || findSlot(input, output) >= 0) return true;
  for (const Entry& entry : table) {
    if (entry.input == kEmpty) return true;
  }
  return false;
}

void remapPairEraseAll() {
  ensureLoaded();
  for (Entry& entry : table) entry.input = kEmpty;
  markDirty();
}

uint8_t remapPairStoredCount() {
  ensureLoaded();
  uint8_t count = 0;
  for (const Entry& entry : table) {
    if (entry.input != kEmpty) ++count;
  }
  return count;
}

void remapPairPersistenceService() {
  if (persistenceWindow.shouldFlush(dirty, settingsPersistenceProtectedSessionActive(),
                                    millis())) {
    (void)remapPairPersistenceFlushNow();
  }
}

bool remapPairPersistenceFlushNow() {
  if (!dirty) {
    persistenceWindow.reset();
    return true;
  }
  // The shared sector editor preserves the bond and hotkey records.
  uint8_t* sector = nullptr;
  if (!sharedProfileFlashBeginEdit(sector)) return false;
  for (uint8_t slot = 0; slot < REMAP_PAIR_CAPACITY; ++slot) {
    uint8_t* raw = sector + REMAP_PAIR_REGION_OFFSET + slot * REMAP_PAIR_ENTRY_SIZE;
    const Entry& entry = table[slot];
    if (entry.input == kEmpty) {
      memset(raw, 0xFF, REMAP_PAIR_ENTRY_SIZE);
      continue;
    }
    raw[0] = entry.input;
    raw[1] = entry.output;
    memcpy(raw + 2, entry.remaps, REMAP_PAIR_BUTTONS);
    const uint16_t crc = entryCrc(entry, REMAP_PAIR_LAYOUT, REMAP_PAIR_BUTTONS);
    raw[2 + REMAP_PAIR_BUTTONS] = (uint8_t)(crc & 0xFF);
    raw[3 + REMAP_PAIR_BUTTONS] = (uint8_t)(crc >> 8);
  }
  if (!sharedProfileFlashCommitEdit()) {
    persistenceWindow.reset();
    return false;
  }
  dirty = false;
  persistenceWindow.reset();
  return true;
}

void remapPairPersistenceDiscardDeferred() {
  // Drop unpublished edits; the next read reloads the published table.
  loaded = false;
  dirty = false;
  persistenceWindow.reset();
}

bool remapPairPersistenceHasPending() {
  return dirty;
}

#if defined(REFLEX_HOST_TEST_SHARED_PROFILE_FLASH)
void remapPairHostReset() {
  loaded = false;
  dirty = false;
  persistenceWindow.reset();
}
#endif
