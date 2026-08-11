/**
 * HelpWnd_stubs.cpp — Deferred HelpWnd rendering methods
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These methods were moved out of HelpWnd.cpp per AGENTS.md rules.
 * Each is marked with its original address and tracked in PROGRESS.md.
 *
 * render_page (0x452230), render_scroll_up (0x452570),
 * render_scroll_down (0x4526B0), draw_scroll_indicator (0x452B00), and
 * update_anim_sprite (0x452C00) are all transcribed and instruction-level
 * validated against Ghidra disassembly below.
 */

// Status: TRANSCRIBED (render_page / render_scroll_up / render_scroll_down /
// draw_scroll_indicator / update_anim_sprite — all validated
// instruction-by-instruction against disassembly at 0x452230, 0x452570,
// 0x4526B0, 0x452B00, 0x452C00)

#include "HelpWnd.h"
#include "ButtonSprite.h"
#include <cstring>

/* vtable[3] = set_mode (0x414340) — inherited from GameWindow.
 * GameWindow::set_mode(int32_t stateId, void* resdata, uint8_t resetPos, uint8_t forceRedraw)
 * is the canonical definition. HelpWnd does not override this slot.
 * No separate HelpWnd::set_mode stub is needed. */

/* ==================================================================== */
/* External declarations                                                */
/*                                                                       */
/* This is a separate translation unit from ui/HelpWnd.cpp (which is     */
/* off-limits for this task — a parallel agent owns it). Most of these   */
/* externs are copied verbatim from ui/HelpWnd.cpp's own extern block    */
/* so render_page/render_scroll_up/render_scroll_down link against the   */
/* same symbols as every other HelpWnd rendering helper (case 4/5/9 in   */
/* update_button_states, go_next_page, load_page, update_scroll,        */
/* measure_text_height all already call through these exact signatures).*/
/*                                                                       */
/* KNOWN DISCREPANCIES (flagged for the coordinator, not fixed here —    */
/* fixing them touches files outside this task's scope):                */
/*   - Cursor_WaitForBlit/Cursor_Render currently resolve to host         */
/*     no-op/assert stubs in shared/defsym_stubs.cpp and                 */
/*     shared/stubs_impl.cpp, not to the real Cursor::wait_for_blit(HWND)*/
/*     / Cursor::render(...) methods (input/Cursor_Editor.cpp,           */
/*     input/Cursor_Render.cpp). Those are real methods with Cursor-     */
/*     relative field offsets that would misread memory if called on a  */
/*     non-Cursor `this` like HelpWnd — the `(Cursor*)this` cast here is *
 *     the existing subsystem-wide convention for a free-function-shaped *
 *     ABI, not real inheritance (HelpWnd derives from GameWindow, not   */
/*     Cursor). Verified via disassembly at 0x4524C1..0x4524C7 that the  */
/*     real 0x414BB0 entry point is actually __thiscall(this, HWND) —    */
/*     ui/HelpWnd.cpp's 1-arg declaration silently drops the HWND arg;   */
/*     preserved as-is here for link/behavior consistency with the rest  */
/*     of the file.                                                      */
/*   - g_resmgr: ui/HelpWnd.cpp used to declare this as `ResourceManager*`*/
/*     (pointer), but that was itself a landmine (see PROGRESS.md,       */
/*     "g_resmgr extern-type-mismatch") fixed to the correct object form */
/*     while this agent was running — `extern ResourceManager g_resmgr;` */
/*     matches resources/ResourceManager.h and the disassembly (`MOV     */
/*     ECX, 0x4855e8` loads the literal address, not a dereferenced      */
/*     pointer — FormatResourceString is a thiscall method on the object)*/
/*     This file now declares the same, correct, object form below.      */
/* ==================================================================== */

