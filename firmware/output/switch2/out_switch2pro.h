#pragma once

// Switch 2 Pro Controller (057E:2069) USB device persona: one TinyUSB class
// driver owning the HID interface (input report 0x09/0x05, rumble report
// 0x02) and the vendor bulk interface the console sends commands on. The
// protocol itself lives in switch2_pro_protocol.h.

#include <arduino/Adafruit_USBD_Device.h>
#include <device/usbd_pvt.h>
#include <tusb_option.h>

#include "switch2_pro_protocol.h"

class Adafruit_USBD_Switch2Pro : public Adafruit_USBD_Interface {
 public:
  explicit Adafruit_USBD_Switch2Pro(const uint8_t boardId[8]);

  bool begin();
  uint16_t getInterfaceDescriptor(uint8_t itfnum, uint8_t* buf,
                                  uint16_t bufsize) override;

  // The console has initialised USB and the input endpoint is idle.
  bool inputReady() const;
  // Packs and submits one input report. `sourcePort` receives the rumble.
  bool sendInputReport(uint8_t sourcePort, uint32_t buttons,
                       const uint8_t leftStick[3], const uint8_t rightStick[3]);
  // Stops rumble when the console stops sending rumble reports.
  void task();

  const switch2_pro::Responder& responder() const { return responder_; }

  // TinyUSB class driver callbacks.
  void busReset();
  uint16_t open(uint8_t rhport, const tusb_desc_interface_t* itf,
                uint16_t maxLength);
  bool controlTransfer(uint8_t rhport, uint8_t stage,
                       const tusb_control_request_t* request);
  bool transferComplete(uint8_t rhport, uint8_t endpoint, xfer_result_t result,
                        uint32_t length);
  bool vendorControlTransfer(uint8_t rhport, uint8_t stage,
                             const tusb_control_request_t* request);

 private:
  void armHidOut();
  void armBulkOut();
  void handleBulkPacket(uint32_t length);

  switch2_pro::Identity identity_{};
  switch2_pro::Responder responder_;
  uint8_t rhport_ = 0;
  uint8_t itf_base_ = 0xFF;
  uint8_t hid_in_ = 0;
  uint8_t hid_out_ = 0;
  uint8_t bulk_in_ = 0;
  uint8_t bulk_out_ = 0;
  uint8_t rumble_port_ = 0;
  bool bulk_zlp_pending_ = false;
  bool rumble_active_ = false;
  uint32_t last_rumble_ms_ = 0;
  switch2_pro::CommandAssembler assembler_;
  uint8_t hid_in_buf_[switch2_pro::kReportSize] = {};
  uint8_t hid_out_buf_[switch2_pro::kReportSize] = {};
  uint8_t bulk_out_buf_[switch2_pro::kPacketSize] = {};
  uint8_t response_[switch2_pro::kMaxResponseSize] = {};
  uint8_t control_buf_[switch2_pro::kReportSize] = {};
};

#ifdef __cplusplus
extern "C" {
#endif
const usbd_class_driver_t* switch2pro_get_driver();
#ifdef __cplusplus
}
#endif

bool switch2pro_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage,
                                       const tusb_control_request_t* request);
