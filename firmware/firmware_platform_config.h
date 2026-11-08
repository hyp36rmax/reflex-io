#pragma once

// Shared board/product preprocessor configuration that used to live inline in
// the main firmware entrypoint.

#ifdef LED_BUILTIN
  #undef LED_BUILTIN
#endif
//#define LED_BUILTIN 29

// Compatibility aliases for implementation code. The common capability and
// feature layers are authoritative; pin presence alone never enables hardware.
#ifdef ADAPT_FEATURE_BUZZER
  #define USE_BUZZER
#endif

#ifdef ADAPT_FEATURE_RGB_LED
  #define USE_WS2812
#endif

// Pin assignments from product_config.h
#ifdef ADAPT_FEATURE_MODE_BUTTON
  #define pinBtn PIN_MODE_BTN
#else
  #define pinBtn 0xFF
#endif

// OLED display (available on most products). Debug hardware variants can
// deliberately reclaim GPIO0/1 for CH340 UART by defining DISABLE_OLED_DISPLAY.
#ifdef ADAPT_FEATURE_OLED_DISPLAY
  #define USE_I2C_DISPLAY
  #define OLED_I2C_SDA PIN_OLED_SDA
  #define OLED_I2C_SCL PIN_OLED_SCL
#endif

// HDMI connector pins for products that physically expose the legacy Adapt 2
// HDMI controller headers.
#if defined(PIN_HDMI1_01) && defined(PIN_HDMI2_13)
  #define HDMI_1_01 PIN_HDMI1_01
  #define HDMI_1_02 PIN_HDMI1_02
  #define HDMI_1_03 PIN_HDMI1_03
  #define HDMI_1_04 PIN_HDMI1_04
  #define HDMI_1_05 PIN_HDMI1_05
  #define HDMI_1_06 PIN_HDMI1_06
  #define HDMI_1_07 PIN_HDMI1_07
  #define HDMI_1_08 PIN_HDMI1_08
  #define HDMI_1_09 PIN_HDMI1_09
  #define HDMI_1_10 PIN_HDMI1_10
  #define HDMI_1_11 PIN_HDMI1_11
  #define HDMI_1_12 PIN_HDMI1_12
  #define HDMI_1_13 PIN_HDMI1_13

  #define HDMI_2_01 PIN_HDMI2_01
  #define HDMI_2_02 PIN_HDMI2_02
  #define HDMI_2_03 PIN_HDMI2_03
  #define HDMI_2_04 PIN_HDMI2_04
  #define HDMI_2_05 PIN_HDMI2_05
  #define HDMI_2_06 PIN_HDMI2_06
  #define HDMI_2_07 PIN_HDMI2_07
  #define HDMI_2_08 PIN_HDMI2_08
  #define HDMI_2_09 PIN_HDMI2_09
  #define HDMI_2_10 PIN_HDMI2_10
  #define HDMI_2_11 PIN_HDMI2_11
  #define HDMI_2_12 PIN_HDMI2_12
  #define HDMI_2_13 PIN_HDMI2_13
#elif defined(PRODUCT_JVSIO2USB)
  // JVSIO2USB has no HDMI controller headers. Keep the legacy symbol surface
  // for dormant compile units, but park it on otherwise unconnected board GPIOs
  // so nothing can overlap the real board signals.
  #define HDMI_1_01 5
  #define HDMI_1_02 6
  #define HDMI_1_03 7
  #define HDMI_1_04 8
  #define HDMI_1_05 9
  #define HDMI_1_06 10
  #define HDMI_1_07 11
  #define HDMI_1_08 14
  #define HDMI_1_09 15
  #define HDMI_1_10 16
  #define HDMI_1_11 17
  #define HDMI_1_12 18
  #define HDMI_1_13 19

  #define HDMI_2_01 20
  #define HDMI_2_02 21
  #define HDMI_2_03 22
  #define HDMI_2_04 23
  #define HDMI_2_05 24
  #define HDMI_2_06 25
  #define HDMI_2_07 5
  #define HDMI_2_08 6
  #define HDMI_2_09 7
  #define HDMI_2_10 8
  #define HDMI_2_11 9
  #define HDMI_2_12 10
  #define HDMI_2_13 11
#endif

// Maximum USB controller slots exposed by the firmware.
// Keep this in shared config instead of out_usb.h so helper modules do not
// need to include the entire output stack just to size arrays.
#if defined(DEBUG_RP2040_PORT) && DEBUG_RP2040_PORT == Serial
  #define MAX_USB_OUT 5
#else
  #define MAX_USB_OUT 6  // Saturn 6-player multitap support
#endif

// Watchdog scratch registers survive soft reset but clear on power cycle.
// Used for passive controller modes that shouldn't persist across power cycles.
#define PASSIVE_MODE_MAGIC 0x50415353  // "PASS" - marker for valid passive mode
#define PASSIVE_MODE_SCRATCH_MAGIC 0   // Scratch register for magic value
#define PASSIVE_MODE_SCRATCH_MODE 1    // Scratch register for mode value

// MODERN2USB firmware deployment temporarily reuses scratch 0/1 while entering
// ROM BOOTSEL. A magic/complement pair cannot be mistaken for a valid passive
// input-mode request, and the newly installed firmware consumes it before
// passive-mode restoration.
#define MODERN2USB_FW_UPDATE_BOOT_HOLD_MAGIC 0x46575550u  // "FWUP"
#define MODERN2USB_FW_UPDATE_BOOT_HOLD_SCRATCH_MAGIC 0
#define MODERN2USB_FW_UPDATE_BOOT_HOLD_SCRATCH_COMPLEMENT 1

// Reflex CTRL and Encode use scratch 0/1 the same way (reflex_direct/adapt_session.h).

// Shared scratch registers used by AUTO output/input chaining across soft reboots.
#define AUTO_DETECT_SCRATCH_MAGIC 2
#define AUTO_DETECT_SCRATCH_STATE 3
#define AUTO_DETECT_MAGIC 0x41555448
#define AUTO_INPUT_MODE_SCRATCH_MAGIC 4
#define AUTO_INPUT_MODE_SCRATCH_MODE 5
#define AUTO_INPUT_MODE_MAGIC 0x41554D44
#define AUTO_PROBE_SCRATCH_STAGE 6
#define AUTO_PROBE_SCRATCH_AUX 7
