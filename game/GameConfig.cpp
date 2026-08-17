/**
 * GameConfig.cpp — GameConfig (DPlay network config) implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Implements the GameConfig class which manages network session settings
 * and provides persistence via NetSettings.dat.
 */

// Status: TRANSCRIBED — see game/GameConfig.h's header comment for the
// precise remaining gap (LoadSettings()/SaveSettings() declared but never
// defined as methods; unreachable, not a correctness issue).

#include "GameConfig.h"
#include "../network/DirectPlay.h"  /* DirectPlayConnectionNode (m_providerList) */
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* CRT helpers */
extern void* __cdecl operator_new(size_t size);       /* 0x465CE0 */
extern void  __cdecl GLOBAL_free(void* ptr);           /* 0x465CD0 */

/* NETMAN_FreePacket — LoadSettings (standalone, declared in Netman.h) */
extern void  __fastcall NETMAN_FreePacket(GameConfig* packetPtr);

/* Game globals */
extern char  g_empty_string;           /* 0x4851D0 */

/** GameConfig::GameConfig
 *  Address: 0x440C60 */
GameConfig::GameConfig()
{
    this->m_clientPlayerCount = 4;                  /* +0x1C */
    this->m_hostPlayerCountAlt = 2;                 /* +0xAC */

    this->m_providerList = nullptr;                 /* +0x10 */
    this->m_magic = 0x006A;                         /* +0x04 */

    this->m_hostFlagAuto       = 0;                 /* +0x18 */
    this->m_clientPlayerCount  = 4;                 /* +0x1C */
    this->m_clientPlayerCountAlt = 2;               /* +0x20 */
    this->m_clientAutoFlag     = 0;                 /* +0x24 */
    this->m_hostPlayerCount    = 4;                 /* +0x28 */
    this->m_hostFlagByte       = 0;                 /* +0x2C */
    this->m_initialized        = 0;                 /* +0x06 */
    this->m_autoStart          = 1;                 /* +0x07 */
    this->m_hostMode           = 0;                 /* +0x08 */
    this->m_timeout            = 0x1E;              /* +0x0C = 30 */

    /* Load existing settings from NetSettings.dat (or create defaults).
     * Previously passed `this` truncated through a bogus
     * static_cast<int32_t>(reinterpret_cast<intptr_t>(this)) that mangled
     * to a symbol NETMAN_FreePacket never defined (landmine: an
     * unresolved call masked only by this target's
     * -Wl,--unresolved-symbols=ignore-all link flag; GameConfig is never
     * actually constructed anywhere in-tree today, so this was inert, not
     * yet crashing). Passing `this` directly now that the declaration is
     * correctly typed GameConfig*. */
    NETMAN_FreePacket(this);
}

/** GameConfig::~GameConfig — vtable[0] body
 *  Address: 0x440CC0 */
GameConfig::~GameConfig()
{
    /* Free the provider linked list */
    DirectPlayConnectionNode* node = this->m_providerList;   /* +0x10 */
    while (node != nullptr) {
        DirectPlayConnectionNode* next = node->next;
        GLOBAL_free(node);
        node = next;
    }
    this->m_providerList = nullptr;
}
