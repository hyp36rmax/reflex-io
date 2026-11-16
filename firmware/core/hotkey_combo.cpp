#include "hotkey_combo.h"

#include <Arduino.h>

#include <stdio.h>
#include <string.h>

#include "../product_config.h"
#include "button_remap.h"
#include "eeprom_helper.h"
#include "settings_store.h"
#include "settings_persistence_deferred.h"
#include "../output/runtime/output_boot_bridge.h"

namespace {

constexpr uint16_t kLegacyBlockMagicHotkeys = 0x484B;  // HK
#if defined(ADAPT_CAP_DIRECTIONAL_HOTKEYS)
constexpr uint32_t kDefaultMenuCombo = 0;
constexpr uint32_t kDefaultSystemMenuCombo = 0;
constexpr uint16_t kPersistedBlockMagicHotkeys = 0x484C;  // Classic per-input shortcuts replace built-ins.
#else
constexpr uint32_t kDefaultMenuCombo = INPUT_PAD_L | INPUT_START;
constexpr uint32_t kDefaultSystemMenuCombo = INPUT_PAD_R | INPUT_START;
constexpr uint16_t kPersistedBlockMagicHotkeys = kLegacyBlockMagicHotkeys;
#endif
constexpr uint32_t kDefaultHomeCombo = INPUT_PAD_D | INPUT_START;
constexpr uint32_t kDefaultCaptureCombo = INPUT_PAD_U | INPUT_START;
constexpr uint8_t kHotkeySettingsPayloadSize = 4;
constexpr uint16_t kHotkeySettingsRecordSize =
  PERSISTED_BLOCK_HEADER_SIZE + kHotkeySettingsPayloadSize;
constexpr uint16_t kHotkeySettingsRecordBase =
  AUTH_KEY_EEPROM_END;

struct HotkeyComboRecord {
  uint8_t menu;
  uint8_t system_menu;
  uint8_t home;
  uint8_t capture;
};

#if defined(ADAPT_CAP_DEFERRED_SETTINGS_PERSISTENCE) || defined(REFLEX_HOST_TEST_DEFERRED_SETTINGS)
HotkeyBindingSet pendingHotkeyBindings{};
bool hotkeyBindingsPending = false;
SettingsPersistenceSafeWindow hotkeyPersistenceWindow{};
#endif

static_assert(sizeof(HotkeyComboRecord) == kHotkeySettingsPayloadSize, "Hotkey combo record size changed");

constexpr uint32_t kHotkeyButtonOrder[] = {
  INPUT_PAD_U,
  INPUT_PAD_D,
  INPUT_PAD_L,
  INPUT_PAD_R,
  INPUT_A,
  INPUT_B,
  INPUT_X,
  INPUT_Y,
  INPUT_L1,
  INPUT_R1,
  INPUT_L2,
  INPUT_R2,
  INPUT_L3,
  INPUT_R3,
  INPUT_SELECT,
  INPUT_START,
};

// Remap button slot (button bit position) of a hotkey face/shoulder button.
uint8_t buttonSlotForHotkey(uint32_t button) {
  switch (button) {
    case INPUT_A: return 0;
    case INPUT_B: return 1;
    case INPUT_X: return 2;
    case INPUT_Y: return 3;
    case INPUT_L1: return 4;
    case INPUT_R1: return 5;
    case INPUT_L2: return 6;
    case INPUT_R2: return 7;
    case INPUT_L3: return 8;
    case INPUT_R3: return 9;
    default: return 0xFF;
  }
}

const char* hotkeyButtonName(DeviceEnum mode, uint32_t button) {
  switch (button) {
    case INPUT_PAD_U: return "Up";
    case INPUT_PAD_D: return "Dn";
    case INPUT_PAD_L: return "Left";
    case INPUT_PAD_R: return "Right";
    case INPUT_SELECT: return mode == RZORD_JAGUAR ? "Opt" : "Sel";
    case INPUT_START: return mode == RZORD_JAGUAR ? "Pau" : "Sta";
    default: {
      const uint8_t slot = buttonSlotForHotkey(button);
      if (slot == 0xFF) {
        return "?";
      }
      return getRemapButtonNameForSlot(mode, slot);
    }
  }
}

bool isHotkeyComboBitPatternValid(uint32_t combo) {
  const uint32_t masked = combo & HOTKEY_ALLOWED_BUTTONS;
  if (combo != masked || masked == 0) {
    return false;
  }
  return singleHotkeyButton(masked) == 0 && singleHotkeyButton(masked & (masked - 1u)) != 0;
}

uint8_t packHotkeyCombo(uint32_t combo) {
  if (combo == 0) {
    return 0;
  }

  uint8_t indices[2] = {0xFF, 0xFF};
  uint8_t count = 0;
  for (uint8_t i = 0; i < sizeof(kHotkeyButtonOrder) / sizeof(kHotkeyButtonOrder[0]); ++i) {
    const uint32_t button = kHotkeyButtonOrder[i];
    if ((combo & button) == 0) {
      continue;
    }
    if (count >= 2) {
      return 0;
    }
    indices[count++] = i;
  }

  if (count != 2 || indices[0] == indices[1]) {
    return 0;
  }
  return (uint8_t)((indices[0] << 4) | indices[1]);
}

uint32_t unpackHotkeyCombo(uint8_t packed) {
  if (packed == 0) {
    return 0;
  }

  const uint8_t firstIndex = (packed >> 4) & 0x0F;
  const uint8_t secondIndex = packed & 0x0F;
  if (firstIndex >= sizeof(kHotkeyButtonOrder) / sizeof(kHotkeyButtonOrder[0]) ||
      secondIndex >= sizeof(kHotkeyButtonOrder) / sizeof(kHotkeyButtonOrder[0]) ||
      firstIndex == secondIndex) {
    return 0;
  }

  return kHotkeyButtonOrder[firstIndex] | kHotkeyButtonOrder[secondIndex];
}

HotkeyComboRecord encodeHotkeyBindings(const HotkeyBindingSet& bindings) {
  HotkeyComboRecord record = {};
  record.menu = packHotkeyCombo(bindings.menu);
  record.system_menu = packHotkeyCombo(bindings.system_menu);
  record.home = packHotkeyCombo(bindings.home);
  record.capture = packHotkeyCombo(bindings.capture);
  return record;
}

HotkeyBindingSet decodeHotkeyBindings(const HotkeyComboRecord& record) {
  HotkeyBindingSet bindings{};
  bindings.menu = unpackHotkeyCombo(record.menu);
  bindings.system_menu = unpackHotkeyCombo(record.system_menu);
  bindings.home = unpackHotkeyCombo(record.home);
  bindings.capture = unpackHotkeyCombo(record.capture);
  return bindings;
}

HotkeyBindingSet buildLegacyHotkeyBindings() {
  HotkeyBindingSet bindings{};
  setDefaultHotkeyBindings(bindings);

  GlobalSettingsRecord globalSettings{};
  if (readGlobalSettings(globalSettings)) {
    if (!menuHotkeyQuickEnabledFromRaw(globalSettings.menu_hotkey)) {
      bindings.menu = 0;
    }
    if (!menuHotkeySystemEnabledFromRaw(globalSettings.menu_hotkey)) {
      bindings.system_menu = 0;
    }
    if ((globalSettings.home_hotkey & HOME_HOTKEY_VALUE_MASK) == 0) {
      bindings.home = 0;
    }
    if (globalSettings.capture_hotkey == 0) {
      bindings.capture = 0;
    }
  }

  sanitizeHotkeyBindings(bindings);
  return bindings;
}

void resolveBindingConflict(HotkeyBindingSet& bindings, HotkeyBindingId id) {
  uint32_t* slot = hotkeyBindingSlot(bindings, id);
  if (slot == nullptr || *slot == 0 || !hotkeyComboConflicts(bindings, id, *slot)) {
    return;
  }

  const uint32_t fallback = defaultHotkeyCombo(id);
  if (fallback != 0 && !hotkeyComboConflicts(bindings, id, fallback)) {
    *slot = fallback;
  } else {
    *slot = 0;
  }
}

bool writeHotkeyBindingsNow(const HotkeyBindingSet& bindings) {
  return ensurePersistedRecordAB(
    kHotkeySettingsRecordBase,
    kHotkeySettingsRecordSize,
    kPersistedBlockMagicHotkeys,
    encodeHotkeyBindings(bindings)
  );
}

}  // namespace

