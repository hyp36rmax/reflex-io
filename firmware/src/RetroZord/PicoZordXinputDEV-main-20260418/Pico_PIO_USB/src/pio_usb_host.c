/**
 * Copyright (c) 2021 sekigon-gonnoc
 *                    Ha Thach (thach@tinyusb.org)
 */

#pragma GCC push_options
#pragma GCC optimize("-O3")

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hardware/sync.h"

#include "pio_usb.h"
#include "pio_usb_ll.h"
#include "usb_crc.h"
#include "usb_rx.pio.h"
#include "usb_tx.pio.h"

static alarm_pool_t *_alarm_pool = NULL;
static repeating_timer_t sof_rt;
// The sof_count may be incremented and then read on different cores.
static volatile uint32_t sof_count = 0;
static bool timer_active;

static volatile bool cancel_timer_flag;
static volatile bool start_timer_flag;
static __unused uint32_t int_stat;

static bool sof_timer(repeating_timer_t *_rt);

extern volatile uint8_t tusb_interval_override;
volatile uint8_t tusb_interval_override __attribute__((weak)) = 0;
typedef enum {
  PIO_USB_POLL_DATA_ACK,
  PIO_USB_POLL_NAK,
  PIO_USB_POLL_STALL,
  PIO_USB_POLL_TOGGLE_MISMATCH,
  PIO_USB_POLL_ERROR_NO_RX_EOP,
  PIO_USB_POLL_ERROR_BAD_SYNC,
  PIO_USB_POLL_ERROR_DATA_INTEGRITY,
  PIO_USB_POLL_ERROR_PID,
  PIO_USB_POLL_ERROR_OTHER,
} pio_usb_poll_outcome_t;

typedef struct {
  volatile uint32_t sequence;
  volatile uint32_t epoch;
  volatile uint32_t allocation_generation;
  volatile pio_usb_host_poll_diag_t value;
} pio_usb_host_poll_diag_internal_t;

static pio_usb_host_poll_diag_internal_t
    poll_diag[PIO_USB_EP_POOL_CNT];
static volatile uint32_t poll_diag_epoch = 1;
static volatile uint32_t
    poll_diag_allocation_generation[PIO_USB_EP_POOL_CNT];
static volatile uint32_t poll_diag_lifecycle_epoch = 1;
static volatile uint32_t poll_diag_lifecycle_drop_count;
static volatile uint32_t poll_alignment_last_frame_us;
static volatile int32_t poll_alignment_pending_adjust_us;
static volatile uint32_t poll_alignment_requests;
static volatile uint32_t poll_alignment_applied;
static volatile uint32_t poll_alignment_busy_rejects;
static volatile int32_t poll_alignment_last_adjust_us;

static __always_inline int poll_diag_endpoint_index(const endpoint_t *ep) {
  const int index = (int)(ep - pio_usb_ep_pool);
  return (index >= 0 && index < PIO_USB_EP_POOL_CNT) ? index : -1;
}

static __always_inline void poll_diag_saturating_increment(
    volatile uint32_t *value) {
  if (*value != UINT32_MAX) {
    ++(*value);
  }
}

static __always_inline void poll_diag_note_lifecycle_drop(void) {
  const uint32_t epoch =
      __atomic_load_n(&poll_diag_epoch, __ATOMIC_ACQUIRE);
  if (poll_diag_lifecycle_epoch != epoch) {
    poll_diag_lifecycle_epoch = epoch;
    poll_diag_lifecycle_drop_count = 0;
  }
  poll_diag_saturating_increment(&poll_diag_lifecycle_drop_count);
}

static __always_inline uint8_t poll_diag_gap_bin(uint32_t gap_us) {
  if (gap_us <= 750) return 0;
  if (gap_us <= 1250) return 1;
  if (gap_us <= 1750) return 2;
  if (gap_us <= 2500) return 3;
  if (gap_us <= 5000) return 4;
  return 5;
}

static __always_inline void poll_diag_reset_value(
    pio_usb_host_poll_diag_internal_t *slot, uint32_t epoch,
    uint32_t allocation_generation, const endpoint_t *ep) {
  slot->epoch = epoch;
  slot->allocation_generation = allocation_generation;
  slot->value.valid = true;
  slot->value.root_idx = ep->root_idx;
  slot->value.dev_addr = ep->dev_addr;
  slot->value.ep_addr = ep->ep_num;
  slot->value.interval = ep->interval;
  slot->value.attempts = 0;
  slot->value.data_acks = 0;
  slot->value.naks = 0;
  slot->value.stalls = 0;
  slot->value.toggle_mismatches = 0;
  slot->value.errors = 0;
  slot->value.first_attempt_us = 0;
  slot->value.last_attempt_us = 0;
  slot->value.gap_sum_us = 0;
  slot->value.error_no_rx_eop = 0;
  slot->value.error_bad_sync = 0;
  slot->value.error_data_integrity = 0;
  slot->value.error_pid = 0;
  slot->value.error_other = 0;
  slot->value.last_gap_us = 0;
  slot->value.min_gap_us = 0;
  slot->value.max_gap_us = 0;
  for (uint8_t i = 0; i < PIO_USB_HOST_POLL_GAP_HISTOGRAM_BINS; ++i) {
    slot->value.gap_histogram[i] = 0;
  }
}

static void __no_inline_not_in_flash_func(poll_diag_record)(
    endpoint_t *ep, uint32_t attempt_us, uint32_t allocation_generation,
    pio_usb_poll_outcome_t outcome) {
  const int index = poll_diag_endpoint_index(ep);
  if (index < 0) {
    poll_diag_note_lifecycle_drop();
    return;
  }
  if (((ep->attr & 0x03) != EP_ATTR_INTERRUPT) ||
      !(ep->ep_num & EP_IN)) {
    // Control, bulk, and OUT transactions share usb_in_transaction() but are
    // outside this interrupt-IN cadence metric. They are not lifecycle loss.
    return;
  }
  if (__atomic_load_n(&poll_diag_allocation_generation[index],
                      __ATOMIC_ACQUIRE) != allocation_generation) {
    poll_diag_note_lifecycle_drop();
    return;
  }

  const uint32_t epoch =
      __atomic_load_n(&poll_diag_epoch, __ATOMIC_ACQUIRE);
  pio_usb_host_poll_diag_internal_t *slot = &poll_diag[index];
  // The 1 ms timer is the sole writer. Release stores keep the seqlock
  // entirely in RAM; 32-bit fetch-add pulls a flash-backed libatomic helper.
  const uint32_t write_sequence = slot->sequence & ~1u;
  __atomic_store_n(&slot->sequence, write_sequence + 1u, __ATOMIC_RELEASE);

  if (slot->epoch != epoch ||
      slot->allocation_generation != allocation_generation) {
    poll_diag_reset_value(slot, epoch, allocation_generation, ep);
  }

  if (slot->value.attempts == 0) {
    slot->value.first_attempt_us = attempt_us;
  } else {
    const uint32_t gap_us = attempt_us - slot->value.last_attempt_us;
    slot->value.last_gap_us = gap_us;
    if (slot->value.min_gap_us == 0 || gap_us < slot->value.min_gap_us) {
      slot->value.min_gap_us = gap_us;
    }
    if (gap_us > slot->value.max_gap_us) {
      slot->value.max_gap_us = gap_us;
    }
    if (UINT64_MAX - slot->value.gap_sum_us < gap_us) {
      slot->value.gap_sum_us = UINT64_MAX;
    } else {
      slot->value.gap_sum_us += gap_us;
    }
    poll_diag_saturating_increment(
        &slot->value.gap_histogram[poll_diag_gap_bin(gap_us)]);
  }
  slot->value.last_attempt_us = attempt_us;
  poll_diag_saturating_increment(&slot->value.attempts);

  switch (outcome) {
    case PIO_USB_POLL_DATA_ACK:
      poll_diag_saturating_increment(&slot->value.data_acks);
      break;
    case PIO_USB_POLL_NAK:
      poll_diag_saturating_increment(&slot->value.naks);
      break;
    case PIO_USB_POLL_STALL:
      poll_diag_saturating_increment(&slot->value.stalls);
      break;
    case PIO_USB_POLL_TOGGLE_MISMATCH:
      poll_diag_saturating_increment(&slot->value.toggle_mismatches);
      break;
    case PIO_USB_POLL_ERROR_NO_RX_EOP:
      poll_diag_saturating_increment(&slot->value.errors);
      poll_diag_saturating_increment(&slot->value.error_no_rx_eop);
      break;
    case PIO_USB_POLL_ERROR_BAD_SYNC:
      poll_diag_saturating_increment(&slot->value.errors);
      poll_diag_saturating_increment(&slot->value.error_bad_sync);
      break;
    case PIO_USB_POLL_ERROR_DATA_INTEGRITY:
      poll_diag_saturating_increment(&slot->value.errors);
      poll_diag_saturating_increment(&slot->value.error_data_integrity);
      break;
    case PIO_USB_POLL_ERROR_PID:
      poll_diag_saturating_increment(&slot->value.errors);
      poll_diag_saturating_increment(&slot->value.error_pid);
      break;
    case PIO_USB_POLL_ERROR_OTHER:
    default:
      poll_diag_saturating_increment(&slot->value.errors);
      poll_diag_saturating_increment(&slot->value.error_other);
      break;
  }

  __atomic_store_n(&slot->sequence, write_sequence + 2u, __ATOMIC_RELEASE);
}

