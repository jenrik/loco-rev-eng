/**
 * DPlayManager.cpp — DPlayManager implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

// Status: TRANSCRIBED

#include "DPlayManager.h"
#include "../game/PlayerConfig.h"
#include "../ui/NameEntryPanel.h"
#include <new>
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern "C" {

/* Win32 API — imported via IAT */
void* __stdcall CreateFileA(const char* lpFileName, uint32_t dwDesiredAccess,
                            uint32_t dwShareMode, void* lpSecurityAttributes,
                            uint32_t dwCreationDisposition,
                            uint32_t dwFlagsAndAttributes, void* hTemplateFile);
int32_t __stdcall WriteFile(void* hFile, const void* lpBuffer, uint32_t nNumberOfBytesToWrite,
                            uint32_t* lpNumberOfBytesWritten, void* lpOverlapped);
int32_t __stdcall ReadFile(void* hFile, void* lpBuffer, uint32_t nNumberOfBytesToRead,
                           uint32_t* lpNumberOfBytesRead, void* lpOverlapped);
int32_t __stdcall CloseHandle(void* hObject);
int32_t __stdcall wsprintfA(char* lpOut, const char* lpFmt, ...);
void* __stdcall SelectObject(void* hdc, void* hgdiobj);
int32_t __stdcall DrawTextA(void* hdc, const char* lpchText, int32_t cchText,
                            RECT* lprc, uint32_t format);
int32_t __stdcall CopyRect(RECT* lprcDst, const RECT* lprcSrc);
int32_t __stdcall OffsetRect(RECT* lprc, int32_t dx, int32_t dy);

/* PtInRect — Win32 region hit-test (IAT thunk at 0x0047734C) */
int32_t __stdcall PtInRect(const RECT* lprc, POINT pt);

/* CRT helpers */
int32_t __cdecl CRT_atoi(const char* str);                 /* 0x466390 */
int32_t __cdecl CRT_rand(void);                            /* 0x466150 */

} /* extern "C" */

/* C++ allocation helper — wraps HeapAlloc */
void* __cdecl operator_new(size_t size);                   /* 0x465CE0 */

/* Game-internal functions (C++ linkage) */
/* Real def: ui/UIPANEL.cpp, HDC(void*) — was declared taking int32_t against
 * a real void* parameter (call-0 landmine: the mismatched mangled name bound
 * to the no-op stub `UIPANEL_BeginPaint(int32_t)` in shared/link_stubs.cpp
 * instead of the real implementation). */
HDC  __fastcall UIPANEL_BeginPaint(void* self);           /* 0x426B00 */
/* Real def: ui/UIPANEL.cpp:0x426B90 — the 2nd param is `int hdc`, not
 * `void*`. Was declared here with a void* 2nd param — a distinct mangled
 * symbol (call-0-class landmine, same shape as UIPANEL_BeginPaint above)
 * that bound to shared/stubs_impl.cpp's host no-op instead of the real
 * present pipeline (docs/landmine-sweep-worklist.md). */
void __thiscall UIPANEL_EndPaintEx(void* panel, int32_t hdc, int32_t unlockParam,
                                     uint8_t unlockFlag, RECT* restrictRect); /* 0x426B90 */
/* Real def: ui/UIPANEL_Surface.cpp, bool(void*,uint32_t,uint32_t,int32_t,
 * uint32_t,void*,uint32_t,uint32_t,int32_t,uint32_t,uint32_t) — was declared
 * uniformly int32_t, which doesn't match the real mixed uint32_t/int32_t
 * shape (call-0 landmine). */
bool __cdecl    UIPANEL_Blit(void* surface, uint32_t srcX, uint32_t srcY,
                              int32_t srcW, uint32_t srcH, void* dstSurface,
                              uint32_t dstX, uint32_t dstY, int32_t dstW,
                              uint32_t dstH, uint32_t flags);     /* 0x42B050 */
void __cdecl    UI_CenterWindow(RECT* outer, RECT* inner);     /* 0x425A50 */
char* __cdecl PlayerConfig_SaveToFile(void* config);           /* 0x453320 */

/* Inline memory copy utility — matches MSVC's rep movsd pattern */
static void inline_memcpy(void* dst, const void* src, int32_t len)
{
    uint32_t* d32 = reinterpret_cast<uint32_t*>(dst);
    const uint32_t* s32 = reinterpret_cast<const uint32_t*>(src);
    int32_t dwords = len >> 2;
    int32_t rem = len & 3;
    int32_t i;
    for (i = 0; i < dwords; i++) d32[i] = s32[i];
    uint8_t* d8 = reinterpret_cast<uint8_t*>(dst);
    const uint8_t* s8 = reinterpret_cast<const uint8_t*>(src);
    for (i = 0; i < rem; i++) d8[dwords * 4 + i] = s8[dwords * 4 + i];
}

