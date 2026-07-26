/**
 * postcard_album.c — PostcardAlbum window management functions
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * The PostcardAlbum is the full-screen window shown when the player views
 * their collection of received postcards. It manages a scrollable grid of
 * postcard tiles with tile name labels, scroll buttons, stamp overlays,
 * and photo preview areas.
 *
 * Functions in this file:
 *   PostcardAlbum_CreateFromResource  (0x401F50) — Factory constructor
 *   PostcardAlbum_DestroyFromResource (0x401FB0) — Scalar deleting dtor
 *   PostcardAlbum_InitFromResource    (0x401FD0) — Core initialization
 *   PostcardAlbum_FreeAllSprites      (0x402380) — Free all sprites + cleanup
 *   PostcardAlbum_InitWindow          (0x402520) — Create child window
 *   PostcardAlbum_DestroyWindow       (0x402660) — Destroy child window
 *   PostcardAlbum_PaintWindow         (0x402690) — Window message handler
 *   PostcardAlbum_UpdateSprite        (0x403BA0) — Update sprite visibility
 *   PostcardAlbum_HitTest             (0x403CD0) — Hit-test for click dispatch
 *   PostcardAlbum_BlitElement         (0x403E80) — Render album element
 *   PostcardAlbum_InitWindowSurface   (0x404720) — Create DDRAW surface
 *   PostcardAlbum_InitSprites         (0x404770) — Init child sprites
 *   PostcardAlbum_FreeSprites         (0x404830) — Free child sprites
 *   PostcardAlbum_RenderTileName      (0x4048E0) — Render tile name label
 *   PostcardAlbum_RenderAllTiles      (0x404AC0) — Render all tile labels
 *
 * Struct: PostcardAlbum (extends UI_WindowBase, vtable 0x4773F0, size ~0x254)
 *
 * Field layout (+0x000): UI_WindowBase base fields (0x00-0xD3)
 *   +0xD4:  scroll_rect_left    (int32) — scroll offset X
 *   +0xD8:  scroll_rect_top     (int32) — scroll offset Y
 *   +0xE8:  hIcon               (HICON) — window icon
 *   +0xEC:  viewport_offset_x   (int32) — viewport offset X
 *   +0xF0:  viewport_offset_y   (int32) — viewport offset Y
 *   +0xFC:  surface_initialized (byte)  — surface created flag
 *   +0x110: is_visible           (byte)  — window visibility
 *   +0x111: sprites_initialized  (byte)  — sprites created flag
 *   +0x112: content_loaded       (byte)  — content loaded flag
 *   +0x114: current_entry_offset (int32) — current postcard page offset
 *   +0x118: current_album_index  (int32) — active album index
 *   +0x11C: entries_per_page     (int32) — postcards per page
 *   +0x120: hit_sprite_index     (int32) — last hit sprite ID
 *   +0x124: visible_columns      (int32) — number of visible columns
 *   +0x128: selected_index       (int32) — selected postcard index
 *   +0x12C: (unused)
 *   +0x130: scroll_position      (int32) — scrollwheel position
 *   +0x134: hi_res_flag          (int32) — 1=hi-res (>=800x600), 0=lo-res
 *   +0x138: pResource            (void*) — album surface resource
 *   +0x13C: ddraw_surface        (void*) — DirectDraw surface
 *   +0x140: pStampResource       (void*) — stamp area resource
 *   +0x144: stamp_ddraw_surface  (void*) — stamp DirectDraw surface
 *   +0x148: title_sprite         (void*) — ButtonSprite (ID 0x3c04)
 *   +0x14C: stamp_sprite         (void*) — ButtonSprite (ID 0x3c09)
 *   +0x150: photo_sprite         (void*) — ButtonSprite (ID 0x3c05)
 *   +0x154: peruse_sprite        (void*) — ButtonSprite (ID 0x3c08)
 *   +0x158: done_sprite          (void*) — ButtonSprite (ID 0x3c0f)
 *   +0x15C: scroll_left_sprite   (void*) — ButtonSprite (ID 0x3c06)
 *   +0x160: scroll_right_sprite  (void*) — ButtonSprite (ID 0x3c07)
 *   +0x164: album_sprite         (void*) — ButtonSprite (ID 0x3c0c/0x3c0d)
 *   +0x168: tile_sprites[6]      (void*[6]) — 6 tile area sprites (0x3c0e)
 *   +0x180: tile_bg_sprites[6]   (void*[6]) — 6 tile bg sprites
 *   +0x198: tile_label_sprites[6](void*[6]) — 6 tile label sprites
 *   +0x1B0: thumbnail_sprites[9] (void*[9]) — 9 thumbnail sprites
 *   +0x1D4: flags[6]             (byte[6])  — 6 per-tile flag bytes
 *   +0x1DA: tile_names[6]        (char[6][20]) — 6 tile name buffers
 */

#include "../shared/types.h"
#include "../ui/UI_WindowBase.h"

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void* operator_new(uint32_t size);

extern void  __thiscall UI_WindowBase_Ctor(void* this, void* hInst, uint32_t resId);
extern void* __thiscall UI_WindowBase_BaseDtor(void* this);

extern void* __thiscall ButtonSprite_Ctor(void* this, uint32_t resId);
extern void  __thiscall Sprite_Init(void* sprite);
extern void  __thiscall Sprite_Destroy(void* sprite);
extern void* __thiscall Sprite_SetState(void* sprite, int32_t state, int32_t* param);

extern void* ResourceManager_GetById(void* resmgr, uint32_t id);

extern uint32_t  __cdecl PixelDataCache_Unlock(void* cache, int32_t index);
extern uint32_t  __cdecl PixelDataCache_GetEntryCount(void* cache);
extern void*     __thiscall PixelDataCache_LookupAsset(void* cache, int32_t offset, int32_t index);

extern uint8_t   __thiscall UI_CreateFullWindow(void* this, int32_t param1, void* hWnd,
                        int32_t left, int32_t top, int32_t width, int32_t height,
                        void* hMenu, void* hIcon, int32_t param10);

extern void*     __thiscall UIPANEL_EndPaintEx(void* self, void* surface, void* hdc,
                        uint8_t clear, void* rect);

extern uint8_t   __cdecl UIPANEL_Blit(void* src_surf, int32_t src_l, int32_t src_t,
                        int32_t src_r, int32_t src_b, void* dst_surf,
                        int32_t dst_l, int32_t dst_t, int32_t dst_r, int32_t dst_b,
                        int32_t flags);

extern void*     __thiscall UIPANEL_BeginPaint(void* self);

extern void      __cdecl PlaySound(uint32_t resId);
extern void      __stdcall OutputDebugStringA(const char* str);

extern void      __thiscall DPLAY_RenderPlayer(void* dplay, void* hdc, int32_t data,
                        void* surface, int32_t x, int32_t y, int32_t w, void* h);

