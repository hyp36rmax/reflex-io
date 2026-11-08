#include "../../product_config.h"
#include "../../firmware_platform_config.h"

#include <Arduino.h>

#include "../../platform/button_handler.h"
#include "../../platform/buzzer.h"
#include "Input_AutoDetect.h"
#include "input_autodetect_saturn_probe_internal.h"
#if defined(ENABLE_EXPERIMENTAL_CONSOLE_MOUSE) && defined(ENABLE_INPUT_SATURN)
#include "saturn_mouse_autodetect.h"

namespace {
struct AutoSaturnMouseBus {
  const autodetect_pins_t& pins;
#ifdef AUTODETECT_DEBUG
  uint8_t attempt = 0, count = 0, nibbles[10] = {};
  bool timeout = false;
#endif
  void begin() {
#ifdef AUTODETECT_DEBUG
    ++attempt;
    count = 0;
    timeout = false;
    for (uint8_t& nibble : nibbles) nibble = 0;
#endif
    gpio_put(pins.sat_th, 1);
    gpio_set_dir(pins.sat_th, GPIO_OUT);
    gpio_put(pins.sat_tr, 1);
    gpio_set_dir(pins.sat_tr, GPIO_OUT);
  }
  void end() {
    gpio_put(pins.sat_th, 1);
    gpio_put(pins.sat_tr, 1);
    gpio_set_dir(pins.sat_th, GPIO_IN);
    gpio_set_dir(pins.sat_tr, GPIO_IN);
    gpio_pull_up(pins.sat_th);
    gpio_pull_up(pins.sat_tr);
  }
  void th(bool value) { gpio_put(pins.sat_th, value); }
  void tr(bool value) { gpio_put(pins.sat_tr, value); }
  void waitUs(unsigned us) { delayMicroseconds(us); }
  bool waitTl(bool value) {
    const uint32_t start = micros();
    while (gpio_get(pins.sat_tl) != value) {
      if (uint32_t(micros() - start) >= 100) {
#ifdef AUTODETECT_DEBUG
        timeout = true;
#endif
        return false;
      }
    }
    return true;
  }
  uint8_t nibble() {
    const uint32_t values = gpio_get_all();
    const uint8_t sample = ((values >> pins.sat_d0) & 1) | (((values >> pins.sat_d1) & 1) << 1) |
           (((values >> pins.sat_d2) & 1) << 2) | (((values >> pins.sat_d3) & 1) << 3);
#ifdef AUTODETECT_DEBUG
    if (count < 10) nibbles[count++] = sample;
#endif
    return sample;
  }
};
bool confirmSaturnMouseDirect(const autodetect_pins_t& pins, uint8_t port) {
  AutoSaturnMouseBus bus{pins};
  const bool result = confirmAutoSaturnMouse(bus);
#ifdef AUTODETECT_DEBUG
  if (port < kAutoDetectDebugPortCount) {
    AutoDetectDebug& debug = lastDebug[port];
    debug.sat_mouse_attempt = bus.attempt;
    debug.sat_mouse_count = bus.count;
    debug.sat_mouse_timeout = bus.timeout;
    for (uint8_t i = 0; i < 10; ++i) debug.sat_mouse_nibbles[i] = bus.nibbles[i];
  }
#else
  (void)port;
#endif
  return result;
}
}
#endif

#ifdef ENABLE_INPUT_AUTODETECT
#if defined(ENABLE_EXPERIMENTAL_CONSOLE_MOUSE) && defined(ENABLE_INPUT_SATURN)
bool AutoDetector::confirmSaturnMouse(const autodetect_pins_t& pins, uint8_t port,
                                      bool is_hotswap) {
  initializeSaturnProbeBus(pins, is_hotswap);
  return confirmSaturnMouseDirect(pins, port);
}
#endif

AutoDetectResult AutoDetector::probeSaturn(const autodetect_pins_t& pins, uint8_t port, bool* sawBusActivity, bool is_hotswap) {
  const uint8_t sat_settle_us = 10;
  SaturnProbeState state;

  initializeSaturnProbeBus(pins, is_hotswap);
#if defined(ENABLE_EXPERIMENTAL_CONSOLE_MOUSE) && defined(ENABLE_INPUT_SATURN)
  if (confirmSaturnMouseDirect(pins, port)) {
    if (sawBusActivity) *sawBusActivity = true;
    return AUTODETECT_SATURN;
  }
  gpio_set_dir(pins.sat_th, GPIO_OUT);
#endif
  captureSaturnProbeSamples(pins, sat_settle_us, state);
  retrySaturnQuietWindow(pins, sat_settle_us, state);
  retrySaturnToggleWindow(pins, sat_settle_us, state);

  if (passiveTlOnlyLow(state)) {
    if (sawBusActivity != nullptr) {
      *sawBusActivity = false;
    }
    return AUTODETECT_NONE;
  }

  if (passivePin12DirectSignature(state)) {
    if (sawBusActivity != nullptr) {
      *sawBusActivity = false;
    }
    return AUTODETECT_NONE;
  }

  confirmSaturnViaLibraries(pins, port, is_hotswap, state);
  applySaturnInvertedFallback(pins, state);
  applySaturnSatlibStableFallback(state);

  bool bus_active = saturnBusActive(state);
  if (sawBusActivity != nullptr) {
    *sawBusActivity = bus_active;
  }

  #ifdef AUTODETECT_DEBUG
  if (port < kAutoDetectDebugPortCount) {
    lastDebug[port].saturn_hits = state.saturn_hits;
    lastDebug[port].megadrive_hits = state.megadrive_hits;
    lastDebug[port].saturn_activity_hits = state.activity_hits;
    lastDebug[port].saturn_bus_active = bus_active;
    lastDebug[port].sat_nibble0_first = state.first_nibble_0;
    lastDebug[port].sat_nibble1_first = state.first_nibble_1;
    lastDebug[port].sat_nibble0_last = state.last_nibble_0;
    lastDebug[port].sat_nibble1_last = state.last_nibble_1;
    lastDebug[port].sat_tl0_first = state.first_tl_0;
    lastDebug[port].sat_tr0_first = state.first_tr_0;
    lastDebug[port].sat_tl1_first = state.first_tl_1;
    lastDebug[port].sat_tr1_first = state.first_tr_1;
    lastDebug[port].sat_tl0_last = state.last_tl_0;
    lastDebug[port].sat_tr0_last = state.last_tr_0;
    lastDebug[port].sat_tl1_last = state.last_tl_1;
    lastDebug[port].sat_tr1_last = state.last_tr_1;
    lastDebug[port].saturn_inv_fallback = state.saturn_inv_fallback;
  }
  #endif

  if (state.multitap_detected) {
    return AUTODETECT_NONE;
  }

  if (state.saturn_hits > 0) {
    return AUTODETECT_SATURN;
  }

  if (state.megadrive_hits >= 3) {
    return AUTODETECT_MEGADRIVE;
  }

  if (state.activity_hits > 0 && state.megadrive_hits > 0) {
    return AUTODETECT_SATURN;
  }

  if (state.megadrive_hits > 0) {
    return AUTODETECT_MEGADRIVE;
  }

  return AUTODETECT_NONE;
}

#endif
