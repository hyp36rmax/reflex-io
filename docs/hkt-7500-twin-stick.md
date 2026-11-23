# Dreamcast HKT-7500 Twin Stick

## Detection

The Dreamcast profile labels a controller `Twin-Stick` only when Maple reports
the product name `Twin-Stick` or `Twin Stick` and the exact capability mask
`0x0000FEFE`. Other dual-D-pad devices continue through their existing profile
detection, including the Mission Stick.

## Routing and defaults

D-pad 1 remains the Dreamcast native D-pad and uses the existing D-pad mode
setting. D-pad 2 keeps its four directions in the Dreamcast `EXTRA` bits until
the final output mapping stage. Its mode is independently configurable as
D-pad, Left Stick, Right Stick, or Buttons. Digital stick routing uses full
scale; an opposing pair on one axis centers that axis.

| Input | XInput | DInput / generic HID |
| --- | --- | --- |
| D-pad 1 | D-pad | POV hat |
| D-pad 2 Up | LB | Button 15 |
| D-pad 2 Down | RB | Button 16 |
| D-pad 2 Left | LS | Button 17 |
| D-pad 2 Right | RS | Button 18 |
| Pause (D) | Back | Button 19 |
| Start | Start | Existing Start |

The existing A/B/X/Y and trigger/thumb assignments are unchanged. Twin Stick
direction and Pause assignments are available in the OLED remap menu and
Adapt.html. Quick Config exposes D-pad 1 and D-pad 2 routing when the detected
controller is the HKT-7500. In the OLED remap menu, select a Twin Stick source,
then use the menu navigation controls to cycle through destinations supported
by the selected output. Select confirms the destination; Save writes the
remaps. Other controller profiles keep their existing physical-button
destination selection.

When the Hold Start setting is enabled, holding Start alone for 1000 ms on a
detected Twin Stick sends a brief Home/Guide signal. A short press remains
Start. Standard Dreamcast controllers do not use this shortcut.

Mixed Dreamcast ports share the Dreamcast input/output remap pair. Automatic
Twin Stick initialization checks every connected Dreamcast frame. Direction
and Pause transformations remain gated by each frame's own controller type, so
a standard pad on another port does not receive Twin Stick inputs.

## Storage

D-pad 2 mode uses Dreamcast remap slot 13, which is unused by Dreamcast input.
The setting is encoded in that existing slot. Direction and Pause assignments
reuse slots 14–18. Remap table widths and flash record versions are unchanged;
existing records continue to load through the existing remap migration path.

## Validation status

All validation gates have been completed, including physical HKT-7500 testing.

| Gate | Status | Result |
| --- | --- | --- |
| HKT-7500 detection | PASS | Classic2USB identifies the Twin Stick using its Maple product name and capability mask. |
| D-pad routing | PASS | Both D-pads operate independently. Directional, diagonal, opposing-input, analog-stick, and neutral-release behavior validated. |
| XInput | PASS | D-pad 2 maps to LB, RB, LS, and RS. Pause (D) maps to Back. Start and existing controls retain their assignments. |
| DInput | PASS | D-pad 1 uses the POV hat. D-pad 2 uses buttons 15 through 18, and Pause (D) uses button 19. |
| Start hold shortcut | PASS | The Twin Stick Start hold sends Home/Guide after 1000 ms. A short press remains Start; standard Dreamcast Start behavior is unchanged. |
| Configuration and persistence | PASS | OLED and WebHID remapping, default restoration, saved mappings across reboot, and output switching validated. |
| Mixed-port hardware | PASS | Twin Stick and standard Dreamcast controllers validated in both port orders, including disconnect and reconnect. |
| Twin Stick regression tests | PASS | 8 host-side checks covering routing, defaults, identification, and input behavior. |
| OLED layout checks | PASS | 11 layout checks completed. |
| Classic2USB firmware | PASS | PlatformIO build completed; RP2040 UF2 validated. The recorded build measurements appear below. |
| Firmware 1.0.2 UAT | PASS | Project hardware validation completed. The final 1.0.2 UF2 hash is not included in the earlier build record below. |

## Recorded firmware build

The following measurements are from commit `1f175c2`. They are not a build record for the later 1.0.2 branch head.

- Command: `pio run -e classic2usb` using PlatformIO with Python 3.12
- Result: PASS
- UF2: `.pio/build/classic2usb/firmware.uf2`, 3,682,816 bytes, 7,193 RP2040 UF2 blocks
- SHA-256: `fa1e64d396f5d69e8ee53bba6bbf2da43811b24b0a407353b8e013b60b32b3dd`
- RAM: 187,404 / 262,144 bytes, 71.5%
- Flash: 1,829,300 / 2,068,480 bytes, 88.4%
- Warnings: 8 repeated `-Wstringop-overflow` diagnostics in bundled RP2040 lwIP `dhcp.c`; none from project source
