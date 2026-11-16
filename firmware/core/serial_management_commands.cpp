#include "../product_config.h"
#include "../firmware_build_info.h"

#include "serial_management_commands.h"

#include <Arduino.h>
#include <string.h>
#include <stdlib.h>

#include "button_remap.h"
#include "remap_pairs.h"
#include "controller_frame_state.h"
#include "controller_selection_state.h"
#include "device_runtime_state.h"
#include "firmware_support.h"
#include "hid_mapping_profiles.h"
#include "../input/usb_host/input_usb/sdl_gamecontrollerdb.h"
#include "mapping_export_store.h"
#include "serial_command_parser.h"
#include "serial_core_commands.h"
#include "settings_store.h"
#include "deferred_mode_saves.h"
#include "turbo.h"
#include "../firmware_platform_config.h"
#include "../menu/menu_input_mode.h"
#include "../menu/menu_mode_labels.h"
#include "../menu/menu_mode_state.h"
#include "../output/auth/auth_storage.h"
#ifdef ADAPT_OUTPUT_USB_DEVICE
#include "../output/auth/webhid_auth_runtime.h"
#endif
#include "../output/autodetect/output_autodetect_diagnostics.h"
#include "../output/output_runtime_state.h"
#include "../platform/runtime/platform_menu_runtime.h"
#include "../platform/webhid_runtime.h"
#include "../features/output_trial/output_trial.h"

#if ADAPT_POLICY_CONTROLLER_FEEDBACK_COMMANDS
#include "controller_feedback_config.h"
#endif

#if defined(ADAPT_CAP_MULTI_SOURCE_CONTROLLER_INPUT) && defined(ENABLE_INPUT_USB_BT)
#include "../input/usb_bt_composite/Input_UsbBt.h"
#include "../input/usb_host/input_usb/input_usb.h"
#ifdef ENABLE_HID_COMPAT_CAPTURE
#include "../features/dinput_mapping/dinput_mapping_wizard.h"
#endif
#endif

#if defined(ADAPT_CAP_ESP32_CONTROLLER_BRIDGE) && defined(ENABLE_INPUT_ESP32_SPI)
#include "../input/esp32_spi/Input_Esp32Spi.h"
#include "../input/esp32_spi/esp32_spi_protocol.h"
#include "../output/bluetooth/modern2usb_bt_output.h"
#endif

