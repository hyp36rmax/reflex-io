#pragma once

#include "output_usb_product_identity_runtime.h"

// Internal HID-family USB mode setup helpers extracted from output_usb_mode_setup_runtime.h.

static void configure_management_composite_buffer_runtime() {
  // CDC plus one or more HID interfaces can exceed Adafruit TinyUSB's default
  // 256-byte configuration buffer. One shared buffer covers DInput and the
  // specialized MiSTer PSX composites without changing their HID reports.
  static uint8_t management_configuration_buffer[512] __attribute__((aligned(4)));
  TinyUSBDevice.setConfigurationBuffer(
    management_configuration_buffer,
    sizeof(management_configuration_buffer)
  );
}

static void configure_generic_mister_hid_output_runtime() {
  use_device_hid_class = true;

  // DInput combines CDC, MSC, and one or more HID interfaces. The Auto probe
  // also adds the Xbox security interface. Either composite can exceed
  // Adafruit TinyUSB's default 256-byte configuration buffer, which causes
  // Windows to reject the device descriptor and leaves boot apparently hung.
  configure_management_composite_buffer_runtime();

  if (can_run_usb_detection()) {
    // Keep the RetroZord generic VID/PID and Xbox security side-interface.
    // The HID report adds local PS3 feature/output discriminators alongside
    // the donor PS4 trigger so mounted generic HID hosts can still time out.
    use_device_hid_class = true;
    xinput_seen_string_0xEE = false;
    xinput_seen_string_4 = false;
    xinput_seen_in_xfer = false;
    xinput_auth_invalidate();
    const uint8_t* autoDetectReportDesc = desc_hidgeneric_autodetect_report;
    const uint16_t autoDetectReportDescLen = sizeof(desc_hidgeneric_autodetect_report);
    autodetect_register_descriptor_hooks(autoDetectReportDescLen);
    usb_device_hid[0] = new Adafruit_USBD_HID(
      autoDetectReportDesc,
      autoDetectReportDescLen,
      HID_ITF_PROTOCOL_NONE,
      1,
      true
    );
    _usb_detect_xinput_auth = new Adafruit_USBD_XInputAuth();

    TinyUSBDevice.setID(0x0FFF, 0x0FF9);
    TinyUSBDevice.setVersion(0x0200);
    TinyUSBDevice.setDeviceVersion(usb_detect_probe_device_version());
    TinyUSBDevice.setDeviceClass(0x00);
    TinyUSBDevice.setDeviceSubClass(0x00);
    TinyUSBDevice.setDeviceProtocol(0x00);
    TinyUSBDevice.setConfigurationMaxPower(500);
    TinyUSBDevice.setConfigurationAttribute(0xA0);
    configure_product_autodetect_hid_identity_runtime();
    return;
  }

  configure_product_mister_hid_identity_runtime();
  TinyUSBDevice.setManufacturerDescriptor("MiSTerAddons");
  TinyUSBDevice.setProductDescriptor(PRODUCT_NAME);
  TinyUSBDevice.setSerialDescriptor(get_reflex_input_usb_serial_descriptor());

  const bool managementEndpoints =
    output_allows_management_usb_endpoints(get_effective_output_mode());
  const uint8_t* reportDesc = managementEndpoints
    ? desc_hidgeneric_report
    : desc_hidgeneric_clean_report;
  const uint16_t reportDescSize = managementEndpoints
    ? sizeof(desc_hidgeneric_report)
    : sizeof(desc_hidgeneric_clean_report);

  for (uint8_t i = 0; i < output_usb_player_count(); ++i) {
    usb_device_hid[i] = new Adafruit_USBD_HID(
      reportDesc,
      reportDescSize,
      HID_ITF_PROTOCOL_NONE,
      1,
      managementEndpoints
    );
  }
}

