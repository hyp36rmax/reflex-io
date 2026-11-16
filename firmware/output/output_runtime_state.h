#pragma once

#include <Arduino.h>
#include "../firmware_platform_config.h"
#include "../core/controller_settings_state.h"
#include "../core/device_runtime_state.h"
#include "output_mode.h"

#if defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED)
  #include "hardware/watchdog.h"
#endif

extern outputMode_t configuredOutputMode;
extern outputMode_t outputMode;
extern socdMode_t socdMode;
extern bool use_device_hid_class;
extern uint8_t hid_begin_success_count;
extern uint8_t hid_first_fail_index;

// Inputs that offer an optional native Nintendo Switch Online identity.
bool input_mode_supports_nso_special(DeviceEnum mode);
uint8_t max_switch_device_selection_for_input_mode(DeviceEnum mode);
uint8_t sanitize_switch_device_selection(DeviceEnum mode, uint8_t selection);
const char* switch_device_selection_label(DeviceEnum mode, uint8_t selection);
bool switch_device_selection_is_nso_for_input_mode(DeviceEnum mode,
                                                   uint8_t selection);
bool switch_device_selection_uses_gc_adapter(DeviceEnum mode,
                                             uint8_t selection);
bool output_mode_supports_switch_device_selection(outputMode_t mode);

// Inputs that need pre-USB resolution while Switch output is being restored.
bool input_mode_has_automatic_nso_profile(DeviceEnum mode);

// Native N64 NSO exposes one USB controller. It is effective only while one
// physical N64 controller is connected; other manually selected NSO profiles
// retain their explicit one-player behavior.
bool input_mode_allows_nso_special_for_connected_players(
    DeviceEnum mode, uint8_t connectedPlayers);
void latch_nso_special_connected_players(DeviceEnum mode,
                                         uint8_t connectedPlayers);
// Save/restore the latch across a detection that is applied and then
// rejected, so a discarded input mode cannot leave the NSO identity behind.
bool nso_special_connected_player_latch();
void restore_nso_special_connected_player_latch(bool allowed);
bool nso_special_effective_for_input_mode(DeviceEnum mode);

// True only when a supported input has opted into its NSO identity.
bool is_nso_special_active();

outputMode_t canonicalizeOutputMode(outputMode_t mode);
// Apply build-capability policy without resolving OUTPUT_AUTO. Unsupported
// native personalities are mapped to their compatible advertised mode here.
outputMode_t sanitizeConfiguredOutputMode(outputMode_t mode);
bool is_xinput2p_output_enabled();

outputMode_t sanitizeRuntimeOutputMode(outputMode_t mode);
outputMode_t get_configured_windows_auto_output_mode();
// False for a PC or MiSTer preference that falls back to another output here.
bool host_output_preference_available(uint8_t preference);
outputMode_t get_configured_mister_auto_output_mode();
// Persisted Classic2USB preference: false keeps the normal gamepad report.
bool get_configured_ps2_remote_output();
// System Settings > Switch 2: Switch Pro (default) or Switch 2 Pro.
outputMode_t get_configured_switch2_auto_output_mode();

// Z-button output policy:
// - N64 mode 0 combines Z into L1; mode 1 keeps Z on L2.
// - GameCube mode 0 maps Z to R1/RB; mode 1 keeps Z on Select/Back.
// Check if output mode supports D-Pad as buttons (only HID and MiSTer have flexible button counts)
bool output_supports_dpad_buttons(outputMode_t mode);

// Native PS5 is timing-sensitive. Keep nonessential firmware
// services quiet while this output is active so USB auth/report timing stays
// as close to the donor path as possible.
bool is_ps5_timing_quiet_mode_active();

// Clear all OUTPUT_AUTO scratch state so manual output modes cannot be
// overridden by stale auto-detect state.
void auto_detect_clear_scratch_state();

// Clear only chained AUTO input scratch while leaving any preserved AUTO output
// result intact for an input-mode reboot.
void auto_detect_clear_input_scratch_state();

enum AutoDetectProbeStage : uint8_t {
  AUTO_PROBE_GENERIC = 0,
  AUTO_PROBE_PS3,
  AUTO_PROBE_PS3_AFTER_PS4,
  AUTO_PROBE_PS3_AFTER_WINDOWS,
  AUTO_PROBE_XID,
  AUTO_PROBE_XINPUT,
  AUTO_PROBE_XBOXONE,
};

extern AutoDetectProbeStage autoDetectProbeStage;
extern uint8_t output_autodetect_last_flags;
extern uint8_t output_autodetect_aux_flags;
extern uint8_t output_autodetect_usb_hw_flags;
extern uint8_t output_autodetect_usb_irq_observed;
extern uint8_t output_autodetect_usb_irq_flags;
extern uint16_t output_autodetect_probe_counters;
extern uint16_t output_autodetect_last_ms;
extern uint8_t output_autodetect_transition_reason;

