#pragma once

// Byte 24 of the 0xED remap GET report: remaps are per input+output pair
// and byte 23 names the output. Older firmware leaves both zero.
// Byte 4 of the 0xFF mode change: byte 2 is an output the user chose.
#define WEBHID_MODE_CHANGE_OUTPUT_CHOSEN 0x01

#define WEBHID_REMAP_PER_OUTPUT_MARKER 0xA5
// The remap report's 19-entry layout predates the D-pad rows. Firmware with
// D-pad rows puts this marker at byte 25 and Up, Down, Left, Right at 26-29,
// in both directions; a bulk write without the marker leaves them unchanged.
#define WEBHID_REMAP_DPAD_MARKER 0xD4
#define WEBHID_REMAP_DPAD_OFFSET 25

#include <stdint.h>

// Report IDs 0xE0-0xEF are reserved for WebHID configuration.
#define WEBHID_REPORT_DEVICE_INFO   0xE0
#define WEBHID_REPORT_COMMAND       0xE3
#define WEBHID_REPORT_KEY_STATUS    0xE4
#define WEBHID_REPORT_KEY_WRITE     0xE5
#define WEBHID_REPORT_KEY_CLEAR     0xE6
#define WEBHID_REPORT_INPUT_STATE   0xE7
#define WEBHID_REPORT_GPIO_STATE    0xE8
#define WEBHID_REPORT_RAW_DATA      0xE9
#define WEBHID_REPORT_SETTINGS      0xEA
#define WEBHID_REPORT_INPUT_MODE    0xEB
#define WEBHID_REPORT_TURBO         0xEC
#define WEBHID_REPORT_REMAP         0xED
#define WEBHID_REPORT_INPUT_HISTORY 0xEE
#define WEBHID_REPORT_STATS         0xEF

#define WEBHID_CMD_REBOOT           0x01
#define WEBHID_CMD_BOOTLOADER       0x02
#define WEBHID_CMD_FACTORY_RESET    0x04
#define WEBHID_CMD_RUMBLE_TEST      0x05

// RAW_DATA multiplex commands for 32-slot unknown-HID mapping profile
// import/export. A profile is transferred as two <=58-byte chunks.
#define WEBHID_RAW_LIVE_DATA          0x00
#define WEBHID_RAW_PROFILE_SELECT     0x01
#define WEBHID_RAW_PROFILE_WRITE      0x02
#define WEBHID_RAW_PROFILE_COMMIT     0x03
#define WEBHID_RAW_PROFILE_DELETE     0x04
#define WEBHID_RAW_PROFILE_CLEAR_ALL  0x05
#define WEBHID_RAW_PROFILE_RESPONSE   0x80

// MODERN2USB management also shares RAW_DATA so the public WebHID report range
// remains stable. Commands are stateful: send one command, then read RAW_DATA
// for WEBHID_RAW_MANAGEMENT_RESPONSE and the current authoritative state.
#define WEBHID_RAW_MANAGEMENT_STATUS             0x10
#define WEBHID_RAW_MANAGEMENT_PAIR               0x11
#define WEBHID_RAW_MANAGEMENT_CANCEL_PAIR        0x12
#define WEBHID_RAW_MANAGEMENT_SELECT_BOND        0x13
#define WEBHID_RAW_MANAGEMENT_RECONNECT_BOND     0x14
#define WEBHID_RAW_MANAGEMENT_FORGET_BOND        0x15
#define WEBHID_RAW_MANAGEMENT_FORGET_ALL_BONDS   0x16
#define WEBHID_RAW_MANAGEMENT_CONTROLLER_LEDS    0x17
#define WEBHID_RAW_MANAGEMENT_DEVICE_LEDS        0x18
#define WEBHID_RAW_MANAGEMENT_IDENTIFY_PLAYER    0x19
#define WEBHID_RAW_MANAGEMENT_RESPONSE           0x90

