#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
void refineRemoteOutputModeAtBoot();
bool serviceRemoteOutputSelection();
uint8_t remote_output_source_port();
#ifdef __cplusplus
}
#endif
