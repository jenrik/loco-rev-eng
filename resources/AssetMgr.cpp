/**
 * AssetMgr.cpp — Asset Manager implementation for Lego Loco
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * The Asset Manager manages a 4-ary tile-adjacency tree — NOT the file
 * archive loader (that turned out to be a completely different,
 * unrelated class colliding only on a Ghidra auto-generated name; see
 * resources/AssetArchive.h/.cpp, formerly native/assetmgr_loadfile.c).
 * Functions here handle tree traversal, adjacency computation, and packed
 * pair matrix access.
 *
 * Struct layout (partial, see AssetMgr.h for the canonical/evidenced copy):
 *   AssetMgr:
 *     +0x00: entry_count (int) — number of entries
 *     +0x04: pair_matrix (uint8_t*) — packed adjacency pairs, upper-tri
 *     +0x08: resource_array (void**) — per-entry resource table
 *     +0x10: active_flag (uint8_t*) — per-entry active flags
 *     +0x14: traversal_step (int) — step counter
 *     +0x18: step_direction_buf (uint8_t*) — allocated per-step direction bytes
 *     +0x1C: step_node_id_buf (int*) — allocated per-step node-id words
 *     +0x20: result_array (uint*) — allocated result array
 *     +0x24: tree_entry (uint*) — tree entry lookup
 *     +0x28: heap_entry (uint*) — heap entry lookup
 *
 * Directory entry (0x2C bytes per node):
 *     +0x00: node_id (int)
 *     +0x04: parent_ptr (int)
 *     +0x08: children[4] (int[4], 16 bytes)
 *     +0x18: subtree_refs[4] (int[4], 16 bytes)
 *     +0x28: direction_byte (byte)
 */

// Status: TRANSCRIBED

#include "AssetMgr.h"
#include <new>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern int32_t  g_game_mode;          /* 0x4851F4 */
extern int32_t  g_object_count;       /* 0x4AAD04 */
extern void*    g_tilemap;            /* 0x4AAD08 */

/* ================================================================== */
/* Timer_Resize / Timer helpers (SEH-protected dynamic array)          */
/* ================================================================== */
extern void __fastcall Timer_Resize(void* arr, int32_t count);
extern void* DAT_004a9994;  /* game object collection */

/* TileMap functions */
extern uint8_t __cdecl TileMap_UpdateViewport(void* tilemap, void* obj, int16_t param);
extern uint8_t __cdecl TileMap_SetViewport(void* tilemap, void* obj);
extern void   __cdecl TileMap_GetTileRect(void* tilemap, void* obj);
extern void   __cdecl TileMap_GetTileAt(void* tilemap, void* obj);
extern char   g_empty_string;         /* 0x4851D0 */

/* Error string */
const char s_invalid_path_error[] = "ERROR: Invalid path in GetOppositeDirection";

namespace {
using CollectionLookup = int (__thiscall*)(uint32_t);

class CollectionView {
public:
    virtual void Slot0(uint8_t flags) = 0;
    virtual void Slot1() = 0;
    virtual void Slot2() = 0;
    virtual void Remove(uint32_t index) = 0;
    virtual void Slot4() = 0;
    virtual void Slot5() = 0;
    virtual void Slot6() = 0;
    virtual void Slot7() = 0;
    virtual void* Get(uint32_t index) = 0;
    virtual void Slot9() = 0;
    virtual int32_t Add(uint32_t index, void* object) = 0;

protected:
    ~CollectionView() = default;
};

class CollectionFirstView {
public:
    virtual void Slot0(uint8_t flags) = 0;
    virtual void Slot1() = 0;
    virtual void Slot2() = 0;
    virtual void Slot3() = 0;
    virtual void Slot4() = 0;
    virtual void Slot5() = 0;
    virtual void Slot6() = 0;
    virtual void Slot7() = 0;
    virtual void* First() = 0;

protected:
    ~CollectionFirstView() = default;
};

void** timer_array()
{
    return reinterpret_cast<void**>(&g_empty_string);
}

void* collection_object(void** array, uint32_t index)
{
    return reinterpret_cast<CollectionView*>(array)->Get(index);
}

void* global_collection_object(uint32_t index)
{
    const CollectionLookup lookup = reinterpret_cast<CollectionLookup>(DAT_004a9994);
    return reinterpret_cast<void*>(static_cast<uintptr_t>(lookup(index)));
}

void* collection_first(void** array)
{
    return reinterpret_cast<CollectionFirstView*>(array)->First();
}

int32_t collection_add(void** array, uint32_t index, void* object)
{
    return reinterpret_cast<CollectionView*>(array)->Add(index, object);
}

void collection_remove(void** array, uint32_t index)
{
    reinterpret_cast<CollectionView*>(array)->Remove(index);
}

template <typename T>
T* object_field(void* object, size_t offset)
{
    return reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(object) + offset);
}

template <typename T>
T pointer_from_word(uint32_t value)
{
    return reinterpret_cast<T>(static_cast<uintptr_t>(value));
}

uint32_t pointer_to_word(const void* pointer)
{
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pointer));
}
}

/* ================================================================== */
/* Direction reversal helper — returns opposite direction              */
/* 0->2, 1->3, 2->0, 3->1                                            */
/* ================================================================== */
static int32_t OppositeDirection(int32_t dir)
{
    switch (dir) {
    case 0: return 2;
    case 1: return 3;
    case 2: return 0;
    case 3: return 1;
    default:
        OutputDebugStringA(s_invalid_path_error);
        return 0xFF;
    }
}

