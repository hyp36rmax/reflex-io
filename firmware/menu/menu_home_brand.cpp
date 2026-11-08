#include "menu_home_brand.h"

#include "../product_display_identity.h"
#include "menu.h"
#include "menu_runtime_state.h"
#include "screensaver_rflx_bitmap.h"

#include <Arduino.h>
#include <cstring>

namespace {

constexpr uint8_t kHomeLogoCanvasHeight = 16;
#if defined(PRODUCT_CLASSIC2USB)
// Inset the bitmap inside the existing header; keep the summary and pads fixed.
constexpr uint8_t kHomeLogoInset = 1;
#else
constexpr uint8_t kHomeLogoInset = 0;
#endif
constexpr uint8_t kHomeLogoHeight = kHomeLogoCanvasHeight - 2 * kHomeLogoInset;
constexpr uint8_t kHomeLogoPages = kHomeLogoCanvasHeight / 8;
constexpr uint8_t kHomeLogoStartCol =
    (128 - kRflxScreensaverLogoWidth) / 2;
constexpr uint8_t kRflxLogoSourceBytesPerRow =
    (kRflxScreensaverLogoWidth + 7) / 8;

bool homeLogoPixel(uint8_t x, uint8_t y) {
  if (y < kHomeLogoInset || y >= kHomeLogoCanvasHeight - kHomeLogoInset) return false;
  y -= kHomeLogoInset;
  const uint8_t sourceY = (uint8_t)(
      ((uint16_t)y * (kRflxScreensaverLogoHeight - 1) +
       ((kHomeLogoHeight - 1) / 2)) /
      (kHomeLogoHeight - 1));
  const uint16_t sourceIndex =
      (uint16_t)sourceY * kRflxLogoSourceBytesPerRow + (x / 8);
  const uint8_t sourceByte = pgm_read_byte(
      &kRflxScreensaverLogoBitmap[sourceIndex]);
  return (sourceByte & (uint8_t)(1U << (x & 7))) != 0;
}

void drawRflxLogo() {
  for (uint8_t page = 0; page < kHomeLogoPages; ++page) {
    display.setCursor(kHomeLogoStartCol, page);
    for (uint8_t x = 0; x < kRflxScreensaverLogoWidth; ++x) {
      uint8_t pageBits = 0;
      for (uint8_t bit = 0; bit < 8; ++bit) {
        const uint8_t y = (uint8_t)(page * 8 + bit);
        if (homeLogoPixel(x, y)) {
          pageBits |= (uint8_t)(1U << bit);
        }
      }
      display.ssd1306WriteRamBuf(pageBits);
    }
  }
  display.flush();
}

void drawTextTitle() {
  display.set2X();
  display.setRow(0);
  const char* title = getProductHomeDisplayTitle();
  const size_t titlePixels = std::strlen(title) * 12;
  const uint8_t titleStart =
      titlePixels < 128 ? (uint8_t)((128 - titlePixels) / 2) : 0;
  display.setCol(titleStart);
  display.print(title);
  display.set1X();
}

#if ADAPT_POLICY_TOURNAMENT_KIOSK
void drawTournamentBadge() {
  display.setFont(System5x7);
  display.set1X();
  display.setCursor(34, 0);
  display.setInvertMode(true);
  display.print(F(" TOURNAMENT "));
  display.setInvertMode(false);
  display.setCursor(52, 1);
  display.print(F("RFLX"));
}
#endif

}  // namespace

void renderProductHomeBrand() {
#if ADAPT_POLICY_TOURNAMENT_KIOSK
  if (menu_kiosk_mode != 0) {
    drawTournamentBadge();
    return;
  }
#endif
  if (productUsesRflxHomeLogo()) {
    drawRflxLogo();
    return;
  }
  drawTextTitle();
}
