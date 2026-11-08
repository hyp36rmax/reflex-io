#pragma once

// JVS Constants
#define JVS_SYNC       0xE0   // Sync byte
#define JVS_ESCAPE     0xD0   // Escape byte
#define JVS_BROADCAST  0xFF   // Broadcast address (for address assignment)
#define JVS_MASTER     0x00   // Master address (game board)

// JVS Commands (from master to slave)
#define JVS_CMD_RESET           0xF0  // Reset all devices
#define JVS_CMD_SET_ADDRESS     0xF1  // Assign address to device
#define JVS_CMD_COMM_CHANGE     0xF2  // Change communication settings
#define JVS_CMD_REQUEST_ID      0x10  // Request device ID string
#define JVS_CMD_GET_CMD_FORMAT  0x11  // Get command format version
#define JVS_CMD_GET_JVS_VERSION 0x12  // Get JVS version
#define JVS_CMD_GET_COMM_VER    0x13  // Get communication version
#define JVS_CMD_GET_FEATURES    0x14  // Get feature list
#define JVS_CMD_READ_SWITCHES   0x20  // Read switch inputs
#define JVS_CMD_READ_COINS      0x21  // Read coin counters
#define JVS_CMD_READ_ANALOG     0x22  // Read analog inputs
#define JVS_CMD_READ_ROTARY     0x23  // Read rotary encoders
#define JVS_CMD_READ_KEYPAD     0x24  // Read keypad
#define JVS_CMD_READ_LIGHTGUN   0x25  // Read lightgun position
#define JVS_CMD_READ_GPI        0x26  // Read general-purpose inputs
#define JVS_CMD_DECREASE_COIN   0x30  // Decrease coin counter
#define JVS_CMD_WRITE_GPO       0x32  // Write general-purpose outputs
#define JVS_CMD_WRITE_ANALOG    0x33  // Write analog outputs
#define JVS_CMD_WRITE_DISPLAY   0x34  // Write character display
#define JVS_CMD_COIN_ADD        0x35  // Add coin (for testing)
#define JVS_CMD_COIN_SUB        0x36  // Subtract coin

// JVS Status bytes (in response)
#define JVS_STATUS_OK           0x01  // Normal status
#define JVS_STATUS_UNKNOWN_CMD  0x02  // Unknown command
#define JVS_STATUS_SUM_ERROR    0x03  // Checksum error
#define JVS_STATUS_ACK_OVERFLOW 0x04  // ACK overflow

// JVS Report codes (per-command status)
#define JVS_REPORT_OK           0x01  // Command executed OK
#define JVS_REPORT_PARAM_ERROR  0x02  // Parameter error
#define JVS_REPORT_BUSY         0x03  // Busy

// JVS Feature codes
#define JVS_FEATURE_END         0x00  // End of feature list
#define JVS_FEATURE_SWITCH      0x01  // Switch inputs
#define JVS_FEATURE_COIN        0x02  // Coin slots
#define JVS_FEATURE_ANALOG      0x03  // Analog inputs
#define JVS_FEATURE_ROTARY      0x04  // Rotary encoders
#define JVS_FEATURE_KEYPAD      0x05  // Keypad
#define JVS_FEATURE_LIGHTGUN    0x06  // Lightgun
#define JVS_FEATURE_GPI         0x07  // General-purpose inputs
#define JVS_FEATURE_CARD        0x10  // Card reader
#define JVS_FEATURE_MEDAL       0x11  // Medal hopper
#define JVS_FEATURE_GPO         0x12  // General-purpose outputs
#define JVS_FEATURE_ANALOG_OUT  0x13  // Analog outputs
#define JVS_FEATURE_DISPLAY     0x14  // Character display
#define JVS_FEATURE_BACKUP      0x15  // Backup memory

// Hardware configuration
// Using UART0 - pins must support UART function
// Default pins use HDMI_2 connector pins 8-10 (3.3V with pull-ups)
// These are suitable for 3.3V RS-485 transceivers (MAX3485, SN65HVD3082, etc.)
#ifndef JVS_UART
#define JVS_UART Serial1  // UART0 on RP2040 (GPIO 16/17 are UART0 alternate pins)
#endif

#ifndef JVS_TX_PIN
  #ifdef PIN_JVS_TX
    #define JVS_TX_PIN PIN_JVS_TX
  #else
    #define JVS_TX_PIN 16  // HDMI_2_08 - UART0 TX to RS-485 DI
  #endif
#endif

#ifndef JVS_RX_PIN
  #ifdef PIN_JVS_RX
    #define JVS_RX_PIN PIN_JVS_RX
  #else
    #define JVS_RX_PIN 17  // HDMI_2_09 - UART0 RX from RS-485 RO
  #endif
#endif

#ifndef JVS_DE_PIN
  #ifdef PIN_JVS_DE
    #define JVS_DE_PIN PIN_JVS_DE
  #else
    #define JVS_DE_PIN 14  // HDMI_2_10 - RS-485 direction control (DE/RE tied together)
  #endif
#endif

// JVS I/O board configuration
#define JVS_NUM_PLAYERS    2   // Number of player panels
#define JVS_NUM_SWITCHES   16  // Switches per player (2 bytes)
#define JVS_NUM_COINS      2   // Coin slots
#define JVS_NUM_ANALOG     8   // Analog inputs (4 per player: LX, LY, RX, RY)
