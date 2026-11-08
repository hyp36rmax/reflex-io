#pragma once

#include "../base/RZInputModule.h"

// Philips CD-i pointing devices on the Classic2USB HDMI port, wired like the
// SNAC CDi adapter board (mini-DIN 8 to HDMI):
//   mini-DIN 2 (RXD, data from the device) -> HDMI 2
//   mini-DIN 7 (RTS, to the device)        -> HDMI 6
//   mini-DIN 8 (+5 V for the device)       -> HDMI 16 and 18
//   mini-DIN 5 (ground)                    -> HDMI 15
// HDMI 1-7 are BSS138 level shifters with 10k pull-ups to +5 V on the
// connector side, so the firmware can only pull them low. RTS is asserted by
// releasing HDMI 6 and negated by pulling it low. The data line idles low
// (mark) while a powered device is attached and reads high through the
// pull-up when none is. Protocol: cdi_protocol.h.
typedef struct {
  uint8_t rxd;
  uint8_t rts;
} input_cdi_config_t;

extern const input_cdi_config_t input_cdi_config[2];

class RZInputCdi : public RZInputModule {
  private:
    static constexpr uint8_t input_ports = 2;

    void setRts(uint8_t port, bool asserted);

  public:
    RZInputCdi();
    // Stops the receivers and releases RTS, so a mode switch leaves the
    // HDMI lines as other modes expect them.
    ~RZInputCdi() override;

    const char* getUsbId() override;
    void configureBcdDeviceVersion() override;
    const char* getDescription() override;
    void setup() override;
    void setup2() override;
    bool poll() override;
};
