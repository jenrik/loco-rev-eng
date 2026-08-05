#pragma once

#ifndef _WIN32
#if defined(__linux__) && !defined(__ANDROID__)

#include "network_discovery.h"

namespace lego_loco::network {

/** Desktop-Linux DNS-SD backend using org.freedesktop.Avahi on the system bus. */
DiscoveryBackendFactory AvahiDbusDiscoveryFactory();

}  // namespace lego_loco::network

#endif  // defined(__linux__) && !defined(__ANDROID__)
#endif  // !_WIN32
