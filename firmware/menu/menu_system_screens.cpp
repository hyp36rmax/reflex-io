#include "../product_config.h"

#include "menu_system_screens.h"

#include <string.h>

#include "menu_display_state.h"
#include "menu_system_product_services.h"
#include "menu_ui_state.h"
#include "../platform/display_runtime_state.h"

namespace {

constexpr char kWebUploadTarget[] = "WEB UPLOAD";

const char* firmwareUpdateTargetName(FirmwareUpdateTarget target) {
  switch (target) {
    case FirmwareUpdateTarget::Rp2040:
      return "RP2040";
    case FirmwareUpdateTarget::Esp32:
      return "ESP32";
    case FirmwareUpdateTarget::Web:
      return kWebUploadTarget;
    default:
      return "";
  }
}

FirmwareUpdatePhase firmwareUpdateDisplayPhase(FirmwareUpdateTarget target,
                                               FirmwareUpdatePhase phase) {
  return target == FirmwareUpdateTarget::Rp2040 &&
                 phase == FirmwareUpdatePhase::Complete
      ? FirmwareUpdatePhase::Applying
      : phase;
}

#ifdef USE_I2C_DISPLAY
uint8_t centeredTextX(const char* text, uint8_t characterWidth) {
  const size_t width = strlen(text) * characterWidth;
  return width < 128 ? static_cast<uint8_t>((128 - width) / 2) : 0;
}

void drawResetChoices(const char* const* options, uint8_t selection, uint8_t y) {
  const uint8_t optX[2] = { 8, 78 };
  const uint8_t optW[2] = {
    static_cast<uint8_t>(strlen(options[0]) * 6 + 10),
    static_cast<uint8_t>(strlen(options[1]) * 6 + 10),
  };

  u8g2.setFont(u8g2_font_6x10_tr);
  for (uint8_t i = 0; i < 2; ++i) {
    if (i == selection) {
      u8g2.drawBox(optX[i], y, optW[i], 14);
      u8g2.setDrawColor(0);
      u8g2.drawStr(optX[i] + 3, y + 10, options[i]);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawFrame(optX[i], y, optW[i], 14);
      u8g2.drawStr(optX[i] + 3, y + 10, options[i]);
    }
  }
}

void drawSafeResetChoices(uint8_t selection, uint8_t y) {
  static const char* const kOptions[2] = { "CANCEL", "RESET" };
  drawResetChoices(kOptions, selection, y);
}

// Factory reset button hint, in the setup guide's wording: the left button
// moves the selection, the right button confirms it.
void drawFactoryResetHint() {
  static const char kHint[] = "Left=Next  Right=OK";
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(centeredTextX(kHint, 5), 60, kHint);
}
#endif

}  // namespace

void firmwareUpdateProductPoll() {
  productFirmwareUpdatePoll();
}

void firmwareUpdateProductOnClear() {
  productFirmwareUpdateOnClear();
}

void drawFactoryResetPrompt(uint8_t selection) {
#ifndef USE_I2C_DISPLAY
  (void)selection;
  return;
#else
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(18, 14, "FACTORY RESET");

  u8g2.drawHLine(0, 18, 128);

  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(7, 32, "Reset all settings?");

  drawSafeResetChoices(selection, 36);

  drawFactoryResetHint();

  u8g2.sendBuffer();
#endif
}

void drawFactoryResetRemapPrompt(uint8_t selection) {
#ifndef USE_I2C_DISPLAY
  (void)selection;
  return;
#else
  static const char* const kOptions[2] = { "KEEP", "ERASE" };
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(18, 14, "FACTORY RESET");

  u8g2.drawHLine(0, 18, 128);

  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(4, 32, "Erase button remaps?");

  drawResetChoices(kOptions, selection, 36);

  drawFactoryResetHint();

  u8g2.sendBuffer();
#endif
}

void drawFactoryResetLearnedMapPrompt(uint8_t selection) {
#ifndef USE_I2C_DISPLAY
  (void)selection;
  return;
#else
  static const char* const kOptions[2] = { "KEEP", "ERASE" };
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(18, 14, "FACTORY RESET");

  u8g2.drawHLine(0, 18, 128);

  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(7, 32, "Erase learned maps?");

  drawResetChoices(kOptions, selection, 36);

  drawFactoryResetHint();

  u8g2.sendBuffer();
#endif
}

