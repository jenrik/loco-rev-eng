/**
 * UIPANEL.cpp — UIPANEL implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * NOTE: Surface/blit functions are in UIPANEL_Surface.cpp.
 * Drawing/sprite functions are in UIPANEL_Draw.cpp.
 * This file contains the UIPANEL class lifecycle and rendering pipeline.
 */

// Status: TRANSCRIBED

#include "UIPANEL.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
// vtable_addrs.h removed — compiler manages vtables via virtual methods

/* Windows API stubs — specific declarations to avoid conflicts with types.h */
extern "C" {
BOOL      GetCursorPos(POINT*);
BOOL      UnionRect(RECT*, const RECT*, const RECT*);
LRESULT   DefWindowProcA(HWND, UINT, WPARAM, LPARAM);
BOOL      DestroyWindow(HWND);
void      PostQuitMessage(int);
void      Sleep(DWORD);
void      ExitProcess(UINT);
}

/* DirectDraw blit flags (from ddraw.h, avoiding full include) */
#define DDBLT_WAIT  0x00000010

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* Windows API — provided by stubs/windows.h via types.h; no need to redeclare */

/* Heap / CRTs (C++ linkage — NOT extern "C") */
extern void* operator_new(size_t size);              /* 0x465CE0 */
extern void  GLOBAL_free(void* ptr);                 /* 0x465CD0 */
extern void  WIN32_FatalError(void);

/* DirectDraw globals — void* to avoid IDirectDraw4 dependency */
extern void* g_ddraw;                                /* 0x4FD394 */
extern void* g_primary_surface;                      /* 0x4FD3C4 */
extern void* g_backbuffer;                           /* 0x4FD3C0 */

/* DDRAW helpers */
extern void DDRAW_UnlockPrimary(void);               /* 0x4014CD */
/* DDRAW_PresentRect (0x401280) declared canonically in graphics/LOCOBITMAP.h
 * (included transitively via UIPANEL.h), guarded #ifdef _WIN32/#else so host
 * builds see the SDL3-path signature instead of the original RECT/HWND one. */
/* Address corrected: 0x4412F0 disassembles to NameEntryPanel_CreateWindow,
 * not DDRAW_SelectBuilding; confirmed via Ghidra at 0x459180. This is the
 * (void*, void*) overload — a different mangled symbol from the
 * (void*, int) one used by world/tilemap.cpp — see graphics/DDRAW.cpp. */
extern void DDRAW_SelectBuilding(void* ddraw, void* building); /* 0x459180 */

/* External functions */
extern void __thiscall RESDATA_BaseInit(void* self);                         /* 0x4544E0 */
extern void __thiscall RESDATA_DtorBase(void* self);                         /* 0x454630 */

/* Free-function WndProc wrappers retain their recovered binary entry points;
 * declare them before their definitions so strict builds see their ABI. */
extern void __thiscall UIPANEL_WindowProc(void* self, HWND hwnd, UINT msg,
                                          WPARAM wParam, LPARAM lParam);
extern LRESULT __fastcall UIPANEL_OnDestroy(void* self);
extern HDC __fastcall UIPANEL_BeginPaint(void* self);
extern void __fastcall UIPANEL_EndPaint(void* self);
extern void __fastcall UIPANEL_CreateSurface(void* surface);                 /* 0x42A110 */
extern void* __thiscall UIPANEL_CreateSprite(void* panel, void* entry);           /* creates a sprite from file entry */
extern void __fastcall UIPANEL_LockSurface(void* surface);                   /* 0x42A370 */
/* Real def: ui/UIPANEL_Surface.cpp, bool(void*,uint32_t,uint32_t,int32_t,
 * uint32_t,void*,uint32_t,uint32_t,int32_t,uint32_t,uint32_t) — was declared
 * `int` for most params, which doesn't match the real mixed uint32_t/int32_t
 * shape (call-0 landmine). */
extern bool __thiscall UIPANEL_Blit(void* tile_map, uint32_t src_x, uint32_t src_y,
    int32_t dest_x, uint32_t dest_y, void* dest_surface, uint32_t clip_x, uint32_t clip_y,
    int32_t clip_w, uint32_t clip_h, uint32_t flags);                                 /* 0x42B050 */
extern int __thiscall GameObject_BaseCtor(void* self, int a, int b,
    int c, int d);                                                           /* 0x405790 */
extern void __thiscall GameObject_DtorBody(void* self);                      /* 0x405870 */
extern void __thiscall Panel_DtorBody(void* self);                           /* 0x4545A0 */
/* Address corrected: 0x449650 disassembles to RESDATA_ScriptedObject_Start,
 * not RESDATA_CreateChildSprite; confirmed via Ghidra at 0x4546D0. */
extern void* __thiscall RESDATA_CreateChildSprite(void* parent,
    void* resource, int x, int y);                                           /* 0x4546D0 */
// REMOVED: duplicate ResourceManager_GetById (see Panel.h)
/* UI_IsBitmapReady(int) already declared (C++ linkage) via Panel.h, included
 * above through UIPANEL.h; that's the symbol game/Panel.cpp's own call site
 * binds to (shared/stubs_impl.cpp's stub). Re-declaring it here with a void*
 * param mangled to a distinct, unlinked symbol — the actual landmine. */
extern void __thiscall RESDATA_SoundObject_Init(void* sprite, const char* str); /* 0x44CA90 */

/* Forward declarations for functions defined later in this file */
extern void __thiscall UIPANEL_Render(void* self, uint8_t enable_tile_map);
extern void __thiscall UIPANEL_EndPaintEx(void* self, int hdc, int unlock_param, uint8_t unlock_flag, RECT* restrict_rect);

/* External functions referenced from UIPANEL drawing */
class InputMgr;
extern void __fastcall UIPANEL_DrawEditField(int param_1);                  /* 0x429490 */
extern void INPUT_SaveCurrentWorld(InputMgr* input, const char* name); /* 0x41D9B0 */
extern void __thiscall RESDATA_GameObject_UpdateAnimation(void* obj);        /* 0x44B810 */
extern void __fastcall PlaySound(int sound_id);                              /* 0x44A290 */
extern void* __thiscall RESDATA_SoundObject_GetState(int sprite);            /* 0x44CAC0? */
extern int __thiscall RESDATA_SoundObject_GetTextLength(int sprite);        /* 0x44CAE0? */

/* Global variables */
extern void*  g_resource_mgr;            /* 0x4B375C (ResourceManager) */
extern char   g_empty_string[];          /* 0x476934 */
extern void*  g_scripted_object;         /* 0x4AA5B8 */
extern void*  g_active_panel;            /* TBD */
extern int    g_world_width;             /* TBD */
class InputMgr;
extern InputMgr g_input_mgr;             /* 0x4A9990 — static InputMgr object */

/* ================================================================ */
/* BUG NOTE: The UIPANEL destructor and ClearChildren both call      */
/* vtable[6] dispatch on self, but the vtable has already been set   */
/* to VTBL_UIPANEL -- this is the MSVC "reset to most-derived vtable" */
/* pattern for partial-destruction safety.                           */
/* ================================================================ */

