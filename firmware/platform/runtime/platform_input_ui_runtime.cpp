#include "platform_input_ui_runtime.h"

#include "../../product_config.h"
#include "../../menu/no_host_controller_test_runtime.h"

#ifdef ENABLE_INPUT_USB
#include "../../input/usb_host/input_usb/input_usb.h"
#endif

bool servicePlatformControllerUiBeforeMenuHotkeys() {
  return updateNoHostControllerTest();
}

bool platformPhysicalInputConnectionChanged(bool initialized) {
#ifdef ENABLE_INPUT_USB
  static uint32_t lastUsbMountCount = 0;
  static uint32_t lastUsbUmountCount = 0;
  const usb_input_debug_status_t usbStatus = inputusb_debug_status();
  const bool changed =
      initialized &&
      (usbStatus.device_mount_count != lastUsbMountCount ||
       usbStatus.device_umount_count != lastUsbUmountCount);
  lastUsbMountCount = usbStatus.device_mount_count;
  lastUsbUmountCount = usbStatus.device_umount_count;
  return changed;
#else
  (void)initialized;
  return false;
#endif
}
