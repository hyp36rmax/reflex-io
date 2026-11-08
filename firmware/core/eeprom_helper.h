#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <EEPROM.h>

#include "settings_journal.h"
#include "settings_layout.h"

template <typename T>
inline bool eeprom_value_matches(uint16_t address, const T& value) {
  T current{};
  EEPROM.get(address, current);
  return memcmp(&current, &value, sizeof(T)) == 0;
}

inline bool eeprom_byte_matches(uint16_t address, uint8_t value) {
  return EEPROM.read(address) == value;
}

inline bool& eeprom_commit_retry_pending() {
  static bool pending = false;
  return pending;
}

// Every save goes through here, so a product with the journal protects them all.
// The journal can fail where EEPROM.commit() could not, so a failure is latched
// here for the callers that don't check it.
inline bool persistentEepromCommit() {
#if defined(ADAPT_SETTINGS_JOURNAL_ACTIVE)
  const bool committed = settingsJournalCommitEeprom();
  eeprom_commit_retry_pending() = !committed;
  return committed;
#else
  return EEPROM.commit();
#endif
}

struct EepromTransaction {
  bool dirty;

  EepromTransaction() : dirty(eeprom_commit_retry_pending()) {}

  inline void write(uint16_t address, uint8_t value) {
    if (!eeprom_byte_matches(address, value)) {
      EEPROM.write(address, value);
      dirty = true;
    }
  }

  template <typename T>
  inline void put(uint16_t address, const T& value) {
    if (!eeprom_value_matches(address, value)) {
      EEPROM.put(address, value);
      dirty = true;
    }
  }

  inline bool commit() {
    if (!dirty) {
      return true;
    }
    if (!persistentEepromCommit()) {
      eeprom_commit_retry_pending() = true;
      dirty = true;
      return false;
    }
    eeprom_commit_retry_pending() = false;
    dirty = false;
    return true;
  }
};

inline uint16_t calculateEepromBlockCrc(const uint8_t* bytes, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; ++i) {
    crc ^= (uint16_t)bytes[i] << 8;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
  }
  return crc;
}

template <typename T>
inline uint16_t calculateEepromBlockCrc(const T& value) {
  return calculateEepromBlockCrc(reinterpret_cast<const uint8_t*>(&value), sizeof(T));
}

inline uint16_t calculatePersistedHeaderCrc(const PersistedBlockHeader& header) {
  PersistedBlockHeader temp = header;
  temp.header_crc16 = 0;
  return calculateEepromBlockCrc(
    reinterpret_cast<const uint8_t*>(&temp),
    offsetof(PersistedBlockHeader, header_crc16)
  );
}

inline PersistedBlockHeader buildPersistedBlockHeader(
    uint16_t magic,
    uint8_t payloadSize,
    uint16_t generation,
    uint16_t payloadCrc
) {
  PersistedBlockHeader header = {};
  header.magic = magic;
  header.schema_version = SETTINGS_SCHEMA_VERSION;
  header.payload_size = payloadSize;
  header.generation = generation;
  header.payload_crc16 = payloadCrc;
  header.header_crc16 = calculatePersistedHeaderCrc(header);
  return header;
}

inline bool isPersistedBlockHeaderValid(
    const PersistedBlockHeader& header,
    uint16_t magic,
    uint8_t payloadSize
) {
  return header.magic == magic &&
         header.schema_version == SETTINGS_SCHEMA_VERSION &&
         header.payload_size == payloadSize &&
         header.header_crc16 == calculatePersistedHeaderCrc(header);
}

inline bool isGenerationNewer(uint16_t lhs, uint16_t rhs) {
  return lhs != rhs && (uint16_t)(lhs - rhs) < 0x8000u;
}

template <typename T>
inline void stagePersistedRecord(
    EepromTransaction& txn,
    uint16_t recordAddress,
    uint16_t magic,
    const T& value,
    uint16_t generation
) {
  txn.put(recordAddress + PERSISTED_BLOCK_HEADER_SIZE, value);
  const PersistedBlockHeader header = buildPersistedBlockHeader(
    magic,
    (uint8_t)sizeof(T),
    generation,
    calculateEepromBlockCrc(value)
  );
  txn.put(recordAddress, header);
}

template <typename T>
inline void writePersistedRecord(
    uint16_t recordAddress,
    uint16_t magic,
    const T& value,
    uint16_t generation
) {
  EepromTransaction txn;
  stagePersistedRecord(txn, recordAddress, magic, value, generation);
  txn.commit();
}

