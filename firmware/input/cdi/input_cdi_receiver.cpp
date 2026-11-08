#include "input_cdi_receiver.h"

#include <Arduino.h>
#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <hardware/pio.h>
#include <hardware/structs/iobank0.h>

#include "cdi_uart_rx.pio.h"

namespace cdi_receiver {
namespace {

constexpr uint8_t kPorts = 2;
constexpr uint8_t kPioBlocks = 2;
constexpr uint8_t kCyclesPerBit = 8;

struct Receiver {
  PIO pio;
  int sm;
  uint8_t pin;
  uint8_t pioIndex;
};

Receiver receivers[kPorts] = {{nullptr, -1, 0, 0}, {nullptr, -1, 0, 0}};
// Program offset and the number of ports using it, per PIO block.
int programOffset[kPioBlocks] = {-1, -1};
uint8_t programUsers[kPioBlocks] = {0, 0};

PIO pioBlock(uint8_t index) {
  return index == 0 ? pio0 : pio1;
}

// The pushed word holds the 7 data bits in bits 31..25.
constexpr uint8_t kShiftToCharacter = 25;

// The framing flag the program raises with "irq 4 rel".
uint irqFlag(const Receiver& rx) {
  return 4u + (uint)rx.sm;
}

void configure(Receiver& rx, uint32_t baud) {
  pio_sm_set_enabled(rx.pio, rx.sm, false);
  pio_sm_config config =
      cdi_uart_rx_program_get_default_config((uint)programOffset[rx.pioIndex]);
  sm_config_set_in_pins(&config, rx.pin);
  sm_config_set_jmp_pin(&config, rx.pin);
  sm_config_set_in_shift(&config, true, false, 32);
  sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_RX);
  sm_config_set_clkdiv(&config, (float)clock_get_hz(clk_sys) /
                                    (float)(kCyclesPerBit * baud));
  pio_sm_init(rx.pio, rx.sm, (uint)programOffset[rx.pioIndex], &config);
  pio_sm_clear_fifos(rx.pio, rx.sm);
  pio_interrupt_clear(rx.pio, irqFlag(rx));
  pio_sm_set_enabled(rx.pio, rx.sm, true);
}

bool claim(Receiver& rx) {
  for (uint8_t index = 0; index < kPioBlocks; ++index) {
    PIO pio = pioBlock(index);
    if (programOffset[index] < 0 && !pio_can_add_program(pio, &cdi_uart_rx_program)) {
      continue;
    }
    const int sm = pio_claim_unused_sm(pio, false);
    if (sm < 0) continue;
    if (programOffset[index] < 0) {
      programOffset[index] = (int)pio_add_program(pio, &cdi_uart_rx_program);
    }
    ++programUsers[index];
    rx.pio = pio;
    rx.sm = sm;
    rx.pioIndex = index;
    return true;
  }
  return false;
}

}  // namespace

bool start(uint8_t port, uint8_t pin, uint32_t baud) {
  if (port >= kPorts) return false;
  stop(port);
  Receiver& rx = receivers[port];
  rx.pin = pin;
  gpio_init(pin);
  gpio_pull_up(pin);
  // The CD-i line idles low; the program expects a UART that idles high.
  gpio_set_inover(pin, GPIO_OVERRIDE_INVERT);
  if (!claim(rx)) {
    gpio_set_inover(pin, GPIO_OVERRIDE_NORMAL);
    return false;
  }
  configure(rx, baud);
  return true;
}

void restart(uint8_t port, uint32_t baud) {
  if (port >= kPorts || !running(port)) return;
  configure(receivers[port], baud);
}

void stop(uint8_t port) {
  if (port >= kPorts) return;
  Receiver& rx = receivers[port];
  if (rx.sm < 0) return;
  pio_sm_set_enabled(rx.pio, rx.sm, false);
  pio_interrupt_clear(rx.pio, irqFlag(rx));
  pio_sm_unclaim(rx.pio, (uint)rx.sm);
  if (programUsers[rx.pioIndex] > 0 && --programUsers[rx.pioIndex] == 0) {
    pio_remove_program(rx.pio, &cdi_uart_rx_program, (uint)programOffset[rx.pioIndex]);
    programOffset[rx.pioIndex] = -1;
  }
  gpio_set_inover(rx.pin, GPIO_OVERRIDE_NORMAL);
  rx.pio = nullptr;
  rx.sm = -1;
}

bool running(uint8_t port) {
  return port < kPorts && receivers[port].sm >= 0;
}

int read(uint8_t port) {
  if (!running(port)) return -1;
  const Receiver& rx = receivers[port];
  if (pio_sm_is_rx_fifo_empty(rx.pio, (uint)rx.sm)) return -1;
  return (int)((pio_sm_get(rx.pio, (uint)rx.sm) >> kShiftToCharacter) & 0x7F);
}

bool takeFramingError(uint8_t port) {
  if (!running(port)) return false;
  const Receiver& rx = receivers[port];
  if (!pio_interrupt_get(rx.pio, irqFlag(rx))) return false;
  pio_interrupt_clear(rx.pio, irqFlag(rx));
  return true;
}

bool lineAtMark(uint8_t pin) {
  return (iobank0_hw->io[pin].status & IO_BANK0_GPIO0_STATUS_INFROMPAD_BITS) == 0;
}

}  // namespace cdi_receiver
