/**
 * stubs_link001_batch5_ui_graphics.cpp — LINK-001 call-0 landmine fixes,
 * batch 5 of 6 (UI/Graphics/Cursor/DDRAW/CGWND family).
 *
 * Status: TRANSCRIBED
 *
 * Every undefined symbol assigned to this batch is handled below. Each
 * entry documents its evidence (caller file/line, Ghidra address where
 * decompiled, and the real definition it either forwards to or the
 * reason it can't safely do so yet). Per this pass's constraints, no
 * existing file is edited — every "the real fix belongs in the caller"
 * case is a loud stub or a best-effort forwarding shim here instead, with
 * the intended real fix documented for a follow-up commit (see the
 * dispatching session's final report).
 */

#include <cstdint>
#include <cstdio>
#include <cassert>
#include <cstring>

#include "../shared/types.h"            /* RECT */
#include "../core/GameObject.h"         /* GameObject::InvalidateRect */
#include "../core/Entity.h"             /* Entity::resource (+0x40) */
#include "../game/Panel.h"              /* Panel (UIPANEL base) */
#include "../ui/UIPANEL.h"              /* UIPANEL::StopSound decl */
#include "../game/TrackPiece.h"         /* TrackPiece::SetZoom */
#include "../graphics/DDRAW_Building.h" /* DDRAW_Building::DispatchToSubObjects decl */

class GameWindow; /* Cursor_UpdateDirtyRect/RenderWithViewport only need the pointer type */

/* ===================================================================
 * SYMBOL: UI_CalcDialogCoords
 * CALLER: BuildingPanel::draw_occupant_dots(int*, int*)
 *         (game/BuildingPanel.cpp:113, declared inside an extern "C"
 *         block alongside genuine Win32 APIs -- so this must be
 *         extern "C" too, even though it isn't a Win32 function.)
 * ACTION: real-implementation. ADDRESS: 0x425AC0.
 *
 * Ghidra decompile (0x425AC0, __cdecl, 8 real instructions of fixed-point
 * arithmetic, void return -- the caller's own `int` return type is unused
 * garbage, doesn't affect the mangled symbol either way since this is
 * extern "C"): transforms point (*x,*y) from srcRect's coordinate space
 * into destRect's, using a 1000-scaled fixed-point ratio. Integer
 * truncation in the division is intentional (matches the original
 * assembly exactly), not a rounding bug to "fix".
 * =================================================================== */
extern "C" int UI_CalcDialogCoords(int* x, int* y, int* srcRect, int* destRect)
{
    *x -= srcRect[0];
    *y -= srcRect[1];
    *x = (*x * (((destRect[2] - destRect[0]) * 1000) / (srcRect[2] - srcRect[0]))) / 1000;
    *y = (*y * (((destRect[3] - destRect[1]) * 1000) / (srcRect[3] - srcRect[1]))) / 1000;
    *x += destRect[0];
    *y += destRect[1];
    return 0;
}

/* ===================================================================
 * SYMBOL: UI_ChildWindow_Render(void*, void*)
 * CALLER: ScriptedObject::HandleEvent(unsigned int, char const*)
 * ACTION: not-actually-undefined.
 *
 * game/ScriptedObject.cpp:49 declares `char UI_ChildWindow_Render(void*,
 * void*)`; the real definition (ui/UI_ChildWindow.cpp:796) is
 * `uint8_t UI_ChildWindow_Render(void* self, void* stream)`. Parameter
 * types match exactly (void*, void*) and Itanium C++ mangling does not
 * encode return type for ordinary functions, so both declarations
 * already produce the identical mangled symbol `_Z18UI_ChildWindow_RenderPvS_`.
 * Verified this is a plain global-scope free function (not inside the
 * anonymous namespace at UI_ChildWindow.cpp:107-170, which closes well
 * before line 796) with no #ifdef _WIN32 guard around its body. No
 * definition added here -- adding one would be a duplicate-definition
 * link error against the real one.
 * =================================================================== */

/* ===================================================================
 * SYMBOL: UI_CenterWindow(RECT*, RECT*) and UI_CenterWindow(void*, void*)
 * CALLERS: RenderConnectionPanel(NameEntryPanel*) [network/DPlayManager.cpp:66,221]
 *          GameSetupPanel::drawTitle() [ui/GameSetupPanel.cpp:98,679]
 * ACTION: caller-declaration-is-wrong (both).
 *
 * The real, working implementation is shared/stubs_impl.cpp:189
 * `void UI_CenterWindow(int* outer, int* inner)` (0x425A50) -- centers
 * `inner` within `outer`, both treated as {left,top,right,bottom} int[4].
 * RECT (shared/types.h) is exactly 4×int32_t, so both mismatched overloads
 * below just forward via reinterpret_cast; no behavior change, just a
 * correct mangled symbol for each caller's own (wrong) declared shape.
 * SHOULD_BE_FIXED_AT: network/DPlayManager.cpp:66 (retype to int*, or
 * better, include the real declaration instead of a local extern) and
 * ui/GameSetupPanel.cpp:98 (same).
 * =================================================================== */
