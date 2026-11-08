#pragma once

#include <stddef.h>
#include <stdint.h>

enum SerialCommandLineResult : uint8_t {
  SERIAL_COMMAND_LINE_NONE = 0,
  SERIAL_COMMAND_LINE_READY,
  SERIAL_COMMAND_LINE_OVERFLOW,
};

// Assembles one line. After OVERFLOW the buffer still holds the line's start
// and the rest of the line is discarded through its CR or LF.
SerialCommandLineResult serialCommandLinePush(
    char value, char* buffer, size_t capacity, size_t* length, bool* discarding);

// A MiSTer Main record (MISTER_STATUS/1, MISTER_REQUEST/1, ...): exact,
// case-sensitive prefix. No text command starts with it.
bool serialLineIsMisterRecord(const char* line);

char* serialSkipSpaces(char* text);
bool serialTokenEquals(const char* token, const char* expected);
bool serialTextEqualsExact(const char* text, const char* expected);
bool serialCommandStartsWith(const char* command, const char* expected, char** remainder);
bool serialParseLongToken(char*& text, long* value);
bool serialParseUint32Token(char*& text, uint32_t* value);