/* ================================================================== */
/* AssetMgr::UpdateAdjacencyGraph — Rebuild the 4-way tile adjacency   */
/* graph (previously AssetMgr_LoadFileEx — misnamed)                   */
/* Address: 0x45CE40                                                   */
/*                                                                     */
/* Builds a 4-way tile adjacency graph:                                */
/*   1. Frees old tree data via ClearTree                              */
/*   2. Iterates game objects, checks TileMap viewport inclusion       */
/*   3. Allocates entry array (0x2C bytes each)                        */
/*   4. Links each entry's 4 neighbor directions to neighbors          */
/*   5. Allocates packed pair matrix (pair_matrix, +0x04)              */
/*                                                                     */
/* Called by: TileMap::UpdateAll (0x45735B)                            */
/* ================================================================== */
void AssetMgr::UpdateAdjacencyGraph()
{
    /* SEH prologue */
    void** timer_arr = timer_array();
    uint32_t timer_cap = 0;
    void* exception_buf = nullptr;
    uint32_t accepted_count = 0;

    Timer_Resize(&timer_arr, 10);
    timer_arr = timer_array();  /* timer type change */

    /* Step 1: Free old data */
    ClearTree();

    /* Step 2: Iterate all game objects, count those in viewport */
    for (uint32_t i = 0; i < static_cast<uint32_t>(g_object_count); i++) {
        /* Get object via the global collection lookup operation. */
        void* obj = global_collection_object(i);
        *object_field<int>(obj, 0xE4) = -1;  /* clear neighbor index */

        if (TileMap_UpdateViewport(g_tilemap, obj, static_cast<int16_t>(mode))) {
            /* Grow timer array if needed */
            if (timer_cap <= accepted_count) {
                Timer_Resize(&timer_arr, 1 - static_cast<int32_t>(accepted_count));
            }
            uint32_t idx = accepted_count;
            accepted_count++;
            int32_t result = collection_add(timer_arr, idx, obj);
            if (result == 0) {
                accepted_count--;
            }
        }
    }

    entry_count = accepted_count;

    /* Step 3: Allocate entry array (one 0x2C-byte entry per entry) */
    if (accepted_count != 0) {
        uint32_t* entry_arr = static_cast<uint32_t*>(CRT_malloc_zero(accepted_count * 4));
        resource_array = reinterpret_cast<void**>(entry_arr);
        for (uint32_t i = 0; i < accepted_count; i++) {
            entry_arr[i] = 0;
        }
    }

    /* Step 4: Allocate and initialize each entry. 0x2C is a fixed-size raw
     * array of 11 uint32_t "word" slots (truncated-pointer handles via
     * pointer_to_word/pointer_from_word below, not native pointer members)
     * — every write stays within entry[0..10], so this is safe as-is on a
     * 64-bit host; not a C++ object needing sizeof(). */
    for (uint32_t i = 0; i < entry_count; i++) {
        void* entry_mem = operator_new(0x2C);
        reinterpret_cast<uint32_t**>(resource_array)[i] = static_cast<uint32_t*>(entry_mem);
        uint32_t* entry = static_cast<uint32_t*>(entry_mem);
        for (int j = 0; j < 11; j++) {
            entry[j] = 0;
        }
        entry[0] = i;  /* node_id */

        /* Get game object for this entry and assign index */
        uint32_t* obj = static_cast<uint32_t*>(collection_object(timer_arr, i));
        obj[0xE4 / 4] = i;
    }

    /* Step 5: Get tile rects for all objects */
    for (uint32_t i = 0; i < static_cast<uint32_t>(g_object_count); i++) {
        void* obj = global_collection_object(i);
        TileMap_GetTileRect(g_tilemap, obj);
    }

    /* Step 6: Build adjacency links between entries */
    for (uint32_t srcIdx = 0; srcIdx < entry_count; srcIdx++) {
        uint32_t* srcObj = static_cast<uint32_t*>(collection_object(timer_arr, srcIdx));
        int32_t* neighbor_ptrs = object_field<int32_t>(srcObj, 0xC4); /* 4 neighbor slots */

        uint32_t* srcEntry = reinterpret_cast<uint32_t**>(resource_array)[srcIdx];
        srcEntry[1] = pointer_to_word(srcObj);  /* parent_ptr */

        int32_t* dir_slots = reinterpret_cast<int32_t*>(srcEntry + 2); /* children[4] */
        int32_t* ref_slots = reinterpret_cast<int32_t*>(srcEntry + 6); /* subtree_refs[4] */

        for (int dir = 0; dir < 4; dir++) {
            if (neighbor_ptrs[dir] == 0) {
                ref_slots[dir] = 0;
                dir_slots[dir] = 0;
            } else {
                uint32_t neighborIdx = *object_field<uint32_t>(
                    reinterpret_cast<void*>(static_cast<uintptr_t>(neighbor_ptrs[dir])), 0xE4);
                if (neighborIdx < entry_count) {
                    ref_slots[dir] = pointer_to_word(
                        reinterpret_cast<uint32_t**>(resource_array)[neighborIdx]);
                    if (dir_slots[dir] == 0) {
                        /* Create 0x10-byte edge node linking src->neighbor.
                         * Fixed-size raw array of 4 int32_t slots (edge[0..3]
                         * below) — no pointer members, so this is safe as-is
                         * on a 64-bit host; not a C++ object needing sizeof(). */
                        int32_t* edge = static_cast<int32_t*>(operator_new(0x10));
                        dir_slots[dir] = static_cast<int32_t>(reinterpret_cast<uintptr_t>(edge));
                        edge[1] = 0;
                        edge[0] = neighbor_ptrs[4];  /* store neighbor data */
                        edge[2] = srcIdx;
                        edge[3] = neighborIdx;

                        int32_t oppDir = OppositeDirection(dir);
                        /* Set back-link in neighbor's entry */
                        reinterpret_cast<uint32_t**>(resource_array)[neighborIdx][2 + oppDir] =
                            pointer_to_word(edge);
                    }
                }
            }
        }
    }

    /* Step 7: Free timer objects */
    while (accepted_count != 0) {
        collection_remove(timer_arr, accepted_count - 1);
    }

    /* Step 8: Free old pair buffer and allocate new one */
    if (pair_matrix != nullptr) {
        CRT_free(pair_matrix);
        pair_matrix = nullptr;
    }

    uint32_t n = entry_count;
    if (n != 0) {
        uint32_t matrix_size = (n - 1) * n / 2;
        uint8_t* matrix = static_cast<uint8_t*>(CRT_malloc_zero(matrix_size));
        pair_matrix = matrix;
        /* Initialize all bytes to 0x80 (valid unset) */
        for (uint32_t i = 0; i < matrix_size; i++) {
            matrix[i] = 0x80;
        }
    }
}

