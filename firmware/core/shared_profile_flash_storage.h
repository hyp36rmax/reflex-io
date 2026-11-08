#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr uint16_t SHARED_PROFILE_FLASH_SECTOR_SIZE = 4096;

// The Bluetooth-bond and controller-hotkey stores share one reserved erase
// sector. Edits use one common 4 KiB scratch image so every writer preserves
// the other writer's bytes across the sector erase/program cycle.
const uint8_t* sharedProfileFlashData();
bool sharedProfileFlashBeginEdit(uint8_t*& sectorImage);
bool sharedProfileFlashCommitEdit();

#if defined(REFLEX_HOST_TEST_SHARED_PROFILE_FLASH)
void sharedProfileFlashHostReset(uint8_t fill = 0xFF);
void sharedProfileFlashHostFailNextPrograms(uint8_t count);
uint32_t sharedProfileFlashHostProgramCount();
void sharedProfileFlashHostClearProgramCount();
uint8_t* sharedProfileFlashHostData();
#endif
