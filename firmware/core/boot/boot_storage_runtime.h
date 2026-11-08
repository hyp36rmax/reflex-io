#pragma once

void beginPersistentStorage();
bool loadPersistedBootConfiguration();
void initializeRuntimeSettingsFromStorage();

#include <stdint.h>

// End of every EEPROM record this build places, and whether it fits the 4 KiB
// Arduino-Pico EEPROM sector (checked at boot; tools/test_eeprom_layout.py
// asserts it per product so an overflowing layout fails before release).
uint16_t persistentStorageOccupiedEnd();
extern bool persistentStorageLayoutFits;
