#include "../product_config.h"

#include "serial_debug_runtime.h"
#include "serial_command_parser.h"
#include "serial_core_commands.h"
#include "serial_latency_commands.h"
#include "serial_management_commands.h"
#include "serial_memcard_commands.h"
#include "serial_neutral_frame_injection.h"
#include "serial_oled_commands.h"
#include "usb_poll_alignment.h"
#include "usb_latency_trace.h"
#include "serial_rumble_commands.h"
#include "runtime/runtime_serial_debug.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <Arduino.h>
#include <EEPROM.h>
#include <hardware/gpio.h>

#ifdef ADAPT_OUTPUT_USB_DEVICE
#include <Adafruit_TinyUSB.h>
#endif

#include "button_chord_remap.h"
#include "controller_frame_state.h"
#include "controller_settings_state.h"
#if defined(ENABLE_ESP32_RP2040_UPDATE)
#include "esp32_rp2040_update.h"
#endif
#if ADAPT_POLICY_CONTROLLER_FEEDBACK_COMMANDS
#include "controller_feedback_config.h"
#endif
#if defined(ENABLE_ESP32_RP2040_UPDATE)
#include "rp2040_update_rollback.h"
#endif
#include "device_runtime_state.h"
#include "firmware_support.h"
#include "hotkey_combo.h"
#include "oled_serial_runtime.h"
#include "settings_registry.h"
#include "settings_store.h"
#include "../features/feature_module.h"
#ifdef ENABLE_TTY2OLED_SERIAL
#include "../features/tty2oled/tty2oled_feature.h"
#endif
#include "../input/runtime/input_frame_runtime.h"
#ifdef ENABLE_INPUT_CDI
#include "../input/cdi/input_cdi_debug.h"
#endif
#ifdef ENABLE_INPUT_PCE
#include "../input/pce/input_pce_runtime_state.h"
#endif
#include "../input/runtime/input_module_runtime.h"
#ifdef ENABLE_INPUT_ESP32_SPI
#include "../input/esp32_spi/esp32_service_uart.h"
#endif
#ifdef ENABLE_INPUT_AUTODETECT
#include "../input/autodetect/Input_AutoDetect.h"
#include "../input/autodetect/input_autodetect_benchmark.h"
#endif
#include "../menu/quick_config.h"
#include "../menu/menu_bridge.h"
#include "../menu/menu_idle_runtime.h"
#include "../menu/menu_runtime_state.h"
#include "../menu/menu_system_screens.h"
#include "../menu/menu_ui_state.h"
#include "../output/auth/auth_storage.h"
#if defined(ENABLE_ESP32_RP2040_UPDATE)
#include "../output/bluetooth/modern2usb_bt_output.h"
#endif
#include "../output/output_runtime_state.h"
#include "../output/runtime/output_loop_runtime.h"
#if defined(ADAPT_OUTPUT_USB_DEVICE) && defined(ENABLE_XINPUT2P_WIRELESS_TRANSPORT)
#include "../output/xinputw/out_xinputw.h"
#endif
#if defined(ADAPT_OUTPUT_USB_DEVICE) && defined(ENABLE_EXPERIMENTAL_XINPUT2P_OUTPUT)
#endif
#include "../platform/runtime/platform_menu_runtime.h"
#if ADAPT_POLICY_RP2040_CRASH_TELEMETRY
#include "../platform/runtime/rp2040_crash_telemetry.h"
#endif
#ifdef USE_WS2812
#include "../platform/rgb_led.h"
#endif

#ifdef ENABLE_INPUT_PCE
#include "../input/pce/Input_Pce.h"
#include "../input/pce/input_pce_runtime_state.h"
#endif

#ifdef ENABLE_INPUT_SATURN
#include "../input/saturn/input_saturn_runtime_state.h"
#endif

#ifdef ENABLE_INPUT_SNES
#include "../input/snes/input_snes_runtime_state.h"
#endif

#ifdef ENABLE_INPUT_PSX
#include "../input/psx/Input_Psx.h"
#include "../input/psx/input_psx_runtime_state.h"
#endif

#ifdef ENABLE_INPUT_DREAMCAST
#include "../input/dreamcast/Input_Dreamcast.h"
#endif

#ifdef ENABLE_INPUT_WII
#include "../input/wii/input_wii_trace.h"
#endif

#ifdef ENABLE_INPUT_N64
#include "../input/gc64/Input_GC64.h"
#endif

#ifdef ENABLE_USB_AUTH_SIDECAR
#include "../input/usb_host/input_usb_host_service.h"
#include "../output/auth/ps_auth_dongle_runtime.h"
#ifdef ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT
#include "../output/auth/xbone_auth_passthrough.h"
#endif
#endif
#if defined(ENABLE_INPUT_USB) && !defined(ENABLE_USB_AUTH_SIDECAR)
#include "../input/usb_host/input_usb_host_service.h"
#endif

