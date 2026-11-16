#include "../product_config.h"
#include "directional_hotkeys.h"

#include "serial_core_commands.h"

#include <Arduino.h>
#include <hardware/gpio.h>
#include <string.h>

#include "button_chord_remap.h"
#include "classic_dual_merge_config.h"
#include "controller_frame_state.h"
#include "controller_output_cache_state.h"
#include "controller_settings_state.h"
#include "device_runtime_state.h"
#include "firmware_support.h"
#include "hotkey_combo.h"
#include "negcon_input_modes.h"
#include "psx_poll_timing.h"
#include "rotary_profile.h"
#include "serial_command_parser.h"
#include "serial_semantic_command_parser.h"
#include "serial_semantic_settings.h"
#include "setting_access.h"
#include "settings_registry.h"
#include "settings_store.h"
#include "../features/feature_module.h"
#include "../input/mixed/input_mixed_runtime_state.h"
#include "../input/runtime/input_frame_runtime.h"
#include "../input/runtime/input_module_runtime.h"
#include "../menu/quick_config.h"
#include "../menu/menu_idle_runtime.h"
#include "../menu/menu_input.h"
#include "../menu/menu_display_state.h"
#include "../menu/menu_runtime_state.h"
#include "../menu/menu_ui_state.h"
#include "../menu/menu_working_state.h"
#include "../output/auth/auth_storage.h"
#include "../output/output_runtime_state.h"
#include "../output/runtime/output_loop_runtime.h"
#include "../output/usb/output_touchpad_mouse_debug.h"
#ifdef ADAPT_OUTPUT_USB_DEVICE
#include "../output/xinput/output_xinput_auth_runtime_state.h"
#endif
#if defined(ADAPT_OUTPUT_USB_DEVICE) && defined(ENABLE_EXPERIMENTAL_XINPUT2P_OUTPUT)
#endif
#include "../platform/runtime/platform_menu_runtime.h"
#include "../platform/buzzer.h"
#include "../platform/latency_test.h"
#include "../platform/rgb_led.h"

#ifdef ENABLE_INPUT_PCE
#include "../input/pce/Input_Pce.h"
#include "../input/pce/input_pce_runtime_state.h"
#endif

#ifdef ENABLE_INPUT_SATURN
#include "../input/saturn/input_saturn_runtime_state.h"
#endif

#ifdef ENABLE_INPUT_SNES
#include "../input/snes/input_snes_runtime_state.h"
// Raw HVC-051 reads for serial FCNET (input/snes/input_snes_famicom_network.cpp).
void printFamicomNetworkRawDump(Print& out);
#endif

#ifdef ENABLE_INPUT_PSX
#include "../input/psx/Input_Psx.h"
#include "../input/psx/input_psx_runtime_state.h"
#endif

#ifdef ENABLE_INPUT_DREAMCAST
#include "../input/dreamcast/Input_Dreamcast.h"
#endif

#ifdef ENABLE_INPUT_N64
#include "../input/gc64/Input_GC64.h"
#endif

#ifdef ENABLE_USB_AUTH_SIDECAR
#include "../input/usb_host/input_usb_host_service.h"
#include "../output/auth/ps_auth_dongle_runtime.h"
#endif

