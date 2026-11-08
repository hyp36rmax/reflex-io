#include "../../product_config.h"
#ifdef ADAPT_OUTPUT_USB_DEVICE

#include <Arduino.h>
#include <string.h>

#include "out_switch2pro.h"
#include "../output_runtime_state.h"

extern void rumble_callback(uint8_t index, uint8_t left, uint8_t right);

namespace {

Adafruit_USBD_Switch2Pro* g_switch2pro = nullptr;

constexpr uint8_t kHidDescriptorType = 0x21;
constexpr uint8_t kHidReportDescriptorType = 0x22;
constexpr uint8_t kHidGetReport = 0x01;
constexpr uint8_t kHidGetIdle = 0x02;
constexpr uint8_t kHidGetProtocol = 0x03;
constexpr uint8_t kHidSetReport = 0x09;
constexpr uint8_t kHidSetIdle = 0x0A;
constexpr uint8_t kHidSetProtocol = 0x0B;
// The console sends rumble about every 4 ms while an effect plays.
constexpr uint32_t kRumbleTimeoutMs = 250;

const uint8_t kHidDescriptor[9] = {
  0x09, kHidDescriptorType, 0x11, 0x01, 0x00, 0x01, kHidReportDescriptorType,
  switch2_pro::kHidReportDescriptorSize, 0x00,
};

// Persisted modes are restored after TinyUSB snapshots the driver table, so
// the driver is always registered and claims interfaces only in this mode.
Adafruit_USBD_Switch2Pro* activeDevice() {
  return get_effective_output_mode() == OUTPUT_SWITCH2PRO ? g_switch2pro : nullptr;
}

uint16_t openEndpoints(uint8_t rhport, const tusb_desc_interface_t* itf,
                       uint16_t maxLength, uint8_t* in, uint8_t* out) {
  const uint8_t* p = reinterpret_cast<const uint8_t*>(itf);
  const uint8_t* end = p + maxLength;
  uint16_t length = tu_desc_len(p);
  p = tu_desc_next(p);
  uint8_t found = 0;
  while (found < itf->bNumEndpoints && p < end) {
    const uint8_t type = tu_desc_type(p);
    if (type == TUSB_DESC_INTERFACE || type == TUSB_DESC_INTERFACE_ASSOCIATION) break;
    if (type == TUSB_DESC_ENDPOINT) {
      const auto* endpoint = reinterpret_cast<const tusb_desc_endpoint_t*>(p);
      if (!usbd_edpt_open(rhport, endpoint)) return 0;
      if (tu_edpt_dir(endpoint->bEndpointAddress) == TUSB_DIR_IN) {
        *in = endpoint->bEndpointAddress;
      } else {
        *out = endpoint->bEndpointAddress;
      }
      ++found;
    }
    length = static_cast<uint16_t>(length + tu_desc_len(p));
    p = tu_desc_next(p);
  }
  return found == itf->bNumEndpoints ? length : 0;
}

}  // namespace

Adafruit_USBD_Switch2Pro::Adafruit_USBD_Switch2Pro(const uint8_t boardId[8]) {
  switch2_pro::deriveIdentity(boardId, &identity_);
  responder_.reset(identity_);
}

uint16_t Adafruit_USBD_Switch2Pro::getInterfaceDescriptor(uint8_t itfnum,
                                                         uint8_t* buf,
                                                         uint16_t bufsize) {
  (void)itfnum;
  const uint16_t length = switch2_pro::kInterfaceDescriptorsSize;
  if (buf == nullptr) return length;
  if (bufsize < length) return 0;
  // Allocation order gives the captured endpoints: 0x01/0x81 HID, then
  // 0x02/0x82 bulk, when nothing is added before this interface.
  itf_base_ = TinyUSBDevice.allocInterface(2);
  const uint8_t hidOut = TinyUSBDevice.allocEndpoint(TUSB_DIR_OUT);
  const uint8_t hidIn = TinyUSBDevice.allocEndpoint(TUSB_DIR_IN);
  const uint8_t bulkOut = TinyUSBDevice.allocEndpoint(TUSB_DIR_OUT);
  const uint8_t bulkIn = TinyUSBDevice.allocEndpoint(TUSB_DIR_IN);
  switch2_pro::writeInterfaceDescriptors(buf, itf_base_, hidIn, hidOut, bulkOut, bulkIn);
  return length;
}

