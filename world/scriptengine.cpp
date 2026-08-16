/**
 * scriptengine.cpp — ScriptEngine implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * The ScriptEngine is a RESDATA-derived class that manages script-like
 * animation/callback dispatch (see scriptengine.h's BLOCKER note on its
 * own vtable/inheritance for an open question this file does not resolve).
 *
 * RECONCILIATION (2026-08-16): this file's own `RESDATA_ScriptedObject`
 * class — a duplicate reconstruction of `game/ScriptedObject.h`'s
 * `ScriptedObject` — has been removed; see scriptengine.h and
 * game/ScriptedObject.h for the full trail.
 *
 * Key field offsets (byte offsets, verified from disassembly):
 *   ScriptEngine: vtable=+0x00, CS=+0x04 (sizeof 0x18 on Win9x)
 */

// Status: TRANSCRIBED

#include "scriptengine.h"
#include "../resources/ResourceManager.h"  /* for PlaySoundAt, etc. */
#include "../core/Entity.h"                /* embedded resource-backed entity fields */
#include "../core/CGWND.h"                 /* for CGWND::hWnd (g_main_window+0x08) */
#include "../game/TrackPiece.h"            /* ScriptEngine_Run/HandleToolClick's real operand type */
#include "../ui/UI_ChildWindow.h"
#include <new>                              /* placement new */

