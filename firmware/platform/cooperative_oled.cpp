#include "cooperative_oled.h"
#include <Arduino.h>

#if ADAPT_POLICY_COOPERATIVE_OLED
#include "display_runtime_state.h"
#include "ssd1306_shadow.h"
#include <hardware/i2c.h>
#include <hardware/resets.h>
#include <pico/stdlib.h>
#include <cstdint>

#if !defined(ADAPT_FEATURE_OLED_DISPLAY) || defined(ADAPT_FEATURE_NFC_BRIDGE)
#error "Cooperative OLED requires exclusive ownership of its fitted I2C bus"
#endif

namespace {
Ssd1306Shadow shadow;
bool active = false;
u8x8_msg_cb originalByteCallback;
u8x8_msg_cb originalGpioCallback;
i2c_inst_t* const bus = (PIN_OLED_SDA & 2) ? i2c1 : i2c0;
constexpr uint32_t kTransferTimeoutUs = 20000;
constexpr uint32_t kRecoveryTimeoutUs = 2000;
constexpr uint32_t kRetryIntervalUs = 100000;
constexpr uint32_t kFifoBudget = 16;

enum class State { Idle, Disabled, Enabled, Fill, Stop, Abort,
                   RecoveryDisabled, ResetRelease, ResetWait, Backoff };
enum class Packet { Configuration, Address, Data };
State state = State::Idle;
Packet packet = Packet::Configuration;
uint32_t stateStartedUs;
uint32_t packetStartedUs;
uint32_t appliedRevision = UINT32_MAX;
uint32_t pendingRevision;
uint8_t commands[48];
uint8_t pixels[129];
const uint8_t* bytes;
size_t length;
size_t offset;
size_t pixelLength;
struct { uint32_t con, hcnt, lcnt, hold, spklen, dma; } timing;

uint32_t resetMask() {
  return bus == i2c0 ? RESETS_RESET_I2C0_BITS : RESETS_RESET_I2C1_BITS;
}
void setState(State next) {
  state = next;
  stateStartedUs = time_us_32();
}
bool expired(uint32_t since, uint32_t limit) {
  return static_cast<uint32_t>(time_us_32() - since) >= limit;
}
void beginPacket(const uint8_t* data, size_t size, Packet kind) {
  bytes = data; length = size; offset = 0; packet = kind;
  packetStartedUs = time_us_32();
  i2c_get_hw(bus)->enable = 0;
  setState(State::Disabled);
}
void resetTransport() {
  reset_block_mask(resetMask());
  setState(State::ResetRelease);
}
void abortTransport() {
  shadow.invalidate();
  appliedRevision = UINT32_MAX;
  auto* hw = i2c_get_hw(bus);
  (void)(uint32_t)hw->clr_tx_abrt;
  if (!(hw->enable & I2C_IC_ENABLE_ENABLE_BITS)) {
    resetTransport();
  } else {
    hw->enable = I2C_IC_ENABLE_ENABLE_BITS | I2C_IC_ENABLE_ABORT_BITS;
    setState(State::Abort);
  }
}

uint8_t captureU8g2Bytes(u8x8_t* u8x8, uint8_t message,
                       uint8_t count, void* argument) {
  if (message == U8X8_MSG_BYTE_START_TRANSFER) shadow.startI2cTransfer();
  if (message == U8X8_MSG_BYTE_SEND) {
    const auto* data = static_cast<const uint8_t*>(argument);
    for (uint8_t i = 0; i < count; ++i) shadow.i2cByte(data[i]);
  }
  // Boot uses the original transport. Runtime init/draw calls only change RAM.
  return active ? 1 : originalByteCallback(u8x8, message, count, argument);
}
uint8_t captureU8g2Gpio(u8x8_t* u8x8, uint8_t message,
                      uint8_t count, void* argument) {
  // There is no reset pin. Runtime re-initialization must not repeat boot delays.
  return active ? 1 : originalGpioCallback(u8x8, message, count, argument);
}
}  // namespace

CooperativeOledU8g2::CooperativeOledU8g2()
    : U8G2_SSD1306_128X64_NONAME_F_HW_I2C(U8G2_R0, U8X8_PIN_NONE) {
  auto* u8x8 = getU8x8();
  originalByteCallback = u8x8->byte_cb;
  originalGpioCallback = u8x8->gpio_and_delay_cb;
  u8x8->byte_cb = captureU8g2Bytes;
  u8x8->gpio_and_delay_cb = captureU8g2Gpio;
}
void CooperativeOledAscii::writeDisplay(uint8_t value, uint8_t mode) {
  if (mode == SSD1306_MODE_CMD) shadow.command(value);
  else shadow.data(value);
  if (!active) SSD1306AsciiWire::writeDisplay(value, mode);
}
void CooperativeOledAscii::flush() {
  if (!active) SSD1306AsciiWire::flush();
}
bool cooperativeOledActive() { return active; }