/* C++ heap helper — Address: 0x465CD0. */
void __cdecl GLOBAL_free(void* ptr);

/* ================================================================== */
/* Global variables referenced                                         */
/* ================================================================== */

extern PlayerConfig* g_player_config; /* 0x4AA4A8 — PlayerConfig singleton */
extern char  g_empty_string;     /* 0x4851D0 — empty string constant  */
extern void* g_primary_surface;  /* 0x4FD3C4 — primary DDraw surface  */

/* Format strings */
#define FMT_PLAYER_DATA   "%s_%s.crd"      /* 0x47EB84 */

/* Matches the established idiom in game/PlayerConfig.cpp and others. */
#define DPLAY_INVALID_HANDLE_VALUE reinterpret_cast<void*>(static_cast<intptr_t>(-1))

/* ================================================================== */
/* Internally-called DPLAY_DestroySession — populates DPLAY_SessionData
 * from this DPlayManager (player slot).
 * Not a member of DPlayManager; called by GetPlayerData.
 * Address: 0x442EC0                                                  */
/* ================================================================== */
static void InitSessionDataSnapshot(DPLAY_SessionData* session, const DPlayManager* slot)
{
    session->data_blk1[0] = 0;
    session->data_blk1[20] = 0;
    session->data_blk2[0] = 0;

    /* Dynamic dispatch is compiler-managed in reconstructed C++. */
    inline_memcpy(session->data_blk1, slot->m_sessionBlk1, sizeof(session->data_blk1));
    inline_memcpy(session->data_blk2, slot->m_sessionBlk2, sizeof(session->data_blk2));
    inline_memcpy(session->player_name, slot->m_playerName, sizeof(session->player_name));
    session->unknown_8B = slot->m_unknown93;
    session->session_type = slot->m_playerType;
    session->session_track = slot->m_playerTrack;
    session->word_value = slot->m_wordValue;
    session->dword_value = slot->m_dwordValue;
    session->flag_38 = slot->m_flag40;
    session->flag_39 = slot->m_flag41;
    session->flag_3A = slot->m_flag42;
    session->entry_count = 0x80;
    inline_memcpy(session->track_entries, slot->m_trackEntries,
                  sizeof(session->track_entries));
}

/** DPLAY_SessionData destructor body — Address: 0x442EA0. */
DPLAY_SessionData::~DPLAY_SessionData()
{
    // The original body has no object-owned cleanup; MSVC's scalar-deleting
    // wrapper performs heap release when invoked through delete.
}

