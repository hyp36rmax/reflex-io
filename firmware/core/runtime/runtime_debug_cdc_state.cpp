#include "../../product_config.h"

#include "runtime_debug_cdc_state.h"

#ifdef ENABLE_RUNTIME_SERIAL_DEBUG_CDC
#if defined(ADAPT_MISTER_SERIAL_REQUEST_TAG) && defined(ADAPT_FEATURE_MISTER_SERIAL_STATUS)
// Test builds until combined display/card hardware acceptance: Main opens one
// framed session for status, artwork and card requests on this capability tag.
NamedUsbCdc runtimeDebugCdc("Reflex Adapt Control (MiSTer Serial: art=ssd1306-128x64, card=psx+n64+tpak)");
#else
// "(MiSTer Status)" lets MiSTer Main discover the port and stream status.
NamedUsbCdc runtimeDebugCdc("Reflex Adapt Control (MiSTer Status)");
#endif
bool runtimeDebugCdcEnabled = false;
#endif