enum AutoDetectTransitionReason : uint8_t {
  AUTO_TRANSITION_NONE = 0,
  AUTO_TRANSITION_GENERIC_NO_MOUNT_TO_FALLBACK = 1,
  AUTO_TRANSITION_GENERIC_NO_CONNECT_TO_FALLBACK = 2,
  // Deprecated aliases kept so old diagnostics/scripts remain readable while
  // AUTO returns to RetroZord's single-pass generic HID fallback model.
  AUTO_TRANSITION_GENERIC_NO_MOUNT_TO_PS3 = AUTO_TRANSITION_GENERIC_NO_MOUNT_TO_FALLBACK,
  AUTO_TRANSITION_GENERIC_NO_CONNECT_TO_XID = AUTO_TRANSITION_GENERIC_NO_CONNECT_TO_FALLBACK,
  AUTO_TRANSITION_DIAG_PULLUP_PULSE = 3,
  AUTO_TRANSITION_PLAYSTATION_NO_PS4_AUTH_TO_PS5 = 4,
  AUTO_TRANSITION_PS4_EXTENDED_TO_PS5 =
      AUTO_TRANSITION_PLAYSTATION_NO_PS4_AUTH_TO_PS5,
  AUTO_TRANSITION_PS5_PROBE_TO_PS4 = 5,
  AUTO_TRANSITION_GENERIC_PS3_ASSIST = 6,
  AUTO_TRANSITION_PS3_ASSIST_TO_XBOXONE = 7,
  AUTO_TRANSITION_XBOXONE_PROBE_CONFIRMED = 8,
  AUTO_TRANSITION_XBOXONE_PROBE_TO_FALLBACK = 9,
  AUTO_TRANSITION_GENERIC_XBOXONE_FINGERPRINT_TO_PROBE = 10,
  AUTO_TRANSITION_GENERIC_XBOXONE_FINGERPRINT_NO_DONOR = 11,
  // A WCID host configured the Xbox persona but sent no console auth.
  AUTO_TRANSITION_XBOXONE_PROBE_TO_WINDOWS = 12,
  // These two decide, not only report: a no-connect fallback re-arms the
  // probe once when a host first sends SETUP, and a no-connect after that
  // re-arm is final.
  AUTO_TRANSITION_GENERIC_REARM_AFTER_NO_CONNECT = 13,
  AUTO_TRANSITION_GENERIC_NO_CONNECT_AFTER_REARM = 14,
};

void auto_detect_persist_probe_stage(AutoDetectProbeStage stage);

void auto_detect_persist_debug_snapshot(uint8_t flags, uint8_t aux_flags);

void auto_detect_restore_debug_snapshot();

AutoDetectProbeStage auto_detect_load_probe_stage();

uint8_t auto_detect_load_transition_reason();

outputMode_t auto_detect_probe_output_mode(AutoDetectProbeStage stage);

enum AutoDetectState : uint8_t {
  AUTO_STATE_IDLE = 0,
  AUTO_STATE_PS3,
  AUTO_STATE_WINDOWS,
  AUTO_STATE_PS4,
  AUTO_STATE_OG_XBOX,
  AUTO_STATE_XBOX_360,
  AUTO_STATE_SWITCH,
  AUTO_STATE_FALLBACK_HID,
  AUTO_STATE_PS5,
  AUTO_STATE_PLAYSTATION,
  AUTO_STATE_XBOX_ONE,
};

extern AutoDetectState autoDetectState;

#define OUTPUT_AUTODETECT_TIMING_VALID 0x01

bool is_auto_output_mode_selected();
bool auto_detect_no_data_host_fallback_active();
// Auto fell back to the MiSTer/generic persona after the host talked to the
// probe and showed no PlayStation sign. A no-traffic fallback may be a
// console that wakes later, so it does not count.
bool auto_detect_fallback_saw_generic_host();

bool auto_detect_has_resolved_runtime_mode();
// True when Auto is running a persona the saved PC Mode, MiSTer Mode or
// Switch2 choice no longer selects, so USB has to enumerate again.
bool auto_detect_resolved_mode_is_stale();

void auto_detect_persist_runtime_mode(outputMode_t mode, AutoDetectState state);

outputMode_t auto_detect_load_persisted_runtime_mode();

AutoDetectState auto_detect_load_persisted_state(outputMode_t mode);

AutoDetectState auto_detect_state_for_output_mode(outputMode_t mode);

bool auto_detect_preserve_resolved_runtime_mode_for_reboot();
bool auto_detect_preserve_current_runtime_mode_for_input_reboot();
bool auto_detect_preserve_known_runtime_mode_for_input_reboot();

outputMode_t get_effective_output_mode();

bool ps5_diag_boot_arm_is_present();
void ps5_diag_arm_next_boot();
void ps5_diag_clear_boot_arm();

#ifdef __cplusplus
extern "C" {
#endif
uint8_t get_max_output_for_mode(outputMode_t mode);
const char* get_mode_name(outputMode_t mode);
#ifdef __cplusplus
}
#endif
