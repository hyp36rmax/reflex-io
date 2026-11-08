/*******************************************************************************
 * USB Host Input Library for Reflex Adapt
 * Provides USB host functionality for reading HID gamepads and XInput controllers
 *
 * Uses Pico-PIO-USB for USB host on RP2040
 */

#ifndef INPUT_USB_H
#define INPUT_USB_H

#include <stdint.h>
#include <stdbool.h>

#include "../../../core/hid_mapping_profiles.h"
#include "../../../core/touch_contact.h"

struct HidDescriptorLayout;

constexpr uint8_t USB_HID_LEARNING_AXIS_COUNT = 12;

#ifndef MAX_USB_DEVICES
#define MAX_USB_DEVICES 4
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations for TinyUSB types
struct xinputh_interface_t;
struct denshah_interface_t;
struct horifs2h_interface_t;

// Device types
enum usb_device_type_t {
  DEVICE_TYPE_NONE = 0,
  DEVICE_TYPE_DIGITAL,
  DEVICE_TYPE_DUALSTICK,
  DEVICE_TYPE_XINPUT,
  DEVICE_TYPE_SWITCH,
  DEVICE_TYPE_PS3,
  DEVICE_TYPE_PS4,
  DEVICE_TYPE_PS5,
  DEVICE_TYPE_8BITDO,
  DEVICE_TYPE_XID,
};

// Axis precision
enum axis_precision_t {
  AXIS_8BITS = 8,
  AXIS_10BITS = 10,
  AXIS_12BITS = 12,
  AXIS_16BITS = 16,
};

// Analog axis with precision info
typedef struct {
  int32_t value;
  uint8_t precision;  // bits of precision (8, 10, 12, 16)
} usb_analog_axis_t;

// Configuration flags
typedef struct {
  uint8_t DEVICE_TYPE;
  uint8_t HAS_BTN_HOME : 1;
  uint8_t HAS_ANALOG_TRIGGERS : 1;
  uint8_t HAS_ANALOG_MAIN_BUTTONS : 1;
  uint8_t HAS_ANALOG_DPAD : 1;
  uint8_t FACE_BUTTONS_LAYOUT : 2;  // 4 or 6 button layout
  uint8_t reserved : 2;
} usb_input_config_t;

// USB input report structure
typedef struct {
  usb_input_config_t CONFIG;

  // Digital buttons
  uint8_t PAD_U : 1;
  uint8_t PAD_D : 1;
  uint8_t PAD_L : 1;
  uint8_t PAD_R : 1;
  uint8_t A : 1;
  uint8_t B : 1;
  uint8_t X : 1;
  uint8_t Y : 1;

  uint8_t L1 : 1;
  uint8_t R1 : 1;
  uint8_t L2 : 1;
  uint8_t R2 : 1;
  uint8_t L3 : 1;
  uint8_t R3 : 1;
  uint8_t SELECT : 1;
  uint8_t START : 1;

  uint8_t HOME : 1;
  uint8_t CAPTURE : 1;
  uint8_t reserved_btns : 6;

  // Analog sticks
  usb_analog_axis_t LX;
  usb_analog_axis_t LY;
  usb_analog_axis_t RX;
  usb_analog_axis_t RY;

  // Analog triggers
  usb_analog_axis_t ANALOG_L2;
  usb_analog_axis_t ANALOG_R2;

  // Analog face buttons (PS2/PS3 style)
  uint8_t ANALOG_A;
  uint8_t ANALOG_B;
  uint8_t ANALOG_X;
  uint8_t ANALOG_Y;
  uint8_t ANALOG_L1;
  uint8_t ANALOG_R1;

  // Analog dpad
  uint8_t ANALOG_PAD_U;
  uint8_t ANALOG_PAD_D;
  uint8_t ANALOG_PAD_L;
  uint8_t ANALOG_PAD_R;

  ControllerTouchContact touch[2];
} usb_input_report_t;

