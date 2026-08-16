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
#include "../game/ScriptedObject.h"
#include "../platform/ddraw_interfaces.h"   /* IDirectDrawSurface4 — for BeginPaint's GetDC() */
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

/* DDBLT_WAIT previously had a local, wrong-valued (0x10, actually
 * DDBLT_ASYNC) shadow #define here — removed now that this file includes
 * platform/ddraw_interfaces.h (for BeginPaint's IDirectDrawSurface4), which
 * already defines the correct value (0x01000000). Harmless either way per
 * PROGRESS.md: Sdl3DirectDrawSurface::Blt ignores its flags parameter. */

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* Windows API — provided by stubs/windows.h via types.h; no need to redeclare */

/* Heap / CRTs (C++ linkage — NOT extern "C") */
extern void* operator_new(size_t size);              /* 0x465CE0 */
extern void  GLOBAL_free(void* ptr);                 /* 0x465CD0 */
extern void  WIN32_FatalError(void);

/* DirectDraw globals — void* to avoid IDirectDraw4 dependency */
extern void* g_ddraw;                                /* 0x485440 — confirmed via Ghidra xrefs 2026-08-14; 0x4FD394 (this file's prior comment) resolves to unrelated network/train-queue code, not DirectDraw */
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

/* UIPANEL_WindowProc (0x426900) and UIPANEL_OnDestroy (0x426A90) were
 * removed 2026-08-16 — they were dead duplicates of the real
 * UI_WindowBase::on_mouse_move()/on_close() (ui/UI_WindowBase.cpp); see
 * the correction note further below and PROGRESS.md's 2026-08-16 entry.
 * UIPANEL_BeginPaint(void*) is a thin compatibility shim over the real
 * UI_WindowBase::BeginPaint() method (defined in ui/UI_WindowBase.cpp) —
 * kept as a free function only because ~9 other files in the tree still
 * declare/call it that way with a mix of mismatched extern signatures
 * (some correct, some pre-existing call-0-class landmines: wrong return
 * type, wrong calling convention, wrong parameter type). Fixing those
 * callers is separately scoped (see docs/landmine-sweep-worklist.md /
 * PROGRESS.md) and intentionally untouched here. */
extern HDC __fastcall UIPANEL_BeginPaint(void* self);
/* UIPANEL_EndPaint/EndPaintEx/Render's compatibility shims moved to
 * ui/UI_WindowBase.cpp alongside UIPANEL_BeginPaint's (see the correction
 * note below and PROGRESS.md's 2026-08-16 entry) — not declared here since
 * this file has no callers of its own. */
extern void __fastcall UIPANEL_CreateSurface(void* surface);                 /* 0x42A110 */
extern void __thiscall UIPANEL_CreateSprite(UIPANEL* panel, SaveSprite* entry);   /* 0x429850 -- real return type is void, not void* (fixed: was a call-0-shaped landmine) */
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

/* External functions referenced from UIPANEL drawing */
class InputMgr;
extern uint32_t __fastcall UIPANEL_DrawEditField(UIPANEL* self);            /* 0x429490 */
extern void INPUT_SaveCurrentWorld(InputMgr* input, const char* name); /* 0x41D9B0 */
extern void __thiscall RESDATA_GameObject_UpdateAnimation(void* obj);        /* 0x44B810 */
extern void __fastcall PlaySound(int sound_id);                              /* 0x44A290 */
extern void* __thiscall RESDATA_SoundObject_GetState(int sprite);            /* 0x44CAC0? */
extern int __thiscall RESDATA_SoundObject_GetTextLength(int sprite);        /* 0x44CAE0? */

