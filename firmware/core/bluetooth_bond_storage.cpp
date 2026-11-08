#include "bluetooth_bond_storage.h"

#include <Arduino.h>
#include <EEPROM.h>
#include <string.h>

#include "eeprom_helper.h"
#include "button_chord_remap.h"
#include "hotkey_combo.h"
#include "settings_store.h"
#include "settings_layout.h"

#if defined(ARDUINO_ARCH_RP2040) && \
    !defined(REFLEX_HOST_TEST_EEPROM_BONDS)
#include "reserved_flash_layout.h"
#include "shared_profile_flash_storage.h"

#define REFLEX_BT_BOND_RAW_FLASH 1
#endif

namespace {

constexpr uint16_t kPersistedBlockMagicBluetoothBondLegacy = 0x4254;  // BT
constexpr uint16_t kPersistedBlockMagicBluetoothBondListV1 = 0x424C;  // BL
constexpr uint16_t kPersistedBlockMagicBluetoothBondList = 0x4232;    // B2
constexpr uint16_t kBluetoothBondListRecordSize =
  PERSISTED_BLOCK_HEADER_SIZE + sizeof(BluetoothBondListRecord);

struct LegacyBluetoothBondSlot {
  uint8_t addr[6];
  uint8_t link_key[16];
  uint8_t key_type;
  uint8_t class_of_device[3];
  uint8_t order;
  uint8_t flags;
};

static_assert(sizeof(LegacyBluetoothBondSlot) == 28,
              "LegacyBluetoothBondSlot size changed unexpectedly");

struct LegacyBluetoothBondListRecord {
  LegacyBluetoothBondSlot slots[BLUETOOTH_BOND_SLOT_COUNT];
};

constexpr uint16_t kLegacyBluetoothBondListRecordSize =
  PERSISTED_BLOCK_HEADER_SIZE + sizeof(LegacyBluetoothBondListRecord);

struct LegacyBluetoothBondRecord {
  uint8_t addr[6];
  uint8_t link_key[16];
  uint8_t key_type;
  uint8_t class_of_device[3];
  uint8_t reserved[2];
};

static_assert(sizeof(LegacyBluetoothBondRecord) == 28,
              "LegacyBluetoothBondRecord size changed unexpectedly");

constexpr uint16_t kLegacyBluetoothBondRecordSize =
  PERSISTED_BLOCK_HEADER_SIZE + sizeof(LegacyBluetoothBondRecord);

#if defined(REFLEX_BT_BOND_RAW_FLASH)
static_assert((PERSISTED_AB_SLOT_COUNT * kBluetoothBondListRecordSize) <=
                  SHARED_PROFILE_FLASH_SECTOR_SIZE,
              "Bluetooth bond records must fit in one reserved flash sector");

const uint8_t* bluetoothBondFlashSector() {
  return sharedProfileFlashData();
}

const uint8_t* legacyBluetoothBondFlashSector() {
  // Before the three-sector layout, bonds lived one sector below EEPROM. That
  // address is now reserved for auth so only read it during migration.
  return reinterpret_cast<const uint8_t*>(
      reflexLegacyAuthFlashSectorAddress());
}

const uint8_t* previousBluetoothBondFlashSector() {
  return reinterpret_cast<const uint8_t*>(
      reflexLegacyBluetoothBondFlashSectorAddress());
}
#endif

uint16_t bluetoothBondRecordBase() {
  return buttonChordStorageRequiredEnd();
}

bool isValidSlot(const BluetoothBondSlot& slot) {
  return (slot.flags & BLUETOOTH_BOND_FLAG_VALID) != 0;
}

bool isValidLegacySlot(const LegacyBluetoothBondSlot& slot) {
  return (slot.flags & BLUETOOTH_BOND_FLAG_VALID) != 0;
}

void sanitizeBondList(BluetoothBondListRecord& bonds) {
  for (uint8_t i = 0; i < BLUETOOTH_BOND_SLOT_COUNT; ++i) {
    if (!isValidSlot(bonds.slots[i])) {
      memset(&bonds.slots[i], 0, sizeof(bonds.slots[i]));
      bonds.slots[i].order = 0xFF;
      continue;
    }
    if (bonds.slots[i].controller_type > BLUETOOTH_CONTROLLER_TYPE_MAX_KNOWN) {
      bonds.slots[i].controller_type = BLUETOOTH_CONTROLLER_TYPE_UNKNOWN;
    }
    const uint8_t flags = bonds.slots[i].flags & (BLUETOOTH_BOND_FLAG_VALID | BLUETOOTH_BOND_FLAG_LE);
    bonds.slots[i].flags = flags | BLUETOOTH_BOND_FLAG_VALID;
    if ((bonds.slots[i].flags & BLUETOOTH_BOND_FLAG_LE) == 0) {
      bonds.slots[i].reserved = 0;
    } else {
      bonds.slots[i].reserved &= 0x01;
      bonds.slots[i].key_type = BLUETOOTH_BOND_KEY_TYPE_NONE;
      memset(bonds.slots[i].link_key, 0, sizeof(bonds.slots[i].link_key));
    }
  }

  bool assigned[BLUETOOTH_BOND_SLOT_COUNT] = {};
  uint8_t compactOrder = 0;
  for (uint8_t pass = 0; pass < BLUETOOTH_BOND_SLOT_COUNT; ++pass) {
    uint8_t bestSlot = BLUETOOTH_BOND_SLOT_NONE;
    uint8_t bestOrder = 0xFF;
    for (uint8_t i = 0; i < BLUETOOTH_BOND_SLOT_COUNT; ++i) {
      if (!isValidSlot(bonds.slots[i]) || assigned[i]) {
        continue;
      }
      if (bonds.slots[i].order < bestOrder) {
        bestOrder = bonds.slots[i].order;
        bestSlot = i;
      }
    }
    if (bestSlot == BLUETOOTH_BOND_SLOT_NONE) {
      continue;
    }
    assigned[bestSlot] = true;
    bonds.slots[bestSlot].order = compactOrder++;
  }
}

bool readLegacyBond(LegacyBluetoothBondRecord& bond) {
  uint8_t slot = 0;
  uint16_t generation = 0;
  return readLatestPersistedRecordAB(
    bluetoothBondRecordBase(),
    kLegacyBluetoothBondRecordSize,
    kPersistedBlockMagicBluetoothBondLegacy,
    slot,
    generation,
    bond
  );
}

bool readEepromBondList(BluetoothBondListRecord& bonds) {
  uint8_t slot = 0;
  uint16_t generation = 0;
  if (!readLatestPersistedRecordAB(
        bluetoothBondRecordBase(),
        kBluetoothBondListRecordSize,
        kPersistedBlockMagicBluetoothBondList,
        slot,
        generation,
        bonds)) {
    return false;
  }

  sanitizeBondList(bonds);
  return true;
}

#if defined(REFLEX_BT_BOND_RAW_FLASH)
bool readFlashPersistedBondRecordFrom(const uint8_t* sector,
                                  uint8_t candidate,
                                  BluetoothBondListRecord& value,
                                  uint16_t* generation) {
  if (candidate >= PERSISTED_AB_SLOT_COUNT) {
    return false;
  }

  const uint32_t recordOffset = (uint32_t)candidate * kBluetoothBondListRecordSize;
  const uint8_t* record = sector + recordOffset;

  PersistedBlockHeader header{};
  memcpy(&header, record, sizeof(header));
  if (!isPersistedBlockHeaderValid(
        header,
        kPersistedBlockMagicBluetoothBondList,
        (uint8_t)sizeof(BluetoothBondListRecord))) {
    return false;
  }

  memcpy(&value, record + PERSISTED_BLOCK_HEADER_SIZE, sizeof(value));
  if (header.payload_crc16 != calculateEepromBlockCrc(value)) {
    return false;
  }

  if (generation) {
    *generation = header.generation;
  }
  return true;
}

bool readLegacyFlashBondList(BluetoothBondListRecord& bonds) {
  BluetoothBondListRecord slotValue[PERSISTED_AB_SLOT_COUNT] = {};
  uint16_t slotGeneration[PERSISTED_AB_SLOT_COUNT] = {};
  bool valid[PERSISTED_AB_SLOT_COUNT] = {};
  for (uint8_t candidate = 0; candidate < PERSISTED_AB_SLOT_COUNT; ++candidate) {
    valid[candidate] = readFlashPersistedBondRecordFrom(
        legacyBluetoothBondFlashSector(), candidate, slotValue[candidate],
        &slotGeneration[candidate]);
  }
  if (!valid[0] && !valid[1]) {
    return false;
  }
  uint8_t slot = !valid[0] ? 1 : 0;
  if (valid[0] && valid[1] &&
      isGenerationNewer(slotGeneration[1], slotGeneration[0])) {
    slot = 1;
  }
  bonds = slotValue[slot];
  sanitizeBondList(bonds);
  return true;
}

bool readFlashBondListFrom(const uint8_t* sector,
                           BluetoothBondListRecord& bonds,
                           uint8_t* activeSlot = nullptr,
                           uint16_t* activeGeneration = nullptr) {
  BluetoothBondListRecord slotValue[PERSISTED_AB_SLOT_COUNT] = {};
  uint16_t slotGeneration[PERSISTED_AB_SLOT_COUNT] = {};
  bool valid[PERSISTED_AB_SLOT_COUNT] = {};

  for (uint8_t candidate = 0; candidate < PERSISTED_AB_SLOT_COUNT; ++candidate) {
    valid[candidate] = readFlashPersistedBondRecordFrom(
        sector, candidate, slotValue[candidate], &slotGeneration[candidate]);
  }

  if (!valid[0] && !valid[1]) {
    return false;
  }

  uint8_t slot = 0;
  if (!valid[0]) {
    slot = 1;
  } else if (valid[1] && isGenerationNewer(slotGeneration[1], slotGeneration[0])) {
    slot = 1;
  }

  bonds = slotValue[slot];
  sanitizeBondList(bonds);
  if (activeSlot) {
    *activeSlot = slot;
  }
  if (activeGeneration) {
    *activeGeneration = slotGeneration[slot];
  }
  return true;
}

bool readFlashBondList(BluetoothBondListRecord& bonds,
                       uint8_t* activeSlot = nullptr,
                       uint16_t* activeGeneration = nullptr) {
  return readFlashBondListFrom(
      bluetoothBondFlashSector(), bonds, activeSlot, activeGeneration);
}

bool writeFlashBondRecord(uint8_t targetSlot,
                          uint16_t nextGeneration,
                          const BluetoothBondListRecord& bonds) {
  uint8_t* sector = nullptr;
  if (!sharedProfileFlashBeginEdit(sector)) return false;
  const uint32_t recordOffset = (uint32_t)targetSlot * kBluetoothBondListRecordSize;
  memset(sector + recordOffset, 0xFF, kBluetoothBondListRecordSize);
  memcpy(
    sector + recordOffset + PERSISTED_BLOCK_HEADER_SIZE,
    &bonds,
    sizeof(bonds)
  );

  const PersistedBlockHeader header = buildPersistedBlockHeader(
    kPersistedBlockMagicBluetoothBondList,
    (uint8_t)sizeof(BluetoothBondListRecord),
    nextGeneration,
    calculateEepromBlockCrc(bonds)
  );
  memcpy(sector + recordOffset, &header, sizeof(header));
  return sharedProfileFlashCommitEdit();
}

bool writeFlashBondList(const BluetoothBondListRecord& bonds) {
  uint8_t currentSlot = 0;
  uint16_t currentGeneration = 0;
  BluetoothBondListRecord currentValue{};
  const bool hasCurrent = readFlashBondList(currentValue, &currentSlot, &currentGeneration);

  if (hasCurrent && memcmp(&currentValue, &bonds, sizeof(bonds)) == 0) {
    return true;
  }

  const uint8_t targetSlot = hasCurrent ? (uint8_t)(currentSlot ^ 0x01u) : 0;
  const uint16_t nextGeneration = hasCurrent ? (uint16_t)(currentGeneration + 1u) : 1u;
  if (!writeFlashBondRecord(targetSlot, nextGeneration, bonds)) return false;

  BluetoothBondListRecord verify{};
  return readFlashBondList(verify) && memcmp(&verify, &bonds, sizeof(bonds)) == 0;
}

#endif

bool migrateLegacyBond(BluetoothBondListRecord& bonds) {
  LegacyBluetoothBondRecord legacy{};
  if (!readLegacyBond(legacy)) {
    return false;
  }

  memset(&bonds, 0, sizeof(bonds));
  BluetoothBondSlot& slot = bonds.slots[0];
  memcpy(slot.addr, legacy.addr, sizeof(slot.addr));
  memcpy(slot.link_key, legacy.link_key, sizeof(slot.link_key));
  slot.key_type = legacy.key_type;
  memcpy(slot.class_of_device, legacy.class_of_device, sizeof(slot.class_of_device));
  slot.controller_type = BLUETOOTH_CONTROLLER_TYPE_UNKNOWN;
  slot.order = 0;
  slot.flags = BLUETOOTH_BOND_FLAG_VALID;
  bluetoothBondWriteList(bonds);
  return true;
}

bool migrateEepromBondList(BluetoothBondListRecord& bonds) {
  if (!readEepromBondList(bonds)) {
    return false;
  }

  bluetoothBondWriteList(bonds);
  return true;
}

bool migrateLegacyBondList(BluetoothBondListRecord& bonds) {
  LegacyBluetoothBondListRecord legacy{};
  uint8_t slot = 0;
  uint16_t generation = 0;
  if (!readLatestPersistedRecordAB(
        bluetoothBondRecordBase(),
        kLegacyBluetoothBondListRecordSize,
        kPersistedBlockMagicBluetoothBondListV1,
        slot,
        generation,
        legacy)) {
    return false;
  }

  memset(&bonds, 0, sizeof(bonds));
  for (uint8_t i = 0; i < BLUETOOTH_BOND_SLOT_COUNT; ++i) {
    if (!isValidLegacySlot(legacy.slots[i])) {
      continue;
    }
    BluetoothBondSlot& next = bonds.slots[i];
    memcpy(next.addr, legacy.slots[i].addr, sizeof(next.addr));
    memcpy(next.link_key, legacy.slots[i].link_key, sizeof(next.link_key));
    next.key_type = legacy.slots[i].key_type;
    memcpy(next.class_of_device, legacy.slots[i].class_of_device, sizeof(next.class_of_device));
    next.controller_type = BLUETOOTH_CONTROLLER_TYPE_UNKNOWN;
    next.order = legacy.slots[i].order;
    next.flags = BLUETOOTH_BOND_FLAG_VALID;
  }
  sanitizeBondList(bonds);
  bluetoothBondWriteList(bonds);
  return true;
}

uint8_t findSlotByAddress(const BluetoothBondListRecord& bonds, const uint8_t addr[6]) {
  if (!addr) {
    return BLUETOOTH_BOND_SLOT_NONE;
  }
  for (uint8_t i = 0; i < BLUETOOTH_BOND_SLOT_COUNT; ++i) {
    if (isValidSlot(bonds.slots[i]) &&
        memcmp(bonds.slots[i].addr, addr, sizeof(bonds.slots[i].addr)) == 0) {
      return i;
    }
  }
  return BLUETOOTH_BOND_SLOT_NONE;
}

uint8_t findEmptyOrOldestSlot(const BluetoothBondListRecord& bonds) {
  uint8_t oldestSlot = 0;
  uint8_t oldestOrder = 0;
  for (uint8_t i = 0; i < BLUETOOTH_BOND_SLOT_COUNT; ++i) {
    if (!isValidSlot(bonds.slots[i])) {
      return i;
    }
    if (bonds.slots[i].order >= oldestOrder) {
      oldestOrder = bonds.slots[i].order;
      oldestSlot = i;
    }
  }
  return oldestSlot;
}

void markSlotMostRecent(BluetoothBondListRecord& bonds, uint8_t mostRecentSlot) {
  for (uint8_t i = 0; i < BLUETOOTH_BOND_SLOT_COUNT; ++i) {
    if (!isValidSlot(bonds.slots[i]) || i == mostRecentSlot) {
      continue;
    }
    if (bonds.slots[i].order < 0xFE) {
      bonds.slots[i].order++;
    }
  }
  bonds.slots[mostRecentSlot].order = 0;
  bonds.slots[mostRecentSlot].flags =
      (bonds.slots[mostRecentSlot].flags & BLUETOOTH_BOND_FLAG_LE) |
      BLUETOOTH_BOND_FLAG_VALID;
}

}  // namespace

