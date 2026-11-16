#include "../../product_config.h"

#include "rp2040_crash_telemetry.h"

#include <Arduino.h>

#include "../../core/serial_command_parser.h"
#include "../../firmware_platform_config.h"

#if ADAPT_POLICY_RP2040_CRASH_TELEMETRY
#include <pico/platform.h>
#endif

#if ADAPT_POLICY_RP2040_CRASH_TELEMETRY && \
    defined(PICO_RP2040) && PICO_RP2040

#include <hardware/regs/psm.h>
#include <hardware/regs/watchdog.h>
#include <hardware/watchdog.h>
#include <hardware/structs/psm.h>
#include <hardware/structs/sio.h>
#include <hardware/structs/watchdog.h>

namespace {

constexpr uint32_t kCrashMagic = 0x52464348u;  // "RFCH"
constexpr uint16_t kCrashVersion = 1;
constexpr uint32_t kWatchdogBreadcrumbMagic = 0x52465744u;  // "RFWD"
constexpr uint16_t kWatchdogBreadcrumbVersion = 1;
constexpr uint32_t kRuntimeWatchdogTimeoutMs = 4000;
// watchdog_enable() clamps loads above 8,388 ms (RP2040-E1); the former
// 12000 ms request was 8.4 s in effect.
constexpr uint32_t kEarlyBootWatchdogTimeoutMs = 8000;
static_assert(kRuntimeWatchdogTimeoutMs <= 0xFFFFFFu / 2000u &&
                  kEarlyBootWatchdogTimeoutMs <= 0xFFFFFFu / 2000u,
              "RP2040 watchdog loads above 8,388 ms are silently clamped");
constexpr uint32_t kUnexpectedResetStabilizationMs = 8000;
constexpr uintptr_t kRp2040SramStart = 0x20000000u;
constexpr uintptr_t kRp2040SramEnd = 0x20042000u;

struct Rp2040CrashRecord {
  uint32_t magic;
  uint32_t magic_inverse;
  uint16_t version;
  uint16_t size;
  uint32_t core;
  uint32_t phase_report_flags;
  uint32_t exception_lr;
  uint32_t stack_pointer;
  uint32_t stacked_lr;
  uint32_t stacked_pc;
  uint32_t stacked_xpsr;
  uint32_t checksum;
};

struct Rp2040WatchdogBreadcrumb {
  uint32_t magic;
  uint32_t magic_inverse;
  uint16_t version;
  uint16_t size;
  uint16_t phase;
  uint16_t reserved;
  uint32_t checksum;
};

static_assert(sizeof(Rp2040CrashRecord) == 44,
              "Retained crash record layout changed");
static_assert(sizeof(Rp2040WatchdogBreadcrumb) == 20,
              "Retained watchdog breadcrumb layout changed");

// Pico SDK's .uninitialized_data section is NOLOAD and is not zeroed by crt0.
// The fault reset deliberately excludes SRAM banks from PSM_WDSEL.
__attribute__((section(".uninitialized_data.rp2040_crash"), used, aligned(4)))
volatile Rp2040CrashRecord retainedCrashRecord;
__attribute__((section(".uninitialized_data.rp2040_watchdog"), used, aligned(4)))
volatile Rp2040WatchdogBreadcrumb retainedWatchdogBreadcrumb;
uint8_t lastLoopStep = 0;

volatile uint16_t currentCrashPhase;
volatile uint8_t currentCrashReport;

Rp2040CrashRecord lastCrashRecord = {};
bool lastCrashValid = false;
bool retainedCrashCorrupt = false;
uint32_t lastResetReason = 0;
uint16_t lastWatchdogPhase = RP2040_CRASH_PHASE_NONE;
bool lastWatchdogBreadcrumbValid = false;
bool bootTelemetryInitialized = false;
bool runtimeWatchdogConfigured = false;

__attribute__((always_inline)) static inline uint32_t mixCrashWord(
    uint32_t hash, uint32_t value) {
  hash ^= value;
  hash *= 16777619u;
  return hash;
}

__attribute__((always_inline)) static inline uint32_t crashRecordChecksum(
    const volatile Rp2040CrashRecord& record) {
  uint32_t hash = 2166136261u;
  hash = mixCrashWord(hash, record.magic_inverse);
  hash = mixCrashWord(hash,
                      (uint32_t)record.version | ((uint32_t)record.size << 16));
  hash = mixCrashWord(hash, record.core);
  hash = mixCrashWord(hash, record.phase_report_flags);
  hash = mixCrashWord(hash, record.exception_lr);
  hash = mixCrashWord(hash, record.stack_pointer);
  hash = mixCrashWord(hash, record.stacked_lr);
  hash = mixCrashWord(hash, record.stacked_pc);
  hash = mixCrashWord(hash, record.stacked_xpsr);
  return hash;
}

uint32_t watchdogBreadcrumbChecksum(
    const volatile Rp2040WatchdogBreadcrumb& record) {
  uint32_t hash = 2166136261u;
  hash = mixCrashWord(hash, record.magic_inverse);
  hash = mixCrashWord(hash,
                      (uint32_t)record.version | ((uint32_t)record.size << 16));
  hash = mixCrashWord(hash,
                      (uint32_t)record.phase | ((uint32_t)record.reserved << 16));
  return hash;
}

bool retainedWatchdogBreadcrumbIsValid() {
  return retainedWatchdogBreadcrumb.magic == kWatchdogBreadcrumbMagic &&
         retainedWatchdogBreadcrumb.magic_inverse ==
             ~kWatchdogBreadcrumbMagic &&
         retainedWatchdogBreadcrumb.version == kWatchdogBreadcrumbVersion &&
         retainedWatchdogBreadcrumb.size ==
             sizeof(Rp2040WatchdogBreadcrumb) &&
         retainedWatchdogBreadcrumb.checksum ==
             watchdogBreadcrumbChecksum(retainedWatchdogBreadcrumb);
}

void clearRetainedWatchdogBreadcrumb() {
  retainedWatchdogBreadcrumb.magic = 0;
  retainedWatchdogBreadcrumb.magic_inverse = 0;
  retainedWatchdogBreadcrumb.checksum = 0;
}

void writeRetainedWatchdogBreadcrumb(uint16_t phase) {
  retainedWatchdogBreadcrumb.magic = 0;
  retainedWatchdogBreadcrumb.magic_inverse = ~kWatchdogBreadcrumbMagic;
  retainedWatchdogBreadcrumb.version = kWatchdogBreadcrumbVersion;
  retainedWatchdogBreadcrumb.size = sizeof(Rp2040WatchdogBreadcrumb);
  retainedWatchdogBreadcrumb.phase = phase;
  retainedWatchdogBreadcrumb.reserved = 0;
  retainedWatchdogBreadcrumb.checksum =
      watchdogBreadcrumbChecksum(retainedWatchdogBreadcrumb);
  __asm volatile("dmb" ::: "memory");
  retainedWatchdogBreadcrumb.magic = kWatchdogBreadcrumbMagic;
  __asm volatile("dmb" ::: "memory");
}

bool retainedRecordValid() {
  return retainedCrashRecord.magic == kCrashMagic &&
         retainedCrashRecord.magic_inverse == ~kCrashMagic &&
         retainedCrashRecord.version == kCrashVersion &&
         retainedCrashRecord.size == sizeof(Rp2040CrashRecord) &&
         retainedCrashRecord.checksum == crashRecordChecksum(retainedCrashRecord);
}

void clearRetainedRecord() {
  retainedCrashRecord.magic = 0;
  retainedCrashRecord.magic_inverse = 0;
  retainedCrashRecord.checksum = 0;
}

void copyRetainedRecord() {
  lastCrashRecord.magic = retainedCrashRecord.magic;
  lastCrashRecord.magic_inverse = retainedCrashRecord.magic_inverse;
  lastCrashRecord.version = retainedCrashRecord.version;
  lastCrashRecord.size = retainedCrashRecord.size;
  lastCrashRecord.core = retainedCrashRecord.core;
  lastCrashRecord.phase_report_flags = retainedCrashRecord.phase_report_flags;
  lastCrashRecord.exception_lr = retainedCrashRecord.exception_lr;
  lastCrashRecord.stack_pointer = retainedCrashRecord.stack_pointer;
  lastCrashRecord.stacked_lr = retainedCrashRecord.stacked_lr;
  lastCrashRecord.stacked_pc = retainedCrashRecord.stacked_pc;
  lastCrashRecord.stacked_xpsr = retainedCrashRecord.stacked_xpsr;
  lastCrashRecord.checksum = retainedCrashRecord.checksum;
}

void printHex32(Print& out, uint32_t value) {
  out.print(F("0x"));
  out.print((unsigned long)value, HEX);
}

}  // namespace