/* ================================================================== */
/* External CRT / Windows helpers                                      */
/* ================================================================== */
/* Win32 imports retain C linkage; game-internal functions use C++ linkage. */
extern "C" {
    void __stdcall InitializeCriticalSection(void* lpCriticalSection);
    void __stdcall DeleteCriticalSection(void* lpCriticalSection);
    void __stdcall EnterCriticalSection(void* lpCriticalSection);
    void __stdcall LeaveCriticalSection(void* lpCriticalSection);
}
    void __fastcall RESDATA_BaseInit(void* ptr);        /* 0x4544E0 */
    void __fastcall RESDATA_DtorBase(void* ptr);        /* 0x454630 */
    void GLOBAL_free(void* ptr);                        /* 0x465CD0 */
    void __thiscall GameObject_BaseCtor(void* obj, int32_t x, int32_t y,
                                        int32_t w, int32_t h); /* 0x405790 */
    /* Entity::Update/Draw/PtInRect are invoked through typed Entity
     * methods below; recovered bodies are 0x405C40, 0x405E60, 0x436A10. */
    void __fastcall UIPANEL_InitScrollPanel(void* panel); /* 0x427370 */
    void __fastcall UIPANEL_ScrollPanel_Dtor(void* panel); /* 0x427460 */
    void __fastcall Panel_DtorBody(void* panel);        /* 0x4545A0 */
    /* TODO(evidence-insufficient): address corrected 0x427BD0 -> 0x4277D0
     * (0x427BD0 is mid-body; Ghidra resolves it to the function starting at
     * 0x4277D0). The `param` parameter is a genuine pointer, not an int32_t:
     * several call sites inside RESDATA_ScriptedObject::HandleToolClick pass
     * a raw child-list node pointer here (see HandleToolClick's `pvVar4`
     * dispatch sites in the Ghidra decompilation of 0x44A250), and the real
     * body at 0x4277D0 stores it verbatim into a 4-byte "last active tool"
     * slot at this+0xD4 without ever treating it as a small integer. On this
     * 64-bit host, passing a real pointer through an int32_t parameter is a
     * truncating-pointer-arithmetic landmine (loses the pointer's upper 32
     * bits). NOT fixed here: game/ScriptedObject.cpp:90 declares this same
     * symbol as `(void*, int, int)` and shared/stubs_impl.cpp:518 /
     * shared/defsym_stubs.cpp:105 each define it differently (one with zero
     * parameters) — retyping only this declaration would silently rebind
     * this TU's call to a different symbol than intended. Needs a dedicated
     * pass auditing all four declarations/definitions together; tracked in
     * docs/landmine-sweep-worklist.md. Left as int32_t/truncating here. */
    void __thiscall UIPANEL_ScrollPanel_HandleDrag(void* panel, int32_t param, int32_t action); /* @ 0x4277D0 */
    void __thiscall RESDATA_SetPosition(void* obj, int32_t x, int32_t y);  /* @ 0x44E700 */
    /* TODO(deferred — see docs/landmine-sweep-worklist.md): address corrected
     * 0x44E6C0 -> 0x4549E0 (0x44E6C0 falls inside World_RenderAll; the real
     * function, confirmed by Ghidra symbol lookup, decompiles cleanly and
     * already has a typed C++ implementation at Panel::HitTestChildren,
     * game/Panel.cpp:510). NOT wired up to call it here: two other TUs
     * (graphics/DDRAW.cpp:102 @ "0x44A0C0" - actually
     * RESDATA_ScriptedObject::HitTest itself; town/Town.cpp:171 @ "0x44B200"
     * - actually RESDATA_ScriptedObject::DtorChain) declare the same symbol
     * at two more wrong addresses with mismatched return types. Because
     * Itanium mangling ignores return type, all three declarations plus
     * shared/stubs_impl.cpp:431's `void`-returning stub collide on one
     * mangled symbol, and every real call site today silently binds to that
     * stub's assert(0) instead of running. Wiring a forwarder to
     * Panel::HitTestChildren is NOT a safe drop-in fix on its own: that
     * method dispatches child->vtable[0x11] as a `void**` array index, which
     * is byte offset 0x88 on this 64-bit (8-byte vtable entry) host, not the
     * original x86 byte offset 0x44 — the same vtable-byte-offset-
     * misalignment landmine class this cleanup sweep watches for. Fixing
     * this cluster requires resolving slot 17's real signature first;
     * tracked as a dedicated follow-up, not done in this commit. */
    char __thiscall RESDATA_HitTestChildren(void* obj, int32_t x, int32_t y); /* @ 0x4549E0 */
    /* Address corrected: 0x44E530 falls inside World_ProcessEvents, not
     * RESDATA_CreateChildSprite; confirmed via Ghidra at 0x4546D0, which
     * also confirms the real return type is `void*` (not `void` — an ODR
     * mismatch against shared/defsym_stubs.cpp's now-corrected definition).
     * This declaration's second param is really the resource pointer
     * (passed to UI_IsBitmapReady/TrackPiece_Ctor/RESMGR_SoundObject_Ctor
     * in the disassembly), not a bare resId int, but that distinction is
     * left alone here since this whole call path (RESDATA_ScriptedObject::
     * Start) has zero callers in this tree today. */
    void* __thiscall RESDATA_CreateChildSprite(void* obj, int32_t resId, int32_t param3, int32_t param4); /* @ 0x4546D0 */
    int32_t __thiscall ResourceManager_GetById(void* resmgr, int32_t resId); /* @ 0x446EA0 */
    /* Addresses corrected: 0x428DA0/0x428E40 do not exist as functions
     * (confirmed via Ghidra — no function, zero xrefs at either address).
     * The real UI_CreateTooltip/UI_DestroyTooltip are UI_Manager::
     * createTooltip/destroyTooltip at 0x423C50/0x423D20 (ui/UI_Utils.cpp).
     * `param`'s real width is int16_t, not int32_t (matches the one
     * canonical UI_CreateTooltip overload used tree-wide — see
     * ui/UIEntity.cpp; this file's own previous int32_t declaration was a
     * distinct, redundant overload bound to a since-removed separate stub
     * in shared/stubs_impl.cpp). UI_DestroyTooltip's real return type is
     * void, not void* (unused by any caller here either way). */
    Entity* __thiscall UI_CreateTooltip(void* mgr, int32_t resId, int16_t param, int32_t x, int32_t y); /* @ 0x423C50 */
    void __thiscall UI_DestroyTooltip(void* mgr, int32_t tooltipId);  /* @ 0x423D20 */
    /* TODO(deferred — see docs/landmine-sweep-worklist.md): address corrected
     * 0x40DD90 -> 0x40D170. Ghidra confirms every `TrackPiece_SetZoom(...)`
     * call this file's own disassembly resolves to (e.g. inside
     * ScriptEngine_Run and HandleToolClick below) targets 0x40D170, which is
     * TrackPiece::SetZoom (game/TrackPiece.cpp) — a real `void __thiscall
     * (TrackPiece* this, int16_t zoom)`, not this declaration's `int32_t
     * (void*, int32_t)`. shared/stubs_impl.cpp:216 is the only actual
     * definition bound to this declared symbol name/signature, so every
     * call site under this name (here and in game/Panel.cpp,
     * ui/UIPANEL_Draw.cpp) is currently a silent-wrong-stub landmine that
     * asserts if reached. NOT fixed here: retyping `engine`/`toolObj` to
     * TrackPiece* below does not change this — TrackPiece* implicitly
     * converts to void* at these call sites, so behavior is unchanged.
     * Fixing the stub itself is a separate, wider-blast-radius pass (it
     * would newly activate currently-dormant call paths in those other
     * files too); tracked as a dedicated follow-up. */
    int32_t __thiscall CGWND_TrackPiece_SetZoom(void* obj, int32_t zoom); /* @ 0x40D170 */
    /* CGWND_SetBuildMode/CGWND_SetMode are declared in core/CGWND.h (now
     * included above for CGWND::hWnd) with no calling-convention
     * annotation, matching their real implementation in core/CGWND.cpp.
     * This file's own `__thiscall`-annotated local redeclarations were a
     * calling-convention mismatch invisible on this native 64-bit host
     * (where __thiscall/__fastcall/etc. are empty macros — see
     * stubs/compat.h) but a hard "ambiguating declaration" error on the
     * real 32-bit mingw typecheck build, where they're genuinely different
     * ABIs. Removed in favor of core/CGWND.h's declarations. CGWND_SetMode
     * previously had a real call-0 landmine fixed here (real def:
     * core/CGWND.cpp, void(int), 0x408130 — a bogus 0x40DF50 address and a
     * void* param mismatch had silently bound it to shared/link_stubs.cpp's
     * void* no-op stub); core/CGWND.h's declaration carries the same
     * corrected signature. */
    void __thiscall GameAudio_UpdateVolume(void* audio, uint8_t mute);    /* @ 0x413150 */
    int32_t __thiscall HelpWnd_PlayNarration(void* audioMgr, int32_t page, int32_t param); /* @ 0x4510B0 */
    void __fastcall INPUT_ExitGame(void* obj, uint32_t resId, int32_t strPtr); /* @ 0x41E570 */
    void __thiscall INPUT_CreateEditControl(void* obj);   /* @ 0x41ED80 */
    uint8_t __fastcall INPUT_EditWndProc(void* obj, void* stream); /* @ 0x41EE50 */
    uint8_t __thiscall CGWND_TrackPiece_UpdateAnim(void* obj);  /* @ 0x40D2F0 */

    /* Globals */
    /* g_resmgr is declared in ResourceManager.h as ResourceManager */
    /* g_tilemap is declared in tilemap.h (now included above) as
     * `extern TileMap* g_tilemap;` — this file previously redeclared it as
     * `void*`, an undetected type mismatch across TUs until this file
     * started including tilemap.h directly (for RESDATA_ScriptedObject_
     * Dispatch's declaration) and the compiler could see both at once. */
    /* Address corrected: 0x4FD220, not 0x4AA500 (matches every other
     * declaration of this global tree-wide, and DDRAW.cpp's definition). */
    class UI_Manager;
    extern UI_Manager* g_tooltip_mgr;         /* @ 0x4FD220 */
    extern void* g_audio;                      /* @ 0x4FD3BC */
    extern void* g_audio_mgr;                  /* @ 0x4A9E0C */
    /* Real address 0x4852A0 (see core/GameView.h). The prior comment here,
     * 0x4A9E1C, has ZERO xrefs anywhere in the binary (confirmed via
     * get_xrefs_to) — fabricated, not just stale. See PROGRESS.md's
     * g_town_view item. */
    extern void* g_town_view;                  /* @ 0x4852A0 */
    extern void* g_ddraw_active;               /* @ 0x4AA4A4 */
    extern void* g_ddraw_building;             /* @ 0x4FD758 */
    extern void* g_town_mode;                  /* @ 0x485490 */
    extern void* g_netman;                     /* @ 0x4FD3AC */
    extern int32_t g_world_width;              /* @ 0x4AA42C */
    extern int32_t g_world_height;             /* @ 0x4A9E28 */
    extern int32_t g_cursor_world_x;           /* @ 0x4AA410 */
    extern int32_t g_cursor_world_y;           /* @ 0x4AA414 */
    extern int32_t g_drag_start_x;             /* @ 0x4AA418 */
    extern int32_t g_drag_start_y;             /* @ 0x4AA41C */
    /* g_disable_input is declared in tilemap.h as `extern uint8_t
     * g_disable_input;` — this file previously redeclared it as int32_t,
     * a latent type mismatch (a 4-byte read of a real 1-byte global) only
     * surfaced once tilemap.h's own declaration became visible here. */
    extern int32_t g_demo_mode;                /* @ 0x4A9918 */
    extern int32_t g_screen_width;             /* @ 0x4AA420 */
    extern uint8_t g_is_fullscreen;            /* @ 0x4AA418 */
    extern uint8_t g_allow_building_placement; /* @ 0x4AA428 */
    extern int32_t g_in_build_mode;            /* @ 0x4A9E10 */
    extern int32_t g_viewport_x;               /* @ 0x4AA43C */
    extern int32_t g_viewport_y;               /* @ 0x4AA440 */
    /* Real type is `void*` (see shared/stubs_impl.cpp's definition and
     * every other TU's declaration: ui/UIPANEL.cpp, town/Town.cpp,
     * graphics/DDRAW.h, game/ScriptedObject.cpp). This file previously
     * declared it `int32_t` — an undetected type mismatch that only
     * surfaced as a pointer-truncation warning once this function's
     * `g_active_panel = this;` assignment (below) was reached; corrected. */
    extern void* g_active_panel;               /* @ 0x4AA428 */
    extern void* g_trainstation_window;        /* @ 0x4A9E04 */
    extern int32_t g_game;                     /* @ 0x4FCCCC */
    void __thiscall Game_CheckScreensaverTimeout(int32_t* game); /* @ 0x40E160 */
