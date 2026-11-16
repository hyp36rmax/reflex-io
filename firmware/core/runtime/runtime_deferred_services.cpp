#include "runtime_deferred_services.h"

#include "../../product_config.h"

#include "../adapt_state_stream_runtime.h"
#include "../input_overlay_runtime.h"
#include "../oled_serial_runtime.h"
#include "../serial_debug_runtime.h"
#include "../../output/auth/ps_auth_dongle_runtime.h"
#include "../../output/output_runtime_state.h"
#include "../../platform/latency_service_mask.h"
#include "../../platform/cooperative_oled.h"
#include "../../platform/latency_test.h"
#include "../../platform/runtime/platform_deferred_diagnostics_runtime.h"
#include "runtime_debug_cdc_state.h"
#include "runtime_serial_debug.h"
#include "../../features/psx_capture/psx_capture.h"
#include "../../features/genesis_capture/genesis_capture.h"
#include "../../features/jogcon_capture/jogcon_capture.h"
#include "../../features/n64_capture/n64_capture.h"
#include "../../features/host_capture/host_capture.h"
#include "../../features/output_trial/output_trial.h"

void runDeferredRuntimeServices() {
#if defined(ADAPT_FEATURE_OUTPUT_TRIAL)
  outputTrialService();
#endif
#if defined(ADAPT_FEATURE_N64_CAPTURE)
  n64CaptureService();
#endif
#if defined(ADAPT_FEATURE_HOST_CAPTURE)
  hostCaptureService();
#endif
#if defined(ADAPT_FEATURE_JOGCON_CAPTURE)
  jogconCaptureService();
#endif
#if defined(ADAPT_FEATURE_GENESIS_IDENTIFICATION_CAPTURE)
  genesisCaptureService();
#endif
#if defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE)
  psxCaptureService();
#endif
  serviceCooperativeOled();
  serialDebugRuntimeTask();

#ifdef ENABLE_RUNTIME_SERIAL_DEBUG_CDC
  if (runtimeDebugCdcEnabled && !is_ps5_timing_quiet_mode_active()) {
    processRuntimeSerialCommands();
    if (!oledSerialFrameInFlight() &&
        !latencyServiceDisabled(LATENCY_SERVICE_SERIAL_LOG) &&
        latencyTest.isSerialLogEnabled()) {
      latencyTest.flushLog(runtimeDebugCdc);
    }
    if (!latencyServiceDisabled(LATENCY_SERVICE_ADAPT_STATE) &&
        adaptStateIsEnabled()) {
      adaptStateTask(runtimeDebugCdc);
    }
    if (!latencyServiceDisabled(LATENCY_SERVICE_INPUT_OVERLAY) &&
        inputOverlayIsEnabled()) {
      inputOverlayTask(runtimeDebugCdc);
    }
    if (oledSerialFrameInFlight() ||
        (!latencyServiceDisabled(LATENCY_SERVICE_OLED_SERIAL) &&
         oledSerialIsEnabled())) {
      oledSerialTask(runtimeDebugCdc);
    }
#ifdef ENABLE_USB_AUTH_SERIAL_TRACE
    if (!oledSerialFrameInFlight()) {
      ps_auth_dongle_trace_flush(runtimeDebugCdc);
    }
#endif
  }
#endif

  servicePlatformDeferredDiagnostics();
}
