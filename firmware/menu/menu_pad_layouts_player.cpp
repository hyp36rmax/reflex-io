#include "../product_config.h"

#include "menu_pad_layouts_internal.h"
#include "menu_classic_pad_art.h"

#include <cstring>

#include "../core/controller_frame_state.h"
#include "../core/device_runtime_state.h"
#include "../input/jaguar/input_jaguar_runtime_state.h"

using namespace menu_pad_layouts_internal;

void getLayoutForPlayer(uint8_t player, const PadButton** layout, uint8_t* layoutCount) {
  const char* layoutName = nullptr;
  const char* typeName = controllerFrameConst(player).controller_type_name;

#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  if (std::strcmp(typeName,"SNES Mouse")==0 || std::strcmp(typeName,"Saturn Mouse")==0 ||
      std::strcmp(typeName,"Sega Mouse")==0 ||
      std::strcmp(typeName,"PSX Mouse")==0 || std::strcmp(typeName,"N64 Mouse")==0 ||
      std::strcmp(typeName,"DC Mouse")==0 || std::strcmp(typeName,"CD-i Mouse")==0 ||
      std::strcmp(typeName,"PCE Mouse")==0 || std::strcmp(typeName,"3DO Mouse")==0) {
    if (std::strcmp(typeName,"3DO Mouse")==0) {
      *layout=mouse_art::three;
      *layoutCount=sizeof(mouse_art::three)/sizeof(PadButton);
      return;
    }
    if (std::strcmp(typeName,"PCE Mouse")==0) {
      *layout=mouse_art::pce;
      *layoutCount=sizeof(mouse_art::pce)/sizeof(PadButton);
      return;
    }
    const bool saturn=std::strcmp(typeName,"Saturn Mouse")==0 || std::strcmp(typeName,"Sega Mouse")==0 || std::strcmp(typeName,"DC Mouse")==0;
    *layout=saturn?mouse_art::saturn:mouse_art::snes;
    *layoutCount=saturn?sizeof(mouse_art::saturn)/sizeof(PadButton):sizeof(mouse_art::snes)/sizeof(PadButton);
    return;
  }
#endif

  if (std::strcmp(typeName, "Power Pad") == 0) {
    *layout = padLayoutPowerPad;
    *layoutCount = PAD_LAYOUT_POWERPAD_COUNT;
    return;
  }
  if (std::strcmp(typeName, "Dance Pad") == 0) {
    *layout = padLayoutDancePad;
    *layoutCount = PAD_LAYOUT_DANCEPAD_COUNT;
    return;
  }
  #ifdef ENABLE_INPUT_NES
  if (std::strcmp(typeName, "NES Pad") == 0 ||
      std::strcmp(typeName, "Four Score") == 0) {
    *layout = padLayoutNES;
    *layoutCount = PAD_LAYOUT_NES_COUNT;
    return;
  }
  #endif
  #ifdef ENABLE_INPUT_SNES
  if (std::strcmp(typeName, "SNES Pad") == 0 ||
      std::strcmp(typeName, "NTT Data") == 0) {
    *layout = padLayoutSnes;
    *layoutCount = PAD_LAYOUT_SNES_COUNT;
    return;
  }
  #endif
  #ifdef ENABLE_INPUT_VBOY
  if (std::strcmp(typeName, "VB Pad") == 0) {
    *layout = padLayoutVBoy;
    *layoutCount = PAD_LAYOUT_VBOY_COUNT;
    return;
  }
  #endif
  #ifdef ENABLE_INPUT_N64
  if (std::strncmp(typeName, "N64", 3) == 0) {
    *layout = padLayoutN64;
    *layoutCount = PAD_LAYOUT_N64_COUNT;
    return;
  }
  #endif
  #ifdef ENABLE_INPUT_GAMECUBE
  if (std::strcmp(typeName, "GC Pad") == 0 ||
      std::strcmp(typeName, "WaveBird") == 0) {
    *layout = padLayoutGC;
    *layoutCount = PAD_LAYOUT_GC_COUNT;
    return;
  }
  #endif
  #ifdef ENABLE_INPUT_WII
  if (deviceMode == RZORD_WII && std::strcmp(typeName, "Nunchuk") == 0) {
    *layout = padLayoutWiiNunchuk;
    *layoutCount = PAD_LAYOUT_WII_NUNCHUK_COUNT;
    return;
  }
  #endif
  #ifdef ENABLE_INPUT_DRIVING
  if (std::strcmp(typeName, "Driving") == 0) {
    *layout = padLayoutDriving;
    *layoutCount = PAD_LAYOUT_DRIVING_COUNT;
    return;
  }
  #endif
  #ifdef ENABLE_INPUT_DREAMCAST
  if (deviceMode == RZORD_DREAMCAST &&
      (std::strncmp(typeName, "Whl", 3) == 0 ||
       std::strcmp(typeName, "Wheel") == 0)) {
    *layout = padLayoutDreamcastWheel;
    *layoutCount = PAD_LAYOUT_DREAMCAST_WHEEL_COUNT;
    return;
  }
  #endif
  #ifdef ENABLE_INPUT_USB
  if (deviceMode == RZORD_USB &&
      getSharedControllerTypePadLayout(typeName, layout, layoutCount)) {
    return;
  }
  #endif
  #ifdef ENABLE_INPUT_MEGADRIVE
  if (std::strcmp(typeName, "Mega3") == 0) {
    *layout = padLayoutGenesis3;
    *layoutCount = PAD_LAYOUT_GENESIS3_COUNT;
    return;
  }
  if (std::strcmp(typeName, "Mega6") == 0 ||
      std::strcmp(typeName, "M30") == 0) {
    *layout = padLayoutGenesis6;
    *layoutCount = PAD_LAYOUT_GENESIS6_COUNT;
    return;
  }
  #endif
  #ifdef ENABLE_INPUT_SATURN