static void poll_diag_mark_endpoint_reallocated(endpoint_t *ep) {
  const int index = poll_diag_endpoint_index(ep);
  if (index >= 0) {
    __atomic_add_fetch(&poll_diag_allocation_generation[index], 1,
                       __ATOMIC_ACQ_REL);
  }
}

void pio_usb_host_poll_diag_reset(void) {
  __atomic_add_fetch(&poll_diag_epoch, 1, __ATOMIC_ACQ_REL);
}

uint32_t pio_usb_host_poll_diag_lifecycle_drops(void) {
  const uint32_t epoch =
      __atomic_load_n(&poll_diag_epoch, __ATOMIC_ACQUIRE);
  if (__atomic_load_n(&poll_diag_lifecycle_epoch, __ATOMIC_ACQUIRE) != epoch) {
    return 0;
  }
  return __atomic_load_n(&poll_diag_lifecycle_drop_count, __ATOMIC_ACQUIRE);
}

bool pio_usb_host_request_phase_adjustment(int16_t adjust_us) {
  if (adjust_us == 0 ||
      adjust_us < -PIO_USB_HOST_POLL_ALIGNMENT_MAX_ADJUST_US ||
      adjust_us > PIO_USB_HOST_POLL_ALIGNMENT_MAX_ADJUST_US) {
    return false;
  }

  int32_t expected = 0;
  if (!__atomic_compare_exchange_n(&poll_alignment_pending_adjust_us,
                                   &expected, adjust_us, false,
                                   __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
    __atomic_add_fetch(&poll_alignment_busy_rejects, 1, __ATOMIC_RELAXED);
    return false;
  }
  __atomic_add_fetch(&poll_alignment_requests, 1, __ATOMIC_RELAXED);
  return true;
}

void pio_usb_host_poll_alignment_snapshot(
    pio_usb_host_poll_alignment_diag_t *out) {
  if (!out) return;
  out->last_frame_us =
      __atomic_load_n(&poll_alignment_last_frame_us, __ATOMIC_ACQUIRE);
  out->requests =
      __atomic_load_n(&poll_alignment_requests, __ATOMIC_ACQUIRE);
  out->applied =
      __atomic_load_n(&poll_alignment_applied, __ATOMIC_ACQUIRE);
  out->busy_rejects =
      __atomic_load_n(&poll_alignment_busy_rejects, __ATOMIC_ACQUIRE);
  out->last_adjust_us = (int16_t)__atomic_load_n(
      &poll_alignment_last_adjust_us, __ATOMIC_ACQUIRE);
  out->pending_adjust_us = (int16_t)__atomic_load_n(
      &poll_alignment_pending_adjust_us, __ATOMIC_ACQUIRE);
}

static void poll_diag_copy_value(
    const volatile pio_usb_host_poll_diag_t *source,
    pio_usb_host_poll_diag_t *destination) {
  destination->valid = source->valid;
  destination->root_idx = source->root_idx;
  destination->dev_addr = source->dev_addr;
  destination->ep_addr = source->ep_addr;
  destination->interval = source->interval;
  destination->attempts = source->attempts;
  destination->data_acks = source->data_acks;
  destination->naks = source->naks;
  destination->stalls = source->stalls;
  destination->toggle_mismatches = source->toggle_mismatches;
  destination->errors = source->errors;
  destination->error_no_rx_eop = source->error_no_rx_eop;
  destination->error_bad_sync = source->error_bad_sync;
  destination->error_data_integrity = source->error_data_integrity;
  destination->error_pid = source->error_pid;
  destination->error_other = source->error_other;
  destination->first_attempt_us = source->first_attempt_us;
  destination->last_attempt_us = source->last_attempt_us;
  destination->gap_sum_us = source->gap_sum_us;
  destination->last_gap_us = source->last_gap_us;
  destination->min_gap_us = source->min_gap_us;
  destination->max_gap_us = source->max_gap_us;
  for (uint8_t i = 0; i < PIO_USB_HOST_POLL_GAP_HISTOGRAM_BINS; ++i) {
    destination->gap_histogram[i] = source->gap_histogram[i];
  }
}

bool pio_usb_host_poll_diag_snapshot(uint8_t endpoint_index,
                                     pio_usb_host_poll_diag_t *out) {
  if (!out || endpoint_index >= PIO_USB_EP_POOL_CNT) {
    return false;
  }
  memset(out, 0, sizeof(*out));

  pio_usb_host_poll_diag_internal_t *slot = &poll_diag[endpoint_index];
  endpoint_t *ep = PIO_USB_ENDPOINT(endpoint_index);
  for (uint8_t attempt = 0; attempt < 4; ++attempt) {
    const uint32_t sequence_before =
        __atomic_load_n(&slot->sequence, __ATOMIC_ACQUIRE);
    if (sequence_before & 1u) {
      continue;
    }
    const uint32_t epoch_before =
        __atomic_load_n(&poll_diag_epoch, __ATOMIC_ACQUIRE);
    const uint32_t allocation_before = __atomic_load_n(
        &poll_diag_allocation_generation[endpoint_index], __ATOMIC_ACQUIRE);
    const uint32_t slot_epoch = slot->epoch;
    const uint32_t slot_allocation = slot->allocation_generation;
    pio_usb_host_poll_diag_t candidate = {0};
    poll_diag_copy_value(&slot->value, &candidate);

    const uint16_t size = ep->size;
    const uint8_t attr = ep->attr;
    const uint8_t root_idx = ep->root_idx;
    const uint8_t dev_addr = ep->dev_addr;
    const uint8_t ep_addr = ep->ep_num;
    const uint8_t interval = ep->interval;

    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    const uint32_t sequence_after =
        __atomic_load_n(&slot->sequence, __ATOMIC_ACQUIRE);
    const uint32_t epoch_after =
        __atomic_load_n(&poll_diag_epoch, __ATOMIC_ACQUIRE);
    const uint32_t allocation_after = __atomic_load_n(
        &poll_diag_allocation_generation[endpoint_index], __ATOMIC_ACQUIRE);
    if (sequence_before != sequence_after || (sequence_after & 1u) ||
        epoch_before != epoch_after || allocation_before != allocation_after) {
      continue;
    }

    const bool active_interrupt_in =
        size != 0 && ((attr & 0x03) == EP_ATTR_INTERRUPT) &&
        (ep_addr & EP_IN);
    if (!active_interrupt_in) {
      return false;
    }

    if (slot_epoch == epoch_after && slot_allocation == allocation_after &&
        candidate.valid && candidate.root_idx == root_idx &&
        candidate.dev_addr == dev_addr && candidate.ep_addr == ep_addr) {
      *out = candidate;
    }
    out->root_idx = root_idx;
    out->dev_addr = dev_addr;
    out->ep_addr = ep_addr;
    out->interval = interval;
    out->valid = out->valid && out->attempts != 0;
    return true;
  }
  return false;
}

//--------------------------------------------------------------------+
// Application API
//--------------------------------------------------------------------+

static void start_timer(alarm_pool_t *alarm_pool) {
  if (timer_active) {
    return;
  }

  if (alarm_pool != NULL) {
    alarm_pool_add_repeating_timer_us(alarm_pool, -1000, sof_timer, NULL,
                                      &sof_rt);
  }

  timer_active = true;
}

static __unused void stop_timer(void) {
  cancel_repeating_timer(&sof_rt);
  timer_active = false;
}

usb_device_t *pio_usb_host_init(const pio_usb_configuration_t *c) {
  pio_port_t *pp = PIO_USB_PIO_PORT(0);
  root_port_t *root = PIO_USB_ROOT_PORT(0);

  pio_usb_bus_init(pp, c, root);
  root->mode = PIO_USB_MODE_HOST;

  float const cpu_freq = (float)clock_get_hz(clk_sys);
  pio_calculate_clkdiv_from_float(cpu_freq / 48000000,
                                  &pp->clk_div_fs_tx.div_int,
                                  &pp->clk_div_fs_tx.div_frac);
  pio_calculate_clkdiv_from_float(cpu_freq / 6000000,
                                  &pp->clk_div_ls_tx.div_int,
                                  &pp->clk_div_ls_tx.div_frac);

  pio_calculate_clkdiv_from_float(cpu_freq / 96000000,
                                  &pp->clk_div_fs_rx.div_int,
                                  &pp->clk_div_fs_rx.div_frac);
  pio_calculate_clkdiv_from_float(cpu_freq / 12000000,
                                  &pp->clk_div_ls_rx.div_int,
                                  &pp->clk_div_ls_rx.div_frac);

  if (!c->skip_alarm_pool) {
    _alarm_pool = c->alarm_pool;
    if (!_alarm_pool) {
      _alarm_pool = alarm_pool_create(2, 1);
    }
  }
  start_timer(_alarm_pool);

  return &pio_usb_device[0];
}

void pio_usb_host_stop(void) {
  cancel_timer_flag = true;
  while (cancel_timer_flag) {
    continue;
  }
}

void pio_usb_host_restart(void) {
  start_timer_flag = true;
  while (start_timer_flag) {
    continue;
  }
}

//--------------------------------------------------------------------+
// Bus functions
//--------------------------------------------------------------------+

static void __no_inline_not_in_flash_func(override_pio_program)(PIO pio, const pio_program_t* program, uint offset) {
    for (uint i = 0; i < program->length; ++i) {
      uint16_t instr = program->instructions[i];
      pio->instr_mem[offset + i] =
          pio_instr_bits_jmp != _pio_major_instr_bits(instr) ? instr
                                                             : instr + offset;
    }
}

static __always_inline void override_pio_rx_program(PIO pio,
                                             const pio_program_t *program,
                                             const pio_program_t *debug_program,
                                             uint offset, int debug_pin) {
  if (debug_pin < 0) {
    override_pio_program(pio, program, offset);
  } else {
    override_pio_program(pio, debug_program, offset);
  }
}

static void __no_inline_not_in_flash_func(configure_fullspeed_host)(
    pio_port_t const *pp, root_port_t *port) {
  override_pio_program(pp->pio_usb_tx, pp->fs_tx_program, pp->offset_tx);
  SM_SET_CLKDIV(pp->pio_usb_tx, pp->sm_tx, pp->clk_div_fs_tx);

  pio_sm_set_jmp_pin(pp->pio_usb_rx, pp->sm_rx, port->pin_dp);
  SM_SET_CLKDIV_MAXSPEED(pp->pio_usb_rx, pp->sm_rx);

  pio_sm_set_jmp_pin(pp->pio_usb_rx, pp->sm_eop, port->pin_dm);
  pio_sm_set_in_pins(pp->pio_usb_rx, pp->sm_eop, port->pin_dp);
  SM_SET_CLKDIV(pp->pio_usb_rx, pp->sm_eop, pp->clk_div_fs_rx);

  usb_tx_configure_pins(pp->pio_usb_tx, pp->sm_tx, port->pin_dp, port->pin_dm);
}

static void __no_inline_not_in_flash_func(configure_lowspeed_host)(
    pio_port_t const *pp, root_port_t *port) {
  override_pio_program(pp->pio_usb_tx, pp->ls_tx_program, pp->offset_tx);
  SM_SET_CLKDIV(pp->pio_usb_tx, pp->sm_tx, pp->clk_div_ls_tx);

  pio_sm_set_jmp_pin(pp->pio_usb_rx, pp->sm_rx, port->pin_dm);
  SM_SET_CLKDIV_MAXSPEED(pp->pio_usb_rx, pp->sm_rx);

  pio_sm_set_jmp_pin(pp->pio_usb_rx, pp->sm_eop, port->pin_dp);
  pio_sm_set_in_pins(pp->pio_usb_rx, pp->sm_eop, port->pin_dm);
  SM_SET_CLKDIV(pp->pio_usb_rx, pp->sm_eop, pp->clk_div_ls_rx);

  usb_tx_configure_pins(pp->pio_usb_tx, pp->sm_tx, port->pin_dp, port->pin_dm);
}

static void __no_inline_not_in_flash_func(configure_root_port)(
    pio_port_t *pp, root_port_t *root) {
  if (root->is_fullspeed) {
    configure_fullspeed_host(pp, root);
  } else {
    configure_lowspeed_host(pp, root);
  }
}

static void __no_inline_not_in_flash_func(restore_fs_bus)(const pio_port_t *pp) {
  // change bus speed to full-speed
  pio_sm_set_enabled(pp->pio_usb_tx, pp->sm_tx, false);
  SM_SET_CLKDIV(pp->pio_usb_tx, pp->sm_tx, pp->clk_div_fs_tx);

  pio_sm_set_enabled(pp->pio_usb_rx, pp->sm_rx, false);
  SM_SET_CLKDIV_MAXSPEED(pp->pio_usb_rx, pp->sm_rx);
  pio_sm_set_enabled(pp->pio_usb_rx, pp->sm_rx, true);

  pio_sm_set_enabled(pp->pio_usb_rx, pp->sm_eop, false);
  SM_SET_CLKDIV(pp->pio_usb_rx, pp->sm_eop, pp->clk_div_fs_rx);
  pio_sm_set_enabled(pp->pio_usb_rx, pp->sm_eop, true);
}

// Time about 1us ourselves so it lives in RAM.
static void __not_in_flash_func(busy_wait_1_us)(void) {
  uint32_t start = timer_hw->timerawl;
  while (timer_hw->timerawl == start) {
      tight_loop_contents();
  }
}

static bool __no_inline_not_in_flash_func(connection_check)(root_port_t *port) {
  if (pio_usb_bus_get_line_state(port) == PORT_PIN_SE0) {
    busy_wait_1_us();

    if (pio_usb_bus_get_line_state(port) == PORT_PIN_SE0) {
      busy_wait_1_us();
      // device disconnect
      port->connected = false;
      port->suspended = true;
      port->ints |= PIO_USB_INTS_DISCONNECT_BITS;
      return false;
    }
  }

  return true;
}

//--------------------------------------------------------------------+
// SOF
//--------------------------------------------------------------------+
static int usb_setup_transaction(pio_port_t *pp, endpoint_t *ep);
static int usb_in_transaction(pio_port_t *pp, endpoint_t *ep);
static int usb_out_transaction(pio_port_t *pp, endpoint_t *ep);

void __not_in_flash_func(pio_usb_host_frame)(void) {
  if (!timer_active) {
    return;
  }
  __atomic_store_n(&poll_alignment_last_frame_us, timer_hw->timerawl,
                   __ATOMIC_RELEASE);
  static uint8_t sof_packet[4] = {USB_SYNC, USB_PID_SOF, 0x00, 0x10};

  pio_port_t *pp = PIO_USB_PIO_PORT(0);

  // Send SOF
  for (int root_idx = 0; root_idx < PIO_USB_ROOT_PORT_CNT; root_idx++) {
    root_port_t *root = PIO_USB_ROOT_PORT(root_idx);
    if (!(root->initialized && root->connected && !root->suspended &&
          connection_check(root))) {
      continue;
    }
    configure_root_port(pp, root);
    pio_usb_bus_usb_transfer(pp, sof_packet, 4);
  }

  // Carry out all queued endpoint transaction
  for (int root_idx = 0; root_idx < PIO_USB_ROOT_PORT_CNT; root_idx++) {
    root_port_t *root = PIO_USB_ROOT_PORT(root_idx);
    if (!(root->initialized && root->connected && !root->suspended)) {
      continue;
    }

    configure_root_port(pp, root);

    for (int ep_pool_idx = 0; ep_pool_idx < PIO_USB_EP_POOL_CNT;
         ep_pool_idx++) {
      endpoint_t *ep = PIO_USB_ENDPOINT(ep_pool_idx);
      if ((ep->root_idx == root_idx) && ep->size) {
        bool const is_periodic = ((ep->attr & 0x03) == EP_ATTR_INTERRUPT);

        if (is_periodic && (ep->interval_counter > 0)) {
          ep->interval_counter--;
          continue;
        }

        if (ep->has_transfer && !ep->transfer_aborted) {
          ep->transfer_started = true;

          if (ep->need_pre) {
            pp->need_pre = true;
          }

          if (ep->ep_num == 0 && ep->data_id == USB_PID_SETUP) {
            usb_setup_transaction(pp, ep);
          } else {
            if (ep->ep_num & EP_IN) {
              usb_in_transaction(pp, ep);
            } else {
              usb_out_transaction(pp, ep);
            }

            if (is_periodic) {
              // ep->interval_counter = ep->interval - 1;
              uint8_t interval_override_local = tusb_interval_override;
              ep->interval_counter = (interval_override_local ? interval_override_local : ep->interval) - 1;
            }
          }

          if (ep->need_pre) {
            pp->need_pre = false;
            restore_fs_bus(pp);
          }

          ep->transfer_started = false;
        }
      }
    }
  }

  // check for new connection to root hub
  for (int root_idx = 0; root_idx < PIO_USB_ROOT_PORT_CNT; root_idx++) {
    root_port_t *root = PIO_USB_ROOT_PORT(root_idx);
    if (root->initialized && !root->connected) {
      port_pin_status_t const line_state = pio_usb_bus_get_line_state(root);
      if (line_state == PORT_PIN_FS_IDLE || line_state == PORT_PIN_LS_IDLE) {
        root->is_fullspeed = (line_state == PORT_PIN_FS_IDLE);
        root->connected = true;
        root->suspended = true; // need a bus reset before operating
        root->ints |= PIO_USB_INTS_CONNECT_BITS;
      }
    }
  }

  // Invoke IRQHandler if interrupt status is set
  for (uint8_t root_idx = 0; root_idx < PIO_USB_ROOT_PORT_CNT; root_idx++) {
    if (PIO_USB_ROOT_PORT(root_idx)->ints) {
      pio_usb_host_irq_handler(root_idx);
    }
  }

  sof_count++;

  // SOF counter is 11-bit
  uint16_t const sof_count_11b = sof_count & 0x7ff;
  sof_packet[2] = sof_count_11b & 0xff;
  sof_packet[3] = (calc_usb_crc5(sof_count_11b) << 3) | (sof_count_11b >> 8);
}

static bool __no_inline_not_in_flash_func(sof_timer)(repeating_timer_t *_rt) {
  pio_usb_host_frame();

  // A negative repeating-timer period is measured start-to-start. Apply one
  // bounded interval correction, then restore the normal 1 ms cadence on the
  // next callback. This shifts phase without pausing PIO-USB or re-enumerating
  // any controller.
  const int32_t adjust_us = __atomic_exchange_n(
      &poll_alignment_pending_adjust_us, 0, __ATOMIC_ACQ_REL);
  if (adjust_us != 0) {
    _rt->delay_us = -(1000 + adjust_us);
    __atomic_store_n(&poll_alignment_last_adjust_us, adjust_us,
                     __ATOMIC_RELEASE);
    __atomic_add_fetch(&poll_alignment_applied, 1, __ATOMIC_RELAXED);
  } else {
    _rt->delay_us = -1000;
  }

  return true;
}

//--------------------------------------------------------------------+
// Host Controller functions
//--------------------------------------------------------------------+

uint32_t pio_usb_host_get_frame_number(void) {
  return sof_count;
}

void pio_usb_host_port_reset_start(uint8_t root_idx) {
  root_port_t *root = PIO_USB_ROOT_PORT(root_idx);
  pio_port_t *pp = PIO_USB_PIO_PORT(0);

  // bus is not operating while in reset
  root->suspended = true;

  // Force line state to SE0
  pio_sm_set_pins_with_mask(pp->pio_usb_tx, pp->sm_tx, 0,
                            (1 << root->pin_dp) | (1 << root->pin_dm));
  pio_sm_set_pindirs_with_mask(pp->pio_usb_tx, pp->sm_tx,
                               (1 << root->pin_dp) | (1 << root->pin_dm),
                               (1 << root->pin_dp) | (1 << root->pin_dm));
}

void pio_usb_host_port_reset_end(uint8_t root_idx) {
  root_port_t *root = PIO_USB_ROOT_PORT(root_idx);
  pio_port_t *pp = PIO_USB_PIO_PORT(0);

  // line state to input
  pio_sm_set_pindirs_with_mask(pp->pio_usb_tx, pp->sm_tx, 0,
                               (1 << root->pin_dp) | (1 << root->pin_dm));

  busy_wait_us(100); // TODO check if this is neccessary

  // bus back to operating
  root->suspended = false;
}

void pio_usb_host_close_device(uint8_t root_idx, uint8_t device_address) {
  for (int ep_pool_idx = 0; ep_pool_idx < PIO_USB_EP_POOL_CNT; ep_pool_idx++) {
    endpoint_t *ep = PIO_USB_ENDPOINT(ep_pool_idx);
    if ((ep->root_idx == root_idx) && (ep->dev_addr == device_address) &&
        ep->size) {
      poll_diag_mark_endpoint_reallocated(ep);
      ep->size = 0;
      ep->has_transfer = false;
    }
  }
}

static inline __force_inline endpoint_t * _find_ep(uint8_t root_idx, 
                                                   uint8_t device_address, uint8_t ep_address) {
  for (int ep_pool_idx = 0; ep_pool_idx < PIO_USB_EP_POOL_CNT; ep_pool_idx++) {
    endpoint_t *ep = PIO_USB_ENDPOINT(ep_pool_idx);
    // note 0x00 and 0x80 are matched as control endpoint of opposite direction
    if ((ep->root_idx == root_idx) && (ep->dev_addr == device_address) &&
        ep->size &&
        ((ep->ep_num == ep_address) ||
         (((ep_address & 0x7f) == 0) && ((ep->ep_num & 0x7f) == 0)))) {
      return ep;
    }
  }

  return NULL;
}

bool pio_usb_host_endpoint_open(uint8_t root_idx, uint8_t device_address,
                                uint8_t const *desc_endpoint, bool need_pre) {
  const endpoint_descriptor_t *d = (const endpoint_descriptor_t *)desc_endpoint;
  for (int ep_pool_idx = 0; ep_pool_idx < PIO_USB_EP_POOL_CNT; ep_pool_idx++) {
    endpoint_t *ep = PIO_USB_ENDPOINT(ep_pool_idx);
    // ep size is used as valid indicator
    if (ep->size == 0) {
      pio_usb_ll_configure_endpoint(ep, desc_endpoint);
      ep->root_idx = root_idx;
      ep->dev_addr = device_address;
      ep->need_pre = need_pre;
      ep->is_tx = (d->epaddr & 0x80) ? false : true; // host endpoint out is tx
      poll_diag_mark_endpoint_reallocated(ep);
      return true;
    }
  }

  return false;
}

bool pio_usb_host_send_setup(uint8_t root_idx, uint8_t device_address,
                             uint8_t const setup_packet[8]) {
  endpoint_t *ep = _find_ep(root_idx, device_address, 0);
  if (!ep) {
    printf("cannot find ep 0x00\r\n");
    return false;
  }

  ep->ep_num = 0; // setup is is OUT
  ep->data_id = USB_PID_SETUP;
  ep->is_tx = true;

  return pio_usb_ll_transfer_start(ep, (uint8_t *)setup_packet, 8);
}

bool pio_usb_host_endpoint_transfer(uint8_t root_idx, uint8_t device_address,
                                    uint8_t ep_address, uint8_t *buffer,
                                    uint16_t buflen) {
  endpoint_t *ep = _find_ep(root_idx, device_address, ep_address);
  if (!ep) {
    printf("no endpoint 0x%02X\r\n", ep_address);
    return false;
  }

  // Control endpoint, address may switch between 0x00 <-> 0x80
  // therefore we need to update ep_num and is_tx
  if ((ep_address & 0x7f) == 0) {
    ep->ep_num = ep_address;
    ep->is_tx = ep_address == 0;
    ep->data_id = 1; // data and status always start with DATA1
  }

  return pio_usb_ll_transfer_start(ep, buffer, buflen);
}

bool pio_usb_host_endpoint_abort_transfer(uint8_t root_idx, uint8_t device_address,
                                          uint8_t ep_address) {
  endpoint_t *ep = _find_ep(root_idx, device_address, ep_address);
  if (!ep) {
    printf("no endpoint 0x%02X\r\n", ep_address);
    return false;
  }

  if (!ep->has_transfer) {
    return false; // no transfer to abort
  }

  // mark transfer as aborted
  ep->transfer_aborted = true;

  // Race potential: SOF timer can be called before transfer_aborted is actually set
  // and started the transfer. Wait 1 usb frame for transaction to complete.
  // On the next SOF timer, transfer_aborted will be checked and skipped
  while (ep->has_transfer && ep->transfer_started) {
    busy_wait_ms(1);
  }

  // check if transfer is still active (could be completed)
  bool const still_active = ep->has_transfer;
  if (still_active) {
    ep->has_transfer = false;
  }
  ep->transfer_aborted = false;

  return still_active; // still active means transfer is successfully aborted
}

//--------------------------------------------------------------------+
// Transaction helper
//--------------------------------------------------------------------+

static int __no_inline_not_in_flash_func(usb_in_transaction)(pio_port_t *pp,
                                                             endpoint_t *ep) {
  int res = 0;
  pio_usb_poll_outcome_t poll_outcome = PIO_USB_POLL_ERROR_OTHER;
  uint32_t poll_allocation_generation = 0;
  uint8_t expect_pid = (ep->data_id == 1) ? USB_PID_DATA1 : USB_PID_DATA0;
  uint8_t alternate_pid = (ep->data_id == 1) ? USB_PID_DATA0 : USB_PID_DATA1;

  pio_usb_bus_prepare_receive(pp);
  const uint32_t poll_attempt_us = timer_hw->timerawl;
  const int poll_endpoint_index = poll_diag_endpoint_index(ep);
  if (poll_endpoint_index >= 0) {
    poll_allocation_generation = __atomic_load_n(
        &poll_diag_allocation_generation[poll_endpoint_index], __ATOMIC_ACQUIRE);
  }
  pio_usb_bus_send_token(pp, USB_PID_IN, ep->dev_addr, ep->ep_num);
  pio_usb_bus_start_receive(pp);

  int receive_len = pio_usb_bus_receive_packet_and_handshake(pp, USB_PID_ACK);
  uint8_t const receive_pid = pp->usb_rx_buffer[1];
  bool const receive_sync = pp->usb_rx_buffer[0] == USB_SYNC;

  if (receive_len >= 0 && receive_sync) {
    if (receive_pid == expect_pid) {
      memcpy(ep->app_buf, &pp->usb_rx_buffer[2], receive_len);
      pio_usb_ll_transfer_continue(ep, receive_len);
      poll_outcome = PIO_USB_POLL_DATA_ACK;
    } else if (receive_pid == alternate_pid) {
      poll_outcome = PIO_USB_POLL_TOGGLE_MISMATCH;
      // DATA0/1 mismatched, 0 for re-try next frame
    } else {
      res = -1;
      poll_outcome = PIO_USB_POLL_ERROR_PID;
      pio_usb_ll_transfer_complete(ep, PIO_USB_INTS_ENDPOINT_ERROR_BITS);
    }
  } else if (receive_sync && receive_pid == USB_PID_NAK) {
    poll_outcome = PIO_USB_POLL_NAK;
    // NAK try again next frame
  } else if (receive_sync && receive_pid == USB_PID_STALL) {
    poll_outcome = PIO_USB_POLL_STALL;
    pio_usb_ll_transfer_complete(ep, PIO_USB_INTS_ENDPOINT_STALLED_BITS);
  } else {
    res = -1;
    if ((pp->pio_usb_rx->irq & IRQ_RX_COMP_MASK) == 0) {
      res = -2;
      poll_outcome = PIO_USB_POLL_ERROR_NO_RX_EOP;
    } else if (!receive_sync) {
      poll_outcome = PIO_USB_POLL_ERROR_BAD_SYNC;
    } else if (receive_pid == USB_PID_DATA0 ||
               receive_pid == USB_PID_DATA1) {
      // The current receive helper reports CRC and short-packet failures with
      // the same negative result. Keep that limitation explicit instead of
      // claiming a pure CRC count.
      poll_outcome = PIO_USB_POLL_ERROR_DATA_INTEGRITY;
    } else if (receive_pid != 0) {
      poll_outcome = PIO_USB_POLL_ERROR_PID;
    } else {
      poll_outcome = PIO_USB_POLL_ERROR_OTHER;
    }
    pio_usb_ll_transfer_complete(ep, PIO_USB_INTS_ENDPOINT_ERROR_BITS);
  }

  pio_sm_set_enabled(pp->pio_usb_rx, pp->sm_rx, false);
  pp->usb_rx_buffer[0] = 0;
  pp->usb_rx_buffer[1] = 0;

  poll_diag_record(ep, poll_attempt_us, poll_allocation_generation,
                   poll_outcome);
  return res;
}

static int __no_inline_not_in_flash_func(usb_out_transaction)(pio_port_t *pp,
                                                              endpoint_t *ep) {
  int res = 0;

  uint16_t const xact_len = pio_usb_ll_get_transaction_len(ep);

  pio_usb_bus_prepare_receive(pp);
  pio_usb_bus_send_token(pp, USB_PID_OUT, ep->dev_addr, ep->ep_num);
  // ensure previous tx complete
  while ((pp->pio_usb_tx->irq & IRQ_TX_COMP_MASK) == 0) {
    continue;
  }

  pio_usb_bus_usb_transfer(pp, ep->buffer, xact_len + 4);
  pio_usb_bus_start_receive(pp);

  pio_usb_bus_wait_handshake(pp);

  uint8_t const receive_token = pp->usb_rx_buffer[1];

  if (receive_token == USB_PID_ACK) {
    pio_usb_ll_transfer_continue(ep, xact_len);
  } else if (receive_token == USB_PID_NAK) {
    // NAK try again next frame
  } else if (receive_token == USB_PID_STALL) {
    pio_usb_ll_transfer_complete(ep, PIO_USB_INTS_ENDPOINT_STALLED_BITS);
  } else {
    pio_usb_ll_transfer_complete(ep, PIO_USB_INTS_ENDPOINT_ERROR_BITS);
  }

  pio_sm_set_enabled(pp->pio_usb_rx, pp->sm_rx, false);
  pp->usb_rx_buffer[0] = 0;
  pp->usb_rx_buffer[1] = 0;

  return res;
}

static int __no_inline_not_in_flash_func(usb_setup_transaction)(
    pio_port_t *pp,  endpoint_t *ep) {

  int res = 0;

  // Setup token
  pio_usb_bus_prepare_receive(pp);

  pio_usb_bus_send_token(pp, USB_PID_SETUP, ep->dev_addr, 0);
  // ensure previous tx complete
  while ((pp->pio_usb_tx->irq & IRQ_TX_COMP_MASK) == 0) {
    continue;
  }

  // Data
  ep->data_id = 0; // set to DATA0
  pio_usb_bus_usb_transfer(pp, ep->buffer, 12);

  // Handshake
  pio_usb_bus_start_receive(pp);
  pio_usb_bus_wait_handshake(pp);

  ep->actual_len = 8;

  if (pp->usb_rx_buffer[0] == USB_SYNC && pp->usb_rx_buffer[1] == USB_PID_ACK) {
    pio_usb_ll_transfer_complete(ep, PIO_USB_INTS_ENDPOINT_COMPLETE_BITS);
  } else {
    res = -1;
    pio_usb_ll_transfer_complete(ep, PIO_USB_INTS_ENDPOINT_ERROR_BITS);
  }

  pp->usb_rx_buffer[1] = 0; // reset buffer

  return res;
}

//--------------------------------------------------------------------+
// USB Host Stack
//--------------------------------------------------------------------+
static void on_device_connect(pio_port_t *pp, root_port_t *root,
                              int device_idx) {
    root->failed_count = 0;
  bool fullspeed_flag = false;

  if (pio_usb_bus_get_line_state(root) == PORT_PIN_FS_IDLE) {
    fullspeed_flag = true;
  } else if (pio_usb_bus_get_line_state(root) == PORT_PIN_LS_IDLE) {
    fullspeed_flag = false;
  }

  pio_sm_set_pins_with_mask(pp->pio_usb_tx, pp->sm_tx, 0,
                            (1 << root->pin_dp) | (1 << root->pin_dm));
  pio_sm_set_pindirs_with_mask(pp->pio_usb_tx, pp->sm_tx,
                               (1 << root->pin_dp) | (1 << root->pin_dm),
                               (1 << root->pin_dp) | (1 << root->pin_dm));

  busy_wait_ms(100);

  pio_sm_set_pindirs_with_mask(pp->pio_usb_tx, pp->sm_tx, 0,
                               (1 << root->pin_dp) | (1 << root->pin_dm));

  busy_wait_us(100);

  root->suspended = false;

  if (fullspeed_flag && pio_usb_bus_get_line_state(root) == PORT_PIN_FS_IDLE) {
    root->root_device = &pio_usb_device[device_idx];
    if (!root->root_device->connected) {
      configure_fullspeed_host(pp, root);
      root->root_device->is_fullspeed = true;
      root->root_device->is_root = true;
      root->root_device->connected = true;
      root->root_device->root = root;
      root->root_device->event = EVENT_CONNECT;
    }
  } else if (!fullspeed_flag && pio_usb_bus_get_line_state(root) == PORT_PIN_LS_IDLE) {
    root->root_device = &pio_usb_device[device_idx];
    if (!root->root_device->connected) {
      configure_lowspeed_host(pp, root);
      root->root_device->is_fullspeed = false;
      root->root_device->is_root = true;
      root->root_device->connected = true;
      root->root_device->root = root;
      root->root_device->event = EVENT_CONNECT;
    }
  }

  endpoint_descriptor_t ep0_desc = {
    sizeof(endpoint_descriptor_t), DESC_TYPE_ENDPOINT, 0x00, 0x00, { 0x08, 0x00 }, 0x00
  };
  pio_usb_host_endpoint_open(root - pio_usb_root_port, 0x00,
                             (uint8_t const *)&ep0_desc, false);
}

static int __no_inline_not_in_flash_func(control_out_protocol)(
    usb_device_t *device, uint8_t *setup_data, uint16_t setup_length,
    uint8_t *out_data, uint16_t out_length) {
  int res = 0;

  control_pipe_t *pipe = &device->control_pipe;

  if (pipe->operation == CONTROL_NONE) {
    pipe->setup_packet.tx_address = setup_data;
    pipe->setup_packet.tx_length = setup_length;
    pipe->out_data_packet.tx_address = out_data;
    pipe->out_data_packet.tx_length = out_length;
    pipe->operation = CONTROL_OUT;

    pio_usb_host_send_setup(device->root - pio_usb_root_port, device->address, setup_data);
  } else {
    return -1;
  }

  const uint64_t timeout = 5000 * 1000; // 5s
  uint64_t start_time = time_us_64();
  while (pipe->operation == CONTROL_OUT &&
         time_us_64() - start_time < timeout) {
    if (!device->connected) {
      pipe->operation = CONTROL_NONE;
      return -1;
    }
  }

  if (time_us_64() - start_time >= timeout) {
    printf("control out[timeout]\n");
    res = -2;
  } else if (pipe->operation == CONTROL_ERROR) {
    printf("control out[error]\n");
    res = -1;
  } else if (pipe->operation == CONTROL_COMPLETE) {
    printf("control out[complete]\n");
    res = 0;
  }
  pipe->operation = CONTROL_NONE;

  return res;
}

static int __no_inline_not_in_flash_func(control_in_protocol)(
    usb_device_t *device, uint8_t *tx_data, uint16_t tx_length,
    uint8_t *rx_buffer, uint16_t request_length) {
  int res = 0;

  control_pipe_t *pipe = &device->control_pipe;

  if (pipe->operation == CONTROL_NONE) {
    pipe->setup_packet.tx_address = tx_data;
    pipe->setup_packet.tx_length = tx_length;
    pipe->rx_buffer = rx_buffer;
    pipe->request_length = request_length;
    pipe->operation = CONTROL_IN;

    pio_usb_host_send_setup(device->root - pio_usb_root_port, device->address, tx_data);
  } else {
    return -1;
  }

  const uint64_t timeout = 5000 * 1000; // 5s
  uint64_t start_time = time_us_64();
  while (pipe->operation == CONTROL_IN &&
         time_us_64() - start_time < timeout) {
    if (!device->connected) {
      pipe->operation = CONTROL_NONE;
      return -1;
    }
  }

  if (time_us_64() - start_time >= timeout) {
    printf("control in[timeout]\n");
    res = -2;
  } else if (pipe->operation == CONTROL_ERROR) {
    printf("control in[error]\n");
    res = -1;
  } else if (pipe->operation == CONTROL_COMPLETE) {
    printf("control in[complete]\n");
    res = 0;
  }
  pipe->operation = CONTROL_NONE;

  return res;
}

static int set_hub_feature(usb_device_t *device, uint8_t port, uint8_t value) {
  usb_setup_packet_t req = SET_HUB_FEATURE_REQUEST;
  req.index_lsb = port + 1;
  req.value_lsb = value;
  return control_out_protocol(device, (uint8_t *)&req, sizeof(req), NULL, 0);
}

static int clear_hub_feature(usb_device_t *device, uint8_t port,
                             uint8_t value) {
  usb_setup_packet_t req = CLEAR_HUB_FEATURE_REQUEST;
  req.index_lsb = port + 1;
  req.value_lsb = value;
  return control_out_protocol(device, (uint8_t *)&req, sizeof(req), NULL, 0);
}

static int get_hub_port_status(usb_device_t *device, uint8_t port,
                               hub_port_status_t *status) {
  usb_setup_packet_t req = GET_HUB_PORT_STATUS_REQUEST;
  req.index_lsb = port + 1;
  return control_in_protocol(device, (uint8_t *)&req, sizeof(req), (uint8_t*)status,
                             sizeof(*status));
}

static int initialize_hub(usb_device_t *device) {
  uint8_t rx_buffer[16];
  int res = 0;
  printf("USB Hub detected\n");
  usb_setup_packet_t get_hub_desc_request = GET_HUB_DESCRPTOR_REQUEST;
  control_in_protocol(device, (uint8_t *)&get_hub_desc_request,
                      sizeof(get_hub_desc_request), rx_buffer, 8);
  const hub_descriptor_t *desc = (hub_descriptor_t *)rx_buffer;
  uint8_t port_num = desc->port_num;

  printf("\tTurn on port powers\n");
  for (int idx = 0; idx < port_num; idx++) {
    res = set_hub_feature(device, idx, HUB_SET_PORT_POWER);
    if (res != 0) {
      printf("\tFailed to turn on ports\n");
      break;
    }
  }

  busy_wait_ms(500);

  return res;
}

static int get_string_descriptor(usb_device_t *device, uint8_t idx,
                                 uint8_t *rx_buffer, uint8_t *str_buffer) {
  int res = -1;
  usb_setup_packet_t req = GET_DEVICE_DESCRIPTOR_REQ_DEFAULT;
  req.value_msb = DESC_TYPE_STRING;
  req.value_lsb = idx;
  req.length_lsb = 1;
  req.length_msb = 0;
  res = control_in_protocol(device, (uint8_t *)&req, sizeof(req), rx_buffer, 1);
  if (res != 0) {
    return res;
  }

  uint8_t len = rx_buffer[0];
  req.length_lsb = len;
  req.length_msb = 0;
  res =
      control_in_protocol(device, (uint8_t *)&req, sizeof(req), rx_buffer, len);
  if (res != 0) {
    return res;
  }

  uint16_t *wchar_buffer = (uint16_t *)(uintptr_t) rx_buffer;
  for (int i = 0; i < (len - 2) / 2; i++) {
    str_buffer[i] = wchar_buffer[i + 1];
  }
  str_buffer[(len - 2) / 2] = '\0';

  return res;
}

static int enumerate_device(usb_device_t *device, uint8_t address) {
  int res = 0;
  uint8_t rx_buffer[512];

  usb_setup_packet_t get_device_descriptor_request =
      GET_DEVICE_DESCRIPTOR_REQ_DEFAULT;
  res = control_in_protocol(device, (uint8_t *)&get_device_descriptor_request,
                            sizeof(get_device_descriptor_request), rx_buffer, 18);
  if (res != 0) {
    pio_usb_host_close_device(device->root - pio_usb_root_port, 0);
    return res;
  }

  const device_descriptor_t *desc =
      (device_descriptor_t *)device->control_pipe.rx_buffer;
  device->vid = desc->vid[0] | (desc->vid[1] << 8);
  device->pid = desc->pid[0] | (desc->pid[1] << 8);
  device->device_class = desc->device_class;
  uint8_t idx_manufacture = desc->manufacture;
  uint8_t idx_product = desc->product;
  uint8_t idx_serial = desc->serial;

  printf("Enumerating %04x:%04x, class:%d, address:%d\n", device->vid,
         device->pid, device->device_class, address);

  usb_setup_packet_t set_address_request = SET_ADDRESS_REQ_DEFAULT;
  set_address_request.value_lsb = address;
  set_address_request.value_msb = 0;
  res = control_out_protocol(device, (uint8_t *)&set_address_request,
                             sizeof(set_address_request), NULL, 0);
  pio_usb_host_close_device(device->root - pio_usb_root_port, 0);
  if (res != 0) {
    return res;
  }
  device->address = address;

  endpoint_descriptor_t ep0_desc = {
    sizeof(endpoint_descriptor_t), DESC_TYPE_ENDPOINT, 0x00, 0x00, { desc->max_packet_size, 0x00 }, 0x00
  };
  pio_usb_host_endpoint_open(device->root - pio_usb_root_port, address,
                             (uint8_t const *)&ep0_desc,
                             !device->is_root && !device->is_fullspeed);

  uint8_t str[64];
  if (idx_manufacture != 0) {
    res = get_string_descriptor(device, idx_manufacture, rx_buffer, str);
    if (res == 0) {
      printf("Manufacture:%s\n", str);
    } else {
      printf("Failed to get string descriptor (Manufacture)\n");
    }
    stdio_flush();
  }

  if (idx_product != 0) {
    res = get_string_descriptor(device, idx_product, rx_buffer, str);
    if (res == 0) {
      printf("Product:%s\n", str);
    } else {
      printf("Failed to get string descriptor (Product)\n");
    }
    stdio_flush();
  }

  if (idx_serial != 0) {
    res = get_string_descriptor(device, idx_serial, rx_buffer, str);
    if (res == 0) {
      printf("Serial:%s\n", str);
    } else {
      printf("Failed to get string descriptor (Serial)\n");
    }
    stdio_flush();
  }

  usb_setup_packet_t get_configuration_descriptor_request =
      GET_CONFIGURATION_DESCRIPTOR_REQ_DEFAULT;
  get_configuration_descriptor_request.length_lsb = 9;
  get_configuration_descriptor_request.length_msb = 0;
  res = control_in_protocol(
      device, (uint8_t *)&get_configuration_descriptor_request,
      sizeof(get_configuration_descriptor_request), rx_buffer, 9);
  if (res != 0) {
    return res;
  }

  get_configuration_descriptor_request.length_lsb =
      ((configuration_descriptor_t *)(device->control_pipe.rx_buffer))
          ->total_length_lsb;
  get_configuration_descriptor_request.length_msb =
      ((configuration_descriptor_t *)(device->control_pipe.rx_buffer))
          ->total_length_msb;
  uint16_t request_length =
      get_configuration_descriptor_request.length_lsb |
      (get_configuration_descriptor_request.length_msb << 8);
  res = control_in_protocol(
      device, (uint8_t *)&get_configuration_descriptor_request,
      sizeof(get_configuration_descriptor_request), rx_buffer, request_length);

  if (res != 0) {
    return res;
  }
  uint8_t configuration_descrptor_data[512];
  int16_t configuration_descrptor_length =
      request_length > 512 ? 512 : request_length;
  memcpy(configuration_descrptor_data,
         (const void *)device->control_pipe.rx_buffer,
         configuration_descrptor_length);

  usb_setup_packet_t set_usb_configuration_request =
      SET_CONFIGURATION_REQ_DEFAULT;
  set_usb_configuration_request.value_lsb =
      ((configuration_descriptor_t *)(device->control_pipe.rx_buffer))
          ->configuration_value;
  res = control_out_protocol(device, (uint8_t *)&set_usb_configuration_request,
                             sizeof(set_usb_configuration_request), NULL, 0);

  if (res != 0) {
    return res;
  }
  volatile uint8_t ep_id_idx = 0;
  volatile uint8_t interface = 0;
  volatile uint8_t class = 0;
  uint8_t *descriptor = configuration_descrptor_data;
  while (configuration_descrptor_length > 0) {
    switch (descriptor[1]) {
      case DESC_TYPE_INTERFACE: {
        const interface_descriptor_t *d =
            (const interface_descriptor_t *)descriptor;
        printf(
            "inum:%d, altsetting:%d, numep:%d, iclass:%d, isubclass:%d, "
            "iprotcol:%d, iface:%d\n",
            d->inum, d->altsetting, d->numep, d->iclass, d->isubclass,
            d->iprotocol, d->iface);
        interface = d->inum;
        class = d->iclass;
      } break;
      case DESC_TYPE_ENDPOINT: {
        const endpoint_descriptor_t *d =
            (const endpoint_descriptor_t *)descriptor;
        printf("\t\t\tepaddr:0x%02x, attr:%d, size:%d, interval:%d\n",
               d->epaddr, d->attr, d->max_size[0] | (d->max_size[1] << 8),
               d->interval);

        if ((class == CLASS_HID || class == CLASS_HUB) &&
            d->attr == EP_ATTR_INTERRUPT) {
          volatile endpoint_t *ep = NULL;
          for (int ep_pool_idx = 0; ep_pool_idx < PIO_USB_EP_POOL_CNT;
               ep_pool_idx++) {
            if (pio_usb_ep_pool[ep_pool_idx].size == 0) {
              ep = &pio_usb_ep_pool[ep_pool_idx];
              device->endpoint_id[ep_id_idx] = ep_pool_idx + 1;
              ep_id_idx++;
              break;
            }
          }

          if (ep != NULL) {
            ep->interval = d->interval;
            ep->interval_counter = 0;
            ep->size = d->max_size[0] | (d->max_size[1] << 8);
            ep->attr = d->attr | EP_ATTR_ENUMERATING;
            ep->ep_num = d->epaddr;

            ep->root_idx = device->root - pio_usb_root_port;
            ep->dev_addr = device->address;
            ep->need_pre = !device->is_root && !device->is_fullspeed;
            ep->is_tx = (d->epaddr & 0x80) ? false : true;
            poll_diag_mark_endpoint_reallocated((endpoint_t *)ep);
          } else {
            printf("No empty EP\n");
          }
        }
      } break;
      case DESC_TYPE_HID: {
        const hid_descriptor_t *d = (const hid_descriptor_t *)descriptor;
        printf(
            "\tbcdHID:%x.%x, country:%d, desc num:%d, desc_type:%d, "
            "desc_size:%d\n",
            d->bcd_hid[1], d->bcd_hid[0], d->contry_code, d->num_desc,
            d->desc_type, d->desc_len[0] | (d->desc_len[1] << 8));

        usb_setup_packet_t set_hid_idle_request = SET_HID_IDLE_REQ_DEFAULT;
        set_hid_idle_request.value_lsb = interface;
        set_hid_idle_request.value_msb = 0;
        control_out_protocol(device, (uint8_t *)&set_hid_idle_request,
                             sizeof(set_hid_idle_request), NULL, 0);

        usb_setup_packet_t get_hid_report_descrpitor_request =
            GET_HID_REPORT_DESCRIPTOR_DEFAULT;
        get_hid_report_descrpitor_request.index_lsb = interface;
        get_hid_report_descrpitor_request.index_msb = 0;
        get_hid_report_descrpitor_request.length_lsb = d->desc_len[0];
        get_hid_report_descrpitor_request.length_msb = d->desc_len[1];
        uint16_t desc_len = d->desc_len[0] | (d->desc_len[1] << 8);
        control_in_protocol(
            device, (uint8_t *)&get_hid_report_descrpitor_request,
            sizeof(get_hid_report_descrpitor_request), rx_buffer, desc_len);
        printf("\t\tReport descriptor:");
        for (int i = 0; i < desc_len; i++) {
          printf("%02x ", device->control_pipe.rx_buffer[i]);
        }
        printf("\n");
        stdio_flush();

      } break;
      default:
        break;
    }

    configuration_descrptor_length -= descriptor[0];
    descriptor += descriptor[0];
  }

  for (int epidx = 0; epidx < PIO_USB_DEV_EP_CNT; epidx++) {
    endpoint_t *ep = pio_usb_get_endpoint(device, epidx);
    if (ep == NULL) {
      break;
    }
    ep->attr &= ~EP_ATTR_ENUMERATING;

    // prepare transfer
    if (ep->ep_num & EP_IN) {
      pio_usb_ll_transfer_start(ep, ep->buffer, ep->size);
    }
  }

  return res;
}

static void device_disconnect(usb_device_t *device) {
  printf("Disconnect device %d\n", device->address);
  for (int port = 0; port < PIO_USB_HUB_PORT_CNT; port++) {
    if (device->child_devices[port] != 0) {
      device_disconnect(&pio_usb_device[device->child_devices[port]]);
    }
  }

  for (int ep_idx = 0; ep_idx < PIO_USB_DEV_EP_CNT; ep_idx++) {
    endpoint_t *ep = pio_usb_get_endpoint(device, ep_idx);
    if (ep == NULL) {
      break;
    }
    poll_diag_mark_endpoint_reallocated(ep);
    memset(ep, 0, sizeof(*ep));
  }

  if (device->address == 0 && device->root != NULL) {
    device->root->addr0_exists = false;
  }

  memset(device, 0, sizeof(*device));
}

static int device_pool_vacant(void) {
  for (int idx = 0; idx < PIO_USB_DEVICE_CNT; idx++) {
    if (!pio_usb_device[idx].connected) {
      return idx;
    }
  }

  return -1;
}

static int assign_new_device_to_port(usb_device_t *hub_device, uint8_t port, bool is_ls) {
  int idx = device_pool_vacant();
  if (idx >= 0) {
    hub_device->child_devices[port] = idx;
    pio_usb_device[idx].parent_device = hub_device;
    pio_usb_device[idx].parent_port = port;
    pio_usb_device[idx].root = hub_device->root;
    pio_usb_device[idx].connected = true;
    pio_usb_device[idx].is_fullspeed = !is_ls;
    pio_usb_device[idx].event = EVENT_CONNECT;
    printf("Assign device %d to %d-%d\n", idx, hub_device->address, port);

    endpoint_descriptor_t ep0_desc = {
      sizeof(endpoint_descriptor_t), DESC_TYPE_ENDPOINT, 0x00, 0x00, { 0x08, 0x00 }, 0x00
    };
    pio_usb_host_endpoint_open(hub_device->root - pio_usb_root_port, 0x00,
                               (uint8_t const *)&ep0_desc, is_ls);

    return 0;
  }

  printf("Failed to assign device\n");

  return -1;
}

static void __no_inline_not_in_flash_func(process_hub_event)(
    usb_device_t *device) {
  volatile endpoint_t *ep = pio_usb_get_endpoint(device, 0);
  uint8_t bitmap = ep->buffer[0];
  for (int bit = 1; bit < 8; bit++) {
    if (!(bitmap & (1 << bit))) {
      continue;
    }
    uint8_t port = bit - 1;
    hub_port_status_t status;
    int res = get_hub_port_status(device, port, &status);
    if (res != 0) {
      printf("Failed to get port%d-%d status\n", device->address, port);
      continue;
    }
    printf("port%d-%d status:%d %d\n", device->address, port,
           status.port_change, status.port_status);

    if (status.port_change & HUB_CHANGE_PORT_CONNECTION) {
      if (status.port_status & HUB_STAT_PORT_CONNECTION) {
        printf("new device on port %d, reset port\n", port);
        if (device->child_devices[port] != 0) {
          printf("device is already assigned. disconnect previous\n");
          device_disconnect(&pio_usb_device[device->child_devices[port]]);
        }

        if (device->root->addr0_exists) {
          printf("Address 0 already exists\n");
          continue;
        }

        if (device_pool_vacant() >= 0) {
          set_hub_feature(device, port, HUB_SET_PORT_RESET);
          device->root->addr0_exists = true;
        } else {
          printf("No vacant in device pool\n");
        }
      } else {
        printf("device removed from port %d\n", port);
        if (device->child_devices[port] != 0) {
          device_disconnect(&pio_usb_device[device->child_devices[port]]);
        }
      }
      clear_hub_feature(device, port, HUB_CLR_PORT_CONNECTION);
    } else if (status.port_change & HUB_CHANGE_PORT_RESET) {
      printf("reset port %d complete\n", port);
      res = clear_hub_feature(device, port, HUB_CLR_PORT_RESET);
      if (res == 0) {
        assign_new_device_to_port(device, port,
                                  status.port_status & HUB_STAT_PORT_LOWSPEED);
      }
    } else if (status.port_change & HUB_CHANGE_PORT_ENABLE) {
      clear_hub_feature(device, port, HUB_CLR_PORT_ENABLE);
    }
  }

  device->event = EVENT_NONE;
}

void __no_inline_not_in_flash_func(pio_usb_host_task)(void) {
  for (int root_idx = 0; root_idx < PIO_USB_ROOT_PORT_CNT; root_idx++) {
    if (pio_usb_root_port[root_idx].event == EVENT_CONNECT) {
      printf("Root %d connected\n", root_idx);
      int dev_idx = device_pool_vacant();
      if (dev_idx >= 0) {
        on_device_connect(&pio_port[0], &pio_usb_root_port[root_idx], dev_idx);
        pio_usb_root_port[root_idx].addr0_exists = true;
      }
      pio_usb_root_port[root_idx].event = EVENT_NONE;
    } else if (pio_usb_root_port[root_idx].event == EVENT_DISCONNECT) {
      printf("Root %d disconnected\n", root_idx);
      pio_usb_host_close_device(
          root_idx, pio_usb_root_port[root_idx].root_device->address);
      pio_usb_root_port[root_idx].root_device->connected = false;
      pio_usb_root_port[root_idx].root_device->event = EVENT_DISCONNECT;
      pio_usb_root_port[root_idx].root_device = NULL;
      pio_usb_root_port[root_idx].event = EVENT_NONE;
    }
  }

  for (int idx = 0; idx < PIO_USB_DEVICE_CNT; idx++) {
    usb_device_t *device = &pio_usb_device[idx];

    if (device->event == EVENT_CONNECT) {
      device->event = EVENT_NONE;
      printf("Device %d Connected\n", idx);
      int res = enumerate_device(device, idx + 1);
      if (res == 0) {
        device->enumerated = true;
        device->root->addr0_exists = false;
        if (device->device_class == CLASS_HUB) {
          res = initialize_hub(device);
        }
      }

      if (res != 0) {
        printf("Enumeration failed(%d)\n", res);
        // retry
        if (device->is_root) {
          device->root->event = EVENT_DISCONNECT;
        } else {
          set_hub_feature(device->parent_device, device->parent_port,
                          HUB_SET_PORT_RESET);
          device_disconnect(device);
        }
      }
    } else if (device->event == EVENT_DISCONNECT) {
      device->event = EVENT_NONE;
      printf("Disconnect\n");
      device_disconnect(device);
    } else if (device->event == EVENT_HUB_PORT_CHANGE) {
      process_hub_event(device);
    }
  }

  if (cancel_timer_flag) {
    int_stat = save_and_disable_interrupts();
    stop_timer();
    if (pio_usb_root_port->root_device != NULL) {
      device_disconnect(pio_usb_root_port->root_device);
    }
    cancel_timer_flag = false;
  }

  if (start_timer_flag) {
    start_timer(_alarm_pool);
    restore_interrupts(int_stat);
    start_timer_flag = false;
  }
}

static void __no_inline_not_in_flash_func(handle_endpoint_irq)(
    root_port_t *root, uint32_t flag, volatile uint32_t *ep_reg) {
  (void)root;
  const uint32_t ep_all = *ep_reg;

  for (uint8_t ep_idx = 0; ep_idx < PIO_USB_EP_POOL_CNT; ep_idx++) {
    if (ep_all & (1u << ep_idx)) {
      endpoint_t *ep = PIO_USB_ENDPOINT(ep_idx);
      usb_device_t *device = NULL;

      // find device this endpoint belongs to
      for (int idx = 0; idx < PIO_USB_DEVICE_CNT; idx++) {
        usb_device_t *dev = &pio_usb_device[idx];
        if (dev->connected && (ep->dev_addr == dev->address)) {
          device = dev;
          break;
        }
      }

      if (device) {
        // control endpoint is either 0x00 or 0x80
        if ((ep->ep_num & 0x7f) == 0) {
          control_pipe_t *pipe = &device->control_pipe;

          if (flag != PIO_USB_INTS_ENDPOINT_COMPLETE_BITS) {
            pipe->stage = STAGE_SETUP;
            pipe->operation = CONTROL_ERROR;
          } else {
            ep->data_id = 1; // both data and status have DATA1
            if (pipe->stage == STAGE_SETUP) {
              if (pipe->operation == CONTROL_IN) {
                pipe->stage = STAGE_IN;
                ep->ep_num = 0x80;
                ep->is_tx = false;
                pio_usb_ll_transfer_start(ep,
                                          (uint8_t *)(uintptr_t)pipe->rx_buffer,
                                          pipe->request_length);
              } else if (pipe->operation == CONTROL_OUT) {
                if (pipe->out_data_packet.tx_address != NULL) {
                  pipe->stage = STAGE_OUT;
                  ep->ep_num = 0x00;
                  ep->is_tx = true;
                  pio_usb_ll_transfer_start(ep,
                                            pipe->out_data_packet.tx_address,
                                            pipe->out_data_packet.tx_length);
                } else {
                  pipe->stage = STAGE_STATUS;
                  ep->ep_num = 0x80;
                  ep->is_tx = false;
                  pio_usb_ll_transfer_start(ep, NULL, 0);
                }
              }
            } else if (pipe->stage == STAGE_IN) {
              pipe->stage = STAGE_STATUS;
              ep->ep_num = 0x00;
              ep->is_tx = true;
              pio_usb_ll_transfer_start(ep, NULL, 0);
            } else if (pipe->stage == STAGE_OUT) {
              pipe->stage = STAGE_STATUS;
              ep->ep_num = 0x80;
              ep->is_tx = false;
              pio_usb_ll_transfer_start(ep, NULL, 0);
            } else if (pipe->stage == STAGE_STATUS) {
              pipe->stage = STAGE_SETUP;
              pipe->operation = CONTROL_COMPLETE;
            }
          }
        } else if (device->device_class == CLASS_HUB && (ep->ep_num & EP_IN)) {
          // hub interrupt endpoint
          device->event = EVENT_HUB_PORT_CHANGE;
        }
      }
    }
  }

  // clear all
  (*ep_reg) &= ~ep_all;
}

// IRQ Handler
static void __no_inline_not_in_flash_func(__pio_usb_host_irq_handler)(uint8_t root_id) {
  root_port_t *root = PIO_USB_ROOT_PORT(root_id);
  uint32_t const ints = root->ints;

  if (ints & PIO_USB_INTS_CONNECT_BITS) {
    root->event = EVENT_CONNECT;
  }

  if (ints & PIO_USB_INTS_DISCONNECT_BITS) {
    root->event = EVENT_DISCONNECT;
  }

  if (ints & PIO_USB_INTS_ENDPOINT_COMPLETE_BITS) {
    handle_endpoint_irq(root, PIO_USB_INTS_ENDPOINT_COMPLETE_BITS,
                        &root->ep_complete);
  }

  if (ints & PIO_USB_INTS_ENDPOINT_STALLED_BITS) {
    handle_endpoint_irq(root, PIO_USB_INTS_ENDPOINT_STALLED_BITS,
                        &root->ep_stalled);
  }

  if (ints & PIO_USB_INTS_ENDPOINT_ERROR_BITS) {
    handle_endpoint_irq(root, PIO_USB_INTS_ENDPOINT_ERROR_BITS,
                        &root->ep_error);
  }

  // clear all
  root->ints &= ~ints;
}

// weak alias to __pio_usb_host_irq_handler
void pio_usb_host_irq_handler(uint8_t root_id) __attribute__ ((weak, alias("__pio_usb_host_irq_handler")));

#pragma GCC pop_options
