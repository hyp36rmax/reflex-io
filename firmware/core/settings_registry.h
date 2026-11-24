#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../product_config.h"
#include "../output/output_persona_availability.h"
#include "../menu/screensaver_animation.h"
#include "../platform/buzzer.h"
#include "../platform/rgb_led.h"
#include "classic_analog_range.h"
#include "controller_settings_state.h"
#include "nes_device_mode.h"
#include "rotary_settings.h"
#include "rotary_profile.h"
#include "settings_store.h"
#include "../output/switch/switch_device_selection.h"
#include "../output/output_runtime_state.h"

using SettingDefaultFn = int32_t (*)(DeviceEnum mode);
using SettingSanitizerFn = int32_t (*)(DeviceEnum mode, int32_t value);

struct SettingSpec {
  SettingScope scope;
  SettingValueType value_type;
  uint16_t offset;
  int32_t default_value;
  int32_t min_value;
  int32_t max_value;
  SettingDefaultFn default_fn;
  SettingSanitizerFn sanitizer;
};

inline constexpr int32_t defaultPersistedInputModeValue(DeviceEnum) {
  return (int32_t)DEFAULT_INPUT_MODE;
}

inline constexpr int32_t defaultConfiguredOutputModeValue(DeviceEnum) {
  #if defined(ADAPT_HARDWARE_STUB)
  return (int32_t)OUTPUT_MISTER;
  #elif defined(ADAPT_PRIMARY_EGRESS_ESP32_WIRELESS)
  return (int32_t)OUTPUT_ESP32_BT;
  #elif defined(ADAPT_PRIMARY_EGRESS_JVS_BOARD)
  return (int32_t)OUTPUT_JVS;
  #elif ADAPT_POLICY_DEFAULT_JVS_USB_OUTPUT
  #if defined(FORCE_JVSIO_XINPUT2P_OUTPUT) && defined(ENABLE_EXPERIMENTAL_XINPUT2P_OUTPUT)
  return (int32_t)OUTPUT_XINPUT2P;
  #elif defined(FORCE_JVSIO_XID_OUTPUT)
  return (int32_t)OUTPUT_XID;
  #elif defined(FORCE_JVSIO_XBOXONE_OUTPUT) && defined(ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT)
  return (int32_t)OUTPUT_XBOXONE;
  #elif defined(FORCE_JVSIO_MANAGEMENT_OUTPUT)
  return (int32_t)OUTPUT_MISTER;
  #else
  return (int32_t)OUTPUT_AUTO;
  #endif
  #elif defined(ADAPT_PRIMARY_EGRESS_CLASSIC_CONSOLE)
  return (int32_t)OUTPUT_CONSOLE_SNES;
  #elif defined(ADAPT_PRIMARY_EGRESS_DB15)
  return (int32_t)OUTPUT_DB15_SUPERGUN;
  #elif defined(ADAPT_BRIDGE_CLASSIC_TO_USB) && defined(FORCE_CLASSIC_XINPUT2P_OUTPUT) && \
        defined(ENABLE_EXPERIMENTAL_XINPUT2P_OUTPUT)
  return (int32_t)OUTPUT_XINPUT2P;
  #elif defined(ADAPT_BRIDGE_CLASSIC_TO_USB) && defined(FORCE_CLASSIC_XINPUT_OUTPUT)
  // Diagnostic-only Xbox 360 build. Keep this on OUTPUT_XINPUT, not
  // OUTPUT_XINPUT2P, so console validation exercises the XSM3 auth path.
  return (int32_t)OUTPUT_XINPUT;
  #elif defined(ADAPT_BRIDGE_CLASSIC_TO_USB) && defined(FORCE_CLASSIC_MANAGEMENT_OUTPUT)
  return (int32_t)OUTPUT_MISTER;
  #elif defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  // Also the forced value of FORCE_CLASSIC_AUTO_OUTPUT (host capture image).
  return (int32_t)OUTPUT_AUTO;
  #elif defined(ADAPT_PRIMARY_EGRESS_USB_HOST)
  return (int32_t)OUTPUT_AUTO;
  #else
  return (int32_t)OUTPUT_MISTER;
  #endif
}

inline int32_t defaultSpinnerSpeedValue(DeviceEnum mode) {
  return defaultSpinnerSpeedForInputMode(mode);
}

inline int32_t defaultButtonMapModeValue(DeviceEnum mode) {
  return defaultButtonMapModeForInputMode(mode);
}

inline int32_t defaultTriggerModeValue(DeviceEnum mode) {
  return defaultTriggerModeForInputMode(mode);
}

inline int32_t defaultRumbleLevelValue(DeviceEnum mode) {
  return defaultRumbleLevelForInputMode(mode);
}

inline int32_t defaultNsoSpecialValue(DeviceEnum mode) {
  return defaultNsoSpecialForInputMode(mode);
}

inline int32_t sanitizeSwitchDeviceSelectionValue(DeviceEnum mode,
                                                  int32_t value) {
  if (value < 0 || value > 255) return defaultNsoSpecialForInputMode(mode);
  return sanitize_switch_device_selection(mode, static_cast<uint8_t>(value));
}

inline int32_t defaultZButtonModeValue(DeviceEnum mode) {
  return defaultZButtonModeForInputMode(mode);
}

inline int32_t defaultN64CStickModeValue(DeviceEnum mode) {
  return defaultN64CStickModeForInputMode(mode);
}

// Appended modes have no slot in the dual-merge record (device_mode.h).
inline int32_t sanitizeClassicDualMergeValue(DeviceEnum mode, int32_t value) {
  if (isAppendedInputMode(mode) || value < 0 || value > 1) return 0;
  return value;
}

inline int32_t sanitizeSocdValue(DeviceEnum, int32_t value) {
  #if ADAPT_POLICY_SOCD_FIXED_OFF
  (void)value;
  return SOCD_OFF;
  #else
  return (value >= 0 && value < SOCD_LAST_ENUM) ? value : SOCD_OFF;
  #endif
}

