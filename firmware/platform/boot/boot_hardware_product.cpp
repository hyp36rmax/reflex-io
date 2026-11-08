#include "../../product_config.h"
#include "boot_hardware_product.h"
#include "boot_hardware_product_services.h"

#if defined(ADAPT_PRODUCT_HAS_EARLY_VARIANT_INIT) && \
    defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED)
void initVariant() {
  productEarlyVariantInit();
}
#endif

void prepareProductHardwareForBoot() { productPrepareHardwareForBoot(); }
