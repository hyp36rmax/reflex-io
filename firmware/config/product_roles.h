#pragma once

// Compatibility aliases consumed by the common module gates.
#if defined(ADAPT_PRIMARY_INGRESS_USB_DEVICE)
  #define ADAPT_INPUT_USB_HOST
#endif
#if defined(ADAPT_PRIMARY_INGRESS_ESP32)
  #define ADAPT_INPUT_ESP32
#endif
#if defined(ADAPT_PRIMARY_INGRESS_JVS)
  #define ADAPT_INPUT_JVS
#endif
#if defined(ADAPT_PRIMARY_INGRESS_RETRO)
  #define ADAPT_INPUT_RETRO
#endif
#if defined(ADAPT_PRIMARY_INGRESS_DIRECT_GPIO)
  #define ADAPT_INPUT_DIRECT_GPIO
#endif
#if defined(ADAPT_PRIMARY_EGRESS_USB_HOST)
  #define ADAPT_OUTPUT_USB_DEVICE
#endif
