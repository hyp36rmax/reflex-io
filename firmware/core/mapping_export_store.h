#pragma once

#include <stdint.h>

#include "hid_mapping_profiles.h"

// A taught mapping only becomes a gamecontrollerdb.txt line while the
// controller is attached, because Linux evdev order comes from its report
// descriptor and the stored profile keeps only a CRC of that. USB2DB15 has one
// USB port, so the controller is gone by the time the adapter reaches a PC.
// The finished line is therefore written here at teach time and read back
// later with nothing plugged in.
constexpr uint8_t MAPPING_EXPORT_ENTRY_COUNT = 8;
constexpr uint16_t MAPPING_EXPORT_LINE_SIZE = 208;

struct __attribute__((packed)) MappingExportEntry {
  HidMappingProfileKey key;
  char line[MAPPING_EXPORT_LINE_SIZE];
};

static_assert(sizeof(MappingExportEntry) == 224,
              "MappingExportEntry must stay 224 bytes");

struct MappingExportList {
  MappingExportEntry entries[MAPPING_EXPORT_ENTRY_COUNT];
};

// Schema 2 adds freshness checks inside the same reserved erase sector.
// Profile order is excluded because the mapping store updates its LRU order.
struct MappingExportSnapshot {
  MappingExportList list;
  uint32_t profile_crc32[MAPPING_EXPORT_ENTRY_COUNT];
};
static_assert(sizeof(MappingExportSnapshot) == 1824,
              "Export snapshots must fit twice in the existing sector");

inline uint32_t mappingExportProfileCrc32(const HidMappingProfile& profile) {
  const auto* bytes = reinterpret_cast<const uint8_t*>(&profile);
  uint32_t crc = 0xFFFFFFFFu;
  for (uint8_t index = 0; index < sizeof(profile) - 1; ++index) {
    crc ^= bytes[index];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320u : 0);
    }
  }
  return ~crc;
}

enum class MappingExportStatus : uint8_t {
  NotAttempted, Saved, SavedWithEviction, Unavailable, WriteFailed
};
MappingExportStatus mappingExportLastStatus();
void mappingExportSetStatus(MappingExportStatus status);

// Store one controller's line, replacing the entry with the same key. The
// oldest entry is dropped when the list is full, because the newest teach is
// the one a user is about to send.
bool mappingExportStoreSave(const HidMappingProfile& profile, const char* line);

// Read every stored line. Entries with an empty line are unused.
bool mappingExportStoreRead(MappingExportList& list);

uint8_t mappingExportStoreCount();

bool mappingExportStoreClear();
