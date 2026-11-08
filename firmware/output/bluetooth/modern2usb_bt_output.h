#pragma once

#include <stdint.h>

enum ProductOutputTransportPreference : uint8_t {
  MODERN2USB_OUTPUT_TRANSPORT_AUTO = 0,
  MODERN2USB_OUTPUT_TRANSPORT_USB,
  MODERN2USB_OUTPUT_TRANSPORT_BLUETOOTH,
  MODERN2USB_OUTPUT_TRANSPORT_COUNT,
};

enum Modern2UsbActiveOutputTransport : uint8_t {
  MODERN2USB_ACTIVE_OUTPUT_WAITING = 0,
  MODERN2USB_ACTIVE_OUTPUT_REQUESTING_BLUETOOTH,
  MODERN2USB_ACTIVE_OUTPUT_USB,
  MODERN2USB_ACTIVE_OUTPUT_BLUETOOTH,
  MODERN2USB_ACTIVE_OUTPUT_UNSUPPORTED,
};

struct Modern2UsbBtOutputTestFrameStatus {
  bool active;
  bool connected;
  uint32_t digital_buttons;
  int16_t lx;
  int16_t ly;
  int16_t rx;
  int16_t ry;
  uint8_t analog_l2;
  uint8_t analog_r2;
  uint32_t remaining_ms;
  uint32_t sequence;
};

enum Modern2UsbBtOutputTestStartResult : uint8_t {
  MODERN2USB_BT_OUTPUT_TEST_START_OK = 0,
  MODERN2USB_BT_OUTPUT_TEST_START_INVALID_RATE,
  MODERN2USB_BT_OUTPUT_TEST_START_INVALID_DURATION,
  MODERN2USB_BT_OUTPUT_TEST_START_ALREADY_ACTIVE,
  MODERN2USB_BT_OUTPUT_TEST_START_OUTPUT_INACTIVE,
  MODERN2USB_BT_OUTPUT_TEST_START_LINK_NOT_READY,
};

enum Modern2UsbBtOutputTestStopReason : uint8_t {
  MODERN2USB_BT_OUTPUT_TEST_STOP_RESET = 0,
  MODERN2USB_BT_OUTPUT_TEST_STOP_RUNNING,
  MODERN2USB_BT_OUTPUT_TEST_STOP_MANUAL,
  MODERN2USB_BT_OUTPUT_TEST_STOP_COMPLETE,
  MODERN2USB_BT_OUTPUT_TEST_STOP_LINK_LOST,
  MODERN2USB_BT_OUTPUT_TEST_STOP_TRANSPORT_CHANGED,
  MODERN2USB_BT_OUTPUT_TEST_STOP_FIRMWARE_UPDATE,
};

struct Modern2UsbBtOutputTestStatus {
  bool active;
  uint16_t requested_hz;
  uint16_t duration_seconds;
  uint32_t generated_reports;
  uint32_t elapsed_ms;
  Modern2UsbBtOutputTestStopReason stop_reason;
};

void modern2usbBtOutputInitialize(uint8_t preference, uint8_t persona);
void modern2usbBtOutputBeginStartupGrace();
void modern2usbBtOutputSetFirmwareUpdateHold(bool enabled);
bool modern2usbBtOutputFirmwareUpdateHold();
void modern2usbBtOutputTask();
void modern2usbBtOutputSendPreparedFrame();
Modern2UsbBtOutputTestStartResult modern2usbBtOutputTestStart(
    uint16_t requested_hz, uint16_t duration_seconds);
bool modern2usbBtOutputTestStop();
Modern2UsbBtOutputTestStatus modern2usbBtOutputTestStatus();
const char* modern2usbBtOutputTestStopReasonName(
    Modern2UsbBtOutputTestStopReason reason);
bool modern2usbBtOutputIsActive();
uint8_t modern2usbBtOutputSourceSlot();
bool modern2usbBtOutputAllowsUsbConnect();
ProductOutputTransportPreference modern2usbBtOutputPreference();
Modern2UsbActiveOutputTransport modern2usbBtOutputActiveTransport();
uint8_t modern2usbBtOutputPersona();
bool modern2usbBtOutputSetTestFrame(
    bool connected, uint32_t digital_buttons,
    int16_t lx, int16_t ly, int16_t rx, int16_t ry,
    uint8_t analog_l2, uint8_t analog_r2, uint32_t duration_ms);
void modern2usbBtOutputClearTestFrame();
Modern2UsbBtOutputTestFrameStatus modern2usbBtOutputTestFrameStatus();
const char* modern2usbBtOutputPersonaName(uint8_t persona);
const char* modern2usbBtOutputPreferenceName(uint8_t preference);
const char* modern2usbBtOutputActiveName();
