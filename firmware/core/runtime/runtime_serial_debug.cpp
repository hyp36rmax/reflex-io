#include "../../product_config.h"
#include "../../firmware_build_info.h"
#include "../management_identity.h"

#include "runtime_serial_debug.h"

#include <Arduino.h>
#include <stdlib.h>

#if defined(ARDUINO_ARCH_RP2040)
#include <hardware/adc.h>
#include <hardware/clocks.h>
#include <hardware/watchdog.h>
#include <pico/unique_id.h>
#endif

#ifdef ADAPT_OUTPUT_USB_DEVICE
#include <Adafruit_TinyUSB.h>
#endif

#ifdef ENABLE_INPUT_JVS
#include "../../input/jvs/jvs_host_runtime.h"
#include "../../input/jvs/jvs_analog_config.h"
#include "../../input/jvs/jvs_analog_range_runtime.h"
#include "../../input/jvs/jvs_panel_map.h"
#endif
#ifdef ENABLE_INPUT_DREAMCAST
#include "../../input/dreamcast/Input_Dreamcast.h"
#endif
#if defined(ENABLE_USB_AUTH_SIDECAR) || defined(ENABLE_INPUT_USB)
#include "../../input/usb_host/input_usb_host_service.h"
#include <pio_usb.h>
extern "C" uint32_t adapt_tinyusb_hub_debug_word(uint8_t slot, uint8_t field);
#endif
#ifdef ENABLE_USB_BT_HCI
#include "../../input/usb_bt/usb_bt_hci_host.h"
#endif
#if defined(ENABLE_USB_XINPUT_HOST) || defined(ENABLE_USB_XINPUT_AUTH_HOST)
#include "../../input/usb_host/usb_xinput_host.h"
#endif
#ifdef ENABLE_INPUT_USB
#include <input_usb.h>
#include "../hid_mapping_profiles.h"
#include "../../input/usb_host/input_usb/sdl_gamecontrollerdb.h"
#endif
#ifdef ENABLE_INPUT_ESP32_SPI
#include "../../input/esp32_spi/esp32_service_uart.h"
#endif
#ifdef ENABLE_INPUT_AUTODETECT
#include "../../input/autodetect/input_autodetect_runtime_state.h"
#endif
#ifdef ADAPT_OUTPUT_USB_DEVICE
#include "../../output/auth/webhid_auth_runtime.h"
#endif
#ifdef ENABLE_USB_AUTH_SIDECAR
#include "../../output/auth/ps_auth_dongle_runtime.h"
#endif
#ifdef ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT
#include "../../output/auth/xbone_auth_passthrough.h"
#include "../../output/xboxone/out_xboxone.h"
#endif
#include "../../features/feature_module.h"
#ifdef ADAPT_FEATURE_MISTER_SERIAL_STATUS
#include "../../features/mister_status/mister_status_feature.h"
#endif
#ifdef ENABLE_TTY2OLED_SERIAL
#include "../../features/tty2oled/tty2oled_feature.h"
#endif
#include "../../menu/menu_idle_runtime.h"
#include "../../menu/menu_mode_state.h"
#include "../../menu/menu_runtime_state.h"
#include "../../menu/menu_working_state.h"
#include "../../output/output_runtime_state.h"
#include "../../output/runtime/output_loop_runtime.h"
#include "../../platform/latency_service_mask.h"
#include "../../platform/latency_trace_gpio.h"
#include "../../platform/latency_test.h"
#include "../adapt_state_stream_runtime.h"
#include "../controller_frame_state.h"
#include "../device_runtime_state.h"
#include "../firmware_support.h"
#include "../input_overlay_runtime.h"
#include "../mister_serial_device.h"
#include "../oled_serial_runtime.h"
#include "../serial_command_parser.h"
#include "../serial_debug_runtime.h"
#include "../serial_management_commands.h"
#include "../serial_memcard_commands.h"
#include "../settings_registry.h"
#include "../settings_store.h"
#include "runtime_debug_cdc_state.h"

