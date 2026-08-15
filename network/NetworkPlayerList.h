/**
 * NetworkPlayerList.h — DPLAY-level player list / surface cache class
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * NetworkPlayerList is the top-level DPLAY object that manages player
 * slot data, per-player rendering state, and a bitmap surface cache
 * for the multiplayer lobby UI. It has a frame counter for LRU cache
 * eviction, 256 cached UIPANEL surfaces keyed by 3-byte tags, and
 * a player name array for the lobby.
 *
 * Original x86 size: 0xBE4 bytes
 * Binary dispatch table: 0x47826C
 *   [0] scalar deleting destructor (NetworkPlayerList_Term, 0x4431F0)
 *
 * === Architecture ===
 *
 * The class has 4 main sections:
 * 1. Surface cache (offsets +0x04 to +0x807):
 *    256 surface pointers + 256 per-surface LRU timestamps
 * 2. Frame counter (+0x404)
 * 3. Tag array (+0x808): 256 entries of 3 bytes each
 * 4. Player data (+0xB08 to +0xBE2):
 *    - +0xB08: resource manager pointer
 *    - +0xB0C: resource data pointer
 *    - +0xB10: message count (int16 cache, -1 = uncached)
 *    - +0xB12: enumeration flag
 *    - +0xB13: 16 player names @ 0xD bytes each = 0xD0 bytes
 *
 * === Methods ===
 *
 * These are C++ methods (__thiscall / __fastcall with ECX = this):
 *   NET_GetOrCreateSurface    — Lookup or create cached surface
 *   NET_RenderTrackEntry      — Blit track entry surface to HDC
 *   DPLAY_RenderPlayer        — Render full player list UI entry
 *   DPLAY_RenderSessionFrame  — Render session frame overlay
 *   DPLAY_RenderSessionBase   — Render session base overlay
 *   DPLAY_PeekMessage         — Render track entry and manage resource
 *   NET_RegisterPlayer        — Register player via .crd file
 *   NET_UnregisterPlayer      — Unregister player via file deletion
 *   NET_GetPlayerAddress      — Delete player .crd file
 */

#pragma once

#include "../shared/types.h"
#include "PostBagFileNode.h"

// Status: TRANSCRIBED
/* Dispatch-table addresses are documentation only; C++ manages dispatch. */

struct UIPANEL_Surface;
class DPlayManager;
/* ================================================================== */
/* DPlayPlayer struct (a fictional partial view of the 0x39C-byte DPLAY */
/* player record, matching only +0x3A..+0x43) removed 2026-08-14: it   */
/* was never independently evidenced and its own comment in            */
/* RenderPlayer (this file's .cpp, below) already flagged that it did  */
/* not match every offset RenderPlayer reads. The real type is         */
/* DPlayManager (network/DPlayManager.h, full 0x39C layout modeled) —  */
/* see input/Cursor.cpp's obj_184 usage for the independent confirming */
/* evidence, and RenderPlayer's own updated comment for what remains   */
/* unresolved about `playerData`'s exact identity in this file. */

/* ================================================================== */
/* NetworkPlayerList — Top-level DPLAY player/surface cache class      */
/*                                                                     */
/* Original x86 size: 0xBE4 bytes                                      */
/* Vtable: 0x47826C                                                    */
/* ================================================================== */
class NetworkPlayerList {
public:
    virtual ~NetworkPlayerList();

    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */

    /* +0x00: compiler-managed dispatch pointer (binary table 0x47826C) */
    /* vtable at +0x00 is compiler-managed via virtual methods */
    /* +0x04 to +0x403: Array of 256 surface pointers (4 bytes each) */
    UIPANEL_Surface* surface_cache[256];

    /* +0x404: Frame counter — incremented per render, used for LRU eviction */
    uint32_t    frame_counter;

    /* +0x408 to +0x807: Array of 256 LRU timestamps (4 bytes each) */
    uint32_t    lru_timestamps[256];

    /* +0x808 to +0xB07: Tag array — 256 entries of 3 bytes each */
    struct {
        uint8_t type_hi;     /* param_1 (high 5 bits) */
        uint8_t variant;     /* param_2 ((tag & 7) + 1) */
        uint8_t tag_low;     /* param_3 (low byte) */
    } tags[256];

    /* +0xB08: Resource manager pointer (cached) */
    void*       resource_mgr;

