#include "../../product_config.h"
#include "output_usb_interval.h"

#ifdef ADAPT_OUTPUT_USB_DEVICE
#include <Adafruit_TinyUSB.h>
#endif

uint32_t outputUsbInputIntervalUs() {
#ifdef ADAPT_OUTPUT_USB_DEVICE
  if (!tud_mounted()) return 0;
  const uint8_t* configuration = tud_descriptor_configuration_cb(0);
  if (configuration == nullptr) return 0;
  const size_t length = static_cast<size_t>(configuration[2]) |
                        (static_cast<size_t>(configuration[3]) << 8);
  return output_usb_interval_detail::usbInputIntervalUs(
    configuration, length, tud_speed_get() == TUSB_SPEED_HIGH);
#else
  return 0;
#endif
}
