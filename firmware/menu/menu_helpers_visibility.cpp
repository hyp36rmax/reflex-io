#include "../product_config.h"

#include "menu_helpers.h"

#include <cstring>

#include "../core/button_map_mode.h"
#include "../core/controller_frame_state.h"
#include "../core/controller_selection_state.h"
#include "../core/device_runtime_state.h"
#include "../core/settings_registry.h"
#include "../features/feature_module.h"
#include "../input/runtime/input_frame_runtime.h"
#include "../output/output_runtime_state.h"
#include "menu_catalog.h"
#include "menu_capabilities.h"
#include "menu_mode_state.h"
#include "menu_runtime_state.h"
#include "menu_ui_state.h"
#include "menu_working_state.h"

namespace {

bool menuInputIsPsx() {
#ifdef ENABLE_INPUT_PSX
  return menu_input == RZORD_PSX;
#else
  return false;
#endif
}

bool menuInputIsNes() {
#ifdef ENABLE_INPUT_NES
  return menu_input == RZORD_NES;
#else
  return false;
#endif
}

bool menuInputIsSnes() {
#ifdef ENABLE_INPUT_SNES
  return menu_input == RZORD_SNES;
#else
  return false;
#endif
}

bool menuInputIsN64() {
#ifdef ENABLE_INPUT_N64
  return menu_input == RZORD_N64;
#else
  return false;
#endif
}

bool menuInputIsGameCube() {
#ifdef ENABLE_INPUT_GAMECUBE
  return menu_input == RZORD_GAMECUBE;
#else
  return false;
#endif
}

bool menuInputIsWii() {
#ifdef ENABLE_INPUT_WII
  return menu_input == RZORD_WII;
#else
  return false;
#endif
}

bool menuInputSupportsSelectableButtonMapMode() {
  const outputMode_t selectedOutput =
    (menu_output == OUTPUT_AUTO) ? get_effective_output_mode() : menu_output;
  return buttonMapModeIsUserSelectable(
    menu_input, selectedOutput, menu_nso_special != 0);
}

}  // namespace

