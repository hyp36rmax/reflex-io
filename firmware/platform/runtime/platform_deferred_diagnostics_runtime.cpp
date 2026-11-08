#include "platform_deferred_diagnostics_runtime.h"

#include "../../product_config.h"
#include "platform_feedback_runtime.h"

void servicePlatformDeferredDiagnostics() {
  #if ADAPT_POLICY_PS5_DIAGNOSTIC_RUNTIME
  runPs5DiagAuthStatusDisplay();
  #endif
}

