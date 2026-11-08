#include "../../product_config.h"

#include "modern2usb_bt_output.h"
#include "../../input/esp32_spi/esp32_spi_protocol.h"

#if ADAPT_POLICY_USB_BT_OUTPUT_SETTINGS && \
    defined(ENABLE_EXPERIMENTAL_MODERN2USB_BT_OUTPUT) && \
    defined(ENABLE_INPUT_ESP32_SPI)

#include <Adafruit_TinyUSB.h>
#include <Arduino.h>

#include "../../core/controller_frame_state.h"
#include "../../input/esp32_spi/Input_Esp32Spi.h"
namespace {

// USB remains authoritative during startup. Bluetooth output is selected only
// after the host has had enough time to enumerate management CDC and assert a
// firmware-update hold, including when an older image could not leave the
// watchdog marker before a UF2 migration.
constexpr uint32_t kUsbEnumerationGraceMs = 15000;
constexpr uint32_t kBluetoothAckTimeoutMs = 5000;

ProductOutputTransportPreference preference =
    MODERN2USB_OUTPUT_TRANSPORT_AUTO;
Modern2UsbActiveOutputTransport activeTransport =
    MODERN2USB_ACTIVE_OUTPUT_WAITING;
bool graceStarted = false;
uint32_t graceStartedAtMs = 0;
uint32_t bluetoothRequestedAtMs = 0;
uint8_t selectedSourceSlot = 0xFF;
uint8_t persona = ESP32_SPI_BT_OUTPUT_PERSONA_REFLEX;
bool firmwareUpdateHold = false;
Modern2UsbBtOutputTestFrameStatus testFrame{};
uint32_t testFrameExpiresAtMs = 0;

void clearTestFrame() {
  testFrame.active = false;
  testFrame.connected = false;
  testFrame.digital_buttons = 0;
  testFrame.lx = 0;
  testFrame.ly = 0;
  testFrame.rx = 0;
  testFrame.ry = 0;
  testFrame.analog_l2 = 0;
  testFrame.analog_r2 = 0;
  testFrame.remaining_ms = 0;
  testFrameExpiresAtMs = 0;
}

bool testFrameExpired(uint32_t nowMs) {
  return testFrame.active &&
         static_cast<int32_t>(nowMs - testFrameExpiresAtMs) >= 0;
}

constexpr uint16_t kBtOutputTestMinHz = 1;
constexpr uint16_t kBtOutputTestMaxHz = 125;
constexpr uint16_t kBtOutputTestMinSeconds = 1;
constexpr uint16_t kBtOutputTestMaxSeconds = 3600;

struct BtOutputTestRuntime {
  bool active;
  uint16_t requestedHz;
  uint16_t durationSeconds;
  uint32_t generatedReports;
  uint32_t sequence;
  uint32_t startedAtMs;
  uint32_t stoppedElapsedMs;
  uint32_t nextDueAtMs;
  uint16_t deadlineRemainder;
  Modern2UsbBtOutputTestStopReason stopReason;
};

BtOutputTestRuntime outputTest{};

void sendLiveSourceFrame() {
  const bool sourceNeedsRefresh = selectedSourceSlot >= MAX_USB_OUT ||
      !controllerFrameConst(selectedSourceSlot).connected;
  if (sourceNeedsRefresh) {
    selectedSourceSlot = 0xFF;
    // The underlying USB output mode can expose fewer players than MODERN2USB's
    // physical/composite input side. Bluetooth output still needs to accept a
    // wired source from either USB port, even when that mode sets max_devices
    // to one.
    for (uint8_t slot = 0; slot < MAX_USB_OUT; ++slot) {
      if (controllerFrameConst(slot).connected) {
        selectedSourceSlot = slot;
        break;
      }
    }
  }
  if (selectedSourceSlot == 0xFF) {
    esp32SpiSetBtOutputFrame(false, 0, 0, 0, 0, 0, 0, 0);
    return;
  }
  const controller_state_t& frame = controllerFrameConst(selectedSourceSlot);
  esp32SpiSetBtOutputFrame(
      frame.connected, frame.digital_buttons,
      frame.LX, frame.LY, frame.RX, frame.RY,
      frame.ANALOG_L2, frame.ANALOG_R2);
}

bool btOutputTestLinkReady(const Esp32SpiRuntimeStatus& status) {
  return status.bt_output_status_valid && status.bt_output_active &&
         status.bt_output_connected && status.bt_output_encrypted &&
         status.bt_output_notify_enabled;
}

bool stopBtOutputTest(Modern2UsbBtOutputTestStopReason reason) {
  if (!outputTest.active) {
    return false;
  }
  outputTest.active = false;
  outputTest.stoppedElapsedMs = millis() - outputTest.startedAtMs;
  outputTest.stopReason = reason;
  sendLiveSourceFrame();
  return true;
}

void resetBtOutputTest() {
  if (outputTest.active) {
    sendLiveSourceFrame();
  }
  outputTest = {};
  outputTest.stopReason = MODERN2USB_BT_OUTPUT_TEST_STOP_RESET;
}

void advanceBtOutputTestDeadline(uint32_t nowMs) {
  const uint16_t wholeMs =
      static_cast<uint16_t>(1000u / outputTest.requestedHz);
  const uint16_t remainder =
      static_cast<uint16_t>(1000u % outputTest.requestedHz);
  do {
    outputTest.nextDueAtMs += wholeMs;
    outputTest.deadlineRemainder = static_cast<uint16_t>(
        outputTest.deadlineRemainder + remainder);
    if (outputTest.deadlineRemainder >= outputTest.requestedHz) {
      outputTest.nextDueAtMs++;
      outputTest.deadlineRemainder = static_cast<uint16_t>(
          outputTest.deadlineRemainder - outputTest.requestedHz);
    }
    // Skip missed schedule points without emitting catch-up reports. A single
    // modern2usbBtOutputTask() call can therefore publish at most one frame.
  } while (static_cast<int32_t>(nowMs - outputTest.nextDueAtMs) >= 0);
}

void serviceBtOutputTest() {
  if (!outputTest.active) {
    return;
  }
  if (!btOutputTestLinkReady(esp32SpiRuntimeStatus())) {
    stopBtOutputTest(MODERN2USB_BT_OUTPUT_TEST_STOP_LINK_LOST);
    return;
  }

  const uint32_t nowMs = millis();
  const uint32_t durationMs =
      static_cast<uint32_t>(outputTest.durationSeconds) * 1000u;
  if (nowMs - outputTest.startedAtMs >= durationMs) {
    stopBtOutputTest(MODERN2USB_BT_OUTPUT_TEST_STOP_COMPLETE);
    return;
  }
  if (static_cast<int32_t>(nowMs - outputTest.nextDueAtMs) < 0) {
    return;
  }

  const uint32_t sequence = ++outputTest.sequence;
  const uint16_t sequenceLow = static_cast<uint16_t>(sequence);
  esp32SpiSetBtOutputFrame(
      true, 0,
      static_cast<int16_t>(sequenceLow),
      static_cast<int16_t>(static_cast<uint16_t>(~sequenceLow)),
      static_cast<int16_t>(static_cast<uint16_t>(nowMs)),
      static_cast<int16_t>(static_cast<uint16_t>(nowMs >> 16)),
      0xA5, 0x5A);
  outputTest.generatedReports++;
  advanceBtOutputTestDeadline(nowMs);
}

void latchUsb() {
  clearTestFrame();
  stopBtOutputTest(MODERN2USB_BT_OUTPUT_TEST_STOP_TRANSPORT_CHANGED);
  activeTransport = MODERN2USB_ACTIVE_OUTPUT_USB;
  bluetoothRequestedAtMs = 0;
  esp32SpiSetBtOutputEnabled(false, persona);
}

void requestBluetooth() {
  if (firmwareUpdateHold) {
    stopBtOutputTest(MODERN2USB_BT_OUTPUT_TEST_STOP_FIRMWARE_UPDATE);
    latchUsb();
    return;
  }
  activeTransport = MODERN2USB_ACTIVE_OUTPUT_REQUESTING_BLUETOOTH;
  bluetoothRequestedAtMs = millis();
  // Keep management USB attached until the ESP32 proves that it implements
  // the optional RBOS extension and has safely entered peripheral mode.
  esp32SpiSetBtOutputEnabled(true, persona);
}

void latchBluetoothAfterAck() {
  if (firmwareUpdateHold) {
    stopBtOutputTest(MODERN2USB_BT_OUTPUT_TEST_STOP_FIRMWARE_UPDATE);
    latchUsb();
    return;
  }
  tud_disconnect();
  activeTransport = MODERN2USB_ACTIVE_OUTPUT_BLUETOOTH;
}

void failBluetoothRequest() {
  clearTestFrame();
  stopBtOutputTest(MODERN2USB_BT_OUTPUT_TEST_STOP_TRANSPORT_CHANGED);
  esp32SpiSetBtOutputEnabled(false, persona);
  bluetoothRequestedAtMs = 0;
  activeTransport = MODERN2USB_ACTIVE_OUTPUT_UNSUPPORTED;
}

}  // namespace

