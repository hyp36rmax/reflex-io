#include "controller_feedback_config.h"

#include <Arduino.h>

#include "settings_store.h"

uint8_t controller_feedback_dualsense_led_policy = DUALSENSE_LED_ALL;

namespace {
constexpr uint8_t kStoredMarker = 0x80;
constexpr uint8_t kModeMask = 0x03;
constexpr uint8_t kBrightnessShift = 2;
constexpr uint8_t kBrightnessMask = 0x0C;
constexpr uint8_t kDeviceColorShift = 4;
constexpr uint8_t kDeviceColorMask = 0x70;
constexpr uint8_t kDeviceColorCount = 8;
constexpr uint8_t kDualSenseStoredMarker = 0x80;

bool loaded = false;
ControllerLedMode mode = CONTROLLER_LED_PLAYER;
uint8_t brightness = 1;
ControllerFeedbackColor customColor{0, 64, 255};
constexpr uint8_t kDefaultDeviceColorPreset = 4;  // White.
uint8_t deviceColorPreset = kDefaultDeviceColorPreset;
uint8_t identifyPlayer = 0xFF;
uint32_t identifyUntilMs = 0;

uint8_t scaleChannel(uint8_t channel) {
  static constexpr uint8_t kScale[] = {64, 144, 255};
  return static_cast<uint8_t>((static_cast<uint16_t>(channel) *
                               kScale[brightness > 2 ? 2 : brightness]) /
                              255u);
}

ControllerFeedbackColor playerColor(uint8_t player) {
  switch (player & 0x03u) {
    case 0: return {0, 64, 255};
    case 1: return {255, 0, 0};
    case 2: return {0, 255, 32};
    default: return {255, 32, 128};
  }
}

ControllerFeedbackColor colorPreset(uint8_t preset) {
  static constexpr ControllerFeedbackColor kColors[kDeviceColorCount] = {
      {0, 64, 255}, {255, 0, 0}, {0, 255, 32}, {255, 32, 128},
      {255, 255, 255}, {255, 128, 0}, {0, 255, 255}, {128, 0, 255}};
  return kColors[preset < kDeviceColorCount
      ? preset
      : kDefaultDeviceColorPreset];
}

void ensureLoaded() {
  if (!loaded) {
    controllerFeedbackConfigLoad();
  }
}
}  // namespace

void controllerFeedbackConfigLoad() {
  loaded = true;
  PerModeSettingsRecord settings{};
  if (!readPerModeSettings(RZORD_USB, settings)) {
    mode = CONTROLLER_LED_PLAYER;
    brightness = 1;
    customColor = {0, 64, 255};
    deviceColorPreset = kDefaultDeviceColorPreset;
    controller_feedback_dualsense_led_policy = DUALSENSE_LED_ALL;
    return;
  }

  const uint8_t packed = settings.reserved_musical_buttons;
  if ((packed & kStoredMarker) == 0) {
    mode = CONTROLLER_LED_PLAYER;
    brightness = 1;
    customColor = {0, 64, 255};
    deviceColorPreset = kDefaultDeviceColorPreset;
    controller_feedback_dualsense_led_policy = DUALSENSE_LED_ALL;
    return;
  } else {
    const uint8_t storedMode = packed & kModeMask;
    mode = storedMode <= CONTROLLER_LED_CUSTOM
        ? static_cast<ControllerLedMode>(storedMode)
        : CONTROLLER_LED_PLAYER;
    brightness = (packed & kBrightnessMask) >> kBrightnessShift;
    if (brightness > 2) brightness = 2;
    deviceColorPreset = (packed & kDeviceColorMask) >> kDeviceColorShift;
    if (deviceColorPreset >= kDeviceColorCount) {
      deviceColorPreset = kDefaultDeviceColorPreset;
    }
  }
  customColor = {
      settings.reserved_mouse_analog,
      settings.reserved_mouse_sensitivity,
      settings.reserved_mouse_stick};
  if ((customColor.r | customColor.g | customColor.b) == 0) {
    customColor = {0, 64, 255};
  }

  const uint8_t storedDualSensePolicy =
      settings.reserved_dualsense_led_policy;
  controller_feedback_dualsense_led_policy =
      (storedDualSensePolicy & kDualSenseStoredMarker) != 0
          ? storedDualSensePolicy & DUALSENSE_LED_ALL
          : DUALSENSE_LED_ALL;
}

