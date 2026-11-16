#pragma once

inline bool productQuiesceForFlash(unsigned long) { return true; }
inline void productReleaseFlashQuiesce() {}
// False while something else owns the watchdog's expiry and a flash write
// must not postpone it.
inline bool productFlashMayFeedWatchdog() { return true; }
