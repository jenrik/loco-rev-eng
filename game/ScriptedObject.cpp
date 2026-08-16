/**
 * ScriptedObject.cpp — ScriptedObject implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Status: TRANSCRIBED
 *
 * RECONCILIATION (2026-08-16): this file absorbs the method bodies that
 * were previously duplicated (and, in several places, wrongly field-mapped)
 * in world/scriptengine.cpp's `RESDATA_ScriptedObject` class — see
 * ScriptedObject.h's class doc comment for the full trail. Every ported
 * body below was re-derived against the VERIFIED real field offsets in
 * GameObject.h/Entity.h/Panel.h (not copied from world/scriptengine.cpp's
 * own field names), because that file's raw-index reads (`param_1[N]` is a
 * byte offset of N*4, not N) had several latent mis-mappings:
 *   - `param_1[9]` (byte +0x24) is `Entity::visible`, not "drag_flag".
 *   - `param_1[0x24]` (byte +0x90) is `Panel::drag_active`, ALSO called
 *     "drag_flag" there — i.e. two different original fields were
 *     collapsed into one name.
 *   - `param_1[0xa]` (byte +0x28) is `Entity::anim_index`, not
 *     "tooltip_state".
 *   - `param_1[0x15]` (byte +0x54) is `Entity::frame_index`, not
 *     "anim_index" (a different field, at +0x28).
 *   - Every vtable dispatch through slot +0x0C (on `this`, on the embedded
 *     Entity, or on the tooltip handle) is `MoveTo`, not a fictitious
 *     virtual "SetPosition" — Panel does have a real, differently-
 *     addressed non-virtual `SetPosition` (0x454820), but that is not what
 *     these vtable calls reach.
 * See ScriptedObject.h for the corresponding field documentation.
 *
 * VERIFICATION STATUS per method (2026-08-16 pass) — given the density of
 * mis-mappings found in the source this file replaces, ported bodies are
 * NOT uniformly trustworthy; this records exactly how each was checked:
 *   - Ctor (0x449430), ~Dtor/InitSubObjects (0x4494E0), Update (0x4497A0),
 *     EnterBuildMode (0x44A9D0), IsDragging (0x449CE0), GetDragOffset
 *     (0x449D80), CheckClick (0x449D00), HitTest (0x44A0C0), Dispatch
 *     (0x449C00): re-derived directly from disassemble_function output,
 *     instruction-by-instruction, cross-checked against GameObject.h/
 *     Entity.h/Panel.h's canonical vtable slot tables. High confidence.
 *   - Shutdown, Start, MoveTo, HandleToolClick, UpdateToolState: ported from
 *     world/scriptengine.cpp's decompilation with field names corrected
 *     against the canonical headers (see mis-mapping list above) but NOT
 *     re-verified instruction-by-instruction against fresh disassembly.
 *     Medium confidence — plausible, not disassembly-proven.
 *   - OnUpdateChild, InitState: undecompiled stubs (pre-existing gap, not
 *     introduced by this pass) — see TODO markers at their definitions.
 * Do not treat "ported" and "disassembly-verified" as equivalent elsewhere
 * in this file; the tier above is the actual state per method.
 */

#include "ScriptedObject.h"
#include "../shared/types.h"
#include "../game/TrackPiece.h"
#include "../core/Entity.h"
#include "../town/Town.h"
#include "../resources/ResourceManager.h"
#include "../core/CGWND.h"
#include "../stubs/windows.h"

/* ================================================================== */
/* Win32 API imports — C linkage only                                  */
/* ================================================================== */
/* ClientToScreen/SetCursorPos: declared by stubs/windows.h (canonical
 * POINT*-typed signature), included directly above. */

/* ================================================================== */
/* CRT helpers — C++ linkage                                            */
/* ================================================================== */
void  __cdecl CRT_free(void* ptr);                            /* 0x466C70 */
int   __cdecl CRT_sprintf_buf(void* buf, const char* fmt, ...); /* 0x466D60 */

/* Panel helpers declared in Panel.h */
extern void Panel_DtorBody(void* obj);                         /* 0x4545A0 */

/* ScriptEngine helpers (ScriptEngine's own real inheritance/full vtable
 * shape is unresolved — see ScriptedObject.h's BLOCKER comment on the
 * embedded script_engine_prefix region. Kept as free-function/opaque
 * dispatch, matching the pre-existing style in this file.) */
extern void ScriptEngine_Init(void* obj);                      /* 0x44E8D0 */
extern void ScriptEngine_Call(void* obj);                      /* 0x44E930 */

/* ScrollPanel helpers (same BLOCKER as ScriptEngine — see header) */
extern void UIPANEL_InitScrollPanel(void* obj);                /* 0x427370 */
extern void UIPANEL_ScrollPanel_Dtor(void* obj);               /* 0x427460 */
extern void UIPANEL_ScrollPanel_HandleDrag(void* panel, int32_t param, int32_t action); /* 0x4277D0 */

/* Tooltip */
extern void UI_DestroyTooltip(void* mgr, int handle);          /* 0x423D20 */
extern Entity* UI_CreateTooltip(void* mgr, int res_id, int16_t unk,
                                 int x, int y);                    /* 0x423C50 */

/* Audio */
extern void GameAudio_UpdateVolume(void* audio, uint8_t mute); /* 0x4135B0 */
extern int  HelpWnd_PlayNarration(void* mgr, int category,
                                  int res_id);                 /* 0x44F560 */

/* Input/World */
class InputMgr;
extern void INPUT_NewWorld(InputMgr* input_mgr);                   /* 0x41E120 */
extern void INPUT_LoadWorld(InputMgr* input_mgr, const char* path); /* 0x41D320 */
extern void INPUT_SaveCurrentWorld(InputMgr* input_mgr,
                                   const char* path);          /* 0x41D9B0 */
extern void CGWND_SetBuildMode(int mode);                      /* 0x4089D0 */
extern void CGWND_SetMode(int mode);                           /* 0x408130 */
extern void CGWND_SetPause(void* obj, uint8_t pause);          /* CGWND.cpp */
extern void Game_CheckScreensaverTimeout(void* game);          /* 0x410A20 */
extern void GameAudio_SetMute(void* audio, uint8_t mute);      /* 0x413560 */
extern void TileMap_InvalidateRect(void* tilemap, int l, int t,
                                   int r, int b);              /* 0x45E240 */