extern "C" {
    /* Real signature (matches stubs/windows.h / graphics/sdl3_window.h,
     * the TU that actually implements this on non-Windows hosts): `BOOL
     * __stdcall (HWND, POINT*)`. This file previously declared it as
     * `void __stdcall (void*, void*)`, and shared/stubs_impl.cpp separately
     * declares `int(void*, void*)` — an ODR-mismatch cluster of the same
     * shape already fixed for other Win32 imports in this series; the
     * return value isn't used at this file's one call site either way. */
    BOOL __stdcall ClientToScreen(HWND hwnd, POINT* point);
    void __stdcall SetCursorPos(int32_t x, int32_t y);
}

    /* External helpers used by HandleEvent */
    int32_t __fastcall CRT_sprintf_buf(void* buf, const char* format, ...); /* @ 0x00467F60 */
    void* __thiscall AssetMgr_LoadFile(void* mgr, void* name, int32_t* outSize);  /* @ 0x00457C00 */
    void CRT_free(void* ptr);                       /* @ 0x00465280 */
    void* operator_new(uint32_t size);              /* @ 0x00465CE0 */
    /* The WIN32_StreamOpen/OpenPath/Destroy(Immediate)/WNDPROC_StreamCleanup
     * declarations formerly here (addresses 0x461600-0x460D50) were dead
     * (nothing in this file calls them) AND wrong: those addresses
     * actually belong to unrelated functions (WIN32_QueueAsyncTask/
     * WIN32_SendNetworkData; confirmed via Ghidra decompile), not this
     * family at all — same stale-scaffolding pattern found and removed
     * in resources/ResourceManager.cpp. A future pass adding real
     * WIN32_Stream usage here should use resources/Win32Stream.h's real
     * class (RAII construction/destruction) and its real addresses
     * (WIN32_StreamOpen 0x463890, WIN32_StreamOpenPath 0x463AA0,
     * WIN32_StreamDestroyImmediate 0x463B10) — WIN32_StreamDestroy
     * (0x463A80) no longer exists as a callable symbol at all (see
     * Win32Stream.h's doc comment on that address). */
    /* `engine` is a misnomer inherited from this function's address label:
     * Ghidra confirms the zoom-setting call inside resolves to
     * TrackPiece::SetZoom (0x40D170), and every accessed offset (+0x2C
     * flags, +0x48 zoom_level) matches game/TrackPiece.h's documented
     * layout, not ScriptEngine's. Zero callers tree-wide today; retyped
     * without renaming since the address label is Ghidra's own and no
     * stronger name is evidenced. */
    int CDECL ScriptEngine_Run(TrackPiece* tool, int x, int y);
