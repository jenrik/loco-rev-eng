// Status: VALIDATED
/** resource_archive_test.cpp — RFH/RFD archive regression test. */
#include "pe_string_table.h"
#include "resource_archive.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {
uint32_t read_u32_le(const std::vector<uint8_t>& bytes, size_t offset) {
    return static_cast<uint32_t>(bytes[offset]) |
           (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<uint32_t>(bytes[offset + 3]) << 24);
}

bool fail(const std::string& message) {
    std::fprintf(stderr, "FAIL: %s\n", message.c_str());
    return false;
}
}

int main(int argc, char** argv) {
    const std::string asset_dir = argc == 2 ? argv[1] : "lego-loco-unpacked/art-res";
    loco::assets::Archive archive;
    std::string error;
    if (!archive.open(asset_dir, &error)) return fail(error) ? 0 : 1;
    if (archive.entry_count() != 2522) return fail("unexpected RFH entry count") ? 0 : 1;

    loco::assets::PeStringTable strings;
    if (!strings.open("lego-loco-unpacked/Exe/loco.exe", &error)) return fail(error) ? 0 : 1;
    const std::string* single_up = strings.find(0x407);
    if (!single_up || *single_up != "startup\\singleup") {
        return fail("RT_STRING 0x407 does not map to startup\\singleup") ? 0 : 1;
    }

    std::vector<uint8_t> raw;
    if (!archive.read("roads/half-vwint.dat", &raw, &error)) return fail(error) ? 0 : 1;
    if (raw.size() != 895 || std::string(raw.begin(), raw.begin() + 18) != "physical_occupancy") {
        return fail("uncompressed entry mismatch") ? 0 : 1;
    }

    std::vector<uint8_t> sprite;
    if (!archive.read(*single_up + ".bmp", &sprite, &error)) return fail(error) ? 0 : 1;
    if (sprite.size() != 21360 || sprite[0] != 'B' || sprite[1] != 'M') {
        return fail("compressed BMP did not decode") ? 0 : 1;
    }
    if (read_u32_le(sprite, 18) != 155 || read_u32_le(sprite, 22) != 130) {
        return fail("startup/singleup.bmp dimensions differ from payload") ? 0 : 1;
    }

    std::puts("PASS: PE resource IDs resolved; 2522-entry RFH/RFD archive parsed; raw and Huffman BMP assets decoded");
    return 0;
}
