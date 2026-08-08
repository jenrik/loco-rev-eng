/**
 * NETMAN_NetworkUI — Multiplayer lobby panel / session window functions
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These functions are free-function overrides/helpers on NameEntryPanel
 * (ui/NameEntryPanel.h), a UI_WindowBase subclass (ui/UI_WindowBase.h) used
 * for the multiplayer session enumeration and join dialogs. Confirmed via
 * the vtable data at 0x4781D0 (see ui/NameEntryPanel.h's vtable table) —
 * NOT Netman class methods, and not (yet) real C++ virtual overrides here:
 * NameEntryPanel's actual compiler-managed vtable still dispatches to
 * UI_WindowBase's inherited defaults for hide/show/on_create/on_update/
 * on_lbutton_down, since wiring these free functions in as true overrides
 * would require converting NameEntryPanel to declare/override them, a
 * larger change tracked as a follow-up (PROGRESS.md) rather than done in
 * this cast/offset cleanup pass. Of the 7 functions below, only
 * NETMAN_CreateSession is currently reachable from outside this file
 * (ui/EditWindow.cpp's EditWindow::show()); the other 6 are unreferenced
 * by any other translation unit today.
 *
 * Contains:
 *   NETMAN_EnumerateSessions (0x441720) — Create session enumeration HWND
 *   NETMAN_JoinSession       (0x441870) — Show join session UI (NameEntryPanel::show() override)
 *   NETMAN_CreateSession     (0x4419C0) — Set session mode flags
 *   NETMAN_LeaveSession      (0x441A00) — Cleanup and hide session UI (NameEntryPanel::hide() override)
 *   NETMAN_UpdateSessionInfo (0x441A90) — Blit panel + set sprite states (on_update() override)
 *   NETMAN_GetSessionInfo    (0x441B40) — Update sprite visibility
 *   NETMAN_SetSessionInfo    (0x441C80) — Handle UI click/hit-test (on_lbutton_down() override)
 *
 * NETMAN_DestroySession (0x441F80) is a DIFFERENT function implemented in
 * native/NETMAN_SessionSettings.c, not here (this file's header previously,
 * incorrectly, listed it as one of its own).
 *
 * Field/offset documentation lives on the canonical classes now
 * (ui/NameEntryPanel.h extends ui/UI_WindowBase.h) — see those headers for
 * the full layout instead of a duplicated offset table here. Two scaled-
 * offset transcription bugs were found and fixed while integrating this
 * file against that header: NETMAN_JoinSession's `param_1` is Ghidra-typed
 * `int*`, so its `param_1[N]` accesses are N*4-byte offsets, not raw byte
 * offsets like every other function in this file — `+0x52`/`+0x18`/`+0x19`/
 * `+0x50`/`+0x3C` were previously mistranscribed as byte offsets instead of
 * the real `+0x148`/`+0x60`/`+0x64`/`+0x140`/`+0xF0`.
 *
 * This file's own top-of-file offset table (now removed) previously labeled
 * `+0x08` as "HWND (parent)". Per ui/UI_WindowBase.h, `+0x08` is actually
 * `hWnd` — this window's *own* HWND — not `hWndParent` (`+0x0C`, unused by
 * any function here). "Parent" described its role (parent of the child
 * EDIT control created in NETMAN_EnumerateSessions), not its identity;
 * every access below now reads `panel->hWnd` accordingly.
 */
#include "../shared/types.h"
#include "../ui/NameEntryPanel.h"
#include "../ui/ButtonSprite.h"
#include "../game/GameConfig.h"
#include "../network/DPlayManager.h"

#include <cassert>
#include <cstdio>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void*    g_ui_main;              /* 0x4A8860 */
extern void*    _g_primary_surface;     /* 0x4FD3C4 */
extern void*    g_resmgr;               /* resource manager */
extern char     g_empty_string;         /* 0x4851D0 */
extern void*    g_font_small;           /* 0x4855F8 — shared small UI font; canonical
                                          * name/type from network/NetworkPlayerList.cpp
                                          * (real definition: shared/stubs_impl.cpp). This
                                          * file previously dereferenced this address as a
                                          * raw `*(uint32_t*)0x4855F8` read instead of using
                                          * the named global directly. */