/* ================================================================== */
/* UIPANEL constructor                                                 */
/* Address: 0x427370                                                   */
/*                                                                     */
/* Builds a scrollable UIPANEL instance. Calls Panel base constructor   */
/* (RESDATA_BaseInit which calls GameObject_BaseCtor), then creates     */
/* an embedded GameObject at +0x3F0 and a UIPANEL_Surface at +0x478.   */
/* Sets vtable to VTBL_UIPANEL (0x477CC8), type=0x0C, and zeroes all  */
/* sprite pointers (+0x4A0..+0x4DC).                                   */
/*                                                                     */
/* Called from: RESDATA_ScriptedObject_Ctor at 0x449488                */
/* ================================================================== */
UIPANEL::UIPANEL()
{
    /* Step 1: Call base Panel constructor (sets vtable to VTBL_PANEL) */
    RESDATA_BaseInit(this);                     /* 0x4544E0 */

    /* Step 2: Create embedded GameObject at +0x3F0 */
    /* This GameObject manages child sprites (sub-panel items) */
    GameObject_BaseCtor(&this->child_sprites,   /* +0x3F0 */
                        -1, -1, 0, 0);          /* 0x405790 */

    /* Step 3: Create embedded offscreen surface at +0x478 */
    UIPANEL_CreateSurface(&this->surface_buf);  /* 0x42A110 */

    /* Step 4: Set final vtable -- UIPANEL */
/* In the binary: sets vtable here. Compiler-managed in natural C++. */

    /* Step 5: Zero the mode word */
    this->mode = 0;                             /* +0x49C */

    /* Step 6: Zero 6 dwords for the content item sprites: +0x4C0..+0x4D7 */
    for (int i = 0; i < 6; i++) {
        this->item_sprites[i] = NULL;
    }

    /* Step 7: Zero all individual sprite pointers */
    this->type = 0x0C;                          /* +0x04 -- set to 12 */
    this->tab_sprites[2] = NULL;                /* +0x4A8 */
    this->list_text_sprite = NULL;              /* +0x4B8 */
    this->list_bg_sprite = NULL;                /* +0x4B4 */
    this->content_bg_sprite = NULL;             /* +0x4B0 */
    this->tab_sprites[1] = NULL;                /* +0x4A4 */
    this->tab_sprites[0] = NULL;                /* +0x4A0 */
    this->sound_btn_sprite = NULL;              /* +0x4BC */
    this->sprite_list_tail = NULL;              /* +0x4DC */
    this->sprite_list_head = NULL;              /* +0x4D8 */

    /* Step 8: Zero misc fields */
    *(uint8_t*)((intptr_t)this + 0xE0) = 0;     /* +0xE0 -- byte flag / string start */
    *(uint8_t*)((intptr_t)this + 0x2EA) = 0;    /* +0x2EA -- byte */
    this->_field_498 = 0;                       /* +0x498 */
}

/* ================================================================== */
/* ================================================================== */
/* UIPANEL destructor (scalar-deleting dtor handled by C++ compiler)   */
/* Address: 0x427460 (dtor body)                                        */
/* Address: 0x427460                                                   */
/*                                                                     */
/* Full destructor: destroys sprite linked list, dispatches cleanup     */
/* vtable[6] on self and embedded GameObject, calls Panel base cleanup, */
/* locks surface, destroys embedded GameObject, calls Panel::DtorBody. */
/* ================================================================== */
UIPANEL::~UIPANEL()
{
    /* Reset vtable for partial-destruction safety */
/* In the binary: sets vtable here. Compiler-managed in natural C++. */

    /* Walk the linked list at +0x4D8 and destroy every sprite */
    void* current = this->sprite_list_head;          /* +0x4D8 */
    while (current != NULL) {
        void* next = *(void**)((intptr_t)current + 0x22C);  /* linked list next ptr */
        this->sprite_list_head = next;                /* +0x4D8 */

        if (current != NULL) {
            /* Call vtable[0] (scalar deleting destructor) with flags=1 */
            using DtorFunc = void* (__thiscall*)(void* self, byte flags);
            void** vt = *(void***)current;
            DtorFunc dtor = (DtorFunc)vt[0];
            dtor(current, 1);
        }

        current = this->sprite_list_head;
    }

    /* Dispatch cleanup on self */
    this->Init(0, -1, 0);

    /* Dispatch cleanup on embedded GameObject at +0x3F0 */
    this->child_sprites.InitBase(0, -1, false);

    /* Call Panel base destructor (releases child surface + tooltip) */
    RESDATA_DtorBase(this);                         /* 0x454630 */

    /* Lock the offscreen surface before destroying sub-objects */
    UIPANEL_LockSurface(&this->surface_buf);        /* +0x478, 0x42A370 */

    /* Destroy the embedded GameObject at +0x3F0 */
    GameObject_DtorBody(&this->child_sprites);      /* 0x405870 */

    /* Call Panel destructor body (calls GameObject_DtorBody on this) */
    Panel_DtorBody(this);                           /* 0x4545A0 */
}

/* ================================================================== */
/* UIPANEL::ClearChildren                                              */
/* Address: 0x427520                                                   */
/*                                                                     */
/* Destructively clears all child sprites in the linked list. Used     */
/* during reinitialization (e.g., tab switches). Pattern is identical  */
/* to the linked-list walk in the destructor.                          */
/* ================================================================== */
void UIPANEL::ClearChildren()
{
    /* Walk the linked list at +0x4D8 and destroy every sprite */
    void* current = this->sprite_list_head;          /* +0x4D8 */
    while (current != NULL) {
        void* next = *(void**)((intptr_t)current + 0x22C);  /* linked list next ptr */
        this->sprite_list_head = next;                /* +0x4D8 */

        if (current != NULL) {
            using DtorFunc = void* (__thiscall*)(void* self, byte flags);
            void** vt = *(void***)current;
            DtorFunc dtor = (DtorFunc)vt[0];
            dtor(current, 1);
        }

        current = this->sprite_list_head;
    }

    /* Dispatch cleanup on self */
    this->Init(0, -1, 0);

    /* Dispatch cleanup on embedded GameObject */
    this->child_sprites.InitBase(0, -1, false);

    /* Call Panel base destructor */
    RESDATA_DtorBase(this);                         /* 0x454630 */
}

