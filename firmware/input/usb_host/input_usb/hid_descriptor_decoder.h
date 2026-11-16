#pragma once

#include <stdint.h>

#include "../../../core/hid_mapping_profiles.h"
#include "input_usb.h"

constexpr uint8_t HID_DECODED_BUTTON_COUNT = 32;
constexpr uint8_t HID_DECODED_AXIS_COUNT = 12;

struct HidDecodedField {
  uint16_t bit_offset;
  uint16_t usage_page;
  uint16_t usage;
  uint32_t application;
  uint32_t physical;
  int32_t logical_min;
  int32_t logical_max;
  uint8_t bit_size;
  uint8_t report_id;
};

struct HidDescriptorLayout {
  HidDecodedField buttons[HID_DECODED_BUTTON_COUNT];
  HidDecodedField axes[HID_DECODED_AXIS_COUNT];
  HidDecodedField dpad[4];
  HidDecodedField hat;
  uint32_t descriptor_crc32;
  uint8_t button_count;
  uint8_t axis_count;
  uint8_t dpad_mask;
  uint8_t has_hat;
  uint8_t first_report_id;
  uint8_t has_report_ids;
  uint8_t valid;
};

struct HidDecodedState {
  uint32_t buttons;
  uint32_t button_valid_mask;
  int32_t axes[HID_DECODED_AXIS_COUNT];
  uint16_t axis_valid_mask;
  uint8_t dpad_mask;
  uint8_t hat;
  uint8_t hat_present;
  uint8_t report_id;
};

inline int8_t hidAxisDpadDirection(const HidDecodedField& field, int32_t value) {
  const int64_t range = int64_t(field.logical_max) - field.logical_min;
  if (range <= 0) return 0;
  const int64_t center = int64_t(field.logical_min) + range / 2;
  return value > center + range / 4 ? 1 : value < center - range / 4 ? -1 : 0;
}

bool hidParseReportDescriptor(const uint8_t* descriptor, uint16_t length,
                              HidDescriptorLayout& layout);
bool hidDecodeInputReport(const HidDescriptorLayout& layout,
                          const uint8_t* report, uint16_t length,
                          HidDecodedState& state);
uint8_t hidPreferredGamepadReportId(const HidDescriptorLayout& layout);
void hidBuildDefaultMappingProfile(const HidDescriptorLayout& layout,
                                   const HidMappingProfileKey& key,
                                   HidMappingProfile& profile);
bool hidBuildAutomaticMappingProfile(const HidDescriptorLayout& layout,
                                     const HidDecodedState& state,
                                     const HidMappingProfileKey& key,
                                     HidMappingProfile& profile);
void hidApplyMappingProfile(const HidDescriptorLayout& layout,
                            const HidDecodedState& state,
                            const HidMappingProfile& profile,
                            usb_input_report_t& output);
uint32_t hidDescriptorCrc32(const uint8_t* data, uint16_t length);

// Report-descriptor shapes that Sony-licensed third-party pads and sticks
// share, whatever their VID/PID. Both number their buttons in PlayStation
// order: Square, Cross, Circle, Triangle, L1, R1, L2, R2, Select/Share,
// Start/Options, L3, R3, PS, then the DS4 touchpad click.
enum HidSignatureFamily : uint8_t {
  HID_SIGNATURE_FAMILY_NONE = 0,
  // 13 buttons (14 with the touchpad, as GP2040-CE declares), hat, 8-bit
  // X/Y/Z/Rz, the twelve 8-bit pressure inputs (vendor 0x20-0x2B) and the
  // vendor Feature 0x2621 the PS3 asks for.
  HID_SIGNATURE_FAMILY_PS3 = 1,
  // Report ID 1 with a 64-byte input holding 14 buttons, hat and 8-bit
  // X/Y/Z/Rz/Rx/Ry, plus Feature report 0x03 carrying the DS4 definition
  // usage 0x2721 (DualSense-style pads use 0x2821) and auth Features
  // 0xF0-0xF3.
  HID_SIGNATURE_FAMILY_DS4 = 2,
};

// Classifies the raw descriptor the layout was parsed from. report_id is the
// gamepad report the mapping key uses. Anything short of a full match is NONE.
HidSignatureFamily hidClassifyDescriptorSignature(
    const uint8_t* descriptor, uint16_t length,
    const HidDescriptorLayout& layout, uint8_t report_id);
// Builds that family's fixed layout, the same neutral frame the native
// DualShock 3/4 decoders produce (PS3: digital L2/R2; DS4: Rx/Ry triggers).
// False for NONE or when a source is missing from key.report_id.
bool hidBuildSignatureFamilyProfile(HidSignatureFamily family,
                                    const HidDescriptorLayout& layout,
                                    const HidMappingProfileKey& key,
                                    HidMappingProfile& profile);