namespace {

char upperAscii(char value) {
  return value >= 'a' && value <= 'z'
      ? static_cast<char>(value - ('a' - 'A'))
      : value;
}

bool ignoredNamePunctuation(char value) {
  return value == ' ' || value == '\t' || value == '_' || value == '-' ||
         value == '+' || value == '/' || value == '.';
}

bool normalizedNameEquals(const char* left, const char* right) {
  if (left == nullptr || right == nullptr) return false;
  while (true) {
    while (ignoredNamePunctuation(*left)) ++left;
    while (ignoredNamePunctuation(*right)) ++right;
    if (*left == '\0' || *right == '\0') {
      while (ignoredNamePunctuation(*left)) ++left;
      while (ignoredNamePunctuation(*right)) ++right;
      return *left == '\0' && *right == '\0';
    }
    if (upperAscii(*left) != upperAscii(*right)) return false;
    ++left;
    ++right;
  }
}

bool readToken(char*& text, char* token, size_t tokenSize) {
  text = serialSkipSpaces(text);
  if (*text == '\0' || token == nullptr || tokenSize == 0) return false;
  size_t length = 0;
  while (*text != '\0' && *text != ' ' && *text != '\t') {
    if (length + 1 < tokenSize) token[length++] = *text;
    ++text;
  }
  token[length] = '\0';
  text = serialSkipSpaces(text);
  return length != 0;
}

bool parseStrictLong(char*& text, long* value) {
  if (!serialParseLongToken(text, value)) return false;
  return true;
}

bool parseSingleLong(char* text, long* value) {
  return parseStrictLong(text, value) && *serialSkipSpaces(text) == '\0';
}

bool verifyManagementSettingWrite(
    SettingId id, int32_t expected, Print& out) {
  const int32_t stored = loadSettingValue(id, RZORD_NONE);
  if (stored == expected) return true;
  out.print(F("ERR:SETTING_WRITE_FAILED ID="));
  out.print((int)id);
  out.print(F(" EXPECT="));
  out.print(expected);
  out.print(F(" VAL="));
  out.println(stored);
  return false;
}

bool parseInputMode(const char* text, DeviceEnum* mode) {
  if (text == nullptr || mode == nullptr) return false;
  char* mutableText = const_cast<char*>(text);
  long numeric = -1;
  if (parseSingleLong(mutableText, &numeric)) {
    if (numeric > RZORD_NONE && numeric < RZORD_LAST &&
        !should_hide_input_mode(static_cast<DeviceEnum>(numeric))) {
      *mode = static_cast<DeviceEnum>(numeric);
      return true;
    }
    return false;
  }

  for (uint8_t raw = 1; raw < static_cast<uint8_t>(RZORD_LAST); ++raw) {
    const DeviceEnum candidate = static_cast<DeviceEnum>(raw);
    if (!should_hide_input_mode(candidate) &&
        normalizedNameEquals(text, getInputModeName(candidate))) {
      *mode = candidate;
      return true;
    }
  }
  return false;
}

bool parseOutputMode(const char* text, outputMode_t* mode) {
  if (text == nullptr || mode == nullptr) return false;
  char* mutableText = const_cast<char*>(text);
  long numeric = -1;
  if (parseSingleLong(mutableText, &numeric)) {
    if (numeric >= 0 && numeric < OUTPUT_LAST &&
        !should_hide_output_mode(static_cast<outputMode_t>(numeric))) {
      *mode = canonicalizeOutputMode(static_cast<outputMode_t>(numeric));
      return true;
    }
    return false;
  }

  for (uint8_t raw = 0; raw < static_cast<uint8_t>(OUTPUT_LAST); ++raw) {
    const outputMode_t candidate = static_cast<outputMode_t>(raw);
    if (!should_hide_output_mode(candidate) &&
        normalizedNameEquals(text, getOutputShortName(candidate))) {
      *mode = canonicalizeOutputMode(candidate);
      return true;
    }
  }

  struct Alias {
    const char* name;
    outputMode_t mode;
  };
  static const Alias aliases[] = {
      {"HID", OUTPUT_HID}, {"DINPUT", OUTPUT_HID},
      {"MISTER", OUTPUT_MISTER}, {"LINUX", OUTPUT_MISTER},
      {"XINPUT", OUTPUT_XINPUT2P}, {"XINPUTPC", OUTPUT_XINPUT2P},
      {"XINPUT4P", OUTPUT_XINPUT2P}, {"XBOX360", OUTPUT_XINPUT},
      {"X360", OUTPUT_XINPUT}, {"XID", OUTPUT_XID},
      {"XBOXOG", OUTPUT_XID}, {"XBOXONE", OUTPUT_XBOXONE},
      {"XBONE", OUTPUT_XBOXONE}, {"SWITCH", OUTPUT_SWITCHPRO},
      {"SWITCHPRO", OUTPUT_SWITCHPRO}, {"GC", OUTPUT_GCWIIU},
      {"GCWIIU", OUTPUT_GCWIIU}, {"KBD", OUTPUT_KEYBOARD},
  };
  for (const Alias& alias : aliases) {
    if (normalizedNameEquals(text, alias.name) &&
        alias.mode < OUTPUT_LAST && !should_hide_output_mode(alias.mode)) {
      *mode = canonicalizeOutputMode(alias.mode);
      return true;
    }
  }
  return false;
}

void printInputStatus(Print& out) {
  out.print(F("INPUT CURRENT="));
  out.print((int)deviceMode);
  out.print(F(" NAME=\""));
  out.print(getInputModeName(deviceMode));
  out.print(F("\" SAVED="));
  out.print((int)savedDeviceMode);
  out.print(F(" SAVED_NAME=\""));
  out.print(getInputModeName(savedDeviceMode));
  out.println('"');
}

void printInputList(Print& out) {
  for (uint8_t raw = 1; raw < static_cast<uint8_t>(RZORD_LAST); ++raw) {
    const DeviceEnum mode = static_cast<DeviceEnum>(raw);
    if (should_hide_input_mode(mode)) continue;
    out.print(F("INPUTMODE ID="));
    out.print((int)raw);
    out.print(F(" NAME=\""));
    out.print(getInputModeName(mode));
    out.println('"');
  }
  out.println(F("OK:INPUT_LIST"));
}

bool handleInputCommand(char* text, Print& out) {
  text = serialSkipSpaces(text);
  if (*text == '\0' || serialTextEqualsExact(text, "STATUS") ||
      serialTextEqualsExact(text, "GET")) {
    printInputStatus(out);
    return true;
  }
  if (serialTextEqualsExact(text, "LIST")) {
    printInputList(out);
    return true;
  }
  char* modeText = text;
  char* remainder = nullptr;
  if (serialCommandStartsWith(text, "SET", &remainder)) modeText = remainder;

  DeviceEnum mode = RZORD_NONE;
  if (!parseInputMode(modeText, &mode)) {
    out.println(F("ERR:BAD_INPUT_MODE"));
    return true;
  }
  persistConfiguredInputMode(mode);
  if (!verifyManagementSettingWrite(
          SettingId::PersistedInputMode, mode, out)) {
    return true;
  }
  out.print(F("OK:INPUT="));
  out.print((int)mode);
  out.print(F(" NAME=\""));
  out.print(getInputModeName(mode));
  out.println(F("\" REBOOT=1"));
  delay(100);
  reboot();
  return true;
}

void printOutputStatus(Print& out) {
  out.print(F("OUTPUT CONFIGURED="));
  out.print((int)configuredOutputMode);
  out.print(F(" NAME=\""));
  out.print(getOutputShortName(configuredOutputMode));
  out.print(F("\" RUNTIME="));
  out.print((int)outputMode);
  out.print(F(" RUNTIME_NAME=\""));
  out.print(getOutputShortName(outputMode));
  out.print(F("\""));
  out.print(' ');
  appendOutputAutodetectDiagnostics(out);
  out.println();
}

void printOutputList(Print& out) {
  for (uint8_t raw = 0; raw < static_cast<uint8_t>(OUTPUT_LAST); ++raw) {
    const outputMode_t mode = static_cast<outputMode_t>(raw);
    if (should_hide_output_mode(mode) ||
        canonicalizeOutputMode(mode) != mode) {
      continue;
    }
    out.print(F("OUTPUTMODE ID="));
    out.print((int)raw);
    out.print(F(" NAME=\""));
    out.print(getOutputShortName(mode));
    out.println('"');
  }
  out.println(F("OK:OUTPUT_LIST"));
}

bool handleOutputCommand(char* text, Print& out) {
  text = serialSkipSpaces(text);
  if (*text == '\0' || serialTextEqualsExact(text, "STATUS") ||
      serialTextEqualsExact(text, "GET")) {
    printOutputStatus(out);
    return true;
  }
  if (serialTextEqualsExact(text, "LIST")) {
    printOutputList(out);
    return true;
  }
  char* modeText = text;
  char* remainder = nullptr;
  if (serialCommandStartsWith(text, "SET", &remainder)) modeText = remainder;

  outputMode_t mode = OUTPUT_MISTER;
  if (!parseOutputMode(modeText, &mode)) {
    out.println(F("ERR:BAD_OUTPUT_MODE"));
    return true;
  }
#if defined(ADAPT_FEATURE_OUTPUT_TRIAL)
  // A saved choice ends a trial, so the trial guard cannot override it.
  outputTrialEnd();
#endif
  saveSettingValue(SettingId::ConfiguredOutputMode, mode, RZORD_NONE);
  if (!verifyManagementSettingWrite(
          SettingId::ConfiguredOutputMode, mode, out)) {
    return true;
  }
  if (mode == OUTPUT_AUTO) {
    autoDetectState = AUTO_STATE_IDLE;
    auto_detect_clear_scratch_state();
  }
  out.print(F("OK:OUTPUT="));
  out.print((int)mode);
  out.print(F(" NAME=\""));
  out.print(getOutputShortName(mode));
  out.println(F("\" REBOOT=1"));
  delay(100);
  reboot();
  return true;
}

bool parsePlayerNumber(char*& text, uint8_t* player) {
  long number = 0;
  if (!parseStrictLong(text, &number) || number < 1 || number > MAX_USB_OUT) {
    return false;
  }
  *player = static_cast<uint8_t>(number - 1);
  return true;
}

void printPlayerLine(Print& out, uint8_t player) {
  const controller_state_t& frame = controllerFrameConst(player);
  out.print(F("PLAYER N="));
  out.print((int)player + 1);
  out.print(F(" SELECTED="));
  out.print(selectedControllerPlayer() == player ? 1 : 0);
  out.print(F(" CONNECTED="));
  out.print(frame.connected ? 1 : 0);
#if defined(ADAPT_CAP_MULTI_SOURCE_CONTROLLER_INPUT) && defined(ENABLE_INPUT_USB_BT)
  UsbBtPlayerInfo info{};
  usbBtGetPlayerInfo(player, &info);
  out.print(F(" SOURCE="));
  out.print(info.source == USB_BT_INPUT_USB ? F("USB") :
            (info.source == USB_BT_INPUT_BLUETOOTH ? F("BT") : F("NONE")));
  out.print(F(" SLOT="));
  if (info.source == USB_BT_INPUT_NONE) out.print('-');
  else out.print((int)info.source_slot);
  out.print(F(" VID="));
  out.print(info.vid, HEX);
  out.print(F(" PID="));
  out.print(info.pid, HEX);
  out.print(F(" BAT="));
  out.print((int)info.battery_percent);
#endif
  out.print(F(" TYPE=\""));
  out.print(frame.controller_type_name);
  out.println('"');
}

bool handlePlayerCommand(char* text, Print& out) {
  text = serialSkipSpaces(text);
  if (*text == '\0' || serialTextEqualsExact(text, "STATUS") ||
      serialTextEqualsExact(text, "LIST")) {
    for (uint8_t player = 0; player < MAX_USB_OUT; ++player) {
      printPlayerLine(out, player);
    }
    out.println(F("OK:PLAYER_LIST"));
    return true;
  }

  char* remainder = nullptr;
  if (serialCommandStartsWith(text, "SELECT", &remainder)) {
    uint8_t player = 0;
    if (!parsePlayerNumber(remainder, &player) ||
        *serialSkipSpaces(remainder) != '\0') {
      out.println(F("ERR:BAD_PLAYER"));
      return true;
    }
    setSelectedControllerPlayer(player);
    out.print(F("OK:PLAYER_SELECTED="));
    out.println((int)player + 1);
    return true;
  }

#if ADAPT_POLICY_CONTROLLER_FEEDBACK_COMMANDS
  if (serialCommandStartsWith(text, "IDENTIFY", &remainder)) {
    uint8_t player = selectedControllerPlayer();
    long duration = 2000;
    if (*remainder != '\0' && !parsePlayerNumber(remainder, &player)) {
      out.println(F("ERR:BAD_PLAYER"));
      return true;
    }
    if (*remainder != '\0' &&
        (!parseStrictLong(remainder, &duration) ||
         duration < 100 || duration > 30000)) {
      out.println(F("ERR:BAD_IDENTIFY_DURATION"));
      return true;
    }
    if (*serialSkipSpaces(remainder) != '\0') {
      out.println(F("ERR:BAD_IDENTIFY_ARGS"));
      return true;
    }
    identifyControllerPlayer(player, static_cast<uint16_t>(duration));
    out.print(F("OK:PLAYER_IDENTIFY="));
    out.print((int)player + 1);
    out.print(F(" MS="));
    out.println(duration);
    return true;
  }
#endif

  uint8_t player = 0;
  if (parsePlayerNumber(text, &player) && *serialSkipSpaces(text) == '\0') {
    setSelectedControllerPlayer(player);
    out.print(F("OK:PLAYER_SELECTED="));
    out.println((int)player + 1);
    return true;
  }
  out.println(F("ERR:BAD_PLAYER_CMD"));
  return true;
}

#if defined(ADAPT_CAP_ESP32_CONTROLLER_BRIDGE) && defined(ENABLE_INPUT_ESP32_SPI)
bool parseTransport(const char* text, uint8_t* value) {
  if (normalizedNameEquals(text, "AUTO")) {
    *value = MODERN2USB_OUTPUT_TRANSPORT_AUTO;
  } else if (normalizedNameEquals(text, "USB")) {
    *value = MODERN2USB_OUTPUT_TRANSPORT_USB;
  } else if (normalizedNameEquals(text, "BLUETOOTH") ||
             normalizedNameEquals(text, "BT")) {
    *value = MODERN2USB_OUTPUT_TRANSPORT_BLUETOOTH;
  } else {
    return false;
  }
  return true;
}

bool parsePersona(const char* text, uint8_t* value) {
  if (normalizedNameEquals(text, "REFLEX")) {
    *value = ESP32_SPI_BT_OUTPUT_PERSONA_REFLEX;
  } else if (normalizedNameEquals(text, "GENERIC") ||
             normalizedNameEquals(text, "GENERICHID") ||
             normalizedNameEquals(text, "HID")) {
    *value = ESP32_SPI_BT_OUTPUT_PERSONA_GENERIC_HID;
  } else {
    return false;
  }
  return true;
}

void printTransportStatus(Print& out) {
  out.print(F("TRANSPORT PREF="));
  out.print((int)modern2usbBtOutputPreference());
  out.print(F(" NAME="));
  out.print(modern2usbBtOutputPreferenceName(modern2usbBtOutputPreference()));
  out.print(F(" PERSONA="));
  out.print((int)modern2usbBtOutputPersona());
  out.print(F(" PERSONA_NAME="));
  out.print(modern2usbBtOutputPersonaName(modern2usbBtOutputPersona()));
  out.print(F(" ACTIVE="));
  out.println(modern2usbBtOutputActiveName());
}

bool handleTransportCommand(char* text, Print& out) {
  text = serialSkipSpaces(text);
  if (*text == '\0' || serialTextEqualsExact(text, "STATUS") ||
      serialTextEqualsExact(text, "GET")) {
    printTransportStatus(out);
    return true;
  }

  char* remainder = nullptr;
  if (serialCommandStartsWith(text, "PERSONA", &remainder)) {
    uint8_t persona = 0;
    if (!parsePersona(remainder, &persona)) {
      out.println(F("ERR:BAD_BT_PERSONA"));
      return true;
    }
    saveSettingValue(SettingId::ProductWirelessOutputPersona, persona, RZORD_NONE);
    if (!verifyManagementSettingWrite(
            SettingId::ProductWirelessOutputPersona, persona, out)) {
      return true;
    }
    out.print(F("OK:TRANSPORT_PERSONA="));
    out.print(modern2usbBtOutputPersonaName(persona));
    out.println(F(" REBOOT=1"));
    delay(100);
    reboot();
    return true;
  }

  if (serialCommandStartsWith(text, "SET", &remainder)) text = remainder;
  char transportToken[20] = {};
  if (!readToken(text, transportToken, sizeof(transportToken))) {
    out.println(F("ERR:BAD_TRANSPORT"));
    return true;
  }
  uint8_t transport = 0;
  if (!parseTransport(transportToken, &transport)) {
    out.println(F("ERR:BAD_TRANSPORT"));
    return true;
  }
  uint8_t persona = modern2usbBtOutputPersona();
  if (*text != '\0') {
    char personaToken[20] = {};
    if (!readToken(text, personaToken, sizeof(personaToken)) ||
        *text != '\0' || !parsePersona(personaToken, &persona)) {
      out.println(F("ERR:BAD_BT_PERSONA"));
      return true;
    }
  }
  saveSettingValue(SettingId::ProductOutputTransport, transport, RZORD_NONE);
  saveSettingValue(SettingId::ProductWirelessOutputPersona, persona, RZORD_NONE);
  if (!verifyManagementSettingWrite(
          SettingId::ProductOutputTransport, transport, out) ||
      !verifyManagementSettingWrite(
          SettingId::ProductWirelessOutputPersona, persona, out)) {
    return true;
  }
  out.print(F("OK:TRANSPORT="));
  out.print(modern2usbBtOutputPreferenceName(transport));
  out.print(F(" PERSONA="));
  out.print(modern2usbBtOutputPersonaName(persona));
  out.println(F(" REBOOT=1"));
  delay(100);
  reboot();
  return true;
}
#endif

#if ADAPT_POLICY_CONTROLLER_FEEDBACK_COMMANDS
const char* controllerLedModeName(ControllerLedMode mode) {
  switch (mode) {
    case CONTROLLER_LED_OFF: return "Off";
    case CONTROLLER_LED_PLAYER: return "Player";
    case CONTROLLER_LED_CUSTOM: return "Custom";
  }
  return "Unknown";
}

void printControllerLedStatus(Print& out) {
  const ControllerFeedbackColor color = controllerLedCustomColor();
  out.print(F("CONTROLLERLED MODE="));
  out.print((int)controllerLedMode());
  out.print(F(" NAME="));
  out.print(controllerLedModeName(controllerLedMode()));
  out.print(F(" BRIGHT="));
  out.print((int)controllerLedBrightness());
  out.print(F(" RGB="));
  out.print((int)color.r);
  out.print(',');
  out.print((int)color.g);
  out.print(',');
  out.println((int)color.b);
}

bool handleControllerLedCommand(char* text, Print& out) {
  text = serialSkipSpaces(text);
  if (*text == '\0' || serialTextEqualsExact(text, "STATUS") ||
      serialTextEqualsExact(text, "GET")) {
    printControllerLedStatus(out);
    return true;
  }
  char* remainder = nullptr;
  if (serialCommandStartsWith(text, "MODE", &remainder)) {
    ControllerLedMode mode = CONTROLLER_LED_PLAYER;
    if (normalizedNameEquals(remainder, "OFF") ||
        normalizedNameEquals(remainder, "0")) {
      mode = CONTROLLER_LED_OFF;
    } else if (normalizedNameEquals(remainder, "PLAYER") ||
               normalizedNameEquals(remainder, "1")) {
      mode = CONTROLLER_LED_PLAYER;
    } else if (normalizedNameEquals(remainder, "CUSTOM") ||
               normalizedNameEquals(remainder, "2")) {
      mode = CONTROLLER_LED_CUSTOM;
    } else {
      out.println(F("ERR:BAD_CONTROLLER_LED_MODE"));
      return true;
    }
    setControllerLedMode(mode);
    controllerFeedbackConfigSave();
    printControllerLedStatus(out);
    return true;
  }
  if (serialCommandStartsWith(text, "BRIGHTNESS", &remainder) ||
      serialCommandStartsWith(text, "BRIGHT", &remainder)) {
    long value = -1;
    if (!parseSingleLong(remainder, &value) || value < 0 || value > 2) {
      out.println(F("ERR:BAD_CONTROLLER_LED_BRIGHTNESS"));
      return true;
    }
    setControllerLedBrightness(static_cast<uint8_t>(value));
    controllerFeedbackConfigSave();
    printControllerLedStatus(out);
    return true;
  }
  if (serialCommandStartsWith(text, "COLOR", &remainder) ||
      serialCommandStartsWith(text, "RGB", &remainder)) {
    long red = -1;
    long green = -1;
    long blue = -1;
    if (!parseStrictLong(remainder, &red) ||
        !parseStrictLong(remainder, &green) ||
        !parseStrictLong(remainder, &blue) ||
        *serialSkipSpaces(remainder) != '\0' ||
        red < 0 || red > 255 || green < 0 || green > 255 ||
        blue < 0 || blue > 255) {
      out.println(F("ERR:BAD_CONTROLLER_LED_COLOR"));
      return true;
    }
    setControllerLedCustomColor({
        static_cast<uint8_t>(red),
        static_cast<uint8_t>(green),
        static_cast<uint8_t>(blue)});
    controllerFeedbackConfigSave();
    printControllerLedStatus(out);
    return true;
  }
  out.println(F("ERR:BAD_CONTROLLER_LED_CMD"));
  return true;
}
#endif

bool parseTurboRate(const char* text, TurboRate* rate) {
  if (text == nullptr || rate == nullptr) return false;
  char* mutableText = const_cast<char*>(text);
  long numeric = -1;
  if (parseSingleLong(mutableText, &numeric)) {
    if (numeric >= 0 && numeric < TURBO_RATE_LAST) {
      *rate = static_cast<TurboRate>(numeric);
      return true;
    }
    return false;
  }
  static const char* const names[TURBO_RATE_LAST] = {
      "OFF", "SLOW", "MEDIUM", "FAST", "MAX", "ULTRA",
      "SLOW_LATCHED", "MEDIUM_LATCHED", "FAST_LATCHED",
      "MAX_LATCHED", "ULTRA_LATCHED"};
  for (uint8_t raw = 0; raw < TURBO_RATE_LAST; ++raw) {
    if (normalizedNameEquals(text, names[raw])) {
      *rate = static_cast<TurboRate>(raw);
      return true;
    }
  }
  return false;
}

bool parseTurboButton(DeviceEnum mode, const char* text, uint8_t* button) {
  if (text == nullptr || button == nullptr) return false;
  char* mutableText = const_cast<char*>(text);
  long numeric = -1;
  if (parseSingleLong(mutableText, &numeric)) {
    if (numeric >= 0 && numeric < TURBO_BTN_COUNT) {
      *button = static_cast<uint8_t>(numeric);
      return true;
    }
    return false;
  }
  const TurboButtonConfig& config =
      getTurboButtonConfig(getTurboInputModeForDeviceMode(mode));
  for (uint8_t index = 0; index < config.count; ++index) {
    if (normalizedNameEquals(text, config.names[index])) {
      *button = config.indices[index];
      return true;
    }
  }
  return false;
}

void printTurboList(Print& out) {
  PerModeSettingsRecord settings{};
  readPerModeSettings(deviceMode, settings);
  const TurboButtonConfig& config =
      getTurboButtonConfig(getTurboInputModeForDeviceMode(deviceMode));
  for (uint8_t index = 0; index < config.count; ++index) {
    const uint8_t button = config.indices[index];
    out.print(F("TURBO BUTTON="));
    out.print((int)button);
    out.print(F(" NAME="));
    out.print(config.names[index]);
    out.print(F(" RATE="));
    out.print((int)settings.turbo_rates[button]);
    out.print(F(" RATE_NAME=\""));
    out.print(getTurboRateFullName(
        static_cast<TurboRate>(settings.turbo_rates[button])));
    out.println('"');
  }
  out.println(F("OK:TURBO_LIST"));
}

bool handleTurboCommand(char* text, Print& out) {
  text = serialSkipSpaces(text);
  if (*text == '\0' || serialTextEqualsExact(text, "STATUS") ||
      serialTextEqualsExact(text, "LIST")) {
    printTurboList(out);
    return true;
  }
  char* clearRemainder = nullptr;
  if (serialCommandStartsWith(text, "CLEAR", &clearRemainder) ||
      serialCommandStartsWith(text, "DEFAULT", &clearRemainder)) {
    if (!serialTextEqualsExact(clearRemainder, "CONFIRM")) {
      out.println(F("ERR:CONFIRM_REQUIRED"));
      return true;
    }
    PerModeSettingsRecord settings{};
    readPerModeSettings(deviceMode, settings);
    memset(settings.turbo_rates, TURBO_OFF, sizeof(settings.turbo_rates));
    writePerModeSettings(deviceMode, settings);
    turbo.setAllRates(settings.turbo_rates);
    out.println(F("OK:TURBO_CLEAR"));
    return true;
  }
  char* remainder = nullptr;
  if (!serialCommandStartsWith(text, "SET", &remainder)) {
    out.println(F("ERR:BAD_TURBO_CMD"));
    return true;
  }
  char buttonToken[24] = {};
  char rateToken[24] = {};
  if (!readToken(remainder, buttonToken, sizeof(buttonToken)) ||
      !readToken(remainder, rateToken, sizeof(rateToken)) ||
      *remainder != '\0') {
    out.println(F("ERR:BAD_TURBO_ARGS"));
    return true;
  }
  uint8_t button = 0;
  TurboRate rate = TURBO_OFF;
  if (!parseTurboButton(deviceMode, buttonToken, &button) ||
      !parseTurboRate(rateToken, &rate)) {
    out.println(F("ERR:BAD_TURBO_ARGS"));
    return true;
  }
  turbo.setButtonRate(static_cast<TurboButton>(button), rate);
  requestDeferredTurboSave();
  out.print(F("OK:TURBO_SET BUTTON="));
  out.print((int)button);
  out.print(F(" RATE="));
  out.println((int)rate);
  return true;
}

bool parseRemapButton(DeviceEnum mode, const char* text, uint8_t* slot) {
  if (text == nullptr || slot == nullptr) return false;
  char* mutableText = const_cast<char*>(text);
  long numeric = -1;
  if (parseSingleLong(mutableText, &numeric)) {
    if (numeric >= 0 && numeric < REMAP_MAX_BUTTONS) {
      *slot = static_cast<uint8_t>(numeric);
      return true;
    }
    return false;
  }
  const uint8_t count = getRemapButtonCount(mode);
  for (uint8_t display = 0; display < count; ++display) {
    if (normalizedNameEquals(text, getRemapButtonName(mode, display))) {
      *slot = getRemapButtonSlot(mode, display);
      return *slot < REMAP_MAX_BUTTONS;
    }
  }
  struct Alias {
    const char* name;
    uint8_t slot;
  };
  static const Alias aliases[] = {
      {"START", 10}, {"SELECT", 11}, {"HOME", 12},
      {"CAPTURE", 13}, {"SHARE", 13},
  };
  for (const Alias& alias : aliases) {
    if (normalizedNameEquals(text, alias.name)) {
      *slot = alias.slot;
      return true;
    }
  }
  return false;
}

void printRemapList(Print& out) {
  // Remaps belong to the current input+output pair.
  uint8_t remaps[REMAP_MAX_BUTTONS];
  readRemapsForPair(deviceMode, configuredOutputMode, remaps);
  out.print(F("REMAP OUTPUT="));
  out.println(getOutputShortName(remapPairOutputFor(configuredOutputMode)));
  const uint8_t count = getRemapButtonCount(deviceMode);
  for (uint8_t display = 0; display < count; ++display) {
    const uint8_t source = getRemapButtonSlot(deviceMode, display);
    const uint8_t destination = remaps[source];
    out.print(F("REMAP SRC="));
    out.print((int)source);
    out.print(F(" SRC_NAME="));
    out.print(getRemapButtonNameForSlot(deviceMode, source));
    out.print(F(" DST="));
    out.print((int)destination);
    out.print(F(" DST_NAME="));
    out.println(getRemapButtonNameForSlot(deviceMode, destination));
  }
  out.println(F("OK:REMAP_LIST"));
}

bool handleRemapCommand(char* text, Print& out) {
  text = serialSkipSpaces(text);
  if (*text == '\0' || serialTextEqualsExact(text, "STATUS") ||
      serialTextEqualsExact(text, "LIST")) {
    printRemapList(out);
    return true;
  }
  char* clearRemainder = nullptr;
  if (serialCommandStartsWith(text, "CLEAR", &clearRemainder) ||
      serialCommandStartsWith(text, "DEFAULT", &clearRemainder)) {
    if (!serialTextEqualsExact(clearRemainder, "CONFIRM")) {
      out.println(F("ERR:CONFIRM_REQUIRED"));
      return true;
    }
    for (uint8_t index = 0; index < REMAP_MAX_BUTTONS; ++index) {
      active_remaps[index] = index;
    }
    (void)writeRemapsForPair(deviceMode, configuredOutputMode, active_remaps);
    out.println(F("OK:REMAP_CLEAR"));
    return true;
  }
  char* remainder = nullptr;
  if (!serialCommandStartsWith(text, "SET", &remainder)) {
    out.println(F("ERR:BAD_REMAP_CMD"));
    return true;
  }
  char sourceToken[24] = {};
  char destinationToken[24] = {};
  if (!readToken(remainder, sourceToken, sizeof(sourceToken)) ||
      !readToken(remainder, destinationToken, sizeof(destinationToken)) ||
      *remainder != '\0') {
    out.println(F("ERR:BAD_REMAP_ARGS"));
    return true;
  }
  uint8_t source = 0;
  uint8_t destination = 0;
  if (!parseRemapButton(deviceMode, sourceToken, &source) ||
      !parseRemapButton(deviceMode, destinationToken, &destination) ||
      !remapValueAllowed(deviceMode, source, destination)) {
    out.println(F("ERR:BAD_REMAP_ARGS"));
    return true;
  }
  uint8_t remaps[REMAP_MAX_BUTTONS];
  memcpy(remaps, active_remaps, REMAP_MAX_BUTTONS);
  remaps[source] = destination;
  if (!remapsFitForPair(deviceMode, configuredOutputMode, remaps)) {
    out.println(F("ERR:REMAP_STORE_FULL"));
    return true;
  }
  active_remaps[source] = destination;
  requestDeferredRemapSave();
  out.print(F("OK:REMAP_SET SRC="));
  out.print((int)source);
  out.print(F(" DST="));
  out.println((int)destination);
  return true;
}

#if defined(ENABLE_INPUT_USB)
void printProfileKey(Print& out, const HidMappingProfile& profile,
                     uint8_t slot) {
  out.print(F("PROFILE SLOT="));
  out.print((int)slot + 1);
  out.print(F(" VALID="));
  out.print((profile.flags & HID_MAPPING_PROFILE_VALID) ? 1 : 0);
  out.print(F(" VID="));
  out.print(profile.key.vid, HEX);
  out.print(F(" PID="));
  out.print(profile.key.pid, HEX);
  out.print(F(" BCD="));
  out.print(profile.key.bcd_device, HEX);
  out.print(F(" TRANSPORT="));
  out.print((int)profile.key.transport);
  out.print(F(" REPORT="));
  out.print((int)profile.key.report_id);
  out.print(F(" CRC="));
  out.print(profile.key.descriptor_crc32, HEX);
  out.print(F(" FLAGS="));
  out.println((int)profile.flags);
}

int8_t hexNibble(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  value = upperAscii(value);
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  return -1;
}

bool decodeProfileHex(const char* text, HidMappingProfile* profile) {
  if (text == nullptr || profile == nullptr) return false;
  uint8_t* bytes = reinterpret_cast<uint8_t*>(profile);
  size_t byteIndex = 0;
  int8_t high = -1;
  while (*text != '\0') {
    if (*text == ' ' || *text == '\t' || *text == ':' || *text == '-') {
      ++text;
      continue;
    }
    const int8_t nibble = hexNibble(*text++);
    if (nibble < 0) return false;
    if (high < 0) {
      high = nibble;
    } else {
      if (byteIndex >= sizeof(HidMappingProfile)) return false;
      bytes[byteIndex++] =
          static_cast<uint8_t>((static_cast<uint8_t>(high) << 4) | nibble);
      high = -1;
    }
  }
  return high < 0 && byteIndex == sizeof(HidMappingProfile);
}

bool profilePayloadValid(const HidMappingProfile& profile) {
  if (profile.key.transport < HID_MAPPING_TRANSPORT_USB ||
      profile.key.transport > HID_MAPPING_TRANSPORT_BLUETOOTH_LE) {
    return false;
  }
  for (uint8_t source : profile.button_source) {
    if (source != HID_MAPPING_SOURCE_NONE && source >= 32) return false;
  }
  for (uint8_t source : profile.axis_source) {
    if (source != HID_MAPPING_SOURCE_NONE && source >= 12) return false;
  }
  const uint8_t allowedProfileFlags =
      HID_MAPPING_PROFILE_VALID |
      HID_MAPPING_PROFILE_IGNORED |
      HID_MAPPING_PROFILE_LEARNED |
      HID_MAPPING_PROFILE_BUTTON_DPAD |
      HID_MAPPING_PROFILE_AXIS_DPAD |
      HID_MAPPING_PROFILE_DATABASE_CONFIRMED |
      HID_MAPPING_PROFILE_LAYOUT_CLASSIFIED;
  if (profile.flags & ~allowedProfileFlags) {
    return false;
  }
  for (uint8_t axis = 0; axis < HID_MAPPING_AXIS_COUNT; ++axis) {
    const uint8_t allowedAxisFlags =
        (profile.flags & (HID_MAPPING_PROFILE_BUTTON_DPAD |
                          HID_MAPPING_PROFILE_AXIS_DPAD)) && axis < 4
            ? 0xFF
            : static_cast<uint8_t>(HID_MAPPING_AXIS_INVERT |
                                   HID_MAPPING_AXIS_TRIGGER);
    if (profile.axis_flags[axis] & ~allowedAxisFlags) {
      return false;
    }
  }
  const uint8_t style = hidMappingControllerStyle(profile.layout);
  if (!hidMappingControllerStyleValid(style) ||
      hidMappingLayoutTriggerType(profile.layout) ==
          HID_MAPPING_LAYOUT_TRIGGER_MASK ||
      ((profile.flags & HID_MAPPING_PROFILE_LAYOUT_CLASSIFIED) &&
       style == HID_MAPPING_LAYOUT_UNSPECIFIED)) {
    return false;
  }
  return profile.hat_source == HID_MAPPING_SOURCE_NONE ||
         profile.hat_source == 0;
}

void printProfileHex(Print& out, const HidMappingProfile& profile,
                     uint8_t slot) {
  static const char digits[] = "0123456789ABCDEF";
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&profile);
  out.print(F("PROFILE SLOT="));
  out.print((int)slot + 1);
  out.print(F(" DATA="));
  for (size_t index = 0; index < sizeof(HidMappingProfile); ++index) {
    out.print(digits[bytes[index] >> 4]);
    out.print(digits[bytes[index] & 0x0F]);
  }
  out.println();
}

