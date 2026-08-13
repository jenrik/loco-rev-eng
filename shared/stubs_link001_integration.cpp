/**
 * stubs_link001_integration.cpp — LINK-001 cleanup, final integration pass
 *
 * The 6 stubs_link001_batch*.cpp files (each written by an isolated agent
 * covering a disjoint slice of the undefined-symbol list) left exactly 12
 * genuinely-called symbols unresolved after merge — either because a batch's
 * worktree was stale relative to this tree's uncommitted resources/Win32Stream
 * work (batch2), or because a symbol fell through the cracks between two
 * batches' assigned lists (ScriptedObject_ParseStream/InitBase). This file
 * closes the remaining gap. See PROGRESS.md for the session write-up.
 */

// Status: TRANSCRIBED

#include <cstdio>
#include <cstdint>
#include <cassert>

#include "../ui/HelpWnd.h"

/* ===================================================================
 * WIN32_Stream* bare (extern "C") forms.
 *
 * resources/Win32Stream.cpp's real implementations are plain C++-linkage
 * (uint32_t-typed params, no `extern "C"`). Several callers instead declare
 * these names inside their own local `extern "C" { }` blocks (game/
 * TrainStation.cpp, input/BuildingDescriptorEditor.cpp, ui/CursorEditWindow.cpp,
 * ui/HelpWnd.cpp, ui/UIPANEL_Surface.cpp, input/Cursor_internal.h) — giving
 * them unmangled linkage, a genuinely distinct linker symbol from the real
 * C++-mangled body. Bridging each to the real implementation (not
 * duplicating logic) so every caller reaches the one real code path.
 *
 * A same-named, same-parameter-list extern "C" declaration can't coexist in
 * one translation unit with resources/Win32Stream.h's C++-linkage
 * declaration of the identical signature (that's a hard "conflicting
 * declaration with different language linkage" error, not an overload) —
 * so the real functions are re-declared here under their already-mangled
 * link names via GCC's asm-label extension instead of #include-ing that
 * header, letting both linkages of the same spelled name coexist as the
 * distinct symbols they actually are. */
extern void* Win32Stream_RealOpen(void*, int) asm("_Z16WIN32_StreamOpenPvi");
extern void Win32Stream_RealOpenPath(void*, const char*, uint32_t, uint32_t)
    asm("_Z20WIN32_StreamOpenPathPvPKcjj");
extern void* Win32Stream_RealOpenFile(void*, const char*, uint32_t, uint32_t, int)
    asm("_Z20WIN32_StreamOpenFilePvPKcjji");
extern void* Win32Stream_RealRead(void*, void*, uint32_t) asm("_Z16WIN32_StreamReadPvS_j");
extern void Win32Stream_RealDestroyImmediate(void*) asm("_Z28WIN32_StreamDestroyImmediatePv");
/* No WIN32_StreamDestroy bridge here anymore: resources/Win32Stream.h/.cpp
 * no longer define that symbol at all (0x463A80 is pure MSVC vptr-
 * retagging bookkeeping with no observable effect once real C++
 * virtual-base destruction is in play — see that header's doc comment on
 * it), and every real caller has been converted to a real WIN32_Stream
 * local that no longer calls it. */

extern "C" {

void* WIN32_StreamOpen(void* stream, int mode)
{
    return Win32Stream_RealOpen(stream, mode);
}

void WIN32_StreamOpenPath(void* stream, const char* path, int mode, int extra)
{
    Win32Stream_RealOpenPath(stream, path, static_cast<uint32_t>(mode),
                              static_cast<uint32_t>(extra));
}

/* input/Cursor_internal.h's extern "C" 5-arg shape. */
void* WIN32_StreamOpenFile(void* stream, const char* path, uint32_t flags,
                            uint32_t shareMask, uint32_t initBase)
{
    return Win32Stream_RealOpenFile(stream, path, flags, shareMask,
                                     static_cast<int>(initBase));
}

void* WIN32_StreamRead(void* stream, void* buf, int32_t size)
{
    return Win32Stream_RealRead(stream, buf, static_cast<uint32_t>(size));
}

void WIN32_StreamDestroyImmediate(void* stream)
{
    Win32Stream_RealDestroyImmediate(stream);
}

} /* extern "C" */

/* ===================================================================
 * WIN32_StreamOpen(void*, char const*, int, void*, int) — C++-linkage
 * 5-arg overload ui/GameSetupPanel.cpp:75 declares (outside the file's
 * extern "C" block, which closes at line 64), used by loadLayouts(bool).
 * Distinct symbol from both the real 2-arg WIN32_StreamOpen and the bare
 * extern "C" bridge above (different mangled/unmangled names).
 * =================================================================== */
