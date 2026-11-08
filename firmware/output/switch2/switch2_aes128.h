#pragma once

#include <stdint.h>

// AES-128 block encryption (FIPS-197), used only for the Switch 2 pairing
// challenge. Encrypt-only; no key schedule is cached between calls.
namespace switch2_aes128 {

void encryptBlock(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]);

}  // namespace switch2_aes128