/* _g_netman_data — the same 0x4FD3A8 singleton game/GameConfig.h documents
 * as GameConfig / g_dplayConfig, and network/Netman.cpp treats as a typed
 * `GameConfig*`. Declared `void*` here (not `GameConfig*`) because its one
 * real definition is `void* _g_netman_data = nullptr;`
 * (shared/defsym_stubs.cpp:159) — matching that exactly, then casting
 * locally, avoids adding a second, differently-typed extern declaration of
 * the same global (a data-linkage variant of the call-0 landmine class:
 * unlike functions, mismatched extern *variable* types don't even get a
 * different mangled name, so the mismatch would link silently).
 *
 * HOST NOTE: nothing in this codebase's host build ever assigns
 * `_g_netman_data` (GameLoop_Setup's GameConfig construction, 0x406C5A, is
 * not ported), so it is permanently nullptr on the host today — a
 * pre-existing gap, not introduced here (network/Netman.cpp's own
 * `_g_netman_data->m_hostMode` accesses share it). On real Windows this
 * pointer is always valid by the time any of these panel functions run
 * (GameLoop_Setup executes at startup, long before the multiplayer lobby
 * panel can open), so every `#ifndef _WIN32` null-guard below is a
 * host-initialization-gap deviation, not original behavior. */
extern void*    _g_netman_data;         /* 0x4FD3A8 */

extern "C" {

extern int32_t __stdcall wsprintfA(char* lpOut, const char* lpFmt, ...);
extern int32_t __stdcall IsWindowVisible(void* hWnd);
extern int32_t __stdcall SetWindowTextA(void* hWnd, const char* lpString);
extern int32_t __stdcall GetWindowTextA(void* hWnd, char* lpString, int32_t nMaxCount);
extern void    __stdcall PostMessageA(void* hWnd, uint32_t Msg, void* wParam, uint32_t lParam);
extern void*   __stdcall SetTimer(void* hWnd, uint32_t nIDEvent, uint32_t uElapse, void* lpTimerFunc);
extern int32_t __stdcall KillTimer(void* hWnd, uint32_t uIDEvent);
extern void    __stdcall SetFocus(void* hWnd);
extern void    __stdcall ShowWindow(void* hWnd, int32_t nCmdShow);
extern void*   __stdcall SetWindowLongA(void* hWnd, int32_t nIndex, void* dwNewLong);
extern void*   __stdcall CreateWindowExA(uint32_t dwExStyle, const char* lpClassName,
                                          const char* lpWindowName, uint32_t dwStyle,
                                          int32_t x, int32_t y, int32_t nWidth, int32_t nHeight,
                                          void* hWndParent, void* hMenu,
                                          void* hInstance, void* lpParam);
extern void    __stdcall Sleep(uint32_t dwMilliseconds);
extern int32_t __stdcall PtInRect(const RECT* lprc, POINT pt);
extern LRESULT __stdcall DefWindowProcA(void* hWnd, uint32_t Msg, uint32_t wParam, uint32_t lParam);

extern int32_t __cdecl  ResourceManager_GetStringById(void* resmgr, uint32_t id);
extern void  __cdecl    RESMGR_LoadSoundResource(int32_t resId);
extern void  __cdecl    Sprite_Init(void* sprite);
extern void  __cdecl    Sprite_Destroy(void* sprite);
extern void  __cdecl    Sprite_SetState(void* sprite, int32_t state, int32_t* unk);
extern void  __cdecl    FormatResourceString(void* resmgr, uint32_t id, char* buf, int32_t bufsize);
extern void  __cdecl    PlaySound(int32_t soundId);
extern void  __cdecl    PlaySoundAt(int32_t soundId, int32_t x, int32_t y, int32_t flags);
extern int32_t __cdecl  CRT_rand(void);
extern void  __cdecl    UI_MainMenu_SetState(void* ui_main, int32_t state);
}

/* Real def: ui/UIPANEL.cpp (0x426B70/0x426B90), C++ linkage (not
 * extern "C"), void(void*) / void(void*, int, int, uint8_t, RECT*) — the
 * 2nd EndPaintEx param is `int hdc`, not `void* hwnd`/`HWND`. Both were
 * declared inside the extern "C" block above (UIPANEL_EndPaint's params
 * matched but its linkage didn't; EndPaintEx's linkage *and* 2nd param
 * type were both wrong), so both silently bound to
 * shared/stubs_impl.cpp's host no-op instead of the real present pipeline
 * — the identical landmine already fixed for UIPANEL_Blit in this same
 * file. Confirmed safe to call with this file's `NameEntryPanel*` as
 * `self`: EndPaintEx's Path A (the only path reachable here, since
 * UI_WindowBase::field_14 — read as "tile_map" — is always null on this
 * class) uses `self+0xD4` (UI_WindowBase::workRect) and `self+0x08`
 * (UI_WindowBase::hWnd), both of which NameEntryPanel has via inheritance;
 * NULL `restrict_rect` (passed at every call site below, matching what
 * Ghidra shows the original assembly passes too) is explicitly handled by
 * falling back to that same viewport rect, not dereferenced. */
