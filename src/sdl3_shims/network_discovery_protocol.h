#pragma once

#ifndef _WIN32

#include "network_discovery.h"

#include <cstdint>
#include <string>
#include <vector>

namespace lego_loco::network::discovery_protocol {

inline constexpr char kServiceType[] = "_legoloco._tcp";
inline constexpr char kQualifiedServiceType[] = "_legoloco._tcp.local.";
inline constexpr char kDomain[] = "local";
inline constexpr std::uint16_t kLegacyVersion = 300;

bool IsValidUtf8(const std::string& value) noexcept;
bool ValidateAdvertisement(const SessionAdvertisement& value, std::string& error);
std::vector<std::string> EncodeTxt(const SessionAdvertisement& value);
bool ParseTxt(
    const std::vector<std::string>& entries,
    std::uint16_t port,
    SessionAdvertisement& result,
    std::string& error);
std::string InstanceName(const SessionAdvertisement& value);

}  // namespace lego_loco::network::discovery_protocol

#endif  // !_WIN32
