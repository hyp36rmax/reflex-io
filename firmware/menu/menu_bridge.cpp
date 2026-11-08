#include "../product_config.h"

#include "menu_bridge.h"
#include "menu_submenu_oled.h"
#include "menu_submenu_kiosk.h"
#include "menu_submenu_sound.h"
#include "menu_bridge_internal.h"

#include <Adafruit_TinyUSB.h>

#include "../core/firmware_support.h"
#include "../core/settings_store.h"
#include "../platform/buzzer.h"

#include "mapping_display.h"
#include "menu_catalog.h"
#include "menu_choice_list.h"
#include "menu_display_state.h"
#include "menu_helpers.h"
#include "menu_idle_runtime.h"
#include "menu_input.h"
#include "menu_item_handlers.h"
#include "menu_mode_state.h"
#include "menu_navigation.h"
#include "menu_list_navigation.h"
#include "menu_runtime_state.h"
#include "menu_setup_wizard.h"
#include "menu_mode_lists.h"
#include "menu_ui_state.h"
#include "menu_product_pages.h"

#include "../platform/display_runtime_state.h"
#include "../output/output_runtime_state.h"

namespace {

constexpr uint32_t kMenuActionFeedbackMs = 900;
MenuActionFeedback menuActionFeedback = MenuActionFeedback::None;
uint32_t menuActionFeedbackUntil = 0;
bool menuActionFeedbackDrawn = false;
bool menuSaveFailed = false;

void clearMenuBridgeDisplay() {
#ifdef USE_I2C_DISPLAY
  display.clear();
#endif
}

void showSavedRebootMessage() {
#ifdef USE_I2C_DISPLAY
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(15, 20, "SETTINGS SAVED");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(28, 38, "Rebooting...");
  u8g2.sendBuffer();
#endif
}

void saveSystemMenuAndReboot() {
  if (!saveMenuModeListsPreferences()) {
    showMenuActionFeedback(MenuActionFeedback::SaveFailed);
    buzzer.playError();
    return;
  }
  menuSaveAllItems();
  saveMenuModeSettings();
  buzzer.playSave();

  showSavedRebootMessage();

  uint32_t waitStart = millis();
  while (millis() - waitStart < 1500) {
    buzzer.update();
    delayWithCooperativeOled(5);
  }
  reboot();
}

}  // namespace

void closeMenu() {
  isMenuOpen = false;
  menu_navigation_state = 255;
  menu_selected_visible = 0;
  menu_scroll_offset = 0;
  menu_bottom_right_selected = false;
  mainDisplayInitialized = false;
  if (mappingDisplayActive) {
    mappingDisplayActive = false;
    deactivateMappingDisplay();
  }
  closeMenuChoiceList();
  games_submenu_active = false;
  oled_submenu_active = false;
  screensaver_submenu_active = false;
  sound_submenu_active = false;
  hotkeys_submenu_active = false;
  hotkeys_capture_active = false;
  kiosk_submenu_active = false;
  closeProductMenuPage();
  about_screen_active = false;
  whats_new_screen_active = false;
  factory_reset_stage = 0;
  bootloader_stage = 0;
  cancelSetupWizard();
  closeMenuModeLists();
  clearMenuBridgeDisplay();
}

void discardAndCloseMenu() {
  menuResetAllItems();
  closeMenu();
}

void saveAndCloseMenu() {
  finishMenuChoiceListForHold(true);
  // Holding Mode during first-boot setup saves the wizard's choices; the
  // System menu's own rows were never shown.
  if (setupWizardActive()) {
    (void)finishSetupWizard();
    return;
  }
  if (!saveMenuModeListsPreferences()) {
    showMenuActionFeedback(MenuActionFeedback::SaveFailed);
    buzzer.playError();
    return;
  }
  menuSaveAllItems();
  const bool rebootRequired = saveMenuModeSettings();
  buzzer.playSave();

  if (rebootRequired) {
    showSavedRebootMessage();
    waitWithBuzzerUpdates(250);
    reboot();
  }

  closeMenu();
  showMenuActionFeedback(MenuActionFeedback::Saved);
}

bool finishSystemMenuEditorForHold(bool save) {
  finishMenuChoiceListForHold(save);
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
  (void)finishOledSubmenuForHold(save);
  (void)finishKioskSubmenuForHold(save);
  (void)finishSoundSubmenuForHold(save);
  finishMenuModePickerForHold(save);
  // A hold must never confirm reset or bootloader actions.
  factory_reset_stage = 0;
  bootloader_stage = 0;
#else
  (void)save;
#endif
  return true;
}

