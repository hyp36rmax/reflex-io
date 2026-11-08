#include "platform_input_autodetect_runtime.h"

#include "../../menu/no_host_controller_test_runtime.h"

bool platformAllowsInitialAutoDetectBeforeHomeVisible() {
  return noHostControllerTestActive();
}
