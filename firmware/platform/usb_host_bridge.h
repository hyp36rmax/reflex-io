#pragma once

#include <stdint.h>

// USB host queue callbacks and core1 bridge entrypoints.
// This keeps the TinyUSB host glue in a normal compiled module instead of
// relying on firmware.cpp implementation includes.

void queue_set_output_report(uint8_t dev_addr, uint8_t idx, bool use_interrupt_edp, uint8_t report_id, const uint8_t* buffer, uint8_t len);
void queue_set_feature_report(uint8_t dev_addr, uint8_t idx, bool use_interrupt_edp, uint8_t report_id, const uint8_t* buffer, uint8_t len);
void queue_get_feature_report(uint8_t dev_addr, uint8_t idx, bool use_interrupt_edp, uint8_t report_id, uint8_t len);

#ifdef __cplusplus
extern "C" {
#endif
void host_get_report_cb(uint8_t dev_addr, uint8_t interface, uint8_t report_id, uint8_t report_type, uint8_t* report, uint16_t len);
void host_set_report_complete_cb(uint8_t dev_addr, uint8_t interface,
                                 uint8_t report_id, uint8_t report_type,
                                 uint16_t len);
#ifdef __cplusplus
}
#endif

#ifdef ENABLE_INPUT_USB
extern "C" {
void setup1();
void loop1();
}
#endif