void modern2usbBtOutputInitialize(
    uint8_t storedPreference, uint8_t storedPersona) {
  resetBtOutputTest();
  preference = storedPreference < MODERN2USB_OUTPUT_TRANSPORT_COUNT
      ? static_cast<ProductOutputTransportPreference>(storedPreference)
      : MODERN2USB_OUTPUT_TRANSPORT_AUTO;
  persona = storedPersona < ESP32_SPI_BT_OUTPUT_PERSONA_COUNT
      ? storedPersona
      : ESP32_SPI_BT_OUTPUT_PERSONA_REFLEX;
  graceStarted = false;
  graceStartedAtMs = 0;
  bluetoothRequestedAtMs = 0;
  selectedSourceSlot = 0xFF;
  clearTestFrame();
  if (firmwareUpdateHold) {
    activeTransport = MODERN2USB_ACTIVE_OUTPUT_USB;
    return;
  }
  switch (preference) {
    case MODERN2USB_OUTPUT_TRANSPORT_USB:
      activeTransport = MODERN2USB_ACTIVE_OUTPUT_USB;
      break;
    case MODERN2USB_OUTPUT_TRANSPORT_BLUETOOTH:
    case MODERN2USB_OUTPUT_TRANSPORT_AUTO:
    default:
      activeTransport = MODERN2USB_ACTIVE_OUTPUT_WAITING;
      break;
  }
}