extern void      __stdcall CopyRect(void* dst, const void* src);
extern void      __stdcall OffsetRect(void* rect, int32_t dx, int32_t dy);
extern int32_t   __stdcall PtInRect(const void* rect, uint32_t pt);
extern void      __stdcall SetBkMode(void* hdc, int32_t mode);
extern uint32_t  __stdcall SetTextColor(void* hdc, uint32_t color);
extern void*     __stdcall SelectObject(void* hdc, void* obj);
extern int32_t   __stdcall DrawTextA(void* hdc, const char* text, int32_t len,
                        void* rect, uint32_t fmt);
extern void      __stdcall Sleep(uint32_t ms);
extern void*     __stdcall DefWindowProcA(void* hWnd, uint32_t msg, void* wParam, void* lParam);

extern void      __stdcall CGWND_SetMode(void* mode);

extern int32_t   g_screen_width;      /* 0x4A9E40 */
extern int32_t   g_screen_height;     /* 0x4A9E44 */
extern void*     g_resmgr;            /* 0x4A9F40 */
extern void*     _g_dplay_config;     /* 0x4A9DEC */
extern void*     _g_dplay;            /* 0x4A9DE8 */
extern void*     _g_primary_surface;  /* 0x4A9E48 */
extern void*     g_font_small;        /* 0x4A9E4C */

static const char s_blit_failure[] = "AW_Blit_failure_reported";
/* ^ 0x0047e0d8 */

/* ================================================================== */
/* PostcardAlbum_CreateFromResource                                    */
/* Address: 0x401F50                                                   */
/* ================================================================== */
void* __thiscall PostcardAlbum_CreateFromResource(
    void* this,
    void* hInst,
    uint32_t resId)
{
    /* Factory/constructor for PostcardAlbum window (vtable 0x4773F0).
     * Calls UI_WindowBase_Ctor, sets vtable, dispatches InitFromResource. */
    void* old_exception;

    old_exception = EXCEPTION_LIST;
    EXCEPTION_LIST = &old_exception;

    UI_WindowBase_Ctor(this, hInst, resId);
    *(void***)this = &VTBL_POSTCARD_ALBUM;  /* 0x4773F0 */
    PostcardAlbum_InitFromResource(this);

    EXCEPTION_LIST = old_exception;
    return this;
}

/* ================================================================== */
/* PostcardAlbum_DestroyFromResource                                   */
/* Address: 0x401FB0                                                   */
/* ================================================================== */
void* __thiscall PostcardAlbum_DestroyFromResource(
    void* this,
    uint8_t flags)
{
    /* Scalar deleting destructor (vtable[0]). Calls FreeAllSprites +
     * DestroyWindow + UI_WindowBase_BaseDtor + optional GLOBAL_free. */
    PostcardAlbum_FreeAllSprites(this);
}

/* ================================================================== */
/* PostcardAlbum_DestroyWindow                                         */
/* Address: 0x402660                                                   */
/* ================================================================== */
void __thiscall PostcardAlbum_DestroyWindow(void* this)
{
    /* Destroys the postcard album window. Hides, releases DDRAW surfaces,
     * and destroys the HWND. Counterpart to InitWindow. */
    if (*(uint8_t*)((uint8_t*)this + 0x110) != 0) {
        /* Hide window, release surfaces, destroy HWND */
        *(void**)((uint8_t*)this + 0xE8) = 0;   /* +0xE8: hIcon */
        *(void**)((uint8_t*)this + 0x13C) = 0;  /* +0x13C: ddraw_surface */
        *(void**)((uint8_t*)this + 0x144) = 0;  /* +0x144: stamp_ddraw_surface */
        *(uint8_t*)((uint8_t*)this + 0x110) = 0; /* +0x110: is_visible = 0 */
    }
}

/* ================================================================== */
/* PostcardAlbum_InitFromResource                                      */
/* Address: 0x401FD0 (939 bytes)                                       */
/* ================================================================== */
void __fastcall PostcardAlbum_InitFromResource(void* this)
{
    /* Core init for PostcardAlbum (~0x254 byte struct). Loads postcard layout
     * resources (0x3A79-0x3AFE range), creates 7 main ButtonSprite objects
     * (title, stamp, photo, peruse, done, left/right scroll), 6 tile area
     * sprites with background and label sub-sprites, and 9 thumbnail sprites.
     * Also initialises 3D scrollwheel state and hi-res flag. */
    int i;
    uint8_t* self = (uint8_t*)this;

    self[0x111] = 0;         /* sprites_initialized = false */
    *(int32_t*)(self + 0x114) = 0;   /* current_entry_offset = 0 */
    *(int32_t*)(self + 0x118) = 0;   /* current_album_index = 0 */
    *(int32_t*)(self + 0x11C) = 0;   /* entries_per_page = 0 */
    *(int32_t*)(self + 0x120) = 0;   /* hit_sprite_index = 0 */
    *(int32_t*)(self + 0x124) = 9;   /* visible_columns = 9 */
    *(int32_t*)(self + 0x128) = 0;   /* selected_index = 0 */
    self[0x12C] = 1;                 /* (unknown flag) */
    self[0x112] = 0;                 /* content_loaded = false */
    self[0x110] = 0;                 /* is_visible = false */

    if (g_screen_width < 0x321 && g_screen_height < 0x259) {
        /* < 800x600: lo-res */
        *(int32_t*)(self + 0x134) = 0;
    } else {
        /* >= 800x600: hi-res */
        *(int32_t*)(self + 0x134) = 1;
    }

    /* Create 7 main button sprites */

    /* +0x148: title_sprite (0x3c04) */
    *(void**)(self + 0x148) = operator_new(0x24);
    if (*(void**)(self + 0x148)) {
        ButtonSprite_Ctor(*(void**)(self + 0x148), 0x3c04);
    }

    /* +0x14C: stamp_sprite (0x3c09) */
    *(void**)(self + 0x14C) = operator_new(0x24);
    if (*(void**)(self + 0x14C)) {
        ButtonSprite_Ctor(*(void**)(self + 0x14C), 0x3c09);
    }

    /* +0x150: photo_sprite (0x3c05) */
    *(void**)(self + 0x150) = operator_new(0x24);
    if (*(void**)(self + 0x150)) {
        ButtonSprite_Ctor(*(void**)(self + 0x150), 0x3c05);
    }

    /* +0x154: peruse_sprite (0x3c08) */
    *(void**)(self + 0x154) = operator_new(0x24);
    if (*(void**)(self + 0x154)) {
        ButtonSprite_Ctor(*(void**)(self + 0x154), 0x3c08);
    }

    /* +0x158: done_sprite (0x3c0f) */
    *(void**)(self + 0x158) = operator_new(0x24);
    if (*(void**)(self + 0x158)) {
        ButtonSprite_Ctor(*(void**)(self + 0x158), 0x3c0f);
    }

    /* +0x15C: scroll_left_sprite (0x3c06) */
    *(void**)(self + 0x15C) = operator_new(0x24);
    if (*(void**)(self + 0x15C)) {
        ButtonSprite_Ctor(*(void**)(self + 0x15C), 0x3c06);
    }

    /* +0x160: scroll_right_sprite (0x3c07) */
    *(void**)(self + 0x160) = operator_new(0x24);
    if (*(void**)(self + 0x160)) {
        ButtonSprite_Ctor(*(void**)(self + 0x160), 0x3c07);
    }

    /* +0x164: album_sprite — hi-res uses 0x3c0d, lo-res uses 0x3c0c */
    if (*(int32_t*)(self + 0x134) == 0) {
        *(void**)(self + 0x164) = operator_new(0x24);
        if (*(void**)(self + 0x164)) {
            ButtonSprite_Ctor(*(void**)(self + 0x164), 0x3c0c);
        }
    } else {
        *(void**)(self + 0x164) = operator_new(0x24);
        if (*(void**)(self + 0x164)) {
            ButtonSprite_Ctor(*(void**)(self + 0x164), 0x3c0d);
        }
    }

    /* Create 6 tile area sprites (at +0x168, +0x180, +0x198) */
    for (i = 0; i < 6; i++) {
        /* +0x168: tile_sprite[i] */
        *(void**)(self + 0x168 + i * 4) = operator_new(0x24);
        if (*(void**)(self + 0x168 + i * 4)) {
            ButtonSprite_Ctor(*(void**)(self + 0x168 + i * 4), 0);
        }

        /* +0x180: tile_bg_sprite[i] (0x3c0e) */
        *(void**)(self + 0x180 + i * 4) = operator_new(0x24);
        if (*(void**)(self + 0x180 + i * 4)) {
            ButtonSprite_Ctor(*(void**)(self + 0x180 + i * 4), 0x3c0e);
        }

        /* +0x198: tile_label_sprite[i] */
        *(void**)(self + 0x198 + i * 4) = operator_new(0x24);
        if (*(void**)(self + 0x198 + i * 4)) {
            ButtonSprite_Ctor(*(void**)(self + 0x198 + i * 4), 0);
        }

        /* +0x1DA + i*0x14: tile_name[i] buffer — zero-initialized */
        self[0x1DA + i * 0x14] = 0;
    }

    /* Create 9 thumbnail sprites (at +0x1B0) */
    for (i = 0; i < 9; i++) {
        *(void**)(self + 0x1B0 + i * 4) = operator_new(0x24);
        if (*(void**)(self + 0x1B0 + i * 4)) {
            ButtonSprite_Ctor(*(void**)(self + 0x1B0 + i * 4), 0);
        }
    }

    /* Initialize 6 per-tile flag bytes */
    self[0x1D4] = 1;
    self[0x1D5] = 1;
    self[0x1D6] = 1;
    self[0x1D7] = 1;
    self[0x1D8] = 1;
    self[0x1D9] = 1;

    /* +0x138: pResource = 0 (loaded later) */
    *(int32_t*)(self + 0x138) = 0;
    /* +0x130: scroll_position = 0 */
    *(int32_t*)(self + 0x130) = 0;
    /* +0xFC: surface_initialized = false */
    self[0xFC] = 0;
}

