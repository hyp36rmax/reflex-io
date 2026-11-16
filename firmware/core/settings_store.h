#pragma once

#include <stdint.h>
#include <stddef.h>

#include "device_mode.h"
#include "persisted_input_mode.h"
#include "pointer_mouse_config.h"
#include "setup_wizard_settings.h"
#include "../output/output_mode.h"
#include "turbo.h"

constexpr uint8_t PERSISTED_AB_SLOT_COUNT = 2;
constexpr uint8_t SETTINGS_SCHEMA_VERSION = 1;

constexpr uint16_t PERSISTED_BLOCK_MAGIC_GLOBAL = 0x4753;   // GS
constexpr uint16_t PERSISTED_BLOCK_MAGIC_PER_MODE = 0x504D; // PM
constexpr uint16_t PERSISTED_BLOCK_MAGIC_MENU_MODE_VISIBILITY = 0x4D56; // MV

constexpr uint8_t PER_MODE_TURBO_SIZE = TURBO_BTN_COUNT;
constexpr uint8_t PER_MODE_REMAP_SIZE = 19;
constexpr uint8_t MAX_INPUT_MODES = (uint8_t)RZORD_LAST;
// Slot count of the layouts persisted per mode, indexed by persisted input-mode
// ID: every mode except those appended after 2026-10-10 (device_mode.h).
constexpr uint8_t PERSISTED_LAYOUT_INPUT_MODES = persistedInputModeLayoutSlots();
static_assert(PERSISTED_LAYOUT_INPUT_MODES ==
                  MAX_INPUT_MODES - RZORD_APPENDED_MODE_COUNT,
              "per-mode layout slots no longer match the enabled modes");
constexpr uint8_t HOME_SCREEN_DEBUG_MASK = 0x01;
constexpr uint8_t HOME_BUTTON_LABELS_MASK = 0x02;
constexpr uint8_t HOME_JVS_VIEW_SHIFT = 2;
constexpr uint8_t HOME_JVS_VIEW_MASK = 0x0C;
constexpr uint8_t HOTKEY_HOLD_TIME_SHIFT = 4;
constexpr uint8_t HOTKEY_HOLD_TIME_MASK = 0x70;
constexpr uint8_t CLASSIC_DUAL_MERGE_MASK = 0x80;
constexpr uint8_t MENU_HOTKEY_QUICK_MASK = 0x01;
constexpr uint8_t MENU_HOTKEY_SYSTEM_MASK = 0x02;
constexpr uint8_t MENU_HOTKEY_KIOSK_MASK = 0x04;
constexpr uint8_t USB2DB15_MULTI_BIND_OFF_MASK = 0x08;
// Retired two-bit field. Only 0x10 means On; legacy zero and erased/invalid
// 0xFF records both decode to Off before the sanitizer rewrites them.
constexpr uint8_t USB2DB15_GAME_SELECTION_MASK = 0x30;
constexpr uint8_t USB2DB15_GAME_SELECTION_ON_VALUE = 0x10;
constexpr uint8_t MISTER_DISPLAY_INPUTS_MASK = 0x40;
static_assert((MISTER_DISPLAY_INPUTS_MASK & (MENU_HOTKEY_QUICK_MASK |
    MENU_HOTKEY_SYSTEM_MASK | MENU_HOTKEY_KIOSK_MASK |
    USB2DB15_MULTI_BIND_OFF_MASK | USB2DB15_GAME_SELECTION_MASK)) == 0,
    "MiSTer display mode must not overlap existing menu-hotkey fields");
constexpr uint8_t MENU_HOTKEY_OPTION_MASK = MENU_HOTKEY_KIOSK_MASK |
    USB2DB15_MULTI_BIND_OFF_MASK | USB2DB15_GAME_SELECTION_MASK |
    MISTER_DISPLAY_INPUTS_MASK;
