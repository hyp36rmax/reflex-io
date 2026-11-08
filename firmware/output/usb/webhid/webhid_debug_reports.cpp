#include "../../../product_config.h"

#include <string.h>

#include "../../../core/button_remap.h"
#include "../../../core/directional_hotkeys.h"
#include "../../../menu/menu_ui_state.h"
#include "../../output_capabilities.h"
#include "../../../core/classic_analog_range.h"
#include "../../../core/controller_frame_state.h"
#include "../../../core/controller_output_cache_state.h"
#include "../../../core/controller_settings_state.h"
#include "../../../core/device_runtime_state.h"
#include "../../../core/hid_mapping_profiles.h"
#include "../../../core/setting_access.h"
#include "../../../core/settings_registry.h"
#if ADAPT_POLICY_CONTROLLER_FEEDBACK_COMMANDS
  #include "../../../core/bluetooth_pairing_runtime.h"
  #include "../../../core/controller_feedback_config.h"
  #include "../../../core/settings_store.h"
  #include "../../../input/esp32_spi/Input_Esp32Spi.h"
  #include "../../../platform/rgb_led.h"
#endif
#include "../../../input/autodetect/input_autodetect_runtime_state.h"
#ifdef ENABLE_INPUT_JAGUAR
  #include "../../../input/jaguar/input_jaguar_runtime_state.h"
#endif
#include "../../runtime/input_runtime_output_bridge.h"
#include "../../output_runtime_state.h"
#include "webhid_debug_reports.h"
#include "webhid_input_modes.h"
#include "webhid_profile_response.h"
#include "webhid_protocol.h"

#if defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED)
  #include "hardware/gpio.h"
#endif