#ifdef ENABLE_XBOX_WIRELESS_ADAPTER_FIRMWARE_LAB
#include "../input/usb_host/usb_xbox_wireless_adapter.h"
#endif
namespace {

// A maximum update DATA command contains a 448-byte binary chunk encoded as
// 896 hexadecimal characters plus its command and offset.
constexpr size_t kDebugCommandBufferSize = 1024;
char debugUartCommandBuffer[kDebugCommandBufferSize];
size_t debugUartCommandLength = 0;
bool debugUartCommandDiscarding = false;
bool debugUartOledStreamActive = false;

#ifndef DEBUG_UART_BAUD
constexpr uint32_t kDebugUartBaud = 115200;
#else
constexpr uint32_t kDebugUartBaud = DEBUG_UART_BAUD;
#endif

void printAutodetectDebugLine(Print& out, uint8_t port) {
#ifdef ENABLE_INPUT_AUTODETECT
  char dbgLine[512];
  getAutoDetectSerialDebugLine(port, dbgLine, sizeof(dbgLine));
  out.print(F("AUTODETECT "));
  out.println(dbgLine);
  if (getAutoDetectJaguarSerialDebugLine(port, dbgLine, sizeof(dbgLine))) {
    out.print(F("AUTODETECT "));
    out.println(dbgLine);
  }
#else
  (void)out;
  (void)port;
#endif
}

bool handleAutodetectSerialCommand(const char* command, Print& out) {
#ifdef ENABLE_INPUT_AUTODETECT
  if (!serialTokenEquals(command, "AUTODETECT") &&
      !serialTokenEquals(command, "ADSCAN")) {
    return false;
  }

  const DeviceEnum detectedMode = runAutoDetection(true);
  out.print(F("AUTODETECT RESULT="));
  out.print(autoDetectResultName(autoDetectResult));
  out.print(F(" MODE="));
  out.print((int)detectedMode);
  out.print(F(" RAW="));
  out.println((int)autoDetectResult);
  printAutodetectDebugLine(out, 0);
  printAutodetectDebugLine(out, 1);
  out.println(F("OK:AUTODETECT"));
  return true;
#else
  (void)command;
  (void)out;
  return false;
#endif
}

bool handleThreeDoProbeSerialCommand(const char* command, Print& out) {
#if defined(ENABLE_INPUT_AUTODETECT) && defined(ENABLE_INPUT_3DO)
  if (!serialTokenEquals(command, "3DOPROBE")) {
    return false;
  }

  constexpr uint8_t kFrameCount = 4;
  for (uint8_t port = 0; port < kAutoDetectPortCount; ++port) {
    uint16_t frames[kFrameCount] = {};
    uint8_t idleLevel = 0;
    const uint8_t captured = AutoDetector::capture3doDiagnosticFrames(
        port, frames, kFrameCount, &idleLevel);
    uint8_t directValid = 0;
    uint8_t invertedValid = 0;

    out.print(F("3DOPROBE P="));
    out.print((int)(port + 1));
    out.print(F(" IDLE="));
    out.print((int)idleLevel);
    out.print(F(" FRAMES="));
    for (uint8_t i = 0; i < captured; ++i) {
      if (i != 0) out.print(',');
      if (frames[i] < 0x1000u) out.print('0');
      if (frames[i] < 0x0100u) out.print('0');
      if (frames[i] < 0x0010u) out.print('0');
      out.print(frames[i], HEX);
      if (frames[i] != 0xFFFFu && frames[i] != 0x0000u &&
          (frames[i] & 0xC007u) == 0x0001u) {
        ++directValid;
      }
      const uint16_t inverted = (uint16_t)~frames[i];
      if (inverted != 0xFFFFu && inverted != 0x0000u &&
          (inverted & 0xC007u) == 0x0001u) {
        ++invertedValid;
      }
    }
    out.print(F(" VALID="));
    out.print((int)directValid);
    out.print(F(" INV_VALID="));
    out.println((int)invertedValid);
  }
  out.println(F("OK:3DOPROBE"));
  return true;
#else
  (void)command;
  (void)out;
  return false;
#endif
}

bool handleLedSerialCommand(char* text, Print& out) {
#ifdef USE_WS2812
  if (*text == '\0' || serialTextEqualsExact(text, "STATUS")) {
    out.print(F("LED COUNT="));
    out.print((int)rgbLed.getLedCount());
    out.print(F(" MODE="));
    out.print((int)rgbLed.getMode());
    out.print(F(" BRIGHT="));
    out.print((int)rgbLed.getBrightness());
    for (uint8_t index = 0; index < rgbLed.getLedCount(); ++index) {
      const rgb_color_t color = rgbLed.getLedColor(index);
      out.print(F(" P"));
      out.print((int)index);
      out.print('=');
      out.print((int)color.r);
      out.print(',');
      out.print((int)color.g);
      out.print(',');
      out.print((int)color.b);
    }
#if ADAPT_POLICY_CONTROLLER_FEEDBACK_COMMANDS
    out.print(F(" PRESET="));
    out.print((int)deviceLedColorPreset());
#endif
    out.println();
    return true;
  }

  char* valueText = nullptr;
  if (serialCommandStartsWith(text, "MODE", &valueText)) {
    long value = -1;
    char* numeric = valueText;
    const bool numericMode =
        serialParseLongToken(numeric, &value) &&
        *serialSkipSpaces(numeric) == '\0';
    if (numericMode) {
      // Numeric mode accepted below.
    } else {
      value = -1;
    }
    if (!numericMode && serialTextEqualsExact(valueText, "OFF")) {
      value = LED_MODE_OFF;
    } else if (!numericMode && serialTextEqualsExact(valueText, "STATIC")) {
      value = LED_MODE_STATIC;
    } else if (!numericMode &&
               (serialTextEqualsExact(valueText, "BREATHING") ||
                serialTextEqualsExact(valueText, "BREATHE"))) {
      value = LED_MODE_BREATHING;
    } else if (!numericMode && serialTextEqualsExact(valueText, "RAINBOW")) {
      value = LED_MODE_RAINBOW;
    } else if (!numericMode && serialTextEqualsExact(valueText, "REACTIVE")) {
      value = LED_MODE_REACTIVE;
    } else if (!numericMode && serialTextEqualsExact(valueText, "RUMBLE")) {
      value = LED_MODE_RUMBLE;
    } else if (!numericMode && serialTextEqualsExact(valueText, "ANALOG")) {
      value = LED_MODE_ANALOG;
    } else if (!numericMode &&
               (serialTextEqualsExact(valueText, "HOST") ||
                serialTextEqualsExact(valueText, "HOST COLOR") ||
                serialTextEqualsExact(valueText, "HOSTCOLOR") ||
                serialTextEqualsExact(valueText, "USBPORTS") ||
                serialTextEqualsExact(valueText, "USB PORTS"))) {
      value = LED_MODE_USB_PORTS;
    }
    if (value < LED_MODE_OFF || value >= LED_MODE_LAST) {
      out.println(F("ERR:BAD_LED_MODE"));
      return true;
    }
    led_mode = static_cast<uint8_t>(value);
    saveSettingValue(SettingId::LedMode, led_mode, deviceMode);
    rgbLed.setMode(static_cast<led_mode_t>(led_mode));
    rgbLed.setEnabled(led_mode != LED_MODE_OFF);
    out.print(F("OK:LED_MODE="));
    out.print(value);
    out.print(F(" NAME="));
    out.println(getLedModeName(static_cast<led_mode_t>(value)));
    return true;
  }

  if (serialCommandStartsWith(text, "BRIGHTNESS", &valueText) ||
      serialCommandStartsWith(text, "BRIGHT", &valueText)) {
    long value = -1;
    if (!serialParseLongToken(valueText, &value) ||
        *serialSkipSpaces(valueText) != '\0' || value < 0 || value > 254) {
      out.println(F("ERR:BAD_LED_BRIGHTNESS"));
      return true;
    }
    led_brightness = static_cast<uint8_t>(value);
    saveSettingValue(SettingId::LedBrightness, led_brightness, deviceMode);
    rgbLed.setBrightness(led_brightness);
    out.print(F("OK:LED_BRIGHTNESS="));
    out.println(value);
    return true;
  }

#if ADAPT_POLICY_CONTROLLER_FEEDBACK_COMMANDS
  if (serialCommandStartsWith(text, "COLOR", &valueText) ||
      serialCommandStartsWith(text, "PRESET", &valueText)) {
    long value = -1;
    char* numeric = valueText;
    const bool numericPreset =
        serialParseLongToken(numeric, &value) &&
        *serialSkipSpaces(numeric) == '\0';
    if (numericPreset) {
      // Numeric preset accepted below.
    } else {
      value = -1;
    }
    if (!numericPreset && serialTextEqualsExact(valueText, "BLUE")) {
      value = 0;
    } else if (!numericPreset && serialTextEqualsExact(valueText, "RED")) {
      value = 1;
    } else if (!numericPreset && serialTextEqualsExact(valueText, "GREEN")) {
      value = 2;
    } else if (!numericPreset && serialTextEqualsExact(valueText, "PINK")) {
      value = 3;
    } else if (!numericPreset && serialTextEqualsExact(valueText, "WHITE")) {
      value = 4;
    } else if (!numericPreset && serialTextEqualsExact(valueText, "ORANGE")) {
      value = 5;
    } else if (!numericPreset && serialTextEqualsExact(valueText, "CYAN")) {
      value = 6;
    } else if (!numericPreset && serialTextEqualsExact(valueText, "PURPLE")) {
      value = 7;
    }
    if (value < 0 || value > 7) {
      out.println(F("ERR:BAD_LED_COLOR"));
      return true;
    }
    setDeviceLedColorPreset(static_cast<uint8_t>(value));
    controllerFeedbackConfigSave();
    const ControllerFeedbackColor color = deviceLedColor();
    rgbLed.setBaseColor({color.r, color.g, color.b});
    out.print(F("OK:LED_COLOR="));
    out.print(value);
    out.print(F(" RGB="));
    out.print((int)color.r);
    out.print(',');
    out.print((int)color.g);
    out.print(',');
    out.println((int)color.b);
    return true;
  }
#endif

  char* target = nullptr;
  if (serialCommandStartsWith(text, "TEST", &target)) {
    uint8_t mask = 0;
    const char* label = nullptr;
    if (serialTextEqualsExact(target, "0")) {
      mask = 0x01;
      label = "LED0_RED";
    } else if (serialTextEqualsExact(target, "1")) {
      mask = 0x02;
      label = "LED1_BLUE";
    } else if (serialTextEqualsExact(target, "ALL")) {
      mask = 0xFF;
      label = "ALL";
    } else if (serialTextEqualsExact(target, "OFF")) {
      label = "OFF";
    } else {
      out.println(F("ERR:LED_TEST_TARGET"));
      return true;
    }
    rgbLed.showDebugPattern(mask);
    out.print(F("OK:LED_TEST "));
    out.println(label);
    return true;
  }

  out.print(F("LED CMDS:LED STATUS,LED MODE <OFF|STATIC|BREATHING|RAINBOW|REACTIVE|RUMBLE|ANALOG|HOST>,LED BRIGHTNESS <0-254>"));
#if ADAPT_POLICY_CONTROLLER_FEEDBACK_COMMANDS
  out.print(F(",LED COLOR <BLUE|RED|GREEN|PINK|WHITE|ORANGE|CYAN|PURPLE|0-7>"));
#endif
  out.println(F(",LED TEST <0|1|ALL|OFF>"));
  return true;
#else
  (void)text;
  out.println(F("ERR:LED_UNAVAILABLE"));
  return true;
#endif
}

#if defined(ENABLE_ESP32_RP2040_UPDATE)
int8_t cdcUpdateHexNibble(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  value = static_cast<char>(toupper(static_cast<unsigned char>(value)));
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  return -1;
}

bool parseCdcUpdateMd5(char** text, uint8_t md5[16]) {
  char* start = serialSkipSpaces(*text);
  char* end = start;
  while (*end != '\0' && *end != ' ' && *end != '\t') ++end;
  if (static_cast<size_t>(end - start) != 32) return false;
  for (uint8_t index = 0; index < 16; ++index) {
    const int8_t high = cdcUpdateHexNibble(start[index * 2]);
    const int8_t low = cdcUpdateHexNibble(start[index * 2 + 1]);
    if (high < 0 || low < 0) return false;
    md5[index] = static_cast<uint8_t>((high << 4) | low);
  }
  *text = serialSkipSpaces(end);
  return **text == '\0';
}

bool parseCdcUpdateData(char** text, uint8_t* data, size_t* length) {
  char* start = serialSkipSpaces(*text);
  char* end = start;
  while (*end != '\0' && *end != ' ' && *end != '\t') ++end;
  const size_t characters = static_cast<size_t>(end - start);
  if (characters == 0 || (characters & 1u) != 0 ||
      characters / 2 > RP2040_CDC_UPDATE_MAX_CHUNK) {
    return false;
  }
  for (size_t index = 0; index < characters / 2; ++index) {
    const int8_t high = cdcUpdateHexNibble(start[index * 2]);
    const int8_t low = cdcUpdateHexNibble(start[index * 2 + 1]);
    if (high < 0 || low < 0) return false;
    data[index] = static_cast<uint8_t>((high << 4) | low);
  }
  end = serialSkipSpaces(end);
  if (*end != '\0') return false;
  *length = characters / 2;
  *text = end;
  return true;
}

void printCdcUpdateStatus(Print& out) {
  out.print(F("FWUPDATE CDC ACTIVE="));
  out.print(esp32Rp2040UpdateDirectActive() ? 1 : 0);
  out.print(F(" OFFSET="));
  out.print(esp32Rp2040UpdateDirectOffset());
  out.print(F(" TOTAL="));
  out.print(esp32Rp2040UpdateDirectTotal());
  out.print(F(" ERROR="));
  out.print(esp32Rp2040UpdateLastError());
  out.print(F(" MAX="));
  out.println(RP2040_CDC_UPDATE_MAX_CHUNK);
}

// D2-03: the new image's health trial needs the ESP32 link, so BEGIN is
// refused while it is down; say what to do instead.
void printCdcUpdateRefusal(Print& out) {
  const uint8_t error = esp32Rp2040UpdateLastError();
  if (error == ESP32_SPI_RP_UPDATE_ERROR_BRIDGE_DOWN) {
    out.println(F("FWUPDATE CDC REFUSED=ESP32_LINK_DOWN "
                  "Update or repair the ESP32 first, then the RP2040. "
                  "BOOTSEL UF2 works without the ESP32."));
  } else if (error == ESP32_SPI_RP_UPDATE_ERROR_PEER_TRIAL) {
    out.println(F("FWUPDATE CDC REFUSED=ESP32_TRIAL "
                  "Retry after the ESP32 confirms its new firmware."));
  }
}

bool handleFirmwareUpdateSerialCommand(const char* command, Print& out) {
  char* remainder = nullptr;
  if (!serialCommandStartsWith(command, "FWUPDATE", &remainder)) {
    return false;
  }

  if (*remainder == '\0' || serialTokenEquals(remainder, "STATUS")) {
    out.print(F("FWUPDATE ACTIVE="));
    out.println(firmwareUpdateScreenActive() ? 1 : 0);
    return true;
  }
  if (serialTextEqualsExact(remainder, "ROLLBACK STATUS")) {
    out.print(F("FWUPDATE ROLLBACK NOTICE="));
    out.println(rp2040UpdateRollbackNoticePending() ? 1 : 0);
    return true;
  }
  if (serialTextEqualsExact(remainder, "ROLLBACK DIAG")) {
    out.print(F("FWUPDATE"));
    rp2040UpdateRollbackWriteStatus(out);
    out.println();
    return true;
  }
  if (serialTextEqualsExact(remainder, "ROLLBACK ACK")) {
    out.println(F("ERR:CONFIRM_REQUIRED"));
    return true;
  }
  if (serialTextEqualsExact(remainder, "ROLLBACK ACK CONFIRM")) {
    out.println(rp2040UpdateRollbackAcknowledgeNotice()
                    ? F("OK:FWUPDATE ROLLBACK ACK")
                    : F("ERR:FWUPDATE_ROLLBACK_ACK"));
    return true;
  }
  if (serialTokenEquals(remainder, "CLEAR")) {
    clearFirmwareUpdateScreen();
    out.println(F("OK:FWUPDATE CLEAR"));
    return true;
  }

  FirmwareUpdateTarget target = FirmwareUpdateTarget::None;
  char* phaseText = nullptr;
  if (serialCommandStartsWith(remainder, "RP2040", &phaseText)) {
    target = FirmwareUpdateTarget::Rp2040;
  } else if (serialCommandStartsWith(remainder, "ESP32", &phaseText)) {
    target = FirmwareUpdateTarget::Esp32;
  } else {
    out.println(F("ERR:BAD_FWUPDATE_TARGET"));
    return true;
  }

  FirmwareUpdatePhase phase = FirmwareUpdatePhase::Preparing;
  const char* phaseReply = nullptr;
  if (serialTokenEquals(phaseText, "PREPARE") ||
      serialTokenEquals(phaseText, "PREPARING")) {
    phase = FirmwareUpdatePhase::Preparing;
    phaseReply = "PREPARE";
  } else if (serialTokenEquals(phaseText, "PROGRAM") ||
             serialTokenEquals(phaseText, "PROGRAMMING") ||
             serialTokenEquals(phaseText, "FLASH")) {
    phase = FirmwareUpdatePhase::Programming;
    phaseReply = "PROGRAM";
  } else if (serialTokenEquals(phaseText, "VERIFY") ||
             serialTokenEquals(phaseText, "VERIFYING")) {
    phase = FirmwareUpdatePhase::Verifying;
    phaseReply = "VERIFY";
  } else if (serialTokenEquals(phaseText, "COMPLETE") ||
             serialTokenEquals(phaseText, "DONE")) {
    phase = FirmwareUpdatePhase::Complete;
    phaseReply = "COMPLETE";
  } else if (serialTokenEquals(phaseText, "FAILED") ||
             serialTokenEquals(phaseText, "ERROR")) {
    phase = FirmwareUpdatePhase::Failed;
    phaseReply = "FAILED";
  } else {
    out.println(F("ERR:BAD_FWUPDATE_PHASE"));
    return true;
  }

  // Deployment status owns the display until completion. Closing the settings
  // UI also prevents a menu redraw from replacing the message mid-flash.
  // Keep native management USB attached for this entire boot, including a
  // direct/already-mounted BOOTSEL migration that had no prior firmware to
  // leave the watchdog update marker.
  serialNeutralFrameInjectionCancel();
  modern2usbBtOutputSetFirmwareUpdateHold(true);
  isQuickConfigOpen = false;
  closeMenu();
  showFirmwareUpdateScreen(target, phase);
  out.print(F("OK:FWUPDATE "));
  out.print(target == FirmwareUpdateTarget::Rp2040 ? F("RP2040 ") : F("ESP32 "));
  out.println(phaseReply);
  return true;
}

#else
bool handleFirmwareUpdateSerialCommand(const char*, Print&) { return false; }
#endif

bool handleXinputSerialCommand(const char* command, Print& out) {
  char* remainder = nullptr;
  if (!serialCommandStartsWith(command, "XINPUT", &remainder)) {
    return false;
  }

  if (*remainder != '\0' && !serialTokenEquals(remainder, "STATUS")) {
    out.println(F("ERR:BAD_XINPUT_CMD"));
    return true;
  }

  out.print(F("XINPUT MODE="));
  out.print((int)get_effective_output_mode());
#ifdef ADAPT_OUTPUT_USB_DEVICE
  out.print(F(" MOUNT="));
  out.print(tud_mounted() ? 1 : 0);
#endif
#if defined(ADAPT_OUTPUT_USB_DEVICE) && defined(ENABLE_XINPUT2P_WIRELESS_TRANSPORT)
  out.println(F(" TRANSPORT=RECEIVER"));
  XInputWDiagInfo info{};
  xinputw_get_diag_info(&info);
  for (uint8_t slot = 0; slot < info.controller_count; ++slot) {
    out.print(F("XISLOT I="));
    out.print((int)slot);
    out.print(F(" WANT="));
    out.print((int)info.desired_connected[slot]);
    out.print(F(" STATE="));
    out.print((int)info.info_state[slot]);
    out.print(F(" EP="));
    out.print(info.endpoint_in[slot], HEX);
    out.print('/');
    out.print(info.endpoint_out[slot], HEX);
    out.print(F(" IN="));
    out.print(info.in_xfer_count[slot]);
    out.print(F(" CONN="));
    out.print(info.connection_status_count[slot]);
    out.print(F(" PAD="));
    out.print(info.controller_report_count[slot]);
    out.print(F(" OUT="));
    out.println(info.out_xfer_count[slot]);
  }
#else
  out.println(F(" TRANSPORT=OTHER"));
#endif
  return true;
}


}  // namespace
#if defined(ENABLE_ESP32_RP2040_UPDATE)
bool handleCdcFirmwareUpdateSerialCommand(const char* command, Print& out) {
  char* remainder = nullptr;
  if (!serialCommandStartsWith(command, "FWUPDATE CDC", &remainder)) {
    return false;
  }
  if (*remainder == '\0' || serialTextEqualsExact(remainder, "STATUS")) {
    printCdcUpdateStatus(out);
    return true;
  }

  char* arguments = nullptr;
  if (serialCommandStartsWith(remainder, "BEGIN", &arguments)) {
    uint32_t packageSize = 0;
    uint8_t md5[16] = {};
    if (!serialParseUint32Token(arguments, &packageSize) ||
        !parseCdcUpdateMd5(&arguments, md5)) {
      out.println(F("ERR:BAD_FWUPDATE_CDC_BEGIN"));
      return true;
    }
    const bool updateBusy = esp32Rp2040UpdateActive();
    if (updateBusy ||
        !esp32Rp2040UpdateBeginDirect(packageSize, md5)) {
      out.println(F("ERR:FWUPDATE_CDC_BEGIN"));
      if (!updateBusy) printCdcUpdateRefusal(out);
      printCdcUpdateStatus(out);
      return true;
    }
    out.print(F("OK:FWUPDATE_CDC READY OFFSET=0 MAX="));
    out.println(RP2040_CDC_UPDATE_MAX_CHUNK);
    return true;
  }

  if (serialCommandStartsWith(remainder, "DATA", &arguments)) {
    static uint8_t chunk[RP2040_CDC_UPDATE_MAX_CHUNK];
    uint32_t offset = 0;
    size_t length = 0;
    if (!serialParseUint32Token(arguments, &offset) ||
        !parseCdcUpdateData(&arguments, chunk, &length)) {
      out.println(F("ERR:BAD_FWUPDATE_CDC_DATA"));
      return true;
    }
    if (!esp32Rp2040UpdateWriteDirect(offset, chunk, length)) {
      out.println(F("ERR:FWUPDATE_CDC_WRITE"));
      printCdcUpdateStatus(out);
      return true;
    }
    out.print(F("OK:FWUPDATE_CDC DATA OFFSET="));
    out.println(esp32Rp2040UpdateDirectOffset());
    return true;
  }

  if (serialTextEqualsExact(remainder, "END")) {
    if (!esp32Rp2040UpdateFinishDirect()) {
      out.println(F("ERR:FWUPDATE_CDC_END"));
      printCdcUpdateStatus(out);
      return true;
    }
    out.println(F("OK:FWUPDATE_CDC COMPLETE"));
    out.flush();
    return true;
  }

  if (serialTextEqualsExact(remainder, "ABORT")) {
    if (!esp32Rp2040UpdateDirectActive()) {
      out.println(F("OK:FWUPDATE_CDC IDLE"));
    } else if (esp32Rp2040UpdateAbortDirect()) {
      out.println(F("OK:FWUPDATE_CDC ABORT"));
    } else {
      out.println(F("ERR:FWUPDATE_CDC_ABORT"));
    }
    return true;
  }

  out.println(F("ERR:BAD_FWUPDATE_CDC_CMD"));
  return true;
}

