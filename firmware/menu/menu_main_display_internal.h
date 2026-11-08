#pragma once

#include <stdint.h>

struct PadButton;

namespace menu_main_display_internal {

bool didControllerNamesChange();
bool shouldShowAutoDetectScanning(uint8_t connectedCount);
bool shouldShowMultiControllerNameList();
void renderAutoDetectScanningStatus();
void renderConnectedPortNames();
void renderJvsRawDebugLine(bool force = false);
void renderXinputMultiDiagOverlay(bool force = false);
bool shouldShowXinputMultiDiagOverlay();
void renderModeButtonIndicator(bool force = false);
void updateRealtimeButtons();
void drawPadLayoutPreview(const PadButton* layout, uint8_t layoutCount,
                          uint32_t state, uint8_t colOffset,
                          uint8_t rowOffset);

}  // namespace menu_main_display_internal
