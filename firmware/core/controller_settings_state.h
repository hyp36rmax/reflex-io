#pragma once

#include <stdint.h>

#include "../firmware_platform_config.h"
#include "gamecube_switch_left_mode.h"

extern uint8_t deadzone_percent;
extern uint8_t display_contrast;
extern uint8_t stick_invert;
extern uint8_t screensaver_mode;
extern uint8_t oled_inactive_mode;  // 0 = Screensaver, 1 = Blank OLED
extern uint8_t oled_wake_mode;      // 0 = any input wakes, 1 = Adapt only
extern uint8_t menu_ok_back;        // 0 = PSX X/SNES A, 1 = East, 2 = South
constexpr uint8_t MISTER_DISPLAY_ARTWORK = 0;
constexpr uint8_t MISTER_DISPLAY_INPUTS = 1;
extern uint8_t mister_display_mode;
extern uint8_t spinner_speed;
extern uint8_t button_map_mode;
extern uint8_t gamecube_l_switch_mode;
extern uint8_t psx_poll_timing;     // PSX Quick Menu timing, psx_poll_timing.h
extern uint8_t negcon_digital_inputs; // neGcon I/II/L set to Digital, negcon_input_modes.h
extern uint8_t negcon_gain;           // neGcon Analog gain index, negcon_input_modes.h
extern uint8_t n64_z_mode;
extern uint8_t n64_analog_range;
extern uint8_t n64_cstick_mode;
extern uint8_t wii_analog_range;
extern uint8_t paddle_divisor;
extern bool rotary_reverse;
extern bool driving_wrap;
extern uint8_t rotary_profile;
extern uint8_t jogcon_rotary_profile;
extern const uint8_t spinner_speed_mult[5];
extern const uint8_t driving_speed_mult[5];
extern const char* const spinner_speed_labels[5];
extern uint8_t rumble_left[MAX_USB_OUT];
extern uint8_t rumble_right[MAX_USB_OUT];
extern uint8_t rumble_level;
constexpr uint8_t RUMBLE_MODE_OFF = 0;
constexpr uint8_t RUMBLE_MODE_VARIABLE = 1;
constexpr uint8_t RUMBLE_MODE_MAX = 2;
extern uint8_t snes_rumbletech_enabled;
extern uint8_t trigger_mode;
constexpr uint8_t TRIGGER_MODE_ANALOG = 0;
constexpr uint8_t TRIGGER_MODE_DIGITAL = 1;
constexpr uint8_t TRIGGER_MODE_RSTICK = 2;
constexpr uint8_t TRIGGER_MODE_BOTH = 3;
constexpr uint8_t TRIGGER_DIGITAL_THRESHOLD = 100;
extern uint8_t buzzer_enabled;
extern uint8_t led_mode;
extern uint8_t led_brightness;
extern uint8_t dpad_mode;
// Auto-detected PSX dance pads need D-pad Buttons and SOCD Off while they are
// connected. The override changes only the runtime values, restores the user's
// values when released, and never reaches persisted settings.
void setTransientDancePadOverride(bool active);
// Forget an engaged override without restoring (settings were just reloaded).
void forgetTransientDancePadOverride();
#if ADAPT_POLICY_DIRECT_BOARD_CONTROLS
// A board's LS/RS switch or D-pad chord changes the runtime mode only.
void setTransientDpadMode(bool active, uint8_t mode);
#endif
uint8_t persistedRuntimeDpadMode();
uint8_t persistedRuntimeSocdMode();
// Switch output identity. The EEPROM field retains its historical
// `nso_special` name, but USB/JVS products use all switch_device_selection_t
// values while Classic2USB keeps its contextual 0/1 choice.
extern uint8_t nso_special;
extern uint8_t classic_dual_merge_enabled;
