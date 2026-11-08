#include "usb_latency_trace.h"

#include "../product_config.h"
#include "serial_command_parser.h"
#include "usb_poll_alignment.h"

#if defined(ENABLE_USB_LATENCY_SEGMENT_TRACE) && \
    ADAPT_POLICY_USB_POLL_LATENCY_DIAGNOSTICS && defined(ENABLE_INPUT_USB) && \
    defined(ADAPT_OUTPUT_USB_DEVICE)

#if __has_include("pio_usb.h")
#include <pio_usb.h>
#define ADAPT_LATENCY_HAS_PIO_FRAME 1
#else
#define ADAPT_LATENCY_HAS_PIO_FRAME 0
#endif

namespace {

constexpr uint8_t kPlayers = 4;
constexpr uint8_t kOutputs = 4;
constexpr uint8_t kSamples = 64;
constexpr uint32_t kMaxSampleUs = 20000;

enum Segment : uint8_t {
  kFrameToRaw,
  kRawToDecode,
  kDecodeToNeutral,
  kNeutralToBuild,
  kBuild,
  kBuildToSubmit,
  kRawToSubmit,
  kFrameToSubmit,
  kSubmitToComplete,
  kRawToComplete,
  kFrameToComplete,
  kSegmentCount,
};

const char* const kSegmentNames[kSegmentCount] = {
    "FRAME_TO_RAW",       "RAW_TO_DECODE",    "DECODE_TO_NEUTRAL",
    "NEUTRAL_TO_BUILD",   "BUILD",            "BUILD_TO_SUBMIT",
    "RAW_TO_SUBMIT",      "FRAME_TO_SUBMIT",  "SUBMIT_TO_COMPLETE",
    "RAW_TO_COMPLETE",    "FRAME_TO_COMPLETE",
};

const char* const kInputClassNames[USB_LATENCY_INPUT_CLASS_COUNT] = {
    "UNKNOWN",
    "NATIVE",
    "KNOWN_HID",
    "SDL_HID",
    "GENERIC_HID",
};

struct EventTrace {
  uint32_t generation = 0;
  uint32_t frameUs = 0;
  uint32_t rawUs = 0;
  uint32_t decodeUs = 0;
  uint32_t neutralUs = 0;
  uint32_t buildStartUs = 0;
  uint32_t buildCompleteUs = 0;
  uint8_t inputClass = USB_LATENCY_INPUT_UNKNOWN;
};

// Input callbacks and the neutral-frame/output loop can run on different
// cores. A small sequence lock keeps each diagnostic timestamp bundle
// coherent without taking a lock in either USB hot path.
struct SharedTrace {
  uint32_t version = 0;
  EventTrace event = {};
};

struct PendingTrace {
  bool valid = false;
  uint8_t player = 0;
  EventTrace event = {};
  uint32_t submitUs = 0;
};

struct SegmentSamples {
  uint16_t values[kSamples] = {};
  uint8_t used = 0;
  uint8_t write = 0;
  uint32_t total = 0;
  uint32_t rejected = 0;
};

SharedTrace decoded[kPlayers];
SharedTrace committed[kPlayers];
EventTrace building[kPlayers];
bool buildingValid[kPlayers] = {};
uint32_t completedGeneration[kPlayers] = {};
PendingTrace pending[kOutputs];
SegmentSamples samples[kSegmentCount];
SegmentSamples
    classSamples[USB_LATENCY_INPUT_CLASS_COUNT][kSegmentCount];
bool traceEnabled = false;
uint32_t rawReports = 0;
uint32_t neutralCommits = 0;
uint32_t submits = 0;
uint32_t completes = 0;
uint32_t failedCompletes = 0;
uint32_t incompleteSubmits = 0;

bool isEnabled() {
  return __atomic_load_n(&traceEnabled, __ATOMIC_RELAXED);
}

void publishTrace(SharedTrace& destination, const EventTrace& event) {
  uint32_t version = __atomic_load_n(&destination.version, __ATOMIC_RELAXED);
  if (version & 1u) ++version;
  __atomic_store_n(&destination.version, version + 1u, __ATOMIC_RELEASE);
  destination.event = event;
  __atomic_store_n(&destination.version, version + 2u, __ATOMIC_RELEASE);
}

bool snapshotTrace(const SharedTrace& source, EventTrace& event) {
  for (uint8_t attempt = 0; attempt < 4; ++attempt) {
    const uint32_t before =
        __atomic_load_n(&source.version, __ATOMIC_ACQUIRE);
    if (before & 1u) continue;
    event = source.event;
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    const uint32_t after =
        __atomic_load_n(&source.version, __ATOMIC_ACQUIRE);
    if (before == after && !(after & 1u)) return true;
  }
  return false;
}

uint32_t elapsed(uint32_t newer, uint32_t older) {
  return newer - older;
}

void addBucketSample(SegmentSamples& bucket, uint32_t valueUs) {
  if (valueUs > kMaxSampleUs || valueUs > UINT16_MAX) {
    ++bucket.rejected;
    return;
  }
  bucket.values[bucket.write] = static_cast<uint16_t>(valueUs);
  bucket.write = static_cast<uint8_t>((bucket.write + 1) % kSamples);
  if (bucket.used < kSamples) ++bucket.used;
  ++bucket.total;
}

void addSample(Segment segment, uint8_t inputClass, uint32_t valueUs) {
  addBucketSample(samples[segment], valueUs);
  if (inputClass < USB_LATENCY_INPUT_CLASS_COUNT) {
    addBucketSample(classSamples[inputClass][segment], valueUs);
  }
}

void addIfOrdered(Segment segment, uint8_t inputClass,
                  uint32_t older, uint32_t newer) {
  if (older == 0 || newer == 0) return;
  addSample(segment, inputClass, elapsed(newer, older));
}

void sortValues(uint16_t* values, uint8_t count) {
  for (uint8_t i = 1; i < count; ++i) {
    const uint16_t value = values[i];
    uint8_t j = i;
    while (j != 0 && values[j - 1] > value) {
      values[j] = values[j - 1];
      --j;
    }
    values[j] = value;
  }
}

uint16_t percentile(const uint16_t* values, uint8_t count, uint8_t percent) {
  if (count == 0) return 0;
  const uint16_t scaled = static_cast<uint16_t>(percent) * (count - 1);
  return values[(scaled + 50) / 100];
}

void writeSegment(Print& out, Segment segment, SegmentSamples& bucket,
                  const char* inputClassName = nullptr) {
  uint16_t ordered[kSamples];
  for (uint8_t i = 0; i < bucket.used; ++i) {
    ordered[i] = bucket.values[i];
  }
  sortValues(ordered, bucket.used);
  out.print(F("USBHOST LATENCY "));
  if (inputClassName != nullptr) {
    out.print(F("CLASS="));
    out.print(inputClassName);
    out.print(' ');
  }
  out.print(F("SEGMENT="));
  out.print(kSegmentNames[segment]);
  out.print(F(" N="));
  out.print(bucket.used);
  out.print(F(" TOTAL="));
  out.print(bucket.total);
  out.print(F(" P50_US="));
  out.print(percentile(ordered, bucket.used, 50));
  out.print(F(" P95_US="));
  out.print(percentile(ordered, bucket.used, 95));
  out.print(F(" REJECTED="));
  out.println(bucket.rejected);
}

void writeStatus(Print& out) {
  out.print(F("USBHOST LATENCY ENABLED="));
  out.print(isEnabled() ? 1 : 0);
  out.print(F(" RAW="));
  out.print(rawReports);
  out.print(F(" NEUTRAL="));
  out.print(neutralCommits);
  out.print(F(" SUBMITS="));
  out.print(submits);
  out.print(F(" COMPLETES="));
  out.print(completes);
  out.print(F(" FAILURES="));
  out.print(failedCompletes);
  out.print(F(" INCOMPLETE_SUBMITS="));
  out.println(incompleteSubmits);
  for (uint8_t i = 0; i < kSegmentCount; ++i) {
    writeSegment(out, static_cast<Segment>(i), samples[i]);
  }
  for (uint8_t inputClass = 0;
       inputClass < USB_LATENCY_INPUT_CLASS_COUNT; ++inputClass) {
    bool classUsed = false;
    for (uint8_t segment = 0; segment < kSegmentCount; ++segment) {
      if (classSamples[inputClass][segment].total != 0) {
        classUsed = true;
        break;
      }
    }
    if (!classUsed) continue;
    for (uint8_t segment = 0; segment < kSegmentCount; ++segment) {
      writeSegment(out, static_cast<Segment>(segment),
                   classSamples[inputClass][segment],
                   kInputClassNames[inputClass]);
    }
  }
  out.println(F("USBHOST LATENCY END"));
}

void resetTrace() {
  const bool resume = isEnabled();
  __atomic_store_n(&traceEnabled, false, __ATOMIC_RELEASE);
  memset(decoded, 0, sizeof(decoded));
  memset(committed, 0, sizeof(committed));
  memset(building, 0, sizeof(building));
  memset(buildingValid, 0, sizeof(buildingValid));
  memset(completedGeneration, 0, sizeof(completedGeneration));
  memset(pending, 0, sizeof(pending));
  memset(samples, 0, sizeof(samples));
  memset(classSamples, 0, sizeof(classSamples));
  rawReports = 0;
  neutralCommits = 0;
  submits = 0;
  completes = 0;
  failedCompletes = 0;
  incompleteSubmits = 0;
  __atomic_store_n(&traceEnabled, resume, __ATOMIC_RELEASE);
}

}  // namespace

