#include "output_runtime_state.h"
#include "output_runtime_product_policy.h"
#include "output_persona_availability.h"

#include "../core/settings_store.h"
#include "../core/controller_settings_state.h"
#include "../menu/menu_runtime_state.h"
#include "switch/switch_device_selection.h"
#if ADAPT_POLICY_RESET_HOLD_AUTO_OUTPUT && defined(ENABLE_INPUT_AUTODETECT)
#include "../input/autodetect/input_autodetect_runtime_state.h"
#endif

namespace {

constexpr uint32_t kAutoDetectScratchOutputMask = 0xFFu;
constexpr uint32_t kAutoDetectScratchStateShift = 8u;
constexpr uint32_t kPs5DiagBootArmMagic = 0x50354447u;  // "P5DG"

bool isPersistedAutoDetectStateValid(uint8_t raw) {
#ifdef ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT
  return raw > (uint8_t)AUTO_STATE_IDLE &&
         raw <= (uint8_t)AUTO_STATE_XBOX_ONE;
#else
  return raw > (uint8_t)AUTO_STATE_IDLE &&
         raw <= (uint8_t)AUTO_STATE_PLAYSTATION;
#endif
}

bool isSpecializedMisterPsxOutputMode(outputMode_t mode) {
  switch (canonicalizeOutputMode(mode)) {
    case OUTPUT_MISTER_JOGCON:
    case OUTPUT_MISTER_NEGCON:
    case OUTPUT_MISTER_PADDLE:
    case OUTPUT_MISTER_SPINNER:
    case OUTPUT_MISTER_GUNCON:
    case OUTPUT_ROTARY_MOUSE:
      return true;
    default:
      return false;
  }
}

bool misterAutoOutputAcceptsRuntimeMode(outputMode_t mode) {
  const outputMode_t preferredMode =
    canonicalizeOutputMode(get_configured_mister_auto_output_mode());
  const outputMode_t runtimeMode = canonicalizeOutputMode(mode);
  if (runtimeMode == preferredMode) {
    return true;
  }

  // JogCon, neGcon, and GunCon are descriptor refinements of MiSTer DInput,
  // not different host detections. Preserve them through the reboot that
  // installs their specialized HID descriptor.
  return preferredMode == OUTPUT_MISTER &&
         isSpecializedMisterPsxOutputMode(runtimeMode);
}

}  // namespace

outputMode_t configuredOutputMode = OUTPUT_AUTO;
outputMode_t outputMode = OUTPUT_MISTER;
socdMode_t socdMode = SOCD_OFF;
bool use_device_hid_class = false;
uint8_t hid_begin_success_count = 0;
uint8_t hid_first_fail_index = 255;

AutoDetectProbeStage autoDetectProbeStage = AUTO_PROBE_GENERIC;
AutoDetectState autoDetectState = AUTO_STATE_IDLE;

uint16_t output_autodetect_last_ms = 0;
uint8_t output_autodetect_last_flags = 0;
uint8_t output_autodetect_aux_flags = 0;
uint8_t output_autodetect_usb_hw_flags = 0;
uint8_t output_autodetect_usb_irq_observed = 0;
uint8_t output_autodetect_usb_irq_flags = 0;
uint16_t output_autodetect_probe_counters = 0;
uint8_t output_autodetect_transition_reason = AUTO_TRANSITION_NONE;

namespace {

bool n64NsoPlayerCountAllowed = false;

}  // namespace

bool input_mode_supports_nso_special(DeviceEnum mode) {
  switch (mode) {
    #ifdef ENABLE_INPUT_NES
    case RZORD_NES:
    #endif
    #ifdef ENABLE_INPUT_SNES
    case RZORD_SNES:
    #endif
    #ifdef ENABLE_INPUT_MEGADRIVE
    case RZORD_MEGADRIVE:
    #endif
    #ifdef ENABLE_INPUT_N64
    case RZORD_N64:
    #endif
    #ifdef ENABLE_INPUT_GAMECUBE
    case RZORD_GAMECUBE:
    #endif
    #ifdef ENABLE_INPUT_USB
    case RZORD_USB:
    #endif
    #ifdef ENABLE_INPUT_JVS
    case RZORD_JVS:
    #endif
      return true;
    default:
      return false;
  }
}

