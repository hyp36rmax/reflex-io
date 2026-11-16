#include "rgb_led.h"

namespace {

static const uint16_t ws2812_program_instructions[] = {
    0x6221,
    0x1223,
    0x1300,
    0xa342,
};

static const struct pio_program ws2812_program = {
    .instructions = ws2812_program_instructions,
    .length = 4,
    .origin = -1,
};

}  // namespace

rgb_color_t RgbLed::blendColors(rgb_color_t c1, rgb_color_t c2, uint8_t blend) {
  return (rgb_color_t){
    (uint8_t)((c1.r * (255 - blend) + c2.r * blend) / 255),
    (uint8_t)((c1.g * (255 - blend) + c2.g * blend) / 255),
    (uint8_t)((c1.b * (255 - blend) + c2.b * blend) / 255)
  };
}

rgb_color_t RgbLed::applyBrightness(rgb_color_t c) {
  return (rgb_color_t){
    (uint8_t)((c.r * brightness) / 255),
    (uint8_t)((c.g * brightness) / 255),
    (uint8_t)((c.b * brightness) / 255)
  };
}

rgb_color_t RgbLed::hsvToRgb(uint8_t h, uint8_t s, uint8_t v) {
  if (s == 0) {
    return (rgb_color_t){v, v, v};
  }

  uint8_t region = h / 43;
  uint8_t remainder = (h - (region * 43)) * 6;

  uint8_t p = (v * (255 - s)) >> 8;
  uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
  uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

  switch (region) {
    case 0:  return (rgb_color_t){v, t, p};
    case 1:  return (rgb_color_t){q, v, p};
    case 2:  return (rgb_color_t){p, v, t};
    case 3:  return (rgb_color_t){p, q, v};
    case 4:  return (rgb_color_t){t, p, v};
    default: return (rgb_color_t){v, p, q};
  }
}

void RgbLed::initPio() {
  if (pio_initialized) return;

  pio = pio0;
  sm = pio_claim_unused_sm(pio, true);
  uint offset = pio_add_program(pio, &ws2812_program);

  // Drive the pin low from the PIO before handing the pad over. pio_gpio_init()
  // first left the pin undriven for a moment; a board whose LED shifter pulls
  // the line up (Modern2USB V1.1: R149, R150) then clocked stray bits into the
  // chain, and its second pixel showed green until the next colour change.
  pio_sm_set_pins_with_mask(pio, sm, 0u, 1u << pin);
  pio_sm_set_pindirs_with_mask(pio, sm, 1u << pin, 1u << pin);
  pio_gpio_init(pio, pin);
  pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);

  pio_sm_config c = pio_get_default_sm_config();
  sm_config_set_wrap(&c, offset, offset + ws2812_program.length - 1);
  sm_config_set_sideset(&c, 1, false, false);
  sm_config_set_sideset_pins(&c, pin);
  sm_config_set_out_shift(&c, false, true, 24);
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

  // Ten PIO clocks per 800 kHz bit with T0H=3 clocks, T1H=7 clocks.
  // Keep the hardware-verified waveform: widening T0H to four clocks makes
  // this board's WS2812 chain decode zero bits as spurious colors.
  float div = clock_get_hz(clk_sys) / (800000 * 10);
  sm_config_set_clkdiv(&c, div);

  pio_sm_init(pio, sm, offset, &c);
  pio_sm_set_enabled(pio, sm, true);

  pio_initialized = true;
}

void RgbLed::show(bool force) {
  if (!pio_initialized) return;

  uint32_t wire_frame[RGB_LED_COUNT];
  for (uint8_t i = 0; i < led_count; ++i) {
    const rgb_color_t c = applyBrightness(leds[i]);
    wire_frame[i] = ((uint32_t)c.g << 24) | ((uint32_t)c.r << 16) |
                    ((uint32_t)c.b << 8);
  }
  if (!force && wire_frame_valid &&
      memcmp(wire_frame, last_wire_frame, led_count * sizeof(uint32_t)) == 0) return;
  for (uint8_t i = 0; i < led_count; ++i) {
    pio_sm_put_blocking(pio, sm, wire_frame[i]);
  }
  memcpy(last_wire_frame, wire_frame, led_count * sizeof(uint32_t));
  wire_frame_valid = true;
}

void RgbLed::setAll(rgb_color_t color) {
  for (uint8_t i = 0; i < led_count; i++) {
    leds[i] = color;
  }
}

void RgbLed::updateBreathing() {
  // About a four-second triangle wave: half the original animation speed.
  uint8_t phase = (millis() / 16) & 0xFF;
  int16_t sine = 128 + ((phase < 128) ? (phase * 2 - 128) : (384 - phase * 2));
  if (sine < 0) sine = 0;
  if (sine > 255) sine = 255;

  rgb_color_t c{
      static_cast<uint8_t>((static_cast<uint16_t>(base_color.r) * sine) / 255u),
      static_cast<uint8_t>((static_cast<uint16_t>(base_color.g) * sine) / 255u),
      static_cast<uint8_t>((static_cast<uint16_t>(base_color.b) * sine) / 255u)};
  setAll(c);
}