extern "C" {
    void __stdcall SetRect(void* lprc, int32_t left, int32_t top, int32_t right, int32_t bottom);
}
    void __thiscall TileMap_InvalidateRect(void* tilemap, int32_t left, int32_t top, int32_t right, int32_t bottom);

/* PanelDispatchView/EntityDispatchView/ScriptEngineDragView (local
 * partial-layout vtable views) removed 2026-08-16 — they existed only to
 * support this file's own retired `RESDATA_ScriptedObject` method bodies
 * (see below) and ScriptEngine_Run's now-typed `tool->PtInRect()`/
 * `tool->SetZoom()` calls. Nothing in this file uses them anymore. */

/* ==================================================================== */
/* ScriptEngine::ScriptEngine — Constructor                              */
/* Address: 0x4493A0                                                    */
/* __fastcall (ECX=this)                                                */
/*                                                                      */
/* Compiler-managed base table is at 0x4782A4; initializes critical    */
/* section for thread safety. CRITICAL_SECTION starts at +0x04.        */
/* ==================================================================== */
ScriptEngine::ScriptEngine()
{
/* In the binary: sets vtable here. Compiler-managed in natural C++. */
    InitializeCriticalSection(&this->cs);  /* CRITICAL_SECTION at +0x04 */
}

/* ==================================================================== */
/* ScriptEngine::Init — Full initialization                              */
/* Address: 0x44E8D0                                                   */
/* __fastcall (ECX=this)                                                */
/*                                                                      */
/* Sets up RESDATA base, installs full vtable, clears state flags.     */
/* ==================================================================== */
void __fastcall ScriptEngine::Init()
{
    RESDATA_BaseInit(this);
/* In the binary: sets vtable here. Compiler-managed in natural C++. */
    this->script_state = 0;                     /* +0x1F */
    this->active_flag = 0;                      /* +0x1C */
    this->field_2C = 0;                         /* +0x2C */
    this->field_30 = 0;                         /* +0x30 */
    this->script_flags = 0xB;                     /* init type = 0xB */
}

