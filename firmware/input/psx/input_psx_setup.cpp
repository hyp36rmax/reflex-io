#include "Input_Psx.h"
#include <utility>
#include "../../core/classic_mouse_motion.h"

#include "../../product_config.h"
#include "../../core/controller_settings_state.h"
#include "../../core/psx_poll_timing.h"
#include "../../core/turbo.h"
#include "../../menu/menu_runtime_state.h"
#include <PsxNewLib/PsxDriverHwSpi.h>
#include <PsxNewLib/PsxDriverHwSpiWithAck.h>
#include "psx_capture_driver.h"
#include "jogcon_capture_driver.h"
#ifdef ENABLE_INPUT_AUTODETECT
#include "../autodetect/input_autodetect_runtime_state.h"
#endif

namespace {
constexpr uint8_t kPsxSetupDetectAttempts = 3;
constexpr uint8_t kPsxSetupRetryDelayMs = 50;
constexpr uint8_t kPsxMultitapSetupDetectAttempts = 5;
constexpr uint8_t kPsxMultitapSetupRetryDelayMs = 16;
constexpr uint8_t kPsxPostControllerBeginDelayMs = 5;
constexpr uint16_t kPsxDefaultAttentionIntervalUs = 250;
constexpr uint16_t kPsxJetAttentionIntervalUs = 1000;
constexpr uint16_t kPsxDualShock2AttentionIntervalUs = 3000;
constexpr uint16_t kPsxAutoResolvedPhysicalPresenceGraceMs = 5000;
constexpr uint16_t kPsxRumbleConfigRetryMs = 2000;

bool psxProtocolSupportsDualShockRumble(PsxControllerProtocol proto) {
  return proto == PSPROTO_DUALSHOCK || proto == PSPROTO_DUALSHOCK2;
}

uint8_t autoResolvedPsxPortHint(uint8_t input_ports) {
  #ifdef ENABLE_INPUT_AUTODETECT
  if (savedDeviceMode == RZORD_AUTODETECT &&
      deviceMode == RZORD_PSX &&
      input_auto_resolve_port < input_ports) {
    return input_auto_resolve_port;
  }
  #else
  (void)input_ports;
  #endif
  return 0xFF;
}

uint8_t psxDebugTransferByte(const input_psx_config_t& c, uint8_t txByte) {
  uint8_t rxByte = 0;

  for (uint8_t i = 0; i < 8; ++i) {
    gpio_put(c.cmd, (txByte >> i) & 0x01);
    delayMicroseconds(2);

    gpio_put(c.clk, LOW);
    delayMicroseconds(2);

    // Match Auto's sample before the rising edge can release the last data bit.
    if (gpio_get(c.dat)) {
      rxByte |= (1u << i);
    }

    gpio_put(c.clk, HIGH);
    delayMicroseconds(2);
  }

  return rxByte;
}

void psxDebugRawPoll(const input_psx_config_t& c, byte* in, size_t len) {
  static const byte poll[] = {0x01, 0x42, 0x00, 0xFF, 0xFF};
  if (in == nullptr || len < sizeof(poll)) {
    return;
  }

  gpio_init(c.att);
  gpio_init(c.cmd);
  gpio_init(c.dat);
  gpio_init(c.clk);
  gpio_init(c.ack);

  gpio_set_dir(c.att, GPIO_OUT);
  gpio_set_dir(c.cmd, GPIO_OUT);
  gpio_set_dir(c.dat, GPIO_IN);
  gpio_set_dir(c.clk, GPIO_OUT);
  gpio_set_dir(c.ack, GPIO_IN);
  gpio_pull_up(c.dat);
  gpio_pull_up(c.ack);

  gpio_put(c.att, HIGH);
  gpio_put(c.clk, HIGH);
  gpio_put(c.cmd, HIGH);
  delayMicroseconds(50);

  gpio_put(c.att, LOW);
  delayMicroseconds(20);

  for (size_t i = 0; i < sizeof(poll); ++i) {
    in[i] = psxDebugTransferByte(c, poll[i]);
    delayMicroseconds(20);
  }

  gpio_put(c.att, HIGH);
  gpio_set_dir(c.att, GPIO_IN);
  gpio_set_dir(c.cmd, GPIO_IN);
  gpio_set_dir(c.clk, GPIO_IN);
}
}  // namespace

RZInputPSX::RZInputPSX() : RZInputModule() { }

const char* RZInputPSX::getUsbId() {
  return output_effective_psx_usb_id(isIRRemote);
}

void RZInputPSX::configureBcdDeviceVersion() {
  bcd_device_version.platform = BCD_PLAT_PSX;
  bcd_device_version.platform_sub = 0;
}

const char* RZInputPSX::getDescription() {
  return "PSX";
}

void RZInputPSX::configureControllerFrame(uint8_t i) {
  if (i >= MAX_USB_OUT) {
    return;
  }

  controller_state_t& frame = inputFrame(i);
  frame.HAS_BTN_SELECT = 1;
  frame.HAS_BTN_START = 1;
  frame.sticks_precision_bits = ANALOG_STICK_PRECISION_8;
}

void RZInputPSX::stopRumble(uint8_t i) {
  if (i >= input_ports || psx[i] == nullptr) {
    return;
  }

  // setRumble alone only clears DualShock's two motor bytes. JogCon has a
  // separate cached command, which read() would otherwise retransmit.
  psx[i]->setJogconMotorMode(JOGCON_DIR_NONE, JOGCON_CMD_NONE, 0);
  psx[i]->setRumble(false, 0);
  psx[i]->read();
}

