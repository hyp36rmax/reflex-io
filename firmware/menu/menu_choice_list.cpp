#include "../product_config.h"

#include "menu_choice_list.h"

#include <stdio.h>

#include "menu.h"
#include "menu_catalog.h"
#include "menu_descriptors.h"
#include "menu_helpers.h"
#include "menu_list_navigation.h"
#include "menu_mode_state.h"
#include "menu_ui_state.h"
#include "menu_working_state.h"
#include "../core/settings_registry.h"
#include "../output/output_runtime_state.h"
#include "../output/switch/switch_device_selection.h"
#include "../platform/buzzer.h"

namespace {

constexpr uint8_t kVisibleRows = 6;
constexpr uint8_t kDisplayContrastValues[] = {64, 128, 192, 254};
constexpr uint8_t kDisplayContrastCount = sizeof(kDisplayContrastValues);

MenuChoiceList active_list{};
bool list_active = false;
bool list_needs_render = false;
uint8_t list_cursor = 0;

const MenuItemDescriptor* list_descriptor = nullptr;

void renderList() {
  display.setFont(System5x7);
  display.set1X();
  printCenteredMenuTitle(display, active_list.title);
  const uint8_t first = menu_list_navigation::firstVisible(list_cursor, active_list.count, kVisibleRows);
  for (uint8_t row = 0; row < kVisibleRows; ++row) {
    const uint8_t entry = first + row;
    if (entry >= active_list.count) {
      display.setCursor(0, row + 1);
      display.clearToEOL();
      continue;
    }
    printMenuChoiceRow(display, row + 1, entry == list_cursor, entry == active_list.current,
                       active_list.label(entry));
    display.clearToEOL();
  }
  display.setCursor(0, 7);
  display.clearToEOL();
  display.setCursor(0, 7);
  printMenuButton(display, "Back", list_cursor == active_list.count);
}

void leaveList() {
  list_active = false;
  display.clear();
  if (active_list.closed) active_list.closed();
}

bool enumValueOffered(const MenuItemDescriptor& desc, uint8_t value) {
  // A preference the image cannot honour falls back to something else, so a
  // list that named it would promise an output the host never gets.
  return (desc.setting_id != SettingId::WinOutput && desc.setting_id != SettingId::MisterOutput) ||
         host_output_preference_available(value);
}

uint8_t enumMaxValue(const MenuItemDescriptor& desc) {
  return desc.enum_cfg.get_max ? desc.enum_cfg.get_max() : (desc.enum_cfg.count - 1);
}

uint8_t enumChoiceCount(const MenuItemDescriptor& desc) {
  uint8_t count = 0;
  for (uint8_t value = 0; value <= enumMaxValue(desc); ++value) {
    if (enumValueOffered(desc, value)) ++count;
  }
  return count;
}

uint8_t enumChoiceValue(const MenuItemDescriptor& desc, uint8_t index) {
  for (uint8_t value = 0; value <= enumMaxValue(desc); ++value) {
    if (enumValueOffered(desc, value) && index-- == 0) return value;
  }
  return 0;
}

uint8_t enumChoiceIndex(const MenuItemDescriptor& desc, uint8_t stored) {
  uint8_t index = 0;
  for (uint8_t value = 0; value <= enumMaxValue(desc); ++value) {
    if (!enumValueOffered(desc, value)) continue;
    if (value == stored) return index;
    ++index;
  }
  return 0xFF;
}

const char* enumChoiceLabel(uint8_t index) {
  const uint8_t value = enumChoiceValue(*list_descriptor, index);
  return list_descriptor->setting_id == SettingId::NsoSpecial
      ? switch_device_selection_label(menu_input, value)
      : list_descriptor->enum_cfg.names[value];
}

void chooseEnumValue(uint8_t index) {
  *list_descriptor->menu_var = enumChoiceValue(*list_descriptor, index);
  if (list_descriptor->on_change) list_descriptor->on_change();
}

const char* contrastChoiceLabel(uint8_t index) {
  static char label[5];
  const uint8_t value = kDisplayContrastValues[index];
  snprintf(label, sizeof(label), "%u%%", value >= 254 ? 100u : (value * 100u) / 255u);
  return label;
}

void chooseContrast(uint8_t index) {
  menu_display_contrast = kDisplayContrastValues[index];
  display.setContrast(menu_display_contrast);
}

uint8_t contrastChoiceIndex() {
  for (uint8_t i = 0; i < kDisplayContrastCount; ++i) {
    if (kDisplayContrastValues[i] == menu_display_contrast) return i;
  }
  return 0xFF;
}

void redrawSystemMenu() {
  renderMenu(menu_selected_visible, menu_scroll_offset, menu_bottom_right_selected);
}

const char* itemTitle(menu_item_enum item) {
  for (uint8_t i = 0; i < MENU_TOTAL_ITEMS; ++i) {
    if (menu_items[i].item == item) return menu_items[i].text;
  }
  return "";
}

}  // namespace

void openMenuChoiceList(const MenuChoiceList& list) {
  active_list = list;
  list_cursor = list.current < list.count ? list.current : 0;
  list_active = true;
  list_needs_render = true;
  display.clear();
}

bool menuChoiceListActive() {
  return list_active;
}

void closeMenuChoiceList() {
  list_active = false;
}

void finishMenuChoiceListForHold(bool save) {
  if (!list_active) return;
  list_active = false;
  if (save && list_cursor < active_list.count) active_list.choose(list_cursor);
}

bool handleMenuChoiceList(const MenuControllerInput& ctrl, bool navigatePressed, bool changePressed) {
  if (!list_active) return false;
  if (list_needs_render) {
    list_needs_render = false;
    renderList();
  }

  const uint8_t rows = active_list.count + 1;  // The Back row is the last.
  const bool activate = ctrl.aJust || changePressed;
  if (ctrl.downJust || navigatePressed) {
    list_cursor = (list_cursor + 1) % rows;
    buzzer.playMenuNav();
    renderList();
  } else if (ctrl.upJust) {
    list_cursor = (list_cursor + rows - 1) % rows;
    buzzer.playMenuNav();
    renderList();
  } else if (ctrl.bJust || (activate && list_cursor == active_list.count)) {
    buzzer.playMenuNav();
    leaveList();
  } else if (activate) {
    active_list.choose(list_cursor);
    buzzer.playMenuNav();
    leaveList();
  }
  return true;
}

bool menuItemOpensChoiceList(menu_item_enum item) {
  if (item == menu_item_display_contrast) return true;
  if (!is_system_setting(item)) return false;
  const MenuItemDescriptor* desc = get_menu_descriptor(item);
  return desc && desc->type == MENU_TYPE_ENUM &&
         enumChoiceCount(*desc) >= kMenuChoiceListMinValues;
}

void openMenuItemChoiceList(menu_item_enum item) {
  if (item == menu_item_display_contrast) {
    openMenuChoiceList({itemTitle(item), kDisplayContrastCount, contrastChoiceIndex(),
                        contrastChoiceLabel, chooseContrast, redrawSystemMenu});
    return;
  }
  list_descriptor = get_menu_descriptor(item);
  openMenuChoiceList({itemTitle(item), enumChoiceCount(*list_descriptor),
                      enumChoiceIndex(*list_descriptor, *list_descriptor->menu_var),
                      enumChoiceLabel, chooseEnumValue, redrawSystemMenu});
}
