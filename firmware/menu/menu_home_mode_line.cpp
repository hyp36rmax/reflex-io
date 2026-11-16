#include "../product_config.h"

#include "menu_home_mode_line.h"
#include "oled_pixel_surface.h"
#if defined(PRODUCT_CLASSIC2USB)
#include "menu_home_summary.h"
#include "../input/runtime/input_frame_runtime.h"
#endif

#include <cstdio>
#include <cstring>

#include <tusb.h>

#include "../core/device_runtime_state.h"
#if defined(ADAPT_CAP_ESP32_CONTROLLER_BRIDGE) && defined(ENABLE_INPUT_ESP32_SPI)
#include "../input/esp32_spi/Input_Esp32Spi.h"
#endif
#include "../input/mixed/input_mixed_runtime_state.h"
#include "../output/auth/auth_status.h"
#if defined(ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT)
#include "../output/auth/xbone_auth_passthrough.h"
#endif
#if ADAPT_POLICY_USB_BT_OUTPUT_SETTINGS
#include "../output/bluetooth/modern2usb_bt_output.h"
#endif
#include "../output/output_runtime_state.h"
#include "../platform/display_runtime_state.h"
#include "menu_mode_labels.h"

namespace {

constexpr uint8_t kHomeModeLineRow = 2;
constexpr uint8_t kTextCharPixels = 6;
constexpr uint8_t kFullModeLinePixels = 128;
constexpr uint8_t kStatusGlyphWidth = 12;
constexpr uint8_t kWirelessStatusCol =
    kFullModeLinePixels - kStatusGlyphWidth;
// Wi-Fi icon: three arcs over a dot, 12x8 in OLED page 0 (bit 0 is the top
// row), level with the "BT" label. Replaces font glyph 153 drawn at 2X, whose
// three flat bars read as a triangle (owner, 2026-11-11).
constexpr uint8_t kWirelessNetworkIcon[kStatusGlyphWidth] = {
    0x04, 0x02, 0x09, 0x25, 0x15, 0xD5, 0xD5, 0x15, 0x25, 0x09, 0x02, 0x04};

struct HomeRadioStatus {
  bool bluetoothConnected;
  bool wifiConnected;
};

HomeRadioStatus homeRadioStatus() {
  HomeRadioStatus status{};
#if defined(ADAPT_CAP_ESP32_CONTROLLER_BRIDGE) && defined(ENABLE_INPUT_ESP32_SPI)
  const Esp32SpiRuntimeStatus esp32 = esp32SpiRuntimeStatus();
  status.bluetoothConnected =
      esp32.connected_mask != 0 ||
      (esp32.bt_output_status_valid && esp32.bt_output_connected);
  status.wifiConnected = esp32.wifi_connected;
#endif
  return status;
}

bool modern2usbHomeShowsBluetoothTransport() {
#if ADAPT_POLICY_USB_BT_OUTPUT_SETTINGS && \
    defined(ENABLE_EXPERIMENTAL_MODERN2USB_BT_OUTPUT)
  if (modern2usbBtOutputPreference() == MODERN2USB_OUTPUT_TRANSPORT_USB) {
    return false;
  }
  return modern2usbBtOutputActiveTransport() != MODERN2USB_ACTIVE_OUTPUT_USB;
#else
  return false;
#endif
}

bool formatModern2UsbTransportOutputName(
    char* buffer, size_t bufferSize, bool compact) {
#if ADAPT_POLICY_USB_BT_OUTPUT_SETTINGS && \
    defined(ENABLE_EXPERIMENTAL_MODERN2USB_BT_OUTPUT)
  const ProductOutputTransportPreference preference =
      modern2usbBtOutputPreference();
  const Modern2UsbActiveOutputTransport active =
      modern2usbBtOutputActiveTransport();
  switch (active) {
    case MODERN2USB_ACTIVE_OUTPUT_WAITING:
      if (preference == MODERN2USB_OUTPUT_TRANSPORT_AUTO) {
        snprintf(buffer, bufferSize,
                 compact ? "USB/BT Auto" : "USB / Bluetooth Auto");
        return true;
      }
      if (preference == MODERN2USB_OUTPUT_TRANSPORT_BLUETOOTH) {
        snprintf(buffer, bufferSize,
                 compact ? "Starting BT" : "Starting Bluetooth");
        return true;
      }
      return false;
    case MODERN2USB_ACTIVE_OUTPUT_REQUESTING_BLUETOOTH:
      snprintf(buffer, bufferSize,
               compact ? "Starting BT" : "Starting Bluetooth");
      return true;
    case MODERN2USB_ACTIVE_OUTPUT_BLUETOOTH:
      snprintf(
          buffer,
          bufferSize,
          compact ? "BT %s" : "Bluetooth %s",
          modern2usbBtOutputPersonaName(modern2usbBtOutputPersona()));
      return true;
    case MODERN2USB_ACTIVE_OUTPUT_UNSUPPORTED:
      snprintf(buffer, bufferSize,
               compact ? "BT Error" : "Bluetooth Error");
      return true;
    case MODERN2USB_ACTIVE_OUTPUT_USB:
    default:
      return false;
  }
#else
  (void)buffer;
  (void)bufferSize;
  (void)compact;
  return false;
#endif
}


void drawHomeRadioStatus(const HomeRadioStatus& status) {
#ifndef USE_I2C_DISPLAY
  (void)status;
  return;
#else
  display.clear(0, kStatusGlyphWidth - 1, 0, 1);
  display.clear(kWirelessStatusCol, kFullModeLinePixels - 1, 0, 1);
  display.setFont(ReflexPad5x7);
  if (status.bluetoothConnected) {
    display.setCursor(0, 0);
    display.print("BT");
  }
  if (status.wifiConnected) {
    display.setCursor(kWirelessStatusCol, 0);
    for (uint8_t column = 0; column + 1 < kStatusGlyphWidth; ++column) {
      display.ssd1306WriteRamBuf(kWirelessNetworkIcon[column]);
    }
    display.ssd1306WriteRam(kWirelessNetworkIcon[kStatusGlyphWidth - 1]);
  }
  display.setFont(System5x7);
#endif
}

void formatAutoFallbackStatus(char* buffer, size_t bufferSize, bool compact) {
#if defined(AUTODETECT_SHOW_VERBOSE_OLED) && \
    (defined(AUTODETECT_VERBOSE_USB_HW_OLED) || defined(AUTODETECT_VERBOSE_OLED))
  const uint8_t descDevice = (output_autodetect_probe_counters >> 12) & 0x0F;
  const uint8_t descConfig = (output_autodetect_probe_counters >> 8) & 0x0F;
  const uint8_t interfaces = (output_autodetect_probe_counters >> 4) & 0x0F;
  const uint8_t requests = output_autodetect_probe_counters & 0x0F;
  if (compact) {
    snprintf(buffer, bufferSize, "Fb%02X%02X %X%X%X%X",
             output_autodetect_last_flags,
             output_autodetect_aux_flags,
             descDevice,
             descConfig,
             interfaces,
             requests);
    return;
  }
  snprintf(buffer, bufferSize, "Fb %02X%02X D%XC%XI%XR%X",
           output_autodetect_last_flags,
           output_autodetect_aux_flags,
           descDevice,
           descConfig,
           interfaces,
           requests);
#else
  switch (canonicalizeOutputMode(get_configured_mister_auto_output_mode())) {
    case OUTPUT_XINPUT2P:
      snprintf(buffer, bufferSize, "MiSTer XIn");
      break;
    case OUTPUT_KEYBOARD:
      snprintf(buffer, bufferSize, "MiSTer Kbd");
      break;
    default:
      snprintf(buffer, bufferSize, "MiSTer DIn");
      break;
  }
#endif
}

bool formatAuthAwareOutputName(outputMode_t mode, char* buffer, size_t bufferSize, bool compact) {
  mode = canonicalizeOutputMode(mode);
  if (mode == OUTPUT_PS5) {
    if (authOutputModeHasProvider(OUTPUT_PS5)) {
      snprintf(buffer, bufferSize, compact ? "PS5 Auth" : "PS5 (Auth)");
    } else {
      snprintf(buffer, bufferSize, compact ? "PS5 Lock" : "PS5 Locked");
    }
    return true;
  }

  if (mode != OUTPUT_PS4) {
    if (mode == OUTPUT_XBOXONE) {
#if defined(ENABLE_EXPERIMENTAL_XBOXONE_OUTPUT)
      if (!authOutputModeRequiresAuth(mode)) {
        snprintf(buffer, bufferSize, "Xbox One");
        return true;
      }
      const XboxOneAuthSidecarStatus xbox = xbone_auth_passthrough_status();
      if (xbox.auth_completed) {
        snprintf(buffer, bufferSize, compact ? "XB1 Auth" : "Xbox One (Auth)");
      } else if (xbox.mounted) {
        snprintf(buffer, bufferSize,
                 compact ? "XB1 Auth..." : "Xbox Authenticating");
      } else {
        snprintf(buffer, bufferSize,
                 compact ? "XB1 Lock" : "Xbox One Locked");
      }
#else
      snprintf(buffer, bufferSize, compact ? "XB1 Lock" : "Xbox One Locked");
#endif
      return true;
    }
    return false;
  }

  if (authOutputModeHasProvider(OUTPUT_PS4)) {
    snprintf(buffer, bufferSize, compact ? "PS4 Auth" : "PS4 (Auth)");
    return true;
  }

  // The PS4 host owns its unauthenticated grace policy. A firmware countdown
  // cannot know when the console started or accepted a controller session, so
  // report provider state without promising a misleading deadline.
  snprintf(buffer, bufferSize, compact ? "PS4 NoAuth" : "PS4 (No Auth)");
  return true;
}

bool formatWindowsAutoOutputName(outputMode_t mode, char* buffer, size_t bufferSize, bool compact) {
  switch (canonicalizeOutputMode(mode)) {
    case OUTPUT_HID:
    case OUTPUT_MISTER:
      snprintf(buffer, bufferSize, compact ? "Win DIn" : "Windows DIn");
      return true;
    case OUTPUT_XINPUT2P:
      snprintf(buffer, bufferSize, compact ? "Win XIn" : "Windows (XInput)");
      return true;
    case OUTPUT_KEYBOARD:
      snprintf(buffer, bufferSize, compact ? "Win Kbd" : "Windows (Keyboard)");
      return true;
    default:
      return false;
  }
}

bool formatXbox360OutputName(char* buffer, size_t bufferSize, bool compact) {
  snprintf(buffer, bufferSize, compact ? "XB360" : "Xbox 360");
  return true;
}

bool formatPlayStationResolvingName(char* buffer, size_t bufferSize, bool compact) {
  snprintf(buffer, bufferSize, compact ? "Detect PS" : "Detect PlayStation");
  return true;
}

bool formatForcedJvsioManagementName(outputMode_t mode, char* buffer, size_t bufferSize, bool compact) {
#if defined(FORCE_JVSIO_MANAGEMENT_OUTPUT)
  if (canonicalizeOutputMode(mode) == OUTPUT_MISTER) {
    snprintf(buffer, bufferSize, compact ? "Win DIn" : "Windows DIn");
    return true;
  }
#else
  (void)mode;
  (void)buffer;
  (void)bufferSize;
  (void)compact;
#endif
  return false;
}

void copyLabelChars(char* dest, size_t destSize, const char* src, uint8_t maxChars) {
  if (destSize == 0) {
    return;
  }
  size_t i = 0;
  for (; i + 1 < destSize && i < maxChars && src[i] != '\0'; i++) {
    dest[i] = src[i];
  }
  dest[i] = '\0';
}

}  // namespace

