/**
 * sdl3_dplay.h — DirectPlay 4 → SDL3 stub compatibility shim
 *
 * DirectPlay networking (IPX, serial, modem) has no SDL3 equivalent.
 * This shim provides stub implementations that return success for
 * initialization but do not pass network traffic.
 *
 * Use this for single-player only. Multiplayer requires a full
 * DirectPlay reimplementation (e.g., via ENet or custom UDP protocol).
 *
 * NOT part of the Lego Loco reverse-engineering project.
 */

#ifndef LOCO_SDL3_DPLAY_H
#define LOCO_SDL3_DPLAY_H

#include "sdl3_types.h"
#include <cstdint>

#ifndef _WIN32

/* =========================================================================
 * IDirectPlay4 — Stub
 * ========================================================================= */

struct IDirectPlay4 {
    IDirectPlay4() {}
    ~IDirectPlay4() {}

    /* All methods return DP_OK (0) as stubs */
    int  Initialize(void*)           { return 0; }
    int  Release()                   { delete this; return 0; }
    int  CreatePlayer(DPID*, void*, void*, void*, uint32_t) { return 0; }
    int  DestroyPlayer(DPID)         { return 0; }
    int  Send(DPID, DPID, uint32_t, void*, uint32_t) { return 0; }
    int  Receive(DPID*, DPID*, uint32_t, void*, uint32_t*, void*) { return 0; }
    int  Open(void*, uint32_t)       { return 0; }
    int  Close()                     { return 0; }
    int  GetCaps(void*, uint32_t)    { return 0; }
    int  SetSessionDesc(void*, uint32_t) { return 0; }
    int  GetSessionDesc(void*, uint32_t*) { return 0; }
    int  EnumSessions(void*, uint32_t, void*, uint32_t*, uint32_t) { return 0; }
    int  EnumPlayers(void*, uint32_t, void*, uint32_t*, uint32_t)  { return 0; }
    int  GetPlayerCaps(DPID, void*, uint32_t) { return 0; }
};

/* =========================================================================
 * IDirectPlayAddress — Stub
 * ========================================================================= */

struct IDirectPlayAddress {
    IDirectPlayAddress() {}
    ~IDirectPlayAddress() {}
    int Release() { delete this; return 0; }
};

/* =========================================================================
 * DirectPlayCreate helper
 * ========================================================================= */

static inline int DirectPlayCreate(void* guid, IDirectPlay4** ppDP, void* unk) {
    (void)guid; (void)unk;
    *ppDP = new IDirectPlay4();
    return (*ppDP) ? 0 : -1;
}

#endif /* _WIN32 */

#endif /* LOCO_SDL3_DPLAY_H */
