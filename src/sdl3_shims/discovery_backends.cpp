#ifndef _WIN32

#include "discovery_backends.h"
#include "embedded_mdns_discovery.h"

#if defined(__linux__) && !defined(__ANDROID__)
#include "avahi_dbus_discovery.h"
#endif

namespace lego_loco::network {

std::vector<DiscoveryBackendFactory> PlatformDiscoveryBackendFactories() {
#if defined(__linux__) && !defined(__ANDROID__)
    return {AvahiDbusDiscoveryFactory(), EmbeddedMdnsDiscoveryFactory()};
#else
    // Native Bonjour/Android adapters will replace this portable fallback when
    // their platform build targets are introduced.
    return {EmbeddedMdnsDiscoveryFactory()};
#endif
}

}  // namespace lego_loco::network

#endif  // !_WIN32