/* ================================================================== */
/* RenderConnectionPanel — 0x4421D0                                    */
/*                                                                     */
/* Operates on the singleton join-session/name-entry lobby panel, NOT  */
/* a DPlayManager (player slot). Confirmed via Ghidra: NameEntryPanel's */
/* own vtable slot [2] (+0x08, at 0x4781D8) is overridden with          */
/* NETMAN_JoinSession (0x441870), which ends by calling this function   */
/* on `this` — i.e. `panel` is a NameEntryPanel* (see NameEntryPanel.h). */
/*                                                                     */
/* Called by: NETMAN_JoinSession (0x441870), unnamed 0x442677          */
/* ================================================================== */
void RenderConnectionPanel(NameEntryPanel* panel)
{
    int32_t text_bottom;
    int32_t alignment;

    /* Mark panel as needing text refresh */
    panel->field_E8 = 1;

    /* Panel bounds (+0x18C..+0x19C, 16-byte RECT) */
    const RECT& panel_rect = panel->panelRect;

    /* Blit child surface to primary if visible flag set */
    if (panel->hasSprites != 0) {
        RECT src_rect;
        RECT dst_rect;

        CopyRect(&src_rect, &panel_rect);
        CopyRect(&dst_rect, &panel_rect);

        /* Offset source by scroll offset pair 1 (UI_WindowBase::workRect's
         * left/top, +0xD4/+0xD8) */
        OffsetRect(&src_rect, panel->workRect.left, panel->workRect.top);

        /* Offset destination by scroll offset pair 2 (+0x14C/+0x150) */
        OffsetRect(&dst_rect, panel->scrollOffsetX2, panel->scrollOffsetY2);

        UIPANEL_Blit(
            panel->childSurface,                      /* +0x1D0: child surface */
            src_rect.left, src_rect.top,
            src_rect.right, src_rect.bottom,
            g_primary_surface,                        /* global primary surface */
            dst_rect.left, dst_rect.top,
            dst_rect.right, dst_rect.bottom,
            1);                                        /* blit flags */
    }

    /* Begin painting */
    void* hdc = UIPANEL_BeginPaint(panel);

    /* Select the stock font (DAT_004855fc) */
    void* hFont = *reinterpret_cast<void**>(0x4855FC);
    void* oldFont = SelectObject(hdc, hFont);

    /* Set up text drawing rect (+0x130) from the panel bounds */
    RECT* draw_rect = &panel->textDrawRect;
    draw_rect->left   = panel_rect.left;
    draw_rect->top    = panel_rect.top;
    draw_rect->right  = panel_rect.right;
    draw_rect->bottom = panel_rect.bottom;

    /* Draw text from buffer (+0xF0) */
    text_bottom = DrawTextA(
        hdc,
        panel->textBuffer,                  /* +0xF0: text buffer (64 chars) */
        -1,                                 /* null-terminated */
        draw_rect,
        0x420                               /* DT_CENTER | DT_WORDBREAK */
    );

    /* Restore old font */
    SelectObject(hdc, oldFont);

    UIPANEL_EndPaintEx(
        panel,
        static_cast<int32_t>(reinterpret_cast<intptr_t>(panel->hWnd)),  /* +0x08: panel handle/HWND — ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param */
        static_cast<int32_t>(reinterpret_cast<intptr_t>(hdc)),
        1,                              /* repaint flag */
        nullptr
    );

    /* Update bottom of text rect */
    draw_rect->bottom = text_bottom - 4 + draw_rect->top;

    /* Center text rect within panel */
    UI_CenterWindow(&panel->panelRect, draw_rect);

    /* Apply alignment mode (+0x140, dual-use with gameMode) */
    alignment = panel->gameMode;
    if (alignment == 0) {
        /* Mode 0: Right-align — offset to panel right edge */
        OffsetRect(draw_rect, panel_rect.right - draw_rect->left, 0);
    } else if (alignment == 1) {
        /* Mode 1: Left-align — offset to panel left edge */
        OffsetRect(draw_rect, panel_rect.left - draw_rect->right, 0);
    } else if (alignment == 2) {
        /* Mode 2: Bottom-align — offset to panel bottom edge */
        OffsetRect(draw_rect, 0, panel_rect.bottom - draw_rect->top);
    } else {
        /* Mode 3+: Top-align — offset to panel top edge */
        OffsetRect(draw_rect, 0, panel_rect.top - draw_rect->bottom);
    }
}

/* ================================================================== */
/* CreatePlayer — 0x442850                                            */
/*                                                                     */
/* Called by: NET_ResolveAddress, CGWND_VehicleEditor_Ctor,           */
/*            INPUT_InitNetworkPlayer, Train_HandleLobbyInfo,          */
/*            Train_HandleConnectionSetup, Train_ConnectToServer,       */
/*            Train_HandleJoinMultiplayer                              */
/* ================================================================== */
void DPlayManager::CreatePlayer()
{
    int32_t i;

    /* Zero flags */
    m_sessionBlk1[0] = 0;               /* +0x10 */
    m_sessionBlk1[20] = 0;              /* +0x24 */
    m_sessionBlk2[0] = 0;               /* +0x25 */

    /* Dynamic dispatch is compiler-managed in reconstructed C++. */

    /* Zero more flags */
    m_flag40 = 0;                               /* +0x40 */
    m_flag41 = 0;                               /* +0x41 */
    m_flag42 = 0;                               /* +0x42 */

    /* Set default values */
    m_wordValue = 0;                            /* +0x3A */
    m_dwordValue = 1;                           /* +0x3C */
    m_playerName[0] = 0;                        /* +0x43 */
    m_magic = 0x66;                             /* +0x04 */
    m_unknown93 = 0;                            /* +0x93 */
    m_configId = 0;                             /* +0x0C */

    /* 0x44289C reads the canonical PlayerConfig field at original +0x18. */
    m_colorId = g_player_config != nullptr ? g_player_config->player_id : 0;

    m_playerType = 0;                           /* +0x94 */
    m_playerTrack = 0;                          /* +0x95 */

    /* Zero first two bytes of each track entry */
    for (i = 0; i < 128; i++) {
        m_trackEntries[i * 6] = 0;
        m_trackEntries[i * 6 + 1] = 0;
    }
}