inline int32_t sanitizeWinOutputValue(DeviceEnum, int32_t value) {
  return (value >= HOST_OUTPUT_PREFERENCE_DINPUT &&
          value <= HOST_OUTPUT_PREFERENCE_MAX)
      ? value
      : HOST_OUTPUT_PREFERENCE_DINPUT;
}

inline int32_t sanitizeConfiguredOutputModeValue(DeviceEnum, int32_t value) {
  #if defined(ADAPT_POLICY_FORCE_AUTO_OUTPUT)
  (void)value;
  return OUTPUT_AUTO;
  #endif
  if (value == OUTPUT_RESERVED_JOGCON) {
    return OUTPUT_MISTER_JOGCON;
  }
  if (value == OUTPUT_XINPUTW) {
    #ifdef ENABLE_EXPERIMENTAL_XINPUT2P_OUTPUT
    return OUTPUT_XINPUT2P;
    #else
    return OUTPUT_MISTER;
    #endif
  }
  if (value < 0 || value >= OUTPUT_LAST) {
    return defaultConfiguredOutputModeValue(RZORD_NONE);
  }
  // A persona this image does not build must not survive as a stale choice.
  if (!outputPersonaIsBuilt(value)) {
    return foldUnbuiltOutputPersona(static_cast<outputMode_t>(value));
  }
  #if defined(ADAPT_BRIDGE_CLASSIC_TO_USB) && defined(FORCE_CLASSIC_XINPUT2P_OUTPUT) && \
      defined(ENABLE_EXPERIMENTAL_XINPUT2P_OUTPUT)
  return OUTPUT_XINPUT2P;
  #endif
  #if defined(ADAPT_BRIDGE_CLASSIC_TO_USB) && defined(FORCE_CLASSIC_XINPUT_OUTPUT)
  // Diagnostic-only Xbox 360 build. This deliberately bypasses the normal
  // Windows XInput2P sanitizer so retail Xbox 360 auth remains testable.
  return OUTPUT_XINPUT;
  #endif
  #if defined(ADAPT_BRIDGE_CLASSIC_TO_USB) && defined(FORCE_CLASSIC_MANAGEMENT_OUTPUT)
  return OUTPUT_MISTER;
  #endif
  #if defined(ADAPT_BRIDGE_CLASSIC_TO_USB) && defined(FORCE_CLASSIC_AUTO_OUTPUT)
  // Host capture image: Auto identifies the host, whatever was saved.
  return OUTPUT_AUTO;
  #endif
  #if defined(ADAPT_HARDWARE_STUB)
  return defaultConfiguredOutputModeValue(RZORD_NONE);
  #elif defined(ADAPT_PRIMARY_EGRESS_CLASSIC_CONSOLE)
  if (value != OUTPUT_CONSOLE_NES && value != OUTPUT_CONSOLE_SNES) {
    return defaultConfiguredOutputModeValue(RZORD_NONE);
  }
  #elif defined(ADAPT_PRIMARY_EGRESS_JVS_BOARD)
  if (value != OUTPUT_JVS) {
    return defaultConfiguredOutputModeValue(RZORD_NONE);
  }
  #elif defined(ADAPT_PRIMARY_EGRESS_DB15)
  if (value != OUTPUT_DB15_SUPERGUN) {
    return defaultConfiguredOutputModeValue(RZORD_NONE);
  }
  #endif
  return value;
}

inline int32_t sanitizeScreensaverAnimationValue(DeviceEnum, int32_t value) {
  return sanitizeScreensaverAnimation((uint8_t)value);
}

inline int32_t sanitizeJogconForceValue(DeviceEnum, int32_t value) {
  switch (value) {
    case 1:
    case 3:
    case 7:
    case 15:
      return value;
    default:
      return 1;
  }
}

inline int32_t sanitizeGunconAlignmentValue(DeviceEnum, int32_t value) {
  return normalizeGunconAlignmentOffset(value);
}

inline int32_t sanitizeSoundEventsValue(DeviceEnum, int32_t value) {
  if ((uint32_t)value == 0xFFFFu) {
    return (int32_t)SND_DEFAULT;
  }
  return (int32_t)((uint16_t)value & SND_ALL);
}

inline constexpr int32_t defaultHotkeyHoldTimeValue(DeviceEnum) {
  return 1;  // 0.5 seconds: enough intent without making menu access sluggish.
}

inline constexpr int32_t defaultKioskModeValue(DeviceEnum) {
  return 0;
}

inline constexpr int32_t defaultLedModeValue(DeviceEnum) {
  return (int32_t)ADAPT_POLICY_LED_DEFAULT_MODE;
}

inline constexpr int32_t defaultLedBrightnessValue(DeviceEnum) {
  return ADAPT_POLICY_LED_DEFAULT_BRIGHTNESS;
}

inline constexpr int32_t defaultProductOutputTransportValue(DeviceEnum) {
  // Auto is deliberately conservative: USB gets a bounded enumeration grace
  // and wins permanently when a host mounts during that window.
  return 0;
}

inline constexpr int32_t sanitizeProductOutputTransportValue(
    DeviceEnum, int32_t value) {
#if ADAPT_POLICY_USB_BT_OUTPUT_SETTINGS
  return value >= 0 && value <= 2 ? value : 0;
#else
  (void)value;
  return 0;
#endif
}

inline constexpr int32_t defaultProductWirelessOutputPersonaValue(DeviceEnum) {
  return 0;  // Reflex identity and neutral Reflex button layout.
}

inline constexpr int32_t sanitizeProductWirelessOutputPersonaValue(
    DeviceEnum, int32_t value) {
#if ADAPT_POLICY_USB_BT_OUTPUT_SETTINGS
  return value >= 0 && value <= 1 ? value : 0;
#else
  (void)value;
  return 0;
#endif
}

