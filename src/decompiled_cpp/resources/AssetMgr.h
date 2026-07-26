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
 *   +0x00: file_handle (int, CRT file handle for the archive)
 *   +0x04: search_cursor (uint32_t*, linked list of directory entries)
 *   +0x08: resource_array (void**, per-type resource table)
 *   +0x10: active_flag (byte*, per-entry active flag array)
 *   +0x14: traversal_step (int, counter during traversal)
 *   +0x18: path_id_buffer (int*, allocated ID path buffer)
 *   +0x1C: path_dir_buffer (int*, allocated direction buffer)
 *   +0x20: result_array (uint*, allocated result array)
 *   +0x24: tree_entry (uint*, tree entry lookup)
 *   +0x28: heap_entry (uint*, heap entry lookup)
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
 * All functions are C linkage operating on the AssetMgr struct.
 *
 * References:
 *   - AssetMgr_LoadFile: in native/assetmgr_loadfile.c
 *   - AssetMgr_* functions below: in resources/AssetMgr.cpp
 */

#pragma once

#include "../shared/types.h"

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
    int32_t   file_handle;       /* +0x00  CRT file handle for archive */
    uint32_t* search_cursor;     /* +0x04  linked list of directory entries */
    void**    resource_array;    /* +0x08  per-type resource table */
    uint8_t   _pad_0C[4];        /* +0x0C */
    uint8_t*  active_flag;       /* +0x10  per-entry active flag array */
    int32_t   traversal_step;    /* +0x14  step counter */
    int32_t*  path_id_buf;       /* +0x18  allocated ID path buffer */
    int32_t*  path_dir_buf;      /* +0x1C  allocated direction buffer */
    uint32_t* result_array;      /* +0x20  allocated result array */
    uint32_t* tree_entry;        /* +0x24  tree entry lookup */
    uint32_t* heap_entry;        /* +0x28  heap entry lookup */
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
/* AssetMgr — Extended file enumeration and adjacency building         */
/* ================================================================== */

/**
 * AssetMgr_LoadFileEx — Load files and build adjacency graph.
 * Address: 0x45CE40, __fastcall (ECX = AssetMgr*)
 *
 * Extended loader that additionally builds a 4-way tile adjacency graph
 * between loaded resources. Iterates all game objects, scans neighbor
 * tiles, links them in a mesh of 0x10-byte edge nodes with directional
 * encoding. Allocates a packed adjacency matrix at +0x04.
 *
 * Called by: TileMap_UpdateAll (0x45735B)
 */
void __fastcall AssetMgr_LoadFileEx(uint32_t* param_1);

/**
 * AssetMgr_EnumFiles — Enumerate tiles and relink adjacency graph.
 * Address: 0x45D1C0, __fastcall (ECX = AssetMgr*)
 *
 * Similar to LoadFileEx but uses TileMap_SetViewport/TileMap_GetTileAt
 * instead of TileMap_UpdateViewport/TileMap_GetTileRect. Also stores
 * object index at +0x108 instead of +0xE4.
 *
 * Called by: World_EnumeratePostLoadAssets (0x45736A)
 */
void __fastcall AssetMgr_EnumFiles(uint32_t* param_1);

/**
 * AssetMgr_EnumerateCategory — Search all entries in a category.
 * Address: 0x45D560, __fastcall (ECX = AssetMgr*)
 *
 * Allocates temporary arrays (4 allocations), then for each of *param_1
 * entries, calls AssetMgr_FindFile. Frees temp arrays on exit.
 *
 * Called by: World_EnumeratePostLoadAssets (0x4573A4, 0x4573CA)
 */
void __fastcall AssetMgr_EnumerateCategory(uint32_t* param_1);

/* ================================================================== */
/* AssetMgr — Tree search and path computation                         */
/* ================================================================== */

/**
 * AssetMgr_FindFile — Search file placement in 4-ary tree.
 * Address: 0x45D5F0, __thiscall (ECX = AssetMgr*, param_1 = uint entry index)
 *
 * Clears result arrays, calls AssetMgr_SearchFile to find optimal path,
 * then calls AssetMgr_GetFileInfo for each entry. Frees temporary tree
 * node on exit.
 */
void __thiscall AssetMgr_FindFile(AssetMgr* self, uint32_t entry_index);

/**
 * AssetMgr_SearchFile — Recursive 4-ary tree search with best-path scoring.
 * Address: 0x45D6C0, __thiscall
 *
 * Recursively walks the 4-ary tree from a starting node to find the
 * shortest path to target node. Allocates path node (0x2C bytes) with
 * adjacency and direction info. Returns path node or NULL.
 *
 * Only runs when g_game_mode == 3.
 *
 * @param this      AssetMgr instance
 * @param currentId Current node ID
 * @param depth     Current search depth
 * @param distance  Current path distance
 * @param parentNode Parent path node for linking
 * @return          Allocated path node, or NULL if no path
 */
