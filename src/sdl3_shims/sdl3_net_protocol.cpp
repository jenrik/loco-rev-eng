#ifndef _WIN32

#include "sdl3_net_protocol.h"

#include <algorithm>
#include <limits>

namespace lego_loco::network {
namespace {

constexpr std::array<std::uint8_t, 4> kMagic = {'L', 'O', 'C', 'O'};

void Put16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
}
void Put32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}
std::uint16_t Get16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0]) |
           static_cast<std::uint16_t>(p[1] << 8);
}
std::uint32_t Get32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}
bool PutString(std::vector<std::uint8_t>& out, const std::string& value,
               std::size_t maximum) {
    if (value.size() > maximum || value.size() > 255) return false;
    out.push_back(static_cast<std::uint8_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
    return true;
}
bool GetString(const std::vector<std::uint8_t>& bytes, std::size_t* offset,
               std::size_t maximum, std::string* value) {
    if (*offset >= bytes.size()) return false;
    const std::size_t length = bytes[(*offset)++];
    if (length > maximum || length > bytes.size() - *offset) return false;
    value->assign(reinterpret_cast<const char*>(bytes.data() + *offset), length);
    *offset += length;
    return true;
}
bool IsKnownKind(std::uint8_t kind) {
    return kind >= static_cast<std::uint8_t>(TransportFrameKind::ClientHello) &&
           kind <= static_cast<std::uint8_t>(TransportFrameKind::SessionEnded);
}
void SetError(std::string* error, const char* message) {
    if (error) *error = message;
}

}  // namespace

std::vector<std::uint8_t> EncodeTransportFrame(const TransportFrame& frame) {
    if (frame.payload.size() > kMaximumTransportPayload) return {};
    std::vector<std::uint8_t> out;
    out.reserve(kTransportHeaderSize + frame.payload.size());
    out.insert(out.end(), kMagic.begin(), kMagic.end());
    Put16(out, kTransportVersion);
    out.push_back(static_cast<std::uint8_t>(frame.kind));
    out.push_back(frame.flags);
    Put32(out, frame.source);
    Put32(out, frame.destination);
    Put32(out, static_cast<std::uint32_t>(frame.payload.size()));
    Put32(out, frame.sequence);
    out.insert(out.end(), frame.payload.begin(), frame.payload.end());
    return out;
}

bool TransportFrameDecoder::Append(const std::uint8_t* data, std::size_t size,
                                   std::vector<TransportFrame>* frames,
                                   std::string* error) {
    if (failed_ || !frames || (size != 0 && !data)) {
        SetError(error, "invalid decoder state or arguments");
        failed_ = true;
        return false;
    }
    if (size > (kMaximumTransportPayload + kTransportHeaderSize) * 2 ||
        bytes_.size() > std::numeric_limits<std::size_t>::max() - size) {
        SetError(error, "receive accumulator limit exceeded");
        failed_ = true;
        return false;
    }
    if (size != 0) bytes_.insert(bytes_.end(), data, data + size);

    std::size_t consumed = 0;
    while (bytes_.size() - consumed >= kTransportHeaderSize) {
        const std::uint8_t* header = bytes_.data() + consumed;
        if (!std::equal(kMagic.begin(), kMagic.end(), header)) {
            SetError(error, "invalid transport magic");
            failed_ = true;
            return false;
        }
        if (Get16(header + 4) != kTransportVersion) {
            SetError(error, "unsupported transport version");
            failed_ = true;
            return false;
        }
        if (!IsKnownKind(header[6])) {
            SetError(error, "unknown transport frame kind");
            failed_ = true;
            return false;
        }
        const std::uint32_t payload_size = Get32(header + 16);
        if (payload_size > kMaximumTransportPayload) {
            SetError(error, "transport payload exceeds 64 KiB");
            failed_ = true;
            return false;
        }
        const std::size_t frame_size = kTransportHeaderSize + payload_size;
        if (bytes_.size() - consumed < frame_size) break;

        TransportFrame frame;
        frame.kind = static_cast<TransportFrameKind>(header[6]);
        frame.flags = header[7];
        frame.source = Get32(header + 8);
        frame.destination = Get32(header + 12);
        frame.sequence = Get32(header + 20);
        frame.payload.assign(header + kTransportHeaderSize, header + frame_size);
        frames->push_back(std::move(frame));
        consumed += frame_size;
    }
    if (consumed != 0) bytes_.erase(bytes_.begin(), bytes_.begin() + consumed);
    if (bytes_.size() > kMaximumTransportPayload + kTransportHeaderSize) {
        SetError(error, "incomplete frame exceeds receive limit");
        failed_ = true;
        return false;
    }
    return true;
}