/* ================================================================== */
/* UIPANEL::InitSprites                                                */
/* Address: 0x427580                                                   */
/*                                                                     */
/* Creates child sprites from resources 0x2C00..0x2C13. Guarded by     */
/* child_surface (+0xD0) flag -- returns early if already initialized.  */
/*                                                                     */
/* Resource mapping:                                                   */
/*   0x2C00: Panel resource (used for Init check)                      */
/*   0x2C01: Sub-sprites resource                                      */
/*   0x2C02 -> tab_sprites[0] (Buildings tab)                          */
/*   0x2C03 -> tab_sprites[1] (People tab)                             */
/*   0x2C04 -> content_bg_sprite (+ viewport update at +0xDC)         */
/*   0x2C05 -> tab_sprites[2] (Vehicles tab)                           */
/*   0x2C07 -> list_bg_sprite                                          */
/*   0x2C08 -> list_text_sprite                                        */
/*   0x2C09 -> sound_btn_sprite + 6 content item_sprites               */
/*   0x2C0C -> tab_sprites[3] (Scenery tab)                            */
/*                                                                     */
/* Called by: RESDATA_ScriptedObject_Start (once, on panel activation) */
/* ================================================================== */
byte UIPANEL::InitSprites()
{
    /* Check Init success for self */
    if (this->Init(0x2C00, -1, 0) == 0) {
        return 0;
    }

    /* Check Init success for embedded GameObject at +0x3F0 */
    int result = this->child_sprites.InitBase(0x2C01, -1, false);

    if ((byte)result == 0) {
        return 0;
    }

    /* Guard: if child_surface (+0xD0) is non-NULL, already initialized */
    if (this->child_surface != NULL) {          /* +0xD0 */
        return 1;
    }

    /* Reset sprite pointers before (re)creation */
    this->tab_sprites[0] = NULL;                /* +0x4A0 */
    this->tab_sprites[1] = NULL;                /* +0x4A4 */
    this->content_bg_sprite = NULL;             /* +0x4B0 */
    this->tab_sprites[2] = NULL;                /* +0x4A8 */

    /* Walk resources 0x2C00..0x2C13 and create child sprites */
    for (int res_id = 0x2C00; res_id <= 0x2C13; res_id++) {
        void* resource = ResourceManager_GetById(&g_resource_mgr, res_id);
        if (resource == NULL) {
            continue;
        }

        /* Check if bitmap resource is ready */
        int isReady = UI_IsBitmapReady((int)(intptr_t)(resource));
        if ((char)isReady == 0) {
            continue;
        }

        /* Dispatch based on resource type ID (at +0x04 of resource struct) */
        int typeId = *(int*)((intptr_t)resource + 4);

        switch (typeId) {
        case 0x2C02:
            /* Tab 0 (Buildings) */
            this->tab_sprites[0] = RESDATA_CreateChildSprite(this, resource, 0, 0);
            break;

        case 0x2C03:
            /* Tab 1 (People) */
            this->tab_sprites[1] = RESDATA_CreateChildSprite(this, resource, 0, 0);
            break;

        case 0x2C04:
            /* Content background; also stored at +0xDC for viewport right */
            this->content_bg_sprite = (void*)RESDATA_CreateChildSprite(this, resource, 0, 0);
            *(int*)((intptr_t)this + 0xDC) = (int)(intptr_t)this->content_bg_sprite;
            break;

        case 0x2C05:
            /* Tab 2 (Vehicles) */
            this->tab_sprites[2] = RESDATA_CreateChildSprite(this, resource, 0, 0);
            break;

        case 0x2C07:
            /* List background (scrollbar track) */
            this->list_bg_sprite = RESDATA_CreateChildSprite(this, resource, 0, 0);
            break;

        case 0x2C08:
            /* List text (scrollbar thumb) */
            this->list_text_sprite = RESDATA_CreateChildSprite(this, resource, 0, 0);
            break;

        case 0x2C09:
            /* Sound button -- creates sound_btn + 6 content item sprites */
            this->sound_btn_sprite = RESDATA_CreateChildSprite(this, resource, 0, 10);

            if (this->sound_btn_sprite != NULL) {
                /* SetFrame on sound button sprite: vtable[3](x, y) */
                {
                    void** btnVtable = *(void***)this->sound_btn_sprite;
                    using SetFrameFunc = void (__thiscall*)(void* self, int a, int b);
                    SetFrameFunc setFrame = (SetFrameFunc)(btnVtable[3]);
                    setFrame(this->sound_btn_sprite, 0x0B, 0x82);
                }

                /* Set visibility flag at +0x58 */
                *(uint8_t*)((intptr_t)this->sound_btn_sprite + 0x58) = 1;

                RESDATA_SoundObject_Init(this->sound_btn_sprite, g_empty_string);

                /* Create 6 content item sprites at increasing X offsets */
                int x_offset = 0;
                for (int i = 0; i < 6; i++) {
                    this->item_sprites[i] = RESDATA_CreateChildSprite(this, resource, 0, 10);

                    if (this->item_sprites[i] != NULL) {
                        void** itemVtable = *(void***)this->item_sprites[i];
                        using SetFrameFunc = void (__thiscall*)(void* self, int a, int b);
                        SetFrameFunc setFrame = (SetFrameFunc)(itemVtable[3]);

                        /* SetFrame with width/height offset by x_offset */
                        setFrame(this->item_sprites[i],
                                 *(int*)((intptr_t)this->item_sprites[i] + 8),   /* width */
                                 *(int*)((intptr_t)this->item_sprites[i] + 12) + x_offset); /* height + offset */
                    }
                    x_offset += 0x19;   /* 25 pixels between items */
                }
            }
            break;

        case 0x2C0C:
            /* Tab 3 (Scenery/Signals) */
            this->tab_sprites[3] = RESDATA_CreateChildSprite(this, resource, 0, 0);
            break;

        default:
            /* All other resources: create as generic child sprites */
            RESDATA_CreateChildSprite(this, resource, 0, 0);
            break;
        }
    }

    return 1;
}

/* ================================================================== */
/* 2-byte-aligned string compare (as in original binary)               */
/*                                                                     */
/* The original assembly compares strings 2 bytes at a time by         */
/* checking both bytes before advancing (unrolled strcmp).             */
/* Returns: 0 if equal, 1 if str1 > str2, -1 if str1 < str2           */
/* ================================================================== */
static int UIPANEL_StrCmp2Byte(const char* str1, const char* str2)
{
    do {
        if (*str2 != *str1) {
            return (*str2 < *str1) ? -1 : 1;
        }
        if (*str1 == 0) break;
        if (str2[1] != str1[1]) {
            return (str2[1] < str1[1]) ? -1 : 1;
        }
        str1 += 2;
        str2 += 2;
    } while (*str1 != 0);
    return 0;
}

