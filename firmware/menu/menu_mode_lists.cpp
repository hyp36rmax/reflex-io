#include "../product_config.h"
#include "menu_mode_lists.h"

#if defined(ADAPT_FEATURE_FIRST_BOOT_SETUP)
#include "../core/settings_store.h"
#include "../output/output_runtime_state.h"
#include "../platform/buzzer.h"
#include "../platform/display_runtime_state.h"
#include "menu_helpers.h"
#include "menu_list_navigation.h"
#include "menu_input_mode.h"
#include "menu_mode_state.h"
#include "menu_setup_wizard.h"

namespace {
enum Page : uint8_t { Closed, Root, Inputs, Outputs, PickInput, PickOutput };
Page page = Closed;
uint8_t cursor = 0;
bool redraw = true;
MenuModeVisibilityRecord working{};

uint8_t count() {
  return page == Root ? 3 :
         page == Inputs || page == PickInput ? menuInputChoiceCount(page == Inputs) :
         menuOutputChoiceCount(page == Outputs);
}

void render() {
  display.clear();
  display.setFont(System5x7);
  display.set1X();
  printCenteredMenuTitle(display, page == Root ? "Manual mode lists" :
      page == Inputs ? "Show inputs" : page == Outputs ? "Show outputs" :
      page == PickInput ? "Input mode" : "Output mode");
  const uint8_t n = count();
  const uint8_t first = menu_list_navigation::firstVisible(cursor, n, 5);
  for (uint8_t row = 0; row < 5 && first + row < n; ++row) {
    const uint8_t i = first + row;
    display.setCursor(0, row + 1);
    display.print(cursor == i ? '>' : ' ');
    const char* name;
    if (page == Root) {
      name = i == 0 ? "Inputs >" : i == 1 ? "Outputs >" : "Show all";
    } else {
      const bool input = page == Inputs || page == PickInput;
      const uint8_t mode = input ? (uint8_t)menuInputChoiceAt(i, page == Inputs) :
                                  (uint8_t)menuOutputChoiceAt(i, page == Outputs);
      name = input ? getInputModeName((DeviceEnum)mode) : get_mode_name((outputMode_t)mode);
      if (page == Inputs || page == Outputs) {
        const bool hidden = ((input ? working.hidden_input_modes : working.hidden_output_modes) >> mode) & 1u;
        display.print(hidden ? "[ ]" : "[x]");
      } else {
        display.print(mode == (input ? (uint8_t)menu_input : (uint8_t)menu_output) ? '*' : ' ');
      }
    }
    const uint8_t limit = page == Inputs || page == Outputs ? 17 : 19;
    for (uint8_t c = 0; name[c] && c < limit; ++c) display.print(name[c]);
  }
  display.setCursor(0, 6);
  display.print(page == Inputs || page == Outputs ? "Auto stays available" : "Changes need Save");
  display.setCursor(0, 7);
  printMenuButton(display, "Back", cursor == n);
  redraw = false;
}
}  // namespace

bool handleMenuModeListsItem(menu_item_enum item, menu_item_action action) {
  if (item == menu_item_setup_guide) {
    if (action == item_action_change) beginSetupWizard();
    return true;
  }
  if (item != menu_item_mode_lists && item != menu_item_input_mode && item != menu_item_output_mode) return false;
  if (item == menu_item_mode_lists && action == item_action_reset) {
    (void)readMenuModeVisibility(working);
  } else if (action == item_action_change) {
    page = item == menu_item_mode_lists ? Root : item == menu_item_input_mode ? PickInput : PickOutput;
    cursor = 0;
    if (page == PickInput || page == PickOutput) {
      for (uint8_t i = 0; i < count(); ++i) {
        if (page == PickInput ? menuInputChoiceAt(i) == menu_input : menuOutputChoiceAt(i) == menu_output) cursor = i;
      }
    }
    redraw = true;
  } else {
    return item == menu_item_mode_lists;
  }
  return true;
}

bool handleMenuModeLists(const MenuControllerInput& ctrl, bool navigate, bool select,
                         uint8_t selected, uint8_t scroll, bool bottomRight) {
  if (page == Closed) return false;
  const uint8_t n = count();
  if (redraw) render();
  if (navigate || ctrl.downJust) {
    cursor = (cursor + 1) % (n + 1);
  } else if (ctrl.upJust) {
    cursor = cursor == 0 ? n : cursor - 1;
  } else if (ctrl.bJust || ((select || ctrl.aJust) && cursor == n)) {
    if (page == Inputs || page == Outputs) { cursor = page == Inputs ? 0 : 1; page = Root; }
    else { page = Closed; renderMenu(selected, scroll, bottomRight); return true; }
  } else if (select || ctrl.aJust) {
    if (page == Root) {
      if (cursor == 2) working = MenuModeVisibilityRecord{};
      else { page = cursor == 0 ? Inputs : Outputs; cursor = 0; }
    } else if (page == Inputs || page == Outputs) {
      const bool input = page == Inputs;
      const uint8_t mode = input ? (uint8_t)menuInputChoiceAt(cursor, true) : (uint8_t)menuOutputChoiceAt(cursor, true);
      const bool autoMode = input ? mode == (uint8_t)RZORD_AUTODETECT : mode == (uint8_t)OUTPUT_AUTO;
      if (!autoMode) (input ? working.hidden_input_modes : working.hidden_output_modes) ^= uint32_t(1) << mode;
    } else {
      if (page == PickInput) menu_input = menuInputChoiceAt(cursor);
      else menu_output = menuOutputChoiceAt(cursor);
      page = Closed;
      renderMenu(selected, scroll, bottomRight);
      return true;
    }
  } else return true;
  redraw = true;
  buzzer.playMenuNav();
  return true;
}

bool menuModeListsActive() { return page != Closed; }
void finishMenuModePickerForHold(bool save) {
  if (save && (page == PickInput || page == PickOutput) && cursor < count()) {
    if (page == PickInput) menu_input = menuInputChoiceAt(cursor);
    else menu_output = menuOutputChoiceAt(cursor);
  }
  page = Closed;
}
void closeMenuModeLists() { page = Closed; }
bool saveMenuModeListsPreferences() { return saveMenuModeVisibility(working); }
#else
bool handleMenuModeListsItem(menu_item_enum, menu_item_action) { return false; }
bool handleMenuModeLists(const MenuControllerInput&, bool, bool, uint8_t, uint8_t, bool) { return false; }
bool menuModeListsActive() { return false; }
void finishMenuModePickerForHold(bool) {}
void closeMenuModeLists() {}
bool saveMenuModeListsPreferences() { return true; }
#endif
