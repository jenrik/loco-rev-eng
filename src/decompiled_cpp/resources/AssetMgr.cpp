/**
 * AssetMgr.cpp — Asset Manager implementation for Lego Loco
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * The Asset Manager manages a 4-ary tree of game resource files loaded
 * from the game data archive. Functions here handle file loading (already
 * in native/assetmgr_loadfile.c), tree traversal, adjacency computation,
 * and packed pair matrix access.
 *
 * Struct layout (partial):
 *   AssetMgr:
 *     +0x00: entry_count (int) — number of entries
 *     +0x04: pair_matrix (uint8_t*) — packed adjacency pairs, upper-tri
 *     +0x08: resource_array (void**) — per-entry resource table
 *     +0x10: active_flag (uint8_t*) — per-entry active flags
 *     +0x14: traversal_step (int) — step counter
 *     +0x18: path_id_buf (int*) — allocated ID path buffer
 *     +0x1C: path_dir_buf (int*) — allocated direction buffer
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

#include "AssetMgr.h"

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
/* AssetMgr_LoadFileEx — Extended asset loader with adjacency graph    */
/* Address: 0x45CE40                                                   */
/*                                                                     */
/* After loading files, builds a 4-way tile adjacency graph:           */
/*   1. Frees old tree data via AssetMgr_ReadFile                      */
/*   2. Iterates game objects, checks TileMap viewport inclusion       */
/*   3. Allocates entry array (0x2C bytes each)                        */
/*   4. Links each entry's 4 neighbor directions to neighbors          */
/*   5. Allocates packed pair matrix at param_1[1]                     */
/*                                                                     */
/* Called by: TileMap_UpdateAll (0x45735B)                             */
/* ================================================================== */
void __fastcall AssetMgr_LoadFileEx(uint32_t* param_1)
{
    /* SEH prologue */
    void** timer_arr = (void**)(&g_empty_string);
    uint32_t timer_cap = 0;
    void* exception_buf = 0;
    uint32_t entry_count = 0;

    Timer_Resize(&timer_arr, 10);
    timer_arr = (void**)(&g_empty_string);  /* timer type change */

    /* Step 1: Free old data */
    AssetMgr_ReadFile(param_1);

    /* Step 2: Iterate all game objects, count those in viewport */
    for (uint32_t i = 0; i < (uint32_t)g_object_count; i++) {
        /* Get object via collection vtable[8] */
        void* obj = (void*)((int (__thiscall*)(uint32_t))DAT_004a9994)(i);
        *(int*)((uint8_t*)obj + 0xE4) = -1;  /* clear neighbor index */

        if (TileMap_UpdateViewport(g_tilemap, obj, (int16_t)param_1[3])) {
            /* Grow timer array if needed */
            if (timer_cap <= entry_count) {
                Timer_Resize(&timer_arr, 1 - (int32_t)entry_count);
            }
            uint32_t idx = entry_count;
            entry_count++;
            int32_t result = ((int (__thiscall*)(uint32_t, void*))timer_arr[10])(idx, obj);
            if (result == 0) {
                entry_count--;
            }
        }
    }

    param_1[0] = entry_count;

    /* Step 3: Allocate entry array (one 0x2C-byte entry per entry) */
    if (entry_count != 0) {
        uint32_t* entry_arr = (uint32_t*)CRT_malloc_zero(entry_count * 4);
        param_1[2] = (uint32_t)entry_arr;
        for (uint32_t i = 0; i < entry_count; i++) {
            entry_arr[i] = 0;
        }
    }

    /* Step 4: Allocate and initialize each entry */
    for (uint32_t i = 0; i < param_1[0]; i++) {
        void* entry_mem = operator_new(0x2C);
        ((uint32_t*)param_1[2])[i] = (uint32_t)entry_mem;
        uint32_t* entry = (uint32_t*)entry_mem;
        for (int j = 0; j < 11; j++) {
            entry[j] = 0;
        }
        entry[0] = i;  /* node_id */

        /* Get game object for this entry and assign index */
        uint32_t* obj = (uint32_t*)((int (__thiscall*)(uint32_t))timer_arr[8])(i);
        obj[0xE4 / 4] = i;
    }

    /* Step 5: Get tile rects for all objects */
    for (uint32_t i = 0; i < (uint32_t)g_object_count; i++) {
        void* obj = (void*)((int (__thiscall*)(uint32_t))DAT_004a9994)(i);
        TileMap_GetTileRect(g_tilemap, obj);
    }

    /* Step 6: Build adjacency links between entries */
    for (uint32_t srcIdx = 0; srcIdx < param_1[0]; srcIdx++) {
        uint32_t* srcObj = (uint32_t*)((int (__thiscall*)(uint32_t))timer_arr[8])(srcIdx);
        int32_t* neighbor_ptrs = (int32_t*)((uint8_t*)srcObj + 0xC4); /* 4 neighbor slots */

        uint32_t* srcEntry = (uint32_t*)((uint32_t*)param_1[2])[srcIdx];
        srcEntry[1] = (uint32_t)srcObj;  /* parent_ptr */

        int32_t* dir_slots = (int32_t*)(srcEntry + 2); /* children[4] */
        int32_t* ref_slots = (int32_t*)(srcEntry + 6); /* subtree_refs[4] */

        for (int dir = 0; dir < 4; dir++) {
            if (neighbor_ptrs[dir] == 0) {
                ref_slots[dir] = 0;
                dir_slots[dir] = 0;
            } else {
                uint32_t neighborIdx = *(uint32_t*)((uint8_t*)neighbor_ptrs[dir] + 0xE4);
                if (neighborIdx < param_1[0]) {
                    ref_slots[dir] = ((uint32_t*)param_1[2])[neighborIdx];
                    if (dir_slots[dir] == 0) {
                        /* Create 0x10-byte edge node linking src->neighbor */
                        int32_t* edge = (int32_t*)operator_new(0x10);
                        dir_slots[dir] = (int32_t)edge;
                        edge[1] = 0;
                        edge[0] = neighbor_ptrs[4];  /* store neighbor data */
                        edge[2] = srcIdx;
                        edge[3] = neighborIdx;

                        int32_t oppDir = OppositeDirection(dir);
                        /* Set back-link in neighbor's entry */
                        ((uint32_t*)((uint32_t*)param_1[2])[neighborIdx])[2 + oppDir] = (uint32_t)edge;
                    }
                }
            }
        }
    }

    /* Step 7: Free timer objects */
    while (entry_count != 0) {
        ((void (__thiscall*)(uint32_t))timer_arr[3])(entry_count - 1);
    }

    /* Step 8: Free old pair buffer and allocate new one */
    if ((void*)param_1[1] != NULL) {
        CRT_free((void*)param_1[1]);
        param_1[1] = 0;
    }

    uint32_t n = param_1[0];
    if (n != 0) {
        uint32_t matrix_size = (n - 1) * n / 2;
        uint8_t* matrix = (uint8_t*)CRT_malloc_zero(matrix_size);
        param_1[1] = (uint32_t)matrix;
        /* Initialize all bytes to 0x80 (valid unset) */
        for (uint32_t i = 0; i < matrix_size; i++) {
            matrix[i] = 0x80;
        }
    }
}

