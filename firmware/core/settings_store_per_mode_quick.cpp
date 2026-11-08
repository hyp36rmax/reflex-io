#include "analog_calibration_state.h"
#include "directional_hotkeys.h"
#include "dpad_mode.h"
#include "button_remap.h"
#include "classic_analog_range.h"
#include "negcon_input_modes.h"
#include "nes_device_mode.h"
#include "psx_poll_timing.h"
#include "rotary_settings.h"
#include "rotary_profile.h"
#include "controller_settings_state.h"
#include "device_runtime_state.h"
#include "eeprom_helper.h"
#include "settings_layout.h"
#include "settings_persistence_deferred.h"
#include "settings_store_per_mode_internal.h"
#include "../menu/menu_mode_state.h"
#include "../menu/menu_runtime_state.h"
#include "../menu/menu_working_state.h"
#include "../output/output_runtime_state.h"
#include "../platform/rgb_led.h"
#include "turbo.h"

#include <string.h>

namespace {

constexpr uint8_t kLegacyPerModeTurboSize = 14;
constexpr uint8_t kLegacyPerModeRemapSize = 16;

struct LegacyPerModeSettingsRecord {
  uint8_t deadzone_percent;
  uint8_t socd;
  uint8_t dpad_mode;
  uint8_t stick_invert;
  uint8_t rumble_level;
  uint8_t trigger_mode;
  uint8_t spinner_speed;
  uint8_t button_map_mode;
  uint8_t n64_z_mode;
  uint8_t n64_cstick_mode;
  uint8_t wii_analog_range;
  uint8_t n64_analog_range;
  uint8_t nso_special;
  uint8_t powerpad_mode;
  int8_t guncon_offset_x;
  int8_t guncon_offset_y;
  uint8_t reserved_musical_buttons;
  uint8_t reserved_mouse_analog;
  uint8_t reserved_mouse_sensitivity;
  uint8_t reserved_mouse_stick;
  uint8_t jogcon_mode;
  uint8_t jogcon_force;
  uint8_t wheel_sensitivity;
  uint8_t jogcon_digital_map;
  uint8_t jogcon_wheel_axis;
  int8_t analog_cal_lx_min;
  int8_t analog_cal_lx_max;
  int8_t analog_cal_ly_min;
  int8_t analog_cal_ly_max;
  int8_t analog_cal_rx_min;
  int8_t analog_cal_rx_max;
  int8_t analog_cal_ry_min;
  int8_t analog_cal_ry_max;
  uint8_t analog_cal_enabled;
  uint8_t turbo_rates[kLegacyPerModeTurboSize];
  uint8_t remaps[kLegacyPerModeRemapSize];
};

static_assert(sizeof(LegacyPerModeSettingsRecord) == 64, "Legacy per-mode settings size changed unexpectedly");
static_assert(offsetof(LegacyPerModeSettingsRecord, turbo_rates) ==
              offsetof(PerModeSettingsRecord, turbo_rates),
              "Legacy per-mode settings prefix must match current record");

constexpr uint16_t kLegacyPerModeSettingsRecordSize =
  PERSISTED_BLOCK_HEADER_SIZE + sizeof(LegacyPerModeSettingsRecord);
static_assert(PERSISTED_AB_SLOT_COUNT * kLegacyPerModeSettingsRecordSize >=
                  SETTINGS_SPARE_RECORD_REGION_SIZE,
              "spare record region overlaps a legacy per-mode record");

uint16_t legacyPerModeSettingsRecordBase(DeviceEnum mode) {
  return SETTINGS_PER_MODE_RECORD_BASE +
         ((uint16_t)persistedInputModeId(mode) * PERSISTED_AB_SLOT_COUNT *
          kLegacyPerModeSettingsRecordSize);
}

PerModeSettingsRecord buildDefaultPerModeSettings(DeviceEnum mode) {
  PerModeSettingsRecord settings = {};
  settings.deadzone_percent = 0;
  settings.socd = ADAPT_POLICY_SOCD_DEFAULT;
  settings.dpad_mode = 0;
  settings.stick_invert = 0;
  settings.rumble_level = defaultRumbleLevelForInputMode(mode);
  settings.trigger_mode = defaultTriggerModeForInputMode(mode);
  settings.spinner_speed = defaultSpinnerSpeedForInputMode(mode);
  settings.button_map_mode = defaultButtonMapModeForInputMode(mode);
  settings.n64_z_mode = defaultZButtonModeForInputMode(mode);
  settings.n64_cstick_mode = defaultN64CStickModeForInputMode(mode);
  settings.wii_analog_range = CLASSIC_ANALOG_RANGE_DEFAULT;
  settings.n64_analog_range = CLASSIC_ANALOG_RANGE_DEFAULT;
  settings.nso_special = defaultNsoSpecialForInputMode(mode);
  settings.powerpad_mode = 0;
  settings.guncon_offset_x = 0;
  settings.guncon_offset_y = 0;
  settings.reserved_musical_buttons = 0;
  settings.gamecube_l_switch_mode = GAMECUBE_L_SWITCH_ZL;
  settings.reserved_mouse_sensitivity = 5;
  settings.reserved_mouse_stick = 0;
  settings.jogcon_mode = 0;
  settings.jogcon_force = 15;
  settings.wheel_sensitivity = 1;
  settings.jogcon_digital_map = 0;
  settings.jogcon_wheel_axis = 0;
  settings.analog_cal_lx_min = -128;
  settings.analog_cal_lx_max = 127;
  settings.analog_cal_ly_min = -128;
  settings.analog_cal_ly_max = 127;
  settings.analog_cal_rx_min = -128;
  settings.analog_cal_rx_max = 127;
  settings.analog_cal_ry_min = -128;
  settings.analog_cal_ry_max = 127;
  settings.analog_cal_enabled = 0;
  for (uint8_t i = 0; i < PER_MODE_TURBO_SIZE; ++i) {
    settings.turbo_rates[i] = TURBO_OFF;
  }
  for (uint8_t i = 0; i < PER_MODE_REMAP_SIZE; ++i) {
    settings.remaps[i] = i;
  }
  sanitizePerModeSettings(mode, settings);
  return settings;
}

bool readLegacyPerModeSettings(DeviceEnum mode, PerModeSettingsRecord& settings) {
  LegacyPerModeSettingsRecord legacy{};
  uint8_t slot = 0;
  uint16_t generation = 0;
  if (!readLatestPersistedRecordAB(
        legacyPerModeSettingsRecordBase(mode),
        kLegacyPerModeSettingsRecordSize,
        PERSISTED_BLOCK_MAGIC_PER_MODE,
        slot,
        generation,
        legacy)) {
    return false;
  }

  settings = buildDefaultPerModeSettings(mode);
  memcpy(&settings, &legacy, offsetof(PerModeSettingsRecord, turbo_rates));
#if ADAPT_POLICY_QUICK_RETAIN_RETRO_FIELDS
  // This byte was reserved in legacy records and may contain stale data.
  settings.gamecube_l_switch_mode = GAMECUBE_L_SWITCH_ZL;
#endif
  for (uint8_t i = 0; i < kLegacyPerModeTurboSize && i < PER_MODE_TURBO_SIZE; ++i) {
    settings.turbo_rates[i] = legacy.turbo_rates[i];
  }
  for (uint8_t i = 0; i < kLegacyPerModeRemapSize && i < PER_MODE_REMAP_SIZE; ++i) {
    settings.remaps[i] = legacy.remaps[i];
  }
  sanitizePerModeSettings(mode, settings);
  return true;
}

}  // namespace

