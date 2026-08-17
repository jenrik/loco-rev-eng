/**
 * PixelDataCache.h -- Album pixel data cache class
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * PixelDataCache caches the contents of album index files
 * (AlbIndex_<player>_<idx>.ind) from the PostBag/ directory.
 * Each .ind file contains zero or more sorted 0x18-byte
 * PixelFormatEntry records (name + value).
 *
 * The cache is keyed by album_index (0-8 for letter categories
 * A-Z, and 8 for non-alpha). Files are loaded on demand and
 * flushed to disk when the album index changes.
 *
 * Size: 0x18 bytes
 * Vtable: 0x004773E8
 *
 * Vtable layout:
 *   [0] +0x00: scalar deleting destructor (PixelDataCache_Dtor, 0x401650)
 */
#pragma once

#include "../shared/types.h"

class DPlayManager;

// Status: TRANSCRIBED
/* ================================================================ */
/* PixelFormatEntry -- one record in the album pixel data array      */
/* Size: 0x18 bytes                                                  */
/* ================================================================ */
struct PixelFormatEntry {
    char     name[20];       /* +0x00  null-terminated ASCII asset name */
    uint32_t value;          /* +0x14  numeric asset key / value field  */
};

/* ================================================================ */
/* PixelDataCache -- caches album .ind file data                      */
/* Size: 0x18 bytes                                                  */
/* Vtable: 0x004773E8                                                */
/* ================================================================ */
class PixelDataCache {
public:
    /* ================================================================ */
    /* Fields                                                            */
    /* ================================================================ */

    /* vtable at +0x00 is compiler-managed via virtual methods */
    int32_t        current_album_index; /* +0x04  -1 = none loaded     */
    PixelFormatEntry* pixel_buffer;     /* +0x08  allocated buffer     */
    int32_t        buffer_size;         /* +0x0C  total bytes in buffer*/
    int32_t        insert_index;        /* +0x10  temp: insertion index*/
    int32_t        saved_album_index;   /* +0x14  temp: old album idx  */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * PixelDataCache constructor.
     * Address: 0x401620
     *
     * Sets vtable to 0x4773E8, initializes all fields to sentinel
     * values (-1, NULL, 0), then loads album index 1.
     *
     * Called by: GameLoop_Setup (0x406D1B) for g_pixel_cache
     *
     * @param mem  pre-allocated memory (0x18 bytes from operator_new)
     *             passed in ECX (__fastcall convention)
     * @return     this pointer
     */
    static PixelDataCache* Create(void* mem);

    /**
     * Scalar deleting destructor (vtable[0]).
     * Address: 0x401650
     *
     * Flushes pixel data to disk via Flush(), then optionally frees
     * the heap allocation via GLOBAL_free.
     *
     * @param flags  bit 0: free heap memory (1 = call GLOBAL_free)
     * @return       this pointer
     */
    void* DestroyFromResource(uint8_t flags);

    /* ================================================================ */
    /* Core methods                                                      */
    /* ================================================================ */

    /**
     * Load -- load pixel data for a given album index.
     * Address: 0x401DF0
     *
     * If album_index matches current, does nothing. Otherwise flushes
     * current data, opens <inst>/PostBag/AlbIndex_<player>_<idx>.ind,
     * allocates a buffer, reads the entire file, and sets current
     * album_index. On file-not-found or zero-size, sets empty state.
     *
     * Called by: Ctor, Unlock, Lookup, RemoveByAsset, LookupAsset
     *
     * @param album_index  album category index (0-8, or -1 to no-op)
     */
    void Load(int32_t album_index);

    /**
     * Flush -- write pixel buffer to disk and reset.
     * Address: 0x401C90
     *
     * If current_album_index != -1, builds the output file path,
     * deletes any existing file, writes the buffer via WriteFile,
     * then frees the buffer and resets all fields to sentinel.
     * On file-open failure, prints error via FormatMessageA.
     *
     * A 5-byte JMP thunk at 0x401680 forwards to this function
     * for external callers (PlayerRecord_ctor, PlayerConfig_SetName).
     *
     * Called by: Load, Dtor, and via thunk at 0x401680
     */
    void Flush();

    /**
     * GetEntryCount -- return number of entries in buffer.
     * Address: 0x401810
     *
     * Computes buffer_size / 0x18 via multiplication by 0xAAAAAAAB
     * (unsigned division trick, then SHR 4).
     *
     * @return  count of PixelFormatEntry records in buffer
     */
    int32_t GetEntryCount();

