#pragma once
#include "../../product_config.h"
#include "saturnlib_features.h"

#if defined(ADAPT_FEATURE_GENESIS_IDENTIFICATION_CAPTURE)
// Preinclude dependencies before the narrowly scoped macro interception.
// Every SaturnLib consumer uses this wrapper, preserving inline-reader ODR.
#include <Arduino.h>
#include <hardware/gpio.h>
#include <pico/platform.h>
void genesisCaptureObserveWrite(uint8_t pin, bool value);
void genesisCaptureObserveRead(uint32_t pins);
void genesisCaptureObserveBit(uint8_t pin, bool value);
namespace genesis_capture_transport {
inline void put(uint pin, bool value) { gpio_put(pin, value); genesisCaptureObserveWrite(pin, value); }
inline uint32_t getAll() { const uint32_t pins = gpio_get_all(); genesisCaptureObserveRead(pins); return pins; }
inline bool get(uint pin) { const bool value = gpio_get(pin); genesisCaptureObserveBit(pin, value); return value; }
}
#pragma push_macro("gpio_put")
#pragma push_macro("gpio_get_all")
#pragma push_macro("gpio_get")
#undef gpio_put
#undef gpio_get_all
#undef gpio_get
#define gpio_put(pin, value) genesis_capture_transport::put(pin, value)
#define gpio_get_all() genesis_capture_transport::getAll()
#define gpio_get(pin) genesis_capture_transport::get(pin)
#include <SaturnLib/SaturnLib.h>
#pragma pop_macro("gpio_get")
#pragma pop_macro("gpio_get_all")
#pragma pop_macro("gpio_put")
#else
#include <SaturnLib/SaturnLib.h>
#endif
