#include "../../product_config.h"
#include "platform_menu_input_runtime.h"
#include "../../core/device_runtime_state.h"

#include "../../menu/menu_idle_runtime.h"
#include "../../menu/menu_runtime_state.h"

namespace {

constexpr uint16_t kKioskTapWindowMs = 900;
constexpr uint8_t kKioskMenuTapCount = 2;
constexpr uint8_t kKioskResetTapCount = 2;

SerialMenuAction pendingSerialMenuAction = SerialMenuAction::None;
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
constexpr uint32_t kJvsSerialMenuMaxAgeMs = 250;
uint32_t pendingSerialMenuAtMs = 0;
#endif

struct KioskTapGate {
  uint8_t count = 0;
  uint32_t firstTapMs = 0;
};

KioskTapGate kioskMenuGate;
KioskTapGate kioskResetGate;

bool actionEquals(const char* action, const char* expected) {
  if (action == nullptr) return false;
  while (*action == ' ' || *action == '\t') ++action;
  while (*expected != '\0') {
    char c = *action++;
    if (c >= 'a' && c <= 'z') c = (char)(c - ('a' - 'A'));
    if (c != *expected++) return false;
  }
  return *action == '\0' || *action == ' ' || *action == '\t';
}

void resetKioskTapGate(KioskTapGate& gate) {
  gate.count = 0;
  gate.firstTapMs = 0;
}

ButtonEvent kioskGateTapPress(ButtonEvent event, KioskTapGate& gate,
                              uint8_t requiredTaps) {
  if (event == BTN_EVENT_NONE) {
    if (gate.count != 0 && millis() - gate.firstTapMs > kKioskTapWindowMs) {
      resetKioskTapGate(gate);
    }
    return BTN_EVENT_NONE;
  }
  if (event == BTN_EVENT_LONG || event == BTN_EVENT_LONG_RELEASE) {
    resetKioskTapGate(gate);
    return event;
  }
  if (event != BTN_EVENT_SINGLE && event != BTN_EVENT_DOUBLE) {
    resetKioskTapGate(gate);
    return event;
  }

  const uint8_t tapIncrement = event == BTN_EVENT_DOUBLE ? 2 : 1;
  const uint32_t now = millis();
  if (gate.count == 0 || now - gate.firstTapMs > kKioskTapWindowMs) {
    gate.count = tapIncrement;
    gate.firstTapMs = now;
  } else {
    gate.count += tapIncrement;
  }
  if (gate.count >= requiredTaps) {
    resetKioskTapGate(gate);
    return BTN_EVENT_SINGLE;
  }
  return BTN_EVENT_NONE;
}

}  // namespace

void applyKioskButtonGate(ButtonEvent& modeEvent, ButtonEvent& resetEvent) {
  if (menu_kiosk_mode != 0) {
    modeEvent = kioskGateTapPress(modeEvent, kioskMenuGate, kKioskMenuTapCount);
    resetEvent = kioskGateTapPress(resetEvent, kioskResetGate, kKioskResetTapCount);
    return;
  }
  resetKioskTapGate(kioskMenuGate);
  resetKioskTapGate(kioskResetGate);
}

uint32_t serialMenuActionToButtons(SerialMenuAction action) {
  switch (action) {
    case SerialMenuAction::Up: return 0x01;
    case SerialMenuAction::Down: return 0x02;
    case SerialMenuAction::Left: return 0x04;
    case SerialMenuAction::Right: return 0x08;
    case SerialMenuAction::Ok: return 0x10;
    case SerialMenuAction::Back: return 0x20;
    default: return 0;
  }
}

SerialMenuAction takeSerialMenuAction() {
  const SerialMenuAction action = pendingSerialMenuAction;
  pendingSerialMenuAction = SerialMenuAction::None;
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
  if (deviceMode == RZORD_JVS && action != SerialMenuAction::None &&
      static_cast<uint32_t>(millis() - pendingSerialMenuAtMs) > kJvsSerialMenuMaxAgeMs)
    return SerialMenuAction::None;
#endif
  return action;
}

bool queuePlatformMenuControlAction(const char* action) {
  SerialMenuAction parsed = SerialMenuAction::None;
  if (actionEquals(action, "MENU")) parsed = SerialMenuAction::Menu;
  else if (actionEquals(action, "UP")) parsed = SerialMenuAction::Up;
  else if (actionEquals(action, "DOWN")) parsed = SerialMenuAction::Down;
  else if (actionEquals(action, "LEFT")) parsed = SerialMenuAction::Left;
  else if (actionEquals(action, "RIGHT")) parsed = SerialMenuAction::Right;
  else if (actionEquals(action, "OK")) parsed = SerialMenuAction::Ok;
  else if (actionEquals(action, "BACK")) parsed = SerialMenuAction::Back;
  if (parsed == SerialMenuAction::None) return false;

  pendingSerialMenuAction = parsed;
#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
  pendingSerialMenuAtMs = millis();
#endif
  handleIdleUiActivity(true, IdleWakeSource::Serial);
  return true;
}
