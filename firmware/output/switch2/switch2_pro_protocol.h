#pragma once

#include <stddef.h>
#include <stdint.h>

// Switch 2 Pro Controller (057E:2069) wired protocol, device side.
//
// Sources: ndeadly/switch2_controller_research (descriptors, commands, HID
// reports, memory layout) and that repository's Cynthion capture of a Switch
// 2 console starting a Pro Controller 2 (captures/usb). The console reads the
// device and configuration descriptors, makes three vendor requests on EP0,
// then sends 8-byte-header commands on the vendor bulk OUT endpoint and reads
// one response per command from bulk IN. See docs/internal/switch2-pro-output.md.
// Our research: third_party/licenses/Reflex-Switch2-Pro-Research-LICENSE.
//
// This header has no TinyUSB dependency so host tests can drive it.
namespace switch2_pro {

constexpr uint16_t kVendorId = 0x057E;
constexpr uint16_t kProductId = 0x2069;
// The captured controller (HID + vendor interfaces, no audio) reports 1.01.
// Firmware with the headset DSP adds three audio interfaces and reports 2.00.
constexpr uint16_t kDeviceVersion = 0x0101;
constexpr uint8_t kInputReportId = 0x09;
constexpr uint8_t kRumbleReportId = 0x02;
constexpr uint8_t kReportSize = 64;
constexpr uint8_t kPacketSize = 64;
constexpr uint16_t kMaxResponseSize = 96;
constexpr uint8_t kBulkHeaderSize = 8;

// Reassembles one bulk command from the console's 64-byte OUT packets.
// Header byte 5 is the data length (byte 4 is undocumented and ignored);
// the longest documented command is a 0x80-byte memory write, 144 bytes with
// its two headers. A longer command is still consumed whole and handed over
// truncated, so that every command gets its reply.
class CommandAssembler {
 public:
  static constexpr uint16_t kCapacity = 160;

  // Feeds one packet. Returns the command length held once it is complete
  // (its header plus the data that fit), 0 while more packets are needed.
  uint16_t feed(const uint8_t* packet, uint16_t length) {
    if (packet == nullptr || length > kPacketSize) {
      reset();
      return 0;
    }
    const uint16_t room = static_cast<uint16_t>(kCapacity - held_);
    const uint16_t fit = length < room ? length : room;
    for (uint16_t i = 0; i < fit; ++i) buffer_[held_ + i] = packet[i];
    held_ = static_cast<uint16_t>(held_ + fit);
    received_ = static_cast<uint16_t>(received_ + length);
    if (fit < length) truncated_ = true;
    const uint16_t expected = received_ >= kBulkHeaderSize
        ? static_cast<uint16_t>(kBulkHeaderSize + buffer_[5])
        : static_cast<uint16_t>(kBulkHeaderSize);
    if (received_ < expected) {
      // A packet shorter than the endpoint size ends the transfer, so a
      // runt command is dropped rather than merged with the next one.
      if (length < kPacketSize) reset();
      return 0;
    }
    return held_ < expected ? held_ : expected;
  }
  const uint8_t* command() const { return buffer_; }
  bool truncated() const { return truncated_; }
  void reset() {
    held_ = 0;
    received_ = 0;
    truncated_ = false;
  }

