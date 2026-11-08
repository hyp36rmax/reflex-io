#pragma once

#include "../feature_module.h"

#ifdef ENABLE_TTY2OLED_SERIAL
extern const FeatureModule kTty2OledFeatureModule;
bool tty2oledSerialDrainByte(uint8_t value, Print& out);
// While MiSTer Main holds a session on the port, CMDCOR, CMDAPD and CMDPIX
// are refused: their raw drain would swallow Main's lines.
void tty2oledSerialSetRawModesLocked(bool locked);
void tty2oledSerialEndRawModes();
// A framed Main session replaces the legacy raw-display owner.
void tty2oledSerialReleaseDisplay();
#endif