bool parseProfileSlot(char*& text, uint8_t* slot) {
  long oneBased = 0;
  if (!parseStrictLong(text, &oneBased) ||
      oneBased < 1 || oneBased > HID_MAPPING_PROFILE_COUNT) {
    return false;
  }
  *slot = static_cast<uint8_t>(oneBased - 1);
  return true;
}

bool handleProfileCommand(char* text, Print& out) {
  text = serialSkipSpaces(text);
  HidMappingProfileList list{};
  hidMappingProfileReadList(list);
  if (*text == '\0' || serialTextEqualsExact(text, "STATUS") ||
      serialTextEqualsExact(text, "LIST")) {
    uint8_t count = 0;
    for (uint8_t slot = 0; slot < HID_MAPPING_PROFILE_COUNT; ++slot) {
      if (!(list.profiles[slot].flags & HID_MAPPING_PROFILE_VALID)) continue;
      printProfileKey(out, list.profiles[slot], slot);
      ++count;
    }
    out.print(F("OK:PROFILE_LIST COUNT="));
    out.println((int)count);
    return true;
  }
  char* remainder = nullptr;
  if (serialCommandStartsWith(text, "EXPORT", &remainder)) {
    uint8_t slot = 0;
    if (!parseProfileSlot(remainder, &slot) ||
        *serialSkipSpaces(remainder) != '\0' ||
        !(list.profiles[slot].flags & HID_MAPPING_PROFILE_VALID)) {
      out.println(F("ERR:PROFILE_NOT_FOUND"));
      return true;
    }
    printProfileHex(out, list.profiles[slot], slot);
    out.println(F("OK:PROFILE_EXPORT"));
    return true;
  }
  if (serialCommandStartsWith(text, "IMPORT", &remainder)) {
    HidMappingProfile profile{};
    if (!decodeProfileHex(remainder, &profile) ||
        !profilePayloadValid(profile)) {
      out.println(F("ERR:BAD_PROFILE_DATA"));
      return true;
    }
    profile.flags |= HID_MAPPING_PROFILE_VALID;
    uint8_t stored = 0;
    if (!hidMappingProfileUpsert(profile, &stored)) {
      out.println(F("ERR:PROFILE_WRITE_FAILED"));
      return true;
    }
    out.print(F("OK:PROFILE_IMPORT SLOT="));
    out.println((int)stored + 1);
    return true;
  }
  if (serialCommandStartsWith(text, "DELETE", &remainder)) {
    uint8_t slot = 0;
    if (!parseProfileSlot(remainder, &slot) ||
        !serialTextEqualsExact(remainder, "CONFIRM") ||
        !(list.profiles[slot].flags & HID_MAPPING_PROFILE_VALID)) {
      out.println(F("ERR:PROFILE_DELETE_ARGS"));
      return true;
    }
    out.println(hidMappingProfileRemove(list.profiles[slot].key)
        ? F("OK:PROFILE_DELETE")
        : F("ERR:PROFILE_WRITE_FAILED"));
    return true;
  }
  if (serialCommandStartsWith(text, "CLEAR", &remainder)) {
    if (!serialTextEqualsExact(remainder, "CONFIRM")) {
      out.println(F("ERR:CONFIRM_REQUIRED"));
      return true;
    }
    out.println(hidMappingProfileClear()
        ? F("OK:PROFILE_CLEAR")
        : F("ERR:PROFILE_WRITE_FAILED"));
    return true;
  }
  out.println(F("ERR:BAD_PROFILE_CMD"));
  return true;
}
#endif