uint32_t defaultHotkeyCombo(HotkeyBindingId id) {
  switch (id) {
    case HotkeyBindingId::Menu:
      return kDefaultMenuCombo;
    case HotkeyBindingId::SystemMenu:
      #ifdef ADAPT_CAP_JVS_UI
      return 0;
      #else
      return kDefaultSystemMenuCombo;
      #endif
    case HotkeyBindingId::Home:
      return kDefaultHomeCombo;
    case HotkeyBindingId::Capture:
      return kDefaultCaptureCombo;
    case HotkeyBindingId::Count:
      break;
  }
  return 0;
}

bool isValidHotkeyCombo(uint32_t combo) {
  return isHotkeyComboBitPatternValid(combo);
}

uint32_t sanitizeHotkeyCombo(HotkeyBindingId id, uint32_t combo) {
  if (combo == 0) {
    return 0;
  }
  return isHotkeyComboBitPatternValid(combo) ? combo : defaultHotkeyCombo(id);
}

void setDefaultHotkeyBindings(HotkeyBindingSet& bindings) {
  bindings.menu = defaultHotkeyCombo(HotkeyBindingId::Menu);
  bindings.system_menu = defaultHotkeyCombo(HotkeyBindingId::SystemMenu);
  bindings.home = defaultHotkeyCombo(HotkeyBindingId::Home);
  bindings.capture = defaultHotkeyCombo(HotkeyBindingId::Capture);
}

