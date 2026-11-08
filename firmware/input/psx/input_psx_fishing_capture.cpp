#include "Input_Psx.h"
#include "../../core/serial_command_parser.h"

namespace {
bool fishingUnsigned(char*& text, uint32_t& value) {
  text = serialSkipSpaces(text);
  if (*text < '0' || *text > '9') return false;
  value = 0;
  while (*text >= '0' && *text <= '9') {
    const uint32_t digit = uint32_t(*text++ - '0');
    if (value > (UINT32_MAX - digit) / 10) return false;
    value = value * 10 + digit;
  }
  if (*text && *text != ' ' && *text != '\t') return false;
  text = serialSkipSpaces(text);
  return true;
}

void fishingHex(Print& out, const uint8_t* raw) {
  for (uint8_t i = 0; i < 8; ++i) {
    if (raw[i] < 16) out.print('0');
    out.print(raw[i], HEX);
  }
}

void fishingValues(Print& out, const uint8_t* raw) {
  for (uint8_t i = 0; i < 8; ++i) {
    if (i) out.print(',');
    out.print(unsigned(raw[i]));
  }
}
}  // namespace

void RZInputPSX::printFishingCaptureStatus(Print& out, uint32_t nowUs) {
  const auto state = fishingCapture.state();
  const auto reason = fishingCapture.reason();
  out.print(F("FISHCAP STATUS GEN=")); out.print(fishingCapture.generation());
  out.print(F(" STATE="));
  out.print(state == psx_fishing::Capture::State::Idle ? "IDLE" :
            state == psx_fishing::Capture::State::Running ? "RUNNING" : "FROZEN");
  out.print(F(" SLOT=")); out.print(unsigned(fishingCapture.slot()) + 1);
  out.print(F(" MS=")); out.print(fishingCapture.durationMs());
  out.print(F(" US=")); out.print(fishingCapture.elapsedUs(nowUs));
  out.print(F(" N=")); out.print(fishingCapture.polls());
  out.print(F(" NZ=")); out.print(fishingCapture.nonzero());
  out.print(F(" HIGH=")); out.print(fishingCapture.high());
  out.print(F(" BAD=")); out.print(fishingCapture.invalid());
  out.print(F(" EV=")); out.print(unsigned(fishingCapture.eventCount()));
  out.print(F(" OMIT=")); out.print(fishingCapture.omitted());
  out.print(F(" WHY="));
  out.println(reason == psx_fishing::Capture::Reason::Time ? "TIME" :
              reason == psx_fishing::Capture::Reason::Stop ? "STOP" :
              reason == psx_fishing::Capture::Reason::Disconnect ? "DISCONNECT" : "NONE");
}

void RZInputPSX::handleFishingCaptureCommand(char* command, Print& out) {
  const uint32_t nowUs = micros();
  fishingCapture.tick(nowUs);
  char* args = nullptr;
  uint32_t value = 0, durationMs = 0;
  if (serialCommandStartsWith(command, "START", &args)) {
    if (!fishingUnsigned(args, value) || value == 0 || value > input_ports) {
      out.println(F("ERR:FISHCAP_BAD_SLOT")); return;
    }
    if (!fishingUnsigned(args, durationMs) || *args || durationMs < 1000 || durationMs > 30000) {
      out.println(F("ERR:FISHCAP_BAD_MS")); return;
    }
    // The existing multitap decoder does not classify fishing replies.
    // Capture direct physical ports only rather than implying tap coverage.
    if (isMultitap || multitapPhysicalPresent) {
      out.println(F("ERR:FISHCAP_MULTITAP")); return;
    }
    if (fishingCapture.state() == psx_fishing::Capture::State::Running) {
      out.println(F("ERR:FISHCAP_ACTIVE")); return;
    }
    if (!haveController[value - 1] || lastProto[value - 1] != PSPROTO_FISHING) {
      out.println(F("ERR:FISHCAP_NO_FISHING")); return;
    }
    fishingCapture.start(uint8_t(value - 1), durationMs, nowUs);
    out.print(F("FISHCAP START GEN=")); out.print(fishingCapture.generation());
    out.print(F(" SLOT=")); out.print(value);
    out.print(F(" MS=")); out.println(durationMs);
    return;
  }
  if (serialTextEqualsExact(command, "STOP") || serialTextEqualsExact(command, "STATUS") || !*command) {
    if (serialTextEqualsExact(command, "STOP")) fishingCapture.stop(nowUs);
    printFishingCaptureStatus(out, nowUs);
    return;
  }
  const bool hist = serialCommandStartsWith(command, "HIST", &args);
  const bool event = !hist && serialCommandStartsWith(command, "EVENT", &args);
  const bool sensor = serialTextEqualsExact(command, "SENSOR");
  if (!hist && !event && !sensor) { out.println(F("ERR:FISHCAP_BAD_COMMAND")); return; }
  if (fishingCapture.state() != psx_fishing::Capture::State::Frozen) {
    out.println(F("ERR:FISHCAP_NOT_FROZEN")); return;
  }
  if (hist) {
    if (!fishingUnsigned(args, value) || *args || value > 240 || value % 16) {
      out.println(F("ERR:FISHCAP_BAD_OFFSET")); return;
    }
    out.print(F("FISHCAP HIST GEN=")); out.print(fishingCapture.generation());
    out.print(F(" OFFSET=")); out.print(value);
    out.print(F(" COUNTS="));
    for (uint8_t i = 0; i < 16; ++i) {
      if (i) out.print(',');
      out.print(fishingCapture.histogram(uint8_t(value + i)));
    }
    out.println();
  } else if (event) {
    if (!fishingUnsigned(args, value) || *args || value >= fishingCapture.eventCount()) {
      out.println(F("ERR:FISHCAP_BAD_INDEX")); return;
    }
    const auto* record = fishingCapture.event(uint8_t(value));
    out.print(F("FISHCAP EVENT GEN=")); out.print(fishingCapture.generation());
    out.print(F(" INDEX=")); out.print(value);
    out.print(F(" US=")); out.print(record->elapsedUs);
    out.print(F(" RAW=")); fishingHex(out, record->raw); out.println();
  } else {
    out.print(F("FISHCAP SENSOR GEN=")); out.print(fishingCapture.generation());
    out.print(F(" MIN=")); fishingValues(out, fishingCapture.minimum());
    out.print(F(" MAX=")); fishingValues(out, fishingCapture.maximum());
    out.print(F(" FIRST=")); fishingHex(out, fishingCapture.first());
    out.print(F(" LAST=")); fishingHex(out, fishingCapture.last()); out.println();
  }
}
