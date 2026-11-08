#include "../product_config.h"

#include "menu_navigation.h"

#include "../platform/buzzer.h"
#include "menu_choice_list.h"
#include "menu_descriptors.h"
#include "menu_helpers.h"
#include "menu_mode_state.h"
#include "menu_ui_state.h"
#include "menu_list_navigation.h"
#include "../output/output_runtime_state.h"

bool menuItemSupportsDirectionalAdjustment(menu_item_enum item) {
#if defined(ADAPT_FEATURE_FIRST_BOOT_SETUP)
  if (item == menu_item_input_mode || item == menu_item_output_mode) return false;
#endif
  // Kiosk has a descriptor for its displayed state, but opens a choice page.
  if (item == menu_item_kiosk_mode || menuItemOpensChoiceList(item)) {
    return false;
  }

  if (get_menu_descriptor(item)) {
    return true;
  }

  switch (item) {
    case menu_item_input_mode:
    case menu_item_output_mode:
    case menu_item_jogcon_force:
    case menu_item_wheel_sensitivity:
      return true;
    default:
      return false;
  }
}

bool handleMenuValueChange(
    menu_item_enum current_item,
    menu_item_action action,
    uint8_t& selected_visible,
    uint8_t& scroll_offset,
    bool bottom_right,
    uint8_t item_rows
) {
  if (menu_input != deviceMode && is_mode_specific_setting(current_item)) {
    buzzer.playError();
    return true;
  }

  if (menuItemOpensChoiceList(current_item)) {
    if (action == item_action_change) {
      openMenuItemChoiceList(current_item);
      buzzer.playMenuEnter();
    }
    return true;
  }

  handleMenuItemAction(current_item, action);
  if (current_item == menu_item_mode_lists || current_item == menu_item_setup_guide) return true;

  if (current_item == menu_item_factory_reset && factory_reset_stage != 0) {
    return true;
  }
  if (current_item == menu_item_bootloader && bootloader_stage == 1) {
    return true;
  }
  if ((current_item == menu_item_about && about_screen_active) ||
      (current_item == menu_item_whats_new && whats_new_screen_active)) {
    return true;
  }

  if (current_item == menu_item_output_mode || current_item == menu_item_input_mode) {
    buzzer.playModeChange();
  } else if (current_item != menu_item_factory_reset &&
             current_item != menu_item_bootloader &&
             current_item != menu_item_pad_test &&
             current_item != menu_item_about &&
             current_item != menu_item_whats_new) {
    buzzer.playMenuNav();
  }

  if (current_item == menu_item_output_mode || current_item == menu_item_input_mode) {
    uint8_t new_visible_count = getVisibleItemCount();
    if (selected_visible >= new_visible_count)
      selected_visible = new_visible_count - 1;
    scroll_offset = menu_list_navigation::firstVisible(selected_visible, new_visible_count, item_rows);
  }

  renderMenu(selected_visible, scroll_offset, bottom_right);
  return false;
}