extern "C" {
    extern BOOL   __stdcall SetRect(void* rect, int l, int t, int r, int b);
    extern BOOL   __stdcall OffsetRect(void* rect, int dx, int dy);
    extern BOOL   __stdcall CopyRect(void* dst, void* src);
    extern int    __stdcall SetBkMode(void* hdc, int mode);
    extern int    __stdcall SetTextColor(void* hdc, int color);
    extern void*  __stdcall SelectObject(void* hdc, void* obj);
    extern int    __stdcall DrawTextA(void* hdc, const char* str, int len,
                                       void* rect, int format);
    extern BOOL   __stdcall GetWindowRect(void* hWnd, void* rect);   /* USER32 */
    extern void   __stdcall OutputDebugStringA(const char* msg);      /* @0x477090 - indirect */
}

/* Copied verbatim from ui/HelpWnd.cpp — see discrepancy note above. */
extern int    Cursor_WaitForBlit(void* self);                          /* 0x414BB0 */
extern void   Cursor_Render(void* cursor, int hWnd, int hdc, char flag); /* 0x414C20 */

/* Copied verbatim from ui/HelpWnd.cpp — see discrepancy note above. */
extern void   FormatResourceString(void* resmgr, UINT id, char* buf, int maxLen); /* 0x447330 */

/* Real def: ui/UIPANEL_Surface.cpp (bool __thiscall UIPANEL_Blit(void*,
 * uint32_t,uint32_t,int32_t,uint32_t,void*,uint32_t,uint32_t,int32_t,
 * uint32_t,uint32_t)) — matches ui/ButtonSprite.cpp's already-VALIDATED
 * declaration exactly (confirmed via disassembly at 0x4524AA..0x4524B3:
 * ECX ends up holding btnScrollBar->surface right before the call, i.e.
 * the first param really is passed as `this`). ui/HelpWnd.cpp's own
 * extern for this symbol uses plain `int` in the last four slots where
 * the real definition uses `uint32_t` — a different mangled name,
 * currently unexercised dead code there (flagged for the coordinator).
 * This is the first live call site for UIPANEL_Blit in the HelpWnd
 * subsystem, so the correct form is used here rather than inheriting
 * that mismatch. */
extern bool UIPANEL_Blit(void* srcSurface, uint32_t destX, uint32_t destY,
                          int32_t destW, uint32_t destH, void* targetSurface,
                          uint32_t srcX, uint32_t srcY, int32_t srcW, uint32_t srcH,
                          uint32_t flags); /* 0x42B050 */

extern ResourceManager g_resmgr;       /* 0x4855E8 — object, not a pointer;
                                         * see discrepancy note above. */
extern HFONT g_font_small;             /* 0x4855F4 */

/* Font handle used only by render_scroll_down (0x4526B0), read at
 * 0x4526E0 (`MOV EDX, dword ptr [0x004855ec]`). Immediately adjacent to
 * g_resmgr (0x4855E8) and g_font_small (0x4855F4) but distinct from both;
 * no write site to this address was found anywhere in the binary (its
 * initialization, if any, lives in an unrecovered init routine). Named
 * descriptively rather than left as DAT_004855ec; storage defined in
 * shared/stubs_impl.cpp alongside g_font_small. */
extern HFONT g_font_scroll_down_hint;  /* 0x4855EC */

/* Copied verbatim from ui/GameWindow.cpp's own extern block (same symbols,
 * same addresses — DirectDraw primary surface + surface-loss flag used by
 * draw_scroll_indicator below). */
extern void*   g_primary_surface;   /* 0x4FD3C4 — primary surface (IDirectDrawSurface4*) */
extern uint8_t g_surface_lost;      /* 0x4FD218 — 0=ready, 1=lost */

/* Local macros, matching ui/GameWindow.cpp's own (also file-local) definitions. */
#define DDSD_SIZE   0x7C        /* sizeof(DDSURFACEDESC2) */
#define DDBLT_WAIT  0x1000000