/* ================================================================== */
/* UIPANEL::HandleDrag -- Tab selection state machine                   */
/* Address: 0x4277D0                                                   */
/*                                                                     */
/* Dispatches tab actions 0-5:                                          */
/*   0: Close/reset                                                     */
/*   1: Init all tabs (shows tabs 0,1,2 visible, tab 3 hidden)         */
/*   2-5: Select individual tab (Buildings, People, Vehicles, Scenery)  */
/*                                                                     */
/* Each tab case (2-5) populates the 6-item content viewport, then     */
/* performs scroll-to-target logic: compares the currently-selected    */
/* item's name with the first visible item using 2-byte-aligned string */
/* comparison, and scrolls forward/backward until the selection is     */
/* visible. Falls through to edge-scrolling check after tab setup.     */
/*                                                                     */
/* Key per-case differences in the original binary:                    */
/*   Case 2: sets g_active_panel, uses tab[0], stack-var init          */
/*   Case 3: NO g_active_panel set, uses tab[1], stack-var init        */
/*   Case 4: NO g_active_panel set, uses tab[2], g_empty_string init   */
/*   Case 5: sets g_active_panel, uses tab[3], calls DrawEditField     */
/* ================================================================== */
byte UIPANEL::HandleDrag(int resource, uint16_t action)
{
    /* The resource parameter becomes the viewport left (+0xD4) */
    *(int*)((intptr_t)this + 0xD4) = resource;

    switch (action) {
    case 0:
        /* Close/reset -- stop sound, clear active panel */
        this->StopSound(0);

        *(uint8_t*)((intptr_t)this + 0x88) = 0;                /* +0x88 -- flag cleared */
        g_active_panel = &g_scripted_object;                     /* Back to main panel */
        RESDATA_GameObject_UpdateAnimation((void*)0x4AA5B8);    /* 0x44B810 */
        this->mode = action;                                    /* +0x49C */
        break;

    case 1:
        /* Init all tabs -- setup for tab 1 (default) */
        *(uint8_t*)((intptr_t)this + 0x88) = 1;                /* +0x88 flag = active */
        g_active_panel = this;
        this->mode = action;

        /* Play sound for panel open (sound_id 0x502d) */
        PlaySound(0x502d);

        /* Verify Init succeeds on self and embedded GO */
        if (this->Init(0x2C00, -1, 0) == 0) {
            return 0;
        }
        if ((byte)this->child_sprites.InitBase(0x2C01, -1, false) == 0) {
                return 0;
            }

        /* Position tab buttons and content area via vtable[3] (SetFrame) */
        if (this->tab_sprites[0]) {
            void** vt = *(void***)this->tab_sprites[0];
            using SetFrameFunc = void (__thiscall*)(void* self, int x);
            SetFrameFunc sf = (SetFrameFunc)(vt[3]);
            sf(this->tab_sprites[0], 0x14);
        }
        if (this->tab_sprites[1]) {
            void** vt = *(void***)this->tab_sprites[1];
            using SetFrameFunc = void (__thiscall*)(void* self, int x, int y);
            SetFrameFunc sf = (SetFrameFunc)(vt[3]);
            sf(this->tab_sprites[1], 0xE9, 0x21);
        }
        if (this->tab_sprites[2]) {
            void** vt = *(void***)this->tab_sprites[2];
            using SetFrameFunc = void (__thiscall*)(void* self, int x, int y);
            SetFrameFunc sf = (SetFrameFunc)(vt[3]);
            sf(this->tab_sprites[2], 0x14, 0x92);
        }
        if (this->content_bg_sprite) {
            void** vt = *(void***)this->content_bg_sprite;
            using SetFrameFunc = void (__thiscall*)(void* self, int x, int y);
            SetFrameFunc sf = (SetFrameFunc)(vt[3]);
            sf(this->content_bg_sprite, 0xE9, 0x92);
        }

        /* Active content = content_bg_sprite (+0x4B0), NOT tab_sprites[3] */
        /* BUG FIX: original had this->tab_sprites[3] here */
        *(void**)((intptr_t)this + 0xD8) = this->content_bg_sprite;    /* +0x4B0 */

        /* Set visibility: tabs 0,1,2 visible; tab 3 and sound button hidden */
        if (this->tab_sprites[1]) *(uint8_t*)((intptr_t)this->tab_sprites[1] + 0x56) = 1;
        if (this->tab_sprites[0]) *(uint8_t*)((intptr_t)this->tab_sprites[0] + 0x56) = 1;
        if (this->tab_sprites[2]) *(uint8_t*)((intptr_t)this->tab_sprites[2] + 0x56) = 1;
        if (this->tab_sprites[3]) *(uint8_t*)((intptr_t)this->tab_sprites[3] + 0x56) = 0;
        if (this->sound_btn_sprite) *(uint8_t*)((intptr_t)this->sound_btn_sprite + 0x56) = 0;

        /* Draw edit field and save current world */
        UIPANEL_DrawEditField((intptr_t)this);
        INPUT_SaveCurrentWorld(&g_input_mgr, "curr");
        break;

    case 2:
        /* === Buildings tab === */
        *(uint8_t*)((intptr_t)this + 0x88) = 1;
        g_active_panel = this;
        this->mode = action;

        /* Verify Init on self */
        if (this->Init(0x2C00, -1, 0) == 0) {
            return 0;
        }

        /* Active = tab_sprites[0] (Buildings), hide all other tabs */
        *(void**)((intptr_t)this + 0xD8) = this->tab_sprites[0];    /* +0x4A0 */
        if (this->tab_sprites[1]) *(uint8_t*)((intptr_t)this->tab_sprites[1] + 0x56) = 0;
        if (this->tab_sprites[0]) *(uint8_t*)((intptr_t)this->tab_sprites[0] + 0x56) = 1;
        if (this->tab_sprites[2]) *(uint8_t*)((intptr_t)this->tab_sprites[2] + 0x56) = 0;
        if (this->tab_sprites[3]) *(uint8_t*)((intptr_t)this->tab_sprites[3] + 0x56) = 0;

        /* SetFrame and redraw selected tab */
        if (this->tab_sprites[0]) {
            void** vt = *(void***)this->tab_sprites[0];
            using SetFrameFunc = void (__thiscall*)(void* self, int x);
            SetFrameFunc sf = (SetFrameFunc)(vt[3]);
            sf(this->tab_sprites[0], 10);
            using DrawFunc = void (__thiscall*)(void* self);
            DrawFunc dr = (DrawFunc)(vt[0x20/4]);
            dr(this->tab_sprites[0]);
        }

        /* Position content area background */
        if (this->content_bg_sprite) {
            void** vt = *(void***)this->content_bg_sprite;
            using SetFrameFunc = void (__thiscall*)(void* self, int x, int y);
            SetFrameFunc sf = (SetFrameFunc)(vt[3]);
            sf(this->content_bg_sprite, 0x51, 0x9D);
        }

        /* Populate 6-item viewport from sprite list */
        UIPANEL_CreateSprite(this, *(void**)((intptr_t)this + 0x4D8));

        /* Init sound button with stack buffer (original: CRT_strncat_s on +0xE0 then stack-var) */
        RESDATA_SoundObject_Init(this->sound_btn_sprite, g_empty_string);
        if (this->sound_btn_sprite) {
            *(uint8_t*)((intptr_t)this->sound_btn_sprite + 0x56) = 1;
            void** vt = *(void***)this->sound_btn_sprite;
            using DrawFunc = void (__thiscall*)(void* self);
            DrawFunc dr = (DrawFunc)(vt[0x20/4]);
            dr(this->sound_btn_sprite);
        }

        /* Scroll-to-target: compare sound_btn name with first visible item */
        {
            char* cur_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4BC));
            char* first_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4C0));
            int cmp = UIPANEL_StrCmp2Byte(cur_state, first_state);

            /* Forward scroll: if selection > first visible, scroll forward */
            while (cmp > 0) {
                void* tail = *(void**)((intptr_t)this + 0x4DC);    /* sprite_list_tail */
                if (tail == NULL) break;
                void* next = *(void**)((intptr_t)tail + 0x228);    /* next link */
                if (next == NULL) break;
                UIPANEL_CreateSprite(this, next);
                cur_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4BC));
                first_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4C0));
                cmp = UIPANEL_StrCmp2Byte(cur_state, first_state);
            }

            /* Backward scroll: if selection < first visible, scroll backward */
            while (cmp < 0) {
                int text_len = RESDATA_SoundObject_GetTextLength(*(int*)((intptr_t)this + 0x4D4));
                if (text_len == 0) break;
                void* tail = *(void**)((intptr_t)this + 0x4DC);    /* sprite_list_tail */
                if (tail == NULL) break;
                void* prev = *(void**)((intptr_t)tail + 0x22C);    /* prev link */
                if (prev == NULL) break;
                UIPANEL_CreateSprite(this, prev);
                cur_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4BC));
                first_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4C0));
                cmp = UIPANEL_StrCmp2Byte(cur_state, first_state);
            }
        }
        break;

    case 3:
        /* === People tab === */
        *(uint8_t*)((intptr_t)this + 0x88) = 1;
        /* NOTE: case 3 does NOT set g_active_panel (per original binary) */
        this->mode = action;

        /* Verify Init on self */
        if (this->Init(0x2C00, -1, 0) == 0) {
            return 0;
        }

        /* Active = tab_sprites[1] (People), hide all other tabs */
        *(void**)((intptr_t)this + 0xD8) = this->tab_sprites[1];    /* +0x4A4 */
        if (this->tab_sprites[0]) *(uint8_t*)((intptr_t)this->tab_sprites[0] + 0x56) = 0;
        if (this->tab_sprites[1]) *(uint8_t*)((intptr_t)this->tab_sprites[1] + 0x56) = 1;
        if (this->tab_sprites[2]) *(uint8_t*)((intptr_t)this->tab_sprites[2] + 0x56) = 0;
        if (this->tab_sprites[3]) *(uint8_t*)((intptr_t)this->tab_sprites[3] + 0x56) = 0;

        /* SetFrame and redraw selected tab */
        if (this->tab_sprites[1]) {
            void** vt = *(void***)this->tab_sprites[1];
            using SetFrameFunc = void (__thiscall*)(void* self, int x);
            SetFrameFunc sf = (SetFrameFunc)(vt[3]);
            sf(this->tab_sprites[1], 10);
            using DrawFunc = void (__thiscall*)(void* self);
            DrawFunc dr = (DrawFunc)(vt[0x20/4]);
            dr(this->tab_sprites[1]);
        }

        /* Position content area background */
        if (this->content_bg_sprite) {
            void** vt = *(void***)this->content_bg_sprite;
            using SetFrameFunc = void (__thiscall*)(void* self, int x, int y);
            SetFrameFunc sf = (SetFrameFunc)(vt[3]);
            sf(this->content_bg_sprite, 0x51, 0x9D);
        }

        /* Populate 6-item viewport from sprite list */
        UIPANEL_CreateSprite(this, *(void**)((intptr_t)this + 0x4D8));

        /* Init sound button with stack buffer (original: CRT_strncat_s on +0xE0 then stack-var) */
        RESDATA_SoundObject_Init(this->sound_btn_sprite, g_empty_string);
        if (this->sound_btn_sprite) {
            *(uint8_t*)((intptr_t)this->sound_btn_sprite + 0x56) = 1;
            void** vt = *(void***)this->sound_btn_sprite;
            using DrawFunc = void (__thiscall*)(void* self);
            DrawFunc dr = (DrawFunc)(vt[0x20/4]);
            dr(this->sound_btn_sprite);
        }

        /* Scroll-to-target */
        {
            char* cur_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4BC));
            char* first_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4C0));
            int cmp = UIPANEL_StrCmp2Byte(cur_state, first_state);

            while (cmp > 0) {
                void* tail = *(void**)((intptr_t)this + 0x4DC);
                if (tail == NULL) break;
                void* next = *(void**)((intptr_t)tail + 0x228);
                if (next == NULL) break;
                UIPANEL_CreateSprite(this, next);
                cur_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4BC));
                first_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4C0));
                cmp = UIPANEL_StrCmp2Byte(cur_state, first_state);
            }

            while (cmp < 0) {
                int text_len = RESDATA_SoundObject_GetTextLength(*(int*)((intptr_t)this + 0x4D4));
                if (text_len == 0) break;
                void* tail = *(void**)((intptr_t)this + 0x4DC);
                if (tail == NULL) break;
                void* prev = *(void**)((intptr_t)tail + 0x22C);
                if (prev == NULL) break;
                UIPANEL_CreateSprite(this, prev);
                cur_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4BC));
                first_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4C0));
                cmp = UIPANEL_StrCmp2Byte(cur_state, first_state);
            }
        }
        break;

    case 4:
        /* === Vehicles tab === */
        *(uint8_t*)((intptr_t)this + 0x88) = 1;
        /* NOTE: case 4 does NOT set g_active_panel (per original binary) */
        this->mode = action;

        /* Verify Init on self */
        if (this->Init(0x2C00, -1, 0) == 0) {
            return 0;
        }

        /* Active = tab_sprites[2] (Vehicles), hide all other tabs */
        *(void**)((intptr_t)this + 0xD8) = this->tab_sprites[2];    /* +0x4A8 */
        if (this->tab_sprites[1]) *(uint8_t*)((intptr_t)this->tab_sprites[1] + 0x56) = 0;
        if (this->tab_sprites[0]) *(uint8_t*)((intptr_t)this->tab_sprites[0] + 0x56) = 0;
        if (this->tab_sprites[2]) *(uint8_t*)((intptr_t)this->tab_sprites[2] + 0x56) = 1;
        if (this->tab_sprites[3]) *(uint8_t*)((intptr_t)this->tab_sprites[3] + 0x56) = 0;

        /* SetFrame and redraw selected tab */
        if (this->tab_sprites[2]) {
            void** vt = *(void***)this->tab_sprites[2];
            using SetFrameFunc = void (__thiscall*)(void* self, int x);
            SetFrameFunc sf = (SetFrameFunc)(vt[3]);
            sf(this->tab_sprites[2], 10);
            using DrawFunc = void (__thiscall*)(void* self);
            DrawFunc dr = (DrawFunc)(vt[0x20/4]);
            dr(this->tab_sprites[2]);
        }

        /* Position content area background */
        if (this->content_bg_sprite) {
            void** vt = *(void***)this->content_bg_sprite;
            using SetFrameFunc = void (__thiscall*)(void* self, int x, int y);
            SetFrameFunc sf = (SetFrameFunc)(vt[3]);
            sf(this->content_bg_sprite, 0x51, 0x9D);
        }

        /* Populate 6-item viewport from sprite list */
        UIPANEL_CreateSprite(this, *(void**)((intptr_t)this + 0x4D8));

        /* Init sound button with empty string (case 4 uses g_empty_string directly) */
        RESDATA_SoundObject_Init(this->sound_btn_sprite, g_empty_string);
        if (this->sound_btn_sprite) {
            *(uint8_t*)((intptr_t)this->sound_btn_sprite + 0x56) = 1;
            void** vt = *(void***)this->sound_btn_sprite;
            using DrawFunc = void (__thiscall*)(void* self);
            DrawFunc dr = (DrawFunc)(vt[0x20/4]);
            dr(this->sound_btn_sprite);
        }

        /* Scroll-to-target */
        {
            char* cur_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4BC));
            char* first_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4C0));
            int cmp = UIPANEL_StrCmp2Byte(cur_state, first_state);

            while (cmp > 0) {
                void* tail = *(void**)((intptr_t)this + 0x4DC);
                if (tail == NULL) break;
                void* next = *(void**)((intptr_t)tail + 0x228);
                if (next == NULL) break;
                UIPANEL_CreateSprite(this, next);
                cur_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4BC));
                first_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4C0));
                cmp = UIPANEL_StrCmp2Byte(cur_state, first_state);
            }

            while (cmp < 0) {
                int text_len = RESDATA_SoundObject_GetTextLength(*(int*)((intptr_t)this + 0x4D4));
                if (text_len == 0) break;
                void* tail = *(void**)((intptr_t)this + 0x4DC);
                if (tail == NULL) break;
                void* prev = *(void**)((intptr_t)tail + 0x22C);
                if (prev == NULL) break;
                UIPANEL_CreateSprite(this, prev);
                cur_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4BC));
                first_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4C0));
                cmp = UIPANEL_StrCmp2Byte(cur_state, first_state);
            }
        }
        break;

    case 5:
        /* === Scenery/Signals tab === */
        *(uint8_t*)((intptr_t)this + 0x88) = 1;
        g_active_panel = this;
        this->mode = action;

        /* Verify Init on self */
        if (this->Init(0x2C00, -1, 0) == 0) {
            return 0;
        }

        /* Active = tab_sprites[3] (Scenery), hide all other tabs */
        *(void**)((intptr_t)this + 0xD8) = this->tab_sprites[3];    /* +0x4AC */
        if (this->tab_sprites[1]) *(uint8_t*)((intptr_t)this->tab_sprites[1] + 0x56) = 0;
        if (this->tab_sprites[0]) *(uint8_t*)((intptr_t)this->tab_sprites[0] + 0x56) = 0;
        if (this->tab_sprites[2]) *(uint8_t*)((intptr_t)this->tab_sprites[2] + 0x56) = 0;
        if (this->tab_sprites[3]) *(uint8_t*)((intptr_t)this->tab_sprites[3] + 0x56) = 1;

        /* SetFrame and redraw selected tab */
        if (this->tab_sprites[3]) {
            void** vt = *(void***)this->tab_sprites[3];
            using SetFrameFunc = void (__thiscall*)(void* self, int x);
            SetFrameFunc sf = (SetFrameFunc)(vt[3]);
            sf(this->tab_sprites[3], 10);
            using DrawFunc = void (__thiscall*)(void* self);
            DrawFunc dr = (DrawFunc)(vt[0x20/4]);
            dr(this->tab_sprites[3]);
        }

        /* Position content area background */
        if (this->content_bg_sprite) {
            void** vt = *(void***)this->content_bg_sprite;
            using SetFrameFunc = void (__thiscall*)(void* self, int x, int y);
            SetFrameFunc sf = (SetFrameFunc)(vt[3]);
            sf(this->content_bg_sprite, 0x51, 0x9D);
        }

        /* Draw edit field (scenery/signals has backdrop file picker) */
        UIPANEL_DrawEditField((intptr_t)this);

        /* Populate 6-item viewport from sprite list */
        UIPANEL_CreateSprite(this, *(void**)((intptr_t)this + 0x4D8));

        /* Init sound button with empty string */
        RESDATA_SoundObject_Init(this->sound_btn_sprite, g_empty_string);
        if (this->sound_btn_sprite) {
            *(uint8_t*)((intptr_t)this->sound_btn_sprite + 0x56) = 1;
            void** vt = *(void***)this->sound_btn_sprite;
            using DrawFunc = void (__thiscall*)(void* self);
            DrawFunc dr = (DrawFunc)(vt[0x20/4]);
            dr(this->sound_btn_sprite);
        }

        /* Scroll-to-target */
        {
            char* cur_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4BC));
            char* first_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4C0));
            int cmp = UIPANEL_StrCmp2Byte(cur_state, first_state);

            while (cmp > 0) {
                void* tail = *(void**)((intptr_t)this + 0x4DC);
                if (tail == NULL) break;
                void* next = *(void**)((intptr_t)tail + 0x228);
                if (next == NULL) break;
                UIPANEL_CreateSprite(this, next);
                cur_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4BC));
                first_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4C0));
                cmp = UIPANEL_StrCmp2Byte(cur_state, first_state);
            }

            while (cmp < 0) {
                int text_len = RESDATA_SoundObject_GetTextLength(*(int*)((intptr_t)this + 0x4D4));
                if (text_len == 0) break;
                void* tail = *(void**)((intptr_t)this + 0x4DC);
                if (tail == NULL) break;
                void* prev = *(void**)((intptr_t)tail + 0x22C);
                if (prev == NULL) break;
                UIPANEL_CreateSprite(this, prev);
                cur_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4BC));
                first_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4C0));
                cmp = UIPANEL_StrCmp2Byte(cur_state, first_state);
            }
        }
        break;
    }

    /* ================================================================ */
    /* Post-switch: edge scrolling check                                 */
    /* ================================================================ */
    if (*(uint8_t*)((intptr_t)this + 0x88) == 1) {
        int cursor_y = *(int*)((intptr_t)this + 0x38);   /* +0x38 = cursor_y */
        if (g_world_width - cursor_y < *(int*)0x4AA5C8) {
            *(uint8_t*)((intptr_t)this + 0xAD) = 1;       /* edge scroll flag */
            return 1;
        }
        *(uint8_t*)((intptr_t)this + 0xAD) = 0;
    }
    return 1;
}

