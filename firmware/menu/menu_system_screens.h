#pragma once

#include <stdint.h>

#include "menu_firmware_update_screen.h"

enum class FirmwareUpdateTarget : uint8_t {
  None = 0,
  Rp2040,
  Esp32,
  Web,
};

void drawFactoryResetPrompt(uint8_t selection);
// Second factory reset step: KEEP (0) or ERASE (1) the button remaps.
void drawFactoryResetRemapPrompt(uint8_t selection);
void drawModeDefaultsPrompt(uint8_t selection);
void drawFactoryResetResult(bool complete);
void drawBootloaderPrompt();
void drawBootloaderEnteringScreen();
void showFirmwareUpdateScreen(FirmwareUpdateTarget target,
                              FirmwareUpdatePhase phase);
void showFirmwareUpdateProgress(FirmwareUpdateTarget target,
                                FirmwareUpdatePhase phase,
                                uint8_t percent);
bool firmwareRollbackNoticeActive();
bool acknowledgeFirmwareRollbackNotice();
bool renderFirmwareRollbackNoticeIfActive();
