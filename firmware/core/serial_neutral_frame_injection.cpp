#include "../product_config.h"

#include "serial_neutral_frame_injection.h"

#include <Arduino.h>
#include <cstring>
#include "usb_poll_alignment.h"
#include "usb_latency_trace.h"

#include "controller_delivery_state.h"
#include "controller_frame_state.h"
#include "serial_command_parser.h"
#include "product_neutral_injection_services.h"
#include "../output/output_runtime_state.h"
#include "../features/n64_capture/n64_capture.h"

#if ADAPT_POLICY_SERIAL_NEUTRAL_FRAME_INJECTION || defined(ADAPT_FEATURE_N64_CAPTURE)
#include <tusb.h>
#endif

#if ADAPT_POLICY_SERIAL_NEUTRAL_FRAME_INJECTION

#include <tusb.h>

namespace {

constexpr uint32_t kNeutralFrameButtonMask = 0x0FFFFFFFu;
constexpr uint16_t kDefaultPulseMs = 60;
constexpr uint16_t kMinimumPulseMs = 30;
constexpr uint16_t kMaximumPulseMs = 1000;
constexpr uint16_t kReleaseHoldMs = 30;
constexpr uint16_t kDefaultTrainEdges = 1000;
constexpr uint16_t kMaximumTrainEdges = 4095;
constexpr uint32_t kDefaultTrainPeriodUs = 1000;
constexpr uint32_t kMinimumTrainPeriodUs = 1000;
constexpr uint32_t kMaximumTrainPeriodUs = 100000;
constexpr uint32_t kTrainArmDelayUs = 100000;
constexpr uint32_t kTrainFinalTimeoutUs = 2000000;
constexpr uint32_t kTrainFinalHoldUs = 20000;
constexpr uint32_t kTrainEdgeTimeoutUs = 2000000;
constexpr uint8_t kOutputSlotCount = 4;
constexpr uint16_t kNoPendingTrainTag = 0;
constexpr uint16_t kFinalTrainTag = 0xFFFFu;

enum class InjectionMode : uint8_t {
  None = 0,
  Pulse,
  Train,
};

enum class InjectionPhase : uint8_t {
  Idle = 0,
  Press,
  Release,
  TrainArm,
  TrainEdge,
  TrainFinal,
  Done,
};

struct NeutralFrameInjectionState {
  InjectionMode mode;
  InjectionPhase phase;
  uint8_t player;
  uint8_t playerMask;
  uint32_t mask;
  uint16_t durationMs;
  uint32_t sequence;
  uint32_t commandReceivedUs;
  uint32_t pressAppliedUs;
  uint32_t releaseAppliedUs;
  uint32_t phaseStartedAtMs;
  uint32_t pressStageCount;
  uint32_t releaseStageCount;
  bool phaseStaged;
  uint8_t framesBackedUpMask;
  controller_state_t frameBackup[kOutputSlotCount];

  uint16_t trainCount;
  uint16_t trainEdge;
  uint32_t trainPeriodUs;
  uint32_t trainArmUs;
  uint32_t trainScheduledUs;
  uint32_t trainFinalStartedUs;
  uint32_t trainFinalCompletedUs;
  uint32_t trainSkippedEdges;
  uint32_t trainLateEdges;
  uint32_t trainMaxLatenessUs;
  uint32_t trainPreparedTags;
  uint32_t trainSubmittedTags;
  uint32_t trainCompletedTags;
  uint32_t trainFirstPrepareUs;
  uint32_t trainLastPrepareUs;
  uint32_t trainMaxPrepareGapUs;
  uint16_t trainLastPreparedTag;
  uint16_t trainLastSubmittedTag;
  uint16_t trainLastCompletedTag;
  uint32_t trainLastProgressUs;
  uint32_t trainFirstEdgeCompleteUs;
  uint32_t trainLastEdgeCompleteUs;
  uint32_t trainRetryCount;
  bool trainFinalPrepared;
  bool trainFinalSubmitted;
  bool trainFinalCompleted;
  bool trainTimedOut;
  uint8_t trainSubmittedPlayerMask;
  uint8_t trainRequiredOutputMask;
  uint8_t trainCompletedOutputMask;

