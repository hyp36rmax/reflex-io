#pragma once

#include <stdint.h>

#include "../../product_config.h"
#include "../../firmware_platform_config.h"

struct PceInputDebugSlot {
  uint8_t stable_type;
  uint8_t observed_type;
  uint8_t pending_type;
  uint16_t pending_count;
  uint8_t source_port;
  uint8_t source_index;
};

// Sweeps of a recognised direct mouse whose CLR-high read was not 0000.
struct PceMouseDebugPort {
  uint16_t raw_misses;
  uint16_t open_sweeps;  // of those, sweeps that read as an unplugged bus
  uint8_t last_raw;
  uint8_t last_data[4];
  uint8_t last_buttons[4];
};
constexpr uint8_t kPceDebugMousePorts = 2;

struct PceInputDebugSnapshot {
  uint8_t slot_count;
  PceInputDebugSlot slots[MAX_USB_OUT];
  PceMouseDebugPort mouse[kPceDebugMousePorts];
};

void input_pce_debug_record_mouse_miss(uint8_t port, uint8_t raw, const uint8_t data[4],
                                       const uint8_t buttons[4], bool open_bus);

// PC Engine mouse tuning and raw capture, RAM only: serial PCEM DELAY <us>,
// PCEM GAIN <n>, PCEM CAPTURE, PCEM DUMP.
uint8_t input_pce_mouse_settle_us();
uint8_t input_pce_mouse_gain();
void input_pce_mouse_capture(uint8_t port, uint8_t raw, const uint8_t data[4],
                             const uint8_t buttons[4]);
class Print;
bool input_pce_handle_mouse_serial_command(char* args, Print& out);

void input_pce_debug_update_slot(uint8_t slot, uint8_t stable_type,
                                 uint8_t observed_type, uint8_t pending_type,
                                 uint16_t pending_count, uint8_t source_port,
                                 uint8_t source_index);
void input_pce_debug_clear_slot(uint8_t slot);
PceInputDebugSnapshot input_pce_debug_snapshot();