void startCooperativeOled() {
  if (active) return;
  // Switch ownership only after boot drawing has finished on Wire.
  display.flush();
  (void)displayRuntimeAvailable();
  Wire.end();
  i2c_init(bus, 400000);
  gpio_set_function(PIN_OLED_SDA, GPIO_FUNC_I2C);
  gpio_set_function(PIN_OLED_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(PIN_OLED_SDA);
  gpio_pull_up(PIN_OLED_SCL);
  auto* hw = i2c_get_hw(bus);
  timing = {hw->con, hw->fs_scl_hcnt, hw->fs_scl_lcnt,
            hw->sda_hold, hw->fs_spklen, hw->dma_cr};
  shadow.invalidate();
  active = true;
}

void serviceCooperativeOled() {
  if (!active) return;
  auto* hw = i2c_get_hw(bus);
  switch (state) {
    case State::Idle: {
      if (appliedRevision != shadow.configRevision()) {
        pendingRevision = shadow.configRevision();
        beginPacket(commands, shadow.configuration(commands), Packet::Configuration);
      } else {
        uint8_t page, col, size;
        if (shadow.takePage(page, col, pixels + 1, size)) {
          pixels[0] = 0x40;
          pixelLength = static_cast<size_t>(size) + 1;
          commands[0] = 0x00;
          commands[1] = 0xb0 | page;
          commands[2] = col & 15;
          commands[3] = 0x10 | (col >> 4);
          beginPacket(commands, 4, Packet::Address);
        }
      }
      return;
    }
    case State::Backoff:
      if (expired(stateStartedUs, kRetryIntervalUs)) setState(State::Idle);
      return;
    case State::Abort:
      if (!(hw->enable & I2C_IC_ENABLE_ABORT_BITS) &&
          !(hw->status & I2C_IC_STATUS_MST_ACTIVITY_BITS)) {
        hw->enable = 0;
        setState(State::RecoveryDisabled);
      } else if (expired(stateStartedUs, kRecoveryTimeoutUs)) resetTransport();
      return;
    case State::RecoveryDisabled:
      if (!(hw->enable_status & I2C_IC_ENABLE_STATUS_IC_EN_BITS)) {
        (void)(uint32_t)hw->clr_tx_abrt;
        (void)(uint32_t)hw->clr_stop_det;
        bus->restart_on_next = false;
        setState(State::Backoff);
      } else if (expired(stateStartedUs, kRecoveryTimeoutUs)) resetTransport();
      return;
    case State::ResetRelease:
      unreset_block_mask(resetMask());
      setState(State::ResetWait);
      return;
    case State::ResetWait:
      if (resets_hw->reset_done & resetMask()) {
        hw->enable = 0;
        hw->con = timing.con;
        hw->fs_scl_hcnt = timing.hcnt;
        hw->fs_scl_lcnt = timing.lcnt;
        hw->sda_hold = timing.hold;
        hw->fs_spklen = timing.spklen;
        hw->tx_tl = 0; hw->rx_tl = 0; hw->dma_cr = timing.dma;
        bus->restart_on_next = false;
        setState(State::Backoff);
      } else if (expired(stateStartedUs, kRecoveryTimeoutUs)) {
        // A failed reset remains bounded and can be retried without a busy wait.
        resetTransport();
      }
      return;
    default: break;
  }

  if ((hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_TX_ABRT_BITS) ||
      expired(packetStartedUs, kTransferTimeoutUs)) {
    abortTransport();
    return;
  }
  switch (state) {
    case State::Disabled:
      if (!(hw->enable_status & I2C_IC_ENABLE_STATUS_IC_EN_BITS)) {
        (void)(uint32_t)hw->clr_tx_abrt;
        (void)(uint32_t)hw->clr_stop_det;
        hw->tar = I2C_ADDRESS;
        bus->restart_on_next = false;
        hw->enable = I2C_IC_ENABLE_ENABLE_BITS;
        setState(State::Enabled);
      }
      return;
    case State::Enabled:
      if (hw->enable_status & I2C_IC_ENABLE_STATUS_IC_EN_BITS) setState(State::Fill);
      return;
    case State::Fill: {
      size_t available = i2c_get_write_available(bus);
      if (available > kFifoBudget) available = kFifoBudget;
      for (size_t i = 0; i < available && offset < length; ++i) {
        uint32_t word = bytes[offset++];
        if (offset == length) word |= I2C_IC_DATA_CMD_STOP_BITS;
        hw->data_cmd = word;
      }
      if (offset == length) setState(State::Stop);
      return;
    }
    case State::Stop:
      // An empty FIFO can precede the last byte and STOP. Keep ownership until idle.
      if ((hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_STOP_DET_BITS) &&
          !(hw->status & I2C_IC_STATUS_MST_ACTIVITY_BITS) && hw->txflr == 0) {
        (void)(uint32_t)hw->clr_stop_det;
        if (packet == Packet::Address) beginPacket(pixels, pixelLength, Packet::Data);
        else {
          if (packet == Packet::Configuration) appliedRevision = pendingRevision;
          setState(State::Idle);
        }
      }
      return;
    default: return;
  }
}
#endif

void delayWithCooperativeOled(uint32_t durationMs) {
#if ADAPT_POLICY_COOPERATIVE_OLED
  if (cooperativeOledActive()) {
    const uint32_t startedMs = millis();
    while (static_cast<uint32_t>(millis() - startedMs) < durationMs) {
      serviceCooperativeOled();
      delayMicroseconds(100);
    }
    return;
  }
#endif
  delay(durationMs);
}
