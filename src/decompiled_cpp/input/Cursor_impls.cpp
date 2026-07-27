// Status: TRANSCRIBED
/* Cursor_impls.cpp — Implementations of methods previously stubbed
 *
 * Reconstructed from doc-comments and calling context. Each method is
 * tagged with its original binary address. Implementation behavior is
 * derived from the class documentation in Cursor.h and from call-site
 * analysis.
 */

#include "Cursor.h"
#include "Cursor_internal.h"

#ifndef _WIN32
/* Non-Windows stubs for Win32 functions used by set_capture() */
/* (declared in stubs/windows.h but sdl3_window.h does not yet provide them) */
static inline HWND GetCapture(void) { return NULL; }
static inline HWND SetCapture(HWND hWnd) { (void)hWnd; return NULL; }
static inline BOOL ReleaseCapture(void) { return 0; }
static inline int ShowCursor(int show) { (void)show; return -1; }
#endif

/* ================================================================== */
/* Cursor::set_mode — Set cursor animation state (vtable[3])           */
/* Address: 0x414340                                                    */
/*                                                                     */
/* Changes the cursor's visual state. State 0 hides the cursor;        */
/* non-zero values select an animation strip from RESDATA at +0x44.    */
/*                                                                     */
/* Called by: virtual dispatch from multiple vtables (0x47768C,        */
/*            0x4778A4, 0x47813C, 0x478434). Also called directly     */
/*            from draw_locomotive_preview() and upload_custom_content().*/
/* ================================================================== */
void Cursor::set_mode(int32_t stateId, void* resdata, uint8_t resetPos, uint8_t forceRedraw)
{
    /* If already-hidden (state 0) and requesting state 0, skip */
    if (this->cursor_state() == 0 && stateId == 0)
        return;

    /* If same non-zero state, skip state change but process flags */
    if (stateId != 0 && this->cursor_state() == stateId) {
        /* Still process resetPos and forceRedraw */
        if (resetPos != 0) {
            SetRect(&this->cursor_rect(), 0, 0, 0, 0);
            SetRect(&this->prev_cursor_rect(), 0, 0, 0, 0);
        }
        if (forceRedraw != 0) {
            this->update_dirty_rect(1);
            if (this->viewport_render_enabled() != 0) {
                this->render_with_viewport(1);
            }
        }
        return;
    }

    /* Apply new state */
    this->cursor_state() = stateId;

    if (stateId == 0) {
        /* Hiding cursor: invalidate cursor rects */
        SetRect(&this->cursor_rect(), 0, 0, 0, 0);
        SetRect(&this->prev_cursor_rect(), 0, 0, 0, 0);
        return;
    }

    /* Update animation data and reset rect cache */
    if (resdata != nullptr) {
        this->anim_resdata() = (RESDATA*)resdata;          /* +0x44 */
    }

    if (resetPos != 0) {
        SetRect(&this->cursor_rect(), 0, 0, 0, 0);
        SetRect(&this->prev_cursor_rect(), 0, 0, 0, 0);
    }

    if (forceRedraw != 0) {
        this->update_dirty_rect(1);
        if (this->viewport_render_enabled() != 0) {
            this->render_with_viewport(1);
        }
    }
}


