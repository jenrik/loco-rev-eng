/**
 * ddraw_building_sprites.c — DDRAW building sprite management methods
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * C++ methods on the DDRAW building sprite manager (vtable 0x478548,
 * type=0x0D, size ~0x5C0 bytes). The DDRAW_Sprite struct manages
 * click handling, tooltip display, day/night animation switching,
 * sprite init/cleanup, invalidation, and viewport clamping.
 *
 * Key field layout:
 *   +0x00: vtable (0x478548)
 *   +0x04: type (0x0D = building sprite type)
 *   +0x08: x position
 *   +0x0C: y position
 *   +0x10: RESDATA* (resource data pointer)
 *   +0x38: GameObject[0] sub-object
 *   +0xE8: GameObject[1] sub-object
 *   +0x10A: GameObject[2] sub-object
 *   +0x12C: GameObject[3] sub-object
 *   +0x5A/+0x7C/+0x9E/+0xC0: pattern sub-objects (0x22 byte stride)
 *   +0x530: day animation ID
 *   +0x534: day_time_start
 *   +0x548: day_time_end
 *   +0x1E: night animation ID (within RESDATA+0x1E)
 *   Various state fields at +0x150..+0x16B
 */

#include <stdint.h>

/* ================================================================== */
/* External functions and globals                                      */
/* ================================================================== */

/* Globals */
extern int32_t  g_viewport_x;           /* 0x485200 */
extern int32_t  g_viewport_y;           /* 0x485204 */
extern int32_t  g_client_offset_x;      /* 0x485228 */
extern int32_t  g_client_offset_y;      /* 0x48522C */
extern uint8_t  g_is_town_mode;         /* game mode flags */
extern uint8_t  g_ddraw_active;         /* DDRAW active state flag */
extern int32_t  g_game_difficulty;      /* 0x4A98D8 + difficulty offset */
extern int64_t  g_game_time;            /* 0x4A9930 — game time */
extern void*    g_tooltip_mgr;          /* 0x4FD220 — tooltip manager */

/* Town view / overlay rects for viewport clamping */
extern int32_t  g_town_view_rect[4];    /* Viewport rect in client space */
extern int32_t  g_town_overlay_rect[4]; /* Overlay rect in client space */

/* External helper functions */
extern void  __cdecl PlaySound(uint32_t sound_id);             /* 0x447930 */
extern void  __cdecl DDRAW_SelectBuilding(void* mgr,           /* 0x459180 */
                                            void* sprite);
extern void  __cdecl UI_CreateMessageBox(void* mgr,            /* 0x423AB0 */
                                          uint32_t res_id,
                                          int16_t sub_id,
                                          char direction,
                                          int32_t x, int32_t y,
                                          uint8_t unk);
extern void  __cdecl RESDATA_BaseInit(void* ptr);              /* 0x454380 */
extern void  __cdecl GameObject_BaseCtor(void* obj,            /* 0x405790 */
                                           int32_t a, int32_t b,
                                           int32_t c, int32_t d);
extern void  __cdecl RESDATA_DtorBase(void* ptr);              /* 0x454630 */
extern void  __cdecl CRT_memset_pattern(void* dst, int32_t val,
                                          int32_t size,
                                          void* pattern);     /* 0x4671E0 */
extern void* __cdecl CRT_localtime(int64_t* time);            /* 0x4674E0 */
extern int32_t __cdecl Game_CheckTimeInRange(int32_t* tm,      /* 0x412710 */
                                               int32_t* start, int32_t* end);
extern int32_t __stdcall IntersectRect(int32_t* dst,           /* Win32 */
                                        int32_t* src1, int32_t* src2);

/* ================================================================== */
/* Constants                                                           */
/* ================================================================== */

/* Vtable for DDRAW building sprites */
#define VTBL_DDRAW_BUILDING_SPRITE  0x00478548