// USB device structure
typedef struct {
  bool connected;
  uint8_t dev_addr;
  uint8_t instance;
  uint8_t bluetooth_controller_type;
  // Physical topology is diagnostic only. Logical player ownership follows
  // the first valid controller report, independent of which receptacle is used.
  uint8_t topology_root_port;
  uint8_t topology_depth;
  uint16_t vid;
  uint16_t pid;
  // USB device release from the device descriptor. Keep this beside VID/PID
  // so native XUSB/XGIP/XID devices can be identified as precisely as HID.
  uint8_t display_identity;
  uint16_t bcd_device;
  uint32_t topology_key;
  uint32_t connection_order;
  // Increment whenever an attached generic-HID profile is changed in place.
  // The input poller uses this independently of report bytes so a newly saved
  // profile is published to the neutral frame without reconnecting.
  uint32_t mapping_revision;
  bool identity_valid;
  uint8_t identity_addr[6];
  uint32_t input_report_count;
  uint8_t last_input_report_len;
  uint8_t last_input_report[64];
  uint8_t switch_init_state;
  uint8_t switch_init_pending;
  uint16_t switch_init_attempt_count;
  uint16_t switch_init_fail_count;
  uint8_t switch_rumble_enabled;
  uint8_t switch_rumble_enable_pending;
  uint16_t switch_rumble_enable_attempt_count;
  uint16_t switch_rumble_enable_ack_count;
  uint16_t switch_rumble_enable_fail_count;
  uint16_t switch_rumble_submit_count;
  uint16_t switch_rumble_success_count;
  uint16_t switch_rumble_fail_count;
  usb_input_report_t report;
} usb_input_device_t;

// Stable read-only identity exposed to product features. `attached` becomes
// true at USB mount; `input_active` becomes true after the first valid input
// report. Keeping both states avoids startup UIs deadlocking on a neutral pad.
typedef struct {
  bool attached;
  bool input_active;
  uint16_t vid;
  uint16_t pid;
  uint16_t bcd_device;
#if defined(ADAPT_PRIMARY_EGRESS_DB15)
  // Changes on every physical mount, even when the replacement controller
  // has the same VID:PID. Product UIs use this to detect a new session.
  uint32_t connection_order;
#endif
  const char* display_name;
} usb_input_device_identity_t;

typedef struct {
  uint32_t device_mount_count;
  uint32_t device_umount_count;
  uint32_t hid_mount_count;
  uint32_t hid_umount_count;
  uint32_t report_count;
  uint32_t report_request_count;
  uint32_t report_request_success_count;
  uint32_t report_request_fail_count;
  uint32_t report_send_count;
  uint32_t report_send_success_count;
  uint32_t report_send_fail_count;
  uint32_t hid_recover_count;
  uint32_t stale_reap_count;
  uint32_t mount_replace_count;
  uint32_t mounted_scan_count;
  uint32_t xusb_mount_count;
  uint32_t xusb_umount_count;
  uint32_t xusb_report_count;
  uint32_t xgip_mount_count;
  uint32_t xgip_umount_count;
  uint32_t xgip_report_count;
  uint32_t ps4_rumble_submit_count;
  uint32_t ps4_rumble_success_count;
  uint32_t ps4_rumble_fail_count;
  uint32_t ps3_pair_submit_count;
  uint32_t ps3_pair_success_count;
  uint32_t ps3_pair_fail_count;
  uint32_t ps3_pair_busy_count;
  uint32_t ps3_pair_read_count;
  uint32_t ps3_pair_verify_count;
  uint32_t ps3_pair_verify_fail_count;
  uint8_t ps3_pair_readback[8];
  uint16_t last_vid;
  uint16_t last_pid;
  uint8_t last_dev_addr;
  uint8_t last_instance;
  uint8_t last_report_len;
  uint8_t last_hid_count;
  uint8_t last_report_request_ok;
  uint8_t last_report_send_ok;
  uint8_t last_hid_protocol;
  uint8_t last_ps4_rumble_left;
  uint8_t last_ps4_rumble_right;
  uint16_t last_hid_desc_len;
  uint8_t last_hid_desc_head[32];
  uint8_t last_report_head[64];
} usb_input_debug_status_t;