  uint16_t pendingTrainTag[kOutputSlotCount];
  uint8_t pendingTrainSourcePlayer[kOutputSlotCount];
  uint32_t submitCount[kOutputSlotCount];
  uint32_t completeCount[kOutputSlotCount];
  uint32_t failedCompleteCount[kOutputSlotCount];
  uint32_t firstSubmitUs[kOutputSlotCount];
  uint32_t lastSubmitUs[kOutputSlotCount];
  uint32_t maxSubmitGapUs[kOutputSlotCount];
  uint32_t firstCompleteUs[kOutputSlotCount];
  uint32_t lastCompleteUs[kOutputSlotCount];
  uint32_t maxCompleteGapUs[kOutputSlotCount];
};

NeutralFrameInjectionState injection{};
uint32_t nextSequence = 0;

constexpr uint8_t kAllOutputPlayersMask =
    static_cast<uint8_t>((1u << kOutputSlotCount) - 1u);

bool timeReached(uint32_t now, uint32_t target) {
  return static_cast<int32_t>(now - target) >= 0;
}

void requestSelectedFrameDelivery() {
  for (uint8_t player = 0; player < kOutputSlotCount; ++player) {
    if ((injection.playerMask & (1u << player)) != 0) {
      requestControllerFrameDelivery(player);
    }
  }
}

const char* phaseName(InjectionPhase phase) {
  switch (phase) {
    case InjectionPhase::Press: return "PRESS";
    case InjectionPhase::Release: return "RELEASE";
    case InjectionPhase::TrainArm: return "ARM";
    case InjectionPhase::TrainEdge: return "EDGE";
    case InjectionPhase::TrainFinal: return "FINAL";
    case InjectionPhase::Done: return "DONE";
    case InjectionPhase::Idle:
    default: return "IDLE";
  }
}

const char* modeName(InjectionMode mode) {
  switch (mode) {
    case InjectionMode::Pulse: return "PULSE";
    case InjectionMode::Train: return "TRAIN";
    case InjectionMode::None:
    default: return "NONE";
  }
}

bool phaseUsesOverlay(InjectionPhase phase) {
  return phase == InjectionPhase::Press ||
         phase == InjectionPhase::Release ||
         phase == InjectionPhase::TrainEdge ||
         phase == InjectionPhase::TrainFinal;
}

void printHex32(Print& out, uint32_t value) {
  out.print(F("0x"));
  for (int shift = 28; shift >= 0; shift -= 4) {
    out.print((value >> shift) & 0x0Fu, HEX);
  }
}

void requestPhase(InjectionPhase phase) {
  injection.phase = phase;
  injection.phaseStaged = false;
  injection.phaseStartedAtMs = 0;
  requestSelectedFrameDelivery();
}

void beginRecord(uint32_t mask, uint16_t durationMs,
                 InjectionPhase phase) {
  injection.mode = InjectionMode::Pulse;
  injection.player = 0;
  injection.playerMask = 1u;
  injection.mask = mask;
  injection.durationMs = durationMs;
  injection.sequence = ++nextSequence;
  if (injection.sequence == 0) injection.sequence = ++nextSequence;
  injection.commandReceivedUs = micros();
  injection.pressAppliedUs = 0;
  injection.releaseAppliedUs = 0;
  injection.pressStageCount = 0;
  injection.releaseStageCount = 0;
  requestPhase(phase);
}

void beginTrain(uint16_t count, uint32_t periodUs, uint8_t player,
                uint8_t playerMask) {
  injection = {};
  injection.mode = InjectionMode::Train;
  injection.phase = InjectionPhase::TrainArm;
  injection.player = player;
  injection.playerMask = playerMask;
  injection.trainCount = count;
  injection.trainPeriodUs = periodUs;
  injection.sequence = ++nextSequence;
  if (injection.sequence == 0) injection.sequence = ++nextSequence;
  injection.commandReceivedUs = micros();
  injection.trainArmUs = injection.commandReceivedUs + kTrainArmDelayUs;
}

void noteTrainLateness(uint32_t now, uint32_t scheduled) {
  const uint32_t lateness = now - scheduled;
  if (lateness == 0) return;
  ++injection.trainLateEdges;
  if (lateness > injection.trainMaxLatenessUs) {
    injection.trainMaxLatenessUs = lateness;
  }
}

void activateTrainEdge(uint16_t edge, uint32_t scheduledUs,
                       uint32_t nowUs) {
  injection.phase = InjectionPhase::TrainEdge;
  injection.trainEdge = edge;
  injection.trainScheduledUs = scheduledUs;
  injection.mask = edge;
  injection.trainLastPreparedTag = 0;
  injection.trainLastSubmittedTag = 0;
  injection.trainSubmittedPlayerMask = 0;
  injection.trainRequiredOutputMask = 0;
  injection.trainCompletedOutputMask = 0;
  injection.trainLastProgressUs = nowUs;
  noteTrainLateness(nowUs, scheduledUs);
  requestPhase(InjectionPhase::TrainEdge);
}

void activateTrainFinal(uint32_t nowUs) {
  injection.phase = InjectionPhase::TrainFinal;
  injection.mask = 0;
  injection.trainFinalStartedUs = nowUs;
  injection.trainFinalPrepared = false;
  injection.trainFinalSubmitted = false;
  injection.trainFinalCompleted = false;
  injection.trainSubmittedPlayerMask = 0;
  injection.trainRequiredOutputMask = 0;
  injection.trainCompletedOutputMask = 0;
  requestPhase(InjectionPhase::TrainFinal);
}

void finishTrain(bool timedOut) {
  injection.phase = InjectionPhase::Done;
  injection.mask = 0;
  injection.trainTimedOut = injection.trainTimedOut || timedOut;
  requestSelectedFrameDelivery();
}

void recordFirstLastGap(uint32_t now, uint32_t& first, uint32_t& last,
                        uint32_t& maxGap) {
  if (first == 0) first = now;
  if (last != 0) {
    const uint32_t gap = now - last;
    if (gap > maxGap) maxGap = gap;
  }
  last = now;
}

void printCounterArray(Print& out, const uint32_t* values) {
  for (uint8_t i = 0; i < kOutputSlotCount; ++i) {
    if (i != 0) out.print(',');
    out.print(values[i]);
  }
}

void writeStatus(Print& out) {
  out.print(F("NFRAME ACTIVE="));
  out.print(injection.phase != InjectionPhase::Idle &&
                    injection.phase != InjectionPhase::Done ? 1 : 0);
  out.print(F(" MODE="));
  out.print(modeName(injection.mode));
  out.print(F(" PHASE="));
  out.print(phaseName(injection.phase));
  out.print(F(" P="));
  out.print(injection.playerMask == kAllOutputPlayersMask
                ? 0u
                : static_cast<unsigned>(injection.player + 1u));
  out.print(F(" PMASK="));
  out.print(injection.playerMask, HEX);
  out.print(F(" MASK="));
  printHex32(out, injection.mask);
  out.print(F(" MS="));
  out.print(injection.durationMs);
  out.print(F(" SEQ="));
  out.print(injection.sequence);
  out.print(F(" CMD_US="));
  out.print(injection.commandReceivedUs);
  out.print(F(" PRESS_US="));
  out.print(injection.pressAppliedUs);
  out.print(F(" RELEASE_US="));
  out.print(injection.releaseAppliedUs);
  out.print(F(" STAGED="));
  out.print(injection.pressStageCount);
  out.print('/');
  out.print(injection.releaseStageCount);
  out.print(F(" TRAIN="));
  out.print(injection.trainEdge);
  out.print('/');
  out.print(injection.trainCount);
  out.print(F(" PERIOD_US="));
  out.print(injection.trainPeriodUs);
  out.print(F(" ARM_US="));
  out.print(injection.trainArmUs);
  out.print(F(" SCHED_US="));
  out.print(injection.trainScheduledUs);
  out.print(F(" TAGS="));
  out.print(injection.trainPreparedTags);
  out.print('/');
  out.print(injection.trainSubmittedTags);
  out.print('/');
  out.print(injection.trainCompletedTags);
  out.print(F(" SKIP="));
  out.print(injection.trainSkippedEdges);
  out.print(F(" LATE="));
  out.print(injection.trainLateEdges);
  out.print('/');
  out.print(injection.trainMaxLatenessUs);
  out.print(F(" EDGE_C_US="));
  out.print(injection.trainFirstEdgeCompleteUs);
  out.print('/');
  out.print(injection.trainLastEdgeCompleteUs);
  out.print(F(" RETRY="));
  out.print(injection.trainRetryCount);
  out.print(F(" PREP_US="));
  out.print(injection.trainFirstPrepareUs);
  out.print('/');
  out.print(injection.trainLastPrepareUs);
  out.print('/');
  out.print(injection.trainMaxPrepareGapUs);
  out.print(F(" FINAL="));
  out.print(injection.trainFinalPrepared ? 1 : 0);
  out.print('/');
  out.print(injection.trainFinalSubmitted ? 1 : 0);
  out.print('/');
  out.print(injection.trainFinalCompleted ? 1 : 0);
  out.print(F(" TIMEOUT="));
  out.print(injection.trainTimedOut ? 1 : 0);
  out.print(F(" SUB="));
  printCounterArray(out, injection.submitCount);
  out.print(F(" COMPLETE="));
  printCounterArray(out, injection.completeCount);
  out.print(F(" FAIL="));
  printCounterArray(out, injection.failedCompleteCount);
  out.print(F(" MAX_SGAP_US="));
  printCounterArray(out, injection.maxSubmitGapUs);
  out.print(F(" MAX_CGAP_US="));
  printCounterArray(out, injection.maxCompleteGapUs);
  out.print(F(" S_FIRST_US="));
  printCounterArray(out, injection.firstSubmitUs);
  out.print(F(" S_LAST_US="));
  printCounterArray(out, injection.lastSubmitUs);
  out.print(F(" C_FIRST_US="));
  printCounterArray(out, injection.firstCompleteUs);
  out.print(F(" C_LAST_US="));
  printCounterArray(out, injection.lastCompleteUs);
  out.println();
}

bool injectionBlocked(Print& out) {
  return productNeutralInjectionBlocked(out);
}

bool trainOutputModeIsInstrumented() {
  switch (canonicalizeOutputMode(get_effective_output_mode())) {
    case OUTPUT_HID:
    case OUTPUT_MISTER:
    case OUTPUT_PS3:
    case OUTPUT_SWITCHPRO:
    case OUTPUT_XINPUT:
    case OUTPUT_XINPUT2P:
      return true;
    default:
      return false;
  }
}

}  // namespace