void* WIN32_StreamOpen(void* stream, const char* path, int mode, void* extra, int flag)
{
    static bool warned = false;
    if (!warned) {
        std::fprintf(stderr,
            "STUB: WIN32_StreamOpen(void*, char const*, int, void*, int) not "
            "implemented (GameSetupPanel::loadLayouts's 5-arg open shape has "
            "no evidenced real body distinct from WIN32_StreamOpenFile) -- "
            "layout list load dropped\n");
        warned = true;
    }
    (void)stream; (void)path; (void)mode; (void)extra; (void)flag;
    return nullptr;
}

/* ===================================================================
 * HelpWnd::set_mode(void*, void*, int, int)
 * CALLER: HelpWnd::handle_mouse_move (ui/HelpWnd.cpp:778,786,795,800)
 * ADDRESS: unknown -- ui/HelpWnd.h:271's "0x414340" comment is stale: that
 * address has zero direct-call xrefs anywhere in the binary (only 4 vtable
 * DATA references), i.e. it's the *virtual* GameWindow::set_mode(int,void*,
 * uint8_t,uint8_t) override, not this distinct 4-arg member overload that
 * HelpWnd declares to hide the inherited name. Needs re-derivation from
 * handle_mouse_move's own disassembly in a follow-up session.
 * =================================================================== */
void HelpWnd::set_mode(void* countPtr, void* dataPtr, int modeA, int modeB)
{
    (void)countPtr; (void)dataPtr; (void)modeA; (void)modeB;
    static bool warned = false;
    if (!warned) {
        std::fprintf(stderr,
            "STUB: HelpWnd::set_mode(void*,void*,int,int) not implemented "
            "(TODO: re-derive real address -- ui/HelpWnd.h's cited 0x414340 "
            "is GameWindow's unrelated virtual set_mode override) -- cursor "
            "dispatch dropped\n");
        warned = true;
    }
}

/* ===================================================================
 * ScriptedObject_ParseStream(void*) / ScriptedObject_InitBase(uint32_t,int)
 * CALLER: game/ScriptedObject.cpp (own local declarations, lines 47-48)
 * ADDRESS: unknown -- the caller's own cited addresses (0x41E9F0, 0x4203E0)
 * decompile to unrelated functions (BuildingDescriptorEditor's .dat parser
 * and EditWindow's base destructor respectively), i.e. both comments are
 * stale/wrong, same class of defect found elsewhere in this pass
 * (CGWND_ValidatePaletteData, Cursor_UpdateDirtyRect). Real addresses need
 * re-deriving from ScriptedObject's own xrefs in a follow-up RE session.
 * =================================================================== */
char ScriptedObject_ParseStream(void* stream)
{
    (void)stream;
    static bool warned = false;
    if (!warned) {
        std::fprintf(stderr,
            "STUB: ScriptedObject_ParseStream not implemented (TODO: "
            "re-derive real address -- caller's cited 0x41E9F0 is unrelated) "
            "-- script parse dropped, reporting failure\n");
        warned = true;
    }
    return 0;
}

void ScriptedObject_InitBase(uint32_t resource_id, int zero)
{
    (void)resource_id; (void)zero;
    static bool warned = false;
    if (!warned) {
        std::fprintf(stderr,
            "STUB: ScriptedObject_InitBase not implemented (TODO: re-derive "
            "real address -- caller's cited 0x4203E0 is unrelated) -- base "
            "init dropped\n");
        warned = true;
    }
}

/* ===================================================================
 * SetTimer(void*, unsigned long, unsigned int, TIMERPROC)
 * CALLER: EditorState_StartGameTimer (shared/stubs_link001_batch4_network_world.cpp)
 *
 * A distinct overload from the (void*,uint32_t,uint32_t,void*) shape
 * shared/stubs_link001_batch1_crt_win32.cpp already implements for
 * ui/TrainStationWindow.cpp -- this one's 4th param is a real TIMERPROC
 * callback pointer. Host has no real timer-callback mechanism; matches the
 * existing safe-stub convention (return a nonzero fake id, never invoke the
 * callback).
 * =================================================================== */
uintptr_t SetTimer(void* hWnd, unsigned long nIDEvent, unsigned int uElapse,
                    void (*lpTimerFunc)(void*, unsigned int, unsigned long, unsigned int))
{
    (void)hWnd; (void)uElapse; (void)lpTimerFunc;
    return nIDEvent != 0 ? nIDEvent : 1;
}
