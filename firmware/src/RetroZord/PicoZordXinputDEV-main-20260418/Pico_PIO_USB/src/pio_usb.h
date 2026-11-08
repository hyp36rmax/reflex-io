
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pio_usb_configuration.h"
#include "usb_definitions.h"

#ifdef __cplusplus
 extern "C" {
#endif

// Host functions
usb_device_t *pio_usb_host_init(const pio_usb_configuration_t *c);
int pio_usb_host_add_port(uint8_t pin_dp, PIO_USB_PINOUT pinout);
void pio_usb_host_task(void);
void pio_usb_host_stop(void);
void pio_usb_host_restart(void);
uint32_t pio_usb_host_get_frame_number(void);

// Actual controller-facing interrupt-IN transactions issued by PIO-USB. These
// diagnostics count IN tokens at the bus transaction boundary, including NAKs;
// they are deliberately separate from TinyUSB's delivered-report callbacks.
#define PIO_USB_SUPPORTS_HOST_POLL_DIAGNOSTICS
#define PIO_USB_HOST_POLL_GAP_HISTOGRAM_BINS 6

typedef struct {
  bool valid;
  uint8_t root_idx;
  uint8_t dev_addr;
  uint8_t ep_addr;
  uint8_t interval;
  uint32_t attempts;
  uint32_t data_acks;
  uint32_t naks;
  uint32_t stalls;
  uint32_t toggle_mismatches;
  uint32_t errors;
  uint32_t error_no_rx_eop;
  uint32_t error_bad_sync;
  uint32_t error_data_integrity;
  uint32_t error_pid;
  uint32_t error_other;
  uint32_t first_attempt_us;
  uint32_t last_attempt_us;
  uint64_t gap_sum_us;
  uint32_t last_gap_us;
  uint32_t min_gap_us;
  uint32_t max_gap_us;
  uint32_t gap_histogram[PIO_USB_HOST_POLL_GAP_HISTOGRAM_BINS];
} pio_usb_host_poll_diag_t;

// Reset is epoch-based so management code never writes counters owned by the
// 1 ms PIO host timer. Snapshot uses a bounded sequence check and returns false
// if the endpoint is inactive or a coherent cross-core copy is unavailable.
void pio_usb_host_poll_diag_reset(void);
bool pio_usb_host_poll_diag_snapshot(uint8_t endpoint_index,
                                     pio_usb_host_poll_diag_t *out);
uint32_t pio_usb_host_poll_diag_lifecycle_drops(void);

// MODERN2USB may phase-align the controller-facing 1 kHz scheduler to the
// upstream USB host. A request changes exactly one frame interval and then
// automatically returns to 1000 us; it never stops the host or disconnects
// downstream devices.
#define PIO_USB_SUPPORTS_HOST_POLL_ALIGNMENT
#define PIO_USB_HOST_POLL_ALIGNMENT_MAX_ADJUST_US 125

typedef struct {
  uint32_t last_frame_us;
  uint32_t requests;
  uint32_t applied;
  uint32_t busy_rejects;
  int16_t last_adjust_us;
  int16_t pending_adjust_us;
} pio_usb_host_poll_alignment_diag_t;

bool pio_usb_host_request_phase_adjustment(int16_t adjust_us);
void pio_usb_host_poll_alignment_snapshot(
    pio_usb_host_poll_alignment_diag_t *out);


// Call this every 1ms when skip_alarm_pool is true.
void pio_usb_host_frame(void);

// Device functions
usb_device_t *pio_usb_device_init(const pio_usb_configuration_t *c,
                                  const usb_descriptor_buffers_t *buffers);
void pio_usb_device_task(void);

// Common functions
endpoint_t *pio_usb_get_endpoint(usb_device_t *device, uint8_t idx);
int pio_usb_get_in_data(endpoint_t *ep, uint8_t *buffer, uint8_t len);
int pio_usb_set_out_data(endpoint_t *ep, const uint8_t *buffer, uint8_t len);

// Interval override
// From https://github.com/jfedor2/Pico-PIO-USB/commit/2c8eb1862e2cf10d40d3aa7acb0c37b1f4336457
//void set_interval_override(uint8_t interval);
//uint8_t get_interval_override();

#ifdef __cplusplus
 }
#endif