/* ================================================================== */
/* InitPlayerFromSession — 0x4428E0                                           */
/*                                                                     */
/* Called by: DPLAY_EnumerateSessions (0x442FD9)                      */
/* ================================================================== */
DPlayManager* DPlayManager::InitPlayerFromSession(const DPLAY_SessionData* session)
{
    int32_t i;
    int32_t entry_count;

    /* Clear session data block starts */
    m_sessionBlk1[0] = 0;
    m_sessionBlk1[20] = 0;
    m_sessionBlk2[0] = 0;

    /* Dynamic dispatch is compiler-managed in reconstructed C++. */
    inline_memcpy(m_sessionBlk1, session->data_blk1, sizeof(m_sessionBlk1));
    inline_memcpy(m_sessionBlk2, session->data_blk2, sizeof(m_sessionBlk2));
    inline_memcpy(m_playerName, session->player_name, sizeof(m_playerName));
    m_unknown93 = session->unknown_8B;
    m_playerType = session->session_type;
    m_playerTrack = session->session_track;
    m_wordValue = session->word_value;
    m_dwordValue = session->dword_value;
    m_flag40 = session->flag_38;
    m_flag41 = session->flag_39;
    m_flag42 = session->flag_3A;

    entry_count = session->entry_count;
    if (entry_count > 128) entry_count = 128;
    inline_memcpy(m_trackEntries, session->track_entries, entry_count * 6);

    /* Clear remaining entries (if entry_count < 128) */
    for (i = entry_count; i < 128; i++) {
        m_trackEntries[i * 6] = 0;
        m_trackEntries[i * 6 + 1] = 0;
    }

    m_magic = 0x66;
    return this;
}

#ifndef _WIN32
/** Host wire adapter corresponding to DPlayManager::InitPlayerFromSession (0x4428E0).
 * Native vptr width makes reinterpret_cast<DPLAY_SessionData*> invalid. */
bool DPlayManager::LoadLegacySessionWire(const uint8_t* session, size_t size)
{
    if (session == nullptr || size != 0x390) return false;
    inline_memcpy(m_sessionBlk1, session + 0x08, sizeof(m_sessionBlk1));
    inline_memcpy(m_sessionBlk2, session + 0x1D, sizeof(m_sessionBlk2));
    m_wordValue = static_cast<uint16_t>(session[0x32]) |
                  static_cast<uint16_t>(session[0x33] << 8);
    m_dwordValue = static_cast<int32_t>(
        static_cast<uint32_t>(session[0x34]) |
        (static_cast<uint32_t>(session[0x35]) << 8) |
        (static_cast<uint32_t>(session[0x36]) << 16) |
        (static_cast<uint32_t>(session[0x37]) << 24));
    m_flag40 = session[0x38];
    m_flag41 = session[0x39];
    m_flag42 = session[0x3A];
    inline_memcpy(m_playerName, session + 0x3B, sizeof(m_playerName));
    m_unknown93 = session[0x8B];
    m_playerType = session[0x8C];
    m_playerTrack = session[0x8D];
    const uint16_t wire_count = static_cast<uint16_t>(session[0x8E]) |
                                static_cast<uint16_t>(session[0x8F] << 8);
    const uint16_t entry_count = wire_count > 128 ? 128 : wire_count;
    inline_memcpy(m_trackEntries, session + 0x90, entry_count * 6);
    for (uint16_t index = entry_count; index < 128; ++index) {
        m_trackEntries[index * 6] = 0;
        m_trackEntries[index * 6 + 1] = 0;
    }
    m_magic = 0x66;
    return true;
}
#endif

/* ================================================================== */
/* CleanupPlayer — 0x442A00                                           */
/* ================================================================== */
void DPlayManager::CleanupPlayer()
{
    /* Dynamic dispatch is compiler-managed in reconstructed C++. */
}