 private:
  uint8_t buffer_[kCapacity] = {};
  uint16_t held_ = 0;
  uint16_t received_ = 0;
  bool truncated_ = false;
};

constexpr uint8_t kHidReportDescriptorSize = 97;
extern const uint8_t kHidReportDescriptor[kHidReportDescriptorSize];

// The captured configuration after its 9-byte header: an IAD per function,
// HID interface 0 (interrupt IN/OUT, 64 bytes, 4 ms) and vendor interface 1
// (bulk OUT/IN, 64 bytes). Interface strings are omitted (the console never
// reads strings); the capture has iInterface 5 and 6.
constexpr uint8_t kInterfaceDescriptorsSize = 71;
void writeInterfaceDescriptors(uint8_t out[kInterfaceDescriptorsSize],
                               uint8_t firstInterface, uint8_t hidIn,
                               uint8_t hidOut, uint8_t bulkOut, uint8_t bulkIn);

// Report 0x09 button bits, bytes 3..5 read as one little-endian value.
enum : uint32_t {
  kButtonB = 1u << 0,
  kButtonA = 1u << 1,
  kButtonY = 1u << 2,
  kButtonX = 1u << 3,
  kButtonR = 1u << 4,
  kButtonZR = 1u << 5,
  kButtonPlus = 1u << 6,
  kButtonRStick = 1u << 7,
  kButtonDown = 1u << 8,
  kButtonRight = 1u << 9,
  kButtonLeft = 1u << 10,
  kButtonUp = 1u << 11,
  kButtonL = 1u << 12,
  kButtonZL = 1u << 13,
  kButtonMinus = 1u << 14,
  kButtonLStick = 1u << 15,
  kButtonHome = 1u << 16,
  kButtonCapture = 1u << 17,
  kButtonGR = 1u << 18,
  kButtonGL = 1u << 19,
  kButtonC = 1u << 20,
};

// Repacks the three Switch Pro (057E:2009) report button bytes, so this
// persona inherits the Switch Pro mapping, remaps and shortcuts unchanged.
uint32_t buttonsFromSwitchProBytes(const uint8_t switchProButtons[3]);

// C, GL and GR from the neutral frame's extra buttons (bits 1, 2 and 3), as
// Modern2USB fills them from a Pro Controller 2 or learned HID paddles.
uint32_t buttonsFromNeutralExtras(uint16_t extra);

// Per-unit values the controller reports. The address is in wire order.
struct Identity {
  uint8_t bt_address[6];
  char serial[16];
};

// Derives a stable identity from the RP2040 board ID, without storing it.
void deriveIdentity(const uint8_t boardId[8], Identity* identity);

// The device key every genuine controller returns in 0x15/0x04 (wire order).
extern const uint8_t kPairingDeviceKey[16];

// Pairing challenge reply (0x15/0x02). Wire values are byte-reversed for
// AES; the encrypted block is sent as computed.
void pairingChallengeResponse(const uint8_t ltkWire[16],
                              const uint8_t challengeWire[16],
                              uint8_t responseWire[16]);

struct Diagnostics {
  uint16_t commands;
  uint16_t unhandled;
  uint16_t firmware_update;
  uint8_t last_unhandled_command;
  uint8_t last_unhandled_subcommand;
  bool paired;
};

class Responder {
 public:
  void reset(const Identity& identity);

  // One bulk command. Returns the response length, 0 for none.
  uint16_t handleCommand(const uint8_t* request, uint16_t length,
                         uint8_t* response, uint16_t capacity);

  // EP0 vendor requests. In: returns the byte count, or -1 to stall.
  int16_t vendorRequestIn(uint8_t request, uint8_t* out, uint16_t capacity) const;
  bool vendorRequestOut(uint8_t request) const;

  // Input reports start after the console's Initialise USB command.
  bool reportsEnabled() const { return usb_initialised_; }
  uint8_t selectedInputReport() const { return input_report_; }
  uint8_t enabledFeatures() const { return features_enabled_ & feature_mask_; }
  uint8_t playerLeds() const { return player_leds_; }
  const Diagnostics& diagnostics() const { return diagnostics_; }

  // The selected input report: 0x09 (the console's choice) or 0x05 (SDL's).
  // Sticks are packed 12-bit pairs, larger Y is up, as on the Switch Pro.
  void packInputReport(uint8_t report[kReportSize], uint32_t buttons,
                       const uint8_t leftStick[3], const uint8_t rightStick[3]);

  // Fills `out` with `length` bytes of the emulated controller flash.
  void readFlash(uint32_t address, uint8_t* out, uint16_t length) const;

 private:
  uint16_t ack(const uint8_t* request, uint8_t* response, uint16_t capacity,
               const uint8_t* data, uint16_t dataLength,
               uint8_t direction = 0x01);

  Identity identity_{};
  // A1 ^ device key from this session's key exchange; zero until then. The
  // console stores, rather than checks, the challenge answer over USB.
  uint8_t session_ltk_[16] = {};
  Diagnostics diagnostics_{};
  bool usb_initialised_ = false;
  uint8_t input_report_ = kInputReportId;
  uint8_t feature_mask_ = 0;
  uint8_t features_enabled_ = 0;
  uint8_t player_leds_ = 0;
  uint32_t report_counter_ = 0;
};

// Report 0x05 button bits for the report 0x09 bits above.
uint32_t commonReportButtons(uint32_t buttons);

// Output report 0x02 (rumble): one 16-byte block per motor, each a sequence
// byte and one 5-byte HD rumble sample. Returns 0..255 per side.
bool decodeRumble(const uint8_t* report, uint16_t length,
                  uint8_t* left, uint8_t* right);

}  // namespace switch2_pro
