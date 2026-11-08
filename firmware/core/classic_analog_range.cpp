#include "classic_analog_range.h"

#include <Arduino.h>

namespace {

constexpr uint8_t kClassicAnalogLearnPorts = 4;
constexpr uint8_t kClassicAnalogLearnInitialPercent = 90;
constexpr int8_t kClassicAnalogAutoCenterMaxOffset = 10;
constexpr int8_t kClassicAnalogAutoCenterSnapThreshold = 2;
constexpr uint8_t kN64AnalogLearnInitialMax = 80;
// Measured GameCube cardinals run from 88 (an OEM C-stick) to 119. Start
// below the weakest so every direction reaches full, then learn outward.
constexpr uint8_t kGcAnalogLearnInitialMax = 80;
constexpr uint8_t kGcAnalogLearnFullMax = 100;
constexpr uint8_t kWiiAnalogLearnFullMax = 100;
constexpr uint8_t kGcTriggerLearnFullMax = 160;
constexpr uint16_t kFullTriggerLearnMax = 255;
// Ideal OEM N64 stick: cardinals 85, corners 70 per axis (wermipls/mimi).
// Corners start at 55 so an unswept corner runs hot rather than slow, and stop
// learning at 65 so ordinary pushes short of a worn or noisy maximum still
// reach the full OEM corner.
constexpr int8_t kN64CornerLearnInitial = 55;
constexpr int8_t kN64CornerLearnCap = 65;
constexpr int16_t kN64CornerOutput = (127 * 70 + 42) / 85;  // 105
// Readings within 15 degrees of a diagonal teach its corner (tan 30 = 0.577).
constexpr int32_t kN64CornerWindowTanMilli = 577;

struct StickCornerLearn {
  int8_t initial;  // corner reach assumed until a corner is swept
  int8_t cap;      // corner reach stops learning here
  int16_t output;  // per-axis output at the learned corner
};
constexpr StickCornerLearn kN64CornerLearn = {
    kN64CornerLearnInitial, kN64CornerLearnCap, kN64CornerOutput};
// GameCube main and C sticks: measured corner components run 64-91. A corner
// maps to full output on both axes, as the Wii Classic range does, and stops
// learning at 60 so the weakest measured corner still gets there.
constexpr StickCornerLearn kGcCornerLearn = {55, 60, 127};
constexpr uint8_t kClassicAnalogLearnSticks = CLASSIC_ANALOG_AXIS_COUNT / 2;

struct ClassicAnalogLearnAxisState {
  int8_t min_value;
  int8_t max_value;
  int8_t raw;
  int8_t calibrated;
  int8_t center_value;
  bool valid;
  bool center_valid;
};

struct ClassicAnalogLearnTriggerState {
  uint8_t min_value;
  bool full_scale_seen;
  uint8_t max_value;
  uint8_t raw;
  uint8_t calibrated;
  bool valid;
};

ClassicAnalogLearnAxisState learnState[kClassicAnalogLearnPorts][CLASSIC_ANALOG_AXIS_COUNT];
// Per port, stick and quadrant (bit 0: x negative, bit 1: y negative).
struct StickCornerState {
  int8_t x;
  int8_t y;
};
StickCornerState cornerState[kClassicAnalogLearnPorts][kClassicAnalogLearnSticks][4];
ClassicAnalogLearnTriggerState triggerLearnState[kClassicAnalogLearnPorts][CLASSIC_ANALOG_TRIGGER_COUNT];
DeviceEnum learnMode[kClassicAnalogLearnPorts] = {};

int8_t clampInt8(int16_t value) {
  if (value < INT8_MIN) return INT8_MIN;
  if (value > INT8_MAX) return INT8_MAX;
  return (int8_t)value;
}

constexpr uint8_t learnInitialFromFullMax(uint16_t fullMax) {
  return (uint8_t)((fullMax * kClassicAnalogLearnInitialPercent) / 100u);
}

int8_t initialLearnMagnitude(DeviceEnum mode, ClassicAnalogAxis axis) {
  (void)axis;
  #ifdef ENABLE_INPUT_N64
  if (mode == RZORD_N64) {
    return (int8_t)kN64AnalogLearnInitialMax;
  }
  #endif
  #ifdef ENABLE_INPUT_GAMECUBE
  if (mode == RZORD_GAMECUBE) {
    return (int8_t)kGcAnalogLearnInitialMax;
  }
  #endif
  #ifdef ENABLE_INPUT_WII
  if (mode == RZORD_WII) {
    return (int8_t)learnInitialFromFullMax(kWiiAnalogLearnFullMax);
  }
  #endif
  return (int8_t)learnInitialFromFullMax(kGcAnalogLearnFullMax);
}

const StickCornerLearn& cornerLearnForMode(DeviceEnum mode) {
  #ifdef ENABLE_INPUT_GAMECUBE
  if (mode == RZORD_GAMECUBE) {
    return kGcCornerLearn;
  }
  #endif
  (void)mode;
  return kN64CornerLearn;
}

uint8_t initialTriggerLearnMax(DeviceEnum mode) {
  #ifdef ENABLE_INPUT_GAMECUBE
  if (mode == RZORD_GAMECUBE) {
    return learnInitialFromFullMax(kGcTriggerLearnFullMax);
  }
  #endif
  (void)mode;
  return learnInitialFromFullMax(kFullTriggerLearnMax);
}

void initializeAxis(DeviceEnum mode, uint8_t port, ClassicAnalogAxis axis) {
  if (port >= kClassicAnalogLearnPorts || axis >= CLASSIC_ANALOG_AXIS_COUNT) {
    return;
  }

  const int8_t magnitude = initialLearnMagnitude(mode, axis);
  ClassicAnalogLearnAxisState& axisState = learnState[port][axis];
  axisState.min_value = -magnitude;
  axisState.max_value = magnitude;
  axisState.raw = 0;
  axisState.calibrated = 0;
  axisState.center_value = 0;
  axisState.valid = false;
  axisState.center_valid = false;
}

void ensureAxis(DeviceEnum mode, uint8_t port, ClassicAnalogAxis axis) {
  if (port >= kClassicAnalogLearnPorts || axis >= CLASSIC_ANALOG_AXIS_COUNT) {
    return;
  }
  if (learnMode[port] != mode) {
    resetClassicAnalogLearnState(mode, port);
  }
}

void initializeTrigger(DeviceEnum mode, uint8_t port, ClassicAnalogTrigger trigger) {
  if (port >= kClassicAnalogLearnPorts || trigger >= CLASSIC_ANALOG_TRIGGER_COUNT) {
    return;
  }

  ClassicAnalogLearnTriggerState& triggerState = triggerLearnState[port][trigger];
  triggerState.min_value = 0;
  triggerState.max_value = initialTriggerLearnMax(mode);
  triggerState.raw = 0;
  triggerState.calibrated = 0;
  triggerState.valid = false;
  triggerState.full_scale_seen = false;
}

void ensureTrigger(DeviceEnum mode, uint8_t port, ClassicAnalogTrigger trigger) {
  if (port >= kClassicAnalogLearnPorts || trigger >= CLASSIC_ANALOG_TRIGGER_COUNT) {
    return;
  }
  if (learnMode[port] != mode) {
    resetClassicAnalogLearnState(mode, port);
  }
}

int32_t scaleSignedAxisLinear(int16_t raw, int8_t rawMin, int8_t rawMax) {
  if (rawMin >= 0 || rawMax <= 0) {
    return clampInt8(raw);
  }
  if (raw <= 0) {
    if (raw < rawMin) raw = rawMin;
    return ((int32_t)raw * 127) / (-rawMin);
  }
  if (raw > rawMax) raw = rawMax;
  return ((int32_t)raw * 127) / rawMax;
}

int8_t finishScaledAxis(int32_t normalized) {
  if (normalized > -4 && normalized < 4) {
    return 0;
  }
  return (int8_t)constrain(normalized, -127, 127);
}

int8_t scaleSignedAxis(int16_t raw, int8_t rawMin, int8_t rawMax) {
  if (rawMin >= 0 || rawMax <= 0) {
    return clampInt8(raw);
  }
  return finishScaledAxis(scaleSignedAxisLinear(raw, rawMin, rawMax));
}

void growLearnedRange(ClassicAnalogLearnAxisState& axisState, int8_t raw) {
  if (raw < axisState.min_value) {
    axisState.min_value = raw;
  }
  if (raw > axisState.max_value) {
    axisState.max_value = raw;
  }
}

int16_t absInt16(int16_t value) {
  return value < 0 ? -value : value;
}

int8_t centerClassicAnalogLearnAxisRaw(ClassicAnalogLearnAxisState& axisState,
                                       int16_t raw) {
  const int8_t raw8 = clampInt8(raw);
  if (!axisState.center_valid &&
      absInt16(raw8) <= kClassicAnalogAutoCenterMaxOffset) {
    axisState.center_value = raw8;
    axisState.center_valid = true;
  }

  if (!axisState.center_valid) {
    return raw8;
  }

  int16_t centered = (int16_t)raw8 - (int16_t)axisState.center_value;
  if (absInt16(centered) <= kClassicAnalogAutoCenterSnapThreshold) {
    centered = 0;
  }
  return clampInt8(centered);
}

uint8_t scaleUnsignedTrigger(uint8_t raw, uint8_t rawMin, uint8_t rawMax) {
  if (rawMax <= rawMin) {
    return raw;
  }
  if (raw <= rawMin) {
    return 0;
  }
  if (raw >= rawMax) {
    return 255;
  }
  return (uint8_t)(((uint32_t)(raw - rawMin) * 255u) / (rawMax - rawMin));
}

}  // namespace