/* ================================================================== */
/* AssetMgr_EnumFiles — Enumerate and relink tile adjacency            */
/* Address: 0x45D1C0                                                   */
/*                                                                     */
/* Almost identical to LoadFileEx but:                                 */
/*   - Uses TileMap_SetViewport/TileMap_GetTileAt instead              */
/*   - Stores object index at +0x108 instead of +0xE4                  */
/*   - Checks param_1[3] for == 7 to choose object field offset        */
/*                                                                     */
/* Called by: World_EnumeratePostLoadAssets (0x45736A)                 */
/* ================================================================== */
void __fastcall AssetMgr_EnumFiles(uint32_t* param_1)
{
    void** timer_arr = (void**)(&g_empty_string);
    uint32_t timer_cap = 0;
    void* exception_buf = 0;
    uint32_t entry_count = 0;

    Timer_Resize(&timer_arr, 10);
    timer_arr = (void**)(&g_empty_string);

    /* Free old data */
    AssetMgr_ReadFile(param_1);

    /* Iterate objects, count those accepted by TileMap_SetViewport */
    for (uint32_t i = 0; i < (uint32_t)g_object_count; i++) {
        void* obj = (void*)((int (__thiscall*)(uint32_t))DAT_004a9994)(i);
        *(int*)((uint8_t*)obj + 0x108) = -1;  /* clear index at different offset */

        if (TileMap_SetViewport(g_tilemap, obj)) {
            if (timer_cap <= entry_count) {
                Timer_Resize(&timer_arr, 1 - (int32_t)entry_count);
            }
            uint32_t idx = entry_count;
            entry_count++;
            int32_t result = ((int (__thiscall*)(uint32_t, void*))timer_arr[10])(idx, obj);
            if (result == 0) {
                entry_count--;
            }
        }
    }

    param_1[0] = entry_count;

    /* Allocate entry pointer array */
    if (entry_count != 0) {
        uint32_t* entry_arr = (uint32_t*)CRT_malloc_zero(entry_count * 4);
        param_1[2] = (uint32_t)entry_arr;
        for (uint32_t i = 0; i < entry_count; i++) {
            entry_arr[i] = 0;
        }
    }

    /* Allocate and init entries */
    for (uint32_t i = 0; i < param_1[0]; i++) {
        void* entry_mem = operator_new(0x2C);
        ((uint32_t*)param_1[2])[i] = (uint32_t)entry_mem;
        uint32_t* entry = (uint32_t*)entry_mem;
        for (int j = 0; j < 11; j++) {
            entry[j] = 0;
        }
        entry[0] = i;

        if ((int16_t)param_1[3] == 7) {
            uint32_t* obj = (uint32_t*)((int (__thiscall*)())timer_arr[8])();
            obj[0xE4 / 4] = i;
        } else {
            uint32_t* obj = (uint32_t*)((int (__thiscall*)(uint32_t))timer_arr[8])(i);
            obj[0x108 / 4] = i;
        }
    }

    /* Get tile rects */
    for (uint32_t i = 0; i < (uint32_t)g_object_count; i++) {
        void* obj = (void*)((int (__thiscall*)(uint32_t))DAT_004a9994)(i);
        TileMap_GetTileAt(g_tilemap, obj);
    }

    /* Build adjacency (same logic as LoadFileEx, but using +0xE8 offset for neighbors) */
    for (uint32_t srcIdx = 0; srcIdx < param_1[0]; srcIdx++) {
        uint32_t* srcObj = (uint32_t*)((int (__thiscall*)(uint32_t))timer_arr[8])(srcIdx);
        int32_t* neighbor_ptrs = (int32_t*)((uint8_t*)srcObj + 0xE8); /* different offset */

        uint32_t* srcEntry = (uint32_t*)((uint32_t*)param_1[2])[srcIdx];
        srcEntry[1] = (uint32_t)srcObj;

        int32_t* dir_slots = (int32_t*)(srcEntry + 2);
        int32_t* ref_slots = (int32_t*)(srcEntry + 6);

        for (int dir = 0; dir < 4; dir++) {
            if (neighbor_ptrs[dir] == 0) {
                ref_slots[dir] = 0;
                dir_slots[dir] = 0;
            } else {
                uint32_t neighborIdx = *(uint32_t*)((uint8_t*)neighbor_ptrs[dir] + 0x108);
                if (neighborIdx < param_1[0]) {
                    ref_slots[dir] = ((uint32_t*)param_1[2])[neighborIdx];
                    if (dir_slots[dir] == 0) {
                        int32_t* edge = (int32_t*)operator_new(0x10);
                        dir_slots[dir] = (int32_t)edge;
                        edge[1] = 0;
                        edge[0] = neighbor_ptrs[4];
                        edge[2] = srcIdx;
                        edge[3] = neighborIdx;

                        int32_t oppDir = OppositeDirection(dir);
                        ((uint32_t*)((uint32_t*)param_1[2])[neighborIdx])[2 + oppDir] = (uint32_t)edge;
                    }
                }
            }
        }
    }

    /* Free timer objects */
    while (entry_count != 0) {
        ((void (__thiscall*)(uint32_t))timer_arr[3])(entry_count - 1);
    }

    /* Free old pair buffer, allocate new */
    if ((void*)param_1[1] != NULL) {
        CRT_free((void*)param_1[1]);
        param_1[1] = 0;
    }

    uint32_t n = param_1[0];
    if (n != 0) {
        uint32_t matrix_size = (n - 1) * n / 2;
        uint8_t* matrix = (uint8_t*)CRT_malloc_zero(matrix_size);
        param_1[1] = (uint32_t)matrix;
        for (uint32_t i = 0; i < matrix_size; i++) {
            matrix[i] = 0x80;
        }
    }
}

