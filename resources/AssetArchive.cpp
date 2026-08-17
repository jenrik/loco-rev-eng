/**
 * AssetArchive.cpp — Global installed-asset archive handle implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * See AssetArchive.h for the class identity/layout evidence. The body
 * below is the same logic previously carried in native/assetmgr_loadfile.c
 * (retired by this change — its receiver type was left as `void*` only
 * because this class hadn't been resolved yet), transcribed to use named
 * members instead of raw offsets now that the layout is confirmed.
 *
 * Status: INTEGRATED
 */

#include "AssetArchive.h"

/* ================================================================== */
/* External CRT/Huffman helpers — exact signatures matching their real */
/* definitions (shared/stubs_link001_batch1_crt_win32.cpp for the CRT  */
/* wrappers, shared/stubs_link001_batch6_asset_huf.cpp for Huffman).    */
/* ================================================================== */
extern void     CRT_free(void* ptr);
extern void*    CRT_malloc_zero(uint32_t size);
extern int32_t  CRT_wcsstr(uint8_t* str, uint8_t* sub);
extern void     CRT_0x468610(char* buf, uint32_t size, uint32_t count, int32_t handle); /* fread */
extern void     CRT_0x468790(int32_t handle, int32_t offset, uint32_t origin);          /* fseek */
extern uint32_t Huf_GetUncompressedSize(uint32_t* data);
extern void     Huf_Decode(int32_t* src, uint8_t* dst, int32_t* out_size);

/* ================================================================== */
/* Global instance — 0x485600. Zero-initialized: archive_file == 0     */
/* means "not open", matching the always-zero state observed in the    */
/* shipped binary (see AssetArchive.h header comment).                 */
/* ================================================================== */
AssetArchive g_asset_mgr = { 0, nullptr };

/* ================================================================== */
/* AssetArchive::LoadFile — Address: 0x45CD00                          */
/*                                                                     */
/* Searches entry_list for a filename match, sums preceding entries'   */
/* sizes for the seek offset, reads the raw bytes, and Huffman-decodes */
/* if the entry is marked compressed.                                 */
/* ================================================================== */
uint8_t* AssetArchive::LoadFile(const uint8_t* filename, int32_t* out_size)
{
    if (archive_file == 0) {
        return nullptr;
    }

    Entry* entry = entry_list;
    int32_t total_offset = 0;

    if (entry != nullptr) {
        do {
            /* CRT_wcsstr: despite the name, this is a byte-wise compare
             * (see AssetArchive.h header comment) — nonzero means match. */
            int32_t match = CRT_wcsstr(entry->filename, const_cast<uint8_t*>(filename));
            if (match == 0) {
                break;
            }

            /* Not a match — accumulate THIS entry's size (bytes being
             * skipped over) into the seek offset, then advance. The size
             * must be captured BEFORE advancing to `next`, matching the
             * original instruction order (see native/assetmgr_loadfile.c's
             * prior bug-fix note this replaces: a previous transcription
             * read the size from the wrong node after advancing first). */
            int32_t skipped_size = static_cast<int32_t>(entry->size);
            entry = entry->next;
            total_offset += skipped_size;
        } while (entry != nullptr);

        if (entry == nullptr) {
            return nullptr;  /* end of list without a match */
        }
    }

    if (entry == nullptr) {
        return nullptr;
    }

    /* Seek to the cumulative offset in the archive file. */
    CRT_0x468790(archive_file, total_offset, 0);  /* SEEK_SET */

    uint8_t* raw_data = static_cast<uint8_t*>(CRT_malloc_zero(entry->size));
    if (raw_data == nullptr) {
        return nullptr;
    }

    CRT_0x468610(reinterpret_cast<char*>(raw_data), 1, entry->size, archive_file);

    if (entry->compressed == 0) {
        /* Uncompressed — return the raw data directly. */
        *out_size = static_cast<int32_t>(entry->size);
        return raw_data;
    }

    /* Compressed — decompress via Huffman. */
    uint32_t uncomp_size = Huf_GetUncompressedSize(reinterpret_cast<uint32_t*>(raw_data));
    uint8_t* result = static_cast<uint8_t*>(CRT_malloc_zero(uncomp_size));
    if (result != nullptr) {
        Huf_Decode(reinterpret_cast<int32_t*>(raw_data), result, out_size);
    }
    CRT_free(raw_data);
    return result;
}

/* ================================================================== */
/* C-linkage bridges for native/ *.c translation units.                */
/* ================================================================== */
extern "C" int32_t AssetArchive_IsOpen(void)
{
    return g_asset_mgr.archive_file != 0;
}

extern "C" uint8_t* AssetArchive_LoadFile(const uint8_t* filename, int32_t* out_size)
{
    return g_asset_mgr.LoadFile(filename, out_size);
}
