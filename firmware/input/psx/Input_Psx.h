#pragma once

#include <EEPROM.h>

#include "../base/RZInputModule.h"
#include "input_psx_guncon_runtime_state.h"
#include "input_psx_jogcon_runtime_state.h"
#include "input_psx_negcon_runtime_state.h"
#include "psx_negcon_spinner.h"
#include "psx_fishing_capture.h"
#include "psx_fishing_reel.h"
#include "../../core/settings_store.h"
#include "../../menu/menu_runtime_state.h"
#include "../../platform/display_runtime_state.h"
#include <PsxNewLib/PsxDriver.h>
#include <PsxNewLib/PsxPublicTypes.h>
#include <PsxNewLib/PsxSingleController.h>
#include <PsxNewLib/PsxMultiController.h>

#define NEGCON_FORCE_MODE 1

extern uint8_t spinner_speed;
extern const uint8_t spinner_speed_mult[5];

// Multitap consumes the physical PSX port where it is detected and exposes
// its four controller sockets as logical USB slots.

//#define PSX_ACK 2 // HDMI_08 RESISTOR roxo-amarelo ok
//#define PSX_CMD 3 // HDMI_11 marrom
//#define PSX_DAT 4 // HDMI_12 RESISTOR azul-laranja ok
//#define PSX_ATT 5 // HDMI_13 vermelho
//#define PSX_CLK 6 // HDMI_10 cinza
//#define PSX_RUM HDMI_14 (7v)
//#define 33 HDMI_17 (+3.3) laranja
//#define gnd HDMI_15 (gnd)

#define GUNCON_FORCE_MODE 3

typedef struct {
  uint8_t spi; // 0 or 1
  uint8_t att; // must be unique
  uint8_t ack; // can be shared
  uint8_t cmd; // can be shared
  uint8_t dat; // can be shared
  uint8_t clk; // can be shared
} input_psx_config_t;

const input_psx_config_t input_psx_config[] = {
  { .spi = 0, .att = HDMI_1_13, .ack = HDMI_1_08, .cmd = HDMI_1_11, .dat = HDMI_1_12, .clk = HDMI_1_10 },
  { .spi = 1, .att = HDMI_2_13, .ack = HDMI_2_08, .cmd = HDMI_2_11, .dat = HDMI_2_12, .clk = HDMI_2_10 },
};

static constexpr uint16_t PSX_MEMCARD_FRAME_SIZE = 128;
static constexpr uint16_t PSX_MEMCARD_FRAME_COUNT = 1024;
static constexpr uint8_t PSX_MEMCARD_SLOTS_PER_PORT = 4;

struct PSXMemoryCardInfo {
  bool present = false;
  uint16_t totalBlocks = 0;
  uint16_t blockSize = PSX_MEMCARD_FRAME_SIZE;
  uint8_t flag = 0;
  uint8_t endByte = 0;
  uint8_t address = 0;
  int lastResult = 0;
};

struct PSXMemoryCardRawProbe {
  bool transferOk = false;
  uint8_t address = 0;
  uint8_t command = 0;
  uint8_t padByte = 0;
  uint16_t frame = 0;
  uint8_t responseLen = 0;
  uint8_t response[24] = {};
};

enum class PSXMemoryCardBlockResult {
  SUCCESS,
  NOT_PSX,
  NOT_PRESENT,
  INVALID_BLOCK,
  ADDRESS_MISMATCH,
  CHECKSUM_ERROR,
  READ_ERROR,
  WRITE_ERROR,
};

void psxMemoryCardBridgeNoteActivity(uint16_t holdMs = 250);
bool psxMemoryCardBridgeActive();
class Print;

inline void setPsxControllerTypeName(controller_state_t& frame, const char* name) {
  setInputFrameTypeName(frame, name);
}