/* ================================================================== */
/* AssetMgr_EnumerateCategory — Search all entries in a category       */
/* Address: 0x45D560                                                   */
/*                                                                     */
/* Allocates 4 temporary arrays then calls AssetMgr_FindFile in a      */
/* loop for each entry. Frees all arrays on return.                    */
/*                                                                     */
/* Called by: World_EnumeratePostLoadAssets (twice)                    */
/* ================================================================== */
void __fastcall AssetMgr_EnumerateCategory(uint32_t* param_1)
{
    uint32_t count = param_1[0];

    param_1[4] = CRT_malloc_zero(count);         /* temp byte array */
    param_1[8] = CRT_malloc_zero(count * 4);     /* temp uint32 array */
    param_1[9] = CRT_malloc_zero(count * 4);     /* temp uint32 array */
    param_1[10] = CRT_malloc_zero(count * 4);    /* temp uint32 array */

    for (uint32_t i = 0; i < count; i++) {
        AssetMgr_FindFile((AssetMgr*)param_1, i);
    }

    CRT_free((void*)param_1[8]);
    CRT_free((void*)param_1[9]);
    CRT_free((void*)param_1[10]);
    CRT_free((void*)param_1[4]);
}

/* ================================================================== */
/* AssetMgr_FindFile — Search file placement in 4-ary tree             */
/* Address: 0x45D5F0                                                   */
/*                                                                     */
/* Clears result arrays, calls SearchFile + GetFileInfo for each       */
/* entry, then frees the temporary tree node.                          */
/* ================================================================== */
void __thiscall AssetMgr_FindFile(AssetMgr* self, uint32_t entry_index)
{
    /* Clear result arrays for all entries */
    uint32_t count = *(uint32_t*)self;
    for (uint32_t i = 0; i < count; i++) {
        ((uint32_t*)(self->result_array))[i] = 0xFFFFFFFF;
    }

    /* Clear tree entry array (+0x24) */
    uint32_t* tree_entry = self->tree_entry;
    uint32_t n = *(uint32_t*)self;
    for (uint32_t i = 0; i < n; i++) {
        tree_entry[i] = 0;
    }

    /* Clear heap entry array (+0x28) */
    uint32_t* heap_entry = self->heap_entry;
    for (uint32_t i = 0; i < n; i++) {
        heap_entry[i] = 0;
    }

    /* Clear result array (+0x10) */
    uint8_t* active = self->active_flag;
    for (uint32_t i = 0; i < n; i++) {
        active[i] = 0;
    }

    /* Search for optimal path */
    uint32_t* result = AssetMgr_SearchFile(self, entry_index, 0, 0, 0);

    /* Get file info for each entry */
    for (uint32_t i = 0; i < count; i++) {
        AssetMgr_GetFileInfo(self, result, entry_index, i);
    }

    /* Free temporary tree node (free 4 subtree references + node) */
    if (result != NULL) {
        for (int i = 0; i < 4; i++) {
            if ((void*)result[i + 6] != NULL) {
                AssetMgr_TreeFreeNode((void*)result[i + 6]);
            }
        }
        GLOBAL_free(result);
    }
}

