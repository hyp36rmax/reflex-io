#pragma once

#include "direct_gpio_boards.h"

// ============================================================================
// Pin Assignments
// ============================================================================

#ifdef PRODUCT_CLASSIC2USB
  // ---- Reflex Adapt 2 board (RP2040) ----
  // Sonik's minimal board and the explicit RetroZord HDMI diagnostic use the
  // same alternate two-port map. Only Sonik omits the extra board hardware.
  #if defined(CLASSIC2USB_RETROZORD_HDMI_PINOUT) || \
      defined(CLASSIC2USB_SONIK_PINOUT)
    #define ADAPT_CLASSIC2USB_ALTERNATE_HDMI_PINOUT 1
  #endif

  // Mode button
  #ifdef CLASSIC2USB_SONIK_PINOUT
    #define PIN_MODE_BTN    255
    #define DISABLE_MODE_BUTTON_HARDWARE 1
  #elif defined(CLASSIC2USB_RETROZORD_HDMI_PINOUT)
    #define PIN_MODE_BTN    14
  #else
    #define PIN_MODE_BTN    18
  #endif

  // OLED I2C
  #define PIN_OLED_SDA      0
  #define PIN_OLED_SCL      1

  // GPIO8 is HDMI 1 pin 4 on Sonik's board.
  #ifdef CLASSIC2USB_SONIK_PINOUT
    #define DISABLE_BUZZER_HARDWARE 1
    #define DISABLE_RGB_LED_PIO 1
  #else
    #define PIN_BUZZER_LED    8
  #endif

  #ifdef ADAPT_CLASSIC2USB_ALTERNATE_HDMI_PINOUT
    // HDMI Connector 1 (retro controller port 1), Sonik / alternate map
    #define PIN_HDMI1_01      13
    #define PIN_HDMI1_02      12
    #define PIN_HDMI1_03       3
    #define PIN_HDMI1_04       8
    #define PIN_HDMI1_05       9
    #define PIN_HDMI1_06      10
    #define PIN_HDMI1_07      11
    #define PIN_HDMI1_08       5
    #define PIN_HDMI1_09       6
    #define PIN_HDMI1_10       2
    #define PIN_HDMI1_11       7
    #define PIN_HDMI1_12       4
    #define PIN_HDMI1_13      15

    // HDMI Connector 2 (retro controller port 2), Sonik / alternate map
    #define PIN_HDMI2_01      17
    #define PIN_HDMI2_02      16
    #define PIN_HDMI2_03      18
    #define PIN_HDMI2_04      19
    #define PIN_HDMI2_05      20
    #define PIN_HDMI2_06      21
    #define PIN_HDMI2_07      28
    #define PIN_HDMI2_08      23
    #define PIN_HDMI2_09      22
    #define PIN_HDMI2_10      26
    #define PIN_HDMI2_11      27
    #define PIN_HDMI2_12      24
    #define PIN_HDMI2_13      25
  #else
    // HDMI Connector 1 (retro controller port 1), RetroZord active "new" map
    #define PIN_HDMI1_01      11
    #define PIN_HDMI1_02      10
    #define PIN_HDMI1_03      26
    #define PIN_HDMI1_04      27
    #define PIN_HDMI1_05       9
    #define PIN_HDMI1_06      12
    #define PIN_HDMI1_07      13
    #define PIN_HDMI1_08       5
    #define PIN_HDMI1_09       6
    #define PIN_HDMI1_10       2
    #define PIN_HDMI1_11       7
    #define PIN_HDMI1_12       4
    #define PIN_HDMI1_13       3

    // HDMI Connector 2 (retro controller port 2), RetroZord active "new" map
    #define PIN_HDMI2_01      23
    #define PIN_HDMI2_02      22
    #define PIN_HDMI2_03      28
    #define PIN_HDMI2_04      29
    #define PIN_HDMI2_05      20
    #define PIN_HDMI2_06      21
    #define PIN_HDMI2_07      19
    #define PIN_HDMI2_08      16
    #define PIN_HDMI2_09      17
    #define PIN_HDMI2_10      14
    #define PIN_HDMI2_11      15
    #define PIN_HDMI2_12      24
    #define PIN_HDMI2_13      25
  #endif

  // Sonik has no PIO-USB receptacle, and GPIO6 is HDMI 1 pin 9.
  #ifndef CLASSIC2USB_SONIK_PINOUT
    #define PIN_USB_HOST_DP_DEFAULT   6
  #endif

  // Current canonical RetroZord hardware uses the active "new" HDMI map.
  #ifdef CLASSIC2USB_RETROZORD_PINOUT
    #if PIN_MODE_BTN != 18 || PIN_OLED_SDA != 0 || PIN_OLED_SCL != 1 || \
        PIN_BUZZER_LED != 8 || PIN_USB_HOST_DP_DEFAULT != 6 || \
        PIN_HDMI1_01 != 11 || PIN_HDMI1_02 != 10 || PIN_HDMI1_03 != 26 || \
        PIN_HDMI1_04 != 27 || PIN_HDMI1_05 != 9 || PIN_HDMI1_06 != 12 || \
        PIN_HDMI1_07 != 13 || PIN_HDMI1_08 != 5 || PIN_HDMI1_09 != 6 || \
        PIN_HDMI1_10 != 2 || PIN_HDMI1_11 != 7 || PIN_HDMI1_12 != 4 || \
        PIN_HDMI1_13 != 3 || PIN_HDMI2_01 != 23 || PIN_HDMI2_02 != 22 || \
        PIN_HDMI2_03 != 28 || PIN_HDMI2_04 != 29 || PIN_HDMI2_05 != 20 || \
        PIN_HDMI2_06 != 21 || PIN_HDMI2_07 != 19 || PIN_HDMI2_08 != 16 || \
        PIN_HDMI2_09 != 17 || PIN_HDMI2_10 != 14 || PIN_HDMI2_11 != 15 || \
        PIN_HDMI2_12 != 24 || PIN_HDMI2_13 != 25
      #error "classic2usb_retrozord no longer matches active RetroZord hardware"
    #endif
  #endif

  #ifdef CLASSIC2USB_SONIK_PINOUT
    #if PIN_MODE_BTN != 255 || PIN_OLED_SDA != 0 || PIN_OLED_SCL != 1 || \
        PIN_HDMI1_01 != 13 || PIN_HDMI1_02 != 12 || \
        PIN_HDMI1_03 != 3 || PIN_HDMI1_04 != 8 || PIN_HDMI1_05 != 9 || \
        PIN_HDMI1_06 != 10 || PIN_HDMI1_07 != 11 || PIN_HDMI1_08 != 5 || \
        PIN_HDMI1_09 != 6 || PIN_HDMI1_10 != 2 || PIN_HDMI1_11 != 7 || \
        PIN_HDMI1_12 != 4 || PIN_HDMI1_13 != 15 || PIN_HDMI2_01 != 17 || \
        PIN_HDMI2_02 != 16 || PIN_HDMI2_03 != 18 || PIN_HDMI2_04 != 19 || \
        PIN_HDMI2_05 != 20 || PIN_HDMI2_06 != 21 || PIN_HDMI2_07 != 28 || \
        PIN_HDMI2_08 != 23 || PIN_HDMI2_09 != 22 || PIN_HDMI2_10 != 26 || \
        PIN_HDMI2_11 != 27 || PIN_HDMI2_12 != 24 || PIN_HDMI2_13 != 25
      #error "classic2usb_sonik no longer matches Sonik hardware"
    #endif
    #if !defined(DISABLE_MODE_BUTTON_HARDWARE) || \
        !defined(DISABLE_BUZZER_HARDWARE) || !defined(DISABLE_RGB_LED_PIO)
      #error "classic2usb_sonik must disable absent board hardware"
    #endif
    #if defined(PIN_BUZZER_LED) || defined(PIN_BUZZER) || \
        defined(PIN_RGB_LED) || defined(PIN_STATUS_LED) || \
        defined(PIN_USB_HOST_DP) || defined(PIN_USB_HOST_DP_DEFAULT) || \
        defined(ADAPT_HAS_USB_HOST_STACK) || \
        defined(ENABLE_INPUT_USB) || defined(ENABLE_USB_XINPUT_HOST) || \
        defined(ENABLE_USB_XINPUT_AUTH_HOST)
      #error "classic2usb_sonik cannot claim buzzer, LED, or USB-host hardware"
    #endif
  #endif