void RZInputPSX::tryEnableRumble(uint8_t i) {
  if (i >= input_ports || psx[i] == nullptr) {
    return;
  }

  const PsxControllerProtocol proto = psx[i]->getProtocol();
  if (!psxProtocolSupportsDualShockRumble(proto)) {
    return;
  }

  // DualShock and DualShock 2 use the same rumble configuration. Preserve the
  // latch across transient protocol reads and protocol-family transitions;
  // a confirmed disconnect is responsible for clearing it.
  if (psxProtocolSupportsDualShockRumble(rumbleConfiguredProto[i])) {
    return;
  }

  // The caller retries on every poll while the host asks for rumble, and a
  // refusal must not hold both ports each time.
  if (nextRumbleConfigMs[i] != 0 &&
      static_cast<int32_t>(millis() - nextRumbleConfigMs[i]) < 0) {
    return;
  }

  //if ((options.inputMode == INPUT_MODE_XINPUT || detectDS2) && !isJogcon){ //try to enable rumble
  if (!isJogcon) { //try to enable rumble
    nextRumbleConfigMs[i] = millis() + kPsxRumbleConfigRetryMs;
    psx[i]->setRumble(false, 0);
    if (psx[i]->tryEnterConfigMode()) {
//      if (isWaiwai) // not required but handy if needing to check the return data (on rx/ry)
//        psx[i]->enableAnalogSticks();
      const bool rumbleEnabled = psx[i]->enableRumble();
      setDualShock2PressureState(i, false);
      psx[i]->exitConfigMode();
      stopRumble(i);
      if (rumbleEnabled) {
        rumbleConfiguredProto[i] = proto;
      }
    }
  }
}

bool RZInputPSX::tryEnableAnalogMode(uint8_t i) {
  if (i >= input_ports || psx[i] == nullptr)
    return false;

  if (psx[i]->getProtocol() != PSPROTO_DIGITAL)
    return false;

  if (!psx[i]->enterConfigMode())
    return false;

  const bool enabled = psx[i]->enableAnalogSticks();
  psx[i]->exitConfigMode();

  if (enabled) {
    delay(1);
    psx[i]->read();
  }

  return enabled;
}

// A pad with no Configuration Mode never answers, so a hot-plug spreads
// enterConfigMode()'s tries and time across polls and the other port keeps its
// reports.
void RZInputPSX::armAnalogProbe(uint8_t i) {
  const uint32_t now = millis();
  analogProbeUntilMs[i] = (now + COMMAND_TIMEOUT) | 1u;
  nextAnalogProbeMs[i] = now;
}

void RZInputPSX::serviceAnalogProbe(uint8_t i) {
  if (analogProbeUntilMs[i] == 0) return;
  const uint32_t now = millis();
  if (psx[i] == nullptr || psx[i]->getProtocol() != PSPROTO_DIGITAL ||
      static_cast<int32_t>(now - analogProbeUntilMs[i]) >= 0) {
    analogProbeUntilMs[i] = 0;
    return;
  }
  if (static_cast<int32_t>(now - nextAnalogProbeMs[i]) < 0) return;
  nextAnalogProbeMs[i] = now + COMMAND_RETRY_INTERVAL;
  if (!psx[i]->tryEnterConfigMode()) return;

  analogProbeUntilMs[i] = 0;
  const bool enabled = psx[i]->enableAnalogSticks();
  psx[i]->exitConfigMode();
  if (enabled) {
    delay(1);
    psx[i]->read();
    tryEnableDualShock2Pressure(i);
  }
}

bool RZInputPSX::tryEnableDualShock2Pressure(uint8_t i) {
  if (i >= input_ports || psx[i] == nullptr ||
      psx[i]->getProtocol() == PSPROTO_IRREMOTE || isNeGcon || isJogcon || isGuncon || isWaiwai) {
    return false;
  }

  if (psx[i]->getProtocol() == PSPROTO_DUALSHOCK2 &&
      psx[i]->getAnalogButtonData() != nullptr) {
    setDualShock2PressureState(i, true);
    return true;
  }
  if (psx[i]->getProtocol() != PSPROTO_DUALSHOCK) {
    return false;
  }

  setDualShock2PressureState(i, false);
  if (!psx[i]->enterConfigMode()) {
    return false;
  }
  // Analog-stick mode (0x73) alone has no pressure bytes. Only a DS2 model
  // receives the pressure request; confirm support from its next 0x79 poll.
  if (psx[i]->getControllerType() == PSCTRL_DUALSHOCK2) {
    psx[i]->enableAnalogButtons();
  }
  psx[i]->exitConfigMode();
  delay(1);
  const bool enabled = psx[i]->read() &&
    psx[i]->getProtocol() == PSPROTO_DUALSHOCK2 &&
    psx[i]->getAnalogButtonData() != nullptr;
  setDualShock2PressureState(i, enabled);
  return enabled;
}

void RZInputPSX::updateControllerAttentionInterval(uint8_t i, PsxControllerProtocol proto) {
  if (i >= input_ports || psxControllerDriver[i] == nullptr) {
    return;
  }

  uint16_t interval = kPsxDefaultAttentionIntervalUs;
  if (i < logical_slots && (proto == PSPROTO_DUALSHOCK2 || isDS2[i])) {
    interval = kPsxDualShock2AttentionIntervalUs;
  } else if (i < logical_slots && specialDpadMask[i] == SPECIALMASK_JET) {
    interval = kPsxJetAttentionIntervalUs;
  } else if (i < logical_slots && proto == PSPROTO_DUALSHOCK) {
    interval = psx_poll::kDualShockGapUs;
  }

  psxControllerDriver[i]->setAttentionInterval(
    psx_poll::attentionIntervalUs(psx_poll_timing, interval));
}

void RZInputPSX::clearPhysicalFallbackLatches() {
  isMultitap = false;
  multitapPhysicalPresent = false;
  multitapPhysicalPort = 0;
  multitapMemoryCardPresent = false;
  multitapMemoryCardLastProbeMs = 0;
  autoResolvedPhysicalProbePresent = false;
  autoResolvedPhysicalProbeLastMs = 0;
}

bool RZInputPSX::isGuitarFreaksSignature(PsxSingleController* controller,
                                         PsxControllerProtocol proto,
                                         uint16_t digitalData) const {
  if ((digitalData & SPECIALMASK_JET) != SPECIALMASK_JET)
    return false;

  if (proto == PSPROTO_DIGITAL)
    return true;

  if (proto == PSPROTO_DUALSHOCK) {
    byte analogX;
    byte analogY;
    return controller->getRightAnalog(analogX, analogY) && analogX == 0xFF;
  }

  return false;
}