/* Free-function compatibility shim over AssetMgr::UpdateAdjacencyGraph —
 * see the evidence comment on the declaration in AssetMgr.h. Only reached
 * from world/tilemap.cpp's `_WIN32`-only branch. */
void AssetMgr_LoadFileEx(uint32_t* ptr)
{
    reinterpret_cast<AssetMgr*>(ptr)->UpdateAdjacencyGraph();
}

/* ================================================================== */
/* AssetMgr::EnumeratePostLoadAdjacency — Rebuild adjacency graph      */
/* (post-load variant, previously AssetMgr_EnumFiles — misnamed)       */
/* Address: 0x45D1C0                                                   */
/*                                                                     */
/* Almost identical to UpdateAdjacencyGraph but:                       */
/*   - Uses TileMap_SetViewport/TileMap_GetTileAt instead              */
/*   - Stores object index at +0x108 instead of +0xE4                  */
/*   - Checks mode for == 7 to choose object field offset              */
/*                                                                     */
/* Called by: World_EnumeratePostLoadAssets (0x45736A)                 */
/* ================================================================== */
void AssetMgr::EnumeratePostLoadAdjacency()
{
    void** timer_arr = timer_array();
    uint32_t timer_cap = 0;
    void* exception_buf = nullptr;
    uint32_t accepted_count = 0;

    Timer_Resize(&timer_arr, 10);
    timer_arr = timer_array();

    /* Free old data */
    ClearTree();

    /* Iterate objects, count those accepted by TileMap_SetViewport */
    for (uint32_t i = 0; i < static_cast<uint32_t>(g_object_count); i++) {
        void* obj = global_collection_object(i);
        *object_field<int>(obj, 0x108) = -1;  /* clear index at different offset */

        if (TileMap_SetViewport(g_tilemap, obj)) {
            if (timer_cap <= accepted_count) {
                Timer_Resize(&timer_arr, 1 - static_cast<int32_t>(accepted_count));
            }
            uint32_t idx = accepted_count;
            accepted_count++;
            int32_t result = collection_add(timer_arr, idx, obj);
            if (result == 0) {
                accepted_count--;
            }
        }
    }

    entry_count = accepted_count;

    /* Allocate entry pointer array */
    if (accepted_count != 0) {
        uint32_t* entry_arr = static_cast<uint32_t*>(CRT_malloc_zero(accepted_count * 4));
        resource_array = reinterpret_cast<void**>(entry_arr);
        for (uint32_t i = 0; i < accepted_count; i++) {
            entry_arr[i] = 0;
        }
    }

    /* Allocate and init entries. 0x2C is a fixed-size raw array of 11
     * uint32_t "word" slots (truncated-pointer handles, not native pointer
     * members) — every write stays within entry[0..10], so this is safe
     * as-is on a 64-bit host; not a C++ object needing sizeof(). */
    for (uint32_t i = 0; i < entry_count; i++) {
        void* entry_mem = operator_new(0x2C);
        reinterpret_cast<uint32_t**>(resource_array)[i] = static_cast<uint32_t*>(entry_mem);
        uint32_t* entry = static_cast<uint32_t*>(entry_mem);
        for (int j = 0; j < 11; j++) {
            entry[j] = 0;
        }
        entry[0] = i;

        if (static_cast<int16_t>(mode) == 7) {
            uint32_t* obj = static_cast<uint32_t*>(collection_first(timer_arr));
            obj[0xE4 / 4] = i;
        } else {
            uint32_t* obj = static_cast<uint32_t*>(collection_object(timer_arr, i));
            obj[0x108 / 4] = i;
        }
    }

    /* Get tile rects */
    for (uint32_t i = 0; i < static_cast<uint32_t>(g_object_count); i++) {
        void* obj = global_collection_object(i);
        TileMap_GetTileAt(g_tilemap, obj);
    }

    /* Build adjacency (same logic as UpdateAdjacencyGraph, but using +0xE8 offset for neighbors) */
    for (uint32_t srcIdx = 0; srcIdx < entry_count; srcIdx++) {
        uint32_t* srcObj = static_cast<uint32_t*>(collection_object(timer_arr, srcIdx));
        int32_t* neighbor_ptrs = object_field<int32_t>(srcObj, 0xE8); /* different offset */

        uint32_t* srcEntry = reinterpret_cast<uint32_t**>(resource_array)[srcIdx];
        srcEntry[1] = pointer_to_word(srcObj);

        int32_t* dir_slots = reinterpret_cast<int32_t*>(srcEntry + 2);
        int32_t* ref_slots = reinterpret_cast<int32_t*>(srcEntry + 6);

        for (int dir = 0; dir < 4; dir++) {
            if (neighbor_ptrs[dir] == 0) {
                ref_slots[dir] = 0;
                dir_slots[dir] = 0;
            } else {
                uint32_t neighborIdx = *object_field<uint32_t>(
                    reinterpret_cast<void*>(static_cast<uintptr_t>(neighbor_ptrs[dir])), 0x108);
                if (neighborIdx < entry_count) {
                    ref_slots[dir] = pointer_to_word(
                        reinterpret_cast<uint32_t**>(resource_array)[neighborIdx]);
                    if (dir_slots[dir] == 0) {
                        /* Fixed-size raw array of 4 int32_t slots (edge[0..3]
                         * below), no pointer members, so this is safe as-is
                         * on a 64-bit host; not a C++ object needing sizeof(). */
                        int32_t* edge = static_cast<int32_t*>(operator_new(0x10));
                        dir_slots[dir] = static_cast<int32_t>(reinterpret_cast<uintptr_t>(edge));
                        edge[1] = 0;
                        edge[0] = neighbor_ptrs[4];
                        edge[2] = srcIdx;
                        edge[3] = neighborIdx;

                        int32_t oppDir = OppositeDirection(dir);
                        reinterpret_cast<uint32_t**>(resource_array)[neighborIdx][2 + oppDir] =
                            pointer_to_word(edge);
                    }
                }
            }
        }
    }

    /* Free timer objects */
    while (accepted_count != 0) {
        collection_remove(timer_arr, accepted_count - 1);
    }

    /* Free old pair buffer, allocate new */
    if (pair_matrix != nullptr) {
        CRT_free(pair_matrix);
        pair_matrix = nullptr;
    }

    uint32_t n = entry_count;
    if (n != 0) {
        uint32_t matrix_size = (n - 1) * n / 2;
        uint8_t* matrix = static_cast<uint8_t*>(CRT_malloc_zero(matrix_size));
        pair_matrix = matrix;
        for (uint32_t i = 0; i < matrix_size; i++) {
            matrix[i] = 0x80;
        }
    }
}