    /* +0xB0C: Resource data pointer (cached surface) */
    void*       resource_data;

    /* +0xB10: Message count cache (-1 = uncached, non-negative = cached) */
    int16_t     msg_count_cache;

    /* +0xB12: Enumeration flag (0 = not enumerated, 1 = done) */
    uint8_t     enumerated;

    /* +0xB13 to +0xBE2: Player name array (16 entries, 0xD bytes each) */
    char        player_names[16][13];

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * NetworkPlayerList constructor — Create PostBag directories, init cache.
     * Address: 0x443000
     *
     * The C++ constructor establishes dispatch through the normal
     * constructor chain and initializes the recovered fields,
     * zeros the surface cache, initializes frame counter to
     * 0, sets msg_count_cache to -1, zeros resource_mgr and resource_data,
     * sets enumerated to 0. Then creates all PostBag subdirectories:
     *   PostBag, Easter, Sort, Sort_In, Sort_Out, Sort_Bag,
     *   AlbIndex, Album, Att_In, Att_Out
     *
     * Called by: GameLoop_Setup (0x406CBC)
     *
     */
    NetworkPlayerList();

    /**
     * NetworkPlayerList destructor body.
     * Address: 0x4431F0
     *
     * The compiler supplies the deleting-destructor wrapper for virtual slot [0].
     *
     * Releases the resource manager sub-object, frees all 256 cached
     * surface pointers, and calls DPLAY_SendMessages for PostBag cleanup.
     * The compiler supplies heap release when the deleting wrapper is used.
     */

    /* ================================================================ */
    /* Methods                                                           */
    /* ================================================================ */

    /**
     * GetOrCreateSurface — Lookup or create a cached UIPANEL surface.
     * Address: 0x4442B0
     *
     * Searches the 256-entry cache for a surface matching the 3-byte
     * tag. On cache hit, copies and returns the surface. On cache miss,
     * constructs a filename from the tag and loads the .bmp via
     * UIPANEL_CreateSurface/StretchBlit from <install>\Clipart\.
     *
     * If the cache is full, evicts the least-recently-used entry (by
     * comparing lru_timestamps against frame_counter).
     *
     * @param type_hi   High 5 bits of packed type (determines filename prefix)
     * @param variant   Low 3 bits + 1 (determines filename number)
     * @param tag_low   Third tag byte (determines filename extension number)
     * @param no_evict  If non-zero, skip eviction when cache is full
     * @return          UIPANEL surface pointer, or NULL if not found/created
     */
    UIPANEL_Surface* GetOrCreateSurface(uint8_t type_hi, uint8_t variant,
                                        uint8_t tag_low, uint8_t no_evict);

    /**
     * RenderTrackEntry — Blit a cached track entry surface onto a HDC.
     * Address: 0x4440A0 (formerly NET_RenderTrackEntry)
     *
     * Gets or creates a surface for the track entry's packed type,
     * computes blit rectangle from position/size, clips to target
     * bounds, and blits with UIPANEL_Blit.
     *
     * @param hdc        Target HDC context
     * @param clip_x     Clip region left coordinate
     * @param clip_y     Clip region top coordinate
     * @param clip_right Clip region right boundary
     * @param clip_bot   Clip region bottom boundary
     * @param entry      6-byte track entry (packed_flags, signal_type, x, flag3, y, flag5)
     */
    void RenderTrackEntry(void* hdc, uint32_t clip_x, uint32_t clip_y,
                                      int32_t clip_right, uint32_t clip_bot,
                                      const uint8_t entry[6]);

    /**
     * RenderPlayer — Render full player list UI entry with name, session data,
     * track piece icons, and postcard image.
     * Address: 0x4437C0 (formerly DPLAY_RenderPlayer)
     *
     * Uses GetOrCreateSurface for cached session/game surfaces and
     * RenderTrackEntry for each track entry in the player's slot.
     *
     * @param hdc         Target HDC
     * @param param2      Left/top region coordinates
     * @param param3      Context HDC
     * @param param4      Right/bottom region coordinates
     * @param param5      Rendering offset
     * @param param6      Additional parameter
     * @param param7      Optional highlight rectangle
     */
    void RenderPlayer(void* hdc, int32_t param2, void* param3,
                                  int32_t param4, int32_t param5,
                                  uint32_t param6, const void* param7);

