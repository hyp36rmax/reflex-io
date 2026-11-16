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
// Third step where a reset clears learned HID maps: KEEP (0) or ERASE (1).
void drawFactoryResetLearnedMapPrompt(uint8_t selection);
void drawModeDefaultsPrompt(uint8_t selection);
// The erase flags are the answers given at the KEEP/ERASE prompts; products
// without learned maps pass learnedMapsErased = true.
void drawFactoryResetResult(bool complete, bool remapsErased,
                            bool learnedMapsErased);
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
