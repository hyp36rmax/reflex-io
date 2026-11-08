#include "../product_config.h"

#include "menu.h"
#include "menu_home_brand.h"
#include "menu_home_pixel_runtime.h"
#include "menu_main_display_internal.h"
#include "no_host_controller_test_runtime.h"
#include "menu_system_screens.h"
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
#include "../features/usb2db15/usb2db15_game_artwork.h"
#endif

#include "../core/rumble_test_runtime.h"
#include "../platform/boot/boot_ui_runtime.h"

namespace {

constexpr uint8_t kScreensaverDimContrast = 1;

void applyHomeDisplayContrast() {
  display.setContrast(idleDimActive ? kScreensaverDimContrast : display_contrast);
}

void flushHomeDisplay() {
  display.flush();
}

}  // namespace

void renderMainDisplay() {
#ifndef USE_I2C_DISPLAY
  mainDisplayInitialized = false;
  padDisplayNeedsRedraw = false;
  needsU8g2Clear = false;
  return;
#else
  if (renderFirmwareUpdateScreenIfActive()) {
    return;
  }
  if (renderFirmwareRollbackNoticeIfActive()) {
    return;
  }
  if (renderMenuActionFeedback()) {
    return;
  }

  static DeviceEnum lastInputMode = RZORD_NONE;
  static outputMode_t lastOutputMode = OUTPUT_LAST;
  static bool lastConnected[MAX_USB_OUT] = { false };
  static bool lastShowScanning = false;
  static bool lastShowNoHostController = false;
  static bool firstHardwareClear = true;
  const bool showNoHostController = noHostControllerTestActive();
  const bool keepBootSplashActive =
    !showNoHostController &&
    isBootSplashScreenVisible() && isBootAutoDetectPending();

  if (keepBootSplashActive) {
    return;
  }

  if (needsU8g2Clear || firstHardwareClear) {
    needsU8g2Clear = false;
    firstHardwareClear = false;
    beginDisplayWire();
    u8g2.begin();
    u8g2.setI2CAddress(I2C_ADDRESS * 2);
    restoreOledPanelOrientation();
    u8g2.setDrawColor(1);
    u8g2.clearBuffer();
    u8g2.sendBuffer();
    display.begin(&Adafruit128x64, I2C_ADDRESS);
    restoreOledPanelOrientation();
    display.clear();
    display.invertDisplay(false);
    display.setInvertMode(false);
    applyHomeDisplayContrast();
    flushHomeDisplay();
    mainDisplayInitialized = false;
    padDisplayNeedsRedraw = true;
  }

  bool connectionStateChanged = false;
  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    const bool connected = controllerFrameConst(i).connected;
    if (lastConnected[i] != connected) {
      connectionStateChanged = true;
      lastConnected[i] = connected;
    }
  }

  if (isIdleTimeoutReached()) {
    const bool enteringVisibleScreensaver =
      !idleAnimationActive && !idleDimActive && !idleBlankActive;
    // With Wake = Adapt only the panel idles during play, so rumble stays on.
    if (enteringVisibleScreensaver && !isOledWakeAdaptOnly()) {
      rumbleRuntimeSetHostFeedbackSuppressed(true);
    }
    if (isOledInactiveBlankMode()) {
      // Clear once and stop drawing; any wake input redraws the home screen.
      if (!idleBlankActive) {
        idleBlankActive = true;
        display.clear();
        flushHomeDisplay();
      }
      return;
    }
    if (isScreensaverDimMode()) {
      if (!idleDimActive) {
        idleDimActive = true;
        applyHomeDisplayContrast();
      }
    } else {
      renderSelectedAnimation();
      return;
    }
  }

#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
  // Artwork owns the full panel; do not flush the ASCII pad/header over it.
  if (renderUsb2db15GameArtwork(false)) return;
#endif
  bool connectionChangeNeedsFullRedraw = connectionStateChanged;
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
  if (deviceMode == RZORD_JVS) {
    connectionChangeNeedsFullRedraw = false;
  }
#endif

  bool needsRedraw = !mainDisplayInitialized
                   || lastInputMode != deviceMode
                   || lastOutputMode != outputMode
                   || lastShowNoHostController != showNoHostController
                   || connectionChangeNeedsFullRedraw;

  uint8_t connectedCount = 0;
  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    if (controllerFrameConst(i).connected) {
      connectedCount++;
    }
  }

  bool showScanning = menu_main_display_internal::shouldShowAutoDetectScanning(connectedCount);
  if (lastShowScanning != showScanning) {
    needsRedraw = true;
  }

  const bool showControllerList =
      menu_main_display_internal::shouldShowMultiControllerNameList();
  if (menu_main_display_internal::didControllerNamesChange()) {
    needsRedraw = true;
  }

  if (!needsRedraw) {
    if (mainDisplayInitialized && !showScanning) {
      HomePixelFrame pixelFrame;
      renderHomeModeLine(false);
      menu_main_display_internal::renderModeButtonIndicator();
      if (showControllerList) {
        // Three or four controller identities own the controller-view rows.
      } else if (menu_main_display_internal::shouldShowXinputMultiDiagOverlay()) {
        menu_main_display_internal::renderXinputMultiDiagOverlay();
      } else {
        menu_main_display_internal::updateRealtimeButtons();
      }
      menu_main_display_internal::renderJvsRawDebugLine();
      flushHomeDisplay();
    }
    return;
  }

  mainDisplayInitialized = true;
  HomePixelFrame pixelFrame(true);
  lastInputMode = deviceMode;
  lastOutputMode = outputMode;
  lastShowScanning = showScanning;
  lastShowNoHostController = showNoHostController;

  display.begin(&Adafruit128x64, I2C_ADDRESS);
  restoreOledPanelOrientation();
  u8g2.setDrawColor(1);
  display.invertDisplay(false);
  display.setInvertMode(false);
  display.clear();
  applyHomeDisplayContrast();
  padDisplayNeedsRedraw = true;
  display.setFont(System5x7);
  display.set1X();

  renderProductHomeBrand();

  renderHomeModeLine(showScanning, true);
  menu_main_display_internal::renderModeButtonIndicator(true);

  if (showScanning) {
    menu_main_display_internal::renderAutoDetectScanningStatus();
    flushHomeDisplay();
    markBootSplashScreenConsumed();
    return;
  }

  menu_main_display_internal::renderConnectedPortNames();

  if (showControllerList) {
    // renderConnectedPortNames() used rows 3-6 for the player list.
  } else if (menu_main_display_internal::shouldShowXinputMultiDiagOverlay()) {
    menu_main_display_internal::renderXinputMultiDiagOverlay(true);
  } else {
    menu_main_display_internal::updateRealtimeButtons();
  }
  menu_main_display_internal::renderJvsRawDebugLine(true);
  flushHomeDisplay();
  markBootSplashScreenConsumed();
#endif
}
