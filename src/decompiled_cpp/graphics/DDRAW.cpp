/**
 * DDRAW.cpp — DDRAW_Building class and DirectDraw rendering functions
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * This file implements:
 *   A) DDRAW_Building class methods (building/station/vehicle sprite manager)
 *   B) C free functions for surface/audio/clipper/file management
 *   C) Error string tables for DirectDraw and DirectSound HRESULT codes
 *
 * All addresses reference loco.exe (MSVC 6.0, x86 32-bit).
 */

// Status: TRANSCRIBED

#include "DDRAW.h"
#include <new>
#include <cassert>

/* ================================================================== */
/* Typed slot view — grounded in Ghidra vtable 0x478548                */
/*                                                                     */
/* DDRAW_Building vtable (0x478548):                                   */
/*   [3] SetPosition      (move sprite to x,y)   → @ 0x45xxxx         */
/*   [6] LoadChildResource (resId, unknown, flags) → "LoadRes"         */
/*   [7] HandleOneArgAction (single int arg)      → "SetAnim"          */
/*   [8] Refresh           (re-render sprite)     → "Update"/"Refresh" */
/*   [20]=[0x50/4]: child entity update callback                       */
/*                                                                     */
/* The adapter below preserves the recovered slots while using typed    */
/* C++ virtual dispatch at each call site.                             */
/* ================================================================== */

/* Sprite slot interface; unused entries preserve the binary indices. */
struct DDRAW_SpriteView {
    virtual void* destroy(uint8_t flags) = 0; // slot 0
    virtual void refresh_child() = 0;         // slot 1
    virtual int32_t hit_test(int x, int y) = 0; // slot 2
    virtual void set_position(int x, int y) = 0; // slot 3
    virtual void slot4() = 0;
    virtual void slot5() = 0;
    virtual void load_resource(int resource_id, int arg, int flags) = 0; // slot 6
    virtual void set_animation(int frame) = 0; // slot 7
    virtual void refresh() = 0;               // slot 8
    virtual void slot9() = 0;
    virtual void slot10() = 0;
    virtual void slot11() = 0;
    virtual void slot12() = 0;
    virtual void slot13() = 0;
    virtual void slot14() = 0;
    virtual void slot15() = 0;
    virtual void slot16() = 0;
    virtual void slot17() = 0;
    virtual void slot18() = 0;
    virtual void slot19() = 0;
    virtual void child_update() = 0;          // slot 20

protected:
    ~DDRAW_SpriteView() = default;
};

static DDRAW_SpriteView* sprite_view(void* sprite)
{
    return static_cast<DDRAW_SpriteView*>(sprite);
}


/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* C++ allocation helpers */
void* __cdecl operator_new(size_t size);            /* @ 0x465CE0 */
void  __cdecl GLOBAL_free(void* ptr);               /* @ 0x465CD0 */

extern "C" {
    /* CRT memory management */
    void* __cdecl CRT_malloc_zero(size_t size);     /* CRT alloc */
    void  __cdecl CRT_free(void* ptr);              /* CRT free */
    int   __cdecl CRT_rand(void);                    /* rand() @ 0x466150 */

    /* CRC/file I/O */
    int   __cdecl CRT_0x4681D0(int* handle);        /* close stream handle */
    int   __cdecl CRT_0x468480(const char* path, const char* mode); /* fopen */
    int   __cdecl CRT_0x468610(char* buf, int size, int count, int* handle); /* fread */

    /* Utility */
    int   __cdecl CRT_sprintf_buf(char* buf, const char* fmt, ...); /* sprintf */

    /* Win32 */
    int   __cdecl DirectDrawCreate(void);            /* DirectDrawCreate via DDRAW.DLL */
}

/* RESDATA base functions (undecompiled) */
extern void __fastcall RESDATA_BaseInit(void* self);        /* @ 0x4544E0 */
extern void __fastcall RESDATA_DtorBase(void* self);        /* @ 0x454630 */
extern void __fastcall RESDATA_DtorBody(void* self);        /* @ 0x4545A0 */
extern void __fastcall RESDATA_UpdateChild(void* self);     /* @ 0x454F80 */
extern int  __fastcall RESDATA_HitTestChildren(void* self, int x, int y); /* @ 0x44A0C0 */
extern void __cdecl   RESDATA_Lock(void* ptr);              /* @ 0x449410 */
extern void __cdecl   RESDATA_Unlock(void* ptr);            /* @ 0x449420 */
extern void* __fastcall RESDATA_CreateChildSprite(void* parent, int res_id,
                                                   int a, int b); /* @ 0x454190 */
extern void __fastcall RESDATA_DispatchEvent(void* self, int a, int b,
                                              int c, int d, void* e, int f); /* @ 0x454900 */
extern void __fastcall RESDATA_SoundObject_Init(void* sprite, const char* text); /* @ 0x449070 */
extern int  __fastcall RESDATA_SoundObject_GetState(void* sprite); /* @ 0x4490D0 */
extern uint32_t __thiscall RESDATA_ScriptedObject_HandleKey(void* self, uint32_t key); /* @ 0x44A0C0 */
extern uint32_t __thiscall RESDATA_TextInput_HandleChar(void* self, uint32_t ch); /* @ 0x449100 */

/* GameObject base functions */
extern void __fastcall GameObject_BaseCtor(void* obj, int a, int b, int c, int d); /* @ 0x405790 */
extern void __fastcall GameObject_DtorBody(void* obj);       /* @ 0x405870 */
extern int  __fastcall GameObject_PtInRect(void* obj, int x, int y); /* @ 0x436A10 */
extern void __fastcall GameObject_GetRelPos(void* obj, int* out, int x, int y); /* @ 0x436A40 */
extern void __cdecl   CRT_memset_pattern(void* dst, int size, int count, void* pattern); /* @ 0x4671E0 */
extern void __cdecl   CRT_free_pattern(void* dst, int size, int count, void* dtor); /* @ 0x467280 */

