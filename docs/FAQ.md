# Reflex Adapt Classic2USB FAQ

## Do I need to reflash when I change controllers or USB output modes?

No. The Classic2USB firmware image contains its supported controller inputs and
USB output modes. Select the input and output modes in the adapter menu or
Adapt browser configurator.

## How do I configure or remap a controller?

Use Quick Config and Button Remap in the OLED menu, or open
[Adapt.html](../web/Adapt.html) in a compatible browser. The
[Classic2USB manual](https://docs.misteraddons.com/products/reflex-adapt/classic2usb/)
covers adapter setup and the full control layout.

## Does the firmware identify the Sega HKT-7500 Twin Stick?

Yes. The Dreamcast profile requires the Twin Stick Maple product name and the
matching capability mask. This keeps other Dreamcast devices, including the
Mission Stick, on their existing profiles. See the
[HKT-7500 Twin Stick guide](hkt-7500-twin-stick.md) for mappings and validation.

## What does holding Start do on the HKT-7500?

Classic2USB 1.0.3 replaces the fixed 1,000 ms threshold with one global
Hold Start duration: 500-3,000 ms in 500 ms steps, default 2,000 ms. When
Hotkeys is On, holding Start alone sends Home/Guide at selected duration.
Hotkeys Off disables fixed Start shortcuts globally; short Start presses
remain native. HKT-7500 does not have a separate timing profile.

Set duration in Adapt Web > System Settings > Menus and buttons > Hold Start
Duration, or hold MODE on hardware, open System Settings, select Guide Hold
directly after Hotkeys, choose duration, then save.

## Where can I get firmware or build it?

Download tested UF2 files from [GitHub Releases](https://github.com/misteraddons/reflex-io/releases).
To build Classic2USB firmware from source, install PlatformIO Core and run
`pio run -e classic2usb` from the repository root. The UF2 is written to
`.pio/build/classic2usb/firmware.uf2`.