#if !defined(ARDUINO) || defined(PRODUCT_CLASSIC2USB)
  if (deviceMode==RZORD_SATURN &&
      (std::strcmp(typeName,"Mission")==0 || std::strcmp(typeName,"Mission6")==0)) {
    *layout=classic_pad_art::mission;
    *layoutCount=sizeof(classic_pad_art::mission)/sizeof(PadButton);
    return;
  }
#endif
  if (std::strcmp(typeName, "3D Pad") == 0) {
    *layout = padLayoutSaturn3D;
    *layoutCount = PAD_LAYOUT_SATURN3D_COUNT;
    return;
  }
  if (std::strcmp(typeName, "Saturn") == 0 || std::strcmp(typeName, "Pad") == 0) {
    bool saturnFamilyMode = deviceMode == RZORD_SATURN;
    #ifdef ENABLE_INPUT_MEGADRIVE
    saturnFamilyMode = saturnFamilyMode || deviceMode == RZORD_MEGADRIVE;
    #endif
    if (saturnFamilyMode) {
      *layout = padLayoutSaturn;
      *layoutCount = PAD_LAYOUT_SATURN_COUNT;
      return;
    }
  }
  #endif
  #ifdef ENABLE_INPUT_PCE
  if (deviceMode == RZORD_PCE && std::strcmp(typeName, "2-Button") == 0) {
    *layout = padLayoutPCE2;
    *layoutCount = PAD_LAYOUT_PCE2_COUNT;
    return;
  }
  #endif
  #ifdef ENABLE_INPUT_PSX
#if !defined(ARDUINO) || defined(PRODUCT_CLASSIC2USB)
  if (deviceMode==RZORD_PSX) {
    if (std::strcmp(typeName,"GuitarFreaks")==0) {
      *layout=padLayoutGuitarFreaks;*layoutCount=PAD_LAYOUT_GUITARFREAKS_COUNT;return;
    }
    if (std::strcmp(typeName,"Pop'n")==0) {
      *layout=padLayoutPopn;*layoutCount=PAD_LAYOUT_POPN_COUNT;return;
    }
    if (std::strcmp(typeName,"FlightStick")==0) {
      *layout=classic_pad_art::flight;*layoutCount=sizeof(classic_pad_art::flight)/sizeof(PadButton);return;
    }
    if (std::strcmp(typeName,"beatmania")==0) {
      *layout=classic_pad_art::beatmania;*layoutCount=sizeof(classic_pad_art::beatmania)/sizeof(PadButton);return;
    }
    if (std::strcmp(typeName,"GunCon")==0 || std::strcmp(typeName,"Guncon")==0) {
      *layout=classic_pad_art::gun;*layoutCount=sizeof(classic_pad_art::gun)/sizeof(PadButton);return;
    }
  }
#endif
  if (deviceMode == RZORD_PSX &&
      (std::strcmp(typeName, "DualShock") == 0 ||
       std::strcmp(typeName, "DualShock2") == 0)) {
    *layout = padLayoutPSXDualShock;
    *layoutCount = PAD_LAYOUT_PSX_DUALSHOCK_COUNT;
    return;
  }
  if (deviceMode == RZORD_PSX && std::strcmp(typeName, "neGcon") == 0) {
    *layout = padLayoutPSXNeGcon;
    *layoutCount = PAD_LAYOUT_PSX_NEGCON_COUNT;
    return;
  }
  if ((deviceMode == RZORD_PSX || deviceMode == RZORD_PSX_JOG) &&
      std::strncmp(typeName, "JogCon", 6) == 0) {
    *layout = padLayoutPSXJogcon;
    *layoutCount = PAD_LAYOUT_PSX_JOGCON_COUNT;
    return;
  }
#if defined(ADAPT_BRIDGE_CLASSIC_TO_USB)
  if (deviceMode == RZORD_PSX && std::strcmp(typeName, "Fishing") == 0) {
    *layout = fishing_pad_art::layout;
    *layoutCount = sizeof(fishing_pad_art::layout) / sizeof(PadButton);
    return;
  }
#endif
  if (deviceMode == RZORD_PSX &&
      (std::strcmp(typeName, "Digital") == 0 ||
       std::strcmp(typeName, "FlightStick") == 0 ||
       std::strcmp(typeName, "Guncon") == 0 ||
       std::strcmp(typeName, "Fishing") == 0 ||
       std::strcmp(typeName, "Spaceball") == 0)) {
    *layout = padLayoutPSXDigital;
    *layoutCount = PAD_LAYOUT_PSX_DIGITAL_COUNT;
    return;
  }
  #endif
  #ifdef ENABLE_INPUT_JAGUAR
  if (deviceMode == RZORD_JAGUAR && player < MAX_USB_OUT && jaguarRotaryActivePorts[player]) {
    *layout = padLayoutJaguarRotary;
    *layoutCount = PAD_LAYOUT_JAGUAR_ROTARY_COUNT;
    return;
  }
  #endif

  getPadLayoutForMode(deviceMode, layout, layoutCount, &layoutName);
}