extern void UI_CenterWindow(int* outer, int* inner); /* real impl: shared/stubs_impl.cpp:189 */

void UI_CenterWindow(RECT* outer, RECT* inner)
{
    UI_CenterWindow(reinterpret_cast<int*>(outer), reinterpret_cast<int*>(inner));
}

void UI_CenterWindow(void* outer, void* inner)
{
    UI_CenterWindow(reinterpret_cast<int*>(outer), reinterpret_cast<int*>(inner));
}

/* ===================================================================
 * SYMBOL: UI_DestroyTooltip(void*, void*)
 * CALLER: TrainStationWindow::hide() [ui/TrainStationWindow.cpp:30,316]
 * ACTION: caller-declaration-is-wrong.
 *
 * The real (also-just-a-no-op) implementation is shared/stubs_impl.cpp:601
 * `void UI_DestroyTooltip(void* self, int i)`. Every OTHER real caller
 * (world/scriptengine.cpp:689,805) already truncates its tooltip pointer
 * to int32 before calling, matching this signature -- mirror that
 * convention here instead of guessing a new one.
 * SHOULD_BE_FIXED_AT: ui/TrainStationWindow.cpp:30 (declare `int handle`,
 * not `void* tooltip`, and cast `this->tooltip_ptr` at the call site the
 * same way world/scriptengine.cpp does).
 * =================================================================== */
extern void UI_DestroyTooltip(void* self, int i); /* real impl: shared/stubs_impl.cpp:601 */

void UI_DestroyTooltip(void* mgr, void* tooltip)
{
    UI_DestroyTooltip(mgr, static_cast<int>(reinterpret_cast<intptr_t>(tooltip)));
}

/* ===================================================================
 * SYMBOL: UIPANEL_EndPaintEx(void*, void*, int, unsigned char, void*)
 * -- RESOLVED AND REMOVED (2026-08-13).
 *
 * This comment previously deferred the fix pending disassembly evidence
 * for two open questions; both are now answered from 0x426B90's actual
 * instructions:
 *
 * 1. Arity: `RET 0x10` confirms 4 stack args + ECX(this) = 5 params
 *    total, matching every caller's arg count exactly -- the fix is a
 *    pure signature/linkage correction, not an argument-count mismatch.
 * 2. Param-2 ("hdc") IS read, in the unlock_flag==0 branch: it is passed
 *    to a helper at 0x45B940 (the same "unlock primary surface" call
 *    UIPANEL_EndPaint's own reconstruction attributes to
 *    DDRAW_UnlockPrimary, just with an argument ui/UIPANEL.cpp's current
 *    transcription omits -- a separate, pre-existing gap in that file,
 *    not touched here). Concretely: when callers pass `self->hWnd` for
 *    this position with unlock_flag=0 (the common "just end paint, no
 *    HDC" shape), that hWnd value *is* forwarded to the real unlock
 *    helper -- exactly the same "hwnd fed into a hdc-shaped slot" pattern
 *    native/NETMAN_NetworkUI.c already established for the simpler
 *    UIPANEL_EndPaint. When callers instead pass unlock_flag=1 with a
 *    real HDC obtained from BeginPaint (`(int)hdc, 1, NULL` shape), that
 *    branch never reads param-2 at all -- it reads param-3 (unlockParam)
 *    and calls the primary surface's vtable slot 0x68 with it (a
 *    ReleaseDC-shaped call), and returns immediately. Both caller shapes
 *    were already passing correct values in the correct positions; only
 *    the C++ declared types (mangled to a different, wrong-stub-bound
 *    symbol) were wrong.
 *
 * All then-open callers (network/Netman.h/.cpp,
 * ui/GameSetupPanel.cpp/GameSetupPanel_network.cpp, ui/NameEntryPanel.cpp,
 * town/Town.cpp, game/BuildingPanel.cpp, native/NETMAN_SessionSettings.c,
 * network/DPlayManager.cpp/NetworkPlayerList.cpp,
 * input/Cursor_internal.h/Cursor_new_impls.cpp,
 * graphics/LOCOBITMAP.cpp, shared/stubs_link001_batch4_network_world.cpp)
 * are fixed to the real `(void* self, int hdc, int unlockParam,
 * uint8_t unlockFlag, RECT* restrictRect)` signature
 * (docs/landmine-sweep-worklist.md). This stub is confirmed dead via `nm`
 * (zero undefined references to its mangled name,
 * _Z18UIPANEL_EndPaintExPvS_ihS_, across every native and mingw-typecheck
 * .o) and removed.
 * =================================================================== */