/* Free-function compatibility shim over
 * AssetMgr::EnumeratePostLoadAdjacency — see the evidence comment on the
 * declaration in AssetMgr.h. Only reached from world/tilemap.cpp's
 * `_WIN32`-only branch. */
void AssetMgr_EnumFiles(uint32_t* ptr)
{
    reinterpret_cast<AssetMgr*>(ptr)->EnumeratePostLoadAdjacency();
}

/* ================================================================== */
/* AssetMgr::EnumerateCategory — Search all entries in a category      */
/* Address: 0x45D560                                                   */
/*                                                                     */
/* Allocates 4 temporary scratch arrays (reusing active_flag/           */
/* result_array/tree_entry/heap_entry) then calls FindFile in a loop   */
/* for each entry. Frees the scratch arrays on return.                 */
/*                                                                     */
/* Called by: World_EnumeratePostLoadAssets (twice)                    */
/* ================================================================== */
void AssetMgr::EnumerateCategory()
{
    uint32_t count = entry_count;

    active_flag  = static_cast<uint8_t*>(CRT_malloc_zero(count));       /* temp byte array */
    result_array = static_cast<uint32_t*>(CRT_malloc_zero(count * 4));  /* temp uint32 array */
    tree_entry   = static_cast<uint32_t*>(CRT_malloc_zero(count * 4));  /* temp uint32 array */
    heap_entry   = static_cast<uint32_t*>(CRT_malloc_zero(count * 4));  /* temp uint32 array */

    for (uint32_t i = 0; i < count; i++) {
        FindFile(i);
    }

    CRT_free(result_array);
    CRT_free(tree_entry);
    CRT_free(heap_entry);
    CRT_free(active_flag);
}

/* Free-function compatibility shim over AssetMgr::EnumerateCategory — see
 * the evidence comment on the declaration in AssetMgr.h.
 * world/World_enumerate.cpp is the real caller. */
void AssetMgr_EnumerateCategory(uint32_t* param_1)
{
    reinterpret_cast<AssetMgr*>(param_1)->EnumerateCategory();
}

