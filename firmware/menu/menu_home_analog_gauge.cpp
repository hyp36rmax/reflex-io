#include "menu_home_analog_gauge.h"
#include "oled_pixel_surface.h"

#include <cstring>

#include "../product_config.h"
#include "analog_diagnostic.h"
#include "analog_test_renderer.h"
#include "analog_stick_trace.h"
#include "menu.h"
#include "../input/runtime/input_frame_runtime.h"
#include "../output/output_capabilities.h"

namespace {
constexpr uint8_t kPadGlyphWidth = 6;
constexpr uint8_t kSecondPadColOffset = 66;
constexpr uint8_t kGaugeWidth = kSecondPadColOffset - (2 * kPadGlyphWidth);
constexpr uint8_t kPadColumnOffset = kGaugeWidth + kPadGlyphWidth;
constexpr uint8_t kGaugeRow = 3;
constexpr uint8_t kGaugeHeight = 32;
constexpr uint8_t kGaugePages = kGaugeHeight / 8;
constexpr int8_t kCenterX = 30;
constexpr int8_t kCenterY = 15;
constexpr int8_t kRadius = 14;
constexpr int8_t kDotRadius = 1;
constexpr int16_t kN64FullScale = 85;
constexpr uint8_t kGaugeEmpty = 0x22;
constexpr uint8_t kGaugeFilled = 0x3E;

enum class GaugeKind : uint8_t { None = 0, Gate, DualAxes };

struct GaugeState {
  bool initialized = false;
  DeviceEnum mode = RZORD_NONE;
  GaugeKind kind = GaugeKind::None;
  bool octagonal = false;
  int16_t fullScale = 0;
  int8_t dotX = kCenterX;
  int8_t dotY = kCenterY;
  int16_t axes[4] = {0, 0, 0, 0};
  uint8_t columnOffset = 0;
};

GaugeState state;

uint8_t connectedControllerCount() {
  const uint8_t players = output_shown_player_count(get_effective_output_mode());
  uint8_t connected = 0;
  for (uint8_t i = 0; i < players && i < MAX_USB_OUT; ++i) {
    if (controllerFrameConst(i).connected) ++connected;
  }
  return connected;
}

GaugeKind kindForFrame(DeviceEnum mode, const controller_state_t& frame) {
  if (!frame.connected || !frame.HAS_ANALOG_STICK_MAIN) {
    return GaugeKind::None;
  }
  if (analogDiagnosticKindForFrame(mode, frame) != AnalogDiagnosticKind::Stick) {
    return GaugeKind::None;
  }
  return frame.HAS_ANALOG_STICK_AUX ? GaugeKind::DualAxes : GaugeKind::Gate;
}

int16_t fullScaleForFrame(DeviceEnum mode, const controller_state_t& frame) {
#ifdef ENABLE_INPUT_N64
  if (mode == RZORD_N64) return kN64FullScale;
#endif
  return analogTraceFullScale(frame.sticks_precision_bits);
}

int8_t coordinate(int8_t center, int16_t value, int16_t fullScale) {
  if (fullScale <= 0) return center;
  int32_t clamped = value;
  if (clamped < -fullScale) clamped = -fullScale;
  if (clamped > fullScale) clamped = fullScale;
  return static_cast<int8_t>(center + (clamped * kRadius) / fullScale);
}

bool backgroundPixel(uint8_t x, uint8_t y, bool octagonal) {
  const int16_t dx = static_cast<int16_t>(x) - kCenterX;
  const int16_t dy = static_cast<int16_t>(y) - kCenterY;
  const int16_t ax = dx < 0 ? -dx : dx;
  const int16_t ay = dy < 0 ? -dy : dy;
  const bool crosshair = (dx == 0 && (ay % 4) == 0) ||
                         (dy == 0 && (ax % 4) == 0);
  if (!octagonal) {
    const int16_t distanceSquared = dx * dx + dy * dy;
    const int16_t radiusSquared = kRadius * kRadius;
    if (crosshair && distanceSquared <= radiusSquared) return true;
    const int16_t error = distanceSquared > radiusSquared
      ? distanceSquared - radiusSquared : radiusSquared - distanceSquared;
    return error <= kRadius;
  }

  constexpr int16_t shoulder = 10;
  constexpr int16_t corner = kRadius - shoulder;
  const bool insideGate = ax <= kRadius && ay <= kRadius &&
    corner * ax + shoulder * ay <= shoulder * kRadius &&
    shoulder * ax + corner * ay <= shoulder * kRadius;
  if (crosshair && insideGate) return true;
  const int16_t target = shoulder * kRadius;
  const int16_t edgeA = corner * ax + shoulder * ay;
  const int16_t edgeB = shoulder * ax + corner * ay;
  constexpr int16_t tolerance = 5;
  const int16_t errorA = edgeA > target ? edgeA - target : target - edgeA;
  const int16_t errorB = edgeB > target ? edgeB - target : target - edgeB;
  return (errorA <= tolerance && edgeB <= target + tolerance) ||
         (errorB <= tolerance && edgeA <= target + tolerance);
}

uint8_t columnByte(uint8_t x, uint8_t page, int8_t dotX, int8_t dotY,
                   bool octagonal) {
  uint8_t column = 0;
  const uint8_t firstY = page * 8;
  for (uint8_t bit = 0; bit < 8; ++bit) {
    const uint8_t y = firstY + bit;
    const int16_t dotDx = static_cast<int16_t>(x) - dotX;
    const int16_t dotDy = static_cast<int16_t>(y) - dotY;
    const bool dot = dotDx >= -kDotRadius && dotDx <= kDotRadius &&
                     dotDy >= -kDotRadius && dotDy <= kDotRadius;
    if (dot || backgroundPixel(x, y, octagonal)) column |= 1u << bit;
  }
  return column;
}

void writeSpan(uint8_t first, uint8_t last, int8_t dotX, int8_t dotY,
               bool octagonal) {
  if (first >= kGaugeWidth) return;
  if (last >= kGaugeWidth) last = kGaugeWidth - 1;
  for (uint8_t page = 0; page < kGaugePages; ++page) {
    display.setCursor(first, kGaugeRow + page);
    for (uint8_t col = first; col <= last; ++col) {
      display.ssd1306WriteRamBuf(columnByte(col, page, dotX, dotY, octagonal));
    }
  }
}

void writeDotSpan(int8_t centerX, int8_t dotX, int8_t dotY,
                  bool octagonal) {
  int16_t first = centerX - kDotRadius;
  int16_t last = centerX + kDotRadius;
  if (first < 0) first = 0;
  if (last >= kGaugeWidth) last = kGaugeWidth - 1;
  writeSpan(static_cast<uint8_t>(first), static_cast<uint8_t>(last),
            dotX, dotY, octagonal);
}

void drawSignedAxis(int16_t delta, uint8_t col, uint8_t row,
                    uint8_t panelWidth) {
  const uint16_t absDelta = delta < 0 ? -delta : delta;
  const uint16_t bounded = absDelta > 128 ? 128 : absDelta;
  const uint8_t centerLeft = (panelWidth - 1) / 2;
  const uint8_t centerRight = panelWidth / 2;
  const uint8_t halfSpan = centerLeft > 0 ? centerLeft - 1 : 0;
  uint8_t magnitude = (bounded * halfSpan + 63) / 128;
  if (magnitude > halfSpan) magnitude = halfSpan;
  display.setCursor(col, row);
  for (uint8_t i = 0; i < panelWidth; ++i) {
    uint8_t column = kGaugeEmpty;
    if (i == 0 || i == panelWidth - 1 || i == centerLeft || i == centerRight ||
        (delta < 0 && i < centerLeft && i >= centerLeft - magnitude) ||
        (delta > 0 && i > centerRight && i <= centerRight + magnitude)) {
      column = kGaugeFilled;
    }
    display.ssd1306WriteRamBuf(column);
  }
}

void renderGate(DeviceEnum mode, const controller_state_t& frame,
                bool forceRedraw) {
  const bool octagonal = analogTraceUsesOctagonalGate(mode, frame.controller_type_name);
  const int16_t fullScale = fullScaleForFrame(mode, frame);
  const int8_t dotX = coordinate(kCenterX, frame.LX, fullScale);
  const int8_t dotY = coordinate(kCenterY, frame.LY, fullScale);
  const bool contextChanged = !state.initialized || state.mode != mode ||
    state.kind != GaugeKind::Gate || state.octagonal != octagonal ||
    state.fullScale != fullScale;
  if (forceRedraw || contextChanged) {
    writeSpan(0, kGaugeWidth - 1, dotX, dotY, octagonal);
  } else if (state.dotX != dotX || state.dotY != dotY) {
    writeDotSpan(state.dotX, dotX, dotY, octagonal);
    if (state.dotX != dotX) writeDotSpan(dotX, dotX, dotY, octagonal);
  } else {
    return;
  }
  state.initialized = true;
  state.mode = mode;
  state.kind = GaugeKind::Gate;
  state.octagonal = octagonal;
  state.fullScale = fullScale;
  state.dotX = dotX;
  state.dotY = dotY;
}

int16_t scaleAxis(int16_t value, int16_t fullScale) {
  if (fullScale <= 0) return 0;
  int32_t clamped = value;
  if (clamped < -fullScale) clamped = -fullScale;
  if (clamped > fullScale) clamped = fullScale;
  return static_cast<int16_t>((clamped * 128) / fullScale);
}

void renderDualAxes(DeviceEnum mode, const controller_state_t& frame,
                    bool forceRedraw) {
  uint8_t columnOffset=0;
#if !defined(ARDUINO) || defined(PRODUCT_CLASSIC2USB)
#ifdef ENABLE_INPUT_SATURN
  if (mode==RZORD_SATURN && std::strcmp(frame.controller_type_name,"Mission6")==0)
    columnOffset=1;
#endif
#endif
  const int16_t fullScale = analogTraceFullScale(frame.sticks_precision_bits);
  const int16_t axes[4] = {scaleAxis(frame.LX, fullScale),
    scaleAxis(-frame.LY, fullScale), scaleAxis(frame.RX, fullScale),
    scaleAxis(-frame.RY, fullScale)};
  const bool redrawAll = forceRedraw || !state.initialized ||
    state.mode != mode || state.kind != GaugeKind::DualAxes ||
    state.fullScale != fullScale || state.columnOffset != columnOffset;
  if (state.initialized && state.columnOffset!=columnOffset)
    display.clear(0,kGaugeWidth,kGaugeRow,kGaugeRow+3);
  static const char* const labels[4] = {"LX", "LY", "RX", "RY"};
  for (uint8_t axis = 0; axis < 4; ++axis) {
    if (!redrawAll && state.axes[axis] == axes[axis]) continue;
    constexpr uint8_t labelWidth = 12;
    if (redrawAll) {
      display.setFont(System5x7);
      display.setCursor(columnOffset, kGaugeRow + axis);
      display.print(labels[axis]);
    }
    drawSignedAxis(axes[axis], labelWidth+columnOffset, kGaugeRow + axis,
                   kGaugeWidth - labelWidth);
  }
  state.initialized = true;
  state.kind = GaugeKind::DualAxes;
  state.fullScale = fullScale;
  state.mode = mode;
  state.columnOffset = columnOffset;
  for (uint8_t axis = 0; axis < 4; ++axis) state.axes[axis] = axes[axis];
}
}  // namespace

