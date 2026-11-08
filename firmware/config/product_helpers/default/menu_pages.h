#pragma once

inline bool productMenuPageActive() { return false; }
inline void closeProductMenuPage() {}
inline bool handleProductMenuPage(const MenuControllerInput&, bool, bool,
                                  uint8_t, uint8_t, bool) {
  return false;
}
inline void openProductMenuPage(uint8_t) {}
inline void openProductControllerSettingsPage() {}
inline void openProductControllerSelectorPage() {}