bool handleSerialNeutralFrameInjectionCommand(char* text, Print& out) {
  text = serialSkipSpaces(text);
  if (*text == '\0' || serialTokenEquals(text, "HELP")) {
    out.println(F("NFRAME CMDS:NFRAME PULSE <MASK> [MS],NFRAME TRAIN [EDGES] [PERIOD_US] [PLAYER 0=ALL XINPUT4P],NFRAME RELEASE,NFRAME STOP,NFRAME STATUS"));
    return true;
  }
  if (serialTokenEquals(text, "STATUS")) {
    writeStatus(out);
    return true;
  }
  if (serialTokenEquals(text, "STOP")) {
    serialNeutralFrameInjectionCancel();
    out.println(F("OK:NFRAME_STOP"));
    return true;
  }

  char* remainder = nullptr;
  if (serialCommandStartsWith(text, "TRAIN", &remainder)) {
    long edgeCount = kDefaultTrainEdges;
    long periodUs = kDefaultTrainPeriodUs;
    long player = 1;
    if (*remainder != '\0' &&
        !serialParseLongToken(remainder, &edgeCount)) {
      out.println(F("ERR:NFRAME_BAD_TRAIN_EDGES"));
      return true;
    }
    if (*remainder != '\0' &&
        !serialParseLongToken(remainder, &periodUs)) {
      out.println(F("ERR:NFRAME_BAD_TRAIN_PERIOD"));
      return true;
    }
    if (*remainder != '\0' &&
        !serialParseLongToken(remainder, &player)) {
      out.println(F("ERR:NFRAME_BAD_PLAYER"));
      return true;
    }
    if (*remainder != '\0' || edgeCount < 1 ||
        edgeCount > kMaximumTrainEdges) {
      out.println(F("ERR:NFRAME_BAD_TRAIN_EDGES"));
      return true;
    }
    if (periodUs < kMinimumTrainPeriodUs ||
        periodUs > kMaximumTrainPeriodUs) {
      out.println(F("ERR:NFRAME_BAD_TRAIN_PERIOD"));
      return true;
    }
    if (player < 0 || player > kOutputSlotCount) {
      out.println(F("ERR:NFRAME_BAD_PLAYER"));
      return true;
    }
    if (productNeutralTrainBlocked(out)) return true;
    if (!trainOutputModeIsInstrumented()) {
      out.print(F("ERR:NFRAME_TRAIN_OUTPUT_UNINSTRUMENTED MODE="));
      out.println(static_cast<int>(get_effective_output_mode()));
      return true;
    }
    const bool allPlayers = player == 0;
    if (allPlayers &&
        canonicalizeOutputMode(get_effective_output_mode()) !=
            OUTPUT_XINPUT2P) {
      out.println(F("ERR:NFRAME_ALL_REQUIRES_XINPUT4P"));
      return true;
    }

    serialNeutralFrameInjectionCancel();
    beginTrain(static_cast<uint16_t>(edgeCount),
               static_cast<uint32_t>(periodUs),
               allPlayers ? 0u : static_cast<uint8_t>(player - 1),
               allPlayers
                   ? kAllOutputPlayersMask
                   : static_cast<uint8_t>(
                         1u << static_cast<uint8_t>(player - 1)));
    out.print(F("OK:NFRAME_TRAIN EDGES="));
    out.print(edgeCount);
    out.print(F(" PERIOD_US="));
    out.print(periodUs);
    out.print(F(" P="));
    out.print(player);
    out.print(F(" SEQ="));
    out.print(injection.sequence);
    out.print(F(" ARM_US="));
    out.println(injection.trainArmUs);
    return true;
  }

  if (serialCommandStartsWith(text, "PULSE", &remainder)) {
    uint32_t mask = 0;
    long durationMs = kDefaultPulseMs;
    if (!serialParseUint32Token(remainder, &mask) ||
        mask == 0 || (mask & ~kNeutralFrameButtonMask) != 0) {
      out.println(F("ERR:NFRAME_BAD_MASK"));
      return true;
    }
    if (*remainder != '\0' &&
        !serialParseLongToken(remainder, &durationMs)) {
      out.println(F("ERR:NFRAME_BAD_DURATION"));
      return true;
    }
    if (*remainder != '\0' || durationMs < kMinimumPulseMs ||
        durationMs > kMaximumPulseMs) {
      out.println(F("ERR:NFRAME_BAD_DURATION"));
      return true;
    }
    if (injectionBlocked(out)) return true;

    serialNeutralFrameInjectionCancel();
    beginRecord(mask, static_cast<uint16_t>(durationMs),
                InjectionPhase::Press);
    out.print(F("OK:NFRAME_PULSE MASK="));
    printHex32(out, mask);
    out.print(F(" MS="));
    out.print(durationMs);
    out.print(F(" SEQ="));
    out.println(injection.sequence);
    return true;
  }

  if (serialTokenEquals(text, "RELEASE")) {
    if (productNeutralReleaseMustCancel()) {
      serialNeutralFrameInjectionCancel();
      out.println(F("OK:NFRAME_RELEASE CANCELED=1"));
      return true;
    }
    serialNeutralFrameInjectionCancel();
    beginRecord(0, 0, InjectionPhase::Release);
    out.print(F("OK:NFRAME_RELEASE SEQ="));
    out.println(injection.sequence);
    return true;
  }

  out.println(F("ERR:BAD_NFRAME_CMD"));
  return true;
}