inline constexpr SettingSpec kSettingSpecs[] = {
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, persisted_input_mode),   (int32_t)DEFAULT_INPUT_MODE, 0, RZORD_LAST - 1, defaultPersistedInputModeValue, nullptr},
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, configured_output_mode), (int32_t)OUTPUT_AUTO,       0, OUTPUT_LAST - 1, defaultConfiguredOutputModeValue, sanitizeConfiguredOutputModeValue},
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, display_contrast),       128,                          1, 254,              nullptr, nullptr},
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, latency_test),           0,                            0, 1,                nullptr, nullptr},
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, latency_controller_in_loop), 0,                         0, 1,                nullptr, nullptr},
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, latency_host_type),      0,                            0, 2,                nullptr, nullptr},
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, buzzer_mode),            1,                            0, 2,                nullptr, nullptr},
  {SettingScope::Global,  SettingValueType::UInt16, (uint16_t)offsetof(GlobalSettingsRecord, sound_events),           (int32_t)SND_DEFAULT,         0, SND_ALL,          nullptr, sanitizeSoundEventsValue},
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, led_mode),               (int32_t)LED_MODE_OFF,        LED_MODE_OFF, LED_MODE_LAST - 1, defaultLedModeValue, nullptr},
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, led_brightness),         128,                          0, 254,              defaultLedBrightnessValue, nullptr},
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, screensaver_mode),       4,                            0, 6,                nullptr, nullptr},
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, screensaver_animation),  0,                            0, 13,               nullptr, sanitizeScreensaverAnimationValue},
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, menu_hotkey),            0,                            0, 1,                nullptr, nullptr},
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, home_hotkey),            1,                            0, 1,                nullptr, nullptr},
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, capture_hotkey),         1,                            0, 1,                nullptr, nullptr},
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, win_output),             HOST_OUTPUT_PREFERENCE_XINPUT, 0, HOST_OUTPUT_PREFERENCE_MAX, nullptr, sanitizeWinOutputValue},
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, psx_periph),             0,                            0, 0,                nullptr, nullptr},
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, snes_rumbletech),        0,                            0, 1,                nullptr, nullptr},
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, home_screen_debug),      0,                            0, 1,                nullptr, nullptr},
  // Reserved: the retired home-button-label setting must remain at this index.
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, home_screen_debug),      0,                            0, 0,                nullptr, nullptr},
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, home_screen_debug),      0,                            0, 1,                nullptr, nullptr},
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, home_screen_debug),      1,                            0, 6,                defaultHotkeyHoldTimeValue, nullptr},
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, deadzone_percent),      0,                            0, 30,               nullptr, nullptr},
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, socd),                 ADAPT_POLICY_SOCD_DEFAULT,    0, SOCD_LAST_ENUM - 1, nullptr, sanitizeSocdValue},
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, dpad_mode),            0,                            0, 3,                nullptr, nullptr},
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, stick_invert),         0,                            0, 3,                nullptr, nullptr},
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, rumble_level),         RUMBLE_MODE_VARIABLE,         RUMBLE_MODE_OFF, RUMBLE_MODE_MAX, defaultRumbleLevelValue, nullptr},
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, trigger_mode),         0,                            0, TRIGGER_MODE_BOTH, defaultTriggerModeValue, nullptr},
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, spinner_speed),        2,                            0, 4,                defaultSpinnerSpeedValue, nullptr},
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, button_map_mode),      0,                            0, 1,                defaultButtonMapModeValue, nullptr},
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, n64_z_mode),           1,                            0, 1,                defaultZButtonModeValue, nullptr},
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, n64_cstick_mode),      0,                            0, 2,                defaultN64CStickModeValue, nullptr},
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, wii_analog_range),     CLASSIC_ANALOG_RANGE_DEFAULT, 0, CLASSIC_ANALOG_RANGE_COUNT - 1, nullptr, nullptr},
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, n64_analog_range),     CLASSIC_ANALOG_RANGE_DEFAULT, 0, CLASSIC_ANALOG_RANGE_COUNT - 1, nullptr, nullptr},
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, nso_special),          0,                            0, SWITCH_DEVICE_COUNT - 1, defaultNsoSpecialValue, sanitizeSwitchDeviceSelectionValue},
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, powerpad_mode),        NES_DEVICE_PAD,               NES_DEVICE_PAD, NES_DEVICE_MODE_COUNT - 1, nullptr, nullptr},
  {SettingScope::PerMode, SettingValueType::Int8,   (uint16_t)offsetof(PerModeSettingsRecord, guncon_offset_x),      0, GUNCON_ALIGNMENT_MIN, GUNCON_ALIGNMENT_MAX, nullptr, sanitizeGunconAlignmentValue},
  {SettingScope::PerMode, SettingValueType::Int8,   (uint16_t)offsetof(PerModeSettingsRecord, guncon_offset_y),      0, GUNCON_ALIGNMENT_MIN, GUNCON_ALIGNMENT_MAX, nullptr, sanitizeGunconAlignmentValue},
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, reserved_musical_buttons), 0,                         0, 0,                nullptr, nullptr},
#if ADAPT_POLICY_RESERVED_FEEDBACK_STORAGE
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, reserved_mouse_analog),      0, 0, 0,  nullptr, nullptr},
#else
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, gamecube_l_switch_mode),    GAMECUBE_L_SWITCH_ZL, GAMECUBE_L_SWITCH_ZL, GAMECUBE_L_SWITCH_L, nullptr, nullptr},
#endif
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, reserved_mouse_sensitivity), 5, 5, 5,  nullptr, nullptr},
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, reserved_mouse_stick),       0, 0, 0,  nullptr, nullptr},
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, jogcon_mode),          0,                            0, 3,                nullptr, nullptr},
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, jogcon_force),        15,                            0, 15,               nullptr, sanitizeJogconForceValue},
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, wheel_sensitivity),    1,                            0, 2,                nullptr, nullptr},
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, jogcon_digital_map),   0,                            0, 3,                nullptr, nullptr},
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, jogcon_wheel_axis),    0,                            0, 1,                nullptr, nullptr},
  {SettingScope::PerMode, SettingValueType::Int8,   (uint16_t)offsetof(PerModeSettingsRecord, analog_cal_lx_min),   -128,                       -128, 127,             nullptr, nullptr},
  {SettingScope::PerMode, SettingValueType::Int8,   (uint16_t)offsetof(PerModeSettingsRecord, analog_cal_lx_max),    127,                       -128, 127,             nullptr, nullptr},
  {SettingScope::PerMode, SettingValueType::Int8,   (uint16_t)offsetof(PerModeSettingsRecord, analog_cal_ly_min),   -128,                       -128, 127,             nullptr, nullptr},
  {SettingScope::PerMode, SettingValueType::Int8,   (uint16_t)offsetof(PerModeSettingsRecord, analog_cal_ly_max),    127,                       -128, 127,             nullptr, nullptr},
  {SettingScope::PerMode, SettingValueType::Int8,   (uint16_t)offsetof(PerModeSettingsRecord, analog_cal_rx_min),   -128,                       -128, 127,             nullptr, nullptr},
  {SettingScope::PerMode, SettingValueType::Int8,   (uint16_t)offsetof(PerModeSettingsRecord, analog_cal_rx_max),    127,                       -128, 127,             nullptr, nullptr},
  {SettingScope::PerMode, SettingValueType::Int8,   (uint16_t)offsetof(PerModeSettingsRecord, analog_cal_ry_min),   -128,                       -128, 127,             nullptr, nullptr},
  {SettingScope::PerMode, SettingValueType::Int8,   (uint16_t)offsetof(PerModeSettingsRecord, analog_cal_ry_max),    127,                       -128, 127,             nullptr, nullptr},
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, analog_cal_enabled),   0,                            0, 1,                nullptr, nullptr},
  // Stored in the dedicated per-mode ClassicDualMergeRecord, not this compatibility field.
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, reserved_musical_buttons), 0,                          0, 1,                nullptr, sanitizeClassicDualMergeValue},
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, menu_hotkey),            0,                            0, 1,                nullptr, nullptr},
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, menu_hotkey),            0,                            0, 1,                defaultKioskModeValue, nullptr},
  // One byte stores the pointer source and speed. Legacy 1-16 values remain
  // valid and decode as touchpad speed settings.
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, touchpad_mouse),         0,                            0, 255,              nullptr, nullptr},
  // Packed into win_output bits 3-4; bits 0-2 hold the host output preference.
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, win_output),             0,                            0, 2,
    defaultProductOutputTransportValue, sanitizeProductOutputTransportValue},
  // Packed into the remaining win_output bits 5-7.
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, win_output),             0,                            0, 1,
    defaultProductWirelessOutputPersonaValue, sanitizeProductWirelessOutputPersonaValue},
  // Classic ingress reuses the retired PSX byte; other products preserve it independently.
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, psx_periph),              0,                            0, HOST_OUTPUT_PREFERENCE_MAX, nullptr, sanitizeWinOutputValue},
  // Inverted unused option bit: old/default zero bytes enable multi-bind.
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, menu_hotkey),            1,                            0, 1,                nullptr, nullptr},
  // USB drive, default On; packed independently of the MiSTer output mode.
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, psx_periph),             1,                            0, 1,                nullptr, nullptr},
  // A valid two-bit retired value is required to turn this on; legacy and
  // erased/invalid values therefore sanitize to the factory Off default.
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, menu_hotkey),            0,                            0, 1,                nullptr, nullptr},
  // Classic2USB packs rotary policy with the Wii analog range byte. Appending
  // these IDs keeps every existing serial and persisted setting ID stable.
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, wii_analog_range),     1,                            1, 16,               nullptr, nullptr},
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, wii_analog_range),     0,                            0, 1,                nullptr, nullptr},
  {SettingScope::PerMode, SettingValueType::UInt8,  (uint16_t)offsetof(PerModeSettingsRecord, wii_analog_range),     0,                            0, 1,                nullptr, nullptr},
  // Packed into screensaver_mode bit 7: 0 = Screensaver, 1 = Off (blank panel).
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, screensaver_mode),       0,                            0, 1,                nullptr, nullptr},
  // Packed into screensaver_mode bit 6: 0 = any input wakes, 1 = Adapt only.
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, screensaver_mode),       0,                            0, 1,                nullptr, nullptr},
  // Tagged layout in the former global-record padding byte.
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, menu_ok_back),           0,                            0, 2,                nullptr, nullptr},
  // Packed into psx_periph bits 4-5: Switch Pro or Switch 2 Pro for a Switch 2.
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, psx_periph),             SWITCH2_OUTPUT_PREFERENCE_SWITCHPRO, 0, SWITCH2_OUTPUT_PREFERENCE_MAX, nullptr, nullptr},
  // Packed into psx_periph bit 6: zero preserves the existing gamepad output.
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, psx_periph),             0,                            0, 1, nullptr, nullptr},
  // Packed into menu_hotkey bit 6: existing zero records retain Artwork.
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, menu_hotkey),            0,                            0, 1, nullptr, nullptr},
  // Packed into home_hotkey bit 7 as an Off flag: existing records read On.
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, home_hotkey),            1,                            0, 1, nullptr, nullptr},
  // 500ms steps, 0.5-3.0s. Stored in capture_hotkey bits 1-3.
  {SettingScope::Global,  SettingValueType::UInt8,  (uint16_t)offsetof(GlobalSettingsRecord, capture_hotkey),         GUIDE_HOLD_TIME_DEFAULT,      1, 6, nullptr, nullptr},
};