void openFactoryResetConfirmation() {
  // Subpages and tools own the display and input. Keep their staged state
  // intact: the reset chord is available again after returning to the root.
  if (!isMenuOpen || factory_reset_stage != 0 || bootloader_stage != 0 ||
      setupWizardActive() || menuChoiceListActive() || menuModeListsActive() ||
      productMenuPageActive() ||
      sound_submenu_active || games_submenu_active || oled_submenu_active ||
      screensaver_submenu_active || hotkeys_submenu_active || hotkeys_capture_active ||
      kiosk_submenu_active || buttonRemapActive || mappingDisplayActive ||
      padTestActive || analogTestActive || pinDebugActive || jvsStatusActive ||
#if defined(ADAPT_BRIDGE_JVS_TO_USB)
      jvsPanelActive ||
#endif
      about_screen_active || whats_new_screen_active) {
    return;
  }
#ifdef USE_I2C_DISPLAY
  beginDisplayWire();
#endif
  handleMenuItemAction(menu_item_factory_reset, item_action_change);
#ifdef USE_I2C_DISPLAY
  endDisplayWire();
#endif
}

void noteMenuSaveFailed() {
  menuSaveFailed = true;
}

void showMenuActionFeedback(MenuActionFeedback feedback) {
  if (menuSaveFailed) {
    menuSaveFailed = false;
    feedback = MenuActionFeedback::SaveFailed;
    buzzer.playError();
  }
  if (feedback == MenuActionFeedback::Cancelled) {
    buzzer.playCancel();
  }
  menuActionFeedback = feedback;
  menuActionFeedbackUntil = millis() + kMenuActionFeedbackMs;
  menuActionFeedbackDrawn = false;
  mainDisplayInitialized = false;
  padDisplayNeedsRedraw = true;
}

bool renderMenuActionFeedback() {
  if (menuActionFeedback == MenuActionFeedback::None) {
    return false;
  }
  if ((int32_t)(millis() - menuActionFeedbackUntil) >= 0) {
    menuActionFeedback = MenuActionFeedback::None;
    mainDisplayInitialized = false;
    padDisplayNeedsRedraw = true;
    return false;
  }
#ifdef USE_I2C_DISPLAY
  if (!menuActionFeedbackDrawn) {
    display.clear();
    display.setFont(System5x7);
    display.set2X();
    switch (menuActionFeedback) {
      case MenuActionFeedback::Cancelled:
        display.setCursor(10, 3);
        display.print(F("CANCELLED"));
        break;
      case MenuActionFeedback::Saved:
        display.setCursor(34, 3);
        display.print(F("SAVED"));
        break;
      case MenuActionFeedback::SaveFailed:
        display.setCursor(16, 2);
        display.println(F("SAVE"));
        display.setCursor(16, 4);
        display.print(F("FAILED"));
        break;
      case MenuActionFeedback::DefaultsRestored:
        display.setCursor(16, 2);
        display.println(F("DEFAULTS"));
        display.setCursor(16, 4);
        display.print(F("RESTORED"));
        break;
      default:
        break;
    }
    display.flush();
    menuActionFeedbackDrawn = true;
  }
#endif
  return true;
}

void forceMainDisplayRefresh() {
  mainDisplayInitialized = false;
  padDisplayNeedsRedraw = true;
}