namespace {

bool serialSettingSupportedOnProduct(SettingId id) {
  return settingSupportedOnProduct(id);
}

const char* settingScopeName(SettingScope scope) {
  return scope == SettingScope::Global ? "G" : "P";
}

const char* settingTypeName(SettingValueType type) {
  switch (type) {
    case SettingValueType::UInt8: return "U8";
    case SettingValueType::Int8: return "I8";
    case SettingValueType::UInt16: return "U16";
  }
  return "?";
}

const char* settingIdName(SettingId id) {
  switch (id) {
    case SettingId::PersistedInputMode: return "INPUT_MODE";
    case SettingId::ConfiguredOutputMode: return "OUTPUT_MODE";
    case SettingId::DisplayContrast: return "DISPLAY_CONTRAST";
    case SettingId::LatencyTest: return "LATENCY_TEST";
    case SettingId::LatencyControllerInLoop: return "LATENCY_CONTROLLER";
    case SettingId::LatencyHostType: return "LATENCY_HOST";
    case SettingId::BuzzerMode: return "BUZZER_MODE";
    case SettingId::SoundEvents: return "SOUND_EVENTS";
    case SettingId::LedMode: return "DEVICE_LED_MODE";
    case SettingId::LedBrightness: return "DEVICE_LED_BRIGHTNESS";
    case SettingId::ScreensaverMode: return "SCREENSAVER_MODE";
    case SettingId::ScreensaverAnimation: return "SCREENSAVER_ANIMATION";
    case SettingId::MenuHotkey: return "MENU_HOTKEY";
    case SettingId::HomeHotkey: return "HOME_HOTKEY";
    case SettingId::CaptureHotkey: return "CAPTURE_HOTKEY";
    case SettingId::WinOutput: return "WINDOWS_OUTPUT";
    case SettingId::MisterOutput: return "MISTER_OUTPUT";
    case SettingId::ManagementMsc: return "USB_DRIVE";
    case SettingId::PsxPeriph: return "PSX_PERIPHERAL";
    case SettingId::SnesRumbleTech: return "SNES_RUMBLETECH";
    case SettingId::HomeScreenDebug: return "HOME_DEBUG";
    case SettingId::HomeButtonLabels: return "HOME_BUTTON_LABELS";
    case SettingId::HomeJvsView: return "HOME_JVS_VIEW";
    case SettingId::HotkeyHoldTime: return "HOTKEY_HOLD_TIME";
    case SettingId::Deadzone: return "DEADZONE";
    case SettingId::Socd: return "SOCD";
    case SettingId::DpadMode: return "DPAD_MODE";
    case SettingId::StickInvert: return "STICK_INVERT";
    case SettingId::RumbleLevel: return "RUMBLE_LEVEL";
    case SettingId::TriggerMode: return "TRIGGER_MODE";
    case SettingId::SpinnerSpeed: return "SPINNER_SPEED";
    case SettingId::ButtonMapMode: return "BUTTON_MAP_MODE";
    case SettingId::N64ZMode: return "N64_Z_MODE";
    case SettingId::N64CStickMode: return "N64_CSTICK_MODE";
    case SettingId::WiiAnalogRange: return "WII_ANALOG_RANGE";
    case SettingId::N64AnalogRange: return "N64_ANALOG_RANGE";
    case SettingId::NsoSpecial: return "NSO_SPECIAL";
    case SettingId::PowerpadMode: return "POWERPAD_MODE";
    case SettingId::GunconOffsetX: return "GUNCON_OFFSET_X";
    case SettingId::GunconOffsetY: return "GUNCON_OFFSET_Y";
    case SettingId::ReservedMusicalButtons: return "MUSICAL_BUTTONS";
    case SettingId::GameCubeLSwitchMode: return "GAMECUBE_L_MODE";
    case SettingId::ReservedMouseSensitivity: return "MOUSE_SENSITIVITY";
    case SettingId::ReservedMouseStick: return "MOUSE_STICK";
    case SettingId::JogconMode: return "JOGCON_MODE";
    case SettingId::JogconForce: return "JOGCON_FORCE";
    case SettingId::JogconRange: return "JOGCON_RANGE";
    case SettingId::JogconDigitalMap: return "JOGCON_DIGITAL_MAP";
    case SettingId::JogconWheelAxis: return "JOGCON_WHEEL_AXIS";
    case SettingId::AnalogCalLXMin: return "ANALOG_LX_MIN";
    case SettingId::AnalogCalLXMax: return "ANALOG_LX_MAX";
    case SettingId::AnalogCalLYMin: return "ANALOG_LY_MIN";
    case SettingId::AnalogCalLYMax: return "ANALOG_LY_MAX";
    case SettingId::AnalogCalRXMin: return "ANALOG_RX_MIN";
    case SettingId::AnalogCalRXMax: return "ANALOG_RX_MAX";
    case SettingId::AnalogCalRYMin: return "ANALOG_RY_MIN";
    case SettingId::AnalogCalRYMax: return "ANALOG_RY_MAX";
    case SettingId::AnalogCalEnabled: return "ANALOG_CAL_ENABLED";
    case SettingId::ClassicDualMerge: return "TWO_PLAYER_MERGE";
    case SettingId::SystemMenuHotkey: return "SYSTEM_MENU_HOTKEY";
    case SettingId::KioskMode: return "KIOSK_MODE";
    case SettingId::Usb2Db15MultiBind: return "USB2DB15_MULTI_BIND";
    case SettingId::Usb2Db15GameSelection: return "USB2DB15_GAME_SELECTION";
    case SettingId::TouchpadMouse: return "TOUCHPAD_MOUSE";
    case SettingId::ProductOutputTransport: return "OUTPUT_TRANSPORT";
    case SettingId::ProductWirelessOutputPersona: return "BT_OUTPUT_PERSONA";
    case SettingId::PaddleDivisor: return "PADDLE_DIVISOR";
    case SettingId::RotaryReverse: return "ROTARY_REVERSE";
    case SettingId::DrivingWrap: return "DRIVING_WRAP";
    case SettingId::OledInactive: return "OLED_INACTIVE";
    case SettingId::OledWake: return "OLED_WAKE";
    case SettingId::MenuOkBack: return "MENU_OK_BACK";
    case SettingId::Switch2Output: return "SWITCH2_OUTPUT";
    case SettingId::Ps2RemoteOutput: return "PS2_REMOTE_OUTPUT";
    case SettingId::MisterDisplay: return "MISTER_DISPLAY";
    case SettingId::HoldStartHotkey: return "HOLD_START_HOTKEY";
    case SettingId::Count: break;
  }
  return "UNKNOWN";
}

char normalizeSettingNameChar(char value) {
  if (value >= 'a' && value <= 'z') {
    value = static_cast<char>(value - ('a' - 'A'));
  }
  return value == '-' ? '_' : value;
}

bool settingNameEquals(const char* left, const char* right) {
  while (*left != '\0' && *right != '\0') {
    if (normalizeSettingNameChar(*left) != normalizeSettingNameChar(*right)) {
      return false;
    }
    ++left;
    ++right;
  }
  return *left == '\0' && *right == '\0';
}

bool parseSettingIdToken(char*& text, long* rawId) {
  text = serialSkipSpaces(text);
  if (*text == '\0' || rawId == nullptr) {
    return false;
  }

  char* numeric = text;
  long parsed = -1;
  if (serialParseLongToken(numeric, &parsed)) {
    *rawId = parsed;
    text = numeric;
    return true;
  }

  char token[32] = {};
  size_t length = 0;
  while (*text != '\0' && *text != ' ' && *text != '\t') {
    if (length + 1 < sizeof(token)) {
      token[length++] = *text;
    }
    ++text;
  }
  token[length] = '\0';
  text = serialSkipSpaces(text);
  for (uint8_t raw = 0; raw < static_cast<uint8_t>(SettingId::Count); ++raw) {
    if (settingNameEquals(token, settingIdName(static_cast<SettingId>(raw)))) {
      *rawId = raw;
      return true;
    }
  }
  return false;
}

void printHex32(Print& out, uint32_t value) {
  out.print(F("0x"));
  for (int shift = 28; shift >= 0; shift -= 4) {
    out.print((value >> shift) & 0x0F, HEX);
  }
}

bool parsedAllKeyword(char* text) {
  return serialTextEqualsExact(text, "ALL");
}

bool parseSettingModeOrCurrent(char*& text, DeviceEnum* mode) {
  if (mode == nullptr) {
    return false;
  }
  text = serialSkipSpaces(text);
  if (*text == '\0') {
    *mode = deviceMode;
    return true;
  }
  long rawMode = -1;
  if (!serialParseLongToken(text, &rawMode) ||
      rawMode < 0 || rawMode >= RZORD_LAST) {
    return false;
  }
  *mode = static_cast<DeviceEnum>(rawMode);
  return true;
}

bool sanitizedValueInSpecRange(SettingId id, int32_t value) {
  const SettingSpec& spec = settingSpec(id);
  return value >= spec.min_value && value <= spec.max_value;
}

void printSettingMatrixFailure(Print& out, SettingId id, DeviceEnum mode, const char* what,
                               int32_t value, int32_t sanitized) {
  out.print(F("SETMFAIL ID="));
  out.print((int)id);
  out.print(F(" MODE="));
  out.print((int)mode);
  out.print(F(" WHAT="));
  out.print(what);
  out.print(F(" VAL="));
  out.print(value);
  out.print(F(" SAN="));
  out.println(sanitized);
}

bool checkSettingCandidate(Print& out, SettingId id, DeviceEnum mode, const char* what,
                           int32_t value, uint16_t* passCount, uint16_t* failCount) {
  const int32_t sanitized = sanitizeSettingValue(id, mode, value);
  const bool ok = sanitizedValueInSpecRange(id, sanitized) &&
                  sanitizeSettingValue(id, mode, sanitized) == sanitized;
  if (ok) {
    ++(*passCount);
  } else {
    ++(*failCount);
    printSettingMatrixFailure(out, id, mode, what, value, sanitized);
  }
  return ok;
}

void runSettingMatrixForMode(Print& out, DeviceEnum mode, uint16_t* passCount, uint16_t* failCount) {
  for (uint8_t raw = 0; raw < (uint8_t)SettingId::Count; ++raw) {
    const SettingId id = (SettingId)raw;
    if (!serialSettingSupportedOnProduct(id)) {
      continue;
    }
    if (settingIsPerMode(id) && !isConcreteModeForPerSettings(mode)) {
      continue;
    }
    if (settingIsGlobal(id) && mode != RZORD_NONE) {
      continue;
    }

    const SettingSpec& spec = settingSpec(id);
    const int32_t def = defaultSettingValue(id, mode);
    const int32_t sanDef = sanitizeSettingValue(id, mode, def);
    if (sanDef == def && sanitizedValueInSpecRange(id, sanDef)) {
      ++(*passCount);
    } else {
      ++(*failCount);
      printSettingMatrixFailure(out, id, mode, "DEFAULT", def, sanDef);
    }

    checkSettingCandidate(out, id, mode, "MIN", spec.min_value, passCount, failCount);
    checkSettingCandidate(out, id, mode, "MAX", spec.max_value, passCount, failCount);
    if (spec.min_value > INT32_MIN) {
      checkSettingCandidate(out, id, mode, "BELOW", spec.min_value - 1, passCount, failCount);
    }
    if (spec.max_value < INT32_MAX) {
      checkSettingCandidate(out, id, mode, "ABOVE", spec.max_value + 1, passCount, failCount);
    }

    const int32_t loaded = loadSettingValue(id, mode);
    const int32_t sanLoaded = sanitizeSettingValue(id, mode, loaded);
    if (loaded == sanLoaded && sanitizedValueInSpecRange(id, loaded)) {
      ++(*passCount);
    } else {
      ++(*failCount);
      printSettingMatrixFailure(out, id, mode, "LOADED", loaded, sanLoaded);
    }
  }
}

bool handleSettingMatrixCommand(char* text, Print& out) {
  const bool allModes = parsedAllKeyword(text);
  uint16_t passCount = 0;
  uint16_t failCount = 0;
  uint8_t modeCount = 0;

  runSettingMatrixForMode(out, RZORD_NONE, &passCount, &failCount);
  ++modeCount;

  if (allModes) {
    for (uint8_t rawMode = 0; rawMode < (uint8_t)RZORD_LAST; ++rawMode) {
      const DeviceEnum mode = (DeviceEnum)rawMode;
      if (!isConcreteModeForPerSettings(mode)) {
        continue;
      }
      runSettingMatrixForMode(out, mode, &passCount, &failCount);
      ++modeCount;
    }
  } else {
    DeviceEnum mode = deviceMode;
    if (!parseSettingModeOrCurrent(text, &mode) ||
        *serialSkipSpaces(text) != '\0') {
      out.println(F("ERR:BAD_INPUT_MODE"));
      return true;
    }
    if (isConcreteModeForPerSettings(mode)) {
      runSettingMatrixForMode(out, mode, &passCount, &failCount);
      ++modeCount;
    }
  }

  out.print(F("OK:SET_MATRIX PASS="));
  out.print(passCount);
  out.print(F(" FAIL="));
  out.print(failCount);
  out.print(F(" MODES="));
  out.println((int)modeCount);
  return true;
}

bool handleMixedMatrixCommand(Print& out) {
  DeviceEnum supportedModes[(uint8_t)RZORD_LAST] = {};
  uint8_t modeCount = 0;
  for (uint8_t rawMode = 0; rawMode < (uint8_t)RZORD_LAST; ++rawMode) {
    const DeviceEnum mode = (DeviceEnum)rawMode;
    if (inputMixedPortModeSupported(0, mode) &&
        inputMixedPortModeSupported(1, mode)) {
      supportedModes[modeCount++] = mode;
    }
  }

  uint16_t passCount = 0;
  uint16_t failCount = 0;
  uint16_t pairCount = 0;
  uint16_t supervisorCount = 0;
  for (uint8_t i = 0; i < modeCount; ++i) {
    for (uint8_t j = 0; j < modeCount; ++j) {
      const DeviceEnum port0Mode = supportedModes[i];
      const DeviceEnum port1Mode = supportedModes[j];
      const bool need01 = inputMixedModesNeedSupervisor(port0Mode, port1Mode);
      const bool need10 = inputMixedModesNeedSupervisor(port1Mode, port0Mode);
      const bool self0 = inputMixedModesNeedSupervisor(port0Mode, port0Mode);
      const bool self1 = inputMixedModesNeedSupervisor(port1Mode, port1Mode);
      const bool ok = need01 == need10 && !self0 && !self1;

      ++pairCount;
      if (need01) {
        ++supervisorCount;
      }
      if (ok) {
        ++passCount;
      } else {
        ++failCount;
        out.print(F("MIXED FAIL P0="));
        out.print((int)port0Mode);
        out.print(F(" P1="));
        out.print((int)port1Mode);
        out.print(F(" NEED01="));
        out.print(need01 ? 1 : 0);
        out.print(F(" NEED10="));
        out.print(need10 ? 1 : 0);
        out.print(F(" SELF0="));
        out.print(self0 ? 1 : 0);
        out.print(F(" SELF1="));
        out.println(self1 ? 1 : 0);
      }
    }
  }

  out.print(F("OK:MIXED_MATRIX PASS="));
  out.print(passCount);
  out.print(F(" FAIL="));
  out.print(failCount);
  out.print(F(" MODES="));
  out.print((int)modeCount);
  out.print(F(" PAIRS="));
  out.print(pairCount);
  out.print(F(" SUP="));
  out.println(supervisorCount);
  return true;
}

bool parseOutputModeOrConfigured(char*& text, outputMode_t* mode) {
  if (mode == nullptr) {
    return false;
  }
  text = serialSkipSpaces(text);
  if (*text == '\0') {
    *mode = canonicalizeOutputMode(configuredOutputMode);
    return true;
  }
  long rawMode = -1;
  if (!serialParseLongToken(text, &rawMode) ||
      rawMode < 0 || rawMode >= OUTPUT_LAST) {
    return false;
  }
  *mode = canonicalizeOutputMode((outputMode_t)rawMode);
  return true;
}

bool handleSettingVisibleCommand(char* text, Print& out) {
  DeviceEnum inputMode = deviceMode;
  if (!parseSettingModeOrCurrent(text, &inputMode)) {
    out.println(F("ERR:BAD_INPUT_MODE"));
    return true;
  }
  outputMode_t output = configuredOutputMode;
  if (!parseOutputModeOrConfigured(text, &output) ||
      *serialSkipSpaces(text) != '\0') {
    out.println(F("ERR:BAD_OUTPUT_MODE"));
    return true;
  }

  QuickConfigMenu probe;
  QuickConfigItem items[QCI_COUNT * 2];
  const uint8_t itemCount = probe.debugBuildVisibleItems(
    inputMode,
    output,
    menu_win_output,
    items,
    (uint8_t)(sizeof(items) / sizeof(items[0])));

  bool seen[(uint8_t)SettingId::Count] = {};
  uint8_t visibleSettingCount = 0;
  for (uint8_t i = 0; i < itemCount; ++i) {
    SettingId ids[10];
    const uint8_t idCount = quickConfigItemSettingIds(
      items[i],
      inputMode,
      ids,
      (uint8_t)(sizeof(ids) / sizeof(ids[0])));

    if (idCount == 0) {
      out.print(F("VIS ITEM="));
      out.print((int)items[i]);
      out.println(F(" SET=-"));
      continue;
    }

    for (uint8_t idIndex = 0; idIndex < idCount; ++idIndex) {
      const uint8_t rawId = (uint8_t)ids[idIndex];
      if (rawId >= (uint8_t)SettingId::Count) {
        continue;
      }
      if (!serialSettingSupportedOnProduct((SettingId)rawId)) {
        continue;
      }
      if (!seen[rawId]) {
        seen[rawId] = true;
        ++visibleSettingCount;
      }
      out.print(F("VIS SET="));
      out.print((int)rawId);
      out.print(F(" ITEM="));
      out.print((int)items[i]);
      out.print(F(" MODE="));
      out.print((int)inputMode);
      out.print(F(" OUT="));
      out.println((int)output);
    }
  }

  out.print(F("OK:SET_VISIBLE MODE="));
  out.print((int)inputMode);
  out.print(F(" OUT="));
  out.print((int)output);
  out.print(F(" ITEMS="));
  out.print((int)itemCount);
  out.print(F(" SETTINGS="));
  out.println((int)visibleSettingCount);
  return true;
}

// PC Mode, MiSTer Mode and Switch2 choose what Auto runs, and USB keeps the old
// persona until it enumerates again.
bool hostOutputPreferenceChangedRunningOutput(SettingId id, int32_t previous, int32_t stored) {
  return (id == SettingId::WinOutput || id == SettingId::MisterOutput ||
          id == SettingId::Switch2Output) &&
         previous != stored && auto_detect_resolved_mode_is_stale();
}

bool applyStoredSettingToRuntime(SettingId id, DeviceEnum mode) {
  if (!serialSettingSupportedOnProduct(id)) {
    return false;
  }
  const int32_t value = loadSettingValue(id, mode);
  if (settingIsPerMode(id)) {
    if (mode != deviceMode) return false;
    if (id == SettingId::NsoSpecial) {
      const bool descriptorChange =
        static_cast<uint8_t>(value) != menu_nso_special;
      menu_nso_special = sanitize_switch_device_selection(mode, static_cast<uint8_t>(value));
      if (descriptorChange &&
          output_mode_supports_switch_device_selection(get_effective_output_mode()) &&
          input_mode_supports_nso_special(mode)) {
        menu_usb_descriptor_reboot_required = 1;
        return false;
      }
      nso_special = menu_nso_special;
      return true;
    }

    if (id == SettingId::ClassicDualMerge) {
      menu_classic_dual_merge = static_cast<uint8_t>(value);
      return true;
    }
    PerModeSettingsRecord settings{};
    readPerModeSettings(mode, settings);
    applyPerModeSettings(settings);
    return true;
  }

  switch (id) {
    case SettingId::PersistedInputMode:
      return false;  // Deliberately takes effect on the next boot.
    case SettingId::ConfiguredOutputMode:
      configuredOutputMode = sanitizeConfiguredOutputMode(
          static_cast<outputMode_t>(value));
      outputMode = sanitizeRuntimeOutputMode(configuredOutputMode);
      return true;
    case SettingId::DisplayContrast:
      display_contrast = static_cast<uint8_t>(value);
      return true;
    case SettingId::LatencyTest:
      menu_latency_test = static_cast<uint8_t>(value);
      latencyTest.setEnabled(value != 0);
      return true;
    case SettingId::LatencyControllerInLoop:
      menu_latency_controller_in_loop = static_cast<uint8_t>(value);
      latencyTest.setControllerInLoop(value != 0);
      return true;
    case SettingId::LatencyHostType:
      menu_latency_host_type = static_cast<uint8_t>(value);
      latencyTest.setHostType(static_cast<uint8_t>(value));
      return true;
    case SettingId::BuzzerMode:
      menu_buzzer_mode = static_cast<uint8_t>(value);
      buzzer_enabled = value != 0;
      buzzer.setEnabled(buzzer_enabled);
      buzzer.setVolume(value == 1 ? 10 : 100);
      return true;
    case SettingId::SoundEvents:
      menu_sound_events = static_cast<uint16_t>(value);
      buzzer.setEventMask(menu_sound_events);
      return true;
    case SettingId::LedMode:
      led_mode = static_cast<uint8_t>(value);
      rgbLed.setMode(static_cast<led_mode_t>(led_mode));
      rgbLed.setEnabled(led_mode != LED_MODE_OFF);
      return true;
    case SettingId::LedBrightness:
      led_brightness = static_cast<uint8_t>(value);
      rgbLed.setBrightness(led_brightness);
      return true;
    case SettingId::ScreensaverMode:
      screensaver_mode = static_cast<uint8_t>(value);
      menu_screensaver = screensaver_mode;
      return true;
    case SettingId::ScreensaverAnimation:
      screensaver_animation = static_cast<uint8_t>(value);
      menu_screensaver_anim = screensaver_animation;
      return true;
    case SettingId::OledWake:
      oled_wake_mode = static_cast<uint8_t>(value);
      menu_oled_wake = oled_wake_mode;
      return true;
    case SettingId::MenuOkBack:
      menu_ok_back = static_cast<uint8_t>(value);
      menu_menu_ok_back = menu_ok_back;
      suppressMenuControllerInputForMs(100);
      return true;
    case SettingId::OledInactive:
      oled_inactive_mode = static_cast<uint8_t>(value);
      menu_oled_inactive = oled_inactive_mode;
      return true;
    case SettingId::MenuHotkey:
      menu_menu_hotkey = static_cast<uint8_t>(value);
      return true;
    case SettingId::SystemMenuHotkey:
      menu_system_menu_hotkey = static_cast<uint8_t>(value);
      return true;
    case SettingId::HomeHotkey:
      menu_home_hotkey = static_cast<uint8_t>(value);
      return true;
    case SettingId::HoldStartHotkey:
      menu_hold_start_hotkey = static_cast<uint8_t>(value);
      return true;
    case SettingId::CaptureHotkey:
      menu_capture_hotkey = static_cast<uint8_t>(value);
      return true;
    case SettingId::KioskMode:
      menu_kiosk_mode = static_cast<uint8_t>(value);
      return true;
    case SettingId::Usb2Db15MultiBind:
      menu_usb2db15_multi_bind = static_cast<uint8_t>(value);
      return true;
    case SettingId::Usb2Db15GameSelection:
      menu_usb2db15_game_selection = static_cast<uint8_t>(value);
      return true;
#if defined(ADAPT_FEATURE_USB_DRIVE)
    case SettingId::ManagementMsc:
      // Takes effect when USB is next configured (reboot or reconnect).
      menu_management_msc = static_cast<uint8_t>(value);
      return true;
#endif
    case SettingId::WinOutput: {
      const bool changed = static_cast<uint8_t>(value) != menu_win_output;
      if (changed) auto_detect_clear_scratch_state();
      menu_win_output = static_cast<uint8_t>(value);
      return true;
    }
    case SettingId::MisterOutput: {
      const bool changed = static_cast<uint8_t>(value) != menu_mister_output;
      if (changed) auto_detect_clear_scratch_state();
      menu_mister_output = static_cast<uint8_t>(value);
      return true;
    }
    case SettingId::Switch2Output: {
      const bool changed = static_cast<uint8_t>(value) != menu_switch2_output;
      if (changed) auto_detect_clear_scratch_state();
      menu_switch2_output = static_cast<uint8_t>(value);
      return true;
    }
    case SettingId::Ps2RemoteOutput:
      menu_ps2_remote_output = static_cast<uint8_t>(value);
      return true;
    case SettingId::MisterDisplay:
      mister_display_mode = static_cast<uint8_t>(value);
      menu_mister_display_mode = mister_display_mode;
      return true;
    case SettingId::TouchpadMouse:
      menu_touchpad_mouse = static_cast<uint8_t>(value);
      return true;
    case SettingId::PsxPeriph:
      menu_psx_periph = static_cast<uint8_t>(value);
      return true;
    case SettingId::SnesRumbleTech:
      snes_rumbletech_enabled = static_cast<uint8_t>(value);
      menu_snes_rumbletech = snes_rumbletech_enabled;
      return true;
    case SettingId::HomeScreenDebug:
      menu_home_screen_debug = static_cast<uint8_t>(value);
      return true;
    case SettingId::HomeButtonLabels:
      menu_home_button_labels = static_cast<uint8_t>(value);
      return true;
    case SettingId::HomeJvsView:
      menu_home_jvs_view = static_cast<uint8_t>(value);
      return true;
    case SettingId::HotkeyHoldTime:
      menu_hotkey_hold_time = static_cast<uint8_t>(value);
      return true;
    default:
      return false;
  }
}

bool readRuntimeSettingValue(SettingId id, DeviceEnum mode, int32_t* value) {
  if (value == nullptr) return false;
  if (!serialSettingSupportedOnProduct(id)) return false;
  if (settingIsPerMode(id)) {
    if (mode != deviceMode) return false;
    if (id == SettingId::ClassicDualMerge) {
      *value = menu_classic_dual_merge;
      return true;
    }
    PerModeSettingsRecord settings{};
    captureRuntimePerModeSettings(mode, settings);
    *value = readRecordSettingValue(settings, id);
    return true;
  }

  switch (id) {
    case SettingId::PersistedInputMode: return false;
    case SettingId::ConfiguredOutputMode: *value = configuredOutputMode; break;
    case SettingId::DisplayContrast: *value = display_contrast; break;
    case SettingId::LatencyTest: *value = latencyTest.isEnabled() ? 1 : 0; break;
    case SettingId::LatencyControllerInLoop:
      *value = latencyTest.isControllerInLoop() ? 1 : 0; break;
    case SettingId::LatencyHostType: *value = latencyTest.getHostType(); break;
    case SettingId::BuzzerMode: *value = menu_buzzer_mode; break;
    case SettingId::SoundEvents: *value = buzzer.getEventMask(); break;
    case SettingId::LedMode: *value = rgbLed.getMode(); break;
    case SettingId::LedBrightness: *value = rgbLed.getBrightness(); break;
    case SettingId::ScreensaverMode: *value = screensaver_mode; break;
    case SettingId::ScreensaverAnimation: *value = screensaver_animation; break;
    case SettingId::OledInactive: *value = oled_inactive_mode; break;
    case SettingId::OledWake: *value = oled_wake_mode; break;
    case SettingId::MenuOkBack: *value = menu_ok_back; break;
    case SettingId::MenuHotkey: *value = menu_menu_hotkey; break;
    case SettingId::SystemMenuHotkey: *value = menu_system_menu_hotkey; break;
    case SettingId::HomeHotkey: *value = menu_home_hotkey; break;
    case SettingId::HoldStartHotkey: *value = menu_hold_start_hotkey; break;
    case SettingId::CaptureHotkey: *value = menu_capture_hotkey; break;
    case SettingId::KioskMode: *value = menu_kiosk_mode; break;
    case SettingId::Usb2Db15MultiBind: *value = menu_usb2db15_multi_bind; break;
    case SettingId::Usb2Db15GameSelection: *value = menu_usb2db15_game_selection; break;
#if defined(ADAPT_FEATURE_USB_DRIVE)
    case SettingId::ManagementMsc: *value = menu_management_msc; break;
#endif
    case SettingId::WinOutput: *value = menu_win_output; break;
    case SettingId::MisterOutput: *value = menu_mister_output; break;
    case SettingId::Switch2Output: *value = menu_switch2_output; break;
    case SettingId::Ps2RemoteOutput: *value = menu_ps2_remote_output; break;
    case SettingId::MisterDisplay: *value = mister_display_mode; break;
    case SettingId::TouchpadMouse: *value = menu_touchpad_mouse; break;
    case SettingId::PsxPeriph: *value = menu_psx_periph; break;
    case SettingId::SnesRumbleTech: *value = snes_rumbletech_enabled; break;
    case SettingId::HomeScreenDebug: *value = menu_home_screen_debug; break;
    case SettingId::HomeButtonLabels: *value = menu_home_button_labels; break;
    case SettingId::HomeJvsView: *value = menu_home_jvs_view; break;
    case SettingId::HotkeyHoldTime: *value = menu_hotkey_hold_time; break;
    default: return false;
  }
  return true;
}

void printSettingLine(Print& out, SettingId id, DeviceEnum mode) {
  const SettingSpec& spec = settingSpec(id);
  out.print(F("SET ID="));
  out.print((int)id);
  out.print(F(" NAME="));
  out.print(settingIdName(id));
  out.print(F(" S="));
  out.print(settingScopeName(spec.scope));
  out.print(F(" T="));
  out.print(settingTypeName(spec.value_type));
  out.print(F(" MIN="));
  out.print(spec.min_value);
  out.print(F(" MAX="));
  out.print(spec.max_value);
  out.print(F(" DEF="));
  out.print(defaultSettingValue(id, mode));
  out.print(F(" VAL="));
  const int32_t stored = loadSettingValue(id, mode);
  out.print(stored);
  int32_t runtime = 0;
  if (readRuntimeSettingValue(id, mode, &runtime)) {
    out.print(F(" RUN="));
    out.print(runtime);
    out.print(F(" LIVE="));
    out.print(runtime == stored ? 1 : 0);
  } else {
    out.print(F(" LIVE=NA"));
  }
  if (settingIsPerMode(id)) {
    out.print(F(" MODE="));
    out.print((int)mode);
  }
  out.println();
}

const char* hotkeyBindingName(HotkeyBindingId id) {
  switch (id) {
    case HotkeyBindingId::Menu: return "MENU";
    case HotkeyBindingId::SystemMenu: return "SYSTEM";
    case HotkeyBindingId::Home: return "HOME";
    case HotkeyBindingId::Capture: return "CAPTURE";
    case HotkeyBindingId::Count: break;
  }
  return "?";
}

bool parseHotkeyBindingName(char*& text, HotkeyBindingId* id) {
  text = serialSkipSpaces(text);
  if (serialTokenEquals(text, "MENU") || serialTokenEquals(text, "OSD")) {
    *id = HotkeyBindingId::Menu;
  } else if (serialTokenEquals(text, "SYSTEM") || serialTokenEquals(text, "SYSMENU") || serialTokenEquals(text, "CONFIG")) {
    *id = HotkeyBindingId::SystemMenu;
  } else if (serialTokenEquals(text, "HOME") || serialTokenEquals(text, "GUIDE")) {
    *id = HotkeyBindingId::Home;
  } else if (serialTokenEquals(text, "CAPTURE") || serialTokenEquals(text, "CAP")) {
    *id = HotkeyBindingId::Capture;
  } else {
    return false;
  }

  while (*text != '\0' && *text != ' ' && *text != '\t') {
    ++text;
  }
  text = serialSkipSpaces(text);
  return true;
}

void applyHotkeysToRuntime(const HotkeyBindingSet& bindings) {
  HotkeyBindingSet sanitized = bindings;
  sanitizeHotkeyBindings(sanitized);
  menu_hotkey_combo = sanitized.menu;
  menu_system_hotkey_combo = sanitized.system_menu;
  menu_home_hotkey_combo = sanitized.home;
  menu_capture_hotkey_combo = sanitized.capture;
  menu_menu_hotkey = menu_hotkey_combo != 0 ? 1 : 0;
  menu_system_menu_hotkey = menu_system_hotkey_combo != 0 ? 1 : 0;
  menu_home_hotkey = menu_home_hotkey_combo != 0 ? 1 : 0;
  menu_capture_hotkey = menu_capture_hotkey_combo != 0 ? 1 : 0;
}

void printHotkeyLine(Print& out, const HotkeyBindingSet& bindings, HotkeyBindingId id) {
  char formatted[32];
  const uint32_t combo = hotkeyBindingValue(bindings, id);
  formatHotkeyCombo(formatted, sizeof(formatted), combo, deviceMode);
  out.print(F("HOTKEY ID="));
  out.print(hotkeyBindingName(id));
  out.print(F(" MASK="));
  printHex32(out, combo);
  out.print(F(" NAME="));
  out.println(formatted);
}

bool handleSerialHotkeyCommandImpl(char* text, Print& out) {
#if ADAPT_POLICY_FIXED_HOTKEY_ASSIGNMENTS
  if (serialTextEqualsExact(text, "ON") || serialTextEqualsExact(text, "OFF")) {
    menu_home_hotkey = serialTextEqualsExact(text, "ON") ? 1 : 0;
    saveSystemSettingByte(SettingId::HomeHotkey, menu_home_hotkey);
    out.println(F("OK:HOTKEYS"));
  } else if (*text == '\0' || serialTextEqualsExact(text, "HELP") ||
             serialTextEqualsExact(text, "LIST") || serialTextEqualsExact(text, "MODE")) {
    out.print(F("HOTKEYS FIXED=1 ENABLED="));
    out.println(menu_home_hotkey);
    out.println(F("HOTKEY ON/OFF: enable/disable preset Start+direction and Hold Start shortcuts"));
  } else {
    out.println(F("ERR:HOTKEYS_FIXED; USE_HOTKEY_ON_OFF"));
  }
  return true;
#else
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  char* directionArgs=nullptr;
  if (serialCommandStartsWith(text,"MODE",&directionArgs)) {
    PerModeSettingsRecord settings{}; readPerModeSettings(deviceMode,settings);
    uint8_t actions[3]; direction_hotkey::read(settings,actions);
    if (*serialSkipSpaces(directionArgs)) {
      for (auto& action:actions) {
        long value=0;
        if (!serialParseLongToken(directionArgs,&value) || value<0 || value>=direction_hotkey::Count) {
          out.println(F("ERR:HOTKEY_MODE_ARGS")); return true;
        }
        action=uint8_t(value);
      }
      if (*serialSkipSpaces(directionArgs) || !isConcreteModeForPerSettings(deviceMode) ||
          !directionalHotkeysValid(deviceMode,actions)) {
        out.println(F("ERR:HOTKEY_MODE_CONFLICT")); return true;
      }
      direction_hotkey::write(settings,actions); writePerModeSettings(deviceMode,settings);
      applyPerModeSettings(settings);
    }
    out.print(F("HOTKEY MODE LEFT=")); out.print(actions[0]);
    out.print(F(" RIGHT=")); out.print(actions[1]);
    out.print(F(" UP=")); out.println(actions[2]);
    out.println(F("OK:HOTKEY_MODE")); return true;
  }
#endif
  if (*text == '\0' || serialTextEqualsExact(text, "HELP")) {
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
    out.println(F("HOTKEY MODE [LEFT RIGHT UP]: per-input action IDs; AUTO=0 OFF=1 A..R3=2..11 SELECT=12 CAPTURE=13 QUICK=14 SYSTEM=15"));
    out.println(F("HOTKEY SET HOME <0|0x20000400>: global Down+Start; legacy MENU/SYSTEM/CAPTURE bindings are inactive"));
#endif
    out.println(F("HOTKEY CMDS:HOTKEY LIST,HOTKEY GET <ID>,HOTKEY SET <ID> <MASK>,HOTKEY HOLD <0-6>,HOTKEY DEFAULTS CONFIRM"));
    return true;
  }

  HotkeyBindingSet bindings{};
  readHotkeyBindings(bindings);
  if (serialTextEqualsExact(text, "LIST")) {
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
    printHotkeyLine(out, bindings, HotkeyBindingId::Home);
    out.println(F("Per-input Left/Right/Up: use HOTKEY MODE"));
#else
    for (uint8_t raw = 0; raw < (uint8_t)HotkeyBindingId::Count; ++raw) {
      printHotkeyLine(out, bindings, (HotkeyBindingId)raw);
    }
#endif
    out.print(F("HOTKEY HOLD="));
    out.print((int)menu_hotkey_hold_time);
    out.print(F(" MS="));
    out.println(hotkeyHoldMsForSetting(menu_hotkey_hold_time));
    out.println(F("OK:HOTKEY_LIST"));
    return true;
  }

  char* remainder = nullptr;
  if (serialCommandStartsWith(text, "GET", &remainder)) {
    HotkeyBindingId id = HotkeyBindingId::Count;
    if (!parseHotkeyBindingName(remainder, &id) ||
        *serialSkipSpaces(remainder) != '\0') {
      out.println(F("ERR:BAD_HOTKEY_ID"));
      return true;
    }
    printHotkeyLine(out, bindings, id);
    return true;
  }

  if (serialCommandStartsWith(text, "SET", &remainder)) {
    HotkeyBindingId id = HotkeyBindingId::Count;
    uint32_t combo = 0;
    if (!parseHotkeyBindingName(remainder, &id) ||
        !serialParseUint32Token(remainder, &combo) ||
        *serialSkipSpaces(remainder) != '\0') {
      out.println(F("ERR:BAD_HOTKEY_ARGS"));
      return true;
    }
    if (combo != 0 && !isValidHotkeyCombo(combo)) {
      out.println(F("ERR:BAD_HOTKEY_COMBO"));
      return true;
    }
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
    if (id!=HotkeyBindingId::Home || (combo!=0 && combo!=(INPUT_START|INPUT_PAD_D))) {
      out.println(F("ERR:USE_HOTKEY_MODE; DOWN_START_IS_GLOBAL")); return true;
    }
#endif
    if (combo != 0 && hotkeyComboConflicts(bindings, id, combo)) {
      out.println(F("ERR:HOTKEY_CONFLICT"));
      return true;
    }
    uint32_t* slot = hotkeyBindingSlot(bindings, id);
    if (slot == nullptr) {
      out.println(F("ERR:BAD_HOTKEY_ID"));
      return true;
    }
    *slot = combo;
    sanitizeHotkeyBindings(bindings);
    writeHotkeyBindings(bindings);
    applyHotkeysToRuntime(bindings);
    printHotkeyLine(out, bindings, id);
    out.println(F("OK:HOTKEY_SET"));
    return true;
  }

  if (serialCommandStartsWith(text, "HOLD", &remainder)) {
    long rawValue = 0;
    if (!serialParseLongToken(remainder, &rawValue) ||
        *serialSkipSpaces(remainder) != '\0' ||
        rawValue < 0 || rawValue > 6) {
      out.println(F("ERR:BAD_HOLD_VALUE"));
      return true;
    }
    const int32_t sanitized = sanitizeSettingValue(SettingId::HotkeyHoldTime, RZORD_NONE, rawValue);
    saveSettingValue(SettingId::HotkeyHoldTime, sanitized, RZORD_NONE);
    menu_hotkey_hold_time = (uint8_t)sanitized;
    out.print(F("OK:HOTKEY_HOLD VAL="));
    out.print((int)menu_hotkey_hold_time);
    out.print(F(" MS="));
    out.println(hotkeyHoldMsForSetting(menu_hotkey_hold_time));
    return true;
  }

  if (serialTextEqualsExact(text, "DEFAULTS") ||
      serialTextEqualsExact(text, "RESET")) {
    out.println(F("ERR:CONFIRM_REQUIRED"));
    return true;
  }

  if (serialTextEqualsExact(text, "DEFAULTS CONFIRM") ||
      serialTextEqualsExact(text, "RESET CONFIRM")) {
    setDefaultHotkeyBindings(bindings);
    writeHotkeyBindings(bindings);
    applyHotkeysToRuntime(bindings);
    out.println(F("OK:HOTKEY_DEFAULTS"));
    return true;
  }

  out.println(F("ERR:BAD_HOTKEY_CMD"));
  return true;
#endif
}

void printChordLine(Print& out, uint8_t slot) {
  out.print(F("CHORD SLOT="));
  out.print((int)slot);
  out.print(F(" SRC="));
  printHex32(out, active_chord_remaps.slots[slot].source_combo);
  out.print(F(" OUT="));
  printHex32(out, active_chord_remaps.slots[slot].output_combo);
  out.println();
}

bool handleSerialChordCommandImpl(char* text, Print& out) {
  if (*text == '\0' || serialTextEqualsExact(text, "HELP")) {
    out.println(F("CHORD CMDS:CHORD LIST,CHORD SET <SLOT> <SRC_MASK> <OUT_MASK>,CHORD CLR <SLOT|ALL> CONFIRM"));
    return true;
  }

  if (serialTextEqualsExact(text, "LIST")) {
    for (uint8_t slot = 0; slot < BUTTON_CHORD_REMAP_SLOT_COUNT; ++slot) {
      printChordLine(out, slot);
    }
    out.println(F("OK:CHORD_LIST"));
    return true;
  }

  char* remainder = nullptr;
  if (serialCommandStartsWith(text, "SET", &remainder)) {
    long rawSlot = -1;
    uint32_t source = 0;
    uint32_t output = 0;
    if (!serialParseLongToken(remainder, &rawSlot) ||
        rawSlot < 0 ||
        rawSlot >= BUTTON_CHORD_REMAP_SLOT_COUNT ||
        !serialParseUint32Token(remainder, &source) ||
        !serialParseUint32Token(remainder, &output) ||
        *serialSkipSpaces(remainder) != '\0') {
      out.println(F("ERR:BAD_CHORD_ARGS"));
      return true;
    }
    if (!setButtonChordRemap((uint8_t)rawSlot, source, output)) {
      out.println(F("ERR:BAD_CHORD_MASK"));
      return true;
    }
    printChordLine(out, (uint8_t)rawSlot);
    out.println(F("OK:CHORD_SET"));
    return true;
  }

  if (serialCommandStartsWith(text, "CLR", &remainder) ||
      serialCommandStartsWith(text, "CLEAR", &remainder)) {
    char* confirmation = nullptr;
    if (serialCommandStartsWith(remainder, "ALL", &confirmation)) {
      if (!serialTextEqualsExact(confirmation, "CONFIRM")) {
        out.println(F("ERR:CONFIRM_REQUIRED"));
        return true;
      }
      clearButtonChordRemaps();
      out.println(F("OK:CHORD_CLEAR_ALL"));
      return true;
    }
    if (*serialSkipSpaces(remainder) == '\0') {
      out.println(F("ERR:CONFIRM_REQUIRED"));
      return true;
    }
    long rawSlot = -1;
    if (!serialParseLongToken(remainder, &rawSlot) ||
        rawSlot < 0 ||
        rawSlot >= BUTTON_CHORD_REMAP_SLOT_COUNT) {
      out.println(F("ERR:BAD_CHORD_SLOT"));
      return true;
    }
    if (!serialTextEqualsExact(remainder, "CONFIRM")) {
      out.println(F("ERR:CONFIRM_REQUIRED"));
      return true;
    }
    ButtonChordRemapSet remaps = active_chord_remaps;
    remaps.slots[rawSlot].source_combo = 0;
    remaps.slots[rawSlot].output_combo = 0;
    writeButtonChordRemaps(remaps);
    out.println(F("OK:CHORD_CLEAR"));
    return true;
  }

  out.println(F("ERR:BAD_CHORD_CMD"));
  return true;
}

void printFrameLine(Print& out, uint8_t index) {
  const controller_state_t& frame = controllerFrameConst(index);
  out.print(F("FRAME I="));
  out.print((int)index);
  out.print(F(" CONN="));
  out.print(frame.connected ? 1 : 0);
  out.print(F(" CFG="));
  out.print((int)frame.config);
  out.print(F(" BTN="));
  printHex32(out, frame.digital_buttons);
  out.print(F(" LX="));
  out.print(frame.LX);
  out.print(F(" LY="));
  out.print(frame.LY);
  out.print(F(" RX="));
  out.print(frame.RX);
  out.print(F(" RY="));
  out.print(frame.RY);
  out.print(F(" L2="));
  out.print((int)frame.ANALOG_L2);
  out.print(F(" R2="));
  out.print((int)frame.ANALOG_R2);
  out.print(F(" POX="));
  out.print(post_output_lx[index]);
  out.print(F(" POY="));
  out.print(post_output_ly[index]);
  out.print(F(" PORX="));
  out.print(post_output_rx[index]);
  out.print(F(" PORY="));
  out.print(post_output_ry[index]);
  out.print(F(" HIDX="));
  out.print((int)debug_hid_x[index]);
  out.print(F(" HIDY="));
  out.print((int)debug_hid_y[index]);
  out.print(F(" T0="));
  out.print(frame.touch[0].active ? 1 : 0);
  out.print(',');
  out.print((int)frame.touch[0].id);
  out.print(',');
  out.print(frame.touch[0].x);
  out.print(',');
  out.print(frame.touch[0].y);
  out.print(F(" T1="));
  out.print(frame.touch[1].active ? 1 : 0);
  out.print(',');
  out.print((int)frame.touch[1].id);
  out.print(',');
  out.print(frame.touch[1].x);
  out.print(',');
  out.print(frame.touch[1].y);
#if ADAPT_POLICY_TOUCHPAD_MOUSE_TELEMETRY
  out.print(F(" TM="));
  appendTouchpadMouseDebug(out, index);
#endif
  out.print(F(" NAME="));
  out.println(frame.controller_type_name);
}

void printClassicInputDebug(Print& out) {
#ifdef ADAPT_INPUT_RETRO
  out.print(F("CLASSIC MODE="));
  out.print((int)deviceMode);
  out.print(F(" SAVED="));
  out.print((int)savedDeviceMode);
  out.print(F(" PORTS="));
  out.print((int)inputPortCount());
  out.print(F(" POLL_US="));
  out.print(currentInputModulePollIntervalUs());
  out.print(F(" DESC="));
  out.println(currentInputModuleDescriptionOr("-"));

  #ifdef ENABLE_INPUT_PCE
  if (deviceMode == RZORD_PCE || savedDeviceMode == RZORD_PCE) {
    const PceInputDebugSnapshot pce = input_pce_debug_snapshot();
    out.print(F("PCECFG UP="));
    out.print(PCE_TYPE_UPGRADE_CONFIRM_POLLS);
    out.print(F(" CH="));
    out.print(PCE_TYPE_CHANGE_CONFIRM_POLLS);
    out.print(F(" DOWN="));
    out.println(PCE_TYPE_DOWNGRADE_CONFIRM_POLLS);
    for (uint8_t i = 0; i < pce.slot_count && i < MAX_USB_OUT; ++i) {
      const PceInputDebugSlot& slot = pce.slots[i];
      out.print(F("PCE I="));
      out.print((int)i);
      out.print(F(" ST="));
      out.print((int)slot.stable_type);
      out.print(F(" OBS="));
      out.print((int)slot.observed_type);
      out.print(F(" PEND="));
      out.print((int)slot.pending_type);
      out.print(F(" CNT="));
      out.print(slot.pending_count);
      out.print(F(" PORT="));
      out.print((int)slot.source_port);
      out.print(F(" IDX="));
      out.println((int)slot.source_index);
    }
    for (uint8_t p = 0; p < kPceDebugMousePorts; ++p) {
      const PceMouseDebugPort& mouse = pce.mouse[p];
      if (mouse.raw_misses == 0) continue;
      out.print(F("PCEM P="));
      out.print((int)p);
      out.print(F(" MISS="));
      out.print(mouse.raw_misses);
      out.print(F(" OPEN="));
      out.print(mouse.open_sweeps);
      out.print(F(" LAST="));
      out.print(mouse.last_raw, HEX);
      out.print(':');
      for (uint8_t i = 0; i < 4; ++i) out.print(mouse.last_data[i], HEX);
      out.print('/');
      for (uint8_t i = 0; i < 4; ++i) out.print(mouse.last_buttons[i], HEX);
      out.println();
    }
  }
  #endif

  #ifdef ENABLE_INPUT_SATURN
  if (deviceMode == RZORD_SATURN || savedDeviceMode == RZORD_SATURN ||
      deviceMode == RZORD_MEGADRIVE || savedDeviceMode == RZORD_MEGADRIVE) {
    const SaturnInputDebugSnapshot saturn = input_saturn_debug_snapshot();
    for (uint8_t i = 0; i < saturn.port_count && i < MAX_USB_OUT; ++i) {
      const SaturnInputDebugPort& port = saturn.ports[i];
      out.print(F("SATP I="));
      out.print((int)i);
      out.print(F(" TAP="));
      out.print((int)port.tap_ports);
      out.print(F(" LAST="));
      out.print((int)port.last_tap_ports);
      out.print(F(" CNT="));
      out.print((int)port.controller_count);
      out.print(F(" RSV="));
      out.println((int)port.reserved_slots);
    }
    for (uint8_t i = 0; i < saturn.slot_count && i < MAX_USB_OUT; ++i) {
      const SaturnInputDebugSlot& slot = saturn.slots[i];
      out.print(F("SAT I="));
      out.print((int)i);
      out.print(F(" ST="));
      out.print((int)slot.stable_type);
      out.print(F(" OBS="));
      out.print((int)slot.observed_type);
      out.print(F(" MST="));
      out.print((int)slot.megadrive_stable_type);
      out.print(F(" MDOWN="));
      out.print(slot.megadrive_downgrade_ms);
      out.print(F(" TAP="));
      out.print((int)slot.tap_ports);
      out.print(F(" PORT="));
      out.print((int)slot.source_port);
      out.print(F(" IDX="));
      out.println((int)slot.source_index);
    }
    out.print(F("SATM SLOT="));
    out.print((int)saturn_debug_mouse_slot);
    out.print(F(" FLAGS=0x"));
    out.print((int)saturn_debug_mouse_flags, HEX);
    out.print(F(" MX="));
    out.print((int)saturn_debug_mouse_x);
    out.print(F(" MY="));
    out.print((int)saturn_debug_mouse_y);
    out.print(F(" MRX="));
    out.print((int)saturn_debug_mouse_x_min);
    out.print(F("/"));
    out.print((int)saturn_debug_mouse_x_max);
    out.print(F(" MRY="));
    out.print((int)saturn_debug_mouse_y_min);
    out.print(F("/"));
    out.print((int)saturn_debug_mouse_y_max);
    out.print(F(" RAWX="));
    out.print((int)saturn_debug_mouse_raw_x);
    out.print(F(" RAWY="));
    out.print((int)saturn_debug_mouse_raw_y);
    out.print(F(" RRX="));
    out.print((int)saturn_debug_mouse_raw_x_min);
    out.print(F("/"));
    out.print((int)saturn_debug_mouse_raw_x_max);
    out.print(F(" RRY="));
    out.print((int)saturn_debug_mouse_raw_y_min);
    out.print(F("/"));
    out.print((int)saturn_debug_mouse_raw_y_max);
    out.print(F(" MBTN="));
    out.print((int)saturn_debug_mouse_buttons);
    out.print(F(" MOVF="));
    out.println(saturn_debug_mouse_overflow ? 1 : 0);

    out.print(F("SATRAW SLOT="));
    out.print((int)saturn_debug_raw_slot);
    out.print(F(" CT="));
    out.print((int)saturn_debug_raw_type);
    out.print(F(" DS="));
    out.print((int)saturn_debug_raw_size);
    out.print(F(" N="));
    out.print((int)saturn_debug_raw_count);
    out.print(F(" X="));
    out.print((int)saturn_debug_raw_x);
    out.print(F(" XR="));
    out.print((int)saturn_debug_raw_x_min);
    out.print(F("/"));
    out.print((int)saturn_debug_raw_x_max);
    out.print(F(" Y="));
    out.print((int)saturn_debug_raw_y);
    out.print(F(" L="));
    out.print((int)saturn_debug_raw_l);
    out.print(F(" R="));
    out.print((int)saturn_debug_raw_r);
    out.print(F(" NB="));
    for (uint8_t i = 0; i < saturn_debug_raw_count; ++i) {
      out.print((int)(saturn_debug_raw_nibbles[i] & 0x0F), HEX);
    }
    out.println();

    out.print(F("SATMAP SLOT="));
    out.print((int)saturn_debug_map_slot);
    out.print(F(" LX="));
    out.print(saturn_debug_map_lx);
    out.print(F(" LXR="));
    out.print(saturn_debug_map_lx_min);
    out.print(F("/"));
    out.print(saturn_debug_map_lx_max);
    out.print(F(" PAD="));
    out.print((int)saturn_debug_map_paddle);
    out.print(F(" FL="));
    out.println((int)saturn_debug_map_flags, HEX);
  }
  #endif

  #ifdef ENABLE_INPUT_SNES
  if (deviceMode == RZORD_NES || savedDeviceMode == RZORD_NES ||
      deviceMode == RZORD_SNES || savedDeviceMode == RZORD_SNES ||
      deviceMode == RZORD_VBOY || savedDeviceMode == RZORD_VBOY) {
    out.print(F("SNES P0 ID="));
    out.print((int)snes_debug_id0);
    out.print(F(" OBS="));
    out.print((int)snes_debug_dtype0);
    out.print(F(" ST="));
    out.print((int)snes_debug_stable_dtype0);
    out.print(F(" CAND="));
    out.print((int)snes_debug_candidate_dtype0);
    out.print(F("/"));
    out.print((int)snes_debug_candidate_count0);
    out.print(F(" GLITCH="));
    out.print(snes_debug_glitch_frames0);
    out.print(F(" RAW=0x"));
    out.print(snes_debug_raw_digital0, HEX);
    out.print(F(" FILT=0x"));
    out.print(snes_debug_filtered_digital0, HEX);
    out.print(F(" EXT=0x"));
    out.print(snes_debug_extended0, HEX);
    out.print(F(" FDROP="));
    out.print(snes_debug_filter_drop0);
    out.print(F(" BAD="));
    out.print(snes_debug_invalid_frame0);
    out.print(F("/"));
    out.print(snes_debug_invalid_id0);
    out.print(F("/"));
    out.print(snes_debug_all_pressed0);
    out.print(F(" RTECH="));
    out.print(snes_rumbletech_detected0 ? 1 : 0);
    out.print(F(" SAFE="));
    out.print(snes_debug_safe_poll0 ? 1 : 0);
    out.print(F(" RDATA=0x"));
    out.print((int)snes_rumble_debug_data0, HEX);
    out.print(F(" RTX="));
    out.print(snes_rumble_debug_tx0);
    out.print(F(" TAP="));
    out.print((int)snes_debug_tap0);
    out.print(F(" MTAP="));
    out.print(snes_debug_mtap_enabled0 ? 1 : 0);
    out.print(F(" FS="));
    out.print(snes_debug_fourscore0 ? 1 : 0);
    out.print(F(" MEXT=0x"));
    out.print((int)snes_debug_mouse_ext, HEX);
    out.print(F(" MX="));
    out.print((int)snes_debug_mouse_x);
    out.print(F(" MY="));
    out.print((int)snes_debug_mouse_y);
    out.print(F(" MRX="));
    out.print((int)snes_debug_mouse_x_min);
    out.print(F("/"));
    out.print((int)snes_debug_mouse_x_max);
    out.print(F(" MRXE=0x"));
    out.print((int)snes_debug_mouse_x_min_ext, HEX);
    out.print(F("/0x"));
    out.print((int)snes_debug_mouse_x_max_ext, HEX);
    out.print(F(" MRY="));
    out.print((int)snes_debug_mouse_y_min);
    out.print(F("/"));
    out.print((int)snes_debug_mouse_y_max);
    out.print(F(" MRYE=0x"));
    out.print((int)snes_debug_mouse_y_min_ext, HEX);
    out.print(F("/0x"));
    out.print((int)snes_debug_mouse_y_max_ext, HEX);
    out.print(F(" MBTN="));
    out.println((int)snes_debug_mouse_buttons);

    out.print(F("SNES P1 ID="));
    out.print((int)snes_debug_id1);
    out.print(F(" OBS="));
    out.print((int)snes_debug_dtype1);
    out.print(F(" ST="));
    out.print((int)snes_debug_stable_dtype1);
    out.print(F(" CAND="));
    out.print((int)snes_debug_candidate_dtype1);
    out.print(F("/"));
    out.print((int)snes_debug_candidate_count1);
    out.print(F(" GLITCH="));
    out.print(snes_debug_glitch_frames1);
    out.print(F(" RAW=0x"));
    out.print(snes_debug_raw_digital1, HEX);
    out.print(F(" FILT=0x"));
    out.print(snes_debug_filtered_digital1, HEX);
    out.print(F(" EXT=0x"));
    out.print(snes_debug_extended1, HEX);
    out.print(F(" FDROP="));
    out.print(snes_debug_filter_drop1);
    out.print(F(" BAD="));
    out.print(snes_debug_invalid_frame1);
    out.print(F("/"));
    out.print(snes_debug_invalid_id1);
    out.print(F("/"));
    out.print(snes_debug_all_pressed1);
    out.print(F(" RTECH="));
    out.print(snes_rumbletech_detected1 ? 1 : 0);
    out.print(F(" SAFE="));
    out.print(snes_debug_safe_poll1 ? 1 : 0);
    out.print(F(" RDATA=0x"));
    out.print((int)snes_rumble_debug_data1, HEX);
    out.print(F(" RTX="));
    out.print(snes_rumble_debug_tx1);
    out.print(F(" TAP="));
    out.print((int)snes_debug_tap1);
    out.print(F(" MTAP="));
    out.print(snes_debug_mtap_enabled1 ? 1 : 0);
    out.print(F(" FS="));
    out.println(snes_debug_fourscore1 ? 1 : 0);
  }
  #endif

  #ifdef ENABLE_INPUT_N64
  if (deviceMode == RZORD_N64 || savedDeviceMode == RZORD_N64) {
    for (uint8_t i = 0; i < GC64_DEBUG_PORTS; ++i) {
      out.print(F("N64 P"));
      out.print((int)i);
      out.print(F(" TYPE="));
      out.print((int)gc64_debug_device_type[i]);
      out.print(F(" AUX=0x"));
      out.print((int)gc64_n64_accessory_aux[i], HEX);
      out.print(F(" RPAK="));
      out.print(gc64_n64_rumble_pak_detected[i] ? 1 : 0);
      out.print(F(" RPEND="));
      out.print(gc64_n64_rumble_command_pending[i] ? 1 : 0);
      out.print(F(" RTRY="));
      out.print((int)gc64_n64_rumble_probe_attempts[i]);
      out.print(F(" RRES="));
      out.print(gc64_n64_rumble_probe_result[i]);
      out.print(F(" RBYTE=0x"));
      out.print((int)gc64_n64_rumble_probe_byte[i], HEX);
      out.print(F(" WM="));
      out.print(gc64_n64_rumble_motor_result[i]);
      out.print(F(" WX="));
      out.print(gc64_n64_rumble_motor_transport[i]);
      out.print(F(" WEXP=0x"));
      out.print((int)gc64_n64_rumble_motor_expected[i], HEX);
      out.print(F(" WACK=0x"));
      out.println((int)gc64_n64_rumble_motor_response[i], HEX);
    }
  }
  #endif

  #ifdef ENABLE_INPUT_PSX
  if (deviceMode == RZORD_PSX || savedDeviceMode == RZORD_PSX ||
      deviceMode == RZORD_PSX_JOG || savedDeviceMode == RZORD_PSX_JOG) {
    const PsxInputDebugSnapshot psx = input_psx_debug_snapshot();
    for (uint8_t i = 0; i < psx.slot_count && i < MAX_USB_OUT; ++i) {
      const PsxInputDebugSlot& slot = psx.slots[i];
      out.print(F("PSX I="));
      out.print((int)i);
      out.print(F(" CONN="));
      out.print((int)slot.connected);
      out.print(F(" PROTO="));
      out.print((int)slot.protocol);
      out.print(F(" MASK="));
      out.print((int)slot.special_mask);
      out.print(F(" FLAGS="));
      out.print((int)slot.flags);
      out.print(F(" BTN=0x"));
      out.println(slot.buttons, HEX);
    }
  }
  #endif
#endif
}

void printState(Print& out) {
  out.print(F("STATE INPUT="));
  out.print((int)deviceMode);
  out.print(F(" SAVED_INPUT="));
  out.print((int)savedDeviceMode);
  out.print(F(" CONFIG_OUT="));
  out.print((int)configuredOutputMode);
  out.print(F(" RUNTIME_OUT="));
  out.print((int)outputMode);
  out.print(F(" MAX="));
  out.print((int)max_devices);
  out.print(F(" QUIET_PS5="));
  out.print(is_ps5_timing_quiet_mode_active() ? 1 : 0);
#if defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED)
  out.print(F(" HEAP_FREE="));
  out.print(rp2040.getFreeHeap());
  out.print(F(" HEAP_USED="));
  out.print(rp2040.getUsedHeap());
#endif
  out.println();

  // Why the OLED has (not) gone idle: time since the last wake, its source,
  // the active timeout (0 = never) and what the panel currently shows.
  out.print(F("IDLE AGE_MS="));
  out.print((unsigned long)(millis() - lastActivityTime));
  out.print(F(" TIMEOUT_MS="));
  out.print((unsigned long)getScreensaverTimeoutMs());
  out.print(F(" INACTIVE="));
  out.print(isOledInactiveBlankMode() ? F("BLANK") : F("SCREENSAVER"));
  out.print(F(" SHOWING="));
  out.print(idleBlankActive ? F("BLANK") : idleDimActive ? F("DIM") :
            idleAnimationActive ? F("ANIMATION") : F("HOME"));
  out.print(F(" WAKE="));
  out.println(idleWakeSourceName(lastIdleWakeSource()));

  for (uint8_t index = 0; index < max_devices && index < MAX_USB_OUT; ++index) {
    printFrameLine(out, index);
  }

  printClassicInputDebug(out);

  featureModulesAppendSerialState(out);

#ifdef ADAPT_OUTPUT_USB_DEVICE
  const AuthStorageDiagnostics authDiag = authStorageLastDiagnostics();
  out.print(F("AUTH PS4="));
  out.print(authStorageKeyStatus() ? 1 : 0);
  out.print(F(" UP="));
  out.print((int)authStorageLastUploadStatus());
  out.print(F(" CRC="));
  printHex32(out, authStorageLastUploadCrc32());
  out.print(F(" SER="));
  out.print((int)authDiag.serial_region_state);
  out.print(F(" CH="));
  out.print((int)authDiag.received_chunk_count);
  out.print(F("/"));
  out.print((int)authDiag.total_chunk_count);
  out.print(F(" MISS="));
  out.print(authDiag.first_missing_offset);
  out.print(F(" ADDR="));
  out.println(authDiag.target_payload_address);
  // Single-XInput rumble is host/tool dependent; expose the last OUT frame.
  if (get_effective_output_mode() == OUTPUT_XINPUT) {
    const XInputAuthRuntimeState& xin = xinputAuthRuntimeState();
    out.print(F("XIN OUT="));
    out.print(xin.debug_xfer_out_count);
    out.print(F(" SZ="));
    out.print((int)xin.debug_last_out_size);
    out.print(F(" RUM="));
    out.print((int)xin.debug_last_rumble_left);
    out.print(F("/"));
    out.print((int)xin.debug_last_rumble_right);
    out.print(F(" PKT="));
    for (uint8_t b = 0; b < 8; ++b) {
      if (xin.debug_last_out_packet[b] < 16) {
        out.print('0');
      }
      out.print(xin.debug_last_out_packet[b], HEX);
    }
    out.println();
  }
#endif

#ifdef ENABLE_USB_AUTH_SIDECAR
  const UsbHostServiceStatus host = usb_host_service_status();
  const PsAuthDongleStatus auth = ps_auth_dongle_status();
  out.print(F("USBHOST AVAIL="));
  out.print(host.available ? 1 : 0);
  out.print(F(" STARTED="));
  out.print(host.started ? 1 : 0);
  out.print(F(" TASK="));
  out.print(host.task_count);
  out.print(F(" VID="));
  out.print(auth.vid, HEX);
  out.print(F(" PID="));
  out.print(auth.pid, HEX);
  out.print(F(" READY="));
  out.print(auth.signature_ready ? 1 : 0);
  out.print(F(" ERR="));
  out.println((int)auth.last_error);
#endif
}

void printGpioSnapshot(Print& out) {
  out.println(F("GPIO COUNT=30 FMT=P,V,DIR,FUNC,PU,PD"));
  for (uint8_t pin = 0; pin < 30; ++pin) {
    out.print(F("GPIO P="));
    out.print((int)pin);
    out.print(F(" V="));
    out.print(gpio_get(pin) ? 1 : 0);
    out.print(F(" DIR="));
    out.print(gpio_get_dir(pin) ? 'O' : 'I');
    out.print(F(" FUNC="));
    out.print((int)gpio_get_function(pin));
    out.print(F(" PU="));
    out.print(gpio_is_pulled_up(pin) ? 1 : 0);
    out.print(F(" PD="));
    out.println(gpio_is_pulled_down(pin) ? 1 : 0);
  }
  out.println(F("OK:GPIO"));
}

// PSX_TIMING stores only into a PSX record and ROTARY only on classic-to-USB
// images (psx_poll::storedInRecord, serialRotaryProfileApplies). Elsewhere the
// CAP line reports 0 for both and their handlers are compiled out.
#ifdef ADAPT_BRIDGE_CLASSIC_TO_USB
const char* serialRotaryProfileKindName(SerialRotaryProfileKind kind) {
  return kind == SerialRotaryProfileKind::JogCon ? "JOGCON" : "GENERIC";
}
#endif

#ifdef ENABLE_INPUT_PSX
void printSerialPsxTiming(
    Print& out, DeviceEnum mode, const SerialPsxTimingValue& value) {
  out.print(F("PSX_TIMING MODE="));
  out.print((int)mode);
  out.print(F(" RATE="));
  out.print((int)value.rate);
  out.print(F(" GAP="));
  out.print((int)value.gap);
  out.print(F(" CONFIGURED="));
  out.print(value.configured ? 1 : 0);
  out.print(F(" LIVE="));
  out.println(value.live ? 1 : 0);
}
#endif

#ifdef ADAPT_BRIDGE_CLASSIC_TO_USB
void printSerialRotaryProfile(
    Print& out, DeviceEnum mode, SerialRotaryProfileKind kind,
    const SerialRotaryProfileValue& value) {
  out.print(F("ROTARY MODE="));
  out.print((int)mode);
  out.print(F(" PROFILE="));
  out.print(serialRotaryProfileKindName(kind));
  out.print(F(" GAIN="));
  out.print((int)value.gain);
  out.print(F(" REVERSE="));
  out.print(value.reverse ? 1 : 0);
  out.print(F(" CONFIGURED="));
  out.print(value.configured ? 1 : 0);
  out.print(F(" LIVE="));
  out.println(value.live ? 1 : 0);
}
#endif

#ifdef ENABLE_INPUT_PSX
bool handleSerialPsxTimingCommand(char* text, Print& out) {
  SerialPsxTimingRequest request{};
  const SerialSemanticParseStatus parsed =
      parseSerialPsxTimingRequest(text, deviceMode, &request);
  if (parsed == SerialSemanticParseStatus::BadCommand) {
    out.println(F("ERR:BAD_PSX_TIMING_CMD"));
    return true;
  }
  if (parsed == SerialSemanticParseStatus::BadArguments) {
    out.println(F("ERR:BAD_PSX_TIMING_ARGS"));
    return true;
  }
  if (parsed != SerialSemanticParseStatus::Ok) {
    out.println(F("ERR:BAD_INPUT_MODE"));
    return true;
  }

  SerialPsxTimingValue value{};
  const SerialSemanticStatus status = request.write
      ? serialWritePsxTiming(
          request.mode, request.rate, request.gap, &value)
      : serialReadPsxTiming(request.mode, &value);
  if (status == SerialSemanticStatus::UnsupportedMode) {
    out.println(F("ERR:PSX_TIMING_UNSUPPORTED_MODE"));
    return true;
  }
  if (status == SerialSemanticStatus::WriteFailed) {
    out.println(F("ERR:PSX_TIMING_WRITE_FAILED"));
    return true;
  }
  if (status != SerialSemanticStatus::Ok) {
    out.println(F("ERR:BAD_PSX_TIMING_ARGS"));
    return true;
  }
  printSerialPsxTiming(out, request.mode, value);
  out.println(request.write ? F("OK:PSX_TIMING_SET VERIFY=1")
                            : F("OK:PSX_TIMING_GET"));
  return true;
}

void printSerialNegconInputs(
    Print& out, DeviceEnum mode, const SerialNegconInputsValue& value) {
  out.print(F("NEGCON MODE="));
  out.print((int)mode);
  out.print(F(" I="));
  out.print((value.digital & NEGCON_INPUT_I) ? 1 : 0);
  out.print(F(" II="));
  out.print((value.digital & NEGCON_INPUT_II) ? 1 : 0);
  out.print(F(" L="));
  out.print((value.digital & NEGCON_INPUT_L) ? 1 : 0);
  out.print(F(" LIVE="));
  out.println(value.live ? 1 : 0);
}

bool handleSerialNegconCommand(char* text, Print& out) {
  SerialNegconRequest request{};
  const SerialSemanticParseStatus parsed =
      parseSerialNegconRequest(text, deviceMode, &request);
  if (parsed == SerialSemanticParseStatus::BadCommand) {
    out.println(F("ERR:BAD_NEGCON_CMD"));
    return true;
  }
  if (parsed == SerialSemanticParseStatus::BadArguments) {
    out.println(F("ERR:BAD_NEGCON_ARGS"));
    return true;
  }
  if (parsed != SerialSemanticParseStatus::Ok) {
    out.println(F("ERR:BAD_INPUT_MODE"));
    return true;
  }

  SerialNegconInputsValue value{};
  const SerialSemanticStatus status = request.write
      ? serialWriteNegconInputs(request.mode, request.digital, &value)
      : serialReadNegconInputs(request.mode, &value);
  if (status == SerialSemanticStatus::UnsupportedMode) {
    out.println(F("ERR:NEGCON_UNSUPPORTED_MODE"));
    return true;
  }
  if (status == SerialSemanticStatus::WriteFailed) {
    out.println(F("ERR:NEGCON_WRITE_FAILED"));
    return true;
  }
  if (status != SerialSemanticStatus::Ok) {
    out.println(F("ERR:BAD_NEGCON_ARGS"));
    return true;
  }
  printSerialNegconInputs(out, request.mode, value);
  out.println(request.write ? F("OK:NEGCON_SET VERIFY=1")
                            : F("OK:NEGCON_GET"));
  return true;
}
#endif

#ifdef ADAPT_BRIDGE_CLASSIC_TO_USB
bool handleSerialRotaryCommand(char* text, Print& out) {
  SerialRotaryRequest request{};
  const SerialSemanticParseStatus parsed =
      parseSerialRotaryRequest(text, deviceMode, &request);
  if (parsed == SerialSemanticParseStatus::BadCommand) {
    out.println(F("ERR:BAD_ROTARY_CMD"));
    return true;
  }
  if (parsed == SerialSemanticParseStatus::BadProfile) {
    out.println(F("ERR:BAD_ROTARY_PROFILE"));
    return true;
  }
  if (parsed == SerialSemanticParseStatus::BadArguments) {
    out.println(F("ERR:BAD_ROTARY_ARGS"));
    return true;
  }
  if (parsed != SerialSemanticParseStatus::Ok) {
    out.println(F("ERR:BAD_INPUT_MODE"));
    return true;
  }

  SerialRotaryProfileValue value{};
  const SerialSemanticStatus status = request.write
      ? serialWriteRotaryProfile(
          request.mode, request.kind, request.gain, request.reverse, &value)
      : serialReadRotaryProfile(request.mode, request.kind, &value);
  if (status == SerialSemanticStatus::UnsupportedMode) {
    out.println(F("ERR:ROTARY_UNSUPPORTED_MODE"));
    return true;
  }
  if (status == SerialSemanticStatus::WriteFailed) {
    out.println(F("ERR:ROTARY_WRITE_FAILED"));
    return true;
  }
  if (status != SerialSemanticStatus::Ok) {
    out.println(F("ERR:BAD_ROTARY_ARGS"));
    return true;
  }
  printSerialRotaryProfile(out, request.mode, request.kind, value);
  out.println(request.write ? F("OK:ROTARY_SET VERIFY=1")
                            : F("OK:ROTARY_GET"));
  return true;
}
#endif

bool handleSerialSetCommandImpl(char* text, Print& out) {
  if (*text == '\0' || serialTokenEquals(text, "HELP")) {
    out.println(F("SET CMDS:SET COUNT,SET LIST [MODE],SET VISIBLE [MODE] [OUT],SET MATRIX [MODE|ALL],SET MIXED_MATRIX,SET FACTORY_RESET CONFIRM,SET GET <ID|NAME> [MODE],SET SET <ID|NAME> <VALUE> [MODE],SET DEFAULT <ID|NAME> [MODE]"));
#ifdef ENABLE_INPUT_PSX
    out.println(F("SET PSX_TIMING GET [MODE]|SET <RATE 0-5> <GAP 0-8> [MODE]"));
#endif
#ifdef ADAPT_BRIDGE_CLASSIC_TO_USB
    out.println(F("SET ROTARY GET <GENERIC|JOGCON> [MODE]|SET <GENERIC|JOGCON> <GAIN 1-23> <REVERSE 0|1> [MODE]"));
#endif
#ifdef ENABLE_INPUT_PSX
    out.println(F("SET NEGCON GET [MODE]|SET <I 0|1> <II 0|1> <L 0|1> [MODE] (1=DIGITAL)"));
#endif
    out.print(F("CAP PSX_TIMING="));
    out.print(psx_poll::storedInRecord(RZORD_PSX) ? 1 : 0);
    out.print(F(" ROTARY_PROFILE="));
    out.print(serialRotaryProfileApplies(RZORD_PSX,
        SerialRotaryProfileKind::Generic) ? 1 : 0);
    out.print(F(" NEGCON_INPUTS="));
    out.println(negconInputsStoredInRecord(RZORD_PSX) ? 1 : 0);
    return true;
  }

  if (serialTokenEquals(text, "COUNT")) {
    out.print(F("SET COUNT="));
    out.println((int)SettingId::Count);
    return true;
  }

  char* remainder = nullptr;
#ifdef ENABLE_INPUT_PSX
  if (serialCommandStartsWith(text, "PSX_TIMING", &remainder)) {
    return handleSerialPsxTimingCommand(remainder, out);
  }
#endif

#ifdef ADAPT_BRIDGE_CLASSIC_TO_USB
  if (serialCommandStartsWith(text, "ROTARY", &remainder)) {
    return handleSerialRotaryCommand(remainder, out);
  }
#endif

#ifdef ENABLE_INPUT_PSX
  if (serialCommandStartsWith(text, "NEGCON", &remainder)) {
    return handleSerialNegconCommand(remainder, out);
  }
#endif

  if (serialCommandStartsWith(text, "LIST", &remainder)) {
    DeviceEnum mode = deviceMode;
    if (!parseSettingModeOrCurrent(remainder, &mode) ||
        *serialSkipSpaces(remainder) != '\0') {
      out.println(F("ERR:BAD_INPUT_MODE"));
      return true;
    }
    for (uint8_t raw = 0; raw < (uint8_t)SettingId::Count; ++raw) {
      const SettingId id = (SettingId)raw;
      if (serialSettingSupportedOnProduct(id)) {
        printSettingLine(out, id, mode);
      }
    }
    out.println(F("OK:SET_LIST"));
    return true;
  }

  if (serialCommandStartsWith(text, "MATRIX", &remainder) ||
      serialCommandStartsWith(text, "TEST", &remainder)) {
    return handleSettingMatrixCommand(remainder, out);
  }

  if (serialCommandStartsWith(text, "MIXED_MATRIX", &remainder) ||
      serialCommandStartsWith(text, "MIXED", &remainder)) {
    return handleMixedMatrixCommand(out);
  }

  if (serialCommandStartsWith(text, "VISIBLE", &remainder) ||
      serialCommandStartsWith(text, "VIS", &remainder)) {
    return handleSettingVisibleCommand(remainder, out);
  }

  if (serialCommandStartsWith(text, "FACTORY_RESET", &remainder)) {
    if (!serialTextEqualsExact(remainder, "CONFIRM")) {
      out.println(F("ERR:CONFIRM_REQUIRED"));
      return true;
    }
    if (!factoryResetSettings()) {
      out.println(F("ERR:SET_FACTORY_RESET_PARTIAL ESP32_ERASE=0 REBOOT=0"));
      return true;
    }
    out.println(F("OK:SET_FACTORY_RESET REBOOT=1"));
    delay(100);
    reboot();
    return true;
  }

  if (serialCommandStartsWith(text, "GET", &remainder)) {
    long rawId = -1;
    if (!parseSettingIdToken(remainder, &rawId) ||
        rawId < 0 ||
        rawId >= (long)SettingId::Count) {
      out.println(F("ERR:BAD_SETTING_ID"));
      return true;
    }
    DeviceEnum mode = deviceMode;
    if (!parseSettingModeOrCurrent(remainder, &mode) ||
        *serialSkipSpaces(remainder) != '\0') {
      out.println(F("ERR:BAD_INPUT_MODE"));
      return true;
    }
    if (settingIsPerMode(static_cast<SettingId>(rawId)) &&
        !isConcreteModeForPerSettings(mode)) {
      out.println(F("ERR:SETTING_REQUIRES_CONCRETE_MODE"));
      return true;
    }
    const SettingId id = (SettingId)rawId;
    if (!serialSettingSupportedOnProduct(id)) {
      out.println(F("ERR:SETTING_UNSUPPORTED"));
      return true;
    }
    printSettingLine(out, id, mode);
    return true;
  }

  if (serialCommandStartsWith(text, "SET", &remainder)) {
    long rawId = -1;
    long rawValue = 0;
    if (!parseSettingIdToken(remainder, &rawId) ||
        rawId < 0 ||
        rawId >= (long)SettingId::Count ||
        !serialParseLongToken(remainder, &rawValue)) {
      out.println(F("ERR:BAD_SET_ARGS"));
      return true;
    }
    const SettingId id = (SettingId)rawId;
    if (!serialSettingSupportedOnProduct(id)) {
      out.println(F("ERR:SETTING_UNSUPPORTED"));
      return true;
    }
    if (id == SettingId::PersistedInputMode) {
      out.println(F("ERR:USE_INPUT_SET"));
      return true;
    }
    if (id == SettingId::ConfiguredOutputMode) {
      out.println(F("ERR:USE_OUTPUT_SET"));
      return true;
    }
    DeviceEnum mode = deviceMode;
    if (!parseSettingModeOrCurrent(remainder, &mode) ||
        *serialSkipSpaces(remainder) != '\0') {
      out.println(F("ERR:BAD_INPUT_MODE"));
      return true;
    }
    const SettingWriteResult result = settingAccessWrite(id, mode, (int32_t)rawValue);
    if (result.status == SettingAccessStatus::NeedsConcreteMode) {
      out.println(F("ERR:SETTING_REQUIRES_CONCRETE_MODE"));
      return true;
    }
    if (result.status == SettingAccessStatus::WriteFailed) {
      out.print(F("ERR:SET_WRITE_FAILED ID="));
      out.print(rawId);
      out.print(F(" EXPECT="));
      out.print(result.sanitized);
      out.print(F(" VAL="));
      out.println(result.stored);
      return true;
    }
    if (result.rebootNow) {
      out.print(F("OK:SET ID="));
      out.print(rawId);
      out.print(F(" NAME="));
      out.print(settingIdName(id));
      out.print(F(" VAL="));
      out.print(result.stored);
      out.println(F(" REBOOT=1"));
      delay(100);
      reboot();
      return true;
    }
    out.print(F("OK:SET ID="));
    out.print(rawId);
    out.print(F(" REQ="));
    out.print(rawValue);
    out.print(F(" SAN="));
    out.print(result.sanitized);
    out.print(F(" VAL="));
    out.print(result.stored);
    out.print(F(" VERIFY="));
    out.print(result.stored == result.sanitized ? 1 : 0);
    out.print(F(" LIVE="));
    if (result.runtimeKnown) {
      out.print(result.runtime == result.stored ? 1 : 0);
    } else {
      out.print(F("NA"));
    }
    if (result.rebootRequired) {
      out.print(F(" REBOOT=1"));
    }
    out.println();
    return true;
  }

  if (serialCommandStartsWith(text, "DEFAULT", &remainder) ||
      serialCommandStartsWith(text, "RESET", &remainder)) {
    long rawId = -1;
    if (!parseSettingIdToken(remainder, &rawId) ||
        rawId < 0 ||
        rawId >= (long)SettingId::Count) {
      out.println(F("ERR:BAD_SETTING_ID"));
      return true;
    }
    const SettingId id = (SettingId)rawId;
    if (!serialSettingSupportedOnProduct(id)) {
      out.println(F("ERR:SETTING_UNSUPPORTED"));
      return true;
    }
    if (id == SettingId::PersistedInputMode) {
      out.println(F("ERR:USE_INPUT_SET"));
      return true;
    }
    if (id == SettingId::ConfiguredOutputMode) {
      out.println(F("ERR:USE_OUTPUT_SET"));
      return true;
    }
    DeviceEnum mode = deviceMode;
    if (!parseSettingModeOrCurrent(remainder, &mode) ||
        *serialSkipSpaces(remainder) != '\0') {
      out.println(F("ERR:BAD_INPUT_MODE"));
      return true;
    }
    if (settingIsPerMode(id) && !isConcreteModeForPerSettings(mode)) {
      out.println(F("ERR:SETTING_REQUIRES_CONCRETE_MODE"));
      return true;
    }
    const int32_t value = defaultSettingValue(id, mode);
    const bool modeRebootRequired =
        id == SettingId::PersistedInputMode ||
        id == SettingId::ConfiguredOutputMode
#if defined(ADAPT_CAP_OUTPUT_TRANSPORT_SELECTION)
        || id == SettingId::ProductOutputTransport
        || id == SettingId::ProductWirelessOutputPersona
#endif
        ;
    const int32_t previousValue = loadSettingValue(id, mode);
    // Saving 2P Merge applies it at once, so read what USB enumerated first.
    const uint8_t previousDualMerge = classic_dual_merge_enabled;
    if (id == SettingId::PersistedInputMode) {
      persistConfiguredInputMode(static_cast<DeviceEnum>(value));
    } else {
      saveSettingValue(id, value, mode);
    }
    const int32_t stored = loadSettingValue(id, mode);
    if (stored != value) {
      out.print(F("ERR:SET_WRITE_FAILED ID="));
      out.print(rawId);
      out.print(F(" EXPECT="));
      out.print(value);
      out.print(F(" VAL="));
      out.println(stored);
      return true;
    }
    if (modeRebootRequired) {
      out.print(F("OK:SET_DEFAULT ID="));
      out.print(rawId);
      out.print(F(" NAME="));
      out.print(settingIdName(id));
      out.print(F(" VAL="));
      out.print(stored);
      out.println(F(" REBOOT=1"));
      delay(100);
      reboot();
      return true;
    }
    const bool liveNsoSpecialSetting =
      (id == SettingId::NsoSpecial) && (mode == deviceMode) &&
      output_mode_supports_switch_device_selection(get_effective_output_mode()) &&
      input_mode_supports_nso_special(mode);
    const bool liveClassicDualMergeSetting =
      (id == SettingId::ClassicDualMerge) && (mode == deviceMode);
    const bool descriptorRebootRequired =
      (liveClassicDualMergeSetting && ((uint8_t)value != previousDualMerge)) ||
      (liveNsoSpecialSetting && ((uint8_t)value != menu_nso_special));
    const bool runtimeAvailable = applyStoredSettingToRuntime(id, mode);
    const bool outputRebootNow =
        hostOutputPreferenceChangedRunningOutput(id, previousValue, stored);
    if (liveClassicDualMergeSetting) {
      if (descriptorRebootRequired) {
        menu_usb_descriptor_reboot_required = 1;
        // USB still has the old player count until the reboot.
        classic_dual_merge_enabled = previousDualMerge;
      } else {
        classic_dual_merge_enabled = menu_classic_dual_merge;
      }
    }
    int32_t runtime = 0;
    const bool runtimeReadable =
        runtimeAvailable && readRuntimeSettingValue(id, mode, &runtime);
    out.print(F("OK:SET_DEFAULT ID="));
    out.print(rawId);
    out.print(F(" NAME="));
    out.print(settingIdName(id));
    out.print(F(" VAL="));
    out.print(stored);
    out.print(F(" VERIFY="));
    out.print(stored == value ? 1 : 0);
    out.print(F(" LIVE="));
    if (runtimeReadable) {
      out.print(runtime == stored ? 1 : 0);
    } else {
      out.print(F("NA"));
    }
    if (descriptorRebootRequired || outputRebootNow) {
      out.print(F(" REBOOT=1"));
    }
    out.println();
    if (outputRebootNow) {
      delay(100);
      reboot();
    }
    return true;
  }

  out.println(F("ERR:BAD_SET_CMD"));
  return true;
}


}  // namespace