/* ===================================================================
 * SYMBOL: UIPANEL::StopSound(int)
 * CALLER: UIPANEL::HandleDrag(int, unsigned short) [ui/UIPANEL.cpp:465]
 * ACTION: caller-declaration-is-wrong (about the *call site*, not just
 * linkage -- the name/behavior itself is wrong, not just the signature).
 *
 * Ghidra 0x4277D0 (real name: UIPANEL_ScrollPanel_HandleDrag, this is
 * UIPANEL::HandleDrag) case 0 dispatches vtable slot [1] with ZERO
 * arguments: `(**(code**)(*(int*)this+4))();`. ui/UIPANEL.h's own
 * class-level vtable table (line 28) says UIPANEL's slot [1] is
 * "(inherited from GameObject: StopSound)" -- but core/GameObject.h's
 * real vtable (0x477820) shows slot [1] is InvalidateRect (0x436AB0), not
 * StopSound; there is no StopSound at all on the GameObject/Panel/UIPANEL
 * hierarchy (Entity::StopSound, core/Entity.h, is on an unrelated sibling
 * branch -- Panel extends GameObject directly, not Entity). UIPANEL.h's
 * *per-method* doc comment near this declaration (claiming vtable[7])
 * contradicts its own class-level table and is simply wrong. The real
 * original call is InvalidateRect() with no arguments.
 * SHOULD_BE_FIXED_AT: ui/UIPANEL.cpp:465 (call `this->InvalidateRect();`
 * directly, drop the `(0)` argument) and ui/UIPANEL.h:255-265 (remove the
 * spurious `StopSound(int)` declaration/vtable[7] comment entirely -- it
 * doesn't correspond to any real override).
 * =================================================================== */
void UIPANEL::StopSound(int param)
{
    (void)param;
    this->InvalidateRect();
}

/* ===================================================================
 * SYMBOL: Cursor_Render(void*, unsigned long, int, char)
 * CALLER: AboutDialog::Update() [ui/AboutDialog.cpp:127,467]
 * ACTION: caller-declaration-is-wrong.
 *
 * Ghidra 0x414C20 confirms the real 2nd param ("hWnd") is a 32-bit
 * quantity (`undefined4`), matching ui/HelpWnd.cpp's own `int hWnd` and
 * shared/stubs_impl.cpp:267's real (loud-stub) `(void*, int, int, char)`.
 * ui/AboutDialog.cpp declared it `uintptr_t hWnd` -- 8 bytes on this host
 * -- a 64-bit-host size-mismatch bug (PROGRESS.md already flagged this
 * address as "inherited... wrong" from a different angle), not a genuine
 * second overload. Forward, truncating to the real 32-bit width.
 * SHOULD_BE_FIXED_AT: ui/AboutDialog.cpp:127 (declare `int hWnd`, not
 * `uintptr_t hWnd`).
 * =================================================================== */
extern void Cursor_Render(void* cursor, int hWnd, int hdc, char flag); /* real (loud stub): shared/stubs_impl.cpp:267 */

void Cursor_Render(void* cursor, unsigned long hWnd, int hdc, char flag)
{
    Cursor_Render(cursor, static_cast<int>(hWnd), hdc, flag);
}

/* ===================================================================
 * SYMBOL: Cursor_UpdateDirtyRect(GameWindow*, unsigned char)
 * CALLER: GameWindow::set_mode(int, void*, unsigned char, unsigned char)
 *         [ui/GameWindow.cpp:87,422]
 * ACTION: loud-deferred-stub. ADDRESS: unknown.
 *
 * ui/GameWindow.cpp's own comment claims 0x414770, but Ghidra's "loco"
 * database has no function starting at that address -- the claimed
 * address is stale/wrong (likely mid-function, not a real entry point).
 * TODO: disassemble GameWindow::set_mode's real call site to find the
 * true target before attempting a real implementation.
 * =================================================================== */
void Cursor_UpdateDirtyRect(GameWindow* gameWindow, uint8_t flag)
{
    (void)gameWindow; (void)flag;
    static bool warned = false;
    if (!warned) {
        std::fprintf(stderr,
            "STUB: Cursor_UpdateDirtyRect not implemented (claimed address "
            "0x414770 not found in the Ghidra database -- TODO: re-derive the "
            "real address from GameWindow::set_mode's disassembly) -- cursor "
            "dirty-rect update dropped\n");
        warned = true;
    }
}

