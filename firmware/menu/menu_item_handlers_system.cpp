#include "menu_item_handlers_internal.h"

namespace menu_item_handlers_internal {

namespace {

// The remap answer, kept while the learned-map question is open.
bool factoryResetEraseRemaps = false;

void runFactoryReset(bool eraseRemaps, bool eraseLearnedMaps) {
  factory_reset_stage = 0;
  const bool resetComplete = factoryResetSettings(eraseRemaps, eraseLearnedMaps);
  if (resetComplete) {
    buzzer.playFactoryReset();
  } else {
    buzzer.playError();
  }

  drawFactoryResetResult(resetComplete, eraseRemaps, eraseLearnedMaps);
  delayWithCooperativeOled(1500);
  reboot();
}

}  // namespace

void handle_item_factory_reset(menu_item_action action) {
  switch (action) {
    case item_action_display:
      if (factory_reset_stage == 0) {
        display.print("Press to reset");
      } else {
        display.print("(Confirming...)");
      }
      break;
    case item_action_change:
      if (factory_reset_stage == 0) {
        factory_reset_stage = 1;
        factory_reset_selection = 0;
        drawFactoryResetPrompt(factory_reset_selection);
      } else if (factory_reset_stage == 1) {
        if (factory_reset_selection == 0) {
          factory_reset_stage = 0;
          buzzer.playCancel();
          return;
        }
        // Button remaps get their own answer; KEEP is the safe default.
        factory_reset_stage = 2;
        factory_reset_selection = 0;
        drawFactoryResetRemapPrompt(factory_reset_selection);
      } else if (factory_reset_stage == 2) {
        factoryResetEraseRemaps = factory_reset_selection == 1;
#if ADAPT_POLICY_RESERVED_FEEDBACK_STORAGE
        // Learned controller maps get their own answer too (owner,
        // 2026-11-08); KEEP is the safe default.
        factory_reset_stage = 3;
        factory_reset_selection = 0;
        drawFactoryResetLearnedMapPrompt(factory_reset_selection);
#else
        runFactoryReset(factoryResetEraseRemaps, true);
#endif
      } else if (factory_reset_stage == 3) {
        runFactoryReset(factoryResetEraseRemaps, factory_reset_selection == 1);
      }
      break;
    case item_action_change_prev:
      if (factory_reset_stage == 1) {
        factory_reset_selection = (factory_reset_selection + 1) % 2;
        drawFactoryResetPrompt(factory_reset_selection);
      } else if (factory_reset_stage == 2) {
        factory_reset_selection = (factory_reset_selection + 1) % 2;
        drawFactoryResetRemapPrompt(factory_reset_selection);
      } else if (factory_reset_stage == 3) {
        factory_reset_selection = (factory_reset_selection + 1) % 2;
        drawFactoryResetLearnedMapPrompt(factory_reset_selection);
      }
      break;
    default:
      break;
  }
}

void handle_item_bootloader(menu_item_action action) {
  switch (action) {
    case item_action_display:
      if (bootloader_stage == 0) {
        display.print("Enter");
      } else if (bootloader_stage == 1) {
        display.print("(Confirm...)");
      }
      break;
    case item_action_change:
      if (bootloader_stage == 0) {
        bootloader_stage = 1;
        drawBootloaderPrompt();
      } else if (bootloader_stage == 1) {
        bootloader_stage = 0;
        buzzer.playModeChange();
        drawBootloaderEnteringScreen();
        delayWithCooperativeOled(500);
        rebootToBootloader();
      }
      break;
    case item_action_reset:
      if (bootloader_stage == 1) {
        bootloader_stage = 0;
        display.clear();
        forceMainDisplayRefresh();
      }
      break;
    default:
      break;
  }
}

void handle_item_display_contrast(menu_item_action action) {
  switch (action) {
    case item_action_display:
      display.print(menu_display_contrast >= 254 ? 100 : (menu_display_contrast * 100) / 255);
      display.print('%');
      break;
    case item_action_change:
      if (menu_display_contrast <= 64) {
        menu_display_contrast = 128;
      } else if (menu_display_contrast <= 128) {
        menu_display_contrast = 192;
      } else if (menu_display_contrast <= 192) {
        menu_display_contrast = 254;
      } else {
        menu_display_contrast = 64;
      }
      display.setContrast(menu_display_contrast);
      break;
    case item_action_change_prev:
      if (menu_display_contrast >= 254) {
        menu_display_contrast = 192;
      } else if (menu_display_contrast >= 192) {
        menu_display_contrast = 128;
      } else if (menu_display_contrast >= 128) {
        menu_display_contrast = 64;
      } else {
        menu_display_contrast = 254;
      }
      display.setContrast(menu_display_contrast);
      break;
    case item_action_reset:
      menu_display_contrast = display_contrast;
      display.setContrast(menu_display_contrast);
      break;
    case item_action_apply:
      display_contrast = menu_display_contrast;
      break;
    case item_action_save:
      saveSystemSettingByte(SettingId::DisplayContrast, menu_display_contrast);
      break;
  }
}

}
