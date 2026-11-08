#include "switch2_pro_protocol.h"

#include <string.h>

#include "switch2_aes128.h"

namespace switch2_pro {

const uint8_t kHidReportDescriptor[kHidReportDescriptorSize] = {
  0x05, 0x01, 0x09, 0x05, 0xA1, 0x01,
  0x85, 0x05, 0x05, 0xFF, 0x09, 0x01, 0x15, 0x00, 0x26, 0xFF, 0x00,
  0x95, 0x3F, 0x75, 0x08, 0x81, 0x02,
  0x85, 0x09, 0x09, 0x01, 0x95, 0x02, 0x81, 0x02,
  0x05, 0x09, 0x19, 0x01, 0x29, 0x15, 0x25, 0x01, 0x95, 0x15, 0x75, 0x01,
  0x81, 0x02,
  0x95, 0x01, 0x75, 0x03, 0x81, 0x03,
  0x05, 0x01, 0x09, 0x01, 0xA1, 0x00, 0x09, 0x30, 0x09, 0x31, 0x09, 0x33,
  0x09, 0x35, 0x26, 0xFF, 0x0F, 0x95, 0x04, 0x75, 0x0C, 0x81, 0x02, 0xC0,
  0x05, 0xFF, 0x09, 0x02, 0x26, 0xFF, 0x00, 0x95, 0x34, 0x75, 0x08, 0x81, 0x02,
  0x85, 0x02, 0x09, 0x01, 0x95, 0x3F, 0x91, 0x02,
  0xC0,
};

namespace {

constexpr uint8_t kDirectionResponse = 0x01;
// NFC replies without a tag operation use this direction byte, as genuine
// controllers do for some bare acknowledgements (PicoSwitch2 captures).
constexpr uint8_t kDirectionNfcAck = 0x04;
constexpr uint8_t kHeaderSize = 8;
constexpr uint8_t kMaxFlashRead = 0x50;

// The console offers a controller update unless the controller, Bluetooth
// and DSP versions are exactly the current retail ones: 1.1.5 (this
// capture's firmware) and 255.255.255 both prompt. 2.1.4 / 12.0.0 / 0.2.3 is
// a genuine controller's reply that cleared the prompt on a console
// (PicoSwitch2, 2026-07-21). A later console update may need a newer tuple.
constexpr uint8_t kFirmwareVersion[3] = {0x02, 0x01, 0x04};
constexpr uint8_t kBluetoothVersion[3] = {0x0c, 0x00, 0x00};
constexpr uint8_t kDspVersion[3] = {0x00, 0x02, 0x03};
constexpr uint8_t kControllerTypePro = 0x02;

// Domain-separation key for deriveIdentity(); not a secret.
constexpr uint8_t kIdentityKey[16] = {
  'R', 'e', 'f', 'l', 'e', 'x', ' ', 'S', 'w', 'i', 't', 'c', 'h', ' ', '2', 0,
};

// Factory stick parameters at 0x13080/0x130C0 (both sticks), copied from the
// captured Pro Controller 2. They are model data, the same for both sticks.
constexpr uint8_t kStickParameters[0x28] = {
  0x01, 0xad, 0xd9, 0x9a, 0x55, 0x56, 0x65, 0xa0, 0x00, 0x0a, 0xa0, 0x00,
  0x0a, 0xe2, 0x20, 0x0e, 0xe2, 0x20, 0x0e, 0x9a, 0xad, 0xd9, 0x9a, 0xad,
  0xd9, 0x0a, 0xa5, 0x50, 0x0a, 0xa5, 0x50, 0x2f, 0xf6, 0x62, 0x2f, 0xf6,
  0x62, 0x0a, 0xff, 0xff,
};
// Center 2048 and +/-2048 on both axes, the Switch Pro persona's full range.
constexpr uint8_t kStickCalibration[9] = {
  0x00, 0x08, 0x80, 0x00, 0x08, 0x80, 0x00, 0x08, 0x80,
};
// Motion scale constants (0x11/0x03) from the captured Pro Controller 2.
constexpr uint8_t kMotionScale[29] = {
  0x01, 0xc0, 0x03, 0x00, 0x00, 0xe7, 0xd0, 0x1c, 0x3b, 0x79, 0x22, 0xa0,
  0x3a, 0x0a, 0xe8, 0x9c, 0x42, 0x58, 0xa0, 0x0b, 0x42, 0x0a, 0xe8, 0x9c,
  0x41, 0x58, 0xa0, 0x0b, 0x41,
};
// Default black Pro Controller 2: body, buttons, highlight, grip.
constexpr uint8_t kColours[12] = {
  0x23, 0x23, 0x23, 0xa0, 0xa0, 0xa0, 0xe6, 0xe6, 0xe6, 0x32, 0x32, 0x32,
};

// Report 0x09 byte 2: external power, not charging, battery level 9 of 9.
// (The captured wired controller sent 0x23: charging, level 8.)
constexpr uint8_t kPowerInfo = 0x25;
constexpr uint8_t kFeatureRumble = 0x20;

// Full-scale HD rumble amplitude: SDL's maximum (29000) as the 10-bit
// amplitude fields carry it, so SDL's strongest effect decodes to 255.
constexpr uint32_t kRumbleFullScale = 29000u & 0xFFC0u;

uint32_t readLe32(const uint8_t* data) {
  return static_cast<uint32_t>(data[0]) |
         (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) |
         (static_cast<uint32_t>(data[3]) << 24);
}

void writeLe32(uint8_t* out, uint32_t value) {
  out[0] = static_cast<uint8_t>(value);
  out[1] = static_cast<uint8_t>(value >> 8);
  out[2] = static_cast<uint8_t>(value >> 16);
  out[3] = static_cast<uint8_t>(value >> 24);
}

void identityBlock(const uint8_t boardId[8], uint8_t purpose, uint8_t out[16]) {
  uint8_t block[16] = {};
  memcpy(block, boardId, 8);
  block[8] = purpose;
  switch2_aes128::encryptBlock(kIdentityKey, block, out);
}

void reversed16(const uint8_t in[16], uint8_t out[16]) {
  for (uint8_t i = 0; i < 16; ++i) out[i] = in[15 - i];
}

// A region of the emulated flash; unlisted addresses read as erased (0xFF).
struct FlashRegion {
  uint32_t address;
  uint8_t length;
  const uint8_t* data;
};

uint8_t sampleStrength(const uint8_t sample[5]) {
  const uint32_t high = (static_cast<uint32_t>(sample[2] & 0x0Fu) << 12) |
                        (static_cast<uint32_t>(sample[1] & 0xFCu) << 4);
  const uint32_t low = (static_cast<uint32_t>(sample[4]) << 8) |
                       (sample[3] & 0xC0u);
  const uint32_t amplitude = high > low ? high : low;
  if (amplitude >= kRumbleFullScale) return 255;
  return static_cast<uint8_t>((amplitude * 255u) / kRumbleFullScale);
}

}  // namespace

void writeInterfaceDescriptors(uint8_t out[kInterfaceDescriptorsSize],
                               uint8_t firstInterface, uint8_t hidIn,
                               uint8_t hidOut, uint8_t bulkOut, uint8_t bulkIn) {
  const uint8_t vendorInterface = static_cast<uint8_t>(firstInterface + 1);
  const uint8_t descriptors[kInterfaceDescriptorsSize] = {
    0x08, 0x0B, firstInterface, 0x01, 0x03, 0x00, 0x00, 0x00,
    0x09, 0x04, firstInterface, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00,
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, kHidReportDescriptorSize, 0x00,
    0x07, 0x05, hidIn, 0x03, kPacketSize, 0x00, 0x04,
    0x07, 0x05, hidOut, 0x03, kPacketSize, 0x00, 0x04,
    0x08, 0x0B, vendorInterface, 0x01, 0xFF, 0x00, 0x00, 0x00,
    0x09, 0x04, vendorInterface, 0x00, 0x02, 0xFF, 0x00, 0x00, 0x00,
    0x07, 0x05, bulkOut, 0x02, kPacketSize, 0x00, 0x00,
    0x07, 0x05, bulkIn, 0x02, kPacketSize, 0x00, 0x00,
  };
  memcpy(out, descriptors, sizeof(descriptors));
}

uint32_t buttonsFromSwitchProBytes(const uint8_t b[3]) {
  uint32_t out = 0;
  if (b[0] & 0x01) out |= kButtonY;
  if (b[0] & 0x02) out |= kButtonX;
  if (b[0] & 0x04) out |= kButtonB;
  if (b[0] & 0x08) out |= kButtonA;
  if (b[0] & 0x40) out |= kButtonR;
  if (b[0] & 0x80) out |= kButtonZR;
  if (b[1] & 0x01) out |= kButtonMinus;
  if (b[1] & 0x02) out |= kButtonPlus;
  if (b[1] & 0x04) out |= kButtonRStick;
  if (b[1] & 0x08) out |= kButtonLStick;
  if (b[1] & 0x10) out |= kButtonHome;
  if (b[1] & 0x20) out |= kButtonCapture;
  if (b[2] & 0x01) out |= kButtonDown;
  if (b[2] & 0x02) out |= kButtonUp;
  if (b[2] & 0x04) out |= kButtonRight;
  if (b[2] & 0x08) out |= kButtonLeft;
  if (b[2] & 0x40) out |= kButtonL;
  if (b[2] & 0x80) out |= kButtonZL;
  return out;
}

uint32_t buttonsFromNeutralExtras(uint16_t extra) {
  uint32_t out = 0;
  if (extra & 0x02) out |= kButtonC;
  if (extra & 0x04) out |= kButtonGL;
  if (extra & 0x08) out |= kButtonGR;
  return out;
}

void deriveIdentity(const uint8_t boardId[8], Identity* identity) {
  uint8_t block[16];
  identityBlock(boardId, 1, block);
  memcpy(identity->bt_address, block, sizeof(identity->bt_address));
  // Wire order is reversed: byte 5 is the first octet. Locally administered,
  // unicast, so it cannot collide with a Nintendo-assigned address.
  identity->bt_address[5] =
      static_cast<uint8_t>((identity->bt_address[5] | 0x02u) & 0xFEu);

  identityBlock(boardId, 3, block);
  memset(identity->serial, 0, sizeof(identity->serial));
  memcpy(identity->serial, "HEW9", 4);
  for (uint8_t i = 0; i < 10; ++i) {
    identity->serial[4 + i] = static_cast<char>('0' + (block[i] % 10u));
  }
}

const uint8_t kPairingDeviceKey[16] = {
  0x5c, 0xf6, 0xee, 0x79, 0x2c, 0xdf, 0x05, 0xe1,
  0xba, 0x2b, 0x63, 0x25, 0xc4, 0x1a, 0x5f, 0x10,
};

void pairingChallengeResponse(const uint8_t ltkWire[16],
                              const uint8_t challengeWire[16],
                              uint8_t responseWire[16]) {
  uint8_t key[16];
  uint8_t challenge[16];
  reversed16(ltkWire, key);
  reversed16(challengeWire, challenge);
  switch2_aes128::encryptBlock(key, challenge, responseWire);
}

void Responder::reset(const Identity& identity) {
  identity_ = identity;
  memset(session_ltk_, 0, sizeof(session_ltk_));
  diagnostics_ = Diagnostics{};
  usb_initialised_ = false;
  input_report_ = kInputReportId;
  feature_mask_ = 0;
  features_enabled_ = 0;
  player_leds_ = 0;
  report_counter_ = 0;
}

void Responder::readFlash(uint32_t address, uint8_t* out, uint16_t length) const {
  uint8_t factory[0x40];
  memset(factory, 0xFF, sizeof(factory));
  factory[0] = 0x01;
  factory[1] = 0x00;
  memcpy(factory + 0x02, identity_.serial, sizeof(identity_.serial));
  factory[0x12] = static_cast<uint8_t>(kVendorId);
  factory[0x13] = static_cast<uint8_t>(kVendorId >> 8);
  factory[0x14] = static_cast<uint8_t>(kProductId);
  factory[0x15] = static_cast<uint8_t>(kProductId >> 8);
  factory[0x16] = 0x01;
  factory[0x17] = 0x06;
  factory[0x18] = 0x01;
  memcpy(factory + 0x19, kColours, sizeof(kColours));
  // No stored pairings; the updated-firmware marker an updated controller
  // shows at 0x1FD010.
  static const uint8_t kZero4[4] = {};

  // Gyro block: 25.0 C reference temperature, zero bias.
  static const uint8_t kGyro[0x10] = {0x00, 0x00, 0xc8, 0x41};
  // Accelerometer block: level at rest, 9.80665 m/s^2 on Z.
  static const uint8_t kAccel[0x18] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0x0a, 0xe8, 0x1c, 0x41,
  };