namespace {
uint8_t webhid_input_state_player = 0;
bool webhid_raw_profile_mode = false;
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
bool webhid_raw_direction_hotkeys = false;
uint8_t webhid_direction_status = 0;
#endif
bool webhid_raw_management_mode = false;
bool webhid_raw_setting_mode = false;
uint8_t webhid_raw_setting_op = 0;
uint8_t webhid_raw_setting_id = 0;
uint8_t webhid_raw_setting_status = WEBHID_RAW_STATUS_OK;
SettingWriteResult webhid_raw_setting_write{};
uint8_t webhid_raw_profile_index = 0;
uint8_t webhid_raw_profile_chunk = 0;
uint8_t webhid_raw_profile_status = 0;
uint8_t webhid_raw_profile_write_mask = 0;
bool webhid_raw_profile_operation_response = false;
uint8_t webhid_raw_management_status = WEBHID_RAW_STATUS_OK;
HidMappingProfile webhid_raw_profile_staging{};

// Adapt.html and Adapt Manager read the LIST bitmaps at fixed offsets for
// IDs 0-71. Later IDs stay serial/OLED only until that layout grows.
constexpr uint8_t kWebhidListedSettingCount = WEBHID_SETTING_LIST_BITMAP_BYTES * 8;
static_assert(static_cast<uint8_t>(SettingId::MenuOkBack) < kWebhidListedSettingCount,
              "WebHID setting bitmaps must cover every WebHID setting");
constexpr uint8_t kWebhidSettingCount =
    static_cast<uint8_t>(SettingId::Count) < kWebhidListedSettingCount
      ? static_cast<uint8_t>(SettingId::Count) : kWebhidListedSettingCount;

bool webhidSettingAvailable(SettingId id) {
  if (static_cast<uint8_t>(id) >= kWebhidListedSettingCount) return false;
#if defined(ADAPT_CAP_OUTPUT_TRANSPORT_SELECTION)
  // These restart the adapter immediately, before the reply can be read.
  if (id == SettingId::ProductOutputTransport ||
      id == SettingId::ProductWirelessOutputPersona) {
    return false;
  }
#endif
  return id != SettingId::PersistedInputMode &&
         id != SettingId::ConfiguredOutputMode &&
         settingAccessSupported(id);
}

void putWebhidInt32(uint8_t* out, int32_t value) {
  const uint32_t bits = static_cast<uint32_t>(value);
  out[0] = bits & 0xFF;
  out[1] = (bits >> 8) & 0xFF;
  out[2] = (bits >> 16) & 0xFF;
  out[3] = (bits >> 24) & 0xFF;
}

uint8_t webhidSettingStatus(SettingAccessStatus status) {
  switch (status) {
    case SettingAccessStatus::Ok: return WEBHID_RAW_STATUS_OK;
    case SettingAccessStatus::Unsupported: return WEBHID_RAW_STATUS_UNSUPPORTED;
    case SettingAccessStatus::WriteFailed: return WEBHID_RAW_STATUS_FAILED;
    default: return WEBHID_RAW_STATUS_INVALID;
  }
}

void processWebhidSettingRequest(const uint8_t* buffer, uint16_t bufsize) {
  webhid_raw_setting_op = buffer[1];
  webhid_raw_setting_id = bufsize > 2 ? buffer[2] : 0;
  webhid_raw_setting_status = WEBHID_RAW_STATUS_OK;
  webhid_raw_setting_write = {};
  if (webhid_raw_setting_op != WEBHID_RAW_SETTING_SET) return;
  const SettingId id = static_cast<SettingId>(webhid_raw_setting_id);
  if (bufsize < 7) {
    webhid_raw_setting_status = WEBHID_RAW_STATUS_INVALID;
  } else if (!webhidSettingAvailable(id)) {
    webhid_raw_setting_status = WEBHID_RAW_STATUS_UNSUPPORTED;
  } else {
    const int32_t value = static_cast<int32_t>(
        static_cast<uint32_t>(buffer[3]) |
        (static_cast<uint32_t>(buffer[4]) << 8) |
        (static_cast<uint32_t>(buffer[5]) << 16) |
        (static_cast<uint32_t>(buffer[6]) << 24));
    webhid_raw_setting_write = settingAccessWrite(id, deviceMode, value);
    webhid_raw_setting_status = webhidSettingStatus(webhid_raw_setting_write.status);
  }
}

uint16_t getWebhidSettingReport(uint8_t* buffer) {
  memset(buffer, 0, WEBHID_REPORT_SIZE);
  buffer[0] = WEBHID_MAGIC_BYTE;
  buffer[1] = WEBHID_RAW_SETTING_RESPONSE;
  buffer[2] = webhid_raw_setting_op;
  buffer[3] = webhid_raw_setting_status;
  buffer[4] = webhid_raw_setting_id;
  const uint8_t inputMode = webhid_input_mode_from_device(deviceMode);
  if (webhid_raw_setting_op == WEBHID_RAW_SETTING_LIST) {
    bool visible[static_cast<uint8_t>(SettingId::Count)];
    settingAccessVisible(deviceMode, canonicalizeOutputMode(configuredOutputMode), visible);
    buffer[WEBHID_SETTING_LIST_COUNT_INDEX] = kWebhidSettingCount;
    for (uint8_t raw = 0; raw < kWebhidSettingCount; ++raw) {
      const uint8_t bit = static_cast<uint8_t>(1u << (raw % 8));
      if (webhidSettingAvailable(static_cast<SettingId>(raw))) {
        buffer[WEBHID_SETTING_LIST_SUPPORTED_INDEX + raw / 8] |= bit;
        if (visible[raw]) buffer[WEBHID_SETTING_LIST_VISIBLE_INDEX + raw / 8] |= bit;
      }
    }
    buffer[WEBHID_SETTING_LIST_MODE_INDEX] = inputMode;
    return WEBHID_REPORT_SIZE;
  }

  const SettingId id = static_cast<SettingId>(webhid_raw_setting_id);
  if (webhid_raw_setting_op != WEBHID_RAW_SETTING_GET &&
      webhid_raw_setting_op != WEBHID_RAW_SETTING_SET) {
    buffer[3] = WEBHID_RAW_STATUS_INVALID;
    return WEBHID_REPORT_SIZE;
  }
  if (!webhidSettingAvailable(id)) {
    buffer[3] = WEBHID_RAW_STATUS_UNSUPPORTED;
    return WEBHID_REPORT_SIZE;
  }
  if (settingIsPerMode(id) && !isConcreteModeForPerSettings(deviceMode)) {
    buffer[3] = WEBHID_RAW_STATUS_INVALID;
    return WEBHID_REPORT_SIZE;
  }
  if (buffer[3] != WEBHID_RAW_STATUS_OK) return WEBHID_REPORT_SIZE;
  const SettingSpec& spec = settingSpec(id);
  const bool wrote = webhid_raw_setting_op == WEBHID_RAW_SETTING_SET;
  int32_t runtime = 0;
  const bool runtimeKnown = wrote
      ? webhid_raw_setting_write.runtimeKnown
      : settingAccessRuntimeValue(id, deviceMode, &runtime);
  if (wrote) runtime = webhid_raw_setting_write.runtime;
  buffer[WEBHID_SETTING_SCOPE_INDEX] = settingIsPerMode(id) ? 1 : 0;
  buffer[WEBHID_SETTING_TYPE_INDEX] = static_cast<uint8_t>(spec.value_type);
  putWebhidInt32(&buffer[WEBHID_SETTING_STORED_INDEX],
                 wrote ? webhid_raw_setting_write.stored : loadSettingValue(id, deviceMode));
  putWebhidInt32(&buffer[WEBHID_SETTING_DEFAULT_INDEX], defaultSettingValue(id, deviceMode));
  putWebhidInt32(&buffer[WEBHID_SETTING_MIN_INDEX], spec.min_value);
  putWebhidInt32(&buffer[WEBHID_SETTING_MAX_INDEX], spec.max_value);
  putWebhidInt32(&buffer[WEBHID_SETTING_RUNTIME_INDEX], runtime);
  buffer[WEBHID_SETTING_FLAGS_INDEX] =
      (runtimeKnown ? WEBHID_SETTING_FLAG_RUNTIME_KNOWN : 0) |
      (wrote && webhid_raw_setting_write.rebootRequired
           ? WEBHID_SETTING_FLAG_REBOOT_REQUIRED : 0);
  buffer[WEBHID_SETTING_MODE_INDEX] = inputMode;
  return WEBHID_REPORT_SIZE;
}

bool validateImportedProfile(const HidMappingProfile& profile) {
  if (profile.key.transport < HID_MAPPING_TRANSPORT_USB ||
      profile.key.transport > HID_MAPPING_TRANSPORT_BLUETOOTH_LE) {
    return false;
  }
  for (uint8_t source : profile.button_source) {
    if (source != HID_MAPPING_SOURCE_NONE && source >= 32) return false;
  }
  for (uint8_t source : profile.axis_source) {
    if (source != HID_MAPPING_SOURCE_NONE && source >= 12) return false;
  }
  for (uint8_t axis = 0; axis < HID_MAPPING_AXIS_COUNT; ++axis) {
    const uint8_t allowed =
        (profile.flags & HID_MAPPING_PROFILE_BUTTON_DPAD) && axis < 4
            ? 0xFF
            : static_cast<uint8_t>(HID_MAPPING_AXIS_INVERT |
                                   HID_MAPPING_AXIS_TRIGGER);
    if (profile.axis_flags[axis] & ~allowed) {
      return false;
    }
  }
  if (!hidMappingControllerStyleValid(profile.layout) ||
      hidMappingLayoutTriggerType(profile.layout) ==
          HID_MAPPING_LAYOUT_TRIGGER_MASK) {
    return false;
  }
  return profile.hat_source == HID_MAPPING_SOURCE_NONE ||
         profile.hat_source == 0;
}

#if defined(ADAPT_CAP_ESP32_CONTROLLER_BRIDGE) && defined(ENABLE_INPUT_ESP32_SPI)
uint32_t validProfileMask() {
  HidMappingProfileList list{};
  hidMappingProfileReadList(list);
  uint32_t mask = 0;
  for (uint8_t index = 0; index < HID_MAPPING_PROFILE_COUNT; ++index) {
    if (list.profiles[index].flags & HID_MAPPING_PROFILE_VALID) {
      mask |= 1UL << index;
    }
  }
  return mask;
}

void applyDeviceLedWebSettings(uint8_t mode, uint8_t brightness,
                               uint8_t colorPreset) {
  led_mode = mode;
  led_brightness = brightness;
  setDeviceLedColorPreset(colorPreset);
  controllerFeedbackConfigSave();
  saveSettingValue(SettingId::LedMode, led_mode, deviceMode);
  saveSettingValue(SettingId::LedBrightness, led_brightness, deviceMode);
  const ControllerFeedbackColor color = deviceLedColor();
  rgbLed.setBaseColor({color.r, color.g, color.b});
  rgbLed.setMode(static_cast<led_mode_t>(led_mode));
  rgbLed.setBrightness(led_brightness);
  rgbLed.setEnabled(led_mode != LED_MODE_OFF);
}

uint16_t getModern2UsbManagementReport(uint8_t* buffer) {
  const Esp32SpiRuntimeStatus bridge = esp32SpiRuntimeStatus();
  const BluetoothPairingStatus pairing = bluetooth_pairing_status();
  const ControllerFeedbackColor controllerColor =
      controllerLedCustomColor();
  memset(buffer, 0, WEBHID_REPORT_SIZE);
  buffer[0] = WEBHID_MAGIC_BYTE;
  buffer[1] = WEBHID_RAW_MANAGEMENT_RESPONSE;
  buffer[2] = webhid_raw_management_status;
  buffer[3] = 0x0F;  // BT, controller LEDs, device LEDs, HID profiles.
  buffer[4] =
      (pairing.supported ? 0x01 : 0) |
      (pairing.pending ? 0x02 : 0) |
      (pairing.active ? 0x04 : 0) |
      (pairing.last_success ? 0x08 : 0) |
      (bridge.scanning ? 0x10 : 0) |
      (bridge.ready ? 0x20 : 0) |
      (bridge.bt_output_active ? 0x40 : 0);
  buffer[5] = bridge.connected_mask;
  buffer[6] = bridge.player_count;
  buffer[7] = bridge.bond_count;
  buffer[8] = bridge.selected_bond_index;
  memcpy(&buffer[9], bridge.selected_bond_addr,
         sizeof(bridge.selected_bond_addr));
  buffer[15] = static_cast<uint8_t>(controllerLedMode());
  buffer[16] = controllerLedBrightness();
  buffer[17] = controllerColor.r;
  buffer[18] = controllerColor.g;
  buffer[19] = controllerColor.b;
  buffer[20] = led_mode;
  buffer[21] = led_brightness;
  buffer[22] = deviceLedColorPreset();
  const uint32_t profileMask = validProfileMask();
  buffer[23] = static_cast<uint8_t>(__builtin_popcount(profileMask));
  buffer[24] = HID_MAPPING_PROFILE_COUNT;
  buffer[25] = profileMask & 0xFF;
  buffer[26] = (profileMask >> 8) & 0xFF;
  buffer[27] = (profileMask >> 16) & 0xFF;
  buffer[28] = (profileMask >> 24) & 0xFF;
  return WEBHID_REPORT_SIZE;
}

void processModern2UsbManagement(const uint8_t* buffer, uint16_t bufsize) {
  webhid_raw_management_mode = true;
  webhid_raw_profile_mode = false;
  webhid_raw_management_status = WEBHID_RAW_STATUS_OK;
  const Esp32SpiRuntimeStatus bridge = esp32SpiRuntimeStatus();
  switch (buffer[1]) {
    case WEBHID_RAW_MANAGEMENT_STATUS:
      return;
    case WEBHID_RAW_MANAGEMENT_PAIR:
      if (bridge.bt_output_active) {
        webhid_raw_management_status = WEBHID_RAW_STATUS_FAILED;
      } else if (!bluetooth_pairing_supported()) {
        webhid_raw_management_status = WEBHID_RAW_STATUS_UNSUPPORTED;
      } else {
        bluetooth_pairing_request(BLUETOOTH_PAIRING_SOURCE_MENU);
      }
      return;
    case WEBHID_RAW_MANAGEMENT_CANCEL_PAIR:
      bluetooth_pairing_cancel();
      return;
    case WEBHID_RAW_MANAGEMENT_SELECT_BOND:
      if (bufsize < 3 || buffer[2] >= bridge.bond_count) {
        webhid_raw_management_status = WEBHID_RAW_STATUS_INVALID;
      } else {
        esp32SpiSelectBond(buffer[2]);
      }
      return;
    case WEBHID_RAW_MANAGEMENT_RECONNECT_BOND:
      if (bufsize < 3 || buffer[2] >= bridge.bond_count) {
        webhid_raw_management_status = WEBHID_RAW_STATUS_INVALID;
      } else {
        esp32SpiRequestReconnectBond(buffer[2]);
      }
      return;
    case WEBHID_RAW_MANAGEMENT_FORGET_BOND:
      if (bufsize < 3 || buffer[2] >= bridge.bond_count) {
        webhid_raw_management_status = WEBHID_RAW_STATUS_INVALID;
      } else {
        esp32SpiRequestForgetBond(buffer[2]);
      }
      return;
    case WEBHID_RAW_MANAGEMENT_FORGET_ALL_BONDS:
      esp32SpiRequestForgetBonds();
      bluetooth_pairing_cancel();
      return;
    case WEBHID_RAW_MANAGEMENT_CONTROLLER_LEDS:
      if (bufsize < 7 || buffer[2] > CONTROLLER_LED_CUSTOM ||
          buffer[3] > 2) {
        webhid_raw_management_status = WEBHID_RAW_STATUS_INVALID;
      } else {
        setControllerLedMode(static_cast<ControllerLedMode>(buffer[2]));
        setControllerLedBrightness(buffer[3]);
        setControllerLedCustomColor({buffer[4], buffer[5], buffer[6]});
        controllerFeedbackConfigSave();
      }
      return;
    case WEBHID_RAW_MANAGEMENT_DEVICE_LEDS:
      if (bufsize < 5 || buffer[2] >= LED_MODE_LAST ||
          buffer[3] > 254 || buffer[4] >= 8) {
        webhid_raw_management_status = WEBHID_RAW_STATUS_INVALID;
      } else {
        applyDeviceLedWebSettings(buffer[2], buffer[3], buffer[4]);
      }
      return;
    case WEBHID_RAW_MANAGEMENT_IDENTIFY_PLAYER:
      if (bufsize < 3 || buffer[2] >= 4) {
        webhid_raw_management_status = WEBHID_RAW_STATUS_INVALID;
      } else {
        identifyControllerPlayer(buffer[2]);
      }
      return;
    default:
      webhid_raw_management_status = WEBHID_RAW_STATUS_INVALID;
      return;
  }
}
#endif
}

