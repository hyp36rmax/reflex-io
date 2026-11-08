#pragma once

#ifdef ADAPT_OUTPUT_USB_DEVICE
#define ADAPT_POINTER_MOUSE_REPORT_ID 4
inline uint8_t adaptPointerMouseConfigValue() {
  return menu_touchpad_mouse;
}
#include "output_pointer_mouse_runtime.h"
#endif

#ifdef ENABLE_OUTPUT_ESP32_BT
#include "../out_esp32_bt.h"
#define ADAPT_PRODUCT_NON_USB_BACKEND 1
inline void configure_usb_output() { esp32_bt_init(); }
inline void send_usb_report() { esp32_bt_send(); }
#endif
