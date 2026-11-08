#pragma once

#ifndef PRODUCT_CONFIG_H
#define PRODUCT_CONFIG_H

#include "config/product_select.h"

#if defined(ADAPT_PRIVATE_PRODUCT) && !defined(ADAPT_PRODUCT_OVERLAY_HEADER)
  #error "Private products must supply ADAPT_PRODUCT_OVERLAY_HEADER"
#endif
#if defined(ADAPT_PRODUCT_OVERLAY_HEADER)
  #include ADAPT_PRODUCT_OVERLAY_HEADER
#endif

#if defined(ADAPT_SELECTED_PRODUCT_HEADER)
  #include ADAPT_SELECTED_PRODUCT_HEADER
#elif !defined(ADAPT_PRIVATE_PRODUCT)
  #error "No Adapt product profile selected"
#endif
#include "config/product_roles.h"
#include "config/hardware_capabilities.h"
#include "config/product_capabilities.h"
#if defined(ADAPT_PRODUCT_CAPABILITY_EXTENSION_HEADER)
  #include ADAPT_PRODUCT_CAPABILITY_EXTENSION_HEADER
#endif
#include "config/product_policy_defaults.h"
#include "config/feature_gates.h"
#if defined(ADAPT_PRODUCT_FEATURE_EXTENSION_HEADER)
  #include ADAPT_PRODUCT_FEATURE_EXTENSION_HEADER
#endif
#ifndef ADAPT_BOARD_PROFILE_HEADER
  #define ADAPT_BOARD_PROFILE_HEADER "config/board_profiles/adapt_boards.h"
#endif
#include ADAPT_BOARD_PROFILE_HEADER
#if defined(ADAPT_HARDWARE_STUB)
  #include "config/board_profiles/hardware_stub.h"
#endif
#include "config/hardware_contracts.h"
#if defined(ADAPT_PRODUCT_HARDWARE_CONTRACT_EXTENSION_HEADER)
  #include ADAPT_PRODUCT_HARDWARE_CONTRACT_EXTENSION_HEADER
#endif
#include "config/product_identity.h"
#include "config/default_modes.h"

#include "config/usb_config.h"

#endif // PRODUCT_CONFIG_H
