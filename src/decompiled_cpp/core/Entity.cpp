/**
 * Entity.cpp — Entity base class implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

#include "Entity.h"
#include "../shared/vtable_addrs.h"

/* External references */
extern char g_empty_string;     /* empty string constant in .rdata */


/* ================================================================== */
/* Entity::Entity — Base constructor                                   */
/* Address: 0x405790                                                   */
/*                                                                     */
/* Called by:                                                          */
/*   CGWND_VehicleEditor_Ctor (0x40D52A)                               */
/*   UI_Construct (0x422EEE)                                           */
/*   UI_CreateTooltip (0x423C97)                                       */
/*   RESDATA_GameObject_Ctor (0x4580C9)                                */
/*   DirectPlay_Init (0x45E121)                                        */
/*   Game_Ctor (0x410538)                                              */
/*   UIPANEL_InitScrollPanel (0x4273A6)                                */
/*   RESDATA_BaseInit (0x454507)                                       */
/*   Town_ScrollView (0x42CD18)                                        */
/*   Building_BaseCtor (0x433A4D)                                      */
/*   RESDATA_ScriptedObject_Ctor (0x449468)                            */
/*   DDRAW_InitSprites (0x4589E5, multiple calls)                      */
/*                                                                     */
/* Full Entity/GameObject base constructor. Calls GameObject() to      */
/* initialize core fields, then sets Entity vtable and initializes     */
/* all Entity-specific fields to defaults. Optionally loads a resource */
/* via InitBase if resource_id > 0. Uses SEH for exception safety.     */
/* ================================================================== */
Entity::Entity(int resource_id, int16_t anim_idx, int world_x, int world_y)
{
    /* ---- Step 1: Construct base GameObject ----
     * GameObject::GameObject() (0x4369D0) sets:
     *   vtable = 0x477820 (GameObject root)
     *   type   = 1
     *   screen_rect = {0,0,0,0}
     *   initialized  = 1
     *   _pad_1C = _pad_20 = 0                                */
    GameObject::GameObject();

    /* ---- Step 2: Store raw world position ----
     * These are the raw coordinates before resource offset is applied.
     * SetWorldPos will later add the resource's offset_x/offset_y.     */
    this->world_x_raw = world_x;     /* +0x74 */
    this->world_y_raw = world_y;     /* +0x78 */

    /* ---- Step 3: Override vtable to Entity ----
     * 0x477488 — Entity vtable. Derived class constructors will
     * override this again with their own vtables.                      */
    this->vtable = (void**)VTBL_ENTITY;     /* +0x00 */

    /* ---- Step 4: Set type = 2 (Entity) ---- */
    this->type = 2;                  /* +0x04 */

    /* ---- Step 5: Zero-initialize Entity-specific fields ---- */
    this->parent          = nullptr; /* +0x40 */
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
     * The name field at +0x7C is 11 bytes, initially set to "".
     * A 32-bit copy loop is used for speed, then byte remainder.       */
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
        size_t words = len >> 2;
        for (size_t i = 0; i < words; i++) {
            *(uint32_t*)dst = *(const uint32_t*)src;
            src += 4;
            dst += 4;
        }

        /* Remaining bytes */
        size_t remainder = len & 3;
        for (size_t i = 0; i < remainder; i++) {
            *dst++ = *src++;
        }
    }

    /* ---- Step 7: Load resource if ID is positive ----
     * Calls InitBase (vtable[6], 0x405900) which:
     *   - Loads the resource from ResourceManager
     *   - Sets up bounding rects from frame dimensions
     *   - Initializes animation state via SetAnimState (vtable[7])     */
    if (resource_id > 0) {
        this->InitBase(resource_id, anim_idx, false);
    }

    /* ---- Step 8: Clear waiting flag ---- */
    this->waiting_flag = 0;          /* +0x70 */
}
