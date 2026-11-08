#pragma once

// Service management UART and deferred CDC diagnostics after the
// latency-critical controller-to-output path has completed.
void runDeferredRuntimeServices();
