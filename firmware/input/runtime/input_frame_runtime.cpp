#include "input_frame_runtime.h"

#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <cstring>

#include "../../core/controller_delivery_state.h"
#include "../../core/classic_mouse_source.h"
#include "../state/input_poll_runtime_state.h"

bool anyInputFrameConnected(uint8_t port_count) {
  for (uint8_t i = 0; i < port_count; ++i) {
    if (inputFrameConst(i).connected) {
      return true;
    }
  }
  return false;
}

uint8_t connectedInputFrameCount(uint8_t port_count) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < port_count; ++i) {
    if (inputFrameConst(i).connected) {
      ++count;
    }
  }
  return count;
}

bool inputPollUpdated() {
  return inputPollCycleUpdated();
}

void beginInputPollCycle() {
  resetInputPollCycleUpdated();
}

void markInputPollUpdated() {
  markInputPollCycleUpdated();
}

void clearInputFrameButtons(uint8_t index) {
  inputFrame(index).digital_buttons = 0;
  inputFrame(index).extended_buttons = 0;
  inputFrame(index).extended_button_count = 0;
}

namespace {

bool inputFrameHasActiveState(const controller_state_t& frame) {
  return frame.digital_buttons != 0 || frame.extended_buttons != 0 || frame.analog_sticks != 0 ||
         frame.analog_buttons != 0 || frame.analog_pad != 0 ||
         frame.paddle != 128 || frame.spinner != 0 || frame.mouse_x != 0 ||
         frame.mouse_y != 0 || frame.mouse_wheel_x != 0 ||
         frame.mouse_wheel_y != 0 || frame.touch[0].active != 0 ||
         frame.touch[1].active != 0;
}

// Returns the canonical frame slot for a reference into controller_frames, or
// -1 for a caller-owned scratch frame.
int canonicalInputFrameIndex(const controller_state_t& frame) {
  const uintptr_t begin = reinterpret_cast<uintptr_t>(&controllerFrameConst(0));
  const uintptr_t address = reinterpret_cast<uintptr_t>(&frame);
  if (address < begin) {
    return -1;
  }
  const uintptr_t offset = address - begin;
  if (offset % sizeof(controller_state_t) != 0) {
    return -1;
  }
  const uintptr_t index = offset / sizeof(controller_state_t);
  return index < MAX_USB_OUT ? static_cast<int>(index) : -1;
}

}  // namespace

void neutralizeInputFrameState(controller_state_t& frame) {
  frame.digital_buttons = 0;
  frame.extended_buttons = 0;
  frame.analog_sticks = 0;
  frame.analog_buttons = 0;
  frame.analog_pad = 0;
  frame.paddle = 128;
  frame.spinner = 0;
  frame.mouse_x = 0;
  frame.mouse_y = 0;
  frame.mouse_wheel_x = 0;
  frame.mouse_wheel_y = 0;
  memset(frame.touch, 0, sizeof(frame.touch));
}

void setInputFrameConnected(uint8_t index, bool connected) {
  setInputFrameConnected(inputFrame(index), connected);
}

void setInputFrameConnected(controller_state_t& frame, bool connected) {
  if (!connected) {
    // A removed controller must not leave its last buttons/axes held on
    // outputs that do not consult .connected. Request one delivery so the
    // released state reaches the host even if the driver does not.
    const bool hadState = frame.connected || inputFrameHasActiveState(frame);
    const int index = canonicalInputFrameIndex(frame);
    if (index >= 0) setClassicMouseInput(static_cast<uint8_t>(index), false);
    neutralizeInputFrameState(frame);
    frame.extended_button_count = 0;
    if (hadState) {
      if (index >= 0) {
        markInputFrameUpdated(static_cast<uint8_t>(index));
      }
    }
  }
  frame.connected = connected;
}

void clearInputFrameTypeName(uint8_t index) {
  inputFrame(index).controller_type_name[0] = '\0';
}

void clearInputFrameTypeName(controller_state_t& frame) {
  frame.controller_type_name[0] = '\0';
}

void setInputFrameTypeName(controller_state_t& frame, const char* name) {
  if (!name || !name[0]) {
    frame.controller_type_name[0] = '\0';
    return;
  }
  strncpy(frame.controller_type_name, name,
          sizeof(frame.controller_type_name) - 1);
  frame.controller_type_name[sizeof(frame.controller_type_name) - 1] = '\0';
}

void setInputFrameTypeName(uint8_t index, const char* name) {
  setInputFrameTypeName(inputFrame(index), name);
}

void formatInputFrameTypeName(controller_state_t& frame,
                              const char* format, ...) {
  if (!format || !format[0]) {
    clearInputFrameTypeName(frame);
    return;
  }

  va_list args;
  va_start(args, format);
  vsnprintf(frame.controller_type_name, sizeof(frame.controller_type_name),
            format, args);
  va_end(args);
}

void formatInputFrameTypeName(uint8_t index, const char* format, ...) {
  controller_state_t& frame = inputFrame(index);
  if (!format || !format[0]) {
    clearInputFrameTypeName(frame);
    return;
  }

  va_list args;
  va_start(args, format);
  vsnprintf(frame.controller_type_name, sizeof(frame.controller_type_name),
            format, args);
  va_end(args);
}

void markInputFrameUpdated(uint8_t index) {
  requestControllerFrameDelivery(index);
  markInputPollUpdated();
}