// Outside the namespace: the header's inline marker writes it directly.
__attribute__((section(".uninitialized_data.rp2040_loop_step"), used))
volatile uint8_t rp2040RetainedLoopStep;

namespace {
// The last unexpected reset (watchdog timeout or hard fault) since power-on.
// Intentional restarts keep it, so an identity restart right after a hang
// cannot erase the report the way it replaces the one-boot breadcrumb.
constexpr uint32_t kLastResetMagic = 0x52464C52u;  // "RFLR"
constexpr uint8_t kLastResetWatchdog = 1;
constexpr uint8_t kLastResetFault = 2;
struct Rp2040LastResetRecord {
  uint32_t magic;
  uint32_t magic_inverse;
  uint8_t kind;
  uint8_t step;
  uint16_t phase;
  uint32_t pc;
  uint32_t count;
  uint32_t checksum;
};
__attribute__((section(".uninitialized_data.rp2040_last_reset"), used, aligned(4)))
volatile Rp2040LastResetRecord retainedLastReset;
Rp2040LastResetRecord lastReset = {};
bool lastResetValid = false;

uint32_t lastResetChecksum(const volatile Rp2040LastResetRecord& record) {
  uint32_t hash = 2166136261u;
  hash = mixCrashWord(hash, record.magic_inverse);
  hash = mixCrashWord(hash, (uint32_t)record.kind | ((uint32_t)record.step << 8) |
                                ((uint32_t)record.phase << 16));
  hash = mixCrashWord(hash, record.pc);
  hash = mixCrashWord(hash, record.count);
  return hash;
}

bool retainedLastResetValid() {
  return retainedLastReset.magic == kLastResetMagic &&
         retainedLastReset.magic_inverse == ~kLastResetMagic &&
         retainedLastReset.checksum == lastResetChecksum(retainedLastReset);
}

void recordLastUnexpectedReset(uint8_t kind, uint16_t phase, uint8_t step, uint32_t pc) {
  const uint32_t count = retainedLastResetValid() ? retainedLastReset.count + 1 : 1;
  retainedLastReset.magic = 0;
  retainedLastReset.magic_inverse = ~kLastResetMagic;
  retainedLastReset.kind = kind;
  retainedLastReset.step = step;
  retainedLastReset.phase = phase;
  retainedLastReset.pc = pc;
  retainedLastReset.count = count;
  retainedLastReset.checksum = lastResetChecksum(retainedLastReset);
  __asm volatile("dmb" ::: "memory");
  retainedLastReset.magic = kLastResetMagic;
  __asm volatile("dmb" ::: "memory");
}
}  // namespace