constexpr uint8_t MENU_HOTKEY_FORMAT_MASK = 0x80;
// home_hotkey keeps Down+Start On/Off (0/1) in its low bits. The top bit turns
// Hold Start off, so records written before Hold Start existed read it as On.
constexpr uint8_t HOME_HOTKEY_VALUE_MASK = 0x7F;
constexpr uint8_t HOLD_START_HOTKEY_OFF_MASK = 0x80;
constexpr uint8_t WIN_OUTPUT_VALUE_MASK = 0x07;
constexpr uint8_t WIN_OUTPUT_TRANSPORT_SHIFT = 3;
constexpr uint8_t WIN_OUTPUT_TRANSPORT_MASK = 0x18;
constexpr uint8_t WIN_OUTPUT_BT_PERSONA_SHIFT = 5;
constexpr uint8_t WIN_OUTPUT_BT_PERSONA_MASK = 0xE0;
// screensaver_mode keeps its 0-6 value in the low bits. The top bit blanks
// the panel on idle instead, and bit 6 lets only the Adapt's own buttons and
// menus wake it (controller input never does). Records written before either
// existed read as Screensaver / Any input and the global record keeps its size.
constexpr uint8_t SCREENSAVER_MODE_VALUE_MASK = 0x3F;
constexpr uint8_t OLED_WAKE_ADAPT_ONLY_MASK = 0x40;
constexpr uint8_t OLED_INACTIVE_OFF_MASK = 0x80;
// The low value is 0 = never, 1-3 = Dim after 1/5/10 minutes and 4-6 = an
// animation after 1/5/10 minutes. Menus and config tools show it as an
// Inactive timer (Off/1m/5m/10m) plus a Dim-or-animation Screensaver choice.
constexpr uint8_t OLED_INACTIVE_TIMER_COUNT = 4;
constexpr uint8_t screensaverModeTimer(uint8_t mode) {
  return (mode == 0 || mode > 6) ? 0 : (uint8_t)((mode - 1) % 3 + 1);
}
constexpr bool screensaverModeDims(uint8_t mode) {
  return mode >= 1 && mode <= 3;
}
constexpr uint8_t encodeScreensaverMode(uint8_t timer, bool dim) {
  return (timer == 0 || timer >= OLED_INACTIVE_TIMER_COUNT)
      ? 0 : (uint8_t)(dim ? timer : timer + 3);
}

constexpr uint8_t HOST_OUTPUT_PREFERENCE_DINPUT = 0;
constexpr uint8_t HOST_OUTPUT_PREFERENCE_XINPUT = 1;
constexpr uint8_t HOST_OUTPUT_PREFERENCE_KEYBOARD = 2;
constexpr uint8_t HOST_OUTPUT_PREFERENCE_XBOXONE = 3;
constexpr uint8_t HOST_OUTPUT_PREFERENCE_PS3 = 4;
constexpr uint8_t HOST_OUTPUT_PREFERENCE_PS4 = 5;
constexpr uint8_t HOST_OUTPUT_PREFERENCE_PS5 = 6;
constexpr uint8_t HOST_OUTPUT_PREFERENCE_SWITCHPRO = 7;
constexpr uint8_t HOST_OUTPUT_PREFERENCE_MAX = HOST_OUTPUT_PREFERENCE_SWITCHPRO;
constexpr uint8_t MISTER_OUTPUT_PREFERENCE_MASK = 0x07;
constexpr uint8_t HOST_OUTPUT_PREFERENCE_V2_MASK = 0x80;
// USB Drive bit in psx_periph (Classic2USB's drive, reserved elsewhere).
// Since 2026-10-06 a set bit means Off, so a record that never stored a
// choice reads On; earlier builds stored set = On, so their On reads Off.
constexpr uint8_t MANAGEMENT_MSC_OFF_MASK = 0x08;
static_assert((MANAGEMENT_MSC_OFF_MASK &
    (MISTER_OUTPUT_PREFERENCE_MASK | HOST_OUTPUT_PREFERENCE_V2_MASK)) == 0,
    "USB drive preference must not overlap host output selection");

inline constexpr uint8_t misterOutputPreferenceFromRaw(uint8_t raw) { return raw & MISTER_OUTPUT_PREFERENCE_MASK; }
inline constexpr uint8_t encodeMisterOutputPreference(uint8_t raw, uint8_t value) { return (raw & static_cast<uint8_t>(~MISTER_OUTPUT_PREFERENCE_MASK)) | (value & MISTER_OUTPUT_PREFERENCE_MASK); }

