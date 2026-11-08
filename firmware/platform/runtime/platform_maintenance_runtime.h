#pragma once

// Product-specific maintenance runs around the latency-critical USB transport
// sync without exposing updater or service-UART details to the shared loop.
void servicePlatformRuntimeMaintenanceHealth();
bool servicePlatformRuntimeExclusiveMaintenance();