void usbLatencyTraceNoteRawReport(uint8_t player, uint8_t inputClass) {
  usbPollAlignmentNoteRawReport(player, inputClass);
  if (!isEnabled() || player >= kPlayers) return;
  if (inputClass >= USB_LATENCY_INPUT_CLASS_COUNT) {
    inputClass = USB_LATENCY_INPUT_UNKNOWN;
  }
  EventTrace previous = {};
  snapshotTrace(decoded[player], previous);
  EventTrace event = {};
  event.generation = previous.generation + 1u;
  event.rawUs = micros();
  event.inputClass = inputClass;
#if ADAPT_LATENCY_HAS_PIO_FRAME
  pio_usb_host_poll_alignment_diag_t pio = {};
  pio_usb_host_poll_alignment_snapshot(&pio);
  event.frameUs = pio.last_frame_us;
#endif
  publishTrace(decoded[player], event);
  ++rawReports;
}

void usbLatencyTraceNoteDecodeComplete(uint8_t player) {
  if (!isEnabled() || player >= kPlayers) return;
  EventTrace event = {};
  if (!snapshotTrace(decoded[player], event) || event.rawUs == 0) return;
  event.decodeUs = micros();
  publishTrace(decoded[player], event);
}

void usbLatencyTraceNoteNeutralCommit(uint8_t player) {
  if (!isEnabled() || player >= kPlayers) return;
  EventTrace event = {};
  if (!snapshotTrace(decoded[player], event) || event.decodeUs == 0) return;
  event.neutralUs = micros();
  publishTrace(committed[player], event);
  ++neutralCommits;
}