uint8_t max_switch_device_selection_for_input_mode(DeviceEnum mode) {
  #ifdef ENABLE_INPUT_USB
  if (mode == RZORD_USB) return SWITCH_DEVICE_GC_ADAPTER;
  #endif
  #ifdef ENABLE_INPUT_JVS
  if (mode == RZORD_JVS) return SWITCH_DEVICE_GC_ADAPTER;
  #endif
  return input_mode_supports_nso_special(mode) ? 1 : 0;
}

uint8_t sanitize_switch_device_selection(DeviceEnum mode, uint8_t selection) {
  return selection <= max_switch_device_selection_for_input_mode(mode)
      ? selection
      : static_cast<uint8_t>(SWITCH_DEVICE_PRO);
}

bool switch_device_selection_uses_gc_adapter(DeviceEnum mode,
                                             uint8_t selection) {
  selection = sanitize_switch_device_selection(mode, selection);
  #ifdef ENABLE_INPUT_GAMECUBE
  if (mode == RZORD_GAMECUBE) return selection != SWITCH_DEVICE_PRO;
  #endif
  #if defined(ENABLE_INPUT_USB) || defined(ENABLE_INPUT_JVS)
  return selection == SWITCH_DEVICE_GC_ADAPTER;
  #else
  (void)selection;
  return false;
  #endif
}

bool switch_device_selection_is_nso_for_input_mode(DeviceEnum mode,
                                                   uint8_t selection) {
  selection = sanitize_switch_device_selection(mode, selection);
  return selection != SWITCH_DEVICE_PRO &&
         !switch_device_selection_uses_gc_adapter(mode, selection);
}

const char* switch_device_selection_label(DeviceEnum mode, uint8_t selection) {
  selection = sanitize_switch_device_selection(mode, selection);
  if (selection == SWITCH_DEVICE_PRO) return "Switch Pro";
  #ifdef ENABLE_INPUT_GAMECUBE
  if (mode == RZORD_GAMECUBE) return "GC Adapter";
  #endif
  #ifdef ENABLE_INPUT_NES
  if (mode == RZORD_NES) return "NES NSO";
  #endif
  #ifdef ENABLE_INPUT_SNES
  if (mode == RZORD_SNES) return "SNES NSO";
  #endif
  #ifdef ENABLE_INPUT_MEGADRIVE
  if (mode == RZORD_MEGADRIVE) return "Genesis NSO";
  #endif
  #ifdef ENABLE_INPUT_N64
  if (mode == RZORD_N64) return "N64 NSO";
  #endif
  switch (selection) {
    case SWITCH_DEVICE_GENESIS_NSO: return "Genesis NSO";
    case SWITCH_DEVICE_NES_NSO: return "NES NSO";
    case SWITCH_DEVICE_SNES_NSO: return "SNES NSO";
    case SWITCH_DEVICE_N64_NSO: return "N64 NSO";
    case SWITCH_DEVICE_GC_ADAPTER: return "GC Adapter";
    default: return "Switch Pro";
  }
}

bool output_mode_supports_switch_device_selection(outputMode_t mode) {
  mode = canonicalizeOutputMode(mode);
  if (mode == OUTPUT_SWITCHPRO) return true;
  return mode == OUTPUT_GCWIIU &&
         canonicalizeOutputMode(outputMode) == OUTPUT_SWITCHPRO;
}

bool input_mode_has_automatic_nso_profile(DeviceEnum mode) {
#ifdef ENABLE_INPUT_N64
  return mode == RZORD_N64;
#else
  (void)mode;
  return false;
#endif
}

bool input_mode_allows_nso_special_for_connected_players(
    DeviceEnum mode, uint8_t connectedPlayers) {
  if (!input_mode_has_automatic_nso_profile(mode)) {
    return true;
  }
  return connectedPlayers == 1;
}

void latch_nso_special_connected_players(DeviceEnum mode,
                                         uint8_t connectedPlayers) {
  n64NsoPlayerCountAllowed =
      input_mode_allows_nso_special_for_connected_players(
          mode, connectedPlayers);
}

bool nso_special_connected_player_latch() { return n64NsoPlayerCountAllowed; }