uint32_t* __thiscall AssetMgr_SearchFile(AssetMgr* self, uint32_t currentId,
                                          int32_t depth, uint32_t distance,
                                          int32_t parentNode);

/* ================================================================== */
/* AssetMgr — Tree node management                                     */
/* ================================================================== */

/**
 * AssetMgr_TreeFreeNode — Recursively free a 4-ary tree node.
 * Address: 0x45D810, C function
 *
 * Recursively frees the node and all 4 children. Does NOT update
 * lookup tables (raw subtree deletion).
 */
void AssetMgr_TreeFreeNode(void* node);

/**
 * AssetMgr_RemoveNode — Remove node from tree and free.
 * Address: 0x45D850, __thiscall
 *
 * Removes node from lookup tables at +0x24, unlinks from parent at +0x28,
 * then recursively removes children and frees the node.
 */
void __thiscall AssetMgr_RemoveNode(AssetMgr* self, int32_t* node);

/**
 * AssetMgr_ReadFile — MISNAMED: free all tree nodes and clear.
 * Address: 0x45D8C0, __fastcall (ECX = param_1)
 *
 * Walks each entry's 4 child pointers, removes cross-references,
 * frees child nodes, frees entry nodes, clears arrays, resets counts.
 */
void __fastcall AssetMgr_ReadFile(uint32_t* param_1);

/* ================================================================== */
/* AssetMgr — Path computation helpers                                 */
/* ================================================================== */

/**
 * AssetMgr_WriteFile — Recursive shortest path search.
 * Address: 0x45D980, C function
 *
 * Recursively searches for the shortest path between two IDs in the
 * 4-ary tree. Returns path distance or -1 if no path.
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
 * AssetMgr_TraverseTo — Walk tree from start node to target.
 * Address: 0x45DA40, __thiscall
 *
 * Walks the 4-ary tree from startNode to targetId, counting steps
 * at +0x14. Stores direction in direction_byte field.
 */
void __thiscall AssetMgr_TraverseTo(AssetMgr* self, int32_t* startNode, int32_t targetId);

/**
 * AssetMgr_DeleteFile — Write adjacency pair values for all path nodes.
 * Address: 0x45DAD0, __fastcall (ECX = param_1)
 *
 * For each pair of entries in the path list, calls ReadPairValue and
 * WritePairValue to store/reverse direction mapping.
 */
void __fastcall AssetMgr_DeleteFile(void* param_1);

/**
 * AssetMgr_GetFileInfo — Compute shortest path between two nodes.
 * Address: 0x45DBC0, __thiscall
 *
 * Calls WriteFile recursively to find shortest path, then TraverseTo
 * to build direction path, then DeleteFile to write adjacency pairs.
 * Returns direction byte or 0xFF on failure.
 */
uint8_t __thiscall AssetMgr_GetFileInfo(AssetMgr* self, uint32_t* treeNode,
                                         uint32_t fromId, uint32_t toId);

/* ================================================================== */
/* AssetMgr — Pair matrix access                                       */
/* ================================================================== */

/**
 * DirectPlay_SessionMgr — Walk tree storing IDs and directions.
 * Address: 0x45DA70, __thiscall
 *
 * Walks the 4-ary tree from startNode to targetId, storing each node's ID
 * in the path buffer at +0x1C and the direction byte in the direction
 * buffer at +0x18. Counter at +0x14 is updated.
 */
void __thiscall DirectPlay_SessionMgr(AssetMgr* self, int32_t* startNode, int32_t targetId);

/**
 * AssetMgr_ReadPairValue — Read 2-bit pair value from matrix.
 * Address: 0x45DD80, __thiscall
 *
 * Reads a 2-bit direction value from a packed upper-triangular matrix.
 * Matrix stored at +0x04, N entries count at +0x00.
 *
 * @param this  AssetMgr instance
 * @param a     First node ID
 * @param b     Second node ID
 * @return      0-3 direction, 0x80 = valid empty, 0xFF = invalid
 */
uint8_t __thiscall AssetMgr_ReadPairValue(AssetMgr* self, uint32_t a, uint32_t b);

/**
 * AssetMgr_WritePairValue — Write 2-bit pair value to matrix.
 * Address: 0x45DDE0, __thiscall
 *
 * Writes a 2-bit direction value to the packed upper-triangular matrix.
 * Value 0xFF clears the entry.
 */
void __thiscall AssetMgr_WritePairValue(AssetMgr* self, uint32_t a, uint32_t b, uint8_t value);

/* ================================================================== */
/* Global state                                                       */
/* ================================================================== */

extern int32_t g_game_mode;  /* 0x4851F4 */
