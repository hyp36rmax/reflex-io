#include "out_jvs.h"

volatile uint16_t jvs_packets_rx = 0;
volatile uint16_t jvs_packets_tx = 0;
volatile uint16_t jvs_errors = 0;
volatile uint8_t jvs_last_cmd = 0;
volatile uint8_t jvs_address = 0;  // 0 = not assigned

void JVSOutput::sendResponse(uint8_t* data, uint8_t len) {
  digitalWrite(JVS_DE_PIN, HIGH);
  delayMicroseconds(10);

  uint8_t sum = JVS_MASTER + (len + 1);
  for (uint8_t i = 0; i < len; i++) {
    sum += data[i];
  }

  JVS_UART.write(JVS_SYNC);
  sendByte(JVS_MASTER);
  sendByte(len + 1);

  for (uint8_t i = 0; i < len; i++) {
    sendByte(data[i]);
  }

  sendByte(sum);
  JVS_UART.flush();

  delayMicroseconds(10);
  digitalWrite(JVS_DE_PIN, LOW);

  jvs_packets_tx++;
}

void JVSOutput::sendByte(uint8_t b) {
  if (b == JVS_SYNC || b == JVS_ESCAPE) {
    JVS_UART.write(JVS_ESCAPE);
    JVS_UART.write(b - 1);
  } else {
    JVS_UART.write(b);
  }
}

void JVSOutput::processPacket() {
  if (rx_len < 2)
    return;

  uint8_t dest = rx_buffer[0];
  uint8_t pkt_len = rx_buffer[1];

  uint8_t sum = 0;
  for (uint8_t i = 0; i < rx_len - 1; i++) {
    sum += rx_buffer[i];
  }
  if (sum != rx_buffer[rx_len - 1]) {
    jvs_errors++;
    return;
  }

  if (dest != jvs_address && dest != JVS_BROADCAST) {
    return;
  }

  jvs_packets_rx++;

  uint8_t cmd_offset = 2;
  uint8_t resp_len = 0;
  tx_buffer[resp_len++] = JVS_STATUS_OK;

  while (cmd_offset < pkt_len + 1) {
    uint8_t cmd = rx_buffer[cmd_offset++];
    jvs_last_cmd = cmd;

    switch (cmd) {
      case JVS_CMD_RESET:
        jvs_address = 0;
        return;

      case JVS_CMD_SET_ADDRESS:
        if (jvs_address == 0 && cmd_offset < rx_len) {
          jvs_address = rx_buffer[cmd_offset++];
          tx_buffer[resp_len++] = JVS_REPORT_OK;
        }
        break;

      case JVS_CMD_REQUEST_ID:
        tx_buffer[resp_len++] = JVS_REPORT_OK;
        for (const char* p = device_id; *p; p++) {
          tx_buffer[resp_len++] = *p;
        }
        tx_buffer[resp_len++] = 0;
        break;

      case JVS_CMD_GET_CMD_FORMAT:
        tx_buffer[resp_len++] = JVS_REPORT_OK;
        tx_buffer[resp_len++] = 0x11;
        break;

      case JVS_CMD_GET_JVS_VERSION:
        tx_buffer[resp_len++] = JVS_REPORT_OK;
        tx_buffer[resp_len++] = 0x20;
        break;

      case JVS_CMD_GET_COMM_VER:
        tx_buffer[resp_len++] = JVS_REPORT_OK;
        tx_buffer[resp_len++] = 0x10;
        break;

      case JVS_CMD_GET_FEATURES:
        tx_buffer[resp_len++] = JVS_REPORT_OK;
        tx_buffer[resp_len++] = JVS_FEATURE_SWITCH;
        tx_buffer[resp_len++] = JVS_NUM_PLAYERS;
        tx_buffer[resp_len++] = JVS_NUM_SWITCHES;
        tx_buffer[resp_len++] = 0;
        tx_buffer[resp_len++] = JVS_FEATURE_COIN;
        tx_buffer[resp_len++] = JVS_NUM_COINS;
        tx_buffer[resp_len++] = 0;
        tx_buffer[resp_len++] = 0;
        tx_buffer[resp_len++] = JVS_FEATURE_ANALOG;
        tx_buffer[resp_len++] = JVS_NUM_ANALOG;
        tx_buffer[resp_len++] = 8;
        tx_buffer[resp_len++] = 0;
        tx_buffer[resp_len++] = JVS_FEATURE_END;
        break;

      case JVS_CMD_READ_SWITCHES: {
        uint8_t num_players = rx_buffer[cmd_offset++];
        uint8_t bytes_per_player = rx_buffer[cmd_offset++];
        tx_buffer[resp_len++] = JVS_REPORT_OK;
        tx_buffer[resp_len++] = 0x00;
        for (uint8_t p = 0; p < num_players && p < JVS_NUM_PLAYERS && p < max_devices; p++) {
          const controller_state_t& frame = controllerFrameConst(p);
          uint16_t switches = 0xFFFF;

          if (frame.connected) {
            if (frame.START)  switches &= ~(1 << 15);
            if (frame.PAD_U)  switches &= ~(1 << 13);
            if (frame.PAD_D)  switches &= ~(1 << 12);
            if (frame.PAD_L)  switches &= ~(1 << 11);
            if (frame.PAD_R)  switches &= ~(1 << 10);
            if (frame.A)      switches &= ~(1 << 9);
            if (frame.B)      switches &= ~(1 << 8);
            if (frame.X)      switches &= ~(1 << 7);
            if (frame.Y)      switches &= ~(1 << 6);
            if (frame.L1)     switches &= ~(1 << 5);
            if (frame.R1)     switches &= ~(1 << 4);
            if (frame.L2)     switches &= ~(1 << 3);
            if (frame.R2)     switches &= ~(1 << 2);
          }

          tx_buffer[resp_len++] = (switches >> 8) & 0xFF;
          if (bytes_per_player > 1) {
            tx_buffer[resp_len++] = switches & 0xFF;
          }
        }
        break;
      }

      case JVS_CMD_READ_COINS: {
        uint8_t slots = rx_buffer[cmd_offset++];
        tx_buffer[resp_len++] = JVS_REPORT_OK;
        for (uint8_t s = 0; s < slots && s < JVS_NUM_COINS; s++) {
          tx_buffer[resp_len++] = 0x00;
          tx_buffer[resp_len++] = coin_count[s] & 0xFF;
        }
        break;
      }

      case JVS_CMD_READ_ANALOG: {
        uint8_t channels = rx_buffer[cmd_offset++];
        tx_buffer[resp_len++] = JVS_REPORT_OK;
        for (uint8_t ch = 0; ch < channels && ch < JVS_NUM_ANALOG; ch++) {
          uint16_t value = 0x8000;
          uint8_t player = ch / 4;
          uint8_t axis = ch % 4;

          if (player < max_devices && controllerFrameConst(player).connected) {
            const controller_state_t& frame = controllerFrameConst(player);
            int16_t raw = 0;
            switch (axis) {
              case 0: raw = frame.LX; break;
              case 1: raw = frame.LY; break;
              case 2: raw = frame.RX; break;
              case 3: raw = frame.RY; break;
            }
            value = (uint16_t)(raw + 32768);
          }

          tx_buffer[resp_len++] = (value >> 8) & 0xFF;
          tx_buffer[resp_len++] = value & 0xFF;
        }
        break;
      }

      case JVS_CMD_DECREASE_COIN: {
        uint8_t slot = rx_buffer[cmd_offset++];
        uint8_t amount = rx_buffer[cmd_offset++];
        tx_buffer[resp_len++] = JVS_REPORT_OK;
        if (slot < JVS_NUM_COINS && coin_count[slot] >= amount) {
          coin_count[slot] -= amount;
        }
        break;
      }

      default:
        tx_buffer[resp_len++] = JVS_REPORT_PARAM_ERROR;
        break;
    }
  }

  if (jvs_address != 0) {
    sendResponse(tx_buffer, resp_len);
  }
}

