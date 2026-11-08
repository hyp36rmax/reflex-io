#include "../../product_config.h"

#include "platform_feedback_runtime.h"
#include "platform_feedback_led_runtime.h"

#include <Arduino.h>
#include <string.h>

#include "../../core/controller_frame_state.h"
#include "../../core/device_runtime_state.h"
#include "../../core/controller_settings_state.h"
#include "../../features/feature_module.h"
#include "../../firmware_platform_config.h"
#include "../../menu/menu_bridge.h"
#include "../../menu/menu_idle_runtime.h"
#include "../../menu/menu_screensaver_bounce.h"
#include "../../menu/menu_system_screens.h"
#include "../../menu/menu_ui_state.h"
#include "../../output/output_runtime_state.h"
#include "../display_runtime_state.h"
#include "../rgb_led.h"
#include "../webhid_runtime.h"

#if defined(ENABLE_USB_AUTH_SIDECAR) || defined(ENABLE_USB_AUTH_SIDECAR_CORE1)
#include "../../input/usb_host/input_usb_host_service.h"
#include "../../output/auth/ps_auth_dongle_runtime.h"
#endif

namespace {

uint32_t last_display_update = 0;

void recordWebHidInputStats() {
  const controller_state_t& primary = controllerFrameConst(0);
  webhid_record_input(
    (max_devices > 0) ? primary.digital_buttons : 0,
    (max_devices > 0) ? primary.LX : 0,
    (max_devices > 0) ? primary.LY : 0,
    (max_devices > 0) ? primary.RX : 0,
    (max_devices > 0) ? primary.RY : 0
  );
}

#ifdef USE_I2C_DISPLAY
// A redraw blocks the main loop, so these are the time input and USB get
// between redraws: a full frame takes about 27 ms at 400 kHz. A previous 1 ms
// animation path could saturate I2C updates enough to destabilize USB.
constexpr uint32_t kOledFrameIntervalMs = 16;
// The bouncing logo sends a quarter of the panel, about 7 ms, so this redraws
// it at about 60 Hz.
constexpr uint32_t kLogoFrameIntervalMs = 8;
constexpr uint32_t kJvsDisplayIntervalMs = 100;

uint32_t oledFrameIntervalMs() {
  return bounceScreensaverActive() ? kLogoFrameIntervalMs : kOledFrameIntervalMs;
}

#ifdef ENABLE_INPUT_JVS
// Each JVS redraw holds up the next poll. A screensaver can have that time,
// unless Wake = Adapt keeps it running during play.
uint32_t jvsDisplayIntervalMs() {
  if (!idleAnimationActive || isOledWakeAdaptOnly()) {
    return kJvsDisplayIntervalMs;
  }
  return oledFrameIntervalMs();
}
#endif
#endif

void renderMainDisplayIfNeeded() {
  #ifdef USE_I2C_DISPLAY
    #ifdef ENABLE_INPUT_JVS
    if (deviceMode == RZORD_JVS) {
      // In JVS mode the donor firmware only touches the OLED in the safe window
      // between host() and sync(). Defer shared home-screen redraws to that
      // path so I2C writes do not steal time from UART RX mid-transaction.
      return;
    }
    #endif
    if (!menuOwnsControllerInput() &&
        millis() - last_display_update >= oledFrameIntervalMs()) {
      beginDisplayWire();
      if (!renderFirmwareUpdateScreenIfActive() &&
          !featureModulesRenderOledTransient()) {
        renderMainDisplay();
      }
      endDisplayWire();
      last_display_update = millis();
    }
  #endif
}

bool renderPs5DiagAuthStatusIfNeeded(bool force = false) {
#if ADAPT_POLICY_PS5_DIAGNOSTIC_RUNTIME && defined(USE_I2C_DISPLAY) && \
    !defined(PS5_DIAG_SUPPRESS_OLED_POLL) && \
    (defined(ENABLE_USB_AUTH_SIDECAR) || defined(ENABLE_USB_AUTH_SIDECAR_CORE1))
  constexpr uint32_t kPs5DiagStatusIntervalMs = 250;
  if (!force && millis() - last_display_update < kPs5DiagStatusIntervalMs) {
    return true;
  }

  const UsbHostServiceStatus host = usb_host_service_status();
  const PsAuthDongleStatus auth = ps_auth_dongle_status();

  beginDisplayWire();
  display.clear();
  display.setFont(System5x7);
  display.set1X();
  display.setRow(0);
  display.setCol(0);
  display.print(F("P5D O"));
  display.print((int)outputMode);
  display.print(F(" C"));
  display.print((int)configuredOutputMode);
  display.print(F(" "));
  display.print(auth.signature_ready ? F("KEY") : F("LOCK"));

  display.setRow(1);
  display.setCol(0);
  display.print(F("H"));
  display.print(host.started ? 1 : 0);
  display.print(F(" R"));
  display.print(host.start_requested ? 1 : 0);
  display.print(F(" C"));
  display.print(auth.connected ? 1 : 0);
  display.print(F(" RDY"));
  display.print(auth.signature_ready ? 1 : 0);

  display.setRow(2);
  display.setCol(0);
  display.print(F("L1"));
  display.print(host.core1_loop_count);
  display.print(F(" PH"));
  display.print((int)host.setup_phase);
  display.print(F(" SU"));
  display.print(host.setup_count);

  display.setRow(3);
  display.setCol(0);
  display.print(F("T"));
  display.print(host.task_count);
  display.print(F(" RB"));
  display.print((int)auth.ps5_raw_buttons0, HEX);
  display.print(F("/"));
  display.print((int)auth.ps5_raw_buttons1, HEX);
  display.print(F("/"));
  display.print((int)auth.ps5_raw_buttons2, HEX);

  display.setRow(4);
  display.setCol(0);
  display.print(F("SB"));
  display.print((int)auth.ps5_signed_buttons0, HEX);
  display.print(F("/"));
  display.print((int)auth.ps5_signed_buttons1, HEX);
  display.print(F("/"));
  display.print((int)auth.ps5_signed_buttons2, HEX);
  display.print(F(" P"));
  display.print((int)auth.protocol);
  display.print(F(" E"));
  display.print((int)auth.last_error);
  display.print(F(" S"));
  display.print((int)auth.state);
  display.print(F(" Q"));
  display.print((int)auth.ps5_hash_flags, HEX);

  display.setRow(5);
  display.setCol(0);
  display.print(F("VID "));
  display.print(auth.vid, HEX);
  display.print(F(":"));
  display.print(auth.pid, HEX);

  display.setRow(6);
  display.setCol(0);
  display.print(F("H"));
  display.print((int)auth.last_host_get_id, HEX);
  display.print(F("/"));
  display.print(auth.last_host_get_len);
  display.print(F(" S"));
  display.print((int)auth.last_host_set_id, HEX);
  display.print(F("/"));
  display.print(auth.last_host_set_len);
  display.print(F(" G"));
  display.print(auth.get_report_count);
  display.print(F("/"));
  display.print(auth.set_report_count);

  display.setRow(7);
  display.setCol(0);
  display.print(F("F"));
  display.print((int)auth.last_p5_f0_command, HEX);
  display.print(F("/"));
  display.print((int)auth.last_p5_f0_detail, HEX);
  display.print(F(" F2"));
  display.print((int)auth.ps5_f2_status0, HEX);
  display.print(F("/"));
  display.print((int)auth.ps5_f2_status1, HEX);
  display.print(F(" T"));
  display.print(auth.ps5_host_submit_count);
  display.print(F(" I"));
  display.print(auth.input_report_count);
  display.print(F(" M"));
  display.print(auth.ps5_hash_mismatch_count);
  endDisplayWire();
  last_display_update = millis();
  return true;
#else
  return false;
#endif
}

#if defined(ENABLE_OUTPUT_PS5) && defined(USE_I2C_DISPLAY) && \
    (defined(ENABLE_USB_AUTH_SIDECAR) || defined(ENABLE_USB_AUTH_SIDECAR_CORE1))
uint32_t ps5QuietHomeStateSignature(const PsAuthDongleStatus& auth) {
  uint32_t signature = 2166136261u;
  const auto mix = [&signature](uint8_t value) {
    signature ^= value;
    signature *= 16777619u;
  };

  mix(auth.connected ? 1 : 0);
  mix(auth.signature_ready ? 1 : 0);
  mix(max_devices);
  for (uint8_t index = 0; index < max_devices && index < MAX_USB_OUT; ++index) {
    const controller_state_t& frame = controllerFrameConst(index);
    mix(frame.connected ? 1 : 0);
    mix(frame.config);
    for (char value : frame.controller_type_name) {
      mix(static_cast<uint8_t>(value));
    }
  }
  return signature;
}
#endif

void renderPs5QuietHomeStatusIfNeeded() {
#if defined(ENABLE_OUTPUT_PS5) && defined(USE_I2C_DISPLAY) && \
    (defined(ENABLE_USB_AUTH_SIDECAR) || defined(ENABLE_USB_AUTH_SIDECAR_CORE1))
  static bool rendered = false;
  static uint32_t lastStateSignature = 0;

  if (menuOwnsControllerInput()) {
    return;
  }

  const PsAuthDongleStatus auth = ps_auth_dongle_status();
  const uint32_t stateSignature = ps5QuietHomeStateSignature(auth);
  if (rendered && mainDisplayInitialized &&
      stateSignature == lastStateSignature) {
    return;
  }

  // Native PS5 keeps periodic OLED work disabled for auth timing, but leaving
  // the boot logo forever looks like a hung device. Render once after boot,
  // after menu close, and when the provider/player identity changes.
  beginDisplayWire();
  renderMainDisplay();
  endDisplayWire();
  last_display_update = millis();
  lastStateSignature = stateSignature;
  rendered = true;
#endif
}
void renderJvsSafeWindowDisplayIfNeededInternal() {
  #ifdef USE_I2C_DISPLAY
  #ifdef ENABLE_INPUT_JVS
  if (deviceMode != RZORD_JVS || menuOwnsControllerInput()) {
    return;
  }

  if (renderPs5DiagAuthStatusIfNeeded()) {
    return;
  }

  if (millis() - last_display_update < jvsDisplayIntervalMs()) {
    return;
  }

  beginDisplayWire();
  if (!renderFirmwareUpdateScreenIfActive() &&
      !featureModulesRenderOledTransient()) {
    renderMainDisplay();
  }
  endDisplayWire();
  last_display_update = millis();
  #endif
  #endif
}

}  // namespace

void runPlatformFeedbackServices(bool updated) {
  if (is_ps5_timing_quiet_mode_active()) {
#if ADAPT_POLICY_PS5_DIAGNOSTIC_RUNTIME && !defined(PS5_DIAG_SUPPRESS_OLED_POLL)
    renderPs5DiagAuthStatusIfNeeded();
#else
    renderPs5QuietHomeStatusIfNeeded();
#endif
    return;
  }

  recordWebHidInputStats();
  updatePlatformRgbLedFromInput(updated);
  if (renderPs5DiagAuthStatusIfNeeded()) {
    return;
  }
  renderMainDisplayIfNeeded();
}

void runJvsSafeWindowDisplayIfNeeded() {
  renderJvsSafeWindowDisplayIfNeededInternal();
}

bool runPs5DiagAuthStatusDisplay() {
  return renderPs5DiagAuthStatusIfNeeded();
}

bool runPs5DiagAuthStatusDisplayForced() {
  return renderPs5DiagAuthStatusIfNeeded(true);
}
