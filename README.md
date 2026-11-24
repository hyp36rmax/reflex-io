# Reflex Adapt Classic2USB

Classic2USB is the RP2040 firmware for Reflex Adapt classic-controller USB
adapters. Every supported input and output mode fits in one firmware image, so
changing controllers or hosts does not need a reflash.

- **Firmware:** the [Releases](https://github.com/misteraddons/reflex-io/releases)
  page holds each tested UF2.
- **Setup and manual:** [docs.misteraddons.com](https://docs.misteraddons.com/products/reflex-adapt/classic2usb/).
- **MiSTer:** [`reflex_adapt_manager.sh`](tools/release_assets/adapt-manager/mister/Scripts/Reflex_Adapt/reflex_adapt_manager.sh)
  installs firmware, rollbacks and mappings.
- **Browser tool:** [`web/Adapt.html`](web/Adapt.html) changes settings, remaps
  buttons and updates firmware.
- **Guide Hold Duration:** [configuration and compatibility](docs/guide-hold-duration.md)
  explains global Hold Start timing for Classic2USB.
- **Hardware:** [Reflex Adapt](https://misteraddons.com/collections/kits/products/reflex-adapt)
  and [HDMI controller adapters](https://misteraddons.com/products/hdmi-controller-adapters).
- **Reflex Adapt V1:** the original firmware is in
  [Reflex-Adapt-Legacy](https://github.com/misteraddons/Reflex-Adapt-Legacy).

## About this repository

Each release publishes the source of its tested UF2, with the MiSTer and browser
tools. Development happens in a private repository, so this one has one commit
for each release.

## Building

You need [PlatformIO](https://platformio.org/) and Git.

```bash
pio run -e classic2usb
```

The UF2 is `.pio/build/classic2usb/firmware.uf2`. A build embeds its own date and
time, so it won't match a released UF2 byte for byte.

## License

GPL-3.0-or-later: see [LICENSE](LICENSE). Third-party notices are in
[NOTICE.md](NOTICE.md), with license copies in
[third_party/licenses](third_party/licenses).