const char* getOutputDisplayName() {
  static char auth_status[24];
  if (formatModern2UsbTransportOutputName(
          auth_status, sizeof(auth_status), false)) {
    return auth_status;
  }
  if (configuredOutputMode == OUTPUT_AUTO) {
    static char auto_status[24];
    if (autoDetectState == AUTO_STATE_FALLBACK_HID) {
      formatAutoFallbackStatus(auto_status, sizeof(auto_status), false);
      return auto_status;
    }
    if (autoDetectState != AUTO_STATE_IDLE) {
      if (autoDetectState == AUTO_STATE_PLAYSTATION) {
        formatPlayStationResolvingName(auth_status, sizeof(auth_status), false);
        return auth_status;
      }
      if (autoDetectState == AUTO_STATE_WINDOWS &&
          formatWindowsAutoOutputName(outputMode, auth_status, sizeof(auth_status), false)) {
        return auth_status;
      }
      if (autoDetectState == AUTO_STATE_XBOX_360) {
        formatXbox360OutputName(auth_status, sizeof(auth_status), false);
        return auth_status;
      }
      if (formatAuthAwareOutputName(outputMode, auth_status, sizeof(auth_status), false)) {
        return auth_status;
      }
      return getOutputShortName(outputMode);
    }
    snprintf(auto_status, sizeof(auto_status), "Detecting host...");
    return auto_status;
  }
  if (configuredOutputMode == OUTPUT_XINPUT) {
    formatXbox360OutputName(auth_status, sizeof(auth_status), false);
    return auth_status;
  }
  if (formatAuthAwareOutputName(outputMode, auth_status, sizeof(auth_status), false)) {
    return auth_status;
  }
  if (formatForcedJvsioManagementName(outputMode, auth_status, sizeof(auth_status), false)) {
    return auth_status;
  }
  return getOutputShortName(outputMode);
}

