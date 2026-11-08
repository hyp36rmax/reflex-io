#include "platform_fast_path_runtime.h"

#include "../../product_config.h"
#include "../../core/device_runtime_state.h"

bool usePlatformFastPathRuntime() {
  #if defined(ADAPT_CAP_JVS_INPUT_STATUS) && defined(ENABLE_JVS_MINIMAL_DEBUG_LOOP) && defined(ENABLE_RUNTIME_SERIAL_DEBUG_CDC) && defined(ENABLE_INPUT_JVS)
  return deviceMode == RZORD_JVS;
  #else
  return false;
  #endif
}
