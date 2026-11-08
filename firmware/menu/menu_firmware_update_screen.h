#pragma once

#include <stdint.h>

enum class FirmwareUpdatePhase : uint8_t {
  Preparing = 0,
  Programming,
  Verifying,
  Complete,
  Failed,
  Uploading,
  Applying,
};

void showFirmwareUpdateScreen(const char* targetLabel,
                              FirmwareUpdatePhase phase);
void showFirmwareUpdateProgress(const char* targetLabel,
                                FirmwareUpdatePhase phase,
                                uint8_t percent);
void queueFirmwareUpdateScreen(const char* targetLabel,
                               FirmwareUpdatePhase phase);
void clearFirmwareUpdateScreen();
bool firmwareUpdateScreenHasTarget();
bool firmwareUpdateScreenMatchesTarget(const char* targetLabel);
bool firmwareUpdateScreenActive();
bool renderFirmwareUpdateScreenIfActive();

// Product code owns transport discovery and cleanup. Classic2USB implements
// no-op hooks; modern-controller products use them for optional transports.
void firmwareUpdateProductPoll();
void firmwareUpdateProductOnClear();