extern "C" __attribute__((noreturn, noinline, used,
                           section(".time_critical.rp2040_crash")))
void rp2040CrashHardFaultFromStack(uint32_t* stack, uint32_t exception_lr) {
  retainedCrashRecord.magic = 0;
  retainedCrashRecord.magic_inverse = ~kCrashMagic;
  retainedCrashRecord.version = kCrashVersion;
  retainedCrashRecord.size = sizeof(Rp2040CrashRecord);
  retainedCrashRecord.core = sio_hw->cpuid & 1u;

  const uintptr_t stackAddress = (uintptr_t)stack;
  const bool stackValid = (stackAddress & 3u) == 0u &&
                          stackAddress >= kRp2040SramStart &&
                          stackAddress <= kRp2040SramEnd - (8u * sizeof(uint32_t));
  retainedCrashRecord.phase_report_flags =
      (uint32_t)currentCrashPhase |
      ((uint32_t)currentCrashReport << 16) |
      (stackValid ? 0x80000000u : 0u);
  retainedCrashRecord.exception_lr = exception_lr;
  retainedCrashRecord.stack_pointer = (uint32_t)stackAddress;
  retainedCrashRecord.stacked_lr = stackValid ? stack[5] : 0;
  retainedCrashRecord.stacked_pc = stackValid ? stack[6] : 0;
  retainedCrashRecord.stacked_xpsr = stackValid ? stack[7] : 0;
  retainedCrashRecord.checksum = crashRecordChecksum(retainedCrashRecord);
  __asm volatile("dmb" ::: "memory");
  retainedCrashRecord.magic = kCrashMagic;
  __asm volatile("dmb" ::: "memory");

  // Reset the full RP2040 subsystem set used by the SDK, except SRAM so the
  // NOLOAD record survives. Direct register access avoids the SDK reboot helper,
  // which overwrites boot-control scratch registers 4-7.
  constexpr uint32_t kRetainedResetMask =
      PSM_WDSEL_BITS &
      ~(PSM_WDSEL_ROSC_BITS | PSM_WDSEL_XOSC_BITS |
        PSM_WDSEL_SRAM0_BITS | PSM_WDSEL_SRAM1_BITS |
        PSM_WDSEL_SRAM2_BITS | PSM_WDSEL_SRAM3_BITS |
        PSM_WDSEL_SRAM4_BITS | PSM_WDSEL_SRAM5_BITS);
  psm_hw->wdsel = kRetainedResetMask;
  watchdog_hw->ctrl = WATCHDOG_CTRL_TRIGGER_BITS;
  while (true) {
    __asm volatile("wfi");
  }
}

