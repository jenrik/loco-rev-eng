// Status: VALIDATED
/** DPlayConfig defaults — GameConfig_constructor @ 0x440C60. */
#include "../src/decompiled_cpp/network/DPlayConfig.h"

#include <cstdio>
#include <cstring>

namespace {
uint16_t read_u16(const uint8_t* bytes, size_t offset) {
    uint16_t value;
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
}
uint32_t read_u32(const uint8_t* bytes, size_t offset) {
    uint32_t value;
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
}
}

int main()
{
    DPlayConfig config;
    const uint8_t* bytes = config.binary_data();
    if (read_u16(bytes, 0x04) != 0x006A || bytes[0x07] != 1 ||
        read_u32(bytes, 0x0C) != 0x1E || read_u32(bytes, 0x1C) != 4 ||
        read_u32(bytes, 0x20) != 2 || read_u32(bytes, 0x28) != 4 ||
        read_u32(bytes, 0xAC) != 2) {
        std::fputs("FAIL: GameConfig_constructor defaults differ from 0x440C60\n", stderr);
        return 1;
    }
    std::puts("PASS: DPlayConfig defaults match GameConfig_constructor 0x440C60");
    return 0;
}
