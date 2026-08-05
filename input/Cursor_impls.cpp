// Status: INTEGRATED
/* Cursor_impls.cpp — Cursor core state/render helpers
 *
 * Each method is validated against the Ghidra database (locon) and tagged
 * with its original binary address.
 *
 * NOTE on set_mode: Cursor does NOT define its own set_mode in the binary.
 * The Cursor vtable slot [3] at 0x47793C points to UI_WindowBase::set_mode
 * (0x425FD0); the GameWindow-family 0x414340 implementation belongs to
 * ui/GameWindow.cpp and is not part of the Cursor class. Code that needs
 * the cursor-mode dispatch calls `this->set_mode(...)` which resolves
 * virtually to the base implementation.
 */

#include "Cursor.h"
#include "Cursor_internal.h"

#ifndef _WIN32
/* Non-Windows stubs for Win32 functions used by set_capture().
 * (declared in stubs/windows.h but sdl3_window.h does not yet provide them;
 * host-only deviation — see AGENTS.md "Host deviations".) */
static inline HWND GetCapture(void) { return nullptr; }
static inline HWND SetCapture(HWND hWnd) { (void)hWnd; return nullptr; }
static inline BOOL ReleaseCapture(void) { return 0; }
static inline int ShowCursor(int show) { (void)show; return -1; }
#endif

/* ================================================================== */
/* Cursor::on_show — Pre-show virtual hook (vtable[7])                */
/* Address: 0x417180 (unlabeled code region; NOT the 0x426130 stub)    */
/*                                                                     */
/* The Cursor vtable slot [7] (0x47794C) points at 0x417180, an        */
/* unlabeled region between Cursor_Hide (ends 0x417033) and            */
/* Cursor_InitEditorSprites (0x417F20). The only verifiable fact is    */
/* the instruction at 0x417186 calling UI_WindowBase::on_create        */
/* (0x425D30) — the base client-rect synchronizer. The rest of the     */
/* region's behavior is unconfirmed (no function is defined there in   */
/* Ghidra). Dispatched with no arguments from Cursor::show @ 0x416B8F. */
/* ================================================================== */
void Cursor::on_create()
{
    /* Chain to the base on_create (0x425D30) per the 0x417186 call. */
    this->on_create();
}


/* ================================================================== */
/* Cursor::handle_window_paint — Handle WM_PAINT for cursor window    */
/* Address: 0x414A80                                                    */
/*                                                                     */
/* If hWnd matches this->hWnd: unlocks primary, updates dirty rect,   */
/* unlocks all surfaces, optionally renders with viewport.             */
/* Returns 0 always.                                                    */
/* ================================================================== */
int32_t Cursor::handle_window_paint(HWND hWnd)
{
    if (hWnd != this->hWnd)                           /* +0x08 */
        return 0;

    DDRAW_UnlockPrimary();
    this->update_dirty_rect(1);

    Cursor_UnlockAllSurfaces();

    if (this->viewport_render_enabled() != 0) {         /* +0x88 */
        this->render_with_viewport(1);
    }

    return 0;
}


/* ================================================================== */
/* Cursor::set_capture — Toggle Windows mouse capture and OS cursor   */
/* Address: 0x414290                                                    */
/*                                                                     */
/* releaseFlag != 0 (ACQUIRE): sets capture_flag, releases Windows    */
/*   capture, updates dirty rect, optionally renders with viewport.   */
/* releaseFlag == 0 (RELEASE): clears capture_flag, calls SetCapture, */
/*   hides OS cursor via ShowCursor loop.                             */
/*                                                                     */
/* Validated: control flow, +0x58 byte flag, GetCapture()==hWnd check,*/
/* Game_SetScreenMode(g_game,0,0,0), +0x88 viewport gate.             */
/* ================================================================== */
void Cursor::set_capture(uint8_t releaseFlag)
{
    if (releaseFlag != 0) {
        /* ACQUIRE game cursor */
        if (this->capture_flag() == 0) {                /* +0x58 */
            this->capture_flag() = 1;

            /* Release Windows mouse capture if our window owns it */
            HWND capturedWnd = GetCapture();
            if (capturedWnd == this->hWnd) {          /* +0x08 */
                ReleaseCapture();
            }

            DDRAW_UnlockPrimary();
            this->update_dirty_rect(1);
            Cursor_UnlockAllSurfaces();

            /* Dispatch screen mode change notification */
            Game_SetScreenMode(g_game, 0, 0, 0);

            if (this->viewport_render_enabled() != 0) { /* +0x88 */
                this->render_with_viewport(1);
            }
        }
    } else {
        /* RELEASE game cursor */
        if (this->capture_flag() != 0 || GetCapture() != this->hWnd) {
            this->capture_flag() = 0;                   /* +0x58 */

            /* Re-acquire Windows mouse capture */
            SetCapture(this->hWnd);

            /* Hide OS cursor — loop until ShowCursor returns < 0 */
            int showCount = ShowCursor(0);  /* FALSE = hide */
            while (showCount >= 0) {
                showCount = ShowCursor(0);
            }
        }
    }
}