#if defined(ENABLE_INPUT_USB) && defined(ADAPT_CAP_SDL_CONTROLLER_MAPPINGS)
// Print the attached controller's mapping as one gamecontrollerdb.txt line,
// ready to paste into gamecontrollerdb_user.txt or send upstream. The line
// needs the live report descriptor for Linux evdev ordering, so it can only
// be produced while that controller is attached.
bool handleMappingCommand(char* text, Print& out) {
  text = serialSkipSpaces(text);
#if defined(ADAPT_CAP_MAPPING_EXPORT_STORE)
  if (serialTextEqualsExact(text, "LIST")) {
    // Cached at teach time, so this answers with nothing plugged in, which is
    // the only state the port can be in while a PC is listening.
    MappingExportList stored{};
    (void)mappingExportStoreRead(stored);
    out.println(F("MAPPING BEGIN"));
    for (uint8_t index = 0; index < MAPPING_EXPORT_ENTRY_COUNT; ++index) {
      if (stored.entries[index].line[0] == '\0') continue;
      out.println(stored.entries[index].line);
    }
    out.println(F("MAPPING END"));
    return true;
  }
  if (serialTextEqualsExact(text, "CLEAR")) {
    out.println(mappingExportStoreClear() ? F("MAPPING CLEARED")
                                          : F("MAPPING ERR CLEAR"));
    return true;
  }
#endif
  if (*text != '\0' && !serialTextEqualsExact(text, "EXPORT")) {
    out.println(F("MAPPING ERR USAGE=EXPORT|LIST|CLEAR"));
    return true;
  }
  HidMappingProfile profile{};
  HidDescriptorLayout layout{};
  if (!inputusb_generic_copy_active_mapping(0, &profile) ||
      !inputusb_generic_copy_descriptor_layout(0, &layout)) {
    out.println(F("MAPPING ERR NO_CONTROLLER"));
    return true;
  }
  char line[320];
  const size_t length = sdlGameControllerDbFormatMapping(
      layout, profile.key, profile,
      sdlGameControllerDbName(profile.key.vid, profile.key.pid,
                              profile.key.bcd_device),
      line, sizeof(line));
  if (length == 0) {
    // The parser refuses ambiguous descriptors; so does the printer, rather
    // than emit a line that would map the wrong buttons on a PC.
    out.println(F("MAPPING ERR NOT_REPRESENTABLE"));
    return true;
  }
  out.println(F("MAPPING BEGIN"));
  out.println(line);
  out.println(F("MAPPING END"));
  return true;
}
#endif