  const FlashRegion regions[] = {
    {0x13000, sizeof(factory), factory},
    {0x13040, sizeof(kGyro), kGyro},
    {0x13080, sizeof(kStickParameters), kStickParameters},
    {0x130A8, sizeof(kStickCalibration), kStickCalibration},
    {0x130C0, sizeof(kStickParameters), kStickParameters},
    {0x130E8, sizeof(kStickCalibration), kStickCalibration},
    {0x13100, sizeof(kAccel), kAccel},
    {0x1FA000, 1, kZero4},
    {0x1FD010, sizeof(kZero4), kZero4},
  };

  for (uint16_t i = 0; i < length; ++i) {
    const uint32_t at = address + i;
    uint8_t value = 0xFF;
    for (const FlashRegion& region : regions) {
      if (at >= region.address && at < region.address + region.length) {
        value = region.data[at - region.address];
        break;
      }
    }
    out[i] = value;
  }
}

uint16_t Responder::ack(const uint8_t* request, uint8_t* response,
                        uint16_t capacity, const uint8_t* data,
                        uint16_t dataLength, uint8_t direction) {
  const uint16_t total = static_cast<uint16_t>(kHeaderSize + dataLength);
  if (capacity < total) return 0;
  response[0] = request[0];
  response[1] = direction;
  response[2] = request[2];
  response[3] = request[3];
  response[4] = 0x00;
  response[5] = 0xF8;
  response[6] = 0x00;
  response[7] = 0x00;
  if (dataLength != 0 && data != response + kHeaderSize) {
    memcpy(response + kHeaderSize, data, dataLength);
  }
  return total;
}