/* ===================================================================
 * SYMBOL: Cursor_RenderWithViewport(GameWindow*, unsigned char)
 * CALLER: GameWindow::set_mode(int, void*, unsigned char, unsigned char)
 *         [ui/GameWindow.cpp:88,425]
 * ACTION: loud-deferred-stub. ADDRESS: unknown.
 *
 * Same situation as Cursor_UpdateDirtyRect above: claimed address
 * 0x414810 is also not found in Ghidra's "loco" database.
 * =================================================================== */
void Cursor_RenderWithViewport(GameWindow* gameWindow, uint8_t param)
{
    (void)gameWindow; (void)param;
    static bool warned = false;
    if (!warned) {
        std::fprintf(stderr,
            "STUB: Cursor_RenderWithViewport not implemented (claimed address "
            "0x414810 not found in the Ghidra database) -- cursor "
            "render-with-viewport dropped\n");
        warned = true;
    }
}

/* ===================================================================
 * SYMBOL: DDRAW_Building::DispatchToSubObjects(int, int, int, int, void*)
 * CALLER: DDRAW_DispatchToSubObjects(int, int, int, int, void*)
 *         [graphics/DDRAW.cpp:1280-1283, which itself just forwards to
 *         g_ddraw_building->DispatchToSubObjects(...)]
 * ACTION: loud-deferred-stub. ADDRESS: 0x45A1A0 (decompiled).
 *
 * graphics/DDRAW_Building.h:489 declares this member (with a full,
 * detailed doc comment already citing the address) but no .cpp anywhere
 * defines a body -- confirmed via grep across the whole tree. Ghidra
 * decompile obtained and matches the header's doc comment: dispatches
 * DrawConnected (vtable slot 11) to self (via Panel::Draw, the real
 * name for what this comment's own "RESDATA_DispatchEvent" meant),
 * sub_object_1 (+0xE0, a real typed GameObject member), popup_panel
 * (+0x3A0, `void*` -- calls BOTH vtable slot 11 AND slot 12 on it),
 * pattern_container (+0x428, `void*`) + 4 pattern sprites (+0x168,
 * treated as a raw uint8_t[] in the header, stride 0x88), and
 * track_sprite (+0x4B0, `void*`). sub_object_1's DrawConnected call would
 * be safe (real typed field, real virtual method), but popup_panel's
 * second call (vtable slot 12) has no independently-evidenced target --
 * shared/vtable_addrs.h shows *some* Entity-derived classes also have
 * "DrawConnected" at slot 12 (inherited from Entity), but popup_panel's
 * own concrete class is undetermined, and pattern_container/track_sprite
 * are likewise untyped void*. Implementing this now would mean guessing
 * at least 3 of 6 dispatch targets -- exactly the vtable-byte-offset/
 * wrong-target risk this cleanup sweep exists to avoid. Deferring with
 * the full decompile preserved here for whoever integrates the
 * popup_panel/pattern_container/track_sprite types next.
 * =================================================================== */
void DDRAW_Building::DispatchToSubObjects(int32_t left, int32_t top, int32_t right,
                                           int32_t bottom, void* param5)
{
    (void)left; (void)top; (void)right; (void)bottom; (void)param5;
    static bool warned = false;
    if (!warned) {
        std::fprintf(stderr,
            "STUB: DDRAW_Building::DispatchToSubObjects not implemented "
            "(0x45A1A0 decompiled, but popup_panel/pattern_container/"
            "track_sprite concrete types are unverified -- see "
            "shared/stubs_link001_batch5_ui_graphics.cpp) -- sub-object "
            "DrawConnected dispatch dropped\n");
        warned = true;
    }
}

/* ===================================================================
 * SYMBOL: DDRAW_UpdateBuildingSprites(void*)
 * CALLER: DDRAW_UpdateBuilding(void*) [town/sdl3_town_mode3.cpp:147,272]
 * ACTION: loud-deferred-stub. ADDRESS: 0x4597E0 (1375 bytes -- decompiled
 * far enough to confirm size/shape, too large to safely transcribe in
 * this pass).
 * =================================================================== */
void DDRAW_UpdateBuildingSprites(void* self)
{
    (void)self;
    static bool warned = false;
    if (!warned) {
        std::fprintf(stderr,
            "STUB: DDRAW_UpdateBuildingSprites not implemented "
            "(TODO: decompile 0x4597E0, 1375 bytes) -- building sprite "
            "refresh dropped\n");
        warned = true;
    }
}