void serialNeutralFrameInjectionService() {
  if (injection.phase == InjectionPhase::Idle ||
      injection.phase == InjectionPhase::Done) {
    return;
  }
  if (productNeutralRuntimeBlocked()) {
    serialNeutralFrameInjectionCancel();
    return;
  }

  if (injection.mode == InjectionMode::Train) {
    const uint32_t nowUs = micros();
    if (injection.phase == InjectionPhase::TrainFinal) {
      if (injection.trainFinalCompleted) {
        if (timeReached(nowUs, injection.trainFinalCompletedUs +
                                  kTrainFinalHoldUs)) {
          finishTrain(false);
        }
      } else if (
          timeReached(nowUs, injection.trainFinalStartedUs +
                                 kTrainFinalTimeoutUs)) {
        finishTrain(true);
      }
      return;
    }

    if (injection.phase != InjectionPhase::TrainArm &&
        injection.phase != InjectionPhase::TrainEdge) {
      return;
    }
    if (!timeReached(nowUs, injection.trainArmUs)) return;

    if (injection.phase == InjectionPhase::TrainArm) {
      activateTrainEdge(1, injection.trainArmUs, nowUs);
      return;
    }

    // A HID/XInput endpoint carries state, not an event queue. The old
    // wall-clock scheduler jumped straight to the newest due tag whenever the
    // main loop crossed a period, permanently dropping every intermediate
    // edge. Keep gameplay latest-state semantics unchanged, but make this
    // diagnostic train completion-paced: one unique tag must complete before
    // the next can become visible.
    if (injection.trainLastCompletedTag != injection.trainEdge) {
      if (timeReached(nowUs,
                      injection.trainLastProgressUs + kTrainEdgeTimeoutUs)) {
        injection.trainTimedOut = true;
        activateTrainFinal(nowUs);
      } else if (injection.trainLastSubmittedTag != injection.trainEdge) {
        requestSelectedFrameDelivery();
      }
      return;
    }

    if (injection.trainEdge >= injection.trainCount) {
      activateTrainFinal(nowUs);
      return;
    }

    const uint16_t nextEdge = injection.trainEdge + 1u;
    const uint32_t scheduledUs = injection.trainArmUs +
        static_cast<uint32_t>(nextEdge - 1u) *
            injection.trainPeriodUs;
    if (timeReached(nowUs, scheduledUs)) {
      activateTrainEdge(nextEdge, scheduledUs, nowUs);
    }
    return;
  }

  if (!injection.phaseStaged) return;

  const uint32_t now = millis();
  if (injection.phase == InjectionPhase::Press &&
      timeReached(now, injection.phaseStartedAtMs + injection.durationMs)) {
    requestPhase(InjectionPhase::Release);
  } else if (injection.phase == InjectionPhase::Release &&
             timeReached(now, injection.phaseStartedAtMs + kReleaseHoldMs)) {
    injection.phase = InjectionPhase::Idle;
    injection.phaseStaged = false;
    injection.phaseStartedAtMs = 0;
  }
}

