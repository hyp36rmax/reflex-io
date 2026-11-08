#pragma once

// Product diagnostics may temporarily own output delivery without leaking
// their protocol into the shared scheduler.
bool runPlatformPriorityOutputOverride();
void servicePlatformPriorityOutput();
void preparePlatformPriorityOutputFrame();
void finishPlatformPriorityOutputFrame();