extern void  __fastcall UIPANEL_EndPaint(void* self);
extern void  __thiscall UIPANEL_EndPaintEx(void* self, int32_t hdc, int32_t unlockParam,
                                            uint8_t unlockFlag, RECT* restrictRect);

/* Real def: native/NETMAN_SessionSettings.c, C++ linkage (not extern "C"),
 * void(uint8_t*). Was declared inside the extern "C" block above with a
 * `void*` param — same UIPANEL_EndPaintEx-class landmine (call-0; already
 * tracked for ui/EditWindow.cpp's separate, still-unfixed copy in
 * docs/landmine-sweep-worklist.md). Fixed here only for this file's two
 * call sites, both inside a null-`_g_netman_data`-guarded block (see the
 * `_g_netman_data` comment above) — neither is reachable from outside this
 * file today, so this carries none of ui/EditWindow.cpp's live-call-site
 * risk (real file I/O to NetSettings.dat on an unguarded, non-null
 * `_g_netman_state`); left that one alone. */
extern void  __fastcall NETMAN_SendPacket(uint8_t* packetPtr);

/* Real def: resources/resource_manager_sdl3.cpp returns `void*`, matching
 * every other in-tree caller of ResourceManager_GetById (ui/AboutDialog.cpp,
 * ui/ButtonSprite.cpp, game/BuildingMgr.cpp, etc.). This file previously
 * declared an `int32_t` return type, silently truncating the returned
 * pointer to 32 bits on store — a 32-to-64-bit-recompilation landmine
 * distinct from (but the same class as) the call-0 landmines below: return
 * types don't affect C++ mangling, so this one is a silent value bug, not
 * a link-time mismatch. */
extern void* __cdecl ResourceManager_GetById(void* resmgr, uint32_t id);

/* Real def: ui/UIPANEL_Surface.cpp, C++ linkage (not extern "C"),
 * bool(void*,uint32_t,uint32_t,int32_t,uint32_t,void*,uint32_t,uint32_t,
 * int32_t,uint32_t,uint32_t). This file is compiled as C++ (see meson.build
 * common_c_args: native/*.c uses `-x c++`), so extern "C" linkage here is a
 * real mismatch, not a documentation-only quirk — was declared inside the
 * extern "C" block above with a uniform int32_t shape and void return,
 * neither of which matches the real symbol (call-0 landmine). */
extern bool  __cdecl    UIPANEL_Blit(void* surface, uint32_t srcX, uint32_t srcY,
                                      int32_t srcW, uint32_t srcH, void* dstSurface,
                                      uint32_t dstX, uint32_t dstY, int32_t dstW,
                                      uint32_t dstH, uint32_t flags);

/* Forward declarations for this file's own functions, called out of
 * definition order below. Declared with normal (mangled) C++ linkage,
 * matching their definitions further down — NOT inside the extern "C"
 * block above, where NETMAN_GetSessionInfo/NETMAN_UpdateSessionInfo were
 * previously (mis)declared. In C++, a name's linkage is fixed by its
 * *first* declaration; forward-declaring them extern "C" and then defining
 * them below without repeating extern "C" doesn't create a second entity —
 * it keeps the same C-linkage identity established by the first
 * declaration, so the definitions below were silently emitting unmangled
 * symbols instead of the mangled C++ names network/Netman.h's (C++-linkage)
 * declarations of the same two names look for — the identical landmine
 * class as UIPANEL_EndPaint/UIPANEL_EndPaintEx above, self-inflicted on
 * this file's own functions this time.
 *
 * All 7 are declared here (not just the 2 called out of order) to satisfy
 * -Werror=missing-declarations without pulling in network/Netman.h — that
 * header's own extern "C" block gives C++ linkage to ~13 unrelated
 * functions whose real implementations are C-linkage (see
 * network/NetmanTypes.h's header comment); this file doesn't need any of
 * them, so including Netman.h would only add risk. Kept in sync with
 * Netman.h's matching declarations by hand. */