const char* getOutputDisplayCompactName() {
  static char auth_status[16];
  if (formatModern2UsbTransportOutputName(
          auth_status, sizeof(auth_status), true)) {
    return auth_status;
  }
  if (configuredOutputMode == OUTPUT_AUTO) {
    static char auto_status[16];
    if (autoDetectState == AUTO_STATE_FALLBACK_HID) {
      formatAutoFallbackStatus(auto_status, sizeof(auto_status), true);
      return auto_status;
    }
    if (autoDetectState != AUTO_STATE_IDLE) {
      if (autoDetectState == AUTO_STATE_PLAYSTATION) {
        formatPlayStationResolvingName(auth_status, sizeof(auth_status), true);
        return auth_status;
      }
      if (autoDetectState == AUTO_STATE_WINDOWS &&
          formatWindowsAutoOutputName(outputMode, auth_status, sizeof(auth_status), true)) {
        return auth_status;
      }
      if (autoDetectState == AUTO_STATE_XBOX_360) {
        formatXbox360OutputName(auth_status, sizeof(auth_status), true);
        return auth_status;
      }
      if (formatAuthAwareOutputName(outputMode, auth_status, sizeof(auth_status), true)) {
        return auth_status;
      }
      return getOutputCompactName(outputMode);
    }
    snprintf(auto_status, sizeof(auto_status), "Detecting");
    return auto_status;
  }
  if (configuredOutputMode == OUTPUT_XINPUT) {
    formatXbox360OutputName(auth_status, sizeof(auth_status), true);
    return auth_status;
  }
  if (formatForcedJvsioManagementName(outputMode, auth_status, sizeof(auth_status), true)) {
    return auth_status;
  }
  if (formatAuthAwareOutputName(outputMode, auth_status, sizeof(auth_status), true)) {
    return auth_status;
  }
  return getOutputCompactName(outputMode);
}

