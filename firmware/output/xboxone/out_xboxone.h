/*
 * SPDX-License-Identifier: MIT
 *
 * Xbox One/XGIP output for Reflex Adapt. Transport and auth sequencing are
 * donor-aligned with the GP2040-CE Xbox One implementation, which documents
 * Santroller/GIMX-derived XGIP behavior. Do not copy Santroller GPL source
 * here without an explicit license decision.
 */

#pragma once

#include <arduino/Adafruit_USBD_Device.h>
#include <device/usbd_pvt.h>
#include <tusb_option.h>

struct XboxOneOutputStatus {
  bool supported;
  bool mounted;
  bool ready;
  bool auth_ready;
  bool begin_attempted;
  bool begin_ok;
  bool usb_inited;
  bool usb_connected;
  bool usb_mounted;
  bool usb_suspended;
  uint8_t state;
  uint8_t ep_in;
  uint8_t ep_out;
  uint8_t last_command;
  uint8_t last_sequence;
  uint16_t last_len;
  uint32_t out_count;
  uint32_t in_count;
  uint32_t queue_count;
  uint32_t queue_drop_count;
  uint32_t control_count;
  uint32_t descriptor_query_count;
  uint32_t descriptor_build_count;
  uint32_t descriptor_failure_count;
  uint32_t setup_request_count;
  uint8_t last_control_type;
  uint8_t last_control_request;
  uint16_t last_control_value;
  uint16_t last_control_index;
  uint16_t last_control_length;
  uint32_t auth_console_count;
  uint32_t auth_dongle_count;
  uint32_t auth_console_backpressure_count;
  uint32_t auth_console_retry_count;
  uint32_t auth_dongle_backpressure_count;
  uint32_t auth_donor_restart_count;
};

class Adafruit_USBD_XboxOne : public Adafruit_USBD_Interface {
 public:
  Adafruit_USBD_XboxOne(uint8_t interval_ms = 1);

  bool begin(void);
  bool ready(void);
  bool sendReport(uint8_t port);

  uint16_t getInterfaceDescriptor(uint8_t itfnum, uint8_t* buf, uint16_t bufsize) override;

 private:
  uint8_t _interval_ms;
  uint8_t _endpoint_in = 0;
  uint8_t _endpoint_out = 0;
};

const char* xboxone_usb_manufacturer();
const char* xboxone_usb_product();
const char* xboxone_usb_serial();

#ifdef __cplusplus
extern "C" {
#endif

const usbd_class_driver_t* xboxone_get_driver();
bool xboxone_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, const tusb_control_request_t* request);
bool xboxone_send_report_for_port(uint8_t port);
XboxOneOutputStatus xboxone_output_status();
// The GIP announce has gone out, so a console would now start authentication.
bool xboxone_output_announced();
void xboxone_register_descriptor_hooks();
bool xboxone_windows_multi_persona_selected();
void xboxone_windows_multi_set_enabled(bool enabled);
bool xboxone_windows_multi_enabled();
uint8_t xboxone_windows_multi_slot_count();
uint16_t xboxone_windows_multi_device_version();
XboxOneOutputStatus xboxone_windows_multi_slot_status(uint8_t slot);
uint16_t xboxone_build_gamepad_report(
    uint8_t port, uint8_t sequence, uint8_t* buffer, uint16_t buffer_size);
const uint8_t* xboxone_shared_announce_packet(uint16_t* length);
const uint8_t* xboxone_shared_device_descriptor(uint16_t* length);

#ifdef __cplusplus
}
#endif