#else
bool handleCdcFirmwareUpdateSerialCommand(const char*, Print&) { return false; }
#endif


void serialDebugRuntimeSetup() {
#ifdef ENABLE_INPUT_ESP32_SPI
  esp32ServiceUartSetup();
#endif
#if defined(ENABLE_DEBUG_UART) && defined(PIN_DEBUG_UART_TX) && defined(PIN_DEBUG_UART_RX)
  Serial2.setTX(PIN_DEBUG_UART_TX);
  Serial2.setRX(PIN_DEBUG_UART_RX);
  Serial2.setFIFOSize(4096);
  Serial2.begin(kDebugUartBaud);
#endif
}

void serialDebugRuntimeTask() {
#ifdef ENABLE_INPUT_ESP32_SPI
  esp32ServiceUartTask();
#endif
#if defined(ENABLE_DEBUG_UART) && defined(PIN_DEBUG_UART_TX) && defined(PIN_DEBUG_UART_RX)
  bool sawSerialActivity = false;
  while (Serial2.available() > 0) {
    const int raw = Serial2.read();
    if (raw < 0) {
      break;
    }
    sawSerialActivity = true;

#ifdef ENABLE_TTY2OLED_SERIAL
    if (tty2oledSerialDrainByte((uint8_t)raw, Serial2)) {
      continue;
    }
#endif

    const char ch = (char)raw;
    const SerialCommandLineResult lineResult = serialCommandLinePush(
        ch, debugUartCommandBuffer, kDebugCommandBufferSize,
        &debugUartCommandLength, &debugUartCommandDiscarding);
    if (lineResult == SERIAL_COMMAND_LINE_OVERFLOW) {
      Serial2.println(F("ERR:CMD_TOO_LONG"));
      continue;
    }
    if (lineResult != SERIAL_COMMAND_LINE_READY) {
      continue;
    }
    const bool oledCommand = serialIsOledCommand(debugUartCommandBuffer);
    if (!handleSerialDebugCommand(debugUartCommandBuffer, Serial2)) {
      Serial2.println(F("ERR:UNKNOWN_CMD"));
    }
    if (oledCommand) {
      debugUartOledStreamActive = oledSerialIsEnabled();
    }
    if (lineResult == SERIAL_COMMAND_LINE_READY) {
      debugUartCommandLength = 0;
    }
  }
  if (sawSerialActivity) {
    resetIdleTimer(IdleWakeSource::Serial);
  }
  if (debugUartOledStreamActive) {
    oledSerialTask(Serial2);
  }
#endif
}

