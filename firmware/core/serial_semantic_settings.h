#pragma once

#include <stdint.h>

#include "device_mode.h"

enum class SerialSemanticStatus : uint8_t {
  Ok = 0,
  UnsupportedMode,
  BadValue,
  WriteFailed,
};

struct SerialPsxTimingValue {
  uint8_t rate;
  uint8_t gap;
  bool configured;
  bool live;
};

enum class SerialRotaryProfileKind : uint8_t {
  Generic = 0,
  JogCon,
};

struct SerialRotaryProfileValue {
  uint8_t gain;
  bool reverse;
  bool configured;
  bool live;
};

struct SerialNegconInputsValue {
  uint8_t digital;  // NEGCON_INPUT_* bits set to Digital
  bool live;
};

SerialSemanticStatus serialReadPsxTiming(
    DeviceEnum mode, SerialPsxTimingValue* value);
SerialSemanticStatus serialWritePsxTiming(
    DeviceEnum mode, uint8_t rate, uint8_t gap,
    SerialPsxTimingValue* value);

bool serialRotaryProfileApplies(
    DeviceEnum mode, SerialRotaryProfileKind kind);
SerialSemanticStatus serialReadRotaryProfile(
    DeviceEnum mode, SerialRotaryProfileKind kind,
    SerialRotaryProfileValue* value);
SerialSemanticStatus serialWriteRotaryProfile(
    DeviceEnum mode, SerialRotaryProfileKind kind,
    uint8_t gain, bool reverse, SerialRotaryProfileValue* value);

SerialSemanticStatus serialReadNegconInputs(
    DeviceEnum mode, SerialNegconInputsValue* value);
SerialSemanticStatus serialWriteNegconInputs(
    DeviceEnum mode, uint8_t digital, SerialNegconInputsValue* value);
