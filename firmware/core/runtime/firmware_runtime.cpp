#include "../../product_config.h"

// Override TinyUSB defaults BEFORE any includes that might pull in TinyUSB.
// This translation unit owns the header-only USB output stack.
#if defined(ADAPT_OUTPUT_USB_DEVICE)
#define CFG_TUD_HID 6  // 6-player multitap support (Saturn, etc.)
#endif

#include "firmware_runtime.h"

#include "../device_runtime_state.h"
#include "../../features/jogcon_capture/jogcon_capture.h"
#include "../../features/n64_capture/n64_capture.h"
#include "../../features/host_capture/host_capture.h"

#include <Arduino.h>

// Input auto-detect probe diagnostics (OLED + USB CDC serial).
// Keep enabled while debugging Saturn/Jaguar mis-detection loops.
#ifndef AUTODETECT_DEBUG
  #define AUTODETECT_DEBUG
#endif

// https://github.com/DavidPagels/retro-pico-switch
//
// Switch Pro Rumble
// https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/rumble_data_table.md
// https://github.com/ndeadly/MissionControl
//
// XID (ogXbox) output
// https://github.com/Ryzee119/ogx360_t4
//
// Xinput output
// https://github.com/JonnyHaystack/Adafruit_TinyUSB_XInput/tree/master
//
// PS3 output
// https://github.com/matlo/GIMX-firmwares/blob/master/EMUPS3/
//
// PS4 auth: BearSSL RSA-PSS-SHA256 (ps4_auth.h), no USB host dongle needed
// Keys uploaded via WebHID (1712 bytes: serial + cert + RSA key components)

#include "../../firmware_platform_config.h"

// Input module gates are now set by product_config.h
// To override for development, uncomment individual lines below:
//#define ENABLE_INPUT_DUMMY
//#define ENABLE_INPUT_CUSTOM
//#define ENABLE_INPUT_GBA
//#define ENABLE_INPUT_PSX_DANCE
//#define ENABLE_INPUT_DREAMCAST
//#define ENABLE_INPUT_INTV
//#define ENABLE_INPUT_PADDLE
//#define ENABLE_INPUT_GAMEPORT
//#define ENABLE_INPUT_MEMCARD

#if defined(ENABLE_INPUT_USB)
// MODERN2USB targets use Pico-PIO-USB for RP2040 USB host input.
#include "pio_usb.h"
#endif
#include <Adafruit_TinyUSB.h>

#include "hardware/watchdog.h"

// Verify CFG_TUD_HID is 6 for 6-player multitap support
#if defined(ADAPT_OUTPUT_USB_DEVICE)
static_assert(CFG_TUD_HID == 6, "CFG_TUD_HID must be 6 for 6-player multitap support");
#endif
#if ADAPT_POLICY_USB_HOST_ENUMERATION_BUFFER_SIZE > 0
static_assert(
    CFG_TUH_ENUMERATION_BUFSIZE == ADAPT_POLICY_USB_HOST_ENUMERATION_BUFFER_SIZE,
    "MODERN2USB requires a 1343-byte TinyUSB enumeration buffer for composite XInput/audio controllers");
#endif

// EEPROM - include early so input modules and out_usb.h can access it
#include <EEPROM.h>
#include "../eeprom_helper.h"
#include "../controller_settings_state.h"
#include "../../menu/menu_runtime_state.h"
#include "../settings_store.h"
#include "../firmware_support.h"
#include "../../platform/display_runtime_state.h"

//#include "../../input/shared/input_master.h"
#include "../../input/autodetect/input_autodetect_runtime_state.h"
#include "../../input/autodetect/input_autodetect_benchmark.h"
#include "../../output/autodetect/output_autodetect_diagnostics.h"
#include "../../output/usb/out_usb.h"
#if ADAPT_POLICY_TOUCHPAD_MOUSE_TELEMETRY
#include "../../output/usb/output_touchpad_mouse_debug.h"
#endif
#include "../../platform/buzzer.h"
#include "../../platform/latency_test.h"
#include "../../platform/rgb_led.h"
#include "../../platform/button_handler.h"
#include "../turbo.h"
#include "../stick_center.h"

#if defined(ADAPT_OUTPUT_USB_DEVICE) && (MAX_USB_OUT > CFG_TUD_HID)
  #error MAX_USB_OUT > CFG_TUD_HID // Adafruit_TinyUSB_Library\src\arduino\ports\rp2040\tusb_config_rp2040.h
