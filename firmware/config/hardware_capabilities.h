#pragma once

// The selected product owns fitted hardware. ADAPT_HAS_* means that the PCB
// can support a service; feature_gates.h decides whether it is compiled.

// Private product overlays may additionally opt into ADAPT_HAS_NFC_BRIDGE or
// ADAPT_HAS_RAIL_MONITOR. No public board currently claims either service.
// USB2Classic and Classic2Classic can opt into any hardware capability once
// their real PCB profiles exist.