void drawModeDefaultsPrompt(uint8_t selection) {
#ifndef USE_I2C_DISPLAY
  (void)selection;
  return;
#else
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(18, 14, "MODE DEFAULTS");
  u8g2.drawHLine(0, 18, 128);

  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(7, 31, "Reset mode settings");
  u8g2.drawStr(40, 41, "and save?");

  drawSafeResetChoices(selection, 47);
  u8g2.sendBuffer();
#endif
}

void drawFactoryResetResult(bool complete, bool remapsErased,
                            bool learnedMapsErased) {
#ifndef USE_I2C_DISPLAY
  (void)complete;
  (void)remapsErased;
  (void)learnedMapsErased;
  return;
#else
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(complete ? 12 : 18, 20,
               complete ? "RESET COMPLETE!" : "PARTIAL RESET");

  // Two left-aligned lines, centred as a block. After a complete reset the
  // second line names whatever the user chose to KEEP.
  const char* first = "Settings cleared;";
  const char* second = "BT/Wi-Fi erase failed";
  if (complete && remapsErased && learnedMapsErased) {
    first = "All settings and";
    second = "remaps cleared.";
  } else if (complete && learnedMapsErased) {
    second = "remaps kept.";
  } else if (complete && remapsErased) {
    second = "learned maps kept.";
  } else if (complete) {
    second = "remaps & maps kept.";
  }
  const char* longer = strlen(first) >= strlen(second) ? first : second;
  const uint8_t x = centeredTextX(longer, 6);
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(x, 38, first);
  u8g2.drawStr(x, 50, second);

  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(30, 62, "Rebooting...");

  u8g2.sendBuffer();
#endif
}

void drawBootloaderPrompt() {
#ifndef USE_I2C_DISPLAY
  return;
#else
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(29, 14, "BOOTLOADER");

  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(4, 28, "Enter firmware update");
  u8g2.drawStr(4, 38, "mode? Device will");
  u8g2.drawStr(4, 48, "appear as USB drive.");

  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(4, 62, "MODE=Enter  RESET=Cancel");

  u8g2.sendBuffer();
#endif
}

void drawBootloaderEnteringScreen() {
  showFirmwareUpdateScreen(FirmwareUpdateTarget::Rp2040,
                           FirmwareUpdatePhase::Preparing);
}

void showFirmwareUpdateScreen(FirmwareUpdateTarget target,
                              FirmwareUpdatePhase phase) {
  ::showFirmwareUpdateScreen(firmwareUpdateTargetName(target),
                             firmwareUpdateDisplayPhase(target, phase));
}

void showFirmwareUpdateProgress(FirmwareUpdateTarget target,
                                FirmwareUpdatePhase phase,
                                uint8_t percent) {
  ::showFirmwareUpdateProgress(firmwareUpdateTargetName(target),
                               firmwareUpdateDisplayPhase(target, phase),
                               percent);
}

bool firmwareRollbackNoticeActive() {
  return productFirmwareRollbackNoticeActive();
}

bool acknowledgeFirmwareRollbackNotice() {
  if (!firmwareRollbackNoticeActive()) return true;
  const bool acknowledged = productAcknowledgeFirmwareRollbackNotice();
  if (acknowledged) {
    needsU8g2Clear = true;
    mainDisplayInitialized = false;
  }
  return acknowledged;
}

bool renderFirmwareRollbackNoticeIfActive() {
  if (!firmwareRollbackNoticeActive()) return false;
#ifdef USE_I2C_DISPLAY
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(centeredTextX("FIRMWARE ROLLBACK", 7), 14,
               "FIRMWARE ROLLBACK");
  u8g2.drawHLine(0, 18, 128);

  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(22, 31, "Update failed");
  u8g2.drawStr(10, 43, "Previous firmware");
  u8g2.drawStr(8, 54, "restored safely");

  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(centeredTextX("RIGHT BUTTON = OK", 5), 63,
               "RIGHT BUTTON = OK");
  u8g2.sendBuffer();
#endif
  return true;
}
