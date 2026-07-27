// Status: VALIDATED
/**
 * pe_string_table.h — Standard PE RT_STRING reader for loco.exe
 *
 * Resolves the file stems used by ResourceManager::GetById. The original
 * code calls LoadStringA (e.g. 0x446EA0); this host-side reader accesses the
 * same RT_STRING records directly from the PE resource section.
 */
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace loco::assets {

class PeStringTable {
public:
    bool open(const std::string& executable_path, std::string* error = nullptr);
    const std::string* find(uint32_t resource_id) const;
    size_t size() const { return strings_.size(); }

private:
    std::unordered_map<uint32_t, std::string> strings_;
};

}  // namespace loco::assets