namespace {

#ifdef ENABLE_RUNTIME_SERIAL_DEBUG_CDC
// MiSTer Main sends lines of up to 2,000 bytes (card requests, artwork).
constexpr size_t kSerialCommandBufferSize = 2048;
char serialCommandBuffer[kSerialCommandBufferSize];
size_t serialCommandLength = 0;
bool serialCommandDiscarding = false;

#ifdef ADAPT_FEATURE_MISTER_SERIAL_REQUESTS
// A MiSTer request that arrived while another runs. It stays in the buffer
// and nothing more is read until the device takes it.
bool serialCommandHeld = false;

#ifdef PRODUCT_SHORT_NAME
  #define MISTER_SERIAL_PRODUCT_NAME PRODUCT_SHORT_NAME
#else
  #define MISTER_SERIAL_PRODUCT_NAME PRODUCT_PROTOCOL_ID
#endif

class RuntimeMisterSerialPort final : public mister_serial::Port {
 public:
  uint32_t nowMs() override { return millis(); }
  bool connected() override { return runtimeDebugCdc.dtr() != 0; }
  int availableForWrite() override { return runtimeDebugCdc.availableForWrite(); }
  size_t write(const uint8_t* data, size_t length) override {
    return runtimeDebugCdc.write(data, length);
  }
  void flushOutput() override { runtimeDebugCdc.flush(); }
  bool lastByteWasLineEnd() override { return runtimeDebugCdc.lastByteWasLineEnd(); }
  void service() override { TinyUSB_Device_Task(); }
  void discardInput() override {
    while (runtimeDebugCdc.available() > 0) {
      runtimeDebugCdc.read();
    }
    serialCommandLength = 0;
    serialCommandDiscarding = false;
    serialCommandHeld = false;
  }
  void quietOutputModes() override {
    adaptStateSetEnabled(false);
    inputOverlaySetEnabled(false);
    oledSerialSetEnabled(false);
#ifdef ENABLE_TTY2OLED_SERIAL
    tty2oledSerialEndRawModes();
#endif
  }
#ifdef ADAPT_FEATURE_MISTER_SERIAL_STATUS
  void statusRecord(char** fields, uint8_t count) override {
#ifdef ENABLE_TTY2OLED_SERIAL
    if (count >= 3 && strcmp(fields[2], "HELLO") == 0)
      tty2oledSerialReleaseDisplay();
#endif
    misterStatusReceive(fields, count, millis());
  }
  void statusDisconnected() override { misterStatusDisconnected(); }
#endif
};

RuntimeMisterSerialPort misterSerialPort;
mister_serial::Device misterSerialDevice(misterSerialPort, misterSerialCardBackend(),
                                         MISTER_SERIAL_PRODUCT_NAME, FIRMWARE_VERSION_STRING);

// Binary streams would corrupt the lines of a MiSTer session.
bool refuseStreamDuringMisterSession(Print& out) {
  if (!misterSerialDevice.sessionActive()) {
    return false;
  }
  out.println(F("ERR:MISTER_SESSION"));
  return true;
}
#else
bool refuseStreamDuringMisterSession(Print&) { return false; }
#endif

char uppercaseAscii(char value) {
  return (value >= 'a' && value <= 'z') ? (char)(value - ('a' - 'A')) : value;
}

bool isSerialCommandWhitespace(char value) {
  return value == ' ' || value == '\t';
}

bool serialCommandEquals(const char* expected) {
  size_t index = 0;
  while (expected[index] != '\0') {
    if (index >= serialCommandLength ||
        uppercaseAscii(serialCommandBuffer[index]) != expected[index]) {
      return false;
    }
    ++index;
  }

  return index == serialCommandLength;
}

char* skipSerialSpaces(char* text) {
  while (*text == ' ' || *text == '\t') {
    ++text;
  }
  return text;
}

bool serialCommandStartsWith(const char* expected, char** remainder) {
  size_t index = 0;
  while (expected[index] != '\0') {
    if (uppercaseAscii(serialCommandBuffer[index]) != expected[index]) {
      return false;
    }
    ++index;
  }

  char* tail = &serialCommandBuffer[index];
  if (*tail == '\0') {
    *remainder = tail;
    return true;
  }
  if (*tail != ' ' && *tail != '\t') {
    return false;
  }

  *remainder = skipSerialSpaces(tail);
  return true;
}

bool serialTextStartsWith(char* text, const char* expected, char** remainder) {
  text = skipSerialSpaces(text);
  size_t index = 0;
  while (expected[index] != '\0') {
    if (text[index] == '\0' ||
        uppercaseAscii(text[index]) != expected[index]) {
      return false;
    }
    ++index;
  }

  char* tail = &text[index];
  if (*tail == '\0') {
    *remainder = tail;
    return true;
  }
  if (*tail != ' ' && *tail != '\t') {
    return false;
  }

  *remainder = skipSerialSpaces(tail);
  return true;
}

bool serialTokenEquals(const char* token, const char* expected) {
  size_t index = 0;
  while (expected[index] != '\0') {
    if (token[index] == '\0' ||
        token[index] == ' ' ||
        token[index] == '\t' ||
        uppercaseAscii(token[index]) != expected[index]) {
      return false;
    }
    ++index;
  }

  return token[index] == '\0' || token[index] == ' ' || token[index] == '\t';
}

#if defined(ARDUINO_ARCH_RP2040)
void printRp2040TemperatureStatus() {
  static bool adcInitialized = false;
  if (!adcInitialized) {
    adc_init();
    adcInitialized = true;
  }

  const uint previousInput = adc_get_selected_input();
  const bool tempWasEnabled = (adc_hw->cs & ADC_CS_TS_EN_BITS) != 0;

  adc_set_temp_sensor_enabled(true);
  adc_select_input(ADC_TEMPERATURE_CHANNEL_NUM);

  constexpr uint8_t kSamples = 16;
  uint32_t rawSum = 0;
  for (uint8_t sample = 0; sample < kSamples; ++sample) {
    rawSum += adc_read();
  }

  adc_select_input(previousInput);
  if (!tempWasEnabled) {
    adc_set_temp_sensor_enabled(false);
  }

  const float rawAverage = (float)rawSum / (float)kSamples;
  const float voltage = rawAverage * 3.3f / 4096.0f;
  const float temperatureC = 27.0f - ((voltage - 0.706f) / 0.001721f);

  runtimeDebugCdc.print(F("TEMP CLK_HZ="));
  runtimeDebugCdc.print((uint32_t)clock_get_hz(clk_sys));
  runtimeDebugCdc.print(F(" RAW="));
  runtimeDebugCdc.print(rawAverage, 1);
  runtimeDebugCdc.print(F(" MV="));
  runtimeDebugCdc.print(voltage * 1000.0f, 1);
  runtimeDebugCdc.print(F(" C="));
  runtimeDebugCdc.println(temperatureC, 1);
}
#endif

bool readSerialToken(char*& text, char* token, size_t tokenSize) {
  text = skipSerialSpaces(text);
  if (*text == '\0' || tokenSize == 0) {
    return false;
  }

  size_t index = 0;
  while (*text != '\0' && !isSerialCommandWhitespace(*text)) {
    if (index + 1 < tokenSize) {
      token[index++] = *text;
    }
    ++text;
  }
  token[index] = '\0';
  text = skipSerialSpaces(text);
  return index != 0;
}

void handleLatencyServiceSerialCommand(char* text) {
  if (*text == '\0' || serialTokenEquals(text, "STATUS")) {
    latencyServicePrintStatus(runtimeDebugCdc);
    return;
  }
  if (serialTokenEquals(text, "CLEAR") || serialTokenEquals(text, "RESET")) {
    latencyServiceClearDisabledMask();
    latencyServicePrintStatus(runtimeDebugCdc);
    return;
  }

  char token[24] = {};
  if (!readSerialToken(text, token, sizeof(token))) {
    runtimeDebugCdc.println(F("ERR:BAD_LATENCY_SERVICE"));
    return;
  }

  uint32_t service = 0;
  if (!latencyServiceNameToMask(token, &service)) {
    runtimeDebugCdc.println(F("ERR:BAD_LATENCY_SERVICE_NAME"));
    return;
  }

  if (serialTokenEquals(text, "OFF") || serialTokenEquals(text, "0") ||
      serialTokenEquals(text, "DISABLE")) {
    latencyServiceSetDisabled(service, true);
  } else if (serialTokenEquals(text, "ON") || serialTokenEquals(text, "1") ||
             serialTokenEquals(text, "ENABLE")) {
    latencyServiceSetDisabled(service, false);
  } else {
    runtimeDebugCdc.println(F("ERR:BAD_LATENCY_SERVICE_STATE"));
    return;
  }

  latencyServicePrintStatus(runtimeDebugCdc);
}

void printHexByte(uint8_t value) {
  if (value < 0x10) {
    runtimeDebugCdc.print('0');
  }
  runtimeDebugCdc.print(value, HEX);
}

void printHex32(uint32_t value) {
  runtimeDebugCdc.print(F("0x"));
  for (int shift = 28; shift >= 0; shift -= 4) {
    runtimeDebugCdc.print((value >> shift) & 0x0F, HEX);
  }
}

void printHex16(uint16_t value) {
  for (int shift = 12; shift >= 0; shift -= 4) {
    runtimeDebugCdc.print((value >> shift) & 0x0F, HEX);
  }
}

void printHexByteArray(const uint8_t* values, size_t count) {
  for (size_t index = 0; index < count; ++index) {
    if (index != 0) {
      runtimeDebugCdc.print(' ');
    }
    printHexByte(values[index]);
  }
}

#ifdef ENABLE_USB_BT_HCI
void printBtAddress(Print& out, const uint8_t addr[6]) {
  for (int i = 5; i >= 0; --i) {
    if (addr[i] < 16) {
      out.print('0');
    }
    out.print(addr[i], HEX);
    if (i > 0) {
      out.print(':');
    }
  }
}
#endif

bool parseRuntimeOutputModeToken(const char* token, outputMode_t* mode) {
  if (serialTokenEquals(token, "MISTER") ||
      serialTokenEquals(token, "LINUX") ||
      serialTokenEquals(token, "MISTERDINPUT")) {
    *mode = OUTPUT_MISTER;
    return true;
  }
  if (serialTokenEquals(token, "DINPUT") ||
      serialTokenEquals(token, "HID") ||
      serialTokenEquals(token, "WINDINPUT") ||
      serialTokenEquals(token, "WINDOWSDINPUT")) {
    *mode = OUTPUT_HID;
    return true;
  }
  if (serialTokenEquals(token, "AUTO")) {
    *mode = OUTPUT_AUTO;
    return true;
  }
  if (serialTokenEquals(token, "WINDOWS") ||
      serialTokenEquals(token, "WINXINPUT") ||
      serialTokenEquals(token, "XINPUT") ||
      serialTokenEquals(token, "XINPUT2") ||
      serialTokenEquals(token, "XINPUT2P") ||
      serialTokenEquals(token, "XINPUTW") ||
      serialTokenEquals(token, "XINPUT4") ||
      serialTokenEquals(token, "XINPUT4P") ||
      serialTokenEquals(token, "ANALOGUE") ||
      serialTokenEquals(token, "POCKET") ||
      serialTokenEquals(token, "POCKETDOCK")) {
    *mode = OUTPUT_XINPUT2P;
    return true;
  }
  if (serialTokenEquals(token, "XBOX360") ||
      serialTokenEquals(token, "X360")) {
    *mode = OUTPUT_XINPUT;
    return true;
  }
  if (serialTokenEquals(token, "XID") ||
      serialTokenEquals(token, "XBOXOG")) {
    *mode = OUTPUT_XID;
    return true;
  }
  if (serialTokenEquals(token, "XBOXONE") ||
      serialTokenEquals(token, "XBONE") ||
      serialTokenEquals(token, "XB1")) {
    *mode = OUTPUT_XBOXONE;
    return true;
  }
  if (serialTokenEquals(token, "PS3")) {
    *mode = OUTPUT_PS3;
    return true;
  }
  if (serialTokenEquals(token, "PS4")) {
    *mode = OUTPUT_PS4;
    return true;
  }
  if (serialTokenEquals(token, "PS5")) {
    *mode = OUTPUT_PS5;
    return true;
  }
  if (serialTokenEquals(token, "SWITCH")) {
    *mode = OUTPUT_SWITCHPRO;
    return true;
  }
  if (serialTokenEquals(token, "SWITCHPRO")) {
    *mode = OUTPUT_SWITCHPRO;
    return true;
  }
  if (serialTokenEquals(token, "GC") ||
      serialTokenEquals(token, "GCWIIU") ||
      serialTokenEquals(token, "GCADAPTER")) {
    *mode = OUTPUT_GCWIIU;
    return true;
  }
  if (serialTokenEquals(token, "KEYBOARD") ||
      serialTokenEquals(token, "KBD") ||
      serialTokenEquals(token, "MAME")) {
    *mode = OUTPUT_KEYBOARD;
    return true;
  }
  return false;
}

bool parseWindowsOutputToken(const char* token, uint8_t* winOutput) {
  outputMode_t mode = OUTPUT_HID;
  if (!parseRuntimeOutputModeToken(token, &mode)) return false;
  switch (mode) {
    case OUTPUT_HID:
    case OUTPUT_MISTER: *winOutput = HOST_OUTPUT_PREFERENCE_DINPUT; break;
    case OUTPUT_XINPUT2P: *winOutput = HOST_OUTPUT_PREFERENCE_XINPUT; break;
    case OUTPUT_KEYBOARD: *winOutput = HOST_OUTPUT_PREFERENCE_KEYBOARD; break;
    case OUTPUT_XBOXONE: *winOutput = HOST_OUTPUT_PREFERENCE_XBOXONE; break;
    case OUTPUT_PS3: *winOutput = HOST_OUTPUT_PREFERENCE_PS3; break;
    case OUTPUT_PS4: *winOutput = HOST_OUTPUT_PREFERENCE_PS4; break;
    case OUTPUT_PS5: *winOutput = HOST_OUTPUT_PREFERENCE_PS5; break;
    case OUTPUT_SWITCHPRO: *winOutput = HOST_OUTPUT_PREFERENCE_SWITCHPRO; break;
    default: return false;
  }
  return true;
}
bool parseLatencyModeToken(const char* token, uint8_t* mode) {
  if (serialTokenEquals(token, "OFF")) {
    *mode = LATENCY_MODE_OFF;
    return true;
  }
  if (serialTokenEquals(token, "ON") ||
      serialTokenEquals(token, "PULSE") ||
      serialTokenEquals(token, "SYNTH") ||
      serialTokenEquals(token, "SYNTH_INTERNAL") ||
      serialTokenEquals(token, "NOCTRL")) {
    *mode = LATENCY_MODE_SYNTH_INTERNAL;
    return true;
  }
  if (serialTokenEquals(token, "SYNTHPC") ||
      serialTokenEquals(token, "SYNTH_PC") ||
      serialTokenEquals(token, "NOCTRLPC")) {
    *mode = LATENCY_MODE_SYNTH_PC;
    return true;
  }
  if (serialTokenEquals(token, "SYNTHM") ||
      serialTokenEquals(token, "SYNTH_MISTER") ||
      serialTokenEquals(token, "NOCTRLMISTER")) {
    *mode = LATENCY_MODE_SYNTH_MISTER;
    return true;
  }
  if (serialTokenEquals(token, "TRIGPC") ||
      serialTokenEquals(token, "TRIG_PC") ||
      serialTokenEquals(token, "CTRLPC")) {
    *mode = LATENCY_MODE_TRIGGER_PC;
    return true;
  }
  if (serialTokenEquals(token, "TRIG") ||
      serialTokenEquals(token, "TRIG_INTERNAL") ||
      serialTokenEquals(token, "CTRL")) {
    *mode = LATENCY_MODE_TRIGGER_INTERNAL;
    return true;
  }
  if (serialTokenEquals(token, "TRIGM") ||
      serialTokenEquals(token, "TRIG_MISTER") ||
      serialTokenEquals(token, "CTRLMISTER")) {
    *mode = LATENCY_MODE_TRIGGER_MISTER;
    return true;
  }
  return false;
}

bool parseLatencyHostTypeToken(const char* token, uint8_t* hostType) {
  if (serialTokenEquals(token, "INTERNAL") ||
      serialTokenEquals(token, "LOOP")) {
    *hostType = LATENCY_HOST_INTERNAL;
    return true;
  }
  if (serialTokenEquals(token, "PC")) {
    *hostType = LATENCY_HOST_PC;
    return true;
  }
  if (serialTokenEquals(token, "MISTER") ||
      serialTokenEquals(token, "FPGA")) {
    *hostType = LATENCY_HOST_MISTER;
    return true;
  }
  return false;
}

bool parseUnsignedLongToken(const char* token, uint32_t* value) {
  if (token == nullptr) {
    return false;
  }

  char* end = nullptr;
  const unsigned long parsed = strtoul(token, &end, 10);
  if (end == token) {
    return false;
  }
  while (*end == ' ' || *end == '\t') {
    ++end;
  }
  if (*end != '\0') {
    return false;
  }

  *value = (uint32_t)parsed;
  return true;
}

void saveRuntimeOutputModeFromSerial(outputMode_t newMode) {
  newMode = sanitizeConfiguredOutputMode(newMode);
  GlobalSettingsRecord globalSettings{};
  readGlobalSettings(globalSettings);
  globalSettings.configured_output_mode = (uint8_t)newMode;
  writeGlobalSettings(globalSettings);

  configuredOutputMode = newMode;
  outputMode = sanitizeRuntimeOutputMode(configuredOutputMode);
  autoDetectState = AUTO_STATE_IDLE;
  auto_detect_clear_scratch_state();

  runtimeDebugCdc.print(F("OK:OUT="));
  runtimeDebugCdc.println(get_mode_name(configuredOutputMode));
  runtimeDebugCdc.flush();
  delay(50);
  reboot();
}

void saveWindowsOutputPreferenceFromSerial(uint8_t newWinOutput) {
  saveSystemSettingByte(SettingId::WinOutput, newWinOutput);
  const bool windowsOutputDescriptorChanged =
    newWinOutput != menu_win_output &&
    configuredOutputMode == OUTPUT_AUTO &&
    autoDetectState == AUTO_STATE_WINDOWS;
  if (newWinOutput != menu_win_output) {
    auto_detect_clear_scratch_state();
  }
  menu_win_output = newWinOutput;

  runtimeDebugCdc.print(F("OK:WIN_OUT="));
  runtimeDebugCdc.println((int)menu_win_output);
  if (windowsOutputDescriptorChanged) {
    runtimeDebugCdc.println(F("OK:REBOOT=WINDOWS_OUTPUT"));
    runtimeDebugCdc.flush();
    delay(50);
    reboot();
  }
}

void saveMisterOutputPreferenceFromSerial(uint8_t newMisterOutput) {
  saveSystemSettingByte(SettingId::MisterOutput, newMisterOutput);
  const bool misterOutputDescriptorChanged =
    newMisterOutput != menu_mister_output &&
    configuredOutputMode == OUTPUT_AUTO &&
    autoDetectState == AUTO_STATE_FALLBACK_HID;
  if (newMisterOutput != menu_mister_output) {
    auto_detect_clear_scratch_state();
  }
  menu_mister_output = newMisterOutput;

  runtimeDebugCdc.print(F("OK:MISTER_OUT="));
  runtimeDebugCdc.println((int)menu_mister_output);
  if (misterOutputDescriptorChanged) {
    runtimeDebugCdc.println(F("OK:REBOOT=MISTER_OUTPUT"));
    runtimeDebugCdc.flush();
    delay(50);
    reboot();
  }
}

void saveLatencyConfigurationFromSerial(bool enabled, bool controllerInLoop, uint8_t hostType) {
  GlobalSettingsRecord globalSettings{};
  readGlobalSettings(globalSettings);
  globalSettings.latency_test = enabled ? 1 : 0;
  globalSettings.latency_controller_in_loop = controllerInLoop ? 1 : 0;
  globalSettings.latency_host_type = hostType;
  writeGlobalSettings(globalSettings);
  menu_latency_test = globalSettings.latency_test;
  menu_latency_controller_in_loop = globalSettings.latency_controller_in_loop;
  menu_latency_host_type = globalSettings.latency_host_type;
  latencyTest.setEnabled(menu_latency_test != 0);
  latencyTest.setControllerInLoop(menu_latency_controller_in_loop != 0);
  latencyTest.setHostType(menu_latency_host_type);
}

void saveLatencyModeFromSerial(uint8_t newMode) {
  latencyTest.setMode(newMode);
  saveLatencyConfigurationFromSerial(latencyTest.isEnabled(),
                                     latencyTest.isControllerInLoop(),
                                     latencyTest.getHostType());

  runtimeDebugCdc.print(F("OK:LAT_MODE="));
  runtimeDebugCdc.println(latencyTestModeName(latencyTest.getMode()));
}

void saveLatencyEnabledFromSerial(bool enabled) {
  saveLatencyConfigurationFromSerial(enabled,
                                     latencyTest.isControllerInLoop(),
                                     latencyTest.getHostType());
  runtimeDebugCdc.print(F("OK:LAT_ENABLED="));
  runtimeDebugCdc.println(enabled ? 1 : 0);
}

void saveLatencyControllerFromSerial(bool controllerInLoop) {
  saveLatencyConfigurationFromSerial(latencyTest.isEnabled(),
                                     controllerInLoop,
                                     latencyTest.getHostType());
  runtimeDebugCdc.print(F("OK:LAT_CTRL="));
  runtimeDebugCdc.println(controllerInLoop ? F("CONTROLLER") : F("INTERNAL"));
}

void saveLatencyHostFromSerial(uint8_t hostType) {
  saveLatencyConfigurationFromSerial(latencyTest.isEnabled(),
                                     latencyTest.isControllerInLoop(),
                                     hostType);
  runtimeDebugCdc.print(F("OK:LAT_HOST="));
  runtimeDebugCdc.println(latencyHostTypeName(hostType));
}

#if defined(ENABLE_USB_AUTH_SIDECAR) || defined(ENABLE_INPUT_USB)
void printUsbHostSerialStatus();
#endif
#ifdef ENABLE_USB_AUTH_SIDECAR
void printAuthSidecarSerialStatus(Print& out);
#endif

void printRuntimeSerialStatus() {
  runtimeDebugCdc.print(F("INFO PRODUCT=" PRODUCT_PROTOCOL_ID " VERSION="));
  runtimeDebugCdc.print(F(FIRMWARE_VERSION_STRING));
  runtimeDebugCdc.print(F(" TAG="));
  runtimeDebugCdc.print(F(FIRMWARE_VERSION_TAG));
  runtimeDebugCdc.print(F(" HARDWARE=\""));
  runtimeDebugCdc.print(F(FIRMWARE_HARDWARE_STRING));
  runtimeDebugCdc.println(F("\""));

  runtimeDebugCdc.print(F("STATUS INPUT="));
  runtimeDebugCdc.print((int)deviceMode);
  runtimeDebugCdc.print(F(" SAVED_INPUT="));
  runtimeDebugCdc.print((int)savedDeviceMode);
  runtimeDebugCdc.print(F(" CONFIG_OUT="));
  runtimeDebugCdc.print((int)configuredOutputMode);
  runtimeDebugCdc.print(F(" RUNTIME_OUT="));
  runtimeDebugCdc.print((int)outputMode);
  runtimeDebugCdc.print(F(" WIN_OUT="));
  runtimeDebugCdc.print((int)menu_win_output);
  runtimeDebugCdc.print(F(" MISTER_OUT="));
  runtimeDebugCdc.print((int)menu_mister_output);
  runtimeDebugCdc.print(F(" AUTO_STATE="));
  runtimeDebugCdc.print((int)autoDetectState);
  runtimeDebugCdc.print(F(" PLAYERS="));
  runtimeDebugCdc.print((int)max_devices);
  runtimeDebugCdc.println();

  runtimeDebugCdc.print(F("STATUS CONFIG_NAME="));
  runtimeDebugCdc.print(get_mode_name(configuredOutputMode));
  runtimeDebugCdc.print(F(" RUNTIME_NAME="));
  runtimeDebugCdc.println(get_mode_name(outputMode));

#ifdef ENABLE_INPUT_AUTODETECT
  runtimeDebugCdc.print(F("AUTORESOLVE IN="));
  runtimeDebugCdc.print((int)input_auto_resolve_mode);
  runtimeDebugCdc.print(F(" INSRC="));
  runtimeDebugCdc.print((int)input_auto_resolve_source);
  runtimeDebugCdc.print(F(" OUT="));
  runtimeDebugCdc.print((int)outputMode);
  runtimeDebugCdc.print(F(" OUTSTATE="));
  runtimeDebugCdc.print((int)autoDetectState);
  runtimeDebugCdc.print(F(" PROBE="));
  runtimeDebugCdc.print((int)autoDetectProbeStage);
  runtimeDebugCdc.print(F(" TRANS="));
  runtimeDebugCdc.print((int)output_autodetect_transition_reason);
  runtimeDebugCdc.print(F(" MS="));
  runtimeDebugCdc.print((int)output_autodetect_last_ms);
  runtimeDebugCdc.print(F(" FLAGS=0x"));
  runtimeDebugCdc.print((int)output_autodetect_last_flags, HEX);
  runtimeDebugCdc.print(F(" AUX=0x"));
  runtimeDebugCdc.println((int)output_autodetect_aux_flags, HEX);
#endif

  const UsbDeviceRuntimeDiagnostics& usbDiag = usbDeviceRuntimeDiagnostics();
  runtimeDebugCdc.print(F("STATUS UPTIME_MS="));
  runtimeDebugCdc.print(millis());
#if defined(ARDUINO_ARCH_RP2040)
  runtimeDebugCdc.print(F(" WATCHDOG_REBOOT="));
  runtimeDebugCdc.print(watchdog_caused_reboot() ? 1 : 0);
#endif
  runtimeDebugCdc.print(F(" USB_TASKS="));
  runtimeDebugCdc.print(usbDiag.task_count);
  runtimeDebugCdc.print(F(" USB_GAP_US="));
  runtimeDebugCdc.print(usbDiag.last_task_gap_us);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(usbDiag.max_task_gap_us);
  runtimeDebugCdc.print(F(" USB_MOUNT="));
  runtimeDebugCdc.print(usbDiag.mount_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(usbDiag.umount_count);
  runtimeDebugCdc.print(F(" USB_SUSP="));
  runtimeDebugCdc.print(usbDiag.suspend_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(usbDiag.resume_count);
  runtimeDebugCdc.print(F(" USB_WAKE="));
  runtimeDebugCdc.print(usbDiag.remote_wakeup_armed);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.println(usbDiag.remote_wakeup_count);

#ifdef ENABLE_INPUT_DREAMCAST
  const DreamcastVmuSerialStats& vmuStats = dreamcastVmuSerialStats();
  runtimeDebugCdc.print(F("STATUS VMU_READS="));
  runtimeDebugCdc.print(vmuStats.read_count);
  runtimeDebugCdc.print(F(" OK="));
  runtimeDebugCdc.print(vmuStats.success_count);
  runtimeDebugCdc.print(F(" TO="));
  runtimeDebugCdc.print(vmuStats.timeout_count);
  runtimeDebugCdc.print(F(" TO_ATT="));
  runtimeDebugCdc.print(vmuStats.timeout_attempt_count);
  runtimeDebugCdc.print(F(" RETRY="));
  runtimeDebugCdc.print(vmuStats.retry_count);
  runtimeDebugCdc.print(F(" LAST="));
  runtimeDebugCdc.print((int)vmuStats.last_port);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)vmuStats.last_slot);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)vmuStats.last_block);
  runtimeDebugCdc.print(F(" RES="));
  runtimeDebugCdc.print((int)vmuStats.last_result);
  runtimeDebugCdc.print(F(" US="));
  runtimeDebugCdc.print(vmuStats.last_read_us);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.println(vmuStats.max_read_us);
#endif

  latencyTest.writeStatus(runtimeDebugCdc);
  adaptStateWriteStatus(runtimeDebugCdc);
  inputOverlayWriteStatus(runtimeDebugCdc);
  featureModulesAppendSerialState(runtimeDebugCdc);
#if defined(ENABLE_USB_AUTH_SIDECAR) || defined(ENABLE_INPUT_USB)
  printUsbHostSerialStatus();
#endif
#ifdef ENABLE_USB_AUTH_SIDECAR
  printAuthSidecarSerialStatus(runtimeDebugCdc);
#endif
}

