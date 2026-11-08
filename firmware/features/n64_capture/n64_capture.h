#pragma once
#include "../../product_config.h"
#if defined(ADAPT_FEATURE_N64_CAPTURE)
#include "n64_capture_store.h"
#include "../../core/controller_state.h"
#include "../feature_module.h"
void n64CaptureBootInit();
void n64CaptureBeforeReboot();
void n64CapturePreserveSram();
bool n64CaptureRecoveryActive();
bool n64CapturePublished();
void n64CaptureDecoded(uint8_t port, const controller_state_t &frame, bool connected);
void n64CaptureBusy(uint8_t port, uint8_t slot, uint32_t buttons);
void n64CaptureSubmitPrepare(uint8_t port, uint8_t slot, uint8_t reportId, const uint8_t *data, uint16_t size);
void n64CaptureSubmitResult(uint8_t slot, bool accepted);
void n64CaptureComplete(uint8_t slot, const uint8_t *data, uint16_t size, bool success);
void n64CaptureService();
int32_t n64CaptureReadFile(uint32_t offset, void *buffer, uint32_t size);
extern const FeatureModule kN64CaptureFeatureModule;
#endif
