#pragma once

#include <stdint.h>

constexpr uint8_t HID_MAPPING_PROFILE_COUNT = 32;
constexpr uint8_t HID_MAPPING_BUTTON_COUNT = 19;
constexpr uint8_t HID_MAPPING_AXIS_COUNT = 6;
constexpr uint8_t HID_MAPPING_SOURCE_NONE = 0xFF;

constexpr uint8_t HID_MAPPING_PROFILE_VALID = 0x01;
constexpr uint8_t HID_MAPPING_PROFILE_IGNORED = 0x02;
constexpr uint8_t HID_MAPPING_PROFILE_LEARNED = 0x04;

// The four d-pad directions are ordinary HID button sources. To preserve the
// 63-byte profile format, each source is stored in the unused upper six bits
// of axis_flags[0..3]. Old profiles do not set this flag and keep their
// descriptor-provided hat/discrete d-pad behavior.
constexpr uint8_t HID_MAPPING_PROFILE_BUTTON_DPAD = 0x08;
// Axis-backed d-pads (for example SDL -a0/+a0 mappings) reuse the same packed
// six-bit storage as button-backed d-pads. Bit 4 selects the positive half.
constexpr uint8_t HID_MAPPING_PROFILE_AXIS_DPAD = 0x10;
// A user confirmed this database-derived map against the exact raw HID
// descriptor key. It is persisted so the quick check runs only once.
constexpr uint8_t HID_MAPPING_PROFILE_DATABASE_CONFIRMED = 0x20;
// The layout byte contains an explicit controller style and capability bits.
// Older profiles omit this bit and retain the legacy all-controls OLED.
constexpr uint8_t HID_MAPPING_PROFILE_LAYOUT_CLASSIFIED = 0x40;
// All four movement directions were explicitly taught with release gating.
constexpr uint8_t HID_MAPPING_PROFILE_DIRECTIONS_TAUGHT = 0x80;

constexpr uint8_t HID_MAPPING_AXIS_INVERT = 0x01;
constexpr uint8_t HID_MAPPING_AXIS_TRIGGER = 0x02;

enum HidMappingFaceLayout : uint8_t {
  HID_MAPPING_LAYOUT_UNSPECIFIED = 0,
  HID_MAPPING_LAYOUT_FOUR_FACE = 1,
  HID_MAPPING_LAYOUT_SIX_FACE = 2,
  HID_MAPPING_LAYOUT_N64 = 3,
  HID_MAPPING_LAYOUT_GAMECUBE = 4,
  HID_MAPPING_LAYOUT_VIRTUAL_BOY = 5,
  HID_MAPPING_LAYOUT_RACING_WHEEL = 6,
  HID_MAPPING_LAYOUT_HOTAS = 7,
  HID_MAPPING_LAYOUT_DUAL_JOYSTICK = 8,
  HID_MAPPING_LAYOUT_OTHER = 9,
  // Appended to preserve the persisted values of every existing style.
  HID_MAPPING_LAYOUT_TWO_FACE = 10,
  HID_MAPPING_LAYOUT_THREE_FACE = 11,
};

constexpr uint8_t HID_MAPPING_LAYOUT_STYLE_MASK = 0x0F;
constexpr uint8_t HID_MAPPING_LAYOUT_HAS_SHOULDERS = 0x10;
constexpr uint8_t HID_MAPPING_LAYOUT_TRIGGER_MASK = 0x60;
constexpr uint8_t HID_MAPPING_LAYOUT_TRIGGER_DIGITAL = 0x20;
constexpr uint8_t HID_MAPPING_LAYOUT_TRIGGER_ANALOG = 0x40;
constexpr uint8_t HID_MAPPING_LAYOUT_HAS_GRIPS = 0x80;

inline uint8_t hidMappingControllerStyle(uint8_t layout) {
  return layout & HID_MAPPING_LAYOUT_STYLE_MASK;
}

inline bool hidMappingControllerStyleValid(uint8_t layout) {
  return hidMappingControllerStyle(layout) <= HID_MAPPING_LAYOUT_THREE_FACE;
}

inline uint8_t hidMappingStyleFaceButtonCount(uint8_t layout) {
  switch (hidMappingControllerStyle(layout)) {
    case HID_MAPPING_LAYOUT_TWO_FACE:
      return 2;
    case HID_MAPPING_LAYOUT_THREE_FACE:
      return 3;
    case HID_MAPPING_LAYOUT_SIX_FACE:
    case HID_MAPPING_LAYOUT_N64:
    case HID_MAPPING_LAYOUT_VIRTUAL_BOY:
      return 6;
    default:
      return 4;
  }
}

inline bool hidMappingStyleUsesSixControls(uint8_t layout) {
  return hidMappingStyleFaceButtonCount(layout) == 6;
}

inline bool hidMappingLayoutHasShoulders(uint8_t layout) {
  return (layout & HID_MAPPING_LAYOUT_HAS_SHOULDERS) != 0;
}

inline uint8_t hidMappingLayoutTriggerType(uint8_t layout) {
  return layout & HID_MAPPING_LAYOUT_TRIGGER_MASK;
}

inline bool hidMappingLayoutHasGrips(uint8_t layout) {
  return (layout & HID_MAPPING_LAYOUT_HAS_GRIPS) != 0;
}

enum HidMappingTransport : uint8_t {
  HID_MAPPING_TRANSPORT_USB = 1,
  HID_MAPPING_TRANSPORT_BLUETOOTH_CLASSIC = 2,
  HID_MAPPING_TRANSPORT_BLUETOOTH_LE = 3,
};

