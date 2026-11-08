#pragma once

#if defined(PRODUCT_CLASSIC2USB)
  #define ADAPT_SELECTED_PRODUCT_HEADER "config/products/classic2usb.h"
#elif defined(PRODUCT_MODERN2USB)
  #define ADAPT_SELECTED_PRODUCT_HEADER "config/products/modern2usb.h"
#elif defined(PRODUCT_JVSIO2USB_MODERN2USBDIAG)
  #define ADAPT_SELECTED_PRODUCT_HEADER "config/products/jvsio2usb_modern2usbdiag.h"
#elif defined(PRODUCT_JVSIO2USB)
  #define ADAPT_SELECTED_PRODUCT_HEADER "config/products/jvsio2usb.h"
#elif defined(PRODUCT_USB2JVS)
  #define ADAPT_SELECTED_PRODUCT_HEADER "config/products/usb2jvs.h"
#elif defined(PRODUCT_USB2RF)
  #define ADAPT_SELECTED_PRODUCT_HEADER "config/products/usb2rf.h"
#elif defined(PRODUCT_USB2CLASSIC)
  #define ADAPT_SELECTED_PRODUCT_HEADER "config/products/usb2classic.h"
#elif defined(PRODUCT_USB2DB15)
  #define ADAPT_SELECTED_PRODUCT_HEADER "config/products/usb2db15.h"
#elif defined(PRODUCT_CLASSIC2CLASSIC)
  #define ADAPT_SELECTED_PRODUCT_HEADER "config/products/classic2classic.h"
#elif defined(PRODUCT_REFLEX_ENCODE)
  #define ADAPT_SELECTED_PRODUCT_HEADER "config/products/reflex_encode.h"
#elif defined(PRODUCT_REFLEX_CTRL)
  #define ADAPT_SELECTED_PRODUCT_HEADER "config/products/reflex_ctrl.h"
#elif defined(PRODUCT_MISTERCADE_V2)
  #define ADAPT_SELECTED_PRODUCT_HEADER "config/products/mistercade_v2.h"
#elif defined(PRODUCT_MISTERCADE_2P_SIMPLE)
  #define ADAPT_SELECTED_PRODUCT_HEADER "config/products/mistercade_2p_simple.h"
#elif defined(ADAPT_PRIVATE_PRODUCT)
  // The private overlay is included before this selector and owns the same
  // profile contract without exposing private product details here.
#else
  // Classic2USB remains the default only in the Dev composition layer.
  #define PRODUCT_CLASSIC2USB
  #define ADAPT_SELECTED_PRODUCT_HEADER "config/products/classic2usb.h"
#endif
