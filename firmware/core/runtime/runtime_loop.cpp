#include "../../product_config.h"

#include "runtime_loop.h"
#include "../../platform/runtime/rp2040_crash_telemetry.h"

#include "../konami_code.h"

#include <Arduino.h>
#include <EEPROM.h>
#include <stdlib.h>

#if defined(ARDUINO_ARCH_RP2040)
#include <hardware/watchdog.h>
#endif

#ifdef ADAPT_OUTPUT_USB_DEVICE
#include <Adafruit_TinyUSB.h>
#endif

#ifdef ENABLE_INPUT_DREAMCAST
#include "../../input/dreamcast/Input_Dreamcast.h"
#endif
#if defined(ENABLE_USB_AUTH_SIDECAR) || defined(ENABLE_INPUT_USB)
#include "../../input/usb_host/input_usb_host_service.h"
#endif
#ifdef ENABLE_USB_BT_HCI
#include "../../input/usb_bt/usb_bt_hci_host.h"
#endif
#if defined(ENABLE_USB_XINPUT_HOST) || defined(ENABLE_USB_XINPUT_AUTH_HOST)
#include "../../input/usb_host/usb_xinput_host.h"
#endif
#ifdef ENABLE_INPUT_USB
#include <input_usb.h>
#endif
#ifdef ENABLE_INPUT_AUTODETECT
#include "../../input/autodetect/input_autodetect_runtime_state.h"
#endif
#ifdef ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT
#include "../../output/auth/xbone_auth_passthrough.h"
#include "../../output/xboxone/out_xboxone.h"
#endif
#include "../../input/runtime/input_poll_runtime.h"
#include "../../input/runtime/input_adapter_runtime.h"
#include "../../features/feature_module.h"
#include "../../menu/menu_idle_runtime.h"
#include "../../menu/menu_mode_state.h"
#include "../../menu/menu_runtime_state.h"
#include "../../menu/menu_ui_state.h"
#include "../../menu/menu_working_state.h"
#include "../../output/output_runtime_state.h"
#include "../../output/runtime/output_loop_runtime.h"
#include "../../output/runtime/output_product_services.h"
#include "../../platform/runtime/platform_background_runtime.h"
#include "../../platform/runtime/platform_feedback_runtime.h"
#include "../../platform/runtime/platform_maintenance_runtime.h"
#include "../../platform/runtime/platform_fast_path_runtime.h"
#include "../../platform/runtime/platform_priority_output_runtime.h"
#include "../../platform/runtime/platform_runtime_ui.h"
#include "../../platform/latency_trace_gpio.h"
#include "../../platform/latency_test.h"
#include "../../platform/latency_service_mask.h"
#include "../controller_frame_state.h"
#include "../controller_delivery_state.h"
#include "../device_runtime_state.h"
#include "../firmware_support.h"
#include "../settings_store.h"
#include "../controller_runtime_core.h"
#include "../turbo.h"
#include "runtime_deferred_services.h"

namespace {

constexpr uint32_t kActiveFeedbackUiIntervalUs = 33333;
constexpr uint32_t kIdleFeedbackUiIntervalUs = 16667;
uint32_t lastPostPollUiServiceUs = 0;
uint32_t lastFeedbackUiUs = 0;
bool pendingRuntimeUiUpdate = false;
bool menuOwnedControllerInputLastPass = false;

// A menu taking the controls neutralizes the frame the host gets, and giving
// them back restores it, without the controller itself changing. Outputs that
// report only on a change (DInput, XInput, Keyboard) would otherwise keep a
// button held through the menu until the next input change.
void __not_in_flash_func(requestDeliveryWhenMenuOwnershipChanges)() {
  const bool owned = menuOwnsControllerInput();
  if (owned == menuOwnedControllerInputLastPass) return;
  menuOwnedControllerInputLastPass = owned;
  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    if (controllerFrameConst(i).connected) requestControllerFrameDelivery(i);
  }
}

bool __not_in_flash_func(outputFramePreparationRequired)() {
#if defined(ENABLE_OUTPUT_DB15)
  // Parallel lines remain live between USB reports. Reapply remaps, SOCD and
  // timed turbo every pass, rather than sending the restored physical frame.
  return true;
#endif
  if (latencyTest.isEnabled() || outputMode == OUTPUT_PS3) {
    return true;
  }
#ifdef ADAPT_OUTPUT_USB_DEVICE
  // A product transport that replaces USB (Modern2USB Bluetooth output)
  // publishes the current frame on every send pass, pending or not.
  if (productOutputTransportIsActive()) {
    return true;
  }
#endif
  // send_usb_single_report() also sends these outputs without a pending
  // frame (its force_update, including the PS4 keepalive), so finalize every
  // pass; otherwise those reports carry the restored, unremapped frame.
  // send_usb_grouped_report() streams the GC Adapter the same way.
  const outputMode_t effectiveOutput = get_effective_output_mode();
  if (effectiveOutput == OUTPUT_PS3 || effectiveOutput == OUTPUT_PS4 ||
      effectiveOutput == OUTPUT_GCWIIU) {
    return true;
  }
#ifdef ENABLE_OUTPUT_PS5
  if (effectiveOutput == OUTPUT_PS5) {
    return true;
  }
#endif
#ifdef ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT
  if (effectiveOutput == OUTPUT_XBOXONE) {
    return true;
  }
#endif

  for (uint8_t i = 0; i < MAX_USB_OUT; ++i) {
    if (controllerFrameNeedsDelivery(i)) {
      return true;
    }
  }
  return false;
}

