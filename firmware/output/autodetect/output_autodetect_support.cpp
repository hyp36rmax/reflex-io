#include "../../product_config.h"
#include "../runtime/input_runtime_output_bridge.h"
#include "../auth/auth_status.h"
#include "../output_runtime_state.h"
#include "output_autodetect_support.h"
#if ADAPT_POLICY_RESET_HOLD_AUTO_OUTPUT && defined(ENABLE_INPUT_AUTODETECT)
#include "../../input/autodetect/input_autodetect_runtime_state.h"
#endif

extern "C" bool can_run_usb_detection(void) {
#if ADAPT_POLICY_RESET_HOLD_AUTO_OUTPUT && defined(ENABLE_INPUT_AUTODETECT)
  if (inputAutoDetectRebootPending()) return false;
#endif
  if (!is_auto_output_mode_selected() || autoDetectState != AUTO_STATE_IDLE) {
    return false;
  }
#ifdef ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT
  // The staged Xbox personality is itself the discriminator. Its native GIP
  // class driver must own every vendor/control request so it can observe and
  // forward the console authentication challenge. The generic detector would
  // otherwise consume the shared WCID request as Windows and starve GIP auth.
  if (autoDetectProbeStage == AUTO_PROBE_XBOXONE) return false;
#endif
  return true;
}

uint16_t usb_detect_probe_device_version(void) {
  // AUTO probing is a distinct logical profile, not a new report ABI.
  // Keeping HH stable avoids creating a new GameControllerDB generation.
  constexpr uint8_t kAutoProbeProfile = 99;
  return encode_bcd_device_version(bcd_device_version.revision,
                                   kAutoProbeProfile);
}

bool is_ps5_extended_feature_probe(uint8_t report_id) {
  switch (report_id) {
    // Extended Sony/DS4-style feature IDs beyond the minimal 0x03 + F0-F3 auth path.
    // If a PlayStation-family host asks for any of these after AUTO has already
    // resolved to the PS4-compatible descriptor, treat that as a best-effort
    // PS5 refinement instead of adding another reboot stage.
    case 0x02:
    case 0x05:
    case 0x08:
    case 0x09:
    case 0x0A:
    case 0x12:
    case 0x20:
    case 0x21:
    case 0x81:
    case 0x82:
    case 0x83:
    case 0x84:
    case 0x85:
    case 0xA3:
    case 0xA4:
    case 0xA5:
      return true;
    default:
      return false;
  }
}