void restore_nso_special_connected_player_latch(bool allowed) {
  n64NsoPlayerCountAllowed = allowed;
}

bool nso_special_effective_for_input_mode(DeviceEnum mode) {
  if (!switch_device_selection_is_nso_for_input_mode(mode, nso_special)) {
    return false;
  }
  return !input_mode_has_automatic_nso_profile(mode) ||
         n64NsoPlayerCountAllowed;
}

bool is_nso_special_active() {
  return canonicalizeOutputMode(outputMode) == OUTPUT_SWITCHPRO &&
         nso_special_effective_for_input_mode(deviceMode);
}

outputMode_t canonicalizeOutputMode(outputMode_t mode) {
  if (mode == OUTPUT_RESERVED_JOGCON) {
    return OUTPUT_MISTER_JOGCON;
  }
  if (mode == OUTPUT_RESERVED_MOUSE) {
    return OUTPUT_MISTER;
  }
  if (mode == OUTPUT_XINPUTW) {
    return OUTPUT_XINPUT2P;
  }
  return mode;
}

outputMode_t sanitizeConfiguredOutputMode(outputMode_t mode) {
  return foldUnbuiltOutputPersona(canonicalizeOutputMode(mode));
}

bool is_xinput2p_output_enabled() {
#ifdef ENABLE_EXPERIMENTAL_XINPUT2P_OUTPUT
  return true;
#else
  return false;
#endif
}

outputMode_t sanitizeRuntimeOutputMode(outputMode_t mode) {
  mode = sanitizeConfiguredOutputMode(mode);
  #if defined(ADAPT_PRIMARY_EGRESS_CLASSIC_CONSOLE)
  if (mode == OUTPUT_CONSOLE_NES || mode == OUTPUT_CONSOLE_SNES) {
    return mode;
  }
  return OUTPUT_CONSOLE_SNES;
  #elif defined(ADAPT_PRIMARY_EGRESS_JVS_BOARD)
  (void)mode;
  return OUTPUT_JVS;
  #elif defined(ADAPT_PRIMARY_EGRESS_DB15)
  (void)mode;
  return OUTPUT_DB15_SUPERGUN;
  #else
  outputMode_t forcedMode = mode;
  if (productForcedRuntimeOutputMode(forcedMode)) {
    return forcedMode;
  }
  if (mode == OUTPUT_XINPUT) {
    return OUTPUT_XINPUT;
  }
  if (mode == OUTPUT_XINPUT2P && !is_xinput2p_output_enabled()) {
    return OUTPUT_MISTER;
  }
  if (mode == OUTPUT_XBOXONE) {
    return productAllowsXboxOneRuntimeOutput() ? OUTPUT_XBOXONE : OUTPUT_MISTER;
  }
  if (mode == OUTPUT_AUTO) {
    return OUTPUT_MISTER;
  }
  return mode;
  #endif
}

namespace {

outputMode_t configuredHostOutputMode(uint8_t preference,
                                      outputMode_t dinputMode) {
  if (preference == HOST_OUTPUT_PREFERENCE_DINPUT ||
      preference > HOST_OUTPUT_PREFERENCE_MAX) {
    return dinputMode;
  }
  if (preference == HOST_OUTPUT_PREFERENCE_XINPUT) {
    return is_xinput2p_output_enabled() ? OUTPUT_XINPUT2P : dinputMode;
  }
  static const uint8_t modes[] = {
    OUTPUT_KEYBOARD,
    productWindowsXboxOnePreferenceMode(dinputMode),
    OUTPUT_PS3,
    OUTPUT_PS4,
#ifdef ENABLE_OUTPUT_PS5
    OUTPUT_PS5,
#else
    OUTPUT_PS4,
#endif
    OUTPUT_SWITCHPRO,
  };
  const outputMode_t mode =
      static_cast<outputMode_t>(modes[preference - HOST_OUTPUT_PREFERENCE_KEYBOARD]);
  return (preference == HOST_OUTPUT_PREFERENCE_XBOXONE && mode == OUTPUT_HID)
      ? dinputMode
      : mode;
}

}  // namespace