#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
// Keep the legacy MiSTer VID/PID and product-name compatibility checks, but
// give each specialized PSX report family its own SDL/GameControllerDB GUID.
static constexpr uint16_t kMisterJogConDeviceVersion = 0x0101;
static constexpr uint16_t kMisterNegConDeviceVersion = 0x0102;
static constexpr uint16_t kMisterGunConDeviceVersion = 0x0103;
// Reserve profile 98 on the current PID; 01..24 are classic inputs, 99 is Auto.
static constexpr uint16_t kMisterPaddleDeviceVersion = 0x0198;
static constexpr uint16_t kMisterSpinnerDeviceVersion = 0x0197;
static constexpr uint16_t kRotaryMouseDeviceVersion = 0x0196;
static constexpr uint16_t kRemoteDeviceVersion = 0x0195;

static void configure_remote_output_runtime() {
  use_device_hid_class = true;
  configure_management_composite_buffer_runtime();
  usb_device_hid[0] = new Adafruit_USBD_HID(
      remote_report_descriptor, sizeof(remote_report_descriptor), HID_ITF_PROTOCOL_NONE, 1, false);
  TinyUSBDevice.setID(RZORD1_VID, RZORD1_PID);
  TinyUSBDevice.setDeviceVersion(kRemoteDeviceVersion);
  TinyUSBDevice.setManufacturerDescriptor(RZORD1_VENDOR);
  TinyUSBDevice.setProductDescriptor("ReflexRemote");
  TinyUSBDevice.setSerialDescriptor("ReflexRemote");
}

static void configure_mister_jogcon_output_runtime() {
  use_device_hid_class = true;
  for (uint8_t i = 0; i < output_usb_player_count(); ++i) {
    usb_device_hid[i] = new Adafruit_USBD_HID(jogconmister_report_descriptor, sizeof(jogconmister_report_descriptor), HID_ITF_PROTOCOL_NONE, 1, false);
  }

  TinyUSBDevice.setID(RZORD1_VID, RZORD1_MISTER_PID);
  TinyUSBDevice.setDeviceVersion(kMisterJogConDeviceVersion);
  TinyUSBDevice.setManufacturerDescriptor(RZORD1_VENDOR);
  TinyUSBDevice.setProductDescriptor(get_reflex_input_product_name());
  TinyUSBDevice.setSerialDescriptor(get_reflex_input_product_name());
}

static void configure_mister_paddle_output_runtime() {
  use_device_hid_class = true;
  for (uint8_t i = 0; i < output_usb_player_count(); ++i) {
    usb_device_hid[i] = new Adafruit_USBD_HID(paddlemister_report_descriptor, sizeof(paddlemister_report_descriptor), HID_ITF_PROTOCOL_NONE, 1, false);
  }

  // A distinct arcade identity avoids MiSTer's ReflexPSWheel quirk, which
  // routes steering to analog X and excludes the dedicated paddle channel.
  TinyUSBDevice.setID(RZORD1_VID, RZORD1_PID);
  TinyUSBDevice.setDeviceVersion(kMisterPaddleDeviceVersion);
  TinyUSBDevice.setManufacturerDescriptor(RZORD1_VENDOR);
  TinyUSBDevice.setProductDescriptor(get_reflex_input_product_name());
  TinyUSBDevice.setSerialDescriptor(get_reflex_input_product_name());
}

static void configure_mister_spinner_output_runtime() {
  use_device_hid_class = true;
  for (uint8_t i = 0; i < output_usb_player_count(); ++i) {
    usb_device_hid[i] = new Adafruit_USBD_HID(spinnermister_report_descriptor, sizeof(spinnermister_report_descriptor), HID_ITF_PROTOCOL_NONE, 1, false);
  }

  // A distinct arcade identity avoids MiSTer's ReflexPSWheel quirk, which
  // routes steering to analog X and excludes the dedicated spinner channel.
  TinyUSBDevice.setID(RZORD1_VID, RZORD1_PID);
  TinyUSBDevice.setDeviceVersion(kMisterSpinnerDeviceVersion);
  TinyUSBDevice.setManufacturerDescriptor(RZORD1_VENDOR);
  TinyUSBDevice.setProductDescriptor(get_reflex_input_product_name());
  TinyUSBDevice.setSerialDescriptor(get_reflex_input_product_name());
}