/* ===================================================================
 * SYMBOL: DDRAW_UpdateVehicleSprites(int)
 * CALLER: DDRAW_UpdateBuilding(void*) [town/sdl3_town_mode3.cpp:148,335]
 * ACTION: loud-deferred-stub. ADDRESS: 0x45A480 (126 bytes, decompiled).
 *
 * Small enough to transcribe, but its loop dispatches a virtual call
 * (`(**(code**)(*puVar3 + 0x20))()`, vtable slot 8) on an untyped
 * `void*` array element whose real object type isn't established here;
 * GameObject's own slot 8 is SetFrame(int,bool) (2 args), but the
 * decompile shows this call site passing 0 explicit args -- an ABI
 * mismatch I can't resolve without knowing the real object's type.
 * Rather than guess an argument count for a virtual call (a genuine
 * crash risk if wrong), deferring.
 * =================================================================== */
void DDRAW_UpdateVehicleSprites(int self)
{
    (void)self;
    static bool warned = false;
    if (!warned) {
        std::fprintf(stderr,
            "STUB: DDRAW_UpdateVehicleSprites not implemented (0x45A480 "
            "decompiled, but its vtable-slot-8 dispatch target's real "
            "type/arg-count is unverified) -- vehicle sprite refresh "
            "dropped\n");
        warned = true;
    }
}

/* ===================================================================
 * SYMBOL: DDRAW_SetSurfaceFormat(void*, void*)
 * CALLER: GameWindow::create(...) [ui/GameWindow.cpp:98,662]
 * ACTION: loud-deferred-stub. ADDRESS: 0x45B9B0 (decompiled).
 *
 * Ghidra's decompile types both params as int/int* (a raw-address
 * artifact, not a real signature difference -- ui/GameWindow.cpp's own
 * `(void* surf, void* desc)` is the better-evidenced typing, matching how
 * it's actually called with `&ddsd`). The real body does a COM-style
 * vtable-slot-22 call on `surf` (a DirectDraw-surface-shaped object,
 * consistent with this codebase's established "literal vtable dispatch
 * for opaque DirectDraw COM objects" precedent, e.g. input/Cursor_Render.cpp)
 * and then writes several not-yet-declared-anywhere globals
 * (DAT_00485278/DAT_0048527c/DAT_00485288/DAT_0048528c/DAT_00485290/
 * _DAT_00485284). Implementing this "for real" would mean introducing
 * brand-new global storage I haven't verified doesn't already exist
 * under a different name elsewhere -- deferring rather than risk a
 * duplicate/conflicting global.
 * =================================================================== */
int DDRAW_SetSurfaceFormat(void* surf, void* desc)
{
    (void)surf; (void)desc;
    static bool warned = false;
    if (!warned) {
        std::fprintf(stderr,
            "STUB: DDRAW_SetSurfaceFormat not implemented (0x45B9B0 "
            "decompiled, but its pixel-format global writes are "
            "unverified against existing global declarations) -- surface "
            "format detection dropped\n");
        warned = true;
    }
    return 0;
}

/* ===================================================================
 * SYMBOL: DDRAW_UnlockPrimary(void*)
 * CALLERS: GameWindow::hide(); GameWindow::show(); GameWindow::set_mode(...)
 *          [ui/GameWindow.cpp:90,285,364,421]
 * ACTION: caller-declaration-is-wrong.
 *
 * ui/GameWindow.cpp is the ONLY caller anywhere in the tree that passes
 * an HWND argument; every other caller (ui/UIPANEL.cpp, ui/UI_WindowBase.cpp,
 * input/Cursor*.cpp, shared/stubs_impl.cpp/defsym_stubs.cpp) calls/declares
 * the 0-arg form, and Ghidra (0x45B940) confirms the real function takes
 * no parameters at all. Forward, discarding the (unused) argument.
 * SHOULD_BE_FIXED_AT: ui/GameWindow.cpp:90 (drop the HWND parameter to
 * match every other caller) and its 3 call sites (285, 364, 421).
 * =================================================================== */
extern void DDRAW_UnlockPrimary(void); /* real 0-arg impl, e.g. shared/stubs_impl.cpp:415 */

void DDRAW_UnlockPrimary(void* hWnd)
{
    (void)hWnd;
    DDRAW_UnlockPrimary();
}

