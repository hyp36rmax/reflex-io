#include <stdio.h>
#include <string.h>

#include <tusb.h>
#include "hardware/timer.h"
#include "pico/mutex.h"

#include "out_report.h"
#ifdef ENABLE_INPUT_USB
#include "../input/usb_host/input_usb/input_usb.h"
#endif

typedef struct {
    uint8_t dev_addr;
    uint8_t interface;
    bool use_interrupt_edp; //interrupt or control
    uint8_t report_id;
    uint8_t len; // uint16_t
    uint8_t request;
    uint8_t type; //OutType type;
    uint8_t report[64];  // XXX
} outgoing_out_report_t;

#define OOR_BUFSIZE 8
static outgoing_out_report_t outgoing_out_reports[OOR_BUFSIZE];
static uint8_t oor_head = 0;
static uint8_t oor_tail = 0;
static uint8_t oor_items = 0;

static uint8_t get_buffer[64];

static bool ready_to_send = true;
auto_init_mutex(out_report_mutex);

// TinyUSB closes a device that leaves mid-transfer without a completion
// callback, so the transfer in flight is released here once its device is
// gone or after OUT_REPORT_TRANSFER_TIMEOUT_US. Otherwise the queue stops for
// good (the Sony identity read, USB/Bluetooth de-duplication).
#define OUT_REPORT_TRANSFER_TIMEOUT_US 1000000u
static uint8_t in_flight_dev_addr = 0;
static uint32_t in_flight_since_us = 0;

static void mark_transfer_in_flight(uint8_t dev_addr) {
    ready_to_send = false;
    in_flight_dev_addr = dev_addr;
    in_flight_since_us = time_us_32();
}

static void release_lost_transfer(void) {
    if (!ready_to_send &&
        (!tuh_mounted(in_flight_dev_addr) ||
         (uint32_t)(time_us_32() - in_flight_since_us) >= OUT_REPORT_TRANSFER_TIMEOUT_US)) {
        ready_to_send = true;
    }
    // Reports queued for a device that has gone would block the head.
    while (oor_items > 0 && !tuh_mounted(outgoing_out_reports[oor_head].dev_addr)) {
        oor_head = (oor_head + 1) % OOR_BUFSIZE;
        oor_items--;
    }
}

void do_reset_out_report_queue() {
    mutex_enter_blocking(&out_report_mutex);
    oor_head = 0;
    oor_tail = 0;
    oor_items = 0;
    ready_to_send = true;
    mutex_exit(&out_report_mutex);
}

void do_queue_out_report(const uint8_t* report, uint16_t len, uint8_t report_id, uint8_t dev_addr, uint8_t interface, bool use_interrupt_edp, uint8_t type) {
    mutex_enter_blocking(&out_report_mutex);
    if (oor_items == OOR_BUFSIZE) {
        printf("out overflow!\n");
        mutex_exit(&out_report_mutex);
        return;
    }
    size_t queued_len = (size_t) len + ((report_id != 0) ? 1u : 0u);
    if (queued_len > sizeof(outgoing_out_reports[oor_tail].report)) {
        mutex_exit(&out_report_mutex);
        return;
    }
    outgoing_out_reports[oor_tail].dev_addr = dev_addr;
    outgoing_out_reports[oor_tail].interface = interface;
    outgoing_out_reports[oor_tail].use_interrupt_edp = use_interrupt_edp;
    outgoing_out_reports[oor_tail].report_id = report_id;
    outgoing_out_reports[oor_tail].request = HID_REQ_CONTROL_SET_REPORT;
    outgoing_out_reports[oor_tail].len = (uint16_t) queued_len;
    outgoing_out_reports[oor_tail].type = type;
    if (report_id != 0) {
        outgoing_out_reports[oor_tail].report[0] = report_id;
    }
    memcpy(outgoing_out_reports[oor_tail].report + ((report_id != 0) ? 1 : 0), report, len);
    oor_tail = (oor_tail + 1) % OOR_BUFSIZE;
    oor_items++;
    mutex_exit(&out_report_mutex);
}