extern int  Town_SelectBuilding(void* town_view, int building);  /* 0x42EC60 */
extern int  DDRAW_SelectBuilding(void* ddraw_building, int building); /* 0x459180 */
extern int  UI_IsBitmapReady(int handle);                       /* 0x424C30 */

/* ================================================================== */
/* Global variables                                                     */
/* ================================================================== */

extern InputMgr g_input_mgr;        /* 0x4A9990 — static InputMgr object */
extern void*    g_audio_mgr;                 /* 0x4FD38C */
extern void*    g_audio;                     /* 0x4FD3BC */
extern void*    g_netman;                    /* 0x4FD3AC */
class UI_Manager;
extern UI_Manager* g_tooltip_mgr;            /* 0x4FD220 */
extern void*    g_tilemap;                   /* 0x4AAE90 */
extern void*    g_town_view;                 /* 0x4852A0 */
extern uint8_t  g_is_town_mode;               /* 0x485328 — town-mode-active flag byte
                                               * (confirmed via direct disassembly of
                                               * 0x4497A0: `MOV AL,[0x485328]; TEST AL,AL`
                                               * — NOT a pointer null-check; a prior pass
                                               * here used the wrong address (0x485490)
                                               * and the wrong void*-nullcheck shape). */
extern uint8_t  g_ddraw_active_flag;          /* 0x4A9F78 — ddraw-active flag byte (same
                                               * correction: `MOV AL,[0x4A9F78]`). */
extern void*    g_ddraw_building;            /* 0x4A9EF0 */
extern void*    g_trainstation_window;       /* 0x485258 */
extern void*    g_game;                      /* 0x4854C8 — Game singleton */

extern int      g_world_width;               /* 0x4AAD0C */
extern int      g_world_height;              /* 0x4AAD10 */
extern int      g_viewport_x;                /* 0x4AAD24 */
extern int      g_viewport_y;                /* 0x4AAD28 */
extern int      g_screen_width;              /* 0x4851D8 */
extern int      g_drag_start_x;              /* 0x485574 */
extern int      g_drag_start_y;              /* 0x485578 */
extern int      g_cursor_world_x;            /* 0x48555C */
extern int      g_cursor_world_y;            /* 0x485560 */
extern char     g_disable_input;             /* 0x4855AC */
extern char     g_is_fullscreen;             /* 0x485210 */
extern char     g_allow_building_placement;  /* 0x4FD3DC */
extern char     g_in_build_mode;             /* 0x4FD199 */
/* g_demo_mode: canonical declaration is shared/types.h's
 * `extern int32_t g_demo_mode;` (0x4A9918) -- this file's own former
 * `extern char g_demo_mode;` was a type mismatch, not a distinct global. */
extern uint32_t g_last_cursor_pos;           /* 0x485558 */
extern void*    g_active_panel;              /* 0x4FD224 */
extern void*    g_main_window;               /* CGWND singleton */

/* ================================================================== */
/* Inline helpers for the still-unresolved ScriptEngine/ScrollPanel      */
/* embedded sub-objects (see ScriptedObject.h's BLOCKER comments).       */
/* These map the binary's literal vtable access to typed operations     */
/* for the two sub-objects whose own class hierarchy this pass could    */
/* not safely re-derive. Extending the pre-existing pattern in this      */
/* file rather than inventing a new one.                                */
/* ================================================================== */

static inline void se_shutdown(ScriptedObject* so)
{
    using Shutdown = void (*)(void*);
    void** vtable = *reinterpret_cast<void***>(so->script_engine_prefix);
    reinterpret_cast<Shutdown>(vtable[15])(so->script_engine_prefix);
}

static inline void se_handle_drag(ScriptedObject* so, void* child, int mode)
{
    using HandleDrag = void (*)(void*, void*, int);
    void** vtable = *reinterpret_cast<void***>(so->script_engine_prefix);
    reinterpret_cast<HandleDrag>(vtable[21])(so->script_engine_prefix, child, mode);
}

static inline void se_vmove(ScriptedObject* so, int x, int y)
{
    using MoveTo = void (*)(void*, int, int);
    void** vtable = *reinterpret_cast<void***>(so->script_engine_prefix);
    reinterpret_cast<MoveTo>(vtable[3])(so->script_engine_prefix, x, y);
}

static inline uint8_t se_ptinrect(ScriptedObject* so, int x, int y)
{
    using PtInRect = uint8_t (*)(void*, int, int);
    void** vtable = *reinterpret_cast<void***>(so->script_engine_prefix);
    return reinterpret_cast<PtInRect>(vtable[2])(so->script_engine_prefix, x, y);
}

static inline uint8_t se_hittest(ScriptedObject* so, int x, int y)
{
    using HitTest = uint8_t (*)(void*, int, int);
    void** vtable = *reinterpret_cast<void***>(so->script_engine_prefix);
    return reinterpret_cast<HitTest>(vtable[4])(so->script_engine_prefix, x, y);
}

static inline void se_update(ScriptedObject* so)
{
    using Update = void (*)(void*);
    void** vtable = *reinterpret_cast<void***>(so->script_engine_prefix);
    reinterpret_cast<Update>(vtable[10])(so->script_engine_prefix);
}

static inline void se_draw(ScriptedObject* so, RECT clip)
{
    using Draw = void (*)(void*, RECT, int32_t, uint32_t);
    void** vtable = *reinterpret_cast<void***>(so->script_engine_prefix);
    reinterpret_cast<Draw>(vtable[11])(so->script_engine_prefix, clip, 0, 0);
}

static inline void sp_shutdown(ScriptedObject* so)
{
    using Shutdown = void (*)(void*);
    void** vtable = *reinterpret_cast<void***>(so->scroll_panel_prefix);
    reinterpret_cast<Shutdown>(vtable[15])(so->scroll_panel_prefix);
}

static inline void sp_vmove(ScriptedObject* so, int x, int y)
{
    using MoveTo = void (*)(void*, int, int);
    void** vtable = *reinterpret_cast<void***>(so->scroll_panel_prefix);
    reinterpret_cast<MoveTo>(vtable[3])(so->scroll_panel_prefix, x, y);
}

