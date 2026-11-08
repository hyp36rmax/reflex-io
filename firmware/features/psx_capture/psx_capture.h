#pragma once
#include "../../product_config.h"
#include "psx_capture_store.h"

#if defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE)
void psxCaptureRecord(const psx_capture::Event &event);
void psxCaptureAutoBegin(uint8_t port, bool hotSwap, uint8_t inputMode);
void psxCaptureAutoEnd(uint8_t port, uint8_t result, uint16_t probes, uint8_t psxResult);
void psxCaptureDecoded(uint8_t port, bool connected, uint8_t protocol, bool pressure);
void psxCaptureReenum(uint8_t stage);
void psxCaptureSelection(uint8_t port, uint8_t mode, bool assisted);
bool psxCaptureDeepActive();
bool psxCapturePublished();
#if defined(ADAPT_FEATURE_PS2_IR_CAPTURE)
bool psxCaptureIrOwnsInput();
uint32_t psxCaptureNormalPollInterval(uint32_t normalUs);
#endif
int32_t psxCaptureReadFile(uint32_t offset, void *buffer, uint32_t size);
void psxCaptureService();
#include "../feature_module.h"
extern const FeatureModule kPsxCaptureFeatureModule;
#else
inline void psxCaptureRecord(const psx_capture::Event &) {}
inline void psxCaptureAutoBegin(uint8_t, bool, uint8_t) {}
inline void psxCaptureAutoEnd(uint8_t, uint8_t, uint16_t, uint8_t) {}
inline void psxCaptureDecoded(uint8_t, bool, uint8_t, bool) {}
inline void psxCaptureReenum(uint8_t) {}
inline void psxCaptureSelection(uint8_t, uint8_t, bool) {}
#endif
