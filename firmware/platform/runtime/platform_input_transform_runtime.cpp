#include "platform_input_transform_runtime.h"

#include "../../product_config.h"

bool platformShouldBypassAnalogCalibrationForInputMode(DeviceEnum mode) {
#ifdef ENABLE_INPUT_JVS
  // JVS supplies 16-bit absolute channels. Classic int8 calibration must not
  // truncate or recenter those cabinet-specific samples.
  return mode == RZORD_JVS;
#else
  (void)mode;
  return false;
#endif
}
