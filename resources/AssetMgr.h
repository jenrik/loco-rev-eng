/**
 * AssetMgr.h — Asset Manager 4-ary tree file registry for Lego Loco
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * The Asset Manager is a non-virtual struct that manages a 4-ary tree of
 * game resource files loaded from the game data archive. It supports
 * file lookup, path computation, adjacency pair tracking, and Huffman
 * decompression of compressed resources.
 *
 * === Struct Layout ===
 * The AssetMgr struct (referenced via `this` pointer) has fields at:
 *   +0x00: entry_count (uint32_t, number of active entries)
 *   +0x04: pair_matrix (uint8_t*, packed adjacency pair matrix)
 *   +0x08: resource_array (void**, per-entry resource table)
 *   +0x0C: mode (uint32_t, enumeration mode)
 *   +0x10: active_flag (byte*, per-entry active flag array)
 *   +0x14: traversal_step (int, counter during traversal)
 *   +0x18: step_direction_buf (uint8_t*, allocated per-step direction bytes)
 *   +0x1C: step_node_id_buf (int*, allocated per-step node-id words)
 *   +0x20: result_array (uint*, allocated result array)
 *   +0x24: tree_entry (uint*, tree entry lookup)
 *   +0x28: heap_entry (uint*, heap entry lookup)
 *
 * NOTE (2026-08-09): +0x18/+0x1C were previously named path_id_buffer /
 * path_dir_buffer, swapped from what they actually hold. Ghidra disassembly
 * of 0x45DA70 (AssetMgr::RecordPath, __thiscall) proves +0x1C is
 * dereferenced as a 4-byte-stride int array and written with the *node id*
 * (`*param_1`), while +0x18 is dereferenced as a 1-byte-stride array and
 * written with the *direction byte* (`param_1[10]`, the tree node's
 * direction_byte field at +0x28). AssetMgr::GetFileInfo's two
 * CRT_malloc_zero calls corroborate: the allocation assigned to +0x18 is
 * sized `traversal_step` (bytes), the one assigned to +0x1C is sized
 * `traversal_step * 4` (words). Renamed to match actual contents.
 *
 * === Directory Entry Layout (4-ary tree node, 0x2C bytes) ===
 *   +0x00: node_id (int)
 *   +0x04: parent_ptr (int, parent node pointer)
 *   +0x08: children[4] (int[4], child node pointers)
 *   +0x18: subtree_refs[4] (int[4], subtree reference results)
 *   +0x28: direction_byte (byte, preferred direction)
 *
 * === Adjacency Pair Matrix ===
 * A packed upper-triangular matrix of 2-bit values stored at +0x04.
 * For N entries:
 *   matrix_size = (N-1)*N/2 bytes
 *   index(i, j) where i > j:  offset = (i-1)*i/2 + j
 *   Each byte stores 4 pairs (2 bits each):
 *     pairs[0] = bits 0-1, pairs[1] = bits 2-3, pairs[2] = bits 4-5, pairs[3] = bits 6-7
 *   Value 0x80 = valid unset, 0xFF = invalid/cleared, 0-3 = direction
 *
 * === Functions ===
 * Every function in this class except AssetMgr_LoadFile and the two
 * genuine free helpers (AssetMgr_TreeFreeNode, AssetMgr_WriteFile — they
 * operate on a raw tree node, never touch `this`) is a real __thiscall
 * method in the original binary (ECX = AssetMgr*, confirmed via Ghidra
 * disassembly, 2026-08-09) and is declared as an AssetMgr member function
 * above. AssetMgr_LoadFile (0x45CD00) is excluded from this pass: it has
 * 3 conflicting first-param types across the tree (see
 * docs/landmine-sweep-worklist.md line 251) that need resolving before a
 * safe method conversion.
 *
 * References:
 *   - AssetMgr_LoadFile: in native/assetmgr_loadfile.c
 *   - Remaining free functions/shims below: in resources/AssetMgr.cpp
 */

#pragma once

#include "../shared/types.h"


// Status: TRANSCRIBED
/* ================================================================== */
/* External declarations for functions used by AssetMgr                */
/* ================================================================== */

