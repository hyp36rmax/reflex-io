#pragma once

#if defined(ADAPT_FEATURE_OLED_DISPLAY) && !defined(ADAPT_HAS_OLED)
  #error "OLED display feature requires ADAPT_HAS_OLED"
#endif
#if defined(ADAPT_HAS_OLED) && !defined(ADAPT_HARDWARE_STUB) && \
    (!defined(PIN_OLED_SDA) || !defined(PIN_OLED_SCL))
  #error "OLED capability requires SDA and SCL pins"
#endif

#if defined(ADAPT_FEATURE_BUZZER) && !defined(ADAPT_HAS_BUZZER)
  #error "Buzzer feature requires ADAPT_HAS_BUZZER"
#endif
#if defined(ADAPT_HAS_BUZZER) && !defined(ADAPT_HARDWARE_STUB) && \
    !defined(PIN_BUZZER) && !defined(PIN_BUZZER_LED)
  #error "Buzzer capability requires PIN_BUZZER or PIN_BUZZER_LED"
#endif

#if defined(ADAPT_FEATURE_RGB_LED) && !defined(ADAPT_HAS_RGB_LED)
  #error "RGB LED feature requires ADAPT_HAS_RGB_LED"
#endif
#if defined(ADAPT_HAS_RGB_LED) && !defined(ADAPT_HARDWARE_STUB) && \
    !defined(PIN_RGB_LED) && !defined(PIN_STATUS_LED) && \
    !defined(PIN_BUZZER_LED)
  #error "RGB LED capability requires a supported LED pin"
#endif

#if defined(ADAPT_HAS_MODE_BUTTON) && !defined(ADAPT_HARDWARE_STUB)
  #if !defined(PIN_MODE_BTN) || PIN_MODE_BTN == 0xFF || PIN_MODE_BTN == 255
    #error "Mode-button capability requires a real GPIO"
  #endif
#endif
#if defined(ADAPT_HAS_RESET_BUTTON) && !defined(PIN_RESET_BTN)
  #error "Reset-button capability requires PIN_RESET_BTN"
#endif
#if defined(ADAPT_HAS_DEBUG_UART) && \
    (!defined(PIN_DEBUG_UART_TX) || !defined(PIN_DEBUG_UART_RX))
  #error "Debug-UART capability requires TX and RX pins"
#endif
#if defined(ENABLE_DEBUG_UART) && !defined(ADAPT_HAS_DEBUG_UART)
  #error "Debug UART was enabled on hardware without that capability"
#endif

#if defined(ADAPT_HAS_USB_HOST_STACK) && \
    !defined(ADAPT_USB_HOST_NATIVE_RHPORT) && \
    !defined(PIN_USB_HOST_DP) && !defined(PIN_USB_HOST_DP_DEFAULT)
  #error "PIO USB host-stack capability requires a D+ pin"
#endif
#if defined(ADAPT_USB_HOST_NATIVE_RHPORT) && \
    !defined(ADAPT_HAS_USB_HOST_STACK)
  #error "Native USB-host routing requires the USB host stack"
#endif
#if defined(ADAPT_USB_HOST_NATIVE_RHPORT) && defined(ADAPT_OUTPUT_USB_DEVICE)
  #error "RP2040 native USB cannot be host and device simultaneously"
#endif
#if defined(ADAPT_USB_HOST_NATIVE_RHPORT) && !defined(USE_TINYUSB_HOST)
  #error "Native RP2040 USB host requires the Arduino-Pico USE_TINYUSB_HOST stack"
#endif
#if defined(ADAPT_HAS_USB_AUTH_SIDECAR) && \
    !defined(ADAPT_HAS_USB_HOST_STACK)
  #error "USB auth sidecar requires the USB host stack"
#endif
#if defined(ADAPT_HAS_ONBOARD_USB_HUB) && \
    (!defined(ADAPT_USB_HOST_LEFT_ROOT_PORT) || \
     !defined(ADAPT_USB_HOST_RIGHT_ROOT_PORT) || \
     !defined(ADAPT_USB_HOST_HAS_ONBOARD_HUB))
  #error "Onboard USB hub capability requires both root-port identities"
#endif

#if defined(ADAPT_HAS_LATENCY_HARNESS) && \
    (!defined(PIN_LATENCY_PULSE) || !defined(PIN_LATENCY_RETURN))
  #error "Latency-harness capability requires pulse and return pins"
#endif

#if defined(ENABLE_NFC) && !defined(ADAPT_HAS_NFC_BRIDGE)
  #error "NFC was enabled without ADAPT_HAS_NFC_BRIDGE"
#endif
#if defined(ADAPT_HAS_NFC_BRIDGE) && \
    (!defined(PIN_NFC_SDA) || !defined(PIN_NFC_SCL))
  #error "NFC capability requires dedicated NFC I2C pin aliases"
#endif
#if defined(ADAPT_HAS_RAIL_MONITOR) && \
    !defined(ADAPT_RAIL_MONITOR_BACKEND_READY)
  #error "Rail-monitor capability requires a private sensor backend contract"
#endif

#if defined(ADAPT_HARDWARE_STUB) && \
    (defined(ADAPT_HAS_OLED) || defined(ADAPT_HAS_BUZZER) || \
     defined(ADAPT_HAS_RGB_LED) || defined(ADAPT_HAS_MODE_BUTTON) || \
     defined(ADAPT_HAS_RESET_BUTTON) || defined(ADAPT_HAS_DEBUG_UART) || \
     defined(ADAPT_HAS_USB_HOST_STACK) || \
     defined(ADAPT_HAS_USB_AUTH_SIDECAR) || \
     defined(ADAPT_HAS_ONBOARD_USB_HUB) || \
     defined(ADAPT_HAS_NFC_BRIDGE) || defined(ADAPT_HAS_RAIL_MONITOR))
  #error "Hardware stubs cannot claim fitted peripherals"
#endif
