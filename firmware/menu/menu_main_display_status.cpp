#include "../product_config.h"

#include "menu.h"
#include "menu_main_display_internal.h"
#include "oled_pixel_surface.h"
#include "no_host_controller_test_runtime.h"

#include "../output/output_capabilities.h"
#include "../output/output_runtime_state.h"
#include "../core/controller_settings_state.h"
#include "../core/rumble_test_runtime.h"

#ifdef ENABLE_INPUT_USB_BT
#include "../input/usb_bt_composite/Input_UsbBt.h"
#endif

#if defined(ENABLE_INPUT_SNES) || defined(ENABLE_INPUT_NES) || defined(ENABLE_INPUT_VBOY)
#include "../input/snes/input_snes_runtime_state.h"
#endif

#ifdef ENABLE_INPUT_JVS
#include "../input/jvs/jvs_host_runtime.h"
#endif
#ifdef ENABLE_EXPERIMENTAL_XINPUT2P_OUTPUT
#ifdef FORCE_XINPUT2P_SINGLE_DRIVER
#include "../output/xinput/out_xinput.h"
#include "../output/xinput/output_xinput_auth_runtime_state.h"
#endif
#ifdef ENABLE_XINPUT2P_WIRELESS_TRANSPORT
#include "../output/xinputw/out_xinputw.h"
#endif
#endif

#include <Arduino.h>
#include <cstdio>
#include <cstring>