extern "C" {
    /* Memory management */
    void* __cdecl operator_new(size_t size);
    void  __cdecl GLOBAL_free(void* ptr);
    void* __cdecl CRT_malloc_zero(size_t size);
    void  __cdecl CRT_free(void* ptr);

    /* File I/O */
    int32_t __cdecl CRT_0x468480(const char* path, const char* mode);  /* fopen */
    void    __cdecl CRT_0x4681D0(int32_t handle);                       /* fclose */
    int32_t __cdecl CRT_0x468610(char* buf, uint32_t size, uint32_t count, int32_t handle); /* fread */
    void    __cdecl CRT_0x468790(int32_t handle, int32_t offset, uint32_t origin); /* fseek */

    /* String/utility */
    int32_t __cdecl CRT_sprintf_buf(char* buf, const char* fmt, ...);
    uint32_t __cdecl CRT_wcsstr(uint8_t* str, uint8_t* sub);

    /* Huffman */
    uint32_t __cdecl Huf_GetUncompressedSize(uint32_t* data);
    void     __cdecl Huf_Decode(int32_t* src, uint8_t* dst, int32_t* out_size);

    /* Win32 */
    void __stdcall OutputDebugStringA(const char* lpOutputString);
}

/* ================================================================== */
/* Global error string constant                                       */
/* ================================================================== */
extern const char s_invalid_path_error[];  /* "ERROR: Invalid path in GetOppositeDirection" */

/* ================================================================== */
/* AssetMgr struct — non-virtual, 0x2C bytes                           */
/* ================================================================== */
struct AssetMgr {
    uint32_t  entry_count;         /* +0x00  number of active entries */
    uint8_t*  pair_matrix;         /* +0x04  packed adjacency pair matrix */
    void**    resource_array;      /* +0x08  per-entry resource table */
    uint32_t  mode;                /* +0x0C  enumeration mode */
    uint8_t*  active_flag;         /* +0x10  per-entry active flag array */
    int32_t   traversal_step;      /* +0x14  step counter */
    uint8_t*  step_direction_buf;  /* +0x18  allocated per-step direction bytes */
    int32_t*  step_node_id_buf;    /* +0x1C  allocated per-step node-id words */
    uint32_t* result_array;        /* +0x20  allocated result array */
    uint32_t* tree_entry;          /* +0x24  tree entry lookup */
    uint32_t* heap_entry;          /* +0x28  heap entry lookup */

    /* ---------------------------------------------------------------- */
    /* Tree search, traversal, and pair-matrix methods.                 */
    /* Real __thiscall (ECX=this) methods on this class in the original */
    /* binary — see per-method address/evidence comments below.        */
    /* ---------------------------------------------------------------- */

    uint32_t* SearchFile(uint32_t currentId, int32_t depth, uint32_t distance,
                          int32_t parentNode);
    void      FindFile(uint32_t entry_index);
    void      RemoveNode(int32_t* node);
    void      TraverseTo(int32_t* startNode, int32_t targetId);
    void      RecordPath(int32_t* startNode, int32_t targetId);
    void      WriteAdjacencyPairs();
    uint8_t   GetFileInfo(uint32_t* treeNode, uint32_t fromId, uint32_t toId);
    uint8_t   ReadPairValue(uint32_t a, uint32_t b);
    void      WritePairValue(uint32_t a, uint32_t b, uint8_t value);

    /* ---------------------------------------------------------------- */
    /* Tile-adjacency (re)construction methods. Also real __thiscall     */
    /* (ECX=this) methods in the original binary — converted 2026-08-09  */
    /* alongside the raw word-indexed field access their bodies used     */
    /* (this cluster previously kept an explicit `self` AND indexed      */
    /* fields as `param_1[N]` instead of by name; see PROGRESS.md).      */
    /* ---------------------------------------------------------------- */

    void ClearTree();
    void UpdateAdjacencyGraph();
    void EnumeratePostLoadAdjacency();
    void EnumerateCategory();
};

/* ================================================================== */
/* AssetMgr — File loading (address 0x45CD00)                         */
/* ================================================================== */

/**
 * AssetMgr_LoadFile — Load a file from asset tree, optionally decode.
 * Address: 0x45CD00, __thiscall
 *
 * Searches the asset directory tree linked list for a file matching
 * 'filename'. Sums entry sizes for seeking, allocates buffer, reads raw
 * data, and optionally Huffman-decompresses. Returns allocated buffer.
 *
 * Called by: UIPANEL_StretchBlit, RESMGR_OpenResourceFile, Game_LoadWaveFile,
 *            Cursor_Init, HelpWnd_ResetPages, TrainStation_Init, and 13 others.
 *
 * @param this     Asset manager instance
 * @param filename ASCII/byte filename to search for
 * @param out_size Receives final data size (decompressed if compressed)
 * @return         Pointer to allocated data buffer, or NULL on failure
 */