/* ================================================================== */
/* UIPANEL::WindowProc                                                 */
/* Address: 0x426900                                                   */
/*                                                                     */
/* __thiscall (ECX = this). Checks if hwnd matches this->hwnd (+0x08), */
/* and if so performs a render cycle: unlocks primary, calls Render(1), */
/* re-locks primary. Always forwards to DefWindowProcA.                 */
/*                                                                     */
/* Note: This is called from the WndProc dispatch table at 0x477C80    */
/* with `this` already in ECX and the standard 4 WndProc params on     */
/* the stack. The RET 0x10 at the end indicates __thiscall with 4      */
/* stack parameters popped by the callee.                              */
/* ================================================================== */
void __thiscall UIPANEL_WindowProc(void* self,
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (hwnd == *(HWND*)((intptr_t)self + 8)) {
        DDRAW_UnlockPrimary();
        UIPANEL_Render(self, 1);
        DDRAW_UnlockPrimary();
    }
    DefWindowProcA(hwnd, msg, wParam, lParam);
}

/* ================================================================== */
/* UIPANEL::OnDestroy                                                  */
/* Address: 0x426A90                                                   */
/*                                                                     */
/* Handles panel window destruction. Clears the alive flag (+0xAB),    */
/* destroys the HWND, and if no child windows remain (+0x0C), posts    */
/* WM_QUIT. Returns 0.                                                 */
/* ================================================================== */
LRESULT __fastcall UIPANEL_OnDestroy(void* self)
{
    *(uint8_t*)((intptr_t)self + 0xAB) = 0;                /* +0xAB -- alive flag */
    DestroyWindow(*(HWND*)((intptr_t)self + 8));             /* +0x08 -- hwnd */

    if (*(int*)((intptr_t)self + 0x0C) == 0) {               /* +0x0C -- child count */
        PostQuitMessage(0);
    }

    return 0;
}

