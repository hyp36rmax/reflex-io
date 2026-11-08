#pragma once

#include <stddef.h>
#include <stdint.h>

// Device side of MiSTer Main's two-way mister_serial channel, specified in
// Reflex-Nova docs/MISTER_SERIAL_REQUESTS.md. Main opens a session with a
// MISTER_STATUS/1 HELLO, sends MISTER_REQUEST/1 lines and reads
// MISTER_REPLY/1 and MISTER_DEVICE/1 lines back. This file has no Arduino
// dependency, so the protocol runs under host tests; the runtime supplies the
// port and the card backend.

namespace mister_serial {

constexpr uint8_t kMaxPorts = 2;
constexpr uint8_t kMaxSlots = 4;
// Largest payload of one CARD_READ or CARD_WRITE (the HELLO `chunk`).
constexpr uint16_t kChunkBytes = 1024;
// The largest Game Boy ROM, 8 MiB, in 32-byte blocks (TPAK_ROM_READ).
constexpr uint32_t kMaxRomBlocks = 262144;
// A reply that cannot be written within this time ends the session.
constexpr uint32_t kWriteTimeoutMs = 1000;
// CARDS records are sent at most this often, carrying the latest state.
constexpr uint32_t kCardsIntervalMs = 100;
// A pending slot that fails its probe is tried again this much later.
constexpr uint32_t kProbeRetryMs = 1000;
constexpr uint8_t kProbeAttempts = 3;

enum class PortMode : uint8_t { None, Psx, N64, Vmu };

enum class SlotState : uint8_t {
  Unknown,  // From observe(): nothing new, keep what is recorded.
  Psx,
  N64,
  Tpak,
  TpakNoCart,
  Rumble,
  Other,
  Empty,
  Pending,
  NoPad,
};

enum class CardType : uint8_t { Psx, N64, Tpak };

struct SlotObservation {
  SlotState state = SlotState::Unknown;
  uint16_t blocks = 0;   // Tpak: 32-byte blocks of the cartridge save.
  bool changed = false;  // Removed and back, re-powered, or another cartridge.
};

enum class BlockStatus : uint8_t { Ok, NoCard, Changed, Failed };

struct TpakInfo {
  bool cart = false;
  bool supported = false;
  char title[17] = {};
  char titleKey[17] = {};
  uint16_t globalChecksum = 0;
  uint8_t headerChecksum = 0;
  uint8_t cgb = 0;
  uint32_t saveSize = 0;
  uint32_t romSize = 0;  // 0: the ROM cannot be read.
};

class Port {
 public:
  virtual uint32_t nowMs() = 0;
  // DTR set and the device configured.
  virtual bool connected() = 0;
  virtual int availableForWrite() = 0;
  virtual size_t write(const uint8_t* data, size_t length) = 0;
  virtual void flushOutput() = 0;
  // Whether the last byte anything wrote to this port was LF.
  virtual bool lastByteWasLineEnd() = 0;
  // Lets USB move data while a write waits for room.
  virtual void service() = 0;
  // Drops received data and the partial line held by the line assembler.
  virtual void discardInput() = 0;
  // Stops output that could swallow or corrupt protocol lines: the binary
  // streams and the tty2oled raw drain.
  virtual void quietOutputModes() = 0;
  // Optional presentation consumer. Fields borrow the current line buffer;
  // a consumer may decode an ART field in place but cannot retain pointers.
  virtual void statusRecord(char**, uint8_t) {}
  virtual void statusDisconnected() {}

 protected:
  ~Port() = default;
};

class CardBackend {
 public:
  virtual PortMode mode(uint8_t port) = 0;
  virtual uint8_t slotCount(uint8_t port) = 0;
  // What polling already shows, without touching a card.
  virtual SlotObservation observe(uint8_t port, uint8_t slot) = 0;
  // Touches the slot once: one CARD_SCAN step, or the probe of a pending slot.
  virtual SlotObservation probe(uint8_t port, uint8_t slot) = 0;
  // Whether pending slots on this port are probed without a request.
  virtual bool probesPendingSlots(uint8_t port) = 0;
  virtual BlockStatus readBlock(CardType type, uint8_t port, uint8_t slot,
                                uint16_t block, uint8_t* data, const char** detail) = 0;
  virtual BlockStatus writeBlock(CardType type, uint8_t port, uint8_t slot,
                                 uint16_t block, const uint8_t* data, const char** detail) = 0;
  // Full cartridge initialisation. Changed when the cartridge differs from
  // the one seen last.
  virtual BlockStatus tpakInfo(uint8_t port, TpakInfo* info, const char** detail) = 0;
  // One 32-byte block of the ROM of the cartridge tpakInfo() initialised.
  // The banks it selects may stay selected until requestFinished().
  virtual BlockStatus readRomBlock(uint8_t port, uint32_t block, uint8_t* data, const char** detail) = 0;
  // Card requests cannot run now (a reboot is pending, a text write stage is
  // open, a raw drain runs).
  virtual bool busy() = 0;
  // A new session: drop text CARD write stages left by another tool.
  virtual void sessionStarted() = 0;
  // A request finished or was abandoned: drop per-request state.
  virtual void requestFinished() = 0;

