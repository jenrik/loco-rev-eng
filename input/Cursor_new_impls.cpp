// Status: INTEGRATED
#include "Cursor.h"
#include "Cursor_internal.h"
#include "../platform/ddraw_interfaces.h"
#include "../network/DPlayManager.h"
#include "../ui/ButtonSprite.h"
#include "../graphics/LOCOBITMAP.h"

#include <cstdio>
#include <cstdint>

/* ================================================================== */
/* Cursor::init_editor_sprites — Init all editor/toolbar sprite objs   */
/* Address: 0x417F20                                                   */
/*                                                                     */
/* Loads editor sprite sheet (res 0x3CB9), retrieves surface via       */
/* RESDATA vtable[1], calls Sprite_Init on all ~49 UISprite objects.  */
/* Guarded by editor_initialized (+0x2C0) flag. Validated: exact       */
/* sprite order and resource/surface writes match the decompilation.  */
/* ================================================================== */
void Cursor::init_editor_sprites()
{
    if (this->editor_initialized) {
        return;
    }

    /* Load editor sprite sheet resource 0x3CB9 */
    RESDATA* resdata = static_cast<RESDATA*>(
        ResourceManager_GetById(&g_resmgr, 0x3CB9));
    this->editor_resdata = resdata;                          /* +0x1F0 */

    if (resdata != nullptr) {
        /* Get surface via RESDATA vtable[1] (GetSurface with flags=0, mode=0) */
        void* surface = RESDATA_GetSurface(resdata, 0, 0);
        this->editor_surface = surface;                     /* +0x1EC */
    }

    /* Initialize all UISprite objects */
    #define SPRITE_INIT(field) do { \
        if (this->field != nullptr) { \
            Sprite_Init(this->field); \
        } \
    } while(0)

    SPRITE_INIT(sprite_148);
    SPRITE_INIT(sprite_14C);
    SPRITE_INIT(sprite_2C4);
    SPRITE_INIT(sprite_2C8);
    SPRITE_INIT(sprite_2E0);
    SPRITE_INIT(sprite_2E4);
    SPRITE_INIT(sprite_2E8);
    SPRITE_INIT(sprite_2EC);
    SPRITE_INIT(sprite_1C0);
    SPRITE_INIT(sprite_1C4);
    SPRITE_INIT(sprite_2CC);
    SPRITE_INIT(sprite_2F0);
    SPRITE_INIT(sprite_2F4);
    SPRITE_INIT(sprite_308);
    SPRITE_INIT(sprite_30C);
    SPRITE_INIT(sprite_310);
    SPRITE_INIT(sprite_314);
    SPRITE_INIT(sprite_318);
    SPRITE_INIT(sprite_31C);
    SPRITE_INIT(sprite_2A4);
    SPRITE_INIT(sprite_2A8);
    SPRITE_INIT(sprite_2AC);

    /* Init 16 bonus sprites at +0x330 */
    for (int i = 0; i < 16; i++) {
        if (this->bonus_sprites[i] != nullptr) {
            Sprite_Init(this->bonus_sprites[i]);
        }
    }

    /* Init 10 editor sprites at +0x1F4 */
    for (int i = 0; i < 10; i++) {
        if (this->editor_sprites[i] != nullptr) {
            Sprite_Init(this->editor_sprites[i]);
        }
    }

    /* Init sprite_37C */
    if (this->sprite_37C != nullptr) {
        Sprite_Init(this->sprite_37C);
    }

    #undef SPRITE_INIT

    this->editor_initialized = 1;
}

/* ================================================================== */
/* Cursor::cleanup_editor_sprites — Destroy all editor sprites        */
/* Address: 0x4180A0                                                   */
/*                                                                     */
/* Reverses init_editor_sprites(). Calls Sprite_Destroy on all ~49    */
/* UISprite objects, releases the editor resdata via vtable[2], and   */
/* clears editor_initialized (+0x2C0). Guarded. Validated.            */
/* ================================================================== */
void Cursor::cleanup_editor_sprites()
{
    if (this->editor_initialized == 0) {
        return;
    }

    #define SPRITE_DESTROY(field) do { \
        if (this->field != nullptr) { \
            Sprite_Destroy(this->field); \
        } \
    } while(0)

    SPRITE_DESTROY(sprite_148);
    SPRITE_DESTROY(sprite_14C);
    SPRITE_DESTROY(sprite_2C4);
    SPRITE_DESTROY(sprite_2C8);
    SPRITE_DESTROY(sprite_2E0);
    SPRITE_DESTROY(sprite_2E4);
    SPRITE_DESTROY(sprite_2E8);
    SPRITE_DESTROY(sprite_2EC);
    SPRITE_DESTROY(sprite_1C0);
    SPRITE_DESTROY(sprite_1C4);
    SPRITE_DESTROY(sprite_2CC);
    SPRITE_DESTROY(sprite_2F0);
    SPRITE_DESTROY(sprite_2F4);
    SPRITE_DESTROY(sprite_308);
    SPRITE_DESTROY(sprite_30C);
    SPRITE_DESTROY(sprite_310);
    SPRITE_DESTROY(sprite_314);
    SPRITE_DESTROY(sprite_318);
    SPRITE_DESTROY(sprite_31C);
    SPRITE_DESTROY(sprite_2A4);
    SPRITE_DESTROY(sprite_2A8);
    SPRITE_DESTROY(sprite_2AC);

    /* Destroy 16 bonus sprites at +0x330 */
    for (int i = 0; i < 16; i++) {
        if (this->bonus_sprites[i] != nullptr) {
            Sprite_Destroy(this->bonus_sprites[i]);
        }
    }

    /* Destroy 10 editor sprites at +0x1F4 */
    for (int i = 0; i < 10; i++) {
        if (this->editor_sprites[i] != nullptr) {
            Sprite_Destroy(this->editor_sprites[i]);
        }
    }

    SPRITE_DESTROY(sprite_37C);

    #undef SPRITE_DESTROY

    /* Release the editor resource via RESDATA vtable[2] */
    if (this->editor_resdata != nullptr) {
        RESDATA_ReleaseSurface(this->editor_resdata);
    }

    this->editor_resdata = nullptr;
    this->editor_surface = nullptr;
    this->editor_initialized = 0;
}

/* ================================================================== */
/* Cursor::render_editor — Full editor toolbar render (vtable[8])     */
/* Address: 0x418210                                                   */
/*                                                                     */
/* Renders the complete editor toolbar: blits background surface to   */
/* primary, draws edit preview, color bars, network status, and       */
/* colour palette. Two paths based on palette_start_idx (+0x2BC).     */
/* Validated: blit geometry (workRect +0xD4..+0xE0 as src, editor_blit */
/* +0x1D8..+0x1E4 as dst), sprite resets, +0x594/+0x59C/+0x58C clears,*/
/* INPUT_SwitchToLocomotiveTab(+0x2B2), delayed-focus (+0xF0) with    */
/* HelpWnd_PlayNarration and +0xEC = 10 on success.                   */
/* ================================================================== */
void Cursor::on_update(int32_t /* param */)
{
    /* Blit background surface to primary display:
     * src = workRect (+0xD4..+0xE0), dst = editor_blit (+0x1D8..+0x1E4) */
    UIPANEL_Blit(
        this->background_surface,               /* +0x1E8 */
        this->workRect.left, this->workRect.top, this->workRect.right, this->workRect.bottom,
        _g_primary_surface,
        this->editor_blit_x, this->editor_blit_y,
        this->editor_blit_w, this->editor_blit_h,
        1);

    /* Blit edit preview */
    this->blit_edit_preview();

    /* Update scroll buttons */
    this->update_scroll_buttons();

    /* Reset all 10 editor sprites to state 0 */
    for (int i = 0; i < 10; i++) {
        if (this->editor_sprites[i] != nullptr) {
            Sprite_SetState(this->editor_sprites[i], 0, nullptr);
        }
    }

    /* Draw color bars */
    this->draw_color_bars(1);

    /* Draw network status */
    this->draw_network_status();

    /* Branch based on palette_start_idx */
    if (this->palette_start_idx < 0) {
        /* Tab-switch mode: end paint, reset surface flags, dispatch */
        UIPANEL_EndPaintEx(this, static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)), 0, 0, nullptr);  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)
        this->surf_a_dirty = 0;                          /* +0x594 */
        this->surf_b_dirty = 0;                          /* +0x59C */
        this->surface_toggle = 0;                        /* +0x58C */
        INPUT_SwitchToLocomotiveTab(this, this->editor_flags[2]);
    } else {
        /* Normal editor mode: draw color palette, reset sprite, end paint */
        this->draw_color_palette(nullptr, 0);
        Sprite_SetState(this->sprite_1C0, 0, nullptr);
        UIPANEL_EndPaintEx(this, static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)), 0, 0, nullptr);  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)
    }

    /* Handle delayed focus + audio narration */
    if (this->delayed_focus_flag != 0) {
        SetFocus(this->hWnd);
        this->delayed_focus_flag = 0;
        int result = HelpWnd_PlayNarration(g_audio_mgr, 3, 0);
        if (result != 0) {
            this->editor_state = 10;
        }
    }
}