void webhid_set_input_state_player(const uint8_t* buffer, uint16_t bufsize) {
  if (bufsize >= 2 && buffer[0] == WEBHID_MAGIC_BYTE) {
    webhid_input_state_player = buffer[1];
  }
}

uint16_t webhid_get_input_state_report(uint8_t* buffer) {
  // Return live controller input state for visualization (split display: input vs output)
  // Format:
  // Byte 0: magic (0xAD)
  // Byte 1: player index (0-5)
  // Byte 2: connected flag
  // Bytes 3-6: OUTPUT buttons (32-bit, WITH remap/turbo applied)
  // Bytes 7-8: INPUT Left X (16-bit signed, raw)
  // Bytes 9-10: INPUT Left Y (16-bit signed, raw)
  // Bytes 11-12: INPUT Right X (16-bit signed, raw)
  // Bytes 13-14: INPUT Right Y (16-bit signed, raw)
  // Byte 15: Left trigger (8-bit)
  // Byte 16: Right trigger (8-bit)
  // Byte 17: Stick precision (0=8-bit, 1=12-bit, 2=16-bit)
  // Bytes 18-29: Controller type name (null-terminated)
  // Byte 30: Button count for current input mode
  // Bytes 31-34: Button mask (32-bit, which bits in digital_buttons are valid)
  // Byte 35: Stable WebHID input mode ID
  // Bytes 36-39: INPUT buttons (32-bit, RAW pre-remap state)
  // Byte 40: Output mode (outputMode_t)
  // Bytes 41-42: OUTPUT Left X (16-bit signed, with dpad_mode applied)
  // Bytes 43-44: OUTPUT Left Y (16-bit signed, with dpad_mode applied)
  // Bytes 45-46: OUTPUT Right X (16-bit signed, with dpad_mode applied)
  // Bytes 47-48: OUTPUT Right Y (16-bit signed, with dpad_mode applied)
  // Byte 49: dpad_mode
  // Byte 50: button_map_mode
  // Byte 51: n64_z_mode
  // Byte 52: configured n64_cstick_mode
  // Byte 53: effective n64_cstick_mode
  // Byte 54: configured output mode
  // Byte 55: effective output mode
  // Byte 56: raw left trigger before analog range learn/normalization
  // Byte 57: raw right trigger before analog range learn/normalization
  // Byte 58: OUTPUT left trigger after the shared output transform/cache path
  // Byte 59: OUTPUT right trigger after the shared output transform/cache path
  // Byte 60: stable WebHID saved input mode ID
  // Byte 61: input auto-detect active flag
  // Byte 62: bit 0, a Jaguar rotary is live on either port (they share a profile)

  uint8_t player = webhid_input_state_player;
  if (player >= max_devices || player >= MAX_USB_OUT) {
    player = 0;
  }
  const controller_state_t& frame = controllerFrameConst(player);
  buffer[0] = WEBHID_MAGIC_BYTE;
  buffer[1] = player;
  buffer[2] = frame.connected ? 1 : 0;

  uint32_t outputButtons = post_remap_buttons[player];
  buffer[3] = outputButtons & 0xFF;
  buffer[4] = (outputButtons >> 8) & 0xFF;
  buffer[5] = (outputButtons >> 16) & 0xFF;
  buffer[6] = (outputButtons >> 24) & 0xFF;

  ClassicAnalogRangeSnapshot analogSnapshot = {};
  const bool hasAnalogSnapshot =
      classicModeHasRangeSetting(deviceMode) &&
      getClassicAnalogRangeSnapshot(deviceMode, player, analogSnapshot);

  int16_t lx = frame.LX;
  int16_t ly = frame.LY;
  int16_t rx = frame.RX;
  int16_t ry = frame.RY;
  if (hasAnalogSnapshot) {
    if (analogSnapshot.valid[CLASSIC_ANALOG_AXIS_LX]) {
      lx = analogSnapshot.raw[CLASSIC_ANALOG_AXIS_LX];
    }
    if (analogSnapshot.valid[CLASSIC_ANALOG_AXIS_LY]) {
      ly = analogSnapshot.raw[CLASSIC_ANALOG_AXIS_LY];
    }
    if (analogSnapshot.valid[CLASSIC_ANALOG_AXIS_RX]) {
      rx = analogSnapshot.raw[CLASSIC_ANALOG_AXIS_RX];
    }
    if (analogSnapshot.valid[CLASSIC_ANALOG_AXIS_RY]) {
      ry = analogSnapshot.raw[CLASSIC_ANALOG_AXIS_RY];
    }
  }

  buffer[7] = lx & 0xFF;
  buffer[8] = (lx >> 8) & 0xFF;
  buffer[9] = ly & 0xFF;
  buffer[10] = (ly >> 8) & 0xFF;
  buffer[11] = rx & 0xFF;
  buffer[12] = (rx >> 8) & 0xFF;
  buffer[13] = ry & 0xFF;
  buffer[14] = (ry >> 8) & 0xFF;

  uint8_t rawAnalogL2 = frame.ANALOG_L2;
  uint8_t rawAnalogR2 = frame.ANALOG_R2;
  uint8_t calibratedAnalogL2 = frame.ANALOG_L2;
  uint8_t calibratedAnalogR2 = frame.ANALOG_R2;
  if (hasAnalogSnapshot) {
    if (analogSnapshot.trigger_valid[CLASSIC_ANALOG_TRIGGER_L2]) {
      rawAnalogL2 = analogSnapshot.trigger_raw[CLASSIC_ANALOG_TRIGGER_L2];
      calibratedAnalogL2 = analogSnapshot.trigger_calibrated[CLASSIC_ANALOG_TRIGGER_L2];
    }
    if (analogSnapshot.trigger_valid[CLASSIC_ANALOG_TRIGGER_R2]) {
      rawAnalogR2 = analogSnapshot.trigger_raw[CLASSIC_ANALOG_TRIGGER_R2];
      calibratedAnalogR2 = analogSnapshot.trigger_calibrated[CLASSIC_ANALOG_TRIGGER_R2];
    }
  }

  buffer[15] = calibratedAnalogL2;
  buffer[16] = calibratedAnalogR2;
  buffer[17] = frame.sticks_precision_bits;

  strncpy((char*)&buffer[18], frame.controller_type_name, 11);
  buffer[29] = 0;

  buffer[30] = getRemapButtonCount(deviceMode);

  uint32_t btnMask = getInputButtonMask(deviceMode);
  buffer[31] = btnMask & 0xFF;
  buffer[32] = (btnMask >> 8) & 0xFF;
  buffer[33] = (btnMask >> 16) & 0xFF;
  buffer[34] = (btnMask >> 24) & 0xFF;

  buffer[35] = webhid_input_mode_from_device(deviceMode);

  uint32_t inputButtons = raw_input_buttons[player];
  buffer[36] = inputButtons & 0xFF;
  buffer[37] = (inputButtons >> 8) & 0xFF;
  buffer[38] = (inputButtons >> 16) & 0xFF;
  buffer[39] = (inputButtons >> 24) & 0xFF;

  buffer[40] = (uint8_t)outputMode;

  int16_t out_lx = post_output_lx[player];
  int16_t out_ly = post_output_ly[player];
  int16_t out_rx = post_output_rx[player];
  int16_t out_ry = post_output_ry[player];
  buffer[41] = out_lx & 0xFF;
  buffer[42] = (out_lx >> 8) & 0xFF;
  buffer[43] = out_ly & 0xFF;
  buffer[44] = (out_ly >> 8) & 0xFF;
  buffer[45] = out_rx & 0xFF;
  buffer[46] = (out_rx >> 8) & 0xFF;
  buffer[47] = out_ry & 0xFF;
  buffer[48] = (out_ry >> 8) & 0xFF;

  buffer[49] = dpad_mode;
  buffer[50] = button_map_mode;
  buffer[51] = n64_z_mode;
  buffer[52] = n64_cstick_mode;
  buffer[53] = (uint8_t)get_effective_n64_cstick_mode();
  buffer[54] = (uint8_t)configuredOutputMode;
  buffer[55] = (uint8_t)get_effective_output_mode();
  buffer[56] = rawAnalogL2;
  buffer[57] = rawAnalogR2;
  buffer[58] = post_output_l2[player];
  buffer[59] = post_output_r2[player];
  buffer[60] = webhid_input_mode_from_device(savedDeviceMode);
  buffer[61] = inputAutoDetectModeActive() ? 1 : 0;
  buffer[62] = 0;
#ifdef ENABLE_INPUT_JAGUAR
  for (uint8_t port = 0; deviceMode == RZORD_JAGUAR && port < MAX_USB_OUT; ++port) {
    if (jaguarRotaryActivePorts[port]) {
      buffer[62] = 1;
    }
  }
#endif

  return WEBHID_REPORT_SIZE;
}