void runPostPollUiIfNeeded(bool polled, bool updated) {
  if (latencyServiceDisabled(LATENCY_SERVICE_POSTPOLL_UI)) {
    pendingRuntimeUiUpdate = false;
    return;
  }
  if (polled) {
    pendingRuntimeUiUpdate = pendingRuntimeUiUpdate || updated;
    const uint32_t nowUs = micros();
    if (lastPostPollUiServiceUs != 0 &&
        (uint32_t)(nowUs - lastPostPollUiServiceUs) < kIdleFeedbackUiIntervalUs) {
      return;
    }
    lastPostPollUiServiceUs = nowUs;
  } else {
    lastPostPollUiServiceUs = micros();
  }

  const bool uiUpdated = pendingRuntimeUiUpdate;
  pendingRuntimeUiUpdate = false;

  runPlatformRuntimeControllerUi(uiUpdated);

  const uint32_t nowUs = micros();
  const uint32_t feedbackIntervalUs =
    uiUpdated ? kActiveFeedbackUiIntervalUs : kIdleFeedbackUiIntervalUs;
  if (lastFeedbackUiUs != 0 &&
      (uint32_t)(nowUs - lastFeedbackUiUs) < feedbackIntervalUs) {
    return;
  }
  lastFeedbackUiUs = nowUs;

  LatencyPhaseTraceScope uiPhaseTrace(LATENCY_TRACE_PHASE_UI);
  runPlatformRuntimeFeedbackUi(uiUpdated);
}

void runMinimalFastPathTransportTasks() {
  #ifdef ADAPT_OUTPUT_USB_DEVICE
  #if defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED)
  TinyUSB_Device_Task();
  #else
  TinyUSBDevice.task();
  #endif
  #endif
}

void runMinimalFastPathRuntimeLoop() {
  runMinimalFastPathTransportTasks();
  runPlatformBackgroundTasks();
  turbo.update();

  if (runPlatformPrePollUi()) {
    runMinimalFastPathTransportTasks();
    return;
  }

  serviceFastInputRuntimeHooks();

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
  if (polled) {
    {
      LatencyPhaseTraceScope processPhaseTrace(LATENCY_TRACE_PHASE_PROCESS);
      LatencyTraceGpioScope processTrace(PIN_LATENCY_TRACE_PROCESS);
      processPolledInputFrame(updated);
    }
  }

  requestDeliveryWhenMenuOwnershipChanges();
  {
    LatencyPhaseTraceScope preparePhaseTrace(LATENCY_TRACE_PHASE_PREPARE);
    LatencyTraceGpioScope prepareTrace(PIN_LATENCY_TRACE_PREPARE);
    const bool latencyTestActive = latencyTest.isEnabled();
    if (latencyTestActive) {
      latencyTest.prepareOutputFrame();
    }
    finalizeControllerFrameForOutput();
    if (latencyTestActive) {
      latencyTest.noteOutputFramePrepared();
    }
  }
  {
    LatencyPhaseTraceScope sendPhaseTrace(LATENCY_TRACE_PHASE_SEND);
    LatencyTraceGpioScope sendTrace(PIN_LATENCY_TRACE_SEND);
    sendPreparedOutputFrame();
  }
  restoreControllerFrameAfterOutputSend();
  if (latencyTest.isEnabled()) {
    latencyTest.restoreOutputFrame();
  }
  updateKonamiCodeObserver(polled);
  runPostPollUiIfNeeded(polled, updated);

  runMinimalFastPathTransportTasks();
}

}  // namespace