/* ================================================================== */
/* Cursor::handle_color_swatch_click — Handle click on color swatch   */
/* Address: 0x418340                                                   */
/*                                                                     */
/* Hit-tests the 10 editor_sprites for a click at (x, y). On match:   */
/* reads RGB from edit_colors[hit], propagates to color_r/g/b,        */
/* updates obj_184 (+0x184) if set. Validated against decompilation.  */
/* ================================================================== */
void Cursor::handle_color_swatch_click(LONG x, LONG y)
{
    for (int i = 0; i < 10; i++) {
        POINT pt;
        pt.x = x;
        pt.y = y;

        /* Hit-test sprite rect at sprite+0x04 */
        if (this->editor_sprites[i] != nullptr) {
            RECT* spriteRect = reinterpret_cast<RECT*>(
                reinterpret_cast<uint8_t*>(this->editor_sprites[i]) + 4);
            if (PtInRect(spriteRect, pt)) {
                /* Highlight the swatch */
                Sprite_SetState(this->editor_sprites[i], 1, nullptr);
                UIPANEL_EndPaintEx(this, static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)), 0, 0, nullptr);  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)
                Sleep(0x96);  /* 150ms pause */

                /* Copy RGB from palette table to active color */
                this->color_r = static_cast<uint32_t>(this->edit_colors[i * 3]);
                this->color_g = static_cast<uint32_t>(this->edit_colors[i * 3 + 1]);
                this->color_b = static_cast<uint32_t>(this->edit_colors[i * 3 + 2]);

                /* Redraw color bars */
                this->draw_color_bars(1);

                /* Propagate to player record if set */
                if (this->obj_184 != nullptr) {
                    this->obj_184->color_r = static_cast<uint8_t>(this->color_r);
                    this->obj_184->color_g = static_cast<uint8_t>(this->color_g);
                    this->obj_184->color_b = static_cast<uint8_t>(this->color_b);
                    this->blit_edit_preview();
                }

                /* Reset all swatches back to state 0 */
                for (int j = 0; j < 10; j++) {
                    if (this->editor_sprites[j] != nullptr) {
                        Sprite_SetState(this->editor_sprites[j], 0, nullptr);
                    }
                }

                UIPANEL_EndPaintEx(this, static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)), 0, 0, nullptr);  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)
                return;
            }
        }
    }
}

/* ================================================================== */
/* Cursor::adjust_color_component — Adjust one R/G/B channel by +/-6  */
/* Address: 0x418450                                                   */
/*                                                                     */
/* Adjusts a single color channel (R=0, G=1, B=2) by +/-6. Sets a    */
/* 100ms auto-repeat timer. Plays sound (res 0x5279) on first adjust. */
/* Clamps to [0,255]. Redraws bars and blits preview. Validated.      */
/* ================================================================== */
void Cursor::adjust_color_component(int32_t component, uint8_t direction, int32_t posX, int32_t posY)
{
    /* Set editor state to 5 (colour-adjust mode) + 100ms auto-repeat delay */
    this->editor_state = 5;

    /* Kill existing animation timer */
    if (this->timer_id_18C != 0) {
        KillTimer(this->hWnd, this->timer_id_18C);
        this->timer_id_18C = 0;
    }

    /* Store component index and direction */
    this->color_adjust_component = component;                /* +0x250 */
    this->color_adjust_direction = direction;                /* +0x254 */

    /* Start colour-bar auto-repeat timer if not already running */
    if (this->counter_24C == 0) {
        UINT_PTR timerId = SetTimer(this->hWnd, 0x4D, 100, nullptr);
        this->counter_24C = timerId;
    }

    /* Handle each component */
    if (component == 0) {
        /* Red channel */
        Sprite_SetState(this->sprite_2A4, 1, nullptr);
        UIPANEL_EndPaintEx(this, static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)), 0, 0, nullptr);  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)

        if (this->color_r < 0xFF && direction != 0) {
            if (this->timer_id_198 < 1) {
                this->timer_id_198 = 10;
                PlaySoundAt(0x5279, posX, posY, 4);
            }
            this->color_r += 6;
        } else if (this->color_r > 0 && direction == 0) {
            if (this->timer_id_198 < 1) {
                this->timer_id_198 = 10;
                PlaySoundAt(0x5279, posX, posY, 4);
            }
            this->color_r -= 6;
        }
    } else if (component == 1) {
        /* Green channel */
        Sprite_SetState(this->sprite_2A8, 1, nullptr);
        UIPANEL_EndPaintEx(this, static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)), 0, 0, nullptr);  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)

        if (this->color_g < 0xFF && direction != 0) {
            if (this->timer_id_198 < 1) {
                this->timer_id_198 = 10;
                PlaySoundAt(0x5279, posX, posY, 4);
            }
            this->color_g += 6;
        } else if (this->color_g > 0 && direction == 0) {
            if (this->timer_id_198 < 1) {
                this->timer_id_198 = 10;
                PlaySoundAt(0x5279, posX, posY, 4);
            }
            this->color_g -= 6;
        }
    } else if (component == 2) {
        /* Blue channel */
        Sprite_SetState(this->sprite_2AC, 1, nullptr);
        UIPANEL_EndPaintEx(this, static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)), 0, 0, nullptr);  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)

        if (this->color_b < 0xFF && direction != 0) {
            if (this->timer_id_198 < 1) {
                this->timer_id_198 = 10;
                PlaySoundAt(0x5279, posX, posY, 4);
            }
            this->color_b += 6;
        } else if (this->color_b > 0 && direction == 0) {
            if (this->timer_id_198 < 1) {
                this->timer_id_198 = 10;
                PlaySoundAt(0x5279, posX, posY, 4);
            }
            this->color_b -= 6;
        }
    }

    /* Clamp all channels to [0, 255] */
    if (this->color_b < 0)   this->color_b = 0;
    if (this->color_r < 0)   this->color_r = 0;
    if (this->color_g < 0)   this->color_g = 0;
    if (this->color_b > 0xFF) this->color_b = 0xFF;
    if (this->color_r > 0xFF) this->color_r = 0xFF;
    if (this->color_g > 0xFF) this->color_g = 0xFF;

    /* Redraw color bars */
    this->draw_color_bars(0);

    /* Propagate to player record if set */
    if (this->obj_184 != nullptr) {
        this->obj_184->color_r = static_cast<uint8_t>(this->color_r);
        this->obj_184->color_g = static_cast<uint8_t>(this->color_g);
        this->obj_184->color_b = static_cast<uint8_t>(this->color_b);
    }

    /* Blit edit preview */
    this->blit_edit_preview();
    UIPANEL_EndPaintEx(this, static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)), 0, 0, nullptr);  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)
}

/* ================================================================== */
/* Cursor::draw_color_bars — Draw three R/G/B vertical color bars     */
/* Address: 0x418780                                                   */
/*                                                                     */
/* Each bar's fill height is proportional to channel value (0-255),   */
/* bottom-aligned within bar RECT (+0x258/+0x268/+0x278).            */
/* If reset_buttons: resets +/- button sprites to state 0.            */
/*                                                                     */
/* Validated: GetStockObject(4) for the background (not NULL_BRUSH),   */
/* fill height = bottom - (scale*value/100) with a 1px minimum        */
/* applied to the scaled height BEFORE the subtraction, DeleteObject   */
/* on all four brushes (including the stock one — matches the binary,  */
/* even though stock brushes are normally owned by the OS), and the    */
/* EndPaintEx with the HDC value.                                      */
/* ================================================================== */
void Cursor::draw_color_bars(uint8_t reset_buttons)
{
    /* The binary uses GetStockObject(4) = BLACK_BRUSH for the bar
     * backgrounds and deletes all four brush handles afterwards
     * (including the stock one — a binary quirk preserved here). */
    HBRUSH stockBrush = GetStockObject(4);
    HBRUSH brushRed   = CreateSolidBrush(0xFF);      /* blue channel fill (BGR) */
    HBRUSH brushGreen = CreateSolidBrush(0xFFFF);    /* green channel fill */
    HBRUSH brushBlue  = CreateSolidBrush(0xFF0000);  /* red channel fill */

    /* Scale factor: ((bottom - top) * 100) / 0xFF from the FIRST bar rect */
    int barFullHeight = this->color_bar_rects[0].bottom - this->color_bar_rects[0].top;
    int heightScale = (barFullHeight * 100) / 0xFF;

    /* Begin paint */
    HDC hdc = UIPANEL_BeginPaint(this);

    /* --- Red bar (color_bar_rects[0] at +0x258) --- */
    FillRect(hdc, &this->color_bar_rects[0], stockBrush);
    if (this->color_r != 0) {
        int scaled = (heightScale * this->color_r) / 100;
        if (scaled == 0) scaled = 1;
        RECT fillRect;
        fillRect.left   = this->color_bar_rects[0].left;
        fillRect.right  = this->color_bar_rects[0].right;
        fillRect.bottom = this->color_bar_rects[0].bottom;
        fillRect.top    = fillRect.bottom - scaled;
        FillRect(hdc, &fillRect, brushRed);
    }

    /* --- Green bar (color_bar_rects[1] at +0x268) --- */
    FillRect(hdc, &this->color_bar_rects[1], stockBrush);
    if (this->color_g != 0) {
        int scaled = (heightScale * this->color_g) / 100;
        if (scaled == 0) scaled = 1;
        RECT fillRect;
        fillRect.left   = this->color_bar_rects[1].left;
        fillRect.right  = this->color_bar_rects[1].right;
        fillRect.bottom = this->color_bar_rects[1].bottom;
        fillRect.top    = fillRect.bottom - scaled;
        FillRect(hdc, &fillRect, brushGreen);
    }

    /* --- Blue bar (color_bar_rects[2] at +0x278) --- */
    FillRect(hdc, &this->color_bar_rects[2], stockBrush);
    if (this->color_b != 0) {
        int scaled = (heightScale * this->color_b) / 100;
        if (scaled == 0) scaled = 1;
        RECT fillRect;
        fillRect.left   = this->color_bar_rects[2].left;
        fillRect.right  = this->color_bar_rects[2].right;
        fillRect.bottom = this->color_bar_rects[2].bottom;
        fillRect.top    = fillRect.bottom - scaled;
        FillRect(hdc, &fillRect, brushBlue);
    }

    /* Cleanup: the binary deletes all four handles. */
    DeleteObject(stockBrush);
    DeleteObject(brushRed);
    DeleteObject(brushGreen);
    DeleteObject(brushBlue);

    /* End paint with HDC */
    UIPANEL_EndPaintEx(this,
                       static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)),  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)
                       static_cast<int>(reinterpret_cast<intptr_t>(hdc)),
                       1, nullptr);

    /* Reset +/- button sprites if requested */
    if (reset_buttons != 0) {
        Sprite_SetState(this->sprite_2A4, 0, nullptr);
        Sprite_SetState(this->sprite_2A8, 0, nullptr);
        Sprite_SetState(this->sprite_2AC, 0, nullptr);
    }
}

