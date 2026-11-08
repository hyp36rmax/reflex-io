#pragma once

// Product selection is composition, not common firmware behavior. Every
// product tree supplies this standalone selector; shared code only consumes
// the resulting profile/header macros.
#ifndef ADAPT_PRODUCT_SELECTOR_HEADER
  #define ADAPT_PRODUCT_SELECTOR_HEADER "products/selected_product.h"
#endif
#include ADAPT_PRODUCT_SELECTOR_HEADER

#if !defined(ADAPT_SELECTED_PRODUCT_HEADER) && !defined(ADAPT_PRIVATE_PRODUCT)
  #error "Product selector did not define ADAPT_SELECTED_PRODUCT_HEADER"
#endif