void renderHomeModeLine(bool showScanning, bool force) {
  oled_pixel::Origin modeOrigin(1, 17, 23);
#ifndef USE_I2C_DISPLAY
  (void)showScanning;
  (void)force;
  return;
#else
#if defined(ADAPT_CAP_ARCADE_MAPPING_SESSION)
  (void)showScanning;
  if (force) {
    display.clear(0, 127, kHomeModeLineRow, kHomeModeLineRow);
  }
  return;
#endif
  const HomeRadioStatus radioStatus = homeRadioStatus();
  constexpr uint8_t maxModeLineChars = 21;
  constexpr uint8_t availablePixels = kFullModeLinePixels;
#if defined(ADAPT_CAP_JVS_INPUT_STATUS) || \
    defined(ADAPT_BRIDGE_USB_TO_USB)
  const char* displayName = showScanning ? "Scanning" : getOutputDisplayName();
  const char* compactName = showScanning ? "Scan" : getOutputDisplayCompactName();
  if (strlen(displayName) > maxModeLineChars) {
    displayName = compactName;
  }

  static char lastModeLine[24] = {0};
  char modeLine[24];
  snprintf(modeLine, sizeof(modeLine), "%s", displayName);
  bool splitModeLine = false;
#else
#if defined(PRODUCT_CLASSIC2USB)
  static char lastModeLine[24] = {0};
  char modeLine[24];
  const auto controllers = home_summary::connected(
      max_devices < MAX_USB_OUT ? max_devices : MAX_USB_OUT,
      [](uint8_t p) -> const controller_state_t& { return controllerFrameConst(p); });
  home_summary::format(modeLine, sizeof(modeLine), controllers, showScanning,
      inputMixedModeActive(), getInputShortName(deviceMode), getInputCompactName(deviceMode),
      getOutputDisplayName(), getOutputDisplayCompactName());
  bool splitModeLine = false;
#else
  const bool showMixedInput = !showScanning && inputMixedModeActive();
  const char* inputName = showScanning ? "Auto" : (showMixedInput ? "Mixed" : getInputShortName(deviceMode));
  const char* compactInputName = showScanning ? "Auto" : (showMixedInput ? "Mixed" : getInputCompactName(deviceMode));
  const char* outputName = getOutputDisplayName();
  const char* compactOutputName = getOutputDisplayCompactName();

  const char* displayInputName = inputName;
  const char* displayOutputName = outputName;
  if (strlen(displayInputName) + 1 + strlen(displayOutputName) > maxModeLineChars) {
    displayOutputName = compactOutputName;
  }
  if (strlen(displayInputName) + 1 + strlen(displayOutputName) > maxModeLineChars) {
    displayInputName = compactInputName;
  }

  static char lastModeLine[24] = {0};
  char modeLine[24];
  const uint8_t maxAvailableChars = availablePixels / kTextCharPixels;
  char combinedModeLine[48];
  snprintf(combinedModeLine, sizeof(combinedModeLine), "%s>%s", displayInputName, displayOutputName);
  copyLabelChars(modeLine, sizeof(modeLine), combinedModeLine, maxAvailableChars);
  bool splitModeLine = false;
#endif
#endif

  static bool lastBluetoothConnected = false;
  static bool lastWifiConnected = false;
  if (!force && lastBluetoothConnected == radioStatus.bluetoothConnected &&
      lastWifiConnected == radioStatus.wifiConnected &&
      strcmp(lastModeLine, modeLine) == 0) {
    return;
  }

  strncpy(lastModeLine, modeLine, sizeof(lastModeLine) - 1);
  lastModeLine[sizeof(lastModeLine) - 1] = '\0';
  lastBluetoothConnected = radioStatus.bluetoothConnected;
  lastWifiConnected = radioStatus.wifiConnected;

  display.clear(0, 127, kHomeModeLineRow, kHomeModeLineRow);
  display.setRow(kHomeModeLineRow);
  if (splitModeLine) {
    display.setCol(0);
    display.print(modeLine);
  } else {
    uint8_t totalPixels = strlen(modeLine) * kTextCharPixels;
    uint8_t startCol = (totalPixels < availablePixels) ? ((availablePixels - totalPixels) / 2) : 0;
    display.setCol(startCol);
    display.print(modeLine);
  }
  drawHomeRadioStatus(radioStatus);
#endif
}