 protected:
  ~CardBackend() = default;
};

enum class LineResult : uint8_t {
  Taken,
  // A request while another runs: offer the same line again later and read
  // nothing further until it is taken.
  Hold,
};

class Device {
 public:
  Device(Port& port, CardBackend* backend, const char* product, const char* firmware);

  // A complete line that starts with "MISTER_". The line may be modified.
  LineResult takeLine(char* line, size_t length);
  // Once per main-loop pass: session end, one block of a running request,
  // pending-slot probes and CARDS records.
  void task();
  bool sessionActive() const { return sessionActive_; }

 private:
  enum class Kind : uint8_t { None, Ping, Scan, Read, Write, TpakInfo, TpakRom };

  struct Slot {
    SlotState state = SlotState::Pending;
    uint16_t gen = 0;
    uint16_t blocks = 0;
    uint8_t probeAttempts = 0;
    uint32_t nextProbeMs = 0;
  };

  struct Request {
    Kind kind = Kind::None;
    uint32_t id = 0;
    CardType type = CardType::Psx;
    uint8_t port = 0;
    uint8_t slot = 0;
    uint32_t start = 0;
    uint16_t count = 0;
    uint16_t blockSize = 0;
    uint16_t done = 0;
    uint16_t gen = 0;
  };

  void handleStatus(char** fields, uint8_t count);
  void handleRequest(char** fields, uint8_t count);
  void startCardRequest(Kind kind, uint32_t id, const char* kindText, char** fields, uint8_t count);
  void step();
  void stepScan();
  void stepRead();
  void stepWrite();
  void stepTpakInfo();
  bool checkGeneration();
  void failBlock(BlockStatus status, int32_t block, const char* detail, const char* code);
  void finishRequest();
  void endSession();
  void refreshPorts();
  void apply(uint8_t port, uint8_t slot, const SlotObservation& observation);
  void probePendingSlot();

  bool beginLine();
  bool put(const char* text);
  bool put(const char* text, size_t length);
  bool putNumber(uint32_t value);
  bool putHex(uint32_t value, uint8_t digits);
  bool putBase64(const uint8_t* data, size_t length);
  bool endLine();
  bool writeFailed();

  bool beginReply(uint32_t id, const char* kind, bool ok);
  void replyError(uint32_t id, const char* kind, const char* code);
  const char* runningKindText() const;
  void replyCardError(const char* code, const char* detail, int32_t block, int32_t done, bool withGen);
  bool putCardFields();
  void writeHello();
  void writeCards();
  bool putModesAndSlots();

  Port& port_;
  CardBackend* backend_;
  const char* product_;
  const char* firmware_;

  bool sessionActive_ = false;
  bool statusActive_ = false;
  char session_[17] = {};
  bool wasConnected_ = false;
  uint32_t lineDeadlineMs_ = 0;
  bool lineFailed_ = false;

  PortMode modes_[kMaxPorts] = {};
  uint8_t slotCounts_[kMaxPorts] = {};
  Slot slots_[kMaxPorts][kMaxSlots];
  // Each port's cartridge ROM in blocks, from the TPAK_INFO under romGen_.
  uint32_t romBlocks_[kMaxPorts] = {};
  uint16_t romGen_[kMaxPorts] = {};
  bool cardsDirty_ = false;
  uint32_t lastCardsMs_ = 0;

  Request running_;
  uint8_t scanPort_ = 0;
  uint8_t scanSlot_ = 0;
  uint8_t block_[kChunkBytes] = {};
};

// zlib's crc32 (CRC-32/ISO-HDLC) over a whole buffer.
uint32_t crc32(const uint8_t* data, size_t length);
// RFC 4648 base64 without whitespace, decoded in place. Rejects characters
// outside the alphabet, a length that is not a multiple of four and padding
// before the last quad. Returns false on any of those.
bool base64DecodeInPlace(char* text, size_t* decodedLength);

}  // namespace mister_serial