#endif

#include "firmware_runtime_forwards.h"
#include "../boot/boot_storage_runtime.h"
#include "../firmware_build_stamp.h"
#include "../../input/base/RZInputModule.h"
#include "../../input/runtime/input_boot_runtime.h"
#include "../../output/runtime/output_boot_runtime.h"
#include "../controller_runtime_core.h"
#include "runtime_product_services.h"
#include "../serial_debug_runtime.h"
#include "runtime_loop.h"

#include "../../menu/menu.h"
#include "../../platform/boot/boot_hardware_runtime.h"
#include "../../platform/boot/boot_ui_runtime.h"
#include "../../platform/runtime/platform_feedback_runtime.h"

#if ADAPT_POLICY_TOUCHPAD_MOUSE_TELEMETRY
void appendTouchpadMouseDebug(Print& out, uint8_t port) {
  if (port >= MAX_USB_OUT) {
    out.print('-');
    return;
  }
  const PointerMouseRuntimeState& state = pointerMouseState[port];
  out.print(state.anchored ? 1 : 0);
  out.print(',');
  out.print((int)state.contact_id);
  out.print(',');
  out.print(state.last_sample_dx);
  out.print(',');
  out.print(state.last_sample_dy);
  out.print(',');
  out.print(state.pending_x);
  out.print(',');
  out.print(state.pending_y);
  out.print(',');
  out.print((int)state.last_sent_dx);
  out.print(',');
  out.print((int)state.last_sent_dy);
  out.print(',');
  out.print(state.sample_count);
  out.print(',');
  out.print(state.rejected_sample_count);
  out.print(',');
  out.print(state.report_count);
}
#endif

