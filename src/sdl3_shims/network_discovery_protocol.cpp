#ifndef _WIN32

#include "network_discovery_protocol.h"

#include <charconv>
#include <cctype>
#include <limits>
#include <map>

namespace lego_loco::network::discovery_protocol {
namespace {

bool IsValidUuid(const std::string& value) {
    if (value.size() != 36) {
        return false;
    }
    for (std::size_t index = 0; index < value.size(); ++index) {
        const bool hyphen = index == 8 || index == 13 || index == 18 || index == 23;
        if (hyphen) {
            if (value[index] != '-') {
                return false;
            }
        } else if (!std::isxdigit(static_cast<unsigned char>(value[index]))) {
            return false;
        }
    }
    return true;
}

template <typename Integer>
bool ParseUnsigned(const std::string& text, Integer& result) {
    unsigned int parsed = 0;
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto conversion = std::from_chars(begin, end, parsed);
    if (conversion.ec != std::errc() || conversion.ptr != end ||
        parsed > static_cast<unsigned int>(std::numeric_limits<Integer>::max())) {
        return false;
    }
    result = static_cast<Integer>(parsed);
    return true;
}

std::string TruncateUtf8(const std::string& value, std::size_t limit) {
    if (value.size() <= limit) {
        return value;
    }
    std::size_t size = limit;
    while (size &&
           (static_cast<unsigned char>(value[size]) & 0xC0U) == 0x80U) {
        --size;
    }
    return value.substr(0, size);
}

}  // namespace

bool IsValidUtf8(const std::string& value) noexcept {
    const auto* bytes = reinterpret_cast<const unsigned char*>(value.data());
    std::size_t index = 0;
    while (index < value.size()) {
        const unsigned char first = bytes[index++];
        if (first <= 0x7F) {
            if (first == 0) {
                return false;
            }
            continue;
        }

        std::uint32_t codepoint = 0;
        std::size_t continuation = 0;
        if (first >= 0xC2 && first <= 0xDF) {
            codepoint = first & 0x1FU;
            continuation = 1;
        } else if (first >= 0xE0 && first <= 0xEF) {
            codepoint = first & 0x0FU;
            continuation = 2;
        } else if (first >= 0xF0 && first <= 0xF4) {
            codepoint = first & 0x07U;
            continuation = 3;
        } else {
            return false;
        }
        if (index + continuation > value.size()) {
            return false;
        }
        for (std::size_t count = 0; count < continuation; ++count) {
            const unsigned char next = bytes[index++];
            if ((next & 0xC0U) != 0x80U) {
                return false;
            }
            codepoint = (codepoint << 6U) | (next & 0x3FU);
        }
        if ((continuation == 2 && codepoint < 0x800U) ||
            (continuation == 3 && codepoint < 0x10000U) ||
            (codepoint >= 0xD800U && codepoint <= 0xDFFFU) ||
            codepoint > 0x10FFFFU) {
            return false;
        }
    }
    return true;
}

bool ValidateAdvertisement(const SessionAdvertisement& value, std::string& error) {
    if (!IsValidUuid(value.session_uuid)) {
        error = "session UUID must use canonical 8-4-4-4-12 hexadecimal form";
        return false;
    }
    if (value.display_name.empty() || value.display_name.size() > 127) {
        error = "session display name must contain 1..127 bytes";
        return false;
    }
    if (!IsValidUtf8(value.display_name)) {
        error = "session display name must be valid UTF-8 without embedded NULs";
        return false;
    }
    if (!value.transport_version) {
        error = "transport version must be nonzero";
        return false;
    }
    if (value.legacy_version != kLegacyVersion) {
        error = "legacy protocol version must be 300";
        return false;
    }
    if (!value.max_players || value.max_players > 9 ||
        value.current_players > value.max_players) {
        error = "player counts must satisfy current <= max <= 9";
        return false;
    }
    if (!value.tcp_port) {
        error = "TCP port must be nonzero";
        return false;
    }
    return true;
}

std::vector<std::string> EncodeTxt(const SessionAdvertisement& value) {
    return {
        "id=" + value.session_uuid,
        "name=" + value.display_name,
        "pv=" + std::to_string(value.transport_version),
        "lv=" + std::to_string(value.legacy_version),
        "players=" + std::to_string(value.current_players),
        "max=" + std::to_string(value.max_players),
    };
}

bool ParseTxt(
    const std::vector<std::string>& entries,
    std::uint16_t port,
    SessionAdvertisement& result,
    std::string& error) {
    std::map<std::string, std::string> fields;
    std::size_t total = 0;
    for (const std::string& entry : entries) {
        total += entry.size();
        if (entry.size() > 255 || total > 1300) {
            error = "DNS-SD TXT data exceeds configured bounds";
            return false;
        }
        const std::size_t equals = entry.find('=');
        if (equals == std::string::npos || equals == 0) {
            continue;
        }
        const std::string key = entry.substr(0, equals);
        if (!fields.emplace(key, entry.substr(equals + 1)).second) {
            error = "DNS-SD TXT data contains a duplicate key";
            return false;
        }
    }

    const char* required[] = {"id", "name", "pv", "lv", "players", "max"};
    for (const char* key : required) {
        if (fields.find(key) == fields.end()) {
            error = std::string("DNS-SD TXT data is missing ") + key;
            return false;
        }
    }

    result.session_uuid = fields["id"];
    result.display_name = fields["name"];
    result.tcp_port = port;
    if (!ParseUnsigned(fields["pv"], result.transport_version) ||
        !ParseUnsigned(fields["lv"], result.legacy_version) ||
        !ParseUnsigned(fields["players"], result.current_players) ||
        !ParseUnsigned(fields["max"], result.max_players)) {
        error = "DNS-SD TXT data contains an invalid integer";
        return false;
    }
    return ValidateAdvertisement(result, error);
}

std::string InstanceName(const SessionAdvertisement& value) {
    const std::string suffix = " [" + value.session_uuid.substr(0, 8) + "]";
    return TruncateUtf8(value.display_name, 63 - suffix.size()) + suffix;
}

}  // namespace lego_loco::network::discovery_protocol

#endif  // !_WIN32