void resetClassicAnalogLearnState(DeviceEnum mode, uint8_t port) {
  if (port >= kClassicAnalogLearnPorts) {
    return;
  }
  learnMode[port] = mode;
  for (uint8_t axis = 0; axis < CLASSIC_ANALOG_AXIS_COUNT; ++axis) {
    initializeAxis(mode, port, (ClassicAnalogAxis)axis);
  }
  const int8_t cornerInitial = cornerLearnForMode(mode).initial;
  for (auto& stickCorners : cornerState[port]) {
    for (StickCornerState& corner : stickCorners) {
      corner.x = cornerInitial;
      corner.y = cornerInitial;
    }
  }
  for (uint8_t trigger = 0; trigger < CLASSIC_ANALOG_TRIGGER_COUNT; ++trigger) {
    initializeTrigger(mode, port, (ClassicAnalogTrigger)trigger);
  }
}

void recordClassicAnalogRangeAxis(DeviceEnum mode, uint8_t port, ClassicAnalogAxis axis,
                                  int16_t raw, int16_t calibrated) {
  if (port >= kClassicAnalogLearnPorts || axis >= CLASSIC_ANALOG_AXIS_COUNT) {
    return;
  }
  ensureAxis(mode, port, axis);

  ClassicAnalogLearnAxisState& axisState = learnState[port][axis];
  axisState.raw = clampInt8(raw);
  axisState.calibrated = clampInt8(calibrated);
  axisState.valid = true;
}

