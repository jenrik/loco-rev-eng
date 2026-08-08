/**
 * assetmgr_loadfile.c — Asset Manager file loader and decompressor
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * AssetMgr_LoadFile is a __thiscall method on the Asset Manager object.
 * It searches for a filename in the resource directory tree, sums the
 * file sizes (for seeking), reads the raw data blob, and optionally
 * decompresses it with Huffman decoding.
 *
 * The asset manager object accessed here is NOT the resources/AssetMgr.h
 * `AssetMgr` struct (that one's +0x00/+0x04 fields are entry_count/
 * pair_matrix, a totally different layout) — per the disassembly at
 * 0x45CD00, `_this+0x00` is an open CRT file handle used directly by
 * fseek/fread-style calls, and `_this+0x04` is the head of a linked list
 * of directory entries, each containing a wide-string filename (+0x00),
 * a compression flag (+0x04), a file size (+0x08), and a pointer to the
 * next entry (+0x0C). This mirrors the multiple already-conflicting
 * AssetMgr_LoadFile declarations tracked in
 * docs/landmine-sweep-worklist.md (line 251) — left as void* here rather
 * than adopting any one of them, since none matches this function's real
 * receiver layout.
 *
 * This function is referenced by ~18 callers throughout the codebase
 * (UIPANEL_StretchBlit, Cursor_Init, TrainStation_Init, etc.)
 */

#include <stdint.h>

/* ================================================================== */
/* External functions                                                  */
/* ================================================================== */

extern void*  __cdecl CRT_malloc_zero(uint32_t size);   /* 0x4673C0 */
extern void   __cdecl CRT_free(void* ptr);               /* 0x466C70 */
extern uint32_t __cdecl CRT_wcsstr(uint8_t* str, uint8_t* sub);   /* 0x471480 */
extern void   __cdecl CRT_0x468610(char* buf, uint32_t size,       /* fread-like read */
                                    uint32_t count, int32_t handle);
extern void   __cdecl CRT_0x468790(int32_t handle, int32_t offset,  /* fseek-like seek */
                                    uint32_t origin);
extern uint32_t __cdecl Huf_GetUncompressedSize(uint32_t* data);    /* 0x45C820 */
extern void   __cdecl Huf_Decode(int32_t* src, uint8_t* dst,        /* 0x45C830 */
                                  int32_t* out_size);

/* File-scope prototype: three other TUs already declare AssetMgr_LoadFile
 * with three mutually-incompatible first-param types (int pointer, void
 * pointer, and AssetMgr pointer — see docs/landmine-sweep-worklist.md line
 * 251), none of which matches this function's real (this+0x00=file handle,
 * this+0x04=entry list) receiver layout. Adding a local prototype here
 * satisfies -Wmissing-declarations without picking a side in that
 * pre-existing, separately-tracked cluster. */
uint8_t* __thiscall AssetMgr_LoadFile(void* _this, uint8_t* filename, int32_t* out_size);

/* ================================================================== */
/* AssetMgr_LoadFile — Load a file from asset tree, optionally decode  */
/* Address: 0x45CD00                                                   */
/* Size: 236 bytes (110 insn)                                          */
/* Calling convention: __thiscall (ECX = _this, 2 stack params)        */
/*                                                                     */
/* Searches the asset directory tree for a file matching 'filename'.   */
/* The tree is a linked list of directory entries, each with:          */
/*   +0x00: wide string filename                                       */
/*   +0x04: flags (0 = uncompressed, non-zero = Huffman compressed)    */
/*   +0x08: file size in bytes                                         */
/*   +0x0C: pointer to next entry                                      */
/*                                                                     */
/* `_this+0x00` is an open CRT file handle (used directly by the fseek/ */
/* fread-style helpers below); `_this+0x04` is the head of the entry    */
/* list above.                                                          */
/*                                                                     */
/* If found: seeks to the cumulative offset, allocates a buffer,       */
/* reads the raw data, optionally decompresses via Huf_Decode, and     */
/* writes the decompressed size to *out_size.                          */
/*                                                                     */
/* @param filename  ASCII/byte filename to search for                   */
/* @param out_size  Receives the final data size (decompressed if comp)*/
/* @return          Pointer to allocated data buffer, or NULL on fail  */
/* ================================================================== */
uint8_t* __thiscall AssetMgr_LoadFile(void* _this,
                                       uint8_t* filename,
                                       int32_t* out_size)
{
    uint32_t** root_entry;
    uint32_t* entry;
    int32_t total_offset;
    uint8_t* raw_data;
    uint8_t* result;

    /* Check if asset tree exists */
    if (*reinterpret_cast<uint32_t*>(_this) == 0) {
        return NULL;
    }

    /* Traverse entries, summing sizes until we find the matching entry */
    entry = *reinterpret_cast<uint32_t**>(static_cast<uint8_t*>(_this) + 4);
    total_offset = 0;

    if (entry != NULL) {
        do {
            /* wcsstr: check if the wide entry name contains our filename */
            uint32_t match = CRT_wcsstr(reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(entry[0])), filename);
            if (match == 0) break;

            /* Not a match — accumulate THIS entry's size (bytes we are
             * skipping over) into the seek offset, then advance to the
             * next entry. The size must be read BEFORE `entry` is
             * overwritten: Ghidra's decompile computes `piVar2 = puVar4+2`
             * (the address of the size field) and only reads *piVar2
             * after `puVar4` has already been advanced, because piVar2 is
             * a separate saved pointer — it still refers to the OLD node.
             * A prior version of this file instead advanced `entry` first
             * and then tried to recover the size via `entry - 4 + 8` on
             * the NEW entry, which reads the new entry's own +0x04 (flags)
             * field, not the old entry's +0x08 (size) — corrupting every
             * seek offset for any file that isn't the first entry in its
             * directory. */
            int32_t skipped_size = static_cast<int32_t>(entry[2]);  /* +0x08 = size, captured first */
            entry = reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(entry[3]));  /* +0x0C = next ptr */
            total_offset += skipped_size;
        } while (entry != NULL);

        if (entry == NULL) {
            return NULL;  /* end of list without match */
        }
    }

    /* entry now points to the matched entry (or NULL) */
    if (entry == NULL) {
        return NULL;
    }

    /* Seek to the cumulative offset in the file */
    CRT_0x468790(*reinterpret_cast<int32_t*>(_this), total_offset, 0);  /* seek SET */

    /* Allocate buffer for raw data */
    raw_data = static_cast<uint8_t*>(CRT_malloc_zero(entry[2]));  /* +0x08 = file size */
    if (raw_data == NULL) {
        return NULL;
    }

    /* Read the raw data */
    CRT_0x468610(reinterpret_cast<char*>(raw_data), 1, entry[2], *reinterpret_cast<int32_t*>(_this));

    /* Check if compressed */
    if (entry[1] == 0) {
        /* Uncompressed — return raw data directly */
        *out_size = static_cast<int32_t>(entry[2]);
        return raw_data;
    }

    /* Compressed — decompress via Huffman */
    {
        uint32_t uncomp_size = Huf_GetUncompressedSize(reinterpret_cast<uint32_t*>(raw_data));
        result = static_cast<uint8_t*>(CRT_malloc_zero(uncomp_size));
        if (result != NULL) {
            Huf_Decode(reinterpret_cast<int32_t*>(raw_data), result, out_size);
        }
        CRT_free(raw_data);
        return result;
    }
}
