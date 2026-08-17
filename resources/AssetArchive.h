/**
 * AssetArchive.h — Global installed-asset archive handle (0x485600)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * === Identity: distinct from resources/AssetMgr.h's `AssetMgr` ===
 * Ghidra auto-named the function at 0x45CD00 "AssetMgr_LoadFile", which
 * collided with the unrelated `AssetMgr` class in resources/AssetMgr.h (the
 * TileMap 4-ary tile-adjacency tree — entry_count/pair_matrix/... at
 * +0x00/+0x04). native/assetmgr_loadfile.c already flagged this collision
 * and deliberately left its receiver as `void*` rather than pick a side.
 * This header resolves it: `AssetArchive` is a SEPARATE, 2-field,
 * non-virtual value type. Its only real method is LoadFile (0x45CD00).
 *
 * === The global is a VALUE object, not a pointer ===
 * Every real call site in the binary loads the ADDRESS of the global as
 * `this`, and the "is it open" guard reads the object's first dword
 * in-place — never through an extra pointer indirection. Confirmed via
 * disassembly (RESMGR_OpenResourceFile @ 0x448A70, Train_DownloadMissingAssets
 * @ 0x438E40, three sites):
 *
 *   MOV EDX, dword ptr [0x00485600]   ; guard reads archive_file directly
 *   CMP EDX, 0 / SETNZ ...
 *   ...
 *   MOV ECX, 0x485600                 ; this = ADDRESS of the global
 *   CALL 0x0045CD00                   ; AssetArchive::LoadFile(this, name, size)
 *
 * If `g_asset_mgr` were a pointer, the call would instead load
 * `dword ptr [0x485600]` into ECX. It never does, at any of the ~18 call
 * sites checked. So `extern AssetArchive g_asset_mgr;` (a value object) is
 * the canonical declaration — NOT `AssetMgr*`, `void*`, `int32_t`, or
 * `uint8_t`, all of which appeared as incompatible per-file redeclarations
 * before this cleanup. Every pre-existing `&g_asset_mgr` call site was
 * already correct; the bug was the inconsistent *declared type* of the
 * global itself, which caused it to bind to the wrong overload of
 * AssetMgr_LoadFile at link time (mostly empty no-op stubs in
 * shared/core_stubs.cpp / shared/link_stubs.cpp) instead of the real,
 * already-decompiled implementation.
 *
 * === Layout (0x45CD00 evidence) ===
 *   +0x00: archive_file (int32_t)  — open CRT file handle used directly by
 *          the fseek/fread-style helpers below (0, i.e. not open, is the
 *          "unopened" sentinel checked by every caller's guard)
 *   +0x04: entry_list (Entry*)     — head of a singly linked list of
 *          directory entries:
 *            +0x00: filename (uint8_t*, byte string pointer — despite the
 *                   "wcs" naming of the real compare helper, Ghidra confirms
 *                   narrow/byte data, matching AssetMgr.h's
 *                   s_invalid_path_error precedent)
 *            +0x04: compressed (uint32_t, 0 = raw, nonzero = Huffman)
 *            +0x08: size (uint32_t, raw byte size on disk)
 *            +0x0C: next (Entry*)
 *
 * No code anywhere in the binary WRITES archive_file/entry_list (zero
 * WRITE xrefs to 0x485600 across the whole image — every one of the ~36
 * xrefs found is a READ). The global is zero-initialized storage and never
 * populated in the shipped retail binary: AssetArchive::LoadFile's
 * `archive_file == 0` guard is always taken, so every caller always falls
 * back to direct file I/O (WIN32_StreamOpenFile/OpenPath or
 * GetFileAttributesA, depending on the call site). This is preserved as
 * real, reachable code — not optimized away — because the guard executes
 * unconditionally at every call site and a future initializer could
 * populate it.
 *
 * === Functions ===
 *   AssetArchive::LoadFile — address 0x45CD00, __thiscall (ECX = this)
 *
 * Global instance: `extern AssetArchive g_asset_mgr;` at 0x485600, defined
 * once in resources/AssetArchive.cpp (previously duplicated as a `void*`
 * placeholder in shared/stubs_impl.cpp and independently in
 * tests/persistence_fixtures.h — both removed/retyped as part of this
 * cleanup).
 *
 * C-linkage bridge functions (AssetArchive_IsOpen/AssetArchive_LoadFile)
 * are provided below for the handful of genuine C translation units
 * (native/cgwnd_palette.c, native/wave_io.c) that cannot include a C++
 * class definition.
 */

#pragma once

#include "../shared/types.h"

#ifdef __cplusplus

struct AssetArchive {
    struct Entry {
        uint8_t*  filename;    /* +0x00 byte string pointer */
        uint32_t  compressed;  /* +0x04 0 = raw, nonzero = Huffman-compressed */
        uint32_t  size;        /* +0x08 raw on-disk byte size */
        Entry*    next;        /* +0x0C */
    };

    int32_t archive_file;  /* +0x00 open CRT file handle, 0 = not open */
    Entry*  entry_list;    /* +0x04 head of directory entry list */

    /**
     * AssetArchive::LoadFile — Load a file from the archive's directory
     * list, optionally Huffman-decompressing it.
     * Address: 0x45CD00, __thiscall
     *
     * Returns nullptr immediately if the archive isn't open (archive_file
     * == 0 — always true in the shipped retail binary, see header comment
     * above). Otherwise walks entry_list comparing filenames, seeks to the
     * cumulative byte offset of the match, reads the raw bytes, and
     * Huffman-decodes them if the entry is marked compressed.
     *
     * @param filename byte-string filename to search for
     * @param out_size receives the final data size (decompressed size, if
     *                 the entry was compressed)
     * @return heap-allocated data buffer (caller frees via CRT_free/
     *         GLOBAL_free — see call sites), or nullptr on failure
     */
    uint8_t* LoadFile(const uint8_t* filename, int32_t* out_size);
};

extern AssetArchive g_asset_mgr;  /* 0x485600 */

#endif /* __cplusplus */

#ifdef __cplusplus
extern "C" {
#endif

/* C-linkage bridges for native/ *.c translation units that cannot include
 * the AssetArchive class definition above. */
int32_t  AssetArchive_IsOpen(void);
uint8_t* AssetArchive_LoadFile(const uint8_t* filename, int32_t* out_size);

#ifdef __cplusplus
}
#endif
