#include "../../product_config.h"

#include "boot_ui_runtime.h"
#include "boot_ui_product.h"
#include "reflex_boot_logo_bitmap.h"

#include <Arduino.h>
#include <string.h>

#include "../../firmware_build_info.h"
#include "../../core/controller_frame_state.h"
#include "../../core/controller_settings_state.h"
#include "../../core/device_runtime_state.h"
#include "../../core/firmware_build_stamp.h"
#include "../../input/autodetect/input_autodetect_benchmark.h"
#include "../../input/autodetect/input_autodetect_runtime_state.h"
#include "../../input/runtime/input_frame_runtime.h"
#if defined(ADAPT_CAP_USB_OUTPUT_AUTODETECT)
#include "../../output/autodetect/output_autodetect_runtime.h"
#endif
#include "../../output/output_runtime_state.h"
#include "../buzzer.h"
#include "../display_runtime_state.h"

namespace {
constexpr uint16_t kBootJingleDelayMs = 1000;
// USB connects after the splash, so the host simply sees the adapter later.
constexpr uint16_t kFirmwareUpdatedNoticeMs = 3000;

bool bootSplashVisible = false;
bool bootUsbDebugInfoSuppressed = false;
bool bootAutoDetectSplashSuppressed = false;

#ifdef USE_I2C_DISPLAY
void printBootNoticeLine(uint8_t row, const char* text) {
  const size_t width = strlen(text) * 6u;  // System5x7 plus its spacing column
  display.setRow(row);
  display.setCol(width < 128 ? static_cast<uint8_t>((128 - width) / 2) : 0);
  display.print(text);
}

// The first boot after an update (core/firmware_build_stamp.h) holds this in
// place of the logo.
void showFirmwareUpdatedNotice() {
  display.clear();
  display.set1X();
  printBootNoticeLine(2, "Firmware updated");
  printBootNoticeLine(3, "to " FIRMWARE_VERSION_TAG);
  delay(kFirmwareUpdatedNoticeMs);
}
#endif

}  // namespace

bool isBootAutoDetectPending() {
#if defined(ADAPT_CAP_USB_OUTPUT_AUTODETECT)
  const bool outputAutoPending =
    configuredOutputMode == OUTPUT_AUTO &&
    !auto_detect_is_final_state();

  return outputAutoPending;
#else
  return false;
#endif
}

void showBootSplashScreen() {
  autoDetectBenchmarkMark(ADBENCH_BOOT_SPLASH);
  if (!displayRuntimeAvailable()) {
    bootSplashVisible = false;
    return;
  }

  if (bootAutoDetectSplashSuppressed) {
    bootAutoDetectSplashSuppressed = false;
    bootSplashVisible = false;
    return;
  }

  #ifdef USE_I2C_DISPLAY
    beginDisplayWire();
    u8g2.begin();
    u8g2.setI2CAddress(I2C_ADDRESS * 2);
    restoreOledPanelOrientation();
    u8g2.setDrawColor(1);
    u8g2.clearBuffer();
    const bool updateNotice = firmwareUpdateNoticeThisBoot();
    if (!updateNotice) {
      u8g2.drawXBMP(0, 0, kReflexBootLogoWidth, kReflexBootLogoHeight, kReflexBootLogoBitmap);
    }
    u8g2.sendBuffer();

    display.beginNoClear(&Adafruit128x64, I2C_ADDRESS);
    restoreOledPanelOrientation();
    display.invertDisplay(false);
    display.setInvertMode(false);
    display.setContrast(display_contrast);
    display.setFont(System5x7);
    if (updateNotice) {
      showFirmwareUpdatedNotice();
    }
    bootSplashVisible = true;
    endDisplayWire();
  #endif
}

bool isBootSplashScreenVisible() {
  return bootSplashVisible;
}

void markBootSplashScreenConsumed() {
  bootSplashVisible = false;
}

void suppressBootUsbDebugInfoOnce() {
  bootUsbDebugInfoSuppressed = true;
}

void suppressBootAutoDetectSplashOnce() {
  bootAutoDetectSplashSuppressed = true;
}

void showBootTraceMarker(const char* marker) {
#if defined(ENABLE_BOOTTRACE_OLED) && defined(USE_I2C_DISPLAY)
  bootSplashVisible = false;
  beginDisplayWire();
  display.begin(&Adafruit128x64, I2C_ADDRESS);
  display.setContrast(255);
  display.setFont(System5x7);
  display.clear();
  display.set1X();
  display.set2X();
  display.setCol(40);
  display.print(F("BOOT"));
  display.set1X();
  display.setRow(3);
  display.setCol(0);
  display.print(marker);
  display.flush();
  endDisplayWire();
#else
  (void)marker;
#endif
}

void showPs5DiagBootMarker(const char* marker) {
  showProductBootDiagnosticMarker(marker);
}
void maybePlayResolvedBootJingle(bool autoOutputProbeBoot, bool autoOutputResolvedBoot) {
  if (canonicalizeOutputMode(get_effective_output_mode()) == OUTPUT_PS5) {
    return;
  }

  const bool inputModeResolvedAtBoot = (deviceMode != RZORD_AUTODETECT);
  const bool outputModeResolvedAtBoot =
    (configuredOutputMode != OUTPUT_AUTO) ||
    (autoDetectState != AUTO_STATE_IDLE) ||
    autoOutputResolvedBoot;
  if (inputModeResolvedAtBoot && outputModeResolvedAtBoot && !autoOutputProbeBoot) {
    buzzer.playBootDelayed(kBootJingleDelayMs);
  }
}

void showBootUsbDebugInfo() {
  #ifdef USE_I2C_DISPLAY
  if (bootUsbDebugInfoSuppressed) {
    bootUsbDebugInfoSuppressed = false;
    return;
  }
  if (isBootSplashScreenVisible() || isBootAutoDetectPending()) {
    return;
  }

  beginDisplayWire();
  restoreOledPanelOrientation();
  display.setRow(6);
  display.setCol(0);
  display.print(F("USB:"));
  display.print(max_devices);
  display.print(F("/"));
  display.print(hid_begin_success_count);
  display.print(F(" T:"));
  display.print(inputPortCount());
  display.flush();
  endDisplayWire();
  #endif
}
