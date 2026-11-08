#include "controller_delivery_state.h"

bool controller_frame_needs_delivery[MAX_USB_OUT] = { false };
bool controller_frame_delivery_deferred = false;
