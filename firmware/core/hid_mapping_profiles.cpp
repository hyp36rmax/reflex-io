#include "hid_mapping_profiles.h"

#include <Arduino.h>
#include <string.h>

#include "eeprom_helper.h"
#include "settings_persistence_deferred.h"
#include "settings_persistence_snapshot.h"
#include "product_flash_services.h"

#if defined(ARDUINO_ARCH_RP2040)
#include <hardware/flash.h>
#include <hardware/sync.h>
#include <pico/mutex.h>
#include "reserved_flash_layout.h"
#endif

namespace {

constexpr uint32_t kProfileMagic = 0x50414D48u;  // HMAP
constexpr uint16_t kProfileSchema = 1;

struct HidProfileSectorHeader {
  uint32_t magic;
  uint16_t schema;
  uint16_t payload_size;
  uint16_t generation;
  uint16_t payload_crc16;
  uint16_t header_crc16;
  uint16_t reserved;
};

static_assert(sizeof(HidProfileSectorHeader) == 16,
              "HID profile sector header must remain 16 bytes");

constexpr uint16_t kProfileRecordSize =
    sizeof(HidProfileSectorHeader) + sizeof(HidMappingProfileList);
constexpr uint8_t kProfileRecordCount = 2;

bool profileIsValid(const HidMappingProfile& profile) {
  return (profile.flags & HID_MAPPING_PROFILE_VALID) != 0;
}

bool keyMatches(const HidMappingProfileKey& a, const HidMappingProfileKey& b) {
  return memcmp(&a, &b, sizeof(a)) == 0;
}

bool stablePersonalityMatches(const HidMappingProfileKey& a,
                              const HidMappingProfileKey& b) {
  // TinyUSB's HID instance is an enumeration index, not the descriptor's
  // bInterfaceNumber. It can change when a composite device remounts or when
  // another interface appears/disappears. The descriptor CRC and preferred
  // report ID already identify the HID personality, so allow a unique saved
  // profile to follow that personality across instance renumbering.
  return a.descriptor_crc32 == b.descriptor_crc32 &&
         a.vid == b.vid && a.pid == b.pid &&
         a.bcd_device == b.bcd_device &&
         a.identity_crc16 == b.identity_crc16 &&
         a.transport == b.transport && a.report_id == b.report_id &&
         a.flags == b.flags;
}

void sanitizeList(HidMappingProfileList& list) {
  for (uint8_t i = 0; i < HID_MAPPING_PROFILE_COUNT; ++i) {
    HidMappingProfile& profile = list.profiles[i];
    if (!profileIsValid(profile)) {
      memset(&profile, 0, sizeof(profile));
      memset(profile.button_source, HID_MAPPING_SOURCE_NONE,
             sizeof(profile.button_source));
      memset(profile.axis_source, HID_MAPPING_SOURCE_NONE,
             sizeof(profile.axis_source));
      profile.hat_source = HID_MAPPING_SOURCE_NONE;
      profile.order = 0xFF;
      continue;
    }
    profile.flags &= HID_MAPPING_PROFILE_VALID |
                     HID_MAPPING_PROFILE_IGNORED |
                     HID_MAPPING_PROFILE_LEARNED |
                     HID_MAPPING_PROFILE_BUTTON_DPAD |
                     HID_MAPPING_PROFILE_AXIS_DPAD |
                     HID_MAPPING_PROFILE_DATABASE_CONFIRMED |
                     HID_MAPPING_PROFILE_LAYOUT_CLASSIFIED |
                     HID_MAPPING_PROFILE_DIRECTIONS_TAUGHT;
    for (uint8_t axis = 0; axis < HID_MAPPING_AXIS_COUNT; ++axis) {
      const uint8_t allowed =
          (profile.flags & (HID_MAPPING_PROFILE_BUTTON_DPAD |
                            HID_MAPPING_PROFILE_AXIS_DPAD)) && axis < 4
              ? 0xFF
              : static_cast<uint8_t>(HID_MAPPING_AXIS_INVERT |
                                     HID_MAPPING_AXIS_TRIGGER);
      profile.axis_flags[axis] &= allowed;
    }
    const uint8_t style = hidMappingControllerStyle(profile.layout);
    if (!hidMappingControllerStyleValid(style) ||
        hidMappingLayoutTriggerType(profile.layout) ==
            HID_MAPPING_LAYOUT_TRIGGER_MASK) {
      profile.layout = HID_MAPPING_LAYOUT_UNSPECIFIED;
      profile.flags &= ~HID_MAPPING_PROFILE_LAYOUT_CLASSIFIED;
    } else if ((profile.flags & HID_MAPPING_PROFILE_LAYOUT_CLASSIFIED) &&
               style == HID_MAPPING_LAYOUT_UNSPECIFIED) {
      profile.flags &= ~HID_MAPPING_PROFILE_LAYOUT_CLASSIFIED;
    }
    profile.key.transport = profile.key.transport > HID_MAPPING_TRANSPORT_BLUETOOTH_LE
        ? static_cast<uint8_t>(HID_MAPPING_TRANSPORT_USB) : profile.key.transport;
  }
}

uint16_t headerCrc(const HidProfileSectorHeader& header) {
  return calculateEepromBlockCrc(
      reinterpret_cast<const uint8_t*>(&header),
      offsetof(HidProfileSectorHeader, header_crc16));
}

bool generationNewer(uint16_t candidate, uint16_t current) {
  return static_cast<int16_t>(candidate - current) > 0;
}

#if defined(ARDUINO_ARCH_RP2040)
constexpr uint32_t kProfileSectorSize = FLASH_SECTOR_SIZE;
static_assert(kProfileSectorSize == 4096,
              "HID profile storage expects a 4KB erase sector");
static_assert(kProfileRecordCount * kProfileRecordSize <= kProfileSectorSize,
              "32 A/B HID profiles exceed their reserved flash sector");

alignas(4) uint8_t profileFlashScratch[kProfileSectorSize];
HidMappingProfileList profileListScratch{};
#if defined(ADAPT_CAP_LEARNED_HID_MAPPINGS)
auto_init_mutex(profilePendingMutex);
CrossCoreDeferredSnapshot<HidMappingProfileList> profilePending(
    &profilePendingMutex);
SettingsPersistenceSafeWindow profilePersistenceWindow{};
#endif

const uint8_t* profileSector() {
  return reinterpret_cast<const uint8_t*>(reflexHidMappingFlashSectorAddress());
}

const uint8_t* legacyProfileSector() {
  return reinterpret_cast<const uint8_t*>(
      reflexLegacyHidMappingFlashSectorAddress());
}

bool validateRecordFrom(const uint8_t* sector, uint8_t slot,
                        HidProfileSectorHeader* validatedHeader = nullptr) {
  if (slot >= kProfileRecordCount) {
    return false;
  }
  const uint8_t* record = sector + slot * kProfileRecordSize;
  HidProfileSectorHeader header{};
  memcpy(&header, record, sizeof(header));
  if (header.magic != kProfileMagic || header.schema != kProfileSchema ||
      header.payload_size != sizeof(HidMappingProfileList) ||
      header.header_crc16 != headerCrc(header)) {
    return false;
  }
  if (header.payload_crc16 != calculateEepromBlockCrc(
          record + sizeof(header), sizeof(HidMappingProfileList))) {
    return false;
  }
  if (validatedHeader) *validatedHeader = header;
  return true;
}

bool validateRecord(uint8_t slot,
                    HidProfileSectorHeader* validatedHeader = nullptr) {
  return validateRecordFrom(profileSector(), slot, validatedHeader);
}

bool readLatestFrom(const uint8_t* sector, HidMappingProfileList& list,
                    uint8_t* active_slot = nullptr,
                    uint16_t* generation = nullptr) {
  HidProfileSectorHeader headers[kProfileRecordCount] = {};
  bool valid[kProfileRecordCount] = {};
  for (uint8_t slot = 0; slot < kProfileRecordCount; ++slot) {
    valid[slot] = validateRecordFrom(sector, slot, &headers[slot]);
  }
  if (!valid[0] && !valid[1]) {
    return false;
  }
  uint8_t selected = !valid[0] ? 1 : 0;
  if (valid[0] && valid[1] &&
      generationNewer(headers[1].generation, headers[0].generation)) {
    selected = 1;
  }
  const uint8_t* record = sector + selected * kProfileRecordSize;
  memcpy(&list, record + sizeof(HidProfileSectorHeader), sizeof(list));
  sanitizeList(list);
  if (active_slot) {
    *active_slot = selected;
  }
  if (generation) {
    *generation = headers[selected].generation;
  }
  return true;
}

bool readLatest(HidMappingProfileList& list, uint8_t* active_slot = nullptr,
                uint16_t* generation = nullptr) {
  return readLatestFrom(profileSector(), list, active_slot, generation);
}

bool programSector() {
#if defined(ADAPT_SETTINGS_JOURNAL_ACTIVE) && ADAPT_POLICY_FIXED_END_RESERVED_FLASH_LAYOUT
  // Both A/B copies share this sector, so only the journal keeps a power cut
  // mid-save from losing every learned map. It parks the USB host itself.
  return settingsJournalWriteSector(SettingsJournalTarget::kHidMappings,
                                    profileFlashScratch, kProfileSectorSize);
#else
  constexpr uint32_t kFlashQuiesceTimeoutMs = 250;
  if (!productQuiesceForFlash(kFlashQuiesceTimeoutMs)) {
    return false;
  }
#ifndef __FREERTOS
  noInterrupts();
#endif
  rp2040.idleOtherCore();
  const uint32_t offset = reflexFlashOffset(reflexHidMappingFlashSectorAddress());
  flash_range_erase(offset, kProfileSectorSize);
  flash_range_program(offset, profileFlashScratch, kProfileSectorSize);
  rp2040.resumeOtherCore();
#ifndef __FREERTOS
  interrupts();
#endif
  productReleaseFlashQuiesce();
  return true;
#endif
}

bool writeListNow(const HidMappingProfileList& input) {
  profileListScratch = input;
  sanitizeList(profileListScratch);
  uint8_t currentSlot = 0;
  uint16_t currentGeneration = 0;
  HidProfileSectorHeader currentHeader{};
  const bool valid0 = validateRecord(0, &currentHeader);
  HidProfileSectorHeader secondHeader{};
  const bool valid1 = validateRecord(1, &secondHeader);
  bool hasCurrent = valid0 || valid1;
  if (!valid0 && valid1) {
    currentSlot = 1;
    currentHeader = secondHeader;
  } else if (valid0 && valid1 &&
             generationNewer(secondHeader.generation, currentHeader.generation)) {
    currentSlot = 1;
    currentHeader = secondHeader;
  }
  if (hasCurrent) currentGeneration = currentHeader.generation;
  const uint8_t* currentPayload = hasCurrent
      ? profileSector() + currentSlot * kProfileRecordSize +
            sizeof(HidProfileSectorHeader)
      : nullptr;
  if (hasCurrent &&
      memcmp(currentPayload, &profileListScratch, sizeof(profileListScratch)) == 0) {
    return true;
  }

  const uint8_t targetSlot = hasCurrent ? currentSlot ^ 1u : 0;
  const uint16_t nextGeneration = hasCurrent ? currentGeneration + 1u : 1u;
  memcpy(profileFlashScratch, profileSector(), sizeof(profileFlashScratch));
  const uint32_t offset = targetSlot * kProfileRecordSize;
  memset(profileFlashScratch + offset, 0xFF, kProfileRecordSize);
  memcpy(profileFlashScratch + offset + sizeof(HidProfileSectorHeader),
         &profileListScratch, sizeof(profileListScratch));

  HidProfileSectorHeader header{};
  header.magic = kProfileMagic;
  header.schema = kProfileSchema;
  header.payload_size = sizeof(HidMappingProfileList);
  header.generation = nextGeneration;
  header.payload_crc16 = calculateEepromBlockCrc(profileListScratch);
  header.reserved = 0;
  header.header_crc16 = headerCrc(header);
  memcpy(profileFlashScratch + offset, &header, sizeof(header));
  if (!programSector()) {
    return false;
  }

  HidProfileSectorHeader verifyHeader{};
  const uint8_t* verifyPayload = profileSector() + targetSlot * kProfileRecordSize +
                                 sizeof(HidProfileSectorHeader);
  return validateRecord(targetSlot, &verifyHeader) &&
         verifyHeader.generation == nextGeneration &&
         memcmp(verifyPayload, &profileListScratch,
                sizeof(profileListScratch)) == 0;
}
#else
HidMappingProfileList volatileProfiles{};

bool readLatest(HidMappingProfileList& list, uint8_t* = nullptr,
                uint16_t* = nullptr) {
  list = volatileProfiles;
  sanitizeList(list);
  return false;
}

bool writeListNow(const HidMappingProfileList& list) {
  volatileProfiles = list;
  return true;
}
#endif

bool writeList(const HidMappingProfileList& input) {
#if defined(ADAPT_CAP_LEARNED_HID_MAPPINGS) && defined(ARDUINO_ARCH_RP2040)
  // Core1 never programs flash itself: waiting for a core1 quiesce ack from
  // core1 always timed out (a 250 ms stall), and the flash/list scratch
  // buffers belong to the core0 persistence service.
  if (get_core_num() != 0 || profilePending.hasPending() ||
      settingsPersistenceProtectedSessionActive()) {
    // Avoid another ~2 KB stack object under Upsert/Find. The immutable input
    // is copied coherently here; read/publish paths sanitize their own copy.
    profilePending.stage(input);
    profilePersistenceWindow.reset();
    return true;
  }
  const bool wrote = writeListNow(input);
  if (!wrote) {
    profilePending.stage(input);
    profilePersistenceWindow.reset();
  }
  return wrote;
#else
  return writeListNow(input);
#endif
}

uint8_t findProfileIndex(const HidMappingProfileList& list,
                         const HidMappingProfileKey& key) {
  for (uint8_t i = 0; i < HID_MAPPING_PROFILE_COUNT; ++i) {
    if (profileIsValid(list.profiles[i]) && keyMatches(list.profiles[i].key, key)) {
      return i;
    }
  }

  uint8_t compatible = HID_MAPPING_SOURCE_NONE;
  for (uint8_t i = 0; i < HID_MAPPING_PROFILE_COUNT; ++i) {
    if (!profileIsValid(list.profiles[i]) ||
        !stablePersonalityMatches(list.profiles[i].key, key)) {
      continue;
    }
    // Never guess if two independently learned interfaces collapse to the
    // same stable personality. An exact match above or a new wizard pass is
    // safer than applying the wrong map.
    if (compatible != HID_MAPPING_SOURCE_NONE) {
      return HID_MAPPING_SOURCE_NONE;
    }
    compatible = i;
  }
  return compatible;
}

uint8_t findReplacementIndex(const HidMappingProfileList& list) {
  uint8_t oldest = 0;
  uint8_t oldestOrder = 0;
  for (uint8_t i = 0; i < HID_MAPPING_PROFILE_COUNT; ++i) {
    if (!profileIsValid(list.profiles[i])) {
      return i;
    }
    if (list.profiles[i].order >= oldestOrder) {
      oldestOrder = list.profiles[i].order;
      oldest = i;
    }
  }
  return oldest;
}

}  // namespace

