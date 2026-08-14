/**
 * DPlayManager.h — DirectPlay player/session core wrapper
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * This file covers the low-level DPLAY_* network infrastructure that
 * wraps Microsoft DirectPlay for multiplayer. It manages player slot
 * data structures, session snapshots, player-type/track assignment,
 * and file-backed player state persistence.
 *
 * === Architecture ===
 *
 * DPlayManager IS the player slot structure. The `this` pointer in
 * all instance methods points directly to the DPLAY_PlayerSlot data.
 * Two data structures are managed:
 *
 * 1. Player Slot (vtable 0x478264, size 0x39C bytes)
 *    The in-memory player slot. Holds player identity (name, color ID,
 *    config ID), session-related state flags, and 128 track entries
 *    (6 bytes each, describing train/signal layout per track segment).
 *    Persisted to .crd files as 0x398 bytes starting at offset +4.
 *
 * 2. SessionSnapshot (vtable 0x478268, size 0x390 bytes)
 *    A serialized snapshot of a player slot, used for network
 *    transmission. Contains the same logical data in a different
 *    field layout. Created by GetPlayerData, consumed by InitPlayerFromSession.
 *
 * === Vtable 0x478264 (DPlayManager / Player Slot) ===
 *   [0] +0x00: scalar deleting destructor (DPLAY_CleanupPlayer, 0x442A00)
 *
 * === Vtable 0x478268 (DPLAY_SessionData) ===
 *   [0] +0x00: scalar deleting destructor (dtor at 0x442EA0)
 *
 * === Calling convention note ===
 * MSVC compiled instance methods with both __thiscall and __fastcall
 * depending on optimization; their `this` pointer is in ECX. The static
 * EnumerateSessions factory at 0x442FA0 is different: its sole session
 * argument is passed in ECX under __fastcall, and it has no `this` pointer.
 *
 * === NOTE on DPLAY_Ctor (0x4421D0, renamed RenderConnectionPanel) ===
 * This function is a UI rendering method, NOT a class constructor. It
 * operates on a Panel-derived UI structure (used by NETMAN_JoinSession),
 * NOT on DPLAY_PlayerSlot. It is in the DPLAY_* group conceptually but
 * does NOT share field offsets with the player slot functions.
 */

#pragma once

#include "../shared/types.h"

// Status: TRANSCRIBED
/* vtable addresses in vtable_addrs.h — compiler manages vtables via virtual methods */
struct DPLAY_SessionData;

/* ================================================================== */
/* DPlayManager — DirectPlay player slot management class              */
/*                                                                     */
/* This IS the DPLAY_PlayerSlot structure (vtable 0x478264).           */
/* Size: 0x39C bytes                                                   */
/*                                                                     */
/* NOTE on editor-local reuse of session fields (2026-08-14): before a */
/* real network session ever applies (InitPlayerFromSession), m_wordValue,*/
/* m_dwordValue, m_unknown93, and the tail of m_sessionBlk1 are unused  */
/* scratch bytes from this class's own perspective — CreatePlayer() just */
/* zeros/defaults them. input/Cursor.cpp's editor UI (Cursor::obj_184,   */
/* a DPlayManager* constructed via init_network_player) legitimately     */
/* repurposes that same storage for local, not-yet-networked UI state    */
/* (upload_id, is_audio_preview, bonus_prize_id, a locomotive-list name) */
/* via direct field/byte access with its own doc comments at each site — */
/* not modeled as separate named fields here since nothing in this      */
/* class's own methods gives them that meaning (only color_r/g/b below  */
/* had independent confirmation from a second real caller). Once a real */
/* session is applied, these bytes take on this class's own semantics.  */
/* ================================================================== */
class DPlayManager {
public:
    virtual ~DPlayManager() {}

    /* ================================================================ */
    /* Fields (offsets from `this`)                                      */
    /* ================================================================ */