/* ================================================================== */
/* PostcardAlbum_FreeAllSprites                                        */
/* Address: 0x402380 (407 bytes)                                       */
/* ================================================================== */
void __fastcall PostcardAlbum_FreeAllSprites(void* this)
{
    /* Frees all album sprites by calling FreeSprites + destroys surfaces
     * for album, stamps, photo, and base areas, then calls base destructor. */
    int i;
    uint8_t* self = (uint8_t*)this;

    *(void***)self = (void*)0x4773F0;  /* reset vtable */

    if (self[0x111]) {
        PostcardAlbum_FreeSprites(this);
    }

    /* Release DDRAW surfaces */
    if (*(void**)(self + 0x138)) {
        (**(void (__stdcall***)(void))(*(void**)(self + 0x138) + 8))();
        *(void**)(self + 0x138) = 0;  /* +0x138: pResource */
    }
    if (*(void**)(self + 0x148)) {
        (*(void (__thiscall**)(void*, uint8_t))(**(void**)(self + 0x148)))(*(void**)(self + 0x148), 1);
        *(void**)(self + 0x148) = 0;
    }
    if (*(void**)(self + 0x14C)) {
        (*(void (__thiscall**)(void*, uint8_t))(**(void**)(self + 0x14C)))(*(void**)(self + 0x14C), 1);
        *(void**)(self + 0x14C) = 0;
    }
    if (*(void**)(self + 0x150)) {
        (*(void (__thiscall**)(void*, uint8_t))(**(void**)(self + 0x150)))(*(void**)(self + 0x150), 1);
        *(void**)(self + 0x150) = 0;
    }
    if (*(void**)(self + 0x154)) {
        (*(void (__thiscall**)(void*, uint8_t))(**(void**)(self + 0x154)))(*(void**)(self + 0x154), 1);
        *(void**)(self + 0x154) = 0;
    }
    if (*(void**)(self + 0x158)) {
        (*(void (__thiscall**)(void*, uint8_t))(**(void**)(self + 0x158)))(*(void**)(self + 0x158), 1);
        *(void**)(self + 0x158) = 0;
    }
    if (*(void**)(self + 0x15C)) {
        (*(void (__thiscall**)(void*, uint8_t))(**(void**)(self + 0x15C)))(*(void**)(self + 0x15C), 1);
        *(void**)(self + 0x15C) = 0;
    }
    if (*(void**)(self + 0x160)) {
        (*(void (__thiscall**)(void*, uint8_t))(**(void**)(self + 0x160)))(*(void**)(self + 0x160), 1);
        *(void**)(self + 0x160) = 0;
    }

    /* Destroy 6 tile area sprite groups */
    for (i = 0; i < 6; i++) {
        if (*(void**)(self + 0x168 + i * 4)) {
            (*(void (__thiscall**)(void*, uint8_t))(**(void**)(self + 0x168 + i * 4)))(*(void**)(self + 0x168 + i * 4), 1);
            *(void**)(self + 0x168 + i * 4) = 0;
        }
        if (*(void**)(self + 0x180 + i * 4)) {
            (*(void (__thiscall**)(void*, uint8_t))(**(void**)(self + 0x180 + i * 4)))(*(void**)(self + 0x180 + i * 4), 1);
            *(void**)(self + 0x180 + i * 4) = 0;
        }
        if (*(void**)(self + 0x198 + i * 4)) {
            (*(void (__thiscall**)(void*, uint8_t))(**(void**)(self + 0x198 + i * 4)))(*(void**)(self + 0x198 + i * 4), 1);
            *(void**)(self + 0x198 + i * 4) = 0;
        }
    }

    /* Destroy 9 thumbnail sprites */
    for (i = 0; i < 9; i++) {
        if (*(void**)(self + 0x1B0 + i * 4)) {
            (*(void (__thiscall**)(void*, uint8_t))(**(void**)(self + 0x1B0 + i * 4)))(*(void**)(self + 0x1B0 + i * 4), 1);
            *(void**)(self + 0x1B0 + i * 4) = 0;
        }
    }

    /* Destroy album scroll-state sprite */
    if (*(void**)(self + 0x164)) {
        (*(void (__thiscall**)(void*, uint8_t))(**(void**)(self + 0x164)))(*(void**)(self + 0x164), 1);
        *(void**)(self + 0x164) = 0;
    }

    /* Destroy stamp resource surface */
    if (*(void**)(self + 0x130)) {
        (*(void (__thiscall**)(void*, uint8_t))(**(void**)(self + 0x130)))(*(void**)(self + 0x130), 1);
        *(void**)(self + 0x130) = 0;
    }

    /* Call base destructor */
    UI_WindowBase_BaseDtor(this);
}