int8_t applyClassicAnalogLearnAxis(DeviceEnum mode, uint8_t port, ClassicAnalogAxis axis,
                                   int16_t raw, uint8_t full_scale_limit) {
  if (port >= kClassicAnalogLearnPorts || axis >= CLASSIC_ANALOG_AXIS_COUNT) {
    return clampInt8(raw);
  }
  ensureAxis(mode, port, axis);

  ClassicAnalogLearnAxisState& axisState = learnState[port][axis];
  const int8_t raw8 = centerClassicAnalogLearnAxisRaw(axisState, raw);
  growLearnedRange(axisState, raw8);

  // A controller with an octagonal gate can deliberately reach full output
  // before its cardinal extremes. Keep that limit after subsequent sweeps;
  // changing only the initial learned range would immediately grow it away.
  if (full_scale_limit > 0 && full_scale_limit <= INT8_MAX) {
    if (axisState.min_value < -(int16_t)full_scale_limit) {
      axisState.min_value = -(int8_t)full_scale_limit;
    }
    if (axisState.max_value > (int8_t)full_scale_limit) {
      axisState.max_value = (int8_t)full_scale_limit;
    }
  }

  const int8_t calibrated = scaleSignedAxis(raw8, axisState.min_value, axisState.max_value);

  axisState.raw = raw8;
  axisState.calibrated = calibrated;
  axisState.valid = true;
  return calibrated;
}

namespace {

int8_t blendCornerAxis(int8_t value, const ClassicAnalogLearnAxisState& axisState,
                       int8_t corner, int16_t cornerOutput,
                       int16_t minorMagnitude, int16_t majorMagnitude) {
  if (majorMagnitude == 0) {
    return 0;
  }
  const int32_t cardinal =
      scaleSignedAxisLinear(value, axisState.min_value, axisState.max_value);
  const int32_t diagonal = ((int32_t)value * cornerOutput) / corner;
  // Weight 0 on a cardinal, 1 on an exact diagonal: continuous in angle.
  return finishScaledAxis((cardinal * (majorMagnitude - minorMagnitude) +
                           diagonal * minorMagnitude) / majorMagnitude);
}

}  // namespace