// Resettable measurement of reports actually delivered by the USB host stack.
// This deliberately does not claim to measure the host's 1 ms poll schedule:
// NAKs and unchanged/no-report intervals are not visible at this layer.
typedef struct {
  bool valid;
  uint32_t reset_generation;
  uint32_t report_count;
  uint32_t window_us;
  uint32_t last_report_age_us;
  uint32_t last_gap_us;
  uint32_t min_gap_us;
  uint32_t max_gap_us;
} usb_input_report_rate_t;

typedef struct {
  bool available;
  bool needs_mapping;
  bool database_mapping_candidate;
  uint8_t mapped_dpad_mask;
  uint8_t mapped_face_buttons;
  uint8_t mapping_layout;
  uint8_t mapping_profile_flags;
  uint8_t device_index;
  uint16_t vid;
  uint16_t pid;
  uint32_t buttons;
  uint32_t button_valid_mask;
  int32_t axes[USB_HID_LEARNING_AXIS_COUNT];
  uint16_t axis_valid_mask;
  uint16_t axis_negative_mask;
  uint16_t axis_positive_mask;
  uint8_t dpad_mask;
  uint8_t hat;
  uint8_t hat_present;
  uint8_t descriptor_button_count;
  uint8_t descriptor_axis_count;
  uint8_t descriptor_has_hat;
  uint8_t descriptor_dpad_mask;
  uint8_t report_id;
} usb_hid_learning_snapshot_t;

typedef struct {
  uint8_t device_index;
  uint32_t connection_order;
  HidMappingProfileKey key;
} usb_hid_mapping_target_t;

#ifdef ENABLE_HID_COMPAT_CAPTURE
enum usb_hid_capture_field_kind_t : uint8_t {
  USB_HID_CAPTURE_BUTTON = 0,
  USB_HID_CAPTURE_AXIS = 1,
  USB_HID_CAPTURE_DPAD = 2,
  USB_HID_CAPTURE_HAT = 3,
};

typedef struct {
  bool available;
  bool saved_mapping;
  bool needs_mapping;
  bool database_mapping_candidate;
  bool descriptor_truncated;
  uint8_t device_index;
  uint8_t interface_number;
  uint8_t report_id;
  uint8_t descriptor_button_count;
  uint8_t descriptor_axis_count;
  uint8_t descriptor_dpad_mask;
  uint8_t descriptor_has_hat;
  uint8_t last_report_length;
  uint16_t vid;
  uint16_t pid;
  uint16_t bcd_device;
  uint16_t descriptor_length;
  uint16_t quirk_flags;
  uint32_t descriptor_crc32;
  uint32_t report_sequence;
} usb_hid_capture_status_t;

typedef struct {
  uint16_t bit_offset;
  uint16_t usage_page;
  uint16_t usage;
  uint32_t application;
  uint32_t physical;
  int32_t logical_min;
  int32_t logical_max;
  uint8_t bit_size;
  uint8_t report_id;
} usb_hid_capture_field_t;
#endif

// Initialize USB host
void input_usb_setup(void);

// Poll USB host (call from main loop)
void input_usb_loop(void);

// Core1 USB-host service hook: recover already-mounted HID interfaces and keep
// TinyUSB host calls on the same core as USBHost.task().
void inputusb_scan_mounted_devices(void);

// Set rumble for a device
void input_usb_set_rumble(uint8_t device_index, uint8_t left, uint8_t right);
void input_usb_set_logical_player_for_device(uint8_t device_index,
                                             uint8_t logical_player);
// Queue the ESP32 host address into a wired DualShock 3. The control transfer
// is performed later on the USB-host core.
bool inputusb_queue_ds3_pairing(const uint8_t host_address[6]);
bool inputusb_has_wired_ds3();
bool inputusb_ds3_pairing_verified(const uint8_t host_address[6]);