/* ================================================================== */
/* Cursor::blit_edit_preview — Blit edit preview to primary surface   */
/* Address: 0x4189A0                                                   */
/*                                                                     */
/* Blits the edit preview area (background_surface portion) to the    */
/* primary display. If obj_184 (+0x184) has a player record, also     */
/* renders the player cursor overlay via DPLAY_RenderPlayer.          */
/*                                                                     */
/* Validated instruction-by-instruction (0x4189A0..0x418A82):         */
/*   src = (left, top-0xB, right, bottom);                            */
/*   dstLeft = editor_blit_x + left;                                  */
/*   dstTop  = editor_blit_y + top - 0xB;                             */
/*   dstRight  = right + dstLeft - left;                              */
/*   dstBottom = bottom + dstTop - (top - 0xB);                       */
/*   flags = 1. The previous transcription missed the -0xB on the     */
/*   source y and the -left/-(top-0xB) subtractions on the dest       */
/*   right/bottom.                                                     */
/* ================================================================== */
void Cursor::blit_edit_preview()
{
    uint srcX = static_cast<uint>(this->edit_preview_rect.left);    /* +0x1A0 */
    uint srcY = static_cast<uint>(this->edit_preview_rect.top) - 0xB; /* +0x1A4 */
    uint srcW = static_cast<uint>(this->edit_preview_rect.right);   /* +0x1A8 */
    uint srcH = static_cast<uint>(this->edit_preview_rect.bottom);  /* +0x1AC */

    uint dstLeft   = static_cast<uint>(this->editor_blit_x) + srcX;  /* +0x1D8 */
    uint dstTop    = static_cast<uint>(this->editor_blit_y) + srcY;  /* +0x1DC */
    uint dstRight  = srcW + dstLeft - srcX;
    uint dstBottom = srcH + dstTop - srcY;

    /* Blit from background surface to primary surface */
    UIPANEL_Blit(
        this->background_surface,           /* +0x1E8 */
        srcX, srcY, static_cast<int>(srcW), srcH,
        _g_primary_surface,
        dstLeft, dstTop, static_cast<int>(dstRight), dstBottom,
        1);

    /* Render player cursor overlay if player record exists */
    if (this->obj_184 != nullptr) {
        /* HDC-like handle: (obj_184 >> 8) with the low byte replaced by
         * the ui_active byte (+0x188). */
        int hdcVal =
            (static_cast<int>(reinterpret_cast<intptr_t>(this->obj_184)) >> 8) &
            0xFFFFFF;
        hdcVal = (hdcVal << 8) | this->ui_active;

        /* The binary call site at 0x418A9C pushes nine stack args to
         * NetworkPlayerList::RenderPlayer (0x4437C0, RET 0x24): (hdc,
         * player, surface, left, top, right, bottom, hWnd, this+0x138).
         * The shared host stub ABI and the other reconstructed call
         * sites use the 8-argument form below (see Cursor_internal.h). */
        DPLAY_RenderPlayer(
            _g_dplay,
            reinterpret_cast<void*>(static_cast<intptr_t>(hdcVal)),
            static_cast<int32_t>(reinterpret_cast<intptr_t>(this->obj_184)),
            _g_primary_surface,
            this->edit_preview_rect.left,
            this->edit_preview_rect.top,
            static_cast<uint32_t>(this->edit_preview_rect.right),
            &this->edit_preview_rect);
    }
}

/* ================================================================== */
/* Cursor::draw_color_palette — Draw colour palette swatch strip      */
/* Address: 0x418A90                                                   */
/*                                                                     */
/* Draws the scrollable palette strip. mode=0: normal draw to primary, */
/* mode!=0: draw to alternate surface. Iterates toolbar_sprites from  */
/* palette_start_idx to palette_end_idx with tiered vertical layout.  */
/*                                                                     */
/* Validated: mode-0 background blit (sprite_37C->y + 1 as height),    */
/* mode!=0 magenta fill keyed on g_surface_bpp (0x7C1F / 0xF81F /     */
/* 0xFF00FF — the previous 0xFE08E0 else-branch was wrong), preview    */
/* blit with src_h = palette_rect.height, per-item tier logic and the  */
/* src/dst roles of the item blit (src = palette rect, dst = 0,clipH,  */
/* w,h), position caching in palette_item_rects, scroll button states. */
/* ================================================================== */
void Cursor::draw_color_palette(void* target_surf, uint8_t mode)
{
    if (target_surf == nullptr) {
        mode = 0;
        target_surf = _g_primary_surface;
    }

    /* Determine palette background sprite height */
    auto* paletteData = static_cast<RESDATA*>(this->sprite_37C->pixelData);
    uint palSpriteH = static_cast<uint>(paletteData->frame_height);

    if (mode == 0) {
        /* Normal mode: blit palette background from source.
         * Height = sprite_37C->y (+0x08) + 1. */
        uint bgH = static_cast<uint>(this->sprite_37C->y + 1);
        UIPANEL_Blit(
            this->background_surface,
            static_cast<uint>(this->palette_rect.left),   /* +0x1B0 */
            static_cast<uint>(this->palette_rect.top),    /* +0x1B4 */
            this->palette_rect.right,                     /* +0x1B8 */
            bgH,
            _g_primary_surface,
            this->editor_blit_x + this->palette_rect.left,
            this->editor_blit_y + this->palette_rect.top,
            this->editor_blit_x + this->palette_rect.right,
            this->editor_blit_y + bgH,
            1);

        /* Reset palette background sprite state */
        Sprite_SetState(this->sprite_37C, 0, nullptr);
    } else {
        /* Preview mode: clear surface with transparent fill */
        /* g_surface_bpp determines the colour-key value: 0x7C1F (15-bit),
         * 0xF81F (16-bit 555), else 0xFF00FF (24-bit magenta). */
        int colorKey;
        if (g_surface_bpp == 0x22B) {
            colorKey = 0x7C1F;
        } else if (g_surface_bpp == 0x235) {
            colorKey = 0xF81F;
        } else {
            colorKey = 0xFF00FF;
        }

        /* Clear via a color-fill Blt (null dest/src rects and src surface,
         * DDBLT_WAIT|DDBLT_COLORFILL flags) on target surface.
         *
         * The previous raw dispatch passed a fabricated 2-int buffer
         * ({100, colorKey}) reinterpreted as a DDBLTFX*. This shim's
         * Sdl3DirectDrawSurface::Blt only ever reads fx->dwFillColor from
         * a real DDBLTFX (graphics/sdl3_ddraw.cpp), so that's the one
         * field that matters here — set directly by name rather than by
         * guessing the old buffer's byte layout. (`100` is very likely
         * the real x86 DDBLTFX's dwSize == sizeof(DDBLTFX) == 0x64 in the
         * original ABI, which this shim's own DDBLTFX — declaration-order
         * only, not ABI-accurate — doesn't reproduce; not investigated
         * further since dwSize isn't read anywhere in this shim.) */
        DDBLTFX fx{};
        fx.dwFillColor = static_cast<uint32_t>(colorKey);
        static_cast<IDirectDrawSurface4*>(target_surf)->Blt(
            nullptr, nullptr, nullptr, 0x1000400, &fx);
    }

    if (mode != 0) {
        /* Preview mode: blit palette sprite to alternate surface.
         * src = (0, height - spriteH, width, height); dst = (0, 0, w, h). */
        int palW = this->palette_rect.right - this->palette_rect.left;
        int palH = this->palette_rect.bottom - this->palette_rect.top;

        UIPANEL_Blit(
            this->sprite_37C->surface,                              /* sprite surface */
            0,
            static_cast<uint>(palH) - palSpriteH,
            palW,
            static_cast<uint>(palH),
            target_surf,
            0, 0, palW, static_cast<int>(palSpriteH),
            0);
    }

    /* Iterate toolbar sprite range from palette_start_idx to palette_end_idx */
    int currentIdx = this->palette_start_idx;              /* +0x2BC = start */
    int endIdx     = this->palette_end_idx;                /* +0x2B8 = end */

    if (currentIdx >= 0 && endIdx >= 0) {
        int availWidth = this->palette_rect.right;
        if (mode != 0) {
            availWidth -= this->palette_rect.left;
        }
        int xPos = availWidth - 4;

        while (currentIdx <= endIdx) {
            UIPANEL_Surface* sprite = this->toolbar_sprites[currentIdx];
            int spriteW = sprite->width;
            int spriteH = sprite->height;

            int availHeight = this->palette_rect.bottom;
            if (mode != 0) {
                availHeight -= this->palette_rect.top;
            }

            int yPos = availHeight;
            int clipH = 0;

            /* Tiered vertical positioning based on sprite height */
            if (static_cast<uint>(spriteH >> 2) * 3 < 0x38) {
                yPos = (availHeight - 0x1C) + (spriteH >> 2);
            } else if ((spriteH / 3) * 2 < 0x38) {
                yPos = (availHeight - 0x1C) + (spriteH / 3);
            } else if ((spriteH & 0xFFFFFFFE) < 0x70) {
                yPos = (availHeight - 0x1C) + (spriteH >> 1);
            } else if (static_cast<uint>(spriteH) < 0x54) {
                yPos = availHeight - spriteH;
            } else {
                yPos = availHeight - 0x54;
                clipH = spriteH - 0x54;
            }

            int destX = xPos;
            int destY = yPos;
            int destW = spriteW;
            int destH = spriteH;

            xPos = destX - spriteW;

            /* Item blit: src = (destX, destY, destX+width, destY+height)
             * i.e. the item position rect; dst = (0, clipH, width, height).
             * (The prior transcription had src/dst swapped.) */
            UIPANEL_Blit(
                sprite,
                destX, destY, destX + destW, destY + destH,
                target_surf,
                0, clipH, spriteW, static_cast<uint>(spriteH),
                0);

            /* Cache position in palette_item_rects at +0x38C */
            int slotIdx = currentIdx - this->palette_start_idx;
            if (slotIdx >= 0 && slotIdx < 16) {
                RECT& cachedRect = this->palette_item_rects[slotIdx];
                cachedRect.left = destX;
                cachedRect.top = destY;
                cachedRect.right = destX + destW;
                cachedRect.bottom = destY + destH;
            }

            /* Advance: subtract 10 for gap between items */
            xPos -= 10;
            currentIdx++;
        }
    }

    /* Update scroll button sprite states (only in normal mode) */
    if (mode == 0) {
        /* Scroll up button */
        int upState;
        if (this->has_next_page == 0 || this->ui_active == 0) {
            upState = 2;    /* disabled */
        } else {
            upState = 0;    /* normal */
        }
        Sprite_SetState(this->sprite_2F4, upState, nullptr);  /* sprite_2F4 = up arrow */

        /* Scroll down button */
        int downState;
        if (this->has_prev_page == 0 || this->ui_active == 0) {
            downState = 2;
        } else {
            downState = 0;
        }
        Sprite_SetState(this->sprite_2F0, downState, nullptr);  /* sprite_2F0 = down arrow */
    }
}