void JVSOutput::begin() {
  pinMode(JVS_DE_PIN, OUTPUT);
  digitalWrite(JVS_DE_PIN, LOW);

  JVS_UART.setTX(JVS_TX_PIN);
  JVS_UART.setRX(JVS_RX_PIN);
  JVS_UART.begin(115200);

  jvs_address = 0;
  rx_len = 0;
  in_packet = false;
}

void JVSOutput::update() {
  while (JVS_UART.available()) {
    uint8_t b = JVS_UART.read();

    if (b == JVS_SYNC) {
      rx_len = 0;
      in_packet = true;
      escaped = false;
      continue;
    }

    if (!in_packet)
      continue;

    if (b == JVS_ESCAPE) {
      escaped = true;
      continue;
    }

    if (escaped) {
      b = b + 1;
      escaped = false;
    }

    if (rx_len < sizeof(rx_buffer)) {
      rx_buffer[rx_len++] = b;
    }

    if (rx_len >= 2) {
      uint8_t pkt_len = rx_buffer[1];
      if (rx_len >= pkt_len + 2) {
        processPacket();
        in_packet = false;
      }
    }
  }
}

void JVSOutput::addCoin(uint8_t slot) {
  if (slot < JVS_NUM_COINS) {
    coin_count[slot]++;
  }
}

const char* JVSOutput::getDebugStatus() {
  static char buf[32];
  snprintf(buf, sizeof(buf), "A%d R%u T%u E%u",
    jvs_address, jvs_packets_rx % 1000, jvs_packets_tx % 1000, jvs_errors % 100);
  return buf;
}

JVSOutput jvsOutput;