bool host_output_preference_available(uint8_t preference) {
  if (preference == HOST_OUTPUT_PREFERENCE_XBOXONE) {
    return configuredHostOutputMode(preference, OUTPUT_HID) == OUTPUT_XBOXONE;
  }
  return preference != HOST_OUTPUT_PREFERENCE_PS5 || outputPersonaIsBuilt(OUTPUT_PS5);
}

outputMode_t get_configured_windows_auto_output_mode() {
  return configuredHostOutputMode(menu_win_output, OUTPUT_HID);
}

outputMode_t get_configured_switch2_auto_output_mode() {
  return menu_switch2_output == SWITCH2_OUTPUT_PREFERENCE_SWITCH2PRO
    ? OUTPUT_SWITCH2PRO : OUTPUT_SWITCHPRO;
}

outputMode_t get_configured_mister_auto_output_mode() {
  if (productForcesMisterDinputPreference()) {
    return OUTPUT_MISTER;
  }
  return configuredHostOutputMode(menu_mister_output, OUTPUT_MISTER);
}

bool output_supports_dpad_buttons(outputMode_t mode) {
  return mode == OUTPUT_HID || mode == OUTPUT_MISTER;
}

bool is_ps5_timing_quiet_mode_active() {
#ifdef ENABLE_OUTPUT_PS5
  return outputMode == OUTPUT_PS5;
#else
  return false;
#endif
}

void auto_detect_clear_scratch_state() {
  watchdog_hw->scratch[AUTO_DETECT_SCRATCH_MAGIC] = 0;
  watchdog_hw->scratch[AUTO_DETECT_SCRATCH_STATE] = 0;
  auto_detect_clear_input_scratch_state();
  watchdog_hw->scratch[AUTO_PROBE_SCRATCH_STAGE] = 0;
  watchdog_hw->scratch[AUTO_PROBE_SCRATCH_AUX] = 0;
  output_autodetect_probe_counters = 0;
  output_autodetect_transition_reason = AUTO_TRANSITION_NONE;
}

void auto_detect_clear_input_scratch_state() {
  watchdog_hw->scratch[AUTO_INPUT_MODE_SCRATCH_MAGIC] = 0;
  watchdog_hw->scratch[AUTO_INPUT_MODE_SCRATCH_MODE] = 0;
}

void auto_detect_persist_probe_stage(AutoDetectProbeStage stage) {
  watchdog_hw->scratch[AUTO_PROBE_SCRATCH_STAGE] = (uint32_t)stage;
  watchdog_hw->scratch[AUTO_DETECT_SCRATCH_STATE] =
    ((uint32_t)output_autodetect_transition_reason << 24);
}

void auto_detect_persist_debug_snapshot(uint8_t flags, uint8_t aux_flags) {
  watchdog_hw->scratch[AUTO_PROBE_SCRATCH_AUX] =
    (uint32_t)flags |
    ((uint32_t)aux_flags << 8) |
    ((uint32_t)output_autodetect_probe_counters << 16);
}

void auto_detect_restore_debug_snapshot() {
  uint32_t snapshot = watchdog_hw->scratch[AUTO_PROBE_SCRATCH_AUX];
  output_autodetect_last_flags = (uint8_t)(snapshot & 0xFFu);
  output_autodetect_aux_flags = (uint8_t)((snapshot >> 8) & 0xFFu);
  output_autodetect_probe_counters = (uint16_t)((snapshot >> 16) & 0xFFFFu);
}

AutoDetectProbeStage auto_detect_load_probe_stage() {
  const uint32_t raw = watchdog_hw->scratch[AUTO_PROBE_SCRATCH_STAGE];
  if (raw == (uint32_t)AUTO_PROBE_PS3) {
    return AUTO_PROBE_PS3;
  }
#ifdef ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT
  if (raw == (uint32_t)AUTO_PROBE_XBOXONE) {
    return AUTO_PROBE_XBOXONE;
  }
#endif
  // Ignore stale XID/XInput staged-probe scratch values from older
  // experimental builds. Xbox One is retained only by products that include
  // the output/auth implementation.
  return AUTO_PROBE_GENERIC;
}

