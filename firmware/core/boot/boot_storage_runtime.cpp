#include "../../product_config.h"
#include "../directional_hotkeys.h"

#include "boot_storage_runtime.h"
#include "../firmware_build_stamp.h"
#include "../settings_journal.h"

#include <EEPROM.h>

#include "../../menu/menu_runtime_state.h"
#include "../../menu/menu_ui_state.h"
#include "../../menu/menu_working_state.h"
#include "../../output/auth/auth_storage.h"
#include "../../output/runtime/input_runtime_output_bridge.h"
#include "../../output/output_runtime_state.h"
#include "../../platform/buzzer.h"
#include "../../platform/boot/boot_ui_runtime.h"
#include "../../output/output_runtime_product_policy.h"
#include "../../platform/latency_trace_gpio.h"
#include "../../platform/latency_test.h"
#include "../../platform/rgb_led.h"
#include "../analog_calibration_state.h"
#include "../button_chord_remap.h"
#include "../classic_dual_merge_config.h"
#include "../controller_settings_state.h"
#include "../controller_feedback_config.h"
#include "../firmware_support.h"
#include "../hotkey_combo.h"
#include "../modern2usb_hotkey_profiles.h"
#include "../negcon_input_modes.h"
#include "../nes_device_mode.h"
#include "../rotary_settings.h"
#include "../rotary_profile.h"
#include "../settings_layout.h"
#include "../settings_registry.h"
#include "../settings_store.h"
#ifdef ENABLE_INPUT_JVS
#include "../../input/jvs/jvs_analog_config.h"
#include "../../input/jvs/jvs_panel_map.h"
#endif
#include "boot_storage_product.h"