/* Completes the `struct IDirectDrawSurface4;` forward declaration from
 * ui/GameWindow.h (this->backbufferSurface's static type). Deliberately
 * NOT `#include "../stubs/ddraw.h"` here: that header transitively pulls
 * in stubs/windows.h's real RECT-pointer / HDC / COLORREF-typed
 * prototypes for SetRect/OffsetRect/SetTextColor/GetWindowRect, which
 * would conflict with this file's own void*-typed externs above (reused verbatim from
 * ui/HelpWnd.cpp's convention, per this task's instructions). This is
 * byte-for-byte the same definition as stubs/ddraw.h's IDirectDrawSurface4
 * (single `vtable` pointer member, identical no-op stub method bodies for
 * the native #ifndef _WIN32 build, which never links real ddraw.lib —
 * the SDL3 host provides the real rendering path via
 * graphics/sdl3_ddraw.h). ui/GameWindow.cpp reaches the same real COM
 * vtable slots this way (Blt=5, Restore=27) plus Lock=25, all confirmed
 * against this file's own disassembly (see draw_scroll_indicator below). */
struct IDirectDrawSurface4 {
    void* vtable;
    int Release() { return 0; }
    int Blt(void* a, void* b, void* c, int d, void* e) { return 0; }
    int Lock(void* a, void* b, int c, int d) { return 0; }
    int Unlock(void* a) { return 0; }
    int GetDC(void** a) { return 0; }
    int ReleaseDC(void* a) { return 0; }
    int SetPalette(void* a) { return 0; }
    int GetSurfaceDesc(void* a) { return 0; }
    int BltFast(int a, int b, void* c, void* d, int e) { return 0; }
    int GetPixelFormat(void* a) { return 0; }
    int SetColorKey(int a, void* b) { return 0; }
    int IsLost() { return 0; }
    int Restore() { return 0; }
};

/**
 * render_page — Render current page text content.
 * Address: 0x452230
 *
 * Reveals the current help page's word-wrapped text one line at a time.
 * update_button_states(4) (0x451FB0, case 4) calls this once per
 * animation tick with a fresh HDC from Cursor_WaitForBlit.
 *
 * this->scrollOffset counts how many lines have already been fully
 * drawn/scrolled past. draw_text(N, hdc) (0x450850, sibling
 * implementation in ui/HelpWnd.cpp — not modified here) computes, via a
 * DT_MODIFYSTRING-based DrawTextA pass over the page's full resource
 * string, the character offset reached after N lines of word wrapping,
 * or -1 once the page text runs out before N lines. This function calls
 * draw_text twice (scrollOffset and scrollOffset+1), extracts the
 * substring between those two offsets — the single next line to reveal
 * — repaints the text-area background from the shared UI panel bitmap
 * (to erase whatever was flushed there previously), then draws that one
 * line of text. Every basic block, GDI state push/pop, and stack buffer
 * matches the disassembly at 0x452230..0x452567 exactly, including the
 * mid-function Cursor_Render/Cursor_WaitForBlit pair that flushes the
 * backbuffer before the background repaint and re-acquires a fresh HDC
 * afterward (the caller's *hdc_p is updated in place, matching
 * `MOV dword ptr [ESI], EAX` at 0x4524D2).
 */
