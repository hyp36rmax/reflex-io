#pragma once

#ifdef ADAPT_HAS_USB_HOST_STACK
  // PIO-USB boards need a concrete D+ GPIO. Native-host boards use the
  // RP2040 USB_DP/USB_DM peripheral pins and intentionally have no GPIO pair.
  #if !defined(ADAPT_USB_HOST_NATIVE_RHPORT)
    #ifndef PIN_USB_HOST_DP
      #ifdef PIN_USB_HOST_DP_DEFAULT
        #define PIN_USB_HOST_DP PIN_USB_HOST_DP_DEFAULT
      #endif
    #endif
  #endif
#endif