    /**
     * Unlock -- switch album if needed, return entry count.
     * Address: 0x401820
     *
     * If the given album_index differs from current, calls Load().
     * Returns the number of entries in the buffer.
     *
     * Called by: PostcardAlbum::PaintWindow, GFX_RenderAllTiles
     *
     * @param album_index  desired album index
     * @return             number of entries after loading
     */
    int32_t Unlock(int32_t album_index);

    /**
     * Lookup -- find or insert a PixelFormatEntry by asset name.
     * Address: 0x401850
     *
     * Extracts the asset name (at asset_desc+0x25) and value
     * (at asset_desc+0x0c). Determines category by first letter
     * of the uppercase name (A-C=0, D-F=1, G-J=2, K-M=3, N-Q=4,
     * R-T=5, U-W=6, X-Z=7, else 8). Loads that category, then
     * performs a linear scan by name (case-sensitive via strcmp)
     * to find the sorted insertion position. If a matching name
     * already exists, advances past all duplicates. Always calls
     * Insert() to ensure an entry exists.
     *
     * Called by: NET_RegisterPlayer (0x444EF4)
     *
     * @param asset_desc  pointer to asset descriptor struct
     *                    (+0x0c = value, +0x25 = name)
     */
    void Lookup(void* asset_desc);

    /**
     * RemoveByAsset -- remove entry matching asset_desc value.
     * Address: 0x401AA0
     *
     * Determines category by first letter of the uppercase name,
     * loads that category, then linear-scans the pixel buffer
     * for an entry whose value field matches asset_desc+0x0c.
     * Calls RemoveEntry() on the first match.
     *
     * Called by: CGWND_GameSetup_RenderPlayerSlots
     *
     * @param asset_desc  pointer to asset descriptor struct
     * @return            TRUE (1) if entry was found and removed,
     *                    FALSE (0) otherwise
     */
    bool RemoveByAsset(void* asset_desc);

    /**
     * LookupAsset -- find first valid asset from start_index.
     * Address: 0x401C10
     *
     * Loads the album for the given album_index. Iterates entries
     * from start_index onward, calling NET_CheckAssetExists and
     * NET_ResolveAddress on each entry's value. Returns the first
     * resolved address, or NULL if none found.
     *
     * Return type resolved 2026-08-17: NET_ResolveAddress (network/
     * DPlayManager.h) returns DPlayManager*, so this does too — confirmed
     * via PostcardAlbum::RenderTileName's real caller, which passes this
     * return value straight into NetworkPlayerList::RenderPlayer's real
     * `DPlayManager* player` parameter and reads DPlayManager fields from
     * it directly (graphics/LOCOBITMAP.cpp).
     *
     * Called by: CGWND_GameSetup_RenderPlayerSlots, GFX_RenderTileName
     *
     * @param start_index  entry index to start searching from
     * @param album_index  album category index to search
     * @return             resolved player record, or NULL
     */
    DPlayManager* LookupAsset(int32_t start_index, int32_t album_index);

    /* ================================================================ */
    /* Internal helpers (should be private, but called by flat C code)   */
    /* ================================================================ */

    /**
     * Insert -- insert a 0x18-byte record at a sorted position.
     * Address: 0x401690
     *
     * Reallocs the buffer with +0x18 bytes, copies existing data
     * around the insertion point, copies the new entry into the
     * slot, then copies remaining data. Frees the old buffer.
     *
     * Sets saved_album_index from current_album_index and stores
     * the insertion index.
     *
     * @param index  position to insert at (0-based entry index)
     * @param entry  pointer to 0x18-byte PixelFormatEntry to insert
     */
    void Insert(int32_t index, const PixelFormatEntry* entry);

    /**
     * RemoveEntry -- remove a 0x18-byte record at index.
     * Address: 0x401760
     *
     * If the buffer contains only 1 entry (size==0x18), frees the
     * entire buffer and resets. Otherwise reallocs with -0x18 bytes,
     * copies data before and after the removal point, frees old buffer.
     *
     * @param index  position of entry to remove
     */
    void RemoveEntry(int32_t index);
};

/* ================================================================ */
/* Global singleton                                                  */
/* ================================================================ */

/**
 * g_pixel_cache -- global PixelDataCache singleton pointer.
 * Address: 0x4FD3B4
 *
 * Created in GameLoop_Setup (0x406D1B). Used by PostcardAlbum,
 * GFX_RenderAllTiles, and the asset management system.
 */
extern PixelDataCache* g_pixel_cache;