// PS2 remote output preference in psx_periph bit 6.  A clear bit is the
// compatibility default: existing records continue to present a gamepad.
constexpr uint8_t PS2_REMOTE_OUTPUT_PREFERENCE_MASK = 0x40;
inline constexpr bool ps2RemoteOutputEnabledFromRaw(uint8_t raw) {
  return (raw & PS2_REMOTE_OUTPUT_PREFERENCE_MASK) != 0;
}
inline constexpr uint8_t encodePs2RemoteOutputPreference(uint8_t raw, bool enabled) {
  return static_cast<uint8_t>((raw & static_cast<uint8_t>(~PS2_REMOTE_OUTPUT_PREFERENCE_MASK)) |
      (enabled ? PS2_REMOTE_OUTPUT_PREFERENCE_MASK : 0));
}

// Switch 2 Mode in psx_periph bits 4-5: what Auto presents to a Switch 2.
// Zero (every existing record) is Switch Pro; unknown values read as it too.
constexpr uint8_t SWITCH2_OUTPUT_PREFERENCE_SHIFT = 4;
constexpr uint8_t SWITCH2_OUTPUT_PREFERENCE_MASK = 0x30;
constexpr uint8_t SWITCH2_OUTPUT_PREFERENCE_SWITCHPRO = 0;
constexpr uint8_t SWITCH2_OUTPUT_PREFERENCE_SWITCH2PRO = 1;
constexpr uint8_t SWITCH2_OUTPUT_PREFERENCE_MAX = SWITCH2_OUTPUT_PREFERENCE_SWITCH2PRO;
static_assert((SWITCH2_OUTPUT_PREFERENCE_MASK & (MISTER_OUTPUT_PREFERENCE_MASK |
    HOST_OUTPUT_PREFERENCE_V2_MASK | MANAGEMENT_MSC_OFF_MASK |
    PS2_REMOTE_OUTPUT_PREFERENCE_MASK)) == 0,
    "Switch 2 Mode must not overlap the other psx_periph fields");
inline constexpr uint8_t switch2OutputPreferenceFromRaw(uint8_t raw) {
  return ((raw & SWITCH2_OUTPUT_PREFERENCE_MASK) >> SWITCH2_OUTPUT_PREFERENCE_SHIFT) <=
      SWITCH2_OUTPUT_PREFERENCE_MAX
    ? static_cast<uint8_t>((raw & SWITCH2_OUTPUT_PREFERENCE_MASK) >> SWITCH2_OUTPUT_PREFERENCE_SHIFT)
    : SWITCH2_OUTPUT_PREFERENCE_SWITCHPRO;
}
inline constexpr uint8_t encodeSwitch2OutputPreference(uint8_t raw, uint8_t value) {
  return static_cast<uint8_t>((raw & static_cast<uint8_t>(~SWITCH2_OUTPUT_PREFERENCE_MASK)) |
      ((value << SWITCH2_OUTPUT_PREFERENCE_SHIFT) & SWITCH2_OUTPUT_PREFERENCE_MASK));
}
static_assert(switch2OutputPreferenceFromRaw(0) == SWITCH2_OUTPUT_PREFERENCE_SWITCHPRO &&
              switch2OutputPreferenceFromRaw(0xFF) == SWITCH2_OUTPUT_PREFERENCE_SWITCHPRO &&
              encodeSwitch2OutputPreference(0x8B, 1) == 0x9B,
              "Switch 2 Mode defaults to Switch Pro and preserves its neighbours");

static_assert(misterOutputPreferenceFromRaw(0) == HOST_OUTPUT_PREFERENCE_DINPUT, "Legacy MiSTer/Linux setting must default to DInput");
static_assert(encodeMisterOutputPreference(0xA0, 1) == 0xA1, "MiSTer/Linux preference must preserve unrelated settings");
static_assert(!ps2RemoteOutputEnabledFromRaw(0) &&
              ps2RemoteOutputEnabledFromRaw(PS2_REMOTE_OUTPUT_PREFERENCE_MASK) &&
              encodePs2RemoteOutputPreference(0x9B, true) == 0xDB &&
              encodePs2RemoteOutputPreference(0xDB, false) == 0x9B,
              "PS2 remote output must default to Gamepad and preserve psx_periph neighbours");