namespace {

#if ADAPT_POLICY_PS5_DIAGNOSTIC_RUNTIME && !defined(PS5_DIAG_USE_PERSISTED_SETTINGS)
GlobalSettingsRecord buildPs5DiagGlobalSettings() {
  GlobalSettingsRecord settings{};
  for (uint8_t i = 0; i < static_cast<uint8_t>(SettingId::Count); ++i) {
    const SettingId id = (SettingId)i;
    if (!settingIsGlobal(id)) {
      continue;
    }
    writeRecordSettingValue(settings, id, defaultSettingValue(id, RZORD_NONE));
  }
  settings.persisted_input_mode = persistedInputModeId(RZORD_DUMMY);
  settings.configured_output_mode = OUTPUT_MISTER;
  settings.display_contrast = 255;
  settings.buzzer_mode = 0;
  sanitizeGlobalSettings(settings);
  return settings;
}

PerModeSettingsRecord buildPs5DiagPerModeSettings(DeviceEnum mode) {
  PerModeSettingsRecord settings{};
  for (uint8_t i = 0; i < static_cast<uint8_t>(SettingId::Count); ++i) {
    const SettingId id = (SettingId)i;
    if (!settingIsPerMode(id)) {
      continue;
    }
    writeRecordSettingValue(settings, id, defaultSettingValue(id, mode));
  }
  sanitizePerModeSettings(mode, settings);
  return settings;
}
#endif

void applyGlobalBootConfiguration(const GlobalSettingsRecord& settings) {
  savedDeviceMode =
      deviceModeFromPersistedInputModeId(settings.persisted_input_mode);
  if (savedDeviceMode > RZORD_NONE && savedDeviceMode < RZORD_LAST) {
    deviceMode = savedDeviceMode;
  } else {
    deviceMode = DEFAULT_INPUT_MODE;
    savedDeviceMode = deviceMode;
  }

  configuredOutputMode = sanitizeConfiguredOutputMode(
      (outputMode_t)settings.configured_output_mode);
  productAdjustBootConfiguration(deviceMode, savedDeviceMode, configuredOutputMode);
  outputMode_t forcedOutputMode = configuredOutputMode;
  if (productForcedRuntimeOutputMode(forcedOutputMode)) configuredOutputMode = forcedOutputMode;
#if ADAPT_POLICY_PS5_DIAGNOSTIC_RUNTIME && !defined(PS5_DIAG_USE_PERSISTED_SETTINGS)
  #if defined(PS5_DIAG_DEFAULT_OUTPUT_PS5)
  const bool ps5BootArmed = true;
  #else
  const bool ps5BootArmed = ps5_diag_boot_arm_is_present();
  #endif
  ps5_diag_clear_boot_arm();
  configuredOutputMode = ps5BootArmed ? OUTPUT_PS5 : OUTPUT_MISTER;
  #ifdef ENABLE_INPUT_DUMMY
  deviceMode = ps5BootArmed ? RZORD_JVS : RZORD_DUMMY;
  savedDeviceMode = deviceMode;
  #endif
#endif
  outputMode = sanitizeRuntimeOutputMode(configuredOutputMode);
  productInitializeOutputTransport(
      static_cast<uint8_t>(readRecordSettingValue(
          settings, SettingId::ProductOutputTransport)),
      static_cast<uint8_t>(readRecordSettingValue(
          settings, SettingId::ProductWirelessOutputPersona)));
}

void initializeCoreRuntimeSettingsFromStorage(const GlobalSettingsRecord& globalSettings,
                                              const PerModeSettingsRecord& perModeSettings) {
  display_contrast = globalSettings.display_contrast;
  snes_rumbletech_enabled = 1;
  stick_invert = perModeSettings.stick_invert;
  rumble_level = perModeSettings.rumble_level;
  trigger_mode = perModeSettings.trigger_mode;
  spinner_speed = perModeSettings.spinner_speed;
  button_map_mode = perModeSettings.button_map_mode;
#if !ADAPT_POLICY_RESERVED_FEEDBACK_STORAGE
  gamecube_l_switch_mode = perModeSettings.gamecube_l_switch_mode;
#endif
  n64_z_mode = perModeSettings.n64_z_mode;
  n64_cstick_mode = perModeSettings.n64_cstick_mode;
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  wii_analog_range = rotaryAnalogRange(perModeSettings.wii_analog_range);
  paddle_divisor = rotaryDivisor(perModeSettings.wii_analog_range);
  rotary_reverse = rotaryReverse(perModeSettings.wii_analog_range);
  driving_wrap = rotaryWrap(perModeSettings.wii_analog_range);
  rotary_profile = rotaryProfileFromJogconDigitalMap(perModeSettings.jogcon_digital_map);
  jogcon_rotary_profile = rotaryProfileFromWheelSensitivity(perModeSettings.wheel_sensitivity);
#else
  wii_analog_range = perModeSettings.wii_analog_range;
  paddle_divisor = 1;
  rotary_reverse = false;
  driving_wrap = false;
  rotary_profile = 0;
  jogcon_rotary_profile = 0;
#endif
  n64_analog_range = perModeSettings.n64_analog_range;
  negcon_digital_inputs = negconDigitalInputsFromRaw(perModeSettings.psx_negcon_inputs);
  negcon_gain = negconGainFromRaw(perModeSettings.psx_negcon_inputs);
  menu_latency_test = globalSettings.latency_test;
  menu_latency_controller_in_loop = globalSettings.latency_controller_in_loop;
  menu_latency_host_type = globalSettings.latency_host_type;

  HotkeyBindingSet hotkeys{};
#if ADAPT_POLICY_PS5_DIAGNOSTIC_RUNTIME && !defined(PS5_DIAG_USE_PERSISTED_SETTINGS)
  setDefaultHotkeyBindings(hotkeys);
#else
  readHotkeyBindings(hotkeys);
#endif
  menu_hotkey_combo = hotkeys.menu;
  menu_system_hotkey_combo = hotkeys.system_menu;
  menu_home_hotkey_combo = hotkeys.home;
  menu_capture_hotkey_combo = hotkeys.capture;
  menu_menu_hotkey = (uint8_t)loadSettingValue(SettingId::MenuHotkey, RZORD_NONE);
  menu_system_menu_hotkey = (uint8_t)loadSettingValue(SettingId::SystemMenuHotkey, RZORD_NONE);
  menu_home_hotkey = (uint8_t)loadSettingValue(SettingId::HomeHotkey, RZORD_NONE);
  menu_capture_hotkey = (uint8_t)loadSettingValue(SettingId::CaptureHotkey, RZORD_NONE);
  menu_kiosk_mode = (uint8_t)loadSettingValue(SettingId::KioskMode, RZORD_NONE);
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
  menu_usb2db15_multi_bind = (uint8_t)loadSettingValue(SettingId::Usb2Db15MultiBind, RZORD_NONE);
  menu_usb2db15_game_selection = (uint8_t)loadSettingValue(SettingId::Usb2Db15GameSelection, RZORD_NONE);
#endif
  loadButtonChordRemaps();
}

void initializeBuzzerAndLatencySettingsFromStorage(const GlobalSettingsRecord& globalSettings) {
  const uint8_t buzzerMode = globalSettings.buzzer_mode;
  buzzer_enabled = buzzerMode > 0;
  const uint8_t buzzerVolume = (buzzerMode == 1) ? 10 : 100;
  buzzer.begin();
  buzzer.setEnabled(buzzer_enabled);
  buzzer.setVolume(buzzerVolume);
  buzzer.setEventMask(globalSettings.sound_events);

  latencyTest.begin();
  latencyTraceGpioBegin();
  latencyTest.setEnabled(globalSettings.latency_test != 0);
  latencyTest.setControllerInLoop(globalSettings.latency_controller_in_loop != 0);
  latencyTest.setHostType(globalSettings.latency_host_type);
}

void initializeLedAndScreensaverSettingsFromStorage(const GlobalSettingsRecord& globalSettings) {
  led_mode = globalSettings.led_mode;
  led_brightness = globalSettings.led_brightness;
  #ifdef USE_WS2812
  rgbLed.begin();
  #if ADAPT_POLICY_RESERVED_FEEDBACK_STORAGE
  controllerFeedbackConfigLoad();
  const ControllerFeedbackColor deviceColor = deviceLedColor();
  rgbLed.setBaseColor({deviceColor.r, deviceColor.g, deviceColor.b});
  #endif
  rgbLed.setMode((led_mode_t)led_mode);
  rgbLed.setBrightness(led_brightness);
  #endif

  screensaver_mode =
    (uint8_t)readRecordSettingValue(globalSettings, SettingId::ScreensaverMode);
  screensaver_animation = globalSettings.screensaver_animation;
  menu_screensaver = screensaver_mode;
  menu_screensaver_anim = screensaver_animation;
  oled_inactive_mode =
    (uint8_t)readRecordSettingValue(globalSettings, SettingId::OledInactive);
  menu_oled_inactive = oled_inactive_mode;
  oled_wake_mode =
    (uint8_t)readRecordSettingValue(globalSettings, SettingId::OledWake);
  menu_oled_wake = oled_wake_mode;
  menu_ok_back = settingSupportedOnProduct(SettingId::MenuOkBack)
    ? (uint8_t)readRecordSettingValue(globalSettings, SettingId::MenuOkBack) : 0;
  menu_menu_ok_back = menu_ok_back;
}

void initializeMenuRuntimeSettingsFromStorage(GlobalSettingsRecord& globalSettings,
                                              const PerModeSettingsRecord& perModeSettings) {
  menu_powerpad_mode = sanitizeNesDeviceMode(perModeSettings.powerpad_mode);
#if !ADAPT_POLICY_RESERVED_FEEDBACK_STORAGE
  menu_jogcon_mode = perModeSettings.jogcon_mode;
#endif
  menu_jogcon_force = perModeSettings.jogcon_force;
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  menu_wheel_sensitivity = rotaryWheelSensitivity(perModeSettings.wheel_sensitivity);
  menu_jogcon_digital_map = rotaryLegacyLowBits(perModeSettings.jogcon_digital_map);
#else
  menu_wheel_sensitivity = perModeSettings.wheel_sensitivity;
  menu_jogcon_digital_map = perModeSettings.jogcon_digital_map;
#endif
  menu_jogcon_wheel_axis = perModeSettings.jogcon_wheel_axis;
#if !ADAPT_POLICY_RESERVED_FEEDBACK_STORAGE
  menu_gamecube_l_switch_mode = perModeSettings.gamecube_l_switch_mode;
#endif
  menu_win_output = static_cast<uint8_t>(
      readRecordSettingValue(globalSettings, SettingId::WinOutput));
  menu_mister_output = static_cast<uint8_t>(
      readRecordSettingValue(globalSettings, SettingId::MisterOutput));
  menu_switch2_output = settingSupportedOnProduct(SettingId::Switch2Output)
    ? static_cast<uint8_t>(readRecordSettingValue(globalSettings, SettingId::Switch2Output))
    : SWITCH2_OUTPUT_PREFERENCE_SWITCHPRO;
  menu_ps2_remote_output = settingSupportedOnProduct(SettingId::Ps2RemoteOutput)
    ? static_cast<uint8_t>(readRecordSettingValue(globalSettings, SettingId::Ps2RemoteOutput))
    : 0;
  mister_display_mode = settingSupportedOnProduct(SettingId::MisterDisplay)
    ? static_cast<uint8_t>(readRecordSettingValue(globalSettings, SettingId::MisterDisplay))
    : MISTER_DISPLAY_ARTWORK;
  menu_mister_display_mode = mister_display_mode;
#if defined(ADAPT_FEATURE_USB_DRIVE)
  menu_management_msc = static_cast<uint8_t>(
      readRecordSettingValue(globalSettings, SettingId::ManagementMsc));
#endif
  menu_touchpad_mouse = static_cast<uint8_t>(
      readRecordSettingValue(globalSettings, SettingId::TouchpadMouse));
  menu_psx_periph = static_cast<uint8_t>(
      readRecordSettingValue(globalSettings, SettingId::PsxPeriph));
  menu_snes_rumbletech = 1;
  menu_home_screen_debug = 0;
  menu_home_button_labels = 0;
  menu_home_jvs_view = (globalSettings.home_screen_debug & HOME_JVS_VIEW_MASK) >> HOME_JVS_VIEW_SHIFT;
  menu_hotkey_hold_time = (globalSettings.home_screen_debug & HOTKEY_HOLD_TIME_MASK) >> HOTKEY_HOLD_TIME_SHIFT;
#if defined(ADAPT_CAP_DUAL_INPUT_MERGE)
  menu_classic_dual_merge = classicDualMergeEnabledForMode(deviceMode) ? 1 : 0;
  classic_dual_merge_enabled = menu_classic_dual_merge;
#else
  menu_classic_dual_merge = 0;
  classic_dual_merge_enabled = 0;
  classic_dual_merge_p2_mask = 0;
#endif
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  menu_wii_analog_range = rotaryAnalogRange(perModeSettings.wii_analog_range);
#else
  menu_wii_analog_range = perModeSettings.wii_analog_range;
#endif
  menu_guncon_offset_x = perModeSettings.guncon_offset_x;
  menu_guncon_offset_y = perModeSettings.guncon_offset_y;
}

void initializeSoundAndUsbIdentitySettingsFromStorage() {
  ps4_type = PS4_DEVICE_TYPE_GAMEPAD;
  bcd_device_version.revision = bcd_input_revision;
}

uint16_t storageEndBeforeAppendedModes() {
  uint16_t storageEnd = max((uint16_t)SETTINGS_STORAGE_REGION_SIZE, hotkeyStorageRequiredEnd());
  storageEnd = max(storageEnd, buttonChordStorageOccupiedEnd());
#if defined(ADAPT_CAP_CONTROLLER_HOTKEY_PROFILES)
  if (modern2usbHotkeyProfilesStorageLayoutValid()) {
    storageEnd = max(storageEnd, modern2usbHotkeyProfilesStorageRequiredEnd());
  }
#endif
#if defined(ADAPT_CAP_DUAL_INPUT_MERGE)
  storageEnd = max(storageEnd, classicDualMergeStorageRequiredEnd());
#endif
  #ifdef ADAPT_OUTPUT_USB_DEVICE
  storageEnd = max(storageEnd, authStorageRequiredEnd());
  #endif
  return productStorageRequiredEnd(storageEnd);
}

}  // namespace