/* ================================================================== */
/* AssetMgr::FindFile — Search file placement in 4-ary tree            */
/* Address: 0x45D5F0                                                   */
/*                                                                     */
/* Clears result arrays, calls SearchFile + GetFileInfo for each       */
/* entry, then frees the temporary tree node.                          */
/* ================================================================== */
void AssetMgr::FindFile(uint32_t entry_index)
{
    /* Clear result arrays for all entries */
    uint32_t count = entry_count;
    for (uint32_t i = 0; i < count; i++) {
        result_array[i] = 0xFFFFFFFF;
    }

    /* Clear tree entry array (+0x24) */
    uint32_t n = entry_count;
    for (uint32_t i = 0; i < n; i++) {
        tree_entry[i] = 0;
    }

    /* Clear heap entry array (+0x28) */
    for (uint32_t i = 0; i < n; i++) {
        heap_entry[i] = 0;
    }

    /* Clear result array (+0x10) */
    for (uint32_t i = 0; i < n; i++) {
        active_flag[i] = 0;
    }

    /* Search for optimal path */
    uint32_t* result = SearchFile(entry_index, 0, 0, 0);

    /* Get file info for each entry */
    for (uint32_t i = 0; i < count; i++) {
        GetFileInfo(result, entry_index, i);
    }

    /* Free temporary tree node (free 4 subtree references + node) */
    if (result != nullptr) {
        for (int i = 0; i < 4; i++) {
            if (result[i + 6] != 0) {
                AssetMgr_TreeFreeNode(pointer_from_word<void*>(result[i + 6]));
            }
        }
        GLOBAL_free(result);
    }
}

/* ================================================================== */
/* AssetMgr::SearchFile — Recursive 4-ary tree search with best-path  */
/* Address: 0x45D6C0                                                   */
/*                                                                     */
/* Recursively walks the 4-ary tree to find the shortest path to      */
/* target node. Allocates and returns a 0x2C-byte path node.          */
/* Only runs when g_game_mode == 3.                                    */
/* ================================================================== */
uint32_t* AssetMgr::SearchFile(uint32_t currentId, int32_t depth, uint32_t distance,
                                int32_t parentNode)
{
    if (g_game_mode != 3 || currentId > entry_count) {
        return nullptr;
    }

    /* Mark self node as active */
    active_flag[currentId] = 1;

    /* Check if self distance is better than the stored best */
    uint32_t* best_dist = result_array + currentId * 4;
    if (distance < *best_dist) {
        *best_dist = distance;

        /* Allocate path node (0x2C bytes, 11 dwords) — fixed-size raw
         * uint32_t[11] array (path_node[0..10] below), no pointer members,
         * so this is safe as-is on a 64-bit host; not a C++ object needing
         * sizeof(). */
        uint32_t* path_node = static_cast<uint32_t*>(operator_new(0x2C));
        uint32_t* src_entry = *reinterpret_cast<uint32_t**>(
            reinterpret_cast<uint8_t*>(resource_array) + currentId * 4);

        /* Copy source entry data into new node */
        for (int i = 0; i < 11; i++) {
            path_node[i] = src_entry[i];
        }

        /* Replace in tree entry lookup */
        if (tree_entry[currentId] != 0) {
            RemoveNode(pointer_from_word<int32_t*>(tree_entry[currentId]));
        }
        tree_entry[currentId] = pointer_to_word(path_node);

        /* Store parent/depth info */
        heap_entry[currentId * 4] = static_cast<uint32_t>(parentNode);

        /* Save original child refs, clear ours */
        int32_t saved_refs[4];
        int32_t* child_slots = reinterpret_cast<int32_t*>(path_node + 2);
        for (int i = 0; i < 4; i++) {
            saved_refs[i] = child_slots[i];
            if (child_slots[i] != 0) {
                uint32_t child_entry = *reinterpret_cast<uint32_t*>(
                    static_cast<uintptr_t>(child_slots[i]) + 4);
                saved_refs[i] = child_entry;
                if (child_entry != 0) {
                    child_slots[4 + i] = 0;  /* clear ref_slot */
                }
            }
        }

        /* Recursively search children */
        for (int i = 0; i < 4; i++) {
            if (child_slots[i] != 0) {
                int32_t* edge = pointer_from_word<int32_t*>(child_slots[i]);
                edge[1] = 1;  /* mark as visited */
                int32_t* edge_data = pointer_from_word<int32_t*>(edge[0]);
                uint32_t childId = edge_data[2];
                if (childId == currentId) {
                    childId = edge_data[3];
                }
                uint32_t* child_result = SearchFile(
                    childId, depth + 1, distance + edge_data[0],
                    static_cast<int32_t>(reinterpret_cast<uintptr_t>(path_node)));
                child_slots[4 + i] = static_cast<int32_t>(
                    reinterpret_cast<uintptr_t>(child_result));
            }
        }

        /* Restore saved refs */
        for (int i = 0; i < 4; i++) {
            if (child_slots[i] != 0) {
                int32_t* edge = pointer_from_word<int32_t*>(child_slots[i]);
                edge[1] = saved_refs[i];
            }
        }

        return path_node;
    }

    return nullptr;
}

/* ================================================================== */
/* AssetMgr_TreeFreeNode — Recursively free a 4-ary tree node         */
/* Address: 0x45D810                                                   */
/*                                                                     */
/* Does NOT update lookup tables — raw subtree deletion.              */
/* ================================================================== */
void AssetMgr_TreeFreeNode(void* node)
{
    if (node == nullptr) return;

    uint32_t* children = object_field<uint32_t>(node, 0x18);
    for (int i = 0; i < 4; i++) {
        if (children[i] != 0) {
            AssetMgr_TreeFreeNode(pointer_from_word<void*>(children[i]));
        }
    }
    GLOBAL_free(node);
}

