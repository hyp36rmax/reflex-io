#pragma once

// Switch 2 Pro descriptor setup, included with the other mode setup helpers
// so configure_usb_output() can reach it.

#include <pico/unique_id.h>

#include "../switch/output_switch_report_stream.h"

// The real controller answers every 4 ms poll; queue a fresh frame as often.
static SwitchReportStream switch2pro_report_stream{0, false, 4};

static void configure_switch2pro_output_runtime() {
  use_device_hid_class = false;
  if (!_switch2pro) {
    pico_unique_board_id_t id{};
    pico_get_unique_board_id(&id);
    _switch2pro = new Adafruit_USBD_Switch2Pro(id.id);
  }
  switch2pro_report_stream.reset();
  TinyUSBDevice.setID(switch2_pro::kVendorId, switch2_pro::kProductId);
  TinyUSBDevice.setVersion(0x0200);
  TinyUSBDevice.setDeviceVersion(switch2_pro::kDeviceVersion);
  // Miscellaneous / common class / interface association, as captured.
  TinyUSBDevice.setDeviceClass(0xEF);
  TinyUSBDevice.setDeviceSubClass(0x02);
  TinyUSBDevice.setDeviceProtocol(0x01);
  TinyUSBDevice.setMaxPacketSize0(64);
  TinyUSBDevice.setManufacturerDescriptor("Nintendo");
  TinyUSBDevice.setProductDescriptor("Switch 2 Pro Controller");
  TinyUSBDevice.setSerialDescriptor("00");
  TinyUSBDevice.setLanguageDescriptor(0x0409);
  TinyUSBDevice.setConfigurationAttribute(0xC0);
  TinyUSBDevice.setConfigurationMaxPower(500);
}
