#include "../../product_config.h"

#include "input_poll_runtime.h"

#include <Arduino.h>

#include "../../core/controller_frame_state.h"
#include "../../core/controller_runtime_core.h"
#include "../../core/firmware_support.h"
#include "../../platform/latency_trace_gpio.h"
#include "../../platform/latency_test.h"
#include "../../platform/runtime/platform_input_autodetect_runtime.h"
#include "../../core/device_runtime_state.h"
#include "../../menu/menu_ui_state.h"
#include "../autodetect/input_autodetect_runtime.h"
#include "../autodetect/input_autodetect_runtime_state.h"
#include "../../output/output_capabilities.h"
#include "../../output/runtime/output_boot_runtime.h"
#include "input_frame_runtime.h"
#ifdef ENABLE_INPUT_JVS
#include "../jvs/jvs_host_runtime.h"
#endif
#include "input_adapter_runtime.h"

#if ADAPT_POLICY_DYNAMIC_MISTER_INPUT_IDENTITY && (defined(ENABLE_INPUT_SATURN) || defined(ENABLE_INPUT_PSX)) && defined(ADAPT_OUTPUT_USB_DEVICE)
#include <Adafruit_TinyUSB.h>
#include "../../output/autodetect/usb_detect.h"
#include "../saturn/saturn_3d_usb_identity.h"
#include "../psx/dualshock2_usb_identity.h"
#include "../mixed/input_mixed_runtime_state.h"
#include "../../output/runtime/input_runtime_output_bridge.h"
#include "../../output/runtime/output_boot_bridge.h"
#endif
#include "../state/input_poll_runtime_state.h"

namespace {

#if ADAPT_POLICY_DYNAMIC_N64_NSO_PLAYER_COUNT && defined(ENABLE_INPUT_N64)
constexpr uint32_t kN64NsoPlayerCountStableMs = 750;
uint8_t pendingN64NsoPlayerCount = 0xFF;
uint32_t pendingN64NsoPlayerCountSince = 0;

bool handleN64NsoPlayerCountProfileChange(bool polled) {
  if (!polled || isMenuOpen || isQuickConfigOpen ||
      inputAutoDetectModeActive() || deviceMode != RZORD_N64 || !nso_special ||
      canonicalizeOutputMode(get_effective_output_mode()) != OUTPUT_SWITCHPRO) {
    pendingN64NsoPlayerCount = 0xFF;
    pendingN64NsoPlayerCountSince = 0;
    return false;
  }

  const uint8_t connectedPlayers = connectedInputFrameCount(inputPortCount());
  const uint8_t expectedUsbSlots = bootUsbSlotCountForModeAndInput(
      get_effective_output_mode(), deviceMode, inputPortCount(), connectedPlayers);
  if (expectedUsbSlots == max_devices) {
    pendingN64NsoPlayerCount = 0xFF;
    pendingN64NsoPlayerCountSince = 0;
    return false;
  }

  const uint32_t now = millis();
  if (pendingN64NsoPlayerCount != connectedPlayers) {
    pendingN64NsoPlayerCount = connectedPlayers;
    pendingN64NsoPlayerCountSince = now;
    return false;
  }
  if ((uint32_t)(now - pendingN64NsoPlayerCountSince) <
      kN64NsoPlayerCountStableMs) {
    return false;
  }

  const bool outputPreserved =
      auto_detect_preserve_known_runtime_mode_for_input_reboot();
  if (is_auto_output_mode_selected() && !outputPreserved) {
    pendingN64NsoPlayerCount = 0xFF;
    pendingN64NsoPlayerCountSince = 0;
    return false;
  }
  preserveInputModeForPlayerCountReboot(
      deviceMode, inputPortCount(), connectedPlayers);
  reboot();
  return true;
}
#else
bool handleN64NsoPlayerCountProfileChange(bool polled) {
  (void)polled;
  return false;
}
#endif

#if ADAPT_POLICY_DYNAMIC_MISTER_INPUT_IDENTITY && (defined(ENABLE_INPUT_SATURN) || defined(ENABLE_INPUT_PSX)) && defined(ADAPT_OUTPUT_USB_DEVICE)
void updateAnalogTriggerUsbIdentity(bool polled) {
  static uint32_t eligibleSince = 0;
  static bool waitingForStable = false;
  static uint8_t pendingPlatform = 0xFF;
  if (!polled) {
    return;
  }
  if (can_run_usb_detection() ||
      !output_is_generic_mister_hid_mode(get_effective_output_mode())) {
    waitingForStable = false;
    return;
  }
  bool eligible = false;
  uint8_t profileSub = 0;
  const auto frameAt = [](uint8_t i) -> const controller_state_t& { return inputFrameConst(i); };
#ifdef ENABLE_INPUT_SATURN
  if (deviceMode == RZORD_SATURN && bcd_device_version.platform == BCD_PLAT_SATURN) {
    eligible = saturn3dIdentityEligible(frameAt, inputPortCount(), trigger_mode, inputMixedModeActive());
    profileSub = BCD_SATURN_3D_PLATFORM_SUB;
  }
#endif
#ifdef ENABLE_INPUT_PSX
  if (deviceMode == RZORD_PSX && bcd_device_version.platform == BCD_PLAT_PSX) {
    eligible = dualShock2IdentityEligible(frameAt, inputPortCount(), trigger_mode, inputMixedModeActive());
    profileSub = BCD_DUALSHOCK2_PLATFORM_SUB;
  }
#endif
  if (!profileSub) {
    waitingForStable = false;
    return;
  }
  if (pendingPlatform != bcd_device_version.platform) {
    pendingPlatform = bcd_device_version.platform;
    waitingForStable = false;
  }
  const bool active = bcd_device_version.platform_sub == profileSub;
  if (eligible == active) {
    waitingForStable = false;
    return;
  }
  // Promote after a stable observation and demote when no analog pad remains.
  if (eligible) {
    const uint32_t now = millis();
    if (!waitingForStable) {
      eligibleSince = now;
      waitingForStable = true;
      return;
    }
    if ((uint32_t)(now - eligibleSince) < 500) {
      return;
    }
  }
  waitingForStable = false;
  tud_disconnect();
  bcd_device_version.platform_sub = eligible ? profileSub : 0;
  TinyUSBDevice.setDeviceVersion(current_bcd_device_version());
  TinyUSBDevice.setSerialDescriptor(get_reflex_input_usb_serial_descriptor());
  delay(20);
  tud_connect();
}
#else
void updateAnalogTriggerUsbIdentity(bool) {}
#endif

bool shouldSkipInputPollForPinDebug() {
  #ifdef ADAPT_INPUT_RETRO
  return pinDebugActive;
  #else
  return false;
  #endif
}

void serviceFastInputRuntimes() {
  #ifdef ENABLE_INPUT_JVS
  if (deviceMode == RZORD_JVS) {
    serviceJvsHostRuntime();
  }
  #endif
}

bool __not_in_flash_func(pollInputModuleIfReady)(bool* updated_out) {
  if (shouldSkipInputPollForPinDebug()) {
    return false;
  }

  bool updated = false;
  bool polled = false;
  {
    LatencyPhaseTraceScope pollPhaseTrace(LATENCY_TRACE_PHASE_POLL);
    LatencyTraceGpioScope pollTrace(PIN_LATENCY_TRACE_POLL);
    polled = pollInputModuleIfDue(&updated);
    if (!polled) {
      pollPhaseTrace.cancel();
    }
  }
  if (!polled) {
    return false;
  }
  if (updated_out) {
    *updated_out = updated;
  }

  if (latencyTest.isEnabled()) {
    for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
      const controller_state_t& frame = controllerFrameConst(i);
      if (frame.connected) {
        latencyTest.observeButtons(0, frame.digital_buttons, true);
        break;
      }
      if (i == 0) {
        latencyTest.observeButtons(0, 0, false);
      }
    }
  }