    /**
     * RenderSessionFrame — Render session frame overlay onto HDC.
     * Address: 0x443F00 (formerly DPLAY_RenderSessionFrame)
     *
     * Gets session surface (tag 0x1E) and blits to target with offset.
     *
     * @param hdc  Target HDC
     */
    void RenderSessionFrame(void* hdc);

    /**
     * RenderSessionBase — Render session base overlay onto HDC.
     * Address: 0x443FF0 (formerly DPLAY_RenderSessionBase)
     *
     * Gets session base surface (tag 0x1F) and blits to target
     * with position parameters.
     *
     * @param hdc         Target HDC
     * @param param2      Additional parameter
     * @param param3      X position parameter
     * @param param4      Y position parameter
     * @param param5      Additional parameter
     * @param param6      Flag byte
     */
    void RenderSessionBase(void* hdc, uint32_t param2, int32_t param3,
                                       int32_t param4, uint32_t param5, uint8_t param6);

    /**
     * PeekMessage — Render track entries and manage resource cache.
     * Address: 0x4436C0 (formerly DPLAY_PeekMessage)
     *
     * MISNAMED — does NOT peek messages. Increments frame counter,
     * caches resource from ResourceManager_GetById if needed, renders
     * the last non-empty track entry, and draws a frame rect.
     *
     * @param player_slot  DPLAY_PlayerSlot data
     * @param hdc          Target HDC
     * @param param3       Clip offset X
     * @param param4       Clip offset Y
     * @param param5       Clip right bound
     * @param param6       Clip bottom bound
     * @param param7       Additional parameter
     */
    void PeekMessage(void* player_slot, void* hdc,
                                 uint32_t param3, uint32_t param4,
                                 int32_t param5, uint32_t param6,
                                 uint32_t param7);

    /**
     * EnumeratePlayers — Load cached player names from PostBag easter_usr file.
     * Address: 0x443260
     *
     * Loads up to 16 player names (13 bytes each) from a locale-specific
     * easter_usr file under PostBag\Easter\<language>. Names are read as
     * newline-separated entries, stored in player_names array. Guarded by
     * enumerated flag at +0xB12 — returns immediately if already enumerated.
     * Sets enumerated to 1 after successful load. Called from Cursor UI
     * when opening livery/cursor editor in multiplayer.
     *
     * Called by: Cursor_UpdateNetworkNames (0x416EEB), NETMAN_ReceiveSignalChange (0x43E941)
     */
    void EnumeratePlayers();

    /**
     * RegisterPlayer — Save a DPLAY_PlayerSlot to .crd file in PostBag.
     * Address: 0x444D00 (formerly NET_RegisterPlayer)
     *
     * Constructs filepath based on PostBag subdirectory type, calls
     * DPlayManager::SetPlayerData to write the .crd file, updates the
     * message count cache by enumerating Sort_Out via NET_GetHostName.
     *
     * @param player_slot  Real DPlayManager instance to register
     * @param type         PostBag subdirectory type (0-7)
     * @param param3       Additional parameter (subdirectory suffix or 0)
     * @return             1 on success, 0xFFFFFF00 on write failure
     */
    uint32_t RegisterPlayer(DPlayManager* player_slot, int32_t type, int32_t param3);

    /**
     * UnregisterPlayer — Delete a player's .crd file from PostBag.
     * Address: 0x444FB0 (formerly NET_UnregisterPlayer)
     *
     * @param filepath  Full path to the .crd file to delete
     */
    void UnregisterPlayer(const char* filepath);

    /**
     * GetPlayerAddress — Delete a player's .crd file by player slot ID.
     * Address: 0x445000 (formerly NET_GetPlayerAddress)
     *
     * Builds path from player_slot->m_configId, deletes the file,
     * and updates message count cache.
     *
     * @param player_slot  Real DPlayManager instance
     * @param type         PostBag subdirectory type (0-7)
     */
    void GetPlayerAddress(DPlayManager* player_slot, int32_t type);
};

/** Process-owned NetworkPlayerList singleton — original address 0x4FD3B0. */
extern NetworkPlayerList* g_dplay;

#if defined(_WIN32) && UINTPTR_MAX == 0xffffffffu
static_assert(sizeof(NetworkPlayerList) == 0xBE4,
              "NetworkPlayerList must retain the recovered x86 allocation size");
#endif