bool homeAnalogGaugeShouldShow(DeviceEnum mode, uint8_t player,
                               bool centeredSinglePad) {
  return centeredSinglePad && player < max_devices &&
         connectedControllerCount() == 1 &&
         kindForFrame(mode, controllerFrameConst(player)) != GaugeKind::None;
}

uint8_t homeAnalogGaugePadColumnOffset() { return kPadColumnOffset; }
uint8_t homeAnalogGaugePadColumnOffset(DeviceEnum mode, const controller_state_t& frame) {
#if (!defined(ARDUINO) || defined(PRODUCT_CLASSIC2USB)) && defined(ENABLE_INPUT_SATURN)
  if (mode==RZORD_SATURN && std::strcmp(frame.controller_type_name,"Mission")==0)
    return kGaugeWidth;
#endif
  (void)mode;(void)frame;
  return kPadColumnOffset;
}

void homeAnalogGaugeRender(DeviceEnum mode, const controller_state_t& frame,
                           bool forceRedraw) {
  oled_pixel::Origin origin(oled_pixel::centerOffset(24, 54));
  switch (kindForFrame(mode, frame)) {
    case GaugeKind::Gate: renderGate(mode, frame, forceRedraw); break;
    case GaugeKind::DualAxes: renderDualAxes(mode, frame, forceRedraw); break;
    default: homeAnalogGaugeReset(); break;
  }
}

void homeAnalogGaugeReset() { state = {}; }