void NETMAN_EnumerateSessions(NameEntryPanel* panel);
void NETMAN_JoinSession(NameEntryPanel* panel);
void NETMAN_CreateSession(NameEntryPanel* panel);
void NETMAN_LeaveSession(NameEntryPanel* panel);
void NETMAN_UpdateSessionInfo(NameEntryPanel* panel);
void NETMAN_GetSessionInfo(NameEntryPanel* panel);
LRESULT NETMAN_SetSessionInfo(NameEntryPanel* panel, void* hWnd, uint32_t msg,
                               uint32_t wParam, uint32_t lParam);

/* Network provider linked-list node, walked from GameConfig::m_providerList
 * (game/GameConfig.h). Evidence: NETMAN_JoinSession (0x441870) and
 * NETMAN_CreateSession (0x4419C0) both walk this exact 2-field shape. No
 * dedicated canonical header models network providers yet — kept local to
 * this file rather than guessing where else it might belong. */
struct NetworkProviderNode {
    NetworkProviderNode* next;  /* +0x00 */
    int32_t               type;  /* +0x04  2 => a 4-player provider, 4 => a 2-player provider */
};

/* Shared provider-list walk (identical loop body in NETMAN_CreateSession
 * and NETMAN_JoinSession's original assembly). */
static void NETMAN_ApplyProviderModes(NameEntryPanel* panel, GameConfig* config)
{
#ifndef _WIN32
    /* Host-only: see the `_g_netman_data` comment above — GameConfig is
     * never constructed on the host today, so config is always nullptr
     * here. On Windows this branch cannot be taken.
     *
     * Empirically verified (2026-08-08): temporarily disabling this guard
     * and running the `opens_main_menu` GUI integration test reproduces an
     * immediate crash right after the "EditWindow::show: create session"
     * trace line — i.e. exactly this call, exactly as predicted. */
    if (config == nullptr) {
        return;
    }
#endif
    for (NetworkProviderNode* provider = static_cast<NetworkProviderNode*>(config->m_providerList);
         provider != nullptr; provider = provider->next) {
        if (provider->type == 2) {
            panel->supportsFourPlayerMode = 1;
        } else if (provider->type == 4) {
            panel->supportsTwoPlayerMode = 1;
        }
    }
}

/* EDIT-control subclass WndProc — TODO: decompile 0x4417E0.
 * NETMAN_EnumerateSessions below subclasses the session-name edit control
 * to this procedure via SetWindowLongA(hwnd, GWL_WNDPROC, ...); it isn't
 * decompiled yet, and NETMAN_EnumerateSessions itself is currently
 * unreferenced by any other translation unit, so this is not reachable
 * from any passing test today. Per AGENTS.md's stub policy, this is a
 * loud, asserting stub (matching ui/UI_ChildWindow.cpp's precedent) rather
 * than a silent no-op or a raw, unresolvable function-pointer literal
 * (which is what this file previously embedded — a bare address with no
 * matching implementation anywhere in the tree). */
