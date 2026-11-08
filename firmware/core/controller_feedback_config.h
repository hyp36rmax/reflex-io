#pragma once

#include <stdint.h>

enum ControllerLedMode : uint8_t {
  CONTROLLER_LED_OFF = 0,
  CONTROLLER_LED_PLAYER = 1,
  CONTROLLER_LED_CUSTOM = 2,
};

struct ControllerFeedbackColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

void controllerFeedbackConfigLoad();
void controllerFeedbackConfigSave();
ControllerLedMode controllerLedMode();
void setControllerLedMode(ControllerLedMode mode);
uint8_t controllerLedBrightness();  // 0=low, 1=medium, 2=high
void setControllerLedBrightness(uint8_t brightness);
ControllerFeedbackColor controllerLedCustomColor();
void setControllerLedCustomColor(ControllerFeedbackColor color);
uint8_t deviceLedColorPreset();
void setDeviceLedColorPreset(uint8_t preset);
ControllerFeedbackColor deviceLedColor();
ControllerFeedbackColor controllerFeedbackColorForPlayer(uint8_t player);
uint8_t controllerFeedbackPlayerLedMask(uint8_t player);
enum DualSenseLedPolicy : uint8_t {
  DUALSENSE_LED_PLAYER = 0x01,
  DUALSENSE_LED_LIGHTBAR = 0x02,
  DUALSENSE_LED_ALL = DUALSENSE_LED_PLAYER | DUALSENSE_LED_LIGHTBAR,
};
extern uint8_t controller_feedback_dualsense_led_policy;
inline uint8_t controllerFeedbackDualSenseLedPolicy() {
  return controller_feedback_dualsense_led_policy;
}
inline void setControllerFeedbackDualSenseLedPolicy(uint8_t policy) {
  controller_feedback_dualsense_led_policy = policy & DUALSENSE_LED_ALL;
}
void identifyControllerPlayer(uint8_t player, uint16_t duration_ms = 2000);
bool controllerPlayerIdentifyActive(uint8_t player);