/* ================================================================== */
/* CopyPlayerData — 0x4426D0                                          */
/*                                                                     */
/* Called by: Train_ProcessMessages (0x439977)                        */
/*                                                                     */
/* NOTE — offset +0x00 is DPlayManager's vtable slot (confirmed:        */
/* DPlayManager::CleanupPlayer/0x442A00 is literally                   */
/* `*this = &PTR_LAB_00478264;`, a dedicated vtable-repair routine).    */
/* The original binary genuinely overwrites that slot with the packet's */
/* first dword ("ID") here — faithfully transcribed below, not a bug    */
/* introduced by this pass (CLAUDE.md: preserve exact numeric semantics */
/* absent a *confirmed* fix). On this 64-bit host the 4-byte store only */
/* clobbers the low half of the 8-byte vptr, leaving it malformed       */
/* rather than merely stale, for however long it takes CleanupPlayer to */
/* run. TODO: trace every path from Train_ProcessMessages (0x439977) to */
/* confirm no virtual dispatch (e.g. ~DPlayManager()) happens on this    */
/* object between the clobber and the next CleanupPlayer call before    */
/* treating this as safe on a 64-bit vptr. Also note SetPlayerData/     */
/* GetPlayerName (this same class) treat this+4 as the start of the     */
/* 0x398-byte wire view — which is what makes +0x00 look like a stray   */
/* field here rather than the vtable pointer it actually is.            */
/* ================================================================== */
void DPlayManager::CopyPlayerData(const void* packet_ptr)
{
    const uint8_t* pkt = static_cast<const uint8_t*>(packet_ptr);

    /* Packet layout:
     *   0x00: dword ID      -> this+0x00 (vtable slot — see NOTE above)
     *   0x3A: byte flag     -> this+0x04
     *   0x0C: string1       -> this+0x05
     *   0x19: string2       -> this+0x12
     *   0x04: dword val     -> this+0x32
     *   0x39: byte flag     -> this+0x36
     *   0x08: dword val2    -> this+0x48
     */

    *reinterpret_cast<int32_t*>(this) = *reinterpret_cast<const int32_t*>(pkt);   /* +0x00 */
    reinterpret_cast<uint8_t*>(this)[4] = pkt[0x3A];                             /* +0x04 */
    inline_memcpy(reinterpret_cast<uint8_t*>(&m_magic) + 1, pkt + 0x0C, 13);   /* +0x05: string1 up to +0x12 */
    inline_memcpy(&m_sessionBlk1[2], pkt + 0x19, 32);         /* +0x12: string2 (up to 32 bytes) */
    *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(this) + 0x32) =
        *reinterpret_cast<const int32_t*>(pkt + 4);                             /* +0x32 */
    reinterpret_cast<uint8_t*>(this)[0x36] = pkt[0x39];                         /* +0x36 */
    *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(this) + 0x48) =
        *reinterpret_cast<const int32_t*>(pkt + 8);                             /* +0x48 */
}

/* ================================================================== */
/* InitPlayerSlot — 0x442750                                          */
/*                                                                     */
/* Called by: NETMAN_SyncGameState (0x43FCCD)                         */
/* ================================================================== */
void DPlayManager::InitPlayerSlot(const DPlayManager* source)
{
    /* Same-layout copy of all relevant fields. +0x00 is the vtable slot —
     * see the NOTE above CopyPlayerData; the original copies it byte-for-byte
     * from `source` the same way. */

    *reinterpret_cast<int32_t*>(this) = *reinterpret_cast<const int32_t*>(source);   /* +0x00: vtable/ID */
    reinterpret_cast<uint8_t*>(this)[4] = reinterpret_cast<const uint8_t*>(source)[4]; /* +0x04: flag */

    inline_memcpy(reinterpret_cast<uint8_t*>(&m_magic) + 1,
                  reinterpret_cast<const uint8_t*>(&source->m_magic) + 1, 13); /* +0x05: string1 */
    inline_memcpy(&m_sessionBlk1[2], &source->m_sessionBlk1[2], 32);                 /* +0x12: string2 */

    *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(this) + 0x32) =
        *reinterpret_cast<const int32_t*>(reinterpret_cast<const uint8_t*>(source) + 0x32);  /* +0x32 */
    reinterpret_cast<uint8_t*>(this)[0x36] = reinterpret_cast<const uint8_t*>(source)[0x36]; /* +0x36 */
    *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(this) + 0x48) =
        *reinterpret_cast<const int32_t*>(reinterpret_cast<const uint8_t*>(source) + 0x48);  /* +0x48 */
}

void DPlayManager::CopyLogicalStateFrom(const DPlayManager& source)
{
    m_magic = source.m_magic;
    m_colorId = source.m_colorId;
    m_configId = source.m_configId;
    inline_memcpy(m_sessionBlk1, source.m_sessionBlk1, sizeof(m_sessionBlk1));
    inline_memcpy(m_sessionBlk2, source.m_sessionBlk2, sizeof(m_sessionBlk2));
    m_flag39 = source.m_flag39;
    m_wordValue = source.m_wordValue;
    m_dwordValue = source.m_dwordValue;
    m_flag40 = source.m_flag40;
    m_flag41 = source.m_flag41;
    m_flag42 = source.m_flag42;
    inline_memcpy(m_playerName, source.m_playerName, sizeof(m_playerName));
    m_unknown93 = source.m_unknown93;
    m_playerType = source.m_playerType;
    m_playerTrack = source.m_playerTrack;
    inline_memcpy(m_trackEntries, source.m_trackEntries, sizeof(m_trackEntries));
}