bool Adafruit_USBD_Switch2Pro::begin() {
  busReset();
  g_switch2pro = this;
  return TinyUSBDevice.addInterface(*this);
}

void Adafruit_USBD_Switch2Pro::busReset() {
  if (rumble_active_) rumble_callback(rumble_port_, 0, 0);
  rumble_active_ = false;
  hid_in_ = hid_out_ = bulk_in_ = bulk_out_ = 0;
  bulk_zlp_pending_ = false;
  assembler_.reset();
  // A reconnecting console repeats Initialise USB before it reads reports.
  responder_.reset(identity_);
}

uint16_t Adafruit_USBD_Switch2Pro::open(uint8_t rhport,
                                        const tusb_desc_interface_t* itf,
                                        uint16_t maxLength) {
  rhport_ = rhport;
  if (itf->bInterfaceNumber == itf_base_ &&
      itf->bInterfaceClass == TUSB_CLASS_HID) {
    const uint16_t length = openEndpoints(rhport, itf, maxLength, &hid_in_, &hid_out_);
    if (length != 0) armHidOut();
    return length;
  }
  if (itf->bInterfaceNumber == itf_base_ + 1 &&
      itf->bInterfaceClass == TUSB_CLASS_VENDOR_SPECIFIC &&
      itf->bInterfaceSubClass == 0x00 && itf->bInterfaceProtocol == 0x00) {
    const uint16_t length = openEndpoints(rhport, itf, maxLength, &bulk_in_, &bulk_out_);
    if (length != 0) armBulkOut();
    return length;
  }
  return 0;
}

bool Adafruit_USBD_Switch2Pro::controlTransfer(uint8_t rhport, uint8_t stage,
                                               const tusb_control_request_t* request) {
  if (request->bmRequestType_bit.recipient != TUSB_REQ_RCPT_INTERFACE ||
      tu_u16_low(request->wIndex) != itf_base_) {
    return false;  // The vendor interface takes no control requests.
  }
  const uint16_t limit = request->wLength < sizeof(control_buf_)
      ? request->wLength : static_cast<uint16_t>(sizeof(control_buf_));
  if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD) {
    if (request->bRequest != TUSB_REQ_GET_DESCRIPTOR) return false;
    if (stage != CONTROL_STAGE_SETUP) return true;
    const uint8_t type = tu_u16_high(request->wValue);
    if (type == kHidReportDescriptorType) {
      return tud_control_xfer(rhport, request,
                              const_cast<uint8_t*>(switch2_pro::kHidReportDescriptor),
                              switch2_pro::kHidReportDescriptorSize);
    }
    if (type == kHidDescriptorType) {
      return tud_control_xfer(rhport, request, const_cast<uint8_t*>(kHidDescriptor),
                              sizeof(kHidDescriptor));
    }
    return false;
  }
  if (request->bmRequestType_bit.type != TUSB_REQ_TYPE_CLASS) return false;
  switch (request->bRequest) {
    case kHidGetReport:
      if (stage == CONTROL_STAGE_SETUP) {
        memcpy(control_buf_, hid_in_buf_, sizeof(control_buf_));
        return tud_control_xfer(rhport, request, control_buf_, limit);
      }
      return true;
    case kHidSetReport:
      if (stage == CONTROL_STAGE_SETUP) {
        return tud_control_xfer(rhport, request, control_buf_, limit);
      }
      if (stage == CONTROL_STAGE_DATA) {
        uint8_t left = 0, right = 0;
        if (switch2_pro::decodeRumble(control_buf_, limit, &left, &right)) {
          rumble_active_ = (left | right) != 0;
          last_rumble_ms_ = millis();
          rumble_callback(rumble_port_, left, right);
        }
      }
      return true;
    case kHidGetIdle:
    case kHidGetProtocol:
      if (stage == CONTROL_STAGE_SETUP) {
        // Idle rate 0; report protocol.
        control_buf_[0] = request->bRequest == kHidGetProtocol ? 1 : 0;
        return tud_control_xfer(rhport, request, control_buf_, 1);
      }
      return true;
    case kHidSetIdle:
    case kHidSetProtocol:
      if (stage == CONTROL_STAGE_SETUP) return tud_control_status(rhport, request);
      return true;
    default:
      return false;
  }
}