uint8_t* __thiscall AssetMgr_LoadFile(AssetMgr* self, uint8_t* filename, int32_t* out_size);

/* ================================================================== */
/* AssetMgr — Tree clearing, adjacency (re)construction, and category  */
/* enumeration (real methods)                                          */
/*                                                                     */
/* Ghidra disassembly (2026-08-09) confirms all four are __thiscall     */
/* with ECX=this — previously transcribed as free functions taking an   */
/* explicit `uint32_t* param_1`/`void* param_1` AND indexing struct     */
/* fields by raw word offset (`param_1[0]`=entry_count, `param_1[1]`=   */
/* pair_matrix, `param_1[2]`=resource_array, `param_1[3]`=mode) instead */
/* of by name. Both issues are fixed together below.                    */
/* ================================================================== */

/**
 * AssetMgr::ClearTree — Free all tree nodes and reset.
 * (Previously AssetMgr_ReadFile — misnamed: frees, doesn't read.)
 * Address: 0x45D8C0, __thiscall
 *
 * Walks each entry's 4 child nodes, removes cross-references, frees
 * child nodes, frees entry nodes, clears resource_array/entry_count.
 *
 *   void AssetMgr::ClearTree();
 */

/**
 * AssetMgr::UpdateAdjacencyGraph — Rebuild the 4-way tile adjacency graph.
 * (Previously AssetMgr_LoadFileEx — misnamed: doesn't load a file.)
 * Address: 0x45CE40, __thiscall
 *
 * Clears the old tree (ClearTree), iterates all game objects checking
 * TileMap viewport inclusion (neighbor index cached at object+0xE4),
 * allocates one 0x2C-byte entry node per accepted object, links each
 * entry's 4 neighbor directions via 0x10-byte edge nodes, then
 * reallocates the packed pair matrix (pair_matrix, +0x04) sized for the
 * new entry count.
 *
 * Called by: TileMap::UpdateAll (0x45735B) — Windows-only path; the SDL3
 * host path returns before reaching this call (see world/tilemap.cpp).
 *
 *   void AssetMgr::UpdateAdjacencyGraph();
 */

/**
 * AssetMgr::EnumeratePostLoadAdjacency — Rebuild adjacency graph
 * (post-load variant).
 * (Previously AssetMgr_EnumFiles — misnamed: doesn't enumerate files.)
 * Address: 0x45D1C0, __thiscall
 *
 * Same structure as UpdateAdjacencyGraph but uses
 * TileMap_SetViewport/TileMap_GetTileAt instead of
 * TileMap_UpdateViewport/TileMap_GetTileRect, and caches the neighbor
 * index at object+0x108 instead of +0xE4 (except the `mode == 7` special
 * case, which still uses +0xE4 — see AssetMgr.cpp for the exact branch).
 *
 * Called by: World_EnumeratePostLoadAssets (0x45736A)
 *
 *   void AssetMgr::EnumeratePostLoadAdjacency();
 */

/**
 * AssetMgr::EnumerateCategory — Search all entries in a category.
 * Address: 0x45D560, __thiscall
 *
 * Allocates 4 temporary scratch arrays (reusing active_flag/result_array/
 * tree_entry/heap_entry as working storage — same struct slots, matching
 * types), then calls FindFile for each of entry_count entries. Frees the
 * scratch arrays on exit.
 *
 * Called by: World_EnumeratePostLoadAssets (0x4573A4, 0x4573CA)
 *
 *   void AssetMgr::EnumerateCategory();
 */