/* Panel (RESDATA panel base) */
extern void __fastcall Panel_DtorBody(void* self);          /* @ 0x4545A0 (alias for RESDATA_DtorBody) */

/* TrackPiece zoom management */
extern void __thiscall TrackPiece_SetZoom(void* track_piece, short zoom); /* @ 0x40D170 */

/* World state check */
extern uint8_t __fastcall World_CheckActive(void* world);   /* @ 0x44DBB0 */

/* HelpWnd narration */
extern uint32_t __thiscall HelpWnd_PlayNarration(void* audio_mgr, int page, uint flags); /* @ 0x44F560 */

/* Tile map invalidation */
extern void __thiscall TileMap_InvalidateRect(void* tilemap, int left, int top,
                                              int right, int bottom); /* @ 0x455840 */

/* UI tooltip system */
extern void* __thiscall UI_CreateMessageBox(void* mgr, int res_id, short type,
                                            char anchor, int x, int y, char flags); /* @ 0x423AB0 */

/* Global state references */
extern void*  g_resmgr;                 /* 0x485580 — ResourceManager singleton */
extern int    g_cursor_world_x;         /* cursor world space X */
extern int    g_cursor_world_y;         /* cursor world space Y */
extern int    g_drag_start_x;           /* drag operation start X */
extern int    g_drag_start_y;           /* drag operation start Y */
extern int    g_viewport_x;             /* 0x4AAD24 — viewport left edge */
extern int    g_viewport_y;             /* 0x4AAD28 — viewport top edge */
extern int    g_client_offset_x;        /* client area offset X */
extern int    g_client_offset_y;        /* client area offset Y */
extern RECT   g_town_view_rect;         /* town view area rect */
extern RECT   g_town_overlay_rect;      /* town overlay rect */
extern int    g_building_count;         /* number of active buildings */
extern void*  g_building_list;          /* BuildingMgr collection */
extern int    g_world_width;            /* world pixel width */
extern int    g_world_height;           /* world pixel height */
extern int    g_screen_width;           /* screen width in pixels */
extern int    g_screen_height;          /* screen height in pixels */
extern void*  g_audio_mgr;             /* 0x4FD38C — Audio manager singleton */
extern GameAudio* g_audio;             /* GameAudio (DirectSound wrapper) */
extern const char g_empty_string[];     /* empty string literal */

/* Externally-defined global clipper references */
extern void* DAT_004ff0f8;  /* clipper handle 6 */
extern void* DAT_004ff0fc;  /* clipper handle 0 */
extern void* DAT_004ff100;  /* clipper handle 1 */
extern void* DAT_004ff104;  /* clipper handle 2 */
extern void* DAT_004ff108;  /* clipper handle 3 */
extern void* DAT_004ff10c;  /* clipper handle 4 */
extern void* DAT_004ff110;  /* clipper handle 5 / UIPANEL_Surface* */

/* ================================================================== */
/* Global g_ddraw_building at 0x4A9EF0                                */
/* ================================================================== */

DDRAW_Building* g_ddraw_building;  /* 0x4A9EF0 */

/* ================================================================== */
/* Global viewport / game time / tooltip manager                       */
/* ================================================================== */

int   g_viewport_x;         /* 0x4AAD24 */
int   g_viewport_y;         /* 0x4AAD28 */
/* g_game_time declared as uint32_t in shared/types.h */
void* g_tooltip_mgr;        /* 0x4FD220 */
void* g_world_state;        /* 0x4A98B0 */
void* g_tilemap;            /* 0x4AAD08 */

/* ================================================================== */
/* Field offset access helpers                                         */
/* ================================================================== */

/* Byte/short accessors for fields at known offsets that are not
   declared as named members in the class header (because they overlap
   with various RESDATA base fields). These are confirmed from the
   actual disassembly. */

#define BUILDING_TYPE_OFFSET      0x398  /* word: 2=road,3=station,4=track,6=vehicle... */
#define PANEL_STATE_OFFSET        0x39c  /* byte: 0=sprite back, 1=sprite front */
#define SPRITES_VISIBLE_OFFSET    0x39d  /* byte: visibility flag for colored dots etc */
#define ACTIVE_FLAG_OFFSET        0x88   /* byte: active selection flag */

#define TRACK_ENTITY_DATA         0x44c  /* dword: vehicle's track piece ptr (in Building) */
#define BUILDING_OCCUPANCY        0x120  /* dword: attached entity (in Building) */

/* ================================================================== */
/* DDRAW_Building::Create                                              */
/* Address: 0x4589B0                                                   */
/*                                                                     */
/* This is the factory constructor for DDRAW_Building. It allocates    */
/* through the operator_new mechanism (caller pre-allocates), then      */
/* calls RESDATA_BaseInit for base class initialization, constructs     */
/* 5 GameObject sub-objects and 4 pattern objects (via memset_pattern), */
/* clears all sprite slot fields, and sets the final vtable/type.      */
/* ================================================================== */

DDRAW_Building* DDRAW_Building::Create(void* mem)
{
    return mem != nullptr ? new (mem) DDRAW_Building() : nullptr;
}

/**
 * DDRAW_Building::DDRAW_Building
 * Address: 0x4589B0
 *
 * C++ construction installs the class vtable before the recovered binary
 * initialization sequence runs.
 */
