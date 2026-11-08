#include "usb_poll_alignment.h"

#include "../product_config.h"
#include "serial_command_parser.h"
#include "usb_latency_trace.h"

#if ADAPT_POLICY_USB_POLL_LATENCY_DIAGNOSTICS && defined(ENABLE_INPUT_USB) && \
    defined(ADAPT_OUTPUT_USB_DEVICE) && defined(ENABLE_USB_LATENCY_SEGMENT_TRACE) && \
    __has_include("pio_usb.h")

#include <pio_usb.h>
#include <tusb.h>

// RP2040 TinyUSB records this at the SOF interrupt before its deferred user
// callback runs. It is the bus-frame timestamp; micros() in tud_sof_cb is not.
extern "C" volatile uint32_t e15_last_sof;

namespace {

constexpr uint16_t kUsbFrameUs = 1000;
constexpr uint16_t kDefaultTargetLeadUs = 250;
constexpr uint16_t kMaximumTargetLeadUs = kUsbFrameUs - 1;
constexpr uint16_t kPreparationGuardUs = 75;
constexpr uint8_t kPreparationSamples = 16;
constexpr uint8_t kMinimumSamplesForDynamicLead = 8;
constexpr uint8_t kPlayers = 4;
constexpr uint16_t kLockToleranceUs = 100;
constexpr uint8_t kStableSamplesToLock = 4;
constexpr uint16_t kMinimumSofGapUs = 750;
constexpr uint16_t kMaximumSofGapUs = 1250;
constexpr uint32_t kMinCorrectionGapUs = 4000;
constexpr int16_t kMaxCorrectionUs =
    PIO_USB_HOST_POLL_ALIGNMENT_MAX_ADJUST_US;

struct UsbPollAlignmentState {
  bool enabled = false;
  bool locked = false;
  bool manualTarget = false;
  uint8_t stableSamples = 0;
  uint32_t inputObservations = 0;
  uint32_t upstreamSofs = 0;
  uint32_t upstreamCompletes = 0;
  uint32_t upstreamFailures = 0;
  uint32_t cadenceRejects = 0;
  uint32_t phaseCorrections = 0;
  uint32_t correctionBusy = 0;
  uint32_t lastInputObservedUs = 0;
  uint32_t lastSofUs = 0;
  uint32_t lastSofFrame = 0;
  uint32_t lastUpstreamCompleteUs = 0;
  uint32_t lastCorrectionUs = 0;
  uint32_t lastCadenceGapUs = 0;
  uint16_t lastCadenceErrorUs = 0;
  uint16_t lastFrameAgeUs = 0;
  int16_t lastPhaseErrorUs = 0;
  int16_t lastRequestedCorrectionUs = 0;
  uint32_t inputToCompleteSamples = 0;
  uint64_t inputToCompleteSumUs = 0;
  uint32_t inputToCompleteMinUs = 0;
  uint32_t inputToCompleteMaxUs = 0;
  uint16_t targetLeadUs = kDefaultTargetLeadUs;
  uint16_t upstreamTokenPhaseUs[kPreparationSamples] = {};
  uint8_t upstreamTokenPhaseUsed = 0;
  uint8_t upstreamTokenPhaseWrite = 0;
  uint16_t medianUpstreamTokenPhaseUs = 0;
};

UsbPollAlignmentState alignment;

struct RawObservation {
  uint32_t rawUs = 0;
  uint32_t frameUs = 0;
  uint8_t inputClass = USB_LATENCY_INPUT_UNKNOWN;
};

struct SharedRawObservation {
  uint32_t version = 0;
  RawObservation observation = {};
};

struct PendingPreparation {
  bool valid = false;
  RawObservation observation = {};
};

struct PreparationStats {
  uint16_t rawToSubmitUs[kPreparationSamples] = {};
  uint16_t frameToSubmitUs[kPreparationSamples] = {};
  uint8_t used = 0;
  uint8_t write = 0;
  uint32_t total = 0;
  uint16_t lastRawToSubmitUs = 0;
  uint16_t lastFrameToSubmitUs = 0;
  uint16_t rollingMaxRawToSubmitUs = 0;
  uint16_t rollingMaxFrameToSubmitUs = 0;
  uint16_t targetLeadUs = kDefaultTargetLeadUs;
  bool clamped = false;
};

SharedRawObservation rawObservations[kPlayers];
PendingPreparation pendingPreparation[kPlayers];
PreparationStats preparation[USB_LATENCY_INPUT_CLASS_COUNT];

const char* const kInputClassNames[USB_LATENCY_INPUT_CLASS_COUNT] = {
    "UNKNOWN", "NATIVE", "KNOWN_HID", "SDL_HID", "GENERIC_HID",
};

void publishRawObservation(uint8_t player,
                           const RawObservation& observation) {
  SharedRawObservation& shared = rawObservations[player];
  uint32_t version = __atomic_load_n(&shared.version, __ATOMIC_RELAXED);
  if (version & 1u) ++version;
  __atomic_store_n(&shared.version, version + 1u, __ATOMIC_RELEASE);
  shared.observation = observation;
  __atomic_store_n(&shared.version, version + 2u, __ATOMIC_RELEASE);
}

bool snapshotRawObservation(uint8_t player, RawObservation& observation) {
  const SharedRawObservation& shared = rawObservations[player];
  for (uint8_t attempt = 0; attempt < 4; ++attempt) {
    const uint32_t before =
        __atomic_load_n(&shared.version, __ATOMIC_ACQUIRE);
    if (before & 1u) continue;
    observation = shared.observation;
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    const uint32_t after =
        __atomic_load_n(&shared.version, __ATOMIC_ACQUIRE);
    if (before == after && !(after & 1u)) return true;
  }
  return false;
}

uint16_t rollingMaximum(const uint16_t* values, uint8_t used) {
  uint16_t maximum = 0;
  for (uint8_t i = 0; i < used; ++i) {
    if (values[i] > maximum) maximum = values[i];
  }
  return maximum;
}

uint16_t percentile50(const uint16_t* values, uint8_t used) {
  if (used == 0) return 0;
  uint16_t ordered[kPreparationSamples];
  memcpy(ordered, values, used * sizeof(uint16_t));
  for (uint8_t i = 1; i < used; ++i) {
    const uint16_t value = ordered[i];
    uint8_t j = i;
    while (j != 0 && ordered[j - 1] > value) {
      ordered[j] = ordered[j - 1];
      --j;
    }
    ordered[j] = value;
  }
  return ordered[used / 2u];
}

uint16_t percentile95(const uint16_t* values, uint8_t used) {
  if (used == 0) return 0;
  uint16_t ordered[kPreparationSamples];
  memcpy(ordered, values, used * sizeof(uint16_t));
  for (uint8_t i = 1; i < used; ++i) {
    const uint16_t value = ordered[i];
    uint8_t j = i;
    while (j != 0 && ordered[j - 1] > value) {
      ordered[j] = ordered[j - 1];
      --j;
    }
    ordered[j] = value;
  }
  return ordered[((95u * (used - 1u)) + 50u) / 100u];
}

uint16_t circularMedianPhase(const uint16_t* values, uint8_t used) {
  if (used == 0) return 0;
  const int16_t reference = static_cast<int16_t>(values[used - 1u]);
  int16_t offsets[kPreparationSamples];
  for (uint8_t i = 0; i < used; ++i) {
    int16_t delta = static_cast<int16_t>(values[i]) - reference;
    if (delta > static_cast<int16_t>(kUsbFrameUs / 2u)) {
      delta -= static_cast<int16_t>(kUsbFrameUs);
    } else if (delta < -static_cast<int16_t>(kUsbFrameUs / 2u)) {
      delta += static_cast<int16_t>(kUsbFrameUs);
    }
    offsets[i] = delta;
  }
  for (uint8_t i = 1; i < used; ++i) {
    const int16_t value = offsets[i];
    uint8_t j = i;
    while (j != 0 && offsets[j - 1] > value) {
      offsets[j] = offsets[j - 1];
      --j;
    }
    offsets[j] = value;
  }
  int32_t phase = static_cast<int32_t>(reference) + offsets[used / 2u];
  while (phase < 0) phase += kUsbFrameUs;
  while (phase >= kUsbFrameUs) phase -= kUsbFrameUs;
  return static_cast<uint16_t>(phase);
}

void refreshTargetLead() {
  if (alignment.manualTarget) return;
  if (alignment.upstreamTokenPhaseUsed < kMinimumSamplesForDynamicLead) {
    alignment.targetLeadUs = kDefaultTargetLeadUs;
    return;
  }

  uint16_t slowestMedianFrameToSubmitUs = 0;
  for (uint8_t inputClass = 0;
       inputClass < USB_LATENCY_INPUT_CLASS_COUNT; ++inputClass) {
    PreparationStats& stats = preparation[inputClass];
    stats.targetLeadUs = kDefaultTargetLeadUs;
    stats.clamped = false;
    if (stats.used < kMinimumSamplesForDynamicLead) continue;
    const uint16_t medianFrameToSubmitUs =
        percentile50(stats.frameToSubmitUs, stats.used);
    if (medianFrameToSubmitUs > slowestMedianFrameToSubmitUs) {
      slowestMedianFrameToSubmitUs = medianFrameToSubmitUs;
    }
  }
  if (slowestMedianFrameToSubmitUs == 0) {
    alignment.targetLeadUs = kDefaultTargetLeadUs;
    return;
  }

  // The downstream poll and upstream IN token are both 1 kHz clocks. The
  // preparation budget must therefore be reduced modulo one USB frame, then
  // offset by the observed upstream token phase. Treating a 1,138 us path as
  // an absolute lead previously clamped it to 850 us and placed submissions
  // just after MiSTer's token. The correct phase is roughly 138 us plus guard.
  const uint32_t desiredBeforeToken =
      static_cast<uint32_t>(slowestMedianFrameToSubmitUs) +
      kPreparationGuardUs + kUsbFrameUs -
      alignment.medianUpstreamTokenPhaseUs;
  alignment.targetLeadUs =
      static_cast<uint16_t>(desiredBeforeToken % kUsbFrameUs);

  for (uint8_t inputClass = 0;
       inputClass < USB_LATENCY_INPUT_CLASS_COUNT; ++inputClass) {
    PreparationStats& stats = preparation[inputClass];
    if (stats.used < kMinimumSamplesForDynamicLead) continue;
    stats.targetLeadUs = alignment.targetLeadUs;
    stats.clamped = alignment.targetLeadUs > kMaximumTargetLeadUs;
  }
}

void addPreparationSample(uint8_t inputClass, uint32_t rawToSubmitUs,
                          uint32_t frameToSubmitUs) {
  // Before SOF lock, FRAME_TO_SUBMIT includes arbitrary relative phase and is
  // not a preparation budget. Sampling it here previously drove the adaptive
  // lead to its 850 us clamp before alignment had even converged.
  if (!alignment.locked) return;
  if (inputClass >= USB_LATENCY_INPUT_CLASS_COUNT ||
      rawToSubmitUs > UINT16_MAX || frameToSubmitUs > UINT16_MAX) return;
  PreparationStats& stats = preparation[inputClass];
  stats.rawToSubmitUs[stats.write] = static_cast<uint16_t>(rawToSubmitUs);
  stats.frameToSubmitUs[stats.write] = static_cast<uint16_t>(frameToSubmitUs);
  stats.write = static_cast<uint8_t>((stats.write + 1u) % kPreparationSamples);
  if (stats.used < kPreparationSamples) ++stats.used;
  ++stats.total;
  stats.lastRawToSubmitUs = static_cast<uint16_t>(rawToSubmitUs);
  stats.lastFrameToSubmitUs = static_cast<uint16_t>(frameToSubmitUs);
  stats.rollingMaxRawToSubmitUs = rollingMaximum(stats.rawToSubmitUs, stats.used);
  stats.rollingMaxFrameToSubmitUs = rollingMaximum(stats.frameToSubmitUs, stats.used);
  refreshTargetLead();
}

uint16_t cadenceErrorUs(uint32_t gapUs) {
  const uint16_t remainder = static_cast<uint16_t>(gapUs % kUsbFrameUs);
  return remainder <= (kUsbFrameUs / 2)
      ? remainder
      : static_cast<uint16_t>(kUsbFrameUs - remainder);
}

int16_t clampCorrection(int16_t value) {
  if (value < -kMaxCorrectionUs) return -kMaxCorrectionUs;
  if (value > kMaxCorrectionUs) return kMaxCorrectionUs;
  return value;
}

void clearMeasurements(bool keepEnabled) {
  const bool enabled = keepEnabled ? alignment.enabled : false;
  alignment = UsbPollAlignmentState{};
  alignment.enabled = enabled;
  memset(rawObservations, 0, sizeof(rawObservations));
  memset(pendingPreparation, 0, sizeof(pendingPreparation));
  memset(preparation, 0, sizeof(preparation));
}

void writeStatus(Print& out) {
  pio_usb_host_poll_alignment_diag_t pio = {};
  pio_usb_host_poll_alignment_snapshot(&pio);
  const uint32_t nowUs = micros();
  const uint32_t frameAgeUs = pio.last_frame_us != 0
      ? nowUs - pio.last_frame_us
      : 0;
  const uint32_t averageInputToCompleteUs =
      alignment.inputToCompleteSamples != 0
          ? static_cast<uint32_t>(alignment.inputToCompleteSumUs /
                                  alignment.inputToCompleteSamples)
          : 0;

  out.print(F("USBHOST ALIGN ENABLED="));
  out.print(alignment.enabled ? 1 : 0);
  out.print(F(" LOCKED="));
  out.print(alignment.locked ? 1 : 0);
  out.print(F(" REF=SOF"));
  out.print(F(" TARGET_MODE="));
  out.print(alignment.manualTarget ? F("MANUAL") : F("AUTO"));
  out.print(F(" STABLE="));
  out.print(alignment.stableSamples);
  out.print(F(" TARGET_LEAD_US="));
  out.print(alignment.targetLeadUs);
  out.print(F(" DEFAULT_LEAD_US="));
  out.print(kDefaultTargetLeadUs);
  out.print(F(" MAX_LEAD_US="));
  out.print(kMaximumTargetLeadUs);
  out.print(F(" TOKEN_PHASE_US="));
  out.print(alignment.medianUpstreamTokenPhaseUs);
  out.print(F(" TOKEN_SAMPLES="));
  out.print(alignment.upstreamTokenPhaseUsed);
  out.print(F(" COMPLETES="));
  out.print(alignment.upstreamCompletes);
  out.print(F(" SOFS="));
  out.print(alignment.upstreamSofs);
  out.print(F(" FAILURES="));
  out.print(alignment.upstreamFailures);
  out.print(F(" CADENCE_REJECTS="));
  out.println(alignment.cadenceRejects);

  out.print(F("USBHOST ALIGN CADENCE_GAP_US="));
  out.print(alignment.lastCadenceGapUs);
  out.print(F(" CADENCE_ERROR_US="));
  out.print(alignment.lastCadenceErrorUs);
  out.print(F(" FRAME_AGE_US="));
  out.print(frameAgeUs);
  out.print(F(" PHASE_ERROR_US="));
  out.print(alignment.lastPhaseErrorUs);
  out.print(F(" LAST_REQUEST_US="));
  out.println(alignment.lastRequestedCorrectionUs);

  out.print(F("USBHOST ALIGN INPUT_OBS="));
  out.print(alignment.inputObservations);
  out.print(F(" INPUT_TO_COMPLETE_SAMPLES="));
  out.print(alignment.inputToCompleteSamples);
  out.print(F(" AVG_US="));
  out.print(averageInputToCompleteUs);
  out.print(F(" MIN_US="));
  out.print(alignment.inputToCompleteMinUs);
  out.print(F(" MAX_US="));
  out.println(alignment.inputToCompleteMaxUs);

  out.print(F("USBHOST ALIGN PIO_REQUESTS="));
  out.print(pio.requests);
  out.print(F(" PIO_APPLIED="));
  out.print(pio.applied);
  out.print(F(" PIO_BUSY="));
  out.print(pio.busy_rejects);
  out.print(F(" PIO_LAST_ADJUST_US="));
  out.print(pio.last_adjust_us);
  out.print(F(" PIO_PENDING_US="));
  out.print(pio.pending_adjust_us);
  out.print(F(" CORRECTIONS="));
  out.print(alignment.phaseCorrections);
  out.print(F(" CORRECTION_BUSY="));
  out.println(alignment.correctionBusy);

  for (uint8_t inputClass = 0;
       inputClass < USB_LATENCY_INPUT_CLASS_COUNT; ++inputClass) {
    const PreparationStats& stats = preparation[inputClass];
    if (stats.total == 0) continue;
    out.print(F("USBHOST ALIGN CLASS="));
    out.print(kInputClassNames[inputClass]);
    out.print(F(" N="));
    out.print(stats.used);
    out.print(F(" TOTAL="));
    out.print(stats.total);
    out.print(F(" RAW_LAST_US="));
    out.print(stats.lastRawToSubmitUs);
    out.print(F(" RAW_P95_US="));
    out.print(percentile95(stats.rawToSubmitUs, stats.used));
    out.print(F(" RAW_MAX_US="));
    out.print(stats.rollingMaxRawToSubmitUs);
    out.print(F(" FRAME_LAST_US="));
    out.print(stats.lastFrameToSubmitUs);
    out.print(F(" FRAME_P95_US="));
    out.print(percentile95(stats.frameToSubmitUs, stats.used));
    out.print(F(" FRAME_MAX_US="));
    out.print(stats.rollingMaxFrameToSubmitUs);
    out.print(F(" LEAD_US="));
    out.print(stats.targetLeadUs);
    out.print(F(" CLAMPED="));
    out.println(stats.clamped ? 1 : 0);
  }
}

}  // namespace