void RgbLed::updateRainbow() {
  uint8_t base_hue = (millis() / 20) & 0xFF;
  for (uint8_t i = 0; i < led_count; i++) {
    uint8_t hue = base_hue + (i * 256 / led_count);
    leds[i] = hsvToRgb(hue, 255, 200);
  }
}

void RgbLed::updateReactive() {
  for (uint8_t i = 0; i < 16; i++) {
    if (button_brightness[i] > 0) {
      button_brightness[i] = (button_brightness[i] > 8) ? button_brightness[i] - 8 : 0;
    }
  }

  uint8_t max_brightness = 0;
  for (uint8_t i = 0; i < 16; i++) {
    if (button_brightness[i] > max_brightness) {
      max_brightness = button_brightness[i];
    }
  }

  if (max_brightness > 0) {
    rgb_color_t c = base_color;
    c.r = (c.r * max_brightness) / 255;
    c.g = (c.g * max_brightness) / 255;
    c.b = (c.b * max_brightness) / 255;
    setAll(c);
  } else {
    setAll(COLOR_OFF);
  }
}

void RgbLed::updateRumble() {
  if (rumble_intensity > 0) {
    const rgb_color_t c{
        static_cast<uint8_t>(
            (static_cast<uint16_t>(base_color.r) * rumble_intensity) / 255u),
        static_cast<uint8_t>(
            (static_cast<uint16_t>(base_color.g) * rumble_intensity) / 255u),
        static_cast<uint8_t>(
            (static_cast<uint16_t>(base_color.b) * rumble_intensity) / 255u)};
    setAll(c);
  } else {
    setAll(COLOR_OFF);
  }
}

void RgbLed::updateAnalog() {
  int16_t dx = stick_lx - 128;
  int16_t dy = stick_ly - 128;
  uint16_t distance = sqrt(dx * dx + dy * dy);
  if (distance > 127) distance = 127;

  uint8_t hue = 160 - (distance * 160 / 127);
  uint8_t sat = 255;
  uint8_t val = 64 + (distance * 191 / 127);

  setAll(hsvToRgb(hue, sat, val));
}

RgbLed::RgbLed()
  : pin(RGB_LED_PIN),
    led_count(RGB_LED_COUNT),
    brightness(128),
    mode(LED_MODE_OFF),
    enabled(true),
    base_color(COLOR_WHITE),
    pio_initialized(false),
    last_update(0),
    animation_step(0),
    rumble_intensity(0),
    stick_lx(128),
    stick_ly(128),
    status_override_active(false),
    diagnostic_override_until_ms(0) {
  memset(leds, 0, sizeof(leds));
  memset(status_pixels, 0, sizeof(status_pixels));
  memset(button_brightness, 0, sizeof(button_brightness));
}

void RgbLed::begin() {
  if (!pio_initialized) {
    // Hold DIN low for a complete WS2812 reset interval before handing the
    // pin to PIO. This prevents a boot-time pin transition from latching a
    // stray first-pixel color (normally seen as green because WS2812 is GRB).
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, false);
    delayMicroseconds(300);
  }
  initPio();
  setAll(COLOR_OFF);
  // Send two independently latched black frames. The redundant frame makes
  // current level-shifted hardware tolerant of a slow pull-up rising edge.
  show(true);
  delayMicroseconds(300);
  show(true);
  delayMicroseconds(300);
}

void RgbLed::setEnabled(bool en) {
  enabled = en;
  if (!en) {
    setAll(COLOR_OFF);
    show();
    delayMicroseconds(80);
  }
}

bool RgbLed::isEnabled() {
  return enabled;
}

void RgbLed::setMode(led_mode_t m) {
  mode = m;
  animation_step = 0;
  if (mode == LED_MODE_OFF) {
    setAll(COLOR_OFF);
    show();
  } else if (mode == LED_MODE_STATIC) {
    setAll(base_color);
    show();
  }
}

led_mode_t RgbLed::getMode() {
  return mode;
}

void RgbLed::setBrightness(uint8_t b) {
  brightness = b;
}

uint8_t RgbLed::getBrightness() {
  return brightness;
}

void RgbLed::setBaseColor(rgb_color_t color) {
  base_color = color;
  if (mode == LED_MODE_STATIC) {
    setAll(base_color);
    show();
  }
}

rgb_color_t RgbLed::getBaseColor() {
  return base_color;
}

uint8_t RgbLed::getLedCount() {
  return led_count;
}

rgb_color_t RgbLed::getLedColor(uint8_t index) {
  return index < led_count ? leds[index] : COLOR_OFF;
}

