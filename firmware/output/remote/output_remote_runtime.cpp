#include "../../product_config.h"
#include "output_remote_runtime.h"
#include "output_remote.h"
#include "../../core/controller_frame_state.h"
#include "../../core/settings_store.h"
#include "../../input/runtime/input_frame_runtime.h"
#include "../output_runtime_state.h"

#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB) && defined(ADAPT_OUTPUT_USB_DEVICE)
#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include "../autodetect/output_autodetect_runtime.h"
#include "../autodetect/usb_detect.h"
#if ADAPT_POLICY_RESET_HOLD_AUTO_OUTPUT && defined(ENABLE_INPUT_AUTODETECT)
#include "../../input/autodetect/input_autodetect_runtime_state.h"
#endif
#endif

uint8_t remote_output_source_port() {
  const uint8_t count = inputPortCount() < MAX_USB_OUT ? inputPortCount() : MAX_USB_OUT;
  const uint8_t source = remoteOutputSource(controller_frames, count);
  // Slot zero delivers a released report when the final receiver disappears.
  return source == REMOTE_OUTPUT_NO_SOURCE ? 0 : source;
}

namespace {
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB) && defined(ADAPT_OUTPUT_USB_DEVICE)
bool genericRemoteHost() {
  constexpr uint8_t consoleSignals = DETECT_PS3 | DETECT_PS4 | DETECT_OGXBOX |
      DETECT_XBOX360 | DETECT_SWITCH1 | DETECT_SWITCH2;
  return autoDetectState == AUTO_STATE_WINDOWS ||
      (autoDetectState == AUTO_STATE_FALLBACK_HID &&
       (output_autodetect_last_flags & consoleSignals) == 0);
}
bool hasRemoteSource() {
  const uint8_t source = remote_output_source_port();
  return inputFrameConst(source).connected &&
         inputFrameConst(source).extended_button_count == REMOTE_OUTPUT_BUTTON_COUNT;
}
#endif
}

void refineRemoteOutputModeAtBoot() {
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB) && defined(ADAPT_OUTPUT_USB_DEVICE) && \
    !defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE) && !defined(ADAPT_FEATURE_GENESIS_IDENTIFICATION_CAPTURE)
  if (remoteOutputSelectionWanted(configuredOutputMode, genericRemoteHost(),
                                  get_configured_ps2_remote_output(), hasRemoteSource())) {
    outputMode = OUTPUT_REMOTE;
  } else if (outputMode == OUTPUT_REMOTE && !get_configured_ps2_remote_output() &&
             (configuredOutputMode == OUTPUT_HID || configuredOutputMode == OUTPUT_MISTER ||
              (configuredOutputMode == OUTPUT_AUTO && genericRemoteHost()))) {
    // Quick Save can retain the resolved host in scratch. Turning Remote off
    // must install Gamepad on this boot, rather than enumerate Remote again
    // and wait for the runtime service to request a second descriptor reboot.
    outputMode = configuredOutputMode == OUTPUT_AUTO
        ? (autoDetectState == AUTO_STATE_WINDOWS ? get_configured_windows_auto_output_mode()
                                                : get_configured_mister_auto_output_mode())
        : configuredOutputMode;
  }
#endif
}

bool serviceRemoteOutputSelection() {
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB) && defined(ADAPT_OUTPUT_USB_DEVICE) && \
    !defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE) && !defined(ADAPT_FEATURE_GENESIS_IDENTIFICATION_CAPTURE)
  static RemoteOutputDebounce debounce;
#if ADAPT_POLICY_RESET_HOLD_AUTO_OUTPUT && defined(ENABLE_INPUT_AUTODETECT)
  if (inputAutoDetectRebootPending()) { debounce.pending = false; return false; }
#endif
  const bool generic = genericRemoteHost();
  const bool eligible = configuredOutputMode == OUTPUT_HID || configuredOutputMode == OUTPUT_MISTER ||
      (configuredOutputMode == OUTPUT_AUTO && generic);
  if (!eligible || !tud_mounted()) { debounce.pending = false; return false; }
  const bool wanted = remoteOutputSelectionWanted(configuredOutputMode, generic,
      get_configured_ps2_remote_output(), hasRemoteSource());
  const bool current = canonicalizeOutputMode(outputMode) == OUTPUT_REMOTE;
  if (!debounce.ready(wanted, current, millis())) return false;
  const outputMode_t base = configuredOutputMode == OUTPUT_AUTO
      ? (autoDetectState == AUTO_STATE_WINDOWS ? get_configured_windows_auto_output_mode()
                                              : get_configured_mister_auto_output_mode())
      : configuredOutputMode;
  // Reboot before changing report shape. This also preserves resolved Auto
  // input/host state without changing its detection stages or saved output.
  auto_detect_trigger_reenum(static_cast<uint32_t>(wanted ? OUTPUT_REMOTE : base));
  return true;
#else
  return false;
#endif
}
