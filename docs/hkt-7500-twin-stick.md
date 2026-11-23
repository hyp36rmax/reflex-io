# Dreamcast HKT-7500 Twin Stick

## Detection

Twin Stick identification requires Maple product name `Twin-Stick` or
`Twin Stick` plus capability mask `0x0000FEFE`. Mission Stick and other
dual-D-pad devices retain existing detection behavior.

## Routing and defaults

D-pad 1 follows standard Dreamcast routing. D-pad 2 directions remain in
Dreamcast `EXTRA` bits until output mapping. Each D-pad supports independent
routing to D-pad, Left Stick, Right Stick, or Buttons. Digital-to-analog routing
uses full scale; opposing directions center corresponding axes.

| Input | XInput | DInput / generic HID |
| --- | --- | --- |
| D-pad 1 | D-pad | POV hat |
| D-pad 2 Up | LB | Button 15 |
| D-pad 2 Down | RB | Button 16 |
| D-pad 2 Left | LS | Button 17 |
| D-pad 2 Right | RS | Button 18 |
| Pause (D) | Back | Button 19 |
| Start | Start | Existing Start |

Existing A/B/X/Y and trigger/thumb assignments remain unchanged.
OLED remapping and `Adapt.html` expose D-pad 2 directions and Pause (D)
when HKT-7500 is detected. Quick Config offers separate D-pad routing.

OLED remapping: select source, cycle available output destinations, confirm
selection, then Save. Standard Dreamcast pads retain physical-button
destination selection.

With Hold Start enabled, holding Start alone for 1000 ms sends Home/Guide.
Short press sends Start. Standard Dreamcast controllers retain normal Start
behavior.

Both Dreamcast ports share one input/output remap pair. Initialization scans
connected frames for HKT-7500. Directional and Pause processing stays scoped
to identified Twin Stick frames; standard pads do not receive Twin Stick
extra inputs.

## Storage

Dreamcast remap slot 13 stores D-pad 2 mode. Slots 14–18 hold directional
and Pause assignments. Remap table sizes, flash record versions, and
migration paths remain unchanged.

## Validation status

Physical HKT-7500 UAT complete. All validation gates passed.

| Gate | Status | Result |
| --- | --- | --- |
| HKT-7500 detection | PASS | Maple identity and capability mask confirmed on Classic2USB. |
| D-pad routing | PASS | Independent directions, diagonals, opposing inputs, analog routing, and neutral release confirmed. |
| XInput | PASS | D-pad 2: LB/RB/LS/RS. Pause (D): Back. Start and existing controls retained. |
| DInput | PASS | D-pad 1: POV hat. D-pad 2: Buttons 15–18. Pause (D): Button 19. |
| Start hold | PASS | Hold Start for 1000 ms: Home/Guide. Short press: Start. Standard pad behavior retained. |
| Configuration | PASS | OLED, WebHID, default restoration, save/reboot persistence, and output switching. |
| Mixed-port hardware | PASS | Both port orders, disconnect, and reconnect. |
| Twin Stick regression | PASS | 8 host-side checks for routing, defaults, detection, and input handling. |
| OLED layout | PASS | 11 checks. |
| Classic2USB build | PASS | PlatformIO build and RP2040 UF2 checks. |
| Firmware 1.0.2 UAT | PASS | Hardware validation completed. Latest UF2 checksum not recorded below. |

## Recorded firmware build

Build measurements from commit `1f175c2`, preceding later 1.0.2 branch
updates. Values below identify only this recorded build.

- Command: `pio run -e classic2usb` using PlatformIO with Python 3.12
- Result: PASS
- UF2: `.pio/build/classic2usb/firmware.uf2`, 3,682,816 bytes, 7,193 RP2040 UF2 blocks
- SHA-256: `fa1e64d396f5d69e8ee53bba6bbf2da43811b24b0a407353b8e013b60b32b3dd`
- RAM: 187,404 / 262,144 bytes, 71.5%
- Flash: 1,829,300 / 2,068,480 bytes, 88.4%
- Warnings: 8 repeated `-Wstringop-overflow` diagnostics in bundled RP2040 lwIP `dhcp.c`; no project-source warnings