bool should_hide_menu_item(menu_item_enum item) {
  if (item == menu_item_ok_back) {
    return !settingSupportedOnProduct(SettingId::MenuOkBack);
  }
#ifdef PRODUCT_JVSIO2USB
  if (item == menu_item_jvs_status) return false;
#else
  if (item == menu_item_jvs_status) return true;
#endif
#if defined(ADAPT_BRIDGE_JVS_TO_USB)
  if (item == menu_item_jvs_panel) return true;
  // Quick Analog Setup owns controls testing; JVS Status opens axis diagnostics.
  if (item == menu_item_analog_test) return true;
#endif
#if ADAPT_POLICY_SYSTEM_MENU_USB_INPUT
  // MODERN2USB owns Bluetooth management through its dedicated submenu. Apply
  // this before feature modules can expose the shared legacy top-level items.
  if (item == menu_item_bluetooth_pairing ||
      item == menu_item_bluetooth_forget) {
    return true;
  }
#endif

  bool featureHidden = false;
  if (featureModulesShouldHideMenuItem(item, &featureHidden)) {
    return featureHidden;
  }

#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
  if (item == menu_item_auth_status) return false;
  if (item == menu_item_hotkeys || item == menu_item_games) return true;
#endif

  if (system_menu_only &&
      (item == menu_item_input_mode ||
       item == menu_item_output_mode ||
       is_mode_specific_setting(item))) {
    return true;
  }

  if (menu_input != deviceMode && is_mode_specific_setting(item)) {
    return true;
  }

#if ADAPT_POLICY_SYSTEM_MENU_USB_INPUT
  // A short Mode press owns controller selection and per-controller settings.
  // The held system menu contains only device-wide controls.
  if (item == menu_item_controller_settings ||
      item == menu_item_players) {
    return true;
  }
  if (item == menu_item_controller_leds) return false;
  if (item == menu_item_games ||
      item == menu_item_kiosk_mode) {
    return true;
  }
  if (item == menu_item_input_mode ||
      is_mode_specific_setting(item)) {
    return true;
  }
#if !ADAPT_POLICY_USB_BT_OUTPUT_SETTINGS
  // No Bluetooth output in this image, so nothing to choose.
  if (item == menu_item_output_transport) return true;
#endif
#else
  if (item == menu_item_players ||
      item == menu_item_controller_leds ||
      item == menu_item_bluetooth_devices ||
      item == menu_item_web_config ||
      item == menu_item_mapping_profiles ||
      item == menu_item_auth_status ||
      item == menu_item_device_leds ||
      item == menu_item_diagnostics ||
      item == menu_item_output_transport) {
    return true;
  }
  if (item == menu_item_controller_settings) {
    return true;
  }
#endif

  #if !defined(ADAPT_FEATURE_RGB_LED)
  if (item == menu_item_device_leds) {
    return true;
  }
  #endif

  if (item == menu_item_home_button_labels) {
    return true;
  }

  switch (item) {
    case menu_item_home_screen:
      return true;
#if !defined(ADAPT_FEATURE_BUZZER)
    case menu_item_buzzer:
      return true;
#endif
    case menu_item_hotkey_hold_time:
    case menu_item_menu_hotkey:
    case menu_item_system_menu_hotkey:
    case menu_item_home_hotkey:
    case menu_item_hold_start_hotkey:
    case menu_item_capture_hotkey:
      return true;
#if !defined(ENABLE_BLUETOOTH_PAIRING)
    case menu_item_bluetooth_pairing:
    case menu_item_bluetooth_forget:
      return true;
#endif
#if defined(ADAPT_PRIMARY_INGRESS_JVS) || defined(ADAPT_PRIMARY_INGRESS_USB_DEVICE)
    case menu_item_input_mode:
      return true;
#endif
    #if ADAPT_POLICY_SYSTEM_MENU_JVS_INPUT
    case menu_item_home_button_labels:
    case menu_item_mapping_view:
    case menu_item_rumble:
      return true;
    #endif
#if defined(ADAPT_PRIMARY_INGRESS_USB_DEVICE) && defined(ADAPT_PRIMARY_EGRESS_USB_HOST)
    case menu_item_home_button_labels:
      return true;
#endif
    case menu_item_nso_special: {
      const outputMode_t selectedOutput =
        (menu_output == OUTPUT_AUTO) ? get_effective_output_mode() : menu_output;
      return !output_mode_supports_switch_device_selection(selectedOutput) ||
             !input_mode_supports_nso_special(menu_input) ||
             !input_mode_allows_nso_special_for_connected_players(
               menu_input, connectedInputFrameCount(inputPortCount()));
    }
    case menu_item_analog_deadzone:
    case menu_item_stick_invert:
      return !input_has_analog();
    case menu_item_analog_test:
#ifdef PRODUCT_JVSIO2USB
      return false;
#else
      return !menuModeHasLiveAnalogDiagnostic(menu_input);
#endif
    case menu_item_trigger_mode:
      return !input_has_analog_triggers() || menu_output == OUTPUT_SWITCHPRO ||
             menu_output == OUTPUT_SWITCH2PRO;
    case menu_item_switch_rtrig_stick:
      return !((menu_output == OUTPUT_SWITCHPRO ||
                menu_output == OUTPUT_SWITCH2PRO) &&
               input_has_analog_triggers() &&
               menuModeSupportsTriggerAxisMode(menu_input));
    case menu_item_gamecube_l_switch:
    {
      const outputMode_t gameCubeOutput = menu_output == OUTPUT_AUTO
          ? get_effective_output_mode()
          : menu_output;
      return !(menuInputIsGameCube() &&
               (gameCubeOutput == OUTPUT_SWITCHPRO ||
                gameCubeOutput == OUTPUT_SWITCH2PRO));
    }
    case menu_item_guncon_offset:
      return !menuInputIsPsx();
    case menu_item_powerpad:
      return !menuInputIsNes();
    case menu_item_jogcon_force:
      if (menuInputIsPsx()) return false;
      if (strncmp(selectedControllerFrame().controller_type_name, "JogCon", 6) == 0) return false;
      if (outputMode == OUTPUT_MISTER_JOGCON) return false;
      return true;
    case menu_item_jogcon_mode:
      return true;
    case menu_item_wheel_sensitivity:
      if (strncmp(selectedControllerFrame().controller_type_name, "JogCon", 6) == 0) return false;
      if (outputMode == OUTPUT_MISTER_JOGCON) return false;
      return true;
    case menu_item_jogcon_digital:
    case menu_item_jogcon_wheel_axis:
      if (menuInputIsPsx()) return false;
      if (strncmp(selectedControllerFrame().controller_type_name, "JogCon", 6) == 0) return false;
      if (outputMode == OUTPUT_MISTER_JOGCON) return false;
      return true;
    case menu_item_button_map:
      return !menuInputSupportsSelectableButtonMapMode();
    case menu_item_n64_z_mode: {
      const outputMode_t selectedOutput =
        (menu_output == OUTPUT_AUTO) ? get_effective_output_mode() : menu_output;
      return !menuShouldShowN64ZMode(
        menu_input, selectedOutput, is_nso_special_active());
    }
    case menu_item_n64_cstick_mode: {
      if (is_nso_special_active()) return true;
      return !menuInputIsN64();
    }
    case menu_item_wii_analog_range:
      return !(menuInputIsWii() || menuInputIsGameCube());
    case menu_item_n64_analog_range:
      return true;
    case menu_item_latency_test:
    #if defined(ADAPT_ENABLE_LATENCY_TEST)
      return false;
    #else
      return true;
    #endif
    case menu_item_latency_controller:
    case menu_item_latency_host:
    case menu_item_latency_run:
    #if defined(ADAPT_ENABLE_LATENCY_TEST)
      return menu_latency_test == 0;
    #else
      return true;
    #endif
    case menu_item_win_output:
      #if defined(ADAPT_PRIMARY_EGRESS_DB15)
      return true;
      #else
      return false;
      #endif
    case menu_item_mister_output:
      #if defined(ADAPT_PRIMARY_EGRESS_DB15)
      return true;
      #else
      return false;
      #endif
    case menu_item_switch2_output:
      return !settingSupportedOnProduct(SettingId::Switch2Output);
    case menu_item_ps2_remote_output:
      return !settingSupportedOnProduct(SettingId::Ps2RemoteOutput);
    case menu_item_mister_display:
      return !settingSupportedOnProduct(SettingId::MisterDisplay);
    case menu_item_psx_periph:
      return true;
    case menu_item_socd:
#if !ADAPT_POLICY_QUICK_SHOW_SOCD
      return true;
#else
      return false;
#endif
    case menu_item_dpad_as_buttons:
      if (is_nso_special_active()) return true;
      return false;
#if !ADAPT_POLICY_SYSTEM_MENU_JVS_INPUT
    case menu_item_rumble:
      return !input_has_variable_rumble_strength();
#endif
    case menu_item_hotkeys:
      #if defined(ADAPT_PRIMARY_EGRESS_DB15)
      return true;
      #else
      return false;
      #endif
    case menu_item_kiosk_mode:
      return false;
    default:
      return false;
  }
}