/* ================================================================== */
/* Cursor::update_dirty_rect — Update cursor dirty-rect tracker       */
/* Address: 0x414FB0                                                    */
/*                                                                     */
/* Gets current cursor position, adjusts by hotspot, clips to         */
/* viewport (+0x18..+0x24), unions with stored cursor_rect (+0x68),   */
/* and composites the cursor. Two paths: accelerated (<256px dirty    */
/* rect) vs normal.                                                    */
/*                                                                     */
/* Validated: GetCursorPos → hotspot (+0x44+0x32/+0x34) → clip →      */
/* UnionRect(+0x68) → accelerate gate requires +0x14 (state), +0x70   */
/* (secondary flag), param, +0x58 (capture) clear and both union      */
/* dims < 0x100 → 4px expansion → re-clip → background restore        */
/* (g_backbuffer ← primary +0x38) when not accelerated → IntersectRect*/
/* → dirty markers +0x50/+0x54 = -1 → 3-blit composite (capture,      */
/* colour-key overlay, composite).                                     */
/* ================================================================== */
void Cursor::update_dirty_rect(uint8_t param)
{
    /* Get current cursor position */
    POINT cursorPos;
    GetCursorPos(&cursorPos);

    /* Get animation data; adjust by hotspot offset */
    void* animData = this->anim_resdata();             /* +0x44 */
    uint32_t spriteW, spriteH;
    if (animData == nullptr) {
        spriteW = 0;
        spriteH = 0;
    } else {
        auto* animationBytes = static_cast<uint8_t*>(animData);
        cursorPos.x -= *reinterpret_cast<int16_t*>(animationBytes + 0x32); /* hotspot_x */
        cursorPos.y -= *reinterpret_cast<int16_t*>(animationBytes + 0x34); /* hotspot_y */
        spriteW = *reinterpret_cast<uint16_t*>(animationBytes + 0x14); /* width  */
        spriteH = *reinterpret_cast<uint16_t*>(animationBytes + 0x16); /* height */
    }

    /* Build new cursor rect */
    RECT newRect;
    newRect.right  = cursorPos.x + static_cast<int>(spriteW);
    newRect.bottom = cursorPos.y + static_cast<int>(spriteH);

    /* Clip left/top edges to viewport */
    newRect.left = cursorPos.x;
    if (cursorPos.x < this->clip_rect_left()) {          /* +0x18 */
        newRect.left   = this->clip_rect_left();
        spriteW        = newRect.right - this->clip_rect_left();
    }
    newRect.top = cursorPos.y;
    if (cursorPos.y < this->clip_rect_top()) {           /* +0x1C */
        newRect.top    = this->clip_rect_top();
        spriteH        = newRect.bottom - this->clip_rect_top();
    }
    /* Clip right/bottom edges */
    if (this->clip_rect_right() < newRect.right) {       /* +0x20 */
        newRect.right  = this->clip_rect_right();
        spriteW        = static_cast<uint32_t>(this->clip_rect_right() - newRect.left);
    }
    if (this->clip_rect_bottom() < newRect.bottom) {     /* +0x24 */
        newRect.bottom = this->clip_rect_bottom();
        spriteH        = static_cast<uint32_t>(this->clip_rect_bottom() - newRect.top);
    }

    /* Union with stored cursor rect (+0x68) */
    RECT unionRect;
    UnionRect(&unionRect, &this->cursor_rect(), &newRect);

    /* Accelerated-path eligibility (before the 4px expansion):
     * state active (+0x14), established cursor rect (+0x70 = cursor_rect.right
     * non-zero), param set, not captured (+0x58), and union < 256px in both
     * dimensions. */
    bool accelerated = false;
    if (unionRect.right - unionRect.left >= 0 &&
        unionRect.bottom - unionRect.top >= 0 &&
        this->cursor_state() != 0 &&
        this->cursor_rect().right != 0 && param != 0 &&
        this->capture_flag() == 0 &&
        unionRect.right - unionRect.left < 0x100 &&
        unionRect.bottom - unionRect.top < 0x100) {
        accelerated = true;
    }

    /* Expand by 4px (anti-alias bleed) */
    unionRect.left   -= 4;
    unionRect.top    -= 4;
    unionRect.right  += 4;
    unionRect.bottom += 4;

    /* Re-clip to viewport */
    if (this->clip_rect_right() < unionRect.right)
        unionRect.right = this->clip_rect_right();
    if (this->clip_rect_bottom() < unionRect.bottom)
        unionRect.bottom = this->clip_rect_bottom();
    if (unionRect.top < this->clip_rect_top())
        unionRect.top = this->clip_rect_top();
    if (unionRect.left < this->clip_rect_left())
        unionRect.left = this->clip_rect_left();

    /* NORMAL-path background restore: copy the old cursor area from the
     * primary surface (+0x38) into the backbuffer (+0x5C) via the legacy
     * Blt slot [5] (the binary: g_backbuffer ← primary). Gated on the
     * established cursor rect (+0x70) as in the accelerate condition. */
    if (this->cursor_rect().right != 0 && param != 0 && !accelerated) {
        RECT restoreRect;
        CopyRect(&restoreRect, &this->cursor_rect());
        OffsetRect(&restoreRect,
                   -this->clip_rect_left(), -this->clip_rect_top());
        Cursor_SurfaceLegacyBlt(_g_backbuffer)(
            _g_backbuffer,
            reinterpret_cast<int*>(&this->cursor_rect()),
            this->primary_surface(),
            reinterpret_cast<int*>(&restoreRect),
            BLIT_WAIT, nullptr);
    }

    /* Intersect new rect with clip rect for the final blit sizes.
     * (The binary computes this at 0x4150B2; the result is overwritten in
     * both composite paths, so it only serves as the normal-path blit A
     * left edge.) */
    [[maybe_unused]] RECT intersectRect;
    IntersectRect(&intersectRect, &newRect, this->clip_rect());

    /* Dirty-rect markers */
    this->dirty_rect_left() = -1;                        /* +0x50 */
    this->dirty_rect_top()  = -1;                        /* +0x54 */

    /* Main render: save new cursor rect and enter the composite path */
    if (this->cursor_state() != 0 && this->capture_flag() == 0) {
        this->cursor_rect() = newRect;                   /* +0x68 */

        RECT dstRect;
        dstRect.left   = 0;
        dstRect.top    = 0;
        dstRect.right  = 0;
        dstRect.bottom = 0;

        if (accelerated) {
            /* ACCELERATED PATH: single composite across the union rect */
            dstRect.right  = unionRect.right - unionRect.left;
            dstRect.bottom = unionRect.bottom - unionRect.top;

            RECT offsetUnion;
            CopyRect(&offsetUnion, &unionRect);
            OffsetRect(&offsetUnion,
                       -this->clip_rect_left(), -this->clip_rect_top());

            /* 1. Capture background into backbuffer */
            Cursor_SurfaceBlt(this->backbuffer())(
                this->backbuffer(),
                &dstRect,
                this->primary_surface(),
                &offsetUnion,
                BLIT_WAIT, nullptr);

            /* Animation frame advance (frame count at RESDATA+0x160) */
            auto* animationBytes = reinterpret_cast<uint8_t*>(this->anim_resdata());
            uint16_t frameCount = *reinterpret_cast<uint16_t*>(animationBytes + 0x160);
            uint32_t frameOffset = 0;
            if (frameCount > 1) {
                if (static_cast<int32_t>(frameCount) <= this->anim_frame()) {
                    this->anim_frame() = 0;
                }
                frameOffset = *reinterpret_cast<uint16_t*>(animationBytes + 0x14) *
                              static_cast<uint32_t>(this->anim_frame());
            }

            /* 2. Overlay cursor sprite (colour-keyed) */
            RECT srcRect;
            srcRect.left  = static_cast<LONG>(frameOffset);
            srcRect.top   = 0;
            srcRect.right = static_cast<LONG>(spriteW + frameOffset);
            srcRect.bottom = static_cast<LONG>(spriteH);
            Cursor_SurfaceBlt(this->primary_surface())(
                this->primary_surface(),
                &dstRect,
                this->cursor_sprite_surface(),
                &srcRect,
                BLIT_KEYSRC_WAIT, nullptr);

            /* 3. Composite backbuffer to scene backbuffer */
            RECT destRect;
            destRect.left   = unionRect.left;
            destRect.top    = unionRect.top;
            destRect.right  = unionRect.right;
            destRect.bottom = unionRect.bottom;
            Cursor_SurfaceBlt(_g_backbuffer)(
                _g_backbuffer,
                &destRect,
                this->primary_surface(),
                &dstRect,
                BLIT_WAIT, nullptr);
        } else {
            /* NORMAL PATH: separate restore + sprite composite.
             * The binary's normal-path blits (backbuffer Blt with
             * &local_60.right / &local_78 rects, register-derived) are only
             * partially recoverable from the decompiler; the pattern below
             * keeps the established three-blit structure (restore, keyed
             * overlay, composite) with the clip-verified rectangles. */
            dstRect.right  = static_cast<LONG>(spriteW);
            dstRect.bottom = static_cast<LONG>(spriteH);

            /* 1. Restore background: backbuffer (+0x5C) ← primary (+0x38) */
            Cursor_SurfaceBlt(this->backbuffer())(
                this->backbuffer(),
                &newRect,
                this->primary_surface(),
                &newRect,
                BLIT_WAIT, nullptr);

            /* 2. Overlay cursor sprite (colour-keyed) onto backbuffer */
            Cursor_SurfaceBlt(this->backbuffer())(
                this->backbuffer(),
                &newRect,
                this->cursor_sprite_surface(),
                nullptr,
                BLIT_KEYSRC_WAIT, nullptr);

            /* 3. Composite backbuffer to scene backbuffer */
            Cursor_SurfaceBlt(_g_backbuffer)(
                _g_backbuffer,
                &newRect,
                this->backbuffer(),
                &newRect,
                BLIT_WAIT, nullptr);
        }
    }
}


