#include "../product_config.h"

#include "menu_firmware_update_screen.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "menu_display_state.h"
#include "menu_ui_state.h"
#include "../platform/display_runtime_state.h"

namespace {

constexpr size_t kFirmwareUpdateTargetCapacity = 16;
char firmwareUpdateTargetLabel[kFirmwareUpdateTargetCapacity] = {};
FirmwareUpdatePhase firmwareUpdatePhase = FirmwareUpdatePhase::Preparing;
uint32_t firmwareUpdateCompletionDeadlineMs = 0;
bool firmwareUpdateScreenDirty = false;
uint8_t firmwareUpdateProgressPercent = 0xFF;

#ifdef USE_I2C_DISPLAY
constexpr uint8_t kFirmwareProgressX = 8;
constexpr uint8_t kFirmwareProgressY = 48;
constexpr uint8_t kFirmwareProgressWidth = 112;
constexpr uint8_t kFirmwareProgressHeight = 7;
#endif

const char* firmwareUpdatePhaseName(FirmwareUpdatePhase phase) {
  switch (phase) {
    case FirmwareUpdatePhase::Preparing:
      return "Preparing...";
    case FirmwareUpdatePhase::Programming:
      return "Programming...";
    case FirmwareUpdatePhase::Verifying:
      return "Verifying...";
    case FirmwareUpdatePhase::Complete:
      return "100% complete";
    case FirmwareUpdatePhase::Failed:
      return "Update failed";
    case FirmwareUpdatePhase::Uploading:
      return "Uploading...";
    case FirmwareUpdatePhase::Applying:
      return "Applying update";
    default:
      return "";
  }
}

#ifdef USE_I2C_DISPLAY
uint8_t centeredFirmwareUpdateTextX(const char* text, uint8_t characterWidth) {
  const size_t width = strlen(text) * characterWidth;
  return width < 128 ? static_cast<uint8_t>((128 - width) / 2) : 0;
}

void drawFirmwareUpdateScreen() {
  const char* phase = firmwareUpdatePhaseName(firmwareUpdatePhase);
  char phaseWithProgress[22] = {};
  if (firmwareUpdateProgressPercent <= 100 &&
      firmwareUpdatePhase == FirmwareUpdatePhase::Programming) {
    snprintf(phaseWithProgress, sizeof(phaseWithProgress),
             "Programming... %u%%", firmwareUpdateProgressPercent);
    phase = phaseWithProgress;
  }

  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(centeredFirmwareUpdateTextX("FIRMWARE UPDATE", 7), 14,
               "FIRMWARE UPDATE");
  u8g2.drawHLine(0, 18, 128);

  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(centeredFirmwareUpdateTextX(firmwareUpdateTargetLabel, 6), 31,
               firmwareUpdateTargetLabel);
  u8g2.drawStr(centeredFirmwareUpdateTextX(phase, 6), 44, phase);

  const bool showProgressBar = firmwareUpdateProgressPercent <= 100;
  if (showProgressBar) {
    constexpr uint8_t kProgressInteriorWidth = kFirmwareProgressWidth - 2;
    const uint8_t filledWidth = static_cast<uint8_t>(
        (static_cast<uint16_t>(kProgressInteriorWidth) *
             firmwareUpdateProgressPercent +
         50u) /
        100u);
    u8g2.drawFrame(kFirmwareProgressX, kFirmwareProgressY,
                   kFirmwareProgressWidth, kFirmwareProgressHeight);
    if (filledWidth != 0) {
      u8g2.drawBox(kFirmwareProgressX + 1, kFirmwareProgressY + 1,
                   filledWidth, kFirmwareProgressHeight - 2);
    }
  }

  u8g2.setFont(u8g2_font_5x7_tr);
  const char* footer = firmwareUpdatePhase == FirmwareUpdatePhase::Complete
      ? "Safe to continue"
      : firmwareUpdatePhase == FirmwareUpdatePhase::Failed
          ? "Check the computer"
          : "Do not disconnect";
  u8g2.drawStr(centeredFirmwareUpdateTextX(footer, 5),
               showProgressBar ? 63 : 60, footer);
  u8g2.sendBuffer();
}
#endif

void setFirmwareUpdateProgress(const char* targetLabel,
                               FirmwareUpdatePhase phase,
                               uint8_t percent,
                               bool drawImmediately) {
  if (targetLabel == nullptr || targetLabel[0] == '\0') {
    clearFirmwareUpdateScreen();
    return;
  }

  strncpy(firmwareUpdateTargetLabel, targetLabel,
          kFirmwareUpdateTargetCapacity - 1);
  firmwareUpdateTargetLabel[kFirmwareUpdateTargetCapacity - 1] = '\0';
  firmwareUpdatePhase = phase;
  firmwareUpdateProgressPercent = percent <= 100 ? percent : 0xFF;
  firmwareUpdateCompletionDeadlineMs =
      (phase == FirmwareUpdatePhase::Complete ||
       phase == FirmwareUpdatePhase::Applying)
          ? millis() + 3000u
          : 0;
  firmwareUpdateScreenDirty = true;
  mainDisplayInitialized = false;

#ifdef USE_I2C_DISPLAY
  if (drawImmediately) {
    beginDisplayWire();
    drawFirmwareUpdateScreen();
    endDisplayWire();
    firmwareUpdateScreenDirty = false;
  }
#else
  (void)drawImmediately;
#endif
}

bool expireCompletedFirmwareUpdateScreen() {
  if (!firmwareUpdateScreenHasTarget() ||
      firmwareUpdateCompletionDeadlineMs == 0 ||
      static_cast<int32_t>(millis() - firmwareUpdateCompletionDeadlineMs) < 0) {
    return false;
  }

  clearFirmwareUpdateScreen();
  return true;
}

}  // namespace

void showFirmwareUpdateScreen(const char* targetLabel,
                              FirmwareUpdatePhase phase) {
  showFirmwareUpdateProgress(targetLabel, phase, 0xFF);
}

void showFirmwareUpdateProgress(const char* targetLabel,
                                FirmwareUpdatePhase phase,
                                uint8_t percent) {
  setFirmwareUpdateProgress(targetLabel, phase, percent, true);
}

void queueFirmwareUpdateScreen(const char* targetLabel,
                               FirmwareUpdatePhase phase) {
  setFirmwareUpdateProgress(targetLabel, phase, 0xFF, false);
}

void clearFirmwareUpdateScreen() {
  firmwareUpdateTargetLabel[0] = '\0';
  firmwareUpdateCompletionDeadlineMs = 0;
  firmwareUpdateScreenDirty = false;
  firmwareUpdateProgressPercent = 0xFF;
  needsU8g2Clear = true;
  mainDisplayInitialized = false;
  firmwareUpdateProductOnClear();
}

bool firmwareUpdateScreenHasTarget() {
  return firmwareUpdateTargetLabel[0] != '\0';
}

bool firmwareUpdateScreenMatchesTarget(const char* targetLabel) {
  return targetLabel != nullptr &&
         strcmp(firmwareUpdateTargetLabel, targetLabel) == 0;
}

bool firmwareUpdateScreenActive() {
  firmwareUpdateProductPoll();
  expireCompletedFirmwareUpdateScreen();
  return firmwareUpdateScreenHasTarget();
}

bool renderFirmwareUpdateScreenIfActive() {
  if (!firmwareUpdateScreenActive()) {
    return false;
  }
#ifdef USE_I2C_DISPLAY
  if (firmwareUpdateScreenDirty) {
    drawFirmwareUpdateScreen();
    firmwareUpdateScreenDirty = false;
  }
#endif
  return true;
}
