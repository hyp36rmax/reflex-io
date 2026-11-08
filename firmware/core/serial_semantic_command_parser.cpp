#include "serial_semantic_command_parser.h"

#include "negcon_input_modes.h"
#include "psx_poll_timing.h"
#include "rotary_profile.h"
#include "serial_command_parser.h"

namespace {

SerialSemanticParseStatus parseOptionalMode(
    char*& text, DeviceEnum currentMode, DeviceEnum* mode) {
  text = serialSkipSpaces(text);
  if (*text == '\0') {
    *mode = currentMode;
    return SerialSemanticParseStatus::Ok;
  }
  long rawMode = -1;
  if (!serialParseLongToken(text, &rawMode) ||
      rawMode < 0 || rawMode >= RZORD_LAST ||
      *serialSkipSpaces(text) != '\0') {
    return SerialSemanticParseStatus::BadMode;
  }
  *mode = static_cast<DeviceEnum>(rawMode);
  return SerialSemanticParseStatus::Ok;
}

bool parseRotaryKind(char*& text, SerialRotaryProfileKind* kind) {
  text = serialSkipSpaces(text);
  if (serialTokenEquals(text, "GENERIC")) {
    *kind = SerialRotaryProfileKind::Generic;
  } else if (serialTokenEquals(text, "JOGCON")) {
    *kind = SerialRotaryProfileKind::JogCon;
  } else {
    return false;
  }
  while (*text != '\0' && *text != ' ' && *text != '\t') ++text;
  text = serialSkipSpaces(text);
  return true;
}

}  // namespace

SerialSemanticParseStatus parseSerialPsxTimingRequest(
    char* text, DeviceEnum currentMode, SerialPsxTimingRequest* request) {
  if (text == nullptr || request == nullptr) {
    return SerialSemanticParseStatus::BadArguments;
  }
  char* remainder = nullptr;
  request->write = serialCommandStartsWith(text, "SET", &remainder);
  if (!request->write &&
      !serialCommandStartsWith(text, "GET", &remainder)) {
    return SerialSemanticParseStatus::BadCommand;
  }

  long rate = 0;
  long gap = 0;
  if (request->write &&
      (!serialParseLongToken(remainder, &rate) ||
       !serialParseLongToken(remainder, &gap) ||
       rate < 0 || rate >= psx_poll::kRateCount ||
       gap < 0 || gap >= psx_poll::kGapCount)) {
    return SerialSemanticParseStatus::BadArguments;
  }
  request->rate = static_cast<uint8_t>(rate);
  request->gap = static_cast<uint8_t>(gap);
  return parseOptionalMode(remainder, currentMode, &request->mode);
}

SerialSemanticParseStatus parseSerialRotaryRequest(
    char* text, DeviceEnum currentMode, SerialRotaryRequest* request) {
  if (text == nullptr || request == nullptr) {
    return SerialSemanticParseStatus::BadArguments;
  }
  char* remainder = nullptr;
  request->write = serialCommandStartsWith(text, "SET", &remainder);
  if (!request->write &&
      !serialCommandStartsWith(text, "GET", &remainder)) {
    return SerialSemanticParseStatus::BadCommand;
  }
  if (!parseRotaryKind(remainder, &request->kind)) {
    return SerialSemanticParseStatus::BadProfile;
  }

  long gain = 0;
  long reverse = 0;
  if (request->write &&
      (!serialParseLongToken(remainder, &gain) ||
       !serialParseLongToken(remainder, &reverse) ||
       gain < 1 || gain > ROTARY_PROFILE_MAX_GAIN ||
       (reverse != 0 && reverse != 1))) {
    return SerialSemanticParseStatus::BadArguments;
  }
  request->gain = static_cast<uint8_t>(gain);
  request->reverse = reverse != 0;
  return parseOptionalMode(remainder, currentMode, &request->mode);
}

#ifdef ENABLE_INPUT_PSX
SerialSemanticParseStatus parseSerialNegconRequest(
    char* text, DeviceEnum currentMode, SerialNegconRequest* request) {
  if (text == nullptr || request == nullptr) {
    return SerialSemanticParseStatus::BadArguments;
  }
  char* remainder = nullptr;
  request->write = serialCommandStartsWith(text, "SET", &remainder);
  if (!request->write &&
      !serialCommandStartsWith(text, "GET", &remainder)) {
    return SerialSemanticParseStatus::BadCommand;
  }

  request->digital = 0;
  if (request->write) {
    const uint8_t inputs[] = {NEGCON_INPUT_I, NEGCON_INPUT_II, NEGCON_INPUT_L};
    for (uint8_t input : inputs) {
      long digital = 0;
      if (!serialParseLongToken(remainder, &digital) ||
          (digital != 0 && digital != 1)) {
        return SerialSemanticParseStatus::BadArguments;
      }
      if (digital != 0) request->digital |= input;
    }
  }
  return parseOptionalMode(remainder, currentMode, &request->mode);
}
#endif