/* ================================================================== */
/* AssetMgr::RemoveNode — Remove node from tree and free              */
/* Address: 0x45D850                                                   */
/*                                                                     */
/* Removes from lookup table at +0x24, unlinks from parent at +0x28,  */
/* then recursively removes children and frees.                        */
/* ================================================================== */
void AssetMgr::RemoveNode(int32_t* node)
{
    if (node == nullptr) return;

    /* Remove from tree entry lookup at +0x24 */
    tree_entry[node[0] * 4] = 0;

    /* Unlink from parent at +0x28 */
    int32_t parent_word = static_cast<int32_t>(heap_entry[node[0] * 4]);
    void* parent = reinterpret_cast<void*>(static_cast<uintptr_t>(parent_word));
    for (uint32_t offset = 0x18; offset < 0x28; offset += 4) {
        if (*object_field<int32_t*>(parent, offset) == node) {
            *object_field<uint32_t>(parent, offset) = 0;
        }
    }

    /* Recursively remove children */
    int32_t* child_ptr = node + 6;
    for (int i = 0; i < 4; i++) {
        if (child_ptr[i] != 0) {
            RemoveNode(pointer_from_word<int32_t*>(child_ptr[i]));
        }
    }

    GLOBAL_free(node);
}

/* ================================================================== */
/* AssetMgr::ClearTree — Free all tree nodes and reset (previously     */
/* AssetMgr_ReadFile — misnamed)                                       */
/* Address: 0x45D8C0                                                   */
/*                                                                     */
/* Walks each entry's 4 child nodes, removes cross-references, frees  */
/* child nodes, frees entry nodes, clears arrays.                      */
/* ================================================================== */
void AssetMgr::ClearTree()
{
    if (resource_array == nullptr) return;

    uint32_t* entries = reinterpret_cast<uint32_t*>(resource_array);

    /* For each entry, remove cross-references in children, then free */
    for (uint32_t i = 0; i < entry_count; i++) {
        uint32_t* entry = pointer_from_word<uint32_t*>(entries[i]);
        /* Scan children at offsets 8, 0xC, 0x10, 0x14 */
        for (uint32_t childOff = 8; childOff < 0x18; childOff += 4) {
            uint32_t child_word = *object_field<uint32_t>(entry, childOff);
            if (child_word != 0) {
                void* child = pointer_from_word<void*>(child_word);
                uint32_t childId = *object_field<uint32_t>(child, 8);
                if (childId == i) {
                    childId = *object_field<uint32_t>(child, 0xC);
                }
                /* Remove back-reference from child's entry */
                uint32_t* childEntry = pointer_from_word<uint32_t*>(entries[childId]);
                for (uint32_t backOff = 8; backOff < 0x18; backOff += 4) {
                    if (*object_field<uint32_t>(childEntry, backOff) == child_word) {
                        *object_field<uint32_t>(childEntry, backOff) = 0;
                    }
                }
                GLOBAL_free(child);
            }
        }
    }

    /* Free entry nodes themselves */
    for (uint32_t i = 0; i < entry_count; i++) {
        GLOBAL_free(pointer_from_word<void*>(entries[i]));
        entries[i] = 0;
    }

    /* Free entry array and reset */
    CRT_free(resource_array);
    entry_count = 0;
    resource_array = nullptr;
}

/* AssetMgr_ReadFile free-function shim removed 2026-08-14: its one real
 * caller (formerly "DDRAW_SpriteDataDtor", graphics/DDRAW.cpp) was
 * SpriteData::~SpriteData(), which is now AssetMgr::~AssetMgr() below —
 * an ordinary member function calling ClearTree() directly, no
 * free-function bridge needed. */

/* ================================================================== */
/* AssetMgr::AssetMgr / AssetMgr::~AssetMgr                            */
/* Addresses: 0x45CDF0 / 0x45CE10 — see AssetMgr.h for full evidence.   */
/* ================================================================== */
AssetMgr::AssetMgr(uint16_t mode)
    : entry_count(0)
    , pair_matrix(nullptr)
    , resource_array(nullptr)
    , mode(mode)
    , active_flag(nullptr)
    , traversal_step(0)
    , step_direction_buf(nullptr)
    , step_node_id_buf(nullptr)
    , result_array(nullptr)
    , tree_entry(nullptr)
    , heap_entry(nullptr)
{
}

AssetMgr::~AssetMgr()
{
    ClearTree();
    if (pair_matrix != nullptr) {
        CRT_free(pair_matrix);
        pair_matrix = nullptr;
    }
}

/* Thin bridges so callers that can't safely #include this header (its
 * extern "C" operator_new/GLOBAL_free/CRT_wcsstr block conflicts with
 * their own locally-declared, differently-typed copies — see
 * world/tilemap.cpp) can still construct/destroy a real AssetMgr through
 * a minimal signature. */
void* AssetMgr_Construct(void* mem, uint16_t mode)
{
    return new (mem) AssetMgr(mode);
}

void AssetMgr_Destruct(void* obj)
{
    static_cast<AssetMgr*>(obj)->~AssetMgr();
}

/* AssetMgr_Size — real host sizeof(AssetMgr) for callers across the same
 * boundary that can't #include this header. sizeof(AssetMgr) is NOT 0x2C
 * on this host: every field past mode is a raw pointer (pair_matrix,
 * resource_array, active_flag, step_direction_buf, step_node_id_buf,
 * result_array, tree_entry, heap_entry), 8 bytes here vs. 4 on the
 * original x86 — the stale 0x2C literal at TileMap::TileMap()'s
 * operator_new call site undersizes the allocation and corrupts the heap
 * (caught live via meson test's integration suite, 2026-08-14; see
 * PROGRESS.md). */
size_t AssetMgr_Size()
{
    return sizeof(AssetMgr);
}

