#include "../../product_config.h"
#include "../../firmware_platform_config.h"

#include <Arduino.h>

#include "../../core/firmware_support.h"
#include "../../platform/button_handler.h"
#include "../../platform/buzzer.h"
#include "Input_AutoDetect.h"
#include "../wii/input_wii_runtime_state.h"

#ifdef ENABLE_INPUT_AUTODETECT
#ifdef ENABLE_INPUT_WII
// Wire1 panics on a pin it cannot reach. It has SDA on GPIO 2 mod 4 and SCL on
// GPIO 3 mod 4, and the alternate HDMI map puts some Wii pairs on I2C0 pins.
static bool wire1CanUsePins(uint8_t sda, uint8_t scl) {
  return sda % 4 == 2 && scl % 4 == 3;
}

AutoDetectResult AutoDetector::probeWii(const autodetect_pins_t& pins, uint8_t port) {
  constexpr uint8_t kWiiI2cAddr = 0x52;
  constexpr uint8_t kProbeAttempts = 2;
  TwoWire& wiiWire = Wire1;
  // These pins may belong to a non-I2C controller. A stuck-low Saturn data
  // line can look like an ACK; do not spend Stream's default second waiting
  // for each Wii initialization write. Keep this budget local to discovery.
  const uint32_t savedWireTimeout = wiiWire.getTimeout();
  wiiWire.setTimeout(25, false);
  bool wiiDetected = false;
  bool identityRead = false;
  bool ack52A = false;
  bool ack52B = false;
  ExtensionType wiiType = ExtensionType::NoController;
  uint8_t id[6] = {0};
  uint8_t detectedPinPair = 0xFF;
  clearWiiAutodetectIdentity(port);
  const struct {
    uint8_t sda;
    uint8_t scl;
  } wiiPinPairs[] = {
    { pins.wii_sda, pins.wii_scl },
    { pins.wii_alt_sda, pins.wii_alt_scl },
  };

  auto beginBus = [&](uint8_t sda, uint8_t scl) {
    wiiWire.setSDA(sda);
    wiiWire.setSCL(scl);
    wiiWire.setClock(INPUT_WII_WIRE_SPEED);
    wiiWire.begin();
  };

  auto probeAddress = [&](uint8_t addr) -> bool {
    wiiWire.beginTransmission(addr);
    return (wiiWire.endTransmission(true) == 0);
  };


  for (uint8_t pairIndex = 0; pairIndex <
       (uint8_t)(sizeof(wiiPinPairs) / sizeof(wiiPinPairs[0])); ++pairIndex) {
    const auto& pair = wiiPinPairs[pairIndex];
    if (!wire1CanUsePins(pair.sda, pair.scl)) {
      continue;
    }
    wiiWire.end();
    gpio_init(pair.sda);
    gpio_init(pair.scl);
    gpio_set_dir(pair.sda, GPIO_IN);
    gpio_set_dir(pair.scl, GPIO_IN);
    gpio_pull_up(pair.sda);
    gpio_pull_up(pair.scl);
    delayMicroseconds(20);
    // I2C must be idle before START. In particular, the alternate pin pair
    // overlaps Saturn D1/D0, which are actively driven by the 3D pad.
    if (!gpio_get(pair.sda) || !gpio_get(pair.scl)) {
      continue;
    }
    beginBus(pair.sda, pair.scl);
    const bool pairAck52A = probeAddress(kWiiI2cAddr);
    autoDetectDelay(4);
    const bool pairAck52B = probeAddress(kWiiI2cAddr);
    ack52A = ack52A || pairAck52A;
    ack52B = ack52B || pairAck52B;

    if (pairAck52A || pairAck52B) {
      for (uint8_t attempt = 0; attempt < kProbeAttempts && !wiiDetected; ++attempt) {
        if (attempt != 0) {
          wiiWire.end();
          autoDetectDelay(6);
          beginBus(pair.sda, pair.scl);
        }

        if (!NintendoExtensionCtrl::ExtensionController::initialize(wiiWire)) {
          autoDetectDelay(6);
          continue;
        }

        autoDetectDelay(6);
        if (NintendoExtensionCtrl::ExtensionController::requestIdentity(wiiWire, id)) {
          identityRead = true;
          wiiType = NintendoExtensionCtrl::decodeIdentity(id);
          const bool knownType =
            wiiType != ExtensionType::NoController &&
            wiiType != ExtensionType::UnknownController;
          // Identity is the reliable autodetect signal. Some valid extensions
          // do not make a control report available immediately after identity;
          // normal runtime polling validates reports with retries and debounce.
          wiiDetected = knownType;
          if (knownType) detectedPinPair = pairIndex;
        }
      }
    }

    if (wiiDetected) {
      break;
    }
  }

  wiiWire.end();
  wiiWire.setTimeout(savedWireTimeout, false);

  if (wiiDetected && detectedPinPair != 0xFF) {
    cacheWiiAutodetectIdentity(port, detectedPinPair, id);
  }

  for (const auto& pair : wiiPinPairs) {
    gpio_set_dir(pair.sda, GPIO_IN);
    gpio_set_dir(pair.scl, GPIO_IN);
    gpio_pull_up(pair.sda);
    gpio_pull_up(pair.scl);
  }

  #ifdef AUTODETECT_DEBUG
  if (port < kAutoDetectDebugPortCount) {
    lastDebug[port].wii_ack52_a = ack52A;
    lastDebug[port].wii_ack52_b = ack52B;
    lastDebug[port].wii_detected = wiiDetected;
    lastDebug[port].wii_type = (uint8_t)wiiType;
    if (identityRead) {
      lastDebug[port].wii_id2 = id[2];
      lastDebug[port].wii_id3 = id[3];
    }
  }
  #endif

  return wiiDetected ? AUTODETECT_WII : AUTODETECT_NONE;
}
#endif