#if defined(ADAPT_CAP_MULTI_SOURCE_CONTROLLER_INPUT) && defined(ENABLE_INPUT_USB_BT)
bool handleDinputCommand(char* text, Print& out) {
  text = serialSkipSpaces(text);
  char* remainder = nullptr;
  bool reset = false;
  if (*text == '\0' || serialTextEqualsExact(text, "STATUS")) {
    const uint8_t player = selectedControllerPlayer();
    const UsbBtSlotOwner owner = usbBtLogicalPlayerOwner(player);
    out.print(F("DINPUT PLAYER="));
    out.print((int)player + 1);
    out.print(F(" SAVED="));
    out.print(usbBtPlayerHasSavedDinputMapping(player) ? 1 : 0);
    HidMappingProfile profile{};
    const bool profileAvailable =
        owner.source == USB_BT_INPUT_USB &&
        inputusb_generic_copy_active_mapping(owner.source_slot, &profile);
    const bool classified =
        profileAvailable &&
        (profile.flags & HID_MAPPING_PROFILE_LAYOUT_CLASSIFIED) != 0;
    out.print(F(" CLASSIFIED="));
    out.print(classified ? 1 : 0);
    out.print(F(" STYLE="));
    out.print(profileAvailable
        ? (int)hidMappingControllerStyle(profile.layout) : 0);
    out.print(F(" SHOULDERS="));
    out.print(classified && hidMappingLayoutHasShoulders(profile.layout) ? 1 : 0);
    out.print(F(" TRIGGERS="));
    out.print(classified
        ? (int)(hidMappingLayoutTriggerType(profile.layout) >> 5) : 0);
    out.print(F(" GRIPS="));
    out.print(classified && hidMappingLayoutHasGrips(profile.layout) ? 1 : 0);
    out.print(F(" FLAGS=0x"));
    out.println(profileAvailable ? profile.flags : 0, HEX);
    return true;
  }
  if (serialCommandStartsWith(text, "RESET", &remainder)) {
    reset = true;
  } else if (serialCommandStartsWith(text, "AUTO", &remainder) ||
             serialCommandStartsWith(text, "AUTOMAP", &remainder)) {
  } else {
    out.println(F("ERR:BAD_DINPUT_CMD"));
    return true;
  }

  uint8_t player = selectedControllerPlayer();
  if (reset) {
    if (!serialTextEqualsExact(remainder, "CONFIRM")) {
      if (!parsePlayerNumber(remainder, &player)) {
        out.println(F("ERR:BAD_PLAYER"));
        return true;
      }
    }
    if (!serialTextEqualsExact(remainder, "CONFIRM")) {
      out.println(F("ERR:CONFIRM_REQUIRED"));
      return true;
    }
    usb_hid_mapping_target_t target{};
    if (!usbBtCapturePlayerDinputMapping(player, &target)) {
      out.println(F("ERR:DINPUT_MAPPING_NOT_FOUND"));
      return true;
    }
    out.println(usbBtResetCapturedDinputMapping(&target)
        ? F("OK:DINPUT_MAPPING_RESET")
        : F("ERR:DINPUT_MAPPING_RESET_FAILED"));
    return true;
  }

  if (*remainder != '\0') {
    if (!parsePlayerNumber(remainder, &player) ||
        *serialSkipSpaces(remainder) != '\0') {
      out.println(F("ERR:BAD_PLAYER"));
      return true;
    }
  }
  const UsbBtSlotOwner owner = usbBtLogicalPlayerOwner(player);
  if (owner.source != USB_BT_INPUT_USB ||
      !inputusb_generic_save_automatic_mapping(owner.source_slot)) {
    out.println(F("ERR:DINPUT_AUTOMAP_FAILED"));
    return true;
  }
  out.print(F("OK:DINPUT_AUTOMAP PLAYER="));
  out.println((int)player + 1);
  return true;
}
#endif