/* ================================================================== */
/* FreePlayerSlot — 0x4427D0                                          */
/*                                                                     */
/* Called by: NETMAN_ReceiveLayoutSelect (0x4400B6)                   */
/* ================================================================== */
void DPlayManager::FreePlayerSlot(void* packet_ptr)
{
    uint8_t* pkt = static_cast<uint8_t*>(packet_ptr);

    /* Reverse mapping of CopyPlayerData:
     *   this+0x00  -> packet+0x00  (dword ID; vtable slot, see NOTE above CopyPlayerData)
     *   this+0x04  -> packet+0x3A  (byte flag)
     *   this+0x05  -> packet+0x0C  (string1)
     *   this+0x12  -> packet+0x19  (string2)
     *   this+0x32  -> packet+0x04  (dword val)
     *   this+0x36  -> packet+0x39  (byte flag)
     *   this+0x48  -> packet+0x08  (dword val2)
     */

    *reinterpret_cast<int32_t*>(pkt) = *reinterpret_cast<int32_t*>(this);             /* +0x00 */
    pkt[0x3A] = reinterpret_cast<uint8_t*>(this)[4];                                 /* +0x04 -> +0x3A */
    inline_memcpy(pkt + 0x0C, reinterpret_cast<uint8_t*>(&m_magic) + 1, 13);      /* +0x05 -> +0x0C */
    inline_memcpy(pkt + 0x19, &m_sessionBlk1[2], 32);            /* +0x12 -> +0x19 */
    *reinterpret_cast<int32_t*>(pkt + 4) =
        *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(this) + 0x32);        /* +0x32 -> +0x04 */
    pkt[0x39] = reinterpret_cast<uint8_t*>(this)[0x36];                             /* +0x36 -> +0x39 */
    *reinterpret_cast<int32_t*>(pkt + 8) =
        *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(this) + 0x48);        /* +0x48 -> +0x08 */
}

/* ================================================================== */
/* GetPlayerData — 0x442A10                                           */
/*                                                                     */
/* Called by: Train_SendPlayerInfo (0x43CD89)                         */
/* ================================================================== */
void* DPlayManager::GetPlayerData()
{
    DPLAY_SessionData* session;
    void* result;

    void* storage = operator_new(sizeof(DPLAY_SessionData));
    if (storage != nullptr) {
        session = ::new (storage) DPLAY_SessionData;
        InitSessionDataSnapshot(session, this);
        result = session;
    } else {
        result = nullptr;
    }
    return result;
}

/* ================================================================== */
/* SetPlayerData — 0x442A70                                           */
/*                                                                     */
/* Called by: NET_RegisterPlayer (0x444EAD)                           */
/* ================================================================== */
int32_t DPlayManager::SetPlayerData(const char* name)
{
    char filename[0x504];                /* stack buffer for filename */
    void* hFile;
    uint32_t bytesWritten;
    char* config_str;
    int32_t result;

    /* Initialize filename buffer */
    filename[0] = g_empty_string;
    /* Zero the rest of the buffer (0x140 dwords = 0x500 bytes) */
    {
        uint32_t* p = reinterpret_cast<uint32_t*>(&filename[1]);
        int32_t i;
        for (i = 0; i < 0x140; i++) {
            p[i] = 0;
        }
    }
    filename[0x501] = 0;
    filename[0x502] = 0;

    /* Get player config string and parse to int */
    config_str = PlayerConfig_SaveToFile(g_player_config);
    m_configId = CRT_atoi(config_str);            /* +0x0C */

    /* Build filename: "%s_%s.crd" */
    wsprintfA(filename, FMT_PLAYER_DATA, name, config_str);

    /* Create file for writing */
    hFile = CreateFileA(
        filename,
        0x40000000,         /* GENERIC_WRITE */
        1,                  /* FILE_SHARE_READ */
        nullptr,
        2,                  /* CREATE_ALWAYS */
        0x8000000,          /* FILE_FLAG_SEQUENTIAL_SCAN */
        nullptr
    );

    if (hFile == DPLAY_INVALID_HANDLE_VALUE) {
        return 0xFFFFFF00;  /* INVALID_HANDLE_VALUE */
    }

    /* Write 0x398 bytes starting from this+4 (skip vtable) */
    if (!WriteFile(hFile, reinterpret_cast<const uint8_t*>(this) + 4, 0x398, &bytesWritten,
                   nullptr)) {
        CloseHandle(hFile);
        return 0xFFFFFF00;
    }

    CloseHandle(hFile);
    return 1;
}