void serialNeutralFrameInjectionPrepareOutputFrame() {
  if (!phaseUsesOverlay(injection.phase) ||
      injection.framesBackedUpMask != 0) {
    return;
  }

  if (injection.mode == InjectionMode::Train) {
    const uint32_t nowUs = micros();
    if (injection.phase == InjectionPhase::TrainEdge &&
        injection.trainLastPreparedTag != injection.trainEdge) {
      recordFirstLastGap(nowUs, injection.trainFirstPrepareUs,
                         injection.trainLastPrepareUs,
                         injection.trainMaxPrepareGapUs);
      injection.trainLastPreparedTag = injection.trainEdge;
      ++injection.trainPreparedTags;
    } else if (injection.phase == InjectionPhase::TrainFinal &&
               !injection.trainFinalPrepared) {
      injection.trainFinalPrepared = true;
    }
  }

  controller_state_t frame{};
  frame.connected = true;
  frame.HAS_BTN_HOME = 1;
  frame.HAS_BTN_SELECT = 1;
  frame.HAS_BTN_START = 1;
  frame.sticks_precision_bits = ANALOG_STICK_PRECISION_16;
  frame.digital_buttons =
      injection.phase == InjectionPhase::Press ||
              injection.phase == InjectionPhase::TrainEdge
          ? injection.mask
          : 0;
  std::strncpy(frame.controller_type_name,
               injection.mode == InjectionMode::Train
                   ? "USB Train"
                   : "CDC Test",
               sizeof(frame.controller_type_name) - 1u);
  for (uint8_t player = 0; player < kOutputSlotCount; ++player) {
    const uint8_t playerBit = static_cast<uint8_t>(1u << player);
    if ((injection.playerMask & playerBit) == 0) continue;
    injection.frameBackup[player] = controllerFrameConst(player);
    injection.framesBackedUpMask |= playerBit;
    controllerFrame(player) = frame;
  }
}