void migrateLegacyPerModeSettings() {
  // A current record is larger than a legacy one, so writing it lands on the
  // legacy records of higher modes. Going from the highest mode down moves each
  // one before anything can overwrite it.
  EepromTransaction transaction;
  for (uint8_t id = PERSISTED_LAYOUT_INPUT_MODES; id-- > 0;) {
    const DeviceEnum mode = deviceModeFromPersistedInputModeId(id);
    if (!isConcreteModeForPerSettings(mode)) continue;
    const uint16_t base = getPerModeSettingsRecordBase(mode);
    PerModeSettingsRecord settings{};
    uint8_t slot = 0;
    uint16_t generation = 0;
    if (readLatestPersistedRecordAB(base, PER_MODE_SETTINGS_RECORD_SIZE,
                                    PERSISTED_BLOCK_MAGIC_PER_MODE, slot, generation,
                                    settings) ||
        !readLegacyPerModeSettings(mode, settings)) {
      continue;
    }
    stagePersistedRecordAB(transaction, base, PER_MODE_SETTINGS_RECORD_SIZE,
                           PERSISTED_BLOCK_MAGIC_PER_MODE, settings);
  }
  (void)transaction.commit();
}

bool readPerModeSettings(DeviceEnum mode, PerModeSettingsRecord& settings) {
  settings = buildDefaultPerModeSettings(mode);
  if (!isConcreteModeForPerSettings(mode)) {
    return false;
  }
  if (settingsPersistenceReadPendingPerMode(mode, settings)) {
    sanitizePerModeSettings(mode, settings);
    return true;
  }

  uint8_t slot = 0;
  uint16_t generation = 0;
  if (!readLatestPersistedRecordAB(
        getPerModeSettingsRecordBase(mode),
        PER_MODE_SETTINGS_RECORD_SIZE,
        PERSISTED_BLOCK_MAGIC_PER_MODE,
        slot,
        generation,
        settings)) {
    if (readLegacyPerModeSettings(mode, settings)) {
      return true;
    }
    settings = buildDefaultPerModeSettings(mode);
    return false;
  }

  sanitizePerModeSettings(mode, settings);
  return true;
}

