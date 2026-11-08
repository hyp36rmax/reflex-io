#pragma once

#include <stdint.h>

struct XboxOneAuthPacket {
  uint8_t command;
  uint8_t sequence;
  uint16_t length;
  uint8_t data[1024];
};

enum XboxOneAuthTraceStage : uint8_t {
  XBONE_TRACE_CONSOLE_RAW = 1,
  XBONE_TRACE_CONSOLE_AGGREGATE = 2,
  XBONE_TRACE_DONOR_RAW = 3,
  XBONE_TRACE_DONOR_AGGREGATE = 4,
  XBONE_TRACE_TO_DONOR_RAW = 5,
  XBONE_TRACE_TO_CONSOLE_RAW = 6,
};

struct XboxOneAuthTraceEntry {
  uint32_t timestamp_ms;
  uint8_t stage;
  uint8_t command;
  uint8_t sequence;
  uint8_t flags;
  uint16_t wire_length;
  uint16_t data_length;
  uint8_t head[16];
};

struct XboxOneAuthSidecarStatus {
  bool supported;
  bool mounted;
  bool dongle_ready;
  bool auth_completed;
  bool donor_removable;
  uint8_t candidate_count;
  uint8_t dev_addr;
  uint8_t instance;
  uint8_t state;
  uint8_t last_command;
  uint8_t last_sequence;
  uint16_t last_length;
  uint16_t vid;
  uint16_t pid;
  uint32_t mount_count;
  uint32_t input_count;
  uint32_t sent_count;
  uint32_t queue_count;
  uint32_t queue_drop_count;
  uint32_t invalid_count;
  uint16_t last_invalid_length;
  uint8_t last_invalid_head[16];
  uint32_t console_to_dongle_count;
  uint32_t dongle_to_console_count;
  uint8_t donor_queue_depth;
  uint8_t donor_queue_high_water;
  uint32_t console_backpressure_count;
  uint32_t donor_backpressure_count;
  uint32_t donor_resume_count;
  uint32_t wake_queue_count;
  uint32_t wake_send_count;
  uint32_t wake_fail_count;
};

#ifdef __cplusplus
extern "C" {
#endif

void xbone_auth_passthrough_reset();
void xbone_auth_passthrough_task();
void xbone_auth_passthrough_mount(uint8_t dev_addr, uint8_t instance, uint8_t controller_type, uint8_t subtype);
void xbone_auth_passthrough_umount(uint8_t dev_addr, uint8_t instance);
bool xbone_auth_passthrough_claims(uint8_t dev_addr, uint8_t instance);
bool xbone_auth_passthrough_report_received(uint8_t dev_addr, uint8_t instance, const uint8_t* report, uint16_t len);
void xbone_auth_passthrough_report_sent(uint8_t dev_addr, uint8_t instance, const uint8_t* report, uint16_t len);
bool xbone_auth_passthrough_resume_dongle_receive();
bool xbone_auth_passthrough_wake();
bool xbone_auth_passthrough_submit_rumble(const uint8_t* report, uint16_t len);
bool xbone_auth_passthrough_ready();
bool xbone_auth_passthrough_authenticated();
void xbone_auth_passthrough_mark_authenticated();
bool xbone_auth_passthrough_submit_console_packet(uint8_t command, uint8_t sequence, const uint8_t* data, uint16_t len);
bool xbone_auth_passthrough_take_dongle_packet(XboxOneAuthPacket* packet);
XboxOneAuthSidecarStatus xbone_auth_passthrough_status();
void xbone_auth_trace_record(uint8_t stage, const uint8_t* wire,
                             uint16_t wire_len, uint8_t command,
                             uint8_t sequence, const uint8_t* data,
                             uint16_t data_len);
uint8_t xbone_auth_trace_count();
bool xbone_auth_trace_entry(uint8_t ordinal, XboxOneAuthTraceEntry* entry);
uint8_t xbone_auth_trace_snapshot(XboxOneAuthTraceEntry* entries,
                                  uint8_t max_entries);
void xbone_auth_trace_clear();

#ifdef __cplusplus
}
#endif
