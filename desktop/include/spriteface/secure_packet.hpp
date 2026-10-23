#pragma once

#include <string>
#include <string_view>

namespace spriteface {
// SF2 = ASCII magic, 12-byte random nonce, AES-128-GCM ciphertext, 16-byte tag.
// The magic is authenticated as additional data; the pairing token is the key.
std::string seal_packet(std::string_view plaintext, std::string_view token);
std::string open_packet(std::string_view packet, std::string_view token);
std::string secure_random_hex(); // 128-bit client/server challenge or pairing key.
}