void HelpWnd::render_page(int* hdc_p)
{
    int   prevColor  = SetTextColor((void*)(uintptr_t)*hdc_p, 0xff5c00);
    int   prevBkMode = SetBkMode((void*)(uintptr_t)*hdc_p, 1 /* TRANSPARENT */);
    void* prevFont   = SelectObject((void*)(uintptr_t)*hdc_p, g_font_small);

    char lineText[0x200];
    char pageText[0x200];
    std::memset(lineText, 0, sizeof(lineText));
    std::memset(pageText, 0, sizeof(pageText));

    bool drewNewLine = false;

    if (this->helpDataLoaded != 0 && this->currentPageIdx >= 0) {
        UINT textResId = (UINT)this->pages[this->currentPageIdx].textResId;
        if (textResId != 0) {
            FormatResourceString(&g_resmgr, textResId, pageText, sizeof(pageText));
            if (pageText[0] != '\0') {
                int lineStart = this->draw_text(this->scrollOffset, hdc_p);
                int lineEnd   = this->draw_text(this->scrollOffset + 1, hdc_p);
                if (lineEnd == -1) {
                    lineEnd = (int)std::strlen(pageText);
                }

                /* Extract pageText[lineStart..lineEnd) — the single next
                 * line to reveal. */
                int copyLen = 0;
                for (int i = 0; i < lineEnd; ++i) {
                    if (i >= lineStart) {
                        lineText[copyLen++] = pageText[i];
                    }
                }
                /* Original writes the terminator at index lineEnd (not
                 * copyLen) — MOV byte ptr [ESP+EDI*1+0x240],0 at 0x4523CE
                 * with EDI == lineEnd. Harmless: lineText is zero-filled
                 * above and lineEnd >= copyLen always. Kept literal for
                 * fidelity rather than "fixed" to copyLen. */
                lineText[lineEnd] = '\0';

                RECT textAreaRect;
                textAreaRect.left   = this->btnTextArea->x;
                textAreaRect.top    = this->btnTextArea->y;
                textAreaRect.right  = this->btnTextArea->sourceX;
                textAreaRect.bottom = this->btnTextArea->sourceY;
                RECT destRect;
                CopyRect(&destRect, &textAreaRect);

                /* Flush what's on the backbuffer so far before repainting
                 * the text-area background under the new line. */
                SelectObject((void*)(uintptr_t)*hdc_p, prevFont);
                SetBkMode((void*)(uintptr_t)*hdc_p, prevBkMode);
                SetTextColor((void*)(uintptr_t)*hdc_p, prevColor);
                Cursor_Render((Cursor*)this, (uintptr_t)this->hWnd, *hdc_p, 1);

                /* Repaint the text-area background from the shared UI
                 * panel bitmap (btnScrollBar's surface), erasing whatever
                 * text was just flushed there. Fixed source rect matches
                 * measure_text_height's (0x452170) identical SetRect/
                 * OffsetRect pair exactly. */
                RECT panelSrcRect;
                SetRect(&panelSrcRect, 0, 0, 0xD9, 0x96);
                OffsetRect(&panelSrcRect, 0x2A, 0x23);
                UIPANEL_Blit(this->btnScrollBar->surface,
                             this->btnTextArea->x, this->btnTextArea->y,
                             this->btnTextArea->sourceX, this->btnTextArea->sourceY,
                             this->backbufferSurface,
                             panelSrcRect.left, panelSrcRect.top,
                             panelSrcRect.right, panelSrcRect.bottom, 0);

                this->update_button_states(7);

                int newHdc = Cursor_WaitForBlit((Cursor*)this);
                *hdc_p = newHdc;

                prevColor  = SetTextColor((void*)(uintptr_t)newHdc, 0xff5c00);
                prevBkMode = SetBkMode((void*)(uintptr_t)newHdc, 1);
                prevFont   = SelectObject((void*)(uintptr_t)newHdc, g_font_small);

                OffsetRect(&destRect, 1, -1);
                DrawTextA((void*)(uintptr_t)newHdc, lineText, (int)std::strlen(lineText),
                          &destRect,
                          0x18910 /* DT_MODIFYSTRING|DT_END_ELLIPSIS|DT_NOPREFIX|DT_NOCLIP|DT_WORDBREAK */);

                SelectObject((void*)(uintptr_t)newHdc, prevFont);
                SetBkMode((void*)(uintptr_t)newHdc, prevBkMode);
                SetTextColor((void*)(uintptr_t)newHdc, prevColor);
                Cursor_Render((Cursor*)this, (uintptr_t)this->hWnd, newHdc, 1);
                drewNewLine = true;
            }
        }
    }

    if (!drewNewLine) {
        SelectObject((void*)(uintptr_t)*hdc_p, prevFont);
        SetBkMode((void*)(uintptr_t)*hdc_p, prevBkMode);
        SetTextColor((void*)(uintptr_t)*hdc_p, prevColor);
        Cursor_Render((Cursor*)this, (uintptr_t)this->hWnd, *hdc_p, 1);
    }
}