/* ================================================================== */
/* Cursor::draw_locomotive_preview — Animate locomotive colour preview*/
/* Address: 0x418E20                                                   */
/*                                                                     */
/* Wipe transition showing new locomotive colour. Double-buffers      */
/* between two offscreen surfaces (+0x590/+0x598), alternating calls. */
/*                                                                     */
/* TODO RESOLVED (0x418E20 surface swap): the decompilation proves the */
/* surface roles DO swap between calls. When surface_toggle (+0x58C)   */
/* is 0: draw target = editor_surf_a (+0x590), clean surface =         */
/* editor_surf_b (+0x598), dirty flag read from surf_b_dirty (+0x59C). */
/* When surface_toggle is 1 the assignment is reversed (draw target =  */
/* editor_surf_b, clean = editor_surf_a, dirty flag read from          */
/* surf_a_dirty +0x594). The previous transcription kept both branches */
/* identical — corrected here.                                         */
/* ================================================================== */
void Cursor::draw_locomotive_preview(uint8_t direction)
{
    EnableWindow(this->hWnd, 0);

    /* Show "busy" indicator */
    Sprite_SetState(this->sprite_1C0, 1, nullptr);
    UIPANEL_EndPaint(this);

    /* Play sound effect */
    PlaySound(0x5274);

    /* Determine palette background sprite height */
    auto* paletteData = static_cast<RESDATA*>(this->sprite_37C->pixelData);
    uint palSpriteH = static_cast<uint>(paletteData->frame_height);

    void* drawTarget;   /* first-drawn surface (draw_color_palette target) */
    void* cleanSurf;    /* clean surface (gets the palette sprite blit) */
    uint8_t isCleanDirty;

    /* Toggle between surface A and B (roles swap each call) */
    if (this->surface_toggle == 0) {                 /* +0x58C */
        drawTarget = this->editor_surf_a;   /* +0x590 */
        cleanSurf  = this->editor_surf_b;   /* +0x598 */
        isCleanDirty = this->surf_b_dirty;                     /* +0x59C */
        this->surface_toggle = 1;
        this->surf_b_dirty = 0;
        this->surf_a_dirty = 1;
    } else {
        isCleanDirty = this->surf_a_dirty;                     /* +0x594 */
        drawTarget = this->editor_surf_b;   /* +0x598 */
        cleanSurf  = this->editor_surf_a;   /* +0x590 */
        this->surface_toggle = 0;
        this->surf_b_dirty = 1;
        this->surf_a_dirty = 0;
    }

    /* Unlock primary surface */
    DDRAW_UnlockPrimary();

    /* Draw colour palette to the draw target */
    this->draw_color_palette(drawTarget, 1);

    /* If the clean surface was clean, blit palette sprite to it */
    if (isCleanDirty == 0) {
        int palHeight = this->palette_rect.bottom - this->palette_rect.top;
        int palWidth  = this->palette_rect.right - this->palette_rect.left;

        void* spritePanel = this->sprite_37C->surface; /* sprite surface */

        UIPANEL_Blit(
            spritePanel,
            0,
            palHeight - static_cast<int>(palSpriteH),
            palWidth,
            palHeight,
            cleanSurf,
            0, 0, palWidth, static_cast<int>(palSpriteH),
            0);
    }

    /* Perform wipe animation */
    int totalWidth = this->palette_rect.right - this->palette_rect.left;
    int stepCount = (totalWidth + 3) >> 2;  /* totalWidth / 4, rounded up */
    int bandSize = stepCount / 6;

    if (stepCount > 0) {
        for (int step = 0; step < stepCount; step++) {
            int srcX, srcY, srcW, srcH;
            int dstX, dstY, dstW, dstH;

            if (direction == 0) {
                /* Left-to-right wipe */
                srcX = this->palette_rect.right - step * 4;
                srcY = this->palette_rect.top;
                srcW = step * 4;
                srcH = this->palette_rect.bottom - srcY;
                dstX = 0;
                dstY = 0;
                dstW = srcW;
                dstH = this->palette_rect.right - this->palette_rect.left - srcW;
            } else {
                /* Right-to-left wipe */
                srcX = this->palette_rect.left + step * 4;
                srcY = this->palette_rect.top;
                srcW = this->palette_rect.right - srcX;
                srcH = this->palette_rect.bottom - srcY;
                dstX = srcW - step * 4;
                dstY = 0;
                dstW = this->palette_rect.right - this->palette_rect.left - dstX;
                dstH = srcW;
            }

            /* Blit with colour key.
             *
             * NOTE: these locals hold {x, y, width, height}-shaped values
             * (see their initializers above/below), not DirectDraw's real
             * {left, top, right, bottom} RECT semantics -- a pre-existing
             * mismatch, unrelated to this dispatch-mechanism conversion,
             * not investigated or corrected here (same class of latent
             * issue as the already-documented DDBLT_WAIT/DDBLT_ASYNC value
             * swap found elsewhere this session). Retyped from int[4] to
             * RECT with the exact same field values/order preserved -- no
             * behavior change, just removing the untyped raw-dispatch
             * function-pointer shape. */
            {
                RECT blitRect = { srcX, srcY, srcW, srcH };
                RECT dstRect  = { dstX, dstY, dstW, dstH };
                static_cast<IDirectDrawSurface4*>(_g_primary_surface)->Blt(
                    &blitRect, static_cast<IDirectDrawSurface4*>(drawTarget), &dstRect,
                    0x1008000, nullptr);
            }

            /* Second blit for the other surface */
            if (direction == 0) {
                RECT srcRect2 = { this->palette_rect.left, srcY, step * 4, srcH };
                RECT dstRect2 = { srcW, 0, step * 4, srcH };
                static_cast<IDirectDrawSurface4*>(_g_primary_surface)->Blt(
                    &srcRect2, static_cast<IDirectDrawSurface4*>(cleanSurf), &dstRect2,
                    0x1008000, nullptr);
            } else {
                int srcW2 = this->palette_rect.right - step * 4;
                RECT srcRect2 = { srcW, srcY, srcW2, srcH };
                static_cast<IDirectDrawSurface4*>(_g_primary_surface)->Blt(
                    &srcRect2, static_cast<IDirectDrawSurface4*>(cleanSurf), nullptr,
                    0x1008000, nullptr);
            }

            /* End paint per frame */
            UIPANEL_EndPaint(this);

            /* Dispatch set_mode through vtable slot [3] (resolves to the
             * inherited UI_WindowBase::set_mode, 0x425FD0 — the Cursor
             * vtable does NOT contain the GameWindow 0x414340 variant). */
            this->set_mode(
                static_cast<int32_t>(reinterpret_cast<intptr_t>(this->child_obj_60())),
                reinterpret_cast<void*>(static_cast<intptr_t>(this->curs_pos_x())),
                0, 1);

            /* Per-frame delay: the binary sleeps 2ms for steps inside the
             * first band (stepCount/6) and 1ms for steps before stepCount/2
             * (the decompiler resets the sleep variable each iteration, so
             * the exact cadence is approximate). */
            if (step < bandSize) {
                Sleep(2);
            } else if (step < stepCount / 2) {
                Sleep(1);
            }
        }
    }

    /* Finalize: unlock, redraw palette normally, clean up */
    HWND mainWnd = static_cast<UI_WindowBase*>(g_main_window)->hWnd;
    DDRAW_UnlockPrimary();

    this->draw_color_palette(nullptr, 0);
    UIPANEL_EndPaint(this);
    Sprite_SetState(this->sprite_1C0, 0, nullptr);
    UIPANEL_EndPaintEx(this, static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)), 0, 0, nullptr);  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)

    /* Pump messages */
    CGWND_PumpMessages(0);
    EnableWindow(this->hWnd, 1);
}

