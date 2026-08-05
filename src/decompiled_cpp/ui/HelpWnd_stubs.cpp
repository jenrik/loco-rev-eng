/**
 * HelpWnd_stubs.cpp — Deferred HelpWnd rendering stubs
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These stub methods were moved out of HelpWnd.cpp per AGENTS.md rules.
 * Each stub is marked with its original address and tracked in PROGRESS.md.
 */

// Status: STUB — see PROGRESS.md "Remaining work"

#include "HelpWnd.h"
#include <cassert>
#include <cstdio>

/**
 * set_mode — vtable[3] cursor dispatch. Inherited from GameWindow.
 * Address: 0x414340
 *
 * TODO: decompile 0x414340
 */
void HelpWnd::set_mode(void*, void*, int, int)
{
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — HelpWnd::set_mode 0x414340");
}

/**
 * render_page — Render current page text content.
 * Address: 0x452230
 *
 * TODO: decompile 0x452230
 */
void HelpWnd::render_page(int*)
{
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — HelpWnd::render_page 0x452230");
}

/**
 * render_scroll_up — Render scroll-up indicator.
 * Address: 0x452570
 *
 * TODO: decompile 0x452570
 */
void HelpWnd::render_scroll_up(int*)
{
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — HelpWnd::render_scroll_up 0x452570");
}

/**
 * render_scroll_down — Render scroll-down indicator.
 * Address: 0x4526B0
 *
 * TODO: decompile 0x4526B0
 */
void HelpWnd::render_scroll_down(int*)
{
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — HelpWnd::render_scroll_down 0x4526B0");
}

/**
 * draw_scroll_indicator — Blit the scroll indicator to surface.
 * Address: 0x452B00
 *
 * TODO: decompile 0x452B00
 */
void HelpWnd::draw_scroll_indicator()
{
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — HelpWnd::draw_scroll_indicator 0x452B00");
}

/**
 * update_anim_sprite — Render animation sprite at frame offset.
 * Address: 0x452C00
 *
 * TODO: decompile 0x452C00
 */
void HelpWnd::update_anim_sprite(int)
{
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — HelpWnd::update_anim_sprite 0x452C00");
}

static void stream_vtable_scalar_dtor(int* streamObj)
{
    /* vtable[1] = scalar deleting destructor: (void* this, int flags).
     * Reads vtable ptr at *streamObj, indexes slot [1], calls with flags=1.
     * TODO: decompile stream class at 0x479190 — replace with 'delete streamObj'. */
    using ScalarDeletingDestructor = void (*)(void*, int);
    ScalarDeletingDestructor* vtable =
        *reinterpret_cast<ScalarDeletingDestructor**>(streamObj);
    vtable[1](streamObj, 1);
}