    /* +0x00 (5x): Vtable / Magic / IDs */
    /* +0x00: compiler-managed dispatch pointer (binary table 0x478264) */
    uint16_t    m_magic;             /* +0x04  always 0x66 — file format marker    */

    /* +0x06: Padding */
    uint8_t     _pad_06[2];

    /* +0x08: Player color/vehicle ID — from PlayerConfig+0x18 */
    int32_t     m_colorId;

    /* +0x0C: Player config ID — parsed from PlayerConfig_SaveToFile string */
    int32_t     m_configId;

    /* +0x10 (21 bytes): Session data block 1 — overwritten via InitPlayerFromSession */
    uint8_t     m_sessionBlk1[21];

    /* +0x25 (20 bytes): Session data block 2 — overwritten via InitPlayerFromSession */
    uint8_t     m_sessionBlk2[20];

    /* +0x39: Flag byte — from session. NetworkPlayerList::RenderPlayer
     * reads this as a "use custom color" gate before consuming
     * color_r/g/b below; no stronger evidence for other uses. */
    uint8_t     m_flag39;

    /* +0x3A: Word value — copied from session, initialized 0 */
    uint16_t    m_wordValue;

    /* +0x3C: Dword value — copied from session, initialized 1 */
    int32_t     m_dwordValue;

    /* +0x40 (3 bytes): Player color RGB triple. Confirmed by two independent
     * evidence sources (2026-08-14): NetworkPlayerList::RenderPlayer
     * (network/NetworkPlayerList.cpp, formerly DPLAY_RenderPlayer) computes
     * a fill color via NET_ComputeColor(slot+0x40, +0x41, +0x42) when
     * slot+0x39 (m_flag39) is set, to render another player's list-entry
     * background; input/Cursor.cpp's editor UI independently reads/writes
     * the same three bytes as color_r/g/b for the local player being
     * edited. Previously named m_flag40/41/42 ("byte flags from session")
     * — renamed once both call sites confirmed the same non-flag meaning. */
    uint8_t     color_r;
    uint8_t     color_g;
    uint8_t     color_b;

    /* +0x43 (80 bytes): Null-terminated player name */
    char        m_playerName[80];

    /* +0x93: Unknown byte — from session */
    uint8_t     m_unknown93;

    /* +0x94: Player type (0=none, 1=typeA, 2=typeB) */
    uint8_t     m_playerType;

    /* +0x95: Player track (0=off, 1=track1, 2=track2) */
    uint8_t     m_playerTrack;

    /* +0x96..+0x396: 128 track entries @ 6 bytes each = 0x300 bytes */
    uint8_t     m_trackEntries[128 * 6];

    /* ================================================================ */
    /* UI Render (operates on a Panel, NOT the player slot)              */
    /* ================================================================ */

    /* ================================================================ */
    /* Player Slot Lifecycle                                             */
    /* ================================================================ */

    /**
     * CreatePlayer — Initialize a new empty player slot.
     * Address: 0x442850
     *
     * Sets vtable=0x478264, magic=0x66, copies color ID from
     * g_player_config, and zeros all flags and track entries.
     *
     * Called by: NET_ResolveAddress, CGWND_VehicleEditor_Ctor,
     *   INPUT_InitNetworkPlayer, Train_HandleLobbyInfo,
     *   Train_HandleConnectionSetup, Train_ConnectToServer,
     *   Train_HandleJoinMultiplayer
     */
    void CreatePlayer();

    /**
     * InitPlayerFromSession — Deserialize a session snapshot into this slot.
     * Address: 0x4428E0
     *
     * Reads a DPLAY_SessionData and fills in the DPlayManager fields.
     * Reverse of GetPlayerData. Returns `this`.
     *
     * Called by: DPLAY_EnumerateSessions (0x442FD9)
     *
     * @param session  Source session snapshot to deserialize
     * @return         Pointer to this (the populated player slot)
     */
    DPlayManager* InitPlayerFromSession(const DPLAY_SessionData* session);
#ifndef _WIN32
    /** Host wire adapter for one original 0x390-byte DPLAY_SessionData.
     *  Layout evidence: DPlayManager::InitPlayerFromSession, address 0x4428E0. */
    bool LoadLegacySessionWire(const uint8_t* session, size_t size);
#endif