// Get device by index
usb_input_device_t* get_usb_device(uint8_t device_index);
const char* inputusb_device_display_name(uint8_t device_index);
bool inputusb_device_identity(uint8_t device_index,
                              usb_input_device_identity_t* identity);
// Read the live normalized digital buttons directly from the USB-host decoder.
// This remains available while a product UI owns controller input and the
// normal controller-frame/output pipeline is intentionally suppressed.
bool inputusb_device_button_state(uint8_t device_index, uint32_t* buttons);

// Get number of connected USB devices (for debug)
uint8_t get_usb_device_count(void);

// Get USB input host debug counters.
usb_input_debug_status_t inputusb_debug_status(void);

// Reset and snapshot actual received-report timing without blocking the USB
// callback path. Timing is active only between reset and stop; snapshots are
// safe across the RP2040's two cores.
void inputusb_report_rate_reset(void);
void inputusb_report_rate_stop(void);
bool inputusb_report_rate_snapshot(uint8_t device_index,
                                   usb_input_report_rate_t* snapshot);

// Unknown/generic HID learning interface used by the OLED mapper. Profiles are
// keyed by VID/PID/bcdDevice plus report-descriptor identity and persist in one
// of 32 A/B-protected slots.
bool inputusb_generic_mapping_pending(uint8_t* device_index);
// Accept the current saved/exact/SDL/default profile without persistence.
// Used when no interactive OLED mapper is available.
bool inputusb_generic_accept_current_mapping(uint8_t device_index);
bool inputusb_generic_learning_snapshot(uint8_t device_index,
                                        usb_hid_learning_snapshot_t* snapshot);
bool inputusb_generic_save_mapping(uint8_t device_index,
                                   const uint8_t button_sources[HID_MAPPING_BUTTON_COUNT],
                                   const uint8_t dpad_button_sources[4],
                                   const uint8_t axis_sources[HID_MAPPING_AXIS_COUNT],
                                   const uint8_t axis_flags[HID_MAPPING_AXIS_COUNT],
                                   const uint16_t axis_centers[HID_MAPPING_AXIS_COUNT],
                                   uint8_t classified_layout,
                                   bool ignore_future_prompt,
                                   bool database_confirmed,
                                   const HidMappingProfile* base_profile,
                                   const HidMappingProfile* direction_profile = nullptr,
                                   // Set when analog L2/R2 has no trigger-axis pair; nothing is saved.
                                   bool* missing_trigger_axes = nullptr);
bool inputusb_generic_save_automatic_mapping(uint8_t device_index);
bool inputusb_generic_has_saved_mapping(uint8_t device_index);
bool inputusb_generic_can_map(uint8_t device_index);
bool inputusb_generic_copy_descriptor_layout(uint8_t device_index,
                                             HidDescriptorLayout* layout);
bool inputusb_generic_copy_active_mapping(uint8_t device_index,
                                          HidMappingProfile* profile);
uint8_t inputusb_generic_mapping_layout(uint8_t device_index,
                                        bool* classified = nullptr);
bool inputusb_generic_capture_mapping(
    uint8_t device_index, usb_hid_mapping_target_t* target);
bool inputusb_generic_capture_saved_mapping(
    uint8_t device_index, usb_hid_mapping_target_t* target);
bool inputusb_generic_confirm_database_mapping(uint8_t device_index);
bool inputusb_generic_prepare_remap(
    const usb_hid_mapping_target_t* target);
bool inputusb_generic_reset_mapping(
    const usb_hid_mapping_target_t* target);

#ifdef ENABLE_HID_COMPAT_CAPTURE
bool inputusb_hid_capture_status(uint8_t device_index,
                                 usb_hid_capture_status_t* status);
uint16_t inputusb_hid_capture_descriptor(uint8_t device_index,
                                         uint16_t offset, uint8_t* output,
                                         uint16_t capacity);
bool inputusb_hid_capture_field(uint8_t device_index,
                                usb_hid_capture_field_kind_t kind,
                                uint8_t index,
                                usb_hid_capture_field_t* field);