namespace menu_main_display_internal {

namespace {

constexpr uint8_t kOledWidthPixels = 128;
constexpr uint8_t kSystemFontCharacterWidth = 6;
constexpr size_t kFooterNameCharacterBudget = 20;
constexpr size_t kPlayerListNameCharacterBudget = 17;

uint8_t connectedControllerCount() {
  const uint8_t players = output_shown_player_count(get_effective_output_mode());
  uint8_t count = 0;
  for (uint8_t player = 0;
       player < players && player < MAX_USB_OUT;
       ++player) {
    if (controllerFrameConst(player).connected) {
      ++count;
    }
  }
  return count;
}

void formatControllerDisplayName(uint8_t player, char* out, size_t outSize) {
  if (out == nullptr || outSize == 0) {
    return;
  }
  out[0] = '\0';
  if (player >= max_devices || player >= MAX_USB_OUT) {
    return;
  }
  const controller_state_t& frame = controllerFrameConst(player);
  if (!frame.connected) {
    return;
  }
  const char* name = frame.controller_type_name[0]
      ? frame.controller_type_name : "Connected";
  bool appendBluetooth = false;
#ifdef ENABLE_INPUT_USB_BT
  const UsbBtSlotOwner owner = usbBtLogicalPlayerOwner(player);
  appendBluetooth =
      owner.source == USB_BT_INPUT_BLUETOOTH &&
      std::strstr(name, "BT") == nullptr;
#endif
  std::snprintf(out, outSize, appendBluetooth ? "%s (BT)" : "%s", name);
}

void copyVisibleName(char* out, size_t outSize, const char* name,
                     size_t characterCount) {
  if (out == nullptr || outSize == 0) {
    return;
  }
  const size_t nameLength = name != nullptr ? std::strlen(name) : 0;
  const size_t copyLength = characterCount < nameLength
      ? characterCount : nameLength;
  const size_t boundedLength = copyLength < outSize - 1
      ? copyLength : outSize - 1;
  if (boundedLength != 0) {
    std::memcpy(out, name, boundedLength);
  }
  out[boundedLength] = '\0';
}

void renderMultiSourceControllerNames() {
  uint8_t connectedPlayers[MAX_USB_OUT] = {};
  uint8_t visibleCount = 0;
  for (uint8_t player = 0;
       player < max_devices && player < MAX_USB_OUT;
       ++player) {
    if (controllerFrameConst(player).connected) {
      connectedPlayers[visibleCount++] = player;
    }
  }

  if (visibleCount >= 3) {
    for (uint8_t row = 3; row <= 7; ++row) {
      clearRow(row);
    }
    uint8_t row = visibleCount >= 4 ? 3 : 4;
    for (uint8_t visible = 0; visible < visibleCount && row <= 6;
         ++visible, ++row) {
      const uint8_t player = connectedPlayers[visible];
      char name[22];
      char line[22];
      formatControllerDisplayName(player, name, sizeof(name));
      std::snprintf(line, sizeof(line), "P%u: %.*s",
                    static_cast<unsigned>(player + 1),
                    static_cast<int>(kPlayerListNameCharacterBudget), name);
      display.setRow(row);
      display.setCol(0);
      display.print(line);
    }
    return;
  }

  clearRow(7);
  display.setRow(7);
  if (visibleCount == 0) {
    return;
  }

  char firstName[22];
  formatControllerDisplayName(connectedPlayers[0], firstName,
                              sizeof(firstName));
  if (visibleCount == 1) {
    const size_t namePixels = std::strlen(firstName) *
                              kSystemFontCharacterWidth;
    display.setCol(namePixels < kOledWidthPixels
        ? static_cast<uint8_t>((kOledWidthPixels - namePixels) / 2)
        : 0);
    display.print(firstName);
    return;
  }

  char secondName[22];
  formatControllerDisplayName(connectedPlayers[1], secondName,
                              sizeof(secondName));
  size_t firstChars = std::strlen(firstName);
  size_t secondChars = std::strlen(secondName);
  if (firstChars + secondChars > kFooterNameCharacterBudget) {
    size_t firstBudget = kFooterNameCharacterBudget / 2;
    size_t secondBudget = kFooterNameCharacterBudget - firstBudget;
    if (firstChars < firstBudget) {
      secondBudget += firstBudget - firstChars;
      firstBudget = firstChars;
    } else if (secondChars < secondBudget) {
      firstBudget += secondBudget - secondChars;
      secondBudget = secondChars;
    }
    firstChars = firstChars < firstBudget ? firstChars : firstBudget;
    secondChars = secondChars < secondBudget ? secondChars : secondBudget;
  }

  char firstVisible[22];
  char secondVisible[22];
  copyVisibleName(firstVisible, sizeof(firstVisible), firstName, firstChars);
  copyVisibleName(secondVisible, sizeof(secondVisible), secondName, secondChars);
  display.setCol(0);
  display.print(firstVisible);
  display.setCol(static_cast<uint8_t>(
      kOledWidthPixels - secondChars * kSystemFontCharacterWidth));
  display.print(secondVisible);
}

}  // namespace

#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
namespace {

constexpr uint8_t kStatusCharsPerRow = 21;

const char* getJvsBoardVendorLabel(const char* ioId) {
  if (!ioId || ioId[0] == '\0') {
    return nullptr;
  }
  if (std::strstr(ioId, "SEGA") != nullptr) return "Sega";
  if (std::strstr(ioId, "CAPCOM") != nullptr) return "Capcom";
  if (std::strstr(ioId, "TAITO") != nullptr) return "Taito";
  if (std::strstr(ioId, "KONAMI") != nullptr) return "Konami";
  if (std::strstr(ioId, "NAMCO") != nullptr) return "Namco";
  if (std::strstr(ioId, "BANDAI") != nullptr) return "Bandai";
  if (std::strstr(ioId, "EXA") != nullptr) return "exA";
  return nullptr;
}

void copyTrimmedToken(char* out, size_t outSize, const char* start, size_t length) {
  if (outSize == 0) return;
  out[0] = '\0';
  while (length > 0 && (*start == ' ' || *start == '\t')) {
    ++start;
    --length;
  }
  while (length > 0 && (start[length - 1] == ' ' || start[length - 1] == '\t')) {
    --length;
  }

  size_t outIndex = 0;
  while (outIndex + 1 < outSize && outIndex < length) {
    out[outIndex] = start[outIndex];
    ++outIndex;
  }
  out[outIndex] = '\0';
}

bool tokenLooksLikeJvsModel(const char* token) {
  if (!token || token[0] == '\0') {
    return false;
  }

  bool hasDigit = false;
  bool hasDash = false;
  for (size_t i = 0; token[i] != '\0'; ++i) {
    if (token[i] >= '0' && token[i] <= '9') {
      hasDigit = true;
    } else if (token[i] == '-') {
      hasDash = true;
    }
  }
  return hasDigit && hasDash;
}

void findJvsBoardModelToken(char* out, size_t outSize, const char* ioId) {
  if (outSize == 0) return;
  out[0] = '\0';
  if (!ioId || ioId[0] == '\0') {
    return;
  }

  const char* tokenStart = ioId;
  while (*tokenStart != '\0') {
    const char* tokenEnd = tokenStart;
    while (*tokenEnd != '\0' && *tokenEnd != ';') {
      ++tokenEnd;
    }

    char token[16];
    copyTrimmedToken(token, sizeof(token), tokenStart, (size_t)(tokenEnd - tokenStart));
    if (tokenLooksLikeJvsModel(token)) {
      copyTrimmedToken(out, outSize, token, std::strlen(token));
      return;
    }

    tokenStart = (*tokenEnd == ';') ? tokenEnd + 1 : tokenEnd;
  }
}

void formatJvsBoardDisplayName(char* out, size_t outSize) {
  if (outSize == 0) return;
  out[0] = '\0';
#ifdef ENABLE_INPUT_JVS
  const JvsBoardInfo& info = getJvsBoardInfo();
  if (!info.id_received || info.io_id[0] == '\0') {
    std::snprintf(out, outSize, "JVS");
    return;
  }

  const char* vendor = getJvsBoardVendorLabel(info.io_id);
  if (vendor) {
    std::snprintf(out, outSize, "%s JVS", vendor);
    return;
  }

  char token[12] = {0};
  size_t i = 0;
  while (info.io_id[i] != '\0' &&
         info.io_id[i] != ' ' &&
         info.io_id[i] != ',' &&
         info.io_id[i] != ';' &&
         i + 1 < sizeof(token)) {
    token[i] = info.io_id[i];
    ++i;
  }
  token[i] = '\0';
  if (token[0] != '\0') {
    std::snprintf(out, outSize, "%s JVS", token);
  } else {
    std::snprintf(out, outSize, "JVS");
  }
#else
  std::snprintf(out, outSize, "JVS");
#endif
}

void formatJvsBoardModelLine(char* out, size_t outSize) {
  if (outSize == 0) return;
  out[0] = '\0';
#ifdef ENABLE_INPUT_JVS
  const JvsBoardInfo& info = getJvsBoardInfo();
  if (!info.id_received || info.io_id[0] == '\0') {
    std::snprintf(out, outSize, "JVS");
    return;
  }

  char model[16];
  findJvsBoardModelToken(model, sizeof(model), info.io_id);

  const char* vendor = getJvsBoardVendorLabel(info.io_id);
  if (vendor && model[0] != '\0') {
    std::snprintf(out, outSize, "%s %s JVS", vendor, model);
  } else if (vendor) {
    std::snprintf(out, outSize, "%s JVS", vendor);
  } else if (model[0] != '\0') {
    std::snprintf(out, outSize, "%s JVS", model);
  } else {
    copyTrimmedToken(out, outSize, info.io_id, kStatusCharsPerRow);
  }
#else
  std::snprintf(out, outSize, "JVS");
#endif
}

}  // namespace
#endif

void renderModeButtonIndicator(bool force) {
  (void)force;
}

bool shouldShowAutoDetectScanning(uint8_t connectedCount) {
  #ifdef ENABLE_INPUT_AUTODETECT
  return isAutoDetectMode && (deviceMode == RZORD_AUTODETECT) && (connectedCount == 0);
  #else
  (void)connectedCount;
  return false;
  #endif
}

bool shouldShowMultiControllerNameList() {
#if defined(ADAPT_CAP_MULTI_SOURCE_CONTROLLER_INPUT)
  return connectedControllerCount() >= 3;
#else
  return false;
#endif
}

void renderAutoDetectScanningStatus() {
  oled_pixel::Origin statusOrigin(1, 25, 63);
  display.setRow(3);
  display.setCol(0);
  display.print("Assisted detect hold...");

  display.setRow(4);
  display.setCol(0);
  #if AUTODETECT_ENABLE_PASSIVE_ASSIST
  display.print("Neo-Geo: Start");
  #else
  display.print("Active detect only");
  #endif

  display.setRow(5);
  display.setCol(0);
  #if AUTODETECT_ENABLE_PASSIVE_ASSIST
  display.print("Japanese PC: A/1");
  #else
  display.print("Waiting for pad...");
  #endif

  display.setRow(6);
  display.setCol(0);
  #if AUTODETECT_ENABLE_PASSIVE_ASSIST
  display.print("2600/C64/SMS: Fire");
  #else
  display.print("Reconnect controller");
  #endif

  display.setRow(7);
  display.setCol(0);
  #if AUTODETECT_ENABLE_PASSIVE_ASSIST
    #ifdef ENABLE_INPUT_JAGUAR
  display.print("Jaguar: Pause");
    #else
  display.print("2600/C64/SMS: Fire");
    #endif
  #else
  display.print("for auto-detect");
  #endif
}

void renderConnectedPortNames() {
#if defined(PRODUCT_CLASSIC2USB)
  // Identity/output now share the line below RFLX. No footer, even for 2P
  // or power-only use; rumble tests remain available from the Quick menu.
  return;
#endif
  oled_pixel::Origin nameOrigin(1, 57, 63);
  const char* rumbleHint = noHostControllerRumbleHint();
  if (rumbleHint != nullptr) {
    clearRow(7);
    display.setRow(7);
    const uint8_t width = (uint8_t)(std::strlen(rumbleHint) * 6);
    display.setCol(width < 128 ? (uint8_t)((128 - width) / 2) : 0);
    display.print(rumbleHint);
    return;
  }

#if defined(ADAPT_CAP_JVS_INPUT_STATUS)
  display.setRow(7);
  clearRow(7);

  outputMode_t effectiveOutputModeJvs = get_effective_output_mode();
  if (output_has_secondary_player_slot(effectiveOutputModeJvs) && max_devices > 1) {
    char boardModel[kStatusCharsPerRow + 1];
    formatJvsBoardModelLine(boardModel, sizeof(boardModel));
    display.setCol(0);
    display.print(boardModel);
    return;
  }

  char boardName[22];
  formatJvsBoardDisplayName(boardName, sizeof(boardName));
  display.setCol(0);
  display.print(boardName);

  const char* outputName = getVirtualOutputPadName(deviceMode, effectiveOutputModeJvs);
  if (outputName) {
    display.setCol(66);
    display.print(outputName);
  }
  return;
#endif

  display.setRow(7);
  display.setCol(0);

  const uint8_t physicalConnectedCount = connectedControllerCount();
  const bool centerSingleControllerName = physicalConnectedCount == 1;
#if defined(ADAPT_CAP_MULTI_SOURCE_CONTROLLER_INPUT)
  renderMultiSourceControllerNames();
  return;
#endif


  bool port1_has_device = false;
  const char* port1_name = nullptr;

  #if defined(ENABLE_INPUT_SNES) || defined(ENABLE_INPUT_NES) || defined(ENABLE_INPUT_VBOY)
  if ((deviceMode == RZORD_SNES || deviceMode == RZORD_NES || deviceMode == RZORD_VBOY)
      && snes_debug_mtap_enabled0 && snes_debug_tap0 > 0) {
    port1_has_device = true;
    port1_name = "Multitap";
  } else
  #endif
  if (controllerFrameConst(0).connected) {
    const controller_state_t& port1 = controllerFrameConst(0);
    port1_has_device = true;
    const bool hideNeGconName =
      deviceMode == RZORD_PSX && std::strcmp(port1.controller_type_name, "neGcon") == 0;
    port1_name = hideNeGconName ? "" :
                 (port1.controller_type_name[0] ? port1.controller_type_name :
                  (is_passive_controller_mode(deviceMode) ? nullptr : "Connected"));
  }
  if (centerSingleControllerName && port1_has_device && port1_name) {
    const uint8_t name_pixels = (uint8_t)(strlen(port1_name) * 6);
    display.setCol(name_pixels < 128 ? (uint8_t)((128 - name_pixels) / 2) : 0);
  }
  #ifdef ENABLE_INPUT_JAGUAR
  if (port1_has_device && port1_name && deviceMode != RZORD_JAGUAR) {
    display.print(port1_name);
  } else if (deviceMode != RZORD_JAGUAR) {
  #else
  if (port1_has_device && port1_name) {
    display.print(port1_name);
  } else {
  #endif
    display.setCol(20);
    display.print("--");
  }

  bool port2_has_device = false;
  const char* port2_name = nullptr;
  uint8_t port2_first_idx = 1;
  outputMode_t effectiveOutputMode = get_effective_output_mode();
  const bool mergedSinglePadView = classic_dual_merge_enabled != 0;
  bool hideEmptySecondPad =
    centerSingleControllerName || shouldHideEmptySecondPadInAutoInputMode();
  bool showOutputVirtualPad =
    !mergedSinglePadView &&
    controllerFrameConst(0).connected &&
    shouldShowVirtualOutputPad(deviceMode, effectiveOutputMode) &&
    ((output_shown_player_count(effectiveOutputMode) == 1) || hideEmptySecondPad);
  bool showVirtualOutputName =
    !mergedSinglePadView &&
    !centerSingleControllerName &&
    (showOutputVirtualPad ||
    (controllerFrameConst(0).connected &&
     shouldShowVirtualOutputPad(deviceMode, effectiveOutputMode) &&
     !hideEmptySecondPad));

  if (!mergedSinglePadView && !hideEmptySecondPad) {
    #if defined(ENABLE_INPUT_SNES) || defined(ENABLE_INPUT_NES) || defined(ENABLE_INPUT_VBOY)
    if ((deviceMode == RZORD_SNES || deviceMode == RZORD_NES || deviceMode == RZORD_VBOY)
        && snes_debug_mtap_enabled1 && snes_debug_tap1 > 0) {
      port2_has_device = true;
      port2_name = "Multitap";
    } else {
      if ((deviceMode == RZORD_SNES || deviceMode == RZORD_NES || deviceMode == RZORD_VBOY)
          && snes_debug_mtap_enabled0 && snes_debug_tap0 > 0) {
        port2_first_idx = snes_port0_controller_count;
      }
    #else
    {
    #endif
      if (port2_first_idx < max_devices && port2_first_idx < MAX_USB_OUT && controllerFrameConst(port2_first_idx).connected) {
        const controller_state_t& port2 = controllerFrameConst(port2_first_idx);
        port2_has_device = true;
        const bool hideNeGconName =
          deviceMode == RZORD_PSX && std::strcmp(port2.controller_type_name, "neGcon") == 0;
        port2_name = hideNeGconName ? "" :
                     (port2.controller_type_name[0] ? port2.controller_type_name :
                      (is_passive_controller_mode(deviceMode) ? nullptr : "Connected"));
      }
    }
  }

  if (!port2_has_device && showVirtualOutputName) {
    port2_has_device = true;
    port2_name = getVirtualOutputPadName(deviceMode, effectiveOutputMode);
    if (port2_name != nullptr && std::strcmp(port2_name, "neGcon") == 0) {
      port2_name = "";
    }
  }
  #ifdef ENABLE_INPUT_JAGUAR
  if (port2_has_device && port2_name && deviceMode != RZORD_JAGUAR) {
    const uint8_t name_pixels = (uint8_t)(strlen(port2_name) * 6);
    display.setCol(name_pixels < 128 ? (uint8_t)(128 - name_pixels) : 66);
    display.print(port2_name);
  } else if (!hideEmptySecondPad && deviceMode != RZORD_JAGUAR) {
  #else
  if (port2_has_device && port2_name) {
    const uint8_t name_pixels = (uint8_t)(strlen(port2_name) * 6);
    display.setCol(name_pixels < 128 ? (uint8_t)(128 - name_pixels) : 66);
    display.print(port2_name);
  } else if (!hideEmptySecondPad) {
  #endif
    display.setCol(91);
    display.print("--");
  }
}

void renderJvsRawDebugLine(bool force) {
  #if defined(ADAPT_CAP_JVS_INPUT_STATUS) && defined(ENABLE_JVS_OLED_DIAG) && defined(ENABLE_INPUT_JVS)
  if (deviceMode != RZORD_JVS) {
    return;
  }

  const JvsDebugSnapshot debug = getJvsDebugSnapshot();
  char line[22];
  if (((millis() / 1500u) & 1u) == 0u) {
    std::snprintf(line, sizeof(line), "S%u V%02u C%03lu H%03lu",
                  debug.machine_state,
                  (unsigned)(debug.sense_mv / 100u),
                  (unsigned long)(debug.service_count % 1000u),
                  (unsigned long)(debug.host_ready_count % 1000u));
  } else {
    std::snprintf(line, sizeof(line), "T%03lu R%03lu E%02X L%02X",
                  (unsigned long)(debug.uart_tx_byte_count % 1000u),
                  (unsigned long)(debug.uart_rx_byte_count % 1000u),
                  debug.last_uart_rx_error_flags,
                  debug.last_rx_byte);
  }

  static char lastLine[22] = {0};
  if (!force && std::strcmp(lastLine, line) == 0) {
    return;
  }
  std::strncpy(lastLine, line, sizeof(lastLine) - 1);
  lastLine[sizeof(lastLine) - 1] = '\0';

  clearRow(3);
  display.setRow(3);
  display.setCol(0);
  display.print(line);
  #else
  (void)force;
  #endif
}

bool shouldShowXinputMultiDiagOverlay() {
#if defined(ENABLE_XINPUT_MULTI_OLED_DIAG) && defined(ENABLE_EXPERIMENTAL_XINPUT2P_OUTPUT)
  return get_effective_output_mode() == OUTPUT_XINPUT2P;
#else
  return false;
#endif
}

void renderXinputMultiDiagOverlay(bool force) {
#if defined(ENABLE_XINPUT_MULTI_OLED_DIAG) && defined(ENABLE_EXPERIMENTAL_XINPUT2P_OUTPUT)
  if (!shouldShowXinputMultiDiagOverlay()) {
    return;
  }

  char lines[4][22];
#ifdef FORCE_XINPUT2P_SINGLE_DRIVER
  XInputDebugInfo xin = {};
  xinput_get_debug_info(&xin);
  XInputAuthRuntimeState& state = xinputAuthRuntimeState();
  RumbleRuntimePortDiag rumbleDiag = {};
  rumbleRuntimeGetPortDiag(0, &rumbleDiag);
  std::snprintf(lines[0], sizeof(lines[0]), "S EP %02X%02X I%03u",
                xin.endpoint_in,
                xin.endpoint_out,
                (unsigned)(state.debug_xfer_in_complete_count % 1000u));
  std::snprintf(lines[1], sizeof(lines[1]), "A K%03u F%03u C%03u",
                (unsigned)(state.debug_xfer_out_submit_ok_count % 1000u),
                (unsigned)(state.debug_xfer_out_submit_fail_count % 1000u),
                (unsigned)(state.debug_control_count % 1000u));
  std::snprintf(lines[2], sizeof(lines[2]), "X0 %03u S%02u L%02X",
                (unsigned)(state.debug_xfer_out_count % 1000u),
                state.debug_last_out_size,
                state.debug_last_rumble_left);
  #if defined(ENABLE_INPUT_SNES) || defined(ENABLE_INPUT_NES) || defined(ENABLE_INPUT_VBOY)
  const uint8_t snesRumbleData = (snes_rumble_debug_data0 != 0)
    ? snes_rumble_debug_data0
    : snes_rumble_debug_data1;
  const uint32_t snesRumbleTx = snes_rumble_debug_tx0 + snes_rumble_debug_tx1;
  #else
  const uint8_t snesRumbleData = 0;
  const uint32_t snesRumbleTx = 0;
  #endif
  std::snprintf(lines[3], sizeof(lines[3]), "R%02X M%02X%02X B%02X Q%03u",
                state.debug_last_rumble_right,
                rumbleDiag.scaled_left,
                rumbleDiag.scaled_right,
                snesRumbleData,
                (unsigned)(snesRumbleTx % 1000u));
#elif defined(ENABLE_XINPUT2P_WIRELESS_TRANSPORT)
  XInputWDiagInfo winfo = {};
  xinputw_get_diag_info(&winfo);
  std::snprintf(lines[0], sizeof(lines[0]), "W0 %02X%02X X%03u",
                winfo.endpoint_in[0],
                winfo.endpoint_out[0],
                (unsigned)(winfo.out_xfer_count[0] % 1000u));
  std::snprintf(lines[1], sizeof(lines[1]), "P0 L%02X R%02X S%02u",
                winfo.last_rumble_left[0],
                winfo.last_rumble_right[0],
                winfo.last_out_size[0]);
  std::snprintf(lines[2], sizeof(lines[2]), "W1 %02X%02X X%03u",
                winfo.endpoint_in[1],
                winfo.endpoint_out[1],
                (unsigned)(winfo.out_xfer_count[1] % 1000u));
  std::snprintf(lines[3], sizeof(lines[3]), "P1 L%02X R%02X S%02u",
                winfo.last_rumble_left[1],
                winfo.last_rumble_right[1],
                winfo.last_out_size[1]);
#endif

  static char lastLines[4][22] = {{0}};
  for (uint8_t i = 0; i < 4; ++i) {
    if (force || std::strcmp(lastLines[i], lines[i]) != 0) {
      const uint8_t row = 3 + i;
      clearRow(row);
      display.setRow(row);
      display.setCol(0);
      display.print(lines[i]);
      std::strncpy(lastLines[i], lines[i], sizeof(lastLines[i]) - 1);
      lastLines[i][sizeof(lastLines[i]) - 1] = '\0';
    }
  }
#else
  (void)force;
#endif
}

}  // namespace menu_main_display_internal
