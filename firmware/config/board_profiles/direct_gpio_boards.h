#pragma once

#if defined(PRODUCT_MISTERCADE_2P_SIMPLE)
  // MiSTercade 2P USB Control Encoder Simple V2.0 (Konami, 2026-01-16).
  // The schematic interleaves P1/P2 on GPIO6-29. Pause/Test are global
  // cabinet inputs and occupy the otherwise-unused P1 stick-click slots.
  #define ADAPT_DIRECT_INPUT_PORTS 2
  #define ADAPT_DIRECT_DEBOUNCE_MS 0
  #define PIN_DIRECT_STATUS_LED   -1

  #define PIN_DIRECT_UP            6
  #define PIN_DIRECT_DOWN          8
  #define PIN_DIRECT_LEFT         10
  #define PIN_DIRECT_RIGHT        12
  #define PIN_DIRECT_A            14
  #define PIN_DIRECT_B            16
  #define PIN_DIRECT_X            18
  #define PIN_DIRECT_Y            20
  #define PIN_DIRECT_L1           22
  #define PIN_DIRECT_R1           24
  #define PIN_DIRECT_L2           26
  #define PIN_DIRECT_R2           28
  #define PIN_DIRECT_L3            0
  #define PIN_DIRECT_R3            1
  #define PIN_DIRECT_SELECT        2
  #define PIN_DIRECT_START         4
  #define PIN_DIRECT_HOME         -1
  #define PIN_DIRECT_CAPTURE      -1
  #define PIN_DIRECT_TURBO        -1
  #define PIN_DIRECT_LX           -1
  #define PIN_DIRECT_LY           -1
  #define PIN_DIRECT_RX           -1
  #define PIN_DIRECT_RY           -1
  #define PIN_DIRECT_NONE         -1

  #define PIN_DIRECT_P2_UP         7
  #define PIN_DIRECT_P2_DOWN       9
  #define PIN_DIRECT_P2_LEFT      11
  #define PIN_DIRECT_P2_RIGHT     13
  #define PIN_DIRECT_P2_A         15
  #define PIN_DIRECT_P2_B         17
  #define PIN_DIRECT_P2_X         19
  #define PIN_DIRECT_P2_Y         21
  #define PIN_DIRECT_P2_L1        23
  #define PIN_DIRECT_P2_R1        25
  #define PIN_DIRECT_P2_L2        27
  #define PIN_DIRECT_P2_R2        29
  #define PIN_DIRECT_P2_SELECT     3
  #define PIN_DIRECT_P2_START      5
#endif

#if defined(PRODUCT_REFLEX_ENCODE) || defined(PRODUCT_REFLEX_CTRL)
  #include "reflex_direct_revisions.h"
#endif

#if defined(PRODUCT_MISTERCADE_V2)
  #define PIN_DIRECT_UP       2
  #define PIN_DIRECT_DOWN     3
  #define PIN_DIRECT_RIGHT    4
  #define PIN_DIRECT_LEFT     5
  #define PIN_DIRECT_LX      -1
  #define PIN_DIRECT_LY      -1
  #define PIN_DIRECT_RX      -1
  #define PIN_DIRECT_RY      -1
  #define PIN_DIRECT_NONE    -1
  #define PIN_DIRECT_STATUS_LED 25
  #define ADAPT_DIRECT_DEBOUNCE_MS 0
#endif

