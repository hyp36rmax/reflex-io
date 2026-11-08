#include "../../product_config.h"
#ifdef ADAPT_OUTPUT_USB_DEVICE

#include <Adafruit_TinyUSB.h>
#include <Arduino.h>

#include "../../core/firmware_support.h"
#if defined(ENABLE_INPUT_AUTODETECT)
#include "../../input/autodetect/input_autodetect_runtime.h"
#endif
#if defined(ENABLE_USB_AUTH_SIDECAR) || defined(ENABLE_INPUT_USB)
#include "../../input/usb_host/input_usb_host_service.h"
#endif
#include "../auth/auth_status.h"
#include "../auth/ps_auth_dongle_runtime.h"
#include "../auth/webhid_auth_runtime.h"
#ifdef ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT
#include "../auth/xbone_auth_passthrough.h"
#include "../xboxone/out_xboxone.h"
#endif
#include "../output_runtime_state.h"
#include "../remote/output_remote_runtime.h"
#include "output_autodetect_diagnostics.h"
#include "output_autodetect_rp2040_usb_debug.h"
#include "output_autodetect_runtime.h"
#include "output_autodetect_support.h"
#include "usb_detect.h"
#if defined(ADAPT_FEATURE_HOST_CAPTURE)
#include "../../features/host_capture/host_capture.h"
#endif
#include "../xinput/out_xinput.h"
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB) && defined(ENABLE_EXPERIMENTAL_CONSOLE_MOUSE)
#include "../../core/classic_mouse_source.h"
#include "../../core/rotary_mouse_settings.h"
#include "../../input/runtime/input_frame_runtime.h"
#include "auto_classic_mouse_policy.h"
#if ADAPT_POLICY_RESET_HOLD_AUTO_OUTPUT && defined(ENABLE_INPUT_AUTODETECT)
#include "../../input/autodetect/input_autodetect_runtime_state.h"
#endif
#endif

// HOST DETECTION WARNING:
// This AUTO path is intentionally narrow and timing-sensitive. Windows,
// MiSTer/Linux, Switch, PS3, and Xbox-family hosts overlap at the USB
// descriptor/control-request level, so "harmless" extra probe stages or longer
// generic timeouts can break another host. The generic probe must get one fast
// RetroZord-style pass, then settle to the configured generic-host fallback unless the
// host has produced an explicit console signal. PS3 assist is a secondary pass,
// not the default path. Reverting this toward the old 5s generic wait caused
// Windows to hang on the OLED "Detecting..." screen.
//
// Do not change these constants, stage order, or fallback conditions without
// benching at least Windows, MiSTer/Linux, Switch, PS3, and Xbox-family AUTO
// detection and documenting the result in the commit.
constexpr uint16_t kGenericNoMountResolveMs = 1000;
constexpr uint16_t kGenericNoMountAfterAttachRetryResolveMs = 2000;
constexpr uint16_t kPs3AssistResolveMs = 900;
constexpr uint16_t kXboxOneAssistResolveMs = 10000;
constexpr uint32_t kAutoDetectNoDataHostFallbackMs = 1500;

static uint32_t autoDetectNoDataHostSinceMs = 0;

bool auto_detect_no_data_host_fallback_active() {
  if (!is_auto_output_mode_selected() ||
      autoDetectState != AUTO_STATE_IDLE ||
      tud_connected() || tud_mounted()) {
    autoDetectNoDataHostSinceMs = 0;
    return false;
  }

  const uint32_t now = millis();
  if (autoDetectNoDataHostSinceMs == 0) {
    autoDetectNoDataHostSinceMs = now == 0 ? 1 : now;
  }
  return (uint32_t)(now - autoDetectNoDataHostSinceMs) >=
         kAutoDetectNoDataHostFallbackMs;
}

uint16_t auto_detect_generic_no_mount_resolve_ms() {
  return output_autodetect_usb_attach_retry_needs_grace()
      ? kGenericNoMountAfterAttachRetryResolveMs
      : kGenericNoMountResolveMs;
}

bool auto_detect_is_final_state() {
  if (autoDetectState == AUTO_STATE_XBOX_360) {
    return xinput_is_authenticated();
  }
  if (autoDetectState == AUTO_STATE_PLAYSTATION) {
    return false;
  }
  return autoDetectState != AUTO_STATE_IDLE;
}