#if defined(ADAPT_CAP_MULTI_SOURCE_CONTROLLER_INPUT) && \
    defined(ENABLE_INPUT_USB_BT) && defined(ENABLE_HID_COMPAT_CAPTURE)
#include "serial_hid_compat_capture.inc"
#endif

#ifdef ADAPT_OUTPUT_USB_DEVICE
bool handleStatsCommand(char* text, Print& out) {
  text = serialSkipSpaces(text);
  char* remainder = nullptr;
  if (serialCommandStartsWith(text, "CLEAR", &remainder)) {
    if (!serialTextEqualsExact(remainder, "CONFIRM")) {
      out.println(F("ERR:CONFIRM_REQUIRED"));
      return true;
    }
    webhid_clear_stats();
    out.println(F("OK:STATS_CLEAR"));
    return true;
  }
  if (*text != '\0' && !serialTextEqualsExact(text, "STATUS") &&
      !serialTextEqualsExact(text, "LIST")) {
    out.println(F("ERR:BAD_STATS_CMD"));
    return true;
  }
  out.print(F("STATS POLL_HZ="));
  out.print((int)poll_rate_hz);
  out.print(F(" TOTAL="));
  out.print(total_polls);
  out.print(F(" MODE="));
  out.println((int)deviceMode);
  for (uint8_t index = 0; index < 32; ++index) {
    if (button_press_count[index] == 0) continue;
    out.print(F("STAT BUTTON="));
    out.print((int)index);
    out.print(F(" COUNT="));
    out.println(button_press_count[index]);
  }
  out.println(F("OK:STATS"));
  return true;
}