// Konami's Pop'n and GuitarFreaks controllers identify themselves by holding
// d-pad bits a hand cannot (L+R+D, L+R) in every reply from the moment they
// connect. Only bits held in every reply since this pad connected count, so a
// pad that presses them later (a DDR L+R jump, a beatmania key, an SOCD-less
// hitbox) is never latched as one. Returns the signature mask, or 0.
uint8_t RZInputPSX::konamiDpadSignature(uint8_t i, PsxControllerProtocol proto) {
  if (i >= input_ports || i >= logical_slots || !haveController[i] || psx[i] == nullptr) {
    return 0;
  }

  dpadHeldSinceConnect[i] &= static_cast<uint8_t>(psx[i]->getButtonWord());
  const uint8_t held = dpadHeldSinceConnect[i];
  if (proto == PSPROTO_DIGITAL && (held & SPECIALMASK_POPN) == SPECIALMASK_POPN)
    return SPECIALMASK_POPN;
  if (isGuitarFreaksSignature(psx[i], proto, held))
    return SPECIALMASK_JET;
  return 0;
}

// beatmania replies exactly like a digital pad (FF 41 5A), so the player
// asks for its OLED picture: all five keys plus Start and Select held for
// BEATMANIA_HOLD_MS toggles it, once per hold.
void RZInputPSX::updateBeatmaniaPicture(uint8_t i, PsxControllerProtocol proto) {
  if (i >= input_ports || i >= logical_slots || psx[i] == nullptr) {
    return;
  }
  const bool chord = proto == PSPROTO_DIGITAL &&
    psx[i]->buttonPressed(PSB_SQUARE) && psx[i]->buttonPressed(PSB_L1) &&
    psx[i]->buttonPressed(PSB_CROSS) && psx[i]->buttonPressed(PSB_R1) &&
    psx[i]->buttonPressed(PSB_CIRCLE) && psx[i]->buttonPressed(PSB_START) &&
    psx[i]->buttonPressed(PSB_SELECT);
  if (!chord) {
    beatmaniaHoldSinceMs[i] = 0;
    beatmaniaHoldUsed[i] = false;
    return;
  }
  if (beatmaniaHoldUsed[i]) {
    return;
  }
  const uint32_t nowMs = millis();
  if (beatmaniaHoldSinceMs[i] == 0) {
    beatmaniaHoldSinceMs[i] = nowMs ? nowMs : 1;
  } else if ((uint32_t)(nowMs - beatmaniaHoldSinceMs[i]) >= BEATMANIA_HOLD_MS) {
    isBeatmania[i] = !isBeatmania[i];
    beatmaniaHoldUsed[i] = true;
    beatmaniaHoldSinceMs[i] = 0;
  }
}

void RZInputPSX::applyGuitarFreaksMapping(uint8_t index) {
  controller_state_t& frame = inputFrame(index);
  const bool redFretPressed = frame.R2 || frame.X || frame.A;
  uint8_t redFretAnalog = frame.ANALOG_R2;
  if (frame.ANALOG_X > redFretAnalog)
    redFretAnalog = frame.ANALOG_X;
  if (frame.ANALOG_A > redFretAnalog)
    redFretAnalog = frame.ANALOG_A;

  // Normalize the guitar so hosts see purple/green/red on Y/B/R2, with wailing on L2.
  frame.R2 = redFretPressed;
  frame.A = 0;
  frame.X = 0;
  frame.ANALOG_A = 0;
  frame.ANALOG_X = 0;
  frame.ANALOG_R2 = redFretAnalog;
}

uint16_t RZInputPSX::controllerDisconnectGraceMs(uint8_t port) const {
  if (isJogcon)
    return 1200;
  if (isNeGcon || isGuncon || isMouse[0] || isMouse[1] ||
      (port < logical_slots && lastProto[port] == PSPROTO_IRREMOTE))
    return 600;
  return 250;
}

uint16_t RZInputPSX::controllerStartupGraceMs() const {
  if (isJogcon)
    return 2200;
  if (isNeGcon || isGuncon || isMouse[0] || isMouse[1] || isIRRemote)
    return 1800;
  return 1800;
}

// JogCon and GunCon run on slot 0 alone. One found on the other connector
// takes slot 0, bringing its controller, driver and state with it.
void RZInputPSX::moveSetupControllerToFirstSlot(uint8_t port) {
  std::swap(psx[0], psx[port]);
  std::swap(psxControllerDriver[0], psxControllerDriver[port]);
  haveController[0] = haveController[port];
  haveController[port] = false;
  lastProto[0] = lastProto[port];
  lastProto[port] = PSPROTO_UNKNOWN;
  lastIrPollUs[0] = lastIrPollUs[port];
  lastIrPollUs[port] = 0;
  isDS2[0] = isDS2[port];
  isDS2[port] = false;
  armControllerStartupGrace(0);
  markControllerAlive(0);
}

void RZInputPSX::armControllerStartupGrace(uint8_t port) {
  if (port >= logical_slots) {
    return;
  }
  startupGraceUntilMs[port] = millis() + controllerStartupGraceMs();
}

void RZInputPSX::markControllerAlive(uint8_t port) {
  if (port >= logical_slots) {
    return;
  }
  const uint32_t now = millis();
  lastSuccessfulReadMs[port] = now;
  lastPhysicalControllerSeenMs = now;
}

bool RZInputPSX::shouldDropController(uint8_t port) const {
  if (port >= logical_slots) {
    return true;
  }

  if (lastSuccessfulReadMs[port] == 0)
    return true;

  if (startupGraceUntilMs[port] != 0 &&
      (int32_t)(millis() - startupGraceUntilMs[port]) < 0)
    return false;

  return (millis() - lastSuccessfulReadMs[port]) >= controllerDisconnectGraceMs(port);
}