uint16_t bluetoothBondStorageRequiredEnd() {
#if defined(REFLEX_BT_BOND_RAW_FLASH)
  return bluetoothBondRecordBase();
#else
  return bluetoothBondRecordBase() +
         (PERSISTED_AB_SLOT_COUNT * kBluetoothBondListRecordSize);
#endif
}

uint16_t bluetoothBondEepromOccupiedEnd() {
#if defined(REFLEX_BT_BOND_RAW_FLASH)
  // Only the legacy EEPROM records remain, for migration and clearing. Bytes
  // past the 4 KiB window were never persisted and are not addressable.
  const uint16_t base = bluetoothBondRecordBase();
  if (base >= ADAPT_EEPROM_EMULATION_SIZE) {
    return 0;
  }
  const uint32_t end =
      (uint32_t)base + PERSISTED_AB_SLOT_COUNT * kBluetoothBondListRecordSize;
  return (uint16_t)(end < ADAPT_EEPROM_EMULATION_SIZE
                        ? end
                        : ADAPT_EEPROM_EMULATION_SIZE);
#else
  return bluetoothBondStorageRequiredEnd();
#endif
}

bool bluetoothBondReadList(BluetoothBondListRecord& bonds) {
#if defined(REFLEX_BT_BOND_RAW_FLASH)
  if (readFlashBondList(bonds)) {
    return true;
  }

#if ADAPT_POLICY_FIXED_END_RESERVED_FLASH_LAYOUT
  if (readFlashBondListFrom(previousBluetoothBondFlashSector(), bonds)) {
    return writeFlashBondList(bonds);
  }
#endif

  if (readLegacyFlashBondList(bonds)) {
    return writeFlashBondList(bonds);
  }

  if (migrateEepromBondList(bonds)) {
    return true;
  }
#else
  uint8_t slot = 0;
  uint16_t generation = 0;
  if (readLatestPersistedRecordAB(
        bluetoothBondRecordBase(),
        kBluetoothBondListRecordSize,
        kPersistedBlockMagicBluetoothBondList,
        slot,
        generation,
        bonds)) {
    sanitizeBondList(bonds);
    return true;
  }
#endif

  if (migrateLegacyBondList(bonds)) {
    return true;
  }

  if (migrateLegacyBond(bonds)) {
    return true;
  }

  memset(&bonds, 0, sizeof(bonds));
  sanitizeBondList(bonds);
  return false;
}

