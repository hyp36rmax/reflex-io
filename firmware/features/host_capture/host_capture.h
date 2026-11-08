#pragma once
#include "../../product_config.h"
#if defined(ADAPT_FEATURE_HOST_CAPTURE)
#include <stdint.h>
#include "host_capture_store.h"
#include "../feature_module.h"
void hostCaptureBootInit();
void hostCaptureBeforeReboot();
void hostCapturePreserveSram();
bool hostCaptureRecoveryActive();
bool hostCapturePublished();
void hostCaptureHidReport(uint8_t itf, uint8_t reportId, uint8_t reportType,
                          const uint8_t *data, uint16_t size);
void hostCaptureDetect(uint8_t result, uint8_t hostFlags, bool manufacturerRead,
                       bool reportDescriptorRead, uint32_t elapsedMs);
void hostCaptureService();
int32_t hostCaptureReadFile(uint32_t offset, void *buffer, uint32_t size);
extern const FeatureModule kHostCaptureFeatureModule;
#endif