/* ================================================================== */
/* PostcardAlbum_InitWindow                                            */
/* Address: 0x402520                                                   */
/* ================================================================== */
uint8_t __thiscall PostcardAlbum_InitWindow(void* this, void* hWndParent)
{
    /* Creates child window for postcard album, sized to desktop dimensions.
     * Loads icon 0x65, creates full-screen child window via UI_CreateFullWindow. */
    RECT desktop_rect;
    void* hDesk;
    void* hIcon;

    hDesk = GetDesktopWindow();
    GetClientRect(hDesk, &desktop_rect);
    hIcon = LoadIconA(*(void**)((uint8_t*)this + 4), (const char*)0x65);
    *(void**)((uint8_t*)this + 0xE8) = hIcon;

    return UI_CreateFullWindow(
        this, 0, hWndParent,
        desktop_rect.left, desktop_rect.top,
        desktop_rect.right - desktop_rect.left,
        desktop_rect.bottom - desktop_rect.top,
        (void*)0, hIcon, 0);
}

/* ================================================================== */
/* PostcardAlbum_PaintWindow                                           */
/* Address: 0x402690 (496 bytes)                                       */
/* ================================================================== */
int32_t __thiscall PostcardAlbum_PaintWindow(
    void* this, void* hWnd, uint32_t uMsg, void* wParam, void* lParam)
{
    /* Paints postcard album by blitting album, stamps, photo surfaces.
     * Intercepts keyboard shortcuts (0xD=Enter, 0x1B=Escape) to close.
     * Processes scroll left (VK_LEFT=0x25) and right (VK_RIGHT=0x27)
     * by updating page offset and re-rendering tiles. */
    uint32_t entryCount;
    int32_t newOffset;

    if (*(uint8_t*)((uint8_t*)this + 0x110) != 0) {
        return 0;
    }

    switch ((uint32_t)wParam) {
    case 0x0D:  /* Enter key */
    case 0x1B:  /* Escape key */
        (*(void (__thiscall**)(void*))(*(void**)this + 4))(this);  /* cleanup */
        CGWND_SetMode((void*)0x3);  /* return to town mode */
        return 0;

    case 0x25:  /* VK_LEFT — scroll left */
        if (*(uint8_t*)((uint8_t*)this + 0x1D4) != 1) return 0;
        PostcardAlbum_BlitElement(this, (void*)5);
        UIPANEL_EndPaintEx(this, *(void**)((uint8_t*)this + 8), 0, 0, 0);
        Sleep(150);
        PostcardAlbum_UpdateSprite(this, (void*)5);

        if (*(uint8_t*)((uint8_t*)this + 0x1D8) == 1) {
            /* Wrap to previous page */
            newOffset = *(int32_t*)((uint8_t*)this + 0x128) - 1;
            *(int32_t*)((uint8_t*)this + 0x128) = newOffset;
            *(int32_t*)((uint8_t*)this + 0x118) = newOffset;

            entryCount = PixelDataCache_Unlock(_g_dplay_config, newOffset);
            *(int32_t*)((uint8_t*)this + 0x114) = entryCount / *(int32_t*)((uint8_t*)this + 0x11C);

            entryCount = PixelDataCache_GetEntryCount((int32_t)_g_dplay_config);
            if (entryCount % *(int32_t*)((uint8_t*)this + 0x11C) == 0 && entryCount != 0) {
                *(int32_t*)((uint8_t*)this + 0x114) -= 1;
            }
            *(int32_t*)((uint8_t*)this + 0x114) *= *(int32_t*)((uint8_t*)this + 0x11C);
            Sprite_SetState(*(void**)((uint8_t*)this + 0x164),
                           *(int32_t*)((uint8_t*)this + 0x128), 0);
            goto render_tiles;
        }

        newOffset = *(int32_t*)((uint8_t*)this + 0x114) - *(int32_t*)((uint8_t*)this + 0x11C);
        *(int32_t*)((uint8_t*)this + 0x114) = newOffset;
        goto render_tiles;

    case 0x27:  /* VK_RIGHT — scroll right */
        if (*(uint8_t*)((uint8_t*)this + 0x1D5) != 1) return 0;
        PostcardAlbum_BlitElement(this, (void*)6);
        UIPANEL_EndPaintEx(this, *(void**)((uint8_t*)this + 8), 0, 0, 0);
        Sleep(150);
        PostcardAlbum_UpdateSprite(this, (void*)6);

        if (*(uint8_t*)((uint8_t*)this + 0x1D9) == 1) {
            newOffset = *(int32_t*)((uint8_t*)this + 0x128) + 1;
            *(int32_t*)((uint8_t*)this + 0x128) = newOffset;
            *(int32_t*)((uint8_t*)this + 0x118) = newOffset;
            *(int32_t*)((uint8_t*)this + 0x114) = 0;
            Sprite_SetState(*(void**)((uint8_t*)this + 0x164), newOffset, 0);
            goto render_tiles;
        }

        newOffset = *(int32_t*)((uint8_t*)this + 0x114) + *(int32_t*)((uint8_t*)this + 0x11C);
        *(int32_t*)((uint8_t*)this + 0x114) = newOffset;
        /* fall through to render_tiles */

    default:
        return DefWindowProcA(hWnd, uMsg, wParam, lParam);
    }

render_tiles:
    PostcardAlbum_RenderAllTiles(this);
    UIPANEL_EndPaintEx(this, *(void**)((uint8_t*)this + 8), 0, 0, 0);
    return 0;
}