/* ================================================================== */
/* DDRAW_BuildingClickHandler — Handle click on a building sprite      */
/* Address: 0x458820                                                   */
/* Size: 132 bytes                                                     */
/* Calling convention: __thiscall (_this in ECX, 1 stack param)        */
/*                                                                     */
/* Reads the action_id from element_desc+0x0C (-1 = no action):       */
/*   action 0: calls vtable[7] (0x1C/4) with sub_index (+0x10)       */
/*   action >0: calls vtable[6] (0x18/4) with action_id + sub_index  */
/* Then plays sound 0x571E and selects the building sprite             */
/* (unless in town mode or difficulty restricts it).                   */
/*                                                                     */
/* @param element_desc  Pointer to UI element descriptor (+0x0C=action)*/
/* ================================================================== */
void __thiscall DDRAW_BuildingClickHandler(void* _this, void* element_desc)
{
    void** vtable = *(void***)_this;
    int32_t action_id = *(int32_t*)((uint8_t*)element_desc + 0x0C);

    if (action_id == -1) return;

    if (action_id == 0) {
        /* vtable[7] — single-arg action */
        ((void (*)(int16_t))vtable[7])(*(int16_t*)((uint8_t*)element_desc + 0x10));
    } else {
        /* vtable[6] — three-arg action */
        ((void (*)(int32_t, int16_t, int32_t))vtable[6])(
            action_id,
            *(int16_t*)((uint8_t*)element_desc + 0x10),
            0);
    }

    /* Play selection sound and select (unless town mode/restricted) */
    if (g_is_town_mode == 0 &&
        (g_ddraw_active != 1 || g_game_difficulty != 3)) {
        PlaySound(0x571E);
        DDRAW_SelectBuilding((void*)0x4854D8, _this);  /* g_ddraw_building */
    }
}

/* ================================================================== */
/* DDRAW_BuildingShowTooltip — Show tooltip for a building sprite       */
/* Address: 0x4588B0                                                   */
/* Size: 143 bytes                                                     */
/* Calling convention: __thiscall (_this in ECX, 1 stack param)        */
/*                                                                     */
/* Reads the element descriptor fields:                                */
/*   +0x20: resource ID for tooltip text                               */
/*   +0x24: sub-resource ID (int16)                                    */
/*   +0x28: position type character ('S'=screen, 'W'=world, 'U'=up,   */
/*           'D'=down, others=sprite-relative)                         */
/*   +0x2C: X position                                                 */
/*   +0x30: Y position                                                 */
/*                                                                     */
/* 'S' type: positions relative to viewport                            */
/* 'W' type: absolute world coordinates                                */
/* Others: positions relative to sprite's own position (_this+0x08/C)  */
/*                                                                     */
/* @param element_desc  Tooltip element descriptor                      */
/* ================================================================== */
void __thiscall DDRAW_BuildingShowTooltip(void* _this, void* element_desc)
{
    uint8_t* desc = (uint8_t*)element_desc;
    int32_t res_id = *(int32_t*)(desc + 0x20);
    int16_t sub_id = *(int16_t*)(desc + 0x24);
    uint8_t pos_type = *(uint8_t*)(desc + 0x28);
    int32_t x, y;

    if (res_id <= 0) return;

    /* Calculate screen position */
    if (pos_type == 0x53) {  /* 'S' — screen/viewport-relative */
        x = *(int32_t*)(desc + 0x2C) + g_viewport_x;
        y = *(int32_t*)(desc + 0x30) + g_viewport_y;
    } else if (pos_type == 0x57) {  /* 'W' — world coordinates */
        x = *(int32_t*)(desc + 0x2C);
        y = *(int32_t*)(desc + 0x30);
    } else {
        /* Sprite-relative: add _this->x (+0x08) / _this->y (+0x0C) */
        x = *(int32_t*)(desc + 0x2C) + *(int32_t*)((uint8_t*)_this + 8);
        y = *(int32_t*)(desc + 0x30) + *(int32_t*)((uint8_t*)_this + 0x0C);
    }

    if (pos_type == 0x55 || pos_type == 0x44) {  /* 'U' or 'D' */
        UI_CreateMessageBox(g_tooltip_mgr, res_id, sub_id,
                            pos_type, x, y, 1);
    } else {
        UI_CreateMessageBox(g_tooltip_mgr, res_id, sub_id,
                            'W', x, y, 1);
    }
}