bool handleHistoryCommand(char* text, Print& out) {
  text = serialSkipSpaces(text);
  char* remainder = nullptr;
  if (serialCommandStartsWith(text, "CLEAR", &remainder)) {
    if (!serialTextEqualsExact(remainder, "CONFIRM")) {
      out.println(F("ERR:CONFIRM_REQUIRED"));
      return true;
    }
    input_history_index = 0;
    input_history_count = 0;
    out.println(F("OK:HISTORY_CLEAR"));
    return true;
  }
  if (*text != '\0' && !serialTextEqualsExact(text, "STATUS") &&
      !serialTextEqualsExact(text, "LIST")) {
    out.println(F("ERR:BAD_HISTORY_CMD"));
    return true;
  }
  const uint8_t count = input_history_count < INPUT_HISTORY_SIZE
      ? static_cast<uint8_t>(input_history_count)
      : INPUT_HISTORY_SIZE;
  out.print(F("HISTORY COUNT="));
  out.println((int)count);
  for (uint8_t age = 0; age < count; ++age) {
    const uint8_t index = static_cast<uint8_t>(
        (input_history_index - 1 - age + INPUT_HISTORY_SIZE) %
        INPUT_HISTORY_SIZE);
    const InputHistoryEntry& entry = input_history[index];
    out.print(F("HIST AGE="));
    out.print((int)age);
    out.print(F(" MS="));
    out.print(entry.timestamp);
    out.print(F(" BTN=0x"));
    out.print(entry.buttons, HEX);
    out.print(F(" LX="));
    out.print(entry.lx);
    out.print(F(" LY="));
    out.print(entry.ly);
    out.print(F(" RX="));
    out.print(entry.rx);
    out.print(F(" RY="));
    out.println(entry.ry);
  }
  out.println(F("OK:HISTORY"));
  return true;
}
#endif

bool handleAuthManagementCommand(char* text, Print& out) {
  text = serialSkipSpaces(text);
  if (*text == '\0' || serialTextEqualsExact(text, "STATUS") ||
      serialTextEqualsExact(text, "KEY STATUS")) {
    const AuthStorageDiagnostics diagnostics = authStorageLastDiagnostics();
    out.print(F("AUTHKEY PS4="));
    out.print(authStorageHasValidKey(AUTH_KEY_TYPE_PS4) ? 1 : 0);
    out.print(F(" UPLOAD="));
    out.print((int)authStorageLastUploadStatus());
    out.print(F(" CRC=0x"));
    out.print(authStorageLastUploadCrc32(), HEX);
    out.print(F(" REGION="));
    out.print((int)diagnostics.serial_region_state);
    out.print(F(" CHUNKS="));
    out.print((int)diagnostics.received_chunk_count);
    out.print('/');
    out.print((int)diagnostics.total_chunk_count);
    out.print(F(" MISSING="));
    out.println(diagnostics.first_missing_offset);
#ifdef ADAPT_OUTPUT_USB_DEVICE
    ps4Auth.writeDiagnostics(out);
#endif
    return true;
  }
  char* remainder = nullptr;
  if (serialCommandStartsWith(text, "CLEAR", &remainder) ||
      serialCommandStartsWith(text, "KEY CLEAR", &remainder)) {
    char* confirm = remainder;
    if (serialCommandStartsWith(remainder, "PS4", &confirm)) {
      remainder = confirm;
    }
    if (!serialTextEqualsExact(remainder, "CONFIRM")) {
      out.println(F("ERR:CONFIRM_REQUIRED"));
      return true;
    }
    clearAuthBlob(AUTH_KEY_TYPE_PS4);
    updateAuthKeyStatus();
    out.println(F("OK:AUTH_PS4_CLEARED"));
    return true;
  }
  out.println(F("ERR:BAD_AUTH_CMD"));
  return true;
}

bool handleMenuCommand(char* text, Print& out) {
  text = serialSkipSpaces(text);
  if (*text == '\0' || serialTextEqualsExact(text, "CONTROLLER") ||
      serialTextEqualsExact(text, "QUICK") ||
      serialTextEqualsExact(text, "OPEN")) {
    openControllerMenuFromSerial();
    out.println(F("OK:MENU_CONTROLLER"));
    return true;
  }
  if (serialTextEqualsExact(text, "SYSTEM")) {
    openSystemMenuFromSerial();
    out.println(F("OK:MENU_SYSTEM"));
    return true;
  }
  if (serialTextEqualsExact(text, "CLOSE") ||
      serialTextEqualsExact(text, "BACK")) {
    closeMenusFromSerial();
    out.println(F("OK:MENU_CLOSED"));
    return true;
  }
  out.println(F("ERR:BAD_MENU_CMD"));
  return true;
}