static inline uint8_t sp_ptinrect(ScriptedObject* so, int x, int y)
{
    using PtInRect = uint8_t (*)(void*, int, int);
    void** vtable = *reinterpret_cast<void***>(so->scroll_panel_prefix);
    return reinterpret_cast<PtInRect>(vtable[2])(so->scroll_panel_prefix, x, y);
}

static inline uint8_t sp_hittest(ScriptedObject* so, int x, int y)
{
    using HitTest = uint8_t (*)(void*, int, int);
    void** vtable = *reinterpret_cast<void***>(so->scroll_panel_prefix);
    return reinterpret_cast<HitTest>(vtable[4])(so->scroll_panel_prefix, x, y);
}

static inline void sp_update(ScriptedObject* so)
{
    using Update = void (*)(void*);
    void** vtable = *reinterpret_cast<void***>(so->scroll_panel_prefix);
    reinterpret_cast<Update>(vtable[10])(so->scroll_panel_prefix);
}

static inline void sp_draw(ScriptedObject* so, RECT clip)
{
    using Draw = void (*)(void*, RECT, int32_t, uint32_t);
    void** vtable = *reinterpret_cast<void***>(so->scroll_panel_prefix);
    reinterpret_cast<Draw>(vtable[11])(so->scroll_panel_prefix, clip, 0, 0);
}

static inline uint32_t sp_init(ScriptedObject* so, int32_t a, int32_t b, int32_t c)
{
    using Init = uint32_t (*)(void*, int32_t, int32_t, int32_t);
    void** vtable = *reinterpret_cast<void***>(so->scroll_panel_prefix);
    return reinterpret_cast<Init>(vtable[6])(so->scroll_panel_prefix, a, b, c);
}

/* ABI_BOUNDARY: Panel::tooltip_handle is inherited as an int32_t
   (pre-existing Panel.h field type, not introduced by this pass) rather
   than a typed Entity* — widening it tree-wide is out of scope here. */
static inline void tooltip_vmove(int32_t handle, int x, int y)
{
    if (handle == 0) return;
    reinterpret_cast<Entity*>(static_cast<intptr_t>(handle))->MoveTo(x, y);
}

static inline void tooltip_set_state(int32_t handle, int state)
{
    if (handle == 0) return;
    reinterpret_cast<Entity*>(static_cast<intptr_t>(handle))->SetAnimState(state);
}

/* ================================================================== */
/* Constructor — Address: 0x449430                                      */
/* ================================================================== */

ScriptedObject::ScriptedObject()
    : sub_entity(-1, -1, 0, 0)   /* Entity_Ctor(&sub_entity, -1, -1, 0, 0), 0x405790 */
{
    /* Step 1: Initialize Panel base fields via PartialDtor.
       In the binary this resets the vtable to Panel and zeros child/tooltip fields. */
    this->PartialDtor();

    /* Step 2: Initialize ScriptEngine at +0x178 (see BLOCKER on that field) */
    ScriptEngine_Init(&script_engine_prefix);                        /* 0x44E8D0 */

    /* Step 3: Initialize ScrollPanel at +0x260 (see BLOCKER on that field) */
    UIPANEL_InitScrollPanel(&scroll_panel_prefix);                   /* 0x427370 */

    /* Step 4: Set type to 10 */
    this->type = 10;

    /* Step 5: Zero out trailing fields */
    this->child_sprite1 = nullptr;
    this->child_sprite2 = nullptr;
}


/**
 * ScriptedObject::~ScriptedObject — Body destructor
 *
 * Tears down all sub-objects via InitSubObjects. The scalar-deleting
 * destructor at vtable[0] (0x4494C0) wraps this body, tests flags & 1,
 * and conditionally calls GLOBAL_free — all compiler-generated.
 */
ScriptedObject::~ScriptedObject()
{
    this->InitSubObjects();
}

/* ================================================================== */
/* InitSubObjects — Full teardown of all sub-objects                    */
/* Address: 0x4494E0                                                    */
/* ================================================================== */

void ScriptedObject::InitSubObjects()
{
    /* Step 1: Stop embedded Entity via InitBase(0, -1, 0) — a real,
       non-destructive Entity method (distinct from the C++ member
       lifetime managed automatically by the compiler below). */
    this->sub_entity.InitBase(0, -1, 0);

    /* Step 2: Stop self via Panel::Init */
    this->Init(0, -1, 0);

    /* Step 3: Shutdown ScriptEngine */
    se_shutdown(this);

    /* Step 4: Shutdown ScrollPanel */
    sp_shutdown(this);

    /* Step 5: Clean Panel resources (tooltip, child surface, etc.) */
    this->PartialDtor();                                     /* was RESDATA_DtorBase @ 0x454630 */

    /* Step 6: Destroy ScrollPanel */
    UIPANEL_ScrollPanel_Dtor(&scroll_panel_prefix);                 /* 0x427460 */

    /* Step 7: Stop ScriptEngine */
    ScriptEngine_Call(&script_engine_prefix);                       /* 0x44E930 */

    /* Step 8: Destroy Panel base */
    Panel_DtorBody(this);                                    /* 0x4545A0 */

    /* The original's step 8 (of 9) explicitly ran the embedded Entity's
       destructor BODY (GameObject_DtorBody(&sub_entity), 0x405870) here,
       mid-sequence. Now that sub_entity is a real embedded Entity member,
       the compiler destroys it automatically as part of ~ScriptedObject()
       (after this function returns) — calling ~Entity() explicitly here
       too would double-destroy it. This changes sub_entity's teardown
       from "mid-sequence" to "last, after every other step above", which
       is safe (the other steps do not depend on sub_entity's post-
       destruction state) but is a real, intentional simplification: see
       CLAUDE.md's "let the compiler manage... object destruction". */
}

/* ================================================================== */
/* Shutdown — Lightweight teardown (vtable[15])                        */
/* Address: 0x4495B0                                                    */
/* ================================================================== */

void ScriptedObject::Shutdown()
{
    /* Stop embedded Entity via InitBase */
    this->sub_entity.InitBase(0, -1, 0);

    /* Stop self via Panel::Init */
    this->Init(0, -1, 0);

    /* Stop ScriptEngine */
    se_shutdown(this);

    /* Stop ScrollPanel */
    sp_shutdown(this);

    /* Clean Panel base resources */
    this->PartialDtor();                                     /* 0x454630 */
}