bool inputusb_hid_capture_profile(uint8_t device_index,
                                  HidMappingProfile* profile);
bool inputusb_hid_capture_report(uint8_t device_index, uint8_t* output,
                                 uint8_t capacity, uint8_t* length,
                                 uint32_t* sequence);
bool inputusb_hid_capture_neutral(uint8_t device_index,
                                  usb_input_report_t* report);
#endif

// Convert analog value to target precision
int32_t inputusb_convertAnalogPrecision(usb_analog_axis_t axis, uint8_t target_precision);

// TinyUSB callbacks (called from Input_UsbHost.h callback wrappers)
void inputusb_device_mount_cb(uint8_t daddr);
void inputusb_device_umount_cb(uint8_t daddr);

void inputusb_hid_mount_cb(uint8_t dev_addr, uint8_t idx, uint8_t const* report_desc, uint16_t desc_len);
void inputusb_hid_umount_cb(uint8_t dev_addr, uint8_t idx);
void inputusb_hid_report_received_cb(uint8_t dev_addr, uint8_t idx, uint8_t const* report, uint16_t len);
void inputusb_hid_report_sent_cb(uint8_t dev_addr, uint8_t idx,
                                 uint8_t const* report, uint16_t len);
void inputusb_hid_set_report_complete_cb(uint8_t dev_addr, uint8_t idx,
                                         uint8_t report_id,
                                         uint8_t report_type, uint16_t len);
void inputusb_hid_get_report_response(uint8_t dev_addr, uint8_t idx,
                                      uint8_t report_id,
                                      uint8_t const* report, uint16_t len);
void inputusb_switch2_rumble_complete_cb(uint8_t dev_addr, uint8_t instance,
                                         uint8_t left, uint8_t right,
                                         bool success);

// Native Xbox 360/XUSB callbacks used by the Reflex TinyUSB app driver.
void inputusb_xusb_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t subtype);
void inputusb_xusb_umount_cb(uint8_t dev_addr, uint8_t instance);
void inputusb_xusb_report_received_cb(uint8_t dev_addr, uint8_t instance,
                                     uint8_t const* report, uint16_t len);
void inputusb_xusb_wireless_report_received_cb(
    uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len);

// Native Xbox One/Series GIP callbacks used by the Reflex TinyUSB app driver.
void inputusb_xgip_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t subtype);
void inputusb_xgip_umount_cb(uint8_t dev_addr, uint8_t instance);
void inputusb_xgip_report_received_cb(uint8_t dev_addr, uint8_t instance,
                                     uint8_t const* report, uint16_t len);
void inputusb_xgip_rumble_complete_cb(uint8_t dev_addr, uint8_t instance,
                                     bool success);

// Native Original Xbox/XID callbacks used by the Reflex TinyUSB app driver.
void inputusb_xid_mount_cb(uint8_t dev_addr, uint8_t instance);
void inputusb_xid_umount_cb(uint8_t dev_addr, uint8_t instance);
void inputusb_xid_report_received_cb(uint8_t dev_addr, uint8_t instance,
                                    uint8_t const* report, uint16_t len);

void inputusb_xinput_mount_cb(uint8_t dev_addr, uint8_t instance, const struct xinputh_interface_t *xinput_itf);
void inputusb_xinput_umount_cb(uint8_t dev_addr, uint8_t instance);
void inputusb_xinput_report_received_cb(uint8_t dev_addr, uint8_t instance, struct xinputh_interface_t const* xid_itf, uint16_t len);

void inputusb_densha_mount_cb(uint8_t dev_addr, uint8_t instance, const struct denshah_interface_t *densha_itf);
void inputusb_densha_umount_cb(uint8_t dev_addr, uint8_t instance);
void inputusb_densha_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len);

void inputusb_horifs2_mount_cb(uint8_t dev_addr, uint8_t instance, const struct horifs2h_interface_t *horifs2_itf);
void inputusb_horifs2_umount_cb(uint8_t dev_addr, uint8_t instance);
void inputusb_horifs2_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif // INPUT_USB_H