/**
 * render_scroll_up — Render scroll-up indicator.
 * Address: 0x452570
 *
 * Formats a per-page resource *string* into btnTextArea2's rect. The
 * resource ID comes from this->pages[currentPageIdx].nextPageId — despite
 * the field's name (assigned by load_help_data's script-parsing field
 * order), it is used here purely as a UINT resource string ID, fed
 * straight into FormatResourceString (confirmed at 0x4525E0..0x452636:
 * `MOV EAX,[EBX+ECX*4+0x160]` reads pages[currentPageIdx]+4, then that
 * value is pushed directly as FormatResourceString's resId argument).
 *
 * No-op (after restoring GDI state) if help data isn't loaded, or if the
 * page's resource ID is 0. The caller (update_button_states case 5)
 * already wraps this call in Cursor_WaitForBlit/Cursor_Render.
 */
void HelpWnd::render_scroll_up(int* hdc_p)
{
    int   prevColor  = SetTextColor((void*)(uintptr_t)*hdc_p, 0x461eff);
    int   prevBkMode = SetBkMode((void*)(uintptr_t)*hdc_p, 1 /* TRANSPARENT */);
    void* prevFont   = SelectObject((void*)(uintptr_t)*hdc_p, g_font_small);

    char hintText[0x200];
    std::memset(hintText, 0, sizeof(hintText));

    if (this->helpDataLoaded != 0) {
        /* BUG (preserved): the original has no currentPageIdx >= 0 guard
         * here — only helpDataLoaded is checked before indexing pages[].
         * Disassembly at 0x4525DC..0x4525F5 confirms: JZ on helpDataLoaded
         * only, then an unconditional pages[currentPageIdx] read. Reads
         * pages[-1] (whatever precedes the array in memory) when
         * currentPageIdx == -1. */
        UINT hintResId = (UINT)this->pages[this->currentPageIdx].nextPageId;
        if (hintResId != 0) {
            FormatResourceString(&g_resmgr, hintResId, hintText, sizeof(hintText));

            RECT srcRect;
            srcRect.left   = this->btnTextArea2->x;
            srcRect.top    = this->btnTextArea2->y;
            srcRect.right  = this->btnTextArea2->sourceX;
            srcRect.bottom = this->btnTextArea2->sourceY;
            RECT destRect;
            CopyRect(&destRect, &srcRect);

            DrawTextA((void*)(uintptr_t)*hdc_p, hintText, (int)std::strlen(hintText),
                       &destRect, 0x911 /* DT_CENTER|DT_WORDBREAK|DT_NOCLIP|DT_NOPREFIX */);
        }
    }

    SelectObject((void*)(uintptr_t)*hdc_p, prevFont);
    SetBkMode((void*)(uintptr_t)*hdc_p, prevBkMode);
    SetTextColor((void*)(uintptr_t)*hdc_p, prevColor);
}

/**
 * render_scroll_down — Render scroll-down indicator.
 * Address: 0x4526B0
 *
 * Unlike render_scroll_up, this draws a fixed "..." string (the constant
 * at 0x47F070, confirmed via read_bytes: "...\0") into btnTextArea3's
 * rect — not a per-page resource string. Uses a distinct font global
 * (0x4855EC, g_font_scroll_down_hint) instead of g_font_small.
 *
 * Unconditional — the scrollDownBtnEnabled gate lives entirely in the
 * callers (update_button_states case 9 checks `this->scrollDownBtnEnabled
 * != 0` before calling; highlight_button case 9 similarly), not here.
 * Verified via full disassembly (0x4526B0..0x4527AB): there is no read of
 * +0x144 anywhere in this function's instructions.
 */
void HelpWnd::render_scroll_down(int* hdc_p)
{
    int   prevColor  = SetTextColor((void*)(uintptr_t)*hdc_p, 0xff5c00);
    int   prevBkMode = SetBkMode((void*)(uintptr_t)*hdc_p, 1 /* TRANSPARENT */);
    void* prevFont   = SelectObject((void*)(uintptr_t)*hdc_p, g_font_scroll_down_hint);

    char hintText[0x200];
    std::memset(hintText, 0, sizeof(hintText));
    std::strcpy(hintText, "..."); /* fixed string at 0x47F070 */

    RECT srcRect;
    srcRect.left   = this->btnTextArea3->x;
    srcRect.top    = this->btnTextArea3->y;
    srcRect.right  = this->btnTextArea3->sourceX;
    srcRect.bottom = this->btnTextArea3->sourceY;
    RECT destRect;
    CopyRect(&destRect, &srcRect);

    DrawTextA((void*)(uintptr_t)*hdc_p, hintText, (int)std::strlen(hintText),
               &destRect, 0x812 /* DT_RIGHT|DT_WORDBREAK|DT_NOPREFIX */);

    SelectObject((void*)(uintptr_t)*hdc_p, prevFont);
    SetBkMode((void*)(uintptr_t)*hdc_p, prevBkMode);
    SetTextColor((void*)(uintptr_t)*hdc_p, prevColor);
}