/* Global variables */
extern void*  g_resource_mgr;            /* 0x4B375C (ResourceManager) */
extern char   g_empty_string[];          /* 0x476934 */
extern ScriptedObject* g_scripted_object;   /* 0x4AA5B8 — game/ScriptedObject.h */
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
    this->save_path_buf[0] = 0;                 /* +0x2EA -- byte */
    this->save_header = NULL;                   /* +0x498 */
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
    SaveSprite* current = this->sprite_list_head;    /* +0x4D8 */
    while (current != NULL) {
        SaveSprite* next = current->next;            /* +0x22C */
        this->sprite_list_head = next;                /* +0x4D8 */
        delete current;
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
    SaveSprite* current = this->sprite_list_head;    /* +0x4D8 */
    while (current != NULL) {
        SaveSprite* next = current->next;            /* +0x22C */
        this->sprite_list_head = next;                /* +0x4D8 */
        delete current;
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
        /* Fixed: previously `&g_scripted_object` (address of the pointer
           variable itself, a `ScriptedObject**` — wrong type/value for
           `g_active_panel`) and a hardcoded literal-address cast (invalid
           on this host's process layout; a real crash-on-touch landmine).
           Both were pre-existing bugs in this call site, not behavior this
           pass introduces. */
        g_active_panel = g_scripted_object;                     /* Back to main panel */
        RESDATA_GameObject_UpdateAnimation(g_scripted_object);  /* 0x44B810 */
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
        UIPANEL_DrawEditField(this);
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
        UIPANEL_CreateSprite(this, this->sprite_list_head);

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

            /* cmp = StrCmp2Byte(cur_state, first_state) computes strcmp(first_state,
             * cur_state) (its early-return branch returns the SIGN OF (str2-str1),
             * i.e. args are effectively swapped) -- so cmp>0 means the first visible
             * item's name sorts AFTER the current selection: scroll toward EARLIER
             * entries via ->prev (+0x228). cmp<0 means scroll toward LATER entries
             * via ->next (+0x22C). Confirmed against ui/SaveSprite.h's independently
             * derived next/prev direction (see its doc comment). */
            while (cmp > 0) {
                SaveSprite* tail = this->sprite_list_tail;
                if (tail == NULL) break;
                SaveSprite* prev = tail->prev;
                if (prev == NULL) break;
                UIPANEL_CreateSprite(this, prev);
                cur_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4BC));
                first_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4C0));
                cmp = UIPANEL_StrCmp2Byte(cur_state, first_state);
            }

            while (cmp < 0) {
                int text_len = RESDATA_SoundObject_GetTextLength(*(int*)((intptr_t)this + 0x4D4));
                if (text_len == 0) break;
                SaveSprite* tail = this->sprite_list_tail;
                if (tail == NULL) break;
                SaveSprite* next = tail->next;
                if (next == NULL) break;
                UIPANEL_CreateSprite(this, next);
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
        UIPANEL_CreateSprite(this, this->sprite_list_head);

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
                SaveSprite* tail = this->sprite_list_tail;
                if (tail == NULL) break;
                SaveSprite* prev = tail->prev;
                if (prev == NULL) break;
                UIPANEL_CreateSprite(this, prev);
                cur_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4BC));
                first_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4C0));
                cmp = UIPANEL_StrCmp2Byte(cur_state, first_state);
            }

            while (cmp < 0) {
                int text_len = RESDATA_SoundObject_GetTextLength(*(int*)((intptr_t)this + 0x4D4));
                if (text_len == 0) break;
                SaveSprite* tail = this->sprite_list_tail;
                if (tail == NULL) break;
                SaveSprite* next = tail->next;
                if (next == NULL) break;
                UIPANEL_CreateSprite(this, next);
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
        UIPANEL_CreateSprite(this, this->sprite_list_head);

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
                SaveSprite* tail = this->sprite_list_tail;
                if (tail == NULL) break;
                SaveSprite* prev = tail->prev;
                if (prev == NULL) break;
                UIPANEL_CreateSprite(this, prev);
                cur_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4BC));
                first_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4C0));
                cmp = UIPANEL_StrCmp2Byte(cur_state, first_state);
            }

            while (cmp < 0) {
                int text_len = RESDATA_SoundObject_GetTextLength(*(int*)((intptr_t)this + 0x4D4));
                if (text_len == 0) break;
                SaveSprite* tail = this->sprite_list_tail;
                if (tail == NULL) break;
                SaveSprite* next = tail->next;
                if (next == NULL) break;
                UIPANEL_CreateSprite(this, next);
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
        UIPANEL_DrawEditField(this);

        /* Populate 6-item viewport from sprite list */
        UIPANEL_CreateSprite(this, this->sprite_list_head);

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
                SaveSprite* tail = this->sprite_list_tail;
                if (tail == NULL) break;
                SaveSprite* prev = tail->prev;
                if (prev == NULL) break;
                UIPANEL_CreateSprite(this, prev);
                cur_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4BC));
                first_state = (char*)RESDATA_SoundObject_GetState(*(int*)((intptr_t)this + 0x4C0));
                cmp = UIPANEL_StrCmp2Byte(cur_state, first_state);
            }

            while (cmp < 0) {
                int text_len = RESDATA_SoundObject_GetTextLength(*(int*)((intptr_t)this + 0x4D4));
                if (text_len == 0) break;
                SaveSprite* tail = this->sprite_list_tail;
                if (tail == NULL) break;
                SaveSprite* next = tail->next;
                if (next == NULL) break;
                UIPANEL_CreateSprite(this, next);
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
/* WindowProc (0x426900) / OnDestroy (0x426A90) / BeginPaint (0x426B00) / */
/* EndPaint (0x426B70) / EndPaintEx (0x426B90) / Render (0x426EB0)        */
/* were previously (mis-)transcribed here as UIPANEL methods — corrected  */
/* 2026-08-16: get_xrefs_to on this whole address block (0x426900-        */
/* 0x426EB0) shows every real caller is GameSetupPanel, Cursor,           */
/* NameEntryPanel, BuildingPanel, PostcardAlbum, Town, DPlayManager, or    */
/* NETMAN_* — never a UIPANEL instance — and a Ghidra function-address-   */
/* range listing confirms these sit in the same contiguous MSVC method    */
/* block as UI_WindowBase_SetMode (0x425FD0)/SetRenderSurface (0x426020)/ */
/* dispatch_message (0x426140), ending right before UIPANEL's own real    */
/* ctor begins a new block at 0x427370. These are UI_WindowBase members;  */
/* "UIPANEL_" was a stale Ghidra-era prefix. WindowProc/OnDestroy were     */
/* dead duplicates of the already-real UI_WindowBase::on_mouse_move()/    */
/* on_close() and were removed entirely (see PROGRESS.md's 2026-08-16     */
/* entry). BeginPaint moved to UI_WindowBase::BeginPaint() in an earlier   */
/* pass; EndPaint/EndPaintEx/Render moved to UI_WindowBase::EndPaint()/    */
/* EndPaintEx()/Render() in this pass (ui/UI_WindowBase.h/.cpp) — see      */
/* there for the full evidence trail (ReleaseDC slot, backwards-Blt        */
/* correction, renderSurface/cursorBackSurface field identities). Thin     */
/* free-function compatibility shims for the ~50 remaining external        */
/* callers of UIPANEL_EndPaintEx()/UIPANEL_EndPaint() are defined in       */
/* ui/UI_WindowBase.cpp, next to UIPANEL_BeginPaint's existing shim.       */
/* UIPANEL_Render() had zero external callers (only used from within       */
/* UI_WindowBase.cpp itself) and needed no compatibility shim at all —     */
/* its two internal call sites were converted to typed this->Render()     */
/* calls directly.                                                        */
/* ================================================================== */
#pragma GCC diagnostic pop