void modern2usbBtOutputBeginStartupGrace() {
  graceStarted = true;
  graceStartedAtMs = millis();
}

void modern2usbBtOutputSetFirmwareUpdateHold(bool enabled) {
  if (enabled) {
    stopBtOutputTest(MODERN2USB_BT_OUTPUT_TEST_STOP_FIRMWARE_UPDATE);
  }
  firmwareUpdateHold = enabled;
  if (enabled) {
    activeTransport = MODERN2USB_ACTIVE_OUTPUT_USB;
    bluetoothRequestedAtMs = 0;
    selectedSourceSlot = 0xFF;
    clearTestFrame();
  }
}

bool modern2usbBtOutputFirmwareUpdateHold() {
  return firmwareUpdateHold;
}

void modern2usbBtOutputTask() {
  if (firmwareUpdateHold) {
    stopBtOutputTest(MODERN2USB_BT_OUTPUT_TEST_STOP_FIRMWARE_UPDATE);
    latchUsb();
    return;
  }
  if (activeTransport == MODERN2USB_ACTIVE_OUTPUT_BLUETOOTH) {
    esp32SpiSetBtOutputEnabled(true, persona);
    serviceBtOutputTest();
    return;
  }
  if (activeTransport == MODERN2USB_ACTIVE_OUTPUT_USB ||
      activeTransport == MODERN2USB_ACTIVE_OUTPUT_UNSUPPORTED) {
    esp32SpiSetBtOutputEnabled(false, persona);
    return;
  }
  if (!graceStarted) {
    return;
  }
  if (activeTransport == MODERN2USB_ACTIVE_OUTPUT_REQUESTING_BLUETOOTH) {
    if (preference == MODERN2USB_OUTPUT_TRANSPORT_AUTO && tud_mounted()) {
      latchUsb();
      return;
    }
    esp32SpiSetBtOutputEnabled(true, persona);
    const Esp32SpiRuntimeStatus status = esp32SpiRuntimeStatus();
    if (status.bt_output_active && status.bt_output_status_valid &&
        status.bt_output_ready) {
      latchBluetoothAfterAck();
      return;
    }
    if (millis() - bluetoothRequestedAtMs >= kBluetoothAckTimeoutMs) {
      failBluetoothRequest();
    }
    return;
  }
  if (preference == MODERN2USB_OUTPUT_TRANSPORT_AUTO && tud_mounted()) {
    latchUsb();
    return;
  }
  if (millis() - graceStartedAtMs < kUsbEnumerationGraceMs) {
    return;
  }
  requestBluetooth();
}