/* ================================================================== */
/* UIPANEL::BeginPaint                                                 */
/* Address: 0x426B00                                                   */
/*                                                                     */
/* Begins buffered painting to the offscreen surface. Unlocks primary,  */
/* calls GetDC on primary surface (vtable[0x44]). Retries up to 1000   */
/* times with 10ms delay on failure, exits on persistent failure via   */
/* WIN32_FatalError + ExitProcess. Returns HDC from GetDC stored in    */
/* a PAINTSTRUCT-like struct at +0x4C.                                  */
/* ================================================================== */
HDC __fastcall UIPANEL_BeginPaint(void* self)
{
    int retry = 0;
    /* The PAINTSTRUCT/desc struct is at +0x4C on the UIPANEL object */
    void* desc = (void*)((intptr_t)self + 0x4C);

    DDRAW_UnlockPrimary();

    /* Call GetDC on primary surface via vtable[0x44] */
    using GetDCFunc = HDC (__thiscall*)(void* self, void* desc);
    GetDCFunc getDC = (GetDCFunc)(*(void***)g_primary_surface)[0x44 / 4];
    HDC hdc = getDC(g_primary_surface, desc);

    while (hdc == NULL) {
        retry++;
        if (retry > 1000) {
            WIN32_FatalError();
            ExitProcess(1);
        }
        Sleep(10);
        hdc = getDC(g_primary_surface, desc);
    }

    return hdc;
}

/* ================================================================== */
/* UIPANEL::EndPaint                                                   */
/* Address: 0x426B70                                                   */
/*                                                                     */
/* Simple EndPaint wrapper. Delegates to EndPaintEx with the hwnd      */
/* (from +0x08) as hdc, 0 for unlock_param, 0 for unlock_flag, and    */
/* a stack-local RECT as restrict_rect.                                 */
/* ================================================================== */
void __fastcall UIPANEL_EndPaint(void* self)
{
    RECT stack_rect;
    UIPANEL_EndPaintEx(self,
        *(int*)((intptr_t)self + 8),   /* hwnd as hdc */
        0,                              /* unlock_param = 0 */
        0,                              /* unlock_flag = 0 */
        &stack_rect);                   /* stack RECT as restrict_rect */
}

