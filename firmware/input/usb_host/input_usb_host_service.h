#pragma once

#include <Arduino.h>
#include <stdint.h>

struct UsbHostServiceStatus {
  bool available;
  bool started;
  bool start_requested;
  bool tuh_inited;
  bool tuh_mounted_addr1;
  bool tuh_ready_addr1;
  bool addr1_vid_pid_valid;
  uint16_t addr1_vid;
  uint16_t addr1_pid;
  bool addr1_desc_valid;
  uint8_t addr1_class;
  uint8_t addr1_subclass;
  uint8_t addr1_protocol;
  uint8_t addr1_ep0_size;
  uint8_t addr1_config_count;
  uint8_t addr1_manufacturer_index;
  uint8_t addr1_product_index;
  uint8_t addr1_serial_index;
  uint16_t addr1_bcd_usb;
  uint16_t addr1_bcd_device;
  uint8_t hid_total_count;
  uint8_t hid_addr1_count;
  uint8_t rhport;
  uint8_t dp_pin;
  uint8_t dm_pin;
  uint8_t pinout;
  uint8_t raw_dp_level;
  uint8_t raw_dm_level;
  uint8_t physical_dp_level;
  uint8_t physical_dm_level;
  uint8_t pio_line_state;
  bool root_initialized;
  bool root_connected;
  bool root_suspended;
  bool root_fullspeed;
  uint8_t root_event;
  uint32_t root_ints;
  uint32_t root_ep_complete;
  uint32_t root_ep_error;
  uint32_t root_ep_stalled;
  uint8_t root_failed_count;
  bool root_device_present;
  bool root_device_connected;
  bool root_device_enumerated;
  bool root_device_fullspeed;
  uint8_t root_device_event;
  uint8_t root_device_address;
  uint16_t root_device_vid;
  uint16_t root_device_pid;
  uint8_t attach_kick_count;
  uint32_t attach_kick_age_ms;
  uint32_t task_count;
  uint32_t core1_loop_count;
  uint32_t setup_count;
  uint8_t setup_phase;
  uint8_t tx_pio;
  uint8_t tx_sm;
  uint8_t tx_dma;
  uint8_t rx_pio;
  uint8_t rx_sm;
  uint8_t eop_sm;
};

enum { USB_HOST_INTERFACE_SNAPSHOT_COUNT = 6 };

struct UsbHostInterfaceSnapshot {
  uint8_t dev_addr;
  uint8_t number;
  uint8_t alt;
  uint8_t cls;
  uint8_t subclass;
  uint8_t protocol;
  uint8_t endpoint_count;
};

struct UsbHostCallbackStatus {
  uint32_t device_mount_count;
  uint32_t device_umount_count;
  uint32_t config_desc_count;
  uint32_t hid_mount_count;
  uint32_t hid_umount_count;
  uint32_t hid_report_count;
  uint32_t xinput_mount_count;
  uint32_t xinput_report_count;
  uint32_t bluetooth_hci_detect_count;
  uint16_t last_vid;
  uint16_t last_pid;
  uint16_t last_hid_desc_len;
  uint16_t bluetooth_hci_vid;
  uint16_t bluetooth_hci_pid;
  uint8_t last_dev_addr;
  uint8_t last_instance;
  uint8_t last_report_len;
  uint8_t last_config_index;
  uint8_t last_itf_number;
  uint8_t last_itf_alt;
  uint8_t last_itf_class;
  uint8_t last_itf_subclass;
  uint8_t last_itf_protocol;
  uint8_t bluetooth_hci_addr;
  uint8_t interface_snapshot_count;
  UsbHostInterfaceSnapshot interfaces[USB_HOST_INTERFACE_SNAPSHOT_COUNT];
};

struct UsbHostHubPortRecoveryStatus {
  bool supported;
  bool requested;
  bool automatic;
  bool port_connected;
  uint8_t phase;
  uint8_t last_result;
  uint8_t hub_addr;
  uint8_t port;
  uint8_t automatic_attempts;
  uint32_t request_count;
  uint32_t success_count;
  uint32_t failure_count;
  uint16_t port_status_bits;
  uint16_t port_change_bits;
  uint32_t last_request_age_ms;
};

void usb_host_service_setup();
void usb_host_service_task();
void usb_host_service_request_start();
bool usb_host_service_force_root_reset();
bool usb_host_service_force_hub_port_reset(uint8_t port);
bool usb_host_service_hub_recovery_active();
bool usb_host_service_start_requested();
void usb_host_service_core1_task();
bool usb_host_service_ready();
bool usb_host_service_quiesce_for_flash(uint32_t timeout_ms);
void usb_host_service_release_flash_quiesce();
bool usb_host_service_flash_quiesced();
UsbHostServiceStatus usb_host_service_status();
UsbHostCallbackStatus usb_host_callback_status();
UsbHostHubPortRecoveryStatus usb_host_hub_port_recovery_status();
void usb_host_service_write_compact_status(Print& out);
void usb_host_service_reset_report_rate();
void usb_host_service_write_report_rate(Print& out,
                                        bool stop_measurement = true);
