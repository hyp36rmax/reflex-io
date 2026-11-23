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

Results are recorded against the build or hardware session actually tested.

| Gate | Status | Result |
| --- | --- | --- |
| HKT-7500 detection | PASS | Physical Classic2USB testing confirmed Twin Stick identification. |
| D-pad 2 routing | PASS | D-pad 2 configuration and operation were confirmed during earlier physical testing. Full USB report coverage is not established by this result. |
| Twin Stick regression tests | PASS | 8 host-side tests covering directional routing, remap defaults, port detection, and input handling. |
| OLED layout checks | PASS | 11 tests completed during the recorded Classic2USB build. |
| Classic2USB build | PASS | PlatformIO completed at source commit `1f175c2`. The resulting RP2040 UF2 passed structural checks. |
| Firmware 1.0.2 build | NEEDS VALIDATION | The branch advanced to `1ec2fa4` for the 1.0.2 version update after the recorded build. Rebuild and record the new UF2 hash before release. |
| Start hold shortcut | NEEDS VALIDATION | On physical HKT-7500 hardware, confirm that holding Start for 1000 ms sends Home/Guide, a short press sends Start, and standard Dreamcast controllers retain their existing behavior. |
| USB reports and persistence | NEEDS VALIDATION | Verify XInput LB, RB, LS, RS, and Back; DInput buttons 15 through 19; save and reboot; output switching; and legacy flash migration on hardware. |
| Mixed-port hardware | NEEDS VALIDATION | Verify both controller port orders and disconnect/reconnect with physical controllers. Mixed-port host tests have passed, but do not establish hardware behavior. |

The physical results above apply to the earlier tested firmware. Host tests do not replace USB report captures or save and reboot testing.

## Recorded firmware build

The following measurements are from commit `1f175c2`. They are not a build record for the later 1.0.2 branch head.

- Command: `pio run -e classic2usb` using PlatformIO with Python 3.12
- Result: PASS
- UF2: `.pio/build/classic2usb/firmware.uf2`, 3,682,816 bytes, 7,193 RP2040 UF2 blocks
- SHA-256: `fa1e64d396f5d69e8ee53bba6bbf2da43811b24b0a407353b8e013b60b32b3dd`
- RAM: 187,404 / 262,144 bytes, 71.5%
- Flash: 1,829,300 / 2,068,480 bytes, 88.4%
- Warnings: 8 repeated `-Wstringop-overflow` diagnostics in bundled RP2040 lwIP `dhcp.c`; none from project source