void usbPollAlignmentNoteRawReport(uint8_t player, uint8_t inputClass) {
  if (player >= kPlayers) return;
  RawObservation observation = {};
  observation.rawUs = micros();
  observation.inputClass = inputClass < USB_LATENCY_INPUT_CLASS_COUNT
      ? inputClass : USB_LATENCY_INPUT_UNKNOWN;
  pio_usb_host_poll_alignment_diag_t pio = {};
  pio_usb_host_poll_alignment_snapshot(&pio);
  observation.frameUs = pio.last_frame_us;
  publishRawObservation(player, observation);
}

void usbPollAlignmentNoteInputObserved(uint8_t player) {
  ++alignment.inputObservations;
  alignment.lastInputObservedUs = micros();
  if (player >= kPlayers) return;
  RawObservation observation = {};
  if (!snapshotRawObservation(player, observation) || observation.rawUs == 0) {
    pendingPreparation[player].valid = false;
    return;
  }
  pendingPreparation[player].valid = true;
  pendingPreparation[player].observation = observation;
}

void usbPollAlignmentNoteSubmit(uint8_t sourcePlayer) {
  if (sourcePlayer >= kPlayers || !pendingPreparation[sourcePlayer].valid) return;
  const RawObservation observation = pendingPreparation[sourcePlayer].observation;
  pendingPreparation[sourcePlayer].valid = false;
  const uint32_t submitUs = micros();
  const uint32_t rawToSubmitUs = submitUs - observation.rawUs;
  const uint32_t frameToSubmitUs = observation.frameUs != 0
      ? submitUs - observation.frameUs : rawToSubmitUs;
  if (rawToSubmitUs > 5000 || frameToSubmitUs > 5000) return;
  addPreparationSample(observation.inputClass, rawToSubmitUs, frameToSubmitUs);
}

