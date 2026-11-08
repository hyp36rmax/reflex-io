#include <Arduino.h>

#include "negcon_input_modes.h"

#ifdef ENABLE_INPUT_PSX
uint8_t __not_in_flash_func(triggerModeForFrame)(const controller_state_t& frame) {
  return triggerModeForModeAndFrame(deviceMode, trigger_mode, frame);
}
#endif
