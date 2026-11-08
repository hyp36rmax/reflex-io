#pragma once

#include "product_config.h"

inline const char* getProductBootDisplayTitle() {
  return PRODUCT_OLED_BOOT_TITLE;
}

inline const char* getProductHomeDisplayTitle() {
  return PRODUCT_OLED_HOME_TITLE;
}

inline bool productUsesRflxHomeLogo() {
#ifdef PRODUCT_OLED_HOME_USE_RFLX_LOGO
  return PRODUCT_OLED_HOME_USE_RFLX_LOGO != 0;
#else
  // All Reflex Adapt products share one home-screen brand. Product identity
  // remains explicit on the boot and About screens.
  return true;
#endif
}

inline const char* getProductScreensaverTitle() {
  return "REFLEX";
}

inline const char* getProductAboutDisplayTitle() {
  return PRODUCT_OLED_ABOUT_TITLE;
}

inline const char* getProductAboutRepoPath() {
  return PRODUCT_OLED_ABOUT_LINK;
}