#ifdef ENABLE_INPUT_DREAMCAST
AutoDetectResult AutoDetector::probeDreamcast(const autodetect_pins_t& pins, bool is_hotswap) {
  if (pins.dc_b != pins.dc_a + 1) {
    return AUTODETECT_NONE;
  }

  gpio_init(pins.dc_a);
  gpio_init(pins.dc_b);
  gpio_set_dir(pins.dc_a, GPIO_IN);
  gpio_set_dir(pins.dc_b, GPIO_IN);
  gpio_pull_up(pins.dc_a);
  gpio_pull_up(pins.dc_b);
  autoDetectDelay(2);

  if (!gpio_get(pins.dc_a) && !gpio_get(pins.dc_b)) {
    return AUTODETECT_NONE;
  }

  MaplePort probe;
  if (!probe.begin(pins.dc_a)) {
    #if defined(ENABLE_INPUT_N64) || defined(ENABLE_INPUT_GAMECUBE) || defined(ENABLE_INPUT_GBA)
    joybus_pio_unload(pio0);
    joybus_pio_unload(pio1);
    autoDetectDelay(2);
    if (!probe.begin(pins.dc_a)) {
      return AUTODETECT_NONE;
    }
    #else
    return AUTODETECT_NONE;
    #endif
  }

  const uint32_t accessoryMask = (MAPLE_FUNC_MEMORY | MAPLE_FUNC_LCD | MAPLE_FUNC_CLOCK |
                                  MAPLE_FUNC_PURUPURU | MAPLE_FUNC_MOUSE | MAPLE_FUNC_KEYBOARD |
                                  MAPLE_FUNC_MICROPHONE | MAPLE_FUNC_AR_GUN | MAPLE_FUNC_LIGHT_GUN);
  const uint32_t knownMask = MAPLE_FUNC_CONTROLLER | accessoryMask;

  const uint8_t directAddrs[] = { 0x20, 0x40, 0x80 };
  for (uint8_t i = 0; i < sizeof(directAddrs); ++i) {
    uint32_t nativeFunc = 0;
    uint32_t swappedFunc = 0;
    uint8_t sender = 0;
    uint8_t responseLen = 0;
    if (!probe.probeDeviceInfo(directAddrs[i], &nativeFunc, &swappedFunc, &sender, &responseLen)) {
      continue;
    }
    bool hasKnownFunc = ((nativeFunc & knownMask) != 0) || ((swappedFunc & knownMask) != 0);
    bool senderValid = (sender != 0);
    if (responseLen >= 7 && hasKnownFunc && senderValid) {
      probe.end();
      return AUTODETECT_DREAMCAST;
    }
  }

  uint16_t readOkStart = maple_read_ok;
  uint32_t lastSeenFuncStart = probe.getLastSeenFunction();
  uint32_t accessoryFuncStart = probe.getAccessoryFunctionMask();
  uint8_t devinfoSenderStart = maple_last_devinfo_sender;
  uint8_t devinfoLenStart = maple_last_devinfo_len;
  uint32_t devinfoNativeStart = maple_last_devinfo_func_native;
  uint32_t devinfoSwappedStart = maple_last_devinfo_func_swapped;

  uint32_t timeoutMs = is_hotswap ? 80 : 2000;
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    probe.update();
    if (probe.isConnected() && (probe.hasFunction(MAPLE_FUNC_CONTROLLER) ||
                                probe.hasFunction(MAPLE_FUNC_MOUSE))) {
      probe.end();
      return AUTODETECT_DREAMCAST;
    }

    uint32_t seenFunc = probe.getLastSeenFunction();
    if (seenFunc != lastSeenFuncStart && (seenFunc & knownMask) != 0) {
      probe.end();
      return AUTODETECT_DREAMCAST;
    }

    uint32_t accessoryFunc = probe.getAccessoryFunctionMask();
    if (accessoryFunc != accessoryFuncStart && (accessoryFunc & accessoryMask) != 0) {
      probe.end();
      return AUTODETECT_DREAMCAST;
    }

    bool devinfoChanged =
      maple_last_devinfo_sender != devinfoSenderStart ||
      maple_last_devinfo_len != devinfoLenStart ||
      maple_last_devinfo_func_native != devinfoNativeStart ||
      maple_last_devinfo_func_swapped != devinfoSwappedStart;
    if (maple_read_ok != readOkStart && devinfoChanged && maple_last_devinfo_len >= 7) {
      uint32_t nativeFunc = maple_last_devinfo_func_native;
      uint32_t swappedFunc = maple_last_devinfo_func_swapped;
      bool hasKnownFunc = ((nativeFunc & knownMask) != 0) || ((swappedFunc & knownMask) != 0);
      bool senderValid = (maple_last_devinfo_sender != 0);
      if (!hasKnownFunc || !senderValid) {
        autoDetectDelay(4);
        continue;
      }
      probe.end();
      return AUTODETECT_DREAMCAST;
    }
    autoDetectDelay(4);
  }

  probe.end();
  return AUTODETECT_NONE;
}
#endif
#endif