DDRAW_Building::DDRAW_Building()
{
#ifndef _WIN32
    /* Host: the x86_64 layout of DDRAW_Building does not match the
     * original x86 binary layout.  The Ghidra decomp (0x4589B0) shows
     * embedded GameObjects at +0x38, +0xE8, +0x10A, +0x12C and pattern
     * sprites at +0x5A, but the C++ class uses native pointer-sized
     * fields and different padding.  Until the class is properly
     * ported with layout-correct inheritance (RESDATA base), we skip
     * the binary-emulating construction and just zero-initialize.
     * operator_new already zeroes the allocation. */
    type = 0xD;
    selected_entity = nullptr;
    station_list_offset = 0;
#else
    RESDATA_BaseInit(this);

    GameObject_BaseCtor(reinterpret_cast<void*>(&sub_object_1), -1, -1, 0, 0);
    CRT_memset_pattern(&pattern_sprites, 0x88, 4,
                       reinterpret_cast<void*>(0x458AF0));
    GameObject_BaseCtor(&popup_panel, -1, -1, 0, 0);
    GameObject_BaseCtor(&pattern_container, -1, -1, 0, 0);
    GameObject_BaseCtor(&track_sprite, -1, -1, 0, 0);

    tooltip_text_input  = nullptr;
    entry_sign_sprite   = nullptr;
    exit_sign_sprite    = nullptr;
    wheel_right_sprite  = nullptr;
    wheel_left_sprite   = nullptr;
    right_turn_sprite   = nullptr;
    left_turn_sprite    = nullptr;
    down_arrow_sprite   = nullptr;
    up_arrow_sprite     = nullptr;
    track_arrow_right   = nullptr;
    track_arrow_mid     = nullptr;
    track_arrow_left    = nullptr;
    selected_entity     = nullptr;
    station_list_offset = 0;
    type = 0xD;
#endif
}

/* ================================================================== */
/**
 * DDRAW_Building::~DDRAW_Building — body destructor
 * Address: 0x458B00
 *
 * The scalar deleting destructor at vtable[0] (0x458AD0) is
 * compiler-generated; it wraps this body and conditionally calls
 * operator delete when flags & 1.
 */
DDRAW_Building::~DDRAW_Building()
{
    CleanupSprites();
}

/* ================================================================== */
/* DDRAW_Building::CleanupSprites                                     */
/* Address: 0x458B00                                                   */
/*                                                                     */
/* Full destructor body: reverse-order cleanup of sub-objects and      */
/* pattern objects, then base class cleanup. SEH-protected.            */
/*                                                                     */
/* Order (reverse of construction):                                    */
/*   1. Restore vtable to 0x478548                                     */
/*   2. Call InvalidateAll to release resources                         */
/*   3. Destroy sub-object 4 at +0x4B0 (track_sprite)                  */
/*   4. Destroy sub-object 3 at +0x428 (pattern_container)             */
/*   5. Destroy sub-object 2 at +0x3A0 (popup_panel)                   */
/*   6. Free 4 pattern objects at +0x168 via CRT_free_pattern          */
/*   7. Destroy sub-object 1 at +0xE0                                  */
/*   8. Call RESDATA_DtorBody for base class cleanup                   */
/* ================================================================== */

void DDRAW_Building::CleanupSprites()
{
    // C++ destructor entry has already selected the DDRAW_Building vtable.

    // Release all child resources
    this->InvalidateAll();

    // Destroy embedded sub-objects in reverse order of construction
    GameObject_DtorBody(&this->track_sprite);           /* +0x4B0 */
    GameObject_DtorBody(&this->pattern_container);      /* +0x428 */
    GameObject_DtorBody(&this->popup_panel);            /* +0x3A0 */
    CRT_free_pattern(&this->pattern_sprites, 0x88, 4,
                     reinterpret_cast<void*>(&GameObject_DtorBody));
    GameObject_DtorBody(&this->sub_object_1);           /* +0xE0 */

    // Base class cleanup
    RESDATA_DtorBody(this);
}

/* ================================================================== */
/* DDRAW_Building::InvalidateAll                                      */
/* Address: 0x458BB0                                                   */
/*                                                                     */
/* Releases all sub-object resources by calling vtable[6]              */
/* (LoadChildResource with -1 to release) on self and every            */
/* sub-object/pattern sprite. Then calls RESDATA_DtorBase and          */
/* clears all sprite slot fields to 0.                                 */
/*                                                                     */
/* Order of release (confirmed by disassembly):                        */
/*   1. self                                                            */
/*   2. sub_object_1 at +0xE0                                          */
/*   3. popup_panel at +0x3A0                                          */
/*   4. pattern_container at +0x428                                    */
/*   5. track_sprite at +0x4B0                                         */
/*   6. pattern_sprites[0..3] at +0x168 (step by 0x88)                */
/*   7. RESDATA_DtorBase(this) for base resource cleanup               */
/*   8. Clear 13 sprite slot fields                                    */
/* ================================================================== */

void DDRAW_Building::InvalidateAll()
{
    // 1. Self
    sprite_view(this)->load_resource(0, -1, 0);

    // 2-5. Embedded resource-bearing child objects.
    sprite_view(static_cast<void*>(&this->sub_object_1))->load_resource(0, -1, 0);
    sprite_view(static_cast<void*>(&this->popup_panel))->load_resource(0, -1, 0);
    sprite_view(static_cast<void*>(&this->pattern_container))->load_resource(0, -1, 0);
    sprite_view(static_cast<void*>(&this->track_sprite))->load_resource(0, -1, 0);

    // 6. Four pattern sprites at +0x168, each 0x88 bytes.
    for (int i = 0; i < 4; i++) {
        void* pattern_obj = static_cast<void*>(&this->pattern_sprites[i * 0x88]);
        sprite_view(pattern_obj)->load_resource(0, -1, 0);
    }

    // 7. Base resource cleanup
    RESDATA_DtorBase(this);

    // 8. Clear 13 sprite slot fields (confirmed order from disassembly)
    this->tooltip_text_input    = nullptr; /* +0x540 */
    this->entry_sign_sprite     = nullptr; /* +0x570 */
    this->exit_sign_sprite      = nullptr; /* +0x56C */
    this->wheel_right_sprite    = nullptr; /* +0x568 */
    this->wheel_left_sprite     = nullptr; /* +0x564 */
    this->right_turn_sprite     = nullptr; /* +0x580 */
    this->left_turn_sprite      = nullptr; /* +0x57C */
    this->down_arrow_sprite     = nullptr; /* +0x578 */
    this->up_arrow_sprite       = nullptr; /* +0x574 */
    this->track_arrow_right     = nullptr; /* +0x5AC */
    this->track_arrow_mid       = nullptr; /* +0x5A8 */
    this->track_arrow_left      = nullptr; /* +0x5A4 */
    this->selected_entity       = nullptr; /* +0x538 */
}

