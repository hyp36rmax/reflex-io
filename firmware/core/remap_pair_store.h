#pragma once

#include <stdint.h>

#include "remap_table.h"

// Button remaps belong to an (input mode, output mode) pair. Only pairs that
// differ from the identity map are stored; an absent pair is the identity map.
// The Mouse output settings (rotary_mouse_settings.h) use the same entries
// under a reserved output key, one per input mode.
// They live in the free tail of the shared raw-flash profile sector, after the
// Bluetooth bond and MODERN2USB hotkey records, so no existing store moves.
// Edits are staged in RAM and published through the configuration
// persistence service, like the other raw stores.
//
// Layout 1 holds the 19 button entries. Products with D-pad remap rows use
// layout 2, which adds the four directions. Layout 2 firmware reads a layout 1
// region when no layout 2 entry validates (directions stay themselves) and
// the next save rewrites it as layout 2; older firmware finds no valid entry
// in a layout 2 region, so a downgrade loses the stored pairs.
#if defined(ADAPT_CAP_DPAD_REMAP)
constexpr uint8_t REMAP_PAIR_LAYOUT = 2;
#else
constexpr uint8_t REMAP_PAIR_LAYOUT = 1;
#endif
constexpr uint8_t REMAP_PAIR_BUTTONS = REMAP_MAX_BUTTONS;
constexpr uint16_t REMAP_PAIR_REGION_OFFSET = 2688;
constexpr uint16_t REMAP_PAIR_REGION_END = 4096;
// input, output, the remap bytes, CRC16.
constexpr uint8_t REMAP_PAIR_ENTRY_SIZE = 2 + REMAP_PAIR_BUTTONS + 2;
constexpr uint8_t REMAP_PAIR_CAPACITY =
    (REMAP_PAIR_REGION_END - REMAP_PAIR_REGION_OFFSET) / REMAP_PAIR_ENTRY_SIZE;

// True and fills remaps when the pair has a stored (non-identity) map.
bool remapPairRead(uint8_t input, uint8_t output, uint8_t* remaps);
// Stores the pair; an identity map removes it. False when the store is full.
bool remapPairWrite(uint8_t input, uint8_t output, const uint8_t* remaps);
// True when remapPairWrite() of these remaps would succeed.
bool remapPairCanWrite(uint8_t input, uint8_t output, const uint8_t* remaps);
void remapPairEraseAll();
uint8_t remapPairStoredCount();

void remapPairPersistenceService();
bool remapPairPersistenceFlushNow();
void remapPairPersistenceDiscardDeferred();
bool remapPairPersistenceHasPending();

#if defined(REFLEX_HOST_TEST_SHARED_PROFILE_FLASH)
void remapPairHostReset();
#endif
