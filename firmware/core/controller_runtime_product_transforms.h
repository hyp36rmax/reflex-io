#pragma once

#include "../product_config.h"
#include "controller_state.h"
#include "stick_center.h"

#if ADAPT_POLICY_USB_INPUT_ANALOG_BENCHMARK
#include "../features/modern2usb_analog/modern2usb_analog_feature.h"
#endif
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
#include "../features/usb2db15/usb2db15_arcade_assignment_capture.h"
#endif

inline bool controllerProductBypassesStickDeadzone() {
#if ADAPT_POLICY_USB_INPUT_ANALOG_BENCHMARK
  return true;
#else
  return false;
#endif
}

inline bool controllerProductRequiresDigitalTriggerSources() {
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
  return true;
#else
  return false;
#endif
}

// 8-bit trigger travel that presses on a digital-only product. The arcade
// session shares it with assignment capture and its release gate.
inline bool controllerProductDigitalTriggerPressed(uint8_t value) {
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
  return usb2db15ArcadeAnalogTriggerPressed(value);
#else
  return value != 0;
#endif
}

inline bool controllerProductHandlesStickCentering(
    uint8_t player, controller_state_t& frame) {
#if ADAPT_POLICY_USB_INPUT_ANALOG_BENCHMARK
  stickCenter.update(player, false, 0, 0, 0, 0);
  modern2usbAnalogProcessFrame(player, frame);
  return true;
#else
  (void)player;
  (void)frame;
  return false;
#endif
}