static bool ps5_probe_has_native_host_traffic() {
#if defined(ENABLE_OUTPUT_PS5)
  const PsAuthDongleStatus status = ps_auth_dongle_status();
  // The generic 0x03 and DS4-style feature IDs overlap PS4 and PS5. Require
  // two completed signed-report submissions after the generic probe
  // has already classified the host as PlayStation-family.
  return ps_auth_dongle_has_provider_for_mode(OUTPUT_PS5) &&
         status.ps5_host_submit_count >= 2;
#else
  return false;
#endif
}


static bool ps5_probe_should_fallback_to_ps4(uint32_t elapsed_ms) {
#if defined(ENABLE_OUTPUT_PS5)
  return elapsed_ms >= 5000;
#else
  (void)elapsed_ms;
  return true;
#endif
}

static uint32_t playstation_family_fallback_ms() {
  return 5000u;
}

static bool playstation_ps4_auth_challenge_seen() {
  const PsAuthDongleStatus sidecar = ps_auth_dongle_status();
  return ps4Auth.challengeReportCount() != 0 ||
         (ps_auth_dongle_has_provider_for_mode(OUTPUT_PS4) &&
          sidecar.set_report_count != 0);
}

static uint8_t saturateProbeNibble(uint16_t value) {
  return (value > 0x0Fu) ? 0x0Fu : (uint8_t)value;
}

static void update_auto_detect_probe_counters() {
  XInputDebugInfo info = {};
  xinput_get_debug_info(&info);
  output_autodetect_probe_counters =
    ((uint16_t)saturateProbeNibble(info.desc_device_calls) << 12) |
    ((uint16_t)saturateProbeNibble(info.desc_config_calls) << 8) |
    ((uint16_t)saturateProbeNibble(info.interfaces_opened) << 4) |
    (uint16_t)saturateProbeNibble(info.request_count);
}

static void sync_xinput_descriptor_hints() {
  if (xinput_seen_string_0xEE) {
    host_detection.ms_os_string_read = true;
    usb_detect_mark_result(DETECT_WINDOWS);
  }
  if (xinput_seen_string_4) {
    // Sonik's working detector treats the Xbox security-interface string as a
    // 360 hint even before the full XSM3 request train starts.
    host_detection.vendor_setup_seen = true;
  }
}

void auto_detect_hid_report_read_cb(uint8_t itf) {
  (void) itf;
}

extern "C" bool tud_hid_set_idle_cb(uint8_t instance, uint8_t idle_rate) {
  (void) instance;
  (void) idle_rate;
  return true;
}

extern "C" void tud_string_0xee_requested_cb(void) {
}

static void auto_detect_preserve_auto_input_before_output_reenum() {
#if defined(ENABLE_INPUT_AUTODETECT)
  if (savedDeviceMode != RZORD_AUTODETECT) {
    return;
  }

  DeviceEnum modeToPreserve = deviceMode;
  if (modeToPreserve == RZORD_AUTODETECT || modeToPreserve == RZORD_NONE) {
    // Output AUTO may reboot before the Home screen while input AUTO is still
    // unresolved. Do not run controller probes from the host-detect path.
    preserveDetectedInputModeForReboot(RZORD_NONE);
    return;
  }
  preserveDetectedInputModeForReboot(modeToPreserve);
#endif
}

static void auto_detect_reboot_with_diagnostics(const char* phase) {
#if defined(ENABLE_DEBUG_UART)
  printOutputAutodetectDiagnostics(Serial2, phase);
  Serial2.flush();
#else
  (void)phase;
#endif
  reboot();
}

void auto_detect_trigger_reenum(uint32_t next_output_mode) {
  autoDetectProbeStage = AUTO_PROBE_GENERIC;
  auto_detect_persist_debug_snapshot(output_autodetect_last_flags, output_autodetect_aux_flags);
  auto_detect_persist_runtime_mode((outputMode_t)next_output_mode, autoDetectState);
  auto_detect_preserve_auto_input_before_output_reenum();
  auto_detect_reboot_with_diagnostics("RESOLVE_REBOOT");
}

