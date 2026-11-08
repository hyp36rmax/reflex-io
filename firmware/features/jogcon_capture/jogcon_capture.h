#pragma once
#include "../../product_config.h"
#include "jogcon_capture_store.h"
#include "../../core/controller_state.h"
#if defined(ADAPT_FEATURE_JOGCON_CAPTURE)
void jogconCaptureBootInit();
void jogconCaptureSettingsReady();
void jogconCaptureBeforeReboot();
void jogconCapturePreserveSram();
bool jogconCaptureRecoveryActive();
bool jogconCaptureMotorDisabled();
bool jogconCapturePublished();
void jogconCaptureWire(const jogcon_capture::Event &event);
void jogconCaptureInput(jogcon_capture::Event event);
void jogconCaptureInit(uint8_t phase, bool ready, uint8_t protocol);
void jogconCaptureOutput(uint8_t port, const controller_state_t &frame);
int32_t jogconCaptureReadFile(uint32_t offset, void *buffer, uint32_t size);
void jogconCaptureService();
uint8_t jogconCapturePort();
#include "../feature_module.h"
extern const FeatureModule kJogconCaptureFeatureModule;
#else
inline bool jogconCaptureRecoveryActive() { return false; }
inline bool jogconCaptureMotorDisabled() { return false; }
#endif
