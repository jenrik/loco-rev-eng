/**
 * NETMAN_SessionSettings — Network settings persistence (NetSettings.dat)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Functions for loading and saving network session configuration to
 * NetSettings.dat in the install directory. Operates on the GameConfig
 * singleton (game/GameConfig.h, ~0xB0 bytes, object 0x4FD3A8).
 *
 * Contains:
 *   NETMAN_FreePacket     (0x440D00) — Load NetSettings.dat (MISNAMED;
 *                                       matches GameConfig::LoadSettings,
 *                                       which is declared but never
 *                                       defined in game/GameConfig.h —
 *                                       this free function is the real,
 *                                       live implementation)
 *   NETMAN_SendPacket     (0x440EA0) — Save NetSettings.dat (MISNAMED;
 *                                       matches GameConfig::SaveSettings,
 *                                       same situation as above)
 *
 * NETMAN_DestroySession (0x441F80), previously (mis)transcribed here as a
 * free-function "Session panel WindowProc" taking a `void* panel` first
 * parameter, has been moved: reading the NameEntryPanel vtable's raw bytes
 * at 0x4781D0 (ui/NameEntryPanel.h) shows this address at slot 21 (+0x54),
 * matching UI_WindowBase::on_key_down(HWND, UINT, WPARAM, LPARAM) exactly
 * — it is NameEntryPanel::on_key_down(), a real vtable override, not a
 * plain WindowProc. Moved into ui/NameEntryPanel.cpp 2026-08-17.
 *
 * NETMAN_AllocPacket (0x440CC0), previously in this file, has been
 * removed: it duplicated GameConfig::~GameConfig() (game/GameConfig.cpp),
 * which is already integrated as a real C++ destructor, and it did a
 * forbidden manual vtable write (`*(void***)_this = (void**)0x4781CC`)
 * that CLAUDE.md's anti-pattern list explicitly forbids. See
 * network/Netman.h's NETMAN_FreeProviderList declaration for the
 * orphaned-documentation trail.
 *
 * The on-disk NetSettings.dat format is the original x86 object image
 * bytes [object+0x04, object+0xB0) — 0xAC bytes, starting right after the
 * original binary's vtable pointer. Host GameConfig has no vtable (its
 * destructor isn't declared virtual, unlike the original binary's
 * VTBL_DPLAY_CONFIG at 0x4781CC, which had one virtual slot), so the host
 * memory layout of GameConfig does NOT coincide with this on-disk layout;
 * a raw byte-for-byte blit between the file and the live object (as the
 * original x86 code did with `ReadFile(hFile, packetPtr + 4, 0xAC, ...)`)
 * would corrupt or misread fields on host. GameConfig_ApplyRawSettings/
 * GameConfig_BuildRawSettings below stage the exchange through a temporary
 * 0xAC-byte buffer laid out exactly like the file, with each field copied
 * to/from its named GameConfig member individually — every offset is
 * transcribed from GameConfig.h's documented original layout, not
 * invented. (If a future integration pass gives GameConfig a real
 * `virtual ~GameConfig()` to match the original vtable, a direct-blit path
 * would become valid again under a real 32-bit MSVC build; until then,
 * staging is correct on every platform this class is compiled for, so
 * there is deliberately no `#ifdef _WIN32` fast path here.)
 */
#include "../shared/types.h"
#include "../game/GameConfig.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern char g_install_path[];           /* 0x4A99C8 */

extern "C" {

extern int32_t __stdcall wsprintfA(char* lpOut, const char* lpFmt, ...);
extern void*   __stdcall CreateFileA(const char* lpFileName, uint32_t dwDesiredAccess,
                                      uint32_t dwShareMode, void* lpSecurityAttributes,
                                      uint32_t dwCreationDisposition,
                                      uint32_t dwFlagsAndAttributes, void* hTemplateFile);
extern int32_t __stdcall ReadFile(void* hFile, void* lpBuffer, uint32_t nNumberOfBytesToRead,
                                   uint32_t* lpNumberOfBytesRead, void* lpOverlapped);
extern int32_t __stdcall WriteFile(void* hFile, const void* lpBuffer, uint32_t nNumberOfBytesToWrite,
                                    uint32_t* lpNumberOfBytesWritten, void* lpOverlapped);
extern int32_t __stdcall CloseHandle(void* hObject);
}

/* Canonical declarations live in network/Netman.h; these local prototypes
 * (matching them exactly) satisfy -Wmissing-declarations without pulling
 * in Netman.h's much larger include graph for these two functions. */
extern void    __fastcall  NETMAN_FreePacket(GameConfig* packetPtr);
extern void    __fastcall  NETMAN_SendPacket(GameConfig* packetPtr);

/* Format: "%s\\%s" */
#define FMT_FILE_PATH "%s\\%s"          /* 0x47E8A0 */

#define STR_NET_SETTINGS "NetSettings.dat"  /* 0x47EB74 */