void writePerModeSettings(DeviceEnum mode, const PerModeSettingsRecord& settings) {
  if (!isConcreteModeForPerSettings(mode)) {
    return;
  }
  PerModeSettingsRecord sanitized = settings;
  sanitizePerModeSettings(mode, sanitized);
  if (settingsPersistenceShouldStageWrites()) {
    settingsPersistenceStagePerMode(mode, sanitized);
    return;
  }
  writePersistedRecordAB(
    getPerModeSettingsRecordBase(mode),
    PER_MODE_SETTINGS_RECORD_SIZE,
    PERSISTED_BLOCK_MAGIC_PER_MODE,
    sanitized
  );
}

bool writePerModeSettingsDurable(
    DeviceEnum mode, const PerModeSettingsRecord& settings) {
  if (!isConcreteModeForPerSettings(mode) ||
      settingsPersistenceProtectedSessionActive()) {
    return false;
  }
  // A prior failed commit leaves a dirty EEPROM RAM image. Publish that image
  // (and any deferred records that are now safe) before comparing this write;
  // otherwise an unchanged RAM mirror could masquerade as durable storage.
  if (!settingsPersistenceFlushNow()) {
    return false;
  }
  PerModeSettingsRecord sanitized = settings;
  sanitizePerModeSettings(mode, sanitized);
  return ensurePersistedRecordAB(
      getPerModeSettingsRecordBase(mode),
      PER_MODE_SETTINGS_RECORD_SIZE,
      PERSISTED_BLOCK_MAGIC_PER_MODE,
      sanitized);
}

namespace {
bool transientDancePadOverrideEngaged = false;
uint8_t transientDancePadSavedDpadMode = 0;
uint8_t transientDancePadSavedSocdMode = 0;
}  // namespace

void setTransientDancePadOverride(bool active) {
  if (active == transientDancePadOverrideEngaged) {
    return;
  }
  if (active) {
    transientDancePadSavedDpadMode = dpad_mode;
    transientDancePadSavedSocdMode = (uint8_t)socdMode;
    dpad_mode = DPAD_MODE_BUTTONS;
    socdMode = SOCD_OFF;
    transientDancePadOverrideEngaged = true;
    return;
  }
  // Restore only values the user did not change while the override was on.
  if (dpad_mode == DPAD_MODE_BUTTONS) {
    dpad_mode = transientDancePadSavedDpadMode;
  }
  if (socdMode == SOCD_OFF) {
    socdMode = (socdMode_t)transientDancePadSavedSocdMode;
  }
  transientDancePadOverrideEngaged = false;
}