/* ================================================================== */
/* Start — Activate scripted object                                     */
/* Address: 0x449600                                                    */
/* NOTE: zero callers anywhere in the current tree (also true in the     */
/* pre-merge world/scriptengine.cpp); ported for completeness per        */
/* CLAUDE.md's no-stub policy, not independently exercised.              */
/* ================================================================== */

uint32_t ScriptedObject::Start()
{
    uint32_t initResult = this->Init(0x2400, -1, 0);

    /* Set sub-object mapping flag on the shared resource (matches the
       original's own disassembly; not object-specific despite appearances). */
    if (this->resource != nullptr) {
        static_cast<RESDATA*>(this->resource)->frame_width = 1;
    }

    if (initResult != 0) {
        initResult = this->sub_entity.InitBase(0x2402, -1, 0);
        if (initResult != 0) {
            uint32_t spInit = sp_init(this, 0, -1, 0);
            if (spInit == 0) {
                return spInit;
            }

            /* Create sprites for resources 0x2400-0x2413 */
            for (int32_t resId = 0x2400, count = 0; count < 20; resId++, count++) {
                void* resource = reinterpret_cast<void*>(
                    static_cast<intptr_t>(g_resmgr.GetById(resId)));
                if (resource != nullptr) {
                    if (UI_IsBitmapReady(static_cast<int>(reinterpret_cast<intptr_t>(resource))) != 0) {
                        int32_t resType = static_cast<RESDATA*>(resource)->resource_id;
                        if (resType == 0x2406) {
                            /* Return value intentionally discarded here, matching
                               the pre-existing transcription this was ported
                               from -- the created sprite is tracked internally
                               by CreateChildSprite's own child-list linkage
                               (Panel::child_surface), not through this field. */
                            this->CreateChildSprite(
                                static_cast<int>(reinterpret_cast<intptr_t>(resource)), 0, 0);
                            this->child_sprite1 = nullptr;
                        } else if (resType == 0x240C) {
                            this->CreateChildSprite(
                                static_cast<int>(reinterpret_cast<intptr_t>(resource)), 0, 0);
                            this->child_sprite2 = nullptr;
                        } else {
                            this->CreateChildSprite(
                                static_cast<int>(reinterpret_cast<intptr_t>(resource)), 0, 0);
                        }
                    }
                }
            }

            /* Load game mode resource */
            uint32_t modeResult = this->Init(0x2401, -1, 0);

            if (g_demo_mode == 1) {
                CGWND_SetPause(this, 0);
            }

            /* Destroy old tooltip */
            if (this->tooltip_handle != 0) {
                UI_DestroyTooltip(g_tooltip_mgr, this->tooltip_handle);
            }

            /* Create tooltip */
            if (this->visible != 0) {
                Entity* tooltip = UI_CreateTooltip(
                    g_tooltip_mgr, 0x3887, 1,
                    this->screen_rect.left + 0x32,
                    this->screen_rect.top + 0x32);
                this->tooltip_handle = static_cast<int32_t>(reinterpret_cast<intptr_t>(tooltip));
            }

            if (modeResult != 0) {
                if (this->resource != nullptr) {
                    static_cast<RESDATA*>(this->resource)->frame_width = 1;
                }

                this->mode = 0;                            /* idle */
                this->blit_flags |= 2;                     /* +0x2C (Entity::blit_flags) */
                this->update_child_flags = 0;              /* +0x88 */
                g_active_panel = nullptr;

                this->SetAnimState(0);
                this->SetPosition(0x32, 10);
                return 1;
            }
        }
    }

    return initResult & 0xFFFFFF00;
}

/* ================================================================== */
/* Dispatch (Draw) — NOT a vtable member; see ScriptedObject.h          */
/* Address: 0x449C00                                                    */
/* ================================================================== */

void ScriptedObject::Dispatch(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                               int32_t /*flag*/)
{
    const RECT clip{x1, y1, x2, y2};
    this->Entity::Draw(clip, 0, 0);

    if (this->mode == 3) {
        this->sub_entity.Draw(clip, 0, 0);
    }
    if (this->script_engine_active == 1) {
        se_draw(this, clip);
    }
    if (this->scroll_panel_active == 1) {
        sp_draw(this, clip);
    }
}

/* ================================================================== */
/* IsDragging (PtInRect) — vtable[2]                                    */
/* Address: 0x449CE0                                                    */
/*                                                                      */
/* BUG-mode3-input-processing-crashes.md Crash 3: this previously called */
/* a virtual dispatch through a fabricated vtable view. Ghidra 0x449CE0  */
/* decompiles to a DIRECT (non-virtual) call: GameObject_PtInRect(this,  */
/* x, y) — i.e. the original applies GameObject::PtInRect's own field    */
/* layout (left/top/right/bottom) directly to this object's own base     */
/* address; not a further virtual call. Reproduce the same half-open     */
/* interval test directly. */
/* ================================================================== */

int ScriptedObject::IsDragging(int x, int y)
{
    return x >= this->screen_rect.left && x < this->screen_rect.right &&
           y >= this->screen_rect.top && y < this->screen_rect.bottom;
}

/* ================================================================== */
/* CheckClick — vtable[22]                                              */
/* Address: 0x449D00                                                    */
/* ================================================================== */

/* Verified against 0x449D00 disassembly (47 instructions): the original
 * dispatches through its OWN vtable[2] (0x449CE0) first — the same slot
 * documented as IsDragging above, not the inherited GameObject::PtInRect
 * at a different slot. Both currently produce identical results (IsDragging's
 * body duplicates PtInRect's half-open-interval test verbatim per the
 * Crash-3 fix above), but the call must name the slot-2 override that this
 * class actually declares, not the unrelated base method, so a future
 * change to either body doesn't silently desync CheckClick from vtable[2]. */
bool ScriptedObject::CheckClick(int x, int y)
{
    bool hit = false;

    uint8_t result = static_cast<uint8_t>(this->IsDragging(x, y));
    if (result == 0) {
        /* Own slot 21 (+0x54) secondary hit-test: reuses the compiler-
           generated GetDragOffset override at the same vtable position
           documented for this class. */
        result = static_cast<uint8_t>(this->GetDragOffset(x, y));
        if (result == 0 && this->script_engine_active != 0) {
            result = se_ptinrect(this, x, y);
            hit = result != 0;
        } else {
            hit = result != 0;
        }
    } else {
        hit = true;
    }

    if (!hit && this->scroll_panel_active != 0) {
        result = sp_ptinrect(this, x, y);
        hit = result != 0;
    }

    return hit;
}

