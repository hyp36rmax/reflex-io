#include "../product_config.h"

#include <stdio.h>

#include "menu_submenu_hotkeys.h"

#include "menu.h"
#include "menu_choice_list.h"
#include "menu_helpers.h"
#include "menu_runtime_state.h"
#include "../core/device_runtime_state.h"
#include "../core/hotkey_combo.h"
#include "../platform/buzzer.h"

namespace {

#if defined(ADAPT_CAP_DIRECTIONAL_HOTKEYS)
constexpr uint8_t kHotkeyRowCount = 3;
#else
constexpr uint8_t kHotkeyRowCount = 6;
#endif
constexpr uint8_t kHotkeyValueSize = 12;

const char* const kHotkeyHoldNames[] = {
  "Press", "0.5s", "1.0s", "1.5s", "2.0s", "2.5s", "3.0s"
};
constexpr uint8_t kHotkeyHoldCount = sizeof(kHotkeyHoldNames) / sizeof(kHotkeyHoldNames[0]);
#if defined(ADAPT_CAP_DIRECTIONAL_HOTKEYS)
constexpr char kHotkeyHoldTitle[] = "Menu Hold";
#else
constexpr char kHotkeyHoldTitle[] = "Hotkey Hold";
#endif

struct HotkeyRenderSnapshot {
  bool valid;
  uint8_t cursor;
  uint8_t hold;
  uint8_t menu_enabled;
  uint8_t system_enabled;
  uint8_t home_enabled;
  uint8_t capture_enabled;
  uint32_t menu_combo;
  uint32_t system_combo;
  uint32_t home_combo;
  uint32_t capture_combo;
};

HotkeyRenderSnapshot s_hotkey_render_snapshot{};

const char* compactHotkeyComboName(uint32_t combo) {
  switch (combo) {
    case INPUT_PAD_L | INPUT_START:
      return "<+ST";
    case INPUT_PAD_R | INPUT_START:
      return ">+ST";
    case INPUT_PAD_D | INPUT_START:
      return "v+ST";
    case INPUT_PAD_U | INPUT_START:
      return "^+ST";
    default:
      return nullptr;
  }
}

void formatHotkeyValue(char* buffer, size_t size, uint8_t enabled, uint32_t combo) {
  if (buffer == nullptr || size == 0) {
    return;
  }
  if (!enabled || combo == 0) {
    snprintf(buffer, size, "Off");
    return;
  }

  const char* compact = compactHotkeyComboName(combo);
  if (compact != nullptr) {
    snprintf(buffer, size, "%s", compact);
    return;
  }

  formatHotkeyCombo(buffer, size, combo, deviceMode);
}

bool hotkeyRenderSnapshotMatches(uint8_t cursor) {
  return s_hotkey_render_snapshot.valid &&
         s_hotkey_render_snapshot.cursor == cursor &&
         s_hotkey_render_snapshot.hold == menu_hotkey_hold_time &&
         s_hotkey_render_snapshot.menu_enabled == menu_menu_hotkey &&
         s_hotkey_render_snapshot.system_enabled == menu_system_menu_hotkey &&
         s_hotkey_render_snapshot.home_enabled == menu_home_hotkey &&
         s_hotkey_render_snapshot.capture_enabled == menu_capture_hotkey &&
         s_hotkey_render_snapshot.menu_combo == menu_hotkey_combo &&
         s_hotkey_render_snapshot.system_combo == menu_system_hotkey_combo &&
         s_hotkey_render_snapshot.home_combo == menu_home_hotkey_combo &&
         s_hotkey_render_snapshot.capture_combo == menu_capture_hotkey_combo;
}

void captureHotkeyRenderSnapshot(uint8_t cursor) {
  s_hotkey_render_snapshot.valid = true;
  s_hotkey_render_snapshot.cursor = cursor;
  s_hotkey_render_snapshot.hold = menu_hotkey_hold_time;
  s_hotkey_render_snapshot.menu_enabled = menu_menu_hotkey;
  s_hotkey_render_snapshot.system_enabled = menu_system_menu_hotkey;
  s_hotkey_render_snapshot.home_enabled = menu_home_hotkey;
  s_hotkey_render_snapshot.capture_enabled = menu_capture_hotkey;
  s_hotkey_render_snapshot.menu_combo = menu_hotkey_combo;
  s_hotkey_render_snapshot.system_combo = menu_system_hotkey_combo;
  s_hotkey_render_snapshot.home_combo = menu_home_hotkey_combo;
  s_hotkey_render_snapshot.capture_combo = menu_capture_hotkey_combo;
}

void invalidateHotkeyRenderSnapshot() {
  s_hotkey_render_snapshot.valid = false;
}

void renderHotkeySettingRow(uint8_t row, uint8_t cursor, const char* label, const char* value) {
  display.clear(0, 127, row + 1, row + 1);
  display.setCursor(0, row + 1);
  display.print(row == cursor ? ">" : " ");
  display.print(label);
  display.setCursor(88, row + 1);
  display.print(value);
  display.clearToEOL();
}

void renderHotkeyRows(uint8_t cursor) {
  if (hotkeyRenderSnapshotMatches(cursor)) {
    return;
  }

  if (!s_hotkey_render_snapshot.valid) {
    display.clear();
  }

  display.setFont(System5x7);
  display.set1X();
  printCenteredMenuTitle(display, "Hotkey");

  char value[kHotkeyValueSize];
#if defined(ADAPT_CAP_DIRECTIONAL_HOTKEYS)
  renderHotkeySettingRow(0, cursor, kHotkeyHoldTitle, kHotkeyHoldNames[menu_hotkey_hold_time <= 6 ? menu_hotkey_hold_time : 1]);
  renderHotkeySettingRow(1, cursor, "Down+Start", menu_home_hotkey ? "OSD" : "Off");
  display.clear(0, 127, 3, 3);
  display.setCursor(0, 3);
  printMenuButton(display, "Back", cursor == 2);
  display.setCursor(0, 5);
#if defined(ADAPT_CAP_CONTROLLER_HOTKEY_PROFILES)
  display.print(F("Controller defaults"));
#else
  display.print(F("Other Start shortcuts"));
#endif
  display.setCursor(0, 6);
  display.print(F("are in Quick Settings"));
  display.clearToEOL();
#else
  renderHotkeySettingRow(0, cursor, kHotkeyHoldTitle, kHotkeyHoldNames[menu_hotkey_hold_time <= 6 ? menu_hotkey_hold_time : 1]);
  formatHotkeyValue(value, sizeof(value), menu_menu_hotkey, menu_hotkey_combo);
  if (menu_hotkey_combo == 0) snprintf(value, sizeof(value), "Unset");
  renderHotkeySettingRow(1, cursor, "Quick Menu", value);
  formatHotkeyValue(value, sizeof(value), menu_system_menu_hotkey, menu_system_hotkey_combo);
  if (menu_system_hotkey_combo == 0) snprintf(value, sizeof(value), "Unset");
  renderHotkeySettingRow(2, cursor, "System Menu", value);
  formatHotkeyValue(value, sizeof(value), menu_home_hotkey,
                    effectiveHomeHotkeyComboForMode(deviceMode, menu_home_hotkey_combo));
  renderHotkeySettingRow(3, cursor, "Home", value);
  formatHotkeyValue(value, sizeof(value), menu_capture_hotkey, menu_capture_hotkey_combo);
  renderHotkeySettingRow(4, cursor, "Capture/TPad", value);

  display.setCursor(0, 6);
  printMenuButton(display, "Back", cursor == 5);
  display.clearToEOL();
  display.clear(0, 127, 7, 7);
#endif
  captureHotkeyRenderSnapshot(cursor);
}

const char* holdChoiceLabel(uint8_t index) {
  return kHotkeyHoldNames[index];
}

void chooseHold(uint8_t index) {
  menu_hotkey_hold_time = index;
}

void toggleSelectedHotkey(uint8_t cursor) {
  switch (cursor) {
    case 1:
#if defined(ADAPT_CAP_DIRECTIONAL_HOTKEYS)
      menu_home_hotkey = menu_home_hotkey ? 0 : 1;
      break;
#else
      menu_menu_hotkey = menu_hotkey_combo != 0 && !menu_menu_hotkey;
      break;
    case 2:
      menu_system_menu_hotkey = menu_system_hotkey_combo != 0 && !menu_system_menu_hotkey;
      break;
    case 3:
      menu_home_hotkey = menu_home_hotkey ? 0 : 1;
      break;
    case 4:
      menu_capture_hotkey = menu_capture_hotkey ? 0 : 1;
      // Older Off settings can migrate with an empty binding. Opting in must
      // restore the standard chord, while preserving any configured chord.
      if (menu_capture_hotkey && menu_capture_hotkey_combo == 0) {
        menu_capture_hotkey_combo = defaultHotkeyCombo(HotkeyBindingId::Capture);
      }
      break;
#endif
    default:
      break;
  }
}

void closeHotkeySubmenu(bool& active, uint8_t selected_visible, uint8_t scroll_offset, bool bottom_right) {
  active = false;
  invalidateHotkeyRenderSnapshot();
  display.clear();
  renderMenu(selected_visible, scroll_offset, bottom_right);
}

}  // namespace

