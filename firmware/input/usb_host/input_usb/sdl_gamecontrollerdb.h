#pragma once

#include <stdint.h>

#include <stddef.h>
#include "hid_descriptor_decoder.h"

struct SdlGameControllerDbStats {
  uint16_t source_records;
  uint16_t compatible_records;
  uint16_t identities;
  uint16_t unique_mappings;
  uint16_t blocked_identities;
  uint16_t deduplicated_payloads;
  uint16_t named_identities;
  uint16_t unique_names;
};

// Build a temporary mapping profile from the embedded Linux/USB SDL database.
// User-learned profiles remain authoritative and are checked before this.
bool hidBuildSdlGameControllerDbMappingProfile(
    const HidDescriptorLayout& layout,
    const HidMappingProfileKey& key,
    HidMappingProfile& profile);

// Return the pinned Linux SDL name for a USB identity. Exact bcdDevice matches
// win; a generated revision-zero fallback is used only when every known
// revision has the same canonical name.
const char* sdlGameControllerDbName(uint16_t vid, uint16_t pid,
                                    uint16_t bcd_device);

// Format a mapping as one gamecontrollerdb.txt line, terminated and NUL
// terminated, for a user to paste upstream or into gamecontrollerdb_user.txt.
// Returns 0 when the descriptor cannot be expressed in Linux evdev order,
// which is the same condition under which the parser refuses to read a line.
size_t sdlGameControllerDbFormatMapping(const HidDescriptorLayout& layout,
                                        const HidMappingProfileKey& key,
                                        const HidMappingProfile& profile,
                                        const char* name,
                                        char* out, size_t capacity);

SdlGameControllerDbStats sdlGameControllerDbStats();
const char* sdlGameControllerDbSourceSha256();

// Products that read the table from flash (ADAPT_CAP_SDL_CONTROLLER_FLASH_TABLE)
// validate it here once, on core 0 before the USB host starts. Until then, and
// when the flash copy is blank, torn or of another schema, every lookup finds
// nothing. Elsewhere this does nothing.
void sdlGameControllerDbInit();

#if defined(SDL_GAMECONTROLLERDB_HOST_TEST)
// Host tests only: read rows from a table image (nullptr: the compiled-in
// arrays again). Returns false, leaving the current rows, for an image the
// firmware would refuse.
bool sdlGameControllerDbHostUseTableImage(const uint8_t* image,
                                          uint32_t capacity);
// Host tests only: the exact mapping record for one identity, all 0xFF for a
// blocked identity. Returns false when the identity is not listed.
bool sdlGameControllerDbHostMappingRecord(uint16_t vid, uint16_t pid,
                                          uint16_t version, uint8_t* record,
                                          size_t capacity);
#endif