    /**
     * CleanupPlayer — Reset vtable (exception handler safety).
     * Address: 0x442A00
     *
     * Simply sets vtable to 0x478264. Used by __ehand__ unwind stubs.
     */
    virtual void CleanupPlayer();

    /* ================================================================ */
    /* Data Copy / Conversion (Packet <=> Player Slot)                   */
    /* ================================================================ */

    /**
     * CopyPlayerData — Convert compact packet format into this slot.
     * Address: 0x4426D0
     *
     * Copies fields from a compact network packet (~0x3B bytes) into
     * the DPlayManager field layout. Source and destination have
     * different field ordering.
     *
     * Called by: Train_ProcessMessages (0x439977)
     *
     * @param packet  Source compact packet data
     */
    void CopyPlayerData(const void* packet);

    /**
     * InitPlayerSlot — Copy from another player slot (same layout).
     * Address: 0x442750
     *
     * Field-by-field copy from source to this.
     *
     * Called by: NETMAN_SyncGameState (0x43FCCD)
     *
     * @param source  Source player slot to copy from
     */
    void InitPlayerSlot(const DPlayManager* source);
    // Layout-safe equivalent used by typed editor/session state.
    void CopyLogicalStateFrom(const DPlayManager& source);

    /**
     * FreePlayerSlot — Convert this slot to compact packet format.
     * Address: 0x4427D0
     *
     * Reverse of CopyPlayerData. Packs the slot into network packet
     * format for sending when a player leaves/frees a slot.
     *
     * Called by: NETMAN_ReceiveLayoutSelect (0x4400B6)
     *
     * @param packet  Destination compact packet buffer
     */
    void FreePlayerSlot(void* packet);

    /* ================================================================ */
    /* Session Snapshot (Player Slot <=> SessionData)                    */
    /* ================================================================ */

    /**
     * GetPlayerData — Allocate session snapshot from this player slot.
     * Address: 0x442A10
     *
     * Allocates a new DPLAY_SessionData (0x390 bytes via operator_new),
     * populates it from this slot via DestroySession, and returns it.
     *
     * Called by: Train_SendPlayerInfo (0x43CD89)
     *
     * @return  Newly allocated DPLAY_SessionData, or NULL on failure
     */
    void* GetPlayerData();

    /* ================================================================ */
    /* File I/O Persistence (.crd files)                                 */
    /* ================================================================ */

    /**
     * SetPlayerData — Write this slot to a .crd file.
     * Address: 0x442A70
     *
     * Builds filename "<name>_<config>.crd" and writes 0x398 bytes
     * from offset +4 (after vtable) via CreateFile/WriteFile.
     *
     * Called by: NET_RegisterPlayer (0x444EAD)
     *
     * @param name  Player name/identifier for filename construction
     * @return      1 on success, 0xFFFFFF00 on file error, 0 on write failure
     */
    int32_t SetPlayerData(const char* name);

    /**
     * GetPlayerName — Load this slot from a .crd file.
     * Address: 0x442B50
     *
     * Reads 0x398 bytes into offset +4. Validates magic at +4 == 0x66.
     *
     * Called by: NET_ResolveAddress (0x444CC0)
     *
     * @param path  File path to read from
     * @return      1 on success, 0 on NULL, 0xFFFFFF00 on file errors
     */
    int32_t GetPlayerName(const char* path);

    /* ================================================================ */
    /* Player Type / Track Assignment                                    */
    /* ================================================================ */