  {
    LatencyPhaseTraceScope processPhaseTrace(LATENCY_TRACE_PHASE_PROCESS);
    LatencyTraceGpioScope processTrace(PIN_LATENCY_TRACE_PROCESS);
    processPolledInputFrame(updated);
  }
  return true;
}

bool shouldDeferInitialAutoDetectUntilHomeVisible() {
#if defined(ENABLE_INPUT_AUTODETECT) && defined(USE_I2C_DISPLAY)
  return deviceMode == RZORD_AUTODETECT && !mainDisplayInitialized &&
         !platformAllowsInitialAutoDetectBeforeHomeVisible();
#else
  return false;
#endif
}

bool handleAutoDetectHotSwapIfNeeded() {
  #ifdef ENABLE_INPUT_AUTODETECT
  if (inputAutoDetectModeActive() && !isMenuOpen && !isQuickConfigOpen) {
    if (shouldDeferInitialAutoDetectUntilHomeVisible()) {
      return false;
    }
    if (checkAutoDetectHotSwap()) {
      return true;
    }
  }
  #endif
  return false;
}

}  // namespace
void serviceFastInputRuntimeHooks() {
  serviceFastInputRuntimes();
}


void resetInputPollSchedule() {
  resetInputLastPollAtUs();
}

bool __not_in_flash_func(pollInputModuleIfDue)(uint32_t& last_poll_at_us, bool* updated) {
  const uint32_t now_us = micros();
  return pollActiveInputAdapterIfDue(now_us, last_poll_at_us, updated);
}

bool __not_in_flash_func(pollInputModuleIfDue)(bool* updated) {
  uint32_t last_poll_at_us = inputLastPollAtUs();
  bool polled = pollInputModuleIfDue(last_poll_at_us, updated);
  if (polled) {
    setInputLastPollAtUs(last_poll_at_us);
  }
  return polled;
}

bool __not_in_flash_func(runInputRuntimeCycle)(bool* polled_out, bool* updated_out) {
  serviceFastInputRuntimes();
  bool updated = false;
  const bool polled = pollInputModuleIfReady(&updated);
  if (polled_out) {
    *polled_out = polled;
  }
  if (updated_out) {
    *updated_out = updated;
  }
  updateAnalogTriggerUsbIdentity(polled);
  if (handleN64NsoPlayerCountProfileChange(polled)) {
    return true;
  }
  return handleAutoDetectHotSwapIfNeeded();
}