bool handleSerialDebugCommand(const char* command, Print& out) {
  char* remainder = nullptr;
#if ADAPT_POLICY_RP2040_CRASH_TELEMETRY
  if (rp2040CrashTelemetryHandleSerialCommand(command, out)) {
    return true;
  }
#endif
  if (usbPollAlignmentHandleSerialCommand(command, out)) {
    return true;
  }
  if (usbLatencyTraceHandleSerialCommand(command, out)) {
    return true;
  }
  if (handleCdcFirmwareUpdateSerialCommand(command, out)) {
    return true;
  }
  if (handleFirmwareUpdateSerialCommand(command, out)) {
    return true;
  }
  if (handleXinputSerialCommand(command, out)) {
    return true;
  }
#ifdef ENABLE_INPUT_ESP32_SPI
  if (esp32ServiceUartHandleCommand(command, out)) {
    return true;
  }
#endif
#ifdef ENABLE_USB_AUTH_SIDECAR
#ifdef ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT
  if (serialTextEqualsExact(command, "AUTH TRACE CLEAR") ||
      serialTextEqualsExact(command, "XBAUTH TRACE CLEAR")) {
    runtimeSerialWriteXboxOneAuthTrace(out, true);
    return true;
  }
  if (serialTextEqualsExact(command, "AUTH TRACE") ||
      serialTextEqualsExact(command, "XBAUTH TRACE")) {
    runtimeSerialWriteXboxOneAuthTrace(out, false);
    return true;
  }
  if (serialTextEqualsExact(command, "XBAUTH WAKE") ||
      serialTextEqualsExact(command, "AUTH XBOX WAKE")) {
    out.println(xbone_auth_passthrough_wake()
                    ? F("OK:XBAUTH_WAKE_QUEUED")
                    : F("ERR:XBAUTH_WAKE_NOT_READY"));
    runtimeSerialWriteAuthSidecarStatus(out);
    return true;
  }
#endif
  if (serialTextEqualsExact(command, "AUTH START") ||
      serialTextEqualsExact(command, "PSAUTH START")) {
    usb_host_service_request_start();
    out.println(F("OK:USBHOST_START_REQUESTED"));
    runtimeSerialWriteAuthSidecarStatus(out);
    return true;
  }
#ifdef ENABLE_USB_AUTH_SERIAL_TRACE
  if (serialTextEqualsExact(command, "AUTH P5SIM") ||
      serialTextEqualsExact(command, "PSAUTH P5SIM") ||
      serialTextEqualsExact(command, "PS5SIM")) {
    ps_auth_dongle_debug_ps5_simulate(out);
    runtimeSerialWriteAuthSidecarStatus(out);
    return true;
  }
#endif
#if ADAPT_POLICY_PS5_DIAGNOSTIC_RUNTIME
  if (serialTextEqualsExact(command, "AUTH PS5") ||
      serialTextEqualsExact(command, "PSAUTH PS5") ||
      serialTextEqualsExact(command, "PS5DIAG ARM")) {
    ps5_diag_arm_next_boot();
    configuredOutputMode = OUTPUT_PS5;
    outputMode = sanitizeRuntimeOutputMode(configuredOutputMode);
    out.println(F("OK:PS5_DIAG_ARMED_REBOOT"));
    delay(100);
    reboot();
    return true;
  }
#endif
  if (serialTextEqualsExact(command, "AUTH") ||
      serialTextEqualsExact(command, "PSAUTH")) {
    runtimeSerialWriteAuthSidecarStatus(out);
    return true;
  }
#endif
  if (handleSerialManagementCommand(command, out)) {
    return true;
  }
#if defined(ENABLE_INPUT_USB) || defined(ENABLE_USB_AUTH_SIDECAR)
#ifdef ENABLE_XBOX_WIRELESS_ADAPTER_FIRMWARE_LAB
  if (serialTextEqualsExact(command, "USBHOST XWPAIR") ||
      serialTextEqualsExact(command, "USBHOST XWPAIR START")) {
    out.println(usb_xbox_wireless_adapter_set_pairing(true)
        ? F("OK:USBHOST_XWPAIR_STARTED")
        : F("ERR:USBHOST_XWPAIR_NOT_READY"));
    usb_host_service_write_compact_status(out);
    return true;
  }
  if (serialTextEqualsExact(command, "USBHOST XWPAIR CANCEL") ||
      serialTextEqualsExact(command, "USBHOST XWPAIR STOP")) {
    out.println(usb_xbox_wireless_adapter_set_pairing(false)
        ? F("OK:USBHOST_XWPAIR_CANCELLED")
        : F("ERR:USBHOST_XWPAIR_NOT_READY"));
    usb_host_service_write_compact_status(out);
    return true;
  }
  if (serialTextEqualsExact(command, "USBHOST XWPAIR STATUS")) {
    usb_host_service_write_compact_status(out);
    return true;
  }
#endif
  if (serialTextEqualsExact(command, "USBHOST START")) {
    usb_host_service_request_start();
    out.println(F("OK:USBHOST_START_REQUESTED"));
    usb_host_service_write_compact_status(out);
    return true;
  }
  if (serialCommandStartsWith(command, "USBHOST PORT RESET", &remainder)) {
    long port = 0;
    if (!serialParseLongToken(remainder, &port) ||
        *serialSkipSpaces(remainder) != '\0' || port < 1 || port > 4) {
      out.println(F("ERR:BAD_USBHOST_PORT"));
      return true;
    }
    out.println(usb_host_service_force_hub_port_reset(
                    static_cast<uint8_t>(port))
        ? F("OK:USBHOST_PORT_RESET_QUEUED")
        : F("ERR:USBHOST_PORT_RESET_BUSY"));
    usb_host_service_write_compact_status(out);
    return true;
  }
  if (serialTextEqualsExact(command, "USBHOST RESET")) {
    out.println(usb_host_service_force_root_reset()
        ? F("OK:USBHOST_RESET")
        : F("ERR:USBHOST_NOT_STARTED"));
    usb_host_service_write_compact_status(out);
    return true;
  }
  if (serialTextEqualsExact(command, "USBHOST RATE RESET")) {
    usb_host_service_reset_report_rate();
    out.println(F("OK:USBHOST_RATE_RESET"));
    usb_host_service_write_report_rate(out, false);
    return true;
  }
  if (serialTextEqualsExact(command, "USBHOST RATE")) {
    usb_host_service_write_report_rate(out);
    return true;
  }
  if (serialTextEqualsExact(command, "USBHOST") ||
      serialTextEqualsExact(command, "USBHOST STATUS")) {
    usb_host_service_write_compact_status(out);
    return true;
  }
#endif
  if (serialCommandStartsWith(command, "SET", &remainder)) {
    return handleSerialSetCommand(remainder, out);
  }
  if (serialCommandStartsWith(command, "NFRAME", &remainder)) {
    return handleSerialNeutralFrameInjectionCommand(remainder, out);
  }
  if (serialCommandStartsWith(command, "HOTKEY", &remainder)) {
    return handleSerialHotkeyCommand(remainder, out);
  }
  if (serialCommandStartsWith(command, "CHORD", &remainder)) {
    return handleSerialChordCommand(remainder, out);
  }
  if (serialCommandStartsWith(command, "LATENCY", &remainder) ||
      serialCommandStartsWith(command, "LAT", &remainder)) {
    return handleSerialLatencyCommand(remainder, out);
  }
  if (serialCommandStartsWith(command, "RUMBLE", &remainder)) {
    return handleSerialRumbleCommand(remainder, out);
  }
  if (serialCommandStartsWith(command, "LED", &remainder)) {
    return handleLedSerialCommand(remainder, out);
  }
  if (handleSerialDreamcastCommand(command, out)) {
    return true;
  }
#ifdef ENABLE_INPUT_WII
  if (handleWiiTraceCommand(command, out)) {
    return true;
  }
#endif
#ifdef ADAPT_FEATURE_SERIAL_MEMCARD_API
  if (serialCommandStartsWith(command, "CARD", &remainder)) {
    return handleSerialMemcardCommand(remainder, out);
  }
#endif
  if (featureModulesHandleSerialCommand(command, out)) {
    return true;
  }
#ifdef ENABLE_INPUT_AUTODETECT
  if (serialCommandStartsWith(command, "ADBENCH", &remainder)) {
    return handleAutoDetectBenchmarkCommand(remainder, out);
  }
#endif
#if defined(ENABLE_INPUT_PCE) && defined(ENABLE_EXPERIMENTAL_CONSOLE_MOUSE)
  if (serialCommandStartsWith(command, "PCEM", &remainder)) {
    return input_pce_handle_mouse_serial_command(remainder, out);
  }
#endif
  if (handleAutodetectSerialCommand(command, out)) {
    return true;
  }
  if (handleThreeDoProbeSerialCommand(command, out)) {
    return true;
  }
#ifdef ENABLE_INPUT_PSX
  if (serialCommandStartsWith(command, "FISHCAP", &remainder)) {
    RZInputPSX* psx = currentPsxInputModule();
    if (psx == nullptr) out.println(F("ERR:FISHCAP_NO_PSX"));
    else psx->handleFishingCaptureCommand(remainder, out);
    return true;
  }
  if (serialTokenEquals(command, "PSXDIAG")) {
    RZInputPSX* psx = currentPsxInputModule();
    if (psx == nullptr) {
      out.println(F("PSXDIAG NONE"));
    } else {
      psx->printDiagnostics(out);
    }
    return true;
  }
  if (serialTokenEquals(command, "PSXSTAT") ||
      serialTokenEquals(command, "PSX STATUS")) {
    RZInputPSX* psx = currentPsxInputModule();
    if (psx == nullptr) {
      out.println(F("STATUS PSX NONE"));
    } else {
      psx->printDebugStatus(out);
    }
    return true;
  }
  if (serialTokenEquals(command, "PSXPROBE")) {
    RZInputPSX* psx = currentPsxInputModule();
    if (psx == nullptr) {
      out.println(F("PSXPROBE NONE"));
    } else {
      psx->printDebugProbe(out);
    }
    return true;
  }
#endif
#ifdef ENABLE_INPUT_CDI
  if (serialTokenEquals(command, "CDISTAT")) {
    printCdiStatus(out);
    return true;
  }
#endif
  if (serialCommandStartsWith(command, "UI", &remainder)) {
    return handleSerialUiCommand(remainder, out);
  }
  if (handleSerialOledCommand(command, out)) {
    return true;
  }
  if (handleSerialBootCommand(command, out) ||
      handleSerialStateCommand(command, out) ||
      handleSerialGpioCommand(command, out)) {
    return true;
  }
  if (serialTokenEquals(command, "DHELP") ||
      serialTokenEquals(command, "DEBUG HELP")) {
    out.print(F("DEBUG CMDS:SET,HOTKEY,CHORD,LATENCY,RUMBLE,LED,FWUPDATE,XINPUT STATUS,STATE,NFRAME,GPIO,CRASH STATUS,RESET,BOOT"));
    appendSerialManagementHelp(out);
#if defined(ENABLE_INPUT_USB) || defined(ENABLE_USB_AUTH_SIDECAR)
    out.print(F(",USBHOST,USBHOST START,USBHOST RESET,USBHOST PORT RESET <1-4>,USBHOST RATE,USBHOST RATE RESET,USBHOST ALIGN [STATUS|ON|OFF|RESET]"));
#if defined(ENABLE_USB_LATENCY_SEGMENT_TRACE)
    out.print(F(",USBHOST LATENCY [STATUS|ON|OFF|RESET]"));
#endif
#endif
#ifdef ENABLE_USB_AUTH_SIDECAR
    out.print(F(",AUTH,PSAUTH"));
#ifdef ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT
    out.print(F(",XBAUTH WAKE"));
#endif
#endif
#ifdef ENABLE_INPUT_ESP32_SPI
    esp32ServiceUartAppendHelp(out);
#endif
#ifdef ENABLE_INPUT_DREAMCAST
    out.print(F(",DCSTAT"));
#endif
#ifdef ENABLE_INPUT_PSX
    out.print(F(",PSXSTAT,PSXDIAG,PSXPROBE,FISHCAP"));
#endif
#ifdef ENABLE_INPUT_CDI
    out.print(F(",CDISTAT"));
#endif
#ifdef ENABLE_INPUT_WII
    out.print(F(",WII TRACE"));
#endif
#ifdef ADAPT_FEATURE_SERIAL_MEMCARD_API
    out.print(F(",CARD"));
#endif
    out.print(F(",AUTODETECT,ADSCAN,3DOPROBE,UI <MENU|UP|DOWN|LEFT|RIGHT|OK|BACK|RESET>,OLED,OLED ON,OLED OFF,OLED RATE <HZ>,OLED FRAME"));
    featureModulesAppendSerialHelp(out);
    out.println();
    return true;
  }
  return false;
}
