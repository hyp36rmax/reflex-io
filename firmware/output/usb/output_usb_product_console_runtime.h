#pragma once

static inline bool configure_product_xinput_2p_output_runtime() {
#ifdef ENABLE_XINPUT2P_WIRELESS_TRANSPORT
  configure_xinput_wireless_output_runtime();
  return true;
#else
  return false;
#endif
}

static inline void configure_product_xid_identity_runtime() {
  TinyUSBDevice.setManufacturerDescriptor("MiSTerAddons");
  TinyUSBDevice.setProductDescriptor(PRODUCT_NAME);
  TinyUSBDevice.setSerialDescriptor(get_reflex_input_usb_serial_descriptor());
}

#ifdef ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT
static inline void configure_product_xboxone_output_runtime() {
  use_device_hid_class = false;
  const bool windowsMulti = xboxone_windows_multi_persona_selected();
  xboxone_windows_multi_set_enabled(windowsMulti);
  xboxone_register_descriptor_hooks();
  _xboxone = new Adafruit_USBD_XboxOne();
  if (windowsMulti) {
    TinyUSBDevice.setID(RZORD1_VID, RZORD1_PID);
    TinyUSBDevice.setVersion(0x0200);
    TinyUSBDevice.setDeviceVersion(xboxone_windows_multi_device_version());
    TinyUSBDevice.setMaxPacketSize0(64);
    TinyUSBDevice.setDeviceClass(0x00);
    TinyUSBDevice.setDeviceSubClass(0x00);
    TinyUSBDevice.setDeviceProtocol(0x00);
    TinyUSBDevice.setManufacturerDescriptor(xboxone_usb_manufacturer());
    TinyUSBDevice.setProductDescriptor(xboxone_usb_product());
    TinyUSBDevice.setSerialDescriptor(xboxone_usb_serial());
    TinyUSBDevice.setConfigurationMaxPower(500);
    TinyUSBDevice.setConfigurationAttribute(0xA0);
    return;
  }
  TinyUSBDevice.setID(0x0E6F, 0x02A4);
  TinyUSBDevice.setVersion(0x0200);
  TinyUSBDevice.setDeviceVersion(0x0101);
  TinyUSBDevice.setMaxPacketSize0(64);
  TinyUSBDevice.setDeviceClass(0xFF);
  TinyUSBDevice.setDeviceSubClass(0xFF);
  TinyUSBDevice.setDeviceProtocol(0xFF);
  TinyUSBDevice.setManufacturerDescriptor(xboxone_usb_manufacturer());
  TinyUSBDevice.setProductDescriptor(xboxone_usb_product());
  TinyUSBDevice.setSerialDescriptor(xboxone_usb_serial());
  const uint8_t securityString = TinyUSBDevice.addStringDescriptor(
    "Xbox Security Method 3, Version 1.00, \xc2\xa9 2005 Microsoft Corporation. All rights reserved.");
  // The licensed PDP identity exposes this contract at string index 4.
  (void)securityString;
  TinyUSBDevice.setConfigurationMaxPower(500);
  TinyUSBDevice.setConfigurationAttribute(0xA0);
}
#endif