bool settingAccessSupported(SettingId id) {
  return static_cast<uint8_t>(id) < static_cast<uint8_t>(SettingId::Count) &&
         serialSettingSupportedOnProduct(id);
}

SettingWriteResult settingAccessWrite(SettingId id, DeviceEnum mode, int32_t value) {
  SettingWriteResult result{};
  if (!settingAccessSupported(id)) {
    result.status = SettingAccessStatus::Unsupported;
    return result;
  }
  if (id == SettingId::PersistedInputMode || id == SettingId::ConfiguredOutputMode) {
    result.status = SettingAccessStatus::UseModeCommand;
    return result;
  }
  if (settingIsPerMode(id) && !isConcreteModeForPerSettings(mode)) {
    result.status = SettingAccessStatus::NeedsConcreteMode;
    return result;
  }
  result.sanitized = sanitizeSettingValue(id, mode, value);
  const int32_t previousValue = loadSettingValue(id, mode);
  // Saving 2P Merge applies it at once, so read what USB enumerated first.
  const uint8_t previousDualMerge = classic_dual_merge_enabled;
  saveSettingValue(id, result.sanitized, mode);
  result.stored = loadSettingValue(id, mode);
  if (result.stored != result.sanitized) {
    result.status = SettingAccessStatus::WriteFailed;
    return result;
  }
#if defined(ADAPT_CAP_OUTPUT_TRANSPORT_SELECTION)
  if (id == SettingId::ProductOutputTransport ||
      id == SettingId::ProductWirelessOutputPersona) {
    result.rebootNow = true;
    return result;
  }
#endif
  const bool liveNsoSpecialSetting =
    (id == SettingId::NsoSpecial) && (mode == deviceMode) &&
    output_mode_supports_switch_device_selection(get_effective_output_mode()) &&
    input_mode_supports_nso_special(mode);
  const bool liveClassicDualMergeSetting =
    (id == SettingId::ClassicDualMerge) && (mode == deviceMode);
  result.rebootRequired =
    (liveClassicDualMergeSetting && ((uint8_t)result.sanitized != previousDualMerge)) ||
    (liveNsoSpecialSetting && ((uint8_t)result.sanitized != menu_nso_special));
  const bool runtimeAvailable = applyStoredSettingToRuntime(id, mode);
  if (liveClassicDualMergeSetting) {
    if (result.rebootRequired) {
      menu_usb_descriptor_reboot_required = 1;
      // USB still has the old player count until the reboot.
      classic_dual_merge_enabled = previousDualMerge;
    } else {
      classic_dual_merge_enabled = menu_classic_dual_merge;
    }
  }
  if (hostOutputPreferenceChangedRunningOutput(id, previousValue, result.stored)) {
    result.rebootRequired = true;
    result.rebootNow = true;
  }
  result.runtimeKnown =
      runtimeAvailable && readRuntimeSettingValue(id, mode, &result.runtime);
  return result;
}

