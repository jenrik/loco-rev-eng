/**
 * Entity.cpp — Entity base class implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

#include "Entity.h"
#include <cstring>

Entity::Entity() : Entity(0, 0, 0, 0) {}

/* External references */
extern char g_empty_string;     /* empty string constant in .rdata */


/* ================================================================== */
/* Entity::Entity — Base constructor                                   */
/* Address: 0x405790                                                   */
/* ================================================================== */
Entity::Entity(int resource_id, int16_t anim_idx, int world_x, int world_y)
{
    /* GameObject::GameObject() (0x4369D0) is invoked automatically as
     * the C++ base constructor before this body. */

    /* ---- Step 2: Store raw world position ----
     * These are the raw coordinates before resource offset is applied.   */
    this->world_x_raw = world_x;     /* +0x74 */
    this->world_y_raw = world_y;     /* +0x78 */

    /* In the binary: overrides vtable to 0x477488 (Entity vtable).
     * In natural C++, the compiler sets the vtable automatically. */

    /* ---- Step 4: Set type = 2 (Entity) ---- */
    this->type = 2;                  /* +0x04 */

    /* ---- Step 5: Zero-initialize Entity-specific fields ---- */
    this->resource        = nullptr; /* +0x40 */
    this->sound_res_id    = 0;       /* +0x44 */
    this->audio_channel   = nullptr; /* +0x48 */
    this->anim_index      = 0;       /* +0x28 (inherited, re-zeroed) */
    this->frame_index     = 0;       /* +0x54 */
    this->active_state    = 0;       /* +0x5C */
    this->next_sound_time = 0;       /* +0x60 */
    this->phase_timer     = 0;       /* +0x6C */
    this->timer           = 0;       /* +0x58 */
    this->visible         = 1;       /* +0x24 (inherited, set to visible) */

    /* ---- Step 6: Copy empty string to name buffer ----
     * The name field at +0x7C is 11 bytes, initially set to "".        */
    {
        const char* src = &g_empty_string;
        char* dst = this->name;
        size_t len = 0;

        /* Measure source string length */
        while (*src != '\0') {
            src++;
            len++;
        }
        len++;  /* include null terminator */

        /* 4-byte aligned copy */
        src = &g_empty_string;
        const size_t words = len >> 2;
        if (words != 0) {
            std::memcpy(dst, src, words * sizeof(uint32_t));
            src += words * sizeof(uint32_t);
            dst += words * sizeof(uint32_t);
        }

        /* Remaining bytes */
        const size_t remainder = len & 3;
        for (size_t i = 0; i < remainder; i++) {
            *dst++ = *src++;
        }
    }

    /* ---- Step 7: Load resource if ID is positive ----
     * Calls InitBase (vtable[6], 0x405900)                              */
    if (resource_id > 0) {
        this->InitBase(resource_id, anim_idx, false);
    }

    /* ---- Step 8: Clear waiting flag ---- */
    this->waiting_flag = 0;          /* +0x70 */
}


/* ================================================================== */
/* Entity::GetSubObjectPosition — Get sub-object screen position       */
/* Address: 0x458350                                                   */
/*                                                                     */
/* Called by:                                                          */
/*   TileMap_GetViewport (0x457A1B, 0x457B0E)                         */
/*                                                                     */
/* Retrieves the screen-space position of a sub-object. The resource   */
/* at +0x40 contains an offset table starting at +0x5FC, with 8 bytes  */
/* per entry (int32_t offset_x, int32_t offset_y).                     */
/*                                                                     */
/* The final position is:                                              */
/*   out_x = resource_offset_table[sub_index].x + screen_rect.left     */
/*   out_y = resource_offset_table[sub_index].y + screen_rect.top      */
/*                                                                     */
/* If resource is NULL or both offsets are -1 (marking "no sub-object" */
/* at that index), returns {-1, -1}.                                   */
/* ================================================================== */
void Entity::GetSubObjectPosition(int* out_xy, int sub_index)
{
    void* resource = this->resource;

    /* If no resource, return {-1, -1} */
    if (resource == nullptr) {
        out_xy[0] = -1;
        out_xy[1] = -1;
        return;
    }

    /* Read offset pair from resource's sub-object table */
    int* offset_table = reinterpret_cast<int*>(
        static_cast<uint8_t*>(resource) + 0x5FC);
    int offset_x = offset_table[sub_index * 2];       /* +0x5FC + sub_index*8 */
    int offset_y = offset_table[sub_index * 2 + 1];   /* +0x600 + sub_index*8 */

    /* Check if this sub-object slot is unused (both -1) */
    if (offset_x == -1 && offset_y == -1) {
        out_xy[0] = -1;
        out_xy[1] = -1;
        return;
    }

    /* Compute absolute screen position */
    out_xy[0] = this->screen_rect.left + offset_x;
    out_xy[1] = this->screen_rect.top  + offset_y;
}