bool auto_detect_preserve_current_runtime_mode_for_input_reboot() {
  if (auto_detect_preserve_resolved_runtime_mode_for_reboot()) {
    return true;
  }

  if (!is_auto_output_mode_selected() ||
      autoDetectState != AUTO_STATE_IDLE ||
      canonicalizeOutputMode(outputMode) != OUTPUT_MISTER) {
    return false;
  }

  const bool genericMounted =
      tud_mounted() &&
      usb_detect_has_mounted_generic_hid_activity() &&
      host_detection.any == 0 &&
      !host_detection.ms_os_string_read &&
      !host_detection.vendor_setup_seen &&
      !host_detection.ps3_feature_seen &&
      !host_detection.cleared_input &&
      !host_detection.cleared_output;
  if (!genericMounted) {
    return false;
  }

  autoDetectProbeStage = AUTO_PROBE_GENERIC;
  autoDetectState = AUTO_STATE_FALLBACK_HID;
  output_autodetect_last_flags = host_detection.any | DETECT_TIMEOUT;
  output_autodetect_aux_flags = usb_detect_get_aux_flags();
  auto_detect_persist_debug_snapshot(output_autodetect_last_flags, output_autodetect_aux_flags);
  auto_detect_persist_runtime_mode(
    get_configured_mister_auto_output_mode(), autoDetectState);
  return true;
}

static void auto_detect_trigger_probe_stage(AutoDetectProbeStage next_stage) {
  autoDetectProbeStage = next_stage;
  auto_detect_persist_debug_snapshot(output_autodetect_last_flags, output_autodetect_aux_flags);
  auto_detect_persist_probe_stage(next_stage);
  auto_detect_reboot_with_diagnostics("PROBE_REBOOT");
}

static bool auto_detect_trigger_xboxone_probe(uint8_t transition_reason) {
#ifdef ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT
  output_autodetect_transition_reason = transition_reason;
  auto_detect_trigger_probe_stage(AUTO_PROBE_XBOXONE);
  return true;
#else
  (void)transition_reason;
  return false;
#endif
}

static void auto_detect_trigger_xinput_output() {
  // XSM3 means retail Xbox 360, not Windows. Resolve to the dedicated
  // single-controller OUTPUT_XINPUT path so the security interface and auth
  // state machine match the donor controller shape.
  usb_detect_mark_result(DETECT_XBOX360);
  autoDetectState = AUTO_STATE_XBOX_360;
  auto_detect_trigger_reenum((uint32_t)OUTPUT_XINPUT);
}

static bool auto_detect_promote_windows_xinput_to_xbox360() {
  // Windows XInput2P can produce auth-like control traffic after AUTO has
  // already resolved Windows. Do not reinterpret that as Xbox 360; Xbox 360
  // is selected only by the initial host detector. (The opt-in late promotion
  // watched XSM3 traffic on the wired XInputMulti transport, which is gone.)
  return false;
}

static void auto_detect_trigger_fallback_hid() {
  const bool probeRuntimeActive = can_run_usb_detection();
  const outputMode_t fallbackOutput = get_configured_mister_auto_output_mode();
  autoDetectProbeStage = AUTO_PROBE_GENERIC;
  autoDetectState = AUTO_STATE_FALLBACK_HID;
  output_autodetect_last_flags = host_detection.any | DETECT_TIMEOUT;
  output_autodetect_aux_flags = usb_detect_get_aux_flags();
  if (canonicalizeOutputMode(outputMode) != canonicalizeOutputMode(fallbackOutput) ||
      probeRuntimeActive) {
    auto_detect_trigger_reenum((uint32_t)fallbackOutput);
  }
}

static void auto_detect_trigger_ps3_hid_fallback() {
  const bool probeRuntimeActive = can_run_usb_detection();
  autoDetectProbeStage = AUTO_PROBE_GENERIC;
  autoDetectState = AUTO_STATE_PS3;
  output_autodetect_last_flags = host_detection.any | DETECT_PS3;
  output_autodetect_aux_flags = usb_detect_get_aux_flags();
  if (outputMode != OUTPUT_PS3 || probeRuntimeActive) {
    auto_detect_trigger_reenum((uint32_t)OUTPUT_PS3);
  }
}