/* ================================================================== */
/* UIPANEL::EndPaintEx                                                 */
/* Address: 0x426B90                                                   */
/*                                                                     */
/* Main present pipeline:                                                */
/*   1. If unlock_param != 0, unlock primary surface via vtable[0x68]  */
/*   2. If unlock_flag != 0, just DDRAW_UnlockPrimary and return       */
/*   3. DDRAW_UnlockPrimary                                            */
/*   4. Path A: No tile_map or blocking flag set -- simple PresentRect */
/*   5. Path B: Tile_map present -- cursor-relative blit + bg restore   */
/*                                                                     */
/* Parameters: this, hdc (int), unlock_param (int), unlock_flag (byte), */
/*   restrict_rect (RECT*)                                              */
/* ================================================================== */
void __thiscall UIPANEL_EndPaintEx(void* self,
    int hdc, int unlock_param, uint8_t unlock_flag, RECT* restrict_rect)
{
    /* Step 1: Unlock primary surface if unlock_param is non-zero */
    if (unlock_param != 0) {
        using UnlockFunc = HRESULT (__thiscall*)(void* self, void* rect);
        UnlockFunc unlock = (UnlockFunc)(*(void***)g_primary_surface)[0x68 / 4];
        unlock(g_primary_surface, (void*)(uintptr_t)unlock_param);
    }

    /* Step 2: If unlock_flag is non-zero, just unlock and return */
    if (unlock_flag != 0) {
        DDRAW_UnlockPrimary();
        return;
    }

    DDRAW_UnlockPrimary();

    /* Step 3: Path A -- No tile map or blocking flag set */
    void* tile_map = *(void**)((intptr_t)self + 0x14);       /* +0x14 */
    uint8_t blocking = *(uint8_t*)((intptr_t)self + 0x3C);   /* +0x3C */

    if (tile_map == NULL || blocking != 0) {
        if (restrict_rect == NULL) {
            DDRAW_PresentRect(
                (RECT*)((intptr_t)self + 0xD4),       /* viewport rect at +0xD4 */
                *(HWND*)((intptr_t)self + 8),          /* hwnd */
                NULL, 1);
        } else {
            DDRAW_PresentRect(
                restrict_rect,
                *(HWND*)((intptr_t)self + 8),
                NULL, 1);
        }
        DDRAW_UnlockPrimary();
        return;
    }

    /* Step 4: Path B -- Tile map exists, cursor-relative rendering */
    POINT cursor;
    GetCursorPos(&cursor);

    /* Store cursor position */
    *(int*)((intptr_t)self + 0x34) = cursor.x;           /* +0x34 = cursor_x */
    *(int*)((intptr_t)self + 0x38) = cursor.y;           /* +0x38 = cursor_y */

    /* Convert to panel-relative coords */
    int win_x = *(int*)((intptr_t)self + 0x2C);          /* +0x2C = window_x */
    int win_y = *(int*)((intptr_t)self + 0x30);          /* +0x30 = window_y */
    int cursor_rel_x = cursor.x - win_x;
    int cursor_rel_y = cursor.y - win_y;

    /* Get tile dimensions -- default to 0 if no tile_map */
    int tile_w = *(int*)((intptr_t)self + 0x18);          /* +0x18 */
    int tile_h = *(int*)((intptr_t)self + 0x1C);          /* +0x1C */
    if (tile_map == NULL) {
        tile_w = 0;
        tile_h = 0;
    }

    /* Compute dirty rect around cursor */
    RECT dirty_rect;
    dirty_rect.left   = cursor_rel_x;
    dirty_rect.top    = cursor_rel_y;
    dirty_rect.right  = tile_w + cursor_rel_x;
    dirty_rect.bottom = tile_h + cursor_rel_y;

    /* Clip to viewport (+0xD4..+0xE0) */
    int vp_left   = *(int*)((intptr_t)self + 0xD4);
    int vp_top    = *(int*)((intptr_t)self + 0xD8);
    int vp_right  = *(int*)((intptr_t)self + 0xDC);
    int vp_bottom = *(int*)((intptr_t)self + 0xE0);

    int dx = 0, dy = 0;

    if (dirty_rect.right > vp_right) {
        tile_w = vp_right - cursor_rel_x;
        dirty_rect.right = vp_right;
    }
    if (dirty_rect.bottom > vp_bottom) {
        tile_h = vp_bottom - cursor_rel_y;
        dirty_rect.bottom = vp_bottom;
    }
    if (cursor_rel_y < vp_top) {
        dy = vp_top - cursor_rel_y;
        tile_h = dirty_rect.bottom - vp_top;
        dirty_rect.top = vp_top;
    }
    if (cursor_rel_x < vp_left) {
        dx = vp_left - cursor_rel_x;
        tile_w = dirty_rect.right - vp_left;
        dirty_rect.left = vp_left;
    }

    /* Handle scroll offset when more items exist */
    int scroll_offset = 0;
    int item_count = *(int*)((intptr_t)self + 0x20);           /* +0x20 */
    if (item_count > 1) {
        if (item_count <= *(int*)((intptr_t)self + 0x24)) {   /* +0x24 = scroll_pos */
            *(int*)((intptr_t)self + 0x24) = 0;
        }
        scroll_offset = *(int*)((intptr_t)self + 0x18) *       /* +0x18 = tile_width */
                       *(int*)((intptr_t)self + 0x24);        /* +0x24 = scroll_pos */
    }

    /* Handle restrict_rect: intersect dirty_rect and the cached dirty rect with restrict_rect */
    if (restrict_rect != NULL) {
        RECT intersect1, intersect2;
        BOOL hasI1 = IntersectRect(&intersect1, restrict_rect, &dirty_rect);
        BOOL hasI2 = IntersectRect(&intersect2, restrict_rect,
                                   (RECT*)((intptr_t)self + 0x50));

        if (!hasI1 && !hasI2) {
            DDRAW_PresentRect(restrict_rect,
                *(HWND*)((intptr_t)self + 8), NULL, 1);
            DDRAW_UnlockPrimary();
            return;
        }

        RECT union_rect;
        UnionRect(&union_rect, &dirty_rect, restrict_rect);
        if (*(int*)((intptr_t)self + 0x58) != 0) {             /* cached dirty rect has content */
            RECT full_union;
            UnionRect(&full_union, &union_rect, (RECT*)((intptr_t)self + 0x50));
        }
    }

    /* Save current dirty rect */
    *(int*)((intptr_t)self + 0x50) = dirty_rect.left;
    *(int*)((intptr_t)self + 0x54) = dirty_rect.top;
    *(int*)((intptr_t)self + 0x58) = dirty_rect.right;
    *(int*)((intptr_t)self + 0x5C) = dirty_rect.bottom;

    /* Copy background from offscreen surface to primary (prepare dirty area) */
    {
        using BltFunc = void (__thiscall*)(void* dst, RECT* dstRect,
            void* src, RECT* srcRect, uint32_t flags, int unk);
        BltFunc blt = (BltFunc)(*(void***)g_primary_surface)[0x14 / 4];
        blt(g_primary_surface, NULL,
            *(void**)((intptr_t)self + 0x48), /* offscreen surface */
            &dirty_rect,
            DDBLT_WAIT, 0);
    }

    /* Blit tile content onto primary surface using UIPANEL_Blit */
    UIPANEL_Blit(tile_map,
                 cursor_rel_x, tile_h,    /* src_x, src_y */
                 cursor_rel_x, 0,          /* dest_x, dest_y */
                 (void*)g_primary_surface,
                 scroll_offset + dx, 0,    /* clip_left, clip_top */
                 scroll_offset + dx + (dirty_rect.right - dirty_rect.left),
                 tile_h,                   /* clip_right, clip_bottom */
                 0);                       /* flags */

    /* Present the dirty region to the screen */
    if (restrict_rect == NULL) {
        DDRAW_PresentRect(
            (RECT*)((intptr_t)self + 0xD4),   /* viewport */
            *(HWND*)((intptr_t)self + 8),
            NULL, 1);
    } else {
        RECT present_rect;
        UnionRect(&present_rect, &dirty_rect, restrict_rect);
        DDRAW_PresentRect(&present_rect,
            *(HWND*)((intptr_t)self + 8),
            NULL, 1);
    }

    /* Restore background from backbuffer */
    {
        using BltFunc = void (__thiscall*)(void* dst, RECT* dstRect,
            void* src, RECT* srcRect, uint32_t flags, int unk);
        BltFunc blt = (BltFunc)(*(void***)g_primary_surface)[0x14 / 4];
        blt(g_primary_surface,
            (RECT*)((intptr_t)self + 0x50),         /* saved dirty rect */
            *(void**)((intptr_t)self + 0x48),
            &dirty_rect,
            DDBLT_WAIT, 0);
    }

    DDRAW_UnlockPrimary();
}