void RZInputPSX::markControllerDisconnected(uint8_t port) {
  if (port >= logical_slots) {
    return;
  }

  debugDropCount[port]++;
  fishingCapture.disconnect(port, micros());
  resetFishingState(port);
  if (isNeGcon && port < input_ports) negconSpinner[port].reset(port);
  stopRumble(port);
  haveController[port] = false;
  if (isMouse[port]) {
    configureControllerFrame(port);
    if (isNeGcon) inputFrame(port).HAS_BTN_SELECT = false;
  }
  isMouse[port] = false;
  resetClassicMouseMotion(port);
  lastProto[port] = PSPROTO_UNKNOWN;
  if (port < input_ports) lastIrPollUs[port] = 0;
  isIRRemote = false;
  for (uint8_t i = 0; i < logical_slots; ++i)
    if (haveController[i] && lastProto[i] == PSPROTO_IRREMOTE) isIRRemote = true;
  rumbleConfiguredProto[port] = PSPROTO_UNKNOWN;
  nextRumbleConfigMs[port] = 0;
  analogProbeUntilMs[port] = 0;
  lastSuccessfulReadMs[port] = 0;
  startupGraceUntilMs[port] = 0;
  slotLastSeenMs[port] = 0;
  setDualShock2PressureState(port, false);
  resetPortHeuristics(port);
  if (port < MAX_USB_OUT) {
    // Release everything this pad was holding before the slot goes idle.
    setInputFrameConnected(inputFrame(port), false);
    setUpdated(port);
  }
  printf("lost\n");
}

bool RZInputPSX::explicitDancePadMode() const {
#ifdef ENABLE_INPUT_PSX_DANCE
  return deviceMode == RZORD_PSX_DANCE;
#else
  return false;
#endif
}

// Per-port detections (dance pad, Pop'n/GuitarFreaks mask, CrazyHit) belong to
// the controller that produced them; a disconnect or swap starts over.
void RZInputPSX::resetPortHeuristics(uint8_t port) {
  if (port >= logical_slots) {
    return;
  }
  isDancePad[port] = explicitDancePadMode();
  specialDpadMask[port] = 0;
  dpadHeldSinceConnect[port] = 0xFF;
  isCrazyHit[port] = false;
  crazyHitComboSinceMs[port] = 0;
  isBeatmania[port] = false;
  beatmaniaHoldUsed[port] = false;
  beatmaniaHoldSinceMs[port] = 0;
  beatmaniaTurnDirection[port] = 0;
}

void RZInputPSX::updateTransientDancePadOverride() {
  if (explicitDancePadMode()) {
    // The explicit Dance Pad input mode owns these settings itself.
    return;
  }
  bool anyDancePad = false;
  for (uint8_t i = 0; i < logical_slots; ++i) {
    if (haveController[i] && isDancePad[i]) {
      anyDancePad = true;
      break;
    }
  }
  setTransientDancePadOverride(anyDancePad);
}

void RZInputPSX::resetFishingState(uint8_t i) {
  if (i >= logical_slots) {
    return;
  }
  fishingReelSpeed[i].reset();
}

void RZInputPSX::updateFishingReelAxis(uint8_t i) {
  controller_state_t& frame = inputFrame(i);
  const uint8_t scale = spinner_speed <= 4 ? spinner_speed_mult[spinner_speed] : 4;
  frame.RX = int16_t(fishingReelSpeed[i].value(micros(), scale)) - 128;
  frame.RY = 0;
  frame.spinner = 0;
  frame.paddle = 0x80;
}

void RZInputPSX::applyFishingExperimentalMapping(uint8_t i, PsxSingleController* controller) {
  byte fishingRaw[8];
  byte fishingRawLen = 0;
  if (!controller->getFishingRawData(fishingRaw, fishingRawLen) || fishingRawLen < 8) {
    fishingCapture.sample(i, nullptr, 0, micros());
    resetFishingState(i);
    updateFishingReelAxis(i);
    return;
  }

  fishingCapture.sample(i, fishingRaw, fishingRawLen, micros());
  fishingReelSpeed[i].sample(fishingRaw[7], micros());
  updateFishingReelAxis(i);
}

void RZInputPSX::applyFishingExperimentalMapping(uint8_t i, PsxControllerData& cont) {
  byte fishingRaw[8];
  byte fishingRawLen = 0;
  if (!cont.getFishingRawData(fishingRaw, fishingRawLen) || fishingRawLen < 8) {
    resetFishingState(i);
    updateFishingReelAxis(i);
    return;
  }

  fishingReelSpeed[i].sample(fishingRaw[7], micros());
  updateFishingReelAxis(i);
}

bool RZInputPSX::ensurePsxBusDriversReady(bool configureFrames) {
  if (configureFrames && !psxControllerFramesConfigured) {
    for (uint8_t i = 0; i < logical_slots; ++i) {
      configureControllerFrame(i);
    }
    psxControllerFramesConfigured = true;
  }

  if (memoryCardBridgeDriversReady) {
    return true;
  }

  for (uint8_t i = 0; i < input_ports; ++i) {
    if (psxDriver[i] == nullptr) {
      input_psx_config_t c = input_psx_config[i];
#if defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE)
      psxDriver[i] = new PsxCaptureDriver<PsxDriverHwSpiWithAck>(
          c.spi, c.att, c.ack, c.cmd, c.dat, c.clk, i, 2);
#else
      psxDriver[i] = new PsxDriverHwSpiWithAck(c.spi, c.att, c.ack, c.cmd, c.dat, c.clk);
#endif
    }
    if (psxControllerDriver[i] == nullptr) {
      input_psx_config_t c = input_psx_config[i];
#if defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE)
      psxControllerDriver[i] = new PsxCaptureDriver<PsxDriverHwSpi>(
          c.spi, c.att, c.ack, c.cmd, c.dat, c.clk, i);
#elif defined(ADAPT_FEATURE_JOGCON_CAPTURE)
      psxControllerDriver[i] = new JogconCaptureDriver<PsxDriverHwSpi>(
          c.spi, c.att, c.ack, c.cmd, c.dat, c.clk, i);
#else
      psxControllerDriver[i] = new PsxDriverHwSpi(c.spi, c.att, c.ack, c.cmd, c.dat, c.clk); // without ACK
#endif
    }
    if (psx[i] == nullptr) {
      psx[i] = new PsxSingleController();
    }
    if (psxmulti[i] == nullptr) {
      psxmulti[i] = new PsxMultiController();
    }
  }

  return true;
}