/* ================================================================== */
/* GetDragOffset — vtable[21]                                           */
/* Address: 0x449D80                                                    */
/* ================================================================== */

int ScriptedObject::GetDragOffset(int x, int y)
{
    if (this->drag_rect.left <= x && x < this->drag_rect.right &&
        this->drag_rect.top <= y && y < this->drag_rect.bottom) {
        return 1;
    }
    return 0;
}

/* ================================================================== */
/* MoveTo — Move ScriptedObject to (x, y) with boundary clamping       */
/* Address: 0x449DC0                                                    */
/* ================================================================== */

void ScriptedObject::MoveTo(int x, int y)
{
    /* When mode == 1 (entering build mode), skip boundary clamping */
    if (this->mode != 1) {
        if (this->dim_flag == 1) {
            /* Positive direction: clamp x >= 0 */
            if (x < 0) {
                x = 0;
            }

            int sprite_width = static_cast<const RESDATA*>(this->resource)->frame_width;

            if (this->script_engine_active != 0) {
                int max_x = g_world_width - this->script_engine_offset - sprite_width;
                if (x > max_x) x = max_x;
            }
            else if (this->scroll_panel_offset != 0) {
                int max_x = g_world_width - this->scroll_panel_offset - sprite_width;
                if (x > max_x) x = max_x;
            }
            else {
                if (x > g_world_width - sprite_width) {
                    x = g_world_width - sprite_width;
                }
            }
        }
        else {  /* dim_flag == 0 — negative direction */
            if (x < 0) {
                x = 0;
            }

            if (this->script_engine_active != 0 && x < this->script_engine_offset) {
                x = this->script_engine_offset;
            }
            else if (this->scroll_panel_offset != 0 && x < this->scroll_panel_offset) {
                x = this->scroll_panel_offset;
            }
            else {
                int sprite_width = static_cast<const RESDATA*>(this->resource)->frame_width;
                if (x > g_world_width - sprite_width) {
                    x = g_world_width - sprite_width;
                }
            }
        }

        /* Y boundary clamping. Height bound is Entity::source_rect.bottom
           (+0x3C) — NOT GameObject::screen_rect.bottom (+0x14), a distinct
           field; a prior pass here read the wrong one. */
        if (y < 0) {
            y = 0;
        }
        else {
            int height = this->source_rect.bottom;
            if (y > g_world_height - height) {
                y = g_world_height - height;
            }
        }
    }

    /* Set position via Entity's own MoveTo (real virtual dispatch, not a
       distinct "SetPosition" method — see this file's header comment). */
    this->Entity::MoveTo(x, y);

    /* Update sub_entity position with frame offset from the resource's
       per-frame table (raw data blob, not a modeled game object — an
       ordinary byte-oriented read, matching Entity::resource's own
       documented FrameData layout in shared/types.h). */
    {
        const RESDATA* res = static_cast<const RESDATA*>(this->sub_entity.resource);
        int go_x = x;
        int go_y = y;
        if (res != nullptr) {
            go_x += res->offset_x;
            go_y += res->offset_y;
        }
        this->sub_entity.MoveTo(go_x, go_y);
    }

    /* Update ScriptEngine and ScrollPanel child sprite positions */
    if (this->dim_flag == 0) {
        if (this->script_engine_active != 0) {
            se_vmove(this, x - this->script_engine_offset, y + 14);
        }
        if (this->scroll_panel_offset != 0) {
            sp_vmove(this, x - this->scroll_panel_offset, y + 14);
        }
    }
    else {
        int sprite_w = static_cast<const RESDATA*>(this->resource)->frame_width;

        if (this->script_engine_active != 0) {
            se_vmove(this, sprite_w + x, y + 14);
        }
        if (this->scroll_panel_offset != 0) {
            sp_vmove(this, sprite_w + x, y + 14);
        }
    }

    /* Update drag_rect based on mode */
    if (this->mode == 0) {
        RECT r;
        SetRect(&r,
            this->screen_rect.left   + 0x18,
            this->screen_rect.top    + 3,
            this->screen_rect.left   + 0x2C,
            this->screen_rect.top    + 0x0D);
        this->drag_rect = r;
    }
    else {
        this->drag_rect = this->sub_entity.screen_rect;
    }

    /* Update cursor position if actively dragging */
    {
        int prev_x = this->screen_rect.left;
        int prev_y = this->screen_rect.top;
        if (this->drag_active != 0 && (x != prev_x || y != prev_y)) {
            int screen_x = (x - g_viewport_x) + this->drag_offset_x;
            int screen_y = (y - g_viewport_y) + this->drag_offset_y;

            POINT pt;
            pt.x = screen_x;
            pt.y = screen_y;
            ClientToScreen(reinterpret_cast<CGWND*>(g_main_window)->hWnd, &pt);
            SetCursorPos(pt.x, pt.y);

            g_last_cursor_pos =
                ((static_cast<uint16_t>(y - g_viewport_y) +
                  static_cast<uint16_t>(this->drag_offset_y)) << 16) |
                (static_cast<uint16_t>(x - g_viewport_x) +
                 static_cast<uint16_t>(this->drag_offset_x));

            Game_CheckScreensaverTimeout(g_game);            /* 0x410A20 */
        }
    }

    /* Update tooltip position if one exists */
    if (this->tooltip_handle != 0) {
        tooltip_vmove(this->tooltip_handle,
            this->screen_rect.left + 0x32,
            this->screen_rect.top  + 0x32);
    }
}

/* ================================================================== */
/* HitTest — vtable[4]                                                  */
/* Address: 0x44A0C0                                                    */
/* ================================================================== */