uint16_t webhid_get_gpio_state_report(uint8_t* buffer) {
  // Return GPIO pin states for debugging
  // Format:
  // Byte 0: magic (0xAD)
  // Byte 1: port (0=Port1, 1=Port2)
  // Bytes 2-14: Pin 01-13 states (0=LOW, 1=HIGH)
  // Bytes 15-27: Pin 01-13 GPIO numbers
  // Byte 28: number of pins (13)

  buffer[0] = WEBHID_MAGIC_BYTE;
  buffer[1] = 0;

  const uint8_t port1_pins[] = {11, 10, 26, 27, 9, 12, 13, 5, 6, 2, 7, 4, 3};
  const uint8_t port2_pins[] = {22, 21, 28, 29, 20, 23, 24, 16, 17, 15, 18, 19, 14};

  for (uint8_t i = 0; i < 13; i++) {
    buffer[2 + i] = gpio_get(port1_pins[i]) ? 1 : 0;
    buffer[15 + i] = port1_pins[i];
  }
  buffer[28] = 13;

  for (uint8_t i = 0; i < 13; i++) {
    buffer[29 + i] = gpio_get(port2_pins[i]) ? 1 : 0;
    buffer[42 + i] = port2_pins[i];
  }
  buffer[55] = 13;

  return WEBHID_REPORT_SIZE;
}