class RZInputPSX : public RZInputModule {
  private:
      static const uint8_t input_ports { sizeof(input_psx_config) / sizeof(input_psx_config_t) };
      static constexpr uint8_t multitap_slots = 4;
      static constexpr uint8_t logical_slots = MAX_USB_OUT;
      static const uint16_t SLOT_DISCONNECT_DEBOUNCE_MS = 120;
      PsxDriver* psxDriver[input_ports] { nullptr };//<PSX_ATT, PSX_ACK, PSX_CMD, PSX_DAT, PSX_CLK> memory-card driver;
      PsxDriver* psxControllerDriver[input_ports] { nullptr };
      PsxSingleController* psx[input_ports] { nullptr };
      PsxMultiController* psxmulti[input_ports] { nullptr };
      bool memoryCardBridgeDriversReady { false };
      bool psxControllerFramesConfigured { false };

      enum MultitapModeControllerType { // used on multitap mode
        CONT_NONE,
        CONT_SINGLE,
        CONT_MULTIPLE
      };

      MultitapModeControllerType controllerType;

      //todo change to 16 bits if any mask uses the main buttons
      const uint8_t SPECIALMASK_POPN = PSB_PAD_RIGHT | PSB_PAD_DOWN | PSB_PAD_LEFT; //0xE0;
      const uint8_t SPECIALMASK_JET  = PSB_PAD_RIGHT | PSB_PAD_LEFT; //0xC0;

      bool isMultitap { false };
      bool multitapPhysicalPresent { false };
      uint8_t multitapPhysicalPort { 0 };
      bool multitapMemoryCardPresent { false };
      uint32_t multitapMemoryCardLastProbeMs { 0 };
      mutable bool autoResolvedPhysicalProbePresent { false };
      mutable uint32_t autoResolvedPhysicalProbeLastMs { 0 };
      mutable uint32_t lastPhysicalControllerSeenMs { 0 };
      bool isNeGcon { false };
      bool isJogcon { false };
      bool isGuncon { false };
      bool isMouse[logical_slots] {};
      bool isIRRemote { false };
      uint32_t nextIrProbeMs[input_ports] = {};
      static constexpr uint32_t kIrPollIntervalUs = 16667;
      uint32_t lastIrPollUs[input_ports] = {};
      // PSX peripheral output is fixed to MiSTer variants on MiSTer/Linux (DInput).
      bool isDS2[logical_slots] { false };
      // Konami CrazyHit (Pop'n) remap, per port. It reports as a plain
      // Digital pad, so it is armed only by holding its combo for
      // CRAZYHIT_ARM_HOLD_MS and clears when that port disconnects.
      bool isCrazyHit[logical_slots] { false };
      uint32_t crazyHitComboSinceMs[logical_slots] { 0 };
      static constexpr uint32_t CRAZYHIT_ARM_HOLD_MS = 1000;
      // Konami beatmania (PS1 five-key) also reports as a plain Digital pad.
      // Holding its five keys (Square L1 Cross R1 Circle) with Start and
      // Select for BEATMANIA_HOLD_MS switches the OLED to its picture; the
      // same hold switches back and a disconnect clears it. Picture only:
      // button output is unchanged.
      bool isBeatmania[logical_slots] { false };
      bool beatmaniaHoldUsed[logical_slots] { false };  // until the chord releases
      uint32_t beatmaniaHoldSinceMs[logical_slots] { 0 };
      static constexpr uint32_t BEATMANIA_HOLD_MS = 1000;
      // The turntable reports only Up/Down, so the OLED dial turns at a
      // fixed rate: one of its 32 steps on each press, then every
      // BEATMANIA_TURN_STEP_MS held (Up clockwise, Down counter-clockwise).
      int8_t beatmaniaTurnDirection[logical_slots] { 0 };
      uint32_t beatmaniaTurnStepMs[logical_slots] { 0 };
      // 120 ms (owner, 2026-10-18): about 3.8 s per turn of the dial.
      static constexpr uint32_t BEATMANIA_TURN_STEP_MS = 120;
      bool isWaiwai { false };
      bool isDancePad[logical_slots] { false };
      // Up+Down without Left+Right must stay held for DANCE_PAD_ARM_HOLD_MS
      // before a port becomes a Dance Pad, so a leverless pad rolling between
      // Up and Down, or one bad reply, does not latch it. 150 ms is a
      // judgment, not a measurement: longer than such a roll, shorter than a
      // landed jump.
      uint32_t dancePadComboSinceMs[logical_slots] { 0 };
      static constexpr uint32_t DANCE_PAD_ARM_HOLD_MS = 150;
      uint8_t specialDpadMask[logical_slots] { 0 };
      // D-pad bits held in every reply since this pad connected; Pop'n and
      // GuitarFreaks latch only from these (konamiDpadSignature()).
      uint8_t dpadHeldSinceConnect[logical_slots] { 0 };
      bool haveController[logical_slots] { false };
      PsxControllerProtocol lastProto[logical_slots] { PSPROTO_UNKNOWN };
      uint32_t analogProbeUntilMs[logical_slots] { 0 };
      uint32_t nextAnalogProbeMs[logical_slots] { 0 };
      uint32_t nextRumbleConfigMs[logical_slots] { 0 };
      PsxControllerProtocol rumbleConfiguredProto[logical_slots] { PSPROTO_UNKNOWN };
      uint32_t lastSuccessfulReadMs[logical_slots] { 0 };
      uint32_t startupGraceUntilMs[logical_slots] { 0 };
      uint32_t slotLastSeenMs[logical_slots] { 0 };
      psx_fishing::ReelSpeed fishingReelSpeed[logical_slots];
      psx_fishing::Capture fishingCapture;
      uint32_t debugPollLoops { 0 };
      uint32_t debugPollSkipped[logical_slots] { 0 };
      uint32_t debugBeginAttempt[logical_slots] { 0 };
      uint32_t debugBeginSuccess[logical_slots] { 0 };
      uint32_t debugReadSuccess[logical_slots] { 0 };
      uint32_t debugReadFail[logical_slots] { 0 };
      uint32_t debugDropCount[logical_slots] { 0 };
      bool enableMouseMove { false }; //used on guncon and jogcon modes
      uint8_t pendingWebhidRaw[16] = {};
      bool pendingWebhidRawDirty = false;

