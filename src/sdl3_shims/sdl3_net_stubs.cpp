/**
 * sdl3_net_stubs.cpp — Minimal host stubs for Netman construction
 *
 * These symbols are needed by GameLoop.cpp but the full networking
 * stack (SDL3_net, discovery, avahi) is not built in headless CI.
 * Provides null-object Netman that satisfies the initialization path
 * without requiring SDL3_net or dbus.
 */
#ifndef _WIN32

#include <cstddef>

class Netman {
public:
    void* vtable;
};

namespace lego_loco::network {

Netman* CreateHostNetman() {
    // Return a minimal heap-allocated Netman.  The original x86
    // allocation is 0x804 bytes; the host uses sizeof(Netman).
    // Fields are zeroed by operator_new.
    return new Netman();
}

void PumpTransportIntoGame(Netman*, class TrainSubsystem*, const char*) {
    // No-op: networking stack not compiled in.
}

bool QueueLegacyPayloadForGame(Netman*, TrainSubsystem*,
                               unsigned int,
                               const void*, unsigned long,
                               const char**) {
    return false;
}

}  // namespace lego_loco::network
#endif