void hidMappingProfileInitialize(HidMappingProfile& profile,
                                 const HidMappingProfileKey& key) {
  memset(&profile, 0, sizeof(profile));
  profile.key = key;
  memset(profile.button_source, HID_MAPPING_SOURCE_NONE,
         sizeof(profile.button_source));
  memset(profile.axis_source, HID_MAPPING_SOURCE_NONE,
         sizeof(profile.axis_source));
  profile.hat_source = HID_MAPPING_SOURCE_NONE;
  profile.flags = HID_MAPPING_PROFILE_VALID;
  profile.order = 0;
}

bool hidMappingProfileReadList(HidMappingProfileList& list) {
#if defined(ADAPT_CAP_LEARNED_HID_MAPPINGS) && defined(ARDUINO_ARCH_RP2040)
  if (profilePending.snapshot(&list)) {
    sanitizeList(list);
    return true;
  }
#endif
  if (readLatest(list)) {
    return true;
  }
#if ADAPT_POLICY_FIXED_END_RESERVED_FLASH_LAYOUT
  if (readLatestFrom(legacyProfileSector(), list)) {
#if defined(ADAPT_CAP_LEARNED_HID_MAPPINGS) && defined(ARDUINO_ARCH_RP2040)
    // Boot normally migrates this on core0 before core1 starts. If a late
    // read discovers legacy data, publish only a coherent RAM snapshot here;
    // the core0 persistence service performs the flash operation.
    profilePending.stage(list);
    return true;
#else
    return writeList(list);
#endif
  }
#endif
  memset(&list, 0, sizeof(list));
  sanitizeList(list);
  return false;
}