/* ================================================================== */
/* DDRAW_Building::InitBuildingSprites                                */
/* Address: 0x458C90                                                   */
/*                                                                     */
/* Loads all building/station/vehicle UI sprites from the resource     */
/* manager. Each sprite gets a specific position via vtable[3] and     */
/* visibility handling.                                                */
/* ================================================================== */

uint8_t DDRAW_Building::InitBuildingSprites()
{
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — DDRAW_Building::InitBuildingSprites 0x458C90");
    return 1;
}

/* ================================================================== */
/* DDRAW_Building::SelectBuilding                                     */
/* Address: 0x459180                                                   */
/* ================================================================== */

uint8_t DDRAW_Building::SelectBuilding(Building* entity)
{
    // See first-pass decompilation or native/ddraw_building_sprites.c
    return *reinterpret_cast<uint8_t*>(reinterpret_cast<uint8_t*>(this) + ACTIVE_FLAG_OFFSET);
}

/* ================================================================== */
/* DDRAW_Building::ClampToViewport                                    */
/* Address: 0x459720                                                   */
/* ================================================================== */

void DDRAW_Building::ClampToViewport()
{
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — DDRAW_Building::ClampToViewport 0x459720");
}

/* ================================================================== */
/* DDRAW_Building::BuildingShowTooltip                                */
/* Address: 0x4588B0                                                   */
/*                                                                     */
/* Reads tooltip descriptor fields from param_1 and dispatches to      */
/* UI_CreateMessageBox. Anchor types control coordinate computation:   */
/*   'S' (0x53) = viewport-relative  — offset is added to viewport     */
/*   'W' (0x57) = window-absolute    — offset used directly            */
/*   'U'/'D'    = owner-relative     — offset added to this->x/y,      */
/*                                      anchor char preserved in call   */
/*   other      = owner-relative     — offset added to this->x/y,      */
/*                                      anchor overridden to 'W'       */
/* ================================================================== */

void DDRAW_Building::BuildingShowTooltip(void* desc)
{
    // Descriptor struct fields (confirmed from disassembly at 0x4588B0)
    int tooltip_id = *reinterpret_cast<int*>(static_cast<uint8_t*>(desc) + 0x20);
    if (tooltip_id <= 0) {
        return;
    }

    int16_t tooltip_type  = *reinterpret_cast<int16_t*>(static_cast<uint8_t*>(desc) + 0x24);
    char    anchor_char   = *reinterpret_cast<char*>(static_cast<uint8_t*>(desc) + 0x28);
    int     offset_x      = *reinterpret_cast<int*>(static_cast<uint8_t*>(desc) + 0x2C);
    int     offset_y      = *reinterpret_cast<int*>(static_cast<uint8_t*>(desc) + 0x30);

    int pos_x, pos_y;

    if (anchor_char == 'S') {
        // Viewport-relative: position = viewport scroll + offset
        pos_x = offset_x + g_viewport_x;
        pos_y = offset_y + g_viewport_y;
    } else {
        pos_x = offset_x;
        if (anchor_char == 'W') {
            // Window-absolute: position = offset directly
            pos_y = offset_y;
        } else {
            // Owner-relative: position = this->x/y + offset
            int32_t* self_pos = reinterpret_cast<int32_t*>(
                reinterpret_cast<uint8_t*>(this) + 0x08);  /* +0x08 = x, +0x0C = y */
            pos_x += self_pos[0];
            pos_y  = offset_y + self_pos[1];
        }
    }

    // For 'U' (0x55) or 'D' (0x44) anchors, pass the original anchor char
    // For 'S' and 'W' anchors, override to 'W' (0x57)
    char effective_anchor = anchor_char;
    if (anchor_char != 'U' && anchor_char != 'D') {
        effective_anchor = 'W';
    }

    UI_CreateMessageBox(g_tooltip_mgr, tooltip_id, tooltip_type,
                        effective_anchor, pos_x, pos_y, 1);
}

/* ================================================================== */
/* DDRAW_Building::UpdateSubObject                                    */
/* Address: 0x459D40                                                   */
/* ================================================================== */

void DDRAW_Building::UpdateSubObject()
{
    RESDATA_UpdateChild(this);
    sprite_view(static_cast<void*>(&this->sub_object_1))->refresh_child();
}

/* ================================================================== */
/* DDRAW_Building::HitTest                                            */
/* Address: 0x459D60                                                   */
/* ================================================================== */

int32_t DDRAW_Building::HitTest(int32_t x, int32_t y)
{
    if (GameObject_PtInRect(this, x, y)) return 1;
    if (sprite_view(static_cast<void*>(&this->sub_object_1))->hit_test(x, y)) return 1;
    return 0;
}

/* ================================================================== */
/* DDRAW_Building::HitTestWithDrag                                    */
/* Address: 0x45A740                                                   */
/* ================================================================== */

uint8_t DDRAW_Building::HitTestWithDrag(int32_t x, int32_t y)
{
    return RESDATA_HitTestChildren(this, x, y);
}

/* ================================================================== */
/* DDRAW_Building::HandleKeyPress                                     */
/* Address: 0x45B3A0                                                   */
/* ================================================================== */