void modern2usbBtOutputSendPreparedFrame() {
  if (!modern2usbBtOutputIsActive() || outputTest.active) {
    return;
  }
  const uint32_t nowMs = millis();
  if (testFrameExpired(nowMs)) {
    clearTestFrame();
  }
  if (testFrame.active) {
    testFrame.remaining_ms = testFrameExpiresAtMs - nowMs;
    esp32SpiSetBtOutputFrame(
        testFrame.connected, testFrame.digital_buttons,
        testFrame.lx, testFrame.ly, testFrame.rx, testFrame.ry,
        testFrame.analog_l2, testFrame.analog_r2);
    return;
  }
  sendLiveSourceFrame();
  if (selectedSourceSlot >= MAX_USB_OUT) {
    selectedSourceSlot = 0xFF;
  }
}

Modern2UsbBtOutputTestStartResult modern2usbBtOutputTestStart(
    uint16_t requestedHz, uint16_t durationSeconds) {
  if (requestedHz < kBtOutputTestMinHz ||
      requestedHz > kBtOutputTestMaxHz) {
    return MODERN2USB_BT_OUTPUT_TEST_START_INVALID_RATE;
  }
  if (durationSeconds < kBtOutputTestMinSeconds ||
      durationSeconds > kBtOutputTestMaxSeconds) {
    return MODERN2USB_BT_OUTPUT_TEST_START_INVALID_DURATION;
  }
  if (outputTest.active) {
    return MODERN2USB_BT_OUTPUT_TEST_START_ALREADY_ACTIVE;
  }
  if (!modern2usbBtOutputIsActive()) {
    return MODERN2USB_BT_OUTPUT_TEST_START_OUTPUT_INACTIVE;
  }
  if (!btOutputTestLinkReady(esp32SpiRuntimeStatus())) {
    return MODERN2USB_BT_OUTPUT_TEST_START_LINK_NOT_READY;
  }
  // The numbered soak generator and fixed CDC loopback frame both own the
  // same single Bluetooth-output source. An explicit soak start supersedes a
  // prior bounded loopback frame; while it runs, new fixed frames are rejected
  // below rather than silently fighting for the transport.
  clearTestFrame();

  outputTest = {};
  outputTest.active = true;
  outputTest.requestedHz = requestedHz;
  outputTest.durationSeconds = durationSeconds;
  outputTest.startedAtMs = millis();
  outputTest.nextDueAtMs = outputTest.startedAtMs;
  outputTest.stopReason = MODERN2USB_BT_OUTPUT_TEST_STOP_RUNNING;
  return MODERN2USB_BT_OUTPUT_TEST_START_OK;
}

bool modern2usbBtOutputTestStop() {
  return stopBtOutputTest(MODERN2USB_BT_OUTPUT_TEST_STOP_MANUAL);
}

Modern2UsbBtOutputTestStatus modern2usbBtOutputTestStatus() {
  Modern2UsbBtOutputTestStatus status{};
  status.active = outputTest.active;
  status.requested_hz = outputTest.requestedHz;
  status.duration_seconds = outputTest.durationSeconds;
  status.generated_reports = outputTest.generatedReports;
  status.elapsed_ms = outputTest.active
      ? millis() - outputTest.startedAtMs
      : outputTest.stoppedElapsedMs;
  status.stop_reason = outputTest.stopReason;
  return status;
}

