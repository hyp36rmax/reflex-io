#pragma once

#include "../../firmware_platform_config.h"
#include "../../product_config.h"
#include "../../core/controller_settings_state.h"
#include "../../core/device_runtime_state.h"
#include "output_usb_product_player_count.h"

inline uint8_t output_usb_player_count() {
  uint8_t count = max_devices;
  if (count == 0) {
    count = 1;
  }

  count = adjust_product_usb_player_count(count);
  return (count > MAX_USB_OUT) ? MAX_USB_OUT : count;
}