/* ================================================================== */
/* AssetMgr_SearchFile — Recursive 4-ary tree search with best-path   */
/* Address: 0x45D6C0                                                   */
/*                                                                     */
/* Recursively walks the 4-ary tree to find the shortest path to      */
/* target node. Allocates and returns a 0x2C-byte path node.          */
/* Only runs when g_game_mode == 3.                                    */
/* ================================================================== */
uint32_t* __thiscall AssetMgr_SearchFile(AssetMgr* self, uint32_t currentId,
                                          int32_t depth, uint32_t distance,
                                          int32_t parentNode)
{
    if (g_game_mode != 3 || currentId > *(uint32_t*)self) {
        return NULL;
    }

    /* Mark self node as active */
    *(uint8_t*)(self->active_flag + currentId) = 1;

    /* Check if self distance is better than the stored best */
    uint32_t* best_dist = (uint32_t*)(self->result_array + currentId * 4);
    if (distance < *best_dist) {
        *best_dist = distance;

        /* Allocate path node (0x2C bytes, 11 dwords) */
        uint32_t* path_node = (uint32_t*)operator_new(0x2C);
        uint32_t* src_entry = *(uint32_t**)(self->resource_array + currentId * 4);

        /* Copy source entry data into new node */
        for (int i = 0; i < 11; i++) {
            path_node[i] = src_entry[i];
        }

        /* Replace in tree entry lookup */
        uint32_t** tree_entries = &self->tree_entry;
        if (tree_entries[currentId] != NULL) {
            AssetMgr_RemoveNode(self, (int32_t*)tree_entries[currentId]);
        }
        tree_entries[currentId] = path_node;

        /* Store parent/depth info */
        *(int*)(self->heap_entry + currentId * 4) = parentNode;

        /* Save original child refs, clear ours */
        int32_t saved_refs[4];
        int32_t* child_slots = (int32_t*)(path_node + 2);
        for (int i = 0; i < 4; i++) {
            saved_refs[i] = child_slots[i];
            if (child_slots[i] != 0) {
                uint32_t child_entry = *(uint32_t*)(child_slots[i] + 4);
                saved_refs[i] = child_entry;
                if (child_entry != 0) {
                    child_slots[4 + i] = 0;  /* clear ref_slot */
                }
            }
        }

        /* Recursively search children */
        for (int i = 0; i < 4; i++) {
            if (child_slots[i] != 0) {
                int32_t* edge = (int32_t*)child_slots[i];
                edge[1] = 1;  /* mark as visited */
                int32_t* edge_data = (int32_t*)edge[0];
                uint32_t childId = edge_data[2];
                if (childId == currentId) {
                    childId = edge_data[3];
                }
                uint32_t* child_result = AssetMgr_SearchFile(
                    self, childId, depth + 1, distance + edge_data[0], (int32_t)path_node);
                child_slots[4 + i] = (int32_t)child_result;
            }
        }

        /* Restore saved refs */
        for (int i = 0; i < 4; i++) {
            if (child_slots[i] != 0) {
                int32_t* edge = (int32_t*)child_slots[i];
                edge[1] = saved_refs[i];
            }
        }

        return path_node;
    }

    return NULL;
}