uint16_t Responder::handleCommand(const uint8_t* request, uint16_t length,
                                  uint8_t* response, uint16_t capacity) {
  if (request == nullptr || response == nullptr || length < kHeaderSize ||
      request[1] != 0x91) {
    return 0;
  }
  diagnostics_.commands++;
  const uint8_t command = request[0];
  const uint8_t sub = request[3];
  const uint8_t* data = request + kHeaderSize;
  const uint16_t dataLength = static_cast<uint16_t>(length - kHeaderSize);
  uint8_t* out = response + kHeaderSize;
  const uint16_t outCapacity =
      capacity > kHeaderSize ? static_cast<uint16_t>(capacity - kHeaderSize) : 0;

  switch (command) {
    case 0x01:  // NFC: report the reader present; no tag is ever detected.
      if (sub == 0x0C) {
        static const uint8_t kNfcInfo[4] = {0x61, 0x12, 0x50, 0x10};
        return ack(request, response, capacity, kNfcInfo, sizeof(kNfcInfo));
      }
      return ack(request, response, capacity, nullptr, 0, kDirectionNfcAck);

    case 0x02: {  // Flash memory
      if (dataLength < 8 || outCapacity < 8) break;
      const uint32_t address = readLe32(data + 4);
      if (sub == 0x01 || sub == 0x04) {
        uint8_t readLength = sub == 0x01 ? 0x40 : data[0];
        if (readLength > kMaxFlashRead) readLength = kMaxFlashRead;
        if (outCapacity < static_cast<uint16_t>(8 + readLength)) break;
        memset(out, 0, 8);
        out[0] = readLength;
        writeLe32(out + 4, address);
        readFlash(address, out + 8, readLength);
        return ack(request, response, capacity, out,
                   static_cast<uint16_t>(8 + readLength));
      }
      // Writes and erases are acknowledged but not stored.
      memset(out, 0, 8);
      if (sub == 0x03) return ack(request, response, capacity, out, 4);
      writeLe32(out + 4, address);
      return ack(request, response, capacity, out, 8);
    }

    case 0x03: {  // Initialisation
      static const uint8_t kEnabled[4] = {0x01, 0x00, 0x00, 0x00};
      switch (sub) {
        case 0x03:
          return ack(request, response, capacity, kEnabled, sizeof(kEnabled));
        case 0x0A:
          if (dataLength >= 1 && (data[0] == 0x05 || data[0] == kInputReportId)) {
            input_report_ = data[0];
          }
          return ack(request, response, capacity, nullptr, 0);
        case 0x0D:
          usb_initialised_ = true;
          return ack(request, response, capacity, kEnabled, sizeof(kEnabled));
        case 0x0F: {
          static const uint8_t kUnknown0F[4] = {0x05, 0x00, 0x00, 0x00};
          return ack(request, response, capacity, kUnknown0F, sizeof(kUnknown0F));
        }
        default:
          return ack(request, response, capacity, nullptr, 0);
      }
    }

    case 0x06:  // Shutdown/reboot: nothing to power down.
    case 0x14:
    case 0x17:
      return ack(request, response, capacity, nullptr, 0);

    case 0x07: {
      static const uint8_t kZero = 0x00;
      return ack(request, response, capacity, &kZero, sub == 0x01 ? 1 : 0);
    }

    case 0x08: {  // Charging grip: none attached.
      if (sub == 0x01 || sub == 0x03) {
        const uint16_t size = sub == 0x01 ? 0x20 : 0x40;
        if (outCapacity < 4 + size) break;
        memset(out, 0, 4 + size);
        return ack(request, response, capacity, out, static_cast<uint16_t>(4 + size));
      }
      return ack(request, response, capacity, nullptr, 0);
    }

    case 0x09:  // Player LEDs
      if (sub >= 0x01 && sub <= 0x04) player_leds_ = static_cast<uint8_t>(1u << (sub - 1));
      if (sub == 0x05) player_leds_ = 0x0F;
      if (sub == 0x06) player_leds_ = 0x00;
      if (sub == 0x07 && dataLength >= 1) player_leds_ = data[0] & 0x0F;
      return ack(request, response, capacity, nullptr, 0);

    case 0x0A:  // Vibration samples and settings
      return ack(request, response, capacity, nullptr, 0);

    case 0x0B: {  // Battery: report a full cell on external power.
      static const uint8_t kVoltage[4] = {0xa5, 0x0e, 0x00, 0x00};
      static const uint8_t kCharge[4] = {0x34, 0x00, 0x83, 0x00};
      static const uint8_t kUnknown06[4] = {0x11, 0x00, 0x00, 0x00};
      if (sub == 0x03) return ack(request, response, capacity, kVoltage, 4);
      if (sub == 0x04) return ack(request, response, capacity, kCharge, 4);
      if (sub == 0x06) return ack(request, response, capacity, kUnknown06, 4);
      return ack(request, response, capacity, nullptr, 0);
    }

    case 0x0C: {  // Feature select
      if (outCapacity < 8 + 0x20) break;
      const uint8_t flags = dataLength >= 1 ? data[0] : 0;
      memset(out, 0, 8 + 0x20);
      switch (sub) {
        case 0x01:
          out[4] = (flags & 0x01) ? 0x07 : 0x00;
          out[5] = (flags & 0x02) ? 0x07 : 0x00;
          out[6] = (flags & 0x04) ? 0x01 : 0x00;
          out[7] = (flags & 0x80) ? 0x01 : 0x00;
          out[8] = (flags & 0x10) ? 0x01 : 0x00;
          out[9] = (flags & 0x20) ? 0x03 : 0x00;
          return ack(request, response, capacity, out, 12);
        case 0x02: feature_mask_ = flags; break;
        case 0x03: feature_mask_ = 0; break;
        case 0x04: features_enabled_ |= flags; break;
        case 0x05: features_enabled_ &= static_cast<uint8_t>(~flags); break;
        case 0x06:
          // Sensor register access; there is no IMU, so reads return zero.
          out[4] = dataLength >= 5 ? data[4] : 0;
          return ack(request, response, capacity, out, 8 + 0x20);
        default:
          return ack(request, response, capacity, nullptr, 0);
      }
      return ack(request, response, capacity, out, 4);
    }

    case 0x0D:
      // Firmware update: acknowledged without writing anything, so an update
      // the console starts aborts and the controller keeps working.
      diagnostics_.firmware_update++;
      return ack(request, response, capacity, nullptr, 0);

    case 0x10:
      if (sub == 0x01) {
        const uint8_t info[12] = {
          kFirmwareVersion[0], kFirmwareVersion[1], kFirmwareVersion[2],
          kControllerTypePro,
          kBluetoothVersion[0], kBluetoothVersion[1], kBluetoothVersion[2], 0x00,
          kDspVersion[0], kDspVersion[1], kDspVersion[2], 0x00,
        };
        return ack(request, response, capacity, info, sizeof(info));
      }
      break;

    case 0x11:
      if (sub == 0x01) {
        static const uint8_t kUnknown11[4] = {0x03, 0x00, 0x00, 0x00};
        return ack(request, response, capacity, kUnknown11, sizeof(kUnknown11));
      }
      if (sub == 0x03) {
        return ack(request, response, capacity, kMotionScale, sizeof(kMotionScale));
      }
      return ack(request, response, capacity, nullptr, 0);

    case 0x15: {  // Pairing over the cable
      if (outCapacity < 17) break;
      out[0] = 0x01;
      switch (sub) {
        case 0x01:
          out[1] = 0x04;
          out[2] = 0x01;
          memcpy(out + 3, identity_.bt_address, sizeof(identity_.bt_address));
          return ack(request, response, capacity, out, 9);
        case 0x02:
          if (dataLength < 17) break;
          pairingChallengeResponse(session_ltk_, data + 1, out + 1);
          return ack(request, response, capacity, out, 17);
        case 0x03:
          diagnostics_.paired = true;
          return ack(request, response, capacity, out, 1);
        case 0x04:
          // Genuine controllers always answer with the same device key; the
          // console derives the LTK as host key ^ device key.
          if (dataLength < 17) break;
          for (uint8_t i = 0; i < 16; ++i) {
            session_ltk_[i] = static_cast<uint8_t>(data[1 + i] ^ kPairingDeviceKey[i]);
          }
          memcpy(out + 1, kPairingDeviceKey, sizeof(kPairingDeviceKey));
          return ack(request, response, capacity, out, 17);
        default:
          break;
      }
      break;
    }

    case 0x16:
      if (sub == 0x01 && outCapacity >= 0x18) {
        memset(out, 0, 0x18);
        return ack(request, response, capacity, out, 0x18);
      }
      break;

    case 0x18:
      if (sub == 0x01) {
        static const uint8_t kUnknown18[8] = {0x00, 0x00, 0x40, 0xf0,
                                              0x00, 0x00, 0x60, 0x00};
        return ack(request, response, capacity, kUnknown18, sizeof(kUnknown18));
      }
      if (sub == 0x03 && dataLength >= 1) {
        return ack(request, response, capacity, data, 1);
      }
      break;

    default:
      break;
  }
  // Every command gets a reply: a dropped one stalls the console's start-up.
  diagnostics_.unhandled++;
  diagnostics_.last_unhandled_command = command;
  diagnostics_.last_unhandled_subcommand = sub;
  return ack(request, response, capacity, nullptr, 0);
}

