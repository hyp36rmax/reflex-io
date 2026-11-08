#pragma once

#include <stdint.h>
#include <string.h>

#include "analog_calibration_state.h"
#include "button_remap.h"

// Quick Config must not change live remaps or calibration until the outer
// Save/Default action commits the menu session. Remap tables are the full
// width (remap_table.h); the 19-entry record is never passed here.
using QuickConfigRemaps = uint8_t[REMAP_MAX_BUTTONS];

class QuickConfigTransaction {
 public:
  void begin(const QuickConfigRemaps& remaps, const AnalogCalibrationData& calibration) {
    memcpy(original_remaps, remaps, sizeof(original_remaps));
    memcpy(staged_remaps, remaps, sizeof(staged_remaps));
    original_calibration = calibration;
    staged_calibration = calibration;
    active = true;
  }

  QuickConfigRemaps& remaps() { return staged_remaps; }
  const QuickConfigRemaps& remaps() const { return staged_remaps; }
  AnalogCalibrationData& calibration() { return staged_calibration; }
  const AnalogCalibrationData& calibration() const { return staged_calibration; }

  void stage(const QuickConfigRemaps& remaps, const AnalogCalibrationData& calibration) {
    memcpy(staged_remaps, remaps, sizeof(staged_remaps));
    staged_calibration = calibration;
  }

  // Remap Default: reset the staged remaps only, keeping calibration.
  void resetRemaps(const QuickConfigRemaps& remaps) {
    memcpy(staged_remaps, remaps, sizeof(staged_remaps));
  }

  void resetToDefaults(const QuickConfigRemaps& remaps) {
    memcpy(staged_remaps, remaps, sizeof(staged_remaps));
    clearCalibration();
  }

  void clearCalibration() {
    staged_calibration = {
      .lx = { .raw_min = -128, .raw_max = 127, .raw_center = 0 },
      .ly = { .raw_min = -128, .raw_max = 127, .raw_center = 0 },
      .rx = { .raw_min = -128, .raw_max = 127, .raw_center = 0 },
      .ry = { .raw_min = -128, .raw_max = 127, .raw_center = 0 },
      .enabled = false,
    };
  }

  void commit(QuickConfigRemaps& remaps, AnalogCalibrationData& calibration) {
    memcpy(remaps, staged_remaps, sizeof(staged_remaps));
    calibration = staged_calibration;
    active = false;
  }

  void finish() { active = false; }

  template <typename Settings>
  void copyTo(Settings& settings) const {
    static_assert(sizeof(settings.remaps) == REMAP_RECORD_BUTTONS,
                  "the settings record keeps the 19 button entries");
    settings.analog_cal_lx_min = staged_calibration.lx.raw_min;
    settings.analog_cal_lx_max = staged_calibration.lx.raw_max;
    settings.analog_cal_ly_min = staged_calibration.ly.raw_min;
    settings.analog_cal_ly_max = staged_calibration.ly.raw_max;
    settings.analog_cal_rx_min = staged_calibration.rx.raw_min;
    settings.analog_cal_rx_max = staged_calibration.rx.raw_max;
    settings.analog_cal_ry_min = staged_calibration.ry.raw_min;
    settings.analog_cal_ry_max = staged_calibration.ry.raw_max;
    settings.analog_cal_enabled = staged_calibration.enabled ? 1 : 0;
    remapTableToRecord(settings.remaps, staged_remaps);
  }

  void discard(QuickConfigRemaps& remaps, AnalogCalibrationData& calibration) {
    if (!active) return;
    memcpy(remaps, original_remaps, sizeof(original_remaps));
    calibration = original_calibration;
    active = false;
  }

  void discardForActiveMode(bool entry_mode_is_still_active, QuickConfigRemaps& remaps,
                            AnalogCalibrationData& calibration) {
    if (entry_mode_is_still_active) {
      discard(remaps, calibration);
    } else {
      finish();
    }
  }

 private:
  uint8_t original_remaps[REMAP_MAX_BUTTONS] = {};
  uint8_t staged_remaps[REMAP_MAX_BUTTONS] = {};
  AnalogCalibrationData original_calibration = {};
  AnalogCalibrationData staged_calibration = {};
  bool active = false;
};
