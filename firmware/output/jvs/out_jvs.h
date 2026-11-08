// JVS Output Mode - Emulates a JVS I/O board for arcade PCBs
// JVS (Japan Video Standard) is a serial protocol used by JAMMA arcade boards
//
// Hardware requirements:
//   - RS-485 transceiver (e.g., MAX485, SN75176)
//   - TX/RX pins connected to transceiver
//   - Direction control pin (active high = transmit)
//
// JVS Protocol:
//   - RS-485 at 115200 baud, 8N1
//   - Sync byte: 0xE0
//   - Escape byte: 0xD0 (followed by actual_byte + 1)
//   - Packet: SYNC + DEST + LEN + DATA... + SUM
//   - Checksum: sum of all bytes after SYNC (mod 256)
//
// This module responds to JVS commands from the arcade PCB (master)
// and reports controller inputs as a standard JVS I/O board.

#pragma once

#include <Arduino.h>

#include "../../core/controller_frame_state.h"
#include "jvs_protocol.h"
#include "jvs_runtime_state.h"

class JVSOutput {
private:
  uint8_t rx_buffer[256];
  uint8_t tx_buffer[256];
  uint16_t rx_len = 0;
  bool in_packet = false;
  bool escaped = false;

  uint16_t coin_count[JVS_NUM_COINS] = {0};

  const char* device_id = "REFLEX ADAPT 2;JVS I/O;Ver1.0";

  void sendResponse(uint8_t* data, uint8_t len);
  void sendByte(uint8_t b);
  void processPacket();

public:
  void begin();
  void update();
  void addCoin(uint8_t slot);
  uint8_t getAddress() const { return jvs_address; }
  bool isInitialized() const { return jvs_address != 0; }
  const char* getDebugStatus();
};

extern JVSOutput jvsOutput;