/* ================================================================== */
/* Free-function compatibility shims.                                  */
/*                                                                     */
/* Each external caller below deliberately does not include this       */
/* header (documented `operator_new`/`GLOBAL_free`/`CRT_wcsstr`         */
/* extern-"C"-vs-C++-linkage conflicts — same shape as the              */
/* AssetMgr_ReadPairValue shim above) and calls across that boundary as */
/* a free function with the object passed explicitly. These forward     */
/* straight into the real methods; signatures match each caller's       */
/* existing declaration exactly so no caller-side changes are needed.   */
/*                                                                       */
/*   AssetMgr_ReadFile           — native/ddraw_spritedata.c            */
/*     (fixes a pre-existing call-0 landmine: that file's declaration    */
/*     takes `void*`, but the free function this replaces took          */
/*     `uint32_t*` — a different Itanium mangled symbol. Confirmed via   */
/*     `nm` that the old `_Z17AssetMgr_ReadFilePv` was never defined     */
/*     anywhere in the linked binary before this fix.)                  */
/*   AssetMgr_LoadFileEx/EnumFiles — world/tilemap.{h,cpp}, `_WIN32`-    */
/*     only call sites (dead code on the SDL3 host path; kept for the    */
/*     mingw typecheck build's fidelity).                                */
/*   AssetMgr_EnumerateCategory  — world/World_enumerate.cpp             */
/* ================================================================== */
void AssetMgr_ReadFile(void* tree_ptr);
void AssetMgr_LoadFileEx(uint32_t* ptr);
void AssetMgr_EnumFiles(uint32_t* ptr);
void AssetMgr_EnumerateCategory(uint32_t* param_1);

/* ================================================================== */
/* AssetMgr — Tree search and path computation (real methods)          */
/*                                                                     */
/* Ghidra disassembly (2026-08-09) confirms every function below is    */
/* __thiscall with ECX=this and accesses fields exclusively by name    */
/* (no raw word-indexed access) — genuine class methods, previously    */
/* transcribed as free functions taking an explicit `AssetMgr* self`.  */
/* ================================================================== */

/**
 * AssetMgr::FindFile — Search file placement in 4-ary tree.
 * Address: 0x45D5F0, __thiscall
 *
 * Clears result arrays, calls SearchFile to find the optimal path, then
 * calls GetFileInfo for each entry. Frees the temporary tree node on exit.
 *
 *   void      AssetMgr::FindFile(uint32_t entry_index);
 */

/**
 * AssetMgr::SearchFile — Recursive 4-ary tree search with best-path scoring.
 * Address: 0x45D6C0, __thiscall
 *
 * Recursively walks the 4-ary tree from a starting node to find the
 * shortest path to target node. Allocates path node (0x2C bytes) with
 * adjacency and direction info. Returns path node or NULL.
 *
 * Only runs when g_game_mode == 3.
 *
 *   uint32_t* AssetMgr::SearchFile(uint32_t currentId, int32_t depth,
 *                                  uint32_t distance, int32_t parentNode);
 *
 * @param currentId Current node ID
 * @param depth     Current search depth
 * @param distance  Current path distance
 * @param parentNode Parent path node for linking
 * @return          Allocated path node, or NULL if no path
 */

/* ================================================================== */
/* AssetMgr — Tree node management                                     */
/* ================================================================== */

/**
 * AssetMgr_TreeFreeNode — Recursively free a 4-ary tree node.
 * Address: 0x45D810, C function
 *
 * Recursively frees the node and all 4 children. Does NOT update
 * lookup tables (raw subtree deletion). Operates on a raw tree node, not
 * on an AssetMgr instance — a genuine free helper, not a disguised method.
 */
void AssetMgr_TreeFreeNode(void* node);

/**
 * AssetMgr::RemoveNode — Remove node from tree and free.
 * Address: 0x45D850, __thiscall
 *
 * Removes node from lookup tables at +0x24, unlinks from parent at +0x28,
 * then recursively removes children and frees the node.
 *
 *   void AssetMgr::RemoveNode(int32_t* node);
 */

/* AssetMgr::ClearTree (was AssetMgr_ReadFile) is declared further down,
 * in the "Tree clearing, adjacency (re)construction, and category
 * enumeration" section — grouped with the other three functions that
 * shared its raw-word-indexed-access conversion. */

/* ================================================================== */
/* AssetMgr — Path computation helpers                                 */
/* ================================================================== */

/**
 * AssetMgr_WriteFile — Recursive shortest path search.
 * Address: 0x45D980, C function
 *
 * Recursively searches for the shortest path between two IDs in the
 * 4-ary tree. Returns path distance or -1 if no path. Operates on a raw
 * tree node, not on an AssetMgr instance — a genuine free helper.
 *
 * @param param_1   Tree node to search from
 * @param param_2   Target ID (unused?)
 * @param param_3   Source ID
 * @param param_4   Best distance so far
 * @param param_5   Accumulated distance
 * @return          Path distance, or -1 if no path
 */
