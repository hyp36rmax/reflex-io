#include "../product_config.h"
#include "../product_display_identity.h"

#include "menu_about.h"

#include "../firmware_build_info.h"
#include "menu_helpers.h"
#include "../platform/display_runtime_state.h"
#include "../output/usb/output_usb_interval.h"

void renderAboutScreen() {
  #ifdef USE_I2C_DISPLAY
  display.clear();
  display.setFont(System5x7);
  display.set1X();
  printCenteredMenuTitle(display, "About");
  display.setCursor(0, 1);
  display.print(getProductAboutDisplayTitle());
  display.setCursor(0, 2);
  display.print(F("Firmware: " FIRMWARE_VERSION_STRING));
  display.setCursor(0, 3);
  display.print(F("Built: " FIRMWARE_BUILD_DATE));
  display.setCursor(0, 4);
  display.print(F("Hardware: " FIRMWARE_HARDWARE_STRING));
  display.setCursor(0, 5);
  display.print(F("USB interval: "));
  #ifdef ADAPT_OUTPUT_USB_DEVICE
  const uint32_t intervalUs = outputUsbInputIntervalUs();
  #else
  constexpr uint32_t intervalUs = 0;
  #endif
  if (intervalUs == 0) {
    display.print(F("N/A"));
  } else if (intervalUs % 1000u == 0) {
    display.print(intervalUs / 1000u);
    display.print(F(" ms"));
  } else {
    display.print(intervalUs);
    display.print(F(" us"));
  }
  display.setCursor(0, 6);
  display.print(F("reflex.shop"));
  display.setCursor(0, 7);
  #ifdef ADAPT_OUTPUT_USB_DEVICE
  display.print(F("github: Reflex-Adapt"));
  #else
  // With no USB device there is no serial or HID report to read the build
  // from, so a tester reads it here. The row fits 21 columns, enough digits to
  // find the build. The whole string stays in the image for the release check.
  #ifndef ADAPT_IDENTITY_BUILD
  #define ADAPT_IDENTITY_BUILD "unknown"
  #endif
  static const char identity[] =
      "VERSION=" FIRMWARE_VERSION_STRING " BUILD=" ADAPT_IDENTITY_BUILD;
  const char* build = identity + sizeof("VERSION=" FIRMWARE_VERSION_STRING " BUILD=") - 1;
  display.print(F("Build: "));
  for (uint8_t column = 7; column < 21 && *build; ++column) {
    display.print(*build++);
  }
  #endif
  #endif
}
