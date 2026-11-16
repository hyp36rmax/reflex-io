#pragma once

#include "../product_config.h"

// Keeps a main-loop TinyUSB device call from running beside tud_task().
//
// Arduino-Pico runs tud_task() from a soft IRQ whenever it can take
// __usb_mutex, and nothing else takes that mutex, so the IRQ can preempt the
// main loop in the middle of a TinyUSB call. TinyUSB guards endpoint claims
// with a mutex that waits forever: every HID report, CDC write, read and flush
// claims one. When tud_task preempted a main-loop claim and a callback claimed
// an endpoint too (a CDC transfer callback flushing, for example), it waited
// for the code it had interrupted, on the same core, and the adapter froze.
// Classic2USB did so within minutes of steady serial traffic (adapt-worklog,
// 2026-11-11). Holding __usb_mutex makes the IRQ skip tud_task until the call
// returns.
//
// It does nothing when this core already holds the mutex (inside tud_task,
// from the IRQ or from a thread-context TinyUSB_Device_Task(), or inside an
// outer lock), and an IRQ never waits for it. Releasing it runs the tud_task
// the IRQ may have skipped, so an event does not wait for the next USB
// interrupt.
//
// Never hold it across a wait for USB progress (an endpoint going idle, FIFO
// room, a control transfer finishing): only tud_task makes that progress.
// Keep slow work out of it too, because it delays every USB event. Never call
// TinyUSB device functions from an interrupt other than tud_task's: one that
// preempts a lock holder on this core sees its own core as the owner and runs
// unlocked.
#if defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED) && \
    defined(ADAPT_OUTPUT_USB_DEVICE)
#include <pico/mutex.h>
#include <pico/platform.h>

extern mutex_t __usb_mutex;
extern "C" void TinyUSB_Device_Task(void);

class UsbDeviceLock {
 public:
  UsbDeviceLock() {
    if (!mutex_is_initialized(&__usb_mutex)) return;
    uint32_t owner = 0;
    if (mutex_try_enter(&__usb_mutex, &owner)) {
      locked_ = true;
      return;
    }
    if ((lock_owner_id_t)owner == lock_get_caller_owner_id()) return;
    if (__get_current_exception() != 0) return;
    mutex_enter_blocking(&__usb_mutex);  // the other core is using USB
    locked_ = true;
  }
  ~UsbDeviceLock() {
    if (!locked_) return;
    mutex_exit(&__usb_mutex);
    if (__get_current_exception() == 0) TinyUSB_Device_Task();
  }
  UsbDeviceLock(const UsbDeviceLock&) = delete;
  UsbDeviceLock& operator=(const UsbDeviceLock&) = delete;

 private:
  bool locked_ = false;
};
#else
class UsbDeviceLock {
 public:
  UsbDeviceLock() {}
  UsbDeviceLock(const UsbDeviceLock&) = delete;
  UsbDeviceLock& operator=(const UsbDeviceLock&) = delete;
};
#endif
