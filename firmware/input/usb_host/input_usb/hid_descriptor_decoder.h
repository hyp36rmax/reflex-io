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
