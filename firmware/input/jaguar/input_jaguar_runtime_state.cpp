#include "../../product_config.h"

#include "input_jaguar_runtime_state.h"

bool jaguarRotaryActive = false;
bool jaguarRotaryActivePorts[MAX_USB_OUT] = { false };
uint8_t jaguarRotaryDisplayPhase[MAX_USB_OUT] = {};
RotaryDivider jaguarRotaryDivider[MAX_USB_OUT] = {};
bool jaguarSpinnerOutputWasActive = false;
JaguarRotaryQuadrature jaguarRotaryQuadrature[MAX_USB_OUT] = {};

void resetJaguarRotaryRuntimeState() {
  jaguarRotaryActive = false;
  jaguarSpinnerOutputWasActive = false;
  for (uint8_t i = 0; i < MAX_USB_OUT; ++i) {
    jaguarRotaryActivePorts[i] = false;
    jaguarRotaryDisplayPhase[i] = 0;
    jaguarRotaryDivider[i].reset();
    jaguarRotaryQuadrature[i].reset();
  }
  resetAllRotaryMotion();
}