bool RZInputPSX::refreshPsxBusDrivers() {
  for (uint8_t i = 0; i < input_ports; ++i) {
    if (psxDriver[i] == nullptr || !psxDriver[i]->begin()) {
      return false;
    }
  }

  memoryCardBridgeDriversReady = true;
  return initializePsxControllerDrivers();
}

bool RZInputPSX::initializePsxControllerDrivers() {
  for (uint8_t i = 0; i < input_ports; ++i) {
    if (psxControllerDriver[i] == nullptr || !psxControllerDriver[i]->begin()) {
      return false;
    }
    updateControllerAttentionInterval(i, lastProto[i]);
  }

  return true;
}

bool RZInputPSX::setupMemoryCardBridgeOnly() {
  return ensurePsxBusDriversReady(false) && refreshPsxBusDrivers();
}

bool RZInputPSX::detectMemoryCardPhysicalPresenceOnPort(uint8_t port) {
  if (port >= input_ports || !physical_port_enabled(port)) {
    return false;
  }

  PSXMemoryCardRawProbe probe;
  for (uint8_t slot = 0; slot < PSX_MEMCARD_SLOTS_PER_PORT; ++slot) {
    if (probePSXMemoryCardRaw(port, slot, 0, &probe) &&
        probe.response[2] == 0x5A &&
        probe.response[3] == 0x5D) {
      return true;
    }
  }
  return false;
}

bool RZInputPSX::multitapHasController(uint8_t port) {
  if (port >= input_ports || psxmulti[port] == nullptr) {
    return false;
  }

  PsxControllerData controller;
  for (uint8_t slot = 0; slot < multitap_slots; ++slot) {
    controller.clear();
    if (psxmulti[port]->read(slot, controller) &&
        controller.protocol != PSPROTO_UNKNOWN) {
      return true;
    }
  }
  return false;
}

bool RZInputPSX::autoResolvedPhysicalConnectionActive() const {
#ifdef ENABLE_INPUT_AUTODETECT
  if (savedDeviceMode != RZORD_AUTODETECT ||
      deviceMode != RZORD_PSX ||
      input_auto_resolve_port >= input_ports ||
      !physical_port_enabled(input_auto_resolve_port)) {
    return false;
  }

  const uint8_t port = input_auto_resolve_port;
  if (haveController[port] ||
      (port < MAX_USB_OUT && inputFrameConst(port).connected)) {
    return true;
  }

  // This getter is called by both AUTO hot-swap policy and OLED rendering.
  // It must never touch the PSX pins: the normal controller poller owns the
  // bus after mode setup. Keep only the short-lived evidence captured by the
  // AUTO transition while the first hardware-SPI poll settles.
  const uint32_t now = millis();
  const bool rawProbeGrace = autoResolvedPhysicalProbePresent &&
      autoResolvedPhysicalProbeLastMs != 0 &&
      (uint32_t)(now - autoResolvedPhysicalProbeLastMs) <
          kPsxAutoResolvedPhysicalPresenceGraceMs;
  const bool reconnectGrace = lastPhysicalControllerSeenMs != 0 &&
      (uint32_t)(now - lastPhysicalControllerSeenMs) <
          kPsxAutoResolvedPhysicalPresenceGraceMs;
  return rawProbeGrace || reconnectGrace;
#else
  return false;
#endif
}

bool RZInputPSX::multitapPhysicalConnectionActive() const {
  return isMultitap &&
         (multitapPhysicalPresent ||
          multitapMemoryCardPresent);
}

bool RZInputPSX::hasPhysicalConnectionForHotSwap() const {
  return multitapPhysicalConnectionActive() ||
         autoResolvedPhysicalConnectionActive();
}

const char* RZInputPSX::physicalConnectionDisplayName() const {
  if (multitapPhysicalConnectionActive()) {
    return multitapMemoryCardPresent ? "Multitap+Mem" : "Multitap";
  }
  if (autoResolvedPhysicalConnectionActive()) {
    return "PSX Device";
  }
  return nullptr;
}

