#pragma once

#include <stdint.h>
#include <string.h>

#include "../product_config.h"

// One remap table per input/output pair: entry i is the source slot i, its
// value the slot it becomes. Slots 0-18 are buttons. Classic controllers also
// remap their D-pad: slots 19-22 are Up, Down, Left and Right, which may stay
// themselves or become a button, never another direction.
#if defined(ADAPT_CAP_DPAD_REMAP)
#define REMAP_MAX_BUTTONS 23
#else
#define REMAP_MAX_BUTTONS 19
#endif

// The per-mode EEPROM record keeps only the 19 button entries (its remaps are
// the pre-2026-09-26 legacy copy); D-pad entries exist only in remap pairs.
constexpr uint8_t REMAP_RECORD_BUTTONS = 19;
constexpr uint8_t REMAP_DPAD_FIRST_SLOT = 19;
constexpr uint8_t REMAP_DPAD_SLOTS = REMAP_MAX_BUTTONS - REMAP_RECORD_BUTTONS;

inline constexpr bool remapSlotIsDpad(uint8_t slot) {
  return slot >= REMAP_DPAD_FIRST_SLOT && slot < REMAP_MAX_BUTTONS;
}

// Copies between a full table and the record's 19 button entries. A table
// filled from the record leaves every direction as itself.
inline void remapTableFromRecord(uint8_t (&table)[REMAP_MAX_BUTTONS],
                                 const uint8_t (&record)[REMAP_RECORD_BUTTONS]) {
  memcpy(table, record, REMAP_RECORD_BUTTONS);
  for (uint8_t i = REMAP_RECORD_BUTTONS; i < REMAP_MAX_BUTTONS; ++i) table[i] = i;
}

inline void remapTableToRecord(uint8_t (&record)[REMAP_RECORD_BUTTONS],
                               const uint8_t (&table)[REMAP_MAX_BUTTONS]) {
  memcpy(record, table, REMAP_RECORD_BUTTONS);
}

inline bool remapRecordIsIdentity(const uint8_t (&record)[REMAP_RECORD_BUTTONS]) {
  for (uint8_t i = 0; i < REMAP_RECORD_BUTTONS; ++i) {
    if (record[i] != i) return false;
  }
  return true;
}