// The Pico SDK vector table declares isr_hardfault weak. Keep this naked stub
// in SRAM as well, select MSP/PSP from EXC_RETURN, and tail-branch to the
// non-returning recorder without consuming the possibly damaged fault stack.
extern "C" __attribute__((naked, used,
                           section(".time_critical.rp2040_crash")))
void isr_hardfault(void) {
  __asm volatile(
      "mov r1, lr\n"
      "movs r2, #4\n"
      "tst r1, r2\n"
      "beq 1f\n"
      "mrs r0, psp\n"
      "b rp2040CrashHardFaultFromStack\n"
      "1:\n"
      "mrs r0, msp\n"
      "b rp2040CrashHardFaultFromStack\n");
}

void rp2040CrashTelemetryBootInit() {
  if (bootTelemetryInitialized) return;
  bootTelemetryInitialized = true;
  currentCrashPhase = RP2040_CRASH_PHASE_NONE;
  currentCrashReport = 0;
  lastResetReason = watchdog_hw->reason;
  // watchdog_enable() stamps scratch[4] with the SDK marker, but Auto input's
  // one-boot hint and reboot() keep their own magic there, so a real timeout
  // can leave either. watchdog_reboot() and the boot ROM write other values.
  const bool watchdogTimeout =
      watchdog_enable_caused_reboot() ||
      ((watchdog_hw->reason & WATCHDOG_REASON_TIMER_BITS) != 0 &&
       watchdog_hw->scratch[AUTO_INPUT_MODE_SCRATCH_MAGIC] == AUTO_INPUT_MODE_MAGIC);
  lastWatchdogBreadcrumbValid =
      watchdogTimeout && retainedWatchdogBreadcrumbIsValid();
  lastWatchdogPhase = lastWatchdogBreadcrumbValid
      ? retainedWatchdogBreadcrumb.phase
      : RP2040_CRASH_PHASE_NONE;
  lastCrashValid = retainedRecordValid();
  // The step store has no checksum; trust it only beside a valid record.
  lastLoopStep = (lastWatchdogBreadcrumbValid || lastCrashValid) ? rp2040RetainedLoopStep : 0;
  rp2040RetainedLoopStep = 0;
  retainedCrashCorrupt = !lastCrashValid &&
      (retainedCrashRecord.magic == kCrashMagic ||
       retainedCrashRecord.magic_inverse == ~kCrashMagic);
  if (lastCrashValid) {
    copyRetainedRecord();
    recordLastUnexpectedReset(kLastResetFault,
                              (uint16_t)(lastCrashRecord.phase_report_flags & 0xFFFFu),
                              lastLoopStep, lastCrashRecord.stacked_pc);
  } else if (lastWatchdogBreadcrumbValid &&
             lastWatchdogPhase != RP2040_CRASH_PHASE_INTENTIONAL_RESET) {
    recordLastUnexpectedReset(kLastResetWatchdog, lastWatchdogPhase, lastLoopStep, 0);
  }
  lastResetValid = retainedLastResetValid();
  if (lastResetValid) {
    lastReset.kind = retainedLastReset.kind;
    lastReset.step = retainedLastReset.step;
    lastReset.phase = retainedLastReset.phase;
    lastReset.pc = retainedLastReset.pc;
    lastReset.count = retainedLastReset.count;
  }
  clearRetainedRecord();
  clearRetainedWatchdogBreadcrumb();
}