void RZInputPSX::setup() {
  setInputPortCount(input_ports);

  empty_port_behaviour = EMPTY_PORT_USE_INTERVAL;
  polling_empty_interval_ms = 500;

  controllerType = CONT_NONE;
  for (uint8_t i = 0; i < logical_slots; ++i) {
    isMouse[i] = false;
    resetClassicMouseMotion(i);
    setDualShock2PressureState(i, false);
    resetPortHeuristics(i);
  }

  // Force dance pad mode if selected as input mode
  #ifdef ENABLE_INPUT_PSX_DANCE
  if (deviceMode == RZORD_PSX_DANCE) {
    for (uint8_t i = 0; i < input_ports; ++i) {
      isDancePad[i] = true;
    }
    dpad_mode = DPAD_MODE_BUTTONS;
    socdMode = SOCD_OFF;
  }
  #endif

  // Match RetroZordReborn: initialize both physical SPI buses once, then keep
  // their controller objects alive for direct empty-port retries.
  if (!ensurePsxBusDriversReady(true) || !initializePsxControllerDrivers()) {
    #ifdef USE_I2C_DISPLAY
      Wire.begin();
      display.clear();
      display.print(F("Cannot initialize psx driver"));
      Wire.end();
    #endif
    panic("Cannot initialize psx driver");
  }

#if defined(ADAPT_FEATURE_PS2_IR_CAPTURE)
  // The explicitly requested diagnostic owns all bus commands until export.
  // Avoid native polling/configuration before its recorded baseline.
  if (psxCaptureIrOwnsInput()) return;
#endif
  isMultitap = false;
  multitapPhysicalPresent = false;
  multitapPhysicalPort = 0;
  multitapMemoryCardPresent = false;
  multitapMemoryCardLastProbeMs = 0;
  autoResolvedPhysicalProbePresent = false;
  autoResolvedPhysicalProbeLastMs = 0;

  const uint8_t autoPsxPortHint = autoResolvedPsxPortHint(input_ports);
  PsxControllerProtocol proto = PSPROTO_UNKNOWN;
  uint8_t controllerSetupPort = 0xFF;
  // AUTO has already observed a strict reply on this connector, so try that
  // port first. The detector itself scans the left/P1 connector first, which
  // preserves physical player order when both pads arrive together while
  // allowing a right/P2-only pad to survive the detector-to-driver handoff.
  const uint8_t preferredControllerPort =
      autoPsxPortHint != 0xFF ? autoPsxPortHint : 0;

  auto trySetupSingleController = [&]() {
    for (uint8_t attempt = 0; attempt < kPsxSetupDetectAttempts && controllerSetupPort == 0xFF; ++attempt) {
      for (uint8_t scan = 0; scan < input_ports; ++scan) {
        const uint8_t port = (uint8_t)((preferredControllerPort + scan) % input_ports);
        if (!physical_port_enabled(port) || psx[port] == nullptr ||
            psxControllerDriver[port] == nullptr) {
          continue;
        }
        if (psx[port]->begin(*psxControllerDriver[port])) {
          haveController[port] = true;
          armControllerStartupGrace(port);
          markControllerAlive(port);
          controllerSetupPort = port;
          break;
        }
      }
      if (controllerSetupPort == 0xFF && attempt + 1 < kPsxSetupDetectAttempts) {
        delay(kPsxSetupRetryDelayMs);
      }
    }
  };

  // Single-controller replies are stronger evidence than a multitap fallback.
  // Always exhaust this path first so normal DualShock/analog pads are not
  // disturbed by the heavier multitap probe sequence.
  trySetupSingleController();

  // Try each enabled physical PSX connector. A multitap occupies that
  // connector and its sockets become logical USB player slots.
  // A strict AUTO single-controller reply must not be followed by disruptive
  // multitap configuration traffic if the first SPI transaction only needs
  // time to settle.
  if (controllerSetupPort == 0xFF && autoPsxPortHint == 0xFF) {
    for (uint8_t attempt = 0; attempt < kPsxMultitapSetupDetectAttempts && !isMultitap; ++attempt) {
      for (uint8_t port = 0; port < input_ports; ++port) {
        if (!physical_port_enabled(port) || psxmulti[port] == nullptr ||
            psxControllerDriver[port] == nullptr) {
          continue;
        }

        psxmulti[port]->begin(*psxControllerDriver[port]);
        if (psxmulti[port]->enableMultiTap() && multitapHasController(port)) {
          multitapPhysicalPort = port;
          isMultitap = true;
          break;
        }
      }

      if (!isMultitap && attempt + 1 < kPsxMultitapSetupDetectAttempts) {
        delay(kPsxMultitapSetupRetryDelayMs);
      }
    }
  }

  if (isMultitap) {
    setInputPortCount(min(multitap_slots, (uint8_t)MAX_USB_OUT));
    multitapPhysicalPresent = true;
    multitapMemoryCardPresent = detectMemoryCardPhysicalPresenceOnPort(multitapPhysicalPort);
    multitapMemoryCardLastProbeMs = millis();
    return;
  }

  if (controllerSetupPort == 0xFF) {
    if (autoPsxPortHint != 0xFF && physical_port_enabled(autoPsxPortHint)) {
      // AUTO already observed a strict PSX reply on this connector. Remember
      // that for hotswap status, but do not present a memory-card-only or
      // no-controller multitap route as an active PSX input mode.
      autoResolvedPhysicalProbePresent = true;
      autoResolvedPhysicalProbeLastMs = millis();
    }
  }

  //detect controller
  if (controllerSetupPort == 0xFF) {
    trySetupSingleController();
  }

  if (controllerSetupPort != 0xFF) {
    const uint8_t i = controllerSetupPort;
    clearPhysicalFallbackLatches();
    rumbleConfiguredProto[i] = PSPROTO_UNKNOWN;
    delay(kPsxPostControllerBeginDelayMs);

    //if not forced a mode, then read from currenct connected controller
    if (proto == PSPROTO_UNKNOWN)
      proto = psx[i]->getProtocol();

    if (proto == PSPROTO_GUNCON) {
      isGuncon = true;
    } else if (proto == PSPROTO_MOUSE) {
      isMouse[i] = true;
    } else if (proto == PSPROTO_NEGCON) {
      isNeGcon = true;
    } else if (proto == PSPROTO_JOGCON) {
      isJogcon = true;
      if (psx[i]->buttonPressed(PSB_L2))
        enableMouseMove = true;
      // neGcon MiSTer mode is fixed on the MiSTer/Linux (DInput) path
    } else { //jogcon can't be detected during boot as it needs to be in analog mode. also waiwai
      //Try to detect by it's id
      if (proto == PSPROTO_DIGITAL) {
        // Discover the receiver at 61/0F before scheduling native initialization.
        // IR receiver responds to both 0x01 (as digital controller) and 0x61 (as IR receiver)
        if (psx[i]->detectIR()) {
          isIRRemote = true;
        } else {
          tryEnableAnalogMode(i);
          proto = psx[i]->getProtocol();
          if (proto == PSPROTO_NEGCON) {
            isNeGcon = true;
          } else if (proto == PSPROTO_JOGCON) {
            isJogcon = true;
            if (psx[i]->buttonPressed(PSB_L2))
              enableMouseMove = true;
          } else if (psx[i]->enterConfigMode()) {
            auto ct = psx[i]->getControllerType();
            if (ct == PSCTRL_JOGCON) {
              isJogcon = true;
              if (psx[i]->buttonPressed(PSB_L2))
                enableMouseMove = true;
            } else if (ct == PSCTRL_WAIWAI) {
              isWaiwai = true;
            }
            psx[i]->exitConfigMode();
          }
        }
      }

      if (!isIRRemote && !isNeGcon && !isJogcon && psx[i]->buttonPressed(PSB_SELECT)) { //dualshock used in guncon mode to help map axis on emulators.
        isGuncon = true;
      }
    }

    tryEnableDualShock2Pressure(i);
    proto = psx[i]->getProtocol();
    lastProto[i] = proto;
    lastIrPollUs[i] = micros();
    updateControllerAttentionInterval(i, proto);

  } else { //no controller connected
    if (proto == PSPROTO_JOGCON)
      isJogcon = true;
  }

  if ((isJogcon || isGuncon) && controllerSetupPort != 0xFF && controllerSetupPort != 0) {
    moveSetupControllerToFirstSlot(controllerSetupPort);
  }

  if (isNeGcon) {
    turbo.setInputMode(TURBO_MODE_PSX_NEGCON);
    #if defined(ADAPT_CAP_DUAL_INPUT_MERGE)
    menu_classic_dual_merge = 0;
    classic_dual_merge_enabled = 0;
    #endif
    negconSetup();
  } else if (isJogcon) {
    turbo.setInputMode(TURBO_MODE_PSX_JOG);
    jogconSetup();
  } else if (isGuncon) {
    turbo.setInputMode(TURBO_MODE_PSX);
    gunconSetup();
  } else { //dualshock [default]
    turbo.setInputMode(TURBO_MODE_PSX);
    const uint8_t i = controllerSetupPort == 0xFF ? 0 : controllerSetupPort;
    const uint8_t signature = konamiDpadSignature(i, proto);
    if (signature != 0)
      specialDpadMask[i] = signature;

    setInputPortCount(input_ports);//MAX_USB_STICKS;
  }

  // Promote only after Linux/MiSTer host resolution; generic DInput on Windows
  // is not evidence that a native MiSTer peripheral descriptor is wanted.
  output_promote_psx_peripheral_mode(isJogcon, isNeGcon, isGuncon);
}