/* ================================================================== */
/* Cursor::on_show — Pre-show virtual hook (vtable[7])                */
/* Address: 0x426130                                                    */
/*                                                                     */
/* Called before the cursor editor overlay is shown. Base              */
/* implementation is a no-op; subclasses may override.                 */
/* ================================================================== */
void Cursor::on_show()
{
    /* No-op stub in the binary */
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

    DDRAW_UnlockPrimary(hWnd);
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

            DDRAW_UnlockPrimary(this->hWnd);
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
/* viewport, unions with stored cursor_rect, and composites cursor.   */
/* Two paths: accelerated (<256px dirty rect) vs normal.              */
/* ================================================================== */
void Cursor::update_dirty_rect(uint8_t param)
{
    /* Unlock primary surface */
    DDRAW_UnlockPrimary(this->hWnd);

    /* Get current cursor position */
    POINT cursorPos;
    GetCursorPos(&cursorPos);

    /* Get animation data */
    void* animData = this->anim_resdata();             /* +0x44 */
    if (animData == nullptr) {
        Cursor_UnlockAllSurfaces();
        return;
    }

    /* Adjust by hotspot offset */
    cursorPos.x -= *(int16_t*)((intptr_t)animData + 0x32);  /* hotspot_x */
    cursorPos.y -= *(int16_t*)((intptr_t)animData + 0x34);  /* hotspot_y */

    /* Get sprite dimensions */
    uint32_t spriteW = *(uint16_t*)((intptr_t)animData + 0x14); /* width  */
    uint32_t spriteH = *(uint16_t*)((intptr_t)animData + 0x16); /* height */

    /* Build new cursor rect */
    RECT newRect;
    newRect.left   = cursorPos.x;
    newRect.top    = cursorPos.y;
    newRect.right  = cursorPos.x + (int)spriteW;
    newRect.bottom = cursorPos.y + (int)spriteH;

    /* Clip to viewport */
    if (newRect.right > this->clip_rect_right())         /* +0x20 */
        newRect.right = this->clip_rect_right();
    if (newRect.bottom > this->clip_rect_bottom())       /* +0x24 */
        newRect.bottom = this->clip_rect_bottom();

    /* Union with stored cursor rect */
    RECT unionRect;
    UnionRect(&unionRect, &newRect, &this->cursor_rect());  /* +0x68 */

    /* Expand by 4px (anti-alias bleed) */
    unionRect.left   -= 4;
    unionRect.top    -= 4;
    unionRect.right  += 4;
    unionRect.bottom += 4;

    /* Re-clip to viewport */
    if (unionRect.left < this->clip_rect_left())          /* +0x18 */
        unionRect.left = this->clip_rect_left();
    if (unionRect.top < this->clip_rect_top())            /* +0x1C */
        unionRect.top = this->clip_rect_top();
    if (unionRect.right > this->clip_rect_right())
        unionRect.right = this->clip_rect_right();
    if (unionRect.bottom > this->clip_rect_bottom())
        unionRect.bottom = this->clip_rect_bottom();

    /* Compute dirty dimensions */
    int dirtyW = unionRect.right - unionRect.left;
    int dirtyH = unionRect.bottom - unionRect.top;

    /* Store cursor rect for next frame */
    CopyRect(&this->cursor_rect(), &newRect);             /* +0x68 */

    /* Check if cursor is active and not captured */
    if (this->cursor_state() != 0 && this->capture_flag() == 0) {  /* +0x14, +0x58 */
        if (param != 0 && dirtyW < 256 && dirtyH < 256) {
            /* ACCELERATED PATH: single composite over union rect */
            /* Capture background */
            {
                void** vtbl = *(void***)this->backbuffer();      /* +0x5C */
                ((SurfaceBlt_t)vtbl[0x14 / 4])(
                    this->backbuffer(),
                    &unionRect,
                    this->primary_surface(),                     /* +0x38 */
                    &unionRect,
                    BLIT_WAIT,
                    nullptr);
            }

            /* Overlay cursor sprite (color-keyed) */
            {
                void** vtbl = *(void***)this->primary_surface();
                ((SurfaceBlt_t)vtbl[0x14 / 4])(
                    this->primary_surface(),
                    &unionRect,
                    this->cursor_sprite_surface(),               /* +0x14 */
                    &unionRect,
                    BLIT_KEYSRC_WAIT,
                    nullptr);
            }

            /* Composite to backbuffer */
            {
                void** vtbl = *(void***)_g_backbuffer;
                ((SurfaceBlt_t)vtbl[0x14 / 4])(
                    _g_backbuffer,
                    &unionRect,
                    this->primary_surface(),
                    &unionRect,
                    BLIT_WAIT,
                    nullptr);
            }
        } else {
            /* NORMAL PATH: separate restore + render */
            /* Restore background from primary surface */
            {
                void** vtbl = *(void***)this->backbuffer();
                ((SurfaceBlt_t)vtbl[0x14 / 4])(
                    this->backbuffer(),
                    &newRect,
                    this->primary_surface(),
                    &newRect,
                    BLIT_WAIT,
                    nullptr);
            }

            /* Composite cursor sprite */
            {
                void** vtbl = *(void***)this->primary_surface();
                /* NOTE: srcRect is nullptr for NORMAL PATH blits.
                 *       Per the binary at 0x414FB0, this passes NULL as the
                 *       source rect, which tells DirectDraw to use the entire
                 *       source surface. This is correct for the normal path
                 *       which composites the full cursor sprite. */
                ((SurfaceBlt_t)vtbl[0x14 / 4])(
                    this->primary_surface(),
                    &newRect,
                    this->cursor_sprite_surface(),
                    nullptr,
                    BLIT_KEYSRC_WAIT,
                    nullptr);
            }

            /* Composite to backbuffer */
            {
                void** vtbl = *(void***)_g_backbuffer;
                ((SurfaceBlt_t)vtbl[0x14 / 4])(
                    _g_backbuffer,
                    &newRect,
                    this->primary_surface(),
                    &newRect,
                    BLIT_WAIT,
                    nullptr);
            }
        }
    }

    Cursor_UnlockAllSurfaces();
}


/* ================================================================== */
/* Cursor::render_with_viewport — Render cursor with viewport clipping*/
/* Address: 0x415440                                                    */
/*                                                                     */
/* Viewport-aware version of cursor rendering. Builds a viewport rect */
/* from globals: windowed mode uses g_clientWidth/g_clientHeight;     */
/* fullscreen mode offsets by g_viewportX/g_viewportY.                 */
/* ================================================================== */
void Cursor::render_with_viewport(uint8_t param)
{
    /* Build viewport rect */
    RECT vpRect;

    if (g_is_fullscreen != 0) {
        vpRect.left   = g_viewport_x;
        vpRect.top    = g_viewport_y;
        vpRect.right  = g_viewport_x + g_client_width;
        vpRect.bottom = g_viewport_y + g_client_height;
    } else {
        vpRect.left   = 0;
        vpRect.top    = 0;
        vpRect.right  = g_client_width;
        vpRect.bottom = g_client_height;
    }

    /* Use viewport as clip rect temporarily */
    int32_t savedLeft   = this->clip_rect_left();        /* +0x18 */
    int32_t savedTop    = this->clip_rect_top();         /* +0x1C */
    int32_t savedRight  = this->clip_rect_right();       /* +0x20 */
    int32_t savedBottom = this->clip_rect_bottom();      /* +0x24 */

    this->clip_rect_left()   = vpRect.left;
    this->clip_rect_top()    = vpRect.top;
    this->clip_rect_right()  = vpRect.right;
    this->clip_rect_bottom() = vpRect.bottom;

    this->viewport_render_enabled() = 1;                 /* +0x88 */
    this->update_dirty_rect(param);
    this->viewport_render_enabled() = 0;

    /* Restore original clip rect */
    this->clip_rect_left()   = savedLeft;
    this->clip_rect_top()    = savedTop;
    this->clip_rect_right()  = savedRight;
    this->clip_rect_bottom() = savedBottom;
}


/* ================================================================== */
/* Cursor::update_network_names — Refresh network player names        */
/* Address: 0x416E00                                                    */
/*                                                                     */
/* Populates player_names[26][13] (+0x59E) with up to 26 names:       */
/*   1. If g_netman scenarioId == 2: reads scenario player names from */
/*      g_netman's player entries (stride 0x4C, name at +0x51D).      */
/*   2. Otherwise: uses formatted resource string (#100, 13 chars max)*/
/*      as a single default name.                                      */
/*   3. Calls DPLAY_EnumeratePlayers() then appends names from         */
/*      _g_dplay (+0xB13, stride 0xD, up to 16 entries).              */
/*   4. Zero-fills remaining slots up to 26.                           */
/* Player count stored at +0x6F4 (player_count).                       */
/* ================================================================== */
void Cursor::update_network_names()
{
    int nameIdx = 0;

    /* Check if g_netman has scenario player entries */
    /* g_netman→scenarioId at offset +0x7C4 */
    if (g_netman != nullptr && *(int32_t*)((uint8_t*)g_netman + 0x7C4) == 2) {
        /* Read player names from g_netman's player table.
         * Players stored at g_netman+0x30 (or similar), stride 0x4C,
         * name string at player_entry+0x51D.
         * The binary iterates scenario player entries (usually 2 players). */
        uint8_t* netmanBase = (uint8_t*)g_netman;
        /* Player entries start at offset 0x30, iterate up to 2 entries */
        for (int p = 0; p < 2 && nameIdx < 26; p++) {
            uint8_t* playerEntry = netmanBase + 0x30 + p * 0x4C;
            /* Name at offset +0x51D within the entry (but from player_entry, +0x51D-0x30=+0x4ED?) */
            /* The binary uses a fixed offset from g_netman base */
            const char* srcName = (const char*)(netmanBase + 0x4ED + p * 0x4C);
            if (srcName[0] != '\0') {
                /* Copy up to 12 chars + null */
                for (int c = 0; c < 12; c++) {
                    this->player_names[nameIdx][c] = srcName[c];
                    if (srcName[c] == '\0') break;
                }
                this->player_names[nameIdx][12] = '\0';
                nameIdx++;
            }
        }
    } else {
        /* Offline: use formatted resource string #100 as default name */
        char defaultName[13] = { 0 };
        FormatResourceString(&g_resmgr, 100, defaultName, 13);
        for (int c = 0; c < 12; c++) {
            this->player_names[nameIdx][c] = defaultName[c];
            if (defaultName[c] == '\0') break;
        }
        this->player_names[nameIdx][12] = '\0';
        nameIdx = 1;
    }

    /* Enumerate DPLAY players */
    DPLAY_EnumeratePlayers();

    /* Read names from _g_dplay player table at +0xB13, stride 0xD */
    /* Up to 16 entries */
    if (_g_dplay != nullptr) {
        uint8_t* dplayBase = (uint8_t*)_g_dplay;
        for (int p = 0; p < 16 && nameIdx < 26; p++) {
            const char* srcName = (const char*)(dplayBase + 0xB13 + p * 0xD);
            if (srcName[0] != '\0') {
                /* Copy up to 12 chars + null */
                for (int c = 0; c < 12; c++) {
                    this->player_names[nameIdx][c] = srcName[c];
                    if (srcName[c] == '\0') break;
                }
                this->player_names[nameIdx][12] = '\0';
                nameIdx++;
            }
        }
    }

    /* Zero-fill remaining slots */
    for (int i = nameIdx; i < 26; i++) {
        this->player_names[i][0] = '\0';
    }

    this->player_count = nameIdx;                      /* +0x6F4 */
}


/* ================================================================== */
/* Cursor::init_network_player — Initialize local player data         */
/* Address: 0x41A0E0                                                    */
/*                                                                     */
/* Creates a local player entry in obj_184 (+0x184) when no network   */
/* player data is available. Uses DPLAY_CreatePlayer and populates    */
/* the record with default values from g_player_config.               */
/* ================================================================== */
void Cursor::init_network_player()
{
    if (this->obj_184 != nullptr)                       /* +0x184 */
        return;

    /* Get player name from config */
    const char* cfgName = (const char*)((intptr_t)g_player_config + 6);
    char playerName[64] = { 0 };
    size_t nameLen = strlen(cfgName);
    if (nameLen > 63) nameLen = 63;
    memcpy(playerName, cfgName, nameLen);
    playerName[nameLen] = '\0';

    /* Create a DPLAY player record. The binary calls DPLAY_CreatePlayer
     * with a description ("LEGO LOCO Player") and copies the player
     * name from g_player_config into the record at +0x25. */
    void* record = nullptr;
    if (_g_dplay != nullptr) {
        record = (void*)DPLAY_CreatePlayer(nullptr);
        if (record != nullptr) {
            /* Copy player name into record at offset +0x25 */
            memcpy((void*)((intptr_t)record + 0x25), playerName, nameLen + 1);
        }
    }

    /* If DPLAY_CreatePlayer failed, allocate a raw record */
    if (record == nullptr) {
        record = operator_new(0x46);  /* sizeof(CursorEditorRecord) */
        if (record != nullptr) {
            /* Zero-initialize */
            for (int i = 0; i < 0x46; i++) {
                ((uint8_t*)record)[i] = 0;
            }
            /* Set default values */
            memcpy((void*)((intptr_t)record + 0x25), playerName, nameLen + 1);
            *(int32_t*)((intptr_t)record + 0x3C) = 1;   /* is_audio_preview = 1 */
        }
    }

    this->obj_184 = (CursorEditorRecord*)record;      /* +0x184 */
}
