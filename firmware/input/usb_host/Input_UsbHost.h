#pragma once

#include "../base/RZInputModule.h"

// USB host port can be native or pio-usb:
// Tools -> USB Stack -> Adafruit TinyUSB Host (native)
// Tools -> USB Stack -> Adafruit TinyUSB
// If using PIO-USB, must also set D+ pin as PIN_USB_HOST_DP
//
// IMPORTANT: PIO-USB Requirements for Reflex Adapt 2:
// 1. CPU Speed: Must be 120MHz or 240MHz (Tools -> CPU Speed)
//    - Default 133MHz will NOT work and will hang!
// 2. USB Stack: "Adafruit TinyUSB" (Tools -> USB Stack)
// 3. Install libraries: Adafruit TinyUSB Library, Pico PIO USB
// 4. D+ pin is GPIO6, D- is GPIO7 (consecutive required by PIO-USB)

// USB D+ pin (only if not using the native port for host)
#ifndef USE_TINYUSB_HOST
  #if defined(ARDUINO_WAVESHARE_RP2040_PIZERO)
    // The on-board PIO pins on PiZero are known to have problems with some devices. I recommend to use another pins
    #define PIN_USB_HOST_DP 6 // default pin on PiZero
  #elif defined(ARDUINO_ADAFRUIT_FEATHER_RP2040_USB_HOST)
    //#define PIN_USB_HOST_DP       16 // Arduino Core already sets this
    #define PIN_USB_HOST_POWER    PIN_5V_EN //18
  #elif defined(ARDUINO_CYTRON_MOTION_2350_PRO)
    //#define PIN_USB_HOST_DP       24 // Arduino Core already sets this
  #else
    #ifdef PIN_USB_HOST_DP_DEFAULT
      #define PIN_USB_HOST_DP       PIN_USB_HOST_DP_DEFAULT  // From product_config.h
    #else
      #define PIN_USB_HOST_DP       6   // Fallback: Reflex Adapt 2 (D+ = GPIO6, D- = GPIO7)
    #endif
  #endif
#endif

#ifndef MAX_USB_DEVICES
#define MAX_USB_DEVICES 4 // set to 4 if using a wireless xbox360 receiver?
#endif

//#define INPUTUSB_USE_DEFAULT_CALLBACK_HANDLE

#include <input_usb.h>
#include "input_usb_host_runtime_state.h"

#ifdef ENABLE_INPUT_USB
class RZInputUsbHost : public RZInputModule {
  public:
    RZInputUsbHost();

    const char* getUsbId() override;
    void configureBcdDeviceVersion() override;
    const char* getDescription() override;
    void setup() override;
    void setup2() override;
    bool poll() override;
};
#endif
