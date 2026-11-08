#include "../../product_config.h"

#include "boot_hardware_runtime.h"
#include "boot_hardware_product.h"

#include <Adafruit_TinyUSB.h>
#include <Arduino.h>

#include "hardware/watchdog.h"

#include "../../core/firmware_support.h"
#include "../../output/runtime/output_boot_bridge.h"

void prepareHardwareForBoot() {
  prepareProductHardwareForBoot();

  #ifdef ADAPT_OUTPUT_USB_DEVICE
  tud_disconnect();
  #if defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED)
  rp2040_usb_install_debug_irq();
  #endif
  #endif

  Serial.end();

  modeButton.begin();
  resetButton.begin();

  delay(10);
  #if defined(ADAPT_FEATURE_MODE_BUTTON) && \
      ADAPT_POLICY_MODE_BUTTON_BOOTLOADER_AT_STARTUP
  // Left is the bootloader button. Where it is a GPIO, firmware reads it here;
  // Classic2USB's Left is the chip's BOOTSEL pin. A board with no Left uses Mode.
  #ifdef ADAPT_FEATURE_RESET_BUTTON
  const uint8_t bootloaderHoldPin = PIN_RESET_BTN;
  #else
  const uint8_t bootloaderHoldPin = pinBtn;
  #endif
  // Only a press held from power-on asks for the bootloader. A hold that
  // restarts the adapter leaves a button down, and reboot() is a watchdog
  // reset. Setup ignores that press until it is released.
  if (digitalRead(bootloaderHoldPin) == LOW && !watchdog_caused_reboot()) {
    #ifdef LED_BUILTIN
      pinMode(LED_BUILTIN, OUTPUT);
      for (int i = 0; i < 3; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(100);
        digitalWrite(LED_BUILTIN, LOW);
        delay(100);
      }
    #endif
    rebootToBootloader();
  }
  #endif

  #ifdef LED_BUILTIN
    gpio_init(LED_BUILTIN);
    gpio_set_dir(LED_BUILTIN, 1);
    gpio_put(LED_BUILTIN, LOW);
  #endif
}