void showMenu(bool btnChangeJustPressed, bool btnNavigateJustPressed) {
  const uint8_t menu_invalid = 255;
  const uint8_t menu_reboot = 254;

  uint8_t& menu_state = menu_navigation_state;
  uint8_t& selected_visible = menu_selected_visible;
  uint8_t& scroll_offset = menu_scroll_offset;
  bool& bottom_right = menu_bottom_right_selected;

  MenuControllerInput ctrl = readMenuControllerInput();

  bool ctrlBack = ctrl.bJust;
  bool navDown = btnNavigateJustPressed || ctrl.downJust;
  bool navUp = ctrl.upJust;
  bool navRight = ctrl.rightJust;
  bool navLeft = ctrl.leftJust;
  bool navActivate = btnChangeJustPressed || ctrl.aJust;

  handleIdleUiActivity(
    btnChangeJustPressed || btnNavigateJustPressed ||
    ctrl.upJust || ctrl.downJust || ctrl.leftJust || ctrl.rightJust ||
    ctrl.aJust || ctrl.bJust || ctrl.startJust
  );

  if (handleMenuToolScreens(btnChangeJustPressed, btnNavigateJustPressed, ctrl,
      selected_visible, scroll_offset, bottom_right)) {
    return;
  }

  if (handleMenuSubmenus(btnChangeJustPressed, btnNavigateJustPressed, ctrl,
      selected_visible, scroll_offset, bottom_right)) {
    return;
  }

  if (menu_state == menu_reboot) {
    if (btnNavigateJustPressed) {
      #ifdef ADAPT_OUTPUT_USB_DEVICE
      tud_disconnect();
      #endif
      clearMenuBridgeDisplay();
      delayWithCooperativeOled(1000);
      reboot();
    }
    return;
  }

  if (menu_state == menu_invalid) {
    menu_state = 0;
    selected_visible = 0;
    scroll_offset = 0;
    bottom_right = false;
    menuResetAllItems();
    clearMenuBridgeDisplay();
    renderMenu(selected_visible, scroll_offset, bottom_right);
    return;
  }

  uint8_t visible_count = getVisibleItemCount();
  const uint8_t item_rows = 6;
  bool on_bottom_row = (selected_visible >= visible_count);

  bool in_confirmation = (factory_reset_stage != 0) || (bootloader_stage == 1);

  if (ctrl.startJust && !in_confirmation) {
    saveSystemMenuAndReboot();
    return;
  }

  if (navDown && !in_confirmation) {
    buzzer.playMenuNav();
    if (on_bottom_row) {
      if (!bottom_right) {
        bottom_right = true;
      } else {
        bottom_right = false;
        selected_visible = 0;
        scroll_offset = 0;
      }
    } else {
      selected_visible++;
      if (selected_visible >= visible_count) {
        bottom_right = false;
      }
    }
    scroll_offset = menu_list_navigation::firstVisible(selected_visible, visible_count, item_rows);
    renderMenu(selected_visible, scroll_offset, bottom_right);
  }

  if (navUp && !in_confirmation) {
    buzzer.playMenuNav();
    if (on_bottom_row) {
      selected_visible = visible_count - 1;
      bottom_right = false;
    } else if (selected_visible > 0) {
      selected_visible--;
    } else {
      selected_visible = visible_count;
      bottom_right = true;
    }
    scroll_offset = menu_list_navigation::firstVisible(selected_visible, visible_count, item_rows);
    renderMenu(selected_visible, scroll_offset, bottom_right);
  }

  if (on_bottom_row && (navLeft || navRight)) {
    buzzer.playMenuNav();
    if (navRight && !bottom_right) {
      bottom_right = true;
    } else if (navLeft && bottom_right) {
      bottom_right = false;
    }
    renderMenu(selected_visible, scroll_offset, bottom_right);
    return;
  }

  if (ctrlBack && in_confirmation) {
    buzzer.playError();
    factory_reset_stage = 0;
    bootloader_stage = 0;
    clearMenuBridgeDisplay();
    renderMenu(selected_visible, scroll_offset, bottom_right);
    return;
  }

  if (ctrlBack) {
    buzzer.playError();
    menuResetAllItems();
    menu_state = menu_invalid;
    closeMenu();
    return;
  }

  if (factory_reset_stage != 0) {
    // Factory reset owns its two-button rows while open. The physical
    // Navigate button and controller D-pad Left/Right both move the safe
    // default selection; the configured controller OK/Back buttons continue
    // to confirm/cancel.
    if (btnNavigateJustPressed || navLeft || navRight) {
      buzzer.playMenuNav();
      handleMenuItemAction(menu_item_factory_reset, item_action_change_prev);
    }
    if (navActivate) {
      handleMenuItemAction(menu_item_factory_reset, item_action_change);
      if (factory_reset_stage == 0) {
        clearMenuBridgeDisplay();
        renderMenu(selected_visible, scroll_offset, bottom_right);
      }
    }
    return;
  }

  if (navActivate) {
    if (on_bottom_row) {
      if (!bottom_right) {
        menuResetAllItems();
        menu_state = menu_invalid;
        closeMenu();
        showMenuActionFeedback(MenuActionFeedback::Cancelled);
        return;
      } else {
        saveSystemMenuAndReboot();
        return;
      }
    }

    uint8_t actual_idx = getActualIndex(selected_visible);
    menu_item_enum current_item = menu_items[actual_idx].item;
    if (handleMenuValueChange(current_item, item_action_change, selected_visible, scroll_offset, bottom_right, item_rows)) {
      return;
    }
  }

  if (navRight && !on_bottom_row) {
    uint8_t actual_idx = getActualIndex(selected_visible);
    menu_item_enum current_item = menu_items[actual_idx].item;
    if (menuItemSupportsDirectionalAdjustment(current_item) &&
        handleMenuValueChange(current_item, item_action_change, selected_visible, scroll_offset, bottom_right, item_rows)) {
      return;
    }
  }

  if (navLeft && !on_bottom_row) {
    uint8_t actual_idx = getActualIndex(selected_visible);
    menu_item_enum current_item = menu_items[actual_idx].item;
    if (menuItemSupportsDirectionalAdjustment(current_item) &&
        handleMenuValueChange(current_item, item_action_change_prev, selected_visible, scroll_offset, bottom_right, item_rows)) {
      return;
    }
  }
}