void forgetTransientDancePadOverride() {
  transientDancePadOverrideEngaged = false;
}

#if ADAPT_POLICY_DIRECT_BOARD_CONTROLS
namespace {
bool transientDpadEngaged = false;
uint8_t transientDpadSavedMode = 0;
uint8_t transientDpadMode = 0;
}  // namespace

void setTransientDpadMode(bool active, uint8_t mode) {
  if (active) {
    // A different value here is a setting written while engaged: it is the
    // user's, so it is what a release restores.
    if (!transientDpadEngaged || dpad_mode != transientDpadMode) {
      transientDpadSavedMode = dpad_mode;
    }
    transientDpadEngaged = true;
    transientDpadMode = mode;
    dpad_mode = mode;
    return;
  }
  if (transientDpadEngaged && dpad_mode == transientDpadMode) {
    dpad_mode = transientDpadSavedMode;
  }
  transientDpadEngaged = false;
}
#endif

uint8_t persistedRuntimeDpadMode() {
#if ADAPT_POLICY_DIRECT_BOARD_CONTROLS
  if (transientDpadEngaged && dpad_mode == transientDpadMode) {
    return transientDpadSavedMode;
  }
#endif
  if (transientDancePadOverrideEngaged && dpad_mode == DPAD_MODE_BUTTONS) {
    return transientDancePadSavedDpadMode;
  }
  return dpad_mode;
}

uint8_t persistedRuntimeSocdMode() {
  if (transientDancePadOverrideEngaged && socdMode == SOCD_OFF) {
    return transientDancePadSavedSocdMode;
  }
  return (uint8_t)socdMode;
}

void applyPerModeSettings(const PerModeSettingsRecord& settings) {
  // Reloaded settings replace any transient override; an active dance pad
  // re-engages it on its next poll with these values as the saved ones.
  forgetTransientDancePadOverride();
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB) || defined(ADAPT_BRIDGE_JVS_TO_USB)
  direction_hotkey::read(settings, active_direction_hotkeys);
#endif
  deadzone_percent = settings.deadzone_percent;
  menu_deadzone_percent = settings.deadzone_percent;
  socdMode = (socdMode_t)settings.socd;
  menu_socdMode = (socdMode_t)settings.socd;
  dpad_mode = settings.dpad_mode;
  menu_dpad_mode = settings.dpad_mode;
  stick_invert = settings.stick_invert;
  menu_stick_invert = settings.stick_invert;
  rumble_level = settings.rumble_level;
  menu_rumble_level = settings.rumble_level;
  trigger_mode = settings.trigger_mode;
  menu_trigger_mode = settings.trigger_mode;
  spinner_speed = settings.spinner_speed;
  button_map_mode = settings.button_map_mode;
  menu_button_map = settings.button_map_mode;
#if ADAPT_POLICY_QUICK_RETAIN_RETRO_FIELDS
  gamecube_l_switch_mode = settings.gamecube_l_switch_mode;
  menu_gamecube_l_switch_mode = settings.gamecube_l_switch_mode;
#endif
#if !ADAPT_POLICY_RESERVED_FEEDBACK_STORAGE
  // Outside the PSX record this byte is 0 or a GameCube 1; both read as Auto.
  psx_poll_timing = psx_poll::sanitize(settings.psx_poll_timing);
#endif
  n64_z_mode = settings.n64_z_mode;
  menu_n64_z_mode = settings.n64_z_mode;
  n64_cstick_mode = settings.n64_cstick_mode;
  menu_n64_cstick_mode = settings.n64_cstick_mode;
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  wii_analog_range = rotaryAnalogRange(settings.wii_analog_range);
  menu_wii_analog_range = wii_analog_range;
  paddle_divisor = rotaryDivisor(settings.wii_analog_range);
  rotary_reverse = rotaryReverse(settings.wii_analog_range);
  driving_wrap = rotaryWrap(settings.wii_analog_range);
  rotary_profile = rotaryProfileFromJogconDigitalMap(settings.jogcon_digital_map);
  jogcon_rotary_profile = rotaryProfileFromWheelSensitivity(settings.wheel_sensitivity);