/* ===================================================================
 * SYMBOL: CGWND_ValidatePaletteData(int)
 * CALLER: CursorEditWindow::Render(void*) [ui/CursorEditWindow.cpp:62,221]
 * ACTION: loud-deferred-stub -- deliberately NOT a forwarding shim.
 * ADDRESS: 0x40E950 (real impl exists, native/cgwnd_palette.c).
 *
 * ui/CursorEditWindow.cpp:221 calls `CGWND_ValidatePaletteData((int)
 * (uintptr_t)this)` -- truncating a 64-bit `this` pointer to 32 bits
 * BEFORE this function is ever reached. The real implementation
 * (native/cgwnd_palette.c:86, `byte __fastcall CGWND_ValidatePaletteData
 * (void* obj)`) genuinely needs the full pointer (dereferences
 * obj+0x168/+0x488). Reconstructing a 64-bit pointer from an
 * already-truncated 32-bit int here would be undefined behavior, not a
 * safe forward -- this is the documented 32-to-64-bit pointer-truncation
 * landmine class (see memory: landmine_bug_classes.md), not an ordinary
 * linkage mismatch. The real fix must happen at the call site.
 * SHOULD_BE_FIXED_AT: ui/CursorEditWindow.cpp:62,221 (declare
 * `(void* obj)` matching native/cgwnd_palette.c exactly and pass `this`
 * directly, with no int cast).
 *
 * (The dossier's separate "CGWND_ValidatePaletteData(void*) ->
 * WIN32_StreamOpenFile(...)" row is NOT a second undefined symbol --
 * grepped the whole tree; the only two appearances of this name are this
 * (int) caller and native/cgwnd_palette.c's real (void*) definition, which
 * already links fine on its own. No code added for the (void*) form.)
 * =================================================================== */
uint8_t CGWND_ValidatePaletteData(int classPtr)
{
    (void)classPtr;
    static bool warned = false;
    if (!warned) {
        std::fprintf(stderr,
            "STUB: CGWND_ValidatePaletteData(int) reached with a pointer "
            "already truncated to 32 bits at the call site "
            "(ui/CursorEditWindow.cpp:221) -- cannot safely recover the "
            "real object pointer on this 64-bit host; palette validation "
            "dropped, returning failure\n");
        warned = true;
    }
    return 0;
}

/* ===================================================================
 * SYMBOL: CGWND_GameSetup_DrawGrid_Thunk(void*)
 * CALLERS: Netman::ProcessMessage/HandlePlayerJoin/RemoveInboundTrain/
 *          HandlePlayerLeave [network/Netman.cpp, via network/Netman.h:262]
 * ACTION: loud-deferred-stub. ADDRESS: 0x409970 (a 5-byte thunk to
 * GameSetupPanel__drawGrid, 0x409980-0x409C55, 725 bytes, decompiled).
 *
 * shared/defsym_stubs.cpp has a symbol named `CGWND_GameSetup_DrawGrid_Thunk`
 * but it's a *data* global (`void* = nullptr`), not this function -- a
 * completely different symbol, doesn't help here. The real target
 * function is substantial (netman player-list rendering: iterates
 * g_netman's player/session arrays, draws grid cells via UIPANEL_Blit/
 * DrawTextA/UIPANEL_BeginPaint+EndPaintEx) and touches Netman-internal
 * layout offsets I haven't cross-checked against network/Netman.h's
 * canonical fields -- too large/risky to transcribe in this pass.
 * =================================================================== */
void CGWND_GameSetup_DrawGrid_Thunk(void* uiPanel)
{
    (void)uiPanel;
    static bool warned = false;
    if (!warned) {
        std::fprintf(stderr,
            "STUB: CGWND_GameSetup_DrawGrid_Thunk not implemented (TODO: "
            "decompile 0x409980/GameSetupPanel__drawGrid, 725 bytes) -- "
            "grid draw dropped\n");
        warned = true;
    }
}

/* ===================================================================
 * SYMBOL: CGWND_QuitToMenu() [C++-mangled overload]
 * CALLER: Netman::ProcessMessage(TrainMessage*) [network/Netman.cpp:1325]
 * ACTION: caller-declaration-is-wrong (linkage).
 *
 * network/Netman.h:263 and core/CGWND.h:161 both declare `void
 * CGWND_QuitToMenu(void)` OUTSIDE any extern "C" block, so Netman.cpp's
 * call needs the C++-mangled symbol `_Z16CGWND_QuitToMenuv`.
 * shared/defsym_stubs.cpp already has a 0-arg no-op stub, but it's
 * wrapped in `extern "C"` (unmangled `CGWND_QuitToMenu`) -- a genuinely
 * distinct linker symbol from the one Netman.cpp needs. Mirror its
 * no-op behavior under C++ linkage; this is exactly the mismatch flagged
 * in this batch's dispatch note.
 * SHOULD_BE_FIXED_AT: core/CGWND.cpp (add a real C++-linkage definition
 * there once 0x406E80 is transcribed -- see core/CGWND.h:153's doc
 * comment, which already documents real behavior: shuts down netman,
 * sets quit flag, updates audio, unlocks sprites, clears world state,
 * sets game mode to 2). Until then this stays a host no-op like its
 * extern "C" twin.
 * =================================================================== */