/* ================================================================== */
/* Cursor::render_with_viewport — Render cursor with viewport clipping*/
/* Address: 0x415440                                                    */
/*                                                                     */
/* Standalone viewport-aware render (NOT a wrapper around              */
/* update_dirty_rect — the binary performs its own 3-blit composite).  */
/* Builds the viewport rect from g_is_fullscreen/g_client_width/       */
/* g_viewport_x/y, clips the cursor rect, unions with the previous     */
/* dirty rect (+0x78), and runs the accelerated (<256px) or normal     */
/* composite path.                                                     */
/*                                                                     */
/* Validated: GetCursorPos → hotspot → clip to viewport → UnionRect    */
/* (+0x78) → accelerate gate (+0x14, +0x80, param, +0x58, <0x100) →   */
/* background restore (g_backbuffer vtable[5]) → +0x50/+0x54 = -1 →   */
/* 3-blit composite in each path. The decompiler loses several         */
/* register-held arguments (unaff_EBX/ESI); the blit rectangles below  */
/* follow the recoverable pattern.                                     */
/* ================================================================== */
void Cursor::render_with_viewport(uint8_t param)
{
    /* Get cursor position; adjust by hotspot */
    POINT cursorPos;
    GetCursorPos(&cursorPos);

    uint32_t spriteW = 0, spriteH = 0;
    void* animData = this->anim_resdata();               /* +0x44 */
    if (animData != nullptr) {
        auto* animationBytes = static_cast<uint8_t*>(animData);
        cursorPos.x -= *reinterpret_cast<int16_t*>(animationBytes + 0x32);
        cursorPos.y -= *reinterpret_cast<int16_t*>(animationBytes + 0x34);
        spriteW = *reinterpret_cast<uint16_t*>(animationBytes + 0x14);
        spriteH = *reinterpret_cast<uint16_t*>(animationBytes + 0x16);
    }

    RECT cursorRect;
    cursorRect.top    = cursorPos.y;
    cursorRect.left   = cursorPos.x;
    cursorRect.bottom = cursorPos.y + static_cast<int>(spriteH);
    cursorRect.right  = cursorPos.x + static_cast<int>(spriteW);

    /* Build viewport clip rect */
    RECT vpRect;
    if (g_is_fullscreen == 0) {
        CopyRect(&vpRect, reinterpret_cast<const RECT*>(&g_client_width));
    } else {
        CopyRect(&vpRect, reinterpret_cast<const RECT*>(&g_client_width));
        OffsetRect(&vpRect, g_viewport_x, g_viewport_y);
    }

    /* Clip cursor rect to viewport */
    if (cursorRect.left < vpRect.left) {
        cursorRect.left = vpRect.left;
        spriteW = cursorRect.right - vpRect.left;
    }
    if (cursorRect.top < vpRect.top) {
        cursorRect.top = vpRect.top;
        spriteH = cursorRect.bottom - vpRect.top;
    }
    if (vpRect.right < cursorRect.right) {
        cursorRect.right = vpRect.right;
        spriteW = vpRect.right - cursorRect.left;
    }
    if (vpRect.bottom < cursorRect.bottom) {
        cursorRect.bottom = vpRect.bottom;
        spriteH = vpRect.bottom - cursorRect.top;
    }

    /* Union with previous dirty rect at +0x78 (prev_cursor_rect) */
    RECT unionRect;
    UnionRect(&unionRect, &this->prev_cursor_rect(), &cursorRect);

    /* Accelerated-path eligibility */
    bool accelerated = false;
    if (this->cursor_state() != 0 &&            /* +0x14 */
        this->prev_cursor_rect().bottom != 0 && /* +0x80 (prev dirty bottom) */
        param != 0 &&
        this->capture_flag() == 0 &&            /* +0x58 */
        unionRect.right - unionRect.left < 0x100 &&
        unionRect.bottom - unionRect.top < 0x100) {
        accelerated = true;
    }

    /* Clip union rect to viewport */
    if (vpRect.right < unionRect.right)   unionRect.right = vpRect.right;
    if (vpRect.bottom < unionRect.bottom) unionRect.bottom = vpRect.bottom;
    if (unionRect.top < vpRect.top)       unionRect.top = vpRect.top;
    if (unionRect.left < vpRect.left)     unionRect.left = vpRect.left;

    /* Background restore (normal path): copy old cursor area from primary
     * surface to backbuffer via vtable[5]. */
    if (this->prev_cursor_rect().bottom != 0 && param != 0 && !accelerated) {
        RECT restoreRect;
        CopyRect(&restoreRect, &this->prev_cursor_rect());
        OffsetRect(&restoreRect, -vpRect.left, -vpRect.top);
        Cursor_SurfaceBlt(_g_backbuffer)(
            _g_backbuffer,
            &this->prev_cursor_rect(),
            this->primary_surface(),
            &restoreRect,
            BLIT_WAIT, nullptr);
    }

    /* Dirty markers */
    this->dirty_rect_left() = -1;                        /* +0x50 */
    this->dirty_rect_top()  = -1;                        /* +0x54 */

    if (this->cursor_state() != 0 && this->capture_flag() == 0) {
        /* Save the new cursor rect as the dirty rect */
        this->prev_cursor_rect().left   = cursorRect.left;
        this->prev_cursor_rect().top    = cursorRect.top;
        this->prev_cursor_rect().right  = cursorRect.right;
        this->prev_cursor_rect().bottom = cursorRect.bottom;

        RECT dstRect;
        dstRect.left   = 0;
        dstRect.top    = 0;
        dstRect.right  = 0;
        dstRect.bottom = 0;

        if (accelerated) {
            /* ACCELERATED PATH: single composite across the union rect.
             * The binary (0x41553F..0x4155E7) runs three blits: capture
             * union from primary into the cursor backbuffer (+0x5C), overlay
             * the colour-keyed sprite frame, composite back to the scene
             * backbuffer. */
            dstRect.right  = unionRect.right - unionRect.left;
            dstRect.bottom = unionRect.bottom - unionRect.top;

            RECT offsetUnion;
            CopyRect(&offsetUnion, &unionRect);
            OffsetRect(&offsetUnion, -vpRect.left, -vpRect.top);

            /* 1. Capture background into cursor backbuffer */
            Cursor_SurfaceBlt(this->backbuffer())(
                this->backbuffer(),
                &dstRect,
                this->primary_surface(),
                &offsetUnion,
                BLIT_WAIT, nullptr);

            /* Animation frame advance (frame count at RESDATA+0x160) */
            auto* animationBytes = reinterpret_cast<uint8_t*>(this->anim_resdata());
            uint16_t frameCount = *reinterpret_cast<uint16_t*>(animationBytes + 0x160);
            uint32_t frameOffset = 0;
            if (frameCount > 1) {
                if (static_cast<int32_t>(frameCount) <= this->anim_frame()) {
                    this->anim_frame() = 0;
                }
                frameOffset = *reinterpret_cast<uint16_t*>(animationBytes + 0x14) *
                              static_cast<uint32_t>(this->anim_frame());
            }

            /* 2. Overlay cursor sprite (colour-keyed frame strip) */
            RECT srcRect;
            srcRect.left   = static_cast<LONG>(frameOffset);
            srcRect.top    = 0;
            srcRect.right  = static_cast<LONG>(frameOffset + spriteW);
            srcRect.bottom = static_cast<LONG>(spriteH);
            Cursor_SurfaceBlt(this->primary_surface())(
                this->primary_surface(),
                &dstRect,
                this->cursor_sprite_surface(),
                &srcRect,
                BLIT_KEYSRC_WAIT, nullptr);

            /* 3. Composite to scene backbuffer */
            RECT destRect;
            destRect.left   = unionRect.left;
            destRect.top    = unionRect.top;
            destRect.right  = unionRect.right;
            destRect.bottom = unionRect.bottom;
            Cursor_SurfaceBlt(_g_backbuffer)(
                _g_backbuffer,
                &destRect,
                this->backbuffer(),
                &offsetUnion,
                BLIT_WAIT, nullptr);
        } else {
            /* NORMAL PATH: separate restore + sprite composite.
             * Register-derived rects in the binary's normal path
             * (0x4156D3..0x41574F) are not fully recoverable; the
             * structure (restore, keyed overlay, composite) is preserved
             * with the clip-verified rectangles. */
            dstRect.right  = static_cast<LONG>(spriteW);
            dstRect.bottom = static_cast<LONG>(spriteH);

            RECT offsetUnion;
            CopyRect(&offsetUnion, &unionRect);
            OffsetRect(&offsetUnion, -vpRect.left, -vpRect.top);

            /* 1. Capture background into cursor backbuffer */
            Cursor_SurfaceBlt(this->backbuffer())(
                this->backbuffer(),
                &dstRect,
                this->primary_surface(),
                &offsetUnion,
                BLIT_WAIT, nullptr);

            /* 2. Overlay cursor sprite (colour-keyed) */
            Cursor_SurfaceBlt(this->primary_surface())(
                this->primary_surface(),
                &dstRect,
                this->cursor_sprite_surface(),
                nullptr,
                BLIT_KEYSRC_WAIT, nullptr);

            /* 3. Composite to scene backbuffer */
            RECT destRect;
            destRect.left   = unionRect.left;
            destRect.top    = unionRect.top;
            destRect.right  = unionRect.right;
            destRect.bottom = unionRect.bottom;
            Cursor_SurfaceBlt(_g_backbuffer)(
                _g_backbuffer,
                &destRect,
                this->backbuffer(),
                &offsetUnion,
                BLIT_WAIT, nullptr);
        }
    }
}


