#pragma once

// Product UI work that may consume controller input before menu hotkeys.
bool servicePlatformControllerUiBeforeMenuHotkeys();

// Product-specific physical input connection activity used to wake/reset idle UI.
bool platformPhysicalInputConnectionChanged(bool initialized);
