#include "menu_item_handlers_internal.h"

#include "../features/feature_module.h"
#include "../platform/runtime/platform_menu_runtime.h"
#include "menu_product_pages.h"
#include "menu_mode_lists.h"

void handleMenuItemAction(menu_item_enum item, menu_item_action action) {
  if (handleMenuModeListsItem(item, action)) return;
  if (item == menu_item_kiosk_mode &&
      (action == item_action_display ||
       action == item_action_change ||
       action == item_action_change_prev)) {
    menu_item_handlers_internal::handle_item_kiosk_mode(action);
    return;
  }

  const MenuItemDescriptor* desc = get_menu_descriptor(item);
  if (desc) {
    menu_item_handle_generic(*desc, action);
    return;
  }

  if (featureModulesHandleMenuItem(item, action)) {
    return;
  }

  switch (item) {
    case menu_item_players:
    case menu_item_controller_leds:
    case menu_item_bluetooth_devices:
    case menu_item_web_config:
    case menu_item_mapping_profiles:
    case menu_item_auth_status:
    case menu_item_device_leds:
    case menu_item_diagnostics:
    case menu_item_output_transport:
      if (action == item_action_display) {
#ifdef USE_I2C_DISPLAY
        display.print(">");
#endif
      } else if (action == item_action_change ||
                 action == item_action_change_prev) {
        openProductMenuPage(static_cast<uint8_t>(item - menu_item_players));
      }
      break;
    case menu_item_controller_settings:
      if (action == item_action_display) {
#ifdef USE_I2C_DISPLAY
        display.print(">");
#endif
      } else if (action == item_action_change ||
                 action == item_action_change_prev) {
#if ADAPT_POLICY_SYSTEM_MENU_USB_INPUT
        openProductControllerSettingsPage();
#else
        openCombinedControllerSettingsMenu();
#endif
      }
      break;
    case menu_item_input_mode:
      handle_item_input_mode(action);
      break;
    case menu_item_output_mode:
      handle_item_output_mode(action);
      break;
    case menu_item_display_contrast:
      menu_item_handlers_internal::handle_item_display_contrast(action);
      break;
    case menu_item_oled:
      menu_item_handlers_internal::handle_item_oled(action);
      break;
    case menu_item_games:
      menu_item_handlers_internal::handle_item_games(action);
      break;
    case menu_item_buzzer:
      menu_item_handlers_internal::handle_item_buzzer(action);
      break;
    case menu_item_hotkeys:
      menu_item_handlers_internal::handle_item_hotkeys(action);
      break;
    case menu_item_factory_reset:
      menu_item_handlers_internal::handle_item_factory_reset(action);
      break;
    case menu_item_bootloader:
      menu_item_handlers_internal::handle_item_bootloader(action);
      break;
    case menu_item_button_remap:
      menu_item_handlers_internal::handle_item_button_remap(action);
      break;
    case menu_item_mapping_view:
      menu_item_handlers_internal::handle_item_mapping_view(action);
      break;
    case menu_item_pad_test:
      menu_item_handlers_internal::handle_item_pad_test(action);
      break;
    case menu_item_pin_debug:
      menu_item_handlers_internal::handle_item_pin_debug(action);
      break;
    case menu_item_analog_test:
      menu_item_handlers_internal::handle_item_analog_test(action);
      break;
    case menu_item_jvs_status:
      menu_item_handlers_internal::handle_item_jvs_status(action);
      break;
#if defined(ADAPT_BRIDGE_JVS_TO_USB)
    case menu_item_jvs_panel:
      menu_item_handlers_internal::handle_item_jvs_panel(action);
      break;
#endif
    case menu_item_latency_run:
      menu_item_handlers_internal::handle_item_latency_run(action);
      break;
    case menu_item_guncon_offset:
      menu_item_handlers_internal::handle_item_guncon_offset(action);
      break;
    case menu_item_jogcon_force:
      menu_item_handlers_internal::handle_item_jogcon_force(action);
      break;
    case menu_item_wheel_sensitivity:
      menu_item_handlers_internal::handle_item_wheel_sensitivity(action);
      break;
    case menu_item_about:
      menu_item_handlers_internal::handle_item_about(action);
      break;
    case menu_item_whats_new:
      menu_item_handlers_internal::handle_item_whats_new(action);
      break;
    default:
      break;
  }
}
