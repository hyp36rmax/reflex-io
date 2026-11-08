#pragma once

#include <stdint.h>

#include "../output/output_mode.h"

extern uint8_t menu_deadzone_percent;
extern uint8_t menu_dpad_mode;
extern uint8_t menu_nso_special;
extern socdMode_t menu_socdMode;
extern uint8_t menu_display_contrast;
extern uint8_t menu_stick_invert;
extern uint8_t menu_rumble_level;
extern uint8_t menu_trigger_mode;
extern uint8_t menu_buzzer_mode;
extern uint16_t menu_sound_events;
extern uint8_t menu_latency_test;
extern uint8_t menu_latency_controller_in_loop;
extern uint8_t menu_latency_host_type;
extern uint8_t menu_screensaver;
extern uint8_t menu_screensaver_anim;
extern uint8_t menu_oled_inactive;  // 0 = Screensaver, 1 = Off (blank panel)
extern uint8_t menu_oled_wake;      // 0 = any input wakes, 1 = Adapt only
extern uint8_t menu_menu_ok_back;