void runFirmwareSetup() {
#if defined(ADAPT_FEATURE_N64_CAPTURE)
  n64CaptureBootInit();
#endif
#if defined(ADAPT_FEATURE_HOST_CAPTURE)
  hostCaptureBootInit();
#endif
#if defined(ADAPT_FEATURE_JOGCON_CAPTURE)
  jogconCaptureBootInit();
#endif
  productRuntimeBootInit();
  productRuntimePhaseBoot();
  autoDetectBenchmarkMark(ADBENCH_BOOT_SETUP_START);
  productRuntimePhaseHardware();
  prepareHardwareForBoot();
  showBootTraceMarker("1 hardware");
  productRuntimePhaseStorage();
  beginPersistentStorage();
  // Before anything reads Auto's reboot hand-off from the watchdog scratch
  // registers: a hand-off another firmware build wrote is discarded here.
  checkFirmwareBuildStampAtBoot();
  serialDebugRuntimeSetup();
#if defined(ENABLE_DEBUG_UART)
  Serial2.println(F("BOOT:2 STORAGE"));
  Serial2.flush();
#endif
  showBootTraceMarker("2 storage");
  loadPersistedBootConfiguration();
  restoreInputModeFromScratchRegisters();
  auto_output_boot_state_t autoOutputBootState = restoreAutoOutputBootState();
  resolveAutomaticSwitchProfileInputAtBoot(
      autoOutputBootState.autoOutputResolvedBoot);
  initializeRuntimeSettingsFromStorage();
#if defined(ADAPT_FEATURE_JOGCON_CAPTURE)
  jogconCaptureSettingsReady();
#endif
  productRuntimeMarkSettingsReady();
#if defined(ENABLE_DEBUG_UART)
  Serial2.println(F("BOOT:3 SETTINGS"));
  Serial2.flush();
#endif
  showBootTraceMarker("3 settings");
  const bool outputModeLockedAtBoot = is_auto_output_mode_selected();
  const outputMode_t bootOutputMode = outputMode;

  showBootSplashScreen();

  productRuntimePhaseInput();
  autoDetectBenchmarkMark(ADBENCH_BOOT_INPUT_START);
  initializeInputModuleForBoot();
#if defined(ENABLE_DEBUG_UART)
  Serial2.println(F("BOOT:4 INPUT"));
  Serial2.flush();
#endif
  autoDetectBenchmarkMark(ADBENCH_BOOT_INPUT_END);
  showBootTraceMarker("4 input");
  restoreLockedOutputModeAfterInputSetup(outputModeLockedAtBoot, bootOutputMode);

  maybePlayResolvedBootJingle(autoOutputBootState.autoOutputProbeBoot, autoOutputBootState.autoOutputResolvedBoot);

  productRuntimePhaseOutput();
  autoDetectBenchmarkMark(ADBENCH_BOOT_OUTPUT_START);
  finalizeBootOutputMode();
#if defined(ENABLE_DEBUG_UART)
  Serial2.println(F("BOOT:5 OUTPUT"));
  Serial2.flush();
#endif
  autoDetectBenchmarkMark(ADBENCH_BOOT_OUTPUT_END);
  showBootTraceMarker("5 output");

  productRuntimeSetupInputBridge(false);
#if ADAPT_POLICY_PS5_DIAGNOSTIC_RUNTIME
  delay(250);
#endif
  showBootTraceMarker("6a pre cfg");
  if (is_ps5_timing_quiet_mode_active()) {
    productRuntimeBeforeTimingQuietUsb();
  }
#if !defined(DISABLE_BOOT_USB_CONNECT)
  productRuntimePhaseUsbConfig();
  autoDetectBenchmarkMark(ADBENCH_USB_CONFIG_START);
  configure_usb_output();
#if defined(ENABLE_DEBUG_UART)
  Serial2.println(F("BOOT:6 CONFIGURED"));
  if (is_auto_output_mode_selected()) {
    printOutputAutodetectDiagnostics(Serial2, "BOOT_PRECONNECT");
  }
  Serial2.flush();
#endif
  autoDetectBenchmarkMark(ADBENCH_USB_CONFIG_END);
  showBootTraceMarker("6b cfg");
  connectConfiguredOutputUsb();
  productRuntimeAfterOutputConnect();
#if defined(ENABLE_DEBUG_UART)
  Serial2.println(F("BOOT:6 CONNECTED"));
  Serial2.flush();
#endif
  showBootTraceMarker("6c conn");
#else
  showBootTraceMarker("6x no usb");
#endif
#if defined(ENABLE_INPUT_USB) && defined(DEFER_USB_INPUT_HOST_START)
  // A rollback candidate proves settings and ESP32 SPI health before it
  // writes the durable acceptance tombstone. Keep PIO-USB core1 idle until
  // that flash commit completes so enumeration cannot race the cross-core
  // flash quiesce. The health task starts USB host immediately afterward.
  productRuntimeStartDeferredInputHost();
#endif
  productRuntimeSetupInputBridge(true);
  showBootTraceMarker("6d usb");

  showBootUsbDebugInfo();
  showBootTraceMarker("6e info");
#if ADAPT_POLICY_PS5_DIAGNOSTIC_RUNTIME
  delay(250);
  runPs5DiagAuthStatusDisplayForced();
#endif
  // Update WebHID with current device mode
  webhid_update_device_mode(deviceMode);

  // Ignore any button state transitions that happened during power-up or the
  // boot splash so the first menu action must be a deliberate post-boot press.
  modeButton.suppressUntilRelease();
  resetButton.suppressUntilRelease();
  // Paint home once before runtime AUTO input scans can occupy the UI loop.
  runPlatformFeedbackServices(false);
  autoDetectBenchmarkMark(ADBENCH_MAIN_REFRESH);
  autoDetectBenchmarkMark(ADBENCH_BOOT_SETUP_DONE);
  startCooperativeOled();
  productRuntimePhaseMainLoop();
}

void runFirmwareLoop() {
#if ADAPT_POLICY_PS5_DIAGNOSTIC_RUNTIME
  static bool ps5DiagLoopMarkerShown = false;
  if (!ps5DiagLoopMarkerShown && millis() >= 1000) {
    showPs5DiagBootMarker("7 loop p5d2");
    delay(250);
    runPs5DiagAuthStatusDisplayForced();
    ps5DiagLoopMarkerShown = true;
  }
  runPs5DiagAuthStatusDisplay();
#endif
#if defined(ENABLE_BOOTTRACE_OLED)
  static bool bootTraceLoopMarkerShown = false;
  if (!bootTraceLoopMarkerShown && millis() >= 1000) {
    showBootTraceMarker("7 loop");
    bootTraceLoopMarkerShown = true;
  }
#endif
  runMainRuntimeLoop();
}