const char* modern2usbBtOutputTestStopReasonName(
    Modern2UsbBtOutputTestStopReason reason) {
  switch (reason) {
    case MODERN2USB_BT_OUTPUT_TEST_STOP_RUNNING: return "RUNNING";
    case MODERN2USB_BT_OUTPUT_TEST_STOP_MANUAL: return "MANUAL";
    case MODERN2USB_BT_OUTPUT_TEST_STOP_COMPLETE: return "COMPLETE";
    case MODERN2USB_BT_OUTPUT_TEST_STOP_LINK_LOST: return "LINK_LOST";
    case MODERN2USB_BT_OUTPUT_TEST_STOP_TRANSPORT_CHANGED:
      return "TRANSPORT_CHANGED";
    case MODERN2USB_BT_OUTPUT_TEST_STOP_FIRMWARE_UPDATE:
      return "FIRMWARE_UPDATE";
    case MODERN2USB_BT_OUTPUT_TEST_STOP_RESET:
    default: return "RESET";
  }
}
bool modern2usbBtOutputIsActive() {
  return activeTransport == MODERN2USB_ACTIVE_OUTPUT_BLUETOOTH;
}

uint8_t modern2usbBtOutputSourceSlot() {
  return selectedSourceSlot;
}

bool modern2usbBtOutputAllowsUsbConnect() {
  // Keep USB available until output capability and role are acknowledged.
  return true;
}

ProductOutputTransportPreference modern2usbBtOutputPreference() {
  return preference;
}

Modern2UsbActiveOutputTransport modern2usbBtOutputActiveTransport() {
  return activeTransport;
}

uint8_t modern2usbBtOutputPersona() {
  return persona;
}

bool modern2usbBtOutputSetTestFrame(
    bool connected, uint32_t digitalButtons,
    int16_t lx, int16_t ly, int16_t rx, int16_t ry,
    uint8_t analogL2, uint8_t analogR2, uint32_t durationMs) {
  if (!modern2usbBtOutputIsActive() || outputTest.active || durationMs == 0 ||
      durationMs > 60000) {
    return false;
  }
  testFrame.active = true;
  testFrame.connected = connected;
  testFrame.digital_buttons = digitalButtons;
  testFrame.lx = lx;
  testFrame.ly = ly;
  testFrame.rx = rx;
  testFrame.ry = ry;
  testFrame.analog_l2 = analogL2;
  testFrame.analog_r2 = analogR2;
  testFrame.remaining_ms = durationMs;
  ++testFrame.sequence;
  if (testFrame.sequence == 0) ++testFrame.sequence;
  testFrameExpiresAtMs = millis() + durationMs;
  return true;
}

void modern2usbBtOutputClearTestFrame() {
  clearTestFrame();
}

Modern2UsbBtOutputTestFrameStatus modern2usbBtOutputTestFrameStatus() {
  const uint32_t nowMs = millis();
  if (testFrameExpired(nowMs)) {
    clearTestFrame();
  }
  Modern2UsbBtOutputTestFrameStatus status = testFrame;
  if (status.active) {
    status.remaining_ms = testFrameExpiresAtMs - nowMs;
  }
  return status;
}

const char* modern2usbBtOutputPersonaName(uint8_t value) {
  switch (value) {
    case ESP32_SPI_BT_OUTPUT_PERSONA_GENERIC_HID: return "Generic HID";
    case ESP32_SPI_BT_OUTPUT_PERSONA_REFLEX:
    default: return "Reflex";
  }
}

const char* modern2usbBtOutputPreferenceName(uint8_t value) {
  switch (value) {
    case MODERN2USB_OUTPUT_TRANSPORT_USB: return "USB";
    case MODERN2USB_OUTPUT_TRANSPORT_BLUETOOTH: return "Bluetooth";
    case MODERN2USB_OUTPUT_TRANSPORT_AUTO:
    default: return "Auto";
  }
}

