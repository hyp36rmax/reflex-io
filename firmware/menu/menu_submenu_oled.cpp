#include "../product_config.h"

#include <string.h>

#include "menu.h"
#include "menu_choice_list.h"
#include "menu_helpers.h"
#include "menu_list_navigation.h"
#include "menu_submenu_oled.h"
#include "menu_ui_state.h"
#include "menu_working_state.h"
#include "screensaver_animation.h"
#include "../core/controller_settings_state.h"
#include "../core/settings_store.h"
#include "../platform/buzzer.h"

namespace {

const char* const oled_inactive_names[] = { "Screensaver", "Blank OLED" };
const char* const oled_wake_names[] = { "Any input", "Adapt only" };
const char* const oled_timer_names[OLED_INACTIVE_TIMER_COUNT] = {
  "Off", "1m", "5m", "10m"
};

enum OledRow : uint8_t {
  OLED_ROW_INACTIVE,
  OLED_ROW_TIMER,
  OLED_ROW_WAKE,
  OLED_ROW_SCREENSAVER,
};

constexpr uint8_t kOledRowCapacity = 4;
constexpr uint8_t kVisibleRows = 6;
// Picker entries: Dim Display, then the enabled animations.
constexpr uint8_t kPickerDim = 0;

// Both pages follow the System menu: items, then a Cancel/Save bottom row.
// A cursor equal to the item count is the bottom row. Down walks items,
// Cancel, Save and wraps to the top; Up from the top lands on Save.
struct ListCursor {
  uint8_t index;
  bool save;  // bottom row: Save (true) or Cancel (false)
};

bool oled_submenu_initialized = false;
bool oled_page_save = false;
uint8_t oled_timer = 0;
// Kept apart from menu_screensaver because an Off timer stores no Dim choice.
bool oled_dim = false;
bool picker_save = false;
uint8_t picker_choice = 0;  // pending picker entry until Save

struct OledRenderSnapshot {
  bool valid;
  bool picker;
  uint8_t cursor;
  bool save;
  uint8_t inactive;
  uint8_t wake;
  uint8_t timer;
  bool dim;
  uint8_t animation;
  uint8_t choice;
};

OledRenderSnapshot oled_render_snapshot{};

bool oledRenderSnapshotMatches(const OledRenderSnapshot& current) {
  const OledRenderSnapshot& last = oled_render_snapshot;
  return last.valid && last.picker == current.picker &&
         last.cursor == current.cursor && last.save == current.save &&
         last.inactive == current.inactive && last.wake == current.wake &&
         last.timer == current.timer &&
         last.dim == current.dim && last.animation == current.animation &&
         last.choice == current.choice;
}

void captureOledRenderSnapshot(const OledRenderSnapshot& current) {
  oled_render_snapshot = current;
}

void invalidateOledRenderSnapshot() {
  oled_render_snapshot.valid = false;
}

OledRenderSnapshot currentOledRender(bool picker, uint8_t cursor, bool save) {
  OledRenderSnapshot snapshot{};
  snapshot.valid = true;
  snapshot.picker = picker;
  snapshot.cursor = cursor;
  snapshot.save = save;
  snapshot.inactive = menu_oled_inactive;
  snapshot.wake = menu_oled_wake;
  snapshot.timer = oled_timer;
  snapshot.dim = oled_dim;
  snapshot.animation = menu_screensaver_anim;
  snapshot.choice = picker_choice;
  return snapshot;
}

void moveDown(ListCursor& cursor, uint8_t count) {
  if (cursor.index >= count) {
    if (!cursor.save) {
      cursor.save = true;
    } else {
      cursor.save = false;
      cursor.index = 0;
    }
  } else {
    ++cursor.index;
    cursor.save = false;
  }
}

void moveUp(ListCursor& cursor, uint8_t count) {
  if (cursor.index >= count) {
    cursor.index = count - 1;
    cursor.save = false;
  } else if (cursor.index > 0) {
    --cursor.index;
  } else {
    cursor.index = count;
    cursor.save = true;
  }
}

uint8_t scrollFor(uint8_t cursor, uint8_t count) {
  return menu_list_navigation::firstVisible(cursor, count, kVisibleRows);
}

uint8_t buildOledRows(OledRow* rows) {
  uint8_t count = 0;
  rows[count++] = OLED_ROW_INACTIVE;
  rows[count++] = OLED_ROW_TIMER;
  rows[count++] = OLED_ROW_WAKE;
  if (menu_oled_inactive == 0) rows[count++] = OLED_ROW_SCREENSAVER;
  return count;
}

void syncMenuScreensaverMode() {
  menu_screensaver = encodeScreensaverMode(oled_timer, oled_dim);
}

uint8_t pickerCount() {
  return SCREENSAVER_ENABLED_ANIM_COUNT + 1;
}

uint8_t pickerEntryForCurrent() {
  if (oled_dim) return kPickerDim;
  for (uint8_t i = 0; i < SCREENSAVER_ENABLED_ANIM_COUNT; ++i) {
    if (screensaver_enabled_anim_ids[i] == menu_screensaver_anim) return i + 1;
  }
  return 1;
}

void printRightAligned(const char* text, uint8_t row) {
  const size_t width = strlen(text) * 6;
  display.setCursor(width < 128 ? (uint8_t)(128 - width) : 0, row);
  display.print(text);
}

void renderOledPage(const OledRow* rows, uint8_t count, uint8_t cursor, bool save) {
  display.setFont(System5x7);
  display.set1X();
  printCenteredMenuTitle(display, "OLED");

  for (uint8_t row = 0; row < kVisibleRows; ++row) {
    display.setCursor(0, row + 1);
    if (row >= count) {
      display.clearToEOL();
      continue;
    }
    display.print(row == cursor ? ">" : " ");
    switch (rows[row]) {
      case OLED_ROW_INACTIVE:
        display.print(F("Inactive"));
        display.clearToEOL();
        printRightAligned(oled_inactive_names[menu_oled_inactive ? 1 : 0], row + 1);
        break;
      case OLED_ROW_TIMER:
        display.print(F("Inactive timer"));
        display.clearToEOL();
        printRightAligned(oled_timer_names[oled_timer], row + 1);
        break;
      case OLED_ROW_WAKE:
        display.print(F("Wake"));
        display.clearToEOL();
        printRightAligned(oled_wake_names[menu_oled_wake ? 1 : 0], row + 1);
        break;
      case OLED_ROW_SCREENSAVER:
        display.print(F("Screensaver >"));
        display.clearToEOL();
        break;
    }
  }
  renderBottomRow(cursor >= count, save);
}

void renderScreensaverPicker(uint8_t cursor, bool save) {
  display.setFont(System5x7);
  display.set1X();
  printCenteredMenuTitle(display, "Screensaver");

  const uint8_t count = pickerCount();
  const uint8_t scroll = scrollFor(cursor, count);
  for (uint8_t row = 0; row < kVisibleRows; ++row) {
    const uint8_t entry = scroll + row;
    display.setCursor(0, row + 1);
    if (entry >= count) {
      display.clearToEOL();
      continue;
    }
    display.print(entry == cursor ? ">" : " ");
    if (entry == kPickerDim) {
      display.print(F("Dim Display"));
    } else {
      display.print(screensaver_anim_names[screensaver_enabled_anim_ids[entry - 1]]);
    }
    display.clearToEOL();
    if (entry == picker_choice) {
      display.setCursor(115, row + 1);
      display.print("*");
    }
  }
  renderBottomRow(cursor >= count, save);
}

void closeOledSubmenu(uint8_t selected_visible, uint8_t scroll_offset,
                      bool bottom_right) {
  oled_submenu_active = false;
  oled_submenu_initialized = false;
  screensaver_submenu_active = false;
  invalidateOledRenderSnapshot();
  display.clear();
  renderMenu(selected_visible, scroll_offset, bottom_right);
}

void saveOledSubmenu() {
  syncMenuScreensaverMode();
  oled_inactive_mode = menu_oled_inactive;
  oled_wake_mode = menu_oled_wake;
  screensaver_mode = menu_screensaver;
  screensaver_animation = menu_screensaver_anim;
  saveOledSettings(menu_oled_inactive, menu_oled_wake, menu_screensaver, menu_screensaver_anim);
  resetAnimationState();
  buzzer.playSave();
}

void cancelOledSubmenu() {
  menu_oled_inactive = oled_inactive_mode;
  menu_oled_wake = oled_wake_mode;
  menu_screensaver = screensaver_mode;
  menu_screensaver_anim = screensaver_animation;
  buzzer.playError();
}

const char* timerChoiceLabel(uint8_t index) {
  return oled_timer_names[index];
}

void chooseTimer(uint8_t index) {
  oled_timer = index;
  syncMenuScreensaverMode();
}

void closeScreensaverPicker() {
  screensaver_submenu_active = false;
  invalidateOledRenderSnapshot();
  display.clear();
}

void applyPickerChoice() {
  if (picker_choice == kPickerDim) {
    oled_dim = true;
  } else {
    oled_dim = false;
    menu_screensaver_anim = screensaver_enabled_anim_ids[picker_choice - 1];
  }
  syncMenuScreensaverMode();
}

}  // namespace