bool Adafruit_USBD_Switch2Pro::vendorControlTransfer(uint8_t rhport, uint8_t stage,
                                                     const tusb_control_request_t* request) {
  if (request->bmRequestType_bit.type != TUSB_REQ_TYPE_VENDOR ||
      request->bmRequestType_bit.recipient != TUSB_REQ_RCPT_DEVICE) {
    return false;
  }
  if (stage != CONTROL_STAGE_SETUP) return true;
  if (request->bmRequestType_bit.direction == TUSB_DIR_IN) {
    const uint16_t limit = request->wLength < sizeof(control_buf_)
        ? request->wLength : static_cast<uint16_t>(sizeof(control_buf_));
    const int16_t length = responder_.vendorRequestIn(request->bRequest, control_buf_, limit);
    if (length < 0) return false;
    return tud_control_xfer(rhport, request, control_buf_, static_cast<uint16_t>(length));
  }
  if (!responder_.vendorRequestOut(request->bRequest)) return false;
  return tud_control_status(rhport, request);
}

void Adafruit_USBD_Switch2Pro::armHidOut() {
  if (hid_out_ != 0 && !usbd_edpt_busy(rhport_, hid_out_)) {
    usbd_edpt_xfer(rhport_, hid_out_, hid_out_buf_, sizeof(hid_out_buf_));
  }
}

void Adafruit_USBD_Switch2Pro::armBulkOut() {
  if (bulk_out_ != 0 && !usbd_edpt_busy(rhport_, bulk_out_)) {
    usbd_edpt_xfer(rhport_, bulk_out_, bulk_out_buf_, sizeof(bulk_out_buf_));
  }
}

void Adafruit_USBD_Switch2Pro::handleBulkPacket(uint32_t length) {
  // Each transfer is one packet, so a command longer than 64 bytes arrives
  // in pieces; the assembler reads its length from the header.
  const uint16_t commandLength = assembler_.feed(
      bulk_out_buf_, length > 0xFFFFu ? 0xFFFFu : static_cast<uint16_t>(length));
  if (commandLength == 0) {
    armBulkOut();
    return;
  }
  const uint16_t responseLength = responder_.handleCommand(
      assembler_.command(), commandLength, response_, sizeof(response_));
  assembler_.reset();
  if (responseLength == 0) {
    armBulkOut();
    return;
  }
  // One response per command; the next command is read after it is sent.
  bulk_zlp_pending_ = (responseLength % switch2_pro::kPacketSize) == 0;
  if (!usbd_edpt_xfer(rhport_, bulk_in_, response_, responseLength)) {
    bulk_zlp_pending_ = false;
    armBulkOut();
  }
}

