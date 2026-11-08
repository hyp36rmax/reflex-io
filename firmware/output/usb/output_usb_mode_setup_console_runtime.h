#pragma once

// Internal console-family USB mode setup helpers extracted from output_usb_mode_setup_runtime.h.

#include "output_usb_xinput2p_slots_runtime.h"
#include "../switch2/output_switch2pro_setup_runtime.h"

static void configure_xinput_output_runtime() {
  use_device_hid_class = false;
  xinput_composite_hid_enabled = false;
  xinput_seen_string_0xEE = false;
  xinput_seen_string_4 = false;
  xinput_seen_in_xfer = false;
  xinput_use_raw_descriptors = false;
  xinput_set_subtype(determine_xinput_subtype());
  xinput_register_descriptor_hooks();
  _xinput = new Adafruit_USBD_XInput();
  TinyUSBDevice.setID(0x045E, 0x028E);
  TinyUSBDevice.setDeviceClass(0xFF);
  TinyUSBDevice.setDeviceSubClass(0xFF);
  TinyUSBDevice.setDeviceProtocol(0xFF);
  TinyUSBDevice.setManufacturerDescriptor("\xc2\xa9Microsoft Corporation");
  TinyUSBDevice.setProductDescriptor("Controller");
  TinyUSBDevice.setSerialDescriptor("0000000000000000");
  TinyUSBDevice.setConfigurationMaxPower(500);
  TinyUSBDevice.setConfigurationAttribute(0xA0);
}

static void configure_xinput_wireless_output_runtime() {
  use_device_hid_class = false;
  _xinputw = new Adafruit_USBD_XInputW();
  TinyUSBDevice.setID(0x045E, 0x0719);
  TinyUSBDevice.setVersion(0x0200);
  TinyUSBDevice.setDeviceVersion(0x0100);
  TinyUSBDevice.setManufacturerDescriptor("\xc2\xa9Microsoft Corporation");
  TinyUSBDevice.setProductDescriptor("Xbox 360 Wireless Receiver for Windows");
  TinyUSBDevice.setDeviceClass(0xFF);
  TinyUSBDevice.setDeviceSubClass(0xFF);
  TinyUSBDevice.setDeviceProtocol(0xFF);
  TinyUSBDevice.setConfigurationAttribute(0xA0);
}

#include "output_usb_product_console_runtime.h"

static void configure_xinput_2p_output_runtime() {
#ifdef FORCE_XINPUT2P_SINGLE_DRIVER
  configure_xinput_output_runtime();
  return;
#endif
  use_device_hid_class = false;
  xinput_set_subtype(determine_xinput_subtype());
  configure_product_xinput_2p_output_runtime();
}

static void configure_xid_output_runtime() {
  use_device_hid_class = false;
  _xid = new Adafruit_USBD_XID();
  TinyUSBDevice.setID(0x045E, 0x0289);
  TinyUSBDevice.setDeviceVersion(0x0100);
  TinyUSBDevice.setVersion(0x0110);
  configure_product_xid_identity_runtime();
}

#ifdef ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT
static void configure_xboxone_output_runtime() {
  configure_product_xboxone_output_runtime();
}
#endif

static void configure_ps3_output_runtime() {
  use_device_hid_class = true;
  if (can_run_usb_detection()) {
    // AUTO uses the PS3-shaped descriptor only as a discriminator, but it must
    // include the 0x2621 trigger and F2/F5 feature reports. After probing,
    // runtime PS3 uses the product's transport below.
    usb_device_hid[0] = new Adafruit_USBD_HID(
      desc_ps3_probe_report,
      sizeof(desc_ps3_probe_report),
      HID_ITF_PROTOCOL_NONE,
      1,
      true
    );
    TinyUSBDevice.setID(0x054c, 0x0268);
    TinyUSBDevice.setManufacturerDescriptor("Sony");
    TinyUSBDevice.setProductDescriptor("PLAYSTATION(R)3 Controller");
    TinyUSBDevice.setVersion(0x0200);
    TinyUSBDevice.setDeviceVersion(0x0100);
    return;
  }

#if defined(ADAPT_FEATURE_NATIVE_PS3_OUTPUT)
  resetPs3NativeUsbState();
  usb_device_hid[0] = new Adafruit_USBD_HID(
    ps3_report_descriptor_pure, sizeof(ps3_report_descriptor_pure),
    HID_ITF_PROTOCOL_NONE, 1, true);
  TinyUSBDevice.setID(0x054c, 0x0268);
  TinyUSBDevice.setManufacturerDescriptor("Sony");
  TinyUSBDevice.setProductDescriptor("PLAYSTATION(R)3 Controller");
  TinyUSBDevice.setVersion(0x0200);
  TinyUSBDevice.setDeviceVersion(0x0100);
  return;
#endif

  // Products without native PS3 policy retain the input-only generic transport.
  usb_device_hid[0] = new Adafruit_USBD_HID(
    desc_hidgeneric_clean_report,
    sizeof(desc_hidgeneric_clean_report),
    HID_ITF_PROTOCOL_NONE,
    1,
    false
  );
  TinyUSBDevice.setDeviceVersion(current_bcd_device_version());
  TinyUSBDevice.setID(RZORD1_VID, RZORD1_PID);
  TinyUSBDevice.setManufacturerDescriptor("MiSTerAddons");
  TinyUSBDevice.setProductDescriptor(get_reflex_input_product_name());
  TinyUSBDevice.setSerialDescriptor(get_reflex_input_product_name());
}

