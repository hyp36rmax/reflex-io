#pragma once

#include "../base/RZInputModule.h"

// ColecoVision controller on the Classic2USB HDMI port. The adapter carries no
// power: the port has none, and select lines need the console's idle level.
//   DE9 1/2/3/4 (Up/Down/Left/Right) -> HDMI 1/2/3/4
//   DE9 6 (fire)                     -> HDMI 5
//   DE9 5 (keypad select)            -> HDMI 6
//   DE9 8 (joystick select)          -> HDMI 7
//   DE9 7/9 (Super Action roller)    -> unconnected in the basic adapter
// HDMI 10/11 remain reserved for a future electrically protected roller
// adapter. Firmware does not decode them; never wire 5 V roller signals there.
// HDMI 1-7 are 5 V-tolerant behind level shifters with 10k pull-ups, so a
// released select line idles high like the console's. The firmware only ever
// pulls a select line low or releases it; it never drives one high.
// Atari joysticks work too: their ground (DE9 8) is the joystick select.
typedef struct {
  uint8_t up;
  uint8_t down;
  uint8_t left;
  uint8_t right;
  uint8_t fire;
  uint8_t keypadSelect;
  uint8_t joystickSelect;
  uint8_t rollerA;  // reserved: roller decoding needs a bench check first
  uint8_t rollerB;
} input_colecovision_config_t;

extern const input_colecovision_config_t input_colecovision_config[2];

class RZInputColecoVision : public RZInputModule {
  private:
    static constexpr uint8_t input_ports = 2;

    uint16_t readController(uint8_t port);

  public:
    RZInputColecoVision();
    // Releases the select lines, so a mode switch never leaves one driven.
    ~RZInputColecoVision() override;

    const char* getUsbId() override;
    void configureBcdDeviceVersion() override;
    const char* getDescription() override;
    void setup() override;
    void setup2() override;
    bool poll() override;
};
