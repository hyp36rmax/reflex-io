#pragma once
#include "../../product_config.h"
#include "genesis_capture_bus.h"
class SaturnPort;
#if defined(ADAPT_FEATURE_GENESIS_IDENTIFICATION_CAPTURE)
void genesisCaptureBegin(uint8_t port, uint8_t stage, genesis_capture::Pins pins, uint8_t mode);
void genesisCaptureEnd(SaturnPort &port);
void genesisCaptureEvent(genesis_capture::Kind kind, uint8_t port, uint8_t mode, uint8_t stage = 0);
void genesisCaptureAutoSample(uint8_t thPin, uint8_t stage, uint8_t high, uint8_t low);
void genesisCaptureDecoded(uint8_t port, uint8_t slot, uint8_t observed, uint8_t latched, uint16_t digital);
void genesisCaptureService();
bool genesisCapturePublished();
int32_t genesisCaptureReadFile(uint32_t offset, void *buffer, uint32_t size);
#include "../feature_module.h"
extern const FeatureModule kGenesisCaptureFeatureModule;
#else
inline void genesisCaptureBegin(uint8_t, uint8_t, genesis_capture::Pins, uint8_t) {}
inline void genesisCaptureEnd(SaturnPort &) {}
inline void genesisCaptureEvent(genesis_capture::Kind, uint8_t, uint8_t, uint8_t = 0) {}
inline void genesisCaptureAutoSample(uint8_t, uint8_t, uint8_t, uint8_t) {}
inline void genesisCaptureDecoded(uint8_t, uint8_t, uint8_t, uint8_t, uint16_t) {}
#endif