static void configure_ps4_output_runtime() {
  use_device_hid_class = true;
  usb_device_hid[0] = new Adafruit_USBD_HID(
    ps4_desc_hid_report,
    sizeof(ps4_desc_hid_report),
    HID_ITF_PROTOCOL_NONE,
    PS4_GAMEPAD_POLL_INTERVAL_MS,
    false);
  TinyUSBDevice.setID(PS4_GAMEPAD_VENDOR_ID, PS4_GAMEPAD_PRODUCT_ID);
  TinyUSBDevice.setManufacturerDescriptor("Sony Computer Entertainment");
  TinyUSBDevice.setProductDescriptor("Wireless Controller");
  TinyUSBDevice.setVersion(0x0200);
  TinyUSBDevice.setDeviceVersion(0x0100);
}

#ifdef ENABLE_OUTPUT_PS5
#include "output_usb_mode_setup_ps5_runtime.h"
#endif

static void configure_switch_output_runtime() {
  use_device_hid_class = true;
  for (uint8_t i = 0; i < output_usb_player_count(); ++i) {
    usb_device_hid[i] = new Adafruit_USBD_HID(pokken_report_desc, sizeof(pokken_report_desc), HID_ITF_PROTOCOL_NONE, 1, true);
  }
  TinyUSBDevice.setID(0x0F0D, 0x0092);
  TinyUSBDevice.setManufacturerDescriptor("HORI CO.,LTD.");
  TinyUSBDevice.setProductDescriptor("POKKEN CONTROLLER");
  TinyUSBDevice.setSerialDescriptor("1.0");
  TinyUSBDevice.setLanguageDescriptor(0x0409);
  TinyUSBDevice.setVersion(0x0200);
  TinyUSBDevice.setDeviceVersion(0x0210);
}

static void configure_switchpro_output_runtime() {
  // The output AUTO detector resolves before the input AUTO detector. Select
  // the profile again at the descriptor boundary so a restored/manual N64
  // input always enumerates with the native NSO identity, even if the input
  // module was initialized while another output personality was active.
  output_apply_automatic_switch_profile_for_input_mode(deviceMode);

  use_device_hid_class = true;
  for (uint8_t i = 0; i < output_usb_player_count(); ++i) {
    usb_device_hid[i] = new Adafruit_USBD_HID(switch_usb_report_descriptor, sizeof(switch_usb_report_descriptor), HID_ITF_PROTOCOL_NONE, 1, true);
    switchpro[i] = new OutputSwitchPro();
  }

  switch (SwitchCommon::switchpro_mode) {
    case SWITCHPRO_PRO:
      TinyUSBDevice.setID(0x057E, 0x2009);
      TinyUSBDevice.setManufacturerDescriptor("Nintendo Co., Ltd.");
      TinyUSBDevice.setProductDescriptor("Pro Controller");
      TinyUSBDevice.setSerialDescriptor("000000000001");
      TinyUSBDevice.setLanguageDescriptor(0x0409);
      TinyUSBDevice.setVersion(0x0200);
      TinyUSBDevice.setDeviceVersion(0x0210);
      break;
    case SWITCHPRO_NES:
    case SWITCHPRO_SNES:
      TinyUSBDevice.setID(0x057E, 0x2017);
      TinyUSBDevice.setManufacturerDescriptor("Nintendo Co., Ltd.");
      TinyUSBDevice.setProductDescriptor("SNES Controller");
      TinyUSBDevice.setSerialDescriptor("000000000001");
      TinyUSBDevice.setLanguageDescriptor(0x0409);
      TinyUSBDevice.setVersion(0x0200);
      TinyUSBDevice.setDeviceVersion(0x0210);
      break;
    case SWITCHPRO_GENESIS:
      TinyUSBDevice.setID(0x057E, 0x201E);
      TinyUSBDevice.setManufacturerDescriptor("Nintendo Co., Ltd.");
      TinyUSBDevice.setProductDescriptor("MD/Gen Control Pad");
      TinyUSBDevice.setSerialDescriptor("000000000001");
      TinyUSBDevice.setLanguageDescriptor(0x0409);
      TinyUSBDevice.setVersion(0x0200);
      TinyUSBDevice.setDeviceVersion(0x0212);
      break;
    case SWITCHPRO_N64:
      TinyUSBDevice.setID(0x057E, 0x2019);
      TinyUSBDevice.setManufacturerDescriptor("Nintendo Co., Ltd.");
      TinyUSBDevice.setProductDescriptor("N64 Controller");
      TinyUSBDevice.setSerialDescriptor("000000000001");
      TinyUSBDevice.setLanguageDescriptor(0x0409);
      TinyUSBDevice.setVersion(0x0200);
      TinyUSBDevice.setDeviceVersion(0x0212);
      break;
  }
}