const char* modern2usbBtOutputActiveName() {
  switch (activeTransport) {
    case MODERN2USB_ACTIVE_OUTPUT_USB: return "USB";
    case MODERN2USB_ACTIVE_OUTPUT_BLUETOOTH: return "Bluetooth";
    case MODERN2USB_ACTIVE_OUTPUT_REQUESTING_BLUETOOTH: return "BT request";
    case MODERN2USB_ACTIVE_OUTPUT_UNSUPPORTED: return "Unsupported";
    case MODERN2USB_ACTIVE_OUTPUT_WAITING:
    default: return "USB grace";
  }
}

#else

void modern2usbBtOutputInitialize(uint8_t, uint8_t) {}
void modern2usbBtOutputBeginStartupGrace() {}
void modern2usbBtOutputSetFirmwareUpdateHold(bool) {}
bool modern2usbBtOutputFirmwareUpdateHold() { return false; }
void modern2usbBtOutputTask() {}
void modern2usbBtOutputSendPreparedFrame() {}
Modern2UsbBtOutputTestStartResult modern2usbBtOutputTestStart(
    uint16_t requestedHz, uint16_t durationSeconds) {
  if (requestedHz < 1 || requestedHz > 125) {
    return MODERN2USB_BT_OUTPUT_TEST_START_INVALID_RATE;
  }
  if (durationSeconds < 1 || durationSeconds > 3600) {
    return MODERN2USB_BT_OUTPUT_TEST_START_INVALID_DURATION;
  }
  return MODERN2USB_BT_OUTPUT_TEST_START_OUTPUT_INACTIVE;
}
bool modern2usbBtOutputTestStop() { return false; }
Modern2UsbBtOutputTestStatus modern2usbBtOutputTestStatus() {
  Modern2UsbBtOutputTestStatus status{};
  status.stop_reason = MODERN2USB_BT_OUTPUT_TEST_STOP_RESET;
  return status;
}
const char* modern2usbBtOutputTestStopReasonName(
    Modern2UsbBtOutputTestStopReason reason) {
  switch (reason) {
    case MODERN2USB_BT_OUTPUT_TEST_STOP_RUNNING: return "RUNNING";
    case MODERN2USB_BT_OUTPUT_TEST_STOP_MANUAL: return "MANUAL";
    case MODERN2USB_BT_OUTPUT_TEST_STOP_COMPLETE: return "COMPLETE";
    case MODERN2USB_BT_OUTPUT_TEST_STOP_LINK_LOST: return "LINK_LOST";
    case MODERN2USB_BT_OUTPUT_TEST_STOP_TRANSPORT_CHANGED:
      return "TRANSPORT_CHANGED";
    case MODERN2USB_BT_OUTPUT_TEST_STOP_FIRMWARE_UPDATE:
      return "FIRMWARE_UPDATE";
    case MODERN2USB_BT_OUTPUT_TEST_STOP_RESET:
    default: return "RESET";
  }
}
bool modern2usbBtOutputIsActive() { return false; }
uint8_t modern2usbBtOutputSourceSlot() { return 0xFF; }
bool modern2usbBtOutputAllowsUsbConnect() { return true; }
ProductOutputTransportPreference modern2usbBtOutputPreference() {
  return MODERN2USB_OUTPUT_TRANSPORT_USB;
}
Modern2UsbActiveOutputTransport modern2usbBtOutputActiveTransport() {
  return MODERN2USB_ACTIVE_OUTPUT_USB;
}
uint8_t modern2usbBtOutputPersona() {
  return ESP32_SPI_BT_OUTPUT_PERSONA_REFLEX;
}
bool modern2usbBtOutputSetTestFrame(
    bool, uint32_t, int16_t, int16_t, int16_t, int16_t,
    uint8_t, uint8_t, uint32_t) {
  return false;
}
void modern2usbBtOutputClearTestFrame() {}
Modern2UsbBtOutputTestFrameStatus modern2usbBtOutputTestFrameStatus() {
  return {};
}
const char* modern2usbBtOutputPersonaName(uint8_t) { return "Reflex"; }
const char* modern2usbBtOutputPreferenceName(uint8_t) { return "USB"; }
const char* modern2usbBtOutputActiveName() { return "USB"; }

#endif