// Classic2USB input-specific, single-action Start shortcuts.
#define WEBHID_RAW_DIRECTION_HOTKEY_GET          0x20
#define WEBHID_RAW_DIRECTION_HOTKEY_SET          0x21
#define WEBHID_RAW_DIRECTION_HOTKEY_RESPONSE     0xA0
#define WEBHID_DIRECTION_FIXED_ASSIGNMENTS_INDEX 16

// RAW_DATA setting access by firmware SettingId, the ids serial SET uses.
// Values go through settingAccessWrite(), like the OLED menus and serial.
// Requests: LIST [magic, op]; GET [magic, op, id];
//           SET [magic, op, id, value int32 LE].
// Response [magic, 0xB0, op, status, id, ...]:
//   GET/SET: scope (0 global, 1 per input mode), type (0 u8, 1 s8, 2 u16),
//            stored, default, min, max, runtime (int32 LE each), flags,
//            WebHID input mode.
//   LIST:    setting count, supported bitmap, Quick Menu visible bitmap,
//            WebHID input mode.
#define WEBHID_RAW_SETTING_LIST                  0x30
#define WEBHID_RAW_SETTING_GET                   0x31
#define WEBHID_RAW_SETTING_SET                   0x32
#define WEBHID_RAW_SETTING_RESPONSE              0xB0
#define WEBHID_SETTING_SCOPE_INDEX                5
#define WEBHID_SETTING_TYPE_INDEX                 6
#define WEBHID_SETTING_STORED_INDEX               7
#define WEBHID_SETTING_DEFAULT_INDEX             11
#define WEBHID_SETTING_MIN_INDEX                 15
#define WEBHID_SETTING_MAX_INDEX                 19
#define WEBHID_SETTING_RUNTIME_INDEX             23
#define WEBHID_SETTING_FLAGS_INDEX               27
#define WEBHID_SETTING_MODE_INDEX                28
#define WEBHID_SETTING_FLAG_RUNTIME_KNOWN        0x01
#define WEBHID_SETTING_FLAG_REBOOT_REQUIRED      0x02
#define WEBHID_SETTING_LIST_COUNT_INDEX           5
#define WEBHID_SETTING_LIST_SUPPORTED_INDEX       6
#define WEBHID_SETTING_LIST_VISIBLE_INDEX        15
#define WEBHID_SETTING_LIST_MODE_INDEX           24
#define WEBHID_SETTING_LIST_BITMAP_BYTES          9

#define WEBHID_RAW_STATUS_OK           0
#define WEBHID_RAW_STATUS_UNSUPPORTED  1
#define WEBHID_RAW_STATUS_INVALID      2
#define WEBHID_RAW_STATUS_FAILED       3

#define WEBHID_MAGIC_BYTE           0xAD
#define WEBHID_REPORT_SIZE          63
#define WEBHID_SETTINGS_HOTKEY_FLAGS_INDEX 44
#define WEBHID_SETTINGS_MISTER_OUTPUT_SHIFT 5
#define WEBHID_SETTINGS_MISTER_OUTPUT_MASK  0xE0
#define WEBHID_SETTING_MISTER_OUTPUT        49
#define WEBHID_SETTINGS_GUNCON_X_INDEX 61
#define WEBHID_SETTINGS_GUNCON_Y_INDEX 62

struct __attribute__((packed)) WebHIDDeviceInfo {
  uint8_t magic;
  uint8_t protocol_version;
  uint8_t firmware_major;
  uint8_t firmware_minor;
  uint8_t firmware_patch;
  uint8_t device_type;
  uint8_t output_mode;
  uint8_t max_players;
  uint8_t config_size_lo;
  uint8_t config_size_hi;
  char controller_name[20];
  // Protocol v3: stable product-family ID; field position retained for
  // compatibility with older clients that treated it as a device serial.
  char device_serial[20];
  uint8_t debug_report_count_lo;
  uint8_t debug_report_count_hi;
  uint8_t debug_last_report_id;
  uint8_t debug_last_report_type;
  uint8_t reserved[8];
};

static_assert(sizeof(WebHIDDeviceInfo) <= WEBHID_REPORT_SIZE,
              "WebHIDDeviceInfo must fit in one feature report");