void printRuntimeSerialIdentity() {
  runtimeDebugCdc.print(F("IDENTITY SCHEMA=1 PRODUCT=" PRODUCT_PROTOCOL_ID));
  runtimeDebugCdc.print(F(" MCU=RP2040 BASE_VID="));
  printHex16(ADAPT_PRODUCT_USB_VID);
  runtimeDebugCdc.print(F(" BASE_PID="));
  printHex16(ADAPT_PRODUCT_USB_PID);
  runtimeDebugCdc.print(F(" UID="));
  pico_unique_board_id_t boardId{};
  pico_get_unique_board_id(&boardId);
  for (size_t index = 0; index < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; ++index) {
    printHexByte(boardId.id[index]);
  }
  runtimeDebugCdc.println();
}

#if defined(ENABLE_USB_AUTH_SIDECAR) || defined(ENABLE_INPUT_USB)
void printUsbHostSerialStatus() {
#if !defined(ENABLE_RUNTIME_SERIAL_EXTENDED_USBHOST_STATUS)
  usb_host_service_write_compact_status(runtimeDebugCdc);
#else
  const UsbHostServiceStatus host = usb_host_service_status();

  runtimeDebugCdc.print(F("USBHOST AVAIL="));
  runtimeDebugCdc.print(host.available ? 1 : 0);
  runtimeDebugCdc.print(F(" STARTED="));
  runtimeDebugCdc.print(host.started ? 1 : 0);
  runtimeDebugCdc.print(F(" REQ="));
  runtimeDebugCdc.print(host.start_requested ? 1 : 0);
  runtimeDebugCdc.print(F(" TUH="));
  runtimeDebugCdc.print(host.tuh_inited ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(host.tuh_mounted_addr1 ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(host.tuh_ready_addr1 ? 1 : 0);
  runtimeDebugCdc.print(F(" A1="));
  runtimeDebugCdc.print(host.addr1_vid_pid_valid ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(host.addr1_desc_valid ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(host.addr1_vid, HEX);
  runtimeDebugCdc.print(':');
  runtimeDebugCdc.print(host.addr1_pid, HEX);
  runtimeDebugCdc.print(F(" CLS="));
  runtimeDebugCdc.print((int)host.addr1_class);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)host.addr1_subclass);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)host.addr1_protocol);
  runtimeDebugCdc.print(F(" EP0="));
  runtimeDebugCdc.print((int)host.addr1_ep0_size);
  runtimeDebugCdc.print(F(" CFG="));
  runtimeDebugCdc.print((int)host.addr1_config_count);
  runtimeDebugCdc.print(F(" STR="));
  runtimeDebugCdc.print((int)host.addr1_manufacturer_index);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)host.addr1_product_index);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)host.addr1_serial_index);
  runtimeDebugCdc.print(F(" BCD="));
  runtimeDebugCdc.print(host.addr1_bcd_usb, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(host.addr1_bcd_device, HEX);
  runtimeDebugCdc.print(F(" HID="));
  runtimeDebugCdc.print((int)host.hid_total_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)host.hid_addr1_count);
  runtimeDebugCdc.print(F(" RHP="));
  runtimeDebugCdc.print((int)host.rhport);
  runtimeDebugCdc.print(F(" DP="));
  runtimeDebugCdc.print((int)host.dp_pin);
  runtimeDebugCdc.print(F(" DM="));
  runtimeDebugCdc.print((int)host.dm_pin);
  runtimeDebugCdc.print(F(" PINOUT="));
  runtimeDebugCdc.print(host.pinout == 1 ? F("DMDP") : F("DPDM"));
  runtimeDebugCdc.print(F(" RAW="));
  runtimeDebugCdc.print((int)host.raw_dp_level);
  runtimeDebugCdc.print((int)host.raw_dm_level);
  runtimeDebugCdc.print(F(" PHY="));
  runtimeDebugCdc.print((int)host.physical_dp_level);
  runtimeDebugCdc.print((int)host.physical_dm_level);
  runtimeDebugCdc.print(F(" LINE="));
  if (host.pio_line_state == 1) {
    runtimeDebugCdc.print(F("FS"));
  } else if (host.pio_line_state == 2) {
    runtimeDebugCdc.print(F("LS"));
  } else if (host.pio_line_state == 0) {
    runtimeDebugCdc.print(F("SE0"));
  } else {
    runtimeDebugCdc.print(F("SE1"));
  }
  runtimeDebugCdc.print(F(" ROOT="));
  runtimeDebugCdc.print(host.root_initialized ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(host.root_connected ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(host.root_suspended ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(host.root_fullspeed ? 1 : 0);
  runtimeDebugCdc.print(F(" REVT="));
  runtimeDebugCdc.print((int)host.root_event);
  runtimeDebugCdc.print(F(" RINT="));
  runtimeDebugCdc.print(host.root_ints, HEX);
  runtimeDebugCdc.print(F(" EP="));
  runtimeDebugCdc.print(host.root_ep_complete, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(host.root_ep_error, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(host.root_ep_stalled, HEX);
  runtimeDebugCdc.print(F(" RF="));
  runtimeDebugCdc.print((int)host.root_failed_count);
  runtimeDebugCdc.print(F(" RDEV="));
  runtimeDebugCdc.print(host.root_device_present ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(host.root_device_connected ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(host.root_device_enumerated ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(host.root_device_fullspeed ? 1 : 0);
  runtimeDebugCdc.print(F(" DEV="));
  runtimeDebugCdc.print((int)host.root_device_address);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(host.root_device_vid, HEX);
  runtimeDebugCdc.print(':');
  runtimeDebugCdc.print(host.root_device_pid, HEX);
  runtimeDebugCdc.print(F(" DEVT="));
  runtimeDebugCdc.print((int)host.root_device_event);
  runtimeDebugCdc.print(F(" KICK="));
  runtimeDebugCdc.print((int)host.attach_kick_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(host.attach_kick_age_ms);
  runtimeDebugCdc.print(F(" TXPIO="));
  runtimeDebugCdc.print((int)host.tx_pio);
  runtimeDebugCdc.print(F(" TXSM="));
  runtimeDebugCdc.print((int)host.tx_sm);
  runtimeDebugCdc.print(F(" TXDMA="));
  runtimeDebugCdc.print((int)host.tx_dma);
  runtimeDebugCdc.print(F(" RXPIO="));
  runtimeDebugCdc.print((int)host.rx_pio);
  runtimeDebugCdc.print(F(" RXSM="));
  runtimeDebugCdc.print((int)host.rx_sm);
  runtimeDebugCdc.print(F(" EOPSM="));
  runtimeDebugCdc.print((int)host.eop_sm);
  runtimeDebugCdc.print(F(" TASKS="));
  runtimeDebugCdc.println(host.task_count);
  runtimeDebugCdc.print(F("USBHOST CFG HUB="));
  runtimeDebugCdc.print((int)CFG_TUH_HUB);
  runtimeDebugCdc.print(F(" DEV="));
  runtimeDebugCdc.print((int)CFG_TUH_DEVICE_MAX);
  runtimeDebugCdc.print(F(" HID="));
  runtimeDebugCdc.print((int)CFG_TUH_HID);
  runtimeDebugCdc.print(F(" PIODEV="));
  runtimeDebugCdc.print((int)PIO_USB_DEVICE_CNT);
  runtimeDebugCdc.print(F(" PIOEP="));
  runtimeDebugCdc.println((int)PIO_USB_EP_POOL_CNT);
  for (uint8_t slot = 0; slot < CFG_TUH_HUB; ++slot) {
    runtimeDebugCdc.print(F("USBHUB SLOT="));
    runtimeDebugCdc.print((int)slot);
    runtimeDebugCdc.print(F(" ADDR="));
    runtimeDebugCdc.print(adapt_tinyusb_hub_debug_word(slot, 0));
    runtimeDebugCdc.print(F(" PORTS="));
    runtimeDebugCdc.print(adapt_tinyusb_hub_debug_word(slot, 1));
    runtimeDebugCdc.print(F(" STAT="));
    runtimeDebugCdc.print(adapt_tinyusb_hub_debug_word(slot, 2));
    runtimeDebugCdc.print('/');
    runtimeDebugCdc.print(adapt_tinyusb_hub_debug_word(slot, 3));
    runtimeDebugCdc.print(F(" RES="));
    runtimeDebugCdc.print(adapt_tinyusb_hub_debug_word(slot, 4));
    runtimeDebugCdc.print(F(" CHG="));
    runtimeDebugCdc.print(adapt_tinyusb_hub_debug_word(slot, 5), HEX);
    runtimeDebugCdc.print(F(" GET="));
    runtimeDebugCdc.print(adapt_tinyusb_hub_debug_word(slot, 6));
    runtimeDebugCdc.print(F(" P="));
    runtimeDebugCdc.print(adapt_tinyusb_hub_debug_word(slot, 7));
    runtimeDebugCdc.print(F(" PS="));
    runtimeDebugCdc.print(adapt_tinyusb_hub_debug_word(slot, 8), HEX);
    runtimeDebugCdc.print('/');
    runtimeDebugCdc.println(adapt_tinyusb_hub_debug_word(slot, 9), HEX);
  }

  const UsbHostCallbackStatus hostCb = usb_host_callback_status();
  runtimeDebugCdc.print(F("USBHCB MOUNT="));
  runtimeDebugCdc.print(hostCb.device_mount_count);
  runtimeDebugCdc.print(F(" UMOUNT="));
  runtimeDebugCdc.print(hostCb.device_umount_count);
  runtimeDebugCdc.print(F(" CFGD="));
  runtimeDebugCdc.print(hostCb.config_desc_count);
  runtimeDebugCdc.print(F(" HIDM="));
  runtimeDebugCdc.print(hostCb.hid_mount_count);
  runtimeDebugCdc.print(F(" HIDU="));
  runtimeDebugCdc.print(hostCb.hid_umount_count);
  runtimeDebugCdc.print(F(" HIDR="));
  runtimeDebugCdc.print(hostCb.hid_report_count);
  runtimeDebugCdc.print(F(" XIM="));
  runtimeDebugCdc.print(hostCb.xinput_mount_count);
  runtimeDebugCdc.print(F(" XIR="));
  runtimeDebugCdc.print(hostCb.xinput_report_count);
  runtimeDebugCdc.print(F(" LAST="));
  runtimeDebugCdc.print(hostCb.last_vid, HEX);
  runtimeDebugCdc.print(':');
  runtimeDebugCdc.print(hostCb.last_pid, HEX);
  runtimeDebugCdc.print(F(" A="));
  runtimeDebugCdc.print((int)hostCb.last_dev_addr);
  runtimeDebugCdc.print(F(" I="));
  runtimeDebugCdc.print((int)hostCb.last_instance);
  runtimeDebugCdc.print(F(" DESC="));
  runtimeDebugCdc.print(hostCb.last_hid_desc_len);
  runtimeDebugCdc.print(F(" LEN="));
  runtimeDebugCdc.print((int)hostCb.last_report_len);
  runtimeDebugCdc.print(F(" CFG="));
  runtimeDebugCdc.print((int)hostCb.last_config_index);
  runtimeDebugCdc.print(F(" ITF="));
  runtimeDebugCdc.print((int)hostCb.last_itf_class);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)hostCb.last_itf_subclass);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)hostCb.last_itf_protocol);
  runtimeDebugCdc.print('#');
  runtimeDebugCdc.print((int)hostCb.last_itf_number);
  runtimeDebugCdc.print(F(" BTHCI="));
  runtimeDebugCdc.print(hostCb.bluetooth_hci_detect_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(hostCb.bluetooth_hci_vid, HEX);
  runtimeDebugCdc.print(':');
  runtimeDebugCdc.print(hostCb.bluetooth_hci_pid, HEX);
  runtimeDebugCdc.print(F(" A="));
  runtimeDebugCdc.println((int)hostCb.bluetooth_hci_addr);

  runtimeDebugCdc.print(F("USBHITF COUNT="));
  runtimeDebugCdc.print((int)hostCb.interface_snapshot_count);
  for (uint8_t index = 0;
       index < hostCb.interface_snapshot_count &&
       index < USB_HOST_INTERFACE_SNAPSHOT_COUNT;
       ++index) {
    const UsbHostInterfaceSnapshot& itf = hostCb.interfaces[index];
    runtimeDebugCdc.print(F(" ["));
    runtimeDebugCdc.print((int)itf.number);
    runtimeDebugCdc.print(':');
    runtimeDebugCdc.print((int)itf.cls);
    runtimeDebugCdc.print('/');
    runtimeDebugCdc.print((int)itf.subclass);
    runtimeDebugCdc.print('/');
    runtimeDebugCdc.print((int)itf.protocol);
    runtimeDebugCdc.print(F(" EP"));
    runtimeDebugCdc.print((int)itf.endpoint_count);
    runtimeDebugCdc.print(']');
  }
  runtimeDebugCdc.println();

#if defined(ENABLE_USB_XINPUT_HOST) || defined(ENABLE_USB_XINPUT_AUTH_HOST)
  const UsbXInputHostStatus xinputHost = usb_xinput_host_status();
  runtimeDebugCdc.print(F("XINPUTHOST SUP="));
  runtimeDebugCdc.print(xinputHost.supported ? 1 : 0);
  runtimeDebugCdc.print(F(" MNT="));
  runtimeDebugCdc.print(xinputHost.mounted ? 1 : 0);
  runtimeDebugCdc.print(F(" RDY="));
  runtimeDebugCdc.print(xinputHost.ready ? 1 : 0);
  runtimeDebugCdc.print(F(" DEV="));
  runtimeDebugCdc.print((int)xinputHost.dev_addr);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)xinputHost.instance);
  runtimeDebugCdc.print(F(" TYPE="));
  runtimeDebugCdc.print((int)xinputHost.type);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)xinputHost.subtype);
  runtimeDebugCdc.print(F(" ITF="));
  runtimeDebugCdc.print((int)xinputHost.itf_num);
  runtimeDebugCdc.print(F(" EP="));
  runtimeDebugCdc.print(xinputHost.ep_in, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.ep_out, HEX);
  runtimeDebugCdc.print(F(" SZ="));
  runtimeDebugCdc.print(xinputHost.ep_in_size);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.ep_out_size);
  runtimeDebugCdc.print(F(" VID=0x"));
  runtimeDebugCdc.print(xinputHost.vid, HEX);
  runtimeDebugCdc.print(F(" PID=0x"));
  runtimeDebugCdc.print(xinputHost.pid, HEX);
  runtimeDebugCdc.print(F(" PROBE="));
  runtimeDebugCdc.print(xinputHost.probe_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.last_probe_class, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.last_probe_subclass, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.last_probe_protocol, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.last_probe_endpoints);
  runtimeDebugCdc.print(F(" MATCH="));
  runtimeDebugCdc.print(xinputHost.matched_probe_count);
  runtimeDebugCdc.print(F(" FAIL="));
  runtimeDebugCdc.print(xinputHost.open_fail_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)xinputHost.last_open_error);
  runtimeDebugCdc.print(F(" CNT="));
  runtimeDebugCdc.print(xinputHost.open_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.mount_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.close_count);
  runtimeDebugCdc.print(F(" IO="));
  runtimeDebugCdc.print(xinputHost.input_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.output_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.output_fail_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.receive_requeue_fail_count);
  runtimeDebugCdc.print(F(" MAGIC="));
  runtimeDebugCdc.print(xinputHost.magic_submit_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.magic_success_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.magic_fail_count);
  runtimeDebugCdc.print(F(" XGIP="));
  runtimeDebugCdc.print(xinputHost.xgip_init_queue_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.xgip_init_queue_drop_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.xgip_ack_count);
  runtimeDebugCdc.print(F(" XONER="));
  runtimeDebugCdc.print(xinputHost.xgip_rumble_request_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.xgip_rumble_queue_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.xgip_rumble_success_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.xgip_rumble_fail_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)xinputHost.xgip_rumble_left);
  runtimeDebugCdc.print(':');
  runtimeDebugCdc.print((int)xinputHost.xgip_rumble_right);
  runtimeDebugCdc.print(F(" X360W="));
  runtimeDebugCdc.print(xinputHost.wireless_presence_submit_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.wireless_presence_fail_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.wireless_input_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.wireless_disconnect_count);
  runtimeDebugCdc.print(F(" XONEW="));
  runtimeDebugCdc.print(xinputHost.xbox_wireless_adapter_seen ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.xbox_wireless_adapter_vid, HEX);
  runtimeDebugCdc.print(':');
  runtimeDebugCdc.print(xinputHost.xbox_wireless_adapter_pid, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.xbox_wireless_adapter_probe_count);
  runtimeDebugCdc.print(F(" SW2="));
  runtimeDebugCdc.print(xinputHost.switch2_wake_submit_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.switch2_wake_success_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.switch2_wake_fail_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)xinputHost.switch2_wake_command);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.switch2_wake_ready ? 1 : 0);
  runtimeDebugCdc.print(F(" SW2FB="));
  runtimeDebugCdc.print((int)xinputHost.switch2_feedback_init_step);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.switch2_feedback_init_success_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.switch2_feedback_init_fail_count);
  runtimeDebugCdc.print(F(" SW2R="));
  runtimeDebugCdc.print(xinputHost.switch2_rumble_success_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(xinputHost.switch2_rumble_fail_count);
  runtimeDebugCdc.print(F(" LAST="));
  runtimeDebugCdc.print((int)xinputHost.last_report_len);
  runtimeDebugCdc.print('/');
  const uint8_t xinputRawLength =
      min(xinputHost.last_report_len,
          static_cast<uint8_t>(sizeof(xinputHost.last_report_head)));
  for (uint8_t i = 0; i < xinputRawLength; ++i) {
    if (xinputHost.last_report_head[i] < 0x10) {
      runtimeDebugCdc.print('0');
    }
    runtimeDebugCdc.print(xinputHost.last_report_head[i], HEX);
  }
  runtimeDebugCdc.println();
  runtimeDebugCdc.print(F("XINPUTHOST PH="));
  runtimeDebugCdc.print((int)xinputHost.probe_history_count);
  for (uint8_t i = 0; i < xinputHost.probe_history_count; ++i) {
    const uint8_t index = xinputHost.probe_history_count <
                                  USB_XINPUT_HOST_PROBE_HISTORY
        ? i
        : static_cast<uint8_t>(
              (xinputHost.probe_history_next + i) %
              USB_XINPUT_HOST_PROBE_HISTORY);
    const UsbXInputHostProbe& probe = xinputHost.probe_history[index];
    runtimeDebugCdc.print(F(" ["));
    runtimeDebugCdc.print((int)probe.dev_addr);
    runtimeDebugCdc.print('/');
    runtimeDebugCdc.print((int)probe.itf_num);
    runtimeDebugCdc.print(' ');
    runtimeDebugCdc.print(probe.vid, HEX);
    runtimeDebugCdc.print(':');
    runtimeDebugCdc.print(probe.pid, HEX);
    runtimeDebugCdc.print(' ');
    runtimeDebugCdc.print(probe.cls, HEX);
    runtimeDebugCdc.print('/');
    runtimeDebugCdc.print(probe.subclass, HEX);
    runtimeDebugCdc.print('/');
    runtimeDebugCdc.print(probe.protocol, HEX);
    runtimeDebugCdc.print('/');
    runtimeDebugCdc.print((int)probe.endpoints);
    runtimeDebugCdc.print(F(" M"));
    runtimeDebugCdc.print((int)probe.match_type);
    runtimeDebugCdc.print(F(" R"));
    runtimeDebugCdc.print((int)probe.result);
    runtimeDebugCdc.print(']');
  }
  runtimeDebugCdc.println();
#endif

#ifdef ENABLE_USB_BT_HCI
  const UsbBtHciStatus btHci = usb_bt_hci_status();
  runtimeDebugCdc.print(F("BTHCI SUP="));
  runtimeDebugCdc.print(btHci.supported ? 1 : 0);
  runtimeDebugCdc.print(F(" OPEN="));
  runtimeDebugCdc.print(btHci.opened ? 1 : 0);
  runtimeDebugCdc.print(F(" MNT="));
  runtimeDebugCdc.print(btHci.mounted ? 1 : 0);
  runtimeDebugCdc.print(F(" RDY="));
  runtimeDebugCdc.print(btHci.ready ? 1 : 0);
  runtimeDebugCdc.print(F(" PEND="));
  runtimeDebugCdc.print(btHci.command_pending ? 1 : 0);
  runtimeDebugCdc.print(F(" PH="));
  runtimeDebugCdc.print((int)btHci.phase);
  runtimeDebugCdc.print(F(" DEV="));
  runtimeDebugCdc.print((int)btHci.dev_addr);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.vid, HEX);
  runtimeDebugCdc.print(':');
  runtimeDebugCdc.print(btHci.pid, HEX);
  runtimeDebugCdc.print(F(" ITF="));
  runtimeDebugCdc.print((int)btHci.interface_number);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.interface_alt);
  runtimeDebugCdc.print(F(" EP="));
  runtimeDebugCdc.print(btHci.event_ep, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.event_size);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.event_interval);
  runtimeDebugCdc.print(' ');
  runtimeDebugCdc.print(btHci.acl_in_ep, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.acl_in_size);
  runtimeDebugCdc.print(' ');
  runtimeDebugCdc.print(btHci.acl_out_ep, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.acl_out_size);
  runtimeDebugCdc.print(F(" CNT="));
  runtimeDebugCdc.print(btHci.open_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.mount_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.umount_count);
  runtimeDebugCdc.print(F(" EVT="));
  runtimeDebugCdc.print(btHci.event_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.event_requeue_fail_count);
  runtimeDebugCdc.print(F(" ACL="));
  runtimeDebugCdc.print(btHci.acl_in_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.acl_out_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.acl_in_parse_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.acl_in_requeue_fail_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.acl_out_fail_count);
  runtimeDebugCdc.print(F(" CMD="));
  runtimeDebugCdc.print(btHci.cmd_submit_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.cmd_complete_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.cmd_fail_count);
  runtimeDebugCdc.print(F(" PAIR="));
  runtimeDebugCdc.print(btHci.pairing_request_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.pairing_not_ready_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.pairing_not_implemented_count);
  runtimeDebugCdc.print(F(" INQ="));
  runtimeDebugCdc.print(btHci.inquiry_active ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.inquiry_start_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.inquiry_result_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.inquiry_complete_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.inquiry_fail_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.last_inquiry_status);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.last_inquiry_rssi);
  runtimeDebugCdc.print(F(" CON="));
  runtimeDebugCdc.print(btHci.connection_pending ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.connected ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.connect_request_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.connect_submit_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.connect_complete_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.connect_fail_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.connect_timeout_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.recovery_reset_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.connection_pending_ms);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.disconnect_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.last_connect_status);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.connection_handle, HEX);
  runtimeDebugCdc.print(F("/D"));
  runtimeDebugCdc.print(btHci.last_disconnect_handle, HEX);
  runtimeDebugCdc.print(':');
  runtimeDebugCdc.print((int)btHci.last_disconnect_status, HEX);
  runtimeDebugCdc.print(':');
  runtimeDebugCdc.print((int)btHci.last_disconnect_reason, HEX);
  runtimeDebugCdc.print(F(" BOND="));
  runtimeDebugCdc.print(btHci.bond_valid ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.bond_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.active_bond_slot);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.reconnect_attempt_slot);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.bond_store_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.bond_unchanged_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.bond_persist_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.bond_persist_fail_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.bond_persist_pending ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.auto_connect_request_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.auto_connect_submit_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.auto_connect_fail_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.auto_connect_success_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.bond_key_type);
  runtimeDebugCdc.print(F(" CT="));
  runtimeDebugCdc.print((int)btHci.bond_controller_type);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.active_controller_type);
  runtimeDebugCdc.print(F(" PAGE="));
  runtimeDebugCdc.print(btHci.page_scan_enabled ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.inquiry_scan_enabled ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.scan_enable);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.page_scan_request_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.page_scan_submit_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.page_scan_fail_count);
  runtimeDebugCdc.print(F(" INCON="));
  runtimeDebugCdc.print(btHci.incoming_connection_request_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.incoming_connection_accept_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.incoming_connection_reject_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.incoming_connection_busy_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.last_incoming_reject_reason);
  runtimeDebugCdc.print(F(" BADDR="));
  printBtAddress(runtimeDebugCdc, btHci.bond_addr);
  runtimeDebugCdc.print(F(" AUTHBT="));
  runtimeDebugCdc.print(btHci.link_key_request_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.link_key_reply_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.link_key_negative_reply_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.link_key_notification_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.pin_code_request_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.pin_code_reply_count);
  runtimeDebugCdc.print(F(" SSP="));
  runtimeDebugCdc.print(btHci.simple_pairing_enabled ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.simple_pairing_request_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.simple_pairing_submit_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.simple_pairing_fail_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.io_capability_request_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.io_capability_reply_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.user_confirmation_request_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.user_confirmation_reply_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.user_passkey_request_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.user_passkey_negative_reply_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.simple_pairing_complete_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.last_simple_pairing_status);
  runtimeDebugCdc.print(F(" RN="));
  runtimeDebugCdc.print(btHci.remote_name_request_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.remote_name_submit_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.remote_name_fail_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.remote_name_complete_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.last_remote_name_status);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.remote_name);
  runtimeDebugCdc.print(F(" DID="));
  runtimeDebugCdc.print(btHci.sdp_device_id_valid ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.sdp_device_id_vendor_id, HEX);
  runtimeDebugCdc.print(':');
  runtimeDebugCdc.print(btHci.sdp_device_id_product_id, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.sdp_device_id_version, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.sdp_device_id_vendor_source, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.sdp_device_id_query_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.sdp_device_id_connect_submit_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.sdp_device_id_request_submit_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.sdp_device_id_response_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.sdp_device_id_fail_count);
  runtimeDebugCdc.print(F(" AUTH="));
  runtimeDebugCdc.print(btHci.auth_request_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.auth_request_submit_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.auth_complete_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.last_auth_status);
  runtimeDebugCdc.print(F(" ENC="));
  runtimeDebugCdc.print(btHci.encrypt_request_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.encrypt_request_submit_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.encrypt_complete_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.last_encrypt_status);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.encryption_enabled);
  runtimeDebugCdc.print(F(" L2="));
  runtimeDebugCdc.print(btHci.l2cap_control_pending ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.l2cap_control_open ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.l2cap_control_request_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.l2cap_control_submit_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.l2cap_control_response_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.l2cap_control_fail_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.l2cap_control_local_cid, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.l2cap_control_remote_cid, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.l2cap_control_result);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.l2cap_control_status);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.l2cap_last_code, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.l2cap_last_identifier);
  runtimeDebugCdc.print(F(" L2I="));
  runtimeDebugCdc.print(btHci.l2cap_interrupt_pending ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.l2cap_interrupt_open ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.l2cap_interrupt_request_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.l2cap_interrupt_submit_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.l2cap_interrupt_response_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.l2cap_interrupt_fail_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.l2cap_interrupt_local_cid, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.l2cap_interrupt_remote_cid, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.l2cap_interrupt_result);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.l2cap_interrupt_status);
  runtimeDebugCdc.print(F(" CFG="));
  runtimeDebugCdc.print(btHci.l2cap_config_request_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.l2cap_config_response_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.l2cap_config_submit_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.l2cap_last_psm, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.l2cap_last_scid, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.l2cap_last_dcid, HEX);
  runtimeDebugCdc.print(F(" L2D="));
  runtimeDebugCdc.print(btHci.l2cap_data_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.l2cap_unknown_data_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.acl_in_short_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.l2cap_last_channel_id, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.l2cap_last_len);
  runtimeDebugCdc.print('/');
  for (uint8_t i = 0; i < sizeof(btHci.l2cap_last_data); ++i) {
    if (btHci.l2cap_last_data[i] < 16) {
      runtimeDebugCdc.print('0');
    }
    runtimeDebugCdc.print(btHci.l2cap_last_data[i], HEX);
  }
  runtimeDebugCdc.print(F(" ACLRAW="));
  runtimeDebugCdc.print(btHci.acl_in_last_len);
  runtimeDebugCdc.print('/');
  for (uint8_t i = 0; i < sizeof(btHci.acl_in_last_data); ++i) {
    if (btHci.acl_in_last_data[i] < 16) {
      runtimeDebugCdc.print('0');
    }
    runtimeDebugCdc.print(btHci.acl_in_last_data[i], HEX);
  }
  runtimeDebugCdc.print(F(" REASM="));
  runtimeDebugCdc.print(btHci.acl_reassembly_start_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.acl_reassembly_continue_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.acl_reassembly_complete_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.acl_reassembly_orphan_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.acl_reassembly_overflow_count);
  runtimeDebugCdc.print(F(" HIDBT="));
  runtimeDebugCdc.print(btHci.hid_control_data_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.hid_interrupt_data_count);
  runtimeDebugCdc.print('(');
  runtimeDebugCdc.print(btHci.hid_link_interrupt_data_count);
  runtimeDebugCdc.print('@');
  runtimeDebugCdc.print(btHci.hid_link_start_interrupt_count);
  runtimeDebugCdc.print(')');
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.hid_init_submit_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.hid_init_fail_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.hid_init_skip_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.hid_output_submit_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.hid_output_fail_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.hid_last_cid, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.hid_last_len);
  runtimeDebugCdc.print('/');
  for (uint8_t i = 0; i < sizeof(btHci.hid_last_report); ++i) {
    if (btHci.hid_last_report[i] < 16) {
      runtimeDebugCdc.print('0');
    }
    runtimeDebugCdc.print(btHci.hid_last_report[i], HEX);
  }
  runtimeDebugCdc.print(F(" HSTALL="));
  runtimeDebugCdc.print(btHci.hid_stream_stall_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.hid_stream_recover_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.hid_stream_disconnect_count);
  runtimeDebugCdc.print('/');
  if (btHci.hid_last_interrupt_ms == 0) {
    runtimeDebugCdc.print(0);
  } else {
    runtimeDebugCdc.print((unsigned long)(millis() - btHci.hid_last_interrupt_ms));
  }
  runtimeDebugCdc.print(F(" ED="));
  runtimeDebugCdc.print(btHci.endpoint_candidate_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.endpoint_open_fail_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.missing_endpoint_fail_count);
  runtimeDebugCdc.print(F(" EF="));
  runtimeDebugCdc.print((int)btHci.last_open_fail_code);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.last_open_fail_interface);
  runtimeDebugCdc.print(F(" LEP="));
  runtimeDebugCdc.print(btHci.last_ep_addr, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.last_ep_attr);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.last_ep_dir);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.last_ep_xfer);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.last_ep_size);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.last_ep_interval);
  runtimeDebugCdc.print(F(" CAND="));
  runtimeDebugCdc.print(btHci.candidate_event_ep, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.candidate_acl_in_ep, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.candidate_acl_out_ep, HEX);
  runtimeDebugCdc.print(F(" LAST="));
  runtimeDebugCdc.print(btHci.last_opcode, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.last_status);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.last_event_code);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.last_event_len);
  runtimeDebugCdc.print(F(" VER="));
  runtimeDebugCdc.print((int)btHci.hci_version);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.hci_revision, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.lmp_version);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.manufacturer, HEX);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.lmp_subversion, HEX);
  runtimeDebugCdc.print(F(" BD="));
  for (int i = 5; i >= 0; --i) {
    if (btHci.bd_addr[i] < 16) {
      runtimeDebugCdc.print('0');
    }
    runtimeDebugCdc.print(btHci.bd_addr[i], HEX);
    if (i > 0) {
      runtimeDebugCdc.print(':');
    }
  }
  runtimeDebugCdc.print(F(" REM="));
  for (int i = 5; i >= 0; --i) {
    if (btHci.last_remote_addr[i] < 16) {
      runtimeDebugCdc.print('0');
    }
    runtimeDebugCdc.print(btHci.last_remote_addr[i], HEX);
    if (i > 0) {
      runtimeDebugCdc.print(':');
    }
  }
  runtimeDebugCdc.print(F(" COD="));
  runtimeDebugCdc.print(btHci.last_remote_class[2], HEX);
  runtimeDebugCdc.print(':');
  runtimeDebugCdc.print(btHci.last_remote_class[1], HEX);
  runtimeDebugCdc.print(':');
  runtimeDebugCdc.print(btHci.last_remote_class[0], HEX);
  runtimeDebugCdc.print(F(" TGT="));
  printBtAddress(runtimeDebugCdc, btHci.connect_addr);
  runtimeDebugCdc.print(F(" BLE="));
  runtimeDebugCdc.print(btHci.le_event_mask_configured ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.le_scan_params_configured ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.le_scan_enabled ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.le_event_mask_request_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.le_scan_param_request_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.le_scan_enable_request_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.le_adv_report_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.last_le_status);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.last_le_subevent, HEX);
  runtimeDebugCdc.print(F(" LEADDR="));
  for (int i = 5; i >= 0; --i) {
    if (btHci.last_le_addr[i] < 16) {
      runtimeDebugCdc.print('0');
    }
    runtimeDebugCdc.print(btHci.last_le_addr[i], HEX);
    if (i > 0) {
      runtimeDebugCdc.print(':');
    }
  }
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.last_le_addr_type);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.last_le_event_type);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.last_le_rssi);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.last_le_adv_len);
  runtimeDebugCdc.print('/');
  for (uint8_t i = 0; i < sizeof(btHci.last_le_adv_data); ++i) {
    if (btHci.last_le_adv_data[i] < 16) {
      runtimeDebugCdc.print('0');
    }
    runtimeDebugCdc.print(btHci.last_le_adv_data[i], HEX);
  }
  runtimeDebugCdc.print(F(" LENAME="));
  runtimeDebugCdc.print(btHci.last_le_name);
  runtimeDebugCdc.print(F(" LETYPE="));
  runtimeDebugCdc.print((int)btHci.last_le_controller_type);
  runtimeDebugCdc.print(F(" LECTRL="));
  runtimeDebugCdc.print(btHci.le_controller_seen_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.last_le_seen_controller_type);
  runtimeDebugCdc.print('/');
  for (int i = 5; i >= 0; --i) {
    if (btHci.last_le_controller_addr[i] < 16) {
      runtimeDebugCdc.print('0');
    }
    runtimeDebugCdc.print(btHci.last_le_controller_addr[i], HEX);
    if (i > 0) {
      runtimeDebugCdc.print(':');
    }
  }
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.last_le_controller_addr_type);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.last_le_controller_name);
  runtimeDebugCdc.print(F(" LECON="));
  runtimeDebugCdc.print(btHci.le_connection_pending ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.le_connected ? 1 : 0);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.le_connect_request_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.le_connect_submit_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.le_connect_complete_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.le_connect_fail_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)btHci.last_le_connect_status);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(btHci.le_connection_handle, HEX);
  runtimeDebugCdc.println();

  if (btHci.inquiry_slot_count > 0) {
    runtimeDebugCdc.print(F("BTDEV COUNT="));
    runtimeDebugCdc.print((int)btHci.inquiry_slot_count);
    for (uint8_t slotIndex = 0;
         slotIndex < btHci.inquiry_slot_count &&
         slotIndex < USB_BT_HCI_INQUIRY_SLOT_COUNT;
         ++slotIndex) {
      const UsbBtInquiryDevice& device = btHci.inquiry_slots[slotIndex];
      runtimeDebugCdc.print(F(" ["));
      runtimeDebugCdc.print((int)slotIndex);
      runtimeDebugCdc.print(':');
      for (int i = 5; i >= 0; --i) {
        if (device.addr[i] < 16) {
          runtimeDebugCdc.print('0');
        }
        runtimeDebugCdc.print(device.addr[i], HEX);
        if (i > 0) {
          runtimeDebugCdc.print(':');
        }
      }
      runtimeDebugCdc.print(F(" COD="));
      runtimeDebugCdc.print(device.class_of_device[2], HEX);
      runtimeDebugCdc.print(':');
      runtimeDebugCdc.print(device.class_of_device[1], HEX);
      runtimeDebugCdc.print(':');
      runtimeDebugCdc.print(device.class_of_device[0], HEX);
      runtimeDebugCdc.print(F(" RSSI="));
      runtimeDebugCdc.print((int)device.rssi);
      runtimeDebugCdc.print(F(" PS="));
      runtimeDebugCdc.print((int)device.page_scan_repetition_mode);
      runtimeDebugCdc.print(F(" CLK="));
      runtimeDebugCdc.print(device.clock_offset, HEX);
      runtimeDebugCdc.print(F(" EV="));
      runtimeDebugCdc.print((int)device.event_code, HEX);
      runtimeDebugCdc.print(F(" TYPE="));
      runtimeDebugCdc.print((int)device.controller_type);
      runtimeDebugCdc.print(']');
    }
    runtimeDebugCdc.println();
  }