void RZInputPSX::setup2() {
  if (isNeGcon) {
    negconSetup2();
  } else if (isJogcon) {
    jogconSetup2();
  } else if (isGuncon) {
    gunconSetup2();
  }

  // Multitap mode exposes tap sockets as logical slots from the occupied
  // physical connector, so the normal physical-port LED pass does not apply.
  if (!isMultitap)
    for (uint8_t i = 0; i < input_ports; ++i)
      setPortLed(i, HIGH);
}

void RZInputPSX::noteStickActivity(uint8_t port, uint8_t lx, uint8_t ly, uint8_t rx, uint8_t ry) {
  if (port >= logical_slots) {
    return;
  }
  StickActivity& a = stickActivity[port];
  if (a.polls != 0) {
    if (lx != a.lx || ly != a.ly) ++a.leftChanges;
    if (rx != a.rx || ry != a.ry) ++a.rightChanges;
  }
  a.lx = lx;
  a.ly = ly;
  a.rx = rx;
  a.ry = ry;
  ++a.polls;
}

void RZInputPSX::printDiagnostics(Print& out) {
  for (uint8_t i = 0; i < input_ports && i < logical_slots; ++i) {
    out.print(F("PSXDIAG P"));
    out.print((int)(i + 1));
    out.print(F(" CONN="));
    out.print(haveController[i] ? 1 : 0);
    out.print(F(" PROTO="));
    out.print((int)lastProto[i]);
    PsxDriver* driver = psxControllerDriver[i];
    if (driver != nullptr) {
      const PsxDriver::AckStats& s = driver->ackStats;
      // Microseconds from the end of each byte to its /ACK.
      out.print(F(" ACK_US="));
      out.print(s.samples ? (int)s.minUs : 0);
      out.print('/');
      out.print(s.samples ? (unsigned long)(s.sumUs / s.samples) : 0UL);
      out.print('/');
      out.print((int)s.maxUs);
      out.print(F(" N="));
      out.print((unsigned long)s.samples);
      out.print(F(" MISS="));
      out.print((unsigned long)s.misses);
      out.print(F(" RAW="));
      const byte* reply = driver->lastReply();
      for (byte b = 0; b < driver->lastReplyLength; ++b) {
        if (reply[b] < 0x10) out.print('0');
        out.print(reply[b], HEX);
      }
      if (driver->lastReplyLength == 0) out.print('-');
      driver->resetAckStats();
    }
    const StickActivity& a = stickActivity[i];
    out.print(F(" POLLS="));
    out.print((unsigned long)a.polls);
    out.print(F(" LCHG="));
    out.print((unsigned long)a.leftChanges);
    out.print(F(" RCHG="));
    out.println((unsigned long)a.rightChanges);
    stickActivity[i] = StickActivity{};
  }
}

void RZInputPSX::printDebugStatus(Print& out) const {
  out.print(F("STATUS PSX MT="));
  out.print(isMultitap ? 1 : 0);
  out.print(F(" MTPHYS="));
  out.print(multitapPhysicalPresent ? 1 : 0);
  out.print(F(" MTPORT="));
  out.print((int)multitapPhysicalPort);
  out.print(F(" MTMEM="));
  out.print(multitapMemoryCardPresent ? 1 : 0);
  out.print(F(" ARPHYS="));
  out.print(autoResolvedPhysicalProbePresent ? 1 : 0);
  out.print(F(" CTRL="));
  for (uint8_t i = 0; i < logical_slots; ++i) {
    out.print(haveController[i] ? '1' : '0');
  }
  out.print(F(" RESPORT="));
  #ifdef ENABLE_INPUT_AUTODETECT
  if (input_auto_resolve_port == 0xFF) {
    out.print(F("-"));
  } else {
    out.print((int)input_auto_resolve_port);
  }
  #else
  out.print(F("-"));
#endif
  out.print(F(" POLL="));
  out.print(debugPollLoops);
  out.print(F(" SK="));
  for (uint8_t i = 0; i < input_ports; ++i) {
    if (i) out.print(',');
    out.print(debugPollSkipped[i]);
  }
  out.print(F(" BA="));
  for (uint8_t i = 0; i < input_ports; ++i) {
    if (i) out.print(',');
    out.print(debugBeginAttempt[i]);
  }
  out.print(F(" BO="));
  for (uint8_t i = 0; i < input_ports; ++i) {
    if (i) out.print(',');
    out.print(debugBeginSuccess[i]);
  }
  out.print(F(" RS="));
  for (uint8_t i = 0; i < input_ports; ++i) {
    if (i) out.print(',');
    out.print(debugReadSuccess[i]);
  }
  out.print(F(" RF="));
  for (uint8_t i = 0; i < input_ports; ++i) {
    if (i) out.print(',');
    out.print(debugReadFail[i]);
  }
  out.print(F(" DR="));
  for (uint8_t i = 0; i < input_ports; ++i) {
    if (i) out.print(',');
    out.print(debugDropCount[i]);
  }
  // Effective timing in us: poll period, then each port's command gap.
  out.print(F(" PI="));
  out.print(pollInterval);
  out.print(F(" AI="));
  for (uint8_t i = 0; i < input_ports; ++i) {
    if (i) out.print(',');
    out.print(psxControllerDriver[i] ? psxControllerDriver[i]->getAttentionInterval() : 0);
  }
  out.println();
}