static uint8_t configured_rotary_mouse_players = 0;

bool output_mouse_player_count_changed() {
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  return get_effective_output_mode() == OUTPUT_ROTARY_MOUSE &&
         configured_rotary_mouse_players != 0 &&
         configured_rotary_mouse_players != rotaryMouseOutputPlayerCount();
#else
  return false;
#endif
}

static void configure_rotary_mouse_output_runtime() {
  use_device_hid_class = true;
  configured_rotary_mouse_players = output_usb_player_count();
  for (uint8_t i = 0; i < configured_rotary_mouse_players; ++i) {
    usb_device_hid[i] = new Adafruit_USBD_HID(rotarymouse_report_descriptor, sizeof(rotarymouse_report_descriptor), HID_ITF_PROTOCOL_NONE, 1, false);
  }

  // Report protocol only: the boot mouse subclass would let a host drop the
  // report IDs this composite descriptor needs.
  TinyUSBDevice.setID(RZORD1_VID, RZORD1_PID);
  TinyUSBDevice.setDeviceVersion(kRotaryMouseDeviceVersion);
  TinyUSBDevice.setManufacturerDescriptor(RZORD1_VENDOR);
  TinyUSBDevice.setProductDescriptor(get_reflex_input_product_name());
  TinyUSBDevice.setSerialDescriptor(get_reflex_input_product_name());
}

static void configure_mister_negcon_output_runtime() {
  use_device_hid_class = true;
  for (uint8_t i = 0; i < output_usb_player_count(); ++i) {
    usb_device_hid[i] = new Adafruit_USBD_HID(negconmister_report_descriptor, sizeof(negconmister_report_descriptor), HID_ITF_PROTOCOL_NONE, 1, false);
  }

  TinyUSBDevice.setID(RZORD1_VID, RZORD1_MISTER_PID);
  TinyUSBDevice.setDeviceVersion(kMisterNegConDeviceVersion);
  TinyUSBDevice.setManufacturerDescriptor(RZORD1_VENDOR);
  TinyUSBDevice.setProductDescriptor(get_reflex_input_product_name());
  TinyUSBDevice.setSerialDescriptor(get_reflex_input_product_name());
}

static void configure_mister_guncon_output_runtime() {
  use_device_hid_class = true;
  for (uint8_t i = 0; i < output_usb_player_count(); ++i) {
    usb_device_hid[i] = new Adafruit_USBD_HID(gunconmister_report_descriptor, sizeof(gunconmister_report_descriptor), HID_ITF_PROTOCOL_NONE, 1, false);
  }

  TinyUSBDevice.setID(RZORD1_VID, RZORD1_MISTER_PID);
  TinyUSBDevice.setDeviceVersion(kMisterGunConDeviceVersion);
  TinyUSBDevice.setManufacturerDescriptor(RZORD1_VENDOR);
  TinyUSBDevice.setProductDescriptor(get_reflex_input_product_name());
  TinyUSBDevice.setSerialDescriptor(get_reflex_input_product_name());
}
#endif

static void configure_keyboard_output_runtime() {
  use_device_hid_class = true;

  TinyUSBDevice.setID(RZORD1_VID, RZORD1_PID);
  TinyUSBDevice.setDeviceVersion(RZORD1_VERSION);
  TinyUSBDevice.setManufacturerDescriptor(RZORD1_VENDOR);
  TinyUSBDevice.setProductDescriptor(PRODUCT_NAME);
  TinyUSBDevice.setSerialDescriptor(PRODUCT_NAME);

  for (uint8_t i = 0; i < MAX_USB_OUT; ++i) {
    usb_device_hid[i] = nullptr;
  }

  // One physical keyboard interface carries both MAME P1 and P2 key mappings.
  usb_device_hid[0] = new Adafruit_USBD_HID(
    keyboard_report_desc,
    sizeof(keyboard_report_desc),
    HID_ITF_PROTOCOL_KEYBOARD,
    1,
    false
  );
}