struct HidMappingProfileKey {
  uint32_t descriptor_crc32;
  uint16_t vid;
  uint16_t pid;
  uint16_t bcd_device;
  uint16_t identity_crc16;
  uint8_t transport;
  uint8_t interface_number;
  uint8_t report_id;
  uint8_t flags;
};

static_assert(sizeof(HidMappingProfileKey) == 16,
              "HidMappingProfileKey must remain 16 bytes");

struct __attribute__((packed)) HidMappingProfile {
  HidMappingProfileKey key;
  // Stored as the raw 16-bit HID value. Signed descriptor fields reinterpret
  // this as int16_t; unsigned fields retain their full 0..65535 baseline.
  uint16_t axis_center[HID_MAPPING_AXIS_COUNT];
  // Source indices refer to descriptor-decoded button and axis fields. The
  // button destination order matches REMAP_MAX_BUTTONS.
  uint8_t button_source[HID_MAPPING_BUTTON_COUNT];
  uint8_t axis_source[HID_MAPPING_AXIS_COUNT];
  uint8_t axis_flags[HID_MAPPING_AXIS_COUNT];
  uint8_t hat_source;
  uint8_t layout;
  uint8_t flags;
  uint8_t order;
};

static_assert(sizeof(HidMappingProfile) == 63,
              "HidMappingProfile must remain 63 bytes");

struct HidMappingProfileList {
  HidMappingProfile profiles[HID_MAPPING_PROFILE_COUNT];
};

static_assert(sizeof(HidMappingProfileList) == 2016,
              "32 HID mapping profiles must fit one A/B flash sector");

bool hidMappingProfileReadList(HidMappingProfileList& list);
bool hidMappingProfileFind(const HidMappingProfileKey& key,
                           HidMappingProfile& profile,
                           uint8_t* profile_index = nullptr);
bool hidMappingProfileUpsert(const HidMappingProfile& profile,
                              uint8_t* stored_index = nullptr);
bool hidMappingProfileRemove(const HidMappingProfileKey& key);
bool hidMappingProfileClear();
void hidMappingProfileInitialize(HidMappingProfile& profile,
                                 const HidMappingProfileKey& key);
bool hidMappingProfileUpdaterStorageReady();
void hidMappingProfilePersistenceService();
bool hidMappingProfilePersistenceFlushNow();
void hidMappingProfilePersistenceDiscardDeferred();
bool hidMappingProfilePersistenceHasPending();

inline void hidMappingProfileSetButtonDpadSource(HidMappingProfile& profile,
                                                 uint8_t direction,
                                                 uint8_t source) {
  if (direction >= 4) return;
  const uint8_t packed = source == HID_MAPPING_SOURCE_NONE
      ? 0x3F : static_cast<uint8_t>(source & 0x1F);
  profile.axis_flags[direction] = static_cast<uint8_t>(
      (profile.axis_flags[direction] & 0x03) | (packed << 2));
  if (source != HID_MAPPING_SOURCE_NONE) {
    profile.flags |= HID_MAPPING_PROFILE_BUTTON_DPAD;
    profile.flags &= ~HID_MAPPING_PROFILE_AXIS_DPAD;
  } else if (profile.flags & HID_MAPPING_PROFILE_BUTTON_DPAD) {
    bool anyButtonDpad = false;
    for (uint8_t index = 0; index < 4; ++index) {
      if ((profile.axis_flags[index] >> 2) != 0x3F) {
        anyButtonDpad = true;
        break;
      }
    }
    if (!anyButtonDpad) {
      profile.flags &= ~HID_MAPPING_PROFILE_BUTTON_DPAD;
    }
  }
}

inline void hidMappingProfileSetAxisDpadSource(HidMappingProfile& profile,
                                               uint8_t direction,
                                               uint8_t source,
                                               bool positive) {
  if (direction >= 4) return;
  const uint8_t packed = source == HID_MAPPING_SOURCE_NONE
      ? 0x3F
      : static_cast<uint8_t>((source & 0x0F) | (positive ? 0x10 : 0));
  profile.axis_flags[direction] = static_cast<uint8_t>(
      (profile.axis_flags[direction] & 0x03) | (packed << 2));
  if (source != HID_MAPPING_SOURCE_NONE) {
    profile.flags |= HID_MAPPING_PROFILE_AXIS_DPAD;
    profile.flags &= ~HID_MAPPING_PROFILE_BUTTON_DPAD;
  }
}

inline uint8_t hidMappingProfileAxisDpadSource(
    const HidMappingProfile& profile, uint8_t direction) {
  if (direction >= 4 || !(profile.flags & HID_MAPPING_PROFILE_AXIS_DPAD)) {
    return HID_MAPPING_SOURCE_NONE;
  }
  const uint8_t packed = profile.axis_flags[direction] >> 2;
  return packed == 0x3F ? HID_MAPPING_SOURCE_NONE
                        : static_cast<uint8_t>(packed & 0x0F);
}

inline bool hidMappingProfileAxisDpadPositive(
    const HidMappingProfile& profile, uint8_t direction) {
  return direction < 4 &&
         (profile.flags & HID_MAPPING_PROFILE_AXIS_DPAD) != 0 &&
         ((profile.axis_flags[direction] >> 2) & 0x10) != 0;
}

inline uint8_t hidMappingProfileButtonDpadSource(
    const HidMappingProfile& profile, uint8_t direction) {
  if (direction >= 4 ||
      !(profile.flags & HID_MAPPING_PROFILE_BUTTON_DPAD)) {
    return HID_MAPPING_SOURCE_NONE;
  }
  const uint8_t packed = profile.axis_flags[direction] >> 2;
  return packed == 0x3F ? HID_MAPPING_SOURCE_NONE : packed;
}
