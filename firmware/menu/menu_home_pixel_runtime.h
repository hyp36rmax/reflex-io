#pragma once
#include "../product_config.h"
#include "oled_pixel_surface.h"

// The new Home layout is enabled for the audited Classic2USB renderer only.
// Other products retain their current screen layout; drawing helpers are shared.
void beginHomePixelFrame(bool fullRefresh = false);
void endHomePixelFrame();
class HomePixelFrame {
public:
  explicit HomePixelFrame(bool fullRefresh = false) { beginHomePixelFrame(fullRefresh); }
  ~HomePixelFrame() { endHomePixelFrame(); }
  HomePixelFrame(const HomePixelFrame&) = delete;
  HomePixelFrame& operator=(const HomePixelFrame&) = delete;
};
