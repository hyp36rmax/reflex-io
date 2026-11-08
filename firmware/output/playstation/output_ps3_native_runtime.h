#pragma once

#if defined(ADAPT_FEATURE_NATIVE_PS3_OUTPUT)
#include <pico/unique_id.h>
#include "output_ps3_native_protocol.h"

static Ps3NativeUsbState ps3_native_usb;

inline bool ps3NativeRuntimeActive() {
  // AUTO's feature probe must retain its existing host-detection behavior.
  return get_effective_output_mode() == OUTPUT_PS3 && !can_run_usb_detection();
}

inline void resetPs3NativeUsbState() {
  pico_unique_board_id_t id{};
  static_assert(PICO_UNIQUE_BOARD_ID_SIZE_BYTES == 8, "PS3 native UID size");
  pico_get_unique_board_id(&id);
  ps3_native_usb.reset(id.id);
}
#endif
