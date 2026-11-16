#pragma once
#include "../../../platform/runtime/rp2040_crash_telemetry.h"
// A main loop stuck for 4 s reboots the unit instead of needing a replug;
// CRASH STATUS then reports the phase and the loop step that stopped.
inline void productRuntimeMaintenanceHealth() { rp2040CrashTelemetryRuntimeWatchdogTask(false); }
inline bool productRuntimeExclusiveMaintenance() { return false; }