bool hidMappingProfileUpdaterStorageReady() {
#if ADAPT_POLICY_FIXED_END_RESERVED_FLASH_LAYOUT
  HidMappingProfileList current{};
  if (readLatest(current)) return true;
  HidMappingProfileList legacy{};
  if (!readLatestFrom(legacyProfileSector(), legacy)) return true;
  if (!writeListNow(legacy)) return false;
  HidMappingProfileList verify{};
  return readLatest(verify) &&
         memcmp(&verify, &legacy, sizeof(verify)) == 0;
#else
  return true;
#endif
}

bool hidMappingProfileFind(const HidMappingProfileKey& key,
                           HidMappingProfile& profile,
                           uint8_t* profile_index) {
  HidMappingProfileList list{};
  hidMappingProfileReadList(list);
  const uint8_t index = findProfileIndex(list, key);
  if (index == HID_MAPPING_SOURCE_NONE) {
    return false;
  }
  profile = list.profiles[index];
  if (profile_index) {
    *profile_index = index;
  }
  return true;
}

bool hidMappingProfileUpsert(const HidMappingProfile& input,
                             uint8_t* stored_index) {
  HidMappingProfileList list{};
  hidMappingProfileReadList(list);
  uint8_t target = findProfileIndex(list, input.key);
  if (target == HID_MAPPING_SOURCE_NONE) {
    target = findReplacementIndex(list);
  }
  for (uint8_t i = 0; i < HID_MAPPING_PROFILE_COUNT; ++i) {
    if (i != target && profileIsValid(list.profiles[i]) &&
        list.profiles[i].order < 0xFE) {
      list.profiles[i].order++;
    }
  }
  list.profiles[target] = input;
  list.profiles[target].flags |= HID_MAPPING_PROFILE_VALID;
  list.profiles[target].order = 0;
  const bool wrote = writeList(list);
  if (stored_index) {
    *stored_index = target;
  }
  return wrote;
}