#else
  wii_analog_range = settings.wii_analog_range;
  menu_wii_analog_range = settings.wii_analog_range;
  paddle_divisor = 1;
  rotary_reverse = false;
  driving_wrap = false;
  rotary_profile = 0;
  jogcon_rotary_profile = 0;
#endif
  n64_analog_range = settings.n64_analog_range;
  menu_n64_analog_range = settings.n64_analog_range;
  nso_special = settings.nso_special;
  menu_nso_special = nso_special;
  // NES values are untagged, so outside the PSX record this reads as Analog.
  negcon_digital_inputs = negconDigitalInputsFromRaw(settings.psx_negcon_inputs);
  negcon_gain = negconGainFromRaw(settings.psx_negcon_inputs);
  menu_powerpad_mode = sanitizeNesDeviceMode(settings.powerpad_mode);
  menu_guncon_offset_x = settings.guncon_offset_x;
  menu_guncon_offset_y = settings.guncon_offset_y;
#if ADAPT_POLICY_QUICK_RETAIN_RETRO_FIELDS
  menu_jogcon_mode = settings.jogcon_mode;
#endif
  menu_jogcon_force = settings.jogcon_force;
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  menu_wheel_sensitivity = rotaryWheelSensitivity(settings.wheel_sensitivity);
  menu_jogcon_digital_map = rotaryLegacyLowBits(settings.jogcon_digital_map);
#else
  menu_wheel_sensitivity = settings.wheel_sensitivity;
  menu_jogcon_digital_map = settings.jogcon_digital_map;
#endif
  menu_jogcon_wheel_axis = settings.jogcon_wheel_axis;

  analogCalibration.lx.raw_min = settings.analog_cal_lx_min;
  analogCalibration.lx.raw_max = settings.analog_cal_lx_max;
  analogCalibration.ly.raw_min = settings.analog_cal_ly_min;
  analogCalibration.ly.raw_max = settings.analog_cal_ly_max;
  analogCalibration.rx.raw_min = settings.analog_cal_rx_min;
  analogCalibration.rx.raw_max = settings.analog_cal_rx_max;
  analogCalibration.ry.raw_min = settings.analog_cal_ry_min;
  analogCalibration.ry.raw_max = settings.analog_cal_ry_max;
  analogCalibration.enabled = (settings.analog_cal_enabled != 0);

}

void captureRuntimePerModeSettings(DeviceEnum mode, PerModeSettingsRecord& settings) {
  if (!readPerModeSettings(mode, settings)) {
    settings = buildDefaultPerModeSettings(mode);
  }

  settings.deadzone_percent = deadzone_percent;
  settings.socd = persistedRuntimeSocdMode();
  settings.dpad_mode = persistedRuntimeDpadMode();
  settings.stick_invert = stick_invert;
  settings.rumble_level = rumble_level;
  settings.trigger_mode = trigger_mode;
  settings.spinner_speed = spinner_speed;
  settings.button_map_mode = button_map_mode;
  // Only the Quick Menu changes the PSX poll timing, and it saves the record.
  const uint8_t storedPsxPollTiming = settings.psx_poll_timing;
#if !ADAPT_POLICY_RESERVED_FEEDBACK_STORAGE
  settings.gamecube_l_switch_mode = gamecube_l_switch_mode;
#endif
  if (psx_poll::storedInRecord(mode)) settings.psx_poll_timing = storedPsxPollTiming;
  settings.n64_z_mode = n64_z_mode;
  settings.n64_cstick_mode = n64_cstick_mode;
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  settings.wii_analog_range = encodeRotaryAnalogRange(settings.wii_analog_range, wii_analog_range);
  settings.wii_analog_range = encodeRotaryDivisor(settings.wii_analog_range, paddle_divisor);
  settings.wii_analog_range = encodeRotaryReverse(settings.wii_analog_range, rotary_reverse);
  settings.wii_analog_range = encodeRotaryWrap(settings.wii_analog_range, driving_wrap);
#else
  settings.wii_analog_range = wii_analog_range;
#endif
  settings.n64_analog_range = n64_analog_range;
  settings.nso_special = nso_special;
  // Only the Quick Menu and the NEGCON serial command change the PSX byte.
  if (!negconInputsStoredInRecord(mode)) settings.powerpad_mode = menu_powerpad_mode;
  settings.guncon_offset_x = menu_guncon_offset_x;
  settings.guncon_offset_y = menu_guncon_offset_y;
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB) || defined(ADAPT_BRIDGE_JVS_TO_USB)
#if !ADAPT_POLICY_FIXED_HOTKEY_ASSIGNMENTS
  direction_hotkey::write(settings, active_direction_hotkeys);