void usbLatencyTraceNoteBuildStart(uint8_t player) {
  if (!isEnabled() || player >= kPlayers) return;
  EventTrace event = {};
  if (!snapshotTrace(committed[player], event) || event.neutralUs == 0) {
    buildingValid[player] = false;
    return;
  }
  event.buildStartUs = micros();
  event.buildCompleteUs = 0;
  building[player] = event;
  buildingValid[player] = true;
}

void usbLatencyTraceNoteBuildComplete(uint8_t player) {
  if (!isEnabled() || player >= kPlayers || !buildingValid[player]) return;
  building[player].buildCompleteUs = micros();
}

void usbLatencyTraceNoteSubmit(uint8_t sourcePlayer, uint8_t outputSlot) {
  usbPollAlignmentNoteSubmit(sourcePlayer);
  if (!isEnabled() || sourcePlayer >= kPlayers || outputSlot >= kOutputs) {
    return;
  }
  const EventTrace event = building[sourcePlayer];
  if (!buildingValid[sourcePlayer] || event.rawUs == 0 ||
      event.decodeUs == 0 || event.neutralUs == 0 ||
      event.buildStartUs == 0 || event.buildCompleteUs == 0) {
    ++incompleteSubmits;
    pending[outputSlot].valid = false;
    return;
  }
  if (event.generation == completedGeneration[sourcePlayer]) return;
  if (pending[outputSlot].valid) ++incompleteSubmits;
  ++submits;
  PendingTrace& trace = pending[outputSlot];
  trace.valid = true;
  trace.player = sourcePlayer;
  trace.event = event;
  trace.submitUs = micros();
}