/* Local equivalent of stubs/compat.h's INVALID_HANDLE_VALUE, written with
 * C++-style casts: that macro's own body uses C-style casts and trips
 * -Werror=old-style-cast at every expansion site under STRICT=2 (same
 * fix already applied in network/DPlayManager.cpp as
 * DPLAY_INVALID_HANDLE_VALUE). */
#define SESSION_INVALID_HANDLE_VALUE reinterpret_cast<void*>(static_cast<intptr_t>(-1))

/* ================================================================== */
/* NetSettings.dat <-> GameConfig staging helpers                      */
/* See the file header comment above for why this exists.              */
/* ================================================================== */

/** Copy the 0xAC-byte on-disk image into cfg's named fields. File offset
 *  = object offset - 4. The provider-list slot (object+0x10, file
 *  [0xC..0x10)) is deliberately NOT copied: the original always restores
 *  the pre-call provider list pointer after this read (see
 *  NETMAN_FreePacket below), discarding whatever was on disk there.
 *  connection_caps (object+0x14..+0x17, file [0x10..0x14)) IS copied: the
 *  original's raw ReadFile/WriteFile blit covers this region unconditionally
 *  (it was previously miscategorized as padding and skipped here — fixed
 *  2026-08-17 alongside game/GameConfig.h's field-layout correction). */
static void GameConfig_ApplyRawSettings(GameConfig* cfg, const uint8_t raw[0xAC])
{
    memcpy(&cfg->m_magic,               raw + 0x00, sizeof(cfg->m_magic));
    memcpy(&cfg->m_initialized,         raw + 0x02, sizeof(cfg->m_initialized));
    memcpy(&cfg->m_autoStart,           raw + 0x03, sizeof(cfg->m_autoStart));
    memcpy(&cfg->m_hostMode,            raw + 0x04, sizeof(cfg->m_hostMode));
    memcpy(&cfg->m_timeout,             raw + 0x08, sizeof(cfg->m_timeout));
    memcpy(cfg->m_connectionCaps,       raw + 0x10, sizeof(cfg->m_connectionCaps));
    memcpy(&cfg->m_hostFlagAuto,        raw + 0x14, sizeof(cfg->m_hostFlagAuto));
    memcpy(&cfg->m_clientPlayerCount,   raw + 0x18, sizeof(cfg->m_clientPlayerCount));
    memcpy(&cfg->m_clientPlayerCountAlt,raw + 0x1C, sizeof(cfg->m_clientPlayerCountAlt));
    memcpy(&cfg->m_clientAutoFlag,      raw + 0x20, sizeof(cfg->m_clientAutoFlag));
    memcpy(&cfg->m_hostPlayerCount,     raw + 0x24, sizeof(cfg->m_hostPlayerCount));
    memcpy(&cfg->m_hostFlagByte,        raw + 0x28, sizeof(cfg->m_hostFlagByte));
    memcpy(cfg->m_sessionName,          raw + 0x68, sizeof(cfg->m_sessionName));
    memcpy(&cfg->m_hostPlayerCountAlt,  raw + 0xA8, sizeof(cfg->m_hostPlayerCountAlt));
}

/** Build the 0xAC-byte on-disk image from cfg's named fields. The
 *  provider-list slot (file [0xC..0x10)) and the +0x2D..+0x6B gap are left
 *  zeroed: the original writes whatever raw pointer bits happened to be in
 *  the live object there, but that value is never meaningfully read back
 *  (see above), and a host pointer doesn't fit the original's 4-byte x86
 *  slot, so writing 0 is the faithful choice rather than truncating a live
 *  pointer into the file. */
static void GameConfig_BuildRawSettings(const GameConfig* cfg, uint8_t raw[0xAC])
{
    memset(raw, 0, 0xAC);
    memcpy(raw + 0x00, &cfg->m_magic,               sizeof(cfg->m_magic));
    memcpy(raw + 0x02, &cfg->m_initialized,         sizeof(cfg->m_initialized));
    memcpy(raw + 0x03, &cfg->m_autoStart,           sizeof(cfg->m_autoStart));
    memcpy(raw + 0x04, &cfg->m_hostMode,            sizeof(cfg->m_hostMode));
    memcpy(raw + 0x08, &cfg->m_timeout,             sizeof(cfg->m_timeout));
    memcpy(raw + 0x10, cfg->m_connectionCaps,       sizeof(cfg->m_connectionCaps));
    memcpy(raw + 0x14, &cfg->m_hostFlagAuto,        sizeof(cfg->m_hostFlagAuto));
    memcpy(raw + 0x18, &cfg->m_clientPlayerCount,   sizeof(cfg->m_clientPlayerCount));
    memcpy(raw + 0x1C, &cfg->m_clientPlayerCountAlt,sizeof(cfg->m_clientPlayerCountAlt));
    memcpy(raw + 0x20, &cfg->m_clientAutoFlag,      sizeof(cfg->m_clientAutoFlag));
    memcpy(raw + 0x24, &cfg->m_hostPlayerCount,     sizeof(cfg->m_hostPlayerCount));
    memcpy(raw + 0x28, &cfg->m_hostFlagByte,        sizeof(cfg->m_hostFlagByte));
    memcpy(raw + 0x68, cfg->m_sessionName,          sizeof(cfg->m_sessionName));
    memcpy(raw + 0xA8, &cfg->m_hostPlayerCountAlt,  sizeof(cfg->m_hostPlayerCountAlt));
}

