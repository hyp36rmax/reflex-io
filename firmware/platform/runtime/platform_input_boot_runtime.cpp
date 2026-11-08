#include "platform_input_boot_runtime.h"
#include "../../product_config.h"
#include "../../output/output_capabilities.h"
#include "../../output/output_runtime_state.h"
#include "platform_input_boot_product_services.h"

uint32_t platformAdjustInitialInputPollingDuration(uint32_t durationMs) {
  return productAdjustInitialInputPollingDuration(durationMs);
}

void platformSetupInputUsbBridgeForBoot(bool outputUsbReady) {
  productSetupInputUsbBridgeForBoot(outputUsbReady);
}

bool platformShouldResolveAutomaticInputBeforeUsb(bool autoOutputResolvedBoot) {
  const bool switchProfileBoot =
      output_runtime_is_switchpro_mode() &&
      (!is_auto_output_mode_selected() || autoOutputResolvedBoot);
  const bool resolvedMisterIdentityBoot =
      output_is_generic_mister_hid_mode(get_effective_output_mode()) &&
      (!is_auto_output_mode_selected() || autoOutputResolvedBoot);
  return switchProfileBoot || resolvedMisterIdentityBoot;
}

bool platformAcceptAutomaticInputBeforeUsb(DeviceEnum detectedMode) {
  if (detectedMode == RZORD_NONE) {
    return false;
  }
  return !output_runtime_is_switchpro_mode() ||
         input_mode_has_automatic_nso_profile(detectedMode);
}