/* ==================================================================== */
/* ScriptEngine::Cleanup — Constructor failure / memory cleanup          */
/* Address: 0x4493C0                                                   */
/* __thiscall (this, flags)                                             */
/*                                                                      */
/* Sets vtable to safety sentinel, deletes critical section.           */
/* Returns this. Virtual slot [0] of the table at 0x4782A4.            */
/* ==================================================================== */
void* __thiscall ScriptEngine::Cleanup(uint8_t flags)
{
    DeleteCriticalSection(&this->cs);
    if (flags & 1) {
        GLOBAL_free(this);
    }
    return this;
}

/* ==================================================================== */
/* ScriptEngine::Dtor — Destructor                                       */
/* Address: 0x4493F0                                                   */
/* __fastcall (ECX=this)                                                */
/*                                                                      */
/* Sets vtable to safety sentinel, deletes critical section.           */
/* ==================================================================== */
void __fastcall ScriptEngine::Dtor()
{
/* In the binary: sets vtable here. Compiler-managed in natural C++. */
    DeleteCriticalSection(&this->cs);
}

/* ==================================================================== */
/* ScriptEngine::Lock — EnterCriticalSection wrapper                     */
/* Address: 0x449410                                                   */
/* __fastcall (ECX=this)                                                */
/*                                                                      */
/* RESDATA base vtable[1] method. Returns 1.                           */
/* ==================================================================== */
uint8_t __fastcall ScriptEngine::Lock()
{
    EnterCriticalSection(&this->cs);
    return 1;
}