void CGWND_QuitToMenu()
{
    /* host no-op, matching shared/defsym_stubs.cpp's extern "C" twin --
     * TODO: replace with a real call once 0x406E80 is transcribed. */
}

/* ===================================================================
 * SYMBOL: TrainStationWindow_UpdateTooltip(int)
 * CALLER: TrainStationWindow::show(int, int) [ui/TrainStationWindow.cpp:29,237]
 * ACTION: loud-deferred-stub. ADDRESS: 0x436D60 (345 bytes, decompiled).
 *
 * The `this` pointer is passed through `legacy_this_pointer(this)` at the
 * call site, i.e. as a plain `int` (matching the original 32-bit x86
 * ABI) -- on this 64-bit host that's a real pointer-width mismatch, the
 * same class of issue as CGWND_ValidatePaletteData above. The real body
 * also dispatches 2 virtual calls (vtable slots 1 and 3) on the tooltip
 * object returned by UI_CreateTooltip — now known to be a plain Entity*
 * (ui/UI_Utils.cpp), so slots 1/3 are GameObject::InvalidateRect/
 * Entity::SetWorldPos; that no longer blocks this function, the `int
 * thisPtr` pointer-width mismatch above still does. Deferring rather
 * than reconstruct a pointer from an int.
 * =================================================================== */
void TrainStationWindow_UpdateTooltip(int thisPtr)
{
    (void)thisPtr;
    static bool warned = false;
    if (!warned) {
        std::fprintf(stderr,
            "STUB: TrainStationWindow_UpdateTooltip not implemented "
            "(0x436D60 decompiled, but the `int`-encoded `this` pointer "
            "and the tooltip object's vtable dispatch are unverified on "
            "this 64-bit host) -- tooltip update dropped\n");
        warned = true;
    }
}

/* ===================================================================
 * SYMBOL: EditWindow_InitNetworkPanel(void*)
 * CALLER: MultiplayerLobby_Reload() [native/multiplayer_lobby_reload.c:26,64]
 * ACTION: loud-deferred-stub. ADDRESS: 0x422820 (real impl exists, but
 * inaccessible from here).
 *
 * The real function at this exact address is EditWindow::netPanelInit()
 * (ui/EditWindow.cpp:855, ui/EditWindow.h:245) -- a 0-arg member method,
 * not a free function taking a void* -- confirmed by the matching address
 * comment in both files. The obvious real fix is to delegate:
 * `static_cast<EditWindow*>(ui_main)->netPanelInit()`. That's NOT
 * possible from this file: netPanelInit() is declared `private:` in
 * ui/EditWindow.h (line 239's access specifier applies to it), so calling
 * it from an unrelated free function is illegal C++ without a friend
 * declaration, which would require editing that header. Deferring rather
 * than reimplementing netPanelInit()'s logic a second time here (which
 * would create the exact kind of duplicate-reconstruction drift CLAUDE.md
 * warns against).
 * SHOULD_BE_FIXED_AT: either make ui/EditWindow.h's `netPanelInit()`
 * public, or add a `friend void EditWindow_InitNetworkPanel(void*);`
 * declaration, then implement this as a one-line delegating call.
 * =================================================================== */
void EditWindow_InitNetworkPanel(void* ui_main)
{
    (void)ui_main;
    static bool warned = false;
    if (!warned) {
        std::fprintf(stderr,
            "STUB: EditWindow_InitNetworkPanel not implemented (real body "
            "is EditWindow::netPanelInit(), ui/EditWindow.cpp:855, but it's "
            "private -- cannot delegate without a header change; see "
            "shared/stubs_link001_batch5_ui_graphics.cpp) -- network panel "
            "init dropped\n");
        warned = true;
    }
}

/* ===================================================================
 * SYMBOL: Game_CheckScreensaverTimeout(void*)
 * CALLER: ScriptedObject::MoveTo(int, int) [game/ScriptedObject.cpp:87,576]
 * ACTION: caller-declaration-is-wrong -- NOT a safe forward.
 *
 * The real (loud-stub) implementation is shared/stubs_impl.cpp:565-566
 * `void Game_CheckScreensaverTimeout(int32_t* game)`, and its one working
 * caller (world/scriptengine.cpp:1179) passes `&g_game` -- the ADDRESS of
 * the global pointer slot. game/ScriptedObject.cpp:576 instead passes
 * `g_game` directly (the Game* value itself, one level of indirection
 * short). Forwarding my (void*) overload straight into the real
 * (int32_t*) one would silently reinterpret a Game* as a pointer-to-a-
 * Game-pointer -- wrong by one level of indirection, not a safe
 * type-punning forward. Since the real target is itself only a loud
 * assert(0) stub today anyway, staying a separate loud stub here changes
 * nothing observable while staying honest about the indirection mismatch.
 * SHOULD_BE_FIXED_AT: game/ScriptedObject.cpp:87,576 (declare
 * `(int32_t* game)` and pass `&g_game`, matching world/scriptengine.cpp's
 * already-correct usage).
 * =================================================================== */