uint32_t* hotkeyBindingSlot(HotkeyBindingSet& bindings, HotkeyBindingId id) {
  switch (id) {
    case HotkeyBindingId::Menu:
      return &bindings.menu;
    case HotkeyBindingId::SystemMenu:
      return &bindings.system_menu;
    case HotkeyBindingId::Home:
      return &bindings.home;
    case HotkeyBindingId::Capture:
      return &bindings.capture;
    case HotkeyBindingId::Count:
      break;
  }
  return nullptr;
}

uint32_t hotkeyBindingValue(const HotkeyBindingSet& bindings, HotkeyBindingId id) {
  switch (id) {
    case HotkeyBindingId::Menu:
      return bindings.menu;
    case HotkeyBindingId::SystemMenu:
      return bindings.system_menu;
    case HotkeyBindingId::Home:
      return bindings.home;
    case HotkeyBindingId::Capture:
      return bindings.capture;
    case HotkeyBindingId::Count:
      break;
  }
  return 0;
}

bool hotkeyComboConflicts(const HotkeyBindingSet& bindings, HotkeyBindingId id, uint32_t combo) {
  if (combo == 0) {
    return false;
  }
  for (uint8_t i = 0; i < static_cast<uint8_t>(HotkeyBindingId::Count); ++i) {
    const HotkeyBindingId otherId = static_cast<HotkeyBindingId>(i);
    if (otherId == id) {
      continue;
    }
    if (hotkeyBindingValue(bindings, otherId) == combo) {
      return true;
    }
  }
  return false;
}

void sanitizeHotkeyBindings(HotkeyBindingSet& bindings) {
  bindings.menu = sanitizeHotkeyCombo(HotkeyBindingId::Menu, bindings.menu);
  bindings.system_menu = sanitizeHotkeyCombo(HotkeyBindingId::SystemMenu, bindings.system_menu);
  bindings.home = sanitizeHotkeyCombo(HotkeyBindingId::Home, bindings.home);
  bindings.capture = sanitizeHotkeyCombo(HotkeyBindingId::Capture, bindings.capture);

  resolveBindingConflict(bindings, HotkeyBindingId::SystemMenu);
  resolveBindingConflict(bindings, HotkeyBindingId::Home);
  resolveBindingConflict(bindings, HotkeyBindingId::Capture);
}

bool __not_in_flash_func(hotkeyComboMatches)(uint32_t buttons, uint32_t combo) {
  return combo != 0 && (buttons & HOTKEY_ALLOWED_BUTTONS) == combo;
}

uint32_t singleHotkeyButton(uint32_t buttons) {
  const uint32_t masked = buttons & HOTKEY_ALLOWED_BUTTONS;
  if (masked == 0 || (masked & (masked - 1u)) != 0) {
    return 0;
  }
  return masked;
}

uint32_t __not_in_flash_func(hotkeyButtonsForControllerFrame)(const controller_state_t& frame) {
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
  return buttons & HOTKEY_ALLOWED_BUTTONS;
}

uint16_t hotkeyHoldMsForSetting(uint8_t value) {
  if (value == 0) {
    return 0;
  }
  if (value > 6) {
    value = 6;
  }
  return (uint16_t)value * 500u;
}

void formatHotkeyCombo(char* buffer, size_t size, uint32_t combo, DeviceEnum mode) {
  if (buffer == nullptr || size == 0) {
    return;
  }

  if (combo == 0) {
    snprintf(buffer, size, "Disabled");
    return;
  }

  buffer[0] = '\0';
  bool first = true;
  for (uint8_t i = 0; i < sizeof(kHotkeyButtonOrder) / sizeof(kHotkeyButtonOrder[0]); ++i) {
    const uint32_t button = kHotkeyButtonOrder[i];
    if ((combo & button) == 0) {
      continue;
    }

    const char* name = hotkeyButtonName(mode, button);
    if (!first) {
      strncat(buffer, "+", size - strlen(buffer) - 1u);
    }
    strncat(buffer, name, size - strlen(buffer) - 1u);
    first = false;
  }

  if (buffer[0] == '\0') {
    snprintf(buffer, size, "Disabled");
  }
}