void applyClassicAnalogLearnStick(DeviceEnum mode, uint8_t port, ClassicAnalogAxis xAxis,
                                  int16_t rawX, int16_t rawY, int8_t* outX, int8_t* outY) {
  if (port >= kClassicAnalogLearnPorts ||
      (xAxis != CLASSIC_ANALOG_AXIS_LX && xAxis != CLASSIC_ANALOG_AXIS_RX)) {
    *outX = clampInt8(rawX);
    *outY = clampInt8(rawY);
    return;
  }
  ensureAxis(mode, port, xAxis);

  const StickCornerLearn& learn = cornerLearnForMode(mode);
  ClassicAnalogLearnAxisState& xState = learnState[port][xAxis];
  ClassicAnalogLearnAxisState& yState = learnState[port][xAxis + 1];
  const int8_t x = centerClassicAnalogLearnAxisRaw(xState, rawX);
  const int8_t y = centerClassicAnalogLearnAxisRaw(yState, rawY);
  growLearnedRange(xState, x);
  growLearnedRange(yState, y);

  const int16_t absX = absInt16(x);
  const int16_t absY = absInt16(y);
  const int16_t minor = absX < absY ? absX : absY;
  const int16_t major = absX < absY ? absY : absX;
  StickCornerState& corner =
      cornerState[port][xAxis / 2][(x < 0 ? 1 : 0) | (y < 0 ? 2 : 0)];
  if (minor > 0 && (int32_t)minor * 1000 >= (int32_t)major * kN64CornerWindowTanMilli) {
    if (absX > corner.x) {
      corner.x = (int8_t)(absX < learn.cap ? absX : learn.cap);
    }
    if (absY > corner.y) {
      corner.y = (int8_t)(absY < learn.cap ? absY : learn.cap);
    }
  }

  *outX = blendCornerAxis(x, xState, corner.x, learn.output, minor, major);
  *outY = blendCornerAxis(y, yState, corner.y, learn.output, minor, major);
  xState.raw = x;
  xState.calibrated = *outX;
  xState.valid = true;
  yState.raw = y;
  yState.calibrated = *outY;
  yState.valid = true;
}

void applyClassicAnalogLearnN64Stick(DeviceEnum mode, uint8_t port, int16_t rawX, int16_t rawY,
                                     int8_t* outX, int8_t* outY) {
  applyClassicAnalogLearnStick(mode, port, CLASSIC_ANALOG_AXIS_LX, rawX, rawY, outX, outY);
}

void recordClassicAnalogRangeTrigger(DeviceEnum mode, uint8_t port, ClassicAnalogTrigger trigger,
                                     uint8_t raw, uint8_t calibrated) {
  if (port >= kClassicAnalogLearnPorts || trigger >= CLASSIC_ANALOG_TRIGGER_COUNT) {
    return;
  }
  ensureTrigger(mode, port, trigger);

  ClassicAnalogLearnTriggerState& triggerState = triggerLearnState[port][trigger];
  triggerState.raw = raw;
  triggerState.calibrated = calibrated;
  triggerState.valid = true;
}

uint8_t applyClassicAnalogLearnTrigger(DeviceEnum mode, uint8_t port, ClassicAnalogTrigger trigger,
                                       uint8_t raw, bool full_scale_pressed) {
  if (port >= kClassicAnalogLearnPorts || trigger >= CLASSIC_ANALOG_TRIGGER_COUNT) {
    return raw;
  }
  ensureTrigger(mode, port, trigger);

  ClassicAnalogLearnTriggerState& triggerState = triggerLearnState[port][trigger];
  if (full_scale_pressed && raw > triggerState.min_value &&
      !triggerState.full_scale_seen) {
    triggerState.max_value = raw;
    triggerState.full_scale_seen = true;
  } else if (raw < triggerState.min_value) {
    triggerState.min_value = raw;
  } else if (raw > triggerState.max_value) {
    triggerState.max_value = raw;
  }

  const uint8_t calibrated = scaleUnsignedTrigger(
      raw, triggerState.min_value, triggerState.max_value);
  triggerState.raw = raw;
  triggerState.calibrated = calibrated;
  triggerState.valid = true;
  return calibrated;
}

bool getClassicAnalogRangeSnapshot(DeviceEnum mode, uint8_t port,
                                   ClassicAnalogRangeSnapshot& snapshot) {
  if (port >= kClassicAnalogLearnPorts) {
    return false;
  }
  ensureAxis(mode, port, CLASSIC_ANALOG_AXIS_LX);

  bool anyValid = false;
  for (uint8_t axis = 0; axis < CLASSIC_ANALOG_AXIS_COUNT; ++axis) {
    const ClassicAnalogLearnAxisState& axisState = learnState[port][axis];
    snapshot.raw[axis] = axisState.raw;
    snapshot.calibrated[axis] = axisState.calibrated;
    snapshot.learned_min[axis] = axisState.min_value;
    snapshot.learned_max[axis] = axisState.max_value;
    snapshot.valid[axis] = axisState.valid;
    anyValid = anyValid || axisState.valid;
  }
  for (uint8_t trigger = 0; trigger < CLASSIC_ANALOG_TRIGGER_COUNT; ++trigger) {
    const ClassicAnalogLearnTriggerState& triggerState = triggerLearnState[port][trigger];
    snapshot.trigger_raw[trigger] = triggerState.raw;
    snapshot.trigger_calibrated[trigger] = triggerState.calibrated;
    snapshot.trigger_learned_min[trigger] = triggerState.min_value;
    snapshot.trigger_learned_max[trigger] = triggerState.max_value;
    snapshot.trigger_valid[trigger] = triggerState.valid;
    anyValid = anyValid || triggerState.valid;
  }
  return anyValid;
}