/* ================================================================== */
/* Cursor::draw_postcard_preview — Layout postcard thumbnail icons    */
/* Address: 0x419260                                                   */
/*                                                                     */
/* Walks the toolbar sprite cache forward/backward from the current   */
/* selection to determine which postcard thumbnails are visible,      */
/* loading each via g_dplay->GetOrCreateSurface(). Updates palette_start_idx */
/* (+0x2BC) / palette_end_idx (+0x2B8).                                */
/*                                                                     */
/* Rewritten to match the decompilation: direction=0 walks backward   */
/* from (start-1); direction!=0 walks forward from (end+1), with the   */
/* bonus-mode branch (editor_flags[2] == 0x1F && bonus_mode == 0)     */
/* consuming the random bonus_ids table and resetting prevStart=-1.   */
/* ================================================================== */
uint8_t Cursor::draw_postcard_preview(uint8_t direction)
{
    uint prevStartIdx = static_cast<uint>(this->palette_start_idx); /* +0x2BC */
    this->has_next_page = 1;                                   /* +0x2B4 */

    if (static_cast<int>(prevStartIdx) >= 1) {
        this->has_prev_page = 1;                               /* +0x2B5 */
    } else {
        this->has_prev_page = 0;
    }

    if (direction == 0) {
        /* ---- Backward direction ---- */
        uint8_t idx = static_cast<uint8_t>(this->palette_start_idx) - 1;
        UIPANEL_Surface* surf = this->toolbar_sprites[idx];
        if (surf == nullptr) {
            surf = g_dplay->GetOrCreateSurface(
                this->editor_flags[2], this->editor_flags[1], idx + 1, 1);
            this->toolbar_sprites[idx] = surf;
        }

        if (surf == nullptr) {
            this->has_prev_page = 0;
            return 0;
        }

        int totalWidth = surf->width + 4;
        this->has_next_page = 1;

        uint8_t walkIdx = idx;
        if (totalWidth < this->palette_rect.right - this->palette_rect.left) {
            while (true) {
                uint8_t nextIdx = walkIdx - 1;
                if (static_cast<int>(nextIdx) < 0) break;
                totalWidth += 10;  /* gap */

                surf = this->toolbar_sprites[nextIdx];
                if (surf == nullptr) {
                    surf = g_dplay->GetOrCreateSurface(
                        this->editor_flags[2], this->editor_flags[1],
                        static_cast<uint8_t>(nextIdx + 1), 1);
                    this->toolbar_sprites[nextIdx] = surf;
                }

                if (surf == nullptr) {
                    this->has_prev_page = 0;
                    break;
                }

                totalWidth += surf->width;
                walkIdx = nextIdx;

                if (totalWidth >= this->palette_rect.right - this->palette_rect.left) {
                    break;
                }
            }
        }

        if (this->palette_rect.right - this->palette_rect.left < totalWidth) {
            walkIdx = walkIdx + 1;
        }

        this->palette_start_idx = walkIdx;
        this->palette_end_idx = static_cast<int>(prevStartIdx) - 1;
        this->has_prev_page = (walkIdx > 0) ? 1 : 0;
        return 1;
    }

    /* ---- Forward direction ---- */
    uint8_t seqNum;
    int newStart;
    uint8_t cacheIdx;

    if (this->editor_flags[2] == 0x1F && this->bonus_mode == 0) {
        /* Bonus mode: use random bonus_ids table */
        seqNum = this->bonus_ids[0];
        newStart = -1;
        cacheIdx = 0;
    } else {
        newStart = this->palette_end_idx;
        seqNum = static_cast<uint8_t>(this->palette_end_idx) + 1;
        cacheIdx = seqNum;
    }

    int bonusIdx = 0;
    if (seqNum > 0x40) {
        this->has_next_page = 0;
        return 0;
    }

    UIPANEL_Surface* surf = g_dplay->GetOrCreateSurface(
        this->editor_flags[2], this->editor_flags[1], seqNum + 1, 1);
    this->toolbar_sprites[cacheIdx] = surf;

    if (surf == nullptr) {
        this->has_next_page = 0;
        return 0;
    }

    int totalWidth = surf->width + 4;
    this->has_prev_page = 1;
    uint8_t lastGood = cacheIdx;

    if (totalWidth < this->palette_rect.right - this->palette_rect.left) {
        while (true) {
            uint8_t nextSeq;
            if (this->editor_flags[2] == 0x1F && this->bonus_mode == 0) {
                nextSeq = this->bonus_ids[bonusIdx + 1];
                bonusIdx++;
            } else {
                nextSeq = seqNum + 1;
            }

            cacheIdx = lastGood + 1;
            if (cacheIdx > 0x40) {
                /* Index would overflow the 64-slot cache: stop here. */
                this->has_next_page = 0;
                break;
            }

            totalWidth += 10;  /* gap */

            surf = g_dplay->GetOrCreateSurface(
                this->editor_flags[2], this->editor_flags[1],
                nextSeq + 1, 1);
            this->toolbar_sprites[cacheIdx] = surf;

            if (surf == nullptr) {
                this->has_next_page = 0;
                break;
            }

            totalWidth += surf->width;
            lastGood = cacheIdx;

            if (totalWidth >= this->palette_rect.right - this->palette_rect.left) {
                break;
            }
            seqNum = nextSeq;
        }
    }

    if (this->palette_rect.right - this->palette_rect.left < totalWidth) {
        lastGood = lastGood - 1;
    }

    this->palette_end_idx = lastGood;
    this->palette_start_idx = newStart + 1;
    this->has_prev_page = (newStart + 1 > 0) ? 1 : 0;
    return 1;
}

/* ================================================================== */
/* Cursor::draw_network_status — Update network status indicators     */
/* Address: 0x419560                                                   */
/*                                                                     */
/* Resets all network status sprites, conditionally shows based on    */
/* tab and network state. Iterates 16 bonus_sprites for tab-matching. */
/*                                                                     */
/* Validated: reset order, _g_netman m_gameMode (+0x7C4) == 2 gate    */
/* with the upload_id int16 at record+0x3A (the binary dereferences    */
/* obj_184 unconditionally in that state — the null guard below is a   */
/* host-safety deviation only), editor_state (+0xEC) == 9 gate,        */
/* handle_tab_change, and the bonus loop with the +0x2B2 / +0x188      */
/* conditions.                                                         */
/* ================================================================== */
void Cursor::draw_network_status()
{
    /* Reset all status indicator sprites to state 0 */
    Sprite_SetState(this->sprite_2C4, 0, nullptr);
    Sprite_SetState(this->sprite_2C8, 0, nullptr);
    Sprite_SetState(this->sprite_2E4, 0, nullptr);
    Sprite_SetState(this->sprite_2E8, 0, nullptr);
    Sprite_SetState(this->sprite_1C4, 0, nullptr);
    Sprite_SetState(this->sprite_2CC, 0, nullptr);

    /* If netman is in joined state (m_gameMode at +0x7C4 == 2), show the
     * upload indicator when the player record has an active upload. */
    if (_g_netman != nullptr &&
        *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(_g_netman) + 0x7C4) == 2) {
        if (this->obj_184 != nullptr) {
            int status = static_cast<int16_t>(this->obj_184->m_wordValue);
            Sprite_SetState(this->sprite_2EC, (status != 0) ? 1 : 0, nullptr);
        }
    }

    /* Show "file dialog" indicator if in state 9 */
    Sprite_SetState(this->sprite_2E0, (this->editor_state == 9) ? 1 : 0, nullptr);

    /* Update tab sprites */
    this->handle_tab_change();

    /* Update bonus sprites based on active tab */
    for (uint i = 0; i < 16; i++) {
        int state;
        if (this->bonus_mode == 0) {
            /* Normal mode: highlight sprite matching active tab */
            if (this->editor_flags[2] == static_cast<uint8_t>(i) || this->ui_active == 0) {
                state = 1;   /* hidden/disabled */
            } else {
                state = 0;   /* normal */
            }
        } else {
            /* Bonus mode: offset tab index by 0x10 */
            if (this->editor_flags[2] - 0x10 == static_cast<uint8_t>(i) || this->ui_active == 0) {
                state = 1;   /* hidden/disabled */
            } else {
                state = 0;   /* normal */
            }
        }
        Sprite_SetState(this->bonus_sprites[i], state, nullptr);
    }
}