/* ================================================================== */
/* DDRAW_BuildingTimeUpdate — Day/night animation switch               */
/* Address: 0x458940                                                   */
/* Size: 107 bytes                                                     */
/* Calling convention: __fastcall (param_1 in ECX = DDRAW_Sprite*)    */
/*                                                                     */
/* Checks if the DDRAW_Sprite's type 6 flag (0x18) is set. If so,     */
/* reads the RESDATA pointer at param_1+0x40, then checks:            */
/*   resdata+0x530: day animation ID (int16, -1=disabled)             */
/*   resdata+0x534: day_time_start                                    */
/*   resdata+0x548: day_time_end                                      */
/*   resdata+0x1E: night animation ID (int16)                         */
/* Uses Game_CheckTimeInRange to see if game time falls within the     */
/* day period. If in day range and night anim is playing, switch to    */
/* day anim. If outside day range and day anim is playing, switch to   */
/* night anim.                                                         */
/*                                                                     */
/* @param sprite  DDRAW_Sprite* with RESDATA at +0x40                 */
/* ================================================================== */
void __fastcall DDRAW_BuildingTimeUpdate(void* sprite)
{
    uint8_t* p = (uint8_t*)sprite;

    if (*(uint8_t*)(p + 0x18) != 1) return;  /* type 6 flag not set */

    /* Get current game time */
    int32_t* time_ptr = (int32_t*)CRT_localtime(&g_game_time);

    /* RESDATA pointer at +0x40 (wait — actually at offset 0x40 from root) */
    /* Let me re-check: param_1[0x10] = *(int32_t*)(p + 0x40) */
    uint8_t* resdata = *(uint8_t**)(p + 0x40);
    if (resdata == NULL) return;

    int16_t day_anim_id = *(int16_t*)(resdata + 0x530);
    if (day_anim_id == -1) return;  /* no day animation */

    /* Check time range */
    int32_t* day_start = (int32_t*)(resdata + 0x534);
    int32_t* day_end   = (int32_t*)(resdata + 0x548);
    int32_t in_day_range = Game_CheckTimeInRange(time_ptr, day_start, day_end);

    int16_t night_anim_id = *(int16_t*)(resdata + 0x1E);
    int32_t current_anim = *(int32_t*)(p + 0x28);  /* current animation state at +0x28 */

    if (in_day_range == 0) {
        /* Currently in night time — if day anim is playing, switch to night */
        if (current_anim != (int32_t)day_anim_id) return;
        /* vtable[7] = SetAnimation(night_anim_id) */
        void** vtable = *(void***)sprite;
        ((void (*)(int16_t))vtable[7])(night_anim_id);
    } else {
        /* Currently in day time — if night anim is playing, switch to day */
        if (current_anim == (int32_t)day_anim_id) return;
        /* vtable[7] = SetAnimation(day_anim_id) */
        void** vtable = *(void***)sprite;
        ((void (*)(int16_t))vtable[7])(day_anim_id);
    }
}