void do_queue_get_report(uint8_t report_id, uint8_t dev_addr, uint8_t interface, bool use_interrupt_edp, uint8_t type, uint8_t len) {
    mutex_enter_blocking(&out_report_mutex);
    if (oor_items == OOR_BUFSIZE) {
        printf("out overflow!\n");
        mutex_exit(&out_report_mutex);
        return;
    }
    outgoing_out_reports[oor_tail].dev_addr = dev_addr;
    outgoing_out_reports[oor_tail].interface = interface;
    outgoing_out_reports[oor_tail].use_interrupt_edp = use_interrupt_edp;
    outgoing_out_reports[oor_tail].report_id = report_id;
    outgoing_out_reports[oor_tail].request = HID_REQ_CONTROL_GET_REPORT;
    outgoing_out_reports[oor_tail].type = type;
    outgoing_out_reports[oor_tail].len = len;
    oor_tail = (oor_tail + 1) % OOR_BUFSIZE;
    oor_items++;
    mutex_exit(&out_report_mutex);
}

void do_send_out_report() {
    mutex_enter_blocking(&out_report_mutex);
    release_lost_transfer();
    if ((oor_items > 0) && ready_to_send) {
        outgoing_out_report_t* out = &(outgoing_out_reports[oor_head]);
        if (out->request == HID_REQ_CONTROL_SET_REPORT) {
          if (out->use_interrupt_edp) {
              if (tuh_hid_send_report(out->dev_addr, out->interface, out->report_id, out->report, out->len)) {
                  mark_transfer_in_flight(out->dev_addr);
                  oor_head = (oor_head + 1) % OOR_BUFSIZE;
                  oor_items--;
              }
          } else {
              if (tuh_hid_set_report(out->dev_addr, out->interface, out->report_id, out->type, out->report, out->len)) {
                  mark_transfer_in_flight(out->dev_addr);
                  oor_head = (oor_head + 1) % OOR_BUFSIZE;
                  oor_items--;
              }
          }
        } else if (out->request == HID_REQ_CONTROL_GET_REPORT) {
            if (tuh_hid_get_report(out->dev_addr, out->interface, out->report_id, out->type, get_buffer, out->len)) {
                mark_transfer_in_flight(out->dev_addr);
                oor_head = (oor_head + 1) % OOR_BUFSIZE;
                oor_items--;
            }
        }
    }
    mutex_exit(&out_report_mutex);
}

// Invoked when sent report to device successfully via interrupt endpoint
void tuh_hid_report_sent_cb(uint8_t dev_addr, uint8_t idx, uint8_t const* report, uint16_t len) {
    mutex_enter_blocking(&out_report_mutex);
    ready_to_send = true;
    mutex_exit(&out_report_mutex);
    host_sent_report_cb(dev_addr, idx, report, len);
#ifdef ENABLE_INPUT_USB
    inputusb_hid_report_sent_cb(dev_addr, idx, report, len);
#endif
}

// Invoked when Sent Report to device via either control endpoint
// len = 0 indicate there is error in the transfer e.g stalled response
void tuh_hid_set_report_complete_cb(uint8_t dev_addr, uint8_t instance, uint8_t report_id, uint8_t report_type, uint16_t len) {
    mutex_enter_blocking(&out_report_mutex);
    ready_to_send = true;
    mutex_exit(&out_report_mutex);
    host_set_report_complete_cb(dev_addr, instance, report_id, report_type,
                                len);
}

// Invoked when Get Report to device via either control endpoint
// len = 0 indicate there is error in the transfer e.g stalled response
void tuh_hid_get_report_complete_cb(uint8_t dev_addr, uint8_t idx, uint8_t report_id, uint8_t report_type, uint16_t len) {
    mutex_enter_blocking(&out_report_mutex);
    ready_to_send = true;
    mutex_exit(&out_report_mutex);
    host_get_report_cb(dev_addr, idx, report_id, report_type, get_buffer, len);
}