#endif
#elif !ADAPT_POLICY_RESERVED_FEEDBACK_STORAGE
  settings.reserved_musical_buttons = 0;
#endif
#if !ADAPT_POLICY_RESERVED_FEEDBACK_STORAGE
  settings.jogcon_mode = menu_jogcon_mode;
#endif
  settings.jogcon_force = menu_jogcon_force;
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  settings.wheel_sensitivity = withRotaryProfileInWheelSensitivity(
    menu_wheel_sensitivity, jogcon_rotary_profile);
  settings.jogcon_digital_map = withRotaryProfileInJogconDigitalMap(
    menu_jogcon_digital_map, rotary_profile);
#else
  settings.wheel_sensitivity = menu_wheel_sensitivity;
  settings.jogcon_digital_map = menu_jogcon_digital_map;
#endif
  settings.jogcon_wheel_axis = menu_jogcon_wheel_axis;
  settings.analog_cal_lx_min = analogCalibration.lx.raw_min;
  settings.analog_cal_lx_max = analogCalibration.lx.raw_max;
  settings.analog_cal_ly_min = analogCalibration.ly.raw_min;
  settings.analog_cal_ly_max = analogCalibration.ly.raw_max;
  settings.analog_cal_rx_min = analogCalibration.rx.raw_min;
  settings.analog_cal_rx_max = analogCalibration.rx.raw_max;
  settings.analog_cal_ry_min = analogCalibration.ry.raw_min;
  settings.analog_cal_ry_max = analogCalibration.ry.raw_max;
  settings.analog_cal_enabled = analogCalibration.enabled ? 1 : 0;
  for (uint8_t i = 0; i < TURBO_BTN_COUNT; ++i) {
    settings.turbo_rates[i] = (uint8_t)turbo.getButtonRate((TurboButton)i);
  }
  // Remaps are stored per input+output pair; the record keeps its legacy copy.
  sanitizePerModeSettings(mode, settings);
}

