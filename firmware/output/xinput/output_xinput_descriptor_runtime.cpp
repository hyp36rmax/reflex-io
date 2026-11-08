#include "../../product_config.h"
#ifdef ADAPT_OUTPUT_USB_DEVICE

#include <Arduino.h>
#include <device/usbd_pvt.h>
#include <tusb_option.h>

#include "../output_runtime_state.h"
#include "out_xinput.h"
#include "output_xinput_auth_runtime_state.h"
#include "output_xinput_autodetect_descriptor_data_runtime.h"
#include "output_xinput_descriptor_data_runtime.h"
#include "output_xinput_descriptor_state.h"

namespace {

constexpr uint16_t kMsOs20SetHeaderLen = 0x0A;
constexpr uint16_t kMsOs20ConfigSubsetHeaderLen = 0x08;
constexpr uint16_t kMsOs20FunctionSubsetOffset =
    kMsOs20SetHeaderLen + kMsOs20ConfigSubsetHeaderLen;
constexpr uint16_t kMsOs20FunctionFirstInterfaceOffset = 4;
constexpr uint16_t kMsOs20TotalLengthOffset = 8;

}  // namespace

void xinput_register_descriptor_hooks(void) {
    // The donor TinyUSB library is used unchanged and does not expose the
    // descriptor hook globals from our experimental third_party copy.
    debug_hooks_registered = false;
}

void autodetect_register_descriptor_hooks(uint16_t hid_report_desc_len) {
    xinput_init_autodetect_config(hid_report_desc_len);
    debug_hooks_registered = false;
}

void xinput_fill_descriptor_debug_info(XInputDebugInfo* info) {
    if (!info) return;
    info->raw_descriptors = xinput_use_raw_descriptors;
    info->hooks_registered = debug_hooks_registered;
    info->desc_device_calls = debug_desc_device_calls;
    info->desc_config_calls = debug_desc_config_calls;
}

bool xinput_handle_ms_os_descriptor_request(
    uint8_t rhport,
    const tusb_control_request_t *request
) {
    const uint8_t* descriptor = desc_ms_os_20;

    uint16_t total_len;
    memcpy(&total_len, descriptor + kMsOs20TotalLengthOffset, 2);
    return tud_control_xfer(rhport, request, (void *)descriptor, total_len);
}

extern "C" bool xinput_handle_ms_os_descriptor_request_c(
    uint8_t rhport,
    const tusb_control_request_t *request
) {
    return xinput_handle_ms_os_descriptor_request(rhport, request);
}

void xinput_set_ms_os_first_interface(uint8_t itfnum) {
    desc_ms_os_20[kMsOs20FunctionSubsetOffset +
                  kMsOs20FunctionFirstInterfaceOffset] = itfnum;
}

extern "C" {

const uint8_t *tud_descriptor_bos_cb(void) {
    return desc_bos;
}

}  // extern "C"

#endif  // ADAPT_OUTPUT_USB_DEVICE
