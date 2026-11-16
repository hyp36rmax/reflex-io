#pragma once
#define ADAPT_PRODUCT_HAS_EARLY_VARIANT_INIT 1
#include "../../../platform/runtime/rp2040_crash_telemetry.h"
// Arms the 8 s boot watchdog in initVariant(), before USB, core1 and setup;
// boot phases reload it and the main loop then keeps it fed at 4 s. No
// post-timeout hold: that is for Modern2USB's ESP32. A no-op when
// ADAPT_POLICY_RP2040_CRASH_TELEMETRY is 0.
inline void productEarlyVariantInit() { rp2040CrashTelemetryEarlyBootArm(false); }
inline void productPrepareHardwareForBoot() {}
