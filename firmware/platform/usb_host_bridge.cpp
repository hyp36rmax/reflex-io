#include "../product_config.h"

#include "usb_host_bridge.h"

#include <Adafruit_TinyUSB.h>
#include "pico/stdlib.h"

#include "../output/out_report.h"

#ifdef ENABLE_INPUT_USB
#include "../input/usb_host/input_usb/input_usb.h"
#endif

#if defined(ENABLE_INPUT_USB) || defined(ENABLE_USB_AUTH_SIDECAR_CORE1)
#include "../input/usb_host/input_usb_host_service.h"
#endif

#ifdef ENABLE_USB_AUTH_SIDECAR
#include "../output/auth/ps_auth_dongle_runtime.h"
#endif

#if defined(ENABLE_INPUT_USB) || defined(ENABLE_USB_AUTH_SIDECAR_CORE1)
extern "C" {
// The Arduino RP2040 core launches setup1/loop1 before setup(). PIO-USB host
// work is timing-sensitive and stack-heavy enough that core1 should not share
// core0's boot/runtime stack in USB-input or auth-sidecar builds.
bool core1_separate_stack = true;
}
#endif

void queue_set_output_report(uint8_t dev_addr, uint8_t idx, bool use_interrupt_edp, uint8_t report_id, const uint8_t* buffer, uint8_t len) {
  do_queue_out_report(buffer, len, report_id, dev_addr, idx, use_interrupt_edp, HID_REPORT_TYPE_OUTPUT);
}

void queue_set_feature_report(uint8_t dev_addr, uint8_t idx, bool use_interrupt_edp, uint8_t report_id, const uint8_t* buffer, uint8_t len) {
  do_queue_out_report(buffer, len, report_id, dev_addr, idx, use_interrupt_edp, HID_REPORT_TYPE_FEATURE);
}

void queue_get_feature_report(uint8_t dev_addr, uint8_t idx, bool use_interrupt_edp, uint8_t report_id, uint8_t len) {
  do_queue_get_report(report_id, dev_addr, idx, use_interrupt_edp, HID_REPORT_TYPE_FEATURE, len);
}

void host_get_report_cb(uint8_t dev_addr, uint8_t interface, uint8_t report_id, uint8_t report_type, uint8_t* report, uint16_t len) {
#ifdef ENABLE_INPUT_USB
  if (report_type == HID_REPORT_TYPE_FEATURE) {
    inputusb_hid_get_report_response(dev_addr, interface, report_id,
                                     report, len);
  }
#endif
#ifdef ENABLE_USB_AUTH_SIDECAR
  ps_auth_dongle_handle_get_report_response(dev_addr, interface, report_id, report, len);
#else
  (void)dev_addr; (void)interface; (void)report_id; (void)report_type; (void)report; (void)len;
#endif
}

void host_set_report_complete_cb(uint8_t dev_addr, uint8_t interface,
                                 uint8_t report_id, uint8_t report_type,
                                 uint16_t len) {
#ifdef ENABLE_INPUT_USB
  inputusb_hid_set_report_complete_cb(dev_addr, interface, report_id,
                                      report_type, len);
#endif
#ifdef ENABLE_USB_AUTH_SIDECAR
  ps_auth_dongle_handle_set_report_complete(dev_addr, interface, report_id,
                                            len);
#else
  (void)dev_addr; (void)interface; (void)report_id;
#endif
  (void)report_type; (void)len;
}

void host_sent_report_cb(uint8_t dev_addr, uint8_t idx, uint8_t const* report, uint16_t len) {
#ifdef ENABLE_USB_AUTH_SIDECAR
  ps_auth_dongle_handle_sent_report_response(dev_addr, idx, report, len);
#else
  (void)dev_addr; (void)idx; (void)report; (void)len;
#endif
}

#if defined(ENABLE_INPUT_USB) || defined(ENABLE_USB_AUTH_SIDECAR_CORE1)
void setup1() {
  // PIO-USB host setup is requested by core0, then owned by core1.
}

void loop1() {
  // Always enter the bridge so a firmware-update flash park can be
  // acknowledged even before core0 has requested USB-host startup.
  usb_host_service_core1_task();
}
#endif