/* ================================================================== */
/* UIPANEL::Render                                                     */
/* Address: 0x426EB0                                                   */
/*                                                                     */
/* Per-frame foreground render with cursor overlay.                    */
/*   1. Checks visibility flag (+0x44), returns if hidden              */
/*   2. Computes dirty rect from cursor position, clips to viewport    */
/*   3. Inflates dirty rect by 4px when <256x256 for smooth cursor     */
/*   4. Restores background from backbuffer for cached dirty rect      */
/*   5. Blits tile content from offscreen surface to primary surface   */
/*   6. Updates cursor tracking values to -1 (reset)                   */
/*                                                                     */
/* @param enable_tile_map  byte -- if non-zero and tile_map exists,    */
/*                          render tile content into dirty area        */
/* ================================================================== */
void __thiscall UIPANEL_Render(void* self, uint8_t enable_tile_map)
{
    int dx = 0, dy = 0;
    uint8_t inflate_flag = 0;

    /* Step 1: Check visibility */
    if (*(uint8_t*)((intptr_t)self + 0x44) == 0) {     /* +0x44 = visible */
        return;
    }

    /* Step 2: Get cursor position, convert to panel-relative */
    POINT cursor;
    GetCursorPos(&cursor);

    int win_x = *(int*)((intptr_t)self + 0x2C);         /* +0x2C = window_x */
    int win_y = *(int*)((intptr_t)self + 0x30);         /* +0x30 = window_y */
    int cursor_x = cursor.x - win_x;
    int cursor_y = cursor.y - win_y;

    /* Get tile dimensions */
    int tile_w = *(int*)((intptr_t)self + 0x18);         /* +0x18 = tile_width */
    int tile_h = *(int*)((intptr_t)self + 0x1C);         /* +0x1C = tile_height */

    /* Step 3: Compute dirty rect from cursor + tile size */
    int dirty_left   = cursor_x;
    int dirty_top    = cursor_y;
    int dirty_right  = tile_w + cursor_x;
    int dirty_bottom = tile_h + cursor_y;

    /* Clip to viewport (+0xD4..+0xE0) */
    int vp_left   = *(int*)((intptr_t)self + 0xD4);
    int vp_top    = *(int*)((intptr_t)self + 0xD8);
    int vp_right  = *(int*)((intptr_t)self + 0xDC);
    int vp_bottom = *(int*)((intptr_t)self + 0xE0);

    if (dirty_right > vp_right) {
        tile_w = vp_right - cursor_x;
        dirty_right = vp_right;
    }
    if (dirty_bottom > vp_bottom) {
        tile_h = vp_bottom - cursor_y;
        dirty_bottom = vp_bottom;
    }
    if (cursor_y < vp_top) {
        dy = vp_top - cursor_y;
        tile_h = dirty_bottom - vp_top;
        dirty_top = vp_top;
    }
    if (cursor_x < vp_left) {
        dx = vp_left - cursor_x;
        tile_w = dirty_right - vp_left;
        dirty_left = vp_left;
    }

    /* Step 4: Check if we should inflate the dirty rect (smooth cursor < 256x256) */
    void* tile_map = *(void**)((intptr_t)self + 0x14);   /* +0x14 */
    int cached_dirty_right = *(int*)((intptr_t)self + 0x58);  /* +0x58 */
    uint8_t blocking = *(uint8_t*)((intptr_t)self + 0x3C);    /* +0x3C */

    if (tile_map != NULL &&
        cached_dirty_right != 0 &&
        enable_tile_map != 0 &&
        blocking == 0) {

        /* Union current dirty rect with cached rect from last frame */
        RECT union_rect;
        RECT current_rect = { dirty_left, dirty_top, dirty_right, dirty_bottom };
        UnionRect(&union_rect, (RECT*)((intptr_t)self + 0x50), &current_rect);

        /* Inflate if union is small (< 256x256) */
        if ((union_rect.right - union_rect.left) < 0x100 &&
            (union_rect.bottom - union_rect.top) < 0x100) {
            inflate_flag = 1;

            /* Inflate by 4 pixels on all sides */
            union_rect.left   -= 4;
            union_rect.top    -= 4;
            union_rect.right  += 4;
            union_rect.bottom += 4;

            /* Re-clip to viewport */
            if (union_rect.right  > vp_right)  union_rect.right  = vp_right;
            if (union_rect.bottom > vp_bottom) union_rect.bottom = vp_bottom;
            if (union_rect.top    < vp_top)    union_rect.top    = vp_top;
            if (union_rect.left   < vp_left)   union_rect.left   = vp_left;
        }
    }

    /* Step 5: Restore background from backbuffer for cached dirty rect */
    if (cached_dirty_right != 0 && enable_tile_map != 0 && inflate_flag == 0) {
        using BltFunc = void (__thiscall*)(void* dst, RECT* dstRect,
            void* src, RECT* srcRect, uint32_t flags, int unk);
        BltFunc blt = (BltFunc)(*(void***)g_backbuffer)[0x14 / 4];
        blt(g_backbuffer,
            (RECT*)((intptr_t)self + 0x50),     /* cached dirty rect */
            g_primary_surface,
            (RECT*)((intptr_t)self + 0x50),
            DDBLT_WAIT, 0);
    }

    /* Step 6: Reset cursor tracking */
    *(int*)((intptr_t)self + 0x34) = -1;            /* +0x34 = cursor_x */
    *(int*)((intptr_t)self + 0x38) = -1;            /* +0x38 = cursor_y */

    /* Step 7: Skip tile rendering if no tile_map or blocking */
    if (tile_map == NULL || blocking != 0) {
        return;
    }

    /* Step 8: Save the dirty rect to the cached position */
    *(int*)((intptr_t)self + 0x50) = dirty_left;
    *(int*)((intptr_t)self + 0x54) = dirty_top;
    *(int*)((intptr_t)self + 0x58) = dirty_right;
    *(int*)((intptr_t)self + 0x5C) = dirty_bottom;

    if (inflate_flag != 0) {
        /* === Path A: Inflated dirty rect (small cursor region) === */
        RECT blit_rect = { dirty_left, dirty_top, dirty_right, dirty_bottom };

        /* Copy from offscreen surface to primary */
        using BltFunc = void (__thiscall*)(void* dst, RECT* dstRect,
            void* src, RECT* srcRect, uint32_t flags, int unk);
        BltFunc blt = (BltFunc)(*(void***)(*(void**)((intptr_t)self + 0x48)))[0x14 / 4];
        blt(*(void**)((intptr_t)self + 0x48),
            &blit_rect,
            g_primary_surface,
            &blit_rect,
            DDBLT_WAIT, 0);

        /* Handle scroll offset */
        int scroll_offset = 0;
        int item_count = *(int*)((intptr_t)self + 0x20);
        if (item_count > 1) {
            if (item_count <= *(int*)((intptr_t)self + 0x24)) {
                *(int*)((intptr_t)self + 0x24) = 0;
            }
            scroll_offset = *(int*)((intptr_t)self + 0x24) *
                           *(int*)((intptr_t)self + 0x18);
        }

        /* Blit tile content to offscreen surface via UIPANEL_Blit */
        int src_x = *(int*)((intptr_t)self + 0x50) - dx;
        int src_y = *(int*)((intptr_t)self + 0x54) - dy;
        int tile_w2 = dirty_right - dirty_left;
        int tile_h2 = dirty_bottom - dirty_top;

        UIPANEL_Blit(tile_map,
                     src_x, src_y,
                     scroll_offset + dx, dy,
                     *(void**)((intptr_t)self + 0x48),   /* offscreen surface */
                     scroll_offset + dx, 0,
                     scroll_offset + dx + tile_w2,
                     tile_h2,
                     0);

        /* Copy from offscreen surface back to primary background */
        blt = (BltFunc)(*(void***)g_backbuffer)[0x14 / 4];
        blt(g_backbuffer,
            (RECT*)((intptr_t)self + 0x50),
            *(void**)((intptr_t)self + 0x48),
            (RECT*)&blit_rect,   /* approximate: stack variable reuse */
            DDBLT_WAIT, 0);
        return;
    }

    /* === Path B: Standard (no inflate) === */
    {
        RECT lock_rect = { 0, 0, 0, 0 };

        /* Lock/copy from primary to offscreen surface */
        using BltFunc = void (__thiscall*)(void* dst, RECT* dstRect,
            void* src, RECT* srcRect, uint32_t flags, int unk);
        BltFunc blt = (BltFunc)(*(void***)(*(void**)((intptr_t)self + 0x48)))[0x14 / 4];
        RECT clip = { dirty_left, dirty_top, dirty_right, dirty_bottom };
        blt(*(void**)((intptr_t)self + 0x48),
            &lock_rect,
            g_primary_surface,
            &clip,
            DDBLT_WAIT, 0);

        /* Handle scroll offset */
        int scroll_offset = 0;
        int item_count = *(int*)((intptr_t)self + 0x20);
        if (item_count > 1) {
            if (item_count <= *(int*)((intptr_t)self + 0x24)) {
                *(int*)((intptr_t)self + 0x24) = 0;
            }
            scroll_offset = *(int*)((intptr_t)self + 0x24) *
                           *(int*)((intptr_t)self + 0x18);
        }

        /* Blit tile content from tile_map to offscreen surface */
        UIPANEL_Blit(tile_map,
                     dirty_right, dirty_bottom,
                     dirty_left, dirty_top,
                     *(void**)((intptr_t)self + 0x48),
                     scroll_offset + dx, dy,
                     dirty_left + dx, 0,
                     0);    /* last param unused in this path */

        /* Copy offscreen surface back to primary background (backbuffer) */
        blt = (BltFunc)(*(void***)g_backbuffer)[0x14 / 4];
        blt(g_backbuffer,
            (RECT*)((intptr_t)self + 0x50),
            *(void**)((intptr_t)self + 0x48),
            (RECT*)&tile_h,     /* stack variable reuse */
            DDBLT_WAIT, 0);
    }
}
#pragma GCC diagnostic pop