bool bluetoothBondUpdaterStorageReady() {
#if defined(REFLEX_BT_BOND_RAW_FLASH) && \
    ADAPT_POLICY_FIXED_END_RESERVED_FLASH_LAYOUT
  BluetoothBondListRecord current{};
  if (readFlashBondList(current)) return true;

  BluetoothBondListRecord legacy{};
  const bool hasLegacy =
      readFlashBondListFrom(previousBluetoothBondFlashSector(), legacy) ||
      readLegacyFlashBondList(legacy);
  if (!hasLegacy) return true;
  if (!writeFlashBondList(legacy)) return false;

  BluetoothBondListRecord verify{};
  return readFlashBondList(verify) &&
         memcmp(&verify, &legacy, sizeof(verify)) == 0;
#else
  return true;
#endif
}

bool bluetoothBondWriteList(const BluetoothBondListRecord& bonds) {
  BluetoothBondListRecord sanitized = bonds;
  sanitizeBondList(sanitized);
#if defined(REFLEX_BT_BOND_RAW_FLASH)
  return writeFlashBondList(sanitized);
#else
  return writePersistedRecordAB(
    bluetoothBondRecordBase(),
    kBluetoothBondListRecordSize,
    kPersistedBlockMagicBluetoothBondList,
    sanitized
  );
#endif
}