bool settingAccessRuntimeValue(SettingId id, DeviceEnum mode, int32_t* value) {
  return readRuntimeSettingValue(id, mode, value);
}

void settingAccessVisible(DeviceEnum mode, outputMode_t output,
                          bool (&visible)[static_cast<uint8_t>(SettingId::Count)]) {
  memset(visible, 0, sizeof(visible));
  // Static: the probe is large and this also runs from USB callbacks.
  static QuickConfigMenu probe;
  QuickConfigItem items[QCI_COUNT * 2];
  const uint8_t itemCount = probe.debugBuildVisibleItems(
    mode, output, menu_win_output, items,
    (uint8_t)(sizeof(items) / sizeof(items[0])));
  for (uint8_t i = 0; i < itemCount; ++i) {
    SettingId ids[10];
    const uint8_t idCount = quickConfigItemSettingIds(
      items[i], mode, ids, (uint8_t)(sizeof(ids) / sizeof(ids[0])));
    for (uint8_t idIndex = 0; idIndex < idCount; ++idIndex) {
      if (settingAccessSupported(ids[idIndex])) {
        visible[static_cast<uint8_t>(ids[idIndex])] = true;
      }
    }
  }
}

bool handleSerialSetCommand(char* text, Print& out) {
  return handleSerialSetCommandImpl(text, out);
}