bool persistentStorageLayoutFits = true;

uint16_t persistentStorageOccupiedEnd() {
  return storageEndBeforeAppendedModes() + SETTINGS_APPENDED_MODE_REGION_SIZE;
}

uint16_t settingsAppendedModeRecordBase() {
  return storageEndBeforeAppendedModes();
}

void beginPersistentStorage() {
#if ADAPT_POLICY_PS5_DIAGNOSTIC_RUNTIME && !defined(PS5_DIAG_USE_PERSISTED_SETTINGS)
  showPs5DiagBootMarker("1a eeprom");
  EEPROM.begin(SETTINGS_STORAGE_REGION_SIZE);
  showPs5DiagBootMarker("1b eeprom");
  return;
#else
  settingsJournalRecover();
  const uint16_t occupiedStorageEnd =
      storageEndBeforeAppendedModes() + SETTINGS_APPENDED_MODE_REGION_SIZE;
  // Records past the 4 KiB EEPROM sector silently never persist.
  persistentStorageLayoutFits = eepromRangeFits(0, occupiedStorageEnd);
  // The firmware build stamp follows the last chained store.
  const uint16_t mappedStorageEnd =
      max(occupiedStorageEnd, firmwareBuildStampStorageRequiredEnd());
#if defined(ADAPT_FEATURE_FIRST_BOOT_SETUP)
  EEPROM.begin(max(mappedStorageEnd,
                   menuModeVisibilityStorageRequiredEnd()));
  initializeMenuModeVisibilityFromStorage(occupiedStorageEnd);
#else
  EEPROM.begin(mappedStorageEnd);
#endif
  productPreparePersistentStorage();

  GlobalSettingsRecord globalSettings{};
  if (!readGlobalSettings(globalSettings)) {
    // Without the journal, power lost during a save can blank the settings
    // sector. Remaps have their own flash sector, which that save did not
    // touch.
    factoryResetSettings(false);
  }
  migrateLegacyPerModeSettings();

  authStorageInitialize();
  updateAuthKeyStatus();
#endif
}

