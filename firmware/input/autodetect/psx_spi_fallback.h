#pragma once

#include <SPI.h>
#include <hardware/gpio.h>
#include <hardware/spi.h>

struct PsxSpiFallbackResult {
  uint8_t rx[33]{};
  uint8_t count = 0;
  uint8_t ackCount = 0;
  bool attempted = false;
  bool complete = false;
  uint32_t startedUs = 0;
  uint32_t durationUs = 0;
};

namespace psx_spi_fallback {
constexpr uint32_t kBudgetUs = 8000;
constexpr uint32_t kByteTimeoutUs = 100;
constexpr uint32_t kAckTimeoutUs = 100;

inline uint8_t reverseByte(uint8_t value) {
  value = uint8_t((value >> 4) | (value << 4));
  value = uint8_t(((value & 0xCC) >> 2) | ((value & 0x33) << 2));
  return uint8_t(((value & 0xAA) >> 1) | ((value & 0x55) << 1));
}

inline bool withinBudget(uint32_t started) {
  return uint32_t(micros() - started) < kBudgetUs;
}

// RP2040 SPI setters panic for illegal pins. Validate the complete tuple first.
inline int8_t spiIndex(const autodetect_pins_t& pins) {
  const uint8_t allPins[] = {pins.psx_att, pins.psx_cmd, pins.psx_dat,
                             pins.psx_clk, pins.psx_ack};
  for (uint8_t i = 0; i < sizeof(allPins); ++i) {
    if (allPins[i] >= 30) return -1;
    for (uint8_t j = 0; j < i; ++j)
      if (allPins[i] == allPins[j]) return -1;
  }
  const uint32_t rx0 = (1u << 0) | (1u << 4) | (1u << 16) | (1u << 20);
  const uint32_t clk0 = (1u << 2) | (1u << 6) | (1u << 18) | (1u << 22);
  const uint32_t tx0 = (1u << 3) | (1u << 7) | (1u << 19) | (1u << 23);
  const uint32_t rx1 = (1u << 8) | (1u << 12) | (1u << 24) | (1u << 28);
  const uint32_t clk1 = (1u << 10) | (1u << 14) | (1u << 26);
  const uint32_t tx1 = (1u << 11) | (1u << 15) | (1u << 27);
  const uint32_t rx = 1u << pins.psx_dat;
  const uint32_t clk = 1u << pins.psx_clk;
  const uint32_t tx = 1u << pins.psx_cmd;
  if ((rx & rx0) && (clk & clk0) && (tx & tx0)) return 0;
  if ((rx & rx1) && (clk & clk1) && (tx & tx1)) return 1;
  return -1;
}

inline bool transferByte(spi_inst_t* spi, uint8_t tx, uint8_t& rx, uint32_t started) {
  const uint32_t byteStarted = micros();
  if (!withinBudget(started)) return false;
  while (!spi_is_writable(spi)) {
    if (uint32_t(micros() - byteStarted) >= kByteTimeoutUs || !withinBudget(started))
      return false;
  }
  // The RP2040 peripheral is MSB-first; match Arduino's byte overload's LSB
  // conversion, but bound both FIFO waits instead of using its blocking call.
  spi_get_hw(spi)->dr = reverseByte(tx);
  while (!spi_is_readable(spi)) {
    if (uint32_t(micros() - byteStarted) >= kByteTimeoutUs || !withinBudget(started))
      return false;
  }
  rx = reverseByte(uint8_t(spi_get_hw(spi)->dr));
  return withinBudget(started);
}

inline bool waitAck(uint8_t ack, uint32_t started) {
  const uint32_t ackStarted = micros();
  bool sawLow = false;
  bool acknowledged = false;
  while (uint32_t(micros() - ackStarted) < kAckTimeoutUs && withinBudget(started)) {
    if (!gpio_get(ack)) sawLow = true;
    else if (sawLow) {
      acknowledged = true;
      break;
    }
  }
  const uint32_t elapsed = uint32_t(micros() - ackStarted);
  const uint32_t budgetElapsed = uint32_t(micros() - started);
  if (elapsed < 20 && budgetElapsed < kBudgetUs) {
    const uint32_t gap = 20 - elapsed;
    const uint32_t remaining = kBudgetUs - budgetElapsed;
    delayMicroseconds(gap < remaining ? gap : remaining);
  }
  return acknowledged;
}
}  // namespace psx_spi_fallback