bool handleHotkeysSubmenu(
    uint8_t& hotkeys_submenu_cursor,
    bool& hotkeys_submenu_active,
    uint8_t selected_visible,
    uint8_t scroll_offset,
    bool bottom_right,
    bool ctrlDownJust,
    bool ctrlUpJust,
    bool ctrlLeftJust,
    bool ctrlAJust,
    bool ctrlRightJust,
    bool ctrlBJust,
    bool btnNavigateJustPressed,
    bool btnChangeJustPressed) {
  if (!hotkeys_submenu_active) {
    invalidateHotkeyRenderSnapshot();
    return false;
  }

  if (hotkeys_submenu_cursor >= kHotkeyRowCount) {
    hotkeys_submenu_cursor = 0;
  }

  if (ctrlDownJust || btnNavigateJustPressed) {
    hotkeys_submenu_cursor = (uint8_t)((hotkeys_submenu_cursor + 1) % kHotkeyRowCount);
    buzzer.playMenuNav();
    renderHotkeyRows(hotkeys_submenu_cursor);
    return true;
  }

  if (ctrlUpJust) {
    hotkeys_submenu_cursor = (hotkeys_submenu_cursor == 0) ? (kHotkeyRowCount - 1) : (uint8_t)(hotkeys_submenu_cursor - 1);
    buzzer.playMenuNav();
    renderHotkeyRows(hotkeys_submenu_cursor);
    return true;
  }

  if (ctrlBJust) {
    closeHotkeySubmenu(hotkeys_submenu_active, selected_visible, scroll_offset, bottom_right);
    buzzer.playMenuNav();
    return true;
  }

  if (hotkeys_submenu_cursor == kHotkeyRowCount - 1 && (ctrlAJust || btnChangeJustPressed)) {
    closeHotkeySubmenu(hotkeys_submenu_active, selected_visible, scroll_offset, bottom_right);
    buzzer.playMenuNav();
    return true;
  }

  // The hold time is a list: Left/Right leave its row alone.
  if (hotkeys_submenu_cursor == 0) {
    if (ctrlAJust || btnChangeJustPressed) {
      openMenuChoiceList({kHotkeyHoldTitle, kHotkeyHoldCount, menu_hotkey_hold_time,
                          holdChoiceLabel, chooseHold, invalidateHotkeyRenderSnapshot});
      buzzer.playMenuEnter();
      return true;
    }
  } else if (hotkeys_submenu_cursor < kHotkeyRowCount - 1 &&
             (ctrlAJust || ctrlRightJust || btnChangeJustPressed || ctrlLeftJust)) {
    toggleSelectedHotkey(hotkeys_submenu_cursor);
    buzzer.playMenuNav();
    renderHotkeyRows(hotkeys_submenu_cursor);
    return true;
  }

  renderHotkeyRows(hotkeys_submenu_cursor);
  return true;
}