constexpr int8_t GUNCON_ALIGNMENT_MIN = -50;
constexpr int8_t GUNCON_ALIGNMENT_MAX = 50;

inline constexpr int8_t normalizeGunconAlignmentOffset(int32_t value) {
  return value < GUNCON_ALIGNMENT_MIN
    ? GUNCON_ALIGNMENT_MIN
    : (value > GUNCON_ALIGNMENT_MAX ? GUNCON_ALIGNMENT_MAX : (int8_t)value);
}

inline constexpr bool menuHotkeyQuickEnabledFromRaw(uint8_t raw) {
  return (raw & MENU_HOTKEY_QUICK_MASK) != 0;
}

inline constexpr bool menuHotkeySystemEnabledFromRaw(uint8_t raw) {
  if ((raw & MENU_HOTKEY_FORMAT_MASK) == 0) {
    return raw == MENU_HOTKEY_QUICK_MASK || (raw & MENU_HOTKEY_SYSTEM_MASK) != 0;
  }
  return (raw & MENU_HOTKEY_SYSTEM_MASK) != 0;
}

inline constexpr bool menuKioskEnabledFromRaw(uint8_t raw) {
  return (raw & MENU_HOTKEY_KIOSK_MASK) != 0;
}

inline constexpr uint8_t misterDisplayModeFromRaw(uint8_t raw) {
  return (raw & MISTER_DISPLAY_INPUTS_MASK) ? 1 : 0;
}

inline constexpr uint8_t encodeMenuHotkeyFlags(bool quick, bool system, uint8_t preserve = 0) {
  return MENU_HOTKEY_FORMAT_MASK |
         (preserve & MENU_HOTKEY_OPTION_MASK) |
         (quick ? MENU_HOTKEY_QUICK_MASK : 0) |
         (system ? MENU_HOTKEY_SYSTEM_MASK : 0);
}

// An invalid Down+Start value (an erased 0xFF included) does not vouch for the
// Hold Start bit either, so such a byte reads both settings at their defaults.
inline constexpr bool homeHotkeyRawValid(uint8_t raw) {
  return (raw & HOME_HOTKEY_VALUE_MASK) <= 1;
}

inline constexpr bool holdStartHotkeyEnabledFromRaw(uint8_t raw) {
  return !homeHotkeyRawValid(raw) || (raw & HOLD_START_HOTKEY_OFF_MASK) == 0;
}

inline constexpr uint8_t encodeHomeHotkey(uint8_t raw, bool enabled) {
  return static_cast<uint8_t>(
      (homeHotkeyRawValid(raw) ? (raw & HOLD_START_HOTKEY_OFF_MASK) : 0) |
      (enabled ? 1 : 0));
}

inline constexpr uint8_t encodeHoldStartHotkey(uint8_t raw, bool enabled) {
  return static_cast<uint8_t>(
      (homeHotkeyRawValid(raw) ? (raw & HOME_HOTKEY_VALUE_MASK) : 1) |
      (enabled ? 0 : HOLD_START_HOTKEY_OFF_MASK));
}

inline constexpr uint8_t encodeMisterDisplayMode(uint8_t raw, uint8_t mode) {
  const uint8_t preserve = static_cast<uint8_t>(
      (raw & static_cast<uint8_t>(~MISTER_DISPLAY_INPUTS_MASK)) |
      (mode ? MISTER_DISPLAY_INPUTS_MASK : 0));
  return encodeMenuHotkeyFlags(
      menuHotkeyQuickEnabledFromRaw(raw),
      menuHotkeySystemEnabledFromRaw(raw),
      preserve);
}

struct PersistedBlockHeader {
  uint16_t magic;
  uint8_t schema_version;
  uint8_t payload_size;
  uint16_t generation;
  uint16_t payload_crc16;
  uint16_t header_crc16;
};

static_assert(sizeof(PersistedBlockHeader) == 10, "PersistedBlockHeader size changed unexpectedly");
constexpr uint16_t PERSISTED_BLOCK_HEADER_SIZE = sizeof(PersistedBlockHeader);

