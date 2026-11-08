#include <stdint.h>
#include "../product_config.h"
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

#if defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED) && \
    (defined(ADAPT_OUTPUT_USB_DEVICE) || \
     defined(ADAPT_USB_HOST_NATIVE_RHPORT))
extern "C" bool tud_disconnect(void);

extern "C" void TinyUSB_Device_Init(uint8_t rhport) {
  (void)rhport;
#if defined(ADAPT_OUTPUT_USB_DEVICE)
  // Defer USB device init until configure_usb_output() has restored the saved
  // output mode and registered any raw descriptor hooks. Xbox 360 auth depends
  // on those hooks being in place before tud_init() captures descriptors and
  // application class drivers.
  Serial.end();
#else
  // Arduino-Pico's main() always makes this weak device-init call when the
  // TinyUSB core is selected. Native-host-only products compile CFG_TUD=0, so
  // the call must be a deliberate no-op before USBHost.begin(0) owns rhport 0.
#endif
}
#endif