uint8_t getVisibleItemCount() {
  uint8_t count = 0;
  for (uint8_t i = 0; i < MENU_TOTAL_ITEMS; ++i) {
    if (!should_hide_menu_item(menu_items[i].item))
      ++count;
  }
  return count;
}

uint8_t getActualIndex(uint8_t visible_index) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < MENU_TOTAL_ITEMS; ++i) {
    if (!should_hide_menu_item(menu_items[i].item)) {
      if (count == visible_index)
        return i;
      ++count;
    }
  }
  return 0;
}

uint8_t getVisibleIndex(uint8_t actual_index) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < actual_index; ++i) {
    if (!should_hide_menu_item(menu_items[i].item))
      ++count;
  }
  return count;
}

bool is_mode_specific_setting(menu_item_enum item) {
  switch (item) {
    case menu_item_analog_deadzone:
    case menu_item_analog_test:
    case menu_item_dpad_as_buttons:
    case menu_item_socd:
    case menu_item_nso_special:
    case menu_item_stick_invert:
    case menu_item_rumble:
    case menu_item_trigger_mode:
    case menu_item_switch_rtrig_stick:
    case menu_item_gamecube_l_switch:
    case menu_item_button_remap:
    case menu_item_mapping_view:
    case menu_item_guncon_offset:
    case menu_item_powerpad:
    case menu_item_jogcon_force:
    case menu_item_wheel_sensitivity:
    case menu_item_jogcon_digital:
    case menu_item_jogcon_wheel_axis:
    case menu_item_button_map:
    case menu_item_n64_z_mode:
    case menu_item_n64_cstick_mode:
    case menu_item_wii_analog_range:
      return true;
    default:
      return false;
  }
}