      void handleDpad() {}
      void configureControllerFrame(uint8_t i);
      void stopRumble(uint8_t i);
      void tryEnableRumble(uint8_t i);
      bool tryEnableAnalogMode(uint8_t i);
      void armAnalogProbe(uint8_t i);
      void serviceAnalogProbe(uint8_t i);
      bool tryEnableDualShock2Pressure(uint8_t i);
      void updateControllerAttentionInterval(uint8_t i, PsxControllerProtocol proto);
      void clearPhysicalFallbackLatches();
      bool isAnyDualShock2PressureActive() const;
      void setDualShock2PressureState(uint8_t index, bool enabled);
      void applyDualShock2Pressure(uint8_t index, PsxSingleController* controller);
      void applyDualShock2Pressure(uint8_t index, PsxControllerData& controller);
      bool isGuitarFreaksSignature(PsxSingleController* controller,
                                   PsxControllerProtocol proto,
                                   uint16_t digitalData) const;
      uint8_t konamiDpadSignature(uint8_t i, PsxControllerProtocol proto);
      void updateBeatmaniaPicture(uint8_t i, PsxControllerProtocol proto);
      void updateBeatmaniaTurntable(uint8_t i);
      void applyGuitarFreaksMapping(uint8_t index);
      uint16_t controllerDisconnectGraceMs(uint8_t port) const;
      uint16_t controllerStartupGraceMs() const;
      void moveSetupControllerToFirstSlot(uint8_t port);
      void armControllerStartupGrace(uint8_t port);
      void markControllerAlive(uint8_t port);
      bool shouldDropController(uint8_t port) const;
      bool shouldPollController(uint8_t port, uint32_t nowUs);
      void markControllerDisconnected(uint8_t port);
      void resetPortHeuristics(uint8_t port);
      bool explicitDancePadMode() const;
      void updateTransientDancePadOverride();
      void resetFishingState(uint8_t i);
      void updateFishingReelAxis(uint8_t i);
      void applyFishingExperimentalMapping(uint8_t i, PsxSingleController* controller);
      void applyFishingExperimentalMapping(uint8_t i, PsxControllerData& cont);
      void markMultitapSlotMissing(uint8_t slot, uint32_t now);
      void clearMultitapSlot(uint8_t slot);
      bool ensurePsxBusDriversReady(bool configureFrames);
      bool initializePsxControllerDrivers();
      bool refreshPsxBusDrivers();
      bool detectMemoryCardPhysicalPresenceOnPort(uint8_t port);
      bool refreshMultitapMemoryCardPresence(uint32_t now);
      bool multitapHasController(uint8_t port);
      bool autoResolvedPhysicalConnectionActive() const;
      bool multitapPhysicalConnectionActive() const;
      void stageWebhidRawData(const uint8_t* raw, uint8_t len);
      void flushPendingWebhidRawData();

#include "Input_Psx_Guncon.h"
#include "Input_Psx_Negcon.h"
#include "Input_Psx_Jogcon.h"