void RZInputPSX::printDebugProbe(Print& out) {
  ensurePsxBusDriversReady(false);
  static const byte poll[] = {0x01, 0x42, 0x00, 0xFF, 0xFF};

  for (uint8_t port = 0; port < input_ports; ++port) {
    out.print(F("PSXPROBE P"));
    out.print((int)(port + 1));
    out.print(F(" EN="));
    out.print(physical_port_enabled(port) ? 1 : 0);

    byte raw[sizeof(poll)] = {};
    if (physical_port_enabled(port)) {
      psxDebugRawPoll(input_psx_config[port], raw, sizeof(raw));
#if defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE)
      psx_capture::Event event{};
      event.kind = uint8_t(psx_capture::Kind::AutoProbe);
      event.port = port;
      event.stage = 3;
      event.us = micros();
      event.count = sizeof(poll);
      memcpy(event.tx, poll, sizeof(poll));
      memcpy(event.rx, raw, sizeof(raw));
      psxCaptureRecord(event);
#endif
      refreshPsxBusDrivers();
    }
    out.print(F(" RAW_RX="));
    for (byte i = 0; i < sizeof(raw); ++i) {
      if (i) {
        out.print(',');
      }
      if (raw[i] < 0x10) {
        out.print('0');
      }
      out.print(raw[i], HEX);
    }

    byte in[sizeof(poll)] = {};
    bool ok = false;
    if (physical_port_enabled(port) && psxControllerDriver[port] != nullptr) {
      psxControllerDriver[port]->selectController();
      ok = psxControllerDriver[port]->shiftInOut(poll, sizeof(poll), in, sizeof(in), false);
      psxControllerDriver[port]->deselectController();
    }
    out.print(F(" CTRL_OK="));
    out.print(ok ? 1 : 0);
    out.print(F(" CTRL_RX="));
    for (byte i = 0; i < sizeof(in); ++i) {
      if (i) {
        out.print(',');
      }
      if (in[i] < 0x10) {
        out.print('0');
      }
      out.print(in[i], HEX);
    }

    memset(in, 0, sizeof(in));
    ok = false;
    if (physical_port_enabled(port) && psxDriver[port] != nullptr) {
      psxDriver[port]->selectController();
      ok = psxDriver[port]->shiftInOut(poll, sizeof(poll), in, sizeof(in), false);
      psxDriver[port]->deselectController();
    }
    out.print(F(" ACK_OK="));
#if defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE)
    if (physical_port_enabled(port) && psxDriver[port] != nullptr) {
      psx_capture::Event event{};
      event.kind = uint8_t(psx_capture::Kind::AckResult);
      event.port = port;
      event.stage = 2;
      event.us = micros();
      event.count = sizeof(poll);
      event.flags = psx_capture::PhysicalAck;
      if (ok) event.flags |= psx_capture::TransportOk;
      event.aux = psxDriver[port]->ackStats.misses;
      memcpy(event.tx, poll, sizeof(poll));
      memcpy(event.rx, in, sizeof(in));
      psxCaptureRecord(event);
    }
#endif
    out.print(ok ? 1 : 0);
    out.print(F(" ACK_RX="));
    for (byte i = 0; i < sizeof(in); ++i) {
      if (i) {
        out.print(',');
      }
      if (in[i] < 0x10) {
        out.print('0');
      }
      out.print(in[i], HEX);
    }

    bool beginOk = false;
    bool readOk = false;
    PsxControllerProtocol probeProto = PSPROTO_UNKNOWN;
    PsxButtons probeButtons = PSB_NONE;
    byte lx = 0x80;
    byte ly = 0x80;
    byte rx = 0x80;
    byte ry = 0x80;
    if (physical_port_enabled(port) && psxControllerDriver[port] != nullptr) {
      refreshPsxBusDrivers();
      PsxSingleController probeController;
      beginOk = probeController.begin(*psxControllerDriver[port]);
      if (beginOk) {
        readOk = probeController.read();
        probeProto = probeController.getProtocol();
        probeButtons = probeController.getButtonWord();
        probeController.getLeftAnalog(lx, ly);
        probeController.getRightAnalog(rx, ry);
      }
      refreshPsxBusDrivers();
    }
    out.print(F(" BEGIN_OK="));
#if defined(ADAPT_FEATURE_PSX_IDENTIFICATION_CAPTURE)
    psxCaptureDecoded(port, beginOk && readOk, uint8_t(probeProto), false);
#endif
    out.print(beginOk ? 1 : 0);
    out.print(F(" READ_OK="));
    out.print(readOk ? 1 : 0);
    out.print(F(" PROTO="));
    out.print((int)probeProto);
    out.print(F(" BTN=0x"));
    if ((uint16_t)probeButtons < 0x1000) out.print('0');
    if ((uint16_t)probeButtons < 0x0100) out.print('0');
    if ((uint16_t)probeButtons < 0x0010) out.print('0');
    out.print((uint16_t)probeButtons, HEX);
    out.print(F(" AX="));
    out.print((int)lx);
    out.print(',');
    out.print((int)ly);
    out.print(',');
    out.print((int)rx);
    out.print(',');
    out.print((int)ry);
    out.println();
  }
}