struct GlobalSettingsRecord {
  uint8_t persisted_input_mode;
  uint8_t configured_output_mode;
  uint8_t display_contrast;
  uint8_t latency_test;
  uint8_t latency_controller_in_loop;
  uint8_t latency_host_type;
  uint8_t buzzer_mode;
  uint8_t led_mode;
  uint8_t led_brightness;
  uint8_t screensaver_mode;
  uint8_t screensaver_animation;
  uint8_t menu_hotkey;
  uint8_t home_hotkey;
  uint8_t capture_hotkey;
  uint8_t win_output;
  uint8_t psx_periph;
  uint8_t snes_rumbletech;
  uint8_t touchpad_mouse;
  uint8_t home_screen_debug;
  uint8_t menu_ok_back;
  uint16_t sound_events;
};

static_assert(sizeof(GlobalSettingsRecord) == 22, "GlobalSettingsRecord size changed unexpectedly");
static_assert(offsetof(GlobalSettingsRecord, menu_ok_back) == 19 &&
              offsetof(GlobalSettingsRecord, sound_events) == 20,
              "OK/Back must use legacy padding without moving sound settings");

// Kept outside the fixed global/per-mode records so menu visibility can grow
// without changing their persisted sizes or moving any existing store.
struct MenuModeVisibilityRecord {
  uint32_t hidden_input_modes;
  uint32_t hidden_output_modes;
};

static_assert(sizeof(MenuModeVisibilityRecord) == 8,
              "MenuModeVisibilityRecord size changed unexpectedly");


struct PerModeSettingsRecord {
  uint8_t deadzone_percent;
  uint8_t socd;
  uint8_t dpad_mode;
  uint8_t stick_invert;
  uint8_t rumble_level;
  uint8_t trigger_mode;
  uint8_t spinner_speed;
  uint8_t button_map_mode;
  uint8_t n64_z_mode;
  uint8_t n64_cstick_mode;
  uint8_t wii_analog_range;
  // Retired: the N64 stick always learns, and no menu offers this. The byte
  // stays so the record does not move.
  uint8_t n64_analog_range;
  uint8_t nso_special;
  // The PSX record keeps the neGcon I/II/L choice in the NES Device byte
  // (negcon_input_modes.h).
  union {
    uint8_t powerpad_mode;
    uint8_t psx_negcon_inputs;
  };
  int8_t guncon_offset_x;
  int8_t guncon_offset_y;
  uint8_t reserved_musical_buttons;
  // Product profiles share retired bytes without changing EEPROM layout:
  // classic ingress stores the GameCube L assignment (the PSX record keeps
  // its poll timing here, see psx_poll_timing.h) while feedback-capable
  // products store custom controller color channels.
  union {
    uint8_t gamecube_l_switch_mode;
    uint8_t psx_poll_timing;
    uint8_t reserved_mouse_analog;
  };
  uint8_t reserved_mouse_sensitivity;
  uint8_t reserved_mouse_stick;
  // Products without JogCon ingress use this mode-local byte for their
  // controller-feedback policy.
  union {
    uint8_t jogcon_mode;
    uint8_t reserved_dualsense_led_policy;
  };
  uint8_t jogcon_force;
  uint8_t wheel_sensitivity;
  uint8_t jogcon_digital_map;
  uint8_t jogcon_wheel_axis;
  int8_t analog_cal_lx_min;
  int8_t analog_cal_lx_max;
  int8_t analog_cal_ly_min;
  int8_t analog_cal_ly_max;
  int8_t analog_cal_rx_min;
  int8_t analog_cal_rx_max;
  int8_t analog_cal_ry_min;
  int8_t analog_cal_ry_max;
  uint8_t analog_cal_enabled;
  uint8_t turbo_rates[PER_MODE_TURBO_SIZE];
  uint8_t remaps[PER_MODE_REMAP_SIZE];
};

static_assert(sizeof(PerModeSettingsRecord) == 70, "PerModeSettingsRecord size changed unexpectedly");

using PerModeQuickSettings = PerModeSettingsRecord;

enum class SettingScope : uint8_t {
  Global = 0,
  PerMode,
};

enum class SettingValueType : uint8_t {
  UInt8 = 0,
  Int8,
  UInt16,
};

