#pragma once

#include <stdint.h>

#include "device_mode.h"
#include "serial_semantic_settings.h"

enum class SerialSemanticParseStatus : uint8_t {
  Ok = 0,
  BadCommand,
  BadProfile,
  BadArguments,
  BadMode,
};

struct SerialPsxTimingRequest {
  bool write;
  uint8_t rate;
  uint8_t gap;
  DeviceEnum mode;
};

struct SerialRotaryRequest {
  bool write;
  SerialRotaryProfileKind kind;
  uint8_t gain;
  bool reverse;
  DeviceEnum mode;
};

struct SerialNegconRequest {
  bool write;
  uint8_t digital;  // NEGCON_INPUT_* bits set to Digital
  DeviceEnum mode;
};

SerialSemanticParseStatus parseSerialPsxTimingRequest(
    char* text, DeviceEnum currentMode, SerialPsxTimingRequest* request);
SerialSemanticParseStatus parseSerialRotaryRequest(
    char* text, DeviceEnum currentMode, SerialRotaryRequest* request);
// GET [MODE] or SET <I 0|1> <II 0|1> <L 0|1> [MODE]; 1 is Digital.
SerialSemanticParseStatus parseSerialNegconRequest(
    char* text, DeviceEnum currentMode, SerialNegconRequest* request);
