#pragma once

// One codebase ships several products that mature independently, so a version
// is per product, not per tree. The selected profile may define its own
// PRODUCT_VERSION_* before this header; the values below are the shared
// fallback for products that have not diverged yet. Include the profile first
// so the override is visible no matter which module reaches this header first.
#include "product_config.h"

#if defined(PRODUCT_VERSION_MAJOR) && !defined(FIRMWARE_VERSION_MAJOR)
#define FIRMWARE_VERSION_MAJOR PRODUCT_VERSION_MAJOR
#endif
#if defined(PRODUCT_VERSION_MINOR) && !defined(FIRMWARE_VERSION_MINOR)
#define FIRMWARE_VERSION_MINOR PRODUCT_VERSION_MINOR
#endif
#if defined(PRODUCT_VERSION_PATCH) && !defined(FIRMWARE_VERSION_PATCH)
#define FIRMWARE_VERSION_PATCH PRODUCT_VERSION_PATCH
#endif
#if defined(PRODUCT_VERSION_STRING) && !defined(FIRMWARE_VERSION_STRING)
#define FIRMWARE_VERSION_STRING PRODUCT_VERSION_STRING
#endif

#ifndef FIRMWARE_VERSION_MAJOR
#define FIRMWARE_VERSION_MAJOR 1
#endif
#ifndef FIRMWARE_VERSION_MINOR
#define FIRMWARE_VERSION_MINOR 1
#endif
#ifndef FIRMWARE_VERSION_PATCH
#define FIRMWARE_VERSION_PATCH 0
#endif
#ifndef FIRMWARE_VERSION_STRING
#define FIRMWARE_VERSION_STRING "1.1.0"
#endif
#ifndef FIRMWARE_VERSION_TAG
#define FIRMWARE_VERSION_TAG "v" FIRMWARE_VERSION_STRING
#endif
#define FIRMWARE_BUILD_DATE __DATE__
#define FIRMWARE_BUILD_TIME __TIME__
#define FIRMWARE_HARDWARE_STRING "RP2040 2MB"