static void auto_detect_trigger_ps4_output() {
  if (!authOutputModeCanRun(OUTPUT_PS4)) {
    auto_detect_trigger_fallback_hid();
    return;
  }

  autoDetectProbeStage = AUTO_PROBE_GENERIC;
  autoDetectState = AUTO_STATE_PS4;
  if (outputMode != OUTPUT_PS4) {
    auto_detect_trigger_reenum((uint32_t)OUTPUT_PS4);
  }
}

static void auto_detect_trigger_playstation_family_output() {
#if !defined(ENABLE_OUTPUT_PS5)
  // Builds without the donor-assisted native PS5 capability settle directly
  // into the PS4-compatible path and never stage a PS5 identity.
  auto_detect_trigger_ps4_output();
  return;
#endif

  // RetroZord truth reports only DETECT_PS4 for PlayStation-family hosts. Start
  // and remain on the PS4-compatible runtime because ordinary DS4 feature
  // traffic cannot distinguish PS4 from PS5 safely.
  if (!authOutputModeCanRun(OUTPUT_PS4)) {
    auto_detect_trigger_fallback_hid();
    return;
  }

  autoDetectProbeStage = AUTO_PROBE_GENERIC;
  autoDetectState = AUTO_STATE_PLAYSTATION;
  if (outputMode != OUTPUT_PS4) {
    auto_detect_trigger_reenum((uint32_t)OUTPUT_PS4);
  }
}

static void auto_detect_trigger_windows_output(outputMode_t automode_windows_output) {
  const bool probeRuntimeActive = can_run_usb_detection();
  autoDetectProbeStage = AUTO_PROBE_GENERIC;
  autoDetectState = AUTO_STATE_WINDOWS;
  if (canonicalizeOutputMode(outputMode) != canonicalizeOutputMode(automode_windows_output) ||
      probeRuntimeActive) {
    auto_detect_trigger_reenum((uint32_t)automode_windows_output);
  }
}

#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB) && defined(ENABLE_EXPERIMENTAL_CONSOLE_MOUSE)
static bool auto_detect_service_classic_mouse() {
#if defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE) || defined(ADAPT_FEATURE_GENESIS_IDENTIFICATION_CAPTURE)
  // These RAM-only diagnostic builds must not reboot solely to change USB.
  return false;
#else
  static AutoClassicMouseDebounce debounce;
#if ADAPT_POLICY_RESET_HOLD_AUTO_OUTPUT && defined(ENABLE_INPUT_AUTODETECT)
  if (inputAutoDetectRebootPending()) {
    debounce.pending = false;
    return false;
  }
#endif
  if (!is_auto_output_mode_selected() ||
      (autoDetectState != AUTO_STATE_WINDOWS && autoDetectState != AUTO_STATE_FALLBACK_HID)) {
    debounce.pending = false;
    return false;
  }
  const bool currentMouse = canonicalizeOutputMode(outputMode) == OUTPUT_ROTARY_MOUSE;
  constexpr uint8_t consoleSignals = DETECT_PS3 | DETECT_PS4 | DETECT_OGXBOX |
      DETECT_XBOX360 | DETECT_SWITCH1 | DETECT_SWITCH2;
  // A generic fallback without a mounted host may still be a console waiting
  // for its own probe; do not select a mouse based on missing USB traffic.
  if (!currentMouse && (!tud_mounted() ||
      (autoDetectState == AUTO_STATE_FALLBACK_HID &&
       (output_autodetect_last_flags & consoleSignals) != 0))) {
    debounce.pending = false;
    return false;
  }
  uint32_t connected = 0;
  for (uint8_t port = 0; port < inputPortCount() && port < MAX_USB_OUT && port < 32; ++port) {
    if (inputFrameConst(port).connected) connected |= uint32_t(1) << port;
  }
  const bool wanted = autoClassicMouseWanted(connected, classicMouseInputMask(),
                                              rotaryMouseOutputPlayerCount());
  const uint32_t now = millis();
  // An empty input may still be scanning. Keep the current descriptor until
  // a connected source provides a reason to change it.
  if (currentMouse && connected == 0) {
    debounce.pending = false;
    return false;
  }
  if (!debounce.ready(wanted, currentMouse, now)) return false;
  const outputMode_t target = wanted ? OUTPUT_ROTARY_MOUSE :
      (autoDetectState == AUTO_STATE_WINDOWS ? get_configured_windows_auto_output_mode()
                                            : get_configured_mister_auto_output_mode());
  auto_detect_trigger_reenum(static_cast<uint32_t>(target));
  return true;
#endif
}
#endif