void serialNeutralFrameInjectionNoteOutputStaged() {
  if (injection.framesBackedUpMask == 0 ||
      injection.phase == InjectionPhase::Idle) {
    return;
  }
  if (injection.mode == InjectionMode::Train) return;
  if (!injection.phaseStaged) {
    injection.phaseStaged = true;
    injection.phaseStartedAtMs = millis();
    if (injection.phase == InjectionPhase::Press) {
      injection.pressAppliedUs = micros();
    } else {
      injection.releaseAppliedUs = micros();
    }
  }
  if (injection.phase == InjectionPhase::Press) {
    ++injection.pressStageCount;
  } else {
    ++injection.releaseStageCount;
  }
}

void maybeFinishTrainTransfer(uint16_t tag, uint32_t nowUs) {
  if (injection.trainSubmittedPlayerMask != injection.playerMask ||
      injection.trainRequiredOutputMask == 0 ||
      (injection.trainCompletedOutputMask &
       injection.trainRequiredOutputMask) !=
          injection.trainRequiredOutputMask) {
    return;
  }

  if (tag == kFinalTrainTag) {
    injection.trainFinalCompleted = true;
    injection.trainFinalCompletedUs = nowUs;
    return;
  }
  if (tag != injection.trainEdge ||
      injection.trainLastCompletedTag == tag) {
    return;
  }

  injection.trainLastCompletedTag = tag;
  injection.trainLastProgressUs = nowUs;
  if (injection.trainFirstEdgeCompleteUs == 0) {
    injection.trainFirstEdgeCompleteUs = nowUs;
  }
  injection.trainLastEdgeCompleteUs = nowUs;
  ++injection.trainCompletedTags;
}