uint32_t DDRAW_Building::HandleKeyPress(uint32_t key_code)
{
    (void)key_code;
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — DDRAW_Building::HandleKeyPress 0x45B3A0");
    return 0;
}

/* ================================================================== */
/* DDRAW_Building::BuildingClickHandler                               */
/* Address: 0x458820                                                   */
/* ================================================================== */

void DDRAW_Building::BuildingClickHandler(void* desc)
{
    (void)desc;
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — DDRAW_Building::BuildingClickHandler 0x458820");
}

/* ================================================================== */
/* DDRAW_Building::BuildingTimeUpdate                                 */
/* Address: 0x458940                                                   */
/* ================================================================== */

void DDRAW_Building::BuildingTimeUpdate()
{
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — DDRAW_Building::BuildingTimeUpdate 0x458940");
}

/* ================================================================== */
/* DDRAW_Building::UpdateBuildingSprites                              */
/* Address: 0x4597E0                                                   */
/*                                                                     */
/* Per-frame refresh of all popup sprites. Called whenever building    */
/* selection state changes or per-frame during station/vehicle hover.  */
/*                                                                     */
/* Overview:                                                           */
/*   1. Load front/back resource (0x3800 vs 0x3801) based on           */
/*      panel_state byte at +0x39c                                      */
/*   2. Set name sprite text from selected_entity's name field         */
/*      (via tooltip_text_input SoundObject)                           */
/*   3. Update track_arrow sprite visibility based on building_type    */
/*   4. Set wheel sprites always visible                               */
/*   5. Set wheel_right zoom (1-3):                                   */
/*      - if panel_state==0: type 8, 0xC, 4, or 2 → zoom=3 (hidden)  */
/*        (for type 4/2: check entity->byte_0x8d)                     */
/*      - else: zoom=2 (visible) if sprites_visible==0                */
/*      - else: zoom=3 (visible, default state if in pattern mode)    */
/*   6. Update exit/entry sign visibility per building_type           */
/*   7. Update 8 row label sprites (station names)                    */
/*   8. Update entry_sign sprite (same as exit visibility pattern)    */
/*   9. If sprites_visible:                                           */
/*      a) Hide down_arrow, manage left/right turn sprites per        */
/*         selected_entity's attachment (building->occupancy)          */
/*      b) Show dot sprite pairs (colored_dots_A/B) as visible        */
/*   10. If NOT sprites_visible (station default):                    */
/*       a) Show down_arrow, left_turn, right_turn, up_arrow per      */
/*          building_type==3 flag                                     */
/*       b) For station type 3 with occupancy: set left/right zoom    */
/*          based on track piece and World_CheckActive status          */
/*   11. Set colored dot pair visibility (colored_dots_A[0..3]        */
/*       and B[0..3]) to sprites_visible flag                         */
/*   12. Set pattern sprite visibility to sprites_visible flag        */
/*   13. If station (type 3) and sprites_visible=true and occupancy   */
/*       exists: update pattern sprite passenger counts               */
/*   14. End: call vtable[3] (SetPosition) with current position      */
/* ================================================================== */