/* ==================================================================== */
/* ScriptEngine::Unlock — LeaveCriticalSection wrapper                   */
/* Address: 0x449420                                                   */
/* __fastcall (ECX=this)                                                */
/*                                                                      */
/* RESDATA base vtable[2] method. Returns 1.                           */
/* ==================================================================== */
uint8_t __fastcall ScriptEngine::Unlock()
{
    LeaveCriticalSection(&this->cs);
    return 1;
}

/* ==================================================================== */
/* ScriptEngine::Call — Shutdown/dispatch method                         */
/* Address: 0x44E930                                                   */
/* __fastcall (ECX=this)                                                */
/*                                                                      */
/* Resets vtable to base RESDATA table and calls Panel_DtorBody() to    */
/* dispatch to base destructor logic. Used by Reset to clear state.     */
/* NOTE: Despite the name "Call", this is actually a shutdown/body-     */
/* destructor call, not a script invocation. The vtable entry at        */
/* vtable[2] (offset +0x08) is the "OnInitFromStream" body-destructor. */
/* ==================================================================== */
void __fastcall ScriptEngine::Call()
{
    /* Reset vtable to base RESDATA table */
/* In the binary: sets vtable here. Compiler-managed in natural C++. */
    /* Delegate to Panel base body destructor */
    Panel_DtorBody(this);
}

/* ==================================================================== */
/* ScriptEngine::Reset — Reset/Clear state with optional free           */
/* Address: 0x44E910                                                   */
/* __thiscall (this, param_1=bit0=free_memory)                          */
/*                                                                      */
/* Calls ScriptEngine::Call (vtable reset + body dtor), then optionally */
/* frees memory via GLOBAL_free if param_1 & 1.                         */
/* ==================================================================== */
void* __thiscall ScriptEngine::Reset(uint8_t free_memory)
{
    this->Call();             /* vtable reset + body destructor */
    if (free_memory & 1) {
        GLOBAL_free(this);
    }
    return this;
}

/* ==================================================================== */
/* ScriptEngine_Run — Update a track piece's zoom based on a hit-test   */
/* Address: 0x44EF10                                                   */
/* __cdecl (tool, x, y)                                                 */
/*                                                                      */
/* Real operand type is TrackPiece*, not ScriptEngine* (see the         */
/* declaration's comment above and game/TrackPiece.h for field          */
/* evidence). Checks tool->flags bit 1 (flags&2). If set, dispatches    */
/* PtInRect(x, y) (vtable slot 2, +0x08). On success (non-zero return), */
/* sets zoom=2 if zoom_level==1. On failure, sets zoom=1 if             */
/* zoom_level==2.                                                       */
/*                                                                      */
/* Bug found and fixed: the flags check previously read byte offset     */
/* +0x0C. Ghidra's decompilation is `*(byte*)(param_1 + 0xb)` with      */
/* `param_1` typed `int*`, i.e. byte offset 0xb*4 = 0x2C — matching     */
/* TrackPiece::flags (uint16_t, +0x2C, game/TrackPiece.h). +0x0C falls  */
/* inside TrackPiece's inherited GameObject::screen_rect (the "top"     */
/* field), which has no flags semantics. Corrected to +0x2C.            */
/* ==================================================================== */
int CDECL ScriptEngine_Run(TrackPiece* tool, int x, int y)
{
    int result;

    if (tool == nullptr) {
        return 0;
    }

    if ((tool->flags & 2) == 0) {                        /* +0x2C */
        return 0;
    }

    /* Ghidra 0x44EF10 calls the tool's slot 2 (+0x08, PtInRect) — a real,
       typed TrackPiece/GameObject virtual method, not a raw vtable view. */
    result = tool->PtInRect(x, y);

    if (result != 0) {
        if (tool->zoom_level == 1) {                      /* +0x48 */
            tool->SetZoom(2);
        }
        return 1;
    } else {
        if (tool->zoom_level == 2) {                      /* +0x48 */
            tool->SetZoom(1);
        }
        return 0;
    }
}