void serialNeutralFrameInjectionNoteUsbSubmit(uint8_t sourcePlayer,
                                               uint8_t outputSlot) {
  usbLatencyTraceNoteSubmit(sourcePlayer, outputSlot);
  if (injection.mode != InjectionMode::Train ||
      injection.framesBackedUpMask == 0 ||
      sourcePlayer >= kOutputSlotCount ||
      (injection.playerMask & (1u << sourcePlayer)) == 0 ||
      outputSlot >= kOutputSlotCount ||
      (injection.phase != InjectionPhase::TrainEdge &&
       injection.phase != InjectionPhase::TrainFinal)) {
    return;
  }

  uint16_t tag = kNoPendingTrainTag;
  if (injection.phase == InjectionPhase::TrainEdge) {
    tag = injection.trainEdge;
  } else {
    tag = kFinalTrainTag;
  }
  if (injection.pendingTrainTag[outputSlot] != kNoPendingTrainTag) {
    return;
  }

  const uint32_t nowUs = micros();
  injection.pendingTrainTag[outputSlot] = tag;
  injection.pendingTrainSourcePlayer[outputSlot] = sourcePlayer;
  injection.trainSubmittedPlayerMask |=
      static_cast<uint8_t>(1u << sourcePlayer);
  injection.trainRequiredOutputMask |=
      static_cast<uint8_t>(1u << outputSlot);
  ++injection.submitCount[outputSlot];
  recordFirstLastGap(nowUs, injection.firstSubmitUs[outputSlot],
                     injection.lastSubmitUs[outputSlot],
                     injection.maxSubmitGapUs[outputSlot]);
  if (tag == kFinalTrainTag &&
      injection.trainSubmittedPlayerMask == injection.playerMask) {
    injection.trainFinalSubmitted = true;
  } else if (tag != kFinalTrainTag &&
             injection.trainSubmittedPlayerMask == injection.playerMask &&
             injection.trainLastSubmittedTag != tag) {
    injection.trainLastSubmittedTag = tag;
    ++injection.trainSubmittedTags;
  }
  maybeFinishTrainTransfer(tag, nowUs);
}

void serialNeutralFrameInjectionNoteUsbComplete(uint8_t outputSlot,
                                                 bool success) {
  usbPollAlignmentNoteUpstreamComplete(outputSlot, success);
  usbLatencyTraceNoteComplete(outputSlot, success);
  if (outputSlot >= kOutputSlotCount) return;
  const uint16_t tag = injection.pendingTrainTag[outputSlot];
  if (tag == kNoPendingTrainTag) return;
  const uint8_t sourcePlayer =
      injection.pendingTrainSourcePlayer[outputSlot];
  injection.pendingTrainTag[outputSlot] = kNoPendingTrainTag;
  injection.pendingTrainSourcePlayer[outputSlot] = 0;

  if (!success) {
    ++injection.failedCompleteCount[outputSlot];
    ++injection.trainRetryCount;
    injection.trainSubmittedPlayerMask &=
        static_cast<uint8_t>(~(1u << sourcePlayer));
    injection.trainRequiredOutputMask &=
        static_cast<uint8_t>(~(1u << outputSlot));
    injection.trainCompletedOutputMask &=
        static_cast<uint8_t>(~(1u << outputSlot));
    if (tag == kFinalTrainTag) {
      injection.trainFinalSubmitted = false;
    } else if (tag == injection.trainEdge) {
      if (injection.trainLastSubmittedTag == tag &&
          injection.trainSubmittedTags != 0) {
        --injection.trainSubmittedTags;
      }
      injection.trainLastSubmittedTag = 0;
    }
    injection.trainLastProgressUs = micros();
    requestControllerFrameDelivery(sourcePlayer);
    return;
  }

  const uint32_t nowUs = micros();
  ++injection.completeCount[outputSlot];
  recordFirstLastGap(nowUs, injection.firstCompleteUs[outputSlot],
                     injection.lastCompleteUs[outputSlot],
                     injection.maxCompleteGapUs[outputSlot]);
  injection.trainCompletedOutputMask |=
      static_cast<uint8_t>(1u << outputSlot);
  injection.trainLastProgressUs = nowUs;
  maybeFinishTrainTransfer(tag, nowUs);
}