uint8_t auto_detect_load_transition_reason() {
  return (uint8_t)((watchdog_hw->scratch[AUTO_DETECT_SCRATCH_STATE] >> 24) & 0xFFu);
}

outputMode_t auto_detect_probe_output_mode(AutoDetectProbeStage stage) {
  switch (stage) {
    case AUTO_PROBE_PS3:
      return OUTPUT_PS3;
#ifdef ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT
    case AUTO_PROBE_XBOXONE:
      return OUTPUT_XBOXONE;
#endif
    default:
      return OUTPUT_MISTER;
  }
}

bool is_auto_output_mode_selected() {
  return configuredOutputMode == OUTPUT_AUTO;
}

bool auto_detect_has_resolved_runtime_mode() {
  return configuredOutputMode == OUTPUT_AUTO &&
         autoDetectState != AUTO_STATE_IDLE;
}

void auto_detect_persist_runtime_mode(outputMode_t mode, AutoDetectState state) {
  watchdog_hw->scratch[AUTO_DETECT_SCRATCH_MAGIC] = AUTO_DETECT_MAGIC;
  watchdog_hw->scratch[AUTO_DETECT_SCRATCH_STATE] =
    (uint32_t)sanitizeRuntimeOutputMode(mode) |
    ((uint32_t)state << kAutoDetectScratchStateShift) |
    ((uint32_t)output_autodetect_transition_reason << 24);
}

outputMode_t auto_detect_load_persisted_runtime_mode() {
  if (watchdog_hw->scratch[AUTO_DETECT_SCRATCH_MAGIC] != AUTO_DETECT_MAGIC) {
    return OUTPUT_AUTO;
  }
  return sanitizeRuntimeOutputMode(
    (outputMode_t)(watchdog_hw->scratch[AUTO_DETECT_SCRATCH_STATE] &
                   kAutoDetectScratchOutputMask));
}

AutoDetectState auto_detect_state_for_output_mode(outputMode_t mode) {
  switch (sanitizeRuntimeOutputMode(mode)) {
    case OUTPUT_PS3:
      return AUTO_STATE_PS3;
    case OUTPUT_XINPUT:
      return AUTO_STATE_XBOX_360;
    case OUTPUT_KEYBOARD:
    case OUTPUT_XINPUTW:
    case OUTPUT_XINPUT2P:
      return AUTO_STATE_WINDOWS;
    case OUTPUT_PS4:
      return AUTO_STATE_PS4;
    case OUTPUT_PS5:
      return AUTO_STATE_PS5;
    case OUTPUT_XBOXONE:
      return AUTO_STATE_XBOX_ONE;
    case OUTPUT_XID:
      return AUTO_STATE_OG_XBOX;
    case OUTPUT_SWITCH:
    case OUTPUT_SWITCHPRO:
    case OUTPUT_SWITCH2PRO:
      return AUTO_STATE_SWITCH;
    default:
      return AUTO_STATE_FALLBACK_HID;
  }
}

AutoDetectState auto_detect_load_persisted_state(outputMode_t mode) {
  if (watchdog_hw->scratch[AUTO_DETECT_SCRATCH_MAGIC] != AUTO_DETECT_MAGIC) {
    return AUTO_STATE_IDLE;
  }

  const uint8_t raw_state =
    (uint8_t)((watchdog_hw->scratch[AUTO_DETECT_SCRATCH_STATE] >>
               kAutoDetectScratchStateShift) & 0xFFu);
  if (isPersistedAutoDetectStateValid(raw_state)) {
    return (AutoDetectState)raw_state;
  }

  return auto_detect_state_for_output_mode(mode);
}

bool auto_detect_resolved_mode_is_stale() {
  if (!auto_detect_has_resolved_runtime_mode()) {
    return false;
  }
  if (autoDetectState == AUTO_STATE_WINDOWS &&
      !(outputMode == OUTPUT_REMOTE && get_configured_ps2_remote_output()) &&
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB) && defined(ENABLE_EXPERIMENTAL_CONSOLE_MOUSE)
      outputMode != OUTPUT_ROTARY_MOUSE &&
#endif
      canonicalizeOutputMode(outputMode) !=
        canonicalizeOutputMode(get_configured_windows_auto_output_mode())) {
    return true;
  }
  if (autoDetectState == AUTO_STATE_FALLBACK_HID &&
      !(outputMode == OUTPUT_REMOTE && get_configured_ps2_remote_output()) &&
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB) && defined(ENABLE_EXPERIMENTAL_CONSOLE_MOUSE)
      outputMode != OUTPUT_ROTARY_MOUSE &&