/* ================================================================== */
/* PostcardAlbum_UpdateSprite                                          */
/* Address: 0x403BA0 (250 bytes)                                       */
/* ================================================================== */
void __thiscall PostcardAlbum_UpdateSprite(void* this, int32_t spriteId)
{
    /* Updates postcard album sprite position/visibility based on scroll
     * and selection state. Sets sprite state to 0 (normal) or 2 (disabled). */
    uint8_t* self = (uint8_t*)this;

    switch (spriteId) {
    case 1:  /* title sprite */
        Sprite_SetState(*(void**)(self + 0x148), 0, 0);
        return;
    case 2:  /* stamp sprite */
        if (self[0x1D6] != 1) {
            Sprite_SetState(*(void**)(self + 0x14C), 2, 0);
            return;
        }
        Sprite_SetState(*(void**)(self + 0x14C), 0, 0);
        return;
    case 3:  /* photo sprite */
        Sprite_SetState(*(void**)(self + 0x150), 0, 0);
        return;
    case 4:  /* peruse sprite */
        if (self[0x1D7] != 1) {
            Sprite_SetState(*(void**)(self + 0x154), 2, 0);
            return;
        }
        Sprite_SetState(*(void**)(self + 0x154), 0, 0);
        return;
    case 5:  /* scroll left sprite */
        if (self[0x1D4] != 1) {
            Sprite_SetState(*(void**)(self + 0x15C), 2, 0);
            return;
        }
        Sprite_SetState(*(void**)(self + 0x15C), 0, 0);
        return;
    case 6:  /* scroll right sprite */
        if (self[0x1D5] != 1) {
            Sprite_SetState(*(void**)(self + 0x160), 2, 0);
            return;
        }
        Sprite_SetState(*(void**)(self + 0x160), 0, 0);
        return;
    case 9:  /* done sprite */
        Sprite_SetState(*(void**)(self + 0x158), 0, 0);
        return;
    default:
        return;
    }
}

/* ================================================================== */
/* PostcardAlbum_HitTest                                               */
/* Address: 0x403CD0 (429 bytes)                                       */
/* ================================================================== */
int32_t __thiscall PostcardAlbum_HitTest(void* this, int32_t x, int32_t y)
{
    /* Hit-tests postcard album sprites for click dispatch.
     * Returns sprite ID: 1=title, 2=stamp, 3=photo, 4=peruse,
     * 5=scroll_left, 6=scroll_right, 7=thumbnail, 8=tile_label,
     * 9=done, 10=tile_bg, 0=miss. */
    int i;
    uint16_t u;
    uint32_t pt;
    uint8_t* self = (uint8_t*)this;

    pt = (uint32_t)y << 16 | (uint32_t)(uint16_t)x;

    /* Test main sprites in priority order */
    if (PtInRect(*(void**)(*(void**)(self + 0x148) + 4), pt)) return 1;  /* title */
    if (PtInRect(*(void**)(*(void**)(self + 0x158) + 4), pt)) return 9;  /* done */
    if (PtInRect(*(void**)(*(void**)(self + 0x154) + 4), pt)) return 4;  /* peruse */
    if (PtInRect(*(void**)(*(void**)(self + 0x14C) + 4), pt)) return 2;  /* stamp */
    if (PtInRect(*(void**)(*(void**)(self + 0x150) + 4), pt)) return 3;  /* photo */
    if (PtInRect(*(void**)(*(void**)(self + 0x15C) + 4), pt)) return 5;  /* scroll_left */
    if (PtInRect(*(void**)(*(void**)(self + 0x160) + 4), pt)) return 6;  /* scroll_right */

    /* Test 6 tile background sprites */
    for (i = 0; i < 6; i++) {
        if (PtInRect(*(void**)(*(void**)(self + 0x168 + i * 4) + 4), pt)) {
            *(int32_t*)(self + 0x120) = (uint16_t)i;
            return 8;  /* tile_bg hit */
        }
        if (PtInRect(*(void**)(*(void**)(self + 0x198 + i * 4) + 4), pt)) {
            *(int32_t*)(self + 0x120) = (uint16_t)i;
            return 10; /* tile_label hit */
        }
    }

    /* Test 9 thumbnail sprites */
    for (u = 0; u < 9; u++) {
        if (PtInRect(*(void**)(*(void**)(self + 0x1B0 + u * 4) + 4), pt)) {
            *(int32_t*)(self + 0x120) = (uint16_t)u;
            return 7;  /* thumbnail hit */
        }
    }

    return 0;  /* miss */
}