/* ================================================================== */
/* DDRAW_InitSprites — Initialise the DDRAW sprite manager             */
/* Address: 0x4589B0                                                   */
/* Size: 274 bytes (SEH-protected)                                     */
/* Calling convention: __fastcall (param_1 in ECX = DDRAW_Sprite*)    */
/*                                                                     */
/* Constructor for the DDRAW sprite manager:                           */
/*   1. Calls RESDATA_BaseInit (base class init)                       */
/*   2. Creates 5 GameObjects at +0x38, +0xE8, +0x10A, +0x12C, +300  */
/*      (element 299 = 0x12B, element 0 = 0x38, etc.)                 */
/*   3. Calls CRT_memset_pattern for 4 pattern sub-objects at +0x5A   */
/*   4. Zeroes all state fields at +0x150..+0x16B                     */
/*   5. Sets vtable = 0x478548 and type = 0x0D                        */
/*                                                                     */
/* @param sprite  DDRAW_Sprite* to initialise (returns it)             */
/* ================================================================== */
void* __fastcall DDRAW_InitSprites(void* sprite)
{
    uint32_t* p = (uint32_t*)sprite;

    /* Step 1: Base class initialisation */
    RESDATA_BaseInit(sprite);

    /* Step 2: Create 5 GameObject sub-objects */
    /*   +0x38 (element 0x38/4 = 14) */
    GameObject_BaseCtor((uint8_t*)sprite + 0x38, -1, -1, 0, 0);

    /*   CRT_memset_pattern for 4 pattern structs at +0x5A, +0x7C, +0x9E, +0xC0 */
    /*   Each is 0x22 bytes (34 dwords? Actually the memset writes 0x88 bytes at +0x5A) */
    /*   0x88 bytes = 136 bytes, covering +0x5A to +0xE2 */
    {
        uint8_t* pattern_base = (uint8_t*)sprite + 0x5A;
        for (int i = 0; i < 4; i++) {
            CRT_memset_pattern(pattern_base + i * 0x22, 0x88, 4, /* pattern data */);
            /* TODO: the actual pattern data address is 0x458AF0 */
        }
    }
    /* Original code uses a single CRT_memset_pattern call followed by 4 GameObjects */

    /* Actually the decompilation showed separate steps: */
    /* (+0xE8) GameObject[1] */
    GameObject_BaseCtor((uint8_t*)sprite + 0xE8, -1, -1, 0, 0);
    /* (+0x10A) GameObject[2] */
    GameObject_BaseCtor((uint8_t*)sprite + 0x10A, -1, -1, 0, 0);
    /* (+0x12C) GameObject[3] — note: +300 = 0x12C */
    GameObject_BaseCtor((uint8_t*)sprite + 300, -1, -1, 0, 0);

    /* Step 3: Zero state fields at +0x150 through +0x16B */
    p[0x150] = 0;  /* +0x540 */
    p[0x15C] = 0;  /* +0x570 */
    p[0x15B] = 0;  /* +0x56C */
    p[0x15A] = 0;  /* +0x568 */
    p[0x159] = 0;  /* +0x564 */
    p[0x160] = 0;  /* +0x580 */
    p[0x15F] = 0;  /* +0x57C */
    p[0x15E] = 0;  /* +0x578 */
    p[0x15D] = 0;  /* +0x574 */
    p[0x16B] = 0;  /* +0x5AC */
    p[0x16A] = 0;  /* +0x5A8 */
    p[0x169] = 0;  /* +0x5A4 */
    p[0x14E] = 0;  /* +0x538 */
    p[0x14F] = 0;  /* +0x53C */

    /* Step 4: Set vtable and type */
    p[0] = VTBL_DDRAW_BUILDING_SPRITE;  /* +0x00 — vtable 0x478548 */
    p[1] = 0x0D;                         /* +0x04 — type */

    return sprite;
}

/* ================================================================== */
/* DDRAW_InvalidateAll — Invalidate all DDRAW sprite sub-objects       */
/* Address: 0x458BB0                                                   */
/* Size: 172 bytes                                                     */
/* Calling convention: __fastcall (param_1 in ECX = DDRAW_Sprite*)    */
/*                                                                     */
/* Calls vtable[6] (invalidate, 0x18/4) on every sub-object:          */
/*   - The root DDRAW_Sprite itself                                    */
/*   - 5 GameObjects at +0x38, +0xE8, +0x10A, +0x12C                  */
/*   - 4 pattern sub-objects at +0x5A, +0x7C, +0x9E, +0xC0           */
/* Then calls RESDATA_DtorBase and zeroes state fields.                */
/*                                                                     */
/* Called by: CGWND_Cleanup                                            */
/* ================================================================== */
void __fastcall DDRAW_InvalidateAll(void* sprite)
{
    uint32_t* p = (uint32_t*)sprite;
/* vtable at +0x00 is compiler-managed */
    vtable = *(void***)sprite;
    ((void (*)(int32_t, int32_t, int32_t))vtable[6])(0, -1, 0);

    /* Invalidate 5 GameObjects */
    void* objects[] = {
        (uint8_t*)sprite + 0x38,
        (uint8_t*)sprite + 0xE8,
        (uint8_t*)sprite + 0x10A,
        (uint8_t*)sprite + 0x12C,
        (uint8_t*)sprite + 300
    };
    for (int i = 0; i < 5; i++) {
        vtable = *(void***)objects[i];
        ((void (*)(int32_t, int32_t, int32_t))vtable[6])(0, -1, 0);
    }

    /* Invalidate 4 pattern sub-objects (0x22 byte stride) */
    for (int i = 0; i < 4; i++) {
        void* pattern = (uint8_t*)sprite + 0x5A + i * 0x22;
        vtable = *(void***)pattern;
        ((void (*)(int32_t, int32_t, int32_t))vtable[6])(0, -1, 0);
    }

    /* Clean up base + zero state fields */
    RESDATA_DtorBase(sprite);

    p[0x150] = 0;  /* +0x540 */
    p[0x15C] = 0;  /* +0x570 */
    p[0x15B] = 0;  /* +0x56C */
    p[0x15A] = 0;  /* +0x568 */
    p[0x159] = 0;  /* +0x564 */
    p[0x160] = 0;  /* +0x580 */
    p[0x15F] = 0;  /* +0x57C */
    p[0x15E] = 0;  /* +0x578 */
    p[0x15D] = 0;  /* +0x574 */
    p[0x16B] = 0;  /* +0x5AC */
    p[0x16A] = 0;  /* +0x5A8 */
    p[0x169] = 0;  /* +0x5A4 */
    p[0x14E] = 0;  /* +0x538 */
}