void usbLatencyTraceNoteComplete(uint8_t outputSlot, bool success) {
  if (!isEnabled() || outputSlot >= kOutputs) return;
  const PendingTrace trace = pending[outputSlot];
  pending[outputSlot].valid = false;
  if (!success) {
    ++failedCompletes;
    return;
  }
  if (!trace.valid) return;
  const uint32_t completeUs = micros();
  completedGeneration[trace.player] = trace.event.generation;
  ++completes;
  const uint8_t inputClass = trace.event.inputClass;
  addIfOrdered(kFrameToRaw, inputClass,
               trace.event.frameUs, trace.event.rawUs);
  addIfOrdered(kRawToDecode, inputClass,
               trace.event.rawUs, trace.event.decodeUs);
  addIfOrdered(kDecodeToNeutral, inputClass,
               trace.event.decodeUs, trace.event.neutralUs);
  addIfOrdered(kNeutralToBuild, inputClass, trace.event.neutralUs,
               trace.event.buildStartUs);
  addIfOrdered(kBuild, inputClass, trace.event.buildStartUs,
               trace.event.buildCompleteUs);
  addIfOrdered(kBuildToSubmit, inputClass,
               trace.event.buildCompleteUs, trace.submitUs);
  addIfOrdered(kRawToSubmit, inputClass,
               trace.event.rawUs, trace.submitUs);
  addIfOrdered(kFrameToSubmit, inputClass,
               trace.event.frameUs, trace.submitUs);
  addIfOrdered(kSubmitToComplete, inputClass, trace.submitUs, completeUs);
  addIfOrdered(kRawToComplete, inputClass, trace.event.rawUs, completeUs);
  addIfOrdered(kFrameToComplete, inputClass,
               trace.event.frameUs, completeUs);
}

bool usbLatencyTraceHandleSerialCommand(const char* command, Print& out) {
  if (serialTextEqualsExact(command, "USBHOST LATENCY") ||
      serialTextEqualsExact(command, "USBHOST LATENCY STATUS")) {
    writeStatus(out);
    return true;
  }
  if (serialTextEqualsExact(command, "USBHOST LATENCY ON")) {
    resetTrace();
    __atomic_store_n(&traceEnabled, true, __ATOMIC_RELEASE);
    out.println(F("OK:USBHOST_LATENCY_ON"));
    return true;
  }
  if (serialTextEqualsExact(command, "USBHOST LATENCY OFF")) {
    __atomic_store_n(&traceEnabled, false, __ATOMIC_RELEASE);
    out.println(F("OK:USBHOST_LATENCY_OFF"));
    writeStatus(out);
    return true;
  }
  if (serialTextEqualsExact(command, "USBHOST LATENCY RESET")) {
    resetTrace();
    out.println(F("OK:USBHOST_LATENCY_RESET"));
    writeStatus(out);
    return true;
  }
  return false;
}

#else

void usbLatencyTraceNoteRawReport(uint8_t player, uint8_t inputClass) {
  usbPollAlignmentNoteRawReport(player, inputClass);
}
void usbLatencyTraceNoteDecodeComplete(uint8_t) {}
void usbLatencyTraceNoteNeutralCommit(uint8_t) {}
void usbLatencyTraceNoteBuildStart(uint8_t) {}
void usbLatencyTraceNoteBuildComplete(uint8_t) {}
void usbLatencyTraceNoteSubmit(uint8_t sourcePlayer, uint8_t) {
  usbPollAlignmentNoteSubmit(sourcePlayer);
}
void usbLatencyTraceNoteComplete(uint8_t, bool) {}
bool usbLatencyTraceHandleSerialCommand(const char*, Print&) { return false; }

#endif
