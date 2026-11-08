#pragma once

#include <stdint.h>

// System Settings > OLED: Inactive (Screensaver/Blank OLED), Inactive timer
// and the Screensaver picker. Returns true while the page owns input and the
// display.
bool handleOledSubmenu(
    uint8_t selected_visible,
    uint8_t scroll_offset,
    bool bottom_right,
    bool ctrlDownJust,
    bool ctrlUpJust,
    bool ctrlLeftJust,
    bool ctrlAJust,
    bool ctrlRightJust,
    bool ctrlBJust,
    bool ctrlStartJust,
    bool btnNavigateJustPressed,
    bool btnChangeJustPressed);

// Finalize the active OLED draft before the outer System hold action.
bool finishOledSubmenuForHold(bool save);
