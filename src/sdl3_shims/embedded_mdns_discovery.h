#pragma once

#ifndef _WIN32

#include "network_discovery.h"

namespace lego_loco::network {

/** Daemon-free DNS-SD fallback built on the pinned mjansson/mdns codec. */
DiscoveryBackendFactory EmbeddedMdnsDiscoveryFactory();

}  // namespace lego_loco::network

#endif  // !_WIN32