/* ================================================================== */
/* AssetMgr_WriteFile — Recursive shortest path search                */
/* Address: 0x45D980                                                   */
/*                                                                     */
/* Recursively searches for shortest path between two IDs in the       */
/* 4-ary tree. Returns path distance or -1 if no path found.           */
/* Stores direction in direction_byte field.                           */
/* ================================================================== */
int32_t AssetMgr_WriteFile(int32_t* param_1, int32_t param_2, int32_t param_3,
                            uint32_t param_4, uint32_t param_5)
{
    if (param_1 == nullptr) return -1;
    if (param_1[0] == param_3) return 0;  /* reached target */
    if (param_4 <= param_5) return -1;    /* exceeded max distance */

    int32_t best_dist = -1;
    uint8_t best_dir = 0xFF;

    int32_t* child_refs = param_1 + 2;
    for (uint8_t dir = 0; dir < 4; dir++) {
        if (child_refs[dir] != 0) {
            int32_t result = AssetMgr_WriteFile(
                pointer_from_word<int32_t*>(child_refs[4 + dir]),
                param_2, param_3, param_4,
                param_5 + child_refs[0]);

            if (result != -1) {
                int32_t dist = result + child_refs[0];
                if (best_dist == -1 || dist < best_dist) {
                    best_dist = dist;
                    best_dir = dir;
                }
            }
        }
    }

    if (best_dist != -1) {
        *object_field<uint8_t>(param_1, 0x28) = best_dir;  /* direction_byte */
    }
    return best_dist;
}

/* ================================================================== */
/* AssetMgr::TraverseTo — Walk tree from start node to target          */
/* Address: 0x45DA40                                                   */
/*                                                                     */
/* Walks the 4-ary tree from startNode to targetId, counting steps    */
/* at +0x14. Stores each node's direction byte.                        */
/* ================================================================== */
void AssetMgr::TraverseTo(int32_t* startNode, int32_t targetId)
{
    int32_t steps = traversal_step + 1;
    traversal_step = steps;

    int32_t nodeId = startNode[0];
    while (nodeId != targetId) {
        steps++;
        startNode = pointer_from_word<int32_t*>(
            startNode[6 + *object_field<uint8_t>(startNode, 0x28)]);
        traversal_step = steps;
        nodeId = startNode[0];
    }
}

/* ================================================================== */
/* AssetMgr::WriteAdjacencyPairs — Write adjacency pair values for all */
/* nodes (previously AssetMgr_DeleteFile — misnamed)                   */
/* Address: 0x45DAD0                                                   */
/*                                                                     */
/* For each pair of entries in the path, reads the existing pair value */
/* and writes the reverse direction if unset (value 0x80).            */
/* ================================================================== */
void AssetMgr::WriteAdjacencyPairs()
{
    uint32_t count = traversal_step;
    if (count <= 1) return;

    uint32_t* id_buf = reinterpret_cast<uint32_t*>(step_node_id_buf);
    uint8_t* dir_buf = step_direction_buf;

    for (uint32_t i = 0; i < count - 1; i++) {
        uint32_t idA = id_buf[i];
        for (uint32_t j = i + 1; j < count; j++) {
            uint32_t idB = id_buf[j];

            uint8_t pair_val = ReadPairValue(idA, idB);
            if (pair_val == 0x80) {
                WritePairValue(idA, idB, dir_buf[i]);
                int32_t opp_dir = OppositeDirection(dir_buf[j - 1]);
                WritePairValue(idB, idA, static_cast<uint8_t>(opp_dir));
            }
        }
    }
}

/* ================================================================== */
/* AssetMgr::GetFileInfo — Compute shortest path between two nodes    */
/* Address: 0x45DBC0                                                   */
/*                                                                     */
/* Calls WriteFile for shortest path, then TraverseTo to build        */
/* direction path, then WriteAdjacencyPairs to write adjacency pairs.  */
/* Returns direction byte or 0xFF on failure.                          */
/* ================================================================== */
uint8_t AssetMgr::GetFileInfo(uint32_t* treeNode, uint32_t fromId, uint32_t toId)
{
    uint8_t pair_val = ReadPairValue(fromId, toId);
    if (pair_val != 0x80) {
        return 0xFF;  /* already has a value */
    }

    /* Check if path is valid */
    if (active_flag[toId] != 0 && fromId != toId) {
        uint32_t best_dist = 0xFFFFFFFF;
        int32_t best_dir_results[4];
        uint32_t* child_slots = treeNode + 6;

        for (int i = 0; i < 4; i++) {
            best_dir_results[i] = AssetMgr_WriteFile(
                pointer_from_word<int32_t*>(child_slots[i]),
                static_cast<int32_t>(fromId), static_cast<int32_t>(toId),
                best_dist, 0);

            if (best_dir_results[i] != -1 &&
                (best_dist == 0xFFFFFFFF ||
                 best_dir_results[i] < static_cast<int32_t>(best_dist))) {
                best_dist = best_dir_results[i];
            }
        }

        /* Find best direction from children */
        uint8_t best_dir = 0;
        uint32_t min_dist = best_dir_results[0];
        for (int i = 1; i < 4; i++) {
            if (best_dir_results[i] != -1 &&
                (min_dist == 0xFFFFFFFF ||
                 best_dir_results[i] < static_cast<int32_t>(min_dist))) {
                min_dist = best_dir_results[i];
                best_dir = static_cast<uint8_t>(i);
            }
        }

        if (min_dist != 0xFFFFFFFF) {
            /* Store direction byte and traverse */
            *object_field<uint8_t>(treeNode, 0x28) = best_dir;
            traversal_step = 1;

            if (treeNode[0] != toId) {
                TraverseTo(
                    pointer_from_word<int32_t*>(
                        treeNode[6 + *object_field<uint8_t>(treeNode, 0x28)]), toId);
            }

            /* Allocate path buffers: step_direction_buf is a per-step byte
             * array (traversal_step bytes), step_node_id_buf a per-step
             * word array (traversal_step * 4 bytes) — see the field-swap
             * evidence comment on the struct definition in AssetMgr.h. */
            step_direction_buf = static_cast<uint8_t*>(CRT_malloc_zero(traversal_step));
            step_node_id_buf = static_cast<int32_t*>(CRT_malloc_zero(traversal_step * 4));

            traversal_step = 0;

            /* Store first entry */
            reinterpret_cast<uint32_t*>(step_node_id_buf)[0] = treeNode[0];
            step_direction_buf[traversal_step] = static_cast<uint8_t>(treeNode[10]);
            traversal_step = traversal_step + 1;

            if (treeNode[0] != toId) {
                RecordPath(
                    pointer_from_word<int32_t*>(
                        treeNode[6 + *object_field<uint8_t>(treeNode, 0x28)]), toId);
            }

            /* Write adjacency pairs and clean up */
            WriteAdjacencyPairs();

            CRT_free(step_direction_buf);
            CRT_free(step_node_id_buf);
        }

        if (min_dist == 0xFFFFFFFF) {
            WritePairValue(fromId, toId, 0xFF);
            return 0xFF;
        }

        return best_dir;
    }

    WritePairValue(fromId, toId, 0xFF);
    return 0xFF;
}