/* ================================================================== */
/* Cursor::update_scroll_buttons — Draw scrollable player name list   */
/* Address: 0x419680                                                   */
/*                                                                     */
/* Uses GDI to render player names from player_names[] (13-byte       */
/* stride) into scrollable area. Only active in state 7 (locomotive). */
/*                                                                     */
/* Validated: +0x180 clear, FormatResourceString(100) header,         */
/* state gate at +0xEC == 7, GetStockObject(0) background, scroll     */
/* reset gated on scroll_bottom_idx (+0x174) == 0 (not top_idx — the  */
/* previous transcription checked the wrong field), line loop with    */
/* toolbar_sentinel (+0x6F0) highlight, and the final end-of-list     */
/* check on player_names[bottom_idx + 1][0].                          */
/* ================================================================== */
void Cursor::update_scroll_buttons()
{
    /* Clear scroll-end flag */
    this->scroll_end_flag = 0;                              /* +0x180 */

    char headerBuf[16] = { 0 };
    FormatResourceString(&g_resmgr, 100, headerBuf, 0x10);

    int i = this->scroll_top_idx;                           /* +0x170 */

    if (this->editor_state == 7) {
        this->scroll_line_height = 0;                       /* +0x178 */
        HDC hdc = UIPANEL_BeginPaint(this);

        /* Set up GDI text rendering */
        HGDIOBJ oldFont = SelectObject(hdc, static_cast<HGDIOBJ>(g_font_small));
        COLORREF oldColor = SetTextColor(hdc, 0x40C05C);    /* dark green */
        int oldBkMode = SetBkMode(hdc, 1);                  /* TRANSPARENT */

        /* Fill background */
        HBRUSH stockBrush = GetStockObject(0);              /* WHITE_BRUSH */
        FillRect(hdc, &this->scroll_bg_rect, stockBrush);   /* +0x128 */

        /* Draw header "Player" text */
        DrawTextA(hdc, headerBuf, -1, &this->scroll_header_rect, 0x25);  /* +0x160 */

        /* Draw edge border */
        DrawEdge(hdc, &this->scroll_border_rect, 10, 0x100F);  /* +0x150 */

        /* If the bottom index is 0, reset the scroll position counters */
        if (this->scroll_bottom_idx == 0) {                 /* +0x174 */
            this->scroll_top_idx = 0;                       /* +0x170 */
            this->scroll_visible_count = 0;                 /* +0x17C */
        }

        /* Iterate player name entries */
        i = this->scroll_top_idx;                            /* +0x170 */
        int bottomBound = this->scroll_border_rect.bottom - 2;  /* +0x15C */

        RECT lineRect;
        lineRect.left   = this->scroll_border_rect.left + 2;   /* +0x150 + 2 */
        lineRect.top    = this->scroll_border_rect.top + 2;    /* +0x154 + 2 */
        lineRect.right  = this->scroll_border_rect.right - 2;  /* +0x158 - 2 */
        lineRect.bottom = bottomBound;

        while (lineRect.top < bottomBound - 12) {
            const char* name = this->player_names[i];       /* +0x59E + i*13 */
            if (name[0] == '\0') {
                this->scroll_end_flag = 1;                  /* +0x180 */
                break;
            }

            if (i >= this->scroll_top_idx) {
                /* Set color: highlight selected item */
                COLORREF lineColor;
                if (this->toolbar_sentinel == i) {           /* +0x6F0 */
                    lineColor = 0xA0AFF;                     /* selected: light blue */
                } else {
                    lineColor = 0x40C05C;                    /* normal: dark green */
                }
                SetTextColor(hdc, lineColor);

                int lineH = DrawTextA(hdc, name, -1, &lineRect, 0x20);

                lineRect.top += lineH;

                if (this->scroll_bottom_idx == 0) {
                    this->scroll_visible_count += 1;   /* visible_count++ */
                }
                this->scroll_line_height = lineH;            /* +0x178 */
            }

            i++;
        }

        i--;
        this->scroll_bottom_idx = i;           /* +0x174 */

        /* Restore GDI state */
        SelectObject(hdc, oldFont);
        SetTextColor(hdc, oldColor);
        SetBkMode(hdc, oldBkMode);

        UIPANEL_EndPaintEx(this,
                           static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)),  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)
                           static_cast<int>(reinterpret_cast<intptr_t>(hdc)),
                           1, nullptr);

        /* Reset scroll button sprites */
        Sprite_SetState(this->sprite_148, 0, nullptr);
        Sprite_SetState(this->sprite_14C, 0, nullptr);
    }

    /* Check if at end of list: the binary tests the entry AFTER the last
     * drawn one (player_names[bottom_idx + 1][0] == 0). */
    if (this->player_names[this->scroll_bottom_idx + 1][0] == '\0') {
        this->scroll_end_flag = 1;
    }

    return;
}

/* ================================================================== */
/* Cursor::handle_tab_change — Update toolbar tab sprite states       */
/* Address: 0x4198B0                                                   */
/*                                                                     */
/* Reads tab visibility (+0x2B0) and active tab (+0x2B1, 1-6).       */
/* Hides all tabs or highlights the active tab. Validated.            */
/* ================================================================== */
void Cursor::handle_tab_change()
{
    if (this->editor_flags[0] == 0) {
        /* Tabs hidden: set all 6 tab sprites to state 2 (invisible) */
        Sprite_SetState(this->sprite_308, 2, nullptr);
        Sprite_SetState(this->sprite_30C, 2, nullptr);
        Sprite_SetState(this->sprite_310, 2, nullptr);
        Sprite_SetState(this->sprite_314, 2, nullptr);
        Sprite_SetState(this->sprite_318, 2, nullptr);
        Sprite_SetState(this->sprite_31C, 2, nullptr);
    } else {
        /* Tabs visible: set all to state 0 (default) */
        Sprite_SetState(this->sprite_308, 0, nullptr);
        Sprite_SetState(this->sprite_30C, 0, nullptr);
        Sprite_SetState(this->sprite_310, 0, nullptr);
        Sprite_SetState(this->sprite_314, 0, nullptr);
        Sprite_SetState(this->sprite_318, 0, nullptr);
        Sprite_SetState(this->sprite_31C, 0, nullptr);

        /* Highlight active tab with state 1 */
        switch (this->editor_flags[1]) {
            case 1: Sprite_SetState(this->sprite_308, 1, nullptr); break;
            case 2: Sprite_SetState(this->sprite_30C, 1, nullptr); break;
            case 3: Sprite_SetState(this->sprite_310, 1, nullptr); break;
            case 4: Sprite_SetState(this->sprite_314, 1, nullptr); break;
            case 5: Sprite_SetState(this->sprite_318, 1, nullptr); break;
            case 6: Sprite_SetState(this->sprite_31C, 1, nullptr); break;
        }
    }
}

/* ================================================================== */
/* Cursor::show_file_dialog — Show the custom-content file-open dialog */
/* Address: 0x41A050                                                    */
/* ================================================================== */
void Cursor::show_file_dialog()
{
    if (this->editor_state == 9) {                          /* +0xEC */
        return;
    }

    this->field_190 = 200;                                   /* +0x190 */
    if (this->timer_id_18C == 0) {                           /* +0x18C */
        this->timer_id_18C = SetTimer(this->hWnd, 0x44, 200, nullptr);
    }

    this->selected_idx_384 = -1;                              /* +0x384 */
    this->editor_state = 9;                                   /* +0xEC */
    Sprite_SetState(this->sprite_2E0, 1, nullptr);            /* +0x2E0 */

    /* Dispatch set_mode through vtable slot [3] (inherited base 0x425FD0);
     * matches the (childCount2, childObj2) overlay pair used at this call
     * site — a different overlay pair than child_obj_60()/curs_pos_x(). */
    this->set_mode(this->childCount2, this->childObj2, 0, 1);

    this->sprite_width_hi() = 1;                              /* +0x3D */
    this->sprite_height() = 0;                                /* +0x40 */

    UIPANEL_EndPaintEx(this, static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)), 0, 0, nullptr);  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)
}