static_assert((sizeof(kSettingSpecs) / sizeof(kSettingSpecs[0])) == static_cast<size_t>(SettingId::Count),
              "Setting registry is out of sync with SettingId");

inline constexpr bool firstBootSetupSupportedOnProduct() {
#if defined(ADAPT_FEATURE_FIRST_BOOT_SETUP)
  return true;
#else
  return false;
#endif
}

// One capability-based authority for menus, serial automation, and release
// validation. Runtime code must not maintain its own product allow-list.
inline constexpr bool settingSupportedOnProduct(SettingId id) {
#if defined(ADAPT_HARDWARE_STUB)
  (void)id;
  return false;
#else
  switch (id) {
    case SettingId::MenuOkBack:
#if defined(ADAPT_CAP_RETRO_MODE_SETTINGS) && defined(ADAPT_FEATURE_OLED_DISPLAY)
      return true;
#else
      return false;
#endif
    case SettingId::Switch2Output:
      // Wherever Auto classifies a Switch 2 host, Xbox One builds included:
      // their Xbox candidate needs the WCID request a Switch 2 never sends.
#if defined(ADAPT_CAP_USB_OUTPUT_AUTODETECT)
      return true;
#else
      return false;
#endif
    case SettingId::Ps2RemoteOutput:
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB) && defined(ENABLE_INPUT_PSX)
      return true;
#else
      return false;
#endif
    case SettingId::MisterDisplay:
#if defined(ADAPT_FEATURE_MISTER_SERIAL_STATUS)
      return true;
#else
      return false;
#endif
    case SettingId::MenuHotkey:
    case SettingId::SystemMenuHotkey:
    case SettingId::CaptureHotkey:
    case SettingId::HotkeyHoldTime:
    // Fixed-preset products run Hold Start under their one Hotkeys toggle.
    case SettingId::HoldStartHotkey:
      return !ADAPT_POLICY_FIXED_HOTKEY_ASSIGNMENTS;
    case SettingId::GuideHoldTime:
#if defined(PRODUCT_CLASSIC2USB)
      return true;
#else
      return false;
#endif
    case SettingId::ManagementMsc:
      // The USB drive (features/usb_drive), On unless its psx_periph bit is set.
#if defined(ADAPT_FEATURE_USB_DRIVE)
      return true;
#else
      return false;
#endif
    case SettingId::Usb2Db15MultiBind:
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
      return true;
#else
      return false;
#endif
    case SettingId::Usb2Db15GameSelection:
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
      return true;
#else
      return false;
#endif
    case SettingId::ReservedMusicalButtons:
    case SettingId::ReservedMouseSensitivity:
    case SettingId::ReservedMouseStick:
    case SettingId::HomeButtonLabels:
      return false;

    case SettingId::LatencyTest:
    case SettingId::LatencyControllerInLoop:
    case SettingId::LatencyHostType:
#if defined(ADAPT_ENABLE_LATENCY_TEST)
      return true;
#else
      return false;
#endif

    case SettingId::DisplayContrast:
    case SettingId::ScreensaverMode:
    case SettingId::ScreensaverAnimation:
    case SettingId::OledInactive:
    case SettingId::OledWake:
    case SettingId::HomeScreenDebug:
    case SettingId::KioskMode:
#if defined(ADAPT_FEATURE_OLED_DISPLAY)
      return true;
#else
      return false;
#endif

    case SettingId::BuzzerMode:
    case SettingId::SoundEvents:
#if defined(ADAPT_FEATURE_BUZZER)
      return true;
#else
      return false;
#endif

    case SettingId::LedMode:
    case SettingId::LedBrightness:
#if defined(ADAPT_FEATURE_RGB_LED)
      return true;
#else
      return false;
#endif

    case SettingId::WinOutput:
    case SettingId::MisterOutput:
    case SettingId::TouchpadMouse:
#if defined(ADAPT_CAP_HOST_OUTPUT_PREFERENCES)
      return true;
#else
      return false;
#endif

    case SettingId::ProductOutputTransport:
    case SettingId::ProductWirelessOutputPersona:
#if defined(ADAPT_CAP_OUTPUT_TRANSPORT_SELECTION)
      return true;
#else
      return false;
#endif

    case SettingId::ClassicDualMerge:
#if defined(ADAPT_CAP_DUAL_INPUT_MERGE)
      return true;
#else
      return false;
#endif

    case SettingId::PaddleDivisor:
    case SettingId::RotaryReverse:
    case SettingId::DrivingWrap:
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
      return true;
#else
      return false;
#endif

    case SettingId::PsxPeriph:
    case SettingId::SnesRumbleTech:
    case SettingId::N64ZMode:
    case SettingId::N64CStickMode:
    case SettingId::WiiAnalogRange:
    case SettingId::N64AnalogRange:
    case SettingId::NsoSpecial:
    case SettingId::PowerpadMode:
    case SettingId::GunconOffsetX:
    case SettingId::GunconOffsetY:
    case SettingId::GameCubeLSwitchMode:
    case SettingId::JogconMode:
    case SettingId::JogconForce:
    case SettingId::JogconRange:
    case SettingId::JogconDigitalMap:
    case SettingId::JogconWheelAxis:
#if defined(ADAPT_CAP_RETRO_MODE_SETTINGS)
      return true;
#else
      return false;
#endif

    case SettingId::HomeJvsView:
#if defined(ADAPT_CAP_JVS_UI) && \
    defined(ADAPT_FEATURE_OLED_DISPLAY)
      return true;
#else
      return false;
#endif

    default:
      return true;
  }