enum class SettingId : uint8_t {
  PersistedInputMode = 0,
  ConfiguredOutputMode,
  DisplayContrast,
  LatencyTest,
  LatencyControllerInLoop,
  LatencyHostType,
  BuzzerMode,
  SoundEvents,
  LedMode,
  LedBrightness,
  ScreensaverMode,
  ScreensaverAnimation,
  MenuHotkey,
  HomeHotkey,
  CaptureHotkey,
  WinOutput,
  PsxPeriph,
  SnesRumbleTech,
  HomeScreenDebug,
  HomeButtonLabels,
  HomeJvsView,
  HotkeyHoldTime,
  Deadzone,
  Socd,
  DpadMode,
  StickInvert,
  RumbleLevel,
  TriggerMode,
  SpinnerSpeed,
  ButtonMapMode,
  N64ZMode,
  N64CStickMode,
  WiiAnalogRange,
  N64AnalogRange,
  NsoSpecial,
  PowerpadMode,
  GunconOffsetX,
  GunconOffsetY,
  ReservedMusicalButtons,
  GameCubeLSwitchMode,
  ReservedMouseSensitivity,
  ReservedMouseStick,
  JogconMode,
  JogconForce,
  JogconRange,
  JogconDigitalMap,
  JogconWheelAxis,
  AnalogCalLXMin,
  AnalogCalLXMax,
  AnalogCalLYMin,
  AnalogCalLYMax,
  AnalogCalRXMin,
  AnalogCalRXMax,
  AnalogCalRYMin,
  AnalogCalRYMax,
  AnalogCalEnabled,
  ClassicDualMerge,
  SystemMenuHotkey,
  KioskMode,
  TouchpadMouse,
  ProductOutputTransport,
  ProductWirelessOutputPersona,
  MisterOutput,
  Usb2Db15MultiBind,
  ManagementMsc,
  Usb2Db15GameSelection,
  PaddleDivisor,
  RotaryReverse,
  DrivingWrap,
  OledInactive,
  OledWake,
  MenuOkBack,
  // IDs from here on are serial and OLED only: the WebHID LIST bitmap covers 0-71.
  Switch2Output,
  Ps2RemoteOutput,
  MisterDisplay,
  HoldStartHotkey,
  Count,
};

static_assert(static_cast<uint8_t>(SettingId::ManagementMsc) == 64,
              "Existing persisted setting IDs must remain stable");
static_assert(static_cast<uint8_t>(SettingId::MisterDisplay) == 74,
              "MiSTer display setting must remain appended after existing IDs");
static_assert(static_cast<uint8_t>(SettingId::HoldStartHotkey) == 75,
              "Hold Start setting must remain appended after existing IDs");

extern uint16_t g_eeprom_remap_index;

bool isConcreteModeForPerSettings(DeviceEnum mode);
uint8_t defaultButtonMapModeForInputMode(DeviceEnum mode);
uint8_t defaultZButtonModeForInputMode(DeviceEnum mode);
uint8_t defaultN64CStickModeForInputMode(DeviceEnum mode);
uint8_t defaultSpinnerSpeedForInputMode(DeviceEnum mode);
uint8_t defaultTriggerModeForInputMode(DeviceEnum mode);
uint8_t defaultRumbleLevelForInputMode(DeviceEnum mode);
uint8_t defaultNsoSpecialForInputMode(DeviceEnum mode);

bool readGlobalSettings(GlobalSettingsRecord& settings);
void writeGlobalSettings(const GlobalSettingsRecord& settings);
bool readMenuModeVisibility(MenuModeVisibilityRecord& visibility);
bool saveMenuModeVisibility(const MenuModeVisibilityRecord& visibility);
bool clearMenuModeVisibility();
bool saveGlobalSettingsWithMenuModeVisibility(
    const GlobalSettingsRecord& settings,
    const MenuModeVisibilityRecord& visibility);
bool menuModeVisibilityInputHidden(uint8_t mode);
bool menuModeVisibilityOutputHidden(uint8_t mode);
// Boot-only: validates the fixed-tail reservation before loading the cache.
void initializeMenuModeVisibilityFromStorage(uint16_t occupiedStorageEnd);
// First-boot setup: pending on factory defaults until the wizard saves once.
bool firstBootSetupPending();
bool saveFirstBootSetupSettings(const GlobalSettingsRecord& staged);
bool saveFirstBootSetupSettings(
    const GlobalSettingsRecord& staged,
    const MenuModeVisibilityRecord& visibility);
