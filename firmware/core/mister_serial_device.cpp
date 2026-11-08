#include "mister_serial_device.h"

#include <string.h>

namespace mister_serial {
namespace {

constexpr char kStatusHeader[] = "MISTER_STATUS/1";
constexpr char kRequestHeader[] = "MISTER_REQUEST/1";
constexpr uint8_t kMaxFields = 24;
constexpr uint8_t kMaxKindLength = 32;
constexpr uint16_t kPsxFrames = 1024;
constexpr uint16_t kN64Pages = 128;
constexpr uint16_t kTpakBlockSize = 32;
constexpr char kBase64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

uint8_t splitFields(char* line, char** fields) {
  uint8_t count = 0;
  fields[count++] = line;
  for (char* p = line; *p != '\0'; ++p) {
    if (*p == '\t') {
      *p = '\0';
      if (count < kMaxFields) {
        fields[count++] = p + 1;
      }
    }
  }
  return count;
}

char* findValue(char** fields, uint8_t count, uint8_t first, const char* key) {
  const size_t keyLength = strlen(key);
  for (uint8_t i = first; i < count; ++i) {
    if (strncmp(fields[i], key, keyLength) == 0 && fields[i][keyLength] == '=') {
      return fields[i] + keyLength + 1;
    }
  }
  return nullptr;
}

bool isDigit(char c) { return c >= '0' && c <= '9'; }

// 1 to 4294967295 in decimal, without a leading zero.
bool parseId(const char* text, uint32_t* value) {
  if (text == nullptr || *text < '1' || *text > '9') {
    return false;
  }
  uint64_t parsed = 0;
  size_t length = 0;
  for (; text[length] != '\0'; ++length) {
    if (!isDigit(text[length]) || length >= 10) {
      return false;
    }
    parsed = parsed * 10u + (uint64_t)(text[length] - '0');
  }
  if (parsed > 0xFFFFFFFFull) {
    return false;
  }
  *value = (uint32_t)parsed;
  return true;
}

// "0", or up to six digits without a leading zero, at most `maximum`.
bool parseNumber(const char* text, uint32_t maximum, uint32_t* value) {
  if (text == nullptr || !isDigit(*text) || (text[0] == '0' && text[1] != '\0')) {
    return false;
  }
  uint32_t parsed = 0;
  size_t length = 0;
  for (; text[length] != '\0'; ++length) {
    if (!isDigit(text[length]) || length >= 6) {
      return false;
    }
    parsed = parsed * 10u + (uint32_t)(text[length] - '0');
  }
  if (parsed > maximum) {
    return false;
  }
  *value = parsed;
  return true;
}

int8_t hexValue(char c) {
  if (c >= '0' && c <= '9') return (int8_t)(c - '0');
  if (c >= 'A' && c <= 'F') return (int8_t)(c - 'A' + 10);
  if (c >= 'a' && c <= 'f') return (int8_t)(c - 'a' + 10);
  return -1;
}

bool parseCrc(const char* text, uint32_t* value) {
  if (text == nullptr || strlen(text) != 8) {
    return false;
  }
  uint32_t parsed = 0;
  for (uint8_t i = 0; i < 8; ++i) {
    const int8_t nibble = hexValue(text[i]);
    if (nibble < 0) {
      return false;
    }
    parsed = (parsed << 4) | (uint32_t)nibble;
  }
  *value = parsed;
  return true;
}

bool validSession(const char* text) {
  if (text == nullptr || strlen(text) != 16) {
    return false;
  }
  for (uint8_t i = 0; i < 16; ++i) {
    if (hexValue(text[i]) < 0) {
      return false;
    }
  }
  return true;
}

bool validKind(const char* text) {
  const size_t length = strlen(text);
  if (length == 0 || length > kMaxKindLength) {
    return false;
  }
  for (size_t i = 0; i < length; ++i) {
    const char c = text[i];
    if (!((c >= 'A' && c <= 'Z') || isDigit(c) || c == '_')) {
      return false;
    }
  }
  return true;
}

bool parseType(const char* text, CardType* type) {
  if (text == nullptr) return false;
  if (strcmp(text, "psx") == 0) { *type = CardType::Psx; return true; }
  if (strcmp(text, "n64") == 0) { *type = CardType::N64; return true; }
  if (strcmp(text, "tpak") == 0) { *type = CardType::Tpak; return true; }
  return false;
}

const char* typeText(CardType type) {
  switch (type) {
    case CardType::Psx: return "psx";
    case CardType::N64: return "n64";
    default: return "tpak";
  }
}

uint16_t blockSizeFor(CardType type) {
  switch (type) {
    case CardType::Psx: return 128;
    case CardType::N64: return 256;
    default: return kTpakBlockSize;
  }
}

const char* modeText(PortMode mode) {
  switch (mode) {
    case PortMode::Psx: return "psx";
    case PortMode::N64: return "n64";
    case PortMode::Vmu: return "vmu";
    default: return "none";
  }
}

const char* stateText(SlotState state) {
  switch (state) {
    case SlotState::Psx: return "psx";
    case SlotState::N64: return "n64";
    case SlotState::Tpak: return "tpak";
    case SlotState::TpakNoCart: return "tpak-nocart";
    case SlotState::Rumble: return "rumble";
    case SlotState::Other: return "other";
    case SlotState::Empty: return "empty";
    case SlotState::NoPad: return "nopad";
    default: return "pending";
  }
}

// An accessory polling cannot tell apart; "present" from polling keeps it.
bool identifiedAccessory(SlotState state) {
  return state == SlotState::N64 || state == SlotState::Tpak ||
         state == SlotState::TpakNoCart || state == SlotState::Other ||
         state == SlotState::Pending;
}

bool cardPort(PortMode mode) {
  return mode == PortMode::Psx || mode == PortMode::N64;
}

}  // namespace

uint32_t crc32(const uint8_t* data, size_t length) {
  uint32_t crc = 0xFFFFFFFFu;
  while (length-- > 0) {
    crc ^= *data++;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
  }
  return ~crc;
}

bool base64DecodeInPlace(char* text, size_t* decodedLength) {
  const size_t length = strlen(text);
  if (length == 0 || (length % 4) != 0) {
    return false;
  }
  size_t out = 0;
  for (size_t i = 0; i < length; i += 4) {
    const bool lastQuad = i + 4 == length;
    uint32_t bits = 0;
    uint8_t padding = 0;
    for (uint8_t k = 0; k < 4; ++k) {
      const char c = text[i + k];
      int8_t value = -1;
      if (c == '=') {
        if (!lastQuad || k < 2) return false;
        ++padding;
        value = 0;
      } else {
        if (padding != 0) return false;
        if (c >= 'A' && c <= 'Z') value = (int8_t)(c - 'A');
        else if (c >= 'a' && c <= 'z') value = (int8_t)(c - 'a' + 26);
        else if (isDigit(c)) value = (int8_t)(c - '0' + 52);
        else if (c == '+') value = 62;
        else if (c == '/') value = 63;
        else return false;
      }
      bits = (bits << 6) | (uint32_t)value;
    }
    // Output never overtakes input: three bytes are written for four read.
    text[out++] = (char)(bits >> 16);
    if (padding < 2) text[out++] = (char)((bits >> 8) & 0xFF);
    if (padding < 1) text[out++] = (char)(bits & 0xFF);
  }
  *decodedLength = out;
  return true;
}

Device::Device(Port& port, CardBackend* backend, const char* product, const char* firmware)
    : port_(port), backend_(backend), product_(product), firmware_(firmware) {}

LineResult Device::takeLine(char* line, size_t length) {
  (void)length;
  char* fields[kMaxFields];
  if (strncmp(line, kRequestHeader, sizeof(kRequestHeader) - 1) == 0 &&
      line[sizeof(kRequestHeader) - 1] == '\t') {
    if (running_.kind != Kind::None) {
      return LineResult::Hold;
    }
    handleRequest(fields, splitFields(line, fields));
    return LineResult::Taken;
  }
  if (strncmp(line, kStatusHeader, sizeof(kStatusHeader) - 1) == 0 &&
      line[sizeof(kStatusHeader) - 1] == '\t') {
    handleStatus(fields, splitFields(line, fields));
  }
  // MISTER_COMMAND/1 is for the accessory; other headers are future ones.
  return LineResult::Taken;
}

void Device::handleStatus(char** fields, uint8_t count) {
  if (count < 3 || !port_.connected()) return;
  if (strcmp(fields[2], "HELLO") != 0) {
    if (statusActive_) port_.statusRecord(fields, count);
    return;
  }
  const char* session = findValue(fields, count, 3, "session");
  uint32_t sequence = 0;
  if (!parseId(fields[1], &sequence)) return;
  if (session != nullptr && !validSession(session)) return;
  // Legacy status-only Main has no card session. Its HELLO still authorizes
  // presentation, but cannot authorize card requests or produce card replies.
  if (session == nullptr && sessionActive_) endSession();
  statusActive_ = true;
  port_.statusRecord(fields, count);
  if (session == nullptr) return;
  if (!sessionActive_ || strcmp(session_, session) != 0) {
    // A new session abandons a running request without a reply.
    finishRequest();
    port_.quietOutputModes();
    if (backend_ != nullptr) {
      backend_->sessionStarted();
    }
    memcpy(session_, session, sizeof(session_) - 1);
    session_[sizeof(session_) - 1] = '\0';
    sessionActive_ = true;
  }
  refreshPorts();
  writeHello();
  writeCards();
}

void Device::handleRequest(char** fields, uint8_t count) {
  uint32_t id = 0;
  if (count < 3 || !parseId(fields[1], &id)) {
    return;  // No readable id, no reply: Main's deadline covers it.
  }
  const char* kindText = fields[2];
  const char* session = findValue(fields, count, 3, "session");
  if (!sessionActive_ || session == nullptr || strcmp(session, session_) != 0) {
    replyError(id, kindText, "session");
    return;
  }
  if (!validKind(kindText)) {
    replyError(id, kindText, "bad_request");
    return;
  }
  Kind kind = Kind::None;
  if (strcmp(kindText, "PING") == 0) kind = Kind::Ping;
  else if (strcmp(kindText, "CARD_SCAN") == 0) kind = Kind::Scan;
  else if (strcmp(kindText, "CARD_READ") == 0) kind = Kind::Read;
  else if (strcmp(kindText, "CARD_WRITE") == 0) kind = Kind::Write;
  else if (strcmp(kindText, "TPAK_INFO") == 0) kind = Kind::TpakInfo;
  else if (strcmp(kindText, "TPAK_ROM_READ") == 0) kind = Kind::TpakRom;

  if (kind == Kind::Ping) {
    if (beginReply(id, kindText, true)) {
      endLine();
    }
    return;
  }
  if (kind == Kind::None || backend_ == nullptr) {
    replyError(id, kindText, "unknown");
    return;
  }
  if (backend_->busy()) {
    replyError(id, kindText, "busy");
    return;
  }
  startCardRequest(kind, id, kindText, fields, count);
}

void Device::startCardRequest(Kind kind, uint32_t id, const char* kindText,
                              char** fields, uint8_t count) {
  refreshPorts();
  Request request;
  request.kind = kind;
  request.id = id;
  if (kind == Kind::Scan) {
    scanPort_ = 0;
    scanSlot_ = 0;
    running_ = request;
    return;
  }

  uint32_t port = 0;
  uint32_t slot = 0;
  uint32_t start = 0;
  uint32_t blockCount = 1;
  uint32_t gen = 0;
  bool valid = parseNumber(findValue(fields, count, 3, "port"), kMaxPorts - 1, &port) &&
               parseNumber(findValue(fields, count, 3, "gen"), 0xFFFF, &gen);
  if (kind == Kind::TpakInfo) {
    request.type = CardType::Tpak;
  } else if (kind == Kind::TpakRom) {
    // The cartridge's ROM: no type or slot, and a start past 65535.
    request.type = CardType::Tpak;
    valid = valid &&
            parseNumber(findValue(fields, count, 3, "start"), kMaxRomBlocks - 1, &start) &&
            parseNumber(findValue(fields, count, 3, "count"), 0xFFFF, &blockCount) &&
            blockCount > 0;
  } else {
    valid = valid &&
            parseType(findValue(fields, count, 3, "type"), &request.type) &&
            parseNumber(findValue(fields, count, 3, "slot"), kMaxSlots - 1, &slot) &&
            parseNumber(findValue(fields, count, 3, "start"), 0xFFFF, &start) &&
            parseNumber(findValue(fields, count, 3, "count"), 0xFFFF, &blockCount) &&
            blockCount > 0;
  }
  if (!valid) {
    replyError(id, kindText, "bad_request");
    return;
  }
  request.port = (uint8_t)port;
  request.slot = (uint8_t)slot;
  request.gen = (uint16_t)gen;

  const PortMode mode = modes_[port];
  if (!cardPort(mode)) {
    replyError(id, kindText, "no_backend");
    return;
  }
  if ((request.type == CardType::Psx) != (mode == PortMode::Psx)) {
    replyError(id, kindText, "type");
    return;
  }
  if (slot >= slotCounts_[port]) {
    replyError(id, kindText, "bad_request");
    return;
  }
  const Slot& current = slots_[port][slot];
  if (gen != current.gen) {
    running_ = request;
    replyCardError("changed", nullptr, -1, -1, true);
    finishRequest();
    return;
  }

  const SlotState state = current.state;
  const char* refusal = "no_card";
  bool accepted = false;
  switch (request.type) {
    case CardType::Psx:
      accepted = state == SlotState::Psx;
      break;
    case CardType::N64:
      accepted = state == SlotState::N64;
      if (state == SlotState::Tpak || state == SlotState::TpakNoCart) refusal = "type";
      break;
    case CardType::Tpak:
      // A cartridge whose save cannot be used still has a header and a ROM.
      accepted = state == SlotState::Tpak ||
                 ((kind == Kind::TpakInfo || kind == Kind::TpakRom) && state == SlotState::TpakNoCart);
      if (state == SlotState::N64) refusal = "type";
      break;
  }
  if (!accepted) {
    replyError(id, kindText, refusal);
    return;
  }
  if (kind == Kind::TpakInfo) {
    running_ = request;
    return;
  }

  request.blockSize = blockSizeFor(request.type);
  const uint32_t slotBlocks = kind == Kind::TpakRom ? (romGen_[port] == current.gen ? romBlocks_[port] : 0) :
                              request.type == CardType::Psx ? kPsxFrames :
                              request.type == CardType::N64 ? kN64Pages : current.blocks;
  if (start + blockCount > slotBlocks ||
      blockCount * request.blockSize > kChunkBytes) {
    replyError(id, kindText, "bad_request");
    return;
  }
  request.start = start;
  request.count = (uint16_t)blockCount;

  if (kind == Kind::Write) {
    uint32_t expectedCrc = 0;
    char* data = findValue(fields, count, 3, "data");
    size_t length = 0;
    if (!parseCrc(findValue(fields, count, 3, "crc"), &expectedCrc) || data == nullptr ||
        !base64DecodeInPlace(data, &length) ||
        length != (size_t)blockCount * request.blockSize) {
      replyError(id, kindText, "bad_request");
      return;
    }
    if (crc32(reinterpret_cast<const uint8_t*>(data), length) != expectedCrc) {
      replyError(id, kindText, "crc");
      return;
    }
    // The line buffer is free for status lines while the blocks are written.
    memcpy(block_, data, length);
  }
  running_ = request;
}

void Device::task() {
  const bool connected = port_.connected();
  if (wasConnected_ && !connected) {
    // DTR dropped, or USB reset or unmounted: the session is over. The block
    // already on the bus has finished; no further block starts, no reply.
    endSession();
    port_.discardInput();
    port_.quietOutputModes();
  }
  wasConnected_ = connected;
  if (!sessionActive_ || backend_ == nullptr) {
    return;
  }
  refreshPorts();
  if (running_.kind != Kind::None) {
    step();
  } else {
    probePendingSlot();
  }
  if (sessionActive_ && cardsDirty_ &&
      (int32_t)(port_.nowMs() - lastCardsMs_) >= (int32_t)kCardsIntervalMs) {
    writeCards();
  }
}

void Device::step() {
  switch (running_.kind) {
    case Kind::Scan: stepScan(); break;
    case Kind::Read:
    case Kind::TpakRom: stepRead(); break;
    case Kind::Write: stepWrite(); break;
    case Kind::TpakInfo: stepTpakInfo(); break;
    default: finishRequest(); break;
  }
}

void Device::stepScan() {
  while (scanPort_ < kMaxPorts) {
    if (cardPort(modes_[scanPort_]) && scanSlot_ < slotCounts_[scanPort_]) {
      apply(scanPort_, scanSlot_, backend_->probe(scanPort_, scanSlot_));
      ++scanSlot_;
      return;  // One probe per main-loop pass.
    }
    ++scanPort_;
    scanSlot_ = 0;
  }
  if (beginReply(running_.id, "CARD_SCAN", true) && put("\t") && putModesAndSlots()) {
    endLine();
  }
  finishRequest();
}

bool Device::checkGeneration() {
  if (slots_[running_.port][running_.slot].gen == running_.gen) {
    return true;
  }
  replyCardError("changed", nullptr, -1, running_.kind == Kind::Write ? running_.done : -1, true);
  finishRequest();
  return false;
}

void Device::failBlock(BlockStatus status, int32_t block, const char* detail, const char* code) {
  const int32_t done = running_.kind == Kind::Write ? running_.done : -1;
  if (status == BlockStatus::NoCard) {
    SlotObservation gone;
    gone.state = running_.type == CardType::Tpak ? SlotState::TpakNoCart : SlotState::Empty;
    apply(running_.port, running_.slot, gone);
    replyCardError("no_card", detail, -1, done, false);
  } else if (status == BlockStatus::Changed) {
    SlotObservation changed;
    changed.changed = true;
    apply(running_.port, running_.slot, changed);
    replyCardError("changed", detail, -1, done, true);
  } else {
    replyCardError(code, detail, block, done, false);
  }
  finishRequest();
}

void Device::stepRead() {
  if (!checkGeneration()) {
    return;
  }
  const uint32_t block = running_.start + running_.done;
  const char* detail = nullptr;
  uint8_t* data = block_ + (size_t)running_.done * running_.blockSize;
  const BlockStatus status =
      running_.kind == Kind::TpakRom
          ? backend_->readRomBlock(running_.port, block, data, &detail)
          : backend_->readBlock(running_.type, running_.port, running_.slot, (uint16_t)block, data, &detail);
  if (status != BlockStatus::Ok) {
    failBlock(status, block, detail, "read");
    return;
  }
  if (++running_.done < running_.count) {
    return;
  }
  const size_t length = (size_t)running_.count * running_.blockSize;
  if (beginReply(running_.id, runningKindText(), true) && putCardFields() &&
      put("\tcrc=") && putHex(crc32(block_, length), 8) &&
      put("\tdata=") && putBase64(block_, length)) {
    endLine();
  }
  finishRequest();
}

void Device::stepWrite() {
  if (!checkGeneration()) {
    return;
  }
  const uint16_t block = (uint16_t)(running_.start + running_.done);
  const char* detail = nullptr;
  const uint8_t* data = block_ + (size_t)running_.done * running_.blockSize;
  const BlockStatus status = backend_->writeBlock(running_.type, running_.port, running_.slot,
                                                  block, data, &detail);
  if (status != BlockStatus::Ok) {
    failBlock(status, block, detail, "write");
    return;
  }
  if (++running_.done < running_.count) {
    return;
  }
  if (beginReply(running_.id, "CARD_WRITE", true) && putCardFields() &&
      put("\tdone=") && putNumber(running_.done)) {
    endLine();
  }
  finishRequest();
}

void Device::stepTpakInfo() {
  if (!checkGeneration()) {
    return;
  }
  TpakInfo info;
  const char* detail = nullptr;
  const BlockStatus status = backend_->tpakInfo(running_.port, &info, &detail);
  if (status == BlockStatus::NoCard || status == BlockStatus::Failed) {
    failBlock(status, -1, detail, "read");
    return;
  }
  SlotObservation seen;
  seen.state = info.cart && info.supported && info.saveSize >= kTpakBlockSize
                   ? SlotState::Tpak : SlotState::TpakNoCart;
  seen.blocks = seen.state == SlotState::Tpak ? (uint16_t)(info.saveSize / kTpakBlockSize) : 0;
  seen.changed = status == BlockStatus::Changed;
  apply(running_.port, running_.slot, seen);
  if (!checkGeneration()) {
    return;
  }
  romBlocks_[running_.port] = info.romSize / kTpakBlockSize;
  romGen_[running_.port] = slots_[running_.port][running_.slot].gen;
  char title[sizeof(info.title)];
  size_t titleLength = 0;
  for (; titleLength < sizeof(title) - 1 && info.title[titleLength] != '\0'; ++titleLength) {
    const char c = info.title[titleLength];
    title[titleLength] = (c >= 0x20 && c <= 0x7E) ? c : '_';
  }
  title[titleLength] = '\0';
  char titleKey[sizeof(info.titleKey)];
  size_t keyLength = 0;
  for (; keyLength < sizeof(titleKey) - 1 && info.titleKey[keyLength] != '\0'; ++keyLength) {
    const char c = info.titleKey[keyLength];
    titleKey[keyLength] = ((c >= 'A' && c <= 'Z') || isDigit(c)) ? c : '_';
  }
  titleKey[keyLength] = '\0';
  if (beginReply(running_.id, "TPAK_INFO", true) &&
      put("\tport=") && putNumber(running_.port) &&
      put("\tgen=") && putNumber(slots_[running_.port][running_.slot].gen) &&
      put("\tcart=") && put(info.cart ? "1" : "0") &&
      put("\tsupported=") && put(info.supported ? "1" : "0") &&
      put("\ttitle=") && put(title) &&
      put("\ttitle_key=") && put(titleKey) &&
      put("\tglobal_csum=") && putHex(info.globalChecksum, 4) &&
      put("\theader_csum=") && putHex(info.headerChecksum, 2) &&
      put("\tcgb=") && putHex(info.cgb, 2) &&
      put("\tsave_size=") && putNumber(info.saveSize) &&
      put("\trom_size=") && putNumber(info.romSize)) {
    endLine();
  }
  finishRequest();
}

void Device::finishRequest() {
  if (running_.kind == Kind::None) {
    return;
  }
  running_ = Request{};
  if (backend_ != nullptr) {
    backend_->requestFinished();
  }
}

void Device::endSession() {
  finishRequest();
  sessionActive_ = false;
  statusActive_ = false;
  port_.statusDisconnected();
}

void Device::refreshPorts() {
  if (backend_ == nullptr) {
    return;
  }
  for (uint8_t port = 0; port < kMaxPorts; ++port) {
    const PortMode mode = backend_->mode(port);
    uint8_t slotCount = cardPort(mode) ? backend_->slotCount(port) : 0;
    if (slotCount > kMaxSlots) {
      slotCount = kMaxSlots;
    }
    if (mode != modes_[port] || slotCount != slotCounts_[port]) {
      // A mode switch: every slot starts again under a new generation.
      modes_[port] = mode;
      slotCounts_[port] = slotCount;
      for (uint8_t slot = 0; slot < kMaxSlots; ++slot) {
        Slot& entry = slots_[port][slot];
        entry.state = SlotState::Pending;
        entry.blocks = 0;
        ++entry.gen;
        entry.probeAttempts = 0;
        entry.nextProbeMs = port_.nowMs();
      }
      cardsDirty_ = true;
    }
    for (uint8_t slot = 0; slot < slotCount; ++slot) {
      apply(port, slot, backend_->observe(port, slot));
    }
  }
}

void Device::apply(uint8_t port, uint8_t slot, const SlotObservation& observation) {
  Slot& entry = slots_[port][slot];
  bool moved = observation.changed;
  if (observation.state != SlotState::Unknown) {
    const bool same = observation.state == entry.state ||
                      (observation.state == SlotState::Pending && identifiedAccessory(entry.state));
    if (!same) {
      entry.state = observation.state;
      entry.blocks = 0;
      moved = true;
    }
    if (entry.state == SlotState::Tpak && observation.state == SlotState::Tpak &&
        observation.blocks != entry.blocks) {
      entry.blocks = observation.blocks;
      moved = true;
    }
  }
  if (moved) {
    ++entry.gen;
    entry.probeAttempts = 0;
    entry.nextProbeMs = port_.nowMs();
    cardsDirty_ = true;
  }
}

void Device::probePendingSlot() {
  const uint32_t now = port_.nowMs();
  for (uint8_t port = 0; port < kMaxPorts; ++port) {
    if (!cardPort(modes_[port]) || !backend_->probesPendingSlots(port)) {
      continue;
    }
    for (uint8_t slot = 0; slot < slotCounts_[port]; ++slot) {
      Slot& entry = slots_[port][slot];
      if (entry.state != SlotState::Pending || entry.probeAttempts >= kProbeAttempts ||
          (int32_t)(now - entry.nextProbeMs) < 0) {
        continue;
      }
      ++entry.probeAttempts;
      const SlotObservation seen = backend_->probe(port, slot);
      if (seen.state == SlotState::Pending || seen.state == SlotState::Unknown) {
        entry.nextProbeMs = now + kProbeRetryMs;
      }
      apply(port, slot, seen);
      backend_->requestFinished();
      return;  // One probe per main-loop pass.
    }
  }
}

bool Device::beginLine() {
  lineFailed_ = false;
  lineDeadlineMs_ = port_.nowMs() + kWriteTimeoutMs;
  // Protocol lines start at a line boundary, whatever was written before.
  return port_.lastByteWasLineEnd() || put("\n", 1);
}

bool Device::put(const char* text) {
  return put(text, strlen(text));
}

bool Device::put(const char* text, size_t length) {
  if (lineFailed_) {
    return false;
  }
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(text);
  while (length > 0) {
    if (!port_.connected()) {
      return writeFailed();
    }
    const int room = port_.availableForWrite();
    if (room > 0) {
      const size_t chunk = length < (size_t)room ? length : (size_t)room;
      const size_t written = port_.write(bytes, chunk);
      bytes += written;
      length -= written;
      if (written > 0) {
        continue;
      }
    }
    port_.service();
    if ((int32_t)(port_.nowMs() - lineDeadlineMs_) >= 0) {
      return writeFailed();
    }
  }
  return true;
}

bool Device::writeFailed() {
  // A line that cannot be written within its bound ends the session; the
  // next protocol line starts with LF, so Main sees this one as broken.
  lineFailed_ = true;
  endSession();
  return false;
}

bool Device::putNumber(uint32_t value) {
  char text[11];
  uint8_t length = 0;
  do {
    text[sizeof(text) - 1 - length++] = (char)('0' + value % 10u);
    value /= 10u;
  } while (value != 0);
  return put(text + sizeof(text) - length, length);
}

bool Device::putHex(uint32_t value, uint8_t digits) {
  static const char kHex[] = "0123456789ABCDEF";
  char text[8];
  for (uint8_t i = 0; i < digits; ++i) {
    text[digits - 1 - i] = kHex[value & 0x0F];
    value >>= 4;
  }
  return put(text, digits);
}

bool Device::putBase64(const uint8_t* data, size_t length) {
  char text[64];
  size_t used = 0;
  for (size_t i = 0; i < length; i += 3) {
    const size_t left = length - i;
    uint32_t bits = (uint32_t)data[i] << 16;
    if (left > 1) bits |= (uint32_t)data[i + 1] << 8;
    if (left > 2) bits |= data[i + 2];
    text[used++] = kBase64[(bits >> 18) & 63];
    text[used++] = kBase64[(bits >> 12) & 63];
    text[used++] = left > 1 ? kBase64[(bits >> 6) & 63] : '=';
    text[used++] = left > 2 ? kBase64[bits & 63] : '=';
    if (used == sizeof(text)) {
      if (!put(text, used)) return false;
      used = 0;
    }
  }
  return used == 0 || put(text, used);
}

bool Device::endLine() {
  if (!put("\n", 1)) {
    return false;
  }
  port_.flushOutput();
  return true;
}

bool Device::beginReply(uint32_t id, const char* kind, bool ok) {
  return beginLine() && put("MISTER_REPLY/1\t") && putNumber(id) &&
         put("\t") && put(kind) && put(ok ? "\tOK" : "\tERR");
}

void Device::replyError(uint32_t id, const char* kind, const char* code) {
  if (beginReply(id, kind, false) && put("\tcode=") && put(code)) {
    endLine();
  }
}

const char* Device::runningKindText() const {
  switch (running_.kind) {
    case Kind::Scan: return "CARD_SCAN";
    case Kind::Read: return "CARD_READ";
    case Kind::Write: return "CARD_WRITE";
    case Kind::TpakInfo: return "TPAK_INFO";
    case Kind::TpakRom: return "TPAK_ROM_READ";
    default: return "PING";
  }
}

void Device::replyCardError(const char* code, const char* detail, int32_t block,
                            int32_t done, bool withGen) {
  if (!beginReply(running_.id, runningKindText(), false) || !put("\tcode=") || !put(code)) {
    return;
  }
  if (detail != nullptr && detail[0] != '\0' && (!put("\tdetail=") || !put(detail))) {
    return;
  }
  if (block >= 0 && (!put("\tblock=") || !putNumber((uint32_t)block))) {
    return;
  }
  if (done >= 0 && (!put("\tdone=") || !putNumber((uint32_t)done))) {
    return;
  }
  if (withGen && (!put("\tgen=") || !putNumber(slots_[running_.port][running_.slot].gen))) {
    return;
  }
  endLine();
}

bool Device::putCardFields() {
  // A cartridge's ROM is one per port: TPAK_ROM_READ names no type or slot.
  const bool rom = running_.kind == Kind::TpakRom;
  return (rom || (put("\ttype=") && put(typeText(running_.type)))) &&
         put("\tport=") && putNumber(running_.port) &&
         (rom || (put("\tslot=") && putNumber(running_.slot))) &&
         put("\tstart=") && putNumber(running_.start) &&
         put("\tcount=") && putNumber(running_.count) &&
         put("\tgen=") && putNumber(slots_[running_.port][running_.slot].gen);
}

void Device::writeHello() {
  if (!sessionActive_) {
    return;
  }
  if (beginLine() && put("MISTER_DEVICE/1\tHELLO\tsession=") && put(session_) &&
      put("\tcaps=") && put(backend_ != nullptr ? "card" : "") &&
      put("\tproduct=") && put(product_) &&
      put("\tfirmware=") && put(firmware_) &&
      put("\tchunk=") && putNumber(kChunkBytes)) {
    endLine();
  }
}

void Device::writeCards() {
  if (!sessionActive_ || backend_ == nullptr) {
    return;
  }
  if (beginLine() && put("MISTER_DEVICE/1\tCARDS\tsession=") && put(session_) &&
      put("\t") && putModesAndSlots()) {
    endLine();
  }
  cardsDirty_ = false;
  lastCardsMs_ = port_.nowMs();
}

bool Device::putModesAndSlots() {
  if (!put("mode=")) {
    return false;
  }
  for (uint8_t port = 0; port < kMaxPorts; ++port) {
    if ((port != 0 && !put(",")) || !put(modeText(modes_[port]))) {
      return false;
    }
  }
  if (!put("\tslots=")) {
    return false;
  }
  bool first = true;
  for (uint8_t port = 0; port < kMaxPorts; ++port) {
    if (!cardPort(modes_[port])) {
      continue;
    }
    for (uint8_t slot = 0; slot < slotCounts_[port]; ++slot) {
      const Slot& entry = slots_[port][slot];
      if ((!first && !put(",")) || !putNumber(port) || !put(".") || !putNumber(slot) ||
          !put(":") || !put(stateText(entry.state)) || !put(":") || !putNumber(entry.gen)) {
        return false;
      }
      first = false;
      bool ok = true;
      if (entry.state == SlotState::Psx) ok = put(":1024x128");
      else if (entry.state == SlotState::N64) ok = put(":128x256");
      else if (entry.state == SlotState::Tpak) ok = put(":") && putNumber(entry.blocks) && put("x32");
      if (!ok) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace mister_serial