int16_t Responder::vendorRequestIn(uint8_t request, uint8_t* out,
                                   uint16_t capacity) const {
  if (out == nullptr) return -1;
  if (request == 0x03) {
    // Factory data, the first 64 bytes of 0x13000.
    const uint16_t length = capacity < 0x40 ? capacity : 0x40;
    readFlash(0x13000, out, length);
    return static_cast<int16_t>(length);
  }
  if (request == 0x02) {
    uint8_t info[16] = {kFirmwareVersion[0], kFirmwareVersion[1],
                        kFirmwareVersion[2], 0x00, 0x00, 0x00,
                        kBluetoothVersion[0], 0x00, 0x00, 0x00};
    memcpy(info + 10, identity_.bt_address, sizeof(identity_.bt_address));
    const uint16_t length = capacity < sizeof(info) ? capacity : sizeof(info);
    memcpy(out, info, length);
    return static_cast<int16_t>(length);
  }
  return -1;
}

bool Responder::vendorRequestOut(uint8_t request) const {
  return request == 0x04;
}

uint32_t commonReportButtons(uint32_t buttons) {
  // Report 0x05 keeps the Switch Pro byte layout plus C, GR and GL.
  static const struct { uint32_t from; uint32_t to; } kMap[] = {
    {kButtonY, 1u << 0}, {kButtonX, 1u << 1}, {kButtonB, 1u << 2},
    {kButtonA, 1u << 3}, {kButtonR, 1u << 6}, {kButtonZR, 1u << 7},
    {kButtonMinus, 1u << 8}, {kButtonPlus, 1u << 9},
    {kButtonRStick, 1u << 10}, {kButtonLStick, 1u << 11},
    {kButtonHome, 1u << 12}, {kButtonCapture, 1u << 13}, {kButtonC, 1u << 14},
    {kButtonDown, 1u << 16}, {kButtonUp, 1u << 17}, {kButtonRight, 1u << 18},
    {kButtonLeft, 1u << 19}, {kButtonL, 1u << 22}, {kButtonZL, 1u << 23},
    {kButtonGR, 1u << 24}, {kButtonGL, 1u << 25},
  };
  uint32_t out = 0;
  for (const auto& entry : kMap) {
    if (buttons & entry.from) out |= entry.to;
  }
  return out;
}

