#pragma once

enum class MenuActionFeedback : unsigned char {
  None = 0,
  Cancelled,
  Saved,
  SaveFailed,
  DefaultsRestored,
};

void closeMenu();
void discardAndCloseMenu();
void saveAndCloseMenu();
void openFactoryResetConfirmation();
bool finishSystemMenuEditorForHold(bool save);
// The save that follows could not store a remap. The next feedback says so.
void noteMenuSaveFailed();
void showMenuActionFeedback(MenuActionFeedback feedback);
bool renderMenuActionFeedback();
void forceMainDisplayRefresh();
void renderMainDisplay();
void showMenu(bool btnChangeJustPressed, bool btnNavigateJustPressed);
