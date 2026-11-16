#include "usb_device_lock.h"

#if defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED) && \
    defined(ADAPT_OUTPUT_USB_DEVICE)
#include <Adafruit_TinyUSB.h>

// Replaces Arduino-Pico's weak yield() (cores/rp2040/delay.cpp), which runs
// tud_task and then flushes every CDC port without the USB lock. A flush with
// bytes queued claims the IN endpoint, and an idle CDC available() calls
// yield() on every serial poll, so this is the most frequent claim outside the
// lock. Same work, with the flush under it.
extern "C" void yield(void) {
  TinyUSB_Device_Task();
  UsbDeviceLock usbLock;
  TinyUSB_Device_FlushCDC();
}
#endif