extern "C" void auto_detect_process() {
  static uint32_t ps5_probe_started_at_ms = 0;
  static uint32_t playstation_family_started_at_ms = 0;
  static uint32_t xboxone_probe_started_at_ms = 0;

#if defined(ENABLE_OUTPUT_PS5)
  if (is_auto_output_mode_selected() &&
      autoDetectState == AUTO_STATE_PS5 &&
      canonicalizeOutputMode(outputMode) == OUTPUT_PS5) {
    const uint32_t ms_now = millis();
    if (ps5_probe_started_at_ms == 0) {
      ps5_probe_started_at_ms = ms_now;
    }
    const uint32_t elapsed_ms = ms_now - ps5_probe_started_at_ms;
    output_autodetect_last_ms = (elapsed_ms > 0xFFFFu)
      ? 0xFFFFu
      : (uint16_t)elapsed_ms;

    if (ps5_probe_has_native_host_traffic()) {
      return;
    }

    if (ps5_probe_should_fallback_to_ps4(elapsed_ms)) {
      autoDetectState = AUTO_STATE_PS4;
      output_autodetect_transition_reason = AUTO_TRANSITION_PS5_PROBE_TO_PS4;
      auto_detect_trigger_reenum((uint32_t)OUTPUT_PS4);
    }
    return;
  }
#endif

  ps5_probe_started_at_ms = 0;

  if (is_auto_output_mode_selected() &&
      autoDetectState == AUTO_STATE_PLAYSTATION &&
      canonicalizeOutputMode(outputMode) == OUTPUT_PS4) {
    const uint32_t ms_now = millis();
    if (playstation_family_started_at_ms == 0) {
      playstation_family_started_at_ms = ms_now;
    }
    const uint32_t elapsed_ms = ms_now - playstation_family_started_at_ms;
    output_autodetect_last_ms = (elapsed_ms > 0xFFFFu)
      ? 0xFFFFu
      : (uint16_t)elapsed_ms;

    // The generic 0x03 request identifies only the PlayStation family. A PS4
    // follows the PS4 persona with F0 challenge pages; the live PS5 capture did
    // not. If a PS5 provider is physically present and no PS4 challenge arrives in
    // the bounded grace period, try the PS5 persona exactly once.
    if (playstation_ps4_auth_challenge_seen()) {
      autoDetectState = AUTO_STATE_PS4;
      auto_detect_persist_runtime_mode(outputMode, autoDetectState);
      return;
    }

    if (elapsed_ms >= playstation_family_fallback_ms()) {
#if defined(ENABLE_OUTPUT_PS5)
      if (ps_auth_dongle_ps5_provider_observed()) {
        autoDetectState = AUTO_STATE_PS5;
        output_autodetect_transition_reason =
          AUTO_TRANSITION_PLAYSTATION_NO_PS4_AUTH_TO_PS5;
        auto_detect_trigger_reenum((uint32_t)OUTPUT_PS5);
        return;
      }
#endif
      autoDetectState = AUTO_STATE_PS4;
      auto_detect_persist_runtime_mode(outputMode, autoDetectState);
    }
    return;
  }

  playstation_family_started_at_ms = 0;
#ifdef ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT
  if (is_auto_output_mode_selected() &&
      autoDetectState == AUTO_STATE_IDLE &&
      autoDetectProbeStage == AUTO_PROBE_XBOXONE) {
    const uint32_t ms_now = millis();
    if (xboxone_probe_started_at_ms == 0) {
      xboxone_probe_started_at_ms = ms_now;
    }
    const uint32_t elapsed_ms = ms_now - xboxone_probe_started_at_ms;
    output_autodetect_last_ms = (elapsed_ms > 0xFFFFu)
      ? 0xFFFFu
      : (uint16_t)elapsed_ms;
    const XboxOneOutputStatus probe = xboxone_output_status();
    const bool nativeXboxHostMounted =
        probe.mounted && probe.usb_mounted &&
        probe.setup_request_count != 0;
    if (probe.auth_console_count != 0 || nativeXboxHostMounted) {
      autoDetectState = AUTO_STATE_XBOX_ONE;
      autoDetectProbeStage = AUTO_PROBE_GENERIC;
      outputMode = OUTPUT_XBOXONE;
      output_autodetect_last_flags = DETECT_XBOXONE_CANDIDATE;
      output_autodetect_transition_reason =
        AUTO_TRANSITION_XBOXONE_PROBE_CONFIRMED;
      // The staged USB identity already is the final native Xbox personality.
      // Persist it in place instead of rebooting a third time and making the
      // still-powered authentication donor enumerate yet again. Authentication
      // remains a separate locked/unlocked state and may finish after detection.
      auto_detect_persist_debug_snapshot(
          output_autodetect_last_flags, output_autodetect_aux_flags);
      auto_detect_persist_runtime_mode(outputMode, autoDetectState);
#if defined(ENABLE_DEBUG_UART)
      printOutputAutodetectDiagnostics(Serial2, "RESOLVE_IN_PLACE");
      Serial2.flush();
#endif
      return;
    }

    if (elapsed_ms >= kXboxOneAssistResolveMs) {
      output_autodetect_transition_reason =
        AUTO_TRANSITION_XBOXONE_PROBE_TO_FALLBACK;
      auto_detect_trigger_fallback_hid();
      return;
    }
    return;
  }
#endif

  xboxone_probe_started_at_ms = 0;


  if (serviceRemoteOutputSelection()) return;

  if (auto_detect_promote_windows_xinput_to_xbox360()) {
    return;
  }

#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB) && defined(ENABLE_EXPERIMENTAL_CONSOLE_MOUSE)
  if (auto_detect_service_classic_mouse()) return;
#endif

  if (!can_run_usb_detection())
    return;

  const outputMode_t automode_windows_output = get_configured_windows_auto_output_mode();
  // Use RetroZord's host-detect probe, then enumerate as Switch Pro for the
  // final controller personality.
  const outputMode_t automode_switch1_output = OUTPUT_SWITCHPRO;
  static uint32_t auto_detect_started_at_ms = 0;

  if (auto_detect_started_at_ms == 0) {
    auto_detect_started_at_ms = millis();
  }

  uint32_t ms_now = millis();
  uint32_t auto_detect_elapsed_ms = ms_now - auto_detect_started_at_ms;
  output_autodetect_last_ms = (auto_detect_elapsed_ms > 0xFFFFu)
    ? 0xFFFFu
    : (uint16_t)auto_detect_elapsed_ms;
  sync_xinput_descriptor_hints();
  output_autodetect_last_flags = host_detection.any |
    ((autoDetectState == AUTO_STATE_FALLBACK_HID) ? DETECT_TIMEOUT : 0);
  output_autodetect_aux_flags = usb_detect_get_aux_flags();
  update_auto_detect_probe_counters();

#if defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED)
  update_output_autodetect_rp2040_usb_debug(ms_now, auto_detect_elapsed_ms);
#endif

  if (autoDetectProbeStage != AUTO_PROBE_GENERIC &&
      autoDetectProbeStage != AUTO_PROBE_PS3) {
    // Older builds could leave XID/XInput staged probes in watchdog scratch.
    // Keep only the generic detector plus the delayed PS3 assist stage.
    autoDetectProbeStage = AUTO_PROBE_GENERIC;
  }

  const uint8_t detected_host = usb_host_detection_task();
#if defined(ADAPT_FEATURE_HOST_CAPTURE)
  if (detected_host != DETECT_NONE) {
    hostCaptureDetect(detected_host, host_detection.any,
                      host_detection.string_manufacturer_read,
                      host_detection.hid_descriptor_report_read,
                      auto_detect_elapsed_ms);
  }
#endif

  if (detected_host == DETECT_NONE &&
      autoDetectProbeStage == AUTO_PROBE_PS3 &&
      auto_detect_elapsed_ms >= kPs3AssistResolveMs) {
#ifdef ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT
    // The Xbox and Windows WCID requests overlap. At the PS3-assist timeout,
    // give the request-level detector one more scheduler pass to distinguish
    // an Xbox host (no iManufacturer read) from Windows. Donor readiness is
    // intentionally irrelevant: the controller may not mount until after the
    // Xbox output personality is active.
    if ((host_detection.any & DETECT_WINDOWS) != 0 &&
        !host_detection.string_manufacturer_read) {
      return;
    }
#endif
    output_autodetect_transition_reason = AUTO_TRANSITION_GENERIC_NO_MOUNT_TO_FALLBACK;
    auto_detect_trigger_fallback_hid();
    return;
  }

  if (detected_host == DETECT_NONE &&
      autoDetectProbeStage == AUTO_PROBE_GENERIC &&
      !tud_mounted() &&
      auto_detect_elapsed_ms >=
          auto_detect_generic_no_mount_resolve_ms()) {
    if (tud_connected()) {
      // PS3 often needs the Sony-shaped discriminator descriptor before it
      // asks for the feature reports that identify it. Only connected hosts
      // that did not mount the generic probe get this assist stage.
      output_autodetect_transition_reason = AUTO_TRANSITION_GENERIC_PS3_ASSIST;
      auto_detect_trigger_probe_stage(AUTO_PROBE_PS3);
      return;
    } else {
      output_autodetect_transition_reason = AUTO_TRANSITION_GENERIC_NO_CONNECT_TO_FALLBACK;
    }
    auto_detect_trigger_fallback_hid();
    return;
  }

  switch (detected_host) {
    case DETECT_NONE:
      break;

    case DETECT_PS3:
      auto_detect_trigger_ps3_hid_fallback();
      break;

    case DETECT_WINDOWS:
      auto_detect_trigger_windows_output(automode_windows_output);
      break;

    case DETECT_PS4:
      auto_detect_trigger_playstation_family_output();
      break;

    case DETECT_OGXBOX:
      autoDetectProbeStage = AUTO_PROBE_GENERIC;
      autoDetectState = AUTO_STATE_OG_XBOX;
      if (outputMode != OUTPUT_XID) {
        auto_detect_trigger_reenum((uint32_t)OUTPUT_XID);
      }
      break;

    case DETECT_XBOX360:
      auto_detect_trigger_xinput_output();
      break;

    case DETECT_SWITCH1:
#if !defined(ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT)
    case DETECT_SWITCH2:
#endif
    {
      // A Switch 2 gets its System Settings choice; a Switch 1 keeps Switch Pro.
      const outputMode_t switchOutput = detected_host == DETECT_SWITCH1
          ? automode_switch1_output : get_configured_switch2_auto_output_mode();
      autoDetectProbeStage = AUTO_PROBE_GENERIC;
      autoDetectState = AUTO_STATE_SWITCH;
      if (outputMode != switchOutput) {
        auto_detect_trigger_reenum((uint32_t)switchOutput);
      }
      break;
    }

#ifdef ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT
    case DETECT_XBOXONE_CANDIDATE:
      output_autodetect_last_flags = DETECT_XBOXONE_CANDIDATE;
      output_autodetect_aux_flags = usb_detect_get_aux_flags();
      // Host identity must not depend on an auth donor being attached. Stage
      // the Xbox personality, then require a real console authentication
      // packet before accepting the host as Xbox One/Series.
      auto_detect_trigger_xboxone_probe(
          AUTO_TRANSITION_GENERIC_XBOXONE_FINGERPRINT_TO_PROBE);
      return;
#endif

    case DETECT_TIMEOUT:
      if (autoDetectProbeStage == AUTO_PROBE_GENERIC) {
        output_autodetect_transition_reason = AUTO_TRANSITION_GENERIC_PS3_ASSIST;
        auto_detect_trigger_probe_stage(AUTO_PROBE_PS3);
        return;
      }
      auto_detect_trigger_fallback_hid();
      break;

    default:
      auto_detect_trigger_fallback_hid();
      break;
  }
}

#else

#include "../output_runtime_state.h"

bool auto_detect_no_data_host_fallback_active() {
  return false;
}

bool auto_detect_preserve_current_runtime_mode_for_input_reboot() {
  return auto_detect_preserve_resolved_runtime_mode_for_reboot();
}

#endif  // ADAPT_OUTPUT_USB_DEVICE
