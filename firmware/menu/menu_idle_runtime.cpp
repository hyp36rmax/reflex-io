#include "../product_config.h"
#include "menu_idle_runtime.h"
#include "../platform/runtime/platform_menu_runtime.h"

#include "../core/controller_frame_state.h"
#include "../core/classic_mouse_motion.h"
#include "../core/controller_settings_state.h"
#include "../core/device_runtime_state.h"
#include "../core/rumble_test_runtime.h"
#include "../input/autodetect/input_autodetect_runtime_state.h"
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
#include "../input/jvs/jvs_analog_config.h"
#include "../input/jvs/jvs_host_runtime.h"
#endif
#include "../input/runtime/input_frame_runtime.h"
#include "../platform/display_runtime_state.h"
#include "../platform/runtime/platform_input_ui_runtime.h"
#include "menu_idle_controller_activity.h"
#include "menu_input.h"
#include "menu_display_state.h"
#include "menu_ui_state.h"
#include "menu_working_state.h"

#include <Arduino.h>

namespace {

bool isVisibleScreensaverActive() {
  return idleAnimationActive || idleDimActive || idleBlankActive;
}

bool shouldKeepIdleAwakeForAutoDetectScan() {
  #ifdef ENABLE_INPUT_AUTODETECT
  if (!isAutoDetectMode || deviceMode != RZORD_AUTODETECT) {
    return false;
  }

  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    if (controllerFrameConst(i).connected) {
      return false;
    }
  }

  return true;
  #else
  return false;
  #endif
}

#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
bool jvsIdleDisplayRestorePending = false;
#endif

void restoreNormalIdleDisplayState() {
#ifdef USE_I2C_DISPLAY
  restoreOledPanelOrientation();
  u8g2.setDrawColor(1);
  display.invertDisplay(false);
  display.setInvertMode(false);
  display.setContrast(display_contrast);
#endif
}

}  // namespace

void serviceDeferredIdleDisplayRestore() {
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
  if (jvsIdleDisplayRestorePending && isJvsMenuSafeWindowActive()) {
    jvsIdleDisplayRestorePending = false;
    restoreNormalIdleDisplayState();
  }
#endif
}

uint32_t getScreensaverTimeoutMs() {
  // menu_screensaver: 0=Off, 1-3=Dim 1/5/10m, 4-6=Anim 1/5/10m
  if (menu_screensaver == 0) return 0;

  uint8_t timeIndex = ((menu_screensaver - 1) % 3);
  switch (timeIndex) {
    case 0: return 60000;
    case 1: return 300000;
    case 2: return 600000;
    default: return 60000;
  }
}

bool isScreensaverDimMode() {
  return menu_screensaver >= 1 && menu_screensaver <= 3;
}

bool isOledInactiveBlankMode() {
  return menu_oled_inactive != 0;
}

bool isOledWakeAdaptOnly() {
  return menu_oled_wake != 0;
}

namespace {
IdleWakeSource last_idle_wake_source = IdleWakeSource::None;
}  // namespace

IdleWakeSource lastIdleWakeSource() {
  return last_idle_wake_source;
}

const char* idleWakeSourceName(IdleWakeSource source) {
  switch (source) {
    case IdleWakeSource::None: return "NONE";
    case IdleWakeSource::Controller: return "CONTROLLER";
    case IdleWakeSource::Buttons: return "BUTTONS";
    case IdleWakeSource::Menu: return "MENU";
    case IdleWakeSource::Serial: return "SERIAL";
    case IdleWakeSource::HostRumble: return "HOST_RUMBLE";
    case IdleWakeSource::AutoDetect: return "AUTODETECT";
    case IdleWakeSource::MisterArtwork: return "MISTER_ART";
    default: return "OTHER";
  }
}

bool idleWakeSourceAllowed(IdleWakeSource source) {
  if (!isOledWakeAdaptOnly() || menuOwnsControllerInput()) return true;
  return source != IdleWakeSource::Controller &&
         source != IdleWakeSource::HostRumble &&
         source != IdleWakeSource::AutoDetect;
}