void __not_in_flash_func(runMainRuntimeLoop)() {
  //USBHost.task();
  //tud_task();
  //hid_task();

  servicePlatformRuntimeMaintenanceHealth();
  // Keep the controller-to-USB critical path first: sync the USB device stack,
  // read inputs, process transforms, prepare the output frame, and submit the
  // report before CDC/menu/OLED/WebHID/background services can stall the loop.
  // Step numbers name the stage a watchdog timeout stopped in (WDSTEP).
  rp2040CrashTelemetryMarkLoopStep(1);
  runOutputTransportSyncTasks();

  rp2040CrashTelemetryMarkLoopStep(2);
  if (servicePlatformRuntimeExclusiveMaintenance()) {
    return;
  }
  if (usePlatformFastPathRuntime()) {
    rp2040CrashTelemetryMarkLoopStep(3);
    runMinimalFastPathRuntimeLoop();
    return;
  }

  rp2040CrashTelemetryMarkLoopStep(4);
  if (runPlatformPriorityOutputOverride()) {
    return;
  }

  bool polled = false;
  bool updated = false;
  rp2040CrashTelemetryMarkLoopStep(5);
  if (runInputRuntimeCycle(&polled, &updated)) {
    rp2040CrashTelemetryMarkLoopStep(11);
    runDeferredRuntimeServices();
    return;
  }

  rp2040CrashTelemetryMarkLoopStep(6);
  requestDeliveryWhenMenuOwnershipChanges();
  runPostPollOutputTasks(polled);
  updateKonamiCodeObserver(polled);
  runActiveInputAdapterAfterOutputFrameSent(polled, updated);

  {
    LatencyPhaseTraceScope prePollUiTrace(LATENCY_TRACE_PHASE_PREPOLL_UI);
    rp2040CrashTelemetryMarkLoopStep(7);
    if (!latencyServiceDisabled(LATENCY_SERVICE_PREPOLL_UI) && runPlatformPrePollUi()) {
      rp2040CrashTelemetryMarkLoopStep(11);
      runDeferredRuntimeServices();
      return;
    }
  }

  rp2040CrashTelemetryMarkLoopStep(8);  // post-poll UI and background tasks
  runPostPollUiIfNeeded(polled, updated);
  runLoopBackgroundTasks();

  {
    LatencyPhaseTraceScope pendingOutputTrace(LATENCY_TRACE_PHASE_PENDING_OUTPUT);
    rp2040CrashTelemetryMarkLoopStep(10);
    if (!latencyServiceDisabled(LATENCY_SERVICE_PENDING_OUTPUT)) {
      processPendingOutputRuntimeTasks();
    }
  }

  #ifdef ENABLE_USB_BT_HCI
  usb_bt_hci_persist_task();
  #endif

  rp2040CrashTelemetryMarkLoopStep(11);
  runDeferredRuntimeServices();

//  bool anyConnected = input_report[0].connected || input_report[1].connected || input_report[2].connected || input_report[3].connected;
//  #ifdef LED_BUILTIN
//    gpio_put(LED_BUILTIN, anyConnected ? HIGH : LOW);
//  #endif
}

void runLoopBackgroundTasks() {
  {
    LatencyPhaseTraceScope outputBgTrace(LATENCY_TRACE_PHASE_OUTPUT_BG);
    if (!latencyServiceDisabled(LATENCY_SERVICE_OUTPUT_BG)) {
      runOutputBackgroundTasks();
    }
  }
  {
    LatencyPhaseTraceScope platformBgTrace(LATENCY_TRACE_PHASE_PLATFORM_BG);
    if (!latencyServiceDisabled(LATENCY_SERVICE_PLATFORM_BG)) {
      runPlatformBackgroundTasks();
    }
  }
  {
    LatencyPhaseTraceScope turboTrace(LATENCY_TRACE_PHASE_TURBO);
    if (!latencyServiceDisabled(LATENCY_SERVICE_TURBO_TASK)) {
      turbo.update();
    }
  }
}

void __not_in_flash_func(runPostPollOutputTasks)(bool polled) {
  servicePlatformPriorityOutput();
  // Most loop iterations have no changed controller frame. Avoid copying,
  // transforming, and restoring every controller in that case. The transport
  // task still runs below so host feedback and output-specific housekeeping
  // remain live.
  if (!outputFramePreparationRequired()) {
    // A host report handled in the USB interrupt can request delivery after
    // the check above. That frame has not been finalized, so it waits for
    // the next pass instead of going out without its transforms.
    deferControllerFrameDelivery(true);
    preparePlatformPriorityOutputFrame();
    sendPreparedOutputFrame();
    finishPlatformPriorityOutputFrame();
    deferControllerFrameDelivery(false);
    return;
  }

  // Neo-Geo polls on every runtime pass and processPolledInputFrame() has
  // already applied every digital transform needed by this mode. Avoid
  // copying and rebuilding the same frame before its latency-sensitive send,
  // except while a held Start shortcut needs finalization to add its output.
  const bool useProcessedFrameDirectly =
      polled && deviceMode == RZORD_NEOGEO && !menuOwnsControllerInput() &&
      !processedControllerFrameNeedsFinalize();

  if (!useProcessedFrameDirectly) {
    LatencyPhaseTraceScope preparePhaseTrace(LATENCY_TRACE_PHASE_PREPARE);
    LatencyTraceGpioScope prepareTrace(PIN_LATENCY_TRACE_PREPARE);
    const bool latencyTestActive = latencyTest.isEnabled();
    if (latencyTestActive) {
      latencyTest.prepareOutputFrame();
    }
    finalizeControllerFrameForOutput();
    if (latencyTestActive) {
      latencyTest.noteOutputFramePrepared();
    }
  }
  preparePlatformPriorityOutputFrame();
  {
    LatencyPhaseTraceScope sendPhaseTrace(LATENCY_TRACE_PHASE_SEND);
    LatencyTraceGpioScope sendTrace(PIN_LATENCY_TRACE_SEND);
    sendPreparedOutputFrame();
  }
  finishPlatformPriorityOutputFrame();
  restoreControllerFrameAfterOutputSend();
  if (latencyTest.isEnabled()) {
    latencyTest.restoreOutputFrame();
  }
}
