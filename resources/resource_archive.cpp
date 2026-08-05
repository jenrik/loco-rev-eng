// Status: VALIDATED
/**
 * resource_archive.cpp — Lego Loco RFD/RFH asset archive reader
 *
 * The RFH record layout was verified against art-res/resource.RFH: all 2,522
 * records consume the 0x14e8f-byte index exactly and their sizes sum to the
 * 57,305,835-byte RFD payload. Huffman traversal is transcribed from
 * Huf_Decode at 0x45C830, including LSB-first bit consumption.
 */
#include "resource_archive.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>

#ifndef _WIN32

namespace loco::assets {
namespace {

constexpr size_t kHuffmanHeaderSize = 0x80c;
constexpr size_t kHuffmanTreeStart = 8;
constexpr size_t kHuffmanTreeSize = 0x800;
constexpr uint32_t kMaxDecodedBytes = 256u * 1024u * 1024u;

uint32_t read_u32_le(const uint8_t* bytes) {
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) |
           (static_cast<uint32_t>(bytes[3]) << 24);
}

uint16_t read_u16_le(const uint8_t* bytes) {
    return static_cast<uint16_t>(bytes[0]) |
           (static_cast<uint16_t>(bytes[1]) << 8);
}

bool read_file(const std::string& path, std::vector<uint8_t>* output,
               std::string* error) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        if (error) *error = "cannot open " + path;
        return false;
    }
    const std::streamoff end = file.tellg();
    if (end < 0 || static_cast<uint64_t>(end) > std::numeric_limits<size_t>::max()) {
        if (error) *error = "invalid size for " + path;
        return false;
    }
    output->resize(static_cast<size_t>(end));
    file.seekg(0);
    if (!output->empty() && !file.read(reinterpret_cast<char*>(output->data()), end)) {
        if (error) *error = "cannot read " + path;
        return false;
    }
    return true;
}

}  // namespace

std::string Archive::canonical_name(const std::string& name) {
    std::string result;
    result.reserve(name.size());
    for (unsigned char ch : name) {
        if (ch == '\\') {
            result.push_back('/');
        } else {
            result.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    return result;
}

bool Archive::open(const std::string& directory, std::string* error) {
    std::vector<uint8_t> index;
    const std::string index_path = directory + "/resource.RFH";
    const std::string data_path = directory + "/resource.RFD";
    if (!read_file(index_path, &index, error)) return false;

    std::ifstream data_file(data_path, std::ios::binary | std::ios::ate);
    if (!data_file) {
        if (error) *error = "cannot open " + data_path;
        return false;
    }
    const std::streamoff data_end = data_file.tellg();
    if (data_end < 0) {
        if (error) *error = "cannot determine size of " + data_path;
        return false;
    }
    const uint64_t data_size = static_cast<uint64_t>(data_end);

    std::vector<ArchiveEntry> parsed_entries;
    std::unordered_map<std::string, size_t> parsed_by_name;
    size_t cursor = 0;
    uint64_t offset = 0;
    while (cursor < index.size()) {
        if (index.size() - cursor < 4) {
            if (error) *error = "truncated RFH name length";
            return false;
        }
        const uint32_t name_size = read_u32_le(index.data() + cursor);
        cursor += 4;
        if (name_size == 0 || name_size > index.size() - cursor || name_size < 2) {
            if (error) *error = "invalid RFH name length";
            return false;
        }
        const uint8_t* name_bytes = index.data() + cursor;
        if (name_bytes[name_size - 1] != 0) {
            if (error) *error = "unterminated RFH name";
            return false;
        }
        std::string name(reinterpret_cast<const char*>(name_bytes), name_size - 1);
        cursor += name_size;
        if (index.size() - cursor < 8) {
            if (error) *error = "truncated RFH record";
            return false;
        }
        const uint32_t stored_size = read_u32_le(index.data() + cursor);
        const uint32_t compressed = read_u32_le(index.data() + cursor + 4);
        cursor += 8;
        if (compressed > 1 || stored_size > data_size - offset) {
            if (error) *error = "RFH payload exceeds RFD bounds";
            return false;
        }
        const std::string key = canonical_name(name);
        if (!parsed_by_name.emplace(key, parsed_entries.size()).second) {
            if (error) *error = "duplicate RFH path: " + name;
            return false;
        }
        parsed_entries.push_back({std::move(name), offset, stored_size, compressed != 0});
        offset += stored_size;
    }
    if (offset != data_size) {
        if (error) *error = "RFH payload total does not match RFD size";
        return false;
    }

    data_path_ = data_path;
    entries_ = std::move(parsed_entries);
    by_name_ = std::move(parsed_by_name);
    return true;
}

const ArchiveEntry* Archive::find(const std::string& name) const {
    const auto it = by_name_.find(canonical_name(name));
    return it == by_name_.end() ? nullptr : &entries_[it->second];
}

bool Archive::read(const std::string& name, std::vector<uint8_t>* output,
                   std::string* error) const {
    if (!output) {
        if (error) *error = "null output buffer";
        return false;
    }
    const ArchiveEntry* entry = find(name);
    if (!entry) {
        if (error) *error = "asset not found: " + name;
        return false;
    }
    std::ifstream data(data_path_, std::ios::binary);
    if (!data) {
        if (error) *error = "cannot open " + data_path_;
        return false;
    }
    data.seekg(static_cast<std::streamoff>(entry->offset));
    std::vector<uint8_t> encoded(entry->stored_size);
    if (!encoded.empty() && !data.read(reinterpret_cast<char*>(encoded.data()), encoded.size())) {
        if (error) *error = "truncated RFD payload: " + entry->name;
        return false;
    }
    if (!entry->compressed) {
        *output = std::move(encoded);
        return true;
    }
    return decode_huffman(encoded, output, error);
}

bool Archive::decode_huffman(const std::vector<uint8_t>& encoded,
                             std::vector<uint8_t>* output,
                             std::string* error) {
    if (encoded.size() < kHuffmanHeaderSize) {
        if (error) *error = "truncated Huffman payload";
        return false;
    }
    const uint32_t decoded_size = read_u32_le(encoded.data());
    if (decoded_size > kMaxDecodedBytes) {
        if (error) *error = "Huffman decoded size exceeds limit";
        return false;
    }
    const uint32_t root = read_u32_le(encoded.data() + 4);
    size_t stream_offset = kHuffmanHeaderSize;
    uint32_t bit_word = read_u32_le(encoded.data() + 0x808);
    uint32_t bits_left = 32;
    std::vector<uint8_t> decoded(decoded_size);

    for (uint32_t output_index = 0; output_index < decoded_size; ++output_index) {
        uint32_t node = root;
        while (node > 0xff) {
            const uint32_t bit = bit_word & 1u;
            bit_word >>= 1;
            --bits_left;
            const uint64_t tree_offset = (static_cast<uint64_t>(node) * 2u + bit) * 2u;
            if (tree_offset + 2 > kHuffmanTreeSize) {
                if (error) *error = "Huffman tree index out of bounds";
                return false;
            }
            node = read_u16_le(encoded.data() + kHuffmanTreeStart + tree_offset);
            if (bits_left == 0) {
                if (stream_offset + 4 > encoded.size()) {
                    if (error) *error = "truncated Huffman bitstream";
                    return false;
                }
                bit_word = read_u32_le(encoded.data() + stream_offset);
                stream_offset += 4;
                bits_left = 32;
            }
        }
        decoded[output_index] = static_cast<uint8_t>(node);
    }
    *output = std::move(decoded);
    return true;
}

}  // namespace loco::assets

#endif /* _WIN32 */
