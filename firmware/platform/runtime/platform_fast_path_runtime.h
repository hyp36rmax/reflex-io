#pragma once

// Product policy for latency-sensitive runtimes that must bypass deferred
// background services while retaining the shared poll/process/send scheduler.
bool usePlatformFastPathRuntime();