/* ================================================================== */
/* AssetMgr_TreeFreeNode — Recursively free a 4-ary tree node         */
/* Address: 0x45D810                                                   */
/*                                                                     */
/* Does NOT update lookup tables — raw subtree deletion.              */
/* ================================================================== */
void AssetMgr_TreeFreeNode(void* node)
{
    if (node == NULL) return;

    uint32_t* children = (uint32_t*)((uint8_t*)node + 0x18);
    for (int i = 0; i < 4; i++) {
        if ((void*)children[i] != NULL) {
            AssetMgr_TreeFreeNode((void*)children[i]);
        }
    }
    GLOBAL_free(node);
}

/* ================================================================== */
/* AssetMgr_RemoveNode — Remove node from tree and free              */
/* Address: 0x45D850                                                   */
/*                                                                     */
/* Removes from lookup table at +0x24, unlinks from parent at +0x28,  */
/* then recursively removes children and frees.                        */
/* ================================================================== */
void __thiscall AssetMgr_RemoveNode(AssetMgr* self, int32_t* node)
{
    if (node == NULL) return;

    /* Remove from tree entry lookup at +0x24 */
    *(uint32_t*)(self->tree_entry + node[0] * 4) = 0;

    /* Unlink from parent at +0x28 */
    int32_t parentPtr = *(int*)(self->heap_entry + node[0] * 4);
    uint32_t* parentSlots = (uint32_t*)(parentPtr + 0x18);
    for (uint32_t offset = 0x18; offset < 0x28; offset += 4) {
        if (*(int**)(parentPtr + offset) == node) {
            *(uint32_t*)(parentPtr + offset) = 0;
        }
    }

    /* Recursively remove children */
    int32_t* child_ptr = node + 6;
    for (int i = 0; i < 4; i++) {
        if ((int32_t*)child_ptr[i] != NULL) {
            AssetMgr_RemoveNode(self, (int32_t*)child_ptr[i]);
        }
    }

    GLOBAL_free(node);
}

