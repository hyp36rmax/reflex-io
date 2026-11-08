#include "Input_UsbHost.h"
#include "input_usb_host_service.h"
#include <Adafruit_TinyUSB.h>
#include <tusb.h>

#ifdef ENABLE_USB_AUTH_SIDECAR
#include "../../output/auth/ps_auth_dongle_runtime.h"
#endif

namespace {

UsbHostCallbackStatus callbackStatus = {};

void captureCallbackDevice(uint8_t dev_addr, uint8_t instance = 0) {
  callbackStatus.last_dev_addr = dev_addr;
  callbackStatus.last_instance = instance;
  uint16_t vid = 0;
  uint16_t pid = 0;
  if (tuh_vid_pid_get(dev_addr, &vid, &pid)) {
    callbackStatus.last_vid = vid;
    callbackStatus.last_pid = pid;
  }
}

bool isBluetoothHciInterface(const tusb_desc_interface_t* itf) {
  return itf &&
         itf->bInterfaceClass == TUSB_CLASS_WIRELESS_CONTROLLER &&
         itf->bInterfaceSubClass == 0x01 &&
         itf->bInterfaceProtocol == 0x01;
}

void captureInterfaceDescriptor(uint8_t dev_addr, const tusb_desc_interface_t* itf) {
  if (!itf) {
    return;
  }

  callbackStatus.last_itf_number = itf->bInterfaceNumber;
  callbackStatus.last_itf_alt = itf->bAlternateSetting;
  callbackStatus.last_itf_class = itf->bInterfaceClass;
  callbackStatus.last_itf_subclass = itf->bInterfaceSubClass;
  callbackStatus.last_itf_protocol = itf->bInterfaceProtocol;

  if (callbackStatus.interface_snapshot_count < USB_HOST_INTERFACE_SNAPSHOT_COUNT) {
    UsbHostInterfaceSnapshot& snapshot =
      callbackStatus.interfaces[callbackStatus.interface_snapshot_count++];
    snapshot.dev_addr = dev_addr;
    snapshot.number = itf->bInterfaceNumber;
    snapshot.alt = itf->bAlternateSetting;
    snapshot.cls = itf->bInterfaceClass;
    snapshot.subclass = itf->bInterfaceSubClass;
    snapshot.protocol = itf->bInterfaceProtocol;
    snapshot.endpoint_count = itf->bNumEndpoints;
  }

  if (!isBluetoothHciInterface(itf)) {
    return;
  }

  callbackStatus.bluetooth_hci_detect_count++;
  callbackStatus.bluetooth_hci_addr = dev_addr;
  uint16_t vid = 0;
  uint16_t pid = 0;
  if (tuh_vid_pid_get(dev_addr, &vid, &pid)) {
    callbackStatus.bluetooth_hci_vid = vid;
    callbackStatus.bluetooth_hci_pid = pid;
  }
}

}  // namespace

UsbHostCallbackStatus usb_host_callback_status() {
  return callbackStatus;
}

bool tuh_enum_descriptor_configuration_cb(uint8_t daddr, uint8_t cfg_index, const tusb_desc_configuration_t* desc_config) {
  callbackStatus.config_desc_count++;
  callbackStatus.last_config_index = cfg_index;
  callbackStatus.interface_snapshot_count = 0;
  captureCallbackDevice(daddr);

  if (!desc_config) {
    return true;
  }

  const uint8_t* cfg = reinterpret_cast<const uint8_t*>(desc_config);
  const uint16_t total_len = static_cast<uint16_t>(desc_config->wTotalLength);
  const uint8_t* ptr = cfg + desc_config->bLength;
  const uint8_t* end = cfg + total_len;

  while (ptr + 2 <= end && ptr[0] >= 2 && ptr + ptr[0] <= end) {
    if (ptr[1] == TUSB_DESC_INTERFACE && ptr[0] >= sizeof(tusb_desc_interface_t)) {
      captureInterfaceDescriptor(daddr, reinterpret_cast<const tusb_desc_interface_t*>(ptr));
    }
    ptr += ptr[0];
  }

  return true;
}

void tuh_mount_cb(uint8_t daddr) {
  callbackStatus.device_mount_count++;
  captureCallbackDevice(daddr);
#ifdef ENABLE_INPUT_USB
  inputusb_device_mount_cb(daddr);
#else
  (void)daddr;
#endif
}