uint16_t webhid_get_raw_data_report(uint8_t* buffer) {
  if (webhid_raw_setting_mode) {
    return getWebhidSettingReport(buffer);
  }
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  if (webhid_raw_direction_hotkeys) {
    memset(buffer,0,WEBHID_REPORT_SIZE);
    buffer[0]=WEBHID_MAGIC_BYTE; buffer[1]=WEBHID_RAW_DIRECTION_HOTKEY_RESPONSE;
    buffer[2]=webhid_direction_status; buffer[3]=webhid_input_mode_from_device(deviceMode);
    const auto output=get_effective_output_mode(); buffer[4]=uint8_t(output);
    const auto kind=directionalHotkeyKind(deviceMode,max_devices ? controllerFrameConst(0).controller_type_name : "");
    buffer[5]=uint8_t(kind);
    PerModeSettingsRecord settings{}; readPerModeSettings(deviceMode,settings);
    uint8_t actions[3]; direction_hotkey::read(settings,actions);
#if ADAPT_POLICY_FIXED_HOTKEY_ASSIGNMENTS
    memset(actions, direction_hotkey::Auto, sizeof(actions));
    buffer[WEBHID_DIRECTION_FIXED_ASSIGNMENTS_INDEX]=1;
#endif
    for (uint8_t d=0;d<3;++d) {
      buffer[6+d]=actions[d]; buffer[9+d]=directionalHotkeyDefault(kind,d,output,actions);
      for (uint8_t a=0;a<direction_hotkey::Count;++a) {
#if ADAPT_POLICY_FIXED_HOTKEY_ASSIGNMENTS
        if (a != direction_hotkey::Auto) continue;
#endif
        uint8_t candidate[3]={actions[0],actions[1],actions[2]}; candidate[d]=a;
        if (directionalHotkeysValid(deviceMode,candidate) && (a==direction_hotkey::Auto ||
            directionalHotkeyAvailable(direction_hotkey::Action(a),output)))
          buffer[18+d*3+a/8] |= 1u << (a%8);
      }
    }
    for (uint8_t a=0;a<direction_hotkey::Count;++a)
      if (directionalHotkeyAvailable(direction_hotkey::Action(a),output)) buffer[12+a/8] |= 1u << (a%8);
    buffer[15]=output_uses_switch_genesis_profile() ? 1 : 0;
    return WEBHID_REPORT_SIZE;
  }
#endif
#if defined(ADAPT_CAP_ESP32_CONTROLLER_BRIDGE) && defined(ENABLE_INPUT_ESP32_SPI)
  if (webhid_raw_management_mode) {
    return getModern2UsbManagementReport(buffer);
  }
#endif
  if (webhid_raw_profile_mode) {
    // Byte 0 magic, 1 response marker, 2 slot, 3 chunk, 4 chunk length,
    // 5 status (0=OK, 1=empty/bad request, 2=write failed), 6..62 data.
    constexpr uint8_t kDataOffset = 6;
    constexpr uint8_t kChunkSize = WEBHID_REPORT_SIZE - kDataOffset;
    HidMappingProfileList list{};
    hidMappingProfileReadList(list);
    memset(buffer, 0, WEBHID_REPORT_SIZE);
    buffer[0] = WEBHID_MAGIC_BYTE;
    buffer[1] = WEBHID_RAW_PROFILE_RESPONSE;
    buffer[2] = webhid_raw_profile_index;
    buffer[3] = webhid_raw_profile_chunk;
    const bool selectedProfileValid =
        webhid_raw_profile_index < HID_MAPPING_PROFILE_COUNT &&
        (list.profiles[webhid_raw_profile_index].flags &
         HID_MAPPING_PROFILE_VALID);
    const WebhidRawProfileResponseDecision decision =
        webhidResolveRawProfileResponse(
            webhid_raw_profile_operation_response,
            webhid_raw_profile_status, selectedProfileValid);
    buffer[5] = decision.status;
    if (!decision.include_profile_payload) {
      return WEBHID_REPORT_SIZE;
    }
    const uint8_t offset = webhid_raw_profile_chunk * kChunkSize;
    if (offset >= sizeof(HidMappingProfile)) {
      buffer[5] = 1;
      return WEBHID_REPORT_SIZE;
    }
    const uint8_t remaining = sizeof(HidMappingProfile) - offset;
    const uint8_t length = remaining < kChunkSize ? remaining : kChunkSize;
    buffer[4] = length;
    memcpy(&buffer[kDataOffset],
           reinterpret_cast<const uint8_t*>(&list.profiles[webhid_raw_profile_index]) + offset,
           length);
    return WEBHID_REPORT_SIZE;
  }

  // Return raw controller data buffer
  // Format:
  // Byte 0: magic (0xAD)
  // Byte 1: data length
  // Bytes 2-33: raw data (up to 32 bytes)
  // Byte 34: input mode (deviceMode)
  // Bytes 35+: controller type name

  buffer[0] = WEBHID_MAGIC_BYTE;
  buffer[1] = webhid_raw_data_len;
  memcpy(&buffer[2], webhid_raw_data, webhid_raw_data_len);
  buffer[34] = deviceMode;
  strncpy((char*)&buffer[35], controllerFrameConst(0).controller_type_name, 12);

  return WEBHID_REPORT_SIZE;
}