#endif

#ifdef ENABLE_INPUT_USB
  const usb_input_debug_status_t usbInput = inputusb_debug_status();
  runtimeDebugCdc.print(F("USBIN DEVCOUNT="));
  runtimeDebugCdc.print((int)get_usb_device_count());
  runtimeDebugCdc.print(F(" MOUNT="));
  runtimeDebugCdc.print(usbInput.device_mount_count);
  runtimeDebugCdc.print(F(" UMOUNT="));
  runtimeDebugCdc.print(usbInput.device_umount_count);
  runtimeDebugCdc.print(F(" HIDM="));
  runtimeDebugCdc.print(usbInput.hid_mount_count);
  runtimeDebugCdc.print(F(" HIDU="));
  runtimeDebugCdc.print(usbInput.hid_umount_count);
  runtimeDebugCdc.print(F(" RPT="));
  runtimeDebugCdc.print(usbInput.report_count);
  runtimeDebugCdc.print(F(" RQ="));
  runtimeDebugCdc.print(usbInput.report_request_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(usbInput.report_request_success_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(usbInput.report_request_fail_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)usbInput.last_report_request_ok);
  runtimeDebugCdc.print(F(" TX="));
  runtimeDebugCdc.print(usbInput.report_send_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(usbInput.report_send_success_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(usbInput.report_send_fail_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print((int)usbInput.last_report_send_ok);
  runtimeDebugCdc.print(F(" REC="));
  runtimeDebugCdc.print(usbInput.hid_recover_count);
  runtimeDebugCdc.print(F(" REAP="));
  runtimeDebugCdc.print(usbInput.stale_reap_count);
  runtimeDebugCdc.print(F(" REUSE="));
  runtimeDebugCdc.print(usbInput.mount_replace_count);
  runtimeDebugCdc.print(F(" SCAN="));
  runtimeDebugCdc.print(usbInput.mounted_scan_count);
  runtimeDebugCdc.print(F(" HC="));
  runtimeDebugCdc.print((int)usbInput.last_hid_count);
  runtimeDebugCdc.print(F(" LAST="));
  runtimeDebugCdc.print(usbInput.last_vid, HEX);
  runtimeDebugCdc.print(':');
  runtimeDebugCdc.print(usbInput.last_pid, HEX);
  runtimeDebugCdc.print(F(" A="));
  runtimeDebugCdc.print((int)usbInput.last_dev_addr);
  runtimeDebugCdc.print(F(" I="));
  runtimeDebugCdc.print((int)usbInput.last_instance);
  runtimeDebugCdc.print(F(" LEN="));
  runtimeDebugCdc.print((int)usbInput.last_report_len);
  runtimeDebugCdc.print(F(" RAW="));
  const uint8_t usbInputRawLength =
      min(usbInput.last_report_len,
          static_cast<uint8_t>(sizeof(usbInput.last_report_head)));
  for (uint8_t i = 0; i < usbInputRawLength; ++i) {
    if (usbInput.last_report_head[i] < 0x10) runtimeDebugCdc.print('0');
    runtimeDebugCdc.print(usbInput.last_report_head[i], HEX);
  }
  runtimeDebugCdc.println();
  runtimeDebugCdc.print(F("USBIN PS4RUMBLE="));
  runtimeDebugCdc.print(usbInput.ps4_rumble_submit_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(usbInput.ps4_rumble_success_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(usbInput.ps4_rumble_fail_count);
  runtimeDebugCdc.print(F(" LAST="));
  runtimeDebugCdc.print((int)usbInput.last_ps4_rumble_left);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.println((int)usbInput.last_ps4_rumble_right);
  runtimeDebugCdc.print(F("USBIN DS3PAIR="));
  runtimeDebugCdc.print(usbInput.ps3_pair_submit_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(usbInput.ps3_pair_success_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(usbInput.ps3_pair_fail_count);
  runtimeDebugCdc.print(F(" BUSY="));
  runtimeDebugCdc.print(usbInput.ps3_pair_busy_count);
  runtimeDebugCdc.print(F(" READ="));
  runtimeDebugCdc.print(usbInput.ps3_pair_read_count);
  runtimeDebugCdc.print(F(" VERIFY="));
  runtimeDebugCdc.print(usbInput.ps3_pair_verify_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(usbInput.ps3_pair_verify_fail_count);
  runtimeDebugCdc.print(F(" ADDR="));
  for (uint8_t i = 2; i < sizeof(usbInput.ps3_pair_readback); ++i) {
    if (i != 2) runtimeDebugCdc.print(':');
    if (usbInput.ps3_pair_readback[i] < 0x10) runtimeDebugCdc.print('0');
    runtimeDebugCdc.print(usbInput.ps3_pair_readback[i], HEX);
  }
  runtimeDebugCdc.println();
  runtimeDebugCdc.print(F("USBIN XUSB="));
  runtimeDebugCdc.print(usbInput.xusb_mount_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(usbInput.xusb_report_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(usbInput.xusb_umount_count);
  runtimeDebugCdc.print(F(" XGIP="));
  runtimeDebugCdc.print(usbInput.xgip_mount_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.print(usbInput.xgip_report_count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.println(usbInput.xgip_umount_count);
  for (uint8_t slot = 0; slot < MAX_USB_DEVICES; ++slot) {
    const usb_input_device_t* device = get_usb_device(slot);
    runtimeDebugCdc.print(F("USBIN SLOT="));
    runtimeDebugCdc.print((int)(slot + 1));
    runtimeDebugCdc.print(F(" CONN="));
    runtimeDebugCdc.print(device->connected ? 1 : 0);
    runtimeDebugCdc.print(F(" DEV="));
    runtimeDebugCdc.print((int)device->dev_addr);
    runtimeDebugCdc.print('/');
    runtimeDebugCdc.print((int)device->instance);
    runtimeDebugCdc.print(F(" VIDPID="));
    runtimeDebugCdc.print(device->vid, HEX);
    runtimeDebugCdc.print(':');
    runtimeDebugCdc.print(device->pid, HEX);
    runtimeDebugCdc.print(F(" BCD="));
    runtimeDebugCdc.print(device->bcd_device, HEX);
    runtimeDebugCdc.print(F(" ROOT="));
    runtimeDebugCdc.print((int)device->topology_root_port);
    runtimeDebugCdc.print(F(" DEPTH="));
    runtimeDebugCdc.print((int)device->topology_depth);
    runtimeDebugCdc.print(F(" KEY="));
    runtimeDebugCdc.print(device->topology_key, HEX);
    runtimeDebugCdc.print(F(" ORDER="));
    runtimeDebugCdc.print(device->connection_order);
    runtimeDebugCdc.print(F(" TYPE="));
    runtimeDebugCdc.print((int)device->report.CONFIG.DEVICE_TYPE);
    runtimeDebugCdc.print(F(" RPT="));
    runtimeDebugCdc.print(device->input_report_count);
    runtimeDebugCdc.print(F(" SWI="));
    runtimeDebugCdc.print((int)device->switch_init_state);
    runtimeDebugCdc.print('/');
    runtimeDebugCdc.print((int)device->switch_init_pending);
    runtimeDebugCdc.print('/');
    runtimeDebugCdc.print(device->switch_init_attempt_count);
    runtimeDebugCdc.print('/');
    runtimeDebugCdc.print(device->switch_init_fail_count);
    runtimeDebugCdc.print(F(" SWR="));
    runtimeDebugCdc.print((int)device->switch_rumble_enabled);
    runtimeDebugCdc.print('/');
    runtimeDebugCdc.print((int)device->switch_rumble_enable_pending);
    runtimeDebugCdc.print('/');
    runtimeDebugCdc.print(device->switch_rumble_enable_attempt_count);
    runtimeDebugCdc.print('/');
    runtimeDebugCdc.print(device->switch_rumble_enable_ack_count);
    runtimeDebugCdc.print('/');
    runtimeDebugCdc.print(device->switch_rumble_enable_fail_count);
    runtimeDebugCdc.print('/');
    runtimeDebugCdc.print(device->switch_rumble_submit_count);
    runtimeDebugCdc.print('/');
    runtimeDebugCdc.print(device->switch_rumble_success_count);
    runtimeDebugCdc.print('/');
    runtimeDebugCdc.print(device->switch_rumble_fail_count);
    runtimeDebugCdc.print(F(" RAW="));
    for (uint8_t i = 0; i < device->last_input_report_len; ++i) {
      if (device->last_input_report[i] < 0x10) runtimeDebugCdc.print('0');
      runtimeDebugCdc.print(device->last_input_report[i], HEX);
    }
    runtimeDebugCdc.println();
  }
#endif
#endif
}
#endif

#ifdef ENABLE_INPUT_USB
void printHidMappingProfileStatus() {
  HidMappingProfileList profiles{};
  const bool initialized = hidMappingProfileReadList(profiles);
  uint8_t count = 0;
  for (uint8_t i = 0; i < HID_MAPPING_PROFILE_COUNT; ++i) {
    if ((profiles.profiles[i].flags & HID_MAPPING_PROFILE_VALID) != 0) count++;
  }

  runtimeDebugCdc.print(F("HIDMAP INIT="));
  runtimeDebugCdc.print(initialized ? 1 : 0);
  runtimeDebugCdc.print(F(" COUNT="));
  runtimeDebugCdc.print(count);
  runtimeDebugCdc.print('/');
  runtimeDebugCdc.println(HID_MAPPING_PROFILE_COUNT);

  const SdlGameControllerDbStats db = sdlGameControllerDbStats();
  runtimeDebugCdc.print(F("HIDMAP DB SOURCE="));
  runtimeDebugCdc.print(db.source_records);
  runtimeDebugCdc.print(F(" COMPAT="));
  runtimeDebugCdc.print(db.compatible_records);
  runtimeDebugCdc.print(F(" IDS="));
  runtimeDebugCdc.print(db.identities);
  runtimeDebugCdc.print(F(" MAPS="));
  runtimeDebugCdc.print(db.unique_mappings);
  runtimeDebugCdc.print(F(" BLOCKED="));
  runtimeDebugCdc.print(db.blocked_identities);
  runtimeDebugCdc.print(F(" DEDUP="));
  runtimeDebugCdc.print(db.deduplicated_payloads);
  runtimeDebugCdc.print(F(" SHA256="));
  runtimeDebugCdc.println(sdlGameControllerDbSourceSha256());

  for (uint8_t i = 0; i < HID_MAPPING_PROFILE_COUNT; ++i) {
    const HidMappingProfile& profile = profiles.profiles[i];
    if ((profile.flags & HID_MAPPING_PROFILE_VALID) == 0) continue;
    runtimeDebugCdc.print(F("HIDMAP SLOT="));
    runtimeDebugCdc.print(i);
    runtimeDebugCdc.print(F(" VIDPID="));
    runtimeDebugCdc.print(profile.key.vid, HEX);
    runtimeDebugCdc.print(':');
    runtimeDebugCdc.print(profile.key.pid, HEX);
    runtimeDebugCdc.print(F(" BCD="));
    runtimeDebugCdc.print(profile.key.bcd_device, HEX);
    runtimeDebugCdc.print(F(" DESC="));
    runtimeDebugCdc.print(profile.key.descriptor_crc32, HEX);
    runtimeDebugCdc.print(F(" TRANSPORT="));
    runtimeDebugCdc.print(profile.key.transport);
    runtimeDebugCdc.print(F(" FLAGS="));
    runtimeDebugCdc.print(profile.flags, HEX);
    runtimeDebugCdc.print(F(" AGE="));
    runtimeDebugCdc.println(profile.order);
  }
}
#endif

#endif  // ENABLE_RUNTIME_SERIAL_DEBUG_CDC
#ifdef ENABLE_USB_AUTH_SIDECAR
void printAuthSidecarSerialStatus(Print& out) {
  const PsAuthDongleStatus auth = ps_auth_dongle_status();

  out.print(F("PSAUTH SUP="));
  out.print(auth.supported ? 1 : 0);
  out.print(F(" CONN="));
  out.print(auth.connected ? 1 : 0);
  out.print(F(" READY="));
  out.print(auth.signature_ready ? 1 : 0);
  out.print(F(" BUSY="));
  out.print(auth.busy ? 1 : 0);
  out.print(F(" S5UNLOCK="));
  out.print(auth.s5_unlocked ? 1 : 0);
  out.print(F(" ROOT="));
  out.print((int)auth.root_port);
  out.print(F(" DEV="));
  out.print((int)auth.dev_addr);
  out.print(F(" IF="));
  out.print((int)auth.instance);
  out.print(F(" VID=0x"));
  out.print(auth.vid, HEX);
  out.print(F(" PID=0x"));
  out.print(auth.pid, HEX);
  out.print(F(" LASTVID=0x"));
  out.print(auth.last_vid, HEX);
  out.print(F(" LASTPID=0x"));
  out.print(auth.last_pid, HEX);
  out.print(F(" DESC="));
  out.print(auth.last_desc_len);
  out.print(F(" RC="));
  out.print((int)auth.last_report_count);
  out.print(F(" CAND="));
  out.print(auth.last_descriptor_candidate ? 1 : 0);
  out.print(F(" PROTO="));
  out.print((int)auth.protocol);
  out.print(F(" ERR="));
  out.print((int)auth.last_error);
  out.print(F(" ST="));
  out.print((int)auth.state);
  out.print(F(" NP="));
  out.print((int)auth.nonce_part);
  out.print(F(" SP="));
  out.print((int)auth.signature_part);
  out.print(F(" GET="));
  out.print(auth.get_report_count);
  out.print(F(" SET="));
  out.print(auth.set_report_count);
  out.print(F(" SENT="));
  out.print(auth.sent_report_count);
  out.print(F(" IN="));
  out.print(auth.input_report_count);
  out.print(F(" P5RB="));
  out.print((int)auth.ps5_raw_buttons0, HEX);
  out.print(F("/"));
  out.print((int)auth.ps5_raw_buttons1, HEX);
  out.print(F("/"));
  out.print((int)auth.ps5_raw_buttons2, HEX);
  out.print(F(" P5SB="));
  out.print((int)auth.ps5_signed_buttons0, HEX);
  out.print(F("/"));
  out.print((int)auth.ps5_signed_buttons1, HEX);
  out.print(F("/"));
  out.print((int)auth.ps5_signed_buttons2, HEX);
  out.print(F(" P5Q=0x"));
  out.print((int)auth.ps5_hash_flags, HEX);
  out.print(F(" P5M="));
  out.print(auth.ps5_hash_mismatch_count);
  out.print(F(" P5PREP="));
  out.print(auth.ps5_prepare_count);
  out.print(F(" P5HOST="));
  out.print(auth.ps5_host_submit_count);
  out.print(F(" P5BUSY="));
  out.print(auth.ps5_queue_busy_count);
  out.print(F(" P5TO="));
  out.print(auth.ps5_hash_timeout_count);
  out.print(F(" P5HDT="));
  out.print(auth.ps5_last_host_delta_us);
  out.print(F(" P5DDT="));
  out.print(auth.ps5_last_dongle_delta_us);
  out.print(F(" P5HAGE="));
  out.print(auth.ps5_host_age_us);
  out.print(F(" P5DAGE="));
  out.print(auth.ps5_dongle_age_us);
  out.print(F(" P5IAGE="));
  out.print(auth.ps5_inflight_age_us);
  out.print(F(" P5ILEN="));
  out.println(auth.last_dongle_input_len);

#ifdef ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT
  const XboxOneAuthSidecarStatus xbone = xbone_auth_passthrough_status();
  out.print(F("XBAUTH SUP="));
  out.print(xbone.supported ? 1 : 0);
  out.print(F(" MNT="));
  out.print(xbone.mounted ? 1 : 0);
  out.print(F(" RDY="));
  out.print(xbone.dongle_ready ? 1 : 0);
  out.print(F(" AUTH="));
  out.print(xbone.auth_completed ? 1 : 0);
  out.print(F(" REM="));
  out.print(xbone.donor_removable ? 1 : 0);
  out.print(F(" CAND="));
  out.print((int)xbone.candidate_count);
  out.print(F(" DEV="));
  out.print((int)xbone.dev_addr);
  out.print('/');
  out.print((int)xbone.instance);
  out.print(F(" VID=0x"));
  out.print(xbone.vid, HEX);
  out.print(F(" PID=0x"));
  out.print(xbone.pid, HEX);
  out.print(F(" ST="));
  out.print((int)xbone.state);
  out.print(F(" CMD=0x"));
  out.print((int)xbone.last_command, HEX);
  out.print(F(" SEQ="));
  out.print((int)xbone.last_sequence);
  out.print(F(" LEN="));
  out.print(xbone.last_length);
  out.print(F(" CNT="));
  out.print(xbone.mount_count);
  out.print('/');
  out.print(xbone.input_count);
  out.print('/');
  out.print(xbone.sent_count);
  out.print(F(" Q="));
  out.print(xbone.queue_count);
  out.print('/');
  out.print(xbone.queue_drop_count);
  out.print(F(" BAD="));
  out.print(xbone.invalid_count);
  out.print(F(" INV="));
  out.print(xbone.last_invalid_length);
  out.print('/');
  const uint8_t invalidHeadLength = min(
      static_cast<uint16_t>(sizeof(xbone.last_invalid_head)),
      xbone.last_invalid_length);
  for (uint8_t index = 0; index < invalidHeadLength; ++index) {
    if (xbone.last_invalid_head[index] < 0x10) out.print('0');
    out.print(xbone.last_invalid_head[index], HEX);
  }
  out.print(F(" AUTHIO="));
  out.print(xbone.console_to_dongle_count);
  out.print('/');
  out.print(xbone.dongle_to_console_count);
  out.print(F(" DQ="));
  out.print((int)xbone.donor_queue_depth);
  out.print('/');
  out.print((int)xbone.donor_queue_high_water);
  out.print(F(" BP="));
  out.print(xbone.console_backpressure_count);
  out.print('/');
  out.print(xbone.donor_backpressure_count);
  out.print('/');
  out.print(xbone.donor_resume_count);
  out.print(F(" WAKE="));
  out.print(xbone.wake_queue_count);
  out.print('/');
  out.print(xbone.wake_send_count);
  out.print('/');
  out.println(xbone.wake_fail_count);

  const XboxOneOutputStatus xboneOut = xboxone_output_status();
  out.print(F("XBOUT SUP="));
  out.print(xboneOut.supported ? 1 : 0);
  out.print(F(" MNT="));
  out.print(xboneOut.mounted ? 1 : 0);
  out.print(F(" RDY="));
  out.print(xboneOut.ready ? 1 : 0);
  out.print(F(" AUTH="));
  out.print(xboneOut.auth_ready ? 1 : 0);
  out.print(F(" USB="));
  out.print(xboneOut.usb_inited ? 1 : 0);
  out.print('/');
  out.print(xboneOut.usb_connected ? 1 : 0);
  out.print('/');
  out.print(xboneOut.usb_mounted ? 1 : 0);
  out.print('/');
  out.print(xboneOut.usb_suspended ? 1 : 0);
  out.print(F(" BEGIN="));
  out.print(xboneOut.begin_attempted ? 1 : 0);
  out.print('/');
  out.print(xboneOut.begin_ok ? 1 : 0);
  out.print(F(" DESC="));
  out.print(xboneOut.descriptor_query_count);
  out.print('/');
  out.print(xboneOut.descriptor_build_count);
  out.print('/');
  out.print(xboneOut.descriptor_failure_count);
  out.print(F(" ST="));
  out.print((int)xboneOut.state);
  out.print(F(" EP="));
  out.print(xboneOut.ep_in, HEX);
  out.print('/');
  out.print(xboneOut.ep_out, HEX);
  out.print(F(" CMD=0x"));
  out.print((int)xboneOut.last_command, HEX);
  out.print(F(" SEQ="));
  out.print((int)xboneOut.last_sequence);
  out.print(F(" LEN="));
  out.print(xboneOut.last_len);
  out.print(F(" IO="));
  out.print(xboneOut.out_count);
  out.print('/');
  out.print(xboneOut.in_count);
  out.print(F(" Q="));
  out.print(xboneOut.queue_count);
  out.print('/');
  out.print(xboneOut.queue_drop_count);
  out.print(F(" AUTHIO="));
  out.print(xboneOut.auth_console_count);
  out.print('/');
  out.print(xboneOut.auth_dongle_count);
  out.print(F(" REC="));
  out.print(xboneOut.auth_donor_restart_count);
  out.print(F(" VCTRL="));
  out.print(xboneOut.control_count);
  out.print(F(" SETUP="));
  out.print(xboneOut.setup_request_count);
  out.print(':');
  out.print(xboneOut.last_control_type, HEX);
  out.print('/');
  out.print(xboneOut.last_control_request, HEX);
  out.print('/');
  out.print(xboneOut.last_control_value, HEX);
  out.print('/');
  out.print(xboneOut.last_control_index, HEX);
  out.print('/');
  out.println(xboneOut.last_control_length);
  if (xboxone_windows_multi_enabled()) {
    const uint8_t slotCount = xboxone_windows_multi_slot_count();
    out.print(F("GIPLAB SLOTS="));
    out.print((int)slotCount);
    out.println(F(" DONORREQ=0"));
    for (uint8_t index = 0; index < slotCount; ++index) {
      const XboxOneOutputStatus slot =
          xboxone_windows_multi_slot_status(index);
      out.print(F("GIPSLOT P="));
      out.print((int)(index + 1));
      out.print(F(" MNT="));
      out.print(slot.mounted ? 1 : 0);
      out.print(F(" RDY="));
      out.print(slot.ready ? 1 : 0);
      out.print(F(" ST="));
      out.print((int)slot.state);
      out.print(F(" EP="));
      out.print(slot.ep_in, HEX);
      out.print('/');
      out.print(slot.ep_out, HEX);
      out.print(F(" IO="));
      out.print(slot.out_count);
      out.print('/');
      out.print(slot.in_count);
      out.print(F(" Q="));
      out.print(slot.queue_count);
      out.print('/');
      out.print(slot.queue_drop_count);
      out.print(F(" DESC="));
      out.print(slot.descriptor_query_count);
      out.print('/');
      out.print(slot.descriptor_build_count);
      out.print('/');
      out.println(slot.descriptor_failure_count);
    }
  }
#endif
}
#endif

#ifdef ENABLE_RUNTIME_SERIAL_DEBUG_CDC
#ifdef ENABLE_INPUT_JVS
void printJvsPadStatus(uint8_t index, const JvsHostState& state) {
  const controller_state_t& frame = controllerFrameConst(index);
  runtimeDebugCdc.print(F("JVS P"));
  runtimeDebugCdc.print((int)(index + 1));
  runtimeDebugCdc.print(F(" SW0=0x"));
  printHexByte(state.sw_state0[index]);
  runtimeDebugCdc.print(F(" SW1=0x"));
  printHexByte(state.sw_state1[index]);
  runtimeDebugCdc.print(F(" PAD="));
  printHex32(frame.digital_buttons);
  runtimeDebugCdc.print(F(" CONN="));
  runtimeDebugCdc.print(frame.connected ? 1 : 0);
  runtimeDebugCdc.print(F(" TYPE="));
  runtimeDebugCdc.println(frame.controller_type_name[0] ? frame.controller_type_name : "-");
}

void printJvsSerialStatus() {
  constexpr uint32_t kJvsFreshSyncMs = 250;
  const JvsHostState& state = getJvsHostState();
  const JvsBoardInfo& board = getJvsBoardInfo();
  const JvsDebugSnapshot debug = getJvsDebugSnapshot();

  runtimeDebugCdc.print(F("JVS HOST="));
  runtimeDebugCdc.print((int)debug.machine_state);
  runtimeDebugCdc.print(F(" CONN="));
  runtimeDebugCdc.print(state.connected ? 1 : 0);
  runtimeDebugCdc.print(F(" READY="));
  runtimeDebugCdc.print(state.ready ? 1 : 0);
  runtimeDebugCdc.print(F(" FRESH="));
  runtimeDebugCdc.print(isJvsHostSyncedRecently(kJvsFreshSyncMs) ? 1 : 0);
  runtimeDebugCdc.print(F(" SYNCS="));
  runtimeDebugCdc.print(debug.synced_count);
  runtimeDebugCdc.print(F(" AGE_MS="));
  if (debug.last_sync_at_ms == 0) {
    runtimeDebugCdc.print(F("-1"));
  } else {
    runtimeDebugCdc.print((unsigned long)(millis() - debug.last_sync_at_ms));
  }
  runtimeDebugCdc.print(F(" SVC="));
  runtimeDebugCdc.print(debug.service_count);
  runtimeDebugCdc.print(F(" READY_RET="));
  runtimeDebugCdc.print(debug.host_ready_count);
  runtimeDebugCdc.print(F(" SYNC_REQ="));
  runtimeDebugCdc.println(debug.sync_request_count);

  runtimeDebugCdc.print(F("JVS BUS RXB="));
  runtimeDebugCdc.print(debug.uart_rx_byte_count);
  runtimeDebugCdc.print(F(" TXB="));
  runtimeDebugCdc.print(debug.uart_tx_byte_count);
  runtimeDebugCdc.print(F(" RX_ERR="));
  runtimeDebugCdc.print(debug.uart_rx_error_count);
  runtimeDebugCdc.print(F(" REC="));
  runtimeDebugCdc.print(debug.transport_recovery_count);
  runtimeDebugCdc.print(F(" REPLY_TO="));
  runtimeDebugCdc.print(debug.reply_timeout_count);
  runtimeDebugCdc.print(F(" BAD_REPLY="));
  runtimeDebugCdc.print(debug.bad_reply_count);
  runtimeDebugCdc.print(F(" BUS_RESET="));
  runtimeDebugCdc.print(debug.bus_reset_count);
  runtimeDebugCdc.print(F(" LAST_ERR=0x"));
  printHexByte(debug.last_uart_rx_error_flags);
  runtimeDebugCdc.print(F(" LAST_RX=0x"));
  printHexByte(debug.last_rx_byte);
  runtimeDebugCdc.print(F(" LAST_TX="));
  runtimeDebugCdc.print((int)debug.last_tx_size);
  runtimeDebugCdc.print(F(" TX_ACTIVE="));
  runtimeDebugCdc.print(debug.tx_active ? 1 : 0);
  runtimeDebugCdc.print(F(" RX_READY="));
  runtimeDebugCdc.println(debug.rx_available ? 1 : 0);

  runtimeDebugCdc.print(F("JVS BOARD P="));
  runtimeDebugCdc.print((int)board.players);
  runtimeDebugCdc.print(F(" B="));
  runtimeDebugCdc.print((int)board.buttons);
  runtimeDebugCdc.print(F(" C="));
  runtimeDebugCdc.print((int)board.coins);
  runtimeDebugCdc.print(F(" A="));
  runtimeDebugCdc.print((int)board.analog_channels);
  runtimeDebugCdc.print(F(" ABITS="));
  runtimeDebugCdc.print((int)board.analog_bits);
  runtimeDebugCdc.print(F(" ID="));
  runtimeDebugCdc.print(board.id_received ? 1 : 0);
  runtimeDebugCdc.print(F(" FC="));
  runtimeDebugCdc.print(board.function_check_received ? 1 : 0);
  runtimeDebugCdc.print(F(" NAME="));
  runtimeDebugCdc.println((board.id_received && board.io_id[0]) ? board.io_id : "-");

  runtimeDebugCdc.print(F("JVS INPUT COIN=0x"));
  printHexByte(state.coin_state);
  runtimeDebugCdc.print(F(" TEST="));
  runtimeDebugCdc.println((state.coin_state & 0x80) ? 1 : 0);

  runtimeDebugCdc.print(F("JVS ANALOG CH="));
  runtimeDebugCdc.print((int)state.analog_channels);
  runtimeDebugCdc.print(F(" BITS="));
  runtimeDebugCdc.print((int)board.analog_bits);
  runtimeDebugCdc.print(F(" VALUES="));
  for (uint8_t index = 0; index < state.analog_channels; ++index) {
    if (index) runtimeDebugCdc.print(',');
    runtimeDebugCdc.print((unsigned int)state.analog_values[index], HEX);
  }
  runtimeDebugCdc.println();

  const JvsAnalogConfig& analogConfig = jvsAnalogGetConfig();
  runtimeDebugCdc.print(F("JVS MAP PROFILE="));
  runtimeDebugCdc.print(jvsAnalogProfileName(
      static_cast<JvsAnalogProfile>(analogConfig.profile)));
  runtimeDebugCdc.print(F(" MATCH="));
  runtimeDebugCdc.print(
      jvsAnalogConfigMatchesBoard(
          analogConfig, board.io_id, state.analog_channels,
          board.analog_bits)
          ? 1
          : 0);
  runtimeDebugCdc.print(F(" EXPECT="));
  runtimeDebugCdc.print((int)analogConfig.expected_channels);
  runtimeDebugCdc.print('x');
  runtimeDebugCdc.print((int)analogConfig.expected_bits);
  runtimeDebugCdc.print(F(" BOARDCRC=0x"));
  runtimeDebugCdc.println((unsigned int)analogConfig.board_id_crc, HEX);
  for (uint8_t channel = 0;
       channel < JVS_ANALOG_CONFIG_CHANNELS;
       ++channel) {
    runtimeDebugCdc.print(F("JVS MAP A"));
    runtimeDebugCdc.print((int)channel);
    runtimeDebugCdc.print('=');
    runtimeDebugCdc.print(jvsAnalogTargetName(
        static_cast<JvsAnalogTarget>(analogConfig.target[channel])));
    runtimeDebugCdc.print(F(" REV="));
    runtimeDebugCdc.print(
        (analogConfig.invert_mask & (1u << channel)) ? 1 : 0);
    runtimeDebugCdc.print(F(" CAL="));
    runtimeDebugCdc.print(
        (analogConfig.calibrated_mask & (1u << channel)) ? 1 : 0);
    runtimeDebugCdc.print(F(" RANGE="));
    runtimeDebugCdc.print((unsigned int)analogConfig.minimum[channel], HEX);
    runtimeDebugCdc.print('/');
    runtimeDebugCdc.print((unsigned int)analogConfig.center[channel], HEX);
    runtimeDebugCdc.print('/');
    runtimeDebugCdc.println((unsigned int)analogConfig.maximum[channel], HEX);
  }

  const JvsPanelDetector& detector = jvsPanelDetector();
  const JvsAnalogLayout detected = detector.layout();
  runtimeDebugCdc.print(F("JVS DETECT LAYOUT="));
  runtimeDebugCdc.println(detected == JvsAnalogLayout::Off
                              ? "Unknown" : jvsAnalogLayoutName(detected));
  for (uint8_t channel = 0; channel < detector.channelCount(); ++channel) {
    runtimeDebugCdc.print(F("JVS DETECT A"));
    runtimeDebugCdc.print((int)channel);
    runtimeDebugCdc.print('=');
    runtimeDebugCdc.print(detector.centered(channel) ? "Centered"
                          : detector.pedal(channel) ? "OneWay" : "Still");
    runtimeDebugCdc.print(F(" RANGE="));
    runtimeDebugCdc.print((unsigned int)detector.low[channel], HEX);
    runtimeDebugCdc.print('/');
    runtimeDebugCdc.print((unsigned int)detector.rest[channel], HEX);
    runtimeDebugCdc.print('/');
    runtimeDebugCdc.println((unsigned int)detector.high[channel], HEX);
  }

  for (uint8_t index = 0;
       index < min(kJvsMaxPlayers, (uint8_t)MAX_USB_OUT);
       ++index) {
    printJvsPadStatus(index, state);
  }
}

void printJvsPanelSerialStatus() {
  const JvsBoardInfo& board = getJvsBoardInfo();
  const JvsPanelMap& map = jvsPanelGetMap();
  const bool learned = jvsPanelLearnedActive();
  runtimeDebugCdc.print(F("JVS PANEL MODE="));
  runtimeDebugCdc.print(learned ? F("LEARNED") : F("AUTO"));
  runtimeDebugCdc.print(F(" LAYOUT="));
  runtimeDebugCdc.print((int)jvsPanelLayoutButtons());
  runtimeDebugCdc.print(F(" GUESS="));
  runtimeDebugCdc.print((int)getJvsPhysicalButtonsPerPlayer(board));
  runtimeDebugCdc.print(F(" BOARDCRC=0x"));
  runtimeDebugCdc.println(
      (unsigned int)(board.id_received ? jvsAnalogBoardIdCrc(board.io_id) : 0),
      HEX);
  runtimeDebugCdc.print(F("JVS PANEL STORED="));
  runtimeDebugCdc.print(map.button_count != 0 ? 1 : 0);
  runtimeDebugCdc.print(F(" BUTTONS="));
  runtimeDebugCdc.print((int)map.button_count);
  runtimeDebugCdc.print(F(" BOARDCRC=0x"));
  runtimeDebugCdc.print((unsigned int)map.board_id_crc, HEX);
  for (uint8_t role = 0; role < kJvsPanelRoleCount; ++role) {
    runtimeDebugCdc.print(' ');
    runtimeDebugCdc.print(jvsPanelRoleName(role));
    runtimeDebugCdc.print('=');
    if (map.push[role] != 0) {
      runtimeDebugCdc.print((int)map.push[role]);
    } else {
      runtimeDebugCdc.print('-');
    }
  }
  runtimeDebugCdc.println();
}

void printJvsRawSerialStatus() {
  const JvsDebugSnapshot debug = getJvsDebugSnapshot();
  const JvsHostState& state = getJvsHostState();

  runtimeDebugCdc.print(F("JVSRAW SENSE_MV="));
  runtimeDebugCdc.print((int)debug.sense_mv);
  runtimeDebugCdc.print(F(" HOST="));
  runtimeDebugCdc.print((int)debug.machine_state);
  runtimeDebugCdc.print(F(" SYNCS="));
  runtimeDebugCdc.print(debug.synced_count);
  runtimeDebugCdc.print(F(" RXB="));
  runtimeDebugCdc.print(debug.uart_rx_byte_count);
  runtimeDebugCdc.print(F(" TXB="));
  runtimeDebugCdc.print(debug.uart_tx_byte_count);
  runtimeDebugCdc.print(F(" RX_ERRS="));
  runtimeDebugCdc.print(debug.uart_rx_error_count);
  runtimeDebugCdc.print(F(" REC="));
  runtimeDebugCdc.print(debug.transport_recovery_count);
  runtimeDebugCdc.print(F(" LAST_ERR=0x"));
  printHexByte(debug.last_uart_rx_error_flags);
  runtimeDebugCdc.print(F(" LAST_RX=0x"));
  printHexByte(debug.last_rx_byte);
  runtimeDebugCdc.print(F(" LAST_TX="));
  runtimeDebugCdc.print((int)debug.last_tx_size);
  runtimeDebugCdc.print(F(" TX_ACTIVE="));
  runtimeDebugCdc.print(debug.tx_active ? 1 : 0);
  runtimeDebugCdc.print(F(" RX_READY="));
  runtimeDebugCdc.println(debug.rx_available ? 1 : 0);

  runtimeDebugCdc.print(F("JVSRAW DONOR SETIN="));
  runtimeDebugCdc.print(debug.donor_set_input_count);
  runtimeDebugCdc.print(F(" SETOUT="));
  runtimeDebugCdc.print(debug.donor_set_output_count);
  runtimeDebugCdc.print(F(" TXN="));
  runtimeDebugCdc.print(debug.donor_transaction_count);
  runtimeDebugCdc.print(F(" DRAIN="));
  runtimeDebugCdc.print(debug.donor_drain_count);
  runtimeDebugCdc.print(F(" SVC="));
  runtimeDebugCdc.print(debug.service_count);
  runtimeDebugCdc.print(F(" READY_RET="));
  runtimeDebugCdc.print(debug.host_ready_count);
  runtimeDebugCdc.print(F(" SYNC_REQ="));
  runtimeDebugCdc.println(debug.sync_request_count);

  runtimeDebugCdc.print(F("JVSRAW STATE COIN=0x"));
  printHexByte(state.coin_state);
  runtimeDebugCdc.print(F(" TEST="));
  runtimeDebugCdc.println((state.coin_state & 0x80) ? 1 : 0);

  runtimeDebugCdc.print(F("JVSRAW ANALOG="));
  for (uint8_t index = 0; index < state.analog_channels; ++index) {
    if (index) runtimeDebugCdc.print(',');
    runtimeDebugCdc.print((unsigned int)state.analog_values[index], HEX);
  }
  runtimeDebugCdc.println();

  runtimeDebugCdc.print(F("JVSRAW RXDBG="));
  printHexByteArray(debug.last_rx_debug, sizeof(debug.last_rx_debug));
  runtimeDebugCdc.println();

  runtimeDebugCdc.print(F("JVSRAW TXDBG="));
  printHexByteArray(debug.last_tx_debug, sizeof(debug.last_tx_debug));
  runtimeDebugCdc.println();
}
#endif

void handleRuntimeSerialCommand() {
  if (serialCommandLength == 0) {
    return;
  }

  serialCommandBuffer[serialCommandLength] = '\0';
  char* remainder = nullptr;

  if (serialCommandEquals("PING")) {
    runtimeDebugCdc.println(F("PONG"));
  } else if (serialCommandEquals("IDENTITY")) {
    printRuntimeSerialIdentity();
  } else if (serialCommandEquals("IDENTITY2")) {
    adapt_identity::write(runtimeDebugCdc);
  } else if (serialCommandEquals("STATUS") ||
             serialCommandEquals("INFO")) {
    printRuntimeSerialStatus();
#if defined(ARDUINO_ARCH_RP2040)
  } else if (serialCommandEquals("TEMP") ||
             serialCommandEquals("TEMPERATURE")) {
    printRp2040TemperatureStatus();
#endif
#ifdef ENABLE_INPUT_USB
  } else if (serialCommandEquals("HIDMAP")) {
    printHidMappingProfileStatus();
#endif
#if defined(ENABLE_USB_AUTH_SIDECAR) || defined(ENABLE_INPUT_USB)
  } else if (serialCommandEquals("USBHOST START")) {
    usb_host_service_request_start();
    runtimeDebugCdc.println(F("OK:USBHOST_START_REQUESTED"));
    printUsbHostSerialStatus();
  } else if (serialCommandEquals("USBHOST RESET")) {
    if (usb_host_service_force_root_reset()) {
      runtimeDebugCdc.println(F("OK:USBHOST_RESET"));
      delay(120);
    } else {
      runtimeDebugCdc.println(F("ERR:USBHOST_NOT_STARTED"));
    }
    printUsbHostSerialStatus();
  } else if (serialCommandEquals("USBHOST")) {
    printUsbHostSerialStatus();
#endif
#ifdef ENABLE_USB_AUTH_SIDECAR
  } else if (serialCommandEquals("AUTH START") ||
             serialCommandEquals("PSAUTH START")) {
    usb_host_service_request_start();
    runtimeDebugCdc.println(F("OK:USBHOST_START_REQUESTED"));
    printUsbHostSerialStatus();
    printAuthSidecarSerialStatus(runtimeDebugCdc);
#ifdef ENABLE_USB_AUTH_SERIAL_TRACE
  } else if (serialCommandEquals("AUTH P5SIM") ||
             serialCommandEquals("PSAUTH P5SIM") ||
             serialCommandEquals("PS5SIM")) {
    ps_auth_dongle_debug_ps5_simulate(runtimeDebugCdc);
    printAuthSidecarSerialStatus(runtimeDebugCdc);
#endif
#if ADAPT_POLICY_PS5_DIAGNOSTIC_RUNTIME
  } else if (serialCommandEquals("AUTH PS5") ||
             serialCommandEquals("PSAUTH PS5") ||
             serialCommandEquals("PS5DIAG ARM")) {
    ps5_diag_arm_next_boot();
    configuredOutputMode = OUTPUT_PS5;
    outputMode = sanitizeRuntimeOutputMode(configuredOutputMode);
    runtimeDebugCdc.println(F("OK:PS5_DIAG_ARMED_REBOOT"));
    runtimeDebugCdc.flush();
    delay(100);
    reboot();
#endif
  } else if (serialCommandEquals("AUTH") ||
             serialCommandEquals("PSAUTH")) {
    printAuthSidecarSerialStatus(runtimeDebugCdc);
#endif
#ifdef ENABLE_INPUT_JVS
  } else if (serialCommandEquals("JVS") ||
             serialCommandEquals("JVSSTATUS")) {
    printJvsSerialStatus();
  } else if (serialCommandEquals("JVS PANEL")) {
    printJvsPanelSerialStatus();
  } else if (serialCommandEquals("JVS PANEL CLEAR CONFIRM")) {
    jvsPanelClearMap();
    runtimeDebugCdc.println(F("OK:JVS_PANEL_CLEARED"));
  } else if (serialCommandEquals("JVS PANEL CLEAR")) {
    runtimeDebugCdc.println(F("ERR:CONFIRM_REQUIRED"));
  } else if (serialCommandEquals("JVSRAW")) {
    printJvsRawSerialStatus();
  } else if (serialCommandEquals("JVSSYNC")) {
    requestJvsHostSync();
    runtimeDebugCdc.println(F("OK:JVSSYNC"));
  } else if (serialCommandEquals("JVSCLR")) {
    clearJvsDebugCounters();
    runtimeDebugCdc.println(F("OK:JVSCLR"));
#endif
  } else if (handleCdcFirmwareUpdateSerialCommand(
                 serialCommandBuffer, runtimeDebugCdc)) {
    // Native management CDC owns the signed binary stream.
  } else if (handleSerialDebugCommand(serialCommandBuffer, runtimeDebugCdc)) {
    // Handled by the same transport-independent dispatcher as debug UART.
  } else if (serialCommandEquals("HELP") ||
             serialCommandEquals("?")) {
#ifdef ENABLE_USB_AUTH_SIDECAR
    runtimeDebugCdc.print(F("CMDS:PING,IDENTITY,STATUS,TEMP,STATE,GPIO,DHELP,SET,HOTKEY,CHORD,USBHOST,USBHOST RATE,USBHOST RATE RESET,AUTH,XBAUTH"));
#elif defined(ENABLE_INPUT_USB)
    runtimeDebugCdc.print(F("CMDS:PING,IDENTITY,STATUS,TEMP,STATE,GPIO,DHELP,SET,HOTKEY,CHORD,USBHOST,USBHOST RATE,USBHOST RATE RESET,HIDMAP"));
#else
    runtimeDebugCdc.print(F("CMDS:PING,IDENTITY,STATUS,TEMP,STATE,GPIO,DHELP,SET,HOTKEY,CHORD"));
#endif
    featureModulesAppendSerialHelp(runtimeDebugCdc);
#ifdef ENABLE_INPUT_JVS
    runtimeDebugCdc.print(F(",JVS,JVS PANEL,JVS PANEL CLEAR CONFIRM,JVSRAW,JVSSYNC,JVSCLR"));
#endif
    runtimeDebugCdc.print(F(",FWUPDATE,ADAPTSTATE,OVERLAY,OLED,OUT,WIN,MISTER,LAT"));
    appendSerialManagementHelp(runtimeDebugCdc);
#if defined(ENABLE_LATENCY_PHASE_TRACE_CDC)
    runtimeDebugCdc.print(F(",LAT TRACE <ON|OFF|DUMP|CLR>"));
#endif
    runtimeDebugCdc.println(F(",RESET,BOOT"));
  } else {
    char* remainder = nullptr;
    if (serialCommandStartsWith("OUT", &remainder)) {
      outputMode_t newMode = OUTPUT_MISTER;
      if (!parseRuntimeOutputModeToken(remainder, &newMode)) {
        runtimeDebugCdc.println(F("ERR:BAD_OUT_MODE"));
      } else {
        saveRuntimeOutputModeFromSerial(newMode);
      }
    } else if (serialCommandStartsWith("WIN", &remainder)) {
      uint8_t newWinOutput = 0;
      if (!parseWindowsOutputToken(remainder, &newWinOutput)) {
        runtimeDebugCdc.println(F("ERR:BAD_WIN_MODE"));
      } else {
        saveWindowsOutputPreferenceFromSerial(newWinOutput);
      }
    } else if (serialCommandStartsWith("MISTER", &remainder)) {
      uint8_t newMisterOutput = 0;
      if (!parseWindowsOutputToken(remainder, &newMisterOutput)) {
        runtimeDebugCdc.println(F("ERR:BAD_MISTER_MODE"));
      } else {
        saveMisterOutputPreferenceFromSerial(newMisterOutput);
      }
    } else if (serialCommandStartsWith("ADAPTSTATE", &remainder) ||
               serialCommandStartsWith("STATESTREAM", &remainder)) {
      if (*remainder == '\0' || serialTokenEquals(remainder, "STATUS")) {
        adaptStateWriteStatus(runtimeDebugCdc);
      } else if (serialTokenEquals(remainder, "ON") ||
                 serialTokenEquals(remainder, "START")) {
        if (!refuseStreamDuringMisterSession(runtimeDebugCdc)) {
          inputOverlaySetEnabled(false);
          oledSerialSetEnabled(false);
          adaptStateSetEnabled(true);
          runtimeDebugCdc.println(F("OK:ADAPTSTATE=1"));
        }
      } else if (serialTokenEquals(remainder, "OFF") ||
                 serialTokenEquals(remainder, "STOP")) {
        adaptStateSetEnabled(false);
        runtimeDebugCdc.println(F("OK:ADAPTSTATE=0"));
      } else if (serialCommandStartsWith("ADAPTSTATE RATE", &remainder) ||
                 serialCommandStartsWith("STATESTREAM RATE", &remainder)) {
        uint32_t rateHz = 0;
        if (!parseUnsignedLongToken(remainder, &rateHz) ||
            rateHz < ADAPT_STATE_MIN_RATE_HZ ||
            rateHz > ADAPT_STATE_MAX_RATE_HZ) {
          runtimeDebugCdc.println(F("ERR:BAD_ADAPTSTATE_RATE"));
        } else {
          adaptStateSetRateHz((uint16_t)rateHz);
          runtimeDebugCdc.print(F("OK:ADAPTSTATE_RATE="));
          runtimeDebugCdc.println((int)adaptStateRateHz());
        }
      } else {
        runtimeDebugCdc.println(F("ERR:BAD_ADAPTSTATE_CMD"));
      }
    } else if (serialCommandStartsWith("OVERLAY", &remainder) ||
               serialCommandStartsWith("INPUTOVERLAY", &remainder) ||
               serialCommandStartsWith("RETROSPY", &remainder) ||
               serialCommandStartsWith("RSPY", &remainder)) {
      if (*remainder == '\0' || serialTokenEquals(remainder, "STATUS")) {
        inputOverlayWriteStatus(runtimeDebugCdc);
      } else if (serialTokenEquals(remainder, "ON") ||
                 serialTokenEquals(remainder, "START")) {
        if (!refuseStreamDuringMisterSession(runtimeDebugCdc)) {
          adaptStateSetEnabled(false);
          oledSerialSetEnabled(false);
          inputOverlaySetEnabled(true);
          runtimeDebugCdc.println(F("OK:OVERLAY=1"));
        }
      } else if (serialTokenEquals(remainder, "OFF") ||
                 serialTokenEquals(remainder, "STOP")) {
        inputOverlaySetEnabled(false);
        runtimeDebugCdc.println(F("OK:OVERLAY=0"));
      } else if (serialCommandStartsWith("OVERLAY RATE", &remainder) ||
                 serialCommandStartsWith("INPUTOVERLAY RATE", &remainder) ||
                 serialCommandStartsWith("RETROSPY RATE", &remainder) ||
                 serialCommandStartsWith("RSPY RATE", &remainder)) {
        uint32_t rateHz = 0;
        if (!parseUnsignedLongToken(remainder, &rateHz) ||
            rateHz < INPUT_OVERLAY_MIN_RATE_HZ ||
            rateHz > INPUT_OVERLAY_MAX_RATE_HZ) {
          runtimeDebugCdc.println(F("ERR:BAD_OVERLAY_RATE"));
        } else {
          inputOverlaySetRateHz((uint16_t)rateHz);
          runtimeDebugCdc.print(F("OK:OVERLAY_RATE="));
          runtimeDebugCdc.println((int)inputOverlayRateHz());
        }
      } else {
        runtimeDebugCdc.println(F("ERR:BAD_OVERLAY_CMD"));
      }
    } else if (serialCommandStartsWith("LAT", &remainder) ||
               serialCommandStartsWith("LATENCY", &remainder)) {
      if (*remainder == '\0' || serialTokenEquals(remainder, "STATUS")) {
        latencyTest.writeStatus(runtimeDebugCdc);
      } else if (serialTokenEquals(remainder, "CONFIG")) {
        latencyTest.writeStatus(runtimeDebugCdc);
      } else if (serialTokenEquals(remainder, "ON")) {
        saveLatencyEnabledFromSerial(true);
      } else if (serialTokenEquals(remainder, "OFF")) {
        saveLatencyEnabledFromSerial(false);
      } else if (serialTextStartsWith(remainder, "CTRL", &remainder)) {
        bool controllerInLoop = false;
        if (serialTokenEquals(remainder, "CONTROLLER") ||
            serialTokenEquals(remainder, "CTRL")) {
          controllerInLoop = true;
        } else if (!(serialTokenEquals(remainder, "INTERNAL") ||
                     serialTokenEquals(remainder, "NOCTRL"))) {
          runtimeDebugCdc.println(F("ERR:BAD_LAT_CTRL"));
          serialCommandLength = 0;
          return;
        }
        saveLatencyControllerFromSerial(controllerInLoop);
      } else if (serialTextStartsWith(remainder, "HOST", &remainder)) {
        uint8_t hostType = LATENCY_HOST_INTERNAL;
        if (!parseLatencyHostTypeToken(remainder, &hostType)) {
          runtimeDebugCdc.println(F("ERR:BAD_LAT_HOST"));
        } else {
          saveLatencyHostFromSerial(hostType);
        }
      } else if (serialTextStartsWith(remainder, "MODE", &remainder)) {
        uint8_t newMode = LATENCY_MODE_OFF;
        if (!parseLatencyModeToken(remainder, &newMode)) {
          runtimeDebugCdc.println(F("ERR:BAD_LAT_MODE"));
        } else {
          saveLatencyModeFromSerial(newMode);
        }
      } else if (serialTextStartsWith(remainder, "TRIGGER", &remainder) ||
                 serialTextStartsWith(remainder, "TRIG", &remainder) ||
                 serialTextStartsWith(remainder, "POLARITY", &remainder)) {
        if (serialTokenEquals(remainder, "HIGH") ||
            serialTokenEquals(remainder, "ACTIVE_HIGH") ||
            serialTokenEquals(remainder, "ACTIVEHIGH") ||
            serialTokenEquals(remainder, "1")) {
          latencyTest.setTriggerActiveHigh(true);
          latencyTest.writeStatus(runtimeDebugCdc);
        } else if (serialTokenEquals(remainder, "LOW") ||
                   serialTokenEquals(remainder, "ACTIVE_LOW") ||
                   serialTokenEquals(remainder, "ACTIVELOW") ||
                   serialTokenEquals(remainder, "0")) {
          latencyTest.setTriggerActiveHigh(false);
          latencyTest.writeStatus(runtimeDebugCdc);
        } else {
          runtimeDebugCdc.println(F("ERR:BAD_LAT_TRIGGER"));
        }
      } else if (serialTextStartsWith(remainder, "SVC", &remainder) ||
                 serialTextStartsWith(remainder, "SERVICE", &remainder)) {
        handleLatencyServiceSerialCommand(remainder);
      } else if (serialTokenEquals(remainder, "STOP")) {
        latencyTest.stopBench();
        runtimeDebugCdc.println(F("OK:LAT_STOP"));
      } else if (serialTokenEquals(remainder, "CLR")) {
        latencyTest.clearSamples();
        runtimeDebugCdc.println(F("OK:LAT_CLR"));
      } else if (serialTokenEquals(remainder, "DUMP")) {
        latencyTest.dumpSamples(runtimeDebugCdc);
      } else if (serialTextStartsWith(remainder, "START", &remainder)) {
        uint32_t sampleCount = 0;
        if (*remainder != '\0' && !parseUnsignedLongToken(remainder, &sampleCount)) {
          runtimeDebugCdc.println(F("ERR:BAD_LAT_COUNT"));
        } else if (!latencyTest.isCurrentModeReady()) {
          runtimeDebugCdc.println(F("ERR:LAT_NOT_READY"));
        } else {
          latencyTest.startBench(sampleCount);
          runtimeDebugCdc.print(F("OK:LAT_START="));
          runtimeDebugCdc.println(latencyTest.getRunTargetSamples());
        }
      } else if (serialTextStartsWith(remainder, "ACK", &remainder)) {
        uint32_t seq = 0;
        if (*remainder != '\0' && !parseUnsignedLongToken(remainder, &seq)) {
          runtimeDebugCdc.println(F("ERR:BAD_LAT_ACK"));
        } else {
          latencyTest.acknowledgeHost(seq);
          runtimeDebugCdc.println(F("OK:LAT_ACK"));
        }
      } else {
#if defined(ENABLE_LATENCY_PHASE_TRACE_CDC)
        char* traceRemainder = nullptr;
        if (serialTextStartsWith(remainder, "TRACE", &traceRemainder) ||
            serialTextStartsWith(remainder, "PHASE", &traceRemainder)) {
          if (*traceRemainder == '\0' || serialTokenEquals(traceRemainder, "STATUS")) {
            latencyPhaseTraceWriteStatus(runtimeDebugCdc);
          } else if (serialTokenEquals(traceRemainder, "ON") ||
                     serialTokenEquals(traceRemainder, "START")) {
            latencyPhaseTraceSetEnabled(true);
            latencyPhaseTraceWriteStatus(runtimeDebugCdc);
          } else if (serialTokenEquals(traceRemainder, "OFF") ||
                     serialTokenEquals(traceRemainder, "STOP")) {
            latencyPhaseTraceSetEnabled(false);
            latencyPhaseTraceWriteStatus(runtimeDebugCdc);
          } else if (serialTokenEquals(traceRemainder, "CLR") ||
                     serialTokenEquals(traceRemainder, "CLEAR")) {
            latencyPhaseTraceClear();
            latencyPhaseTraceWriteStatus(runtimeDebugCdc);
          } else if (serialTokenEquals(traceRemainder, "DUMP")) {
            latencyPhaseTraceDump(runtimeDebugCdc);
          } else {
            runtimeDebugCdc.println(F("ERR:BAD_LAT_TRACE_CMD"));
          }
        } else {
          runtimeDebugCdc.println(F("ERR:BAD_LAT_CMD"));
        }
#else
        runtimeDebugCdc.println(F("ERR:BAD_LAT_CMD"));
#endif
      }
    } else if (serialCommandEquals("RESET") ||
               serialCommandEquals("REBOOT")) {
      runtimeDebugCdc.println(F("OK:REBOOT"));
      runtimeDebugCdc.flush();
      delay(20);
      reboot();
    } else if (serialCommandEquals("B") ||
               serialCommandEquals("BOOT") ||
               serialCommandEquals("BOOTLOADER")) {
      runtimeDebugCdc.println(F("OK:BOOTLOADER"));
      runtimeDebugCdc.flush();
      delay(20);
      rebootToBootloader();
    } else {
      runtimeDebugCdc.println(F("ERR:UNKNOWN_CMD"));
    }
  }

  serialCommandLength = 0;
}

void processRuntimeSerialCommandsImpl() {
#ifdef ENABLE_INPUT_ESP32_SPI
  if (esp32ServiceUartBridgeActive()) {
    esp32ServiceUartBridgeTask(runtimeDebugCdc);
    return;
  }
#endif
  if (oledSerialHoldCommands()) {
    return;
  }
#ifdef ADAPT_FEATURE_MISTER_SERIAL_REQUESTS
  misterSerialDevice.task();
#ifdef ENABLE_TTY2OLED_SERIAL
  tty2oledSerialSetRawModesLocked(misterSerialDevice.sessionActive());
#endif
  if (serialCommandHeld) {
    if (misterSerialDevice.takeLine(serialCommandBuffer, serialCommandLength) ==
        mister_serial::LineResult::Hold) {
      return;
    }
    serialCommandHeld = false;
    serialCommandLength = 0;
  }
#endif
  bool sawSerialActivity = false;
#ifdef ADAPT_FEATURE_MISTER_SERIAL_STATUS
  // A complete artwork line spans several passes; controller polling keeps
  // priority even while Main streams a snapshot or load progress.
  uint16_t byteBudget = 256;
#endif
  while (runtimeDebugCdc.available() > 0) {
#ifdef ADAPT_FEATURE_MISTER_SERIAL_STATUS
    if (byteBudget-- == 0) break;
#endif
    const int raw = runtimeDebugCdc.read();
    if (raw < 0) {
      break;
    }

#ifdef ENABLE_TTY2OLED_SERIAL
    if (tty2oledSerialDrainByte((uint8_t)raw, runtimeDebugCdc)) {
      sawSerialActivity = true;
      continue;
    }
#endif

    const char ch = (char)raw;
    const SerialCommandLineResult lineResult = serialCommandLinePush(
        ch, serialCommandBuffer, kSerialCommandBufferSize,
        &serialCommandLength, &serialCommandDiscarding);
    if (lineResult == SERIAL_COMMAND_LINE_READY) {
      if (serialLineIsMisterRecord(serialCommandBuffer)) {
        // MiSTer Main's records are not commands and not user activity, and
        // they never get a text reply.
#ifdef ADAPT_FEATURE_MISTER_SERIAL_REQUESTS
        if (misterSerialDevice.takeLine(serialCommandBuffer, serialCommandLength) ==
            mister_serial::LineResult::Hold) {
          serialCommandHeld = true;
          break;
        }
#endif
        serialCommandLength = 0;
        continue;
      }
      sawSerialActivity = true;
      handleRuntimeSerialCommand();
      continue;
    }
    if (lineResult == SERIAL_COMMAND_LINE_OVERFLOW &&
        !serialLineIsMisterRecord(serialCommandBuffer)) {
      sawSerialActivity = true;
      runtimeDebugCdc.println(F("ERR:CMD_TOO_LONG"));
    }
  }

  if (sawSerialActivity) {
    resetIdleTimer(IdleWakeSource::Serial);
  }
}
#endif


}  // namespace

void processRuntimeSerialCommands() {
#ifdef ENABLE_RUNTIME_SERIAL_DEBUG_CDC
  processRuntimeSerialCommandsImpl();
#endif
}

bool runtimeSerialRefusesStream(Print& out) {
#ifdef ENABLE_RUNTIME_SERIAL_DEBUG_CDC
  return refuseStreamDuringMisterSession(out);
#else
  (void)out;
  return false;
#endif
}

void runtimeSerialWriteAuthSidecarStatus(Print& out) {
#ifdef ADAPT_OUTPUT_USB_DEVICE
  ps4Auth.writeDiagnostics(out);
#endif
#ifdef ENABLE_USB_AUTH_SIDECAR
  printAuthSidecarSerialStatus(out);
#else
  (void)out;
#endif
}

void runtimeSerialWriteXboxOneAuthTrace(Print& out, bool clear) {
#ifdef ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT
  if (clear) {
    xbone_auth_trace_clear();
    out.println(F("OK:XBAUTH_TRACE_CLEARED"));
    return;
  }

  static XboxOneAuthTraceEntry entries[64];
  const uint8_t count = xbone_auth_trace_snapshot(entries, 64);
  out.print(F("XBT COUNT="));
  out.println(count);
  for (uint8_t index = 0; index < count; ++index) {
    const XboxOneAuthTraceEntry& entry = entries[index];
    out.print(F("XBT I="));
    out.print(index);
    out.print(F(" T="));
    out.print(entry.timestamp_ms);
    out.print(F(" ST="));
    switch (entry.stage) {
      case XBONE_TRACE_CONSOLE_RAW: out.print(F("C-RX")); break;
      case XBONE_TRACE_CONSOLE_AGGREGATE: out.print(F("C-AGG")); break;
      case XBONE_TRACE_DONOR_RAW: out.print(F("D-RX")); break;
      case XBONE_TRACE_DONOR_AGGREGATE: out.print(F("D-AGG")); break;
      case XBONE_TRACE_TO_DONOR_RAW: out.print(F("D-TX")); break;
      case XBONE_TRACE_TO_CONSOLE_RAW: out.print(F("C-TX")); break;
      default: out.print(entry.stage); break;
    }
    out.print(F(" CMD=0x"));
    if (entry.command < 0x10) out.print('0');
    out.print(entry.command, HEX);
    out.print(F(" SEQ="));
    out.print(entry.sequence);
    out.print(F(" FL=0x"));
    if (entry.flags < 0x10) out.print('0');
    out.print(entry.flags, HEX);
    out.print(F(" W="));
    out.print(entry.wire_length);
    out.print(F(" D="));
    out.print(entry.data_length);
    out.print(F(" H="));
    const uint16_t available = entry.wire_length != 0
        ? entry.wire_length
        : entry.data_length;
    const uint8_t headLength = available < sizeof(entry.head)
        ? static_cast<uint8_t>(available)
        : static_cast<uint8_t>(sizeof(entry.head));
    for (uint8_t byte = 0; byte < headLength; ++byte) {
      if (entry.head[byte] < 0x10) out.print('0');
      out.print(entry.head[byte], HEX);
    }
    out.println();
  }
#else
  (void)clear;
  out.println(F("ERR:XBAUTH_TRACE_UNSUPPORTED"));
#endif
}
