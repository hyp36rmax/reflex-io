#include "../product_config.h"

#include "menu_descriptors.h"
#include "../core/settings_registry.h"
#include "../output/switch/switch_device_selection.h"

const char* const socd_names[] = { "OFF", "NEUTRAL", "SECOND", "FIRST" };
const char* const dpad_mode_names[] = { "DPad", "Left Stick", "Right Stick", "Buttons" };
const char* const stick_invert_names[] = { "Off", "Left", "Right", "Both" };
const char* const trigger_mode_names[] = { "Analog", "Digital", "RStick", "Analog+Digital" };
const char* const rumble_names[] = { "Off", "Variable", "Max" };
const char* const jogcon_mode_names[] = { "Spinner", "Paddle", "Wheel", "Fake Spin" };
const char* const jogcon_digital_names[] = { "Left/Right", "L3/R3", "L/R", "Up/Down" };
const char* const jogcon_wheel_axis_names[] = { "X", "Y" };
const char* const latency_host_names[] = { "Internal", "PC", "MiSTer" };
const char* const win_output_names[] = { "DInput", "XInput", "Keyboard", "Xbox One", "PS3", "PS4", "PS5", "Switch Pro" };
const char* const switch2_output_names[] = { "Switch Pro", "Switch2 Pro" };
const char* const ps2_remote_output_names[] = { "Gamepad", "Remote" };
const char* const mister_display_names[] = { "Artwork", "Inputs" };
const char* const psx_periph_names[] = { "MiSTer" };
const char* const n64_cstick_mode_names[] = { "Auto", "Buttons", "Stick" };
const char* const classic_analog_range_names[] = { "Raw", "Norm", "Cal", "Learn" };
const char* const nes_device_names[] = { "Pad", "Power Pad", "Vaus NES", "Vaus FC" };
const char* const hotkey_hold_time_names[] = { "Press", "0.5s", "1.0s", "1.5s", "2.0s", "2.5s", "3.0s" };
#if defined(PRODUCT_CLASSIC2USB)
// Value 0 is a reserved sentinel; choice list hides it (only 1-6 displayed).
const char* const guide_hold_time_names[] = { "-", "0.5s", "1.0s", "1.5s", "2.0s", "2.5s", "3.0s" };
#endif
const char* const switch_device_names[] = { "Switch Pro", "Genesis NSO", "NES NSO", "SNES NSO", "N64 NSO", "GC Adapter" };
const char* const menu_ok_back_names[] = { "X / A", "East", "South" };

