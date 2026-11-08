#include "button_chord_remap.h"
#include "controller_button_pressure.h"

#include <Arduino.h>
#include <string.h>

#include "../product_config.h"
#include "eeprom_helper.h"
#include "hotkey_combo.h"
#include "settings_layout.h"
#include "settings_persistence_deferred.h"

namespace {

constexpr uint16_t kPersistedBlockMagicButtonChords = 0x4348;  // CH
// Historical chained address. Products whose chain crosses the 4 KiB EEPROM
// window (authentication keys held in EEPROM) use the spare region instead;
// the chained end still positions the stores that follow it.
const uint16_t kButtonChordChainedBase = hotkeyStorageRequiredEnd();
constexpr uint16_t kButtonChordRecordSize =
  PERSISTED_BLOCK_HEADER_SIZE + sizeof(ButtonChordRemapSet);
constexpr uint16_t kButtonChordAbLength =
  PERSISTED_AB_SLOT_COUNT * kButtonChordRecordSize;
static_assert(SETTINGS_SPARE_BUTTON_CHORD_OFFSET + kButtonChordAbLength <=
                  SETTINGS_SPARE_DUAL_MERGE_OFFSET,
              "button chord record overlaps the spare dual-merge record");
const uint16_t kButtonChordRecordBase = eepromChainedOrSpareRecordBase(
    kButtonChordChainedBase, kButtonChordAbLength,
    SETTINGS_SPARE_BUTTON_CHORD_OFFSET);
constexpr uint32_t kChordDpadMask =
  INPUT_PAD_U | INPUT_PAD_D | INPUT_PAD_L | INPUT_PAD_R;

uint8_t countBits(uint32_t value) {
  uint8_t count = 0;
  while (value != 0) {
    value &= value - 1u;
    ++count;
  }
  return count;
}

uint32_t sanitizeChordMask(uint32_t combo) {
  return combo & BUTTON_CHORD_ALLOWED_BUTTONS;
}

void sanitizeChordRemaps(ButtonChordRemapSet& remaps) {
  for (uint8_t i = 0; i < BUTTON_CHORD_REMAP_SLOT_COUNT; ++i) {
    ButtonChordRemapSlot& slot = remaps.slots[i];
    slot.source_combo = sanitizeChordMask(slot.source_combo);
    slot.output_combo = sanitizeChordMask(slot.output_combo);
    if (!isValidButtonChordSource(slot.source_combo) ||
        !isValidButtonChordOutput(slot.output_combo)) {
      slot.source_combo = 0;
      slot.output_combo = 0;
    }
  }
}

uint32_t buttonsForChordFrame(const controller_state_t& frame) {
  uint32_t buttons = frame.digital_buttons;
  if (frame.PAD_U) {
    buttons |= INPUT_PAD_U;
  }
  if (frame.PAD_D) {
    buttons |= INPUT_PAD_D;
  }
  if (frame.PAD_L) {
    buttons |= INPUT_PAD_L;
  }
  if (frame.PAD_R) {
    buttons |= INPUT_PAD_R;
  }
  return sanitizeChordMask(buttons);
}

void clearButtonsFromFrame(controller_state_t& frame, uint32_t buttons) {
  setControllerPressureForMask(frame, buttons, 0);
  frame.digital_buttons &= ~(buttons & ~kChordDpadMask);
  if ((buttons & INPUT_PAD_U) != 0) {
    frame.PAD_U = 0;
  }
  if ((buttons & INPUT_PAD_D) != 0) {
    frame.PAD_D = 0;
  }
  if ((buttons & INPUT_PAD_L) != 0) {
    frame.PAD_L = 0;
  }
  if ((buttons & INPUT_PAD_R) != 0) {
    frame.PAD_R = 0;
  }
}

void setButtonsOnFrame(controller_state_t& frame, uint32_t buttons) {
  setControllerPressureForMask(frame, buttons, 255);
  frame.digital_buttons |= (buttons & ~kChordDpadMask);
  if ((buttons & INPUT_PAD_U) != 0) {
    frame.PAD_U = 1;
  }
  if ((buttons & INPUT_PAD_D) != 0) {
    frame.PAD_D = 1;
  }
  if ((buttons & INPUT_PAD_L) != 0) {
    frame.PAD_L = 1;
  }
  if ((buttons & INPUT_PAD_R) != 0) {
    frame.PAD_R = 1;
  }
}

bool writeButtonChordRemapsNow(const ButtonChordRemapSet& remaps) {
  return ensurePersistedRecordAB(
    kButtonChordRecordBase,
    kButtonChordRecordSize,
    kPersistedBlockMagicButtonChords,
    remaps
  );
}

}  // namespace

