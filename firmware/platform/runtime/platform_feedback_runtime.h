#pragma once

// Platform-owned feedback and display runtime helpers that run after input has
// been processed for the current frame.

void runPlatformFeedbackServices(bool updated);
void runJvsSafeWindowDisplayIfNeeded();
bool runPs5DiagAuthStatusDisplay();
bool runPs5DiagAuthStatusDisplayForced();