/* ================================================================== */
/* Cursor::update_network_names — Refresh network player names        */
/* Address: 0x416E00                                                    */
/*                                                                     */
/* Populates player_names[26][13] (+0x59E) with up to 26 names:       */
/*   1. If _g_netman->m_gameMode (+0x7C4) == 2 (joined): iterates      */
/*      m_slots[9] at +0x518 (stride 0x4C), skipping the local slot    */
/*      (index == m_mySlotIndex at +0x7D0) and empty dpIds (slot+0x00);*/
/*      copies the name at slot+0x51D (PlayerSlot::compact_name).     */
/*   2. Otherwise: uses formatted resource string #0x6E (13 chars max) */
/*      as a single default name.                                      */
/*   3. Stores the count at +0x6F4, calls DPLAY_EnumeratePlayers(),    */
/*      then appends names from _g_dplay (+0xB13, stride 0xD, up to   */
/*      16 entries).                                                   */
/*   4. Zero-fills remaining slots up to 26.                           */
/*                                                                     */
/* Offsets verified instruction-by-instruction (0x416E37..0x416F6D):  */
/*   +0x7C4 mode, +0x7D0 self slot index, +0x518 slot array (9 x 0x4C),*/
/*   slot dpId at +0x00, name at +0x51D, +0x6F4 count, +0xB13 names.  */
/* ================================================================== */
void Cursor::update_network_names()
{
    int nameIdx = 0;

    /* NetMan joined-state check: m_gameMode at +0x7C4 == 2 */
    if (_g_netman != nullptr &&
        *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(_g_netman) + 0x7C4) == 2) {
        uint8_t* netmanBase = reinterpret_cast<uint8_t*>(_g_netman);
        int32_t selfSlot = *reinterpret_cast<int32_t*>(netmanBase + 0x7D0);

        /* Iterate the 9 player slots at +0x518, stride 0x4C */
        for (int offset = 0; offset < 0x2AC && nameIdx < 26; offset += 0x4C) {
            if (nameIdx >= 26) break;
            if (offset / 0x4C == selfSlot)
                continue;                                   /* skip self */
            if (*reinterpret_cast<int32_t*>(netmanBase + 0x518 + offset) == 0)
                continue;                                   /* empty dpId */

            /* Name at slot+0x51D (PlayerSlot::compact_name) */
            const char* srcName = reinterpret_cast<const char*>(
                netmanBase + 0x51D + offset);
            if (srcName[0] != '\0') {
                for (int c = 0; c < 12; c++) {
                    this->player_names[nameIdx][c] = srcName[c];
                    if (srcName[c] == '\0') break;
                }
                this->player_names[nameIdx][12] = '\0';
                nameIdx++;
            }
        }
    } else {
        /* Offline: use formatted resource string #0x6E as default name */
        char defaultName[13] = { 0 };
        FormatResourceString(&g_resmgr, 0x6E, defaultName, 13);
        for (int c = 0; c < 12; c++) {
            this->player_names[nameIdx][c] = defaultName[c];
            if (defaultName[c] == '\0') break;
        }
        this->player_names[nameIdx][12] = '\0';
        nameIdx = 1;
    }

    /* Store count before DPLAY enumeration (the binary writes +0x6F4
     * before calling DPLAY_EnumeratePlayers, then increments it). */
    this->player_count = nameIdx;                      /* +0x6F4 */

    /* Enumerate DPLAY players */
    DPLAY_EnumeratePlayers();

    /* Read names from _g_dplay player table at +0xB13, stride 0xD.
     * Up to 16 entries (0xD0 bytes). */
    if (_g_dplay != nullptr) {
        uint8_t* dplayBase = static_cast<uint8_t*>(_g_dplay);
        for (int offset = 0; offset < 0xD0 && nameIdx < 26; offset += 0xD) {
            const char* srcName = reinterpret_cast<const char*>(
                dplayBase + 0xB13 + offset);
            if (srcName[0] != '\0') {
                for (int c = 0; c < 12; c++) {
                    this->player_names[nameIdx][c] = srcName[c];
                    if (srcName[c] == '\0') break;
                }
                this->player_names[nameIdx][12] = '\0';
                nameIdx++;
            }
        }
    }

    /* Zero-fill remaining slots (stride 0xD) */
    for (int i = nameIdx; i < 26; i++) {
        this->player_names[i][0] = '\0';
    }

    this->player_count = nameIdx;                      /* +0x6F4 */
}


