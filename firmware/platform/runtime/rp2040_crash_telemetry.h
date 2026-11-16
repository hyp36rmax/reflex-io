#pragma once

#include <stdint.h>
#include <Arduino.h>
#include "../../product_config.h"

class Print;  // host harnesses include this header without Arduino's Print

enum Rp2040CrashPhase : uint16_t {
  RP2040_CRASH_PHASE_NONE = 0,
  RP2040_CRASH_PHASE_BOOT = 1,
  RP2040_CRASH_PHASE_HARDWARE = 2,
  RP2040_CRASH_PHASE_STORAGE = 3,
  RP2040_CRASH_PHASE_INPUT = 4,
  RP2040_CRASH_PHASE_OUTPUT = 5,
  RP2040_CRASH_PHASE_USB_CONFIG = 6,
  RP2040_CRASH_PHASE_RUNTIME = 7,
  RP2040_CRASH_PHASE_WEBHID_GET = 8,
  RP2040_CRASH_PHASE_WEBHID_SET = 9,
  RP2040_CRASH_PHASE_ESP32_READY_ASSERT = 10,
  RP2040_CRASH_PHASE_ESP32_SPI_TRANSFER = 11,
  RP2040_CRASH_PHASE_ESP32_READY_RELEASE = 12,
  RP2040_CRASH_PHASE_INTENTIONAL_RESET = 13,
  RP2040_CRASH_PHASE_EARLY_RECOVERY_HOLD = 14,
};

// Consume a retained hard-fault record before normal firmware initialization.
// The retained copy is cleared after validation; the printable snapshot remains
// available for the rest of this boot.
void rp2040CrashTelemetryBootInit();

// Cheap breadcrumbs read directly by the hard-fault handler.
void rp2040CrashTelemetrySetPhase(uint16_t phase);
void rp2040CrashTelemetrySetReport(uint8_t report_id);

// A boot phase also reloads the boot watchdog, so each boot stage (the splash,
// initial input polling) gets the full 8 s instead of sharing one budget.
void rp2040CrashTelemetryBootPhase(uint16_t phase);

// The main loop's current stage: one store, kept across a watchdog timeout and
// printed as WDSTEP. Free on products without crash telemetry.
#if ADAPT_POLICY_RP2040_CRASH_TELEMETRY && defined(ARDUINO_ARCH_RP2040)
#include <pico/platform.h>
#endif
#if ADAPT_POLICY_RP2040_CRASH_TELEMETRY && defined(PICO_RP2040) && PICO_RP2040
extern volatile uint8_t rp2040RetainedLoopStep;
inline void rp2040CrashTelemetryMarkLoopStep(uint8_t step) { rp2040RetainedLoopStep = step; }
#else
inline void rp2040CrashTelemetryMarkLoopStep(uint8_t) {}
#endif

// Called from initVariant(), before USB/core1/setup. It snapshots the preceding
// reset and arms an early-boot watchdog. Modern2USB also holds 8 s after an
// unexpected runtime timeout so its ESP32 can restart; other products boot at once.
void rp2040CrashTelemetryEarlyBootArm(bool hold_after_unexpected_reset = true);

// MODERN2USB normally has no hardware watchdog owner. Feed a bounded runtime
// watchdog unless the RP2040 updater trial currently owns it. The retained
// phase breadcrumb is preserved across a timeout and printed by CRASH STATUS.
void rp2040CrashTelemetryRuntimeWatchdogTask(bool updater_trial_owns_watchdog);

// Handles CRASH, CRASH STATUS, and RESET STATUS on the shared serial surface.
bool rp2040CrashTelemetryHandleSerialCommand(const char* command, Print& out);