#endif

#ifdef PRODUCT_MODERN2USB
  #if defined(MODERN2USB_RETROZORD_PINOUT)
    // ---- RetroZord single-port MODERN2USB board (RP2040) ----
    // Native RP2040 USB is the device/output port; GPIO0/1 are PIO-USB host
    // D+/D-. No system-button GPIO was supplied, so use the shared no-pin
    // sentinel instead of claiming an unknown signal or entering BOOTSEL if
    // that signal happens to be held low.
    #define PIN_MODE_BTN             255
    #define PIN_USB_HOST_DP_DEFAULT  0
    #define DISABLE_MODE_BUTTON_HARDWARE 1
    #define DISABLE_OLED_DISPLAY     1
    #define DISABLE_RGB_LED_PIO      1
    #define DISABLE_BUZZER_HARDWARE  1

    #if PIN_MODE_BTN != 255 || PIN_USB_HOST_DP_DEFAULT != 0
      #error "minimal MODERN2USB GPIO0/1 pin contract drifted"
    #endif
    #if defined(ENABLE_INPUT_ESP32_SPI) || defined(ENABLE_INPUT_USB_BT) || \
        defined(ENABLE_BLUETOOTH_PAIRING) || \
        defined(ENABLE_EXPERIMENTAL_MODERN2USB_BT_OUTPUT) || \
        defined(ENABLE_ESP32_RP2040_UPDATE) || defined(ENABLE_DEBUG_UART)
      #error "minimal MODERN2USB must not compile ESP32, Bluetooth, or UART paths"
    #endif
    #if defined(PIN_OLED_SDA) || defined(PIN_OLED_SCL) || \
        defined(PIN_RGB_LED) || defined(PIN_RGB_LED_COUNT) || \
        defined(PIN_BUZZER) || defined(PIN_DEBUG_UART_TX) || \
        defined(PIN_DEBUG_UART_RX) || defined(PIN_ESP32_TX) || \
        defined(PIN_ESP32_RX) || defined(PIN_ESP32_GPIO0) || \
        defined(PIN_ESP32_EN) || defined(PIN_ESP32_SPI_MISO) || \
        defined(PIN_ESP32_SPI_CS) || defined(PIN_ESP32_SPI_SCK) || \
        defined(PIN_ESP32_SPI_MOSI) || defined(PIN_ESP32_SPI_READY) || \
        defined(ADAPT_USB_HOST_LEFT_ROOT_PORT) || \
        defined(ADAPT_USB_HOST_RIGHT_ROOT_PORT)
      #error "minimal MODERN2USB must not claim absent peripherals"
    #endif
  #else
    // ---- Reflex Adapt Modern2USB board (RP2040) ----
    // Reserve GPIO8/9 for the CH340 debug UART.
    #define PIN_MODE_BTN      2
    #define PIN_RESET_BTN     6
    #define PIN_OLED_SDA      0
    #define PIN_OLED_SCL      1
    #define PIN_RGB_LED       3
    #define PIN_RGB_LED_COUNT 2
    #define PIN_BUZZER        4

    // Standard CH340 debug UART reserved for new Adapt-family boards.
    #define PIN_DEBUG_UART_TX   8
    #define PIN_DEBUG_UART_RX   9

    // PIO-USB for host input (D+ must be even GPIO, D- is D+ + 1)
    #define PIN_USB_HOST_DP_DEFAULT   12
    // Native RP2040 USB is hardware. The downstream host port uses PIO-USB,
    // so keep PIO0 SM0 free for the WS2812 RGB LED and put USB TX on SM1.
    #ifndef PIN_USB_HOST_PIO_SM_TX
      #define PIN_USB_HOST_PIO_SM_TX  1
    #endif

    // ESP32 service UART and boot controls.
    #define PIN_ESP32_TX      16  // RP2040 TX -> ESP32 RXD0/IO3
    #define PIN_ESP32_RX      17  // RP2040 RX <- ESP32 TXD0/IO1
    #define PIN_ESP32_GPIO0    7  // ESP32 boot strap control
    #define PIN_ESP32_EN      14  // ESP32 EN/reset control

    // ESP32 SPI Bluetooth controller bridge.
    #define PIN_ESP32_SPI_MISO 20
    #define PIN_ESP32_SPI_CS   21
    #define PIN_ESP32_SPI_SCK  22
    #define PIN_ESP32_SPI_MOSI 23
    #define PIN_ESP32_SPI_READY 19
    #define PIN_ESP32_SPI_READY_ACTIVE_LOW 1
    #define ESP32_SPI_CLOCK_HZ 4000000

    // Physical FE1.1S path labels. The left receptacle is preferred P1/OLED
    // left/pixel 1; the right receptacle is preferred P2/OLED right/pixel 2.
    // Bluetooth fills the first slot not claimed by one of these receptacles.
    #define ADAPT_USB_HOST_LEFT_ROOT_PORT  2
    #define ADAPT_USB_HOST_RIGHT_ROOT_PORT 1
    #define ADAPT_USB_HOST_HAS_ONBOARD_HUB 1
  #endif