static LRESULT __stdcall NETMAN_EditControlSubclassProc(void* hWnd, uint32_t msg,
                                                          uint32_t wParam, uint32_t lParam)
{
    std::fprintf(stderr,
        "STUB: NETMAN_EditControlSubclassProc (0x4417E0) reached on host build — "
        "the session-name edit control's subclass WndProc is not yet decompiled "
        "(see PROGRESS.md).\n");
    assert(false && "NETMAN_EditControlSubclassProc (0x4417E0): not yet decompiled");
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

/* ================================================================== */
/* NETMAN_EnumerateSessions — 0x441720                                 */
/* Create session enumeration window (edit control for session name). */
/* ================================================================== */
void NETMAN_EnumerateSessions(NameEntryPanel* panel)
{
    if (panel->sessionNameEditHwnd != nullptr) return;  /* Already created */

    void* hWnd = CreateWindowExA(
        0x200,                          /* WS_EX_CLIENTEDGE */
        "EDIT",                         /* 0x47E464 — "EDIT" window class name */
        &g_empty_string,
        0x40000080,                     /* WS_CHILD | WS_VISIBLE */
        panel->editControlRect.left,
        panel->editControlRect.top,
        panel->editControlRect.right - panel->editControlRect.left,   /* width */
        panel->editControlRect.bottom - panel->editControlRect.top,   /* height */
        panel->hWnd,                     /* parent HWND — this panel's own window,
                                          * of which the edit control is a child */
        reinterpret_cast<void*>(static_cast<uintptr_t>(0x41F)),  /* HMENU = control ID */
        panel->hInstance,
        nullptr
    );

    panel->sessionNameEditHwnd = hWnd;

    if (hWnd != nullptr) {
        PostMessageA(hWnd, 0x30 /* WM_SETFONT */, g_font_small, 1);
        PostMessageA(hWnd, 0xC5 /* EM_LIMITTEXT */,
                     reinterpret_cast<void*>(static_cast<uintptr_t>(0x40)), 0);

        GameConfig* const config = reinterpret_cast<GameConfig*>(_g_netman_data);
#ifndef _WIN32
        if (config != nullptr)
#endif
        {
            SetWindowTextA(hWnd, config->m_sessionName);
        }

        /* Subclass the edit control */
        void* oldWndProc = SetWindowLongA(hWnd, -4 /* GWL_WNDPROC */,
                                           reinterpret_cast<void*>(&NETMAN_EditControlSubclassProc));
        panel->originalEditWndProc = oldWndProc;
    }
}

/* ================================================================== */
/* NETMAN_JoinSession — 0x441870                                       */
/* Initialize and show the join-session UI panel (NameEntryPanel::show()   */
/* override — vtable slot [2], +0x08).                                     */
/* ================================================================== */
void NETMAN_JoinSession(NameEntryPanel* panel)
{
    /* Mark paint-ready flag as false initially. Previously mistranscribed
     * as `p + 0x52` (a byte offset) — Ghidra types this function's `this`
     * as `int*`, so its `param_1[0x52]` index is a ×4-scaled offset,
     * `0x148`, matching NameEntryPanel::paintReadyFlag. */
    panel->paintReadyFlag = 0;

    if (!panel->hasSprites) {
        /* Allocate and initialize 7 sprites, plus a resource-backed child
         * surface (unrelated to the 7 ButtonSprites). NameEntryPanel::
         * spriteTerminator/childSurface are repurposed here per their
         * header documentation. */
        void* res = ResourceManager_GetById(&g_resmgr, 0x439);
        panel->spriteTerminator = res;

        /* res->vtable[1] ("Lock/GetSurface" per shared/types.h's RESDATA
         * convention), called with (0, 0). Matches ui/AboutDialog.cpp's
         * identical, already-reviewed pattern for the same
         * ResourceManager_GetById-sourced, still-unmodeled resource
         * object class (real class not yet in this codebase's hierarchy —
         * every caller of ResourceManager_GetById still uses `void*`).
         * Previously read as `(uint8_t*)res + 4` — missing the dereference
         * of `res` itself to reach its vtable pointer (call-0-shaped bug:
         * it called through an offset *into the object*, not into its
         * vtable) — and used a raw x86 byte offset instead of a
         * pointer-sized slot index (misindexes this host's 8-byte vtable
         * entries). */
        if (res != nullptr) {
            using ResourceGetSurfaceFn = void* (__stdcall*)(void*, int, int);
            void** const resourceVtbl = *reinterpret_cast<void***>(res);
            panel->childSurface = reinterpret_cast<ResourceGetSurfaceFn>(resourceVtbl[1])(res, 0, 0);
        }

        Sprite_Init(panel->sprite0);
        Sprite_Init(panel->sprite1);
        Sprite_Init(panel->sprite2);
        Sprite_Init(panel->sprite3);
        Sprite_Init(panel->sprite4);
        Sprite_Init(panel->sprite5);
        Sprite_Init(panel->sprite6);

        panel->hasSprites = 1;
    }

    /* vtable slot [7] (+0x1C): on_create(). Previously dispatched by literal
     * x86 byte offset through a raw `void**` cast — misindexes this host's
     * 8-byte vtable entries for any slot not a multiple of 8, and dropped
     * type safety entirely; now a normal virtual call through the real
     * UI_WindowBase/NameEntryPanel C++ hierarchy. */
    panel->on_create();

    /* Set 2-player/4-player mode-availability flags from the provider list. */
    GameConfig* const config = reinterpret_cast<GameConfig*>(_g_netman_data);
    NETMAN_ApplyProviderModes(panel, config);

    panel->show();
    SetFocus(panel->hWnd);

    /* vtable slot [3] (+0x0C): set_mode(). Previously dispatched by literal
     * byte offset; args were themselves a scaled-offset bug (`param_1[0x18]`/
     * `param_1[0x19]`, i.e. `+0x60`/`+0x64` = UI_WindowBase::childCount0/
     * childObj0, not the `+0x18`/`+0x19` byte offsets this file previously
     * read — those land inside UI_WindowBase::field_18/field_1C instead). */
    panel->set_mode(panel->childCount0, panel->childObj0, 0, 1);

    /* Load and play sound resource */
    {
        int32_t soundId = ResourceManager_GetStringById(&g_resmgr, 0x5015);
        if (soundId != 0) {
            RESMGR_LoadSoundResource(soundId);
        }
    }

    /* Start animation timer (50ms interval) */
    panel->timerId = static_cast<UINT_PTR>(reinterpret_cast<uintptr_t>(
        SetTimer(panel->hWnd, 0x50, 0x32, nullptr)));
    panel->gameMode = 2;  /* mode state — previously mistranscribed as `+0x50`
                            * (byte offset); real offset is `0x50 * 4 = 0x140`,
                            * matching NameEntryPanel::gameMode. */

    FormatResourceString(&g_resmgr, 0x79, panel->textBuffer, sizeof(panel->textBuffer));
    /* Previously mistranscribed as `p + 0x3C` (byte offset); real offset is
     * `0x3C * 4 = 0xF0`, matching NameEntryPanel::textBuffer — independently
     * corroborated by this header's own pre-existing documentation of this
     * exact call. */
    RenderConnectionPanel(panel);
}

/* ================================================================== */
/* NETMAN_CreateSession — 0x4419C0                                     */
/* Set session mode flags from network provider list.                  */
/*                                                                       */
/* Reachability: called from ui/EditWindow.cpp's EditWindow::show(),    */
/* which every GUI-facing test exercises. See the `_g_netman_data`      */
/* comment above for why the host-side null guard in                   */
/* NETMAN_ApplyProviderModes is required for this call to be safe.      */
/* ================================================================== */
void NETMAN_CreateSession(NameEntryPanel* panel)
{
    GameConfig* const config = reinterpret_cast<GameConfig*>(_g_netman_data);
    NETMAN_ApplyProviderModes(panel, config);
}

/* ================================================================== */
/* NETMAN_LeaveSession — 0x441A00                                      */
/* Cleanup sprites, kill timer, hide panel (NameEntryPanel::hide()         */
/* override — vtable slot [1], +0x04).                                    */
/* ================================================================== */
void NETMAN_LeaveSession(NameEntryPanel* panel)
{
    KillTimer(panel->hWnd, panel->timerId);

    if (panel->hasSprites) {
        /* res->vtable[2] (unnamed; presumably a Release/Destroy analogous
         * to shared/types.h's RESDATA convention). Ghidra shows this called
         * with no explicit arguments (unlike the slot [1] call in
         * NETMAN_JoinSession above, which does pass the resource object as
         * an explicit first arg per ui/AboutDialog.cpp's convention) — kept
         * faithful to what's actually decompiled rather than guessing an
         * argument Ghidra didn't show. Same missing-dereference /
         * byte-vs-slot-index fixes as NETMAN_JoinSession's slot [1] call. */
        void* const res = panel->spriteTerminator;
        if (res != nullptr) {
            using ResourceReleaseFn = void (__stdcall*)();
            void** const resourceVtbl = *reinterpret_cast<void***>(res);
            reinterpret_cast<ResourceReleaseFn>(resourceVtbl[2])();
        }

        Sprite_Destroy(panel->sprite0);
        Sprite_Destroy(panel->sprite1);
        Sprite_Destroy(panel->sprite2);
        Sprite_Destroy(panel->sprite3);
        Sprite_Destroy(panel->sprite4);
        Sprite_Destroy(panel->sprite5);
        Sprite_Destroy(panel->sprite6);
        panel->hasSprites = 0;
    }

    panel->hide();
}

/* ================================================================== */
/* NETMAN_UpdateSessionInfo — 0x441A90                                 */
/* Blit child surface, update sprite states, get session info             */
/* (NameEntryPanel::on_update() override — vtable slot [8], +0x20).       */
/* ================================================================== */
void NETMAN_UpdateSessionInfo(NameEntryPanel* panel)
{
    UIPANEL_Blit(
        panel->childSurface,
        static_cast<uint32_t>(panel->workRect.left),    /* srcX */
        static_cast<uint32_t>(panel->workRect.top),      /* srcY */
        panel->workRect.right,                            /* srcW */
        static_cast<uint32_t>(panel->workRect.bottom),   /* srcH */
        _g_primary_surface,
        static_cast<uint32_t>(panel->scrollOffsetX2),     /* dstX */
        static_cast<uint32_t>(panel->scrollOffsetY2),     /* dstY */
        panel->blitDestWidth,                              /* dstW */
        static_cast<uint32_t>(panel->blitDestHeight),     /* dstH */
        1
    );

    Sprite_SetState(panel->sprite6, 0, nullptr);
    Sprite_SetState(panel->sprite0, 0, nullptr);
    Sprite_SetState(panel->sprite1, 0, nullptr);

    NETMAN_GetSessionInfo(panel);

    UIPANEL_EndPaintEx(panel, static_cast<int32_t>(reinterpret_cast<intptr_t>(panel->hWnd)), 0, 0, nullptr);
    panel->paintReadyFlag = 1;
}

/* ================================================================== */
/* NETMAN_GetSessionInfo — 0x441B40                                    */
/* Update sprite visibility based on session mode flags.               */
/* ================================================================== */
void NETMAN_GetSessionInfo(NameEntryPanel* panel)
{
    GameConfig* const config = reinterpret_cast<GameConfig*>(_g_netman_data);

    Sprite_SetState(panel->sprite6, 0, nullptr);

#ifndef _WIN32
    if (config == nullptr) {
        return;  /* Host-only: see the `_g_netman_data` comment above. */
    }
#endif

    if (config->m_hostMode == 0) {
        /* Host mode */
        if (panel->supportsTwoPlayerMode != 0) {
            if (config->m_hostPlayerCount == 4) {
                Sprite_SetState(panel->sprite2, 1, nullptr);
                ShowWindow(panel->sessionNameEditHwnd, 0);
            } else {
                Sprite_SetState(panel->sprite2, 0, nullptr);
            }
        }
        if (panel->supportsFourPlayerMode == 0) return;

        /* 4-player mode */
        if (config->m_hostPlayerCount == 2) {
            Sprite_SetState(panel->sprite4, 0, nullptr);
            Sprite_SetState(panel->sprite3, 1, nullptr);
            ShowWindow(panel->sessionNameEditHwnd, 5);  /* SW_SHOW */
            SetFocus(panel->sessionNameEditHwnd);
            return;
        }
        ShowWindow(panel->sessionNameEditHwnd, 0);
    } else {
        /* Client mode */
        ShowWindow(panel->sessionNameEditHwnd, 0);
        if (panel->supportsTwoPlayerMode != 0) {
            Sprite_SetState(panel->sprite2,
                             static_cast<int32_t>(config->m_clientPlayerCount == 4), nullptr);
        }
        if (panel->supportsFourPlayerMode == 0) return;
        if (config->m_clientPlayerCount == 2) {
            Sprite_SetState(panel->sprite3, 1, nullptr);
            return;
        }
    }
    Sprite_SetState(panel->sprite3, 0, nullptr);
}

/* ================================================================== */
/* NETMAN_SetSessionInfo — 0x441C80                                    */
/* Handle UI click/hit-test on session panel sprites                      */
/* (NameEntryPanel::on_lbutton_down() override — vtable slot [14], +0x38). */
/*                                                                       */
/* Real signature: slot arithmetic from the NameEntryPanel vtable base    */
/* (0x4781D0) places this at slot [14], matching                          */
/* UI_WindowBase::on_lbutton_down(HWND, UINT, WPARAM, LPARAM) exactly —    */
/* previously declared taking only `this`, with the click coordinates     */
/* hardcoded to (0, 0) and a comment noting they "come from the caller's  */
/* stack frame" via register/stack magic that was never actually wired    */
/* up. That made every click hit-test against the origin — i.e. this      */
/* panel's buttons could never really be clicked. lParam packs the        */
/* WM_LBUTTONDOWN x/y exactly as Ghidra's `in_stack_00000010 >> 0x10`      */
/* (unsigned shift) shows.                                                */
/* Returns: 0                                                             */
/* ================================================================== */
LRESULT NETMAN_SetSessionInfo(NameEntryPanel* panel, void* hWnd, uint32_t msg,
                               uint32_t wParam, uint32_t lParam)
{
    (void)hWnd;
    (void)msg;
    (void)wParam;

    if (panel->paintReadyFlag == 0) return 0;

    POINT pt;
    pt.x = static_cast<int32_t>(lParam & 0xFFFF);
    pt.y = static_cast<int32_t>(lParam >> 16);

    RECT sprite0Rect{ panel->sprite0->x, panel->sprite0->y,
                       panel->sprite0->sourceX, panel->sprite0->sourceY };
    if (PtInRect(&sprite0Rect, pt)) {
        /* Hit-test sprite0 (btn_back/cancel) */
        Sprite_SetState(panel->sprite0, 1, nullptr);
        PlaySound(0x5015);
        UIPANEL_EndPaint(panel);
        Sleep(0x96);
        panel->set_render_surface(nullptr, 0, nullptr, 0, 1);

        GameConfig* const config = reinterpret_cast<GameConfig*>(_g_netman_data);
#ifndef _WIN32
        if (config != nullptr)
#endif
        {
            GetWindowTextA(panel->sessionNameEditHwnd, config->m_sessionName,
                            sizeof(config->m_sessionName));
            if (config->m_hostMode == 0) {
                config->m_clientAutoFlag = 1;
            } else {
                config->m_hostFlagAuto = 1;
            }
            NETMAN_SendPacket(static_cast<uint8_t*>(_g_netman_data));
        }
        UI_MainMenu_SetState(g_ui_main, 3);
        return 0;
    }

    RECT sprite1Rect{ panel->sprite1->x, panel->sprite1->y,
                       panel->sprite1->sourceX, panel->sprite1->sourceY };
    if (PtInRect(&sprite1Rect, pt)) {
        /* Hit-test sprite1 (btn_join/ok) */
        Sprite_SetState(panel->sprite1, 1, nullptr);
        PlaySound(0x5015);
        UIPANEL_EndPaint(panel);
        Sleep(0x96);
        panel->set_render_surface(nullptr, 0, nullptr, 0, 1);
        UI_MainMenu_SetState(g_ui_main, 7);
        return 0;
    }

    RECT sprite2Rect{ panel->sprite2->x, panel->sprite2->y,
                       panel->sprite2->sourceX, panel->sprite2->sourceY };
    if (PtInRect(&sprite2Rect, pt) && panel->supportsTwoPlayerMode != 0) {
        /* Hit-test sprite2 (2-player button) */
        GameConfig* const config = reinterpret_cast<GameConfig*>(_g_netman_data);
#ifndef _WIN32
        if (config != nullptr)
#endif
        {
            if (config->m_hostMode == 0) {
                config->m_hostPlayerCount = 4;
            } else {
                config->m_clientPlayerCount = 4;
            }
        }
        NETMAN_GetSessionInfo(panel);
        PlaySound(0x5015);
        UIPANEL_EndPaintEx(panel, static_cast<int32_t>(reinterpret_cast<intptr_t>(panel->hWnd)), 0, 0, nullptr);
        return 0;
    }

    RECT sprite3Rect{ panel->sprite3->x, panel->sprite3->y,
                       panel->sprite3->sourceX, panel->sprite3->sourceY };
    if (PtInRect(&sprite3Rect, pt) && panel->supportsFourPlayerMode != 0) {
        /* Hit-test sprite3 (4-player button) */
        GameConfig* const config = reinterpret_cast<GameConfig*>(_g_netman_data);
#ifndef _WIN32
        if (config != nullptr)
#endif
        {
            if (config->m_hostMode == 0) {
                config->m_hostPlayerCount = 2;
            } else {
                config->m_clientPlayerCount = 2;
            }
        }
        NETMAN_GetSessionInfo(panel);
        PlaySound(0x5015);
        UIPANEL_EndPaintEx(panel, static_cast<int32_t>(reinterpret_cast<intptr_t>(panel->hWnd)), 0, 0, nullptr);
        return 0;
    }

    /* Hit-test panel background (panelClickRect) for random ambience sound */
    if (PtInRect(&panel->panelClickRect, pt)) {
        int32_t rnd = CRT_rand();
        PlaySoundAt(rnd / 0x1FFF + 0x500F, pt.x, pt.y, 4);
    }

    return 0;
}
