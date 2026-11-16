#include "../../product_config.h"

#include "ps_auth_dongle_runtime.h"

#if defined(ENABLE_USB_AUTH_SIDECAR)

#include "ps_auth_dongle_sidecar.inc"
#else

void ps_auth_dongle_set_output_mode(outputMode_t mode) { (void)mode; }
void ps_auth_dongle_device_connected(uint8_t dev_addr,
                                     uint8_t instance,
                                     uint16_t vid,
                                     uint16_t pid,
                                     uint8_t root_port,
                                     const uint8_t* report_desc,
                                     uint16_t desc_len) {
  (void)dev_addr; (void)instance; (void)vid; (void)pid;
  (void)root_port;
  (void)report_desc; (void)desc_len;
}
void ps_auth_dongle_device_disconnected(uint8_t dev_addr, uint8_t instance) {
  (void)dev_addr; (void)instance;
}
bool ps_auth_dongle_is_auth_device(uint16_t vid, uint16_t pid) {
  (void)vid; (void)pid;
  return false;
}
bool ps_auth_dongle_is_ps5_provider_device(uint16_t vid, uint16_t pid) {
  (void)vid; (void)pid;
  return false;
}
bool ps_auth_dongle_is_auth_instance(uint8_t dev_addr, uint8_t instance) {
  (void)dev_addr; (void)instance;
  return false;
}
bool ps_auth_dongle_ps5_provider_observed() { return false; }
bool ps_auth_dongle_ps5_signer_connected() { return false; }
bool ps_auth_dongle_has_provider_for_mode(outputMode_t mode) {
  (void)mode;
  return false;
}
void ps_auth_dongle_task() {}
uint16_t ps_auth_dongle_handle_get_report(uint8_t report_id,
                                          uint8_t* buffer,
                                          uint16_t reqlen) {
  (void)report_id; (void)buffer; (void)reqlen;
  return 0;
}
bool ps_auth_dongle_handle_set_report(uint8_t report_id,
                                      const uint8_t* buffer,
                                      uint16_t reqlen) {
  (void)report_id; (void)buffer; (void)reqlen;
  return false;
}
void ps_auth_dongle_handle_get_report_response(uint8_t dev_addr,
                                               uint8_t instance,
                                               uint8_t report_id,
                                               const uint8_t* report,
                                               uint16_t len) {
  (void)dev_addr; (void)instance; (void)report_id; (void)report; (void)len;
}
void ps_auth_dongle_handle_set_report_complete(uint8_t dev_addr,
                                               uint8_t instance,
                                               uint8_t report_id,
                                               uint16_t len) {
  (void)dev_addr; (void)instance; (void)report_id; (void)len;
}
void ps_auth_dongle_handle_sent_report_response(uint8_t dev_addr,
                                                uint8_t instance,
                                                const uint8_t* report,
                                                uint16_t len) {
  (void)dev_addr; (void)instance; (void)report; (void)len;
}
void ps_auth_dongle_handle_input_report_received(uint8_t dev_addr,
                                                 uint8_t instance,
                                                 const uint8_t* report,
                                                 uint16_t len) {
  (void)dev_addr; (void)instance; (void)report; (void)len;
}
bool ps_auth_dongle_prepare_ps5_output_report(const uint8_t* report,
                                              uint16_t len,
                                              uint8_t* out,
                                              uint16_t* out_len,
                                              uint16_t out_capacity) {
  (void)report; (void)len; (void)out; (void)out_len; (void)out_capacity;
  return false;
}
void ps_auth_dongle_complete_ps5_output_report() {}
bool ps_auth_dongle_has_provider() { return false; }
PsAuthDongleStatus ps_auth_dongle_status() {
  PsAuthDongleStatus status = {};
  return status;
}
uint32_t ps_auth_dongle_console_request_count() { return 0; }

#endif