/* ================================================================== */
/* PostcardAlbum_BlitElement                                           */
/* Address: 0x403E80 (2165 bytes)                                      */
/* ================================================================== */
void __thiscall PostcardAlbum_BlitElement(void* this, void* elementId)
{
    /* Renders a postcard album element onto the window surface:
     *   1=title, 2=stamp, 3=photo, 4=peruse,
     *   5=scroll_left, 6=scroll_right, 7=album_sprite, 9=done
     *
     * Each element: reads source rect from the ButtonSprite, applies scroll
     * offset (+0xD4/+0xD8) to src and viewport offset (+0xEC/+0xF0) to dest,
     * then blits via UIPANEL_Blit. Plays sound 0x5015 on activation.
     * Disabled elements (flag byte != 1) get Sprite_SetState(2) instead. */
    uintptr_t spriteAddr;
    RECT srcRect, srcRect2, dstRect;
    uint8_t hitResult;
    uint8_t* self = (uint8_t*)this;

    switch ((uintptr_t)elementId) {
    case 1:  /* title element */
        PlaySound(0x5015);
        spriteAddr = *(uintptr_t*)(self + 0x148);
        srcRect.left   = *(int32_t*)(spriteAddr + 4);
        srcRect.top    = *(int32_t*)(spriteAddr + 8);
        srcRect.right  = *(int32_t*)(spriteAddr + 0x0C);
        srcRect.bottom = *(int32_t*)(spriteAddr + 0x10);

        if (self[0x111] && self[0x112]) {
            CopyRect(&srcRect2, &srcRect);
            CopyRect(&dstRect, &srcRect);
            OffsetRect(&srcRect2, *(int32_t*)(self + 0xD4), *(int32_t*)(self + 0xD8));
            OffsetRect(&dstRect,  *(int32_t*)(self + 0xEC), *(int32_t*)(self + 0xF0));
            hitResult = UIPANEL_Blit(
                *(void**)(self + 0x13C),
                srcRect2.left, srcRect2.top, srcRect2.right, srcRect2.bottom,
                _g_primary_surface,
                dstRect.left, dstRect.top, dstRect.right, dstRect.bottom, 1);
            if (!hitResult) {
                OutputDebugStringA(s_blit_failure);
            }
        }
        Sprite_SetState(*(void**)(self + 0x148), 1, 0);
        return;

    case 2:  /* stamp element */
        if (self[0x1D6] != 1) {
            Sprite_SetState(*(void**)(self + 0x14C), 2, 0);
            return;
        }
        PlaySound(0x5015);
        spriteAddr = *(uintptr_t*)(self + 0x14C);
        srcRect.left   = *(int32_t*)(spriteAddr + 4);
        srcRect.top    = *(int32_t*)(spriteAddr + 8);
        srcRect.right  = *(int32_t*)(spriteAddr + 0x0C);
        srcRect.bottom = *(int32_t*)(spriteAddr + 0x10);

        if (self[0x111] && self[0x112]) {
            CopyRect(&srcRect2, &srcRect);
            CopyRect(&dstRect, &srcRect);
            OffsetRect(&srcRect2, *(int32_t*)(self + 0xD4), *(int32_t*)(self + 0xD8));
            OffsetRect(&dstRect,  *(int32_t*)(self + 0xEC), *(int32_t*)(self + 0xF0));
            hitResult = UIPANEL_Blit(
                *(void**)(self + 0x13C),
                srcRect2.left, srcRect2.top, srcRect2.right, srcRect2.bottom,
                _g_primary_surface,
                dstRect.left, dstRect.top, dstRect.right, dstRect.bottom, 1);
            if (!hitResult) {
                OutputDebugStringA(s_blit_failure);
            }
        }
        Sprite_SetState(*(void**)(self + 0x14C), 1, 0);
        return;

    case 3:  /* photo element */
        PlaySound(0x5015);
        spriteAddr = *(uintptr_t*)(self + 0x150);
        srcRect.left   = *(int32_t*)(spriteAddr + 4);
        srcRect.top    = *(int32_t*)(spriteAddr + 8);
        srcRect.right  = *(int32_t*)(spriteAddr + 0x0C);
        srcRect.bottom = *(int32_t*)(spriteAddr + 0x10);

        if (self[0x111] && self[0x112]) {
            CopyRect(&srcRect2, &srcRect);
            CopyRect(&dstRect, &srcRect);
            OffsetRect(&srcRect2, *(int32_t*)(self + 0xD4), *(int32_t*)(self + 0xD8));
            OffsetRect(&dstRect,  *(int32_t*)(self + 0xEC), *(int32_t*)(self + 0xF0));
            hitResult = UIPANEL_Blit(
                *(void**)(self + 0x13C),
                srcRect2.left, srcRect2.top, srcRect2.right, srcRect2.bottom,
                _g_primary_surface,
                dstRect.left, dstRect.top, dstRect.right, dstRect.bottom, 1);
            if (!hitResult) {
                OutputDebugStringA(s_blit_failure);
            }
        }
        Sprite_SetState(*(void**)(self + 0x150), 1, 0);
        return;

    case 4:  /* peruse element */
        if (self[0x1D7] != 1) {
            Sprite_SetState(*(void**)(self + 0x154), 2, 0);
            return;
        }
        PlaySound(0x5015);
        spriteAddr = *(uintptr_t*)(self + 0x154);
        srcRect.left   = *(int32_t*)(spriteAddr + 4);
        srcRect.top    = *(int32_t*)(spriteAddr + 8);
        srcRect.right  = *(int32_t*)(spriteAddr + 0x0C);
        srcRect.bottom = *(int32_t*)(spriteAddr + 0x10);

        if (self[0x111] && self[0x112]) {
            CopyRect(&srcRect2, &srcRect);
            CopyRect(&dstRect, &srcRect);
            OffsetRect(&srcRect2, *(int32_t*)(self + 0xD4), *(int32_t*)(self + 0xD8));
            OffsetRect(&dstRect,  *(int32_t*)(self + 0xEC), *(int32_t*)(self + 0xF0));
            hitResult = UIPANEL_Blit(
                *(void**)(self + 0x13C),
                srcRect2.left, srcRect2.top, srcRect2.right, srcRect2.bottom,
                _g_primary_surface,
                dstRect.left, dstRect.top, dstRect.right, dstRect.bottom, 1);
            if (!hitResult) {
                OutputDebugStringA(s_blit_failure);
            }
        }
        Sprite_SetState(*(void**)(self + 0x154), 1, 0);
        return;

    case 5:  /* scroll_left element */
        if (self[0x1D4] != 1) {
            Sprite_SetState(*(void**)(self + 0x15C), 2, 0);
            return;
        }
        PlaySound(0x5015);
        spriteAddr = *(uintptr_t*)(self + 0x15C);
        srcRect.left   = *(int32_t*)(spriteAddr + 4);
        srcRect.top    = *(int32_t*)(spriteAddr + 8);
        srcRect.right  = *(int32_t*)(spriteAddr + 0x0C);
        srcRect.bottom = *(int32_t*)(spriteAddr + 0x10);

        if (self[0x111] && self[0x112]) {
            CopyRect(&srcRect2, &srcRect);
            CopyRect(&dstRect, &srcRect);
            OffsetRect(&srcRect2, *(int32_t*)(self + 0xD4), *(int32_t*)(self + 0xD8));
            OffsetRect(&dstRect,  *(int32_t*)(self + 0xEC), *(int32_t*)(self + 0xF0));
            hitResult = UIPANEL_Blit(
                *(void**)(self + 0x13C),
                srcRect2.left, srcRect2.top, srcRect2.right, srcRect2.bottom,
                _g_primary_surface,
                dstRect.left, dstRect.top, dstRect.right, dstRect.bottom, 1);
            if (!hitResult) {
                OutputDebugStringA(s_blit_failure);
            }
        }
        Sprite_SetState(*(void**)(self + 0x15C), 1, 0);
        return;

    case 6:  /* scroll_right element */
        if (self[0x1D5] != 1) {
            Sprite_SetState(*(void**)(self + 0x160), 2, 0);
            return;
        }
        spriteAddr = *(uintptr_t*)(self + 0x160);
        srcRect.left   = *(int32_t*)(spriteAddr + 4);
        srcRect.top    = *(int32_t*)(spriteAddr + 8);
        srcRect.right  = *(int32_t*)(spriteAddr + 0x0C);
        srcRect.bottom = *(int32_t*)(spriteAddr + 0x10);

        if (self[0x111] && self[0x112]) {
            CopyRect(&srcRect2, &srcRect);
            CopyRect(&dstRect, &srcRect);
            OffsetRect(&srcRect2, *(int32_t*)(self + 0xD4), *(int32_t*)(self + 0xD8));
            OffsetRect(&dstRect,  *(int32_t*)(self + 0xEC), *(int32_t*)(self + 0xF0));
            hitResult = UIPANEL_Blit(
                *(void**)(self + 0x13C),
                srcRect2.left, srcRect2.top, srcRect2.right, srcRect2.bottom,
                _g_primary_surface,
                dstRect.left, dstRect.top, dstRect.right, dstRect.bottom, 1);
            if (!hitResult) {
                OutputDebugStringA(s_blit_failure);
            }
        }
        Sprite_SetState(*(void**)(self + 0x160), 1, 0);
        PlaySound(0x5015);
        return;

    case 7:  /* album_sprite element */
        PlaySound(0x5015);
        Sprite_SetState(*(void**)(self + 0x164),
                       *(int32_t*)(self + 0x128), 0);
        return;

    case 9:  /* done element */
        PlaySound(0x5015);
        spriteAddr = *(uintptr_t*)(self + 0x158);
        srcRect.left   = *(int32_t*)(spriteAddr + 4);
        srcRect.top    = *(int32_t*)(spriteAddr + 8);
        srcRect.right  = *(int32_t*)(spriteAddr + 0x0C);
        srcRect.bottom = *(int32_t*)(spriteAddr + 0x10);

        if (self[0x111] && self[0x112]) {
            CopyRect(&srcRect2, &srcRect);
            CopyRect(&dstRect, &srcRect);
            OffsetRect(&srcRect2, *(int32_t*)(self + 0xD4), *(int32_t*)(self + 0xD8));
            OffsetRect(&dstRect,  *(int32_t*)(self + 0xEC), *(int32_t*)(self + 0xF0));
            hitResult = UIPANEL_Blit(
                *(void**)(self + 0x13C),
                srcRect2.left, srcRect2.top, srcRect2.right, srcRect2.bottom,
                _g_primary_surface,
                dstRect.left, dstRect.top, dstRect.right, dstRect.bottom, 1);
            if (!hitResult) {
                OutputDebugStringA(s_blit_failure);
            }
        }
        Sprite_SetState(*(void**)(self + 0x158), 1, 0);
        return;

    default:
        return;
    }
}

