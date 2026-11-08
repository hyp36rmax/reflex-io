#pragma once

#if defined(ADAPT_FEATURE_NATIVE_PS3_OUTPUT)
// The built-in HID driver still owns its endpoints, descriptors and input
// reports. Only native feature control transfers bypass its Report-ID framing.
alignas(4) static uint8_t ps3_native_control_buffer[64];

static void ps3NativeDriverInit() {}

static void ps3NativeDriverReset(uint8_t) {
  ps3_native_usb.clearSession();
  rumbleRuntimeClearAllHostFeedback();
}

static uint16_t ps3NativeDriverOpen(uint8_t rhport,
                                   const tusb_desc_interface_t* descriptor,
                                   uint16_t max_length) {
  if (!ps3NativeRuntimeActive() || descriptor->bInterfaceNumber != 0 ||
      descriptor->bInterfaceClass != TUSB_CLASS_HID ||
      descriptor->bInterfaceSubClass != 0 || descriptor->bInterfaceProtocol != 0 ||
      descriptor->bNumEndpoints != 2) return 0;
  return hidd_open(rhport, descriptor, max_length);
}

static bool ps3NativeControlTransfer(uint8_t rhport, uint8_t stage,
                                     const tusb_control_request_t* request) {
  if (!ps3NativeRuntimeActive()) return false;
  if (request->wIndex != 0) return false;
  if (request->bmRequestType_bit.type != TUSB_REQ_TYPE_CLASS ||
      request->bmRequestType_bit.recipient != TUSB_REQ_RCPT_INTERFACE ||
      tu_u16_high(request->wValue) != HID_REPORT_TYPE_FEATURE) {
    return hidd_control_xfer_cb(rhport, stage, request);
  }

  const uint8_t id = tu_u16_low(request->wValue);
  if (request->bmRequestType == 0xa1 &&
      request->bRequest == HID_REQ_CONTROL_GET_REPORT) {
    if (stage != CONTROL_STAGE_SETUP) return true;
    const uint16_t length = ps3_native_usb.getFeature(
        id, ps3_native_control_buffer, request->wLength);
    return length != 0 && tud_control_xfer(
        rhport, request, ps3_native_control_buffer, length);
  }

  if (request->bmRequestType == 0x21 &&
      request->bRequest == HID_REQ_CONTROL_SET_REPORT &&
      (id == 0xf5 || id == 0xef)) {
    if (request->wLength > sizeof(ps3_native_control_buffer) ||
        (id == 0xf5 ? request->wLength < 8 : request->wLength != 48)) return false;
    if (stage == CONTROL_STAGE_SETUP) {
      return tud_control_xfer(rhport, request, ps3_native_control_buffer,
                              request->wLength);
    }
    if (stage == CONTROL_STAGE_ACK) {
      return id == 0xf5
          ? ps3_native_usb.setHostAddress(id, HID_REPORT_TYPE_FEATURE,
                ps3_native_control_buffer, request->wLength)
          : ps3_native_usb.setSensorSelector(ps3_native_control_buffer,
                request->wLength);
    }
    return true;
  }

  // Retain HID's no-op acknowledgment of other native feature writes (F4
  // included). The native callback prevents them reaching Auto or WebHID.
  return hidd_control_xfer_cb(rhport, stage, request);
}

static const usbd_class_driver_t ps3_native_driver = {
  .name = "PS3 native",
  .init = ps3NativeDriverInit,
  .deinit = nullptr,
  .reset = ps3NativeDriverReset,
  .open = ps3NativeDriverOpen,
  .control_xfer_cb = ps3NativeControlTransfer,
  .xfer_cb = hidd_xfer_cb,
  .xfer_isr = nullptr,
  .sof = nullptr,
};
#endif
