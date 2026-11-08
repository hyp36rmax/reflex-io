#pragma once

#include "../../core/device_mode.h"

// Product input types whose native analog domain must bypass classic calibration.
bool platformShouldBypassAnalogCalibrationForInputMode(DeviceEnum mode);