bool hidMappingProfileRemove(const HidMappingProfileKey& key) {
  HidMappingProfileList list{};
  hidMappingProfileReadList(list);
  const uint8_t index = findProfileIndex(list, key);
  if (index == HID_MAPPING_SOURCE_NONE) {
    return true;
  }
  memset(&list.profiles[index], 0, sizeof(list.profiles[index]));
  return writeList(list);
}

bool hidMappingProfileClear() {
#if defined(ARDUINO_ARCH_RP2040)
  HidMappingProfileList empty{};
  sanitizeList(empty);
  // Keep a CRC-valid empty record so the one-time legacy fallback cannot
  // restore profiles after an intentional factory reset.
  return writeList(empty);
#else
  memset(&volatileProfiles, 0, sizeof(volatileProfiles));
  return true;
#endif
}

void hidMappingProfilePersistenceService() {
#if defined(ADAPT_CAP_LEARNED_HID_MAPPINGS) && defined(ARDUINO_ARCH_RP2040)
  if (!profilePersistenceWindow.shouldFlush(
        profilePending.hasPending(),
        settingsPersistenceProtectedSessionActive(),
        millis())) {
    return;
  }
  (void)hidMappingProfilePersistenceFlushNow();
#endif
}

bool hidMappingProfilePersistenceFlushNow() {
#if defined(ADAPT_CAP_LEARNED_HID_MAPPINGS) && defined(ARDUINO_ARCH_RP2040)
  uint32_t revision = 0;
  if (!profilePending.snapshot(&profileListScratch, &revision)) {
    profilePersistenceWindow.reset();
    return true;
  }
  // Flash is programmed from an unlocked copy. A concurrent core may stage a
  // newer mapping while this write is in progress; its revision must survive.
  if (!writeListNow(profileListScratch)) {
    profilePersistenceWindow.reset();
    return false;
  }
  if (!profilePending.clearIfRevision(revision)) {
    profilePersistenceWindow.reset();
    return false;
  }
  profilePersistenceWindow.reset();
#endif
  return true;
}

void hidMappingProfilePersistenceDiscardDeferred() {
#if defined(ADAPT_CAP_LEARNED_HID_MAPPINGS) && defined(ARDUINO_ARCH_RP2040)
  profilePending.discard();
  profilePersistenceWindow.reset();
#endif
}

bool hidMappingProfilePersistenceHasPending() {
#if defined(ADAPT_CAP_LEARNED_HID_MAPPINGS) && defined(ARDUINO_ARCH_RP2040)
  return profilePending.hasPending();
#else
  return false;
#endif
}
