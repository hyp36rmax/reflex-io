#include "../platform/buzzer.h"
#include "../platform/latency_test.h"
#include "../core/controller_settings_state.h"
#include "menu_working_state.h"

uint8_t menu_deadzone_percent = 0;
uint8_t menu_dpad_mode = 0;
uint8_t menu_nso_special = 0;
socdMode_t menu_socdMode = SOCD_OFF;
uint8_t menu_display_contrast = 128;
uint8_t menu_stick_invert = 0;
uint8_t menu_rumble_level = RUMBLE_MODE_VARIABLE;
uint8_t menu_trigger_mode = 0;
uint8_t menu_buzzer_mode = 1;
uint16_t menu_sound_events = SND_DEFAULT;
uint8_t menu_latency_test = 0;
uint8_t menu_latency_controller_in_loop = 0;
uint8_t menu_latency_host_type = LATENCY_HOST_INTERNAL;
uint8_t menu_screensaver = 4;
uint8_t menu_screensaver_anim = 0;
uint8_t menu_oled_inactive = 0;
uint8_t menu_oled_wake = 0;
uint8_t menu_menu_ok_back = 0;