void usbPollAlignmentNoteUpstreamSof(uint32_t frameCount) {
  const uint32_t irqSofUs = e15_last_sof;
  const uint32_t nowUs = irqSofUs != 0 ? irqSofUs : micros();
  ++alignment.upstreamSofs;
  if (alignment.lastSofUs == 0) {
    alignment.lastSofUs = nowUs;
    alignment.lastSofFrame = frameCount;
    return;
  }

  const uint32_t gapUs = nowUs - alignment.lastSofUs;
  const uint32_t frameDelta =
      (frameCount - alignment.lastSofFrame) & 0x7ffu;
  alignment.lastSofUs = nowUs;
  alignment.lastSofFrame = frameCount;
  alignment.lastCadenceGapUs = gapUs;

  const uint16_t cadenceError = cadenceErrorUs(gapUs);
  alignment.lastCadenceErrorUs = cadenceError;
  if (frameDelta != 1 || gapUs < kMinimumSofGapUs ||
      gapUs > kMaximumSofGapUs || cadenceError > kLockToleranceUs) {
    ++alignment.cadenceRejects;
    alignment.stableSamples = 0;
    alignment.locked = false;
    alignment.targetLeadUs = kDefaultTargetLeadUs;
    return;
  }

  if (alignment.stableSamples < UINT8_MAX) {
    ++alignment.stableSamples;
  }
  alignment.locked = alignment.stableSamples >= kStableSamplesToLock;
  if (!alignment.enabled || !alignment.locked ||
      nowUs - alignment.lastCorrectionUs < kMinCorrectionGapUs) {
    return;
  }

  pio_usb_host_poll_alignment_diag_t pio = {};
  pio_usb_host_poll_alignment_snapshot(&pio);
  if (pio.last_frame_us == 0) return;
  const uint32_t rawFrameAgeUs = nowUs - pio.last_frame_us;
  if (rawFrameAgeUs >= 2 * kUsbFrameUs) {
    alignment.locked = false;
    alignment.stableSamples = 0;
    return;
  }

  const uint16_t frameAgeUs =
      static_cast<uint16_t>(rawFrameAgeUs % kUsbFrameUs);
  alignment.lastFrameAgeUs = frameAgeUs;
  const int16_t phaseErrorUs =
      static_cast<int16_t>(frameAgeUs) -
      static_cast<int16_t>(alignment.targetLeadUs);
  alignment.lastPhaseErrorUs = phaseErrorUs;
  if (phaseErrorUs >= -25 && phaseErrorUs <= 25) return;

  const int16_t correctionUs = clampCorrection(phaseErrorUs);
  alignment.lastRequestedCorrectionUs = correctionUs;
  if (pio_usb_host_request_phase_adjustment(correctionUs)) {
    ++alignment.phaseCorrections;
    alignment.lastCorrectionUs = nowUs;
  } else {
    ++alignment.correctionBusy;
  }
}