/* ================================================================== */
/* Cursor::init_network_player — Initialize local player data         */
/* Address: 0x41A0E0 (Ghidra label: INPUT_InitNetworkPlayer)          */
/*                                                                     */
/* Deletes any existing record in obj_184 (+0x184), allocates a        */
/* 0x39C-byte player record, creates it via DPLAY_CreatePlayer,        */
/* copies the description string at 0x47E4EC into record+0x43, the     */
/* player name from g_player_config (+6) into record+0x25, syncs       */
/* colour_r/g/b (+0x298..0x2A0) from record bytes +0x40/+0x41/+0x42,   */
/* and resets toolbar_sentinel (+0x6F0) and selected_idx_384 (+0x384)  */
/* to -1 plus the ui_active flag (+0x188).                             */
/*                                                                     */
/* Validated against the 0x41A0E0 decompilation: allocation 0x39C,     */
/* DPLAY_CreatePlayer(record) return stored, both string copies,       */
/* colour sync, -1 sentinels, +0x189 zero, +0x188 force-1.             */
/* ================================================================== */
void Cursor::init_network_player()
{
    /* Delete any existing record first (the binary unconditionally
     * releases obj_184 via vtable[0](1) and re-creates it). */
    if (this->obj_184 != nullptr) {                      /* +0x184 */
        delete this->obj_184;
        this->obj_184 = nullptr;
    }

    /* Allocate the 0x39C-byte player record and create it via DPLAY */
    void* record = operator_new(0x39C);
    this->obj_184 = static_cast<CursorEditorRecord*>(
        record != nullptr
            ? reinterpret_cast<void*>(static_cast<intptr_t>(DPLAY_CreatePlayer(record)))
            : nullptr);

    if (this->obj_184 != nullptr) {
        uint8_t* playerBytes = reinterpret_cast<uint8_t*>(this->obj_184);

        /* Description string at 0x47E4EC → record+0x43.
         * The string is not in the Ghidra string table (exact bytes
         * unverified); the previous transcription guessed "LEGO LOCO
         * Player" but only the address is evidence-backed. */
        const char* desc = reinterpret_cast<const char*>(0x47E4EC);
        size_t descLen = strlen(desc);
        if (descLen > 0x30) descLen = 0x30;   /* record+0x43..0x73 bound */
        memcpy(playerBytes + 0x43, desc, descLen + 1);

        /* Player name from g_player_config (+6) → record+0x25 */
        const char* cfgName = reinterpret_cast<const char*>(
            static_cast<uint8_t*>(g_player_config) + 6);
        size_t nameLen = strlen(cfgName);
        if (nameLen > 0x24) nameLen = 0x24;   /* record+0x25..0x48 bound */
        memcpy(playerBytes + 0x25, cfgName, nameLen + 1);

        /* Sync colour fields from the record bytes */
        this->color_r = playerBytes[0x40];               /* +0x298 */
        this->color_g = playerBytes[0x41];               /* +0x29C */
        this->color_b = playerBytes[0x42];               /* +0x2A0 */
    }

    /* Reset toolbar selection state */
    this->toolbar_sentinel = -1;                         /* +0x6F0 */
    this->selected_idx_384 = -1;                         /* +0x384 */
    this->_pad_189[0] = 0;                               /* +0x189 */
    if (this->ui_active == 0) {                          /* +0x188 */
        this->ui_active = 1;
    }
}