/* ================================================================== */
/* Cursor::handle_locomotive_select — Handle locomotive-list selection */
/* Address: 0x41A360                                                    */
/* ================================================================== */
void Cursor::handle_locomotive_select(uint32_t index)
{
    if (this->ui_active) {                                    /* +0x188 */
        if (this->editor_state == 9) {                         /* +0xEC */
            if (this->timer_id_18C != 0) {                     /* +0x18C */
                KillTimer(this->hWnd, this->timer_id_18C);
                this->timer_id_18C = 0;
            }
            this->editor_state = 1;
            this->sprite_width_hi() = 0;                       /* +0x3D */
            this->sprite_height() = 0;                         /* +0x40 */
            Sprite_SetState(this->sprite_2E0, 0, nullptr);     /* +0x2E0 */
            this->set_mode(
                static_cast<int32_t>(reinterpret_cast<intptr_t>(this->child_obj_60())),
                reinterpret_cast<void*>(static_cast<intptr_t>(this->curs_pos_x())),
                0, 1);
            UIPANEL_EndPaintEx(this, static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)), 0, 0, nullptr);  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)
        }

        this->editor_state = 2;
        this->selected_idx_384 = static_cast<int32_t>(index);  /* +0x384 */

        UIPANEL_Surface* sprite = this->toolbar_sprites[index]; /* +0x48C */
        /* Decompile: local_8 = sprite->width >> 1; local_4 = sprite->height >> 1;
         * &local_8 passed as the origin pointer, with local_8 at the lower
         * stack address (i.e. the first/x field of the {x,y} pair). */
        UIAnimationOrigin origin{ sprite->width >> 1, sprite->height >> 1 };
        this->set_render_surface(sprite, 0, &origin, 0, 1);

        this->field_388 = 0;                                    /* +0x388 */
        return;
    }

    /* Editor mode: record a bonus prize ID on the player record. */
    if (this->obj_184 != nullptr) {
        this->obj_184->m_unknown93 =
            static_cast<uint8_t>(this->bonus_ids[index & 0xFF]) + 1;   /* +0x370, no bounds check */
    }
    this->blit_edit_preview();
    UIPANEL_EndPaintEx(this, static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)), 0, 0, nullptr);  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)
}

/* ================================================================== */
/* Cursor::handle_toolbar_hover — Hit-test the 6 toolbar tab sprites   */
/* Address: 0x41A460                                                    */
/* ================================================================== */
void Cursor::handle_toolbar_hover(LONG x, LONG y)
{
    uint8_t previousTab = this->editor_flags[1];                /* +0x2B1 */

    if (this->editor_flags[0] == 0) {                            /* +0x2B0 */
        return;
    }

    POINT pt{ x, y };
    if (PtInRect(reinterpret_cast<RECT*>(reinterpret_cast<uint8_t*>(this->sprite_308) + 4), pt)) {
        this->editor_flags[1] = 1;
    } else if (PtInRect(reinterpret_cast<RECT*>(reinterpret_cast<uint8_t*>(this->sprite_30C) + 4), pt)) {
        this->editor_flags[1] = 2;
    } else if (PtInRect(reinterpret_cast<RECT*>(reinterpret_cast<uint8_t*>(this->sprite_310) + 4), pt)) {
        this->editor_flags[1] = 3;
    } else if (PtInRect(reinterpret_cast<RECT*>(reinterpret_cast<uint8_t*>(this->sprite_314) + 4), pt)) {
        this->editor_flags[1] = 4;
    } else if (PtInRect(reinterpret_cast<RECT*>(reinterpret_cast<uint8_t*>(this->sprite_318) + 4), pt)) {
        this->editor_flags[1] = 5;
    } else if (PtInRect(reinterpret_cast<RECT*>(reinterpret_cast<uint8_t*>(this->sprite_31C) + 4), pt)) {
        this->editor_flags[1] = 6;
    }

    if (previousTab == this->editor_flags[1]) {
        return;
    }

    this->selected_idx_384 = -1;         /* +0x384 */
    this->palette_end_idx = -1;          /* +0x2B8 */
    this->toolbar_sentinel = -1;         /* +0x6F0 */

    for (int i = 0; i < 64; ++i) {
        if (this->toolbar_sprites[i] != nullptr) {
            delete this->toolbar_sprites[i];
            this->toolbar_sprites[i] = nullptr;
        }
    }

    if (this->timer_id_18C != 0) {                       /* +0x18C */
        KillTimer(this->hWnd, this->timer_id_18C);
        this->timer_id_18C = 0;
    }
    this->editor_state = 1;                               /* +0xEC */

    this->sprite_width_hi() = 0;                          /* +0x3D */
    this->sprite_height() = 0;                            /* +0x40 */
    Sprite_SetState(this->sprite_2E0, 0, nullptr);        /* +0x2E0 */
    this->set_mode(
        static_cast<int32_t>(reinterpret_cast<intptr_t>(this->child_obj_60())),
        reinterpret_cast<void*>(static_cast<intptr_t>(this->curs_pos_x())),
        0, 1);
    UIPANEL_EndPaintEx(this, static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)), 0, 0, nullptr);  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)

    this->handle_tab_change();
    this->draw_postcard_preview(1);
    UIPANEL_EndPaintEx(this, static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)), 0, 0, nullptr);  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)
    this->draw_locomotive_preview(1);
}

/* ================================================================== */
/* Cursor::handle_locomotive_list_click — Handle a scroll-list click   */
/* Address: 0x41A650                                                    */
/* ================================================================== */
void Cursor::handle_locomotive_list_click(LONG x, LONG y)
{
    POINT pt{ x, y };

    if (PtInRect(reinterpret_cast<RECT*>(reinterpret_cast<uint8_t*>(this->sprite_148) + 4), pt)) {
        Sprite_SetState(this->sprite_148, 1, nullptr);
        UIPANEL_EndPaintEx(this, static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)), 0, 0, nullptr);  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)
        if (this->scroll_top_idx > 0) {                    /* +0x170 */
            int32_t next = this->scroll_top_idx - this->scroll_visible_count; /* +0x17C */
            this->scroll_top_idx = (next < 0) ? 0 : next;
            this->update_scroll_buttons();
        }
        Sleep(0x32);
        Sprite_SetState(this->sprite_148, 0, nullptr);
        UIPANEL_EndPaintEx(this, static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)), 0, 0, nullptr);  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)
        return;
    }

    if (PtInRect(reinterpret_cast<RECT*>(reinterpret_cast<uint8_t*>(this->sprite_14C) + 4), pt)) {
        Sprite_SetState(this->sprite_14C, 1, nullptr);
        UIPANEL_EndPaintEx(this, static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)), 0, 0, nullptr);  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)
        if (this->scroll_end_flag == 0) {                   /* +0x180 */
            this->scroll_top_idx = this->scroll_bottom_idx + 1; /* +0x170 = +0x174 + 1 */
            this->update_scroll_buttons();
        }
        Sleep(0x32);
        Sprite_SetState(this->sprite_14C, 0, nullptr);
        UIPANEL_EndPaintEx(this, static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)), 0, 0, nullptr);  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)
        return;
    }

    if (!PtInRect(&this->scroll_border_rect, pt)) {         /* +0x150 */
        return;
    }
    if (this->scroll_line_height == 0) {                     /* +0x178 */
        return;
    }

    int32_t rowIndex = (y - this->scroll_border_rect.top) / this->scroll_line_height
                        + this->scroll_top_idx;
    if (rowIndex < 0 || rowIndex >= static_cast<int32_t>(
            sizeof(this->player_names) / sizeof(this->player_names[0]))) {
        return;
    }

    const char* rowName = this->player_names[rowIndex];       /* +0x59E, 13-byte stride */
    if (rowName[0] == '\0') {
        return;
    }

    this->toolbar_sentinel = rowIndex;                         /* +0x6F0 */
    if (this->obj_184 != nullptr) {                            /* +0x184 */
        /* Editor-local reuse of DPlayManager::m_sessionBlk1's tail byte
         * and first 20 bytes — see DPlayManager.h's class-level comment. */
        this->obj_184->m_sessionBlk1[20] = (rowIndex < this->player_count) ? 0 : 1; /* +0x6F4 */
        std::snprintf(reinterpret_cast<char*>(this->obj_184->m_sessionBlk1), 20, "%s", rowName);
    }

    this->set_mode(
        static_cast<int32_t>(reinterpret_cast<intptr_t>(this->editor_surface)),
        this->editor_resdata, 0, 1);
    this->editor_state = 6;                                     /* +0xEC */
    this->blit_edit_preview();
    UIPANEL_EndPaint(this);
}

/* ================================================================== */
/* Cursor::toolbar_wndproc — Toolbar edit control window procedure    */
/* Address: 0x419A60                                                   */
/*                                                                     */
/* Subclassed WindowProc for the edit control. Handles custom msgs:   */
/* WM_CTLCOLOREDIT (+0x133), WM_SYSCOMMAND/SC_CLOSE, WM_USER+0x5F5   */
/* for upload, WM_USER+0x5F6 for re-enable. Validated.                */
/* ================================================================== */
LRESULT Cursor::window_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == 0x133) {                     /* WM_CTLCOLOREDIT */
        if (static_cast<int>(lParam) ==
            static_cast<int>(reinterpret_cast<intptr_t>(this->hEditWnd))) {
            HDC hdc = reinterpret_cast<HDC>(static_cast<intptr_t>(wParam));
            SetTextColor(hdc, 0x40C05C);  /* dark green */
            SetBkMode(hdc, 2);             /* OPAQUE */
            SetBkColor(hdc, 0xE8E8E8);     /* light grey */
            return static_cast<LRESULT>(reinterpret_cast<intptr_t>(this->hBrush));
                                                               /* +0x380 */
        }
    } else if (msg == 0x112) {              /* WM_SYSCOMMAND */
        if ((static_cast<uint32_t>(wParam) & 0xFFF0) == 0xF140) {
                                                               /* SC_CLOSE */
            WIN32_PostQuit();
        }
    } else if (msg == 0x5F5) {              /* WM_USER+0x5F5: upload content */
        this->upload_custom_content();
    } else if (msg == 0x5F6) {              /* WM_USER+0x5F6: re-enable window */
        EnableWindow(this->hWnd, 1);
    }

    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