/* ================================================================== */
/* PostcardAlbum_InitWindowSurface                                     */
/* Address: 0x404720                                                   */
/* ================================================================== */
void __fastcall PostcardAlbum_InitWindowSurface(void* this)
{
    /* Creates DirectDraw surface for postcard album window. Loads the
     * album base resource (0x3C0A for lo-res, 0x3C0B for hi-res) and
     * creates the DDRAW surface from it. */
    uint8_t* self = (uint8_t*)this;
    int32_t resId;
    void* pResource;

    if (self[0xFC] == 0) {
        if (*(int32_t*)(self + 0x134) == 0) {
            resId = 0x3C0A;
        } else {
            resId = 0x3C0B;
        }
        pResource = ResourceManager_GetById(&g_resmgr, resId);
        *(void**)(self + 0x138) = pResource;
        *(void**)(self + 0x13C) = (*(void* (__thiscall**)(void*, int32_t))*pResource)(pResource, 0);
        self[0xFC] = 1;
    }
}

/* ================================================================== */
/* PostcardAlbum_InitSprites                                           */
/* Address: 0x404770                                                   */
/* ================================================================== */
void __fastcall PostcardAlbum_InitSprites(void* this)
{
    /* Initializes postcard album child sprites (title, stamp, photo,
     * scroll areas) from resource 0x3CFA via RESDATA_CreateChildSprite.
     * Creates stamp area surface. */
    int i;
    void* pResource;
    uint8_t* self = (uint8_t*)this;

    if (self[0x111] == 0) {
        Sprite_Init(*(void**)(self + 0x148));  /* title */
        Sprite_Init(*(void**)(self + 0x14C));  /* stamp */
        Sprite_Init(*(void**)(self + 0x150));  /* photo */
        Sprite_Init(*(void**)(self + 0x154));  /* peruse */
        Sprite_Init(*(void**)(self + 0x158));  /* done */
        Sprite_Init(*(void**)(self + 0x15C));  /* scroll_left */
        Sprite_Init(*(void**)(self + 0x160));  /* scroll_right */
        Sprite_Init(*(void**)(self + 0x164));  /* album */

        for (i = 0; i < 6; i++) {
            Sprite_Init(*(void**)(self + 0x180 + i * 4));  /* tile_bg sprites */
        }

        pResource = ResourceManager_GetById(&g_resmgr, 0x3CFA);
        *(void**)(self + 0x140) = pResource;
        *(void**)(self + 0x144) = (*(void* (__thiscall**)(void*, int32_t))*pResource)(pResource, 0);
        self[0x111] = 1;
    }
}

/* ================================================================== */
/* PostcardAlbum_FreeSprites                                           */
/* Address: 0x404830                                                   */
/* ================================================================== */
void __fastcall PostcardAlbum_FreeSprites(void* this)
{
    /* Frees postcard album child sprites via Sprite_Destroy on each
     * sub-sprite and releases the stamp area surface. */
    int i;
    uint8_t* self = (uint8_t*)this;

    if (self[0x111] != 0) {
        /* Release stamp resource surface */
        (*(void (__thiscall**)(void*, int32_t))(**(void**)(self + 0x140) + 8))(*(void**)(self + 0x140), 0);
        *(void**)(self + 0x140) = 0;

        /* Destroy 7 main sprites */
        Sprite_Destroy(*(void**)(self + 0x148));
        Sprite_Destroy(*(void**)(self + 0x14C));
        Sprite_Destroy(*(void**)(self + 0x150));
        Sprite_Destroy(*(void**)(self + 0x154));
        Sprite_Destroy(*(void**)(self + 0x158));
        Sprite_Destroy(*(void**)(self + 0x15C));
        Sprite_Destroy(*(void**)(self + 0x160));
        Sprite_Destroy(*(void**)(self + 0x164));

        /* Destroy 6 tile background sprites */
        for (i = 0; i < 6; i++) {
            Sprite_Destroy(*(void**)(self + 0x180 + i * 4));
        }

        self[0x111] = 0;
    }
}