/* ==================================================================== */
/* RESDATA_Lock — EnterCriticalSection wrapper (free function)          */
/* Address: 0x449410                                                   */
/* __fastcall (ECX = ptr, CRITICAL_SECTION at +0x04)                    */
/*                                                                      */
/* RESDATA base vtable[1] method. Returns 1.                           */
/* ==================================================================== */
uint8_t __fastcall RESDATA_Lock(void* ptr)
{
    EnterCriticalSection(reinterpret_cast<uint8_t*>(ptr) + 0x04);
    return 1;
}

/* ==================================================================== */
/* RESDATA_Unlock — LeaveCriticalSection wrapper (free function)        */
/* Address: 0x449420                                                   */
/* __fastcall (ECX = ptr, CRITICAL_SECTION at +0x04)                    */
/*                                                                      */
/* RESDATA base vtable[2] method. Returns 1.                           */
/* ==================================================================== */
uint8_t __fastcall RESDATA_Unlock(void* ptr)
{
    LeaveCriticalSection(reinterpret_cast<uint8_t*>(ptr) + 0x04);
    return 1;
}

/* ==================================================================== */
/* RESDATA_ScriptedObject — RETIRED (2026-08-16)                        */
/*                                                                      */
/* This file's Ctor/Dtor/InitSubObjects/Shutdown/Start/Update/Dispatch/  */
/* IsDragging/CheckClick/GetDragOffset/MoveTo/HitTest/HandleToolClick    */
/* method bodies have been removed. They were an independent, parallel  */
/* reconstruction of the exact same original class as                   */
/* `game/ScriptedObject.h`'s `ScriptedObject` (see that header's class   */
/* doc comment for the full trail). Their substantive logic was ported   */
/* to game/ScriptedObject.cpp, re-deriving each field/vtable-slot        */
/* reference against GameObject.h/Entity.h/Panel.h directly rather than  */
/* copying this file's own `param_1[N]` byte-offset math verbatim — a    */
/* systematic hazard here: `param_1` was declared `int*`, so `param_1[N]`*/
/* is byte offset N*4, not N, and several accesses were misread as a     */
/* result (e.g. `param_1[9]`/+0x24 is `Entity::visible`, not "drag_flag";*/
/* `param_1[0x24]`/+0x90 is `Panel::drag_active`, ALSO called            */
/* "drag_flag" — two different original fields collapsed into one name;  */
/* `param_1[0xa]`/+0x28 is `Entity::anim_index`, not "tooltip_state";     */
/* `param_1[0x15]`/+0x54 is `Entity::frame_index`, not "anim_index").     */
/* See game/ScriptedObject.cpp's own header comment and PROGRESS.md's    */
/* Completed section for the corrected field map and remaining known     */
/* gaps (a floating-point scaling term in Update's mode==1 bounds-clamp  */
/* that this reconciliation pass did not fully re-derive). */
/* ==================================================================== */

/* ================================================================== */
/* ScriptEngine_HostSize/HostConstruct — narrow factory pair for        */
/* GameLoop_Setup's standalone ScriptEngine allocation (core/GameLoop.  */
/* cpp). Declared narrowly there rather than #include-ing this file's   */
/* own scriptengine.h, which this file's own TU doesn't otherwise need   */
/* (GameLoop.cpp gets g_scripted_object from game/ScriptedObject.h now). */
/* GameLoop_Setup previously allocated a literal 0x1C bytes (the        */
/* original x86 sizeof(ScriptEngine)) and placement-constructed via the */
/* narrower ScriptEngine_constructor ABI bridge (shared/stubs_impl.cpp) */
/* -- an undersized-operator_new landmine on this 64-bit host, where    */
/* sizeof(ScriptEngine) is 64 bytes, not 0x1C. That bridge is still      */
/* correct for game/BuildingComplex.cpp's embedded, genuinely-0x1C-byte */
/* BuildingCollectionLock use; this pair is only for a real, full-sized */
/* standalone ScriptEngine object. */
/* ================================================================== */
size_t ScriptEngine_HostSize();
size_t ScriptEngine_HostSize()
{
    return sizeof(ScriptEngine);
}

void* ScriptEngine_HostConstruct(void* mem);
void* ScriptEngine_HostConstruct(void* mem)
{
    return new (mem) ScriptEngine();
}