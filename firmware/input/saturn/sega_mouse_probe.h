#pragma once

// A Genesis pad actively drives TR as B/C. Identify the mouse's 0/B prefix
// with TR left as an input before allowing the mouse handshake to drive it.
// A normal pad's TH-low D3/D2 are fixed at 00, so held buttons cannot form B.
template <typename Bus>
bool passiveSegaMousePrefix(Bus& bus) {
  if (!bus.trIsInput()) return false;
  bus.th(true);
  bus.waitUs(4);
  bool valid = bus.nibble() == 0 && bus.trIsHigh();
  if (valid) {
    bus.th(false);
    bus.waitUs(4);
    valid = bus.nibble() == 0xB && bus.trIsHigh();
  }
  bus.th(true);
  bus.waitUs(4);
  return valid;
}
