// Status: TRANSCRIBED
#include "Cursor.h"
#include "Cursor_internal.h"

/* ================================================================== */
/* Cursor::init_editor_sprites — Init all editor/toolbar sprite objs   */
/* Address: 0x417F20                                                   */
/*                                                                     */
/* Loads editor sprite sheet (res 0x3CB9), retrieves surface via       */
/* RESDATA vtable[1], calls Sprite_Init on all ~49 UISprite objects.  */
/* Guarded by editor_initialized (+0x2C0) flag.                        */
/* ================================================================== */
void Cursor::init_editor_sprites()
{
    if (this->editor_initialized) {
        return;
    }

    /* Load editor sprite sheet resource 0x3CB9 */
    RESDATA* resdata = (RESDATA*)ResourceManager_GetById(&g_resmgr, 0x3CB9);
    this->editor_resdata = resdata;                          /* +0x1F0 */

    if (resdata != nullptr) {
        /* Get surface via RESDATA vtable[1] */
        void* surface = RESDATA_GetSurface(resdata, 0, 0);
        this->editor_surface = (void*)(intptr_t)surface;    /* +0x1EC */
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
/* clears editor_initialized (+0x2C0). Guarded.                        */
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
/* ================================================================== */
void Cursor::render_editor()
{
    /* Blit background surface to primary display */
    {
        RECT srcRect;
        srcRect.left   = this->editor_blit_x;
        srcRect.top    = this->editor_blit_y;
        srcRect.right  = this->editor_blit_w;
        srcRect.bottom = this->editor_blit_h;

        RECT dstRect;
        dstRect.left   = this->workRect.left;
        dstRect.top    = this->workRect.top;
        dstRect.right  = this->workRect.right;
        dstRect.bottom = this->workRect.bottom;

        UIPANEL_Blit(
            this->background_surface,               /* +0x1E8 */
            dstRect.left, dstRect.top, dstRect.right, dstRect.bottom,
            (int)(intptr_t)_g_primary_surface,
            srcRect.left, srcRect.top, srcRect.right, srcRect.bottom,
            1);
    }

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
        UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
        this->field_594 = 0;                         /* +0x594 */
        this->field_59C = 0;                         /* +0x59C */
        this->field_58C = 0;                         /* +0x58C */
        INPUT_SwitchToLocomotiveTab(this, this->editor_flags[2]);
    } else {
        /* Normal editor mode: draw color palette, reset sprite, end paint */
        this->draw_color_palette(nullptr, 0);
        Sprite_SetState(this->sprite_1C0, 0, nullptr);
        UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
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
/* updates obj_184 (+0x184) if set.                                    */
/* ================================================================== */
void Cursor::handle_color_swatch_click(LONG x, LONG y)
{
    for (int i = 0; i < 10; i++) {
        POINT pt;
        pt.x = x;
        pt.y = y;

        /* Hit-test sprite rect at sprite+0x04 */
        if (this->editor_sprites[i] != nullptr) {
            RECT* spriteRect = (RECT*)((uint8_t*)this->editor_sprites[i] + 4);
            if (PtInRect(spriteRect, &pt)) {
                /* Highlight the swatch */
                Sprite_SetState(this->editor_sprites[i], 1, nullptr);
                UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
                Sleep(0x96);  /* 150ms pause */

                /* Copy RGB from palette table to active color */
                this->color_r = (uint32_t)this->edit_colors[i * 3];
                this->color_g = (uint32_t)this->edit_colors[i * 3 + 1];
                this->color_b = (uint32_t)this->edit_colors[i * 3 + 2];

                /* Redraw color bars */
                this->draw_color_bars(1);

                /* Propagate to player record if set */
                if (this->obj_184 != nullptr) {
                    this->obj_184->color_r = (uint8_t)this->color_r;
                    this->obj_184->color_g = (uint8_t)this->color_g;
                    this->obj_184->color_b = (uint8_t)this->color_b;
                    this->blit_edit_preview();
                }

                /* Reset all swatches back to state 0 */
                for (int j = 0; j < 10; j++) {
                    if (this->editor_sprites[j] != nullptr) {
                        Sprite_SetState(this->editor_sprites[j], 0, nullptr);
                    }
                }

                UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
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
/* Clamps to [0,255]. Redraws bars and blits preview.                  */
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
    this->field_250 = component;                        /* +0x250 */
    this->field_254 = direction;    /* +0x254 */

    /* Start colour-bar auto-repeat timer if not already running */
    if (this->counter_24C == 0) {
        UINT_PTR timerId = SetTimer(this->hWnd, 0x4D, 100, nullptr);
        this->counter_24C = timerId;
    }

    /* Handle each component */
    if (component == 0) {
        /* Red channel */
        Sprite_SetState(this->sprite_2A4, 1, nullptr);
        UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);

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
        UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);

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
        UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);

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
        this->obj_184->color_r = (uint8_t)this->color_r;
        this->obj_184->color_g = (uint8_t)this->color_g;
        this->obj_184->color_b = (uint8_t)this->color_b;
    }

    /* Blit edit preview */
    this->blit_edit_preview();
    UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
}

/* ================================================================== */
/* Cursor::draw_color_bars — Draw three R/G/B vertical color bars     */
/* Address: 0x418780                                                   */
/*                                                                     */
/* Each bar's fill height is proportional to channel value (0-255),   */
/* bottom-aligned within bar RECT (+0x258/+0x268/+0x278).            */
/* If reset_buttons: resets +/- button sprites to state 0.            */
/* ================================================================== */
void Cursor::draw_color_bars(uint8_t reset_buttons)
{
    /* Create GDI brushes */
    HBRUSH stockBrush = GetStockObject(0);           /* NULL_BRUSH (hollow) */
    HBRUSH brushRed   = CreateSolidBrush(0xFF);      /* Blue channel bar fill (NOTE: BGR order) */
    HBRUSH brushGreen = CreateSolidBrush(0xFFFF);    /* Green channel bar fill */
    HBRUSH brushBlue  = CreateSolidBrush(0xFF0000);  /* Red channel bar fill */

    /* Calculate scale factor: barHeight = (barRectHeight * 100) / 255 */
    /* The original code uses: ((bottom - top) * 100) / 0xFF */
    int barFullHeight = this->color_bar_rects[0].bottom - this->color_bar_rects[0].top;
    int heightScale = (barFullHeight * 100) / 0xFF;

    /* Begin paint */
    HDC hdc = UIPANEL_BeginPaint(this);

    /* --- Red bar (color_bar_rects[0] at +0x258) --- */
    FillRect(hdc, &this->color_bar_rects[0], stockBrush);
    if (this->color_r != 0) {
        RECT fillRect;
        fillRect.left   = this->color_bar_rects[0].left;
        fillRect.right  = this->color_bar_rects[0].right;  /* color_bar_rects[0].right */
        fillRect.bottom = this->color_bar_rects[0].bottom;
        fillRect.top = fillRect.bottom - ((heightScale * this->color_r) / 100);
        if (fillRect.top == 0) fillRect.top = 1;
        FillRect(hdc, &fillRect, brushRed);
    }

    /* --- Green bar (color_bar_rects[1] at +0x268) --- */
    FillRect(hdc, &this->color_bar_rects[1], stockBrush);
    if (this->color_g != 0) {
        RECT fillRect;
        fillRect.left   = this->color_bar_rects[1].left;
        fillRect.right  = this->color_bar_rects[1].right;  /* color_bar_rects[1].right */
        fillRect.bottom = this->color_bar_rects[1].bottom;
        fillRect.top = fillRect.bottom - ((heightScale * this->color_g) / 100);
        if (fillRect.top == 0) fillRect.top = 1;
        FillRect(hdc, &fillRect, brushGreen);
    }

    /* --- Blue bar (color_bar_rects[2] at +0x278) --- */
    FillRect(hdc, &this->color_bar_rects[2], stockBrush);
    if (this->color_b != 0) {
        RECT fillRect;
        fillRect.left   = this->color_bar_rects[2].left;
        fillRect.right  = this->color_bar_rects[2].right;  /* color_bar_rects[2].right */
        fillRect.bottom = this->color_bar_rects[2].bottom;
        fillRect.top = fillRect.bottom - ((heightScale * this->color_b) / 100);
        if (fillRect.top == 0) fillRect.top = 1;
        FillRect(hdc, &fillRect, brushBlue);
    }

    /* Cleanup GDI objects.
     * NOTE: stockBrush is a GetStockObject(0) NULL_BRUSH — stock GDI objects
     * are owned by the OS and must NOT be deleted. Only delete created brushes. */
    DeleteObject(brushRed);
    DeleteObject(brushGreen);
    DeleteObject(brushBlue);

    /* End paint with HDC */
    UIPANEL_EndPaintEx(this, this->hWnd, (int)(intptr_t)hdc, 1, nullptr);

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
/* primary display. If obj_184 has a cursor bitmap, also renders the  */
/* player cursor overlay via DPLAY_RenderPlayer.                       */
/* ================================================================== */
void Cursor::blit_edit_preview()
{
    /* Calculate destination rect from edit_preview_rect (+0x1A0)
       and editor_clip_rect (+0x1D8) offsets */
    uint editPreviewX = (uint)this->edit_preview_rect.left;     /* +0x1A0 */
    uint editPreviewY = (uint)this->edit_preview_rect.top;     /* +0x1A4 */
    uint editPreviewW = (uint)this->edit_preview_rect.right;     /* +0x1A8 */
    uint editPreviewH = (uint)this->edit_preview_rect.bottom;     /* +0x1AC */

    uint destLeft   = this->editor_blit_x + editPreviewX;  /* +0x1D8 */
    uint destTop    = this->editor_blit_y + editPreviewY - 0xB;  /* +0x1DC */
    uint destRight  = editPreviewW + destLeft;
    uint destBottom = editPreviewH + destTop;

    /* Blit from background surface to primary surface */
    UIPANEL_Blit(
        this->background_surface,           /* +0x1E8 */
        editPreviewX, editPreviewY, (int)editPreviewW, editPreviewH,
        (int)(intptr_t)_g_primary_surface,
        destLeft, destTop, (int)destRight, destBottom,
        1);

    /* Render player cursor overlay if player record exists */
    if (this->obj_184 != nullptr) {
        void* dplay = _g_dplay;
        /* The original code constructs an HDC-like handle from
           obj_184 address + field_188 byte */
        int hdcVal = ((int)(intptr_t)this->obj_184 >> 8) & 0xFFFFFF;
        hdcVal = (hdcVal << 8) | this->field_188;

        /* Ghidra @ 0x4437C0: DPLAY_RenderPlayer last param is RECT*, not int*.
         * Pass address of edit_preview_rect (the binary uses LEA, not MOV). */
        DPLAY_RenderPlayer(
            dplay,
            (HDC)(intptr_t)hdcVal,
            (int)(intptr_t)this->obj_184,
            _g_primary_surface,
            this->edit_preview_rect.left,
            this->edit_preview_rect.top,
            this->edit_preview_rect.right,
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
/* ================================================================== */
void Cursor::draw_color_palette(int* target_surf, uint8_t mode)
{
    if (target_surf == nullptr) {
        mode = 0;
        target_surf = (int*)(intptr_t)_g_primary_surface;
    }

    /* Determine palette background sprite height */
    uint palSpriteH = (uint)*(uint16_t*)(*(int*)((intptr_t)this->sprite_37C + 0x14) + 0x16);

    if (mode == 0) {
        /* Normal mode: blit palette background from source */
        uint bgW = *(int*)((intptr_t)this->sprite_37C + 8) + 1;  /* sprite width + 1 */
        UIPANEL_Blit(
            this->background_surface,
            (uint)this->field_1B0,
            (uint)this->field_1B4,
            this->field_1B8,
            bgW,
            _g_primary_surface,
            this->editor_blit_x + this->field_1B0,
            this->editor_blit_y + this->field_1B4,
            this->editor_blit_x + this->field_1B8,
            this->editor_blit_y + bgW,
            1);

        /* Reset palette background sprite state */
        Sprite_SetState(this->sprite_37C, 0, nullptr);
    } else {
        /* Preview mode: clear surface with transparent fill */
        /* g_surface_bpp determines the colour-key value */
        int colorKey;
        if (g_surface_bpp == 0x22B) {
            colorKey = 0x7C1F;    /* 15-bit: magenta */
        } else if (g_surface_bpp == 0x235) {
            colorKey = 0xF81F;    /* 16-bit: magenta (555) */
        } else {
            colorKey = 0xFE08E0;  /* 16-bit: magenta (565) */
        }

        /* Clear via vtable[5] (fill) on target surface */
        int fillParams[2];
        fillParams[0] = 100;
        fillParams[1] = colorKey;
        void** surfVtbl = *(void***)target_surf;
        ((int (*)(void*, int, int, int, int, int*))surfVtbl[5])(
            target_surf, 0, 0, 0, 0x1000400, fillParams);
    }

    if (mode != 0) {
        /* Preview mode: blit palette sprite to alternate surface */
        int palW = this->field_1B8 - this->field_1B0;

        UIPANEL_Blit(
            *(void**)(*(int*)((intptr_t)this->sprite_37C + 0x18)),  /* sprite surface */
            0,
            (uint)this->field_1BC - this->field_1B4 - palSpriteH,
            palW,
            (uint)this->field_1BC,
            target_surf,
            0, 0, palW, palSpriteH,
            0);
    }

    /* Iterate toolbar sprite range from palette_start_idx to palette_end_idx */
    int currentIdx = this->palette_start_idx;              /* +0x2BC = start */
    int endIdx     = this->palette_end_idx;                /* +0x2B8 = end */

    if (currentIdx >= 0 && endIdx >= 0) {
        int availWidth = this->field_1B8;
        if (mode != 0) {
            availWidth -= this->field_1B0;
        }
        int xPos = availWidth - 4;

        while (currentIdx <= endIdx) {
            void* sprite = this->toolbar_sprites[currentIdx];
            int spriteW = *(int*)((intptr_t)sprite + 8);   /* sprite width */
            int spriteH = *(int*)((intptr_t)sprite + 0xC); /* sprite height */

            int availHeight = this->field_1BC;
            if (mode != 0) {
                availHeight -= this->field_1B4;
            }

            int yPos = availHeight;
            int clipH = 0;

            /* Tiered vertical positioning based on sprite height */
            if ((uint)(spriteH >> 2) * 3 < 0x38) {
                yPos = (availHeight - 0x1C) + (spriteH >> 2);
            } else if ((spriteH / 3) * 2 < 0x38) {
                yPos = (availHeight - 0x1C) + (spriteH / 3);
            } else if ((spriteH & 0xFFFFFFFE) < 0x70) {
                yPos = (availHeight - 0x1C) + (spriteH >> 1);
            } else if ((uint)spriteH < 0x54) {
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

            UIPANEL_Blit(
                sprite,
                destX, destY, destX + spriteW, destY + spriteH,
                target_surf,
                0, clipH, spriteW, (uint)spriteH,
                0);

            /* Cache position in palette_item_rects at +0x38C */
            int slotIdx = currentIdx - this->palette_start_idx;
            if (slotIdx >= 0 && slotIdx < 16) {
                uint* rectStorage = (uint*)&this->palette_item_rects[slotIdx];
                rectStorage[0] = destX;
                rectStorage[1] = destY;
                rectStorage[2] = destX + spriteW;
                rectStorage[3] = destY + spriteH;
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
        if (this->field_2B4 == 0 || this->field_188 == 0) {
            upState = 2;    /* disabled */
        } else {
            upState = 0;    /* normal */
        }
        Sprite_SetState(this->sprite_2F4, upState, nullptr);  /* sprite_2F4 = up arrow */

        /* Scroll down button */
        int downState;
        if (this->field_2B5 == 0 || this->field_188 == 0) {
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
/* ================================================================== */
void Cursor::draw_locomotive_preview(uint8_t direction)
{
    /* NOTE: The surface toggle logic swaps surfA/surfB meanings based on
     * field_58C, then uses isSurfBDirty flag. The field_58C toggle inverts
     * each call, creating a double-buffered wipe animation. When field_58C==0,
     * surfA=editor_surf_b and surfB=editor_surf_a; when field_58C==1, the
     * assignment is reversed. The dirty flag check (isSurfBDirty) protects
     * the initial frame where one surface hasn't been drawn yet.
     * TODO: Ghidra @ 0x418E20 — verify the surface swap and dirty flag
     *       handling against the disassembly. */
    EnableWindow(this->hWnd, 0);

    /* Show "busy" indicator */
    Sprite_SetState(this->sprite_1C0, 1, nullptr);
    UIPANEL_EndPaint(this);

    /* Play sound effect */
    PlaySound(0x5274);

    /* Determine palette background sprite height */
    uint palSpriteH = (uint)*(uint16_t*)(*(int*)((intptr_t)this->sprite_37C + 0x14) + 0x16);

    int* surfA;  /* +0x590 editor_surf_a */
    int* surfB;  /* +0x598 editor_surf_b */
    uint8_t isSurfBDirty;

    /* Toggle between surface A and B */
    if (this->field_58C == 0) {
        surfA = (int*)(intptr_t)this->editor_surf_b;   /* +0x598 */
        surfB = (int*)(intptr_t)this->editor_surf_a;   /* +0x590 */
        isSurfBDirty = this->field_59C;
        this->field_58C = 1;
        this->field_59C = 0;
        this->field_594 = 1;
    } else {
        isSurfBDirty = this->field_594;
        surfA = (int*)(intptr_t)this->editor_surf_b;   /* +0x598 */
        surfB = (int*)(intptr_t)this->editor_surf_a;   /* +0x590 */
        this->field_58C = 0;
        this->field_59C = 1;
        this->field_594 = 0;
    }

    /* Unlock primary surface */
    DDRAW_UnlockPrimary(this->hWnd);

    /* Draw colour palette to surface A */
    this->draw_color_palette(surfA, 1);

    /* If surf B was clean, blit palette sprite to it */
    if (isSurfBDirty == 0) {
        int palHeight = this->field_1BC - this->field_1B4;
        int palWidth  = this->field_1B8 - this->field_1B0;

        void* spritePanel = *(void**)(*(int*)((intptr_t)this->sprite_37C + 0x18)); /* sprite surface */

        UIPANEL_Blit(
            spritePanel,
            0,
            palHeight - palSpriteH,
            palWidth,
            palHeight,
            surfB,
            0, 0, palWidth, palSpriteH,
            0);
    }

    /* Perform wipe animation */
    int totalWidth = this->field_1B8 - this->field_1B0;
    int stepCount = (totalWidth + 3) >> 2;  /* totalWidth / 4, rounded up */
    int bandSize = stepCount / 6;

    if (stepCount > 0) {
        for (int step = 0; step < stepCount; step++) {
            int srcX, srcY, srcW, srcH;
            int dstX, dstY, dstW, dstH;

            if (direction == 0) {
                /* Left-to-right wipe */
                srcX = this->field_1B8 - step * 4;
                srcY = this->field_1B4;
                srcW = step * 4;
                srcH = this->field_1BC - srcY;
                dstX = 0;
                dstY = 0;
                dstW = srcW;
                dstH = this->field_1B8 - this->field_1B0 - srcW;
            } else {
                /* Right-to-left wipe */
                srcX = this->field_1B0 + step * 4;
                srcY = this->field_1B4;
                srcW = this->field_1B8 - srcX;
                srcH = this->field_1BC - srcY;
                dstX = srcW - step * 4;
                dstY = 0;
                dstW = this->field_1B8 - this->field_1B0 - dstX;
                dstH = srcW;
            }

            /* Blit with colour key */
            {
                int blitRect[4] = { srcX, srcY, srcW, srcH };
                int dstRect[4]  = { dstX, dstY, dstW, dstH };
                void** vtbl = *(void***)(intptr_t)_g_primary_surface;
                ((int (*)(void*, int*, int*, int*, int, void*))vtbl[5])(
                    (void*)(intptr_t)_g_primary_surface,
                    blitRect, surfA, dstRect, 0x1008000, 0);
            }

            /* Second blit for the other direction */
            if (direction == 0) {
                int srcRect2[4] = { this->field_1B0, srcY, step * 4, srcH };
                int dstRect2[4]  = { srcW, 0, step * 4, srcH };
                void** vtbl = *(void***)(intptr_t)_g_primary_surface;
                ((int (*)(void*, int*, int*, int*, int, void*))vtbl[5])(
                    (void*)(intptr_t)_g_primary_surface,
                    srcRect2, surfB, dstRect2, 0x1008000, 0);
            } else {
                int srcW2 = this->field_1B8 - step * 4;
                int srcRect2[4] = { srcW, srcY, srcW2, srcH };
                void** vtbl = *(void***)(intptr_t)_g_primary_surface;
                ((int (*)(void*, int*, int*, int*, int, void*))vtbl[5])(
                    (void*)(intptr_t)_g_primary_surface,
                    srcRect2, surfB, 0, 0x1008000, 0);
            }

            /* End paint per frame */
            UIPANEL_EndPaint(this);

            /* Dispatch set_mode to handle messages */
            this->set_mode((int32_t)(intptr_t)this->child_obj_60(),
                           (void*)(intptr_t)this->curs_pos_x(), 0, 1);

            /* Sleep for timing */
            if (step < totalWidth / 2) {
                Sleep(/* small delay */ 0);
            }
        }
    }

    /* Finalize: unlock, redraw palette normally, clean up */
    HWND mainWnd = *(HWND*)(*(int*)(intptr_t)g_main_window + 8);
    DDRAW_UnlockPrimary(mainWnd);

    this->draw_color_palette(nullptr, 0);
    UIPANEL_EndPaint(this);
    Sprite_SetState(this->sprite_1C0, 0, nullptr);
    UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);

    /* Pump messages */
    CGWND_PumpMessages(0);
    EnableWindow(this->hWnd, 1);
}

/* ================================================================== */
/* Cursor::draw_postcard_preview — Layout postcard thumbnail icons    */
/* Address: 0x419260                                                   */
/*                                                                     */
/* Walks toolbar sprite cache forward/backward from current selection */
/* to determine visible postcard thumbnails. Loads via                 */
/* NET_GetOrCreateSurface. Updates palette_start_idx/end_idx.         */
/* ================================================================== */
uint8_t Cursor::draw_postcard_preview(uint8_t direction)
{
    int prevStartIdx = this->palette_start_idx;            /* +0x2BC — Ghidra @ 0x419260 reads *(this+700) = palette_start_idx */
    this->field_2B4 = 1;  /* has_next_page = true */

    if (prevStartIdx >= 1) {
        this->field_2B5 = 1;  /* has_prev_page = true */
    } else {
        this->field_2B5 = 0;  /* has_prev_page = false */
    }

    if (direction != 0) {
        /* ---- Forward direction ---- */
        int bonusIdx = 0;
        uint8_t seqNum;

        if (this->field_59D == 0 && this->editor_flags[2] == 0x1F) {
            /* Bonus mode: use random bonus_ids table */
            seqNum = this->bonus_ids[0];
            prevStartIdx = -1;
        } else {
            seqNum = (uint8_t)this->palette_end_idx + 1;  /* palette_end_idx + 1 */
            prevStartIdx = this->palette_end_idx;
        }

        uint8_t idx = seqNum;
        if (idx > 0x40) {
            this->field_2B4 = 0;
            return 0;
        }

        /* Get or create surface for first item */
        void* surf = NET_GetOrCreateSurface(
            _g_dplay, this->editor_flags[2], this->editor_flags[1], idx + 1, 1);
        this->toolbar_sprites[idx] = surf;  /* store in cache */

        if (surf == nullptr) {
            this->field_2B4 = 0;
            return 0;
        }

        int totalWidth = *(int*)((intptr_t)surf + 8) + 4;  /* width + margin */
        this->field_2B5 = 1;

        int cacheIdx = idx;
        if (totalWidth < this->field_1B8 - this->field_1B0) {
            /* Continue loading more items that fit */
            while (true) {
                uint8_t nextSeq;
                if (this->field_59D == 0 && this->editor_flags[2] == 0x1F) {
                    nextSeq = this->bonus_ids[bonusIdx + 1];
                    bonusIdx++;
                } else {
                    nextSeq = seqNum + 1;
                }

                cacheIdx++;
                if (cacheIdx > 0x40) {
                    this->field_2B4 = 0;
                    cacheIdx--;
                    break;
                }

                totalWidth += 10;  /* gap */

                surf = NET_GetOrCreateSurface(
                    _g_dplay, this->editor_flags[2], this->editor_flags[1], nextSeq + 1, 1);
                this->toolbar_sprites[cacheIdx] = surf;

                if (surf == nullptr) {
                    this->field_2B4 = 0;
                    cacheIdx--;
                    break;
                }

                totalWidth += *(int*)((intptr_t)surf + 8);

                if (totalWidth >= this->field_1B8 - this->field_1B0) {
                    break;
                }

                seqNum = nextSeq;
            }
        }

        if (totalWidth > this->field_1B8 - this->field_1B0) {
            cacheIdx--;
        }

        this->palette_end_idx = cacheIdx;
        int newStart = prevStartIdx + 1;
        this->palette_start_idx = newStart;
        this->field_2B5 = (newStart > 0) ? 1 : 0;
        return 1;

    } else {
        /* ---- Backward direction ---- */
        uint8_t bl = (uint8_t)this->palette_start_idx - 1;  /* prevStartIdx - 1 */
        if (bl < 0) return 0;

        void* surf = this->toolbar_sprites[bl];
        if (surf == nullptr) {
            surf = NET_GetOrCreateSurface(
                _g_dplay, this->editor_flags[1], this->editor_flags[2], bl + 1, 1);
            this->toolbar_sprites[bl] = surf;
        }

        if (surf == nullptr) {
            this->field_2B5 = 0;
            return 0;
        }

        int totalWidth = *(int*)((intptr_t)surf + 8) + 4;
        this->field_2B4 = 1;

        if (totalWidth < this->field_1B8 - this->field_1B0) {
            int idx = bl;
            while (idx > 0) {
                idx--;
                totalWidth += 10;  /* gap */

                surf = this->toolbar_sprites[idx];
                if (surf == nullptr) {
                    surf = NET_GetOrCreateSurface(
                        _g_dplay, this->editor_flags[1], this->editor_flags[2], (uint8_t)(idx + 1), 1);
                    this->toolbar_sprites[idx] = surf;
                }

                if (surf == nullptr) {
                    this->field_2B5 = 0;
                    idx++;
                    break;
                }

                if (totalWidth >= this->field_1B8 - this->field_1B0) {
                    idx++;
                    break;
                }
            }

            if (totalWidth > this->field_1B8 - this->field_1B0) {
                idx++;
            }

            this->palette_end_idx = idx;
            int newStart = prevStartIdx - 1;
            this->palette_start_idx = newStart;
            this->field_2B5 = (idx > 0) ? 1 : 0;
            return 1;
        }

        return 0;
    }
}

/* ================================================================== */
/* Cursor::draw_network_status — Update network status indicators     */
/* Address: 0x419560                                                   */
/*                                                                     */
/* Resets all network status sprites, conditionally shows based on    */
/* tab and network state. Iterates 16 bonus_sprites for tab-matching. */
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

    /* If netman scenario has player entries, show status */
    /* g_netman is an opaque cross-subsystem pointer here; mode is at +0x7C4. */
    if (*(int32_t*)((uint8_t*)g_netman + 0x7C4) == 2) {
        if (this->obj_184 != 0) {
            int status = this->obj_184->upload_id;
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
        if (this->field_59D == 0) {
            /* Normal mode: highlight sprite matching active tab */
            if (this->editor_flags[2] == (uint8_t)i || this->field_188 == 0) {
                state = 1;   /* hidden/disabled */
            } else {
                state = 0;   /* normal */
            }
        } else {
            /* Bonus mode: offset tab index by 0x10 */
            if (this->editor_flags[2] - 0x10 == (uint8_t)i || this->field_188 == 0) {
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
        HGDIOBJ oldFont = SelectObject(hdc, (HGDIOBJ)g_font_small);
        COLORREF oldColor = SetTextColor(hdc, 0x40C05C);    /* dark green */
        int oldBkMode = SetBkMode(hdc, 1);                  /* TRANSPARENT */

        /* Fill background */
        HBRUSH stockBrush = GetStockObject(0);              /* NULL_BRUSH */
        FillRect(hdc, &this->scroll_bg_rect, stockBrush);   /* +0x128 */

        /* Draw header "Player" text */
        DrawTextA(hdc, headerBuf, -1, &this->scroll_header_rect, 0x25);  /* +0x160 */

        /* Draw edge border */
        DrawEdge(hdc, &this->scroll_border_rect, 10, 0x100F);  /* +0x150 */

        /* If first visible index is 0, reset line count */
        if (this->scroll_top_idx == 0) {                    /* +0x170 */
            this->scroll_top_idx = 0;
            this->scroll_visible_count = 0;
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

                if (this->scroll_top_idx == 0) {
                    this->scroll_visible_count += 1;   /* visible_count++ */
                }
                this->scroll_line_height = lineH;            /* +0x178 */
            }

            i++;
        }

        i--;
        this->scroll_bottom_idx = i;           /* scroll_bottom_idx */

        /* Restore GDI state */
        SelectObject(hdc, oldFont);
        SetTextColor(hdc, oldColor);
        SetBkMode(hdc, oldBkMode);

        UIPANEL_EndPaintEx(this, this->hWnd, (int)(intptr_t)hdc, 1, nullptr);

        /* Reset scroll button sprites */
        Sprite_SetState(this->sprite_148, 0, nullptr);
        Sprite_SetState(this->sprite_14C, 0, nullptr);
    }

    /* Check if at end of list */
    if (this->player_names[i][0] == '\0') {
        this->scroll_end_flag = 1;
    }

    return;
}

/* ================================================================== */
/* Cursor::handle_tab_change — Update toolbar tab sprite states       */
/* Address: 0x4198B0                                                   */
/*                                                                     */
/* Reads tab visibility (+0x2B0) and active tab (+0x2B1, 1-6).       */
/* Hides all tabs or highlights the active tab.                       */
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
/* Cursor::toolbar_wndproc — Toolbar edit control window procedure    */
/* Address: 0x419A60                                                   */
/*                                                                     */
/* Subclassed WindowProc for the edit control. Handles custom msgs:   */
/* WM_CTLCOLOREDIT (+0x133), WM_SYSCOMMAND/SC_CLOSE, WM_USER+0x5F5   */
/* for upload, WM_USER+0x5F6 for re-enable.                           */
/* ================================================================== */
LRESULT Cursor::toolbar_wndproc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == 0x133) {                     /* WM_CTLCOLOREDIT */
        if ((int)lParam == (int)(intptr_t)this->hEditWnd) {
            SetTextColor((HDC)(intptr_t)wParam, 0x40C05C);  /* dark green */
            SetBkMode((HDC)(intptr_t)wParam, 2);             /* OPAQUE */
            SetBkColor((HDC)(intptr_t)wParam, 0xE8E8E8);     /* light grey */
            return (LRESULT)(intptr_t)this->hBrush;          /* +0x380 */
        }
    } else if (msg == 0x112) {              /* WM_SYSCOMMAND */
        if (((uint32_t)wParam & 0xFFF0) == 0xF140) {        /* SC_CLOSE */
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
/* ================================================================== */
void Cursor::upload_custom_content()
{
    /* Local buffer for file path */
    char filePath[0x504] = { 0 };
    char fileTitle[0x104] = { 0 };
    char dialogTitle[0x100] = { 0 };
    char sizeMsg[0x100] = { 0 };
    char errMsg[0x100] = { 0 };
    const char* errorText = nullptr;

    /* Set editor state to 8 (uploading) */
    this->editor_state = 8;

    /* Format dialog title */
    FormatResourceString(&g_resmgr, 0x68, dialogTitle, 0x100);

    /* If a player record exists with an active upload, cancel it */
    if (this->obj_184 != nullptr) {
        int16_t uploadId = this->obj_184->upload_id;
        if (uploadId != 0) {
            NET_FindPlayer(4, (uploadId << 16) | (uint16_t)uploadId);
            this->obj_184->upload_id = 0;
            this->blit_edit_preview();
            UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
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
            LPARAM lCustData;
            void*  lpfnHook;
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
        ofn.lpfnHook       = (void*)0x419FD0;                /* hook procedure */

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
                FormatMessageA(0x1100, nullptr, err, 0x400, (LPSTR)&errorText, 0, nullptr);
                MessageBoxA(this->hWnd, errorText, "LEGO LOCO", 0x10);
                LocalFree((HLOCAL)errorText);
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
                FormatMessageA(0x1100, nullptr, err, 0x400, (LPSTR)&errorText, 0, nullptr);
                MessageBoxA(this->hWnd, "LEGO LOCO", errorText, 0x10);
                LocalFree((HLOCAL)errorText);
                errorText = nullptr;
            }
            continue;
        }

        /* Upload the file */
        uint16_t uploadId = NET_UploadAsset(4, filePath);
        this->obj_184->upload_id = uploadId;

        /* Check if it's a WAV file (search for ".WAV" extension).
         * Ghidra @ 0x419EB9: CRT_wcsstr is misidentified — the binary calls
         * CRT strstr (MSVC _strstr) with an ASCII ".WAV" needle at 0x47E4C8.
         * The first arg is filePath + strlen - 4 (last 4 chars = extension). */
        uint32_t fileLen = strlen(filePath);
        int hasWavExt = 0;
        if (fileLen >= 4) {
            hasWavExt = (strstr(filePath + fileLen - 4, ".WAV") != nullptr);
        }

        if (hasWavExt) {
            this->obj_184->is_audio_preview = 1;
        } else {
            this->obj_184->is_audio_preview = 0;
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
    /* Update upload status sprite.
     * Ghidra @ 0x419F32: Sprite_SetState(param_1[0xBB], upload_id!=0, 0)
     * param_1[0xBB] = byte offset 0x2EC = sprite_2EC (resource 0x3CBC).
     * Second arg is (obj_184->upload_id != 0) → 0 or 1. */
    {
        int uploadState = (this->obj_184 != nullptr && this->obj_184->upload_id != 0) ? 1 : 0;
        Sprite_SetState(this->sprite_2EC, uploadState, nullptr);
    }
    UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);

    /* Kill timer if active */
    if (this->timer_id_18C != 0) {
        KillTimer(this->hWnd, (UINT_PTR)this->timer_id_18C);
        this->timer_id_18C = 0;
    }

    /* Reset editor state to 1 (idle) */
    this->editor_state = 1;
    this->palette_end_idx = -1;                               /* +0x2B8 */
    this->field_3D = 0;                   /* +0x3D */
    /* TODO: Ghidra @ 0x419B10 — verify sprite_height = 0 at +0x40.
     *       This resets the cursor sprite height field during file
     *       upload. The field is normally set once in init_sprites().
     *       Suspicious in the upload context — may be accessing a
     *       different field at this offset. */
    this->sprite_height() = 0;                            /* +0x40 */

    /* Reset dialog background sprite */
    Sprite_SetState(this->sprite_2E0, 0, nullptr);

    /* Dispatch set_mode to repaint */
    this->set_mode((int32_t)(intptr_t)this->child_obj_60(),
                   (void*)(intptr_t)this->curs_pos_x(), 0, 1);

    UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
}