/* ================================================================== */
/* PostcardAlbum_RenderTileName                                        */
/* Address: 0x4048E0 (469 bytes)                                       */
/* ================================================================== */
int32_t __thiscall PostcardAlbum_RenderTileName(void* this, int32_t tileIndex)
{
    /* Renders a single tile name onto the postcard album photo surface.
     * Uses PixelDataCache_LookupAsset to find the building/player name,
     * renders via DPLAY_RenderPlayer, copies the name into the tile name
     * buffer at +0x1DA + tileIndex*0x14, and sets the tile bg sprite.
     * Returns 1 on success, 0 if no asset found. */
    void* assetData;
    uint8_t* self = (uint8_t*)this;
    int32_t spriteAddr;
    RECT srcRect, srcRect2, dstRect;
    uint8_t hitResult;
    const char* name;
    char* nameBuf;
    int32_t len;
    uint8_t* src;

    assetData = PixelDataCache_LookupAsset(
        _g_dplay_config,
        *(int32_t*)(self + 0x114) + tileIndex,
        *(int32_t*)(self + 0x118));

    if (assetData == 0) {
        /* No asset — clear tile name and blit empty background */
        self[tileIndex * 0x14 + 0x1DA] = 0;

        spriteAddr = *(int32_t*)(self + tileIndex * 4 + 0x168);
        srcRect.left   = *(int32_t*)(spriteAddr + 4);
        srcRect.top    = *(int32_t*)(spriteAddr + 8);
        srcRect.right  = *(int32_t*)(spriteAddr + 0x0C);
        srcRect.bottom = *(int32_t*)(spriteAddr + 0x10);

        if (self[0x111] && self[0x112]) {
            CopyRect(&srcRect2, &srcRect);
            CopyRect(&dstRect, &srcRect);
            OffsetRect(&srcRect2, *(int32_t*)(self + 0xD4), *(int32_t*)(self + 0xD8));
            OffsetRect(&dstRect,  *(int32_t*)(self + 0xEC), *(int32_t*)(self + 0xF0));
            hitResult = UIPANEL_Blit(
                *(void**)(self + 0x13C),
                srcRect2.left, srcRect2.top, srcRect2.right, srcRect2.bottom,
                _g_primary_surface,
                dstRect.left, dstRect.top, dstRect.right, dstRect.bottom, 1);
            if (!hitResult) {
                OutputDebugStringA(s_blit_failure);
            }
        }
        return 0;
    }

    /* Render player data onto the tile area */
    spriteAddr = *(int32_t*)(self + tileIndex * 4 + 0x168);
    DPLAY_RenderPlayer(
        _g_dplay, (void*)(uintptr_t)*(int32_t*)(spriteAddr + 0x0C), (int32_t)assetData,
        _g_primary_surface,
        *(int32_t*)(spriteAddr + 4), *(int32_t*)(spriteAddr + 8),
        *(int32_t*)(spriteAddr + 0x0C), *(void**)(spriteAddr + 0x10));

    /* Copy name from asset (+0x25 offset) to tile name buffer */
    name = (const char*)((uint8_t*)assetData + 0x25);
    nameBuf = (char*)(self + tileIndex * 0x14 + 0x1DA);
    len = 0;
    while (name[len] != '\0' && len < 19) {
        len++;
    }
    for (int j = 0; j <= len; j++) {
        nameBuf[j] = name[j];
    }

    /* Release asset reference (vtable[0] scalar dtor) */
    (*(void (__thiscall**)(void*, uint8_t))*assetData)(assetData, 1);

    /* Set tile bg sprite to state 0 (normal) */
    Sprite_SetState(*(void**)(self + tileIndex * 4 + 0x180), 0, 0);
    return 1;
}

/* ================================================================== */
/* PostcardAlbum_RenderAllTiles                                        */
/* Address: 0x404AC0 (747 bytes)                                       */
/* ================================================================== */
void __fastcall PostcardAlbum_RenderAllTiles(void* this)
{
    /* Renders all tile names/descriptions onto the postcard album photo
     * surface by iterating the entries_per_page count and calling
     * RenderTileName for each. Then renders text labels via GDI
     * DrawTextA using g_font_small. Updates scroll button states. */
    int32_t i;
    uint8_t* self = (uint8_t*)this;
    void* hdc;
    uint32_t prevColor;
    int32_t prevMode;
    void* prevFont;
    uint32_t entryCount;

    /* Phase 1: Blit all tiles */
    if (*(int32_t*)(self + 0x11C) != 0) {
        for (i = 0; i < *(int32_t*)(self + 0x11C); i++) {
            PostcardAlbum_RenderTileName(this, i);

            /* Blit tile background */
            int32_t labelAddr = *(int32_t*)(self + i * 4 + 0x198);
            RECT labelRect;
            labelRect.left   = *(int32_t*)(labelAddr + 4);
            labelRect.top    = *(int32_t*)(labelAddr + 8);
            labelRect.right  = *(int32_t*)(labelAddr + 0x0C);
            labelRect.bottom = *(int32_t*)(labelAddr + 0x10);

            if (self[0x111] && self[0x112]) {
                RECT srcRect, dstRect;
                CopyRect(&srcRect, &labelRect);
                CopyRect(&dstRect, &labelRect);
                OffsetRect(&srcRect, *(int32_t*)(self + 0xD4), *(int32_t*)(self + 0xD8));
                OffsetRect(&dstRect, *(int32_t*)(self + 0xEC), *(int32_t*)(self + 0xF0));
                if (!UIPANEL_Blit(
                        *(void**)(self + 0x13C),
                        srcRect.left, srcRect.top, srcRect.right, srcRect.bottom,
                        _g_primary_surface,
                        dstRect.left, dstRect.top, dstRect.right, dstRect.bottom, 1))
                {
                    OutputDebugStringA(s_blit_failure);
                }
            }
        }
    }

    /* Phase 2: Render text labels via GDI */
    hdc = UIPANEL_BeginPaint((int32_t)this);
    if (*(int32_t*)(self + 0x11C) != 0) {
        const char* tileText = (const char*)(self + 0x1DA);
        for (i = 0; i < *(int32_t*)(self + 0x11C); i++) {
            if (self[0x12C] == 1) {  /* visible flag */
                int32_t* labelAddr;
                RECT textRect;

                labelAddr = (int32_t*)(self + i * 4 + 0x198);

                SetBkMode(hdc, 1);  /* TRANSPARENT */
                prevColor = SetTextColor(hdc, 0);  /* black */
                prevMode = SetBkMode(hdc, 1);
                prevFont = SelectObject(hdc, g_font_small);

                textRect.left   = labelAddr[1];
                textRect.top    = labelAddr[2];
                textRect.right  = labelAddr[3];
                textRect.bottom = labelAddr[4];
                DrawTextA(hdc, tileText + i * 0x14, -1, &textRect, 0x25);

                SelectObject(hdc, prevFont);
                SetTextColor(hdc, prevColor);
                SetBkMode(hdc, prevMode);
            }
        }
    }

    UIPANEL_EndPaintEx(this, *(void**)(self + 8), (int32_t)hdc, 1, 0);

    /* Phase 3: Update scroll button states */
    entryCount = PixelDataCache_GetEntryCount((int32_t)_g_dplay_config);

    if (*(int32_t*)(self + 0x114) == 0) {
        /* At first page */
        self[0x1D8] = 1;  /* scroll_left at_first_page flag */
        if (*(int32_t*)(self + 0x118) != 0) goto check_right;
        if (self[0x1D4] != 1) goto done;
        self[0x1D4] = 0;  /* disable scroll_left */
    } else {
        self[0x1D8] = 0;  /* not at first page */
check_right:
        if (self[0x1D4] != 0) goto check_right2;
        self[0x1D4] = 1;  /* enable scroll_left */
    }
    PostcardAlbum_UpdateSprite(this, (void*)5);

check_right2:
    if (*(int32_t*)(self + 0x114) + *(int32_t*)(self + 0x11C) < (int32_t)entryCount) {
        self[0x1D9] = 0;  /* not at last page */
    } else {
        self[0x1D9] = 1;  /* at last page */
        if (*(int32_t*)(self + 0x118) > 7) {
            if (self[0x1D5] != 1) goto done;
            self[0x1D5] = 0;  /* disable scroll_right */
            PostcardAlbum_UpdateSprite(this, (void*)6);
            goto done;
        }
    }

    if (self[0x1D5] == 0) {
        self[0x1D5] = 1;  /* enable scroll_right */
        PostcardAlbum_UpdateSprite(this, (void*)6);
    }

done:
    return;
}