bool bluetoothBondUpsert(const BluetoothBondSlot& bond, uint8_t* storedSlot) {
  BluetoothBondListRecord bonds{};
  bluetoothBondReadList(bonds);

  uint8_t targetSlot = findSlotByAddress(bonds, bond.addr);
  if (targetSlot == BLUETOOTH_BOND_SLOT_NONE) {
    targetSlot = findEmptyOrOldestSlot(bonds);
  }

  bonds.slots[targetSlot] = bond;
  bonds.slots[targetSlot].flags =
      (bond.flags & BLUETOOTH_BOND_FLAG_LE) | BLUETOOTH_BOND_FLAG_VALID;
  markSlotMostRecent(bonds, targetSlot);
  const bool wrote = bluetoothBondWriteList(bonds);
  if (storedSlot) {
    *storedSlot = targetSlot;
  }
  return wrote;
}

bool bluetoothBondFindByAddress(const uint8_t addr[6], BluetoothBondSlot& bond, uint8_t* slotIndex) {
  BluetoothBondListRecord bonds{};
  if (!bluetoothBondReadList(bonds)) {
    return false;
  }
  const uint8_t slot = findSlotByAddress(bonds, addr);
  if (slot == BLUETOOTH_BOND_SLOT_NONE) {
    return false;
  }
  bond = bonds.slots[slot];
  if (slotIndex) {
    *slotIndex = slot;
  }
  return true;
}