void serialNeutralFrameInjectionRestoreOutputFrame() {
  if (injection.framesBackedUpMask == 0) return;
  for (uint8_t player = 0; player < kOutputSlotCount; ++player) {
    const uint8_t playerBit = static_cast<uint8_t>(1u << player);
    if ((injection.framesBackedUpMask & playerBit) == 0) continue;
    controllerFrame(player) = injection.frameBackup[player];
  }
  injection.framesBackedUpMask = 0;
}

void serialNeutralFrameInjectionCancel() {
  serialNeutralFrameInjectionRestoreOutputFrame();
  requestSelectedFrameDelivery();
  for (uint8_t i = 0; i < kOutputSlotCount; ++i) {
    injection.pendingTrainTag[i] = kNoPendingTrainTag;
  }
  injection.mode = InjectionMode::None;
  injection.phase = InjectionPhase::Idle;
  injection.phaseStaged = false;
  injection.phaseStartedAtMs = 0;
}

bool serialNeutralFrameInjectionActive() {
  return injection.phase != InjectionPhase::Idle &&
         injection.phase != InjectionPhase::Done;
}

bool serialNeutralFrameInjectionOwnsPriorityOutput() {
  return injection.mode == InjectionMode::Train &&
         injection.phase != InjectionPhase::Idle &&
         injection.phase != InjectionPhase::Done;
}

#else

bool handleSerialNeutralFrameInjectionCommand(char*, Print&) {
  return false;
}

void serialNeutralFrameInjectionService() {}
void serialNeutralFrameInjectionPrepareOutputFrame() {}
void serialNeutralFrameInjectionNoteOutputStaged() {}
void serialNeutralFrameInjectionNoteUsbSubmit(uint8_t, uint8_t) {}
void serialNeutralFrameInjectionNoteUsbComplete(uint8_t outputSlot,
                                                 bool success) {
  usbPollAlignmentNoteUpstreamComplete(outputSlot, success);
}
void serialNeutralFrameInjectionRestoreOutputFrame() {}
void serialNeutralFrameInjectionCancel() {}
bool serialNeutralFrameInjectionActive() { return false; }
bool serialNeutralFrameInjectionOwnsPriorityOutput() { return false; }

#endif

// One callback owner fans out to optional observers. Capture never requests a
// retry or changes the delivery state in response to a completion/failure.
#if ADAPT_POLICY_SERIAL_NEUTRAL_FRAME_INJECTION || defined(ADAPT_FEATURE_N64_CAPTURE)
extern "C" void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report, uint16_t size) {
#if ADAPT_POLICY_SERIAL_NEUTRAL_FRAME_INJECTION
  serialNeutralFrameInjectionNoteUsbComplete(instance, true);
#endif
#if defined(ADAPT_FEATURE_N64_CAPTURE)
  n64CaptureComplete(instance, report, size, true);
#else
  (void)report; (void)size;
#endif
}
extern "C" void tud_hid_report_failed_cb(uint8_t instance, hid_report_type_t reportType, uint8_t const *report, uint16_t size) {
  if(reportType != HID_REPORT_TYPE_INPUT) return;
#if ADAPT_POLICY_SERIAL_NEUTRAL_FRAME_INJECTION
  serialNeutralFrameInjectionNoteUsbComplete(instance, false);
#endif
#if defined(ADAPT_FEATURE_N64_CAPTURE)
  n64CaptureComplete(instance, report, size, false);
#else
  (void)report; (void)size;
#endif
}
#endif