/* ================================================================== */
/* Cursor::upload_custom_content — Launch file-open dialog + upload   */
/* Address: 0x419B10                                                   */
/*                                                                     */
/* Opens file dialog for custom content upload. Validates <= 1MB,     */
/* uploads via NET_UploadAsset, optionally previews audio. Resets to  */
/* editor mode 1 on completion.                                        */
/*                                                                     */
/* TODO RESOLVED (0x419B10 sprite height): the decompilation writes    */
/* byte +0x3D = 0 (sprite_width_hi) and dword +0x40 = 0 (sprite_height)*/
/* at the end of the function — both fields are cleared together as   */
/* part of the sprite-dimension reset. The previous transcription      */
/* zeroed palette_end_idx instead of the verified selected_idx_384     */
/* (+0x384 = -1), and used an unsupported NET_FindPlayer argument      */
/* (CONCAT22 of the record pointer high word and upload_id).           */
/* ================================================================== */
void Cursor::upload_custom_content()
{
    /* Local buffer for file path */
    char filePath[0x504] = { 0 };
    char fileTitle[0x104] = { 0 };
    char dialogTitle[0x100] = { 0 };
    char sizeMsg[0x100] = { 0 };
    char errMsg[0x100] = { 0 };
    char* errorText = nullptr;

    /* Set editor state to 8 (uploading) */
    this->editor_state = 8;

    /* Format dialog title */
    FormatResourceString(&g_resmgr, 0x68, dialogTitle, 0x100);

    /* If a player record exists with an active upload, cancel it */
    if (this->obj_184 != nullptr) {
        int16_t uploadId = static_cast<int16_t>(this->obj_184->m_wordValue);
        if (uploadId != 0) {
            /* The binary builds the NET_FindPlayer key as CONCAT22 of the
             * record pointer's high word and the upload_id. */
            int key = (static_cast<int>(
                           reinterpret_cast<intptr_t>(this->obj_184) >> 16) << 16) |
                      static_cast<uint16_t>(uploadId);
            NET_FindPlayer(4, key);
            this->obj_184->m_wordValue = 0;
            this->blit_edit_preview();
            UIPANEL_EndPaintEx(this, static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)), 0, 0, nullptr);  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)
        }
    }

    /* File open dialog loop */
    while (true) {
        /* Initialize OPENFILENAME struct */
        struct {
            DWORD  lStructSize;
            HWND   hwndOwner;
            HINSTANCE hInstance;
            LPCSTR lpstrFilter;
            LPSTR  lpstrCustom1;
            DWORD  nMaxCustFilter;
            DWORD  nFilterIndex;
            LPSTR  lpstrFile;
            DWORD  nMaxFile;
            LPSTR  lpstrFileTitle;
            DWORD  nMaxFileTitle;
            LPCSTR lpstrInitialDir;
            LPCSTR lpstrTitle;
            DWORD  Flags;
            WORD   nFileOffset;
            WORD   nFileExtension;
            LPCSTR lpstrDefExt;
            void*  lpfnHook;
            LPARAM lCustData;
            void*  lpfnHook2;
            LPCSTR lpTemplateName;
            void*  pvReserved;
            DWORD  dwReserved;
            DWORD  FlagsEx;
        } ofn;

        ofn.lStructSize    = 0x4C;
        ofn.hwndOwner      = this->hWnd;
        ofn.hInstance      = this->hInstance;
        ofn.lpstrFilter    = "All Files (*.*)\0*.*\0\0";    /* @ 0x47E4D8 */
        ofn.nFilterIndex   = 1;
        ofn.lpstrFile      = filePath;
        ofn.nMaxFile       = 0x504;
        ofn.lpstrFileTitle = fileTitle;
        ofn.nMaxFileTitle  = 0x104;
        ofn.lpstrInitialDir = ".\\";                         /* @ 0x47E4E8 */
        ofn.lpstrTitle     = dialogTitle;
        ofn.Flags          = 0x81830;                        /* OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | ... */
        ofn.lpstrDefExt    = "dat";                          /* @ 0x47E4D4 */
        ofn.lpfnHook       = reinterpret_cast<void*>(
            static_cast<intptr_t>(0x419FD0));                  /* hook procedure */

        if (GetOpenFileNameA(&ofn) == 0) {
            break;  /* user cancelled */
        }

        /* Copy selected file path */
        /* (strcpy equivalent - the filePath already contains it from GetOpenFileNameA) */

        /* Open the file */
        HANDLE hFile = CreateFileA(
            filePath,                /* lpFileName */
            0x80000000,              /* GENERIC_READ */
            1,                       /* FILE_SHARE_READ */
            nullptr,                 /* lpSecurityAttributes */
            4,                       /* OPEN_ALWAYS */
            0x8000000,               /* FILE_FLAG_RANDOM_ACCESS */
            nullptr);                /* hTemplateFile */

        if (hFile == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            if (err == 0) {
                FormatResourceString(&g_resmgr, 0x66, errMsg, 0x100);
                MessageBoxA(this->hWnd, errMsg, "LEGO LOCO", 0x10);
            } else {
                FormatMessageA(0x1100, nullptr, err, 0x400,
                               reinterpret_cast<LPSTR>(&errorText), 0, nullptr);
                MessageBoxA(this->hWnd, errorText, "LEGO LOCO", 0x10);
                LocalFree(static_cast<void*>(errorText));
                errorText = nullptr;
            }
            continue;  /* retry dialog */
        }

        DWORD fileSize = GetFileSize(hFile, nullptr);
        CloseHandle(hFile);

        if (fileSize == 0) {
            FormatResourceString(&g_resmgr, 0x69, errMsg, 0x100);
            MessageBoxA(this->hWnd, errMsg, "LEGO LOCO", 0x10);
            continue;
        }

        if (fileSize > 0xFA000 || fileSize == 0xFFFFFFFF) {
            /* File too large (>1MB) */
            DWORD err = GetLastError();
            if (err == 0) {
                FormatResourceString(&g_resmgr, 0x67, errMsg, 0x100);
                wsprintfA(sizeMsg, "%d", 1000);
                MessageBoxA(this->hWnd, errMsg, "LEGO LOCO", 0x10);
            } else {
                FormatMessageA(0x1100, nullptr, err, 0x400,
                               reinterpret_cast<LPSTR>(&errorText), 0, nullptr);
                MessageBoxA(this->hWnd, "LEGO LOCO", errorText, 0x10);
                LocalFree(static_cast<void*>(errorText));
                errorText = nullptr;
            }
            continue;
        }

        /* Upload the file */
        uint16_t uploadId = NET_UploadAsset(4, filePath);
        this->obj_184->m_wordValue = uploadId;

        /* Check if it's a WAV file (search for ".WAV" extension).
         * The binary calls the CRT strstr with the ASCII ".WAV" needle at
         * 0x47E4C8 on the last four characters of the path. */
        uint32_t fileLen = strlen(filePath);
        int hasWavExt = 0;
        if (fileLen >= 4) {
            hasWavExt = (strstr(filePath + fileLen - 4, ".WAV") != nullptr);
        }

        if (hasWavExt) {
            this->obj_184->m_dwordValue = 1;
        } else {
            this->obj_184->m_dwordValue = 0;
            /* Play as audio if not WAV */
            PlaySoundFile(filePath,
                          this->edit_preview_rect.left,
                          this->edit_preview_rect.top,
                          4);
        }

        this->blit_edit_preview();
        break;
    }

    /* Clean up */
    /* Update upload status sprite (sprite_2EC, +0x2EC):
     * state = (obj_184->m_wordValue != 0) ? 1 : 0 */
    {
        int uploadState = (this->obj_184 != nullptr && this->obj_184->m_wordValue != 0) ? 1 : 0;
        Sprite_SetState(this->sprite_2EC, uploadState, nullptr);
    }
    UIPANEL_EndPaintEx(this, static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)), 0, 0, nullptr);  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)

    /* Kill timer if active */
    if (this->timer_id_18C != 0) {
        KillTimer(this->hWnd, static_cast<UINT_PTR>(this->timer_id_18C));
        this->timer_id_18C = 0;
    }

    /* Reset editor state and sprite dimensions. The binary writes:
     *   +0xEC  = 1   (editor_state)
     *   +0x384 = -1  (selected_idx_384 — the previous transcription
     *                 wrongly cleared palette_end_idx +0x2B8)
     *   +0x3D  = 0   (sprite_width_hi, byte)
     *   +0x40  = 0   (sprite_height, dword)
     *   sprite_2E0 state 0, then vtable[3] set_mode dispatch. */
    this->editor_state = 1;
    this->selected_idx_384 = -1;                              /* +0x384 */
    this->sprite_width_hi() = 0;                              /* +0x3D */
    this->sprite_height() = 0;                                /* +0x40 */

    /* Reset dialog background sprite */
    Sprite_SetState(this->sprite_2E0, 0, nullptr);

    /* Dispatch set_mode through vtable slot [3] (inherited base 0x425FD0) */
    this->set_mode(
        static_cast<int32_t>(reinterpret_cast<intptr_t>(this->child_obj_60())),
        reinterpret_cast<void*>(static_cast<intptr_t>(this->curs_pos_x())),
        0, 1);

    UIPANEL_EndPaintEx(this, static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)), 0, 0, nullptr);  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)
}
