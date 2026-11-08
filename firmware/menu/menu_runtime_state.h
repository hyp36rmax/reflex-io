#pragma once

#include <stdint.h>

#include "../core/device_mode.h"
#include "../output/output_mode.h"

extern int8_t menu_guncon_offset_x;
extern int8_t menu_guncon_offset_y;
extern uint8_t menu_powerpad_mode;
extern uint8_t menu_jogcon_mode;
extern uint8_t menu_jogcon_force;
extern uint8_t menu_wheel_sensitivity;
extern uint8_t menu_jogcon_digital_map;
extern uint8_t menu_jogcon_wheel_axis;
extern uint8_t menu_button_map;
extern uint8_t menu_gamecube_l_switch_mode;
extern uint8_t menu_n64_z_mode;
extern uint8_t menu_n64_cstick_mode;
extern uint8_t menu_wii_analog_range;
extern uint8_t menu_n64_analog_range;
extern uint8_t menu_menu_hotkey;
extern uint8_t menu_system_menu_hotkey;
extern uint8_t menu_home_hotkey;
extern uint8_t menu_capture_hotkey;
extern uint8_t menu_kiosk_mode;
extern uint8_t menu_usb2db15_multi_bind;
extern uint8_t menu_usb2db15_game_selection;
extern uint32_t menu_hotkey_combo;
extern uint32_t menu_system_hotkey_combo;
extern uint32_t menu_home_hotkey_combo;
extern uint32_t menu_capture_hotkey_combo;
extern uint8_t menu_win_output;
extern uint8_t menu_mister_output;
// System Settings > Switch 2: what Auto presents to a Switch 2 host.
extern uint8_t menu_switch2_output;
// System/Quick PS2 receiver preference: 0=Gamepad, 1=Remote.
extern uint8_t menu_ps2_remote_output;
// System Settings > MiSTer OLED: staged until the System menu is saved.
extern uint8_t menu_mister_display_mode;
// System Settings > USB Drive (features/usb_drive); read at USB configuration.
extern uint8_t menu_management_msc;
extern uint8_t menu_touchpad_mouse;
extern uint8_t menu_psx_periph;
extern uint8_t menu_snes_rumbletech;
extern uint8_t menu_home_screen_debug;
extern uint8_t menu_home_button_labels;
extern uint8_t menu_home_jvs_view;
extern uint8_t menu_hotkey_hold_time;
extern uint8_t menu_classic_dual_merge;
extern uint8_t menu_usb_descriptor_reboot_required;
