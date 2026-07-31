/**
 * PlayerConfig.h — Per-player settings serialized alongside game state
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * PlayerConfig manages player-specific data stored on disk as a 288-byte
 * binary blob at "{install_path}\{player_name}\usr". The struct includes
 * a magic WORD (0x66), player name, unique player ID, and state tracking.
 *
 * A global instance lives at g_player_config (0x4AA4A8), allocated during
 * GameLoop_Setup and freed in CGWND_Cleanup.
 *
 * Size: 0x124 bytes (on-disk: 0x120 bytes starting at +0x04)
 * Descriptor: 0x4784BC (written to +0x00 by Ctor/Dtor; non-vtable sentinel)
 *
 * Class hierarchy:
 *   PlayerConfig (standalone class, no virtual methods)
 *
 * All methods are non-virtual but called with ECX = this.
 */

#pragma once

#include "../shared/types.h"

/* ================================================================== */
/* Constants                                                           */
/* ================================================================== */
#define PLAYERCONFIG_FILE_SIZE  0x120   /* 288 bytes written to/from disk  */
#define PLAYERCONFIG_MAGIC      0x66    /* magic WORD at +0x04             */
#define PLAYERCONFIG_MAX_ID     999     /* max player ID before wrap       */
#define PLAYERCONFIG_SAVEID_MAX 9999    /* max save-sequence counter        */

class PlayerConfig {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */

    void*    descriptor;         // +0x00  always VTBL_PLAYERCONFIG (0x4784BC)
    uint16_t magic;              // +0x04  magic WORD (0x66)
    char     name[14];           // +0x06  player name subdirectory (null-terminated)
    int32_t  field_14;           // +0x14  (unknown purpose)
    int32_t  player_id;          // +0x18  unique player ID from CLIENT/NextId
    int32_t  save_counter;       // +0x1C  save sequence counter (incremented on save, wraps at 10000)
    uint8_t  reserved[0xE8];     // +0x20  remaining file data / unknown fields
    /* note: +0x20 is also used as a string buffer by SaveToFile (12 chars) */
    int32_t  field_108;          // +0x108 (zeroed by Ctor)
    uint8_t  pad_10C[0x14];      // +0x10C padding to end of file data (+0x120)
    uint8_t  is_new_player;      // +0x120 1 = newly created (not loaded from disk)
    uint8_t  pad_121[3];         // +0x121 padding to total size 0x124

    /* ================================================================ */
    /* Methods                                                           */
    /* ================================================================ */

    /**
     * Constructor — initializes descriptor and copies player name.
     * Address: 0x452CE0
     *
     * Sets descriptor at +0x00 to VTBL_PLAYERCONFIG, copies name string
     * starting at +0x04, zeroes field_108.
     *
     * Called by: g_player_config allocation site in GameLoop_Setup
     *
     * @param name  const char* — player name string (subdirectory name)
     */
    PlayerConfig(const char* name);

    /**
     * Destructor — resets descriptor.
     * Address: 0x452D50
     *
     * Writes VTBL_PLAYERCONFIG back to descriptor slot. Minimal cleanup.
     */
    ~PlayerConfig();

    /**
     * SetName — change player name and reload/create config.
     * Address: 0x452FC0
     *
     * Compares current name at +0x06 with the new name. If different,
     * copies the new name, optionally blits DPlay config, then calls
     * LoadOrCreate. On new-player return, assigns a unique ID from
     * the CLIENT/NextId INI counter and sets is_new_player=1.
     * On existing-player return, sets is_new_player=0. Finally
     * reinitializes the network player list.
     *
     * Called by: UI_Panel_HitTest (0x4226D1) when editing player name
     *
     * @param new_name  const char* — new player name
     */
    void SetName(const char* new_name);

    /**
     * LoadOrCreate — load config from disk or create default entries.
     * Address: 0x4530C0
     *
     * Reads 288 bytes from "{install_path}\{name}\usr". If the file
     * exists and the magic WORD at +0x04 equals 0x66, returns 1 (loaded).
     * If the file doesn't exist, returns 0 (new player — caller must
     * assign an ID). If the file exists but magic is wrong, rewrites
     * the file with a fresh config using the next ID from CLIENT/NextId
     * and returns 1.
     *
     * Builds file path as: g_install_path + "\\" + name + "\\usr"
     *
     * Called by: PlayerConfig::SetName (0x453030), PlayerRecord_constructor (0x452F10)
     *
     * @return  int — 1 if loaded/created, 0 if file not found
     */
    int LoadOrCreate();

    /**
     * Save — write config to disk immediately.
     * Address: 0x4532A0
     *
     * Writes 288 bytes from +0x04 to "{install_path}\{name}\usr".
     * Creates the file with CREATE_ALWAYS. Returns void (errors are
     * silently ignored).
     *
     * Called by: Train_ProcessMessages (0x439A49), UI_Panel_HitTest (0x4226DC)
     */
    void Save();

    /**
     * SaveToFile — save with auto-incrementing counter.
     * Address: 0x453320
     *
     * Formats a 12-character save-ID string at +0x20 from player_id
     * and save_counter ("%03d_%04d" format), increments save_counter
     * (wraps at 10000), then writes 288 bytes to disk. Returns pointer
     * to the formatted string at +0x20.
     *
     * @return  const char* — pointer to the formatted save-ID string at +0x20
     */
    const char* SaveToFile();
};

/* ================================================================== */
/* Global instance                                                     */
/* ================================================================== */
extern PlayerConfig* g_player_config;  /* 0x4AA4A8 */

/** PlayerRecord_constructor — initialize a preallocated PlayerConfig.
 * Address: 0x452E10
 */
PlayerConfig* PlayerRecord_constructor(PlayerConfig* config);

/** Compatibility bridge retained for legacy callers of the recovered ABI.
 * Address: 0x452CE0
 */
void* PlayerConfig_Ctor(void* memory, const char* name);