std::vector<std::uint8_t> EncodeClientHello(const ClientHello& hello) {
    std::vector<std::uint8_t> out(kLegoLocoApplicationId.begin(),
                                  kLegoLocoApplicationId.end());
    Put16(out, kTransportVersion);
    Put16(out, kLegacyProtocolVersion);
    if (!PutString(out, hello.player_name, 11) ||
        !PutString(out, hello.expected_session_uuid, 36)) return {};
    return out;
}

bool DecodeClientHello(const std::vector<std::uint8_t>& payload, ClientHello* hello,
                       std::string* error) {
    if (!hello || payload.size() < 22 ||
        !std::equal(kLegoLocoApplicationId.begin(), kLegoLocoApplicationId.end(),
                    payload.begin())) {
        SetError(error, "wrong Lego Loco application ID");
        return false;
    }
    if (Get16(payload.data() + 16) != kTransportVersion) {
        SetError(error, "client transport version mismatch");
        return false;
    }
    if (Get16(payload.data() + 18) != kLegacyProtocolVersion) {
        SetError(error, "client legacy version mismatch");
        return false;
    }
    std::size_t offset = 20;
    if (!GetString(payload, &offset, 11, &hello->player_name) ||
        hello->player_name.empty() ||
        !GetString(payload, &offset, 36, &hello->expected_session_uuid) ||
        offset != payload.size()) {
        SetError(error, "malformed client hello strings");
        return false;
    }
    return true;
}

std::vector<std::uint8_t> EncodeServerWelcome(const ServerWelcome& welcome) {
    std::vector<std::uint8_t> out;
    Put32(out, welcome.assigned_player_id);
    Put32(out, welcome.host_player_id);
    out.push_back(welcome.current_players);
    out.push_back(welcome.maximum_players);
    if (!PutString(out, welcome.session_uuid, 36) ||
        !PutString(out, welcome.host_player_name, 11)) return {};
    return out;
}

bool DecodeServerWelcome(const std::vector<std::uint8_t>& payload,
                         ServerWelcome* welcome, std::string* error) {
    if (!welcome || payload.size() < 11) {
        SetError(error, "short server welcome");
        return false;
    }
    welcome->assigned_player_id = Get32(payload.data());
    welcome->host_player_id = Get32(payload.data() + 4);
    welcome->current_players = payload[8];
    welcome->maximum_players = payload[9];
    std::size_t offset = 10;
    if (welcome->assigned_player_id < 2 ||
        welcome->host_player_id != kHostPlayerId ||
        welcome->current_players == 0 ||
        welcome->current_players > welcome->maximum_players ||
        !GetString(payload, &offset, 36, &welcome->session_uuid) ||
        welcome->session_uuid.size() != 36 ||
        !GetString(payload, &offset, 11, &welcome->host_player_name) ||
        welcome->host_player_name.empty() || offset != payload.size()) {
        SetError(error, "invalid server welcome");
        return false;
    }
    return true;
}

std::vector<std::uint8_t> EncodePlayerNotice(const PlayerNotice& notice) {
    std::vector<std::uint8_t> out;
    Put32(out, notice.player_id);
    if (!PutString(out, notice.player_name, 11)) return {};
    return out;
}

bool DecodePlayerNotice(const std::vector<std::uint8_t>& payload,
                        PlayerNotice* notice, std::string* error) {
    if (!notice || payload.size() < 5) {
        SetError(error, "short player notice");
        return false;
    }
    notice->player_id = Get32(payload.data());
    std::size_t offset = 4;
    if (notice->player_id == 0 ||
        !GetString(payload, &offset, 11, &notice->player_name) ||
        offset != payload.size()) {
        SetError(error, "invalid player notice");
        return false;
    }
    return true;
}

bool ValidateLegacyPayload(const std::vector<std::uint8_t>& payload,
                           std::string* error) {
    if (payload.size() < 4) {
        SetError(error, "legacy payload is shorter than four bytes");
        return false;
    }
    if (payload.size() > kMaximumTransportPayload) {
        SetError(error, "legacy payload exceeds 64 KiB");
        return false;
    }
    if (Get16(payload.data() + 2) != kLegacyProtocolVersion) {
        SetError(error, "legacy packet version is not 300");
        return false;
    }
    return true;
}

}  // namespace lego_loco::network

#endif  // !_WIN32