bool readHotkeyBindings(HotkeyBindingSet& bindings) {
#if defined(ADAPT_CAP_DEFERRED_SETTINGS_PERSISTENCE) || defined(REFLEX_HOST_TEST_DEFERRED_SETTINGS)
  if (hotkeyBindingsPending) {
    bindings = pendingHotkeyBindings;
    return true;
  }
#endif
  uint8_t slot = 0;
  uint16_t generation = 0;
  HotkeyComboRecord record{};
  if (!readLatestPersistedRecordAB(
        kHotkeySettingsRecordBase,
        kHotkeySettingsRecordSize,
        kPersistedBlockMagicHotkeys,
        slot,
        generation,
        record)) {
#if defined(ADAPT_CAP_DIRECTIONAL_HOTKEYS)
    if (readLatestPersistedRecordAB(
          kHotkeySettingsRecordBase, kHotkeySettingsRecordSize,
          kLegacyBlockMagicHotkeys, slot, generation, record)) {
      bindings = decodeHotkeyBindings(record);
      // Retire the old built-ins once, without losing other custom bindings.
      // The new record distinguishes a later explicit user assignment of
      // these same chords from an inherited default. Record size is unchanged.
      if (bindings.menu == (INPUT_PAD_L | INPUT_START)) bindings.menu = 0;
      if (bindings.system_menu == (INPUT_PAD_R | INPUT_START)) bindings.system_menu = 0;
      sanitizeHotkeyBindings(bindings);
    } else
#endif
    {
      bindings = buildLegacyHotkeyBindings();
    }
    writeHotkeyBindings(bindings);
    return false;
  }

  bindings = decodeHotkeyBindings(record);
  sanitizeHotkeyBindings(bindings);
  return true;
}

void writeHotkeyBindings(const HotkeyBindingSet& bindings) {
  HotkeyBindingSet sanitized = bindings;
  sanitizeHotkeyBindings(sanitized);
#if defined(ADAPT_CAP_DEFERRED_SETTINGS_PERSISTENCE) || defined(REFLEX_HOST_TEST_DEFERRED_SETTINGS)
  if (hotkeyBindingsPending || settingsPersistenceProtectedSessionActive()) {
    pendingHotkeyBindings = sanitized;
    hotkeyBindingsPending = true;
    hotkeyPersistenceWindow.reset();
    return;
  }
#endif
  if (!writeHotkeyBindingsNow(sanitized)) {
#if defined(ADAPT_CAP_DEFERRED_SETTINGS_PERSISTENCE) || defined(REFLEX_HOST_TEST_DEFERRED_SETTINGS)
    pendingHotkeyBindings = sanitized;
    hotkeyBindingsPending = true;
    hotkeyPersistenceWindow.reset();
#endif
  }
}

uint32_t effectiveHomeHotkeyComboForMode(DeviceEnum mode, uint32_t configuredCombo) {
  if (mode == RZORD_JAGUAR && configuredCombo == kDefaultHomeCombo) {
    return INPUT_PAD_D | INPUT_SELECT;
  }
  return configuredCombo;
}

uint16_t hotkeyStorageRequiredEnd() {
  return kHotkeySettingsRecordBase + (PERSISTED_AB_SLOT_COUNT * kHotkeySettingsRecordSize);
}

void hotkeyPersistenceService() {
#if defined(ADAPT_CAP_DEFERRED_SETTINGS_PERSISTENCE) || defined(REFLEX_HOST_TEST_DEFERRED_SETTINGS)
  if (!hotkeyPersistenceWindow.shouldFlush(
        hotkeyBindingsPending,
        settingsPersistenceProtectedSessionActive(),
        millis())) {
    return;
  }
  (void)hotkeyPersistenceFlushNow();
#endif
}

bool hotkeyPersistenceFlushNow() {
#if defined(ADAPT_CAP_DEFERRED_SETTINGS_PERSISTENCE) || defined(REFLEX_HOST_TEST_DEFERRED_SETTINGS)
  if (!hotkeyBindingsPending) {
    hotkeyPersistenceWindow.reset();
    return true;
  }
  if (!writeHotkeyBindingsNow(pendingHotkeyBindings)) {
    hotkeyPersistenceWindow.reset();
    return false;
  }
  hotkeyBindingsPending = false;
  hotkeyPersistenceWindow.reset();
#endif
  return true;
}

void hotkeyPersistenceDiscardDeferred() {
#if defined(ADAPT_CAP_DEFERRED_SETTINGS_PERSISTENCE) || defined(REFLEX_HOST_TEST_DEFERRED_SETTINGS)
  hotkeyBindingsPending = false;
  hotkeyPersistenceWindow.reset();
#endif
}

bool hotkeyPersistenceHasPending() {
#if defined(ADAPT_CAP_DEFERRED_SETTINGS_PERSISTENCE) || defined(REFLEX_HOST_TEST_DEFERRED_SETTINGS)
  return hotkeyBindingsPending;
#else
  return false;
#endif
}