int ScriptedObject::HitTest(int x, int y)
{
    int16_t mode = this->mode;

    if (mode == 1 || mode == 2) return 0;
    if (mode == 3) {
        if (*(reinterpret_cast<uint8_t*>(g_trainstation_window) + 0x1BC) != 0) return 0;
    }

    if (this->drag_active != 0) {
        this->drag_active = 0;
        return 1;
    }

    /* Own slot 21 (+0x54) secondary hit-test (GetDragOffset). */
    int result = this->GetDragOffset(x, y);
    if (result != 0 && (mode == 0 || mode == 3)) {
        this->drag_offset_x = g_drag_start_x - this->screen_rect.left;
        this->drag_offset_y = g_drag_start_y - this->screen_rect.top;
        this->drag_active = 1;
        return 1;
    }

    if (mode != 0) {
        if (this->script_engine_active != 0) {
            if (se_ptinrect(this, x, y) != 0) {
                /* Verified against 0x44A0C0: script_engine_active is
                   deliberately re-read AFTER se_hittest returns, not before
                   — se_hittest's own vtable[4] dispatch can deactivate the
                   embedded script-engine sub-object as a side effect (e.g.
                   closing a modal build-tool dialog on this click), and
                   this->dim_flag is only set when that side effect fired.
                   Not dead code / not a redundant re-test. */
                uint8_t hitResult = se_hittest(this, x, y);
                if (this->script_engine_active != 0) return hitResult;
                this->dim_flag = 1;
                return hitResult;
            }
        }

        if (this->scroll_panel_active != 0) {
            if (sp_ptinrect(this, x, y) != 0) {
                /* Same re-read pattern as above, mirrored for the scroll-
                   panel sub-object (verified against 0x44A0C0). */
                uint8_t hitResult = sp_hittest(this, x, y);
                if (this->scroll_panel_active != 0) return hitResult;
                this->dim_flag = 1;
                return hitResult;
            }
        }

        /* Children linked list: Panel::HitTestChildren is the real,
           canonical method for this (0x4549E0), not a raw free function. */
        return static_cast<int>(this->HitTestChildren(x, y));
    }

    /* Idle state: try own hit-test -> enter build mode. Verified against
       0x44A0C0: this dispatches through the object's own vtable[2]
       (0x449CE0), the same slot documented as IsDragging above — not the
       inherited GameObject::PtInRect at a different slot (see CheckClick's
       equivalent fix/comment above 0x449D00). */
    if (g_disable_input == 0) {
        if (this->IsDragging(x, y)) {
            this->EnterBuildMode(1);
            return 1;
        }
    }

    return 0;
}

/* ================================================================== */
/* HandleToolClick — vtable[17]                                         */
/* Address: 0x44A250                                                    */
/* ================================================================== */

uint32_t ScriptedObject::HandleToolClick(TrackPiece* toolObj, int x, int y)
{
    if (toolObj == nullptr) return 0;
    if (toolObj->render_enabled == 0) return 0;

    if (!toolObj->PtInRect(x, y)) return 0;

    int32_t toolType = toolObj->resource->resource_id;
    int32_t toolIndex = toolType - 0x2403;

    switch (toolIndex) {
    case 0:  /* 0x2403 */
    case 1:  /* 0x2404 */
        if (toolObj->zoom_level != 1) {
            toolObj->SetZoom(1);
            this->dim_flag = 0;
            se_handle_drag(this, toolObj, 0);
            return 1;
        }
        break;

    case 2:  /* 0x2405 */
        if (toolObj->zoom_level != 1) {
            toolObj->SetZoom(1);
            this->dim_flag = 0;
            se_handle_drag(this, toolObj, 0);
            return 1;
        }
        break;

    case 3:  /* 0x2406 — Build mode toggle */
        if (toolObj->zoom_level != 1) {
            toolObj->SetZoom(1);
            CGWND_SetBuildMode(0);
            return 1;
        }
        HelpWnd_PlayNarration(g_audio_mgr, 7, 0x2406);
        toolObj->SetZoom(2);
        CGWND_SetBuildMode(1);
        return 1;

    case 6:  /* 0x2409 */
        if (toolObj->zoom_level != 1) {
            toolObj->SetZoom(1);
            this->dim_flag = 0;
            UIPANEL_ScrollPanel_HandleDrag(
                this->scroll_panel_prefix,
                static_cast<int32_t>(reinterpret_cast<uintptr_t>(toolObj)), 0);
            return 1;
        }
        break;

    default:
        break;
    }

    return 0;
}

/* ================================================================== */
/* UpdateToolState — Per-frame tool zoom update (vtable[20])           */
/* Address: 0x44AC20                                                    */
/* ================================================================== */

uint32_t ScriptedObject::UpdateToolState(TrackPiece* tool)
{
    if (tool == nullptr) {
        return 0;
    }

    /* Decrement frame timer (prev_frame) if non-negative */
    int16_t timer = tool->prev_frame;
    if (timer >= 0) {
        timer--;
        tool->prev_frame = timer;
    }

    /* Auto-reset zoom to 1 when timer expires and zoom==2 */
    if (timer == 0 && tool->zoom_level == 2) {
        tool->SetZoom(1);
    }

    /* Dispatch by tool type (resource->resource_id) */
    int tool_type = tool->resource->resource_id;

    switch (tool_type) {
    case 0x2407:  /* New Game tool */
        if (timer == 0) {
            if (HelpWnd_PlayNarration(g_audio_mgr, 7, 0x2407) == 0) {
                INPUT_NewWorld(&g_input_mgr);                 /* 0x41E120 */
            }
        }
        break;

    case 0x2408:  /* Load Game tool */
        if (timer == 0) {
            HelpWnd_PlayNarration(g_audio_mgr, 7, 0x2408);
            INPUT_LoadWorld(&g_input_mgr, "curr");            /* 0x41D320 */
        }
        break;

    case 0x240B:  /* Building placement toggle */
        if (g_allow_building_placement == 1) {
            tool->SetZoom(2);
        } else {
            tool->SetZoom(1);
        }
        break;

    case 0x240C:  /* Fullscreen toggle */
        if (g_is_fullscreen != 0 && g_world_width <= g_screen_width) {
            tool->SetZoom(2);
        } else {
            tool->SetZoom(1);
        }
        break;

    case 0x240D:  /* Scenario mode indicator */
        if (*reinterpret_cast<const int*>(
                reinterpret_cast<const uint8_t*>(g_netman) + 0x7C4) == 2) {
            tool->SetZoom(3);
        } else if (timer == 0) {
            tool->SetZoom(2);
            HelpWnd_PlayNarration(g_audio_mgr, 0, 0);
        } else {
            tool->SetZoom(1);
        }
        break;

    case 0x240E:  /* Mute toggle */
        if (g_audio != nullptr && *reinterpret_cast<const uint8_t*>(
                reinterpret_cast<const uint8_t*>(g_audio) + 0xB4) == 0) {
            tool->SetZoom(1);
        } else {
            tool->SetZoom(2);
        }
        break;

    case 0x240F:  /* Exit build mode */
        if (timer == 0) {
            HelpWnd_PlayNarration(g_audio_mgr, 7, 0x240F);
            this->EnterBuildMode(0);
        }
        break;

    default:
        break;
    }

    return 0;
}

