#pragma once

#include <stdint.h>
#include "../../core/device_mode.h"

// Adjust the normal input warm-up for product-specific boot transports.
uint32_t platformAdjustInitialInputPollingDuration(uint32_t durationMs);

// Start any product-specific secondary USB-host input bridge needed at boot.
void platformSetupInputUsbBridgeForBoot(bool outputUsbReady);

// Decide whether input identity must be resolved before USB enumeration.
bool platformShouldResolveAutomaticInputBeforeUsb(bool autoOutputResolvedBoot);

// Decide whether a pre-USB autodetection result has the required identity.
bool platformAcceptAutomaticInputBeforeUsb(DeviceEnum detectedMode);
