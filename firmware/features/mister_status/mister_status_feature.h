#pragma once

#include "../feature_module.h"

// Called by the MiSTer serial port after its transport has accepted a STATUS
// record. The state is deliberately feature-owned so serial transport stays
// useful without an OLED.
void misterStatusReceive(char** fields, uint8_t count, uint32_t now);
void misterStatusDisconnected();

extern const FeatureModule kMisterStatusFeatureModule;