#if defined(PRODUCT_MISTERCADE_2P_SIMPLE)
  #if ADAPT_DIRECT_INPUT_PORTS != 2 || \
      PIN_DIRECT_UP != 6 || PIN_DIRECT_P2_UP != 7 || \
      PIN_DIRECT_DOWN != 8 || PIN_DIRECT_P2_DOWN != 9 || \
      PIN_DIRECT_LEFT != 10 || PIN_DIRECT_P2_LEFT != 11 || \
      PIN_DIRECT_RIGHT != 12 || PIN_DIRECT_P2_RIGHT != 13 || \
      PIN_DIRECT_A != 14 || PIN_DIRECT_P2_A != 15 || \
      PIN_DIRECT_B != 16 || PIN_DIRECT_P2_B != 17 || \
      PIN_DIRECT_X != 18 || PIN_DIRECT_P2_X != 19 || \
      PIN_DIRECT_Y != 20 || PIN_DIRECT_P2_Y != 21 || \
      PIN_DIRECT_L1 != 22 || PIN_DIRECT_P2_L1 != 23 || \
      PIN_DIRECT_R1 != 24 || PIN_DIRECT_P2_R1 != 25 || \
      PIN_DIRECT_L2 != 26 || PIN_DIRECT_P2_L2 != 27 || \
      PIN_DIRECT_R2 != 28 || PIN_DIRECT_P2_R2 != 29 || \
      PIN_DIRECT_SELECT != 2 || PIN_DIRECT_P2_SELECT != 3 || \
      PIN_DIRECT_START != 4 || PIN_DIRECT_P2_START != 5 || \
      PIN_DIRECT_L3 != 0 || PIN_DIRECT_R3 != 1
    #error "MiSTercade 2P Encoder Simple GPIO contract drifted"
  #endif
#endif

#if defined(PRODUCT_MISTERCADE_V2)
  // Mirrors GP2040-CE configs/MiSTercadeV2/BoardConfig.h.
  #define PIN_DIRECT_A       13
  #define PIN_DIRECT_B        6
  #define PIN_DIRECT_X       10
  #define PIN_DIRECT_Y       11
  #define PIN_DIRECT_L1       8
  #define PIN_DIRECT_R1      12
  #define PIN_DIRECT_L2       9
  #define PIN_DIRECT_R2       7
  #define PIN_DIRECT_L3      14
  #define PIN_DIRECT_R3      15
  #define PIN_DIRECT_SELECT  16
  #define PIN_DIRECT_START   17
  #define PIN_DIRECT_HOME    -1
  #define PIN_DIRECT_CAPTURE -1
  #define PIN_DIRECT_TURBO   -1
  #define PIN_DIRECT_EXTRA0  18
  #define PIN_DIRECT_EXTRA1  19
  #define PIN_DIRECT_EXTRA2  20
  #define PIN_DIRECT_EXTRA3  21
  #define PIN_DIRECT_EXTRA4  22
  #define PIN_OLED_SDA        0
  #define PIN_OLED_SCL        1
  #define PIN_RGB_LED        28
  #define PIN_RGB_LED_COUNT  12
#endif
#ifndef PIN_DIRECT_EXTRA0
  #define PIN_DIRECT_EXTRA0 -1
  #define PIN_DIRECT_EXTRA1 -1
  #define PIN_DIRECT_EXTRA2 -1
  #define PIN_DIRECT_EXTRA3 -1
  #define PIN_DIRECT_EXTRA4 -1
#endif
#if defined(PRODUCT_MISTERCADE_V2)
  #if PIN_DIRECT_UP != 2 || PIN_DIRECT_DOWN != 3 || \
      PIN_DIRECT_RIGHT != 4 || PIN_DIRECT_LEFT != 5
    #error "Reflex direct-GPIO direction contract drifted"
  #endif
#endif
#if defined(PRODUCT_MISTERCADE_V2)
  #if PIN_DIRECT_A != 13 || PIN_DIRECT_B != 6 || \
      PIN_DIRECT_X != 10 || PIN_DIRECT_Y != 11 || \
      PIN_DIRECT_L1 != 8 || PIN_DIRECT_R1 != 12 || \
      PIN_DIRECT_L2 != 9 || PIN_DIRECT_R2 != 7 || \
      PIN_DIRECT_L3 != 14 || PIN_DIRECT_R3 != 15 || \
      PIN_DIRECT_SELECT != 16 || PIN_DIRECT_START != 17 || \
      PIN_DIRECT_EXTRA0 != 18 || PIN_DIRECT_EXTRA1 != 19 || \
      PIN_DIRECT_EXTRA2 != 20 || PIN_DIRECT_EXTRA3 != 21 || \
      PIN_DIRECT_EXTRA4 != 22 || PIN_OLED_SDA != 0 || \
      PIN_OLED_SCL != 1 || PIN_RGB_LED != 28 || \
      PIN_RGB_LED_COUNT != 12
    #error "MiSTercade V2 GPIO contract drifted"
  #endif
#endif
