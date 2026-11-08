#include "../../product_config.h"
#include "../../firmware_platform_config.h"

#include <Arduino.h>

#include "../../core/firmware_support.h"
#include "../../platform/button_handler.h"
#include "../../platform/buzzer.h"
#include "Input_AutoDetect.h"
#include "../../features/psx_capture/psx_capture.h"

#ifdef ENABLE_INPUT_AUTODETECT
#ifdef ENABLE_INPUT_PSX
#include "psx_spi_fallback.h"

bool AutoDetector::isKnownPsxReplyType(uint8_t controllerType) {
  switch (controllerType) {
    case 0x12:
    case 0x23:
    case 0x41:
    case 0x53:
    case 0x55:
    case 0x63:
    case 0x73:
    case 0x79:
    case 0xE3:
    case 0xE5:
      return true;
    default:
      return false;
  }
}

bool AutoDetector::isLegacyPsxReplyFamily(uint8_t controllerType) {
  if (controllerType == 0x00 || controllerType == 0xFF) {
    return false;
  }

  uint8_t typeNibble = controllerType & 0x0F;
  return typeNibble == 0x01 || typeNibble == 0x02 || typeNibble == 0x03;
}

AutoDetectResult AutoDetector::probePSX(const autodetect_pins_t& pins, uint8_t port) {
  gpio_init(pins.psx_att);
  gpio_init(pins.psx_cmd);
  gpio_init(pins.psx_dat);
  gpio_init(pins.psx_clk);
  gpio_init(pins.psx_ack);

  gpio_set_dir(pins.psx_att, GPIO_OUT);
  gpio_set_dir(pins.psx_cmd, GPIO_OUT);
  gpio_set_dir(pins.psx_dat, GPIO_IN);
  gpio_set_dir(pins.psx_clk, GPIO_OUT);
  gpio_set_dir(pins.psx_ack, GPIO_IN);
  gpio_pull_up(pins.psx_dat);
  gpio_pull_up(pins.psx_ack);

  gpio_put(pins.psx_att, HIGH);
  gpio_put(pins.psx_clk, HIGH);
  gpio_put(pins.psx_cmd, HIGH);
  delayMicroseconds(50);

#if defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE)
  const uint32_t captureStarted = micros();
#endif
  gpio_put(pins.psx_att, LOW);
  delayMicroseconds(20);

  // Use the normal controller poll frame instead of stopping after the type
  // byte. neGcon and other PSX specialty controllers can be unreliable during
  // AUTO if ATT is released before the 0x5A sync phase has been clocked.
  uint8_t recv1 = psxTransferByte(pins, 0x01);
  delayMicroseconds(20);

  uint8_t controllerType = psxTransferByte(pins, 0x42);
  delayMicroseconds(20);

  uint8_t sync = psxTransferByte(pins, 0x00);
  delayMicroseconds(20);

  const uint8_t buttons0 = psxTransferByte(pins, 0xFF);
  delayMicroseconds(20);

  const uint8_t buttons1 = psxTransferByte(pins, 0xFF);
  delayMicroseconds(20);

  gpio_put(pins.psx_att, HIGH);

  gpio_set_dir(pins.psx_att, GPIO_IN);
  gpio_set_dir(pins.psx_cmd, GPIO_IN);
  gpio_set_dir(pins.psx_clk, GPIO_IN);

  #ifdef AUTODETECT_DEBUG
  lastDebug[port].psx_recv1 = recv1;
  lastDebug[port].psx_type = controllerType;
  lastDebug[port].psx_sync = sync;
  #endif

  const bool validHeader = (recv1 == 0xFF);
  const bool validSync = (sync == 0x5A);
  const bool knownType = isKnownPsxReplyType(controllerType);

#if defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE)
  psx_capture::Event event{};
  event.kind = uint8_t(psx_capture::Kind::AutoProbe);
  event.port = port;
  event.us = captureStarted;
  event.durationUs = uint32_t(micros() - captureStarted);
  event.count = 5;
  const uint8_t tx[] = {0x01, 0x42, 0x00, 0xFF, 0xFF};
  const uint8_t rx[] = {recv1, controllerType, sync, buttons0, buttons1};
  memcpy(event.tx, tx, sizeof(tx));
  memcpy(event.rx, rx, sizeof(rx));
  if (validHeader) event.flags |= psx_capture::HeaderFF;
  if (validSync) event.flags |= psx_capture::Sync5A;
  if (knownType) event.flags |= psx_capture::KnownType;
  if (isLegacyPsxReplyFamily(controllerType)) event.flags |= psx_capture::LegacyType;
  if ((validSync && knownType) || (validHeader && knownType) ||
      (validHeader && validSync && isLegacyPsxReplyFamily(controllerType)))
    event.flags |= psx_capture::Accepted;
  psxCaptureRecord(event);
#else
  (void)buttons0;
  (void)buttons1;
#endif

  if (validSync && knownType) {
    return AUTODETECT_PSX;
  }

  if (validHeader && knownType) {
    return AUTODETECT_PSX;
  }

  if (validHeader && validSync && isLegacyPsxReplyFamily(controllerType)) {
    return AUTODETECT_PSX;
  }

  const PsxSpiFallbackResult fallback = psxSpiFallback(pins);
  const bool fallbackAccepted = fallback.complete && fallback.rx[0] == 0xFF &&
                                fallback.rx[2] == 0x5A && isKnownPsxReplyType(fallback.rx[1]);
#if defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE)
  if (fallback.attempted) {
    psx_capture::Event spiEvent{};
    spiEvent.kind = uint8_t(psx_capture::Kind::AutoProbe);
    spiEvent.stage = 4;
    spiEvent.port = port;
    spiEvent.us = fallback.startedUs;
    spiEvent.durationUs = fallback.durationUs;
    spiEvent.count = fallback.count;
    spiEvent.aux = fallback.ackCount;
    for (uint8_t i = 0; i < fallback.count; ++i) {
      spiEvent.tx[i] = i == 0 ? 0x01 : i == 1 ? 0x42 : 0x00;
      spiEvent.rx[i] = fallback.rx[i];
    }
    if (fallback.count > 0 && fallback.rx[0] == 0xFF) spiEvent.flags |= psx_capture::HeaderFF;
    if (fallback.count > 2 && fallback.rx[2] == 0x5A) spiEvent.flags |= psx_capture::Sync5A;
    if (fallback.count > 1 && isKnownPsxReplyType(fallback.rx[1])) spiEvent.flags |= psx_capture::KnownType;
    if (fallback.complete) spiEvent.flags |= psx_capture::Complete | psx_capture::TransportOk;
    if (fallback.ackCount) spiEvent.flags |= psx_capture::PhysicalAck;
    if (fallbackAccepted) spiEvent.flags |= psx_capture::Accepted;
    psxCaptureRecord(spiEvent);
  }
#endif
  return fallbackAccepted ? AUTODETECT_PSX : AUTODETECT_NONE;
}

bool AutoDetector::psxWaitForAck(const autodetect_pins_t& pins, uint16_t timeoutUs) {
  const uint32_t started = micros();
  bool sawLow = false;

  while ((uint32_t)(micros() - started) < timeoutUs) {
    if (!gpio_get(pins.psx_ack)) {
      sawLow = true;
      break;
    }
  }

  if (!sawLow) {
    return false;
  }

  while ((uint32_t)(micros() - started) < timeoutUs) {
    if (gpio_get(pins.psx_ack)) {
      return true;
    }
  }

  return true;
}

uint8_t AutoDetector::psxTransferByte(const autodetect_pins_t& pins, uint8_t txByte) {
  uint8_t rxByte = 0;

  for (uint8_t i = 0; i < 8; i++) {
    gpio_put(pins.psx_cmd, (txByte >> i) & 0x01);
    delayMicroseconds(2);

    gpio_put(pins.psx_clk, LOW);
    delayMicroseconds(2);

    // Capture settled DAT before the rising edge can release a byte's last bit.
    if (gpio_get(pins.psx_dat)) {
      rxByte |= (1 << i);
    }

    gpio_put(pins.psx_clk, HIGH);
    delayMicroseconds(2);
  }

  return rxByte;
}
#endif
#endif