/* ================================================================== */
/* EnterBuildMode — Enter/exit build mode                                */
/* Address: 0x44A9D0                                                    */
/* ================================================================== */

void ScriptedObject::EnterBuildMode(uint8_t enter)
{
    if (enter == 0) {
        /* === EXIT BUILD MODE === */
        if (this->update_child_flags != 0) {
            this->OnUpdateChild();

            TrackPiece* child = static_cast<TrackPiece*>(this->child_surface);
            this->update_child_flags = 0;

            if (this->script_engine_active != 0) {
                se_handle_drag(this, child, 0);
            }

            if (this->scroll_panel_offset != 0) {
                UIPANEL_ScrollPanel_HandleDrag(
                    this->scroll_panel_prefix,
                    static_cast<int32_t>(reinterpret_cast<intptr_t>(child)), 0);
            }

            this->dim_flag = 0;
            CGWND_SetBuildMode(0);

            /* Reset all track piece zooms in the linked list, walked via
               each TrackPiece's own +0x28 field. PRE-EXISTING LANDMINE, not
               introduced by this pass: TrackPiece.h names this offset
               `sub_resource` and types it `int32_t` (matching the original
               x86 layout), but every child-list traversal in both this
               file and the now-retired world/scriptengine.cpp already read
               it as a raw pointer -- i.e. a pointer-truncation hazard on
               this 64-bit host if that field is ever populated by a real
               64-bit TrackPiece* on this host (unverified either way).
               Retyping `sub_resource` is a separate, cross-file TrackPiece.h
               change (used by game/TrackPiece.cpp's own destructor logic
               too) outside this reconciliation's scope; ported the existing
               access pattern verbatim rather than silently changing it. */
            while (child != nullptr) {
                int child_type = child->resource->resource_id;
                switch (child_type) {
                case 0x2403: case 0x2404: case 0x2405:
                case 0x2406: case 0x2409: case 0x240A:
                    child->SetZoom(1);
                    break;
                }
                child = *reinterpret_cast<TrackPiece* const*>(
                    reinterpret_cast<uint8_t*>(child) + 0x28);
            }

            if (g_in_build_mode != 0) {
                INPUT_SaveCurrentWorld(&g_input_mgr, "curr");
            }

            this->Init(0x2400, 2, 0);
            this->mode = 2;   /* exiting build mode — see class doc comment */

            if (this->tooltip_handle != 0) {
                tooltip_set_state(this->tooltip_handle, 2);
            }

            if (g_audio != nullptr) {
                GameAudio_UpdateVolume(g_audio, 0);
            }
        }
    }
    else {
        /* === ENTER BUILD MODE === */
        if (this->update_child_flags == 0) {
            this->Init(0x2400, 1, 0);
            this->mode = 1;

            this->MoveTo(
                this->screen_rect.left - 0x31,
                this->screen_rect.top  - 0x2F);

            CGWND_SetMode(4);

            if (this->tooltip_handle != 0) {
                UI_DestroyTooltip(g_tooltip_mgr, this->tooltip_handle);
                this->tooltip_handle = 0;
            }

            /* Tooltip creation guard is Entity::visible (+0x24) — a prior
               pass here mis-read it as GameObject::initialized (+0x18). */
            if (this->visible != 0) {
                this->tooltip_handle = static_cast<int32_t>(reinterpret_cast<intptr_t>(
                    UI_CreateTooltip(g_tooltip_mgr, 0x3879, 1,
                    this->screen_rect.left + 0x32,
                    this->screen_rect.top  + 0x32)));
            }

            if (g_audio != NULL) {
                GameAudio_UpdateVolume(g_audio, 1);
            }
        }
    }
}

/* ================================================================== */
/* Update — Per-frame update (vtable[10])                                */
/* Address: 0x4497A0                                                    */
/* ================================================================== */