    /**
     * SetPlayerName — Set player_type and player_track.
     * Address: 0x442BF0
     *
     *   trainId 0:  type=0, track=0 (disconnected)
     *   trainId 1:  track=1. If specific==-1, random type (1 or 2).
     *   trainId 2:  track=2. If specific==-1, type=2.
     *   trainId 3:  track=2, type=1
     *
     * Called by: NETMAN_ReceiveSignalChange (0x43EE34),
     *   NETMAN_DeserializePlayerData (0x440B97)
     *
     * @param trainId   Track/type selection (0..3)
     * @param specific  Specific type value, or -1 for auto/random
     */
    void SetPlayerName(int32_t trainId, int8_t specific);

    /* ================================================================ */
    /* Track Entry Management                                            */
    /* ================================================================ */

    /**
     * InitPlayer — Add a track entry to the track array.
     * Address: 0x442C90
     *
     * Finds the first empty entry (signal_type == 0) and fills it.
     * Returns 1 if appended as new entry, 0 if reusing an empty slot.
     *
     * @param packedHigh   High bits for packed_flags (shifted left 3)
     * @param typeLow      Low bits for packed_flags (stored as typeLow-1)
     * @param signalType   Entry signal type byte
     * @param xPos         X position (stored / 2)
     * @param yPos         Y position (stored / 2)
     * @param flag3        Unknown flag
     * @param flag5        Unknown flag
     * @return             1 if appended, 0 if reused empty slot
     */
    uint8_t InitPlayer(uint8_t packedHigh, uint8_t typeLow,
                        uint8_t signalType, int32_t xPos, int32_t yPos,
                        uint8_t flag3, uint8_t flag5);

    /* ================================================================ */
    /* Hit-testing / Compaction                                          */
    /* ================================================================ */

    /**
     * GetSessionName — Hit-test track entries at (x, y) and clear if hit.
     * Address: 0x442D30
     *
     * Iterates all 128 track entries from the last entry backwards.
     * For each non-empty entry (signal_type != 0), computes a bounding
     * rect centered on the entry's position with its width/height flags,
     * and calls PtInRect to check if (x, y) falls inside. On hit:
     * clears the entry's signal_type byte and calls SetSessionName
     * to compact the array (shift subsequent entries to fill the gap).
     *
     * Entry -> rect mapping:
     *   x_center = packed_flags * 2,  width   = flag3
     *   y_center = (y_pos/2) * 2,     height  = flag5
     *   rect = (x_center - width/2, y_center - height/2,
     *           x_center + width/2, y_center + height/2)
     *
     * Called by:
     *   PostcardGame_HandleClick (0x41A98B)
     *   Town_HandleClick (0x41C28E)
     *
     * @param x  X screen coordinate to hit-test
     * @param y  Y screen coordinate to hit-test
     * @return   1 if an entry was hit and cleared, 0 otherwise
     */
    uint8_t GetSessionName(int32_t x, int32_t y);

    /**
     * SetSessionName — Compact track array by removing empty entries.
     * Address: 0x442E00
     *
     * Scans the 128-entry track array from front to back. When an empty
     * entry (signal_type == 0) is found, it searches forward for the
     * next non-empty entry and shifts it back to fill the gap, clearing
     * the source. This ensures all active entries are contiguous at the
     * front of the array.
     *
     * Called by:
     *   InitPlayer (0x442CC1) — after adding an entry (indirect)
     *   GetSessionName (0x442DE0) — on hit-and-clear
     *
     * Note: This is __fastcall (ECX = this), no stack return.
     */
    void SetSessionName();

    /* ================================================================ */
    /* Session Allocation Helper                                         */
    /* ================================================================ */

