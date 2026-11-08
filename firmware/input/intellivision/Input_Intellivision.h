#pragma once

#include "../base/RZInputModule.h"

// Intellivision controller lines B0-B7 on the Classic2USB HDMI port. One pin
// table serves every Intellivision adapter cable (see
// docs/internal/classic2usb/intellivision-colecovision.md):
//   line     HDMI   II/Sears DE9   Flashback DE9   INTV I/III wire
//   B3..B0   1..4   1, 2, 3, 4     9, 4, 8, 1      5, 4, 3, 2
//   B7..B5   5..7   6, 7, 8        2, 7, 6         9, 8, 7
//   B4       9      9              5               6
//   GND      19     5              3               1
// B4 is on HDMI 9 rather than the 1k-pulled HDMI 8/12 so worn membrane
// contacts still read low; HDMI 1-7 are 5 V-tolerant behind level shifters.
// The adapter puts 1 kOhm in series with B4: AUTO's Maple probe drives HDMI 9
// high, and a held B4 direction would otherwise short it to ground.
typedef struct {
  uint8_t b0;
  uint8_t b1;
  uint8_t b2;
  uint8_t b3;
  uint8_t b4;
  uint8_t b5;
  uint8_t b6;
  uint8_t b7;
} input_intv_config_t;

extern const input_intv_config_t input_intv_config[2];

class RZInputIntv : public RZInputModule {
  private:
    static constexpr uint8_t input_ports = 2;

    uint8_t readController(uint8_t port);

  public:
    RZInputIntv();

    const char* getUsbId() override;
    void configureBcdDeviceVersion() override;
    const char* getDescription() override;
    void setup() override;
    void setup2() override;
    bool poll() override;
};
