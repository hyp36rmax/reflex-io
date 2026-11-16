#include "../../product_config.h"

#include "platform_feedback_led_runtime.h"

#include "../../core/controller_frame_state.h"
#include "../../core/controller_settings_state.h"
#include "../../core/device_runtime_state.h"
#include "../../output/output_runtime_state.h"
#include "../rgb_led.h"
void updatePlatformRgbLedFromInput(bool updated) {
  #ifdef USE_WS2812
  if (!updated || led_mode == LED_MODE_OFF) {
    return;
  }

  bool analogAssigned = false;
  uint8_t rumbleMax = 0;
  for (uint8_t i = 0; i < max_devices && i < MAX_USB_OUT; ++i) {
    controller_state_t& frame = controllerFrame(i);
    if (!frame.connected) {
      continue;
    }

    if (led_mode == LED_MODE_REACTIVE) {
      rgbLed.triggerButtons(frame.digital_buttons);
    }
    if (led_mode == LED_MODE_ANALOG && !analogAssigned) {
      uint8_t lx_scaled = (frame.LX + 32768) >> 8;
      uint8_t ly_scaled = (frame.LY + 32768) >> 8;
      rgbLed.setAnalog(lx_scaled, ly_scaled);
      analogAssigned = true;
    }
    if (led_mode == LED_MODE_RUMBLE) {
      const uint8_t playerRumble =
          rumble_left[i] > rumble_right[i] ? rumble_left[i] : rumble_right[i];
      if (playerRumble > rumbleMax) rumbleMax = playerRumble;
    }
  }
  if (led_mode == LED_MODE_RUMBLE) rgbLed.setRumble(rumbleMax);
  #else
  (void)updated;
  #endif
}

