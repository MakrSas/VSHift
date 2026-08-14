#include "core/crypto/aes.h"
#include "core/firmware/ps3_tar.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace {

std::vector<std::uint8_t> Hex(std::initializer_list<std::uint8_t> values) {
    return std::vector<std::uint8_t>(values);
}

} // namespace

int main() {
    const auto key = Hex({0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
                          0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c});
    const auto iv = Hex({0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                         0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f});
    const auto plaintext = Hex({0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
                                0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a});
    const auto cbc_ciphertext = Hex({0x76, 0x49, 0xab, 0xac, 0x81, 0x19, 0xb2, 0x46,
                                     0xce, 0xe9, 0x8e, 0x9b, 0x12, 0xe9, 0x19, 0x7d});
    std::vector<std::uint8_t> decrypted;
    assert(vshift::crypto::AesCbcDecrypt(key, iv, cbc_ciphertext, decrypted).ok());
    assert(decrypted == plaintext);

    const auto zero_counter = std::array<std::uint8_t, 16>{};
    const auto ctr_ciphertext = Hex({0x16, 0x36, 0xd5, 0xee, 0x34, 0xf8, 0x06, 0x25,
                                     0xd7, 0x7f, 0x8e, 0x56, 0xca, 0x88, 0x43, 0x45});
    std::vector<std::uint8_t> crypted;
    assert(vshift::crypto::AesCtrCrypt(key, zero_counter, plaintext, crypted).ok());
    assert(crypted == ctr_ciphertext);

    std::vector<std::uint8_t> tar(1536, 0);
    const char name[] = "dev_flash_012.tar";
    std::copy(name, name + sizeof(name) - 1,
              tar.begin());
    tar[100] = '0';
    tar[156] = '0';
    tar[257] = 'u';
    tar[258] = 's';
    tar[259] = 't';
    tar[260] = 'a';
    tar[261] = 'r';
    tar[124] = '0';
    tar[125] = '0';
    tar[126] = '0';
    tar[127] = '0';
    tar[128] = '0';
    tar[129] = '0';
    tar[130] = '0';
    tar[131] = '0';
    tar[132] = '0';
    tar[133] = '0';
    tar[134] = '0';
    tar[135] = '0';
    tar[136] = '0';
    tar[135] = '1';
    tar[512] = 0x42;
    const auto parsed_tar = vshift::firmware::ParsePs3Tar(tar);
    assert(parsed_tar.ok());
    assert(parsed_tar.entries.size() == 1);
    assert(parsed_tar.entries[0].name == name);
    assert(parsed_tar.entries[0].data_offset == 512);
    assert(parsed_tar.entries[0].data_length == 1);
    return 0;
}
