/**
 * GameConfig.h — Network configuration / DPlay settings manager
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * GameConfig (internally DPlayConfig) is the singleton network session
 * configuration manager. It stores DirectPlay session settings: player
 * counts, host/client mode flags, the session name string, timeout
 * values, and a linked list of network providers. The instance is
 * stored at _g_dplay (0x4FD3A8) and created during GameLoop_Setup.
 *
 * Settings are persisted to and loaded from "NetSettings.dat" in the
 * game install directory.
 *
 * Size: 0xB0 bytes (confirmed)
 * Vtable: 0x4781CC (VTBL_DPLAY_CONFIG)
 *
 * Vtable layout:
 *   [0] +0x00: scalar deleting destructor (NETMAN_FreeProviderList, 0x440CC0)
 *
 * === Field Layout ===
 *   +0x00: vtable (0x4781CC)
 *   +0x04: magic word (0x006A) — file format marker
 *   +0x06: initialized flag (byte) — 1 after successful load
 *   +0x07: auto_start flag (byte) — default 1
 *   +0x08: host_mode flag (byte) — 1 = hosting, 0 = client
 *   +0x09..+0x0B: padding
 *   +0x0C: timeout_value (int32) — default 0x1E (30 seconds)
 *   +0x10: provider_list (void*) — linked list head
 *   +0x14..+0x17: unused/padding
 *   +0x18: host_flag_auto (byte)
 *   +0x19..+0x1B: padding
 *   +0x1C: client_player_count (int32) — default 4
 *   +0x20: client_player_count_alt (int32) — default 2
 *   +0x24: client_auto_flag (byte)
 *   +0x25..+0x27: padding
 *   +0x28: host_player_count (int32) — default 4
 *   +0x2C: host_flag_byte (byte)
 *   +0x2D..+0x6B: padding/unused
 *   +0x6C: session_name (char[64]) — 64-byte session name string
 *   +0xAC: host_player_count_2 (int32) — default 2
 */

#pragma once

#include "../shared/types.h"

// Status: TRANSCRIBED
/* vtable addresses in vtable_addrs.h — compiler manages vtables via virtual methods */
/* ================================================================== */
/* GameConfig — Network configuration / DPlay settings manager        */
/* Size: 0xB0 bytes, vtable: 0x4781CC                                  */
/* ================================================================== */
class GameConfig {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */

    /* +0x00: vtable is compiler-managed (binary: VTBL_DPLAY_CONFIG 0x4781CC) */
    uint16_t   m_magic;                   /* +0x04  magic word = 0x006A            */
    uint8_t    m_initialized;             /* +0x06  1 = loaded from file            */
    uint8_t    m_autoStart;               /* +0x07  1 = auto-start session          */

    uint8_t    m_hostMode;                /* +0x08  1 = hosting, 0 = client        */
    uint8_t    _pad_09[3];                /* +0x09  padding                         */

    int32_t    m_timeout;                 /* +0x0C  session timeout (default 0x1E) */

    void*      m_providerList;            /* +0x10  linked list of providers        */

    uint8_t    _pad_14[4];                /* +0x14  padding                         */

    uint8_t    m_hostFlagAuto;            /* +0x18  host auto-flag (byte)          */
    uint8_t    _pad_19[3];                /* +0x19  padding                         */

    int32_t    m_clientPlayerCount;       /* +0x1C  client mode player count (4)   */
    int32_t    m_clientPlayerCountAlt;    /* +0x20  client alt player count (2)    */
    uint8_t    m_clientAutoFlag;          /* +0x24  client auto-flag (byte)        */
    uint8_t    _pad_25[3];                /* +0x25  padding                         */

    int32_t    m_hostPlayerCount;         /* +0x28  host mode player count (4)      */
    uint8_t    m_hostFlagByte;            /* +0x2C  host flag byte                  */
    uint8_t    _pad_2D[0x3F];            /* +0x2D  padding up to +0x6C             */

    char       m_sessionName[64];         /* +0x6C  session name string (64 bytes)  */

    int32_t    m_hostPlayerCountAlt;      /* +0xAC  host alt player count (2)      */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * GameConfig constructor.
     * Address: 0x440C60
     *
     * Initializes all fields with defaults, then calls LoadSettings()
     * to load existing NetSettings.dat or create defaults if not found.
     *
     * Called by: GameLoop_Setup @ 0x406C5A
     */
    GameConfig();

    /**
     * GameConfig destructor (body only).
     * Address: 0x440CC0
     *
     * Frees the provider linked list at +0x10.
     * The scalar deleting destructor at vtable[0] wraps this body and
     * conditionally calls operator delete when flags & 1 (compiler-generated).
     */
    ~GameConfig();

    /* ================================================================ */
    /* Settings Persistence                                              */
    /* ================================================================ */

    /**
     * LoadSettings — Load network settings from NetSettings.dat.
     * Address: 0x440D00 (originally NETMAN_FreePacket — MISNAMED)
     *
     * Opens NetSettings.dat from the install directory, reads 0xAC bytes
     * into this+4 (after vtable). If the file doesn't exist or the magic
     * word (0x006A) is invalid, initializes all fields to defaults and
     * writes the default settings back to disk.
     *
     * Called by: GameConfig_ctor @ 0x440CAC
     */
    void __fastcall LoadSettings();

    /**
     * SaveSettings — Save network settings to NetSettings.dat.
     * Address: 0x440EA0 (originally NETMAN_SendPacket)
     *
     * Writes 0xAC bytes from this+4 to NetSettings.dat, creating or
     * overwriting the file.
     *
     * Called by:
     *   NETMAN_DestroySession @ 0x442078
     *   NETMAN_SetSessionInfo @ 0x441D58
     *   EditWindow_OnPlayerNameChanged @ 0x422718
     */
    void __fastcall SaveSettings();
};

/* ================================================================== */
/* Global instance pointer references                                   */
/* ================================================================== */
extern GameConfig* g_dplayConfig;   /* 0x4FD3A8 — alias _g_dplay / _g_dplay_config */