/* ================================================================== */
/* AssetMgr_ReadFile — MISNAMED: free all tree nodes and clear        */
/* Address: 0x45D8C0                                                   */
/*                                                                     */
/* Walks each entry's 4 child nodes, removes cross-references, frees  */
/* child nodes, frees entry nodes, clears arrays.                      */
/* ================================================================== */
void __fastcall AssetMgr_ReadFile(uint32_t* param_1)
{
    if (param_1[2] == 0) return;

    /* For each entry, remove cross-references in children, then free */
    for (uint32_t i = 0; i < param_1[0]; i++) {
        uint32_t* entry = *(uint32_t**)(param_1[2] + i * 4);
        /* Scan children at offsets 8, 0xC, 0x10, 0x14 */
        for (uint32_t childOff = 8; childOff < 0x18; childOff += 4) {
            void* child = *(void**)((uint8_t*)entry + childOff);
            if (child != NULL) {
                uint32_t childId = *(uint32_t*)((uint8_t*)child + 8);
                if (childId == i) {
                    childId = *(uint32_t*)((uint8_t*)child + 0xC);
                }
                /* Remove back-reference from child's entry */
                uint32_t* childEntry = *(uint32_t**)(param_1[2] + childId * 4);
                for (uint32_t backOff = 8; backOff < 0x18; backOff += 4) {
                    if (*(void**)((uint8_t*)childEntry + backOff) == child) {
                        *(uint32_t*)((uint8_t*)childEntry + backOff) = 0;
                    }
                }
                GLOBAL_free(child);
            }
        }
    }

    /* Free entry nodes themselves */
    for (uint32_t i = 0; i < param_1[0]; i++) {
        GLOBAL_free(*(void**)(param_1[2] + i * 4));
        *(uint32_t*)(param_1[2] + i * 4) = 0;
    }

    /* Free entry array and reset */
    CRT_free((void*)param_1[2]);
    param_1[0] = 0;
    param_1[2] = 0;
}

