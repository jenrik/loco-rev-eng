/**
 * GameConfig.cpp — GameConfig (DPlay network config) implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Implements the GameConfig class which manages network session settings
 * and provides persistence via NetSettings.dat.
 */

// Status: TRANSCRIBED

#include "GameConfig.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* CRT helpers */
extern void* __cdecl operator_new(size_t size);       /* 0x465CE0 */
extern void  __cdecl GLOBAL_free(void* ptr);           /* 0x465CD0 */

/* NETMAN_FreePacket — LoadSettings (standalone, declared in Netman.h) */
extern void  __fastcall NETMAN_FreePacket(int32_t packetPtr);

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

    /* Load existing settings from NetSettings.dat (or create defaults) */
    NETMAN_FreePacket(static_cast<int32_t>(reinterpret_cast<intptr_t>(this)));
}

/** GameConfig::~GameConfig — vtable[0] body
 *  Address: 0x440CC0 */
GameConfig::~GameConfig()
{
    /* Free the provider linked list */
    void* node = this->m_providerList;              /* +0x10 */
    while (node != nullptr) {
        this->m_providerList = *reinterpret_cast<void**>(node); /* follow linked list */
        GLOBAL_free(node);
        node = this->m_providerList;
    }
}