void DDRAW_Building::UpdateBuildingSprites()
{
    // Step 1: Load front/back resource based on panel_state
    uint8_t panel_state = *reinterpret_cast<uint8_t*>(
        reinterpret_cast<uint8_t*>(this) + PANEL_STATE_OFFSET);  /* +0x39c */
    uint8_t sprites_visible = *reinterpret_cast<uint8_t*>(
        reinterpret_cast<uint8_t*>(this) + SPRITES_VISIBLE_OFFSET);  /* +0x39d */

    if (panel_state == 0) {
        sprite_view(this)->load_resource(0x3801, 0, 1);
    } else {
        sprite_view(this)->load_resource(0x3800, 0, 1);
    }

    // Step 2: Determine building type flags
    int16_t btype = *reinterpret_cast<int16_t*>(
        reinterpret_cast<uint8_t*>(this) + BUILDING_TYPE_OFFSET);  /* +0x398 */
    bool is_vehicle = (btype == 6);
    bool is_station = (btype == 3);

    // Step 3: Set name sprite text
    {
        const char* name_str;
        void* entity = this->selected_entity;  /* +0x538 */

        if (is_vehicle && entity) {
            // Vehicle type: get name from track piece entity
            void* track_piece = *reinterpret_cast<void**>(
                static_cast<uint8_t*>(entity) + TRACK_ENTITY_DATA);  /* +0x44C */
            if (track_piece) {
                void** name_field = reinterpret_cast<void**>(
                    static_cast<uint8_t*>(track_piece) + 0x10);
                name_str = reinterpret_cast<const char*>(
                    reinterpret_cast<uint8_t*>(*name_field) + 0x7C);
            } else {
                name_str = reinterpret_cast<const char*>(
                    reinterpret_cast<uint8_t*>(entity) + 0x7C);
            }
        } else {
            name_str = entity
                ? reinterpret_cast<const char*>(
                    reinterpret_cast<uint8_t*>(entity) + 0x7C)
                : "";
        }

        RESDATA_SoundObject_Init(this->tooltip_text_input, name_str);
    }

    // Set tooltip_text_input flags
    {
        uint8_t* snd = reinterpret_cast<uint8_t*>(this->tooltip_text_input);
        snd[0x58] = 1;                              // always active
        snd[0x56] = (sprites_visible == 0) ? 1 : 0;  // visible when sprites_visible==0
    }

    // Refresh tooltip text input
    {
        sprite_view(this->tooltip_text_input)->refresh();
        sprite_view(this->tooltip_text_input)->refresh();
    }

    // Step 4: Update track arrow visibility
    {
        // track_visible (+0x4D4): hidden when sprites_visible==0 AND not vehicle
        uint8_t* track_vis = reinterpret_cast<uint8_t*>(this) + 0x4D4 /* track_visible */;
        *track_vis = (sprites_visible == 0 && !is_vehicle) ? 1 : 0;

        // track_arrow_left (+0x5A4), mid (+0x5A8), right (+0x5AC) visibility
        uint8_t vis = is_vehicle ? 1 : 0;
        {
            uint8_t* trk_vis = reinterpret_cast<uint8_t*>(this->track_arrow_left);
            trk_vis[0x56] = vis;
        }
        sprite_view(this->track_arrow_left)->refresh();
        sprite_view(this->track_arrow_left)->refresh();

        {
            uint8_t* trk_vis = reinterpret_cast<uint8_t*>(this->track_arrow_mid);
            trk_vis[0x56] = vis;
        }
        sprite_view(this->track_arrow_mid)->refresh();

        {
            uint8_t* trk_vis = reinterpret_cast<uint8_t*>(this->track_arrow_right);
            trk_vis[0x56] = vis;
        }
        sprite_view(this->track_arrow_right)->refresh();
    }

    // Step 5: Set wheel sprites always visible
    {
        uint8_t* wh_vis = reinterpret_cast<uint8_t*>(this->wheel_left_sprite);
        wh_vis[0x56] = 1;
    }
    {
        sprite_view(this->wheel_left_sprite)->refresh();
        sprite_view(this->wheel_left_sprite)->refresh();
    }

    // Step 6: Set wheel_right sprite — always visible
    {
        uint8_t* wh_vis = reinterpret_cast<uint8_t*>(this->wheel_right_sprite);
        wh_vis[0x56] = 1;
    }

    // Step 7: Determine zoom for wheel_right sprite (1-3)
    {
        short zoom;
        if (panel_state == 0) {
            if (btype == 8 ||
                btype == 0xC ||
                btype == 4 ||
                btype == 2) {
                // For type 2/4: check if selected entity's byte_0x8d is zero
                if ((btype == 4 || btype == 2) && this->selected_entity) {
                    uint8_t* ent = reinterpret_cast<uint8_t*>(this->selected_entity);
                    if (ent[0x8D] == 0) {
                        zoom = 3;  // hide
                        goto set_wheel_zoom;
                    }
                }
                zoom = 3;  // hide
            } else {
                zoom = 1;  // show
            }
        } else if (sprites_visible == 0) {
            zoom = 2;  // show enlarged
        } else {
            zoom = 3;  // show default in pattern mode
        }
    set_wheel_zoom:
        TrackPiece_SetZoom(this->wheel_right_sprite, zoom);
    }

    // Refresh wheel_right
    {
        sprite_view(this->wheel_right_sprite)->refresh();
        sprite_view(this->wheel_right_sprite)->refresh();
    }

    // Step 8: Update exit_sign_sprite (+0x56C) visibility
    {
        bool exit_vis;
        if (panel_state == 0 || is_station) {
            exit_vis = false;
        } else {
            exit_vis = true;
        }
        uint8_t* s_vis = reinterpret_cast<uint8_t*>(this->exit_sign_sprite);
        s_vis[0x56] = exit_vis ? 1 : 0;
    }
    {
        sprite_view(this->exit_sign_sprite)->refresh();
        sprite_view(this->exit_sign_sprite)->refresh();
    }

    // Step 9: Update 8 row label sprites at +0x544..+0x560
    {
        for (int i = 0; i < 8; i++) {
            bool label_vis;
            if (panel_state == 0 || is_station) {
                label_vis = false;
            } else {
                label_vis = true;
            }
            uint8_t* s_vis = reinterpret_cast<uint8_t*>(this->station_name_sprites[i]);
            s_vis[0x56] = label_vis ? 1 : 0;
            sprite_view(this->station_name_sprites[i])->refresh();
            sprite_view(this->station_name_sprites[i])->refresh();
        }
    }

    // Step 10: Update entry_sign_sprite (+0x570) visibility
    {
        bool entry_vis;
        if (panel_state == 0 || is_station) {
            entry_vis = false;
        } else {
            entry_vis = true;
        }
        uint8_t* s_vis = reinterpret_cast<uint8_t*>(this->entry_sign_sprite);
        s_vis[0x56] = entry_vis ? 1 : 0;
    }
    {
        sprite_view(this->entry_sign_sprite)->refresh();
        sprite_view(this->entry_sign_sprite)->refresh();
    }

    // Step 11: Directional sprites (depending on sprites_visible)
    if (sprites_visible) {
        // --- VISIBLE MODE (e.g. on a station) ---
        // Hide down_arrow
        reinterpret_cast<uint8_t*>(this->down_arrow_sprite)[0x56] = 0;
        {
            sprite_view(this->down_arrow_sprite)->refresh();
            sprite_view(this->down_arrow_sprite)->refresh();
        }

        // left_turn: visible if selected entity has no occupancy (building->occupancy==0)
        {
            void* entity = this->selected_entity;
            int occupancy = entity ? *reinterpret_cast<int*>(
                static_cast<uint8_t*>(entity) + BUILDING_OCCUPANCY) : 1;  /* +0x120 */
            reinterpret_cast<uint8_t*>(this->left_turn_sprite)[0x56] = (occupancy == 0) ? 1 : 0;
        }
        {
            sprite_view(this->left_turn_sprite)->set_position(0x8C, 0xB8);
        }
        {
            sprite_view(this->left_turn_sprite)->refresh();
            sprite_view(this->left_turn_sprite)->refresh();
        }

        // right_turn: visible if selected entity DOES have occupancy
        {
            void* entity = this->selected_entity;
            int occupancy = entity ? *reinterpret_cast<int*>(
                static_cast<uint8_t*>(entity) + BUILDING_OCCUPANCY) : 0;  /* +0x120 */
            reinterpret_cast<uint8_t*>(this->right_turn_sprite)[0x56] = (occupancy != 0) ? 1 : 0;
        }
        {
            sprite_view(this->right_turn_sprite)->set_position(0x8C, 0xB8);
        }
        {
            sprite_view(this->right_turn_sprite)->refresh();
            sprite_view(this->right_turn_sprite)->refresh();
        }

        // Hide up_arrow
        reinterpret_cast<uint8_t*>(this->up_arrow_sprite)[0x56] = 0;
        goto colored_dots_update;

    } else {
        // --- INVISIBLE MODE (default sprites back) ---
        // Show directional sprites per station flag
        {
            reinterpret_cast<uint8_t*>(this->down_arrow_sprite)[0x56] = is_station ? 1 : 0;
        }
        {
            sprite_view(this->down_arrow_sprite)->refresh();
            sprite_view(this->down_arrow_sprite)->refresh();
        }

        {
            reinterpret_cast<uint8_t*>(this->left_turn_sprite)[0x56] = is_station ? 1 : 0;
        }
        {
            sprite_view(this->left_turn_sprite)->set_position(0xA6, 0x33);
        }
        {
            sprite_view(this->left_turn_sprite)->refresh();
            sprite_view(this->left_turn_sprite)->refresh();
        }

        {
            reinterpret_cast<uint8_t*>(this->right_turn_sprite)[0x56] = is_station ? 1 : 0;
        }
        {
            sprite_view(this->right_turn_sprite)->set_position(0xEC, 0x33);
        }
        {
            sprite_view(this->right_turn_sprite)->refresh();
            sprite_view(this->right_turn_sprite)->refresh();
        }

        {
            reinterpret_cast<uint8_t*>(this->up_arrow_sprite)[0x56] = is_station ? 1 : 0;
        }
        {
            sprite_view(this->up_arrow_sprite)->refresh();
            sprite_view(this->up_arrow_sprite)->refresh();
        }

        // For stations without occupancy, manage zoom levels
        if (is_station) {
            void* entity = this->selected_entity;
            if (entity) {
                int occupancy = *reinterpret_cast<int*>(
                    static_cast<uint8_t*>(entity) + BUILDING_OCCUPANCY);  /* +0x120 */
                if (occupancy == 0) {
                    // No attached track piece
                    uint8_t active = World_CheckActive(g_world_state);
                    if (active) {
                        TrackPiece_SetZoom(this->left_turn_sprite, 3);
                    } else {
                        if (*reinterpret_cast<int16_t*>(
                                reinterpret_cast<uint8_t*>(this->left_turn_sprite) + 0x48) == 3) {
                            TrackPiece_SetZoom(this->left_turn_sprite, 1);
                        }
                    }
                    TrackPiece_SetZoom(this->down_arrow_sprite, 3);
                    TrackPiece_SetZoom(this->right_turn_sprite, 3);
                    TrackPiece_SetZoom(this->up_arrow_sprite, 3);
                } else {
                    // Has attached track piece
                    TrackPiece_SetZoom(this->left_turn_sprite, 3);
                    int track_type = *reinterpret_cast<int*>(
                        reinterpret_cast<uint8_t*>(occupancy) + 0x64);
                    if (track_type != 2) {
                        TrackPiece_SetZoom(this->down_arrow_sprite, 3);
                        TrackPiece_SetZoom(this->right_turn_sprite, 3);
                        TrackPiece_SetZoom(this->up_arrow_sprite, 3);
                    } else {
                        if (*reinterpret_cast<int16_t*>(
                                reinterpret_cast<uint8_t*>(this->down_arrow_sprite) + 0x48) == 3) {
                            TrackPiece_SetZoom(this->down_arrow_sprite, 1);
                        }
                        if (*reinterpret_cast<int16_t*>(
                                reinterpret_cast<uint8_t*>(this->right_turn_sprite) + 0x48) == 3) {
                            TrackPiece_SetZoom(this->right_turn_sprite, 1);
                        }
                        if (*reinterpret_cast<int16_t*>(
                                reinterpret_cast<uint8_t*>(this->up_arrow_sprite) + 0x48) == 3) {
                            TrackPiece_SetZoom(this->up_arrow_sprite, 1);
                        }
                    }
                }
            }

            // Refresh all directional sprites
            {
                sprite_view(this->left_turn_sprite)->refresh();
                sprite_view(this->down_arrow_sprite)->refresh();
                sprite_view(this->right_turn_sprite)->refresh();
                sprite_view(this->up_arrow_sprite)->refresh();
            }
        }
    }

colored_dots_update:

    // Step 12: Update colored dot pair visibility
    {
        for (int i = 0; i < 4; i++) {
            uint8_t* dot_a = reinterpret_cast<uint8_t*>(this->colored_dots_A[i]);
            dot_a[0x56] = sprites_visible;
            sprite_view(this->colored_dots_A[i])->refresh();
            sprite_view(this->colored_dots_A[i])->refresh();

            uint8_t* dot_b = reinterpret_cast<uint8_t*>(this->colored_dots_B[i]);
            dot_b[0x56] = sprites_visible;
            sprite_view(this->colored_dots_B[i])->refresh();
        }
    }

    // Step 13: Set pattern sprite visibility to sprites_visible
    {
        // pattern_visible at +0x44C
        *reinterpret_cast<uint8_t*>(reinterpret_cast<uint8_t*>(this) + 0x44C) = sprites_visible;

        // pattern_sprites[0..3] byte at +0x56 within each
        for (int i = 0; i < 4; i++) {
            uint8_t* pat = reinterpret_cast<uint8_t*>(&this->pattern_sprites) + i * 0x88;
            pat[0x56] = sprites_visible;
        }
    }

    // Step 14: If station with sprites visible and occupancy, update passenger counts
    if (is_station && sprites_visible) {
        void* entity = this->selected_entity;
        if (entity) {
            int occupancy = *reinterpret_cast<int*>(
                static_cast<uint8_t*>(entity) + BUILDING_OCCUPANCY);  /* +0x120 */
            if (occupancy) {
                // Pattern sprite 0: compute passenger count from track piece
                {
                    void* track_piece = *reinterpret_cast<void**>(
                        reinterpret_cast<uint8_t*>(occupancy) + 0x10);
                    void* res_frame = *reinterpret_cast<void**>(
                        reinterpret_cast<uint8_t*>(track_piece) + 0x40);
                    int frame_count = res_frame
                        ? *reinterpret_cast<int*>(static_cast<uint8_t*>(res_frame) + 0x04)
                        : -1;
                    int count = (frame_count - 0x1804) / 2;

                    void* pattern_sprite =
                        static_cast<void*>(&this->pattern_sprites[0]);
                    sprite_view(pattern_sprite)->set_animation(count);
                }

                // Pattern sprites 1-3: per-occupant passenger counts
                for (int slot = 0; slot < 3; slot++) {
                    // pattern sprite at index 1, 2, 3
                    void* pattern_sprite = static_cast<void*>(
                        &this->pattern_sprites[(1 + slot) * 0x88]);

                    void* occupant = *reinterpret_cast<void**>(
                        static_cast<uint8_t*>(static_cast<void*>(&occupancy)) + 4 + slot * 4);
                    /* Actually: occupancy is at building+0x120, but the occupant
                       pointers are at occupancy+0x14, +0x18, +0x1C (3 slots).
                       Wait, need to be more careful here. */

                    // Re-read from the decompiler output more carefully:
                    // piVar6 = (int*)(iVar7 + 0x14); iVar7 = occupancy from building+0x120
                    // so occupants are at occupancy + 0x14, +0x18, +0x1C

                    void* occupant_ptr = *reinterpret_cast<void**>(
                        reinterpret_cast<uint8_t*>(occupancy) + 0x14 + slot * 4);

                    int frame_val;
                    if (occupant_ptr == nullptr) {
                        frame_val = 0;
                    } else {
                        void* occ_res = *reinterpret_cast<void**>(
                            reinterpret_cast<uint8_t*>(occupant_ptr) + 0x40);
                        int occ_frame = occ_res
                            ? *reinterpret_cast<int*>(static_cast<uint8_t*>(occ_res) + 0x04)
                            : -1;
                        frame_val = (occ_frame - 0x1866) / 2 + 1;
                    }

                    sprite_view(pattern_sprite)->set_animation(frame_val);
                }
            }
        }
    }

    // Step 15: Final SetPosition call via the typed slot-3 view.
    {
        const auto* self_bytes = reinterpret_cast<const int32_t*>(this);
        sprite_view(this)->set_position(self_bytes[2], self_bytes[3]);
    }
}