    /**
     * EnumerateSessions — Allocate new player slot from session data.
     * Address: 0x442FA0
     *
     * Allocates a new DPLAY_PlayerSlot (0x39C bytes via operator_new),
     * calls InitPlayerFromSession to populate it from the session snapshot, and
     * returns the new slot. Returns NULL on allocation failure.
     *
     * Uses SEH (__try/__except) for allocation failure recovery.
     *
     * Called by:
     *   Train_HandleTrackBuild (0x43CEFD)
     *
     * @param session  Source DPLAY_SessionData to populate from
     * @return         Newly allocated DPlayManager slot, or NULL on failure
     */
    static DPlayManager* __fastcall EnumerateSessions(
        const DPLAY_SessionData* session);
};

/* ================================================================== */
/* DPLAY_SessionData — serialized player snapshot for network use      */
/* Vtable: 0x478268                                                    */
/* Size:   0x390 bytes                                                 */
/* ================================================================== */
struct DPLAY_SessionData {
    /* +0x00: compiler-managed dispatch pointer (binary table 0x00478268) */
    /* vtable at +0x00 is compiler-managed via virtual methods */
    /** Destructor body, address: 0x442EA0. The scalar-deleting wrapper is compiler-generated. */
    virtual ~DPLAY_SessionData();

    /* +0x04 (4 bytes): Padding / reserved */
    uint8_t     _pad_04[4];

    /* +0x08 (21 bytes): Data block 1 — from player sessionBlk1 */
    uint8_t     data_blk1[21];

    /* +0x1D (20 bytes): Data block 2 — from player sessionBlk2 */
    uint8_t     data_blk2[20];

    /* +0x31: Byte pad */
    uint8_t     _pad_31;

    /* +0x32: Word value — from player m_wordValue */
    uint16_t    word_value;

    /* +0x34: Dword value — from player m_dwordValue */
    int32_t     dword_value;

    /* +0x38+3: Byte flags — from player m_flag40..m_flag42 */
    uint8_t     flag_38;
    uint8_t     flag_39;
    uint8_t     flag_3A;

    /* +0x3B: Player name string (null-terminated) */
    char        player_name[80];  /* up to offset 0x8B */

    /* +0x8B: Unknown byte — stored to player m_unknown93 */
    uint8_t     unknown_8B;

    /* +0x8C: Player type — stored to player m_playerType */
    uint8_t     session_type;

    /* +0x8D: Player track — stored to player m_playerTrack */
    uint8_t     session_track;

    /* +0x8E: Entry count (always 0x80 = 128) */
    uint16_t    entry_count;

    /* +0x90: Array of 128 track entries (6 bytes each = 0x300 bytes) */
    uint8_t     track_entries[128 * 6];
};

/* ================================================================== */
/* Extern declarations for globals used by these functions              */
/* ================================================================== */

class PlayerConfig;
class NameEntryPanel;
extern PlayerConfig* g_player_config; /* 0x4AA4A8 — PlayerConfig singleton */
extern char  g_empty_string;    /* 0x4851D0 — empty string constant  */
extern void* g_primary_surface; /* 0x4FD3C4 — primary DDraw surface  */

/**
 * RenderConnectionPanel — Draw the DirectPlay connection panel UI.
 * Address: 0x4421D0 (originally "DPLAY_Ctor")
 *
 * WARNING: This is a UI rendering method, NOT a player slot method.
 * `panel` is the singleton join-session/name-entry lobby panel — confirmed
 * via Ghidra: NameEntryPanel's own vtable slot [2] (+0x08) is overridden
 * with NETMAN_JoinSession (0x441870), which ends by calling this function
 * on `this`. See ui/NameEntryPanel.h for the full field layout; the fields
 * this function reads/writes are documented there (textBuffer, textDrawRect,
 * gameMode, scrollOffsetX2/Y2, panelRect, hasSprites, childSurface) plus
 * UI_WindowBase::hWnd/workRect (+0x08/+0xD4).
 *
 * Called by:
 *   NETMAN_JoinSession (0x441870)
 *   0x00442677 (unnamed, within DPLAY creation)
 */
void RenderConnectionPanel(NameEntryPanel* panel);
