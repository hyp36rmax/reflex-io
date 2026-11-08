#pragma once

// Internal USB control/runtime hooks for the USB device output stack.
// This header is included from out_usb.h after the XInput/XID objects and
// mapping helpers are available, so it can keep using the existing shared
// state without introducing another wide extern surface.

bool xinput_handle_ms_os_descriptor_request_c(
    uint8_t rhport,
    const tusb_control_request_t* request
);

#include "../xinput/output_xinput_capabilities_runtime.h"
#include "../playstation/output_ps3_native_control_runtime.h"

static const usbd_class_driver_t usbd_drivers_wired[] {
#if defined(ADAPT_FEATURE_NATIVE_PS3_OUTPUT)
  ps3_native_driver,
#endif
  // Claims its HID and vendor interfaces only in Switch 2 Pro output.
  *switch2pro_get_driver(),
  *xid_get_driver(),
  // TinyUSB snapshots the application-driver table before persisted output
  // mode restoration completes. Keep the receiver driver in the baseline
  // table so a boot directly into four-player XInput can still claim its
  // 0xFF/0x5D/0x81 interfaces after descriptors switch to that persona.
  *xinputw_get_driver(),
  *xinput_get_driver(),
#ifdef ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT
  // Persisted Xbox One mode is restored after TinyUSB snapshots this table.
  // Keep the narrowly matching FF/47/D0 GIP driver available from boot so it
  // can claim the interface once the Xbox One descriptor is selected.
  *xboxone_get_driver(),
#endif
};

static const usbd_class_driver_t usbd_drivers_wireless[] {
  *xid_get_driver(),
  *xinputw_get_driver(),
};

#ifdef ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT
static const usbd_class_driver_t usbd_drivers_xboxone[] {
  *xboxone_get_driver(),
};
#endif

bool adapt_usb_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, const void *request_void) {
  const tusb_control_request_t *request =
      static_cast<const tusb_control_request_t *>(request_void);
  const outputMode_t effectiveOutputMode = get_effective_output_mode();
  bool ret = false;
  if (can_run_usb_detection() && usb_detect_handle_vendor_control_xfer(rhport, stage, request)) {
    return true;
  }
  if (can_run_usb_detection() && _usb_detect_xid) {
    auto_detect_init_xid_interface(rhport);
    ret |= xid_get_driver()->control_xfer_cb(rhport, stage, request);
  }
  if (effectiveOutputMode == OUTPUT_XID) {
    if (_xid && tud_mounted()) {
      uint8_t ep_in = (_xid->stored_ep_in != 0) ? _xid->stored_ep_in : 0x81;
      uint8_t ep_out = (_xid->stored_ep_out != 0) ? _xid->stored_ep_out : 0x02;
      xid_manual_init(rhport, _xid->stored_itfnum, ep_in, ep_out);
    }
    ret |= xid_get_driver()->control_xfer_cb(rhport, stage, request);
  } else if (effectiveOutputMode == OUTPUT_XINPUT) {
    ret = _xinput_tud_vendor_control_xfer_cb(rhport, stage, request);
  } else if (effectiveOutputMode == OUTPUT_XINPUT2P) {
  #ifdef FORCE_XINPUT2P_SINGLE_DRIVER
    ret = _xinput_tud_vendor_control_xfer_cb(rhport, stage, request);
  #else
    ret = _xinputw_tud_vendor_control_xfer_cb(rhport, stage, request);
  #endif
  } else if (effectiveOutputMode == OUTPUT_XINPUTW) {
    ret = _xinputw_tud_vendor_control_xfer_cb(rhport, stage, request);
  } else if (effectiveOutputMode == OUTPUT_SWITCH2PRO) {
    ret = switch2pro_vendor_control_xfer_cb(rhport, stage, request);
  #ifdef ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT
  } else if (effectiveOutputMode == OUTPUT_XBOXONE) {
    ret = xboxone_vendor_control_xfer_cb(rhport, stage, request);
  #endif
  }
  return ret;
}

usbd_class_driver_t const* usbd_app_driver_get_cb(uint8_t* driver_count) {
  const outputMode_t effectiveOutputMode = get_effective_output_mode();
  #ifdef ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT
  if (effectiveOutputMode == OUTPUT_XBOXONE) {
    *driver_count = (sizeof(usbd_drivers_xboxone) / sizeof(usbd_class_driver_t));
    return usbd_drivers_xboxone;
  }
  #endif
  if (effectiveOutputMode == OUTPUT_XINPUT2P) {
  #ifdef FORCE_XINPUT2P_SINGLE_DRIVER
    *driver_count = (sizeof(usbd_drivers_wired) / sizeof(usbd_class_driver_t));
    return usbd_drivers_wired;
  #else
    *driver_count = (sizeof(usbd_drivers_wireless) / sizeof(usbd_class_driver_t));
    return usbd_drivers_wireless;
  #endif
  }

  if (effectiveOutputMode == OUTPUT_XINPUTW) {
    *driver_count = (sizeof(usbd_drivers_wireless) / sizeof(usbd_class_driver_t));
    return usbd_drivers_wireless;
  }

  *driver_count = (sizeof(usbd_drivers_wired) / sizeof(usbd_class_driver_t));
  return usbd_drivers_wired;
}

void tud_clear_feature_cb(uint8_t rhport, uint8_t ep_addr) {
  (void) rhport;
  usb_detect_handle_edpt_clear_stall(ep_addr);
}

bool tud_interface_control_xfer_cb(uint8_t rhport, uint8_t itf, uint8_t stage, tusb_control_request_t const * request) {
  (void) itf;

  if (can_run_usb_detection() && _usb_detect_xid) {
    auto_detect_init_xid_interface(rhport);
    if (xid_get_driver()->control_xfer_cb(rhport, stage, request)) {
      return true;
    }
  }

  if (get_effective_output_mode() == OUTPUT_XID) {
    if (_xid) {
      uint8_t ep_in = (_xid->stored_ep_in != 0) ? _xid->stored_ep_in : 0x81;
      uint8_t ep_out = (_xid->stored_ep_out != 0) ? _xid->stored_ep_out : 0x02;
      xid_manual_init(rhport, _xid->stored_itfnum, ep_in, ep_out);
    }
    return xid_get_driver()->control_xfer_cb(rhport, stage, request);
  }

  return false;
}