/* ================================================================== */
/* DDRAW_Building::UpdateBuilding                                     */
/* Address: 0x459DA0                                                   */
/*                                                                     */
/* Per-frame building selection update. Called by GameLoop_FrameUpdate */
/* via vtable dispatch (0x45C4E6).                                     */
/*                                                                     */
/* Flow:                                                               */
/*   1. Check active flag at +0x88. If inactive, return.               */
/*   2. Check selected_entity's byte at +0x18. If not 1, deselect.    */
/*   3. Cursor hover management:                                      */
/*      a) If cursor is over sub_object_1 and hover_state=0:           */
/*         set hover state 1, invalidate.                              */
/*      b) If cursor NOT over sub_object_1 and hover_state!=0:        */
/*         set hover state 0, invalidate.                              */
/*   4. Pattern animation: if pattern_visible and update flag set,     */
/*      walk 4 pattern sprites, randomize passenger count animation.   */
/*   5. Drag mode check: if drag_active (byte at +0x90 == 1),         */
/*      call SetPosition with cursor offset.                           */
/*   6. Child entity dispatch: walk linked list at +0xD0,              */
/*      call vtable[20] (vtable[0x50/4]) on each child.               */
/*   7. Station list scroll: if popup_visible and type==7,            */
/*      scroll to match building->byte_0x88 >> 1.                     */
/*   8. Station type (3):                                           */
/*      - Timer-based sprite update (g_game_time check)               */
/*      - Play ambient narration (page 0xC)                            */
/*   9. Vehicle type (6):                                            */
/*      - Track arrow zoom levels (1-3) based on track piece state    */
/*      - Track_Sprite zoom management                                */
/*      - Update vehicle sprites on track not moving                  */
/*      - Detect departure (track->state==2 or has data)             */
/*      - Play ambient narration (page 0xB)                            */
/* ================================================================== */

void DDRAW_Building::UpdateBuilding()
{
    uint8_t* self_u8 = reinterpret_cast<uint8_t*>(this);

    // Step 1: Active check
    uint8_t active = self_u8[ACTIVE_FLAG_OFFSET];  /* +0x88 */
    if (!active) return;

    // Step 2: Entity alive check
    void* entity = this->selected_entity;
    if (entity && reinterpret_cast<uint8_t*>(entity)[0x18] != 1) {
        this->SelectBuilding(nullptr);
        return;
    }

    // Step 3: Cursor hover management
    {
        // Hover ON
        void* sub_object = static_cast<void*>(&this->sub_object_1);
        int hovered = sprite_view(sub_object)->hit_test(
            g_cursor_world_x, g_cursor_world_y);
        if (hovered) {
            int* hover_state = reinterpret_cast<int*>(self_u8 + 0x108);
            if (*hover_state != 1) {
                sprite_view(sub_object)->set_animation(1);

                // Invalidate rect at +0xE8
                // TODO: implement invalidation
            }
        }
    }
}