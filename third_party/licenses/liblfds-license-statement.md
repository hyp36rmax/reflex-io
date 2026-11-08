# liblfds license statement

liblfds ships no license file, so there is no upstream file to copy. This note
records where its terms come from.

- Source used: `darthcloud/liblfds7.1.1`, a repository holding liblfds 7.1.1, at
  commit `da3494fef10df4681e267d8b2b8cce2c90d5a9fa`. That commit has no license
  or copying file, and its source files carry no license headers.
- Terms: the License section of <https://liblfds.org/>, read 2026-11-01. It says
  the library may be used in any way, places everything in the public domain,
  and grants a range of licenses up front: MIT, BSD, Apache, GPL and LGPL in all
  versions, and every Creative Commons license. A custom license is offered on
  request.
- Local copy: `firmware/targets/modern2usb/esp32/blueretro/components/queue_bss/liblfds/`.
  All 386 files match the commit above byte for byte, and the 71 not copied are
  build support files.
- In the ESP32 image: only the bounded single-producer, single-consumer queue.

`lfds711_prng.h` takes the SplitMix generator from Sebastiano Vigna's site and
marks it CC0. The queue sources in the ESP32 image do not use it.