template <typename T>
inline bool readPersistedRecord(
    uint16_t recordAddress,
    uint16_t magic,
    T& value,
    uint16_t* generation = nullptr
) {
  PersistedBlockHeader header{};
  EEPROM.get(recordAddress, header);
  if (!isPersistedBlockHeaderValid(header, magic, (uint8_t)sizeof(T))) {
    return false;
  }

  EEPROM.get(recordAddress + PERSISTED_BLOCK_HEADER_SIZE, value);
  if (header.payload_crc16 != calculateEepromBlockCrc(value)) {
    return false;
  }

  if (generation != nullptr) {
    *generation = header.generation;
  }
  return true;
}

template <typename T>
inline bool readLatestPersistedRecordAB(
    uint16_t recordBaseAddress,
    uint16_t recordSize,
    uint16_t magic,
    uint8_t& slot,
    uint16_t& generation,
    T& value
) {
  T slotValue[PERSISTED_AB_SLOT_COUNT] = {};
  uint16_t slotGeneration[PERSISTED_AB_SLOT_COUNT] = {};
  bool valid[PERSISTED_AB_SLOT_COUNT] = {false, false};

  for (uint8_t candidate = 0; candidate < PERSISTED_AB_SLOT_COUNT; ++candidate) {
    valid[candidate] = readPersistedRecord(
      recordBaseAddress + (candidate * recordSize),
      magic,
      slotValue[candidate],
      &slotGeneration[candidate]
    );
  }

  if (!valid[0] && !valid[1]) {
    return false;
  }

  slot = 0;
  if (!valid[0]) {
    slot = 1;
  } else if (valid[1] && isGenerationNewer(slotGeneration[1], slotGeneration[0])) {
    slot = 1;
  }

  generation = slotGeneration[slot];
  value = slotValue[slot];
  return true;
}

template <typename T>
inline bool stagePersistedRecordAB(
    EepromTransaction& txn,
    uint16_t recordBaseAddress,
    uint16_t recordSize,
    uint16_t magic,
    const T& value
) {
  uint8_t currentSlot = 0;
  uint16_t currentGeneration = 0;
  T currentValue{};
  const bool hasCurrent = readLatestPersistedRecordAB(
    recordBaseAddress,
    recordSize,
    magic,
    currentSlot,
    currentGeneration,
    currentValue
  );

  if (hasCurrent && memcmp(&currentValue, &value, sizeof(T)) == 0) {
    return false;
  }

  const uint8_t targetSlot = hasCurrent ? (uint8_t)(currentSlot ^ 0x01u) : 0;
  const uint16_t nextGeneration = hasCurrent ? (uint16_t)(currentGeneration + 1u) : 1u;
  stagePersistedRecord(txn, recordBaseAddress + (targetSlot * recordSize), magic, value, nextGeneration);
  return true;
}

template <typename T>
inline bool writePersistedRecordAB(
    uint16_t recordBaseAddress,
    uint16_t recordSize,
    uint16_t magic,
    const T& value
) {
  uint8_t currentSlot = 0;
  uint16_t currentGeneration = 0;
  T currentValue{};
  const bool hasCurrent = readLatestPersistedRecordAB(
    recordBaseAddress,
    recordSize,
    magic,
    currentSlot,
    currentGeneration,
    currentValue
  );

  if (hasCurrent && memcmp(&currentValue, &value, sizeof(T)) == 0) {
    return false;
  }

  const uint8_t targetSlot = hasCurrent ? (uint8_t)(currentSlot ^ 0x01u) : 0;
  const uint16_t nextGeneration = hasCurrent ? (uint16_t)(currentGeneration + 1u) : 1u;
  writePersistedRecord(recordBaseAddress + (targetSlot * recordSize), magic, value, nextGeneration);
  return true;
}

// Returns true when the requested value is already durable or when a changed
// inactive A/B slot commits and verifies. Unlike writePersistedRecordAB(), an
// unchanged record is success and a failed EEPROM.commit() is failure. The
// cross-transaction dirty latch makes a retry publish a prior failed RAM image
// even though its bytes already compare equal on the second attempt.
template <typename T>
inline bool ensurePersistedRecordAB(
    uint16_t recordBaseAddress,
    uint16_t recordSize,
    uint16_t magic,
    const T& value
) {
  EepromTransaction txn;
  (void)stagePersistedRecordAB(
      txn, recordBaseAddress, recordSize, magic, value);
  if (!txn.commit()) {
    return false;
  }

  uint8_t verifySlot = 0;
  uint16_t verifyGeneration = 0;
  T verify{};
  return readLatestPersistedRecordAB(
             recordBaseAddress,
             recordSize,
             magic,
             verifySlot,
             verifyGeneration,
             verify) &&
         memcmp(&verify, &value, sizeof(T)) == 0;
}

inline void stageEepromRangeFill(EepromTransaction& txn, uint16_t start, uint16_t length, uint8_t value) {
  for (uint16_t i = 0; i < length; ++i) {
    txn.write(start + i, value);
  }
}