void RgbLed::showDebugPattern(uint8_t pixelMask) {
  memset(status_pixels, 0, sizeof(status_pixels));
  if (led_count > 0 && (pixelMask & 0x01u)) {
    status_pixels[0] = COLOR_RED;
  }
  if (led_count > 1 && (pixelMask & 0x02u)) {
    status_pixels[1] = COLOR_BLUE;
  }
  for (uint8_t index = 2; index < led_count && index < 8; ++index) {
    if (pixelMask & (1u << index)) {
      status_pixels[index] = COLOR_WHITE;
    }
  }
  status_override_active = true;
  diagnostic_override_until_ms = millis() + 1200u;
  memcpy(leds, status_pixels, sizeof(leds));
  show();
}

void RgbLed::setStatusPixels(rgb_color_t first, rgb_color_t second,
                             bool force) {
  const uint32_t now = millis();
  const bool diagnosticActive = diagnostic_override_until_ms != 0 &&
      static_cast<int32_t>(now - diagnostic_override_until_ms) < 0;
  if (diagnosticActive && !force) return;
  if (force || !diagnosticActive) diagnostic_override_until_ms = 0;
  status_override_active = true;
  if (led_count > 0) status_pixels[0] = first;
  if (led_count > 1) status_pixels[1] = second;
  for (uint8_t index = 2; index < led_count; ++index) {
    status_pixels[index] = COLOR_OFF;
  }
}

void RgbLed::clearStatusPixels() {
  const uint32_t now = millis();
  if (diagnostic_override_until_ms != 0 &&
      static_cast<int32_t>(now - diagnostic_override_until_ms) < 0) {
    return;
  }
  diagnostic_override_until_ms = 0;
  if (!status_override_active) return;
  status_override_active = false;
  if (!enabled || mode == LED_MODE_OFF) {
    setAll(COLOR_OFF);
    show();
  }
}

void RgbLed::update() {
  uint32_t now = millis();
  if (now - last_update < 16) return;
  last_update = now;

  if (status_override_active) {
    for (uint8_t index = 0; index < led_count; ++index) {
      leds[index] = status_pixels[index];
    }
    show();
    return;
  }

  if (!enabled || mode == LED_MODE_OFF) return;

  switch (mode) {
    case LED_MODE_STATIC:
      break;
    case LED_MODE_BREATHING:
      updateBreathing();
      break;
    case LED_MODE_RAINBOW:
      updateRainbow();
      break;
    case LED_MODE_REACTIVE:
      updateReactive();
      break;
    case LED_MODE_RUMBLE:
      updateRumble();
      break;
    case LED_MODE_ANALOG:
      updateAnalog();
      break;
    case LED_MODE_USB_PORTS:
      break;
    default:
      break;
  }

  show();
}

void RgbLed::setColor(rgb_color_t color) {
  setAll(color);
  if (mode == LED_MODE_STATIC) {
    show();
  }
}

void RgbLed::setLed(uint8_t index, rgb_color_t color) {
  if (index < led_count) {
    leds[index] = color;
  }
}

void RgbLed::triggerButton(uint8_t button) {
  if (button < 16) {
    button_brightness[button] = 255;
  }
}

void RgbLed::triggerButtons(uint32_t buttons) {
  if (buttons & 0x0001) triggerButton(0);
  if (buttons & 0x0002) triggerButton(1);
  if (buttons & 0x0004) triggerButton(2);
  if (buttons & 0x0008) triggerButton(3);
  if (buttons & 0x0010) triggerButton(4);
  if (buttons & 0x0020) triggerButton(5);
  if (buttons & 0x0040) triggerButton(6);
  if (buttons & 0x0080) triggerButton(7);
  if (buttons & 0x0100) triggerButton(8);
  if (buttons & 0x0200) triggerButton(9);
  if (buttons & 0x0400) triggerButton(10);
  if (buttons & 0x0800) triggerButton(11);
  if (buttons & 0x1000) triggerButton(12);
  if (buttons & 0x2000) triggerButton(13);
  if (buttons & 0x4000) triggerButton(14);
  if (buttons & 0x8000) triggerButton(15);
}

void RgbLed::setRumble(uint8_t intensity) {
  rumble_intensity = intensity;
}

void RgbLed::setAnalog(int16_t lx, int16_t ly) {
  stick_lx = lx;
  stick_ly = ly;
}

void RgbLed::setPlayerColor(uint8_t player) {
  rgb_color_t colors[] = { COLOR_P1, COLOR_P2, COLOR_P3, COLOR_P4 };
  if (player < 4) {
    setColor(colors[player]);
  }
}

void RgbLed::flash(rgb_color_t color, uint8_t times, uint16_t duration_ms) {
  uint32_t phase = (millis() / duration_ms) % (times * 2);
  if (phase < times * 2 && (phase & 1) == 0) {
    setAll(color);
  } else {
    setAll(COLOR_OFF);
  }
  show();
}

void RgbLed::showConnect() {
  setAll(COLOR_GREEN);
  show();
}

void RgbLed::showDisconnect() {
  setAll(COLOR_RED);
  show();
}

void RgbLed::showError() {
  setAll(COLOR_RED);
  show();
}

void RgbLed::showSuccess() {
  setAll(COLOR_GREEN);
  show();
}

RgbLed rgbLed;
