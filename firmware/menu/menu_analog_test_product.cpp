#include "menu_analog_test_product.h"

#ifdef ADAPT_CAP_USB_INPUT_ANALOG_PROFILES
#include "../features/modern2usb_analog/modern2usb_analog_feature.h"
#endif

bool renderProductAnalogTest(bool modeBtnJustPressed,
                             bool navigateBtnJustPressed) {
#ifdef ADAPT_CAP_USB_INPUT_ANALOG_PROFILES
  modern2usbAnalogRenderTestScreen(modeBtnJustPressed, navigateBtnJustPressed);
  return true;
#else
  (void)modeBtnJustPressed;
  (void)navigateBtnJustPressed;
  return false;
#endif
}