void Responder::packInputReport(uint8_t report[kReportSize], uint32_t buttons,
                                const uint8_t leftStick[3],
                                const uint8_t rightStick[3]) {
  memset(report, 0, kReportSize);
  const uint32_t counter = report_counter_++;
  if (input_report_ == 0x05) {
    // USB offsets are the documented ones plus the report ID byte.
    report[0] = 0x05;
    writeLe32(report + 1, counter);
    writeLe32(report + 5, commonReportButtons(buttons));
    memcpy(report + 11, leftStick, 3);
    memcpy(report + 14, rightStick, 3);
    report[0x20] = 0xa5;  // 3749 mV
    report[0x21] = 0x0e;
    report[0x22] = 0x20;  // charged
    report[0x2A] = 0x01;
    return;
  }
  report[0] = kInputReportId;
  report[1] = static_cast<uint8_t>(counter);
  report[2] = kPowerInfo;
  report[3] = static_cast<uint8_t>(buttons);
  report[4] = static_cast<uint8_t>(buttons >> 8);
  report[5] = static_cast<uint8_t>(buttons >> 16);
  memcpy(report + 6, leftStick, 3);
  memcpy(report + 9, rightStick, 3);
  report[12] = static_cast<uint8_t>(
      0x30u | ((enabledFeatures() & kFeatureRumble) ? 0x08u : 0x00u));
  // 13 NFC idle, 14 no headset, 15 no motion data.
}

bool decodeRumble(const uint8_t* report, uint16_t length, uint8_t* left,
                  uint8_t* right) {
  if (report == nullptr || left == nullptr || right == nullptr ||
      length < 0x17 || report[0] != kRumbleReportId) {
    return false;
  }
  *left = sampleStrength(report + 0x02);
  *right = sampleStrength(report + 0x12);
  return true;
}

}  // namespace switch2_pro