void captureMenuPerModeSettings(DeviceEnum mode, PerModeSettingsRecord& settings) {
  if (!readPerModeSettings(mode, settings)) {
    settings = buildDefaultPerModeSettings(mode);
  }

  settings.deadzone_percent = menu_deadzone_percent;
  settings.socd = (uint8_t)menu_socdMode;
  settings.dpad_mode = menu_dpad_mode;
  settings.stick_invert = menu_stick_invert;
  settings.rumble_level = menu_rumble_level;
  settings.trigger_mode = menu_trigger_mode;
  settings.spinner_speed = spinner_speed;
  settings.button_map_mode = menu_button_map;
  // Only the Quick Menu changes the PSX poll timing, and it saves the record.
  const uint8_t storedPsxPollTiming = settings.psx_poll_timing;
#if !ADAPT_POLICY_RESERVED_FEEDBACK_STORAGE
  settings.gamecube_l_switch_mode = menu_gamecube_l_switch_mode;
#endif
  if (psx_poll::storedInRecord(mode)) settings.psx_poll_timing = storedPsxPollTiming;
  settings.n64_z_mode = menu_n64_z_mode;
  settings.n64_cstick_mode = menu_n64_cstick_mode;
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  settings.wii_analog_range = encodeRotaryAnalogRange(settings.wii_analog_range, menu_wii_analog_range);
  settings.wii_analog_range = encodeRotaryDivisor(settings.wii_analog_range, paddle_divisor);
  settings.wii_analog_range = encodeRotaryReverse(settings.wii_analog_range, rotary_reverse);
  settings.wii_analog_range = encodeRotaryWrap(settings.wii_analog_range, driving_wrap);
#else
  settings.wii_analog_range = menu_wii_analog_range;
#endif
  settings.n64_analog_range = menu_n64_analog_range;
  settings.nso_special = menu_nso_special;
  // Only the Quick Menu and the NEGCON serial command change the PSX byte.
  if (!negconInputsStoredInRecord(mode)) settings.powerpad_mode = menu_powerpad_mode;
  settings.guncon_offset_x = menu_guncon_offset_x;
  settings.guncon_offset_y = menu_guncon_offset_y;
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB) || defined(ADAPT_BRIDGE_JVS_TO_USB)
#if !ADAPT_POLICY_FIXED_HOTKEY_ASSIGNMENTS
  direction_hotkey::write(settings, active_direction_hotkeys);
#endif
#elif !ADAPT_POLICY_RESERVED_FEEDBACK_STORAGE
  settings.reserved_musical_buttons = 0;
#endif
#if !ADAPT_POLICY_RESERVED_FEEDBACK_STORAGE
  settings.jogcon_mode = menu_jogcon_mode;
#endif
  settings.jogcon_force = menu_jogcon_force;
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  settings.wheel_sensitivity = withRotaryProfileInWheelSensitivity(
    menu_wheel_sensitivity, jogcon_rotary_profile);
  settings.jogcon_digital_map = withRotaryProfileInJogconDigitalMap(
    menu_jogcon_digital_map, rotary_profile);
#else
  settings.wheel_sensitivity = menu_wheel_sensitivity;
  settings.jogcon_digital_map = menu_jogcon_digital_map;
#endif
  settings.jogcon_wheel_axis = menu_jogcon_wheel_axis;
  settings.analog_cal_lx_min = analogCalibration.lx.raw_min;
  settings.analog_cal_lx_max = analogCalibration.lx.raw_max;
  settings.analog_cal_ly_min = analogCalibration.ly.raw_min;
  settings.analog_cal_ly_max = analogCalibration.ly.raw_max;
  settings.analog_cal_rx_min = analogCalibration.rx.raw_min;
  settings.analog_cal_rx_max = analogCalibration.rx.raw_max;
  settings.analog_cal_ry_min = analogCalibration.ry.raw_min;
  settings.analog_cal_ry_max = analogCalibration.ry.raw_max;
  settings.analog_cal_enabled = analogCalibration.enabled ? 1 : 0;
  sanitizePerModeSettings(mode, settings);
}

void savePerModeSettingsFromRuntime(DeviceEnum mode) {
  if (!isConcreteModeForPerSettings(mode)) {
    return;
  }
  PerModeSettingsRecord settings{};
  captureRuntimePerModeSettings(mode, settings);
  writePerModeSettings(mode, settings);
}

void savePerModeSettingsFromMenu(DeviceEnum mode) {
  if (!isConcreteModeForPerSettings(mode)) {
    return;
  }
  PerModeSettingsRecord settings{};
  captureMenuPerModeSettings(mode, settings);
  writePerModeSettings(mode, settings);
}

bool loadAndApplyPerModeSettingsForMode(DeviceEnum mode) {
  if (!isConcreteModeForPerSettings(mode)) {
    return false;
  }

  PerModeSettingsRecord settings{};
  const bool found = readPerModeSettings(mode, settings);
  if (!found) {
    writePerModeSettings(mode, settings);
  }
  applyPerModeSettings(settings);
  return !found;
}