#endif

#ifdef PRODUCT_USB2JVS
  // ---- Reflex Adapt 2 board + RS-485 adapter on HDMI2 (RP2040) ----
  // Future USB2JVS board profile. Reserve GPIO8/9 for the debug UART.
  #define PIN_MODE_BTN      18
  #define PIN_OLED_SDA      0
  #define PIN_OLED_SCL      1

  // Standard CH340 debug UART reserved for new Adapt-family boards.
  #define PIN_DEBUG_UART_TX   8
  #define PIN_DEBUG_UART_RX   9

  // PIO-USB for host input
  #define PIN_USB_HOST_DP_DEFAULT   6

  // JVS RS-485 interface (via HDMI connector 2)
  #define PIN_JVS_TX        16  // HDMI_2_08 -> DI on MAX3485
  #define PIN_JVS_RX        17  // HDMI_2_09 -> RO on MAX3485
  #define PIN_JVS_DE        14  // HDMI_2_10 -> DE + /RE tied together
  #define PIN_JVS_UART_INST uart0
  #define JVS_BAUD_RATE     115200

#endif

#if defined(PRODUCT_JVSIO2USB) || defined(PRODUCT_JVSIO2USB_MODERN2USBDIAG)
  // ---- Dedicated JVSIO2USB board (RP2040) ----
  // Pinout matches the standalone JVS I/O-to-USB hardware/firmware.
  // SW1 is routed to GPIO2 through R2 on the JVSIO2USB-DEV schematic.
  #define PIN_MODE_BTN      2
  // SW2 (left navigation) is routed to GPIO6 through R4, not flash BOOTSEL.
  #define PIN_RESET_BTN     6
  #define PIN_OLED_SDA      0
  #define PIN_OLED_SCL      1
  #define PIN_RGB_LED       3
  #define PIN_RGB_LED_COUNT 2
  #define PIN_BUZZER        4

  // USB auth dongle host (PIO-USB, D+ must be even GPIO, D- is D+ + 1)
  #ifndef PIN_USB_HOST_DP_DEFAULT
    #define PIN_USB_HOST_DP_DEFAULT   12
  #endif
  // Native RP2040 USB is hardware. The auth host port uses PIO-USB, so keep
  // PIO0 SM0 free for the WS2812 RGB LED and put USB TX on SM1.
  #ifndef PIN_USB_HOST_PIO_SM_TX
    #define PIN_USB_HOST_PIO_SM_TX    1
  #endif

  // JVS RS-485 interface
  #define PIN_JVS_SENSE     26
  #define PIN_JVS_DE        27
  #define PIN_JVS_TX        28
  #define PIN_JVS_RX        29
  #define PIN_JVS_UART_INST uart0
  #define JVS_BAUD_RATE     115200

  // Optional latency harness stubs for automated bench testing.
  // Other products can define these via build flags or future board configs.
  #define PIN_LATENCY_PULSE   5
  #define PIN_LATENCY_RETURN  7

  // Optional CH340 debug UART for debug branches. GPIO8/9 avoid OLED, JVS,
  // PIO-USB, RGB, buzzer, and latency pins on the current JVSIO2USB board.
  #define PIN_DEBUG_UART_TX   8
  #define PIN_DEBUG_UART_RX   9