bool is_per_input_quick_setting(menu_item_enum item) {
  switch (item) {
    case menu_item_analog_deadzone:
    case menu_item_dpad_as_buttons:
    case menu_item_socd:
    case menu_item_nso_special:
    case menu_item_stick_invert:
    case menu_item_rumble:
    case menu_item_trigger_mode:
    case menu_item_switch_rtrig_stick:
    case menu_item_gamecube_l_switch:
    case menu_item_button_map:
    case menu_item_n64_z_mode:
    case menu_item_n64_cstick_mode:
    case menu_item_wii_analog_range:
    case menu_item_guncon_offset:
    case menu_item_powerpad:
    case menu_item_jogcon_force:
    case menu_item_wheel_sensitivity:
    case menu_item_jogcon_digital:
    case menu_item_jogcon_wheel_axis:
      return true;
    default:
      return false;
  }
}

bool is_system_setting(menu_item_enum item) {
  switch (item) {
    case menu_item_ps2_ir_p1:
    case menu_item_ps2_ir_p2:
    case menu_item_n64_capture_export:
    case menu_item_n64_capture_new:
    case menu_item_host_capture_export:
    case menu_item_host_capture_new:
    case menu_item_jogcon_capture_export:
    case menu_item_jogcon_capture_new:
    case menu_item_genesis_capture_retry:
    case menu_item_genesis_capture_manual:
    case menu_item_genesis_capture_export:
    case menu_item_ok_back:
    case menu_item_management_msc:
    case menu_item_usb2db15_multi_bind:
    case menu_item_usb2db15_game_selection:
    case menu_item_display_contrast:
    case menu_item_home_screen:
    case menu_item_home_button_labels:
    case menu_item_hotkey_hold_time:
    case menu_item_buzzer:
    case menu_item_latency_test:
    case menu_item_latency_controller:
    case menu_item_latency_host:
    case menu_item_oled:
    case menu_item_win_output:
    case menu_item_mister_output:
    case menu_item_switch2_output:
    case menu_item_ps2_remote_output:
    case menu_item_mister_display:
    case menu_item_psx_periph:
    case menu_item_hotkeys:
    case menu_item_kiosk_mode:
    case menu_item_menu_hotkey:
    case menu_item_system_menu_hotkey:
    case menu_item_home_hotkey:
    case menu_item_hold_start_hotkey:
    case menu_item_capture_hotkey:
    case menu_item_bluetooth_pairing:
    case menu_item_bluetooth_forget:
      return true;
    default:
      return false;
  }
}