void Game_CheckScreensaverTimeout(void* game)
{
    (void)game;
    static bool warned = false;
    if (!warned) {
        std::fprintf(stderr,
            "STUB: Game_CheckScreensaverTimeout(void*) reached -- caller "
            "(game/ScriptedObject.cpp) passes the Game* directly where the "
            "real function (shared/stubs_impl.cpp) wants &g_game (one more "
            "level of indirection); not a safe forward -- screensaver "
            "timeout check dropped\n");
        warned = true;
    }
}

/* ===================================================================
 * SYMBOL: GameObject_GetSubObjectWorldPos(void*, int*)
 * CALLER: TileMap::ProcessObjectTimer(ResourceGameObject*) [world/tilemap.cpp]
 * ACTION: real-implementation. ADDRESS: 0x458310 (60 bytes, decompiled;
 * Ghidra's own comment names this Entity::GetSubObjectWorldPos).
 *
 * +0x40 is Entity::resource (core/Entity.h:44, a real named field, part
 * of a union with `Entity* parent`). +0x88/+0x8a on the Entity itself,
 * and the two bytes read from *resource at +0x169/+0x16c, are not yet
 * named in core/Entity.h -- kept as documented raw offsets per CLAUDE.md's
 * allowance for a temporary, evidence-recorded cross-cast offset; a
 * future session should integrate these into core/Entity.h's canonical
 * layout and remove this TODO. Ghidra's CONCAT22(hi,lo) simply packs two
 * 16-bit halves into one 32-bit value with no sign extension, replicated
 * exactly below.
 * =================================================================== */
void GameObject_GetSubObjectWorldPos(void* obj, int32_t* out_packed)
{
    Entity* entity = static_cast<Entity*>(obj);
    const uint8_t* resource = static_cast<const uint8_t*>(entity->resource);
    /* TODO: name these two fields on the resource struct once its type is
     * canonicalized -- Ghidra: byte reads at +0x169/+0x16c. */
    uint8_t loBase = resource[0x169];
    uint8_t hiBase = resource[0x16c];
    /* TODO: name these two fields on Entity itself -- Ghidra: +0x88
     * (uint16, packed into the result's high half) and +0x8a (int16,
     * added into the low half before packing). */
    const uint8_t* self = static_cast<const uint8_t*>(obj);
    int16_t offset = *reinterpret_cast<const int16_t*>(self + 0x8a);
    uint16_t hi16 = *reinterpret_cast<const uint16_t*>(self + 0x88);
    uint16_t lo16 = static_cast<uint16_t>(static_cast<uint16_t>(hiBase - loBase) + offset);
    *out_packed = (static_cast<int32_t>(hi16) << 16) | static_cast<int32_t>(lo16);
}

/* ===================================================================
 * SYMBOL: TrackPiece_SetZoom(void*, int)
 * CALLERS: ScriptedObject::UpdateToolState(TrackPiece*);
 *          ScriptedObject::EnterBuildMode(unsigned char)
 *          [game/ScriptedObject.cpp:64, many call sites]
 * ACTION: real-implementation. ADDRESS: 0x40D170 (same address the
 * caller's own comment already cites).
 *
 * Confirmed genuinely distinct from BOTH existing stubs by Itanium
 * mangling: shared/link_stubs.cpp:320 is `extern "C"` (unmangled), and
 * shared/defsym_stubs.cpp:532 is C++-linkage but takes `short`/int16_t
 * (mangles with 's'), not `int`/int32_t (mangles with 'i') -- this
 * caller's `(void* tool, int zoom)` is a third, distinct symbol needing
 * its own definition. The real target is TrackPiece::SetZoom(short)
 * (game/TrackPiece.cpp:266), a public, already-fully-decompiled method
 * at the exact same address. Every call site passes a small literal
 * (1-3), safe to narrow to int16_t.
 * =================================================================== */
void TrackPiece_SetZoom(void* tool, int zoom)
{
    if (tool != nullptr) {
        static_cast<TrackPiece*>(tool)->SetZoom(static_cast<int16_t>(zoom));
    }
}