bool readPerModeSettings(DeviceEnum mode, PerModeSettingsRecord& settings);
// Runs once at boot, before anything writes a per-mode record.
void migrateLegacyPerModeSettings();
void writePerModeSettings(DeviceEnum mode, const PerModeSettingsRecord& settings);
// Immediate semantic writes use this seam when their acknowledgement promises
// the record survived EEPROM.commit(). Existing fire-and-forget callers retain
// writePerModeSettings().
bool writePerModeSettingsDurable(
    DeviceEnum mode, const PerModeSettingsRecord& settings);

int32_t loadSettingValue(SettingId id, DeviceEnum mode);
void saveSettingValue(SettingId id, int32_t value, DeviceEnum mode);

void sanitizeGlobalSettings(GlobalSettingsRecord& settings);
void sanitizePerModeSettings(DeviceEnum mode, PerModeSettingsRecord& settings);
void applyPerModeSettings(const PerModeSettingsRecord& settings);
void captureRuntimePerModeSettings(DeviceEnum mode, PerModeSettingsRecord& settings);
void captureMenuPerModeSettings(DeviceEnum mode, PerModeSettingsRecord& settings);
void savePerModeSettingsFromRuntime(DeviceEnum mode);
void savePerModeSettingsFromMenu(DeviceEnum mode);

inline bool readPerModeQuickSettings(DeviceEnum mode, PerModeQuickSettings& settings) {
  return readPerModeSettings(mode, settings);
}

inline void writePerModeQuickSettings(DeviceEnum mode, const PerModeQuickSettings& settings) {
  writePerModeSettings(mode, settings);
}

inline void sanitizePerModeQuickSettings(DeviceEnum mode, PerModeQuickSettings& settings) {
  sanitizePerModeSettings(mode, settings);
}

inline void applyPerModeQuickSettings(const PerModeQuickSettings& settings) {
  applyPerModeSettings(settings);
}

inline void captureRuntimeQuickSettings(DeviceEnum mode, PerModeQuickSettings& settings) {
  captureRuntimePerModeSettings(mode, settings);
}

inline void captureMenuQuickSettings(DeviceEnum mode, PerModeQuickSettings& settings) {
  captureMenuPerModeSettings(mode, settings);
}

inline void savePerModeQuickSettingsFromRuntime(DeviceEnum mode) {
  savePerModeSettingsFromRuntime(mode);
}

inline void savePerModeQuickSettingsFromMenu(DeviceEnum mode) {
  savePerModeSettingsFromMenu(mode);
}

void loadPerModeSettings();
// Reloads active_remaps when the output in use changed without a reboot.
void refreshRemapsForOutputChange();
void persistConfiguredInputMode(DeviceEnum mode);
void saveModeSettings();
bool saveMenuModeSettings();
void saveModeSettings(DeviceEnum newInputMode, outputMode_t newOutputMode, const uint8_t* tempRates,
                      const PerModeSettingsRecord& selectedSettings);

void saveSystemSettingByte(SettingId id, uint8_t value);
void saveSystemSettingWord(SettingId id, uint16_t value);
void saveSoundSettings(uint8_t buzzerModeValue, uint16_t soundEvents);
void saveSoundEventSettings(uint16_t soundEvents);
void saveOledSettings(uint8_t oledInactiveValue, uint8_t oledWakeValue, uint8_t screensaverModeValue,
                      uint8_t screensaverAnimationValue);
void saveGunconOffsets(int8_t offsetX, int8_t offsetY);
bool saveButtonRemaps();
// eraseRemaps=false keeps every input mode's button remaps across the reset;
// eraseLearnedMaps=false keeps learned HID controller maps.
bool factoryResetSettings(bool eraseRemaps = true, bool eraseLearnedMaps = true);
// Boot found no valid global record: write the EEPROM records' defaults and
// keep remaps. Learned maps, profiles, bonds and the ESP32's stores live in
// other sectors that the blank one says nothing about, so they stay.
bool repairBlankSettingsAtBoot();

TurboInputMode getTurboInputModeForDeviceMode(DeviceEnum mode);

#include "settings_layout.h"