void controllerFeedbackConfigSave() {
  ensureLoaded();
  PerModeSettingsRecord settings{};
  readPerModeSettings(RZORD_USB, settings);
  settings.reserved_musical_buttons = static_cast<uint8_t>(
      kStoredMarker | (static_cast<uint8_t>(mode) & kModeMask) |
      ((brightness & 0x03u) << kBrightnessShift) |
      ((deviceColorPreset & 0x07u) << kDeviceColorShift));
  settings.reserved_mouse_analog = customColor.r;
  settings.reserved_mouse_sensitivity = customColor.g;
  settings.reserved_mouse_stick = customColor.b;
  settings.reserved_dualsense_led_policy = static_cast<uint8_t>(
      kDualSenseStoredMarker |
      (controller_feedback_dualsense_led_policy & DUALSENSE_LED_ALL));
  writePerModeSettings(RZORD_USB, settings);
}

ControllerLedMode controllerLedMode() {
  ensureLoaded();
  return mode;
}

void setControllerLedMode(ControllerLedMode value) {
  ensureLoaded();
  mode = value <= CONTROLLER_LED_CUSTOM ? value : CONTROLLER_LED_PLAYER;
}

uint8_t controllerLedBrightness() {
  ensureLoaded();
  return brightness;
}

void setControllerLedBrightness(uint8_t value) {
  ensureLoaded();
  brightness = value > 2 ? 2 : value;
}

ControllerFeedbackColor controllerLedCustomColor() {
  ensureLoaded();
  return customColor;
}

void setControllerLedCustomColor(ControllerFeedbackColor color) {
  ensureLoaded();
  customColor = color;
}

uint8_t deviceLedColorPreset() {
  ensureLoaded();
  return deviceColorPreset;
}

void setDeviceLedColorPreset(uint8_t preset) {
  ensureLoaded();
  deviceColorPreset = preset < kDeviceColorCount
      ? preset
      : kDefaultDeviceColorPreset;
}

ControllerFeedbackColor deviceLedColor() {
  ensureLoaded();
  return colorPreset(deviceColorPreset);
}

bool controllerPlayerIdentifyActive(uint8_t player) {
  if (identifyPlayer != player) return false;
  if (static_cast<int32_t>(millis() - identifyUntilMs) >= 0) {
    identifyPlayer = 0xFF;
    identifyUntilMs = 0;
    return false;
  }
  return true;
}

ControllerFeedbackColor controllerFeedbackColorForPlayer(uint8_t player) {
  ensureLoaded();
  if (controllerPlayerIdentifyActive(player)) {
    const bool on = ((millis() / 125u) & 1u) == 0;
    return on ? ControllerFeedbackColor{255, 255, 255}
              : ControllerFeedbackColor{0, 0, 0};
  }
  if (mode == CONTROLLER_LED_OFF) return {0, 0, 0};
  ControllerFeedbackColor color = mode == CONTROLLER_LED_CUSTOM
      ? customColor
      : playerColor(player);
  color.r = scaleChannel(color.r);
  color.g = scaleChannel(color.g);
  color.b = scaleChannel(color.b);
  return color;
}

uint8_t controllerFeedbackPlayerLedMask(uint8_t player) {
  static constexpr uint8_t kPlayerMasks[] = {0x04, 0x0A, 0x15, 0x1B};
  return controllerLedMode() == CONTROLLER_LED_OFF ? 0 : kPlayerMasks[player & 0x03u];
}

void identifyControllerPlayer(uint8_t player, uint16_t duration_ms) {
  identifyPlayer = player;
  identifyUntilMs = millis() + duration_ms;
}