/* ================================================================== */
/* AssetMgr_WriteFile — Recursive shortest path search                 */
/* Address: 0x45D980                                                   */
/*                                                                     */
/* Recursively searches for shortest path between two IDs in the       */
/* 4-ary tree. Returns path distance or -1 if no path found.           */
/* Stores direction in direction_byte field.                           */
/* ================================================================== */
int32_t AssetMgr_WriteFile(int32_t* param_1, int32_t param_2, int32_t param_3,
                            uint32_t param_4, uint32_t param_5)
{
    if (param_1 == NULL) return -1;
    if (param_1[0] == param_3) return 0;  /* reached target */
    if (param_4 <= param_5) return -1;    /* exceeded max distance */

    int32_t best_dist = -1;
    uint8_t best_dir = 0xFF;

    int32_t* child_refs = param_1 + 2;
    for (uint8_t dir = 0; dir < 4; dir++) {
        if (child_refs[dir] != 0) {
            int32_t result = AssetMgr_WriteFile(
                (int32_t*)child_refs[4 + dir],
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
        *(uint8_t*)(param_1 + 10) = best_dir;  /* direction_byte */
    }
    return best_dist;
}

/* ================================================================== */
/* AssetMgr_TraverseTo — Walk tree from start node to target           */
/* Address: 0x45DA40                                                   */
/*                                                                     */
/* Walks the 4-ary tree from startNode to targetId, counting steps    */
/* at +0x14. Stores each node's direction byte.                        */
/* ================================================================== */
void __thiscall AssetMgr_TraverseTo(AssetMgr* self, int32_t* startNode, int32_t targetId)
{
    int32_t steps = self->traversal_step + 1;
    self->traversal_step = steps;

    int32_t nodeId = startNode[0];
    while (nodeId != targetId) {
        steps++;
        startNode = (int32_t*)startNode[6 + *(uint8_t*)(startNode + 10)];
        self->traversal_step = steps;
        nodeId = startNode[0];
    }
}

/* ================================================================== */
/* AssetMgr_DeleteFile — Write adjacency pair values for all nodes    */
/* Address: 0x45DAD0                                                   */
/*                                                                     */
/* For each pair of entries in the path, reads the existing pair value */
/* and writes the reverse direction if unset (value 0x80).            */
/* ================================================================== */
void __fastcall AssetMgr_DeleteFile(void* param_1)
{
    uint32_t count = *(uint32_t*)((uint8_t*)param_1 + 0x14);
    if (count <= 1) return;

    uint32_t* id_buf = *(uint32_t**)((uint8_t*)param_1 + 0x1C);
    uint8_t* dir_buf = *(uint8_t**)((uint8_t*)param_1 + 0x18);

    for (uint32_t i = 0; i < count - 1; i++) {
        uint32_t idA = id_buf[i];
        for (uint32_t j = i + 1; j < count; j++) {
            uint32_t idB = id_buf[j];

            uint8_t pair_val = AssetMgr_ReadPairValue(param_1, idA, idB);
            if (pair_val == 0x80) {
                AssetMgr_WritePairValue(param_1, idA, idB, dir_buf[i]);
                int32_t opp_dir = OppositeDirection(dir_buf[j - 1]);
                AssetMgr_WritePairValue(param_1, idB, idA, (uint8_t)opp_dir);
            }
        }
    }
}

/* ================================================================== */
/* AssetMgr_GetFileInfo — Compute shortest path between two nodes     */
/* Address: 0x45DBC0                                                   */
/*                                                                     */
/* Calls WriteFile for shortest path, then TraverseTo to build        */
/* direction path, then DeleteFile to write adjacency pairs.           */
/* Returns direction byte or 0xFF on failure.                          */
/* ================================================================== */
uint8_t __thiscall AssetMgr_GetFileInfo(AssetMgr* self, uint32_t* treeNode,
                                         uint32_t fromId, uint32_t toId)
{
    uint8_t pair_val = AssetMgr_ReadPairValue(self, fromId, toId);
    if (pair_val != 0x80) {
        return 0xFF;  /* already has a value */
    }

    /* Check if path is valid */
    if (*(uint8_t*)(self->active_flag + toId) != 0 && fromId != toId) {
        uint32_t best_dist = 0xFFFFFFFF;
        int32_t best_dir_results[4];
        uint32_t* child_slots = treeNode + 6;

        for (int i = 0; i < 4; i++) {
            best_dir_results[i] = AssetMgr_WriteFile(
                (int32_t*)child_slots[i], (int32_t)fromId, (int32_t)toId,
                best_dist, 0);

            if (best_dir_results[i] != -1 &&
                (best_dist == 0xFFFFFFFF || best_dir_results[i] < (int32_t)best_dist)) {
                best_dist = best_dir_results[i];
            }
        }

        /* Find best direction from children */
        uint8_t best_dir = 0;
        uint32_t min_dist = best_dir_results[0];
        for (int i = 1; i < 4; i++) {
            if (best_dir_results[i] != -1 &&
                (min_dist == 0xFFFFFFFF || best_dir_results[i] < (int32_t)min_dist)) {
                min_dist = best_dir_results[i];
                best_dir = (uint8_t)i;
            }
        }

        if (min_dist != 0xFFFFFFFF) {
            /* Store direction byte and traverse */
            *(uint8_t*)(treeNode + 10) = best_dir;
            self->traversal_step = 1;

            if (treeNode[0] != toId) {
                AssetMgr_TraverseTo(self,
                    (int32_t*)treeNode[6 + *(uint8_t*)(treeNode + 10)], toId);
            }

            /* Allocate path buffers */
            uint32_t* path_buf = (uint32_t*)CRT_malloc_zero(self->traversal_step);
            self->path_id_buf = (uint32_t)path_buf;

            uint32_t* dir_buf = (uint32_t*)CRT_malloc_zero(self->traversal_step * 4);
            self->path_dir_buf = (uint32_t)dir_buf;

            self->traversal_step = 0;

            /* Store first entry */
            dir_buf[0] = treeNode[0];
            *(uint8_t*)(self->path_id_buf + self->traversal_step) =
                (uint8_t)treeNode[10];
            self->traversal_step = self->traversal_step + 1;

            if (treeNode[0] != toId) {
                DirectPlay_SessionMgr(self,
                    (int32_t*)treeNode[6 + *(uint8_t*)(treeNode + 10)], toId);
            }

            /* Write adjacency pairs and clean up */
            AssetMgr_DeleteFile(self);

            CRT_free((void*)self->path_id_buf);
            CRT_free((void*)self->path_dir_buf);
        }

        if (min_dist == 0xFFFFFFFF) {
            AssetMgr_WritePairValue(self, fromId, toId, 0xFF);
            return 0xFF;
        }

        return best_dir;
    }

    AssetMgr_WritePairValue(self, fromId, toId, 0xFF);
    return 0xFF;
}

/* ================================================================== */
/* DirectPlay_SessionMgr — Walk tree storing IDs and directions       */
/* Address: 0x45DA70                                                   */
/*                                                                     */
/* Walks the 4-ary tree from startNode to targetId, storing each      */
/* node's ID in the path buffer at +0x1C and direction byte in the    */
/* direction buffer at +0x18.                                          */
/* ================================================================== */
void __thiscall DirectPlay_SessionMgr(AssetMgr* self, int32_t* startNode, int32_t targetId)
{
    uint32_t* dir_buf = self->path_dir_buf;
    uint8_t* path_buf = (uint8_t*)self->path_id_buf;
    int32_t* step_ptr = &self->traversal_step;

    dir_buf[*step_ptr] = startNode[0];
    path_buf[*step_ptr] = (uint8_t)startNode[10];
    (*step_ptr)++;

    int32_t nodeId = startNode[0];
    while (nodeId != targetId) {
        startNode = (int32_t*)startNode[6 + startNode[10]];
        dir_buf[*step_ptr] = startNode[0];
        path_buf[*step_ptr] = (uint8_t)startNode[10];
        (*step_ptr)++;
        nodeId = startNode[0];
    }
}

/* ================================================================== */
/* AssetMgr_ReadPairValue — Read 2-bit pair value from matrix         */
/* Address: 0x45DD80                                                   */
/*                                                                     */
/* Reads a 2-bit direction value from a packed upper-triangular       */
/* matrix. Returns 0-3 direction, 0x80 for valid unset, 0xFF invalid. */
/* ================================================================== */
uint8_t __thiscall AssetMgr_ReadPairValue(AssetMgr* self, uint32_t a, uint32_t b)
{
    uint32_t count = *(uint32_t*)self;

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
    uint8_t* matrix = (uint8_t*)self->search_cursor;
    uint8_t val = matrix[offset];

    if (val == 0xFF) return 0xFF;
    if (val == 0x80) return 0x80;

    if (a < b) {
        val >>= 2;  /* upper pair */
    }
    return val & 3;
}

/* ================================================================== */
/* AssetMgr_WritePairValue — Write 2-bit pair value to matrix         */
/* Address: 0x45DDE0                                                   */
/*                                                                     */
/* Writes a 2-bit direction value to the packed upper-triangular      */
/* matrix. Value 0xFF clears the entry.                                */
/* ================================================================== */
void __thiscall AssetMgr_WritePairValue(AssetMgr* self, uint32_t a, uint32_t b, uint8_t value)
{
    if (a == b) return;

    uint32_t i = a, j = b;
    if (a < b) {
        i = b;
        j = a;
    }

    uint32_t offset = (i - 1) * i / 2 + j;
    uint8_t* matrix = (uint8_t*)self->search_cursor;
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