#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
namespace {
// Analog Setup on Auto keeps the USB axes centered, so the frame checks never
// see the controls move: watch the cabinet's own channels.
bool jvsRawAnalogIdleActivity() {
  static IdleRawAnalogActivityTracker tracker;
  const JvsHostState& state = getJvsHostState();
  if (deviceMode != RZORD_JVS || !isJvsHostSyncedRecently(1500)) {
    return updateIdleRawAnalogActivity(tracker, nullptr, 0, 0);
  }
  const uint16_t travel = jvsAnalogMinimumTravel(getJvsBoardInfo().analog_bits);
  return updateIdleRawAnalogActivity(
      tracker, state.analog_values, state.analog_channels,
      travel > kIdleRawAnalogWakeThreshold ? travel : kIdleRawAnalogWakeThreshold);
}
}  // namespace
#endif

void resetIdleTimer(IdleWakeSource source) {
  if (!idleWakeSourceAllowed(source)) return;
  lastActivityTime = millis();
  last_idle_wake_source = source;
  const bool wasVisibleIdle =
      idleAnimationActive || idleDimActive || idleBlankActive;
  if (idleAnimationActive) {
    idleAnimationActive = false;
    mainDisplayInitialized = false;
    padDisplayNeedsRedraw = true;
    needsU8g2Clear = true;
    resetAnimationState();
  }
  if (idleDimActive) {
    idleDimActive = false;
    mainDisplayInitialized = false;
    padDisplayNeedsRedraw = true;
  }
  if (idleBlankActive) {
    idleBlankActive = false;
    mainDisplayInitialized = false;
    padDisplayNeedsRedraw = true;
  }
  if (wasVisibleIdle) {
    rumbleRuntimeSetHostFeedbackSuppressed(false);
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
    if (deviceMode == RZORD_JVS && !isJvsMenuSafeWindowActive()) {
      jvsIdleDisplayRestorePending = true;
    } else
#endif
    {
      restoreNormalIdleDisplayState();
    }
  }
}

bool handleIdleUiActivity(bool uiActivity, IdleWakeSource source) {
  if (!uiActivity || !idleWakeSourceAllowed(source)) {
    return false;
  }

  const bool consumedWake = isVisibleScreensaverActive();
  resetIdleTimer(source);
  return consumedWake;
}

bool handleControllerIdleWakeActivity() {
  static bool initialized = false;
  static uint32_t lastWakeButtons[MAX_USB_OUT] = { 0 };
  static bool lastConnected[MAX_USB_OUT] = { false };
  static IdleControllerActivityTracker motionTrackers[MAX_USB_OUT] = {};

  bool newWakePress = false;
  bool connectionChanged = false;
  bool controllerMotionActivity = false;
  const uint8_t portCount = inputPortCount();
  for (uint8_t i = 0; i < MAX_USB_OUT; ++i) {
    const controller_state_t& frame = controllerFrameConst(i);
    const bool connected = i < portCount && frame.connected;
    const uint32_t wakeButtons = connected ? getScreensaverWakeButtons(frame) : 0;
    controller_state_t motionFrame = frame;
    motionFrame.connected = connected;

    if (initialized && lastConnected[i] != connected) {
      connectionChanged = true;
    }
    if (initialized && (wakeButtons & ~lastWakeButtons[i]) != 0) {
      newWakePress = true;
    }
    if (updateIdleControllerMotionActivity(motionTrackers[i], motionFrame,
                                          classicMouseMotionActivitySequence(i))) {
      controllerMotionActivity = true;
    }

    lastConnected[i] = connected;
    lastWakeButtons[i] = wakeButtons;
  }
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
  if (jvsRawAnalogIdleActivity()) {
    controllerMotionActivity = true;
  }
#endif

  if (!initialized) {
    initialized = true;
    return false;
  }

  const bool discreteActivity =
      newWakePress || connectionChanged ||
      platformPhysicalInputConnectionChanged(initialized);
  return handleIdleUiActivity(discreteActivity || controllerMotionActivity,
                              IdleWakeSource::Controller);
}

bool isIdleTimeoutReached() {
  // The Auto scanning prompt stays visible for a screensaver, but Blank OLED
  // still blanks it; autodetect keeps scanning and a connection wakes it.
  if (!isOledInactiveBlankMode() && shouldKeepIdleAwakeForAutoDetectScan()) {
    return false;
  }

  uint32_t timeout = getScreensaverTimeoutMs();
  if (timeout == 0) return false;
  return (millis() - lastActivityTime) >= timeout;
}
