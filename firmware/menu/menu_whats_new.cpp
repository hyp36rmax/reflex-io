#include "../product_config.h"

#include "menu_whats_new.h"

#include "../platform/display_runtime_state.h"
#if ADAPT_POLICY_WHATS_NEW_QR
#include "whats_new_qr.h"
#endif

// Text on the left, the QR code (already in panel page format) on the right.
void renderWhatsNewScreen() {
#if ADAPT_POLICY_WHATS_NEW_QR && defined(USE_I2C_DISPLAY)
  display.clear();
  display.setFont(System5x7);
  display.set1X();
  display.setCursor(0, 0);
  display.print(F("What's new"));
  display.setCursor(0, 2);
  display.print(F("Scan for"));
  display.setCursor(0, 3);
  display.print(F("release"));
  display.setCursor(0, 4);
  display.print(F("notes"));
  for (uint8_t page = 0; page < kWhatsNewQrPages; ++page) {
    display.setCursor(kWhatsNewQrX, page);
    for (uint8_t column = 0; column + 1 < kWhatsNewQrWidth; ++column) {
      display.ssd1306WriteRamBuf(kWhatsNewQrPageBytes[page][column]);
    }
    display.ssd1306WriteRam(kWhatsNewQrPageBytes[page][kWhatsNewQrWidth - 1]);
  }
#endif
}