bool Adafruit_USBD_Switch2Pro::transferComplete(uint8_t rhport, uint8_t endpoint,
                                                xfer_result_t result, uint32_t length) {
  (void)rhport;
  const bool ok = result == XFER_RESULT_SUCCESS;
  if (endpoint == hid_in_) return true;
  if (endpoint == hid_out_) {
    uint8_t left = 0, right = 0;
    if (ok && switch2_pro::decodeRumble(hid_out_buf_, static_cast<uint16_t>(length),
                                        &left, &right)) {
      rumble_active_ = (left | right) != 0;
      last_rumble_ms_ = millis();
      rumble_callback(rumble_port_, left, right);
    }
    armHidOut();
    return true;
  }
  if (endpoint == bulk_out_) {
    if (ok) {
      handleBulkPacket(length);
    } else {
      assembler_.reset();
      armBulkOut();
    }
    return true;
  }
  if (endpoint == bulk_in_) {
    if (bulk_zlp_pending_) {
      bulk_zlp_pending_ = false;
      if (usbd_edpt_xfer(rhport_, bulk_in_, nullptr, 0)) return true;
    }
    armBulkOut();
    return true;
  }
  return false;
}

bool Adafruit_USBD_Switch2Pro::inputReady() const {
  return hid_in_ != 0 && tud_ready() && responder_.reportsEnabled() &&
         !usbd_edpt_busy(rhport_, hid_in_);
}

bool Adafruit_USBD_Switch2Pro::sendInputReport(uint8_t sourcePort, uint32_t buttons,
                                               const uint8_t leftStick[3],
                                               const uint8_t rightStick[3]) {
  if (!inputReady() || !usbd_edpt_claim(rhport_, hid_in_)) return false;
  rumble_port_ = sourcePort;
  responder_.packInputReport(hid_in_buf_, buttons, leftStick, rightStick);
  const bool sent = usbd_edpt_xfer(rhport_, hid_in_, hid_in_buf_, sizeof(hid_in_buf_));
  usbd_edpt_release(rhport_, hid_in_);
  return sent;
}

void Adafruit_USBD_Switch2Pro::task() {
  if (rumble_active_ && uint32_t(millis() - last_rumble_ms_) > kRumbleTimeoutMs) {
    rumble_active_ = false;
    rumble_callback(rumble_port_, 0, 0);
  }
}

bool switch2pro_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage,
                                       const tusb_control_request_t* request) {
  Adafruit_USBD_Switch2Pro* device = activeDevice();
  return device != nullptr && device->vendorControlTransfer(rhport, stage, request);
}

extern "C" {

static void switch2pro_init(void) {}

static void switch2pro_reset(uint8_t rhport) {
  (void)rhport;
  if (g_switch2pro) g_switch2pro->busReset();
}

static uint16_t switch2pro_open(uint8_t rhport, const tusb_desc_interface_t* itf,
                                uint16_t maxLength) {
  Adafruit_USBD_Switch2Pro* device = activeDevice();
  return device ? device->open(rhport, itf, maxLength) : 0;
}

static bool switch2pro_control_xfer(uint8_t rhport, uint8_t stage,
                                    const tusb_control_request_t* request) {
  Adafruit_USBD_Switch2Pro* device = activeDevice();
  return device != nullptr && device->controlTransfer(rhport, stage, request);
}

static bool switch2pro_xfer(uint8_t rhport, uint8_t endpoint, xfer_result_t result,
                            uint32_t length) {
  Adafruit_USBD_Switch2Pro* device = activeDevice();
  return device != nullptr && device->transferComplete(rhport, endpoint, result, length);
}

static const usbd_class_driver_t switch2pro_driver = {
#if CFG_TUSB_DEBUG >= 2
    .name = "SWITCH2PRO",
#endif
    .init = switch2pro_init,
    .reset = switch2pro_reset,
    .open = switch2pro_open,
    .control_xfer_cb = switch2pro_control_xfer,
    .xfer_cb = switch2pro_xfer,
    .sof = NULL,
};

const usbd_class_driver_t* switch2pro_get_driver() {
  return &switch2pro_driver;
}

}  // extern "C"

#endif  // ADAPT_OUTPUT_USB_DEVICE
