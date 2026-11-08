#pragma once

static inline void configure_product_autodetect_hid_identity_runtime() {
  TinyUSBDevice.setManufacturerDescriptor("MiSTerAddons");
  TinyUSBDevice.setProductDescriptor(PRODUCT_NAME);
  TinyUSBDevice.setSerialDescriptor(get_reflex_input_usb_serial_descriptor());
}

static inline void configure_product_mister_hid_identity_runtime() {
  // Products can share the Classic2USB PSX GameControllerDB identity while
  // retaining distinct serial strings for MiSTer input map files.
  TinyUSBDevice.setDeviceVersion(get_reflex_mister_hid_device_version());
  TinyUSBDevice.setID(RZORD1_VID, get_reflex_mister_hid_pid());
}