/* ================================================================== */
/* NETMAN_FreePacket — 0x440D00                                        */
/* MISNAMED: this function LOADS network settings from NetSettings.dat.*/
/* ================================================================== */
void __fastcall NETMAN_FreePacket(GameConfig* cfg)
{
    char filepath[0x504];
    uint32_t bytesRead;

    /* Save linked list head before overwriting */
    DirectPlayConnectionNode* savedList = cfg->m_providerList;
    cfg->m_providerList = nullptr;

    /* Clear loaded flag */
    cfg->m_initialized = 0;

    /* Build path. The original zeroed filepath byte-by-byte/word-by-word
     * ([0], then a 0x140-iteration uint32_t loop over [1,0x501), then
     * [0x501] and [0x502] explicitly, leaving only the final byte
     * [0x503] untouched); memset to the same 0x503-byte extent is
     * provably equivalent and avoids an old-style reinterpret_cast. */
    memset(filepath, 0, 0x503);
    wsprintfA(filepath, FMT_FILE_PATH, g_install_path, STR_NET_SETTINGS);

    /* Try to open and read existing settings */
    void* hFile = CreateFileA(filepath, 0x80000000, 1, nullptr, 3, 0x8000000, nullptr);
    if (hFile == SESSION_INVALID_HANDLE_VALUE) {
        cfg->m_providerList = savedList;
        return;
    }

    /* Zero-initialized (not just declared): the original x86 ReadFile call
     * only ever runs against a real NetSettings.dat of the expected size, so
     * a short read never happened there. On the host, nothing here checks
     * `bytesRead` against sizeof(raw) before ApplyRawSettings uses the
     * buffer, so a host ReadFile shim that "succeeds" with a short/partial
     * read (e.g. a missing or truncated file) must not hand ApplyRawSettings
     * uninitialized stack bytes -- those would flow into cfg->m_sessionName,
     * which is read back via SetWindowTextA/GetWindowTextA elsewhere and is
     * not otherwise defaulted on this path. */
    uint8_t raw[0xAC] = {0};
    bytesRead = 0;
    if (!ReadFile(hFile, raw, sizeof(raw), &bytesRead, nullptr)) {
        CloseHandle(hFile);
        cfg->m_providerList = savedList;
        return;
    }
    CloseHandle(hFile);
    GameConfig_ApplyRawSettings(cfg, raw);

    /* Check magic marker */
    if (cfg->m_magic == 0x6A) {
        /* Already initialized — mark loaded */
        cfg->m_initialized = 1;
    } else {
        /* First run — initialize defaults */
        cfg->m_magic                = 0x6A;   /* magic */
        cfg->m_hostFlagAuto         = 0;
        cfg->m_clientPlayerCount    = 4;
        cfg->m_clientPlayerCountAlt = 2;
        cfg->m_clientAutoFlag       = 0;
        cfg->m_hostPlayerCount      = 4;
        cfg->m_hostFlagByte         = 0;
        cfg->m_hostPlayerCountAlt   = 2;
        cfg->m_autoStart            = 1;
        cfg->m_hostMode             = 0;
        cfg->m_timeout              = 0x1E;    /* 30-second timeout */
        cfg->m_initialized          = 0;

        /* Save defaults to file */
        {
            uint32_t bytesWritten;
            void* hOut = CreateFileA(filepath, 0x40000000, 1, nullptr, 2, 0x8000000, nullptr);
            if (hOut != SESSION_INVALID_HANDLE_VALUE) {
                uint8_t rawOut[0xAC];
                GameConfig_BuildRawSettings(cfg, rawOut);
                WriteFile(hOut, rawOut, sizeof(rawOut), &bytesWritten, nullptr);
                CloseHandle(hOut);
            }
        }
    }

    cfg->m_providerList = savedList;
}

/* ================================================================== */
/* NETMAN_SendPacket — 0x440EA0                                        */
/* Save network settings to NetSettings.dat.                           */
/* ================================================================== */
void __fastcall NETMAN_SendPacket(GameConfig* cfg)
{
    char filepath[0x504];
    uint32_t bytesWritten;

    wsprintfA(filepath, FMT_FILE_PATH, g_install_path, STR_NET_SETTINGS);

    void* hFile = CreateFileA(filepath, 0x40000000, 1, nullptr, 2, 0x8000000, nullptr);
    if (hFile != SESSION_INVALID_HANDLE_VALUE) {
        uint8_t raw[0xAC];
        GameConfig_BuildRawSettings(cfg, raw);
        WriteFile(hFile, raw, sizeof(raw), &bytesWritten, nullptr);
        CloseHandle(hFile);
    }
}