bool handleSerialHotkeyCommand(char* text, Print& out) {
  return handleSerialHotkeyCommandImpl(text, out);
}

bool handleSerialChordCommand(char* text, Print& out) {
  return handleSerialChordCommandImpl(text, out);
}

bool handleSerialUiCommand(char* text, Print& out) {
  if (serialTextEqualsExact(text, "RESET") ||
      serialTextEqualsExact(text, "REBOOT")) {
    out.println(F("OK:UI RESET"));
    delay(100);
    reboot();
    return true;
  }
  if (queuePlatformMenuControlAction(text)) {
    out.print(F("OK:UI "));
    out.println(text);
  } else {
    out.println(F("ERR:BAD_UI_CMD"));
  }
  return true;
}

bool handleSerialBootCommand(const char* command, Print& out) {
  if (serialTextEqualsExact(command, "RESET") ||
      serialTextEqualsExact(command, "REBOOT")) {
    out.println(F("OK:REBOOT"));
    delay(100);
    reboot();
    return true;
  }
  if (serialTextEqualsExact(command, "B") ||
      serialTextEqualsExact(command, "BOOT") ||
      serialTextEqualsExact(command, "BOOTLOADER")) {
    out.println(F("OK:BOOTLOADER"));
    delay(100);
    rebootToBootloader();
    return true;
  }
  return false;
}

