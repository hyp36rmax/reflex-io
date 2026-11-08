#pragma once

#include <Arduino.h>
#include <stdint.h>

enum WiiTraceEvent : uint8_t {
  WII_TRACE_EVENT_CONNECT = 1,
  WII_TRACE_EVENT_UPDATE,
  WII_TRACE_EVENT_DISCONNECT,
  WII_TRACE_EVENT_PORT,
  WII_TRACE_EVENT_POLL,
  WII_TRACE_EVENT_STAGE_RAW,
  WII_TRACE_EVENT_FLUSH_RAW,
};

// The 5 KB sample ring is a bench diagnostic. Production builds compile it
// out; the diagnostics env defines ENABLE_WII_INPUT_TRACE to keep it.
#if defined(ENABLE_WII_INPUT_TRACE)
extern bool g_wiiTraceEnabled;

inline bool wiiTraceIsEnabled() {
  return g_wiiTraceEnabled;
}

void wiiTraceClear();
void wiiTraceRecord(uint8_t event, uint8_t port, uint32_t startUs, uint32_t endUs,
                    uint8_t ok, uint8_t failCount, uint8_t controllerType);
void wiiTraceRecordInstant(uint8_t event, uint8_t port, uint8_t ok,
                           uint8_t failCount, uint8_t controllerType);
bool handleWiiTraceCommand(const char* command, Print& out);
#else
inline bool wiiTraceIsEnabled() { return false; }
inline void wiiTraceClear() {}
inline void wiiTraceRecord(uint8_t, uint8_t, uint32_t, uint32_t, uint8_t, uint8_t, uint8_t) {}
inline void wiiTraceRecordInstant(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t) {}
inline bool handleWiiTraceCommand(const char*, Print&) { return false; }
#endif