void usbPollAlignmentNoteUpstreamComplete(uint8_t outputSlot, bool success) {
  (void)outputSlot;
  const uint32_t nowUs = micros();
  alignment.lastUpstreamCompleteUs = nowUs;
  if (!success) {
    ++alignment.upstreamFailures;
    return;
  }

  ++alignment.upstreamCompletes;
  if (alignment.lastSofUs != 0) {
    const uint32_t sofAgeUs = nowUs - alignment.lastSofUs;
    if (sofAgeUs < 2u * kUsbFrameUs) {
      alignment.upstreamTokenPhaseUs[alignment.upstreamTokenPhaseWrite] =
          static_cast<uint16_t>(sofAgeUs % kUsbFrameUs);
      alignment.upstreamTokenPhaseWrite = static_cast<uint8_t>(
          (alignment.upstreamTokenPhaseWrite + 1u) % kPreparationSamples);
      if (alignment.upstreamTokenPhaseUsed < kPreparationSamples) {
        ++alignment.upstreamTokenPhaseUsed;
      }
      alignment.medianUpstreamTokenPhaseUs = circularMedianPhase(
          alignment.upstreamTokenPhaseUs, alignment.upstreamTokenPhaseUsed);
      refreshTargetLead();
    }
  }
  if (alignment.lastInputObservedUs == 0) return;
  const uint32_t elapsedUs = nowUs - alignment.lastInputObservedUs;
  if (elapsedUs > 5000) return;
  ++alignment.inputToCompleteSamples;
  alignment.inputToCompleteSumUs += elapsedUs;
  if (alignment.inputToCompleteMinUs == 0 ||
      elapsedUs < alignment.inputToCompleteMinUs) {
    alignment.inputToCompleteMinUs = elapsedUs;
  }
  if (elapsedUs > alignment.inputToCompleteMaxUs) {
    alignment.inputToCompleteMaxUs = elapsedUs;
  }
}