/**
 * draw_scroll_indicator — Blit the scroll indicator to surface.
 * Address: 0x452B00
 *
 * update_button_states (0x451FB0, case 8) calls this before setting
 * btnScrollBar's state. Its job is to refresh this->backbufferSurface's
 * copy of the fixed 0xE8 x 0x130 scroll-indicator region from whatever
 * is currently on-screen at the window's position — a save/sync-from-
 * primary Blt, not a draw of new content — handling DirectDraw surface
 * loss/recovery around it exactly like GameWindow::hide/show
 * (ui/GameWindow.cpp, e.g. 0x413D10) do for the same g_primary_surface/
 * g_surface_lost pair.
 *
 * Every basic block matches the disassembly (0x452B00..0x452BF8):
 *   - SetRect(&indicatorRect, 0,0,0xE8,0x130) — fixed dest-rect size.
 *   - GetWindowRect(this->hWnd, &screenRect), then resize screenRect to
 *     indicatorRect's width/height (screenRect.right/bottom recomputed
 *     from screenRect.left/top + indicatorRect's span) — 0x452B21..
 *     0x452B5B.
 *   - Snapshot g_surface_lost into wasSurfaceLost *before* the Restore
 *     attempt (0x452B4E), matching `MOV AL,[0x4fd218]` preceding the
 *     `TEST AL,AL` / restore branch.
 *   - If surface was lost, call primary->Restore() (vtable[27]) and
 *     clear g_surface_lost on success (0x452B61..0x452B7B).
 *   - Blt (vtable[5]) FROM g_primary_surface's screenRect INTO
 *     this->backbufferSurface's indicatorRect — this->backbufferSurface
 *     is the Blt target ("this"), matching the disassembly's
 *     `MOV ESI,[ESI+0x38]` / `CALL [EDX+0x14]` with ESI (backbufferSurface)
 *     pushed as the first (this) argument (0x452B7C..0x452B9A).
 *   - On Blt failure (non-zero HRESULT), OutputDebugStringA the fixed
 *     string at 0x47F074 ("Error drawing tw bitmap") — 0x452B9D..0x452BAC.
 *   - If the surface was lost coming in AND is no longer marked lost
 *     (i.e. the Restore above succeeded), probe it with a Lock(NULL,
 *     &desc, 0, NULL) call using a zeroed 0x7C-byte DDSURFACEDESC2
 *     (dwSize = 0x7C) — vtable[25], byte offset 0x64 — and if that Lock
 *     itself succeeds, mark the surface lost again (0x452BAC..0x452BF2).
 *     BUG (preserved): the original never calls Unlock for this probe —
 *     verified there is no Unlock/vtable[33] call anywhere in this
 *     function's instructions.
 */