/* ================================================================== */
/* DDRAW_ClampToViewport — Clamp DDRAW sprite position to viewport     */
/* Address: 0x459720                                                   */
/* Size: 180 bytes                                                     */
/* Calling convention: __fastcall (param_1 in ECX = DDRAW_Sprite*)    */
/*                                                                     */
/* Checks if the sprite's bounding rect (param_1[2..5] = left/top/     */
/* right/bottom at +0x08/+0x0C/+0x10/+0x14) intersects the town view  */
/* or overlay rect. If not, skips. If it DOES intersect, adjusts       */
/* sprite position (vtable[3] = SetRenderOffset) relative to client    */
/* area offset minus the sprite's internal offset (+0x10/+0x14).       */
/*                                                                     */
/* Then clamps X: if left < viewport_x, or right > viewport_x - frame_ */
/* width + client_offset_x, adjusts position via vtable[3].            */
/* Similarly for Y.                                                     */
/*                                                                     */
/* Frame width/height read from RESDATA pointer at +0x40:              */
/*   resdata+0x14 = width (uint16)                                     */
/*   resdata+0x16 = height (uint16)                                    */
/*                                                                     */
/* @param sprite  DDRAW_Sprite*                                        */
/* ================================================================== */
void __fastcall DDRAW_ClampToViewport(void* sprite)
{
    int32_t* rect = (int32_t*)((uint8_t*)sprite + 8);  /* left, top, right, bottom */
    int32_t intersect_rect[4];
    void** vtable = *(void***)sprite;

    /* Check intersection with town view rect */
    if (!IntersectRect(intersect_rect, rect, g_town_view_rect)) {
        /* Try overlay rect */
        if (!IntersectRect(intersect_rect, rect, g_town_overlay_rect)) {
            goto CLAMP_X;
        }
    }

    /* Adjust position relative to client area */
    ((void (*)(int32_t, int32_t))vtable[3])(
        g_client_offset_x - *(int32_t*)((uint8_t*)sprite + 0x10),
        g_client_offset_y - *(int32_t*)((uint8_t*)sprite + 0x14));

CLAMP_X:
    /* Clamp X */
    {
        int32_t new_x;

        if (rect[0] < g_viewport_x) {
            new_x = g_viewport_x;
        } else {
            uint8_t* resdata = *(uint8_t**)((uint8_t*)sprite + 0x40);
            int32_t frame_width = *(uint16_t*)(resdata + 0x14);
            new_x = g_viewport_x - frame_width + g_client_offset_x;
            if (new_x >= rect[0]) goto CLAMP_Y;
        }

        /* Adjust X via vtable[3] */
        ((void (*)(int32_t, int32_t))vtable[3])(new_x, *(int32_t*)((uint8_t*)sprite + 0x0C));
    }

CLAMP_Y:
    /* Clamp Y */
    {
        int32_t new_y;

        if (*(int32_t*)((uint8_t*)sprite + 0x0C) < g_viewport_y) {
            new_y = g_viewport_y;
        } else {
            uint8_t* resdata = *(uint8_t**)((uint8_t*)sprite + 0x40);
            int32_t frame_height = *(uint16_t*)(resdata + 0x16);
            new_y = g_viewport_y - frame_height + g_client_offset_y;
            if (new_y >= *(int32_t*)((uint8_t*)sprite + 0x0C)) return;
        }

        /* Adjust Y via vtable[3] */
        ((void (*)(int32_t, int32_t))vtable[3])(*(int32_t*)((uint8_t*)sprite + 8), new_y);
    }
}