bool loadPersistedBootConfiguration() {
#if ADAPT_POLICY_PS5_DIAGNOSTIC_RUNTIME && !defined(PS5_DIAG_USE_PERSISTED_SETTINGS)
  GlobalSettingsRecord globalSettings = buildPs5DiagGlobalSettings();
  applyGlobalBootConfiguration(globalSettings);
  return false;
#else
  GlobalSettingsRecord globalSettings{};
  const bool repaired = !readGlobalSettings(globalSettings);
  if (repaired) {
    writeGlobalSettings(globalSettings);
  }

  applyGlobalBootConfiguration(globalSettings);
  return repaired;
#endif
}

void initializeRuntimeSettingsFromStorage() {
#if ADAPT_POLICY_PS5_DIAGNOSTIC_RUNTIME && !defined(PS5_DIAG_USE_PERSISTED_SETTINGS)
  GlobalSettingsRecord globalSettings = buildPs5DiagGlobalSettings();
  PerModeSettingsRecord perModeSettings = buildPs5DiagPerModeSettings(deviceMode);
#else
  GlobalSettingsRecord globalSettings{};
  readGlobalSettings(globalSettings);

  PerModeSettingsRecord perModeSettings{};
  readPerModeSettings(deviceMode, perModeSettings);
#endif

  initializeCoreRuntimeSettingsFromStorage(globalSettings, perModeSettings);
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB) || defined(ADAPT_BRIDGE_JVS_TO_USB)
  direction_hotkey::read(perModeSettings, active_direction_hotkeys);
#endif
  initializeBuzzerAndLatencySettingsFromStorage(globalSettings);
  initializeLedAndScreensaverSettingsFromStorage(globalSettings);
  initializeMenuRuntimeSettingsFromStorage(globalSettings, perModeSettings);
  initializeSoundAndUsbIdentitySettingsFromStorage();
#if defined(ENABLE_INPUT_JVS) && \
    !(ADAPT_POLICY_PS5_DIAGNOSTIC_RUNTIME && !defined(PS5_DIAG_USE_PERSISTED_SETTINGS))
  jvsAnalogLoadConfig();
  jvsPanelLoadMap();
#endif
}