bool finishOledSubmenuForHold(bool save) {
  if (!oled_submenu_active) return false;
  if (save && oled_submenu_initialized) {
    if (screensaver_submenu_active) applyPickerChoice();
    saveOledSubmenu();
  } else if (!save) {
    cancelOledSubmenu();
  }
  oled_submenu_active = false;
  screensaver_submenu_active = false;
  oled_submenu_initialized = false;
  invalidateOledRenderSnapshot();
  return true;
}

bool handleOledSubmenu(
    uint8_t selected_visible,
    uint8_t scroll_offset,
    bool bottom_right,
    bool ctrlDownJust,
    bool ctrlUpJust,
    bool ctrlLeftJust,
    bool ctrlAJust,
    bool ctrlRightJust,
    bool ctrlBJust,
    bool ctrlStartJust,
    bool btnNavigateJustPressed,
    bool btnChangeJustPressed) {
  if (!oled_submenu_active) {
    oled_submenu_initialized = false;
    return false;
  }

  if (!oled_submenu_initialized) {
    oled_submenu_initialized = true;
    oled_timer = screensaverModeTimer(menu_screensaver);
    oled_dim = screensaverModeDims(menu_screensaver);
    oled_page_save = false;
    menu_screensaver_anim = sanitizeScreensaverAnimation(menu_screensaver_anim);
    screensaver_submenu_active = false;
    invalidateOledRenderSnapshot();
    display.clear();
  }

  const bool activate = ctrlAJust || btnChangeJustPressed;
  const bool down = ctrlDownJust || btnNavigateJustPressed;

  if (screensaver_submenu_active) {
    const uint8_t count = pickerCount();
    ListCursor cursor{screensaver_submenu_cursor, picker_save};
    const OledRenderSnapshot current = currentOledRender(true, cursor.index, cursor.save);
    if (!oledRenderSnapshotMatches(current)) {
      renderScreensaverPicker(cursor.index, cursor.save);
      captureOledRenderSnapshot(current);
    }

    const bool onBottom = cursor.index >= count;
    if (down) {
      moveDown(cursor, count);
      buzzer.playMenuNav();
    } else if (ctrlUpJust) {
      moveUp(cursor, count);
      buzzer.playMenuNav();
    } else if (onBottom && (ctrlLeftJust || ctrlRightJust)) {
      cursor.save = ctrlRightJust;
      buzzer.playMenuNav();
    } else if (ctrlStartJust || (activate && onBottom && cursor.save)) {
      applyPickerChoice();
      buzzer.playMenuNav();
      closeScreensaverPicker();
      return true;
    } else if (ctrlBJust || (activate && onBottom)) {
      buzzer.playMenuNav();
      closeScreensaverPicker();
      return true;
    } else if (activate) {
      picker_choice = cursor.index;
      buzzer.playMenuNav();
    }
    screensaver_submenu_cursor = cursor.index;
    picker_save = cursor.save;
    return true;
  }

  OledRow rows[kOledRowCapacity];
  const uint8_t count = buildOledRows(rows);
  ListCursor cursor{oled_submenu_cursor, oled_page_save};
  if (cursor.index > count) cursor.index = count;

  const OledRenderSnapshot current = currentOledRender(false, cursor.index, cursor.save);
  if (!oledRenderSnapshotMatches(current)) {
    renderOledPage(rows, count, cursor.index, cursor.save);
    captureOledRenderSnapshot(current);
  }

  const bool onBottom = cursor.index >= count;
  if (ctrlStartJust || (activate && onBottom && cursor.save)) {
    saveOledSubmenu();
    closeOledSubmenu(selected_visible, scroll_offset, bottom_right);
    return true;
  }
  if (ctrlBJust || (activate && onBottom)) {
    cancelOledSubmenu();
    closeOledSubmenu(selected_visible, scroll_offset, bottom_right);
    return true;
  }

  if (down) {
    moveDown(cursor, count);
    buzzer.playMenuNav();
  } else if (ctrlUpJust) {
    moveUp(cursor, count);
    buzzer.playMenuNav();
  } else if (onBottom && (ctrlLeftJust || ctrlRightJust)) {
    cursor.save = ctrlRightJust;
    buzzer.playMenuNav();
  } else if (!onBottom) {
    const OledRow row = rows[cursor.index];
    const bool adjust = (row == OLED_ROW_INACTIVE || row == OLED_ROW_WAKE) &&
                        (ctrlLeftJust || ctrlRightJust);
    if (activate || adjust) {
      switch (row) {
        case OLED_ROW_INACTIVE:
          menu_oled_inactive = menu_oled_inactive ? 0 : 1;
          buzzer.playMenuNav();
          break;
        case OLED_ROW_WAKE:
          menu_oled_wake = menu_oled_wake ? 0 : 1;
          buzzer.playMenuNav();
          break;
        case OLED_ROW_TIMER:
          openMenuChoiceList({"Inactive timer", OLED_INACTIVE_TIMER_COUNT, oled_timer,
                              timerChoiceLabel, chooseTimer, invalidateOledRenderSnapshot});
          buzzer.playMenuEnter();
          break;
        case OLED_ROW_SCREENSAVER:
          screensaver_submenu_active = true;
          picker_choice = pickerEntryForCurrent();
          screensaver_submenu_cursor = picker_choice;
          picker_save = false;
          invalidateOledRenderSnapshot();
          display.clear();
          buzzer.playMenuEnter();
          break;
      }
    }
  }
  oled_submenu_cursor = cursor.index;
  oled_page_save = cursor.save;
  return true;
}