void tuh_umount_cb(uint8_t daddr) {
  callbackStatus.device_umount_count++;
  callbackStatus.last_dev_addr = daddr;
#ifdef ENABLE_INPUT_USB
  inputusb_device_umount_cb(daddr);
#else
  (void)daddr;
#endif
}
static uint8_t callbackRootPort(uint8_t dev_addr) {
  uint8_t rootPort = 0;
  uint8_t currentAddr = dev_addr;
  tuh_bus_info_t busInfo = {};
  while (currentAddr != 0 && tuh_bus_info_get(currentAddr, &busInfo) &&
         busInfo.hub_addr != 0) {
    rootPort = busInfo.hub_port;
    currentAddr = busInfo.hub_addr;
  }
  return rootPort;
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t idx, uint8_t const* report_desc, uint16_t desc_len) {
  callbackStatus.hid_mount_count++;
  callbackStatus.last_hid_desc_len = desc_len;
  captureCallbackDevice(dev_addr, idx);
  uint16_t vid;
  uint16_t pid;
  tuh_vid_pid_get(dev_addr, &vid, &pid);

  //printf("tuh_hid_mount_cb %04x:%04x %d %d\n", vid, pid, dev_addr, idx);

  #ifdef ENABLE_USB_AUTH_SIDECAR
    ps_auth_dongle_device_connected(dev_addr, idx, vid, pid,
                                    callbackRootPort(dev_addr), report_desc, desc_len);
  #endif

  #ifdef ENABLE_INPUT_USB
    #ifdef ENABLE_USB_AUTH_SIDECAR
      // device_connected() has already evaluated descriptor-only candidates
      // and output-mode compatibility. Filter only the instance actually
      // accepted by the sidecar so connection order cannot poison auth.
      bool isDongle = ps_auth_dongle_is_auth_instance(dev_addr, idx);
    #else
      bool isDongle = false;
    #endif
    if (!isDongle) {
      inputusb_hid_mount_cb(dev_addr, idx, report_desc, desc_len);
    }
  #endif
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t idx) {
  callbackStatus.hid_umount_count++;
  captureCallbackDevice(dev_addr, idx);
  #ifdef ENABLE_USB_AUTH_SIDECAR
    ps_auth_dongle_device_disconnected(dev_addr, idx);
  #endif
  #ifdef ENABLE_INPUT_USB
    inputusb_hid_umount_cb(dev_addr, idx);
  #endif
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t idx, uint8_t const* report, uint16_t len) {
  callbackStatus.hid_report_count++;
  callbackStatus.last_report_len = (len > 255) ? 255 : (uint8_t)len;
  captureCallbackDevice(dev_addr, idx);
#ifdef ENABLE_USB_AUTH_SIDECAR
  ps_auth_dongle_handle_input_report_received(dev_addr, idx, report, len);
#endif
#ifdef ENABLE_INPUT_USB
  #ifdef ENABLE_USB_AUTH_SIDECAR
  if (ps_auth_dongle_is_auth_instance(dev_addr, idx)) {
    return;
  }
  #endif
  inputusb_hid_report_received_cb(dev_addr, idx, report, len);
#else
  (void)dev_addr; (void)idx; (void)report; (void)len;
#endif
}

void tuh_xinput_mount_cb(uint8_t dev_addr, uint8_t instance, const xinputh_interface_t* xinput_itf) {
  callbackStatus.xinput_mount_count++;
  captureCallbackDevice(dev_addr, instance);
#ifdef ENABLE_INPUT_USB
  inputusb_xinput_mount_cb(dev_addr, instance, xinput_itf);
#else
  (void)dev_addr; (void)instance; (void)xinput_itf;
#endif
}

void tuh_xinput_umount_cb(uint8_t dev_addr, uint8_t instance) {
#ifdef ENABLE_INPUT_USB
  inputusb_xinput_umount_cb(dev_addr, instance);
#else
  (void)dev_addr; (void)instance;
#endif
}

void tuh_xinput_report_received_cb(uint8_t dev_addr, uint8_t instance, xinputh_interface_t const* xid_itf, uint16_t len) {
  callbackStatus.xinput_report_count++;
  callbackStatus.last_report_len = (len > 255) ? 255 : (uint8_t)len;
  captureCallbackDevice(dev_addr, instance);
#ifdef ENABLE_INPUT_USB
  inputusb_xinput_report_received_cb(dev_addr, instance, xid_itf, len);
#else
  (void)dev_addr; (void)instance; (void)xid_itf; (void)len;
#endif
}

void tuh_densha_mount_cb(uint8_t dev_addr, uint8_t instance, const denshah_interface_t* densha_itf) {
#ifdef ENABLE_INPUT_USB
  inputusb_densha_mount_cb(dev_addr, instance, densha_itf);
#else
  (void)dev_addr; (void)instance; (void)densha_itf;
#endif
}

void tuh_densha_umount_cb(uint8_t dev_addr, uint8_t instance) {
#ifdef ENABLE_INPUT_USB
  inputusb_densha_umount_cb(dev_addr, instance);
#else
  (void)dev_addr; (void)instance;
#endif
}

void tuh_densha_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
#ifdef ENABLE_INPUT_USB
  inputusb_densha_report_received_cb(dev_addr, instance, report, len);
#else
  (void)dev_addr; (void)instance; (void)report; (void)len;
#endif
}

void tuh_horifs2_mount_cb(uint8_t dev_addr, uint8_t instance, const horifs2h_interface_t* horifs2_itf) {
#ifdef ENABLE_INPUT_USB
  inputusb_horifs2_mount_cb(dev_addr, instance, horifs2_itf);
#else
  (void)dev_addr; (void)instance; (void)horifs2_itf;
#endif
}

void tuh_horifs2_umount_cb(uint8_t dev_addr, uint8_t instance) {
#ifdef ENABLE_INPUT_USB
  inputusb_horifs2_umount_cb(dev_addr, instance);
#else
  (void)dev_addr; (void)instance;
#endif
}

void tuh_horifs2_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
#ifdef ENABLE_INPUT_USB
  inputusb_horifs2_report_received_cb(dev_addr, instance, report, len);
#else
  (void)dev_addr; (void)instance; (void)report; (void)len;
#endif
}