#endif

#ifdef PRODUCT_USB2RF
  // ---- USB2RF board (RP2040 + ESP32) ----
  #define PIN_MODE_BTN       9  // Pairing button
  #define PIN_STATUS_LED     8  // WS2812B RGB LED

  // No OLED on USB2RF (optional: can be added)
  // #define PIN_OLED_SDA    0
  // #define PIN_OLED_SCL    1

  // ESP32 UART (BT communication)
  #define PIN_ESP32_TX       0  // uart0 TX
  #define PIN_ESP32_RX       1  // uart0 RX
  #define PIN_ESP32_GPIO0    2  // Boot mode control
  #define PIN_ESP32_EN       3  // Reset/Enable
  #define ESP32_UART_INST    uart0
  #define ESP32_BAUD_RATE    921600
#endif

#ifdef PRODUCT_USB2DB15
  // ---- USB2DB15 V0.1 board pinout (RP2040) ----
  // Verified against USB2DB15-20260806 schematic and routed PCB. J16 is the
  // native RP2040 USB host port; GPIO0/1 belong to the OLED, not PIO-USB.
  #define PIN_OLED_SDA       0
  #define PIN_OLED_SCL       1
  #define PIN_MODE_BTN       2
  #define PIN_RGB_LED        3
  #define PIN_RGB_LED_COUNT  2
  #define PIN_TUSB_ID        4
  #define PIN_RESET_BTN      6

  // Single DB15/supergun player. These are active-high RP2040-side inputs to
  // fitted SN74LVC06 open-drain inverters, which sink the active-low J3 lines.
  // The parallel BSS138 level-shifter footprints are DNP on production boards.
  #define PIN_DB15_B3       16
  #define PIN_DB15_B2       17
  #define PIN_DB15_B1       18
  #define PIN_DB15_B4       19
  #define PIN_DB15_START    20
  #define PIN_DB15_COIN     21
  #define PIN_DB15_B5       22
  #define PIN_DB15_B6       23
  #define PIN_DB15_RIGHT    24
  #define PIN_DB15_LEFT     25
  #define PIN_DB15_DOWN     26
  #define PIN_DB15_UP       27

  // GPIO29 monitors the board's 5V rail through the fitted 10K/10K divider.
  #define PIN_USB2DB15_5V_SENSE 29

#endif