bool usbPollAlignmentUsesSof() { return alignment.enabled; }

bool usbPollAlignmentHandleSerialCommand(const char* command, Print& out) {
  if (serialTextEqualsExact(command, "USBHOST ALIGN") ||
      serialTextEqualsExact(command, "USBHOST ALIGN STATUS")) {
    writeStatus(out);
    return true;
  }
  if (serialTextEqualsExact(command, "USBHOST ALIGN ON")) {
    alignment.enabled = true;
    alignment.manualTarget = false;
    refreshTargetLead();
    tud_sof_cb_enable(true);
    out.println(F("OK:USBHOST_ALIGN_ON"));
    writeStatus(out);
    return true;
  }
  if (serialTextEqualsExact(command, "USBHOST ALIGN OFF")) {
    alignment.enabled = false;
    alignment.locked = false;
    alignment.stableSamples = 0;
    tud_sof_cb_enable(false);
    out.println(F("OK:USBHOST_ALIGN_OFF"));
    writeStatus(out);
    return true;
  }
  char* remainder = nullptr;
  if (serialCommandStartsWith(command, "USBHOST ALIGN LEAD", &remainder)) {
    uint32_t leadUs = 0;
    if (!serialParseUint32Token(remainder, &leadUs) ||
        *serialSkipSpaces(remainder) != '\0' || leadUs >= kUsbFrameUs) {
      out.println(F("ERR:USBHOST_ALIGN_LEAD_RANGE"));
      return true;
    }
    alignment.enabled = true;
    alignment.manualTarget = true;
    alignment.targetLeadUs = static_cast<uint16_t>(leadUs);
    alignment.locked = false;
    alignment.stableSamples = 0;
    tud_sof_cb_enable(true);
    out.print(F("OK:USBHOST_ALIGN_LEAD="));
    out.println(alignment.targetLeadUs);
    writeStatus(out);
    return true;
  }
  if (serialTextEqualsExact(command, "USBHOST ALIGN AUTO")) {
    alignment.enabled = true;
    alignment.manualTarget = false;
    alignment.locked = false;
    alignment.stableSamples = 0;
    refreshTargetLead();
    tud_sof_cb_enable(true);
    out.println(F("OK:USBHOST_ALIGN_AUTO"));
    writeStatus(out);
    return true;
  }
  if (serialTextEqualsExact(command, "USBHOST ALIGN RESET")) {
    clearMeasurements(true);
    out.println(F("OK:USBHOST_ALIGN_RESET"));
    writeStatus(out);
    return true;
  }
  return false;
}

extern "C" void adapt_usb_input_xfer_complete(uint8_t output_slot,
                                                bool success) {
  usbPollAlignmentNoteUpstreamComplete(output_slot, success);
  usbLatencyTraceNoteComplete(output_slot, success);
}

#else

void usbPollAlignmentNoteRawReport(uint8_t, uint8_t) {}
void usbPollAlignmentNoteInputObserved(uint8_t) {}
void usbPollAlignmentNoteSubmit(uint8_t) {}
void usbPollAlignmentNoteUpstreamSof(uint32_t) {}
void usbPollAlignmentNoteUpstreamComplete(uint8_t, bool) {}
bool usbPollAlignmentUsesSof() { return false; }
bool usbPollAlignmentHandleSerialCommand(const char*, Print&) { return false; }
extern "C" void adapt_usb_input_xfer_complete(uint8_t, bool) {}

#endif