void printManagementHelp(Print& out) {
  out.println(F("MGMT CORE:PING,INFO,INPUT,OUTPUT,PLAYER,MENU,CONFIG,TURBO,REMAP,AUTH,FACTORY RESET,RESET,BOOT"));
#ifdef ADAPT_OUTPUT_USB_DEVICE
  out.println(F("MGMT TELEMETRY:STATS [LIST|CLEAR CONFIRM],HISTORY [LIST|CLEAR CONFIRM]"));
#endif
#if defined(ADAPT_CAP_ESP32_CONTROLLER_BRIDGE) && defined(ENABLE_INPUT_ESP32_SPI)
  out.println(F("MGMT MODERN2USB:TRANSPORT,BT,WIFI,WEB,ESP32"));
#endif
#if ADAPT_POLICY_CONTROLLER_FEEDBACK_COMMANDS
  out.println(F("MGMT TOOLS:CONTROLLER LED,PLAYER IDENTIFY,PROFILE"));
#elif defined(ENABLE_INPUT_USB)
  out.println(F("MGMT TOOLS:PROFILE"));
#endif
#if defined(ADAPT_CAP_MULTI_SOURCE_CONTROLLER_INPUT) && defined(ENABLE_INPUT_USB_BT)
  out.println(F("MGMT DINPUT:STATUS,AUTO,RESET"));
#ifdef ENABLE_HID_COMPAT_CAPTURE
  out.println(F("MGMT HIDDEV:HIDCAP STATUS|DESCRIPTOR|LAYOUT|PROFILE|REPORT [PLAYER],HIDCAP GUIDED <PLAYER> CONFIRM"));
#endif
#endif
  out.println(F("MGMT CONFIG:SET LIST,TURBO,REMAP"));
}

}  // namespace

bool handleSerialManagementCommand(const char* command, Print& out) {
  char* remainder = nullptr;
  if (serialTextEqualsExact(command, "PING")) {
    out.println(F("PONG"));
    return true;
  }
  if (serialTextEqualsExact(command, "INFO")) {
    out.print(F("INFO PRODUCT=" PRODUCT_PROTOCOL_ID " VERSION="));
    out.print(F(FIRMWARE_VERSION_STRING));
    out.print(F(" TAG="));
    out.print(F(FIRMWARE_VERSION_TAG));
    out.print(F(" HARDWARE=\""));
    out.print(F(FIRMWARE_HARDWARE_STRING));
    out.println(F("\""));
    return true;
  }
  if (serialTextEqualsExact(command, "CAPS") ||
      serialTextEqualsExact(command, "COMMANDS") ||
      serialTextEqualsExact(command, "MGMT") ||
      serialTextEqualsExact(command, "MGMT HELP")) {
    printManagementHelp(out);
    out.println(F("OK:MGMT_HELP"));
    return true;
  }
  if (serialCommandStartsWith(command, "HELP", &remainder) &&
      *remainder != '\0') {
    printManagementHelp(out);
    out.println(F("OK:MGMT_HELP"));
    return true;
  }
  if (serialTextEqualsExact(command, "ABOUT")) {
    out.println(F("OK:ABOUT USE_INFO_COMMAND"));
    return true;
  }
  if (serialTextEqualsExact(command, "SAVE")) {
    out.println(F("OK:SAVE AUTO=1"));
    return true;
  }
  if (serialCommandStartsWith(command, "INPUT", &remainder) ||
      serialCommandStartsWith(command, "IN", &remainder)) {
    return handleInputCommand(remainder, out);
  }
  if (serialCommandStartsWith(command, "OUTPUT", &remainder)) {
    return handleOutputCommand(remainder, out);
  }
  if (serialCommandStartsWith(command, "PLAYER", &remainder)) {
    return handlePlayerCommand(remainder, out);
  }
  if (serialCommandStartsWith(command, "MENU", &remainder)) {
    return handleMenuCommand(remainder, out);
  }
  if (serialCommandStartsWith(command, "CONFIG", &remainder) ||
      serialCommandStartsWith(command, "SETTING", &remainder) ||
      serialCommandStartsWith(command, "SETTINGS", &remainder)) {
    return handleSerialSetCommand(remainder, out);
  }
  if (serialCommandStartsWith(command, "TURBO", &remainder)) {
    return handleTurboCommand(remainder, out);
  }
  if (serialCommandStartsWith(command, "REMAP", &remainder)) {
    return handleRemapCommand(remainder, out);
  }
  if (serialCommandStartsWith(command, "AUTH", &remainder) ||
      serialCommandStartsWith(command, "PSAUTH", &remainder) ||
      serialCommandStartsWith(command, "AUTHKEY", &remainder)) {
    return handleAuthManagementCommand(remainder, out);
  }
#ifdef ADAPT_OUTPUT_USB_DEVICE
  if (serialCommandStartsWith(command, "STATS", &remainder)) {
    return handleStatsCommand(remainder, out);
  }
  if (serialCommandStartsWith(command, "HISTORY", &remainder)) {
    return handleHistoryCommand(remainder, out);
  }
#endif
  if (serialCommandStartsWith(command, "FACTORY RESET", &remainder) ||
      serialCommandStartsWith(command, "FACTORY_RESET", &remainder)) {
    if (!serialTextEqualsExact(remainder, "CONFIRM")) {
      out.println(F("ERR:CONFIRM_REQUIRED"));
      return true;
    }
    if (!factoryResetSettings()) {
      out.println(F("ERR:FACTORY_RESET_PARTIAL ESP32_ERASE=0 REBOOT=0"));
      return true;
    }
    out.println(F("OK:FACTORY_RESET REBOOT=1"));
    delay(100);
    reboot();
    return true;
  }
#if defined(ADAPT_CAP_ESP32_CONTROLLER_BRIDGE) && defined(ENABLE_INPUT_ESP32_SPI)
  if (serialCommandStartsWith(command, "TRANSPORT", &remainder)) {
    return handleTransportCommand(remainder, out);
  }
#endif
#if ADAPT_POLICY_CONTROLLER_FEEDBACK_COMMANDS
  if (serialCommandStartsWith(command, "CONTROLLER LED", &remainder) ||
      serialCommandStartsWith(command, "CONTROLLERLED", &remainder) ||
      serialCommandStartsWith(command, "CLED", &remainder)) {
    return handleControllerLedCommand(remainder, out);
  }
#endif
#if defined(ENABLE_INPUT_USB)
  if (serialCommandStartsWith(command, "PROFILE", &remainder) ||
      serialCommandStartsWith(command, "HIDPROFILE", &remainder) ||
      serialCommandStartsWith(command, "HIDMAP", &remainder)) {
    return handleProfileCommand(remainder, out);
  }
#endif
#if defined(ADAPT_CAP_MULTI_SOURCE_CONTROLLER_INPUT) && defined(ENABLE_INPUT_USB_BT)
#ifdef ENABLE_HID_COMPAT_CAPTURE
  if (serialCommandStartsWith(command, "HIDCAP", &remainder)) {
    return handleHidCaptureCommand(remainder, out);
  }
#endif
  if (serialCommandStartsWith(command, "DINPUT", &remainder)) {
    return handleDinputCommand(remainder, out);
  }
#endif
#if defined(ENABLE_INPUT_USB) && defined(ADAPT_CAP_SDL_CONTROLLER_MAPPINGS)
  if (serialCommandStartsWith(command, "MAPPING", &remainder)) {
    return handleMappingCommand(remainder, out);
  }
#endif
  return false;
}

void appendSerialManagementHelp(Print& out) {
  out.print(F(",CAPS,INPUT,OUTPUT,PLAYER,MENU,CONFIG,TURBO,REMAP,AUTH,MAPPING EXPORT|LIST|CLEAR,FACTORY RESET CONFIRM"));
#ifdef ADAPT_OUTPUT_USB_DEVICE
  out.print(F(",STATS,HISTORY"));
#endif
#if defined(ADAPT_CAP_ESP32_CONTROLLER_BRIDGE) && defined(ENABLE_INPUT_ESP32_SPI)
  out.print(F(",TRANSPORT"));
#endif
#if ADAPT_POLICY_CONTROLLER_FEEDBACK_COMMANDS
  out.print(F(",CONTROLLER LED,PROFILE"));
#elif defined(ENABLE_INPUT_USB)
  out.print(F(",PROFILE"));
#endif
#if defined(ADAPT_CAP_MULTI_SOURCE_CONTROLLER_INPUT) && defined(ENABLE_INPUT_USB_BT)
  out.print(F(",DINPUT"));
#ifdef ENABLE_HID_COMPAT_CAPTURE
  out.print(F(",HIDCAP"));
#endif
#endif
}
