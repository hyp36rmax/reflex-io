#include "serial_command_parser.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

namespace {

char toUpperAscii(char value) {
  return (value >= 'a' && value <= 'z') ? (char)(value - ('a' - 'A')) : value;
}

}  // namespace

SerialCommandLineResult serialCommandLinePush(
    char value, char* buffer, size_t capacity, size_t* length, bool* discarding) {
  if (buffer == nullptr || length == nullptr || discarding == nullptr || capacity < 2) {
    return SERIAL_COMMAND_LINE_OVERFLOW;
  }

  if (*discarding) {
    // The rest of an over-long line is dropped through its end, so no part of
    // it can run as a command of its own.
    if (value == '\r' || value == '\n') {
      *discarding = false;
      *length = 0;
      buffer[0] = '\0';
    }
    return SERIAL_COMMAND_LINE_NONE;
  }

  if (value == '\r' || value == '\n') {
    size_t start = 0;
    while (start < *length &&
           (buffer[start] == ' ' || buffer[start] == '\t')) {
      ++start;
    }
    size_t end = *length;
    while (end > start &&
           (buffer[end - 1] == ' ' || buffer[end - 1] == '\t')) {
      --end;
    }
    if (start == end) {
      *length = 0;
      buffer[0] = '\0';
      return SERIAL_COMMAND_LINE_NONE;
    }
    if (start != 0) {
      memmove(buffer, buffer + start, end - start);
    }
    *length = end - start;
    buffer[*length] = '\0';
    return SERIAL_COMMAND_LINE_READY;
  }

  const uint8_t byte = static_cast<uint8_t>(value);
  // MiSTer records separate their fields with TAB; text commands never need it.
  const bool keepTab = value == '\t' && *length >= 7 && serialLineIsMisterRecord(buffer);
  if ((byte < 0x20u && !keepTab) || byte >= 0x7Fu) {
    return SERIAL_COMMAND_LINE_NONE;
  }
  if (*length >= capacity - 1) {
    // Keep the start so the caller can tell a MiSTer record from a command.
    *length = 0;
    *discarding = true;
    return SERIAL_COMMAND_LINE_OVERFLOW;
  }
  buffer[(*length)++] = value;
  buffer[*length] = '\0';
  return SERIAL_COMMAND_LINE_NONE;
}

bool serialLineIsMisterRecord(const char* line) {
  return line != nullptr && strncmp(line, "MISTER_", 7) == 0;
}

char* serialSkipSpaces(char* text) {
  while (*text == ' ' || *text == '\t') {
    ++text;
  }
  return text;
}

bool serialTokenEquals(const char* token, const char* expected) {
  size_t index = 0;
  while (expected[index] != '\0') {
    if (token[index] == '\0' ||
        toUpperAscii(token[index]) != expected[index]) {
      return false;
    }
    ++index;
  }
  return token[index] == '\0' || token[index] == ' ' || token[index] == '\t';
}

bool serialTextEqualsExact(const char* text, const char* expected) {
  if (text == nullptr || expected == nullptr) {
    return false;
  }
  while (*text == ' ' || *text == '\t') {
    ++text;
  }
  size_t index = 0;
  while (expected[index] != '\0') {
    if (text[index] == '\0' ||
        toUpperAscii(text[index]) != toUpperAscii(expected[index])) {
      return false;
    }
    ++index;
  }
  text += index;
  while (*text == ' ' || *text == '\t') {
    ++text;
  }
  return *text == '\0';
}

bool serialCommandStartsWith(const char* command, const char* expected, char** remainder) {
  size_t index = 0;
  while (expected[index] != '\0') {
    if (toUpperAscii(command[index]) != expected[index]) {
      return false;
    }
    ++index;
  }

  char* tail = const_cast<char*>(&command[index]);
  if (*tail == '\0') {
    *remainder = tail;
    return true;
  }
  if (*tail != ' ' && *tail != '\t') {
    return false;
  }
  *remainder = serialSkipSpaces(tail);
  return true;
}

bool serialParseLongToken(char*& text, long* value) {
  text = serialSkipSpaces(text);
  if (*text == '\0') {
    return false;
  }

  char* end = nullptr;
  const long parsed = strtol(text, &end, 0);
  if (end == text) {
    return false;
  }
  *value = parsed;
  text = serialSkipSpaces(end);
  return true;
}

bool serialParseUint32Token(char*& text, uint32_t* value) {
  text = serialSkipSpaces(text);
  if (*text == '\0') {
    return false;
  }

  char* end = nullptr;
  const unsigned long parsed = strtoul(text, &end, 0);
  if (end == text) {
    return false;
  }
  *value = (uint32_t)parsed;
  text = serialSkipSpaces(end);
  return true;
}