#endif
}

inline constexpr const SettingSpec& settingSpec(SettingId id) {
  return kSettingSpecs[static_cast<size_t>(id)];
}

inline constexpr bool settingIsGlobal(SettingId id) {
  return settingSpec(id).scope == SettingScope::Global;
}

inline constexpr bool settingIsPerMode(SettingId id) {
  return settingSpec(id).scope == SettingScope::PerMode;
}

inline int32_t defaultSettingValue(SettingId id, DeviceEnum mode) {
  const SettingSpec& spec = settingSpec(id);
  return spec.default_fn ? spec.default_fn(mode) : spec.default_value;
}

inline int32_t sanitizeSettingValue(SettingId id, DeviceEnum mode, int32_t value) {
  const SettingSpec& spec = settingSpec(id);
  if (spec.sanitizer != nullptr) {
    return spec.sanitizer(mode, value);
  }
  if (value < spec.min_value || value > spec.max_value) {
    return defaultSettingValue(id, mode);
  }
  return value;
}

template <typename RecordT>
inline int32_t readRecordSettingValue(const RecordT& record, SettingId id) {
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  // Retired compatibility IDs are not the public API for Start shortcuts.
  if (id == SettingId::ReservedMusicalButtons || id == SettingId::ReservedMouseStick) return 0;
  if (id == SettingId::ReservedMouseSensitivity) return 5;
#endif
  const SettingSpec& spec = settingSpec(id);
  const uint8_t* base = reinterpret_cast<const uint8_t*>(&record);
  const uint8_t* field = base + spec.offset;
  switch (spec.value_type) {
    case SettingValueType::UInt8:
      if (id == SettingId::ManagementMsc) return (*field & MANAGEMENT_MSC_OFF_MASK) ? 0 : 1;
      if (id == SettingId::Usb2Db15MultiBind) return (*field & USB2DB15_MULTI_BIND_OFF_MASK) ? 0 : 1;
      if (id == SettingId::Usb2Db15GameSelection) {
        return (*field & USB2DB15_GAME_SELECTION_MASK) ==
            USB2DB15_GAME_SELECTION_ON_VALUE ? 1 : 0;
      }
      if (id == SettingId::MenuHotkey) {
        return menuHotkeyQuickEnabledFromRaw(*reinterpret_cast<const uint8_t*>(field)) ? 1 : 0;
      }
      if (id == SettingId::SystemMenuHotkey) {
        return menuHotkeySystemEnabledFromRaw(*reinterpret_cast<const uint8_t*>(field)) ? 1 : 0;
      }
      if (id == SettingId::KioskMode) {
        return menuKioskEnabledFromRaw(*reinterpret_cast<const uint8_t*>(field)) ? 1 : 0;
      }

      if (id == SettingId::WinOutput) {
        return *reinterpret_cast<const uint8_t*>(field) & WIN_OUTPUT_VALUE_MASK;
      }
      if (id == SettingId::MisterOutput) {
        return misterOutputPreferenceFromRaw(*reinterpret_cast<const uint8_t*>(field));
      }
      if (id == SettingId::PsxPeriph) {
        return 0;
      }

      if (id == SettingId::ProductOutputTransport) {
        return (*reinterpret_cast<const uint8_t*>(field) &
                WIN_OUTPUT_TRANSPORT_MASK) >> WIN_OUTPUT_TRANSPORT_SHIFT;
      }
      if (id == SettingId::ProductWirelessOutputPersona) {
        return (*reinterpret_cast<const uint8_t*>(field) &
                WIN_OUTPUT_BT_PERSONA_MASK) >> WIN_OUTPUT_BT_PERSONA_SHIFT;
      }
      if (id == SettingId::ScreensaverMode) {
        return *field & SCREENSAVER_MODE_VALUE_MASK;
      }
      if (id == SettingId::OledInactive) {
        return (*field & OLED_INACTIVE_OFF_MASK) ? 1 : 0;
      }
      if (id == SettingId::OledWake) {
        return (*field & OLED_WAKE_ADAPT_ONLY_MASK) ? 1 : 0;
      }
      if (id == SettingId::MenuOkBack) {
        return menuOkBackValueFromRaw(*field);
      }
      if (id == SettingId::Switch2Output) {
        return switch2OutputPreferenceFromRaw(*field);
      }
      if (id == SettingId::Ps2RemoteOutput) {
        return ps2RemoteOutputEnabledFromRaw(*field) ? 1 : 0;
      }
      if (id == SettingId::MisterDisplay) {
        return misterDisplayModeFromRaw(*field);
      }
      if (id == SettingId::HomeHotkey) {
        return *field & HOME_HOTKEY_VALUE_MASK;
      }
      if (id == SettingId::CaptureHotkey) {
        return (*field & CAPTURE_HOTKEY_ENABLED_MASK) ? 1 : 0;
      }
      if (id == SettingId::GuideHoldTime) {
        return guideHoldTimeFromRaw(*field);
      }
      if (id == SettingId::HoldStartHotkey) {
        return holdStartHotkeyEnabledFromRaw(*field) ? 1 : 0;
      }
      if (id == SettingId::HomeScreenDebug) {
        return (*reinterpret_cast<const uint8_t*>(field) & HOME_SCREEN_DEBUG_MASK) ? 1 : 0;
      }
      if (id == SettingId::HomeButtonLabels) {
        return (*reinterpret_cast<const uint8_t*>(field) & HOME_BUTTON_LABELS_MASK) ? 1 : 0;
      }
      if (id == SettingId::HomeJvsView) {
        return (*reinterpret_cast<const uint8_t*>(field) & HOME_JVS_VIEW_MASK) >> HOME_JVS_VIEW_SHIFT;
      }
      if (id == SettingId::HotkeyHoldTime) {
        return (*reinterpret_cast<const uint8_t*>(field) & HOTKEY_HOLD_TIME_MASK) >> HOTKEY_HOLD_TIME_SHIFT;
      }
      if (id == SettingId::ClassicDualMerge) return 0;
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
      // The PSX record keeps its poll timing in this byte; it never reads as L.
      if (id == SettingId::GameCubeLSwitchMode) return sanitizeGameCubeLSwitchMode(*field);
      // The PSX record keeps its neGcon I/II/L choice here; it reads as Pad.
      if (id == SettingId::PowerpadMode) return sanitizeNesDeviceMode(*field);
      if (id == SettingId::WiiAnalogRange) return rotaryAnalogRange(*field);
      if (id == SettingId::JogconRange) return rotaryWheelSensitivity(*field);
      if (id == SettingId::JogconDigitalMap) {
        return rotaryLegacyLowBits(*field);
      }
#endif
      if (id == SettingId::PaddleDivisor) return rotaryDivisor(*field);
      if (id == SettingId::RotaryReverse) return rotaryReverse(*field) ? 1 : 0;
      if (id == SettingId::DrivingWrap) return rotaryWrap(*field) ? 1 : 0;
      return *reinterpret_cast<const uint8_t*>(field);
    case SettingValueType::Int8:
      return *reinterpret_cast<const int8_t*>(field);
    case SettingValueType::UInt16:
      return *reinterpret_cast<const uint16_t*>(field);
  }
  return 0;
}

