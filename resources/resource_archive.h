// Status: VALIDATED
/**
 * resource_archive.h — Lego Loco RFD/RFH asset archive reader
 *
 * RFH is the on-disk index used by the shipped art resources. Its records are
 * [u32 name_bytes][NUL-terminated path][u32 stored_size][u32 compressed].
 * RFD is the concatenation of those stored payloads in index order.
 */
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef _WIN32

namespace loco::assets {

struct ArchiveEntry {
    std::string name;
    uint64_t offset;
    uint32_t stored_size;
    bool compressed;
};

/**
 * Archive — reads the shipped resource.RFH/RFD pair.
 *
 * Compressed payloads use Huf_Decode (0x45C830): a 0x808-byte header
 * followed by an LSB-first bitstream. Uncompressed payloads are returned
 * verbatim. No synthetic resources or fallback artwork are used.
 */
class Archive {
public:
    bool open(const std::string& directory, std::string* error = nullptr);
    bool is_open() const { return !data_path_.empty(); }
    size_t entry_count() const { return entries_.size(); }

    const ArchiveEntry* find(const std::string& name) const;
    bool read(const std::string& name, std::vector<uint8_t>* output,
              std::string* error = nullptr) const;

private:
    static std::string canonical_name(const std::string& name);
    static bool decode_huffman(const std::vector<uint8_t>& encoded,
                               std::vector<uint8_t>* output,
                               std::string* error);

    std::string data_path_{};
    std::vector<ArchiveEntry> entries_{};
    std::unordered_map<std::string, size_t> by_name_{};
};

}  // namespace loco::assets

#endif /* _WIN32 */
