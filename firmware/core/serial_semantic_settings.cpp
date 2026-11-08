#include "../product_config.h"

#include "serial_semantic_settings.h"

#include "controller_settings_state.h"
#include "device_runtime_state.h"
#include "negcon_input_modes.h"
#include "psx_poll_timing.h"
#include "rotary_motion_state.h"
#include "rotary_profile.h"
#include "settings_store.h"

#ifdef ENABLE_INPUT_PSX
#include "../input/psx/input_psx_jogcon_runtime_state.h"
#endif

namespace {

uint8_t storedRotaryProfile(
    const PerModeSettingsRecord& settings, SerialRotaryProfileKind kind) {
  return kind == SerialRotaryProfileKind::JogCon
      ? rotaryProfileFromWheelSensitivity(settings.wheel_sensitivity)
      : rotaryProfileFromJogconDigitalMap(settings.jogcon_digital_map);
}

void fillPsxTimingValue(
    DeviceEnum mode, const PerModeSettingsRecord& settings,
    SerialPsxTimingValue* value) {
  const uint8_t raw = psx_poll::sanitize(settings.psx_poll_timing);
  value->rate = psx_poll::rate(raw);
  value->gap = psx_poll::gap(raw);
  value->configured = raw != 0;
  value->live = mode == deviceMode;
}

void fillRotaryProfileValue(
    DeviceEnum mode, const PerModeSettingsRecord& settings,
    SerialRotaryProfileKind kind, SerialRotaryProfileValue* value) {
  const uint8_t profile = storedRotaryProfile(settings, kind);
  value->gain = rotaryProfileGain(profile);
  value->reverse = rotaryProfileReverse(profile);
  value->configured = rotaryProfileConfigured(profile);
  value->live = mode == deviceMode;
}

}  // namespace

SerialSemanticStatus serialReadPsxTiming(
    DeviceEnum mode, SerialPsxTimingValue* value) {
  if (value == nullptr) return SerialSemanticStatus::BadValue;
  if (!psx_poll::storedInRecord(mode)) {
    return SerialSemanticStatus::UnsupportedMode;
  }
  PerModeSettingsRecord settings{};
  readPerModeSettings(mode, settings);
  fillPsxTimingValue(mode, settings, value);
  return SerialSemanticStatus::Ok;
}

SerialSemanticStatus serialWritePsxTiming(
    DeviceEnum mode, uint8_t rate, uint8_t gap,
    SerialPsxTimingValue* value) {
  if (value == nullptr || rate >= psx_poll::kRateCount ||
      gap >= psx_poll::kGapCount) {
    return SerialSemanticStatus::BadValue;
  }
  if (!psx_poll::storedInRecord(mode)) {
    return SerialSemanticStatus::UnsupportedMode;
  }

  PerModeSettingsRecord settings{};
  readPerModeSettings(mode, settings);
  const uint8_t encoded = psx_poll::encode(rate, gap);
  settings.psx_poll_timing = encoded;
  if (!writePerModeSettingsDurable(mode, settings)) {
    return SerialSemanticStatus::WriteFailed;
  }

  PerModeSettingsRecord verified{};
  readPerModeSettings(mode, verified);
  if (psx_poll::sanitize(verified.psx_poll_timing) != encoded) {
    return SerialSemanticStatus::WriteFailed;
  }
  if (mode == deviceMode) {
    psx_poll_timing = encoded;
  }
  fillPsxTimingValue(mode, verified, value);
  return SerialSemanticStatus::Ok;
}

bool serialRotaryProfileApplies(
    DeviceEnum mode, SerialRotaryProfileKind kind) {
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  if (kind == SerialRotaryProfileKind::JogCon) {
    #ifdef ENABLE_INPUT_PSX
    return mode == RZORD_PSX;
    #else
    return false;
    #endif
  }
  switch (mode) {
    #ifdef ENABLE_INPUT_PSX
    case RZORD_PSX:
    #endif
    #ifdef ENABLE_INPUT_NES
    case RZORD_NES:
    #endif
    #ifdef ENABLE_INPUT_DRIVING
    case RZORD_DRIVING:
    #endif
    #ifdef ENABLE_INPUT_SMS
    case RZORD_SMS:
    #endif
    #ifdef ENABLE_INPUT_JAGUAR
    case RZORD_JAGUAR:
    #endif
    #if defined(ENABLE_INPUT_PSX) || defined(ENABLE_INPUT_NES) || \
        defined(ENABLE_INPUT_DRIVING) || defined(ENABLE_INPUT_SMS) || \
        defined(ENABLE_INPUT_JAGUAR)
      return true;
    #endif
    default:
      return false;
  }
#else
  (void)mode;
  (void)kind;
  return false;
#endif
}

