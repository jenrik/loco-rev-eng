#pragma once

#ifndef _WIN32

#include "network_discovery.h"

namespace lego_loco::network {

/** Concrete backend factories in platform preference order. */
std::vector<DiscoveryBackendFactory> PlatformDiscoveryBackendFactories();

}  // namespace lego_loco::network

#endif  // !_WIN32