/* ================================================================== */
/* AssetMgr::RecordPath — Walk tree storing per-step IDs and           */
/* directions (previously DirectPlay_SessionMgr — Ghidra auto-analysis */
/* mislabel; unrelated to DirectPlay)                                  */
/* Address: 0x45DA70                                                   */
/*                                                                     */
/* Walks the 4-ary tree from startNode to targetId, storing each      */
/* node's ID into step_node_id_buf (+0x1C) and its direction byte     */
/* into step_direction_buf (+0x18).                                    */
/* ================================================================== */
void AssetMgr::RecordPath(int32_t* startNode, int32_t targetId)
{
    uint32_t* node_id_buf = reinterpret_cast<uint32_t*>(step_node_id_buf);
    uint8_t* direction_buf = step_direction_buf;
    int32_t* step_ptr = &traversal_step;

    node_id_buf[*step_ptr] = startNode[0];
    direction_buf[*step_ptr] = static_cast<uint8_t>(startNode[10]);
    (*step_ptr)++;

    int32_t nodeId = startNode[0];
    while (nodeId != targetId) {
        startNode = pointer_from_word<int32_t*>(startNode[6 + startNode[10]]);
        node_id_buf[*step_ptr] = startNode[0];
        direction_buf[*step_ptr] = static_cast<uint8_t>(startNode[10]);
        (*step_ptr)++;
        nodeId = startNode[0];
    }
}

/* ================================================================== */
/* AssetMgr::ReadPairValue — Read 2-bit pair value from matrix        */
/* Address: 0x45DD80                                                   */
/*                                                                     */
/* Reads a 2-bit direction value from a packed upper-triangular       */
/* matrix. Returns 0-3 direction, 0x80 for valid unset, 0xFF invalid. */
/* ================================================================== */
uint8_t AssetMgr::ReadPairValue(uint32_t a, uint32_t b)
{
    uint32_t count = entry_count;

    if (a == b || a >= count || b >= count) {
        return 0xFF;
    }

    /* Upper-triangular index: i > j */
    uint32_t i = a, j = b;
    if (a < b) {
        i = b;
        j = a;
    }

    uint32_t offset = (i - 1) * i / 2 + j;
    uint8_t* matrix = pair_matrix;
    uint8_t val = matrix[offset];

    if (val == 0xFF) return 0xFF;
    if (val == 0x80) return 0x80;

    if (a < b) {
        val >>= 2;  /* upper pair */
    }
    return val & 3;
}

/* Free-function compatibility shim over AssetMgr::ReadPairValue — see the
 * evidence comment on the declaration in AssetMgr.h. */
uint8_t AssetMgr_ReadPairValue(AssetMgr* self, uint32_t a, uint32_t b)
{
    return self->ReadPairValue(a, b);
}

/* ================================================================== */
/* AssetMgr::WritePairValue — Write 2-bit pair value to matrix        */
/* Address: 0x45DDE0                                                   */
/*                                                                     */
/* Writes a 2-bit direction value to the packed upper-triangular      */
/* matrix. Value 0xFF clears the entry.                                */
/* ================================================================== */
void AssetMgr::WritePairValue(uint32_t a, uint32_t b, uint8_t value)
{
    if (a == b) return;

    uint32_t i = a, j = b;
    if (a < b) {
        i = b;
        j = a;
    }

    uint32_t offset = (i - 1) * i / 2 + j;
    uint8_t* matrix = pair_matrix;
    uint8_t* cell = &matrix[offset];
    uint8_t cur = *cell;

    if (cur == 0x80) {
        cur = 0;  /* clear the valid-unset marker */
    }

    if (value == 0xFF) {
        *cell = 0xFF;
        return;
    }

    if (a < b) {
        value <<= 2;  /* write to upper pair */
    }
    *cell = cur | value;
}