void HelpWnd::draw_scroll_indicator()
{
    RECT indicatorRect;
    SetRect(&indicatorRect, 0, 0, 0xE8, 0x130);

    RECT screenRect;
    GetWindowRect((void*)(uintptr_t)this->hWnd, &screenRect);
    bool wasSurfaceLost = (g_surface_lost != 0);
    screenRect.right  = screenRect.left + (indicatorRect.right  - indicatorRect.left);
    screenRect.bottom = screenRect.top  + (indicatorRect.bottom - indicatorRect.top);

    if (g_surface_lost != 0) {
        int restoreResult = ((IDirectDrawSurface4*)g_primary_surface)->Restore();
        if (restoreResult == 0) {
            g_surface_lost = 0;
        }
    }

    /* Blt: sync backbufferSurface's indicator region from whatever is
     * currently on-screen at the window's position on the primary
     * surface. */
    int bltResult = ((IDirectDrawSurface4*)this->backbufferSurface)->Blt(
        &indicatorRect, g_primary_surface, &screenRect, DDBLT_WAIT, NULL);
    if (bltResult != 0) {
        OutputDebugStringA("Error drawing tw bitmap"); /* string @ 0x47F074 */
    }

    if (wasSurfaceLost && g_surface_lost == 0) {
        /* Probe: zeroed DDSURFACEDESC2 (31 DWORDs = 0x7C bytes), dwSize
         * set to DDSD_SIZE. See BUG note above re: missing Unlock. */
        DWORD probeDesc[0x1F] = { 0 };
        probeDesc[0] = DDSD_SIZE;
        int lockResult = ((IDirectDrawSurface4*)g_primary_surface)->Lock(
            NULL, probeDesc, 0, 0);
        if (lockResult == 0) {
            g_surface_lost = 1;
        }
    }
}

/**
 * update_anim_sprite — Render animation sprite at frame offset.
 * Address: 0x452C00
 *
 * update_button_states (0x451FB0, case 6) calls this with
 * this->animFrameCount as frameOffset once per animation tick.
 * No-op unless this->active (+0x14C) is set. First refreshes the
 * scroll-indicator background via draw_scroll_indicator() (0x452B00,
 * called unconditionally at 0x452C16 before anything else), then blits
 * btnAnim (+0x130) to this->backbufferSurface.
 *
 * Deliberately NOT the same computation as ButtonSprite::setState
 * (0x454C30, ui/ButtonSprite.cpp): setState reads the source rect's
 * width/height straight from the pixel-data header (pixelData+0x14 /
 * +0x16), but this function instead computes the source rect's
 * right/bottom as btnAnim->sourceX - btnAnim->x and
 * btnAnim->sourceY - btnAnim->y — confirmed at 0x452C31..0x452C43
 * (`SUB EDX,EBP` on sourceX/x, `SUB ECX,EDI` on sourceY/y) — before
 * optionally applying the same frameOffset*frameWidth horizontal
 * OffsetRect that setState uses, where frameWidth is read the same way
 * setState reads it (ushort at pixelData+0x14, 0x452C51..0x452C60).
 *
 * The final UIPANEL_Blit call (0x452C79..0x452CB8, i.e. call to
 * 0x42B050) passes btnAnim's own surface/x/y/sourceX/sourceY as the
 * source-surface/dest-rect arguments and this->backbufferSurface
 * (+0x38) as the target surface — matching every argument at
 * 0x452C7C..0x452CB5 exactly.
 */
void HelpWnd::update_anim_sprite(int frameOffset)
{
    if (this->active == 0) {
        return;
    }

    this->draw_scroll_indicator();

    ButtonSprite* anim = this->btnAnim;

    RECT srcRect;
    srcRect.left   = 0;
    srcRect.top    = 0;
    srcRect.right  = anim->sourceX - anim->x;
    srcRect.bottom = anim->sourceY - anim->y;

    if (frameOffset != 0) {
        /* Frame width from the pixel-data header, +0x14 (ushort) —
         * same layout ButtonSprite::setState documents/reads. */
        const auto* pixelHdr = reinterpret_cast<const uint8_t*>(anim->pixelData);
        uint16_t frameWidth = *reinterpret_cast<const uint16_t*>(pixelHdr + 0x14);
        OffsetRect(&srcRect, frameOffset * (int)frameWidth, 0);
    }

    bool blitOk = UIPANEL_Blit(
        anim->surface, anim->x, anim->y, anim->sourceX, anim->sourceY,
        this->backbufferSurface,
        srcRect.left, srcRect.top, srcRect.right, srcRect.bottom, 0);
    if (!blitOk) {
        OutputDebugStringA("Error drawing tw bitmap"); /* string @ 0x47F074 */
    }
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
