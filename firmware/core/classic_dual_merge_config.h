#pragma once

#include <stdint.h>

#include "device_mode.h"

constexpr uint32_t DUAL_INPUT_MERGE_LEFT_STICK = 1UL << 24;
constexpr uint32_t DUAL_INPUT_MERGE_RIGHT_STICK = 1UL << 25;
constexpr uint8_t CLASSIC_DUAL_MERGE_BUTTON_COUNT = 31;

// The dedicated A/B merge record is allocated only by capable products.
// Keep this capability check beside the storage API so unsupported products can
// reject the setting before any EEPROM address is derived.
constexpr bool classicDualMergeSupportedOnProduct() {
#if defined(ADAPT_CAP_DUAL_INPUT_MERGE)
  return true;
#else
  return false;
#endif
}

extern uint32_t classic_dual_merge_p2_mask;

uint32_t defaultClassicDualMergeP2Mask(DeviceEnum mode);
bool classicDualMergeEnabledForMode(DeviceEnum mode);
uint32_t storedClassicDualMergeP2Mask(DeviceEnum mode);
uint32_t sanitizeClassicDualMergeP2Mask(DeviceEnum mode, uint32_t mask);
uint32_t getClassicDualMergeDisplayMask(DeviceEnum mode);
void loadClassicDualMergeConfigForMode(DeviceEnum mode);
void saveClassicDualMergeEnabledForMode(DeviceEnum mode, bool enabled);
void saveClassicDualMergeP2MaskForMode(DeviceEnum mode, uint32_t mask);
void clearClassicDualMergeConfig();
uint16_t classicDualMergeStorageRequiredEnd();
void classicDualMergePersistenceService();
bool classicDualMergePersistenceFlushNow();
void classicDualMergePersistenceDiscardDeferred();
bool classicDualMergePersistenceHasPending();

uint8_t getClassicDualMergeButtonCount(DeviceEnum mode);
uint32_t getClassicDualMergeButtonMask(DeviceEnum mode, uint8_t displayIndex);
const char* getClassicDualMergeButtonName(DeviceEnum mode, uint8_t displayIndex);