uint8_t bluetoothBondValidCount(const BluetoothBondListRecord& bonds) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < BLUETOOTH_BOND_SLOT_COUNT; ++i) {
    if (isValidSlot(bonds.slots[i])) {
      count++;
    }
  }
  return count;
}

bool bluetoothBondClear() {
  constexpr uint8_t kClearAttempts = 3;
  bool rawFlashCleared = true;
#if defined(REFLEX_BT_BOND_RAW_FLASH)
  BluetoothBondListRecord empty{};
  sanitizeBondList(empty);
  // A valid empty record is also the migration tombstone. Erasing only the
  // new sector would make the still-valid legacy sector resurrect old bonds
  // on the next read.
  rawFlashCleared = false;
  for (uint8_t attempt = 0; attempt < kClearAttempts; ++attempt) {
    if (writeFlashBondList(empty)) {
      BluetoothBondListRecord verify{};
      rawFlashCleared = readFlashBondList(verify) &&
                        bluetoothBondValidCount(verify) == 0;
      if (rawFlashCleared) break;
    }
  }
#endif

  const uint16_t legacyBase = bluetoothBondRecordBase();
  uint16_t legacyLength =
      PERSISTED_AB_SLOT_COUNT * kBluetoothBondListRecordSize;
  // Clear only the addressable part: EEPROM ignores writes past its window and
  // reads them back as 0, which made every factory reset report failure.
  const size_t eepromLength = EEPROM.length();
  if (legacyBase >= eepromLength) {
    legacyLength = 0;
  } else if (legacyBase + legacyLength > eepromLength) {
    legacyLength = (uint16_t)(eepromLength - legacyBase);
  }
  if (legacyLength == 0) {
    return rawFlashCleared;
  }
  bool legacyEepromCleared = false;
  for (uint8_t attempt = 0; attempt < kClearAttempts; ++attempt) {
    EepromTransaction txn;
    stageEepromRangeFill(txn, legacyBase, legacyLength, 0xFF);
    if (!txn.commit()) continue;

    legacyEepromCleared = true;
    for (uint16_t offset = 0; offset < legacyLength; ++offset) {
      if (EEPROM.read(legacyBase + offset) != 0xFF) {
        legacyEepromCleared = false;
        break;
      }
    }
    if (legacyEepromCleared) break;
  }
  return rawFlashCleared && legacyEepromCleared;
}