void rp2040CrashTelemetrySetPhase(uint16_t phase) {
  currentCrashPhase = phase;
  writeRetainedWatchdogBreadcrumb(phase);
}

void rp2040CrashTelemetrySetReport(uint8_t report_id) {
  currentCrashReport = report_id;
}

void rp2040CrashTelemetryBootPhase(uint16_t phase) {
  rp2040CrashTelemetrySetPhase(phase);
  if ((watchdog_hw->ctrl & WATCHDOG_CTRL_ENABLE_BITS) != 0) watchdog_update();
}

void rp2040CrashTelemetryEarlyBootArm(bool holdAfterUnexpectedReset) {
  rp2040CrashTelemetryBootInit();
  const bool unexpectedWatchdogReset =
      holdAfterUnexpectedReset && lastWatchdogBreadcrumbValid &&
      lastWatchdogPhase != RP2040_CRASH_PHASE_INTENTIONAL_RESET;
  if ((watchdog_hw->ctrl & WATCHDOG_CTRL_ENABLE_BITS) == 0) {
    watchdog_enable(kEarlyBootWatchdogTimeoutMs, true);
  }
  // Preserve the NOLOAD hard-fault and watchdog breadcrumbs across both the
  // early and runtime timeout paths.
  hw_clear_bits(
      &psm_hw->wdsel,
      PSM_WDSEL_SRAM0_BITS | PSM_WDSEL_SRAM1_BITS |
      PSM_WDSEL_SRAM2_BITS | PSM_WDSEL_SRAM3_BITS |
      PSM_WDSEL_SRAM4_BITS | PSM_WDSEL_SRAM5_BITS);
  watchdog_update();
  if (!unexpectedWatchdogReset) return;

  rp2040CrashTelemetrySetPhase(
      RP2040_CRASH_PHASE_EARLY_RECOVERY_HOLD);
  const uint32_t startedAtMs = to_ms_since_boot(get_absolute_time());
  while (static_cast<uint32_t>(
             to_ms_since_boot(get_absolute_time()) - startedAtMs) <
         kUnexpectedResetStabilizationMs) {
    watchdog_update();
    sleep_ms(10);
  }
  watchdog_update();
}

void rp2040CrashTelemetryRuntimeWatchdogTask(
    bool updaterTrialOwnsWatchdog) {
  if (updaterTrialOwnsWatchdog) return;
  if (!runtimeWatchdogConfigured ||
      (watchdog_hw->ctrl & WATCHDOG_CTRL_ENABLE_BITS) == 0) {
    watchdog_enable(kRuntimeWatchdogTimeoutMs, true);
    // Preserve only the NOLOAD crash/breadcrumb records. crt0 still
    // initializes ordinary data/BSS after the watchdog reset.
    hw_clear_bits(
        &psm_hw->wdsel,
        PSM_WDSEL_SRAM0_BITS | PSM_WDSEL_SRAM1_BITS |
        PSM_WDSEL_SRAM2_BITS | PSM_WDSEL_SRAM3_BITS |
        PSM_WDSEL_SRAM4_BITS | PSM_WDSEL_SRAM5_BITS);
    runtimeWatchdogConfigured = true;
  }
  watchdog_update();
}