void webhid_set_raw_data_report(const uint8_t* buffer, uint16_t bufsize) {
  if (buffer == nullptr || bufsize < 2 || buffer[0] != WEBHID_MAGIC_BYTE) return;
  webhid_raw_setting_mode = buffer[1] == WEBHID_RAW_SETTING_LIST ||
                            buffer[1] == WEBHID_RAW_SETTING_GET ||
                            buffer[1] == WEBHID_RAW_SETTING_SET;
  if (webhid_raw_setting_mode) {
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
    webhid_raw_direction_hotkeys = false;
#endif
    webhid_raw_profile_mode = false;
    webhid_raw_management_mode = false;
    processWebhidSettingRequest(buffer, bufsize);
    return;
  }
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  webhid_raw_direction_hotkeys = buffer[1]==WEBHID_RAW_DIRECTION_HOTKEY_GET ||
                                 buffer[1]==WEBHID_RAW_DIRECTION_HOTKEY_SET;
  if (webhid_raw_direction_hotkeys) {
    webhid_direction_status=WEBHID_RAW_STATUS_OK;
    if (buffer[1]==WEBHID_RAW_DIRECTION_HOTKEY_SET) {
#if ADAPT_POLICY_FIXED_HOTKEY_ASSIGNMENTS
      webhid_direction_status=WEBHID_RAW_STATUS_UNSUPPORTED;
      return;
#else
      if (bufsize<6 || menuOwnsControllerInput() || !isConcreteModeForPerSettings(deviceMode) ||
          buffer[2]!=webhid_input_mode_from_device(deviceMode) ||
          !directionalHotkeysValid(deviceMode,&buffer[3])) {
        webhid_direction_status=WEBHID_RAW_STATUS_INVALID; return;
      }
      PerModeSettingsRecord settings{}; readPerModeSettings(deviceMode,settings);
      direction_hotkey::write(settings,&buffer[3]);
      writePerModeSettings(deviceMode,settings);
      applyPerModeSettings(settings);
#endif
    }
    return;
  }
#endif
  constexpr uint8_t kDataOffset = 6;
  constexpr uint8_t kChunkSize = WEBHID_REPORT_SIZE - kDataOffset;
  webhid_raw_profile_status = 0;
  webhid_raw_profile_operation_response = false;
  switch (buffer[1]) {
    case WEBHID_RAW_LIVE_DATA:
      webhid_raw_profile_mode = false;
      webhid_raw_management_mode = false;
      return;

    case WEBHID_RAW_PROFILE_SELECT:
      if (bufsize < 4 || buffer[2] >= HID_MAPPING_PROFILE_COUNT || buffer[3] > 1) {
        webhid_raw_profile_mode = true;
        webhid_raw_management_mode = false;
        webhid_raw_profile_operation_response = true;
        webhid_raw_profile_status = WEBHID_RAW_STATUS_INVALID;
        return;
      }
      webhid_raw_profile_mode = true;
      webhid_raw_management_mode = false;
      webhid_raw_profile_index = buffer[2];
      webhid_raw_profile_chunk = buffer[3];
      return;

    case WEBHID_RAW_PROFILE_WRITE: {
      webhid_raw_profile_mode = true;
      webhid_raw_management_mode = false;
      webhid_raw_profile_operation_response = true;
      if (bufsize < kDataOffset) {
        webhid_raw_profile_status = WEBHID_RAW_STATUS_INVALID;
        return;
      }
      const uint8_t expectedLength = buffer[2] == 0
          ? kChunkSize
          : static_cast<uint8_t>(sizeof(HidMappingProfile) - kChunkSize);
      if (buffer[2] > 1 || buffer[3] != expectedLength ||
          static_cast<uint16_t>(kDataOffset + buffer[3]) > bufsize) {
        webhid_raw_profile_status = WEBHID_RAW_STATUS_INVALID;
        return;
      }
      const uint8_t offset = buffer[2] * kChunkSize;
      if (offset + buffer[3] > sizeof(HidMappingProfile)) {
        webhid_raw_profile_status = WEBHID_RAW_STATUS_INVALID;
        return;
      }
      if (buffer[2] == 0) {
        memset(&webhid_raw_profile_staging, 0,
               sizeof(webhid_raw_profile_staging));
        webhid_raw_profile_write_mask = 0;
      }
      memcpy(reinterpret_cast<uint8_t*>(&webhid_raw_profile_staging) + offset,
             &buffer[kDataOffset], buffer[3]);
      webhid_raw_profile_write_mask |= static_cast<uint8_t>(1u << buffer[2]);
      return;
    }

    case WEBHID_RAW_PROFILE_COMMIT: {
      webhid_raw_profile_mode = true;
      webhid_raw_management_mode = false;
      webhid_raw_profile_operation_response = true;
      if (webhid_raw_profile_write_mask != 0x03 ||
          !validateImportedProfile(webhid_raw_profile_staging)) {
        webhid_raw_profile_status = WEBHID_RAW_STATUS_INVALID;
        return;
      }
      webhid_raw_profile_staging.flags |= HID_MAPPING_PROFILE_VALID;
      uint8_t storedIndex = 0;
      if (!hidMappingProfileUpsert(webhid_raw_profile_staging, &storedIndex)) {
        webhid_raw_profile_status = 2;
      } else {
        webhid_raw_profile_index = storedIndex;
        webhid_raw_profile_chunk = 0;
      }
      webhid_raw_profile_write_mask = 0;
      return;
    }

    case WEBHID_RAW_PROFILE_DELETE: {
      webhid_raw_profile_mode = true;
      webhid_raw_management_mode = false;
      webhid_raw_profile_operation_response = true;
      if (bufsize < 3 || buffer[2] >= HID_MAPPING_PROFILE_COUNT) {
        webhid_raw_profile_status = WEBHID_RAW_STATUS_INVALID;
        return;
      }
      HidMappingProfileList list{};
      hidMappingProfileReadList(list);
      if (!(list.profiles[buffer[2]].flags & HID_MAPPING_PROFILE_VALID) ||
          !hidMappingProfileRemove(list.profiles[buffer[2]].key)) {
        webhid_raw_profile_status = 2;
      }
      webhid_raw_profile_index = buffer[2];
      return;
    }

    case WEBHID_RAW_PROFILE_CLEAR_ALL:
      if (!hidMappingProfileClear()) {
        webhid_raw_profile_status = WEBHID_RAW_STATUS_FAILED;
      }
      webhid_raw_profile_index = 0;
      webhid_raw_profile_mode = true;
      webhid_raw_management_mode = false;
      webhid_raw_profile_operation_response = true;
      return;

    default:
#if defined(ADAPT_CAP_ESP32_CONTROLLER_BRIDGE) && defined(ENABLE_INPUT_ESP32_SPI)
      if (buffer[1] >= WEBHID_RAW_MANAGEMENT_STATUS &&
          buffer[1] <= WEBHID_RAW_MANAGEMENT_IDENTIFY_PLAYER) {
        processModern2UsbManagement(buffer, bufsize);
        return;
      }
#endif
      webhid_raw_profile_mode = true;
      webhid_raw_management_mode = false;
      webhid_raw_profile_operation_response = true;
      webhid_raw_profile_status = WEBHID_RAW_STATUS_INVALID;
      return;
  }
}
