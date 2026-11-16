#include "../product_config.h"

#include "feature_module.h"
#if defined(ADAPT_FEATURE_JAGUAR_TRACE)
#include "jaguar_trace/jaguar_trace.h"
#endif
#if defined(ADAPT_FEATURE_N64_CAPTURE)
#include "n64_capture/n64_capture.h"
#endif
#if defined(ADAPT_FEATURE_HOST_CAPTURE)
#include "host_capture/host_capture.h"
#endif
#if defined(ADAPT_FEATURE_JOGCON_CAPTURE)
#include "jogcon_capture/jogcon_capture.h"
#endif
#if defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE)
#include "psx_capture/psx_capture.h"
#endif
#if defined(ADAPT_FEATURE_GENESIS_IDENTIFICATION_CAPTURE)
#include "genesis_capture/genesis_capture.h"
#endif

#if ADAPT_FEATURE_DINPUT_MAPPING_WIZARD
#include "dinput_mapping/dinput_mapping_wizard.h"
#endif

#ifdef ENABLE_BLUETOOTH_PAIRING
#include "bluetooth/bluetooth_feature.h"
#endif
#ifdef ENABLE_TTY2OLED_SERIAL
#include "tty2oled/tty2oled_feature.h"
#endif
#ifdef ADAPT_FEATURE_MISTER_SERIAL_STATUS
#include "mister_status/mister_status_feature.h"
#endif
#ifdef ADAPT_CAP_USB_INPUT_ANALOG_PROFILES
#include "modern2usb_analog/modern2usb_analog_feature.h"
#endif
#ifdef ADAPT_CAP_ARCADE_MAPPING_SESSION
#include "usb2db15/usb2db15_arcade_mapping.h"
#endif
#if defined(ADAPT_FEATURE_OUTPUT_TRIAL)
#include "output_trial/output_trial.h"
#endif

namespace {

const FeatureModule* const kFeatureModules[] = {
#if defined(ADAPT_FEATURE_OUTPUT_TRIAL)
  &kOutputTrialFeatureModule,
#endif
#if defined(ADAPT_FEATURE_JAGUAR_TRACE)
  &kJaguarTraceFeatureModule,
#endif
#if defined(ADAPT_FEATURE_N64_CAPTURE)
  &kN64CaptureFeatureModule,
#endif
#if defined(ADAPT_FEATURE_HOST_CAPTURE)
  &kHostCaptureFeatureModule,
#endif
#if defined(ADAPT_FEATURE_JOGCON_CAPTURE)
  &kJogconCaptureFeatureModule,
#endif
#if defined(ADAPT_FEATURE_GENESIS_IDENTIFICATION_CAPTURE)
  &kGenesisCaptureFeatureModule,
#endif
#if defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE)
  &kPsxCaptureFeatureModule,
#endif
#ifdef ENABLE_BLUETOOTH_PAIRING
  &kBluetoothFeatureModule,
#endif
#ifdef ADAPT_FEATURE_MISTER_SERIAL_STATUS
  &kMisterStatusFeatureModule,
#endif
#ifdef ENABLE_TTY2OLED_SERIAL
  &kTty2OledFeatureModule,
#endif
#ifdef ADAPT_CAP_USB_INPUT_ANALOG_PROFILES
  &kModern2UsbAnalogFeatureModule,
#endif
  nullptr,
};

template <typename Callback>
bool visitModules(Callback callback) {
  for (const FeatureModule* module : kFeatureModules) {
    if (module && callback(*module)) {
      return true;
    }
  }
  return false;
}

}  // namespace

bool featureModulesHandleMenuItem(menu_item_enum item, menu_item_action action) {
  return visitModules([&](const FeatureModule& module) {
    return module.handleMenuItem && module.handleMenuItem(item, action);
  });
}

bool featureModulesShouldHideMenuItem(menu_item_enum item, bool* hidden) {
  return visitModules([&](const FeatureModule& module) {
    return module.shouldHideMenuItem && module.shouldHideMenuItem(item, hidden);
  });
}

bool featureModulesHandleSerialCommand(const char* command, Print& out) {
  return visitModules([&](const FeatureModule& module) {
    return module.handleSerialCommand && module.handleSerialCommand(command, out);
  });
}

void featureModulesAppendSerialHelp(Print& out) {
  for (const FeatureModule* module : kFeatureModules) {
    if (module && module->appendSerialHelp) {
      module->appendSerialHelp(out);
    }
  }
}

void featureModulesAppendSerialState(Print& out) {
  for (const FeatureModule* module : kFeatureModules) {
    if (module && module->appendSerialState) {
      module->appendSerialState(out);
    }
  }
}

bool featureModulesRenderOledTransient() {
  return visitModules([](const FeatureModule& module) {
    return module.renderOledTransient && module.renderOledTransient();
  });
}
bool featureModulesOwnControllerInput() {
#ifdef ADAPT_CAP_ARCADE_MAPPING_SESSION
  if (usb2db15ArcadeUiActive()) {
    return true;
  }
#endif
#if ADAPT_FEATURE_DINPUT_MAPPING_WIZARD
  return hidMappingWizardActive;
#else
  return false;
#endif
}