/* ================================================================== */
/* GetPlayerName — 0x442B50                                           */
/*                                                                     */
/* Called by: NET_ResolveAddress (0x444CC0)                           */
/* ================================================================== */
int32_t DPlayManager::GetPlayerName(const char* path)
{
    void* hFile;
    uint32_t bytesRead;

    bytesRead = 0;

    if (path == nullptr) {
        return 0;
    }

    /* Open existing file for reading */
    hFile = CreateFileA(
        path,
        0x80000000,         /* GENERIC_READ */
        1,                  /* FILE_SHARE_READ */
        nullptr,
        3,                  /* OPEN_EXISTING */
        0x8000000,          /* FILE_FLAG_SEQUENTIAL_SCAN */
        nullptr
    );

    if (hFile == DPLAY_INVALID_HANDLE_VALUE) {
        return 0xFFFFFF00;  /* INVALID_HANDLE_VALUE */
    }

    /* Read 0x398 bytes into this+4 (after vtable) */
    if (!ReadFile(hFile, &m_magic, 0x398, &bytesRead, nullptr)) {
        CloseHandle(hFile);
        return 0xFFFFFF00;
    }

    /* Validate magic word */
    if (m_magic != 0x66) {
        m_magic = 0x66;              /* Force-set magic on mismatch */
        CloseHandle(hFile);
        return 0xFFFFFF00;            /* Data integrity failure */
    }

    CloseHandle(hFile);
    return 1;
}

/* ================================================================== */
/* SetPlayerName — 0x442BF0                                           */
/*                                                                     */
/* Called by: NETMAN_ReceiveSignalChange (0x43EE34),                  */
/*            NETMAN_DeserializePlayerData (0x440B97)                 */
/* ================================================================== */
void DPlayManager::SetPlayerName(int32_t trainId, int8_t specific)
{
    int32_t rnd;
    uint8_t abs_val;

    switch (trainId) {
    case 0:
        /* Disconnect / remove from track */
        m_playerType = 0;
        m_playerTrack = 0;
        break;

    case 1:
        /* Track 1 — random type 1..2 or explicit */
        m_playerTrack = 1;
        if (specific == -1) {
            rnd = CRT_rand();
            /* Absolute value of (rnd % 2), then +1 gives 1 or 2 */
            abs_val = static_cast<uint8_t>((rnd ^ (rnd >> 31)) - (rnd >> 31));
            m_playerType = (abs_val & 1) + 1;
        } else {
            m_playerType = specific;
        }
        break;

    case 2:
        /* Track 2 — default type 2 or explicit */
        m_playerTrack = 2;
        if (specific == -1) {
            m_playerType = 2;
        } else {
            m_playerType = specific;
        }
        break;

    case 3:
        /* Fixed: track 2, type 1 */
        m_playerTrack = 2;
        m_playerType = 1;
        break;

    default:
        /* No change */
        break;
    }
}

/* ================================================================== */
/* InitPlayer — 0x442C90                                              */
/* ================================================================== */
uint8_t DPlayManager::InitPlayer(uint8_t packedHigh, uint8_t typeLow,
                                              uint8_t signalType, int32_t xPos, int32_t yPos,
                                              uint8_t flag3, uint8_t flag5)
{
    int32_t entry_idx = -1;
    int32_t i;

    /* Scan for first empty entry (signal_type byte at offset 1 == 0) */
    for (i = 0; i < 128; i++) {
        if (m_trackEntries[i * 6 + 1] == 0) {
            entry_idx = i;
            break;
        }
    }

    if (entry_idx < 0) {
        /* No empty slot found: force-clear first entry's type */
        m_trackEntries[1] = 0;  /* Clear entry[0].signal_type */
        /* DPLAY_SetSessionName(this) called here in original — not stub */
        entry_idx = 0x7F;                    /* Use last slot */
    }

    /* Populate track entry at this + 0x96 + entry_idx * 6 */
    {
        uint8_t* entry = &m_trackEntries[entry_idx * 6];

        /* Entry layout (6 bytes):
         *   byte 0: packed_flags = (typeLow - 1) | (packedHigh << 3)
         *   byte 1: signal_type
         *   byte 2: x_pos / 2
         *   byte 3: flag3
         *   byte 4: y_pos / 2
         *   byte 5: flag5
         */
        entry[0] = (typeLow - 1) | (packedHigh << 3);
        entry[1] = signalType;
        entry[2] = static_cast<uint8_t>(xPos / 2);
        entry[3] = flag3;
        entry[4] = static_cast<uint8_t>(yPos / 2);
        entry[5] = flag5;
    }

    /* Return 1 if appended as new entry (was full), 0 if reused empty */
    return (entry_idx < 0) ? 1 : 0;
}