inline PsxSpiFallbackResult psxSpiFallback(const autodetect_pins_t& pins) {
  PsxSpiFallbackResult result{};
  const int8_t index = psx_spi_fallback::spiIndex(pins);
  if (index < 0) return result;
  result.attempted = true;
  const uint32_t started = micros();
  result.startedUs = started;
  const uint8_t busPins[] = {pins.psx_att, pins.psx_cmd, pins.psx_dat,
                             pins.psx_clk, pins.psx_ack};
  for (uint8_t pin : busPins) {
    gpio_set_oeover(pin, GPIO_OVERRIDE_NORMAL);
    gpio_set_outover(pin, GPIO_OVERRIDE_NORMAL);
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
  }
  gpio_put(pins.psx_att, HIGH);
  gpio_put(pins.psx_cmd, HIGH);
  gpio_put(pins.psx_clk, HIGH);
  gpio_set_dir(pins.psx_att, GPIO_OUT);
  gpio_set_dir(pins.psx_cmd, GPIO_OUT);
  gpio_set_dir(pins.psx_clk, GPIO_OUT);
  // Let a controller recover from the rejected GPIO poll before a fresh SPI
  // transaction. This remains local to the existing PSX detection stage.
  delayMicroseconds(3000);

  // Auto owns this physical input bus synchronously; the normal PSX driver
  // uses the same fixed pin tuple when it takes over after identification.
  SPIClassRP2040& bus = index ? SPI1 : SPI;
  spi_inst_t* spi = index ? spi1 : spi0;
  const bool configured = bus.setMISO(pins.psx_dat) && bus.setSCK(pins.psx_clk) &&
                          bus.setMOSI(pins.psx_cmd);
  if (configured) {
    // Drop any old FIFO/settings cache before this one-off ownership session.
    bus.end();
    // Match the normal PSX driver's proven GPIO-to-SPI handoff.
    bus.begin(false);
    bus.begin(false);
    bus.beginTransaction(SPISettings(250000, LSBFIRST, SPI_MODE3));
    gpio_put(pins.psx_att, LOW);
    delayMicroseconds(20);
    uint8_t wanted = 3;
    for (uint8_t i = 0; i < wanted; ++i) {
      const uint8_t tx = i == 0 ? 0x01 : i == 1 ? 0x42 : 0x00;
      if (!psx_spi_fallback::transferByte(spi, tx, result.rx[i], started)) break;
      ++result.count;
      if (i == 2) {
        if (result.rx[0] != 0xFF || result.rx[2] != 0x5A ||
            result.rx[1] == 0 || result.rx[1] == 0xFF || (result.rx[1] & 0x0F) == 0)
          break;
        wanted = uint8_t(3 + 2 * (result.rx[1] & 0x0F));
      }
      if (i + 1 < wanted) {
        if (psx_spi_fallback::waitAck(pins.psx_ack, started)) ++result.ackCount;
        if (!psx_spi_fallback::withinBudget(started)) break;
      } else {
        result.complete = true;
      }
    }
    gpio_put(pins.psx_att, HIGH);
    bus.endTransaction();
  }
  bus.end();
  // SPI end() releases its data pins, but Auto also owns ATT/ACK. Restore all
  // five lines even on rejection or timeout before another protocol runs.
  for (uint8_t pin : busPins) {
    gpio_set_oeover(pin, GPIO_OVERRIDE_NORMAL);
    gpio_set_outover(pin, GPIO_OVERRIDE_NORMAL);
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
  }
  result.durationUs = uint32_t(micros() - started);
  return result;
}