bool handleSerialStateCommand(const char* command, Print& out) {
  if (serialTokenEquals(command, "STATE") ||
      serialTokenEquals(command, "FRAME") ||
      serialTokenEquals(command, "FRAMES")) {
    printState(out);
    return true;
  }
#ifdef ENABLE_INPUT_SNES
  if (serialTokenEquals(command, "SNESDBG") ||
      serialTokenEquals(command, "SNESDEBUG")) {
    resetSnesPadDebugCounters();
    out.println(F("OK:SNESDEBUG_RESET"));
    return true;
  }
  if (serialTokenEquals(command, "SNESMOUSE")) {
    resetSnesMouseDebugRange();
    out.println(F("OK:SNESMOUSE_RESET"));
    return true;
  }
  if (serialTokenEquals(command, "FCNET")) {
    printFamicomNetworkRawDump(out);
    return true;
  }
#endif
#ifdef ENABLE_INPUT_SATURN
  if (serialTokenEquals(command, "SATMOUSE")) {
    resetSaturnMouseDebugRange();
    out.println(F("OK:SATMOUSE_RESET"));
    return true;
  }
#endif
  return false;
}

bool handleSerialGpioCommand(const char* command, Print& out) {
  if (serialTokenEquals(command, "GPIO") || serialTokenEquals(command, "PINS")) {
    printGpioSnapshot(out);
    return true;
  }
  return false;
}

bool handleSerialDreamcastCommand(const char* command, Print& out) {
#ifdef ENABLE_INPUT_DREAMCAST
  if (serialTokenEquals(command, "DCSTAT") ||
      serialTokenEquals(command, "DREAMCAST") ||
      serialTokenEquals(command, "DREAMCAST STATUS")) {
    RZInputDreamcast* dreamcast = currentDreamcastInputModule();
    if (dreamcast == nullptr) {
      out.println(F("ERR:DCSTAT_NOT_DREAMCAST"));
    } else {
      dreamcast->printDiagnostics(out);
    }
    return true;
  }
#endif
  return false;
}
