// Status: VALIDATED
/**
 * pe_string_table.cpp — Standard PE RT_STRING reader for loco.exe
 *
 * Validation: loco.exe's RT_STRING id 0x407 resolves to
 * "startup\\singleup", matching the resource name passed through
 * ResourceManager_GetById (0x446EA0) to EditWindow::initSprites (0x421500).
 */
#include "pe_string_table.h"

#include <fstream>
#include <limits>
#include <vector>

namespace loco::assets {
namespace {

constexpr uint16_t kPe32Magic = 0x10b;
constexpr uint32_t kStringResourceType = 6;  // RT_STRING
constexpr size_t kResourceDirectoryOffset = 96 + 2 * 8;  // PE32 data directory[2]

uint16_t read_u16_le(const std::vector<uint8_t>& bytes, size_t offset) {
    return static_cast<uint16_t>(bytes[offset]) |
           (static_cast<uint16_t>(bytes[offset + 1]) << 8);
}

uint32_t read_u32_le(const std::vector<uint8_t>& bytes, size_t offset) {
    return static_cast<uint32_t>(bytes[offset]) |
           (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<uint32_t>(bytes[offset + 3]) << 24);
}

bool has_range(const std::vector<uint8_t>& bytes, size_t offset, size_t size) {
    return offset <= bytes.size() && size <= bytes.size() - offset;
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

struct Section {
    uint32_t virtual_address;
    uint32_t mapped_size;
    uint32_t raw_offset;
};

bool rva_to_file_offset(uint32_t rva, const std::vector<Section>& sections,
                        size_t* offset) {
    for (const Section& section : sections) {
        if (rva >= section.virtual_address && rva - section.virtual_address < section.mapped_size) {
            *offset = static_cast<size_t>(section.raw_offset) + rva - section.virtual_address;
            return true;
        }
    }
    return false;
}

bool resource_entries(const std::vector<uint8_t>& image, size_t resource_base,
                      uint32_t relative_offset,
                      std::vector<std::pair<uint32_t, uint32_t>>* output) {
    const size_t directory = resource_base + relative_offset;
    if (!has_range(image, directory, 16)) return false;
    const size_t count = static_cast<size_t>(read_u16_le(image, directory + 12)) +
                         read_u16_le(image, directory + 14);
    if (count > (std::numeric_limits<size_t>::max() - directory - 16) / 8 ||
        !has_range(image, directory + 16, count * 8)) return false;
    output->clear();
    output->reserve(count);
    for (size_t index = 0; index < count; ++index) {
        const size_t entry = directory + 16 + index * 8;
        output->emplace_back(read_u32_le(image, entry), read_u32_le(image, entry + 4));
    }
    return true;
}

bool numeric_resource_id(uint32_t value, uint32_t* result) {
    if ((value & 0x80000000u) != 0) return false;
    *result = value & 0xffffu;
    return true;
}

}  // namespace

bool PeStringTable::open(const std::string& executable_path, std::string* error) {
    std::vector<uint8_t> image;
    if (!read_file(executable_path, &image, error)) return false;
    if (!has_range(image, 0x3c, 4) || image[0] != 'M' || image[1] != 'Z') {
        if (error) *error = "not a DOS/PE executable";
        return false;
    }
    const size_t pe_offset = read_u32_le(image, 0x3c);
    if (!has_range(image, pe_offset, 24) || image[pe_offset] != 'P' || image[pe_offset + 1] != 'E' ||
        image[pe_offset + 2] != 0 || image[pe_offset + 3] != 0) {
        if (error) *error = "missing PE signature";
        return false;
    }
    const uint16_t section_count = read_u16_le(image, pe_offset + 6);
    const uint16_t optional_size = read_u16_le(image, pe_offset + 20);
    const size_t optional = pe_offset + 24;
    if (!has_range(image, optional, optional_size) || optional_size < kResourceDirectoryOffset + 8 ||
        read_u16_le(image, optional) != kPe32Magic) {
        if (error) *error = "unsupported PE optional header";
        return false;
    }
    const uint32_t resource_rva = read_u32_le(image, optional + kResourceDirectoryOffset);
    if (resource_rva == 0) {
        if (error) *error = "PE has no resource section";
        return false;
    }

    const size_t section_table = optional + optional_size;
    if (section_count > (std::numeric_limits<size_t>::max() - section_table) / 40 ||
        !has_range(image, section_table, static_cast<size_t>(section_count) * 40)) {
        if (error) *error = "truncated PE section table";
        return false;
    }
    std::vector<Section> sections;
    sections.reserve(section_count);
    for (uint16_t index = 0; index < section_count; ++index) {
        const size_t section = section_table + index * 40;
        const uint32_t virtual_size = read_u32_le(image, section + 8);
        const uint32_t raw_size = read_u32_le(image, section + 16);
        const uint32_t raw_offset = read_u32_le(image, section + 20);
        if (!has_range(image, raw_offset, raw_size)) {
            if (error) *error = "PE section payload is out of bounds";
            return false;
        }
        sections.push_back({read_u32_le(image, section + 12),
                            virtual_size > raw_size ? virtual_size : raw_size, raw_offset});
    }

    size_t resource_base = 0;
    if (!rva_to_file_offset(resource_rva, sections, &resource_base)) {
        if (error) *error = "resource RVA is not mapped";
        return false;
    }
    std::vector<std::pair<uint32_t, uint32_t>> root;
    if (!resource_entries(image, resource_base, 0, &root)) {
        if (error) *error = "invalid resource root directory";
        return false;
    }
    uint32_t string_type = 0;
    bool found_type = false;
    for (const auto& entry : root) {
        uint32_t id = 0;
        if (numeric_resource_id(entry.first, &id) && id == kStringResourceType &&
            (entry.second & 0x80000000u) != 0) {
            string_type = entry.second & 0x7fffffffu;
            found_type = true;
            break;
        }
    }
    if (!found_type) {
        if (error) *error = "PE has no RT_STRING resources";
        return false;
    }

    std::vector<std::pair<uint32_t, uint32_t>> blocks;
    if (!resource_entries(image, resource_base, string_type, &blocks)) {
        if (error) *error = "invalid RT_STRING directory";
        return false;
    }
    std::unordered_map<uint32_t, std::string> parsed;
    for (const auto& block_entry : blocks) {
        uint32_t block_id = 0;
        if (!numeric_resource_id(block_entry.first, &block_id) || block_id == 0 ||
            (block_entry.second & 0x80000000u) == 0) continue;
        std::vector<std::pair<uint32_t, uint32_t>> languages;
        if (!resource_entries(image, resource_base, block_entry.second & 0x7fffffffu, &languages) ||
            languages.empty()) {
            if (error) *error = "invalid RT_STRING language directory";
            return false;
        }
        const uint32_t data_entry_relative = languages.front().second;
        if ((data_entry_relative & 0x80000000u) != 0 ||
            !has_range(image, resource_base + data_entry_relative, 16)) {
            if (error) *error = "invalid RT_STRING data entry";
            return false;
        }
        const size_t data_entry = resource_base + data_entry_relative;
        size_t data_offset = 0;
        const uint32_t data_rva = read_u32_le(image, data_entry);
        const uint32_t data_size = read_u32_le(image, data_entry + 4);
        if (!rva_to_file_offset(data_rva, sections, &data_offset) || !has_range(image, data_offset, data_size)) {
            if (error) *error = "RT_STRING data is not mapped";
            return false;
        }
        size_t cursor = data_offset;
        const size_t data_end = data_offset + data_size;
        for (uint32_t slot = 0; slot < 16; ++slot) {
            if (cursor + 2 > data_end) {
                if (error) *error = "truncated RT_STRING block";
                return false;
            }
            const uint16_t length = read_u16_le(image, cursor);
            cursor += 2;
            const size_t text_bytes = static_cast<size_t>(length) * 2;
            if (text_bytes > data_end - cursor) {
                if (error) *error = "truncated RT_STRING value";
                return false;
            }
            if (length != 0) {
                std::string value;
                value.reserve(length);
                for (uint16_t char_index = 0; char_index < length; ++char_index) {
                    const uint16_t code_unit = read_u16_le(image, cursor + char_index * 2);
                    value.push_back(code_unit <= 0x7f ? static_cast<char>(code_unit) : '?');
                }
                parsed.emplace((block_id - 1) * 16 + slot, std::move(value));
            }
            cursor += text_bytes;
        }
    }
    strings_ = std::move(parsed);
    return true;
}

const std::string* PeStringTable::find(uint32_t resource_id) const {
    const auto it = strings_.find(resource_id);
    return it == strings_.end() ? nullptr : &it->second;
}

}  // namespace loco::assets