  public:
#if defined(ADAPT_FEATURE_JOGCON_CAPTURE)
    void captureStopJogconMotor() { if (isJogcon && haveController[0]) stopRumble(0); }
#endif
    RZInputPSX();
    ~RZInputPSX() override {
      if (isJogcon && haveController[0]) stopRumble(0);
      releaseInputPortObjects(psx);
      releaseInputPortObjects(psxmulti);
      releaseInputPortObjects(psxControllerDriver);
      releaseInputPortObjects(psxDriver);
    }

    const char* getUsbId() override;
    void configureBcdDeviceVersion() override;
    const char* getDescription() override;

    void setup() override;
    void setup2() override;
    bool setupMemoryCardBridgeOnly();
    bool hasPhysicalConnectionForHotSwap() const override;
    const char* physicalConnectionDisplayName() const override;

    void mapMultiController(uint8_t i, PsxControllerData& cont);

    bool is_port_connected(const uint8_t index) override;
    void handleSpecialMask(uint8_t index);
    bool pollMultitap();
    bool poll() override;
    void afterOutputFrameSent(bool polled, bool updated) override;

    bool getPSXMemoryCardInfo(uint8_t port, uint8_t slot, PSXMemoryCardInfo* info);
    bool probePSXMemoryCardRaw(uint8_t port, uint8_t slot, uint16_t frame, PSXMemoryCardRawProbe* probe,
                               uint8_t padByte = 0x00, uint8_t command = 0x52,
                               uint8_t addressOverride = 0x00);
    // `flag`, when given, receives the card's FLAG byte from the transfer.
    PSXMemoryCardBlockResult readPSXMemoryCardFrame(uint8_t port, uint8_t slot, uint16_t frame, uint8_t* buffer,
                                                    uint8_t* flag = nullptr);
    PSXMemoryCardBlockResult writePSXMemoryCardFrame(uint8_t port, uint8_t slot, uint16_t frame, const uint8_t* buffer,
                                                     uint8_t* flag = nullptr);
    // A multitap on this physical port addresses four card slots.
    bool multitapOnPort(uint8_t port) const { return multitapPhysicalPresent && multitapPhysicalPort == port; }
    void printDebugStatus(Print& out) const;
    void printDebugProbe(Print& out);
    // PSXDIAG: per-port /ACK timing, last raw reply and stick-change counts
    // since the previous PSXDIAG, which then starts a new window.
    void printDiagnostics(Print& out);
#if defined(ADAPT_FEATURE_PS2_IR_CAPTURE)
    bool beginIrDiagnostic(uint8_t port);
    bool captureIrDiagnostic(uint8_t port, uint8_t phase);
#endif
    void handleFishingCaptureCommand(char* command, Print& out);
    void printFishingCaptureStatus(Print& out, uint32_t nowUs);
    void noteStickActivity(uint8_t port, uint8_t lx, uint8_t ly, uint8_t rx, uint8_t ry);
    struct StickActivity {
      uint8_t lx, ly, rx, ry;
      uint32_t polls, leftChanges, rightChanges;
    };
    StickActivity stickActivity[logical_slots] {};
};
