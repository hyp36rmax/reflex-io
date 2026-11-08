#include "../product_config.h"

#include "menu_catalog.h"

#ifdef PRODUCT_JVSIO2USB
  #define ANALOG_TEST_MENU_TEXT "JVS Analog"
#else
  #define ANALOG_TEST_MENU_TEXT "Analog Test"
#endif

const menu_line_t menu_items[] = {
  { menu_item_input_mode, "In" },
  { menu_item_output_mode, "Out" },
  { menu_item_output_transport, "Transport" },
#if ADAPT_POLICY_SYSTEM_PC_MODE
  { menu_item_win_output, "PC Mode" },
#else
  { menu_item_win_output, "Win Mode" },
#endif
  { menu_item_mister_output, "MiSTer Mode" },
  { menu_item_switch2_output, "Switch2" },
  { menu_item_ps2_remote_output, "PS2 Remote" },
  { menu_item_mister_display, "MiSTer OLED" },
#if defined(ADAPT_FEATURE_USB_DRIVE)
  { menu_item_management_msc, "USB Drive" },
#endif
  { menu_item_psx_periph, "PSX Periph" },
  { menu_item_nso_special, "Device" },
  { menu_item_controller_settings, "Controller" },
  { menu_item_players, "Players" },
  { menu_item_controller_leds, "Controller LEDs" },
  { menu_item_bluetooth_devices, "Bluetooth" },
  { menu_item_web_config, "Web Config" },
  { menu_item_mapping_profiles, "Map Profiles" },
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
  { menu_item_auth_status, "Output/Auth Status" },
#else
  { menu_item_auth_status, "Auth Status" },
#endif
  { menu_item_device_leds, "Device LEDs" },
  { menu_item_diagnostics, "Diagnostics" },
#if defined(ADAPT_FEATURE_N64_CAPTURE)
  { menu_item_n64_capture_export, "N64 Export log" },
  { menu_item_n64_capture_new, "N64 New capture" },
#endif
#if defined(ADAPT_FEATURE_HOST_CAPTURE)
  { menu_item_host_capture_export, "Host Export log" },
  { menu_item_host_capture_new, "Host New capture" },
#endif
#if defined(ADAPT_FEATURE_JOGCON_CAPTURE)
  { menu_item_jogcon_capture_export, "Jog Export log" },
  { menu_item_jogcon_capture_new, "Jog New capture" },
#endif
#if defined(ADAPT_FEATURE_GENESIS_IDENTIFICATION_CAPTURE)
  { menu_item_genesis_capture_retry, "MD Retry Auto" },
  { menu_item_genesis_capture_manual, "MD Manual test" },
  { menu_item_genesis_capture_export, "MD Export log" },
#endif
#if defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE)
#if defined(ADAPT_FEATURE_PS2_IR_CAPTURE)
  { menu_item_ps2_ir_p1, "PS2 IR test P1" },
  { menu_item_ps2_ir_p2, "PS2 IR test P2" },
#else
  { menu_item_psx_capture_retry, "PSX Retry Auto" },
  { menu_item_psx_capture_deep, "PSX Deep probe" },
  { menu_item_psx_capture_export, "PSX Export log" },
#endif
#endif
#ifdef PRODUCT_JVSIO2USB
  { menu_item_jvs_status, "JVS Status" },
#endif
#if defined(ADAPT_BRIDGE_JVS_TO_USB)
  { menu_item_jvs_panel, "JVS Panel" },
#endif
  { menu_item_analog_deadzone, "Deadzone" },
  { menu_item_analog_test, ANALOG_TEST_MENU_TEXT },
  { menu_item_dpad_as_buttons, "DPad Mode" },
  { menu_item_socd, "SOCD" },
  { menu_item_display_contrast, "Contrast" },
  { menu_item_ok_back, "OK/Back" },
#if defined(ADAPT_FEATURE_FIRST_BOOT_SETUP)
  { menu_item_mode_lists, "Mode lists" },
  { menu_item_setup_guide, "Setup guide" },
#endif
  { menu_item_home_screen, "Home Screen" },
  { menu_item_home_button_labels, "Buttons" },
  { menu_item_hotkey_hold_time, "Hotkey Hold" },
  { menu_item_menu_hotkey, "Quick Menu" },
  { menu_item_system_menu_hotkey, "System Menu" },
  { menu_item_home_hotkey, "Home" },
  { menu_item_capture_hotkey, "Capture/TPad" },
  { menu_item_kiosk_mode, ADAPT_PRODUCT_KIOSK_MENU_LABEL },
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
  { menu_item_usb2db15_multi_bind, "Multi-bind" },
  { menu_item_usb2db15_game_selection, "Game selection" },
#endif
#if ADAPT_POLICY_FIXED_HOTKEY_ASSIGNMENTS
  { menu_item_hotkeys, "Hotkeys" },
#else
  { menu_item_hotkeys, "Hotkey" },
#endif
  { menu_item_stick_invert, "Stick Invert" },
  { menu_item_rumble, "Rumble" },
  { menu_item_trigger_mode, "Triggers" },
  { menu_item_switch_rtrig_stick, "RTrig=RStick" },
  { menu_item_gamecube_l_switch, "L Shoulder" },
  { menu_item_buzzer, "Sound" },
  { menu_item_latency_test, "Latency" },
  { menu_item_latency_controller, "Lat Ctrl" },
  { menu_item_latency_host, "Lat Host" },
  { menu_item_latency_run, "Lat Run" },
  { menu_item_button_remap, "Button Remap" },
  { menu_item_button_map, "Button Map" },
  { menu_item_n64_z_mode, "Z Button" },
  { menu_item_n64_cstick_mode, "N64 C Buttons" },
  { menu_item_wii_analog_range, "Stick Range" },
  { menu_item_n64_analog_range, "Stick Range" },
  { menu_item_mapping_view, "View Mapping" },
  //{ menu_item_pad_test, "Pad Test" },  // Removed - main display already shows real-time button visualization
  //{ menu_item_pin_debug, "Pin Debug" },  // Hidden - developer tool only
  { menu_item_oled, "OLED" },
  { menu_item_games, "Games" },
  { menu_item_guncon_offset, "GunCon Align" },
  { menu_item_powerpad, "NES Device" },
  { menu_item_jogcon_mode, "JogCon Mode" },
  { menu_item_jogcon_force, "JogCon Force" },
  { menu_item_wheel_sensitivity, "Wheel Sens" },
  { menu_item_jogcon_digital, "Dig Output" },
  { menu_item_jogcon_wheel_axis, "Wheel Axis" },
  { menu_item_bluetooth_pairing, "BT Pair" },
  { menu_item_bluetooth_forget, "Forget BT" },
  { menu_item_factory_reset, "Factory Reset" },
  { menu_item_bootloader, "Bootloader" },
#if ADAPT_POLICY_WHATS_NEW_QR
  { menu_item_whats_new, "What's new" },
#endif
  { menu_item_about, "About" },
};

#undef ANALOG_TEST_MENU_TEXT

const uint8_t MENU_TOTAL_ITEMS = sizeof(menu_items) / sizeof(menu_line_t);
const uint8_t MENU_ITEM_ROWS = 6;  // Rows 1-6 for items, row 7 for Cancel/Save