/* ================================================================== */
/* GetSessionName — 0x442D30                                          */
/*                                                                     */
/* Hit-test track entries at screen (x, y). On hit, clears the entry   */
/* and compacts the array.                                             */
/*                                                                     */
/* Called by: PostcardGame_HandleClick (0x41A98B),                    */
/*            Town_HandleClick (0x41C28E)                              */
/* ================================================================== */
uint8_t DPlayManager::GetSessionName(int32_t x, int32_t y)
{
    int32_t i;

    /* Scan from last entry (127) down to first (0) */
    for (i = 127; i >= 0; i--) {
        uint8_t* entry = m_trackEntries + i * 6;  /* +0x96 */

        uint8_t signal_type = entry[1];  /* byte 1: signal_type */
        if (signal_type == 0) {
            continue;  /* empty entry */
        }

        /* Compute bounding rect from entry fields.
         * Entry -> rect mapping:
         *   x_center = packed_flags * 2    (byte 0 * 2)
         *   y_center = (y_pos/2) * 2       (byte 4 * 2)
         *   width    = flag3               (byte 3)
         *   height   = flag5               (byte 5)
         *   rect = (x_center - width/2, y_center - height/2,
         *           x_center + width/2, y_center + height/2)
         */
        int32_t x_center = static_cast<int32_t>(entry[0]) * 2;     /* byte 0: packed_flags */
        int32_t width    = static_cast<int32_t>(entry[3]);          /* byte 3: flag3 = width */
        int32_t y_center = static_cast<int32_t>(entry[4]) * 2;     /* byte 4: y_pos/2 = y_center */
        int32_t height   = static_cast<int32_t>(entry[5]);          /* byte 5: flag5 = height */

        RECT entry_rect;
        entry_rect.left   = x_center - (width >> 1);
        entry_rect.right  = entry_rect.left + width;
        entry_rect.top    = y_center - (height >> 1);
        entry_rect.bottom = entry_rect.top + height;

        /* Clamp negative coords to 0 */
        if (entry_rect.left < 0)   entry_rect.left = 0;
        if (entry_rect.top < 0)    entry_rect.top = 0;

        /* Hit-test */
        POINT pt;
        pt.x = x;
        pt.y = y;

        if (PtInRect(&entry_rect, pt) != 0) {
            /* Hit — clear this entry's signal_type and compact array */
            entry[1] = 0;  /* m_trackEntries[i * 6 + 1] = 0 */
            SetSessionName();
            return 1;
        }
    }

    return 0;  /* no hit */
}

/* ================================================================== */
/* SetSessionName — 0x442E00                                          */
/*                                                                     */
/* Compact the track entry array: shifts non-empty entries forward to  */
/* fill gaps left by cleared entries.                                  */
/*                                                                     */
/* Algorithm: for each entry, if it is empty (signal_type==0), scan    */
/* forward for the next non-empty entry and move it back to fill the   */
/* gap. This ensures all active entries are contiguous at the front.   */
/*                                                                     */
/* Called by: GetSessionName (0x442DE0), InitPlayer (0x442CC1)        */
/* ================================================================== */
void DPlayManager::SetSessionName()
{
    int32_t dst;  /* destination index (gap to fill) */
    int32_t src;  /* source index (non-empty entry to move) */

    for (dst = 0; dst < 128; dst++) {
        uint8_t* dst_entry = m_trackEntries + dst * 6;

        /* If current entry is occupied, move on */
        if (dst_entry[1] != 0) {
            continue;
        }

        /* Found a gap: scan forward for next non-empty entry */
        src = dst + 1;
        while (src < 128) {
            uint8_t* src_entry = m_trackEntries + src * 6;
            if (src_entry[1] != 0) {
                break;  /* found one */
            }
            src++;
        }

        if (src >= 128) {
            return;  /* no more entries to compact */
        }

        /* Move the non-empty entry from src back to dst */
        {
            uint8_t* s = m_trackEntries + src * 6;
            /* Copy all 6 bytes */
            dst_entry[0] = s[0];  /* packed_flags */
            dst_entry[1] = s[1];  /* signal_type */
            dst_entry[2] = s[2];  /* x_pos/2 */
            dst_entry[3] = s[3];  /* flag3 */
            dst_entry[4] = s[4];  /* y_pos/2 */
            dst_entry[5] = s[5];  /* flag5 */
            /* Clear source entry's signal_type to mark empty */
            s[1] = 0;
        }
    }
}

/* ================================================================== */
/* EnumerateSessions — 0x442FA0                                       */
/*                                                                     */
/* Factory: allocate new DPlayManager and populate from session data.  */
/* Static __fastcall: ECX carries the sole session-data argument.       */
/*                                                                     */
/* Called by: Train_HandleTrackBuild (0x43CEFD)                       */
/* ================================================================== */
DPlayManager* __fastcall DPlayManager::EnumerateSessions(
    const DPLAY_SessionData* session)
{
    void* new_slot = operator_new(sizeof(DPlayManager));
    if (new_slot != nullptr) {
        DPlayManager* slot = ::new (new_slot) DPlayManager;
        return slot->InitPlayerFromSession(session);
    }
    return nullptr;
}
