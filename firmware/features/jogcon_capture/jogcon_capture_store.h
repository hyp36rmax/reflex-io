#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <type_traits>

namespace jogcon_capture {
constexpr uint32_t kMagic = 0x4A4F4743, kSchema = 1, kArmed = 0x52455354;
constexpr uint16_t kCriticalCapacity = 32, kRecentCapacity = 96, kCapacity = 128;
constexpr uint8_t kWireBytes = 9;
constexpr uint32_t kReportFileBytes = (4 + kCapacity + 2) * 512;
enum class Kind : uint8_t { Boot = 1, Reboot, Init, Wire, Input, Output, Export, NewCapture };
enum Flag : uint16_t { ReadOk = 1, HeaderOk = 2, Truncated = 4, MotorOff = 8,
                      BadDirection = 16, Overflow = 32, PreparedFrame = 64,
                      WireSnapshot = 128, InputSnapshot = 256, OutputSnapshot = 512 };
// POD only: no constructors, pointers, device serials or settings payloads.
struct Event {
  uint32_t sequence, us, durationUs, buttons;
  int32_t spinnerAnalog;
  uint16_t boot, counter, previousCounter, cleanCounter, gapUs, flags, rawButtons;
  int16_t lx, ly, delta, previousMotion;
  uint8_t kind, port, input, output, configuredOutput, mode, force, gain, reverse;
  uint8_t ready, primed, position, revolutions, direction, command, count, paddle;
  uint8_t axis, motorRequested, motorSent, lastDirection, readOk, phase;
  int8_t spinner;
  uint8_t tx[kWireBytes], rx[kWireBytes];
  uint32_t sampleUs[3]; // wire/input/output time within the recorded boot
};
static_assert(sizeof(Event) == 96 && std::is_trivial<Event>::value, "Retained event layout");
struct Retained {
  uint32_t magic, inverse, checksum;
  uint32_t schema, recordBytes, objectBytes, fingerprint, variant, armed, exportRole;
  uint32_t generation, sequence, droppedCritical, droppedRecent, sampledWire, sampledInput, sampledOutput;
  uint32_t wireTransactions, badReplies, unknownDirections, overflows, inputSamples, outputSamples;
  uint16_t boot, criticalCount, recentCount, recentWrite;
  uint8_t frozen, reserved[3];
  Event critical[kCriticalCapacity], recent[kRecentCapacity];
};
static_assert(std::is_trivial<Retained>::value, "NOLOAD state must have no startup constructor");

inline uint32_t checksum(const Retained &s) {
  uint32_t value = 2166136261u;
  const auto *bytes = reinterpret_cast<const uint8_t *>(&s);
  // Fixed extent; unvalidated counts can never control a memory access.
  for (size_t i = offsetof(Retained, schema); i < sizeof(s); ++i) value = (value ^ bytes[i]) * 16777619u;
  return value;
}
inline uint32_t sourceFingerprint(const char *build) {
  uint32_t value = 2166136261u;
  while (*build) value = (value ^ uint8_t(*build++)) * 16777619u;
  return value;
}
inline bool valid(const Retained &s, bool watchdogTimer, uint32_t fingerprint, uint32_t variant) {
  return watchdogTimer && s.magic == kMagic && s.inverse == ~kMagic && s.armed == kArmed &&
      s.schema == kSchema && s.recordBytes == sizeof(Event) && s.objectBytes == sizeof(Retained) &&
      s.fingerprint == fingerprint && s.variant == variant && s.exportRole <= 1 && s.frozen <= 1 &&
      (!s.exportRole || s.frozen) && s.criticalCount <= kCriticalCapacity &&
      s.recentCount <= kRecentCapacity && s.recentWrite < kRecentCapacity &&
      (s.recentCount == kRecentCapacity || s.recentWrite == 0) && s.checksum == checksum(s);
}
inline void clear(Retained &s, uint32_t fingerprint, uint32_t variant, uint32_t generation = 1) {
  memset(&s, 0, sizeof(s));
  s.schema = kSchema; s.recordBytes = sizeof(Event); s.objectBytes = sizeof(Retained);
  s.fingerprint = fingerprint; s.variant = variant; s.generation = generation;
}
inline void prepareSeal(Retained &s) {
  s.magic = 0; s.inverse = ~kMagic; s.armed = kArmed; s.checksum = checksum(s);
  // Caller executes a release barrier before publishing kMagic last.
}
inline void publishSeal(Retained &s) { s.magic = kMagic; }
inline bool criticalKind(uint8_t kind) {
  return kind == uint8_t(Kind::Boot) || kind == uint8_t(Kind::Reboot) || kind == uint8_t(Kind::Init) ||
         kind == uint8_t(Kind::Export) || kind == uint8_t(Kind::NewCapture);
}
inline void push(Retained &s, Event event) {
  if (s.frozen || event.port >= 2) return;
  event.sequence = ++s.sequence;
  event.boot = s.boot;
  if (criticalKind(event.kind) && s.criticalCount < kCriticalCapacity) {
    s.critical[s.criticalCount++] = event;
    return;
  }
  if (event.kind == uint8_t(Kind::Reboot) || event.kind == uint8_t(Kind::Export)) {
    // Repeated init failures cannot fill all protected slots and evict the
    // transition evidence. Prefer replacing an ordinary lifecycle record;
    // after 32 transitions, retain the newest 32 and count the loss.
    uint16_t remove = 0;
    for (uint16_t i = 0; i < s.criticalCount; ++i) {
      if (s.critical[i].kind != uint8_t(Kind::Reboot) && s.critical[i].kind != uint8_t(Kind::Export)) {
        remove = i; break;
      }
    }
    ++s.droppedCritical;
    memmove(s.critical + remove, s.critical + remove + 1,
            (s.criticalCount - remove - 1) * sizeof(Event));
    s.critical[s.criticalCount - 1] = event;
    return;
  }
  if (criticalKind(event.kind)) ++s.droppedCritical;
  if (s.recentCount < kRecentCapacity) s.recent[s.recentCount++] = event;
  else {
    ++s.droppedRecent;
    s.recent[s.recentWrite] = event;
    s.recentWrite = (s.recentWrite + 1) % kRecentCapacity;
  }
}
inline const Event &recentRecord(const Retained &s, uint16_t index) {
  return s.recent[s.recentCount == kRecentCapacity ? (s.recentWrite + index) % kRecentCapacity : index];
}
inline const Event &record(const Retained &s, uint16_t index) {
  // Merge protected lifecycle evidence with the rolling polls by sequence.
  uint16_t c = 0, r = 0;
  while (true) {
    const bool fromCritical = c < s.criticalCount &&
        (r >= s.recentCount || s.critical[c].sequence < recentRecord(s, r).sequence);
    if (index-- == 0) return fromCritical ? s.critical[c] : recentRecord(s, r);
    if (fromCritical) ++c; else ++r;
  }
}
} // namespace jogcon_capture