const MenuItemDescriptor menu_descriptors[] = {
#if defined(ADAPT_FEATURE_USB_DRIVE)
  {
    .id = menu_item_management_msc,
    .type = MENU_TYPE_BOOL,
    .menu_var = &menu_management_msc,
    .runtime_var = nullptr,
    .setting_id = SettingId::ManagementMsc,
    .bool_cfg = { "Off", "On" },
    .on_change = nullptr,
    .on_apply = nullptr
  },
#endif
  {
    .id = menu_item_ok_back,
    .type = MENU_TYPE_ENUM,
    .menu_var = &menu_menu_ok_back,
    .runtime_var = &menu_ok_back,
    .setting_id = SettingId::MenuOkBack,
    .enum_cfg = { menu_ok_back_names, 3, nullptr },
    .on_change = nullptr,
    .on_apply = nullptr
  },
#if ADAPT_POLICY_FIXED_HOTKEY_ASSIGNMENTS
  {
    .id = menu_item_hotkeys,
    .type = MENU_TYPE_BOOL,
    .menu_var = &menu_home_hotkey,
    .runtime_var = nullptr,
    .setting_id = SettingId::HomeHotkey,
    .bool_cfg = { "Off", "On" },
    .on_change = nullptr,
    .on_apply = nullptr
  },
#endif
#if defined(PRODUCT_CLASSIC2USB)
  {
    .id = menu_item_guide_hold_time,
    .type = MENU_TYPE_ENUM,
    .menu_var = &menu_guide_hold_time,
    .runtime_var = nullptr,
    .setting_id = SettingId::GuideHoldTime,
    .enum_cfg = { guide_hold_time_names, 7, nullptr },
    .on_change = nullptr,
    .on_apply = nullptr
  },
#endif
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
  {
    .id = menu_item_usb2db15_multi_bind,
    .type = MENU_TYPE_BOOL,
    .menu_var = &menu_usb2db15_multi_bind,
    .runtime_var = nullptr,
    .setting_id = SettingId::Usb2Db15MultiBind,
    .bool_cfg = { "Off", "On" },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_usb2db15_game_selection,
    .type = MENU_TYPE_BOOL,
    .menu_var = &menu_usb2db15_game_selection,
    .runtime_var = nullptr,
    .setting_id = SettingId::Usb2Db15GameSelection,
    .bool_cfg = { "Off", "On" },
    .on_change = nullptr,
    .on_apply = nullptr
  },
#endif
  {
    .id = menu_item_trigger_mode,
    .type = MENU_TYPE_ENUM,
    .menu_var = &menu_trigger_mode,
    .runtime_var = &trigger_mode,
    .setting_id = SettingId::TriggerMode,
    .enum_cfg = { trigger_mode_names, 4, trigger_mode_get_max },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_nso_special,
    .type = MENU_TYPE_ENUM,
    .menu_var = &menu_nso_special,
    .runtime_var = &nso_special,
    .setting_id = SettingId::NsoSpecial,
    .enum_cfg = { switch_device_names, SWITCH_DEVICE_COUNT, switch_device_get_max },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_latency_test,
    .type = MENU_TYPE_BOOL,
    .menu_var = &menu_latency_test,
    .runtime_var = nullptr,
    .setting_id = SettingId::LatencyTest,
    .bool_cfg = { "Off", "On" },
    .on_change = latency_test_apply,
    .on_apply = latency_test_apply
  },
  {
    .id = menu_item_latency_controller,
    .type = MENU_TYPE_BOOL,
    .menu_var = &menu_latency_controller_in_loop,
    .runtime_var = nullptr,
    .setting_id = SettingId::LatencyControllerInLoop,
    .bool_cfg = { "Internal", "Controller" },
    .on_change = latency_test_apply,
    .on_apply = latency_test_apply
  },
  {
    .id = menu_item_latency_host,
    .type = MENU_TYPE_ENUM,
    .menu_var = &menu_latency_host_type,
    .runtime_var = nullptr,
    .setting_id = SettingId::LatencyHostType,
    .enum_cfg = { latency_host_names, 3, nullptr },
    .on_change = latency_test_apply,
    .on_apply = latency_test_apply
  },
  {
    .id = menu_item_powerpad,
    .type = MENU_TYPE_ENUM,
    .menu_var = &menu_powerpad_mode,
    .runtime_var = nullptr,
    .setting_id = SettingId::PowerpadMode,
    .enum_cfg = { nes_device_names, 4, nullptr },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_menu_hotkey,
    .type = MENU_TYPE_BOOL,
    .menu_var = &menu_menu_hotkey,
    .runtime_var = nullptr,
    .setting_id = SettingId::MenuHotkey,
    .bool_cfg = { "Off", "On" },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_system_menu_hotkey,
    .type = MENU_TYPE_BOOL,
    .menu_var = &menu_system_menu_hotkey,
    .runtime_var = nullptr,
    .setting_id = SettingId::SystemMenuHotkey,
    .bool_cfg = { "Off", "On" },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_home_hotkey,
    .type = MENU_TYPE_BOOL,
    .menu_var = &menu_home_hotkey,
    .runtime_var = nullptr,
    .setting_id = SettingId::HomeHotkey,
    .bool_cfg = { "Off", "On" },
    .on_change = nullptr,
    .on_apply = nullptr
  },
#if !ADAPT_POLICY_FIXED_HOTKEY_ASSIGNMENTS
  {
    .id = menu_item_hold_start_hotkey,
    .type = MENU_TYPE_BOOL,
    .menu_var = &menu_hold_start_hotkey,
    .runtime_var = nullptr,
    .setting_id = SettingId::HoldStartHotkey,
    .bool_cfg = { "Off", "On" },
    .on_change = nullptr,
    .on_apply = nullptr
  },
#endif
  {
    .id = menu_item_capture_hotkey,
    .type = MENU_TYPE_BOOL,
    .menu_var = &menu_capture_hotkey,
    .runtime_var = nullptr,
    .setting_id = SettingId::CaptureHotkey,
    .bool_cfg = { "Off", "On" },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_kiosk_mode,
    .type = MENU_TYPE_BOOL,
    .menu_var = &menu_kiosk_mode,
    .runtime_var = nullptr,
    .setting_id = SettingId::KioskMode,
    .bool_cfg = { "Off", "On" },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_home_screen,
    .type = MENU_TYPE_BOOL,
    .menu_var = &menu_home_screen_debug,
    .runtime_var = nullptr,
    .setting_id = SettingId::HomeScreenDebug,
    .bool_cfg = { "Pad View", "Debug" },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_switch_rtrig_stick,
    .type = MENU_TYPE_CUSTOM,
    .menu_var = &menu_trigger_mode,
    .runtime_var = &trigger_mode,
    .setting_id = SettingId::TriggerMode,
    .bool_cfg = { "Off", "On" },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_gamecube_l_switch,
    .type = MENU_TYPE_BOOL,
    .menu_var = &menu_gamecube_l_switch_mode,
    .runtime_var = &gamecube_l_switch_mode,
    .setting_id = SettingId::GameCubeLSwitchMode,
    .bool_cfg = { "ZL", "L" },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_home_button_labels,
    .type = MENU_TYPE_BOOL,
    .menu_var = &menu_home_button_labels,
    .runtime_var = nullptr,
    .setting_id = SettingId::HomeButtonLabels,
    .bool_cfg = { "Round", "Name" },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_hotkey_hold_time,
    .type = MENU_TYPE_ENUM,
    .menu_var = &menu_hotkey_hold_time,
    .runtime_var = nullptr,
    .setting_id = SettingId::HotkeyHoldTime,
    .enum_cfg = { hotkey_hold_time_names, 7, nullptr },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_button_map,
    .type = MENU_TYPE_BOOL,
    .menu_var = &menu_button_map,
    .runtime_var = &button_map_mode,
    .setting_id = SettingId::ButtonMapMode,
    .bool_cfg = { "Name", "Position" },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_n64_z_mode,
    .type = MENU_TYPE_BOOL,
    .menu_var = &menu_n64_z_mode,
    .runtime_var = &n64_z_mode,
    .setting_id = SettingId::N64ZMode,
    .bool_cfg = { "L1", "L2" },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_n64_cstick_mode,
    .type = MENU_TYPE_ENUM,
    .menu_var = &menu_n64_cstick_mode,
    .runtime_var = &n64_cstick_mode,
    .setting_id = SettingId::N64CStickMode,
    .enum_cfg = { n64_cstick_mode_names, 3, nullptr },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_wii_analog_range,
    .type = MENU_TYPE_ENUM,
    .menu_var = &menu_wii_analog_range,
    .runtime_var = &wii_analog_range,
    .setting_id = SettingId::WiiAnalogRange,
    .enum_cfg = { classic_analog_range_names, CLASSIC_ANALOG_RANGE_COUNT, nullptr },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_n64_analog_range,
    .type = MENU_TYPE_ENUM,
    .menu_var = &menu_n64_analog_range,
    .runtime_var = &n64_analog_range,
    .setting_id = SettingId::N64AnalogRange,
    .enum_cfg = { classic_analog_range_names, CLASSIC_ANALOG_RANGE_COUNT, nullptr },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_socd,
    .type = MENU_TYPE_ENUM,
    .menu_var = (uint8_t*)&menu_socdMode,
    .runtime_var = (uint8_t*)&socdMode,
    .setting_id = SettingId::Socd,
    .enum_cfg = { socd_names, 4, nullptr },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_dpad_as_buttons,
    .type = MENU_TYPE_ENUM,
    .menu_var = &menu_dpad_mode,
    .runtime_var = &dpad_mode,
    .setting_id = SettingId::DpadMode,
    .enum_cfg = { dpad_mode_names, 4, dpad_get_max },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_rumble,
    .type = MENU_TYPE_ENUM,
    .menu_var = &menu_rumble_level,
    .runtime_var = &rumble_level,
    .setting_id = SettingId::RumbleLevel,
    .enum_cfg = { rumble_names, 3, nullptr },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_stick_invert,
    .type = MENU_TYPE_ENUM,
    .menu_var = &menu_stick_invert,
    .runtime_var = &stick_invert,
    .setting_id = SettingId::StickInvert,
    .enum_cfg = { stick_invert_names, 4, stick_invert_get_max },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_jogcon_mode,
    .type = MENU_TYPE_ENUM,
    .menu_var = &menu_jogcon_mode,
    .runtime_var = nullptr,
    .setting_id = SettingId::JogconMode,
    .enum_cfg = { jogcon_mode_names, 4, nullptr },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_jogcon_digital,
    .type = MENU_TYPE_ENUM,
    .menu_var = &menu_jogcon_digital_map,
    .runtime_var = nullptr,
    .setting_id = SettingId::JogconDigitalMap,
    .enum_cfg = { jogcon_digital_names, 4, nullptr },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_jogcon_wheel_axis,
    .type = MENU_TYPE_ENUM,
    .menu_var = &menu_jogcon_wheel_axis,
    .runtime_var = nullptr,
    .setting_id = SettingId::JogconWheelAxis,
    .enum_cfg = { jogcon_wheel_axis_names, 2, nullptr },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_win_output,
    .type = MENU_TYPE_ENUM,
    .menu_var = &menu_win_output,
    .runtime_var = nullptr,
    .setting_id = SettingId::WinOutput,
    .enum_cfg = { win_output_names, 8, win_output_get_max },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_mister_output,
    .type = MENU_TYPE_ENUM,
    .menu_var = &menu_mister_output,
    .runtime_var = nullptr,
    .setting_id = SettingId::MisterOutput,
    .enum_cfg = { win_output_names, 8, win_output_get_max },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_switch2_output,
    .type = MENU_TYPE_ENUM,
    .menu_var = &menu_switch2_output,
    .runtime_var = nullptr,
    .setting_id = SettingId::Switch2Output,
    .enum_cfg = { switch2_output_names, 2, nullptr },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_ps2_remote_output,
    .type = MENU_TYPE_ENUM,
    .menu_var = &menu_ps2_remote_output,
    .runtime_var = nullptr,
    .setting_id = SettingId::Ps2RemoteOutput,
    .enum_cfg = { ps2_remote_output_names, 2, nullptr },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_mister_display,
    .type = MENU_TYPE_ENUM,
    .menu_var = &menu_mister_display_mode,
    .runtime_var = &mister_display_mode,
    .setting_id = SettingId::MisterDisplay,
    .enum_cfg = { mister_display_names, 2, nullptr },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_psx_periph,
    .type = MENU_TYPE_ENUM,
    .menu_var = &menu_psx_periph,
    .runtime_var = nullptr,
    .setting_id = SettingId::PsxPeriph,
    .enum_cfg = { psx_periph_names, 1, nullptr },
    .on_change = nullptr,
    .on_apply = nullptr
  },
  {
    .id = menu_item_analog_deadzone,
    .type = MENU_TYPE_RANGE,
    .menu_var = &menu_deadzone_percent,
    .runtime_var = &deadzone_percent,
    .setting_id = SettingId::Deadzone,
    .range_cfg = { 0, 30, 5, "%" },
    .on_change = nullptr,
    .on_apply = nullptr
  },
};

const uint8_t MENU_DESCRIPTOR_COUNT = sizeof(menu_descriptors) / sizeof(MenuItemDescriptor);