ButtonChordRemapSet active_chord_remaps = {};
bool active_chord_remaps_enabled = false;
#if defined(ADAPT_CAP_DEFERRED_SETTINGS_PERSISTENCE) || defined(REFLEX_HOST_TEST_DEFERRED_SETTINGS)
bool buttonChordRemapsPending = false;
SettingsPersistenceSafeWindow buttonChordPersistenceWindow{};
#endif

bool isValidButtonChordSource(uint32_t combo) {
  combo = sanitizeChordMask(combo);
  return countBits(combo) >= 2;
}

bool isValidButtonChordOutput(uint32_t combo) {
  combo = sanitizeChordMask(combo);
  return combo != 0;
}

bool computeActiveButtonChordRemap(const ButtonChordRemapSet& remaps) {
  for (uint8_t i = 0; i < BUTTON_CHORD_REMAP_SLOT_COUNT; ++i) {
    const ButtonChordRemapSlot& slot = remaps.slots[i];
    if (isValidButtonChordSource(slot.source_combo) &&
        isValidButtonChordOutput(slot.output_combo)) {
      return true;
    }
  }
  return false;
}

bool __not_in_flash_func(hasActiveButtonChordRemap)() {
  return active_chord_remaps_enabled;
}

void loadButtonChordRemaps() {
#if defined(ADAPT_CAP_DEFERRED_SETTINGS_PERSISTENCE) || defined(REFLEX_HOST_TEST_DEFERRED_SETTINGS)
  if (buttonChordRemapsPending) {
    active_chord_remaps_enabled =
        computeActiveButtonChordRemap(active_chord_remaps);
    return;
  }
#endif
  uint8_t slot = 0;
  uint16_t generation = 0;
  ButtonChordRemapSet remaps{};
  bool found = readLatestPersistedRecordAB(
        kButtonChordRecordBase,
        kButtonChordRecordSize,
        kPersistedBlockMagicButtonChords,
        slot,
        generation,
        remaps);
  if (!found && kButtonChordRecordBase != kButtonChordChainedBase) {
    // Earlier firmware wrote this record at its chained address; its A slot
    // fits inside the window. Adopt it until the next save relocates it.
    found = readLatestPersistedRecordAB(
        kButtonChordChainedBase,
        kButtonChordRecordSize,
        kPersistedBlockMagicButtonChords,
        slot,
        generation,
        remaps);
  }
  if (!found) {
    memset(&active_chord_remaps, 0, sizeof(active_chord_remaps));
    active_chord_remaps_enabled = false;
    return;
  }

  sanitizeChordRemaps(remaps);
  active_chord_remaps = remaps;
  active_chord_remaps_enabled = computeActiveButtonChordRemap(active_chord_remaps);
}

void writeButtonChordRemaps(const ButtonChordRemapSet& remaps) {
  ButtonChordRemapSet sanitized = remaps;
  sanitizeChordRemaps(sanitized);
  active_chord_remaps = sanitized;
  active_chord_remaps_enabled = computeActiveButtonChordRemap(active_chord_remaps);
#if defined(ADAPT_CAP_DEFERRED_SETTINGS_PERSISTENCE) || defined(REFLEX_HOST_TEST_DEFERRED_SETTINGS)
  if (buttonChordRemapsPending || settingsPersistenceProtectedSessionActive()) {
    buttonChordRemapsPending = true;
    buttonChordPersistenceWindow.reset();
    return;
  }
#endif
  if (!writeButtonChordRemapsNow(sanitized)) {
#if defined(ADAPT_CAP_DEFERRED_SETTINGS_PERSISTENCE) || defined(REFLEX_HOST_TEST_DEFERRED_SETTINGS)
    buttonChordRemapsPending = true;
    buttonChordPersistenceWindow.reset();
#endif
  }
}