template <typename RecordT>
inline void writeRecordSettingValue(RecordT& record, SettingId id, int32_t value) {
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  // An older client must not erase a saved shortcut profile through these IDs.
  if (id == SettingId::ReservedMusicalButtons || id == SettingId::ReservedMouseSensitivity ||
      id == SettingId::ReservedMouseStick) return;
#endif
  const SettingSpec& spec = settingSpec(id);
  uint8_t* base = reinterpret_cast<uint8_t*>(&record);
  uint8_t* field = base + spec.offset;
  switch (spec.value_type) {
    case SettingValueType::UInt8:
      if (id == SettingId::MenuHotkey) {
        uint8_t& target = *reinterpret_cast<uint8_t*>(field);
        target = encodeMenuHotkeyFlags(value != 0, menuHotkeySystemEnabledFromRaw(target), target);
        break;
      }
      if (id == SettingId::SystemMenuHotkey) {
        uint8_t& target = *reinterpret_cast<uint8_t*>(field);
        target = encodeMenuHotkeyFlags(menuHotkeyQuickEnabledFromRaw(target), value != 0, target);
        break;
      }
      if (id == SettingId::KioskMode) {
        uint8_t& target = *reinterpret_cast<uint8_t*>(field);
        uint8_t preserve = (target & (uint8_t)~MENU_HOTKEY_KIOSK_MASK) |
                           (value ? MENU_HOTKEY_KIOSK_MASK : 0);
        target = encodeMenuHotkeyFlags(
          menuHotkeyQuickEnabledFromRaw(target),
          menuHotkeySystemEnabledFromRaw(target),
          preserve
        );
        break;
      }
      if (id == SettingId::Usb2Db15MultiBind) {
        uint8_t& target = *reinterpret_cast<uint8_t*>(field);
        target = (target & ~USB2DB15_MULTI_BIND_OFF_MASK) | (value ? 0 : USB2DB15_MULTI_BIND_OFF_MASK);
        break;
      }
      if (id == SettingId::Usb2Db15GameSelection) {
        uint8_t& target = *reinterpret_cast<uint8_t*>(field);
        target = (target & ~USB2DB15_GAME_SELECTION_MASK) |
                 (value ? USB2DB15_GAME_SELECTION_ON_VALUE : 0);
        break;
      }

      if (id == SettingId::WinOutput) {
        uint8_t& target = *reinterpret_cast<uint8_t*>(field);
        target = static_cast<uint8_t>(
            (target & static_cast<uint8_t>(~WIN_OUTPUT_VALUE_MASK)) |
            (static_cast<uint8_t>(value) & WIN_OUTPUT_VALUE_MASK));
        break;
      }
      if (id == SettingId::MisterOutput) {
        uint8_t& target = *reinterpret_cast<uint8_t*>(field);
        target = encodeMisterOutputPreference(target, static_cast<uint8_t>(value));
        target |= HOST_OUTPUT_PREFERENCE_V2_MASK;
        break;
      }
      if (id == SettingId::PsxPeriph) {
        uint8_t& target = *reinterpret_cast<uint8_t*>(field);
        target &= static_cast<uint8_t>(MISTER_OUTPUT_PREFERENCE_MASK |
            HOST_OUTPUT_PREFERENCE_V2_MASK | MANAGEMENT_MSC_OFF_MASK |
            SWITCH2_OUTPUT_PREFERENCE_MASK | PS2_REMOTE_OUTPUT_PREFERENCE_MASK);
        break;
      }
      if (id == SettingId::ManagementMsc) {
        uint8_t& target = *reinterpret_cast<uint8_t*>(field);
        target = (target & static_cast<uint8_t>(~MANAGEMENT_MSC_OFF_MASK)) |
                 (value ? 0 : MANAGEMENT_MSC_OFF_MASK);
        break;
      }

      if (id == SettingId::ProductOutputTransport) {
        uint8_t& target = *reinterpret_cast<uint8_t*>(field);
        target = static_cast<uint8_t>(
            (target & static_cast<uint8_t>(~WIN_OUTPUT_TRANSPORT_MASK)) |
            ((static_cast<uint8_t>(value) << WIN_OUTPUT_TRANSPORT_SHIFT) &
             WIN_OUTPUT_TRANSPORT_MASK));
        break;
      }
      if (id == SettingId::ProductWirelessOutputPersona) {
        uint8_t& target = *reinterpret_cast<uint8_t*>(field);
        target = static_cast<uint8_t>(
            (target & static_cast<uint8_t>(~WIN_OUTPUT_BT_PERSONA_MASK)) |
            ((static_cast<uint8_t>(value) << WIN_OUTPUT_BT_PERSONA_SHIFT) &
             WIN_OUTPUT_BT_PERSONA_MASK));
        break;
      }
      if (id == SettingId::ScreensaverMode) {
        uint8_t& target = *reinterpret_cast<uint8_t*>(field);
        // An invalid mode (erased 0xFF included) does not vouch for the Off
        // or Wake bits either, so such a byte returns to the factory defaults.
        const bool validMode = (target & SCREENSAVER_MODE_VALUE_MASK) <= spec.max_value;
        const uint8_t keptFlags = validMode
            ? (uint8_t)(target & (OLED_INACTIVE_OFF_MASK | OLED_WAKE_ADAPT_ONLY_MASK)) : 0;
        target = (uint8_t)(keptFlags | ((uint8_t)value & SCREENSAVER_MODE_VALUE_MASK));
        break;
      }
      if (id == SettingId::OledInactive) {
        uint8_t& target = *reinterpret_cast<uint8_t*>(field);
        target = (target & (uint8_t)~OLED_INACTIVE_OFF_MASK) |
                 (value ? OLED_INACTIVE_OFF_MASK : 0);
        break;
      }
      if (id == SettingId::OledWake) {
        uint8_t& target = *reinterpret_cast<uint8_t*>(field);
        target = (target & (uint8_t)~OLED_WAKE_ADAPT_ONLY_MASK) |
                 (value ? OLED_WAKE_ADAPT_ONLY_MASK : 0);
        break;
      }
      if (id == SettingId::MenuOkBack) {
        *field = encodeMenuOkBackPreservingSetupState(
            *field, (uint8_t)value, firstBootSetupSupportedOnProduct());
        break;
      }
      if (id == SettingId::Switch2Output) {
        *field = encodeSwitch2OutputPreference(*field, static_cast<uint8_t>(value));
        break;
      }
      if (id == SettingId::Ps2RemoteOutput) {
        *field = encodePs2RemoteOutputPreference(*field, value != 0);
        break;
      }
      if (id == SettingId::MisterDisplay) {
        *field = encodeMisterDisplayMode(*field, static_cast<uint8_t>(value));
        break;
      }
      if (id == SettingId::HomeHotkey) {
        *field = encodeHomeHotkey(*field, value != 0);
        break;
      }
      if (id == SettingId::HoldStartHotkey) {
        *field = encodeHoldStartHotkey(*field, value != 0);
        break;
      }
      if (id == SettingId::CaptureHotkey) {
        *field = encodeCaptureHotkey(*field, value != 0);
        break;
      }
      if (id == SettingId::GuideHoldTime) {
        *field = encodeGuideHoldTime(*field, static_cast<uint8_t>(value));
        break;
      }
      if (id == SettingId::HomeScreenDebug) {
        uint8_t& target = *reinterpret_cast<uint8_t*>(field);
        target = (target & (uint8_t)~HOME_SCREEN_DEBUG_MASK) |
                 (value ? HOME_SCREEN_DEBUG_MASK : 0);
        break;
      }
      if (id == SettingId::HomeButtonLabels) {
        uint8_t& target = *reinterpret_cast<uint8_t*>(field);
        target = (target & (uint8_t)~HOME_BUTTON_LABELS_MASK) |
                 (value ? HOME_BUTTON_LABELS_MASK : 0);
        break;
      }
      if (id == SettingId::HomeJvsView) {
        uint8_t& target = *reinterpret_cast<uint8_t*>(field);
        target = (target & (uint8_t)~HOME_JVS_VIEW_MASK) |
                 (uint8_t)(((uint8_t)value << HOME_JVS_VIEW_SHIFT) & HOME_JVS_VIEW_MASK);
        break;
      }
      if (id == SettingId::HotkeyHoldTime) {
        uint8_t& target = *reinterpret_cast<uint8_t*>(field);
        target = (target & (uint8_t)~HOTKEY_HOLD_TIME_MASK) |
                 (uint8_t)(((uint8_t)value << HOTKEY_HOLD_TIME_SHIFT) & HOTKEY_HOLD_TIME_MASK);
        break;
      }
      if (id == SettingId::ClassicDualMerge) break;
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
      if (id == SettingId::JogconRange) {
        uint8_t& target = *reinterpret_cast<uint8_t*>(field);
        target = withRotaryProfileInWheelSensitivity(
          static_cast<uint8_t>(value), rotaryProfileFromWheelSensitivity(target));
        break;
      }
      if (id == SettingId::JogconDigitalMap) {
        uint8_t& target = *reinterpret_cast<uint8_t*>(field);
        target = withRotaryProfileInJogconDigitalMap(
          static_cast<uint8_t>(value), rotaryProfileFromJogconDigitalMap(target));
        break;
      }
      if (id == SettingId::WiiAnalogRange) {
        uint8_t& target = *reinterpret_cast<uint8_t*>(field);
        target = encodeRotaryAnalogRange(target, static_cast<uint8_t>(value));
        break;
      }
#endif
      if (id == SettingId::PaddleDivisor) {
        uint8_t& target = *reinterpret_cast<uint8_t*>(field);
        target = encodeRotaryDivisor(target, static_cast<uint8_t>(value));
        break;
      }
      if (id == SettingId::RotaryReverse) {
        uint8_t& target = *reinterpret_cast<uint8_t*>(field);
        target = encodeRotaryReverse(target, value != 0);
        break;
      }
      if (id == SettingId::DrivingWrap) {
        uint8_t& target = *reinterpret_cast<uint8_t*>(field);
        target = encodeRotaryWrap(target, value != 0);
        break;
      }
      *reinterpret_cast<uint8_t*>(field) = (uint8_t)value;
      break;
    case SettingValueType::Int8:
      *reinterpret_cast<int8_t*>(field) = (int8_t)value;
      break;
    case SettingValueType::UInt16:
      *reinterpret_cast<uint16_t*>(field) = (uint16_t)value;
      break;
  }
}