#endif
      !misterAutoOutputAcceptsRuntimeMode(outputMode)) {
    return true;
  }
  // A Switch host resolved under the other System Settings > Switch2 choice
  // is detected again instead of being carried across the reboot. A Switch 1
  // keeps Switch Pro either way, so with Switch2 Pro chosen it costs one
  // probe; without this, a changed choice stayed stale until a power cycle.
  return autoDetectState == AUTO_STATE_SWITCH &&
         (outputMode == OUTPUT_SWITCH2PRO) !=
           (get_configured_switch2_auto_output_mode() == OUTPUT_SWITCH2PRO);
}

bool auto_detect_preserve_resolved_runtime_mode_for_reboot() {
  if (!auto_detect_has_resolved_runtime_mode() ||
      auto_detect_resolved_mode_is_stale()) {
    return false;
  }

  autoDetectProbeStage = AUTO_PROBE_GENERIC;
  auto_detect_persist_debug_snapshot(output_autodetect_last_flags, output_autodetect_aux_flags);
  auto_detect_persist_runtime_mode(outputMode, autoDetectState);
  return true;
}

bool auto_detect_preserve_known_runtime_mode_for_input_reboot() {
  if (auto_detect_preserve_resolved_runtime_mode_for_reboot()) {
    return true;
  }
  if (!is_auto_output_mode_selected() ||
      autoDetectState != AUTO_STATE_IDLE ||
      !misterAutoOutputAcceptsRuntimeMode(outputMode)) {
    return false;
  }

  autoDetectProbeStage = AUTO_PROBE_GENERIC;
  auto_detect_persist_debug_snapshot(output_autodetect_last_flags, output_autodetect_aux_flags);
  auto_detect_persist_runtime_mode(outputMode, AUTO_STATE_FALLBACK_HID);
  return true;
}

__attribute__((weak)) bool auto_detect_preserve_current_runtime_mode_for_input_reboot() {
  return auto_detect_preserve_resolved_runtime_mode_for_reboot();
}

outputMode_t get_effective_output_mode() {
#if ADAPT_POLICY_RESET_HOLD_AUTO_OUTPUT && defined(ENABLE_INPUT_AUTODETECT)
  // Home recovery keeps the enumerated personality until LEFT is released.
  if (inputAutoDetectRebootPending()) return canonicalizeOutputMode(outputMode);
#endif
  outputMode_t mode;
  if (configuredOutputMode == OUTPUT_AUTO && autoDetectState == AUTO_STATE_IDLE) {
    mode = canonicalizeOutputMode(auto_detect_probe_output_mode(autoDetectProbeStage));
  } else {
    mode = canonicalizeOutputMode(outputMode);
  }
  if (mode == OUTPUT_SWITCHPRO &&
      switch_device_selection_uses_gc_adapter(deviceMode, nso_special)) {
    return OUTPUT_GCWIIU;
  }
  return mode;
}

bool ps5_diag_boot_arm_is_present() {
#if ADAPT_POLICY_PS5_DIAGNOSTIC_RUNTIME
  return watchdog_hw->scratch[AUTO_PROBE_SCRATCH_AUX] == kPs5DiagBootArmMagic;
#else
  return false;
#endif
}

void ps5_diag_arm_next_boot() {
#if ADAPT_POLICY_PS5_DIAGNOSTIC_RUNTIME
  watchdog_hw->scratch[AUTO_PROBE_SCRATCH_AUX] = kPs5DiagBootArmMagic;
#endif
}

void ps5_diag_clear_boot_arm() {
#if ADAPT_POLICY_PS5_DIAGNOSTIC_RUNTIME
  if (watchdog_hw->scratch[AUTO_PROBE_SCRATCH_AUX] == kPs5DiagBootArmMagic) {
    watchdog_hw->scratch[AUTO_PROBE_SCRATCH_AUX] = 0;
  }
#endif
}