void ScriptedObject::Update()
{
    int16_t mode = this->mode;

    /* State 1: In-world — clamp to world bounds via recursive MoveTo
       (the original dispatches through this object's own vtable slot
       +0x0C, which is MoveTo, not a generic "SetPosition").
       BLOCKER (2026-08-16, not resolved by this pass): direct disassembly
       of 0x4497A0 shows each of the four clamp branches below scales its
       "-1 pixel" overshoot by a genuine floating-point ratio (FILD/FDIVP/
       FMUL against `this->frame_index` and a field at `resource->
       anim_table + 0x1A`, then truncated via a real float-to-int call at
       0x466D30) — NOT a flat "-1", which is what this port (and the
       prior world/scriptengine.cpp transcription it replaces) both use.
       Re-deriving the exact formula (what `anim_table+0x1A` represents,
       and how the ratio is applied) needs a dedicated pass; the plain
       "-1" fallback below is a bounded approximation of the real
       behavior, not a verified port. */
    if (mode == 1) {
        if (g_is_town_mode != 0) {
            Town_SelectBuilding(g_town_view, 0);
        }
        if (g_ddraw_active_flag != 0) {
            DDRAW_SelectBuilding(g_ddraw_building, 0);
        }

        if (this->screen_rect.left < 0) {
            this->MoveTo(this->screen_rect.left - 1, this->screen_rect.top);
        }
        if (g_world_width < this->screen_rect.right) {
            this->MoveTo(this->screen_rect.right - 1 + this->screen_rect.left,
                         this->screen_rect.top);
        }
        if (this->screen_rect.top < 0) {
            this->MoveTo(this->screen_rect.left, this->screen_rect.top - 1);
        }
        if (g_world_height - this->source_rect.bottom < this->screen_rect.top) {
            this->MoveTo(this->screen_rect.left,
                         this->screen_rect.top + this->source_rect.bottom - 1);
        }
    }

    /* States 1 or 2: Animation update + tooltip + entity update */
    if (mode == 1 || mode == 2) {
        this->Entity::Update();

        if (this->tooltip_handle != 0) {
            /* ABI_BOUNDARY: Panel::tooltip_handle is inherited as an
               int32_t (pre-existing Panel.h field type, not introduced by
               this pass) rather than a typed Entity* — widening it is a
               tree-wide Panel change out of scope here. */
            reinterpret_cast<Entity*>(static_cast<intptr_t>(this->tooltip_handle))->Update();
        }

        this->OnUpdateChild();

        /* Check animation frame completion for state transitions.
           frame_index (+0x54) is compared to the resource's per-state
           frame table — NOT anim_index (+0x28), a different field. */
        int32_t frameIndex = this->frame_index;
        const RESDATA* frameData = static_cast<const RESDATA*>(this->resource);
        const uint16_t* animTable = reinterpret_cast<const uint16_t*>(frameData->anim_table);
        uint16_t startFrame = animTable[0x19];

        if (frameIndex == static_cast<int32_t>(startFrame)) {
            /* Animation reached start — return to idle */
            this->Init(0x2401, -1, 0);
            this->MoveTo(this->screen_rect.left + 0x31, this->screen_rect.top + 0x2F);
            this->mode = 0;

            if (this->tooltip_handle != 0) {
                UI_DestroyTooltip(g_tooltip_mgr, this->tooltip_handle);
            }
            if (this->visible != 0) {
                Entity* tooltip = UI_CreateTooltip(
                    g_tooltip_mgr, 0x3887, 0,
                    this->screen_rect.left + 0x32,
                    this->screen_rect.top + 0x32);
                this->tooltip_handle = static_cast<int32_t>(reinterpret_cast<intptr_t>(tooltip));
            }
            g_active_panel = nullptr;
            CGWND_SetMode(3);

            SetRect(&this->drag_rect,
                this->screen_rect.left + 0x18,
                this->screen_rect.top + 3,
                this->screen_rect.left + 0x2C,
                this->screen_rect.top + 0x0D);
            TileMap_InvalidateRect(g_tilemap,
                this->drag_rect.left, this->drag_rect.top,
                this->drag_rect.right, this->drag_rect.bottom);

        } else {
            uint16_t endFrame = animTable[0x1A / 2];
            if (frameIndex == static_cast<int32_t>(endFrame)) {
                /* Animation reached end — switch to placed state */
                this->update_child_flags = 1;
                this->OnUpdateChild();
                this->mode = 3;

                g_active_panel = this;
                this->drag_rect = this->sub_entity.screen_rect;
                TileMap_InvalidateRect(g_tilemap,
                    this->sub_entity.screen_rect.left, this->sub_entity.screen_rect.top,
                    this->sub_entity.screen_rect.right, this->sub_entity.screen_rect.bottom);

                /* Update children via linked list at Panel::child_surface
                   (+0xD0). Original dispatches through the CHILD's own
                   vtable slot +0x20 with no arguments — on a TrackPiece
                   that is Render() (vtable[8], 0x40D340), not SetVisible
                   (a different slot on the Entity/Panel hierarchy that
                   TrackPiece does not share, since TrackPiece derives
                   directly from GameObject, not Entity). */
                for (TrackPiece* child = static_cast<TrackPiece*>(this->child_surface);
                     child != nullptr;
                     child = *reinterpret_cast<TrackPiece* const*>(
                         reinterpret_cast<uint8_t*>(child) + 0x28)) {
                    child->Render();
                }

                if (*reinterpret_cast<const int32_t*>(
                        reinterpret_cast<const uint8_t*>(g_netman) + 0x7C4) != 2) {
                    HelpWnd_PlayNarration(g_audio_mgr, 6, 0);
                }
            }
        }
    }

    /* Dragging: follow cursor. Driven entirely by Panel::drag_active
       (+0x90) — a field wholly separate from `mode`. */
    if (this->drag_active == 1) {
        this->MoveTo(g_cursor_world_x - this->drag_offset_x,
                    g_cursor_world_y - this->drag_offset_y);
        return;
    }

    /* State 0 (idle): hover detection */
    if (mode == 0) {
        int hitResult = this->GetDragOffset(g_cursor_world_x, g_cursor_world_y);
        if (hitResult == 0 && this->drag_active != 1) {
            if (this->anim_index == 1) {
                this->StopSound(0);
            } else {
                this->Entity::Update();
            }
        } else if (this->anim_index != 1) {
            this->StopSound(1);
        }
    }

    /* State 3 (placed): dispatch to sub-objects */
    if (mode != 3) return;

    int hoverResult = this->GetDragOffset(g_cursor_world_x, g_cursor_world_y);
    if (hoverResult == 0 && this->drag_active == 0) {
        if (this->sub_entity.anim_index == 0) {
            goto update_children;
        }
        /* Confirmed via direct disassembly (0x449B48-0x449B79): dispatches
           through sub_entity's OWN vtable slot 7 (+0x1C) = StopSound, not
           SetVisible (a different slot on the Entity hierarchy). */
        this->sub_entity.StopSound(0);
    } else {
        if (this->sub_entity.anim_index == 1) {
            goto update_children;
        }
        this->sub_entity.StopSound(1);
    }
    TileMap_InvalidateRect(g_tilemap,
        this->drag_rect.left, this->drag_rect.top,
        this->drag_rect.right, this->drag_rect.bottom);

update_children:
    for (TrackPiece* child = static_cast<TrackPiece*>(this->child_surface);
         child != nullptr;
         child = *reinterpret_cast<TrackPiece* const*>(
             reinterpret_cast<uint8_t*>(child) + 0x28)) {
        this->UpdateToolState(child);
    }

    if (this->script_engine_active != 0) {
        se_update(this);
    }

    if (this->scroll_panel_active != 0) {
        sp_update(this);
    }
}

/* ================================================================== */
/* Stub — not yet decompiled                                            */
/* ================================================================== */

void ScriptedObject::OnUpdateChild()
{
    /* TODO: decompile 0x454890 — delegates to Panel::UpdateChild.
       Neither this file nor the retired world/scriptengine.cpp ever had
       a real body for this; a pre-existing gap, not one introduced here. */
}

int ScriptedObject::InitState()
{
    /* TODO: decompile 0x44ADF0 — pre-existing gap (see OnUpdateChild). */
    return 0;
}