bool rp2040CrashTelemetryHandleSerialCommand(const char* command, Print& out) {
  // Bench checks for the watchdog and fault record: the unit reboots, and the
  // next CRASH STATUS reports the timeout (WATCHDOG=1, WDSTEP) or the fault PC.
  if (serialTextEqualsExact(command, "CRASH TEST HANG CONFIRM")) {
    out.println(F("OK:CRASH_TEST_HANG"));
    delay(50);
    while (true) tight_loop_contents();
  }
  if (serialTextEqualsExact(command, "CRASH TEST FAULT CONFIRM")) {
    out.println(F("OK:CRASH_TEST_FAULT"));
    delay(50);
    __asm volatile("udf #0");
  }
  if (!serialTextEqualsExact(command, "CRASH") &&
      !serialTextEqualsExact(command, "CRASH STATUS") &&
      !serialTextEqualsExact(command, "RESET STATUS")) {
    return false;
  }

  out.print(F("CRASH STATUS VALID="));
  out.print(lastCrashValid ? 1 : 0);
  out.print(F(" CORRUPT="));
  out.print(retainedCrashCorrupt ? 1 : 0);
  out.print(F(" RESET="));
  printHex32(out, lastResetReason);
  out.print(F(" WATCHDOG="));
  out.print(lastWatchdogBreadcrumbValid ? 1 : 0);
  out.print(F(" WDPHASE="));
  out.print((unsigned long)lastWatchdogPhase);
  out.print(F(" WDSTEP="));
  out.print((unsigned long)lastLoopStep);
  if (lastResetValid) {
    out.print(lastReset.kind == kLastResetFault ? F(" LAST=FAULT") : F(" LAST=WATCHDOG"));
    out.print(F(" LASTPHASE="));
    out.print((unsigned long)lastReset.phase);
    out.print(F(" LASTSTEP="));
    out.print((unsigned long)lastReset.step);
    out.print(F(" LASTPC="));
    printHex32(out, lastReset.pc);
    out.print(F(" COUNT="));
    out.print((unsigned long)lastReset.count);
  }
  if (lastCrashValid) {
    out.print(F(" CORE="));
    out.print((unsigned long)lastCrashRecord.core);
    out.print(F(" PHASE="));
    out.print((unsigned long)(lastCrashRecord.phase_report_flags & 0xFFFFu));
    out.print(F(" REPORT="));
    printHex32(out, (lastCrashRecord.phase_report_flags >> 16) & 0xFFu);
    out.print(F(" STACK="));
    out.print((lastCrashRecord.phase_report_flags & 0x80000000u) ? 1 : 0);
    out.print(F(" PC="));
    printHex32(out, lastCrashRecord.stacked_pc);
    out.print(F(" LR="));
    printHex32(out, lastCrashRecord.stacked_lr);
    out.print(F(" XPSR="));
    printHex32(out, lastCrashRecord.stacked_xpsr);
    out.print(F(" SP="));
    printHex32(out, lastCrashRecord.stack_pointer);
    out.print(F(" EXCLR="));
    printHex32(out, lastCrashRecord.exception_lr);
  }
  out.println();
  return true;
}

#else

void rp2040CrashTelemetryBootInit() {}
void rp2040CrashTelemetrySetPhase(uint16_t) {}
void rp2040CrashTelemetrySetReport(uint8_t) {}
void rp2040CrashTelemetryBootPhase(uint16_t) {}
void rp2040CrashTelemetryEarlyBootArm(bool) {}
void rp2040CrashTelemetryRuntimeWatchdogTask(bool) {}
bool rp2040CrashTelemetryHandleSerialCommand(const char*, Print&) {
  return false;
}

#endif