int32_t AssetMgr_WriteFile(int32_t* param_1, int32_t param_2, int32_t param_3,
                            uint32_t param_4, uint32_t param_5);

/**
 * AssetMgr::TraverseTo — Walk tree from start node to target.
 * Address: 0x45DA40, __thiscall
 *
 * Walks the 4-ary tree from startNode to targetId, counting steps into
 * traversal_step (+0x14).
 *
 *   void AssetMgr::TraverseTo(int32_t* startNode, int32_t targetId);
 */

/**
 * AssetMgr::WriteAdjacencyPairs — Write adjacency pair values for all path
 * nodes. (Previously AssetMgr_DeleteFile — misnamed: writes, doesn't
 * delete, and has nothing to do with files.)
 * Address: 0x45DAD0, __thiscall (ECX=this; transcribed as __fastcall
 * taking `void* param_1` before this pass — Ghidra confirms __thiscall)
 *
 * For each pair of entries in the path list, calls ReadPairValue and
 * WritePairValue to store/reverse the direction mapping.
 *
 *   void AssetMgr::WriteAdjacencyPairs();
 */

/**
 * AssetMgr::GetFileInfo — Compute shortest path between two nodes.
 * Address: 0x45DBC0, __thiscall
 *
 * Calls WriteFile recursively to find shortest path, then TraverseTo to
 * build the direction path, then WriteAdjacencyPairs to write adjacency
 * pairs. Returns direction byte or 0xFF on failure.
 *
 *   uint8_t AssetMgr::GetFileInfo(uint32_t* treeNode, uint32_t fromId,
 *                                 uint32_t toId);
 */

/* ================================================================== */
/* AssetMgr — Pair matrix access                                       */
/* ================================================================== */

/**
 * AssetMgr::RecordPath — Walk tree storing per-step node IDs/directions.
 * (Previously DirectPlay_SessionMgr — a Ghidra auto-analysis mislabel:
 * this has nothing to do with DirectPlay. Sole real caller is
 * AssetMgr::GetFileInfo.)
 * Address: 0x45DA70, __thiscall
 *
 * Walks the 4-ary tree from startNode to targetId, storing each node's ID
 * into step_node_id_buf (+0x1C) and its direction byte into
 * step_direction_buf (+0x18). traversal_step (+0x14) is updated per step.
 *
 *   void AssetMgr::RecordPath(int32_t* startNode, int32_t targetId);
 */

/**
 * AssetMgr::ReadPairValue — Read 2-bit pair value from matrix.
 * Address: 0x45DD80, __thiscall
 *
 * Reads a 2-bit direction value from a packed upper-triangular matrix.
 * Matrix stored at +0x04, N entries count at +0x00.
 *
 *   uint8_t AssetMgr::ReadPairValue(uint32_t a, uint32_t b);
 *
 * @param a     First node ID
 * @param b     Second node ID
 * @return      0-3 direction, 0x80 = valid empty, 0xFF = invalid
 */

/**
 * AssetMgr::WritePairValue — Write 2-bit pair value to matrix.
 * Address: 0x45DDE0, __thiscall
 *
 * Writes a 2-bit direction value to the packed upper-triangular matrix.
 * Value 0xFF clears the entry.
 *
 *   void AssetMgr::WritePairValue(uint32_t a, uint32_t b, uint8_t value);
 */

/**
 * AssetMgr_ReadPairValue — free-function compatibility shim over
 * AssetMgr::ReadPairValue.
 *
 * game/Building.cpp deliberately does not include this header (it
 * conflicts with that file's own operator_new/GLOBAL_free extern "C"
 * declarations and a differently-shaped CRT_wcsstr — see Building.cpp's
 * top-of-file comment) and instead forward-declares `struct AssetMgr` and
 * calls the free function below, matching the original opaque-handle
 * boundary that file already uses for g_asset_mgr. This wrapper is the
 * only caller-visible surface across that boundary; it forwards straight
 * into the real method.
 */
uint8_t AssetMgr_ReadPairValue(AssetMgr* self, uint32_t a, uint32_t b);

/* ================================================================== */
/* Global state                                                       */
/* ================================================================== */

extern int32_t g_game_mode;  /* 0x4851F4 */