void clearButtonChordRemaps() {
  ButtonChordRemapSet empty{};
  writeButtonChordRemaps(empty);
}

bool setButtonChordRemap(uint8_t slot, uint32_t source_combo, uint32_t output_combo) {
  if (slot >= BUTTON_CHORD_REMAP_SLOT_COUNT) {
    return false;
  }
  source_combo = sanitizeChordMask(source_combo);
  output_combo = sanitizeChordMask(output_combo);
  if (!isValidButtonChordSource(source_combo) || !isValidButtonChordOutput(output_combo)) {
    return false;
  }

  ButtonChordRemapSet remaps = active_chord_remaps;
  remaps.slots[slot].source_combo = source_combo;
  remaps.slots[slot].output_combo = output_combo;
  writeButtonChordRemaps(remaps);
  return true;
}

void applyButtonChordRemapsToFrame(controller_state_t& frame) {
  if (!frame.connected) {
    return;
  }

  const uint32_t buttons = buttonsForChordFrame(frame);
  for (uint8_t i = 0; i < BUTTON_CHORD_REMAP_SLOT_COUNT; ++i) {
    const ButtonChordRemapSlot& slot = active_chord_remaps.slots[i];
    if (!isValidButtonChordSource(slot.source_combo) ||
        !isValidButtonChordOutput(slot.output_combo)) {
      continue;
    }
    if ((buttons & slot.source_combo) != slot.source_combo) {
      continue;
    }

    clearButtonsFromFrame(frame, slot.source_combo);
    setButtonsOnFrame(frame, slot.output_combo);
    return;
  }
}

uint16_t buttonChordStorageRequiredEnd() {
  return kButtonChordChainedBase + kButtonChordAbLength;
}

uint16_t buttonChordStorageOccupiedEnd() {
  return kButtonChordRecordBase + kButtonChordAbLength;
}

void buttonChordPersistenceService() {
#if defined(ADAPT_CAP_DEFERRED_SETTINGS_PERSISTENCE) || defined(REFLEX_HOST_TEST_DEFERRED_SETTINGS)
  if (!buttonChordPersistenceWindow.shouldFlush(
        buttonChordRemapsPending,
        settingsPersistenceProtectedSessionActive(),
        millis())) {
    return;
  }
  (void)buttonChordPersistenceFlushNow();
#endif
}

bool buttonChordPersistenceFlushNow() {
#if defined(ADAPT_CAP_DEFERRED_SETTINGS_PERSISTENCE) || defined(REFLEX_HOST_TEST_DEFERRED_SETTINGS)
  if (!buttonChordRemapsPending) {
    buttonChordPersistenceWindow.reset();
    return true;
  }
  if (!writeButtonChordRemapsNow(active_chord_remaps)) {
    buttonChordPersistenceWindow.reset();
    return false;
  }
  buttonChordRemapsPending = false;
  buttonChordPersistenceWindow.reset();
#endif
  return true;
}

void buttonChordPersistenceDiscardDeferred() {
#if defined(ADAPT_CAP_DEFERRED_SETTINGS_PERSISTENCE) || defined(REFLEX_HOST_TEST_DEFERRED_SETTINGS)
  buttonChordRemapsPending = false;
  buttonChordPersistenceWindow.reset();
#endif
}

bool buttonChordPersistenceHasPending() {
#if defined(ADAPT_CAP_DEFERRED_SETTINGS_PERSISTENCE) || defined(REFLEX_HOST_TEST_DEFERRED_SETTINGS)
  return buttonChordRemapsPending;
#else
  return false;
#endif
}