SerialSemanticStatus serialReadRotaryProfile(
    DeviceEnum mode, SerialRotaryProfileKind kind,
    SerialRotaryProfileValue* value) {
  if (value == nullptr) return SerialSemanticStatus::BadValue;
  if (!serialRotaryProfileApplies(mode, kind)) {
    return SerialSemanticStatus::UnsupportedMode;
  }
  PerModeSettingsRecord settings{};
  readPerModeSettings(mode, settings);
  fillRotaryProfileValue(mode, settings, kind, value);
  return SerialSemanticStatus::Ok;
}

SerialSemanticStatus serialWriteRotaryProfile(
    DeviceEnum mode, SerialRotaryProfileKind kind,
    uint8_t gain, bool reverse, SerialRotaryProfileValue* value) {
  if (value == nullptr || gain < 1 || gain > ROTARY_PROFILE_MAX_GAIN) {
    return SerialSemanticStatus::BadValue;
  }
  if (!serialRotaryProfileApplies(mode, kind)) {
    return SerialSemanticStatus::UnsupportedMode;
  }

  PerModeSettingsRecord settings{};
  readPerModeSettings(mode, settings);
  const uint8_t profile = encodeRotaryProfile(gain, reverse);
  if (kind == SerialRotaryProfileKind::JogCon) {
    settings.wheel_sensitivity = withRotaryProfileInWheelSensitivity(
        settings.wheel_sensitivity, profile);
  } else {
    settings.jogcon_digital_map = withRotaryProfileInJogconDigitalMap(
        settings.jogcon_digital_map, profile);
  }
  if (!writePerModeSettingsDurable(mode, settings)) {
    return SerialSemanticStatus::WriteFailed;
  }

  PerModeSettingsRecord verified{};
  readPerModeSettings(mode, verified);
  if (storedRotaryProfile(verified, kind) != profile) {
    return SerialSemanticStatus::WriteFailed;
  }
  if (mode == deviceMode) {
    if (kind == SerialRotaryProfileKind::JogCon) {
      jogcon_rotary_profile = profile;
      #ifdef ENABLE_INPUT_PSX
      jogcon_wheelCenterSet = false;
      #endif
    } else {
      rotary_profile = profile;
    }
    // Drop motion scaled by the old profile. Each relative producer observes
    // the changed tuning on its next sample and establishes a fresh baseline.
    resetAllRotaryMotion();
  }
  fillRotaryProfileValue(mode, verified, kind, value);
  return SerialSemanticStatus::Ok;
}

#ifdef ENABLE_INPUT_PSX
SerialSemanticStatus serialReadNegconInputs(
    DeviceEnum mode, SerialNegconInputsValue* value) {
  if (value == nullptr) return SerialSemanticStatus::BadValue;
  if (!negconInputsStoredInRecord(mode)) {
    return SerialSemanticStatus::UnsupportedMode;
  }
  PerModeSettingsRecord settings{};
  readPerModeSettings(mode, settings);
  value->digital = negconDigitalInputsFromRaw(settings.psx_negcon_inputs);
  value->live = mode == deviceMode;
  return SerialSemanticStatus::Ok;
}

SerialSemanticStatus serialWriteNegconInputs(
    DeviceEnum mode, uint8_t digital, SerialNegconInputsValue* value) {
  if (value == nullptr || (digital & ~NEGCON_INPUT_DIGITAL_MASK) != 0) {
    return SerialSemanticStatus::BadValue;
  }
  if (!negconInputsStoredInRecord(mode)) {
    return SerialSemanticStatus::UnsupportedMode;
  }

  PerModeSettingsRecord settings{};
  readPerModeSettings(mode, settings);
  // SET NEGCON edits I/II/L only; the Quick Menu gain stays.
  settings.psx_negcon_inputs =
      encodeNegconInputs(digital, negconGainFromRaw(settings.psx_negcon_inputs));
  if (!writePerModeSettingsDurable(mode, settings)) {
    return SerialSemanticStatus::WriteFailed;
  }

  PerModeSettingsRecord verified{};
  readPerModeSettings(mode, verified);
  const uint8_t stored = negconDigitalInputsFromRaw(verified.psx_negcon_inputs);
  if (stored != digital) {
    return SerialSemanticStatus::WriteFailed;
  }
  if (mode == deviceMode) {
    negcon_digital_inputs = stored;
  }
  value->digital = stored;
  value->live = mode == deviceMode;
  return SerialSemanticStatus::Ok;
}
#endif
