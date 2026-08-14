// Status: INTEGRATED
/* Cursor_Render.cpp — Main cursor compositing and rendering
 *
 * Full reconstruction from Ghidra disassembly of Cursor_Render@0x414C20.
 * 254 instructions, 4-blit compositing pipeline with animation advance.
 * Validated against the decompilation: the skip-render guard, hidden-
 * cursor restore path, dirty markers (+0x50/+0x54 = -1), hotspot/clip,
 * animation frame advance (frame_count at RESDATA+0x160), and the four
 * blits (background capture, colour-keyed overlay, screen composite,
 * background restore) all match. DirectDraw surface calls go through the
 * real platform/ddraw_interfaces.h IDirectDrawSurface4 interface (this
 * shim is API-compatible, not ABI-compatible, so raw vtable-slot dispatch
 * would hit the wrong compiler-generated method — converted 2026-08-14).
 */

#include "Cursor.h"
#include "Cursor_internal.h"
#include "../platform/ddraw_interfaces.h"

void Cursor::render(HWND hWnd, void* hdc, uint8_t skipRender)
{
    /* ================================================================ */
    /* Phase 0: Pre-render surface helper if hdc provided                */
    /* Address: 0x414C28..0x414C3D                                      */
    /* ================================================================ */
    if (hdc != nullptr) {
        static_cast<IDirectDrawSurface4*>(this->primary_surface())->ReleaseDC(hdc);
    }

    /* ================================================================ */
    /* Phase 1: Skip-render guard                                       */
    /* Address: 0x414C3D..0x414C43                                      */
    /* ================================================================ */
    if (skipRender != 0) {
        Cursor_UnlockAllSurfaces();
        return;
    }

    /* ================================================================ */
    /* Phase 2: Unlock primary surface                                  */
    /* Address: 0x414C49..0x414C53                                      */
    /* ================================================================ */
    DDRAW_UnlockPrimary();

    /* ================================================================ */
    /* Phase 3: Active cursor check                                     */
    /* Address: 0x414C53..0x414C66                                      */
    /*   +0x14 = cursor_sprite_surface (non-null = cursor active)       */
    /*   +0x58 = capture_flag  (non-zero = cursor captured/hidden)      */
    /* ================================================================ */
    if (this->cursor_state() == 0 || this->capture_flag() != 0) {
        /* ============================================================ */
        /* HIDDEN-CURSOR PATH                                           */
        /* Address: 0x414E68..0x414EE7                                  */
        /* Just composite primary surface to screen, no cursor drawing. */
        /* ============================================================ */
        POINT screenPt;
        screenPt.x = 0;
        screenPt.y = 0;
        ClientToScreen(this->hWnd, &screenPt);

        RECT* clientR = &this->cursor_client_rect;
        RECT screenDest;
        SetRect(&screenDest,
                clientR->left   + screenPt.x,
                clientR->top    + screenPt.y,
                clientR->right  + screenPt.x,
                clientR->bottom + screenPt.y);

        int result = static_cast<IDirectDrawSurface4*>(_g_backbuffer)->Blt(
            &screenDest,
            static_cast<IDirectDrawSurface4*>(this->primary_surface()),
            clientR,
            BLIT_WAIT,
            nullptr);

        if (result != 0) {
            OutputDebugStringA(&g_empty_string);
        }

        Cursor_UnlockAllSurfaces();
        return;
    }

    /* ================================================================ */
    /* Phase 4: Active cursor rendering                                 */
    /* Address: 0x414C6C..0x414E65                                      */
    /* ================================================================ */

    /* ---- 4a. Reset dirty rect to (-1, -1) "invalid" ---- */
    this->dirty_rect_left() = -1;
    this->dirty_rect_top()  = -1;

    /* ---- 4b. Get cursor position ---- */
    POINT cursorPos;
    GetCursorPos(&cursorPos);

    /* ---- 4c. Get animation RESDATA + adjust hotspot ---- */
    void* animData = this->anim_resdata();  /* +0x44 */
    auto* animationBytes = static_cast<uint8_t*>(animData);
    cursorPos.x -= *reinterpret_cast<int16_t*>(animationBytes + 0x32); /* hotspot_x */
    cursorPos.y -= *reinterpret_cast<int16_t*>(animationBytes + 0x34); /* hotspot_y */

    /* ---- 4d. Get sprite dimensions from RESDATA ---- */
    /* NOTE: the binary adjusts the hotspot (4c) BEFORE the null check,
     * implying anim data is always present when the cursor is active;
     * the guarded read here is host-safe. */
    uint32_t spriteW, spriteH;
    if (animData != nullptr) {
        spriteW = *reinterpret_cast<uint16_t*>(animationBytes + 0x14); /* width  */
        spriteH = *reinterpret_cast<uint16_t*>(animationBytes + 0x16); /* height */
    } else {
        spriteW = 0;
        spriteH = 0;
    }

    /* ---- 4e. Build cursor rect and clip to viewport ---- */
    /* +0x20 = clip_rect_right, +0x24 = clip_rect_bottom */
    RECT cursorRect;
    cursorRect.left   = cursorPos.x;
    cursorRect.top    = cursorPos.y;
    cursorRect.right  = cursorPos.x + static_cast<int>(spriteW);
    cursorRect.bottom = cursorPos.y + static_cast<int>(spriteH);

    /* Clip right edge: if cursorRect.right > clip_rect_right */
    if (this->clip_rect_right() < cursorRect.right) {
        spriteW           = this->clip_rect_right() - cursorPos.x;
        cursorRect.right  = this->clip_rect_right();
    }
    /* Clip bottom edge: if cursorRect.bottom > clip_rect_bottom */
    if (this->clip_rect_bottom() < cursorRect.bottom) {
        spriteH            = this->clip_rect_bottom() - cursorPos.y;
        cursorRect.bottom  = this->clip_rect_bottom();
    }

    /* ---- 4f. Store clipped cursor rect at +0x68 ---- */
    this->cursor_rect() = cursorRect;

    /* ---- 4g. Animation frame advance ---- */
    /* RESDATA+0x160 = frame_count (uint16_t)                     */
    /* RESDATA+0x14  = frame_width  (uint16_t, per-frame stride)  */
    /* Cursor+0x48  = anim_frame   (wraps at frame_count)         */

    RECT  srcRect;      /* source sub-rect within sprite sheet */
    uint16_t frameCount = *reinterpret_cast<uint16_t*>(animationBytes + 0x160);

    if (frameCount <= 1) {
        /* Single-frame sprite: use entire surface */
        srcRect.left   = 0;
        srcRect.right  = spriteW;
    } else {
        /* Multi-frame animation: advance + wrap */
        if (this->anim_frame() >= static_cast<int32_t>(frameCount)) {
            this->anim_frame() = 0;
        }
        uint32_t frameWidth  = *reinterpret_cast<uint16_t*>(animationBytes + 0x14);
        uint32_t frameOffset = frameWidth * this->anim_frame();
        srcRect.left   = frameOffset;
        srcRect.right  = frameOffset + spriteW;
    }
    srcRect.top    = 0;
    srcRect.bottom = spriteH;

    /* ---- 4h. Build dest rect for sprite blit ---- */
    /* tStack_58 in the original: {0, 0, spriteW, spriteH} */
    RECT spriteDestRect;
    spriteDestRect.left   = 0;
    spriteDestRect.top    = 0;
    spriteDestRect.right  = spriteW;
    spriteDestRect.bottom = spriteH;

    /* ---- 4i. Compute offset rect (cursorRect in surface coords) ---- */
    /* offsetRect = CopyRect(cursorRect) then OffsetRect(-vp_x, -vp_y) */
    /* +0x18 = clip_rect_left (viewport x offset)                     */
    /* +x01C = clip_rect_top  (viewport y offset)                     */
    RECT offsetRect;
    CopyRect(&offsetRect, &cursorRect);
    OffsetRect(&offsetRect, -this->clip_rect_left(), -this->clip_rect_top());

    /* ---- 4j. BLIT 1: Background capture ---- */
    /* backbuffer(+0x5C) ← primary_surface(+0x38)                    */
    /* Dest rect: cursorRect (screen coords on backbuffer)           */
    /* Src rect:  offsetRect (surface coords on primary_surface)     */
    /* Flags: DDBLT_WAIT (0x1000000)                                 */
    {
        static_cast<IDirectDrawSurface4*>(this->backbuffer())->Blt(
            &cursorRect,
            static_cast<IDirectDrawSurface4*>(this->primary_surface()),
            &offsetRect,
            BLIT_WAIT,
            nullptr);
    }

    /* ---- 4k. BLIT 2: Color-keyed sprite overlay ---- */
    /* primary_surface(+0x38) ← cursor_sprite(+0x14)                 */
    /* Dest rect: offsetRect (cursor position in surface coords)     */
    /* Src rect:  srcRect (animation frame sub-rect of sprite sheet) */
    /* Flags: DDBLT_WAIT | DDBLT_KEYSRC (0x1008000)                  */
    {
        static_cast<IDirectDrawSurface4*>(this->primary_surface())->Blt(
            &offsetRect,
            static_cast<IDirectDrawSurface4*>(this->cursor_sprite_surface()),   /* +0x14 = cursor sprite surface (aliased via union) */
            &srcRect,
            BLIT_KEYSRC_WAIT,
            nullptr);
    }

    /* ---- 4l. ClientToScreen + SetRect for final composite ---- */
    /* Point starts at (0,0); ClientToScreen converts to screen coords.
    /* Then SetRect adds client_rect(+0x104) offset to get the       */
    /* destination rectangle on the global backbuffer.                */
    POINT screenPt;
    screenPt.x = 0;
    screenPt.y = 0;
    ClientToScreen(this->hWnd, &screenPt);

    /* client_rect at +0x104: {left, top, right, bottom}             */
    RECT* clientRect = &this->cursor_client_rect;

    RECT screenDestRect;
    SetRect(&screenDestRect,
            clientRect->left   + screenPt.x,
            clientRect->top    + screenPt.y,
            clientRect->right  + screenPt.x,
            clientRect->bottom + screenPt.y);

    /* ---- 4m. BLIT 3: Composite to screen ---- */
    /* g_backbuffer ← primary_surface(+0x38)                         */
    /* Dest rect: screenDestRect  (screen-space position)            */
    /* Src rect:  client_rect(+0x104) (surface-space region)         */
    /* Flags: DDBLT_WAIT (0x1000000)                                 */
    {
        int result = static_cast<IDirectDrawSurface4*>(_g_backbuffer)->Blt(
            &screenDestRect,
            static_cast<IDirectDrawSurface4*>(this->primary_surface()),
            clientRect,
            BLIT_WAIT,
            nullptr);

        /* Debug output on failure */
        if (result != 0) {
            OutputDebugStringA(&g_empty_string);
        }
    }

    /* ---- 4n. BLIT 4: Restore background ---- */
    /* primary_surface(+0x38) ← backbuffer(+0x5C)                    */
    /* Dest rect: spriteDestRect  ({0,0,spriteW,spriteH})            */
    /* Src rect:  offsetRect      (cursor position in surface coords)*/
    /* Flags: DDBLT_WAIT (0x1000000)                                 */
    /*                                                               */
    /* Copies the saved background (captured in Blit 1) back onto    */
    /* the primary surface so the next frame starts clean.           */
    {
        static_cast<IDirectDrawSurface4*>(this->primary_surface())->Blt(
            &spriteDestRect,
            static_cast<IDirectDrawSurface4*>(this->backbuffer()),
            &offsetRect,
            BLIT_WAIT,
            nullptr);
    }

    /* ---- 4o. Unlock all surfaces and return ---- */
    Cursor_UnlockAllSurfaces();
}
