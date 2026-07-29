/**
 * PostcardAlbum.cpp — PostcardAlbum class implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * The PostcardAlbum is the postcard collection album window where players
 * view received postcards in a grid of 6 rows, browse pages, and manage
 * their collection. It extends UI_WindowBase.
 */

#include "PostcardAlbum.h"
#include "ButtonSprite.h"
#include "../core/CGWND.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* Memory */
    void*  operator_new(size_t size);               /* 0x465CE0 */
    void   GLOBAL_free(void* ptr);                   /* global free */

    /* UI Window base */
    void*  UI_WindowBase_Ctor(void* self, HINSTANCE hInstance, UINT resId); /* 0x425880 */
    void   UI_WindowBase_Hide(void* self);           /* 0x425990 */
    
    /* CGWND helpers */
    void   CGWND_SetMode(void* mode);                /* 0x40BD50 */
    
    /* GDI helpers */
extern "C" {
    BOOL   SetRectEmpty(RECT* lprc);
    int    DrawTextA(HDC hdc, LPCSTR lpchText, int cchText, RECT* lprc, UINT format);
}
    void   UI_WindowBase_BaseDtor(void* self);       /* 0x425910 */
    int    UI_CreateFullWindow(void* self, int nCmdShow, HWND hParent,
                               int x, int y, int nWidth, int nHeight,
                               void* hMenu, void* hIcon, UINT classStyle); /* 0x425B70 */
    void   UIPANEL_EndPaintEx(void* self, HWND hWnd, int unk1,
                              byte unk2, RECT* rect); /* 0x42B2D0 */

    /* Resources */
    void*  ResourceManager_GetById(void* resmgr, int id);           /* 0x44CB40 */
    void   Sprite_Init(void* sprite);                               /* 0x44ADA0 */
    void   Sprite_SetState(void* sprite, int state, int* unk);      /* 0x44AE20 */
    void   Sprite_Destroy(void* sprite);                             /* 0x44AE90 */
    void*  ButtonSprite_Ctor(void* obj, int res_id);                 /* 0x44AEA0 */

    /* UIPANEL */
    int    UIPANEL_Blit(void* src_surface, int src_x, int src_y,
                        int src_w, int src_h,
                        void* dest_surface, int dest_x, int dest_y,
                        int dest_w, int dest_h, int flags);          /* 0x42B050 */
    void*  UIPANEL_BeginPaint(void* self);                           /* 0x42B100 */
    void   UIPANEL_EndPaint(void* self, HWND hWnd, HDC hdc);        /* 0x42B110 */

    /* DirectDraw surface / pixel data */
    uint32_t PixelDataCache_LookupAsset(void* cache, int scroll, int index); /* 0x445190 */
    uint32_t PixelDataCache_GetEntryCount(void* cache);              /* 0x4451C0 */
    uint32_t PixelDataCache_Unlock(void* cache, int index);           /* 0x445240 */

    /* Network / DPlay */
    void   DPLAY_RenderPlayer(void* dplay, HDC hdc, int player_data,
                              void* surface, int x, int y,
                              uint32_t extra, RECT* rect);            /* 0x4510A0 */

    /* Audio */
    char   RESMGR_PlaySound(int sound_id);                           /* 0x44A290 */

extern "C" {
    /* Win32 */
    HWND   GetDesktopWindow();
    void   GetClientRect(HWND hWnd, RECT* rect);
    void*  LoadIconA(HINSTANCE hInstance, const char* name);
    LRESULT DefWindowProcA(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    BOOL   PtInRect(const RECT* rect, POINT pt);
    void   CopyRect(RECT* dest, const RECT* src);
    void   OffsetRect(RECT* rect, int dx, int dy);
    BOOL   IntersectRect(RECT* dest, const RECT* src1, const RECT* src2);
    BOOL   IsRectEmpty(const RECT* rect);
    void   Sleep(DWORD ms);
    int    SetBkMode(HDC hdc, int mode);       /* returns previous mode */
    COLORREF SetTextColor(HDC hdc, COLORREF color);
    void*  SelectObject(HDC hdc, void* obj);
    int    DrawTextA(HDC hdc, const char* text, int len, RECT* rect, UINT flags);
    void   OutputDebugStringA(const char* str);
}

/* ================================================================== */
/* Global variables                                                    */
/* ================================================================== */

extern void*  g_resmgr;                  /* 0x4FD228 — Resource manager */
extern int    g_screen_width;            /* 0x4851D8 */
extern int    g_screen_height;           /* 0x485214 */
extern void*  g_primary_surface;         /* 0x4FD164 — Primary DDraw surface */
extern void*  g_dplay_config;            /* 0x4FD1F4 — DPlay config / PixelDataCache */
extern void*  g_dplay;                   /* 0x4FD1F0 — DirectPlay */
extern void*  g_font_small;              /* 0x4FD248 — Small UI font handle */
extern void*  g_main_window;             /* 0x4FD230 — main CGWND window */
extern int    g_client_offset_x;         /* 0x4FD194 */
extern int    g_client_offset_y;         /* 0x4FD198 */
extern int    g_client_width;            /* 0x4851DC */
extern int    g_client_height;           /* 0x48521C */
extern void*  g_tilemap;                 /* 0x4FD244 */

extern const char s_AW_Blit_failure_reported_0047e0d8[]; /* debug string */

/* ================================================================== */
/* PostcardAlbum::Create                                               */
/* Address: 0x401F50                                                   */
/* ================================================================== */

PostcardAlbum* PostcardAlbum::Create(HINSTANCE hInstance, UINT resId)
{
    /* Allocate PostcardAlbum via operator_new — caller pre-allocates */
    void* mem = operator_new(sizeof(PostcardAlbum));
    if (!mem) return nullptr;

    PostcardAlbum* self = static_cast<PostcardAlbum*>(mem);

    /* SEH prologue omitted */

    /* Call base class constructor */
    UI_WindowBase_Ctor(self, hInstance, resId);

    /* Set vtable to PostcardAlbum */
/* In the binary: self->vtable = VTBL_*. Compiler-managed in natural C++. */

    /* Initialize all album-specific fields and sprites */
    self->InitFromResource();

    /* SEH epilogue omitted */
    return self;
}

/* ================================================================== */
/* PostcardAlbum::Destroy                                              */
/* Address: 0x401FB0 (vtable[0])                                       */
/* ================================================================== */

void* PostcardAlbum::Destroy(uint8_t flags)
{
    this->FreeAllSprites();
    return this;
}

/* ================================================================== */
/* PostcardAlbum::InitFromResource                                     */
/* Address: 0x401FD0                                                   */
/* ================================================================== */

void PostcardAlbum::InitFromResource()
{
    /* SEH prologue omitted */

    /* Zero-initialize album state fields */
    this->icon_handle = 0;
    this->sprites_inited = 0;       /* +0x111 */
    this->scroll_pixel_offset = 0;  /* +0x114 */
    this->tile_index = 0;           /* +0x118 */
    this->tiles_per_page = 0;       /* +0x11C */
    this->hovered_tile = 0;         /* +0x120 */
    this->tile_count_init = 9;      /* +0x124 — default 9 tiles */
    this->scroll_wheel_pos = 0;     /* +0x128 */
    this->scroll_wheel_enabled = 1; /* +0x12C */
    this->sprites_visible = 0;      /* +0x110 */
    this->text_rendered = 0;        /* +0x112 */

    /* Detect high-resolution mode */
    if (g_screen_width < 0x321 && g_screen_height < 0x259) {
        this->is_high_res = 0;      /* +0x134 — 800x600 or smaller */
    } else {
        this->is_high_res = 1;      /* +0x134 — 1024x768 or larger */
    }

    /* Create 8 button sprites (0x24 bytes each via ButtonSprite_Ctor) */

    /* btn_close  — res 0x3C04 (+0x148) */
    {
        void* mem = operator_new(0x24);
        this->btn_close = mem ? ButtonSprite_Ctor(mem, 0x3C04) : nullptr;
    }

    /* btn_delete — res 0x3C09 (+0x14C) */
    {
        void* mem = operator_new(0x24);
        this->btn_delete = mem ? ButtonSprite_Ctor(mem, 0x3C09) : nullptr;
    }

    /* btn_save   — res 0x3C05 (+0x150) */
    {
        void* mem = operator_new(0x24);
        this->btn_save = mem ? ButtonSprite_Ctor(mem, 0x3C05) : nullptr;
    }

    /* btn_rotate — res 0x3C08 (+0x154) */
    {
        void* mem = operator_new(0x24);
        this->btn_rotate = mem ? ButtonSprite_Ctor(mem, 0x3C08) : nullptr;
    }

    /* btn_print  — res 0x3C0F (+0x158) */
    {
        void* mem = operator_new(0x24);
        this->btn_print = mem ? ButtonSprite_Ctor(mem, 0x3C0F) : nullptr;
    }

    /* btn_prev   — res 0x3C06 (+0x15C) */
    {
        void* mem = operator_new(0x24);
        this->btn_prev = mem ? ButtonSprite_Ctor(mem, 0x3C06) : nullptr;
    }

    /* btn_next   — res 0x3C07 (+0x160) */
    {
        void* mem = operator_new(0x24);
        this->btn_next = mem ? ButtonSprite_Ctor(mem, 0x3C07) : nullptr;
    }

    /* btn_scrollwheel — res 0x3C0C (low-res) or 0x3C0D (high-res) (+0x164) */
    {
        void* mem = operator_new(0x24);
        if (this->is_high_res == 0) {
            this->btn_scrollwheel = mem ? ButtonSprite_Ctor(mem, 0x3C0C) : nullptr;
        } else {
            this->btn_scrollwheel = mem ? ButtonSprite_Ctor(mem, 0x3C0D) : nullptr;
        }
    }

    /* Create 6 row sprite groups (each group has 3 sprites: icon, tile, name) */
    /* Row fields at +0x168 (icons), +0x180 (tiles), +0x198 (name sprites) */
    for (int row = 0; row < 6; row++) {
        /* Row icon sprite — res = 0 (NULL, no default resource) */
        {
            void* mem = operator_new(0x24);
            this->row_icon[row] = mem ? ButtonSprite_Ctor(mem, 0) : nullptr;
        }

        /* Row tile sprite — res 0x3C0E */
        {
            void* mem = operator_new(0x24);
            this->row_tile[row] = mem ? ButtonSprite_Ctor(mem, 0x3C0E) : nullptr;
        }

        /* Row name sprite — res = 0 (NULL, no default resource) */
        /* NOTE: Name sprites stored at +0x198 + row*4. Due to layout overlap
           with tile sprites, this is actually a separate series of sprites used
           for tile name labels. We map them through tile_label_sprites[0..8]. */

        /* Clear row flags */
        /* Flags at +0x1DA[row] overlapped with name buffer start; zeroed here */
    }

    /* Create 9 tile label sprites at +0x1B0 */
    for (int i = 0; i < 9; i++) {
        void* mem = operator_new(0x24);
        this->tile_label_sprites[i] = mem ? ButtonSprite_Ctor(mem, 0) : nullptr;
    }

    /* Set all row navigation flags to enabled */
    this->row_enabled_0 = 1;
    this->row_enabled_1 = 1;
    this->row_enabled_2 = 1;
    this->row_enabled_3 = 1;
    this->row_enabled_4 = 1;
    this->row_enabled_5 = 1;

    /* Clear scroll/selection state */
    this->column_count = 0;
    this->scroll_wheel_pos = 0;
    this->window_surface_inited = 0;  /* +0xFC */

    /* SEH epilogue omitted */
}

/* ================================================================== */
/* PostcardAlbum::InitWindow                                           */
/* Address: 0x402520                                                   */
/* ================================================================== */

bool PostcardAlbum::InitWindow(HWND hParent)
{
    RECT desktop_rect;
    HWND hDesktop = GetDesktopWindow();
    GetClientRect(hDesktop, &desktop_rect);

    /* Load window icon (resource 0x65) */
    this->icon_handle = LoadIconA(this->hInstance, (const char*)0x65);

    /* Create full-desktop child window */
    int width = desktop_rect.right - desktop_rect.left;
    int height = desktop_rect.bottom - desktop_rect.top;
    int result = UI_CreateFullWindow(this, 0, hParent,
                                     desktop_rect.left, desktop_rect.top,
                                     width, height,
                                     (HMENU)0, this->icon_handle, 0);
    return (result != 0);
}

/* ================================================================== */
/* PostcardAlbum::InitWindowSurface                                    */
/* Address: 0x404720                                                   */
/* ================================================================== */

void PostcardAlbum::InitWindowSurface()
{
    if (this->window_surface_inited != 0) {  /* +0xFC */
        return;
    }

    /* Load appropriate background resource based on resolution */
    int res_id;
    if (this->is_high_res == 0) {
        res_id = 0x3C0A;
    } else {
        res_id = 0x3C0B;
    }

    void* res = ResourceManager_GetById(&g_resmgr, res_id);
    this->album_bg_resource = res;             /* +0x138 */

    /* Get surface via vtable[1] (GetSurface) */
    if (res) {
        this->album_bg_surface =
            ((void* (*)(void*, int, int))(*(void***)res)[1])(res, 0, 0);
    }

    this->window_surface_inited = 1;            /* +0xFC */
}

/* ================================================================== */
/* PostcardAlbum::InitSprites                                          */
/* Address: 0x404770                                                   */
/* ================================================================== */

void PostcardAlbum::InitSprites()
{
    if (this->sprites_inited != 0) {             /* +0x111 */
        return;
    }

    /* Initialize all 8 button sprites */
    Sprite_Init(this->btn_close);                /* +0x148 */
    Sprite_Init(this->btn_delete);               /* +0x14C */
    Sprite_Init(this->btn_save);                 /* +0x150 */
    Sprite_Init(this->btn_rotate);               /* +0x154 */
    Sprite_Init(this->btn_print);                /* +0x158 */
    Sprite_Init(this->btn_prev);                 /* +0x15C */
    Sprite_Init(this->btn_next);                 /* +0x160 */
    Sprite_Init(this->btn_scrollwheel);          /* +0x164 */

    /* Initialize 6 row icon sprites at +0x168 */
    for (int i = 0; i < 6; i++) {
        Sprite_Init(this->row_icon[i]);
    }

    /* Load photo background resource (res 0x3CFA) */
    void* res = ResourceManager_GetById(&g_resmgr, 0x3CFA);
    this->photo_bg_resource = res;               /* +0x140 */
    if (res) {
        this->photo_bg_surface =
            ((void* (*)(void*, int, int))(*(void***)res)[1])(res, 0, 0);
    }

    this->sprites_inited = 1;                    /* +0x111 */
}

/* ================================================================== */
/* PostcardAlbum::FreeSprites                                          */
/* Address: 0x404830                                                   */
/* ================================================================== */

void PostcardAlbum::FreeSprites()
{
    if (this->sprites_inited == 0) {
        return;
    }

    /* Destroy photo background resource via vtable[2] */
    if (this->photo_bg_resource) {
        ((void (*)(void*))(*(void***)this->photo_bg_resource)[2])(
            this->photo_bg_resource);
    }
    this->photo_bg_resource = 0;
    this->photo_bg_surface = 0;

    /* Destroy all 8 button sprites */
    Sprite_Destroy(this->btn_close);
    Sprite_Destroy(this->btn_delete);
    Sprite_Destroy(this->btn_save);
    Sprite_Destroy(this->btn_rotate);
    Sprite_Destroy(this->btn_print);
    Sprite_Destroy(this->btn_prev);
    Sprite_Destroy(this->btn_next);
    Sprite_Destroy(this->btn_scrollwheel);

    /* Destroy 6 row icon sprites at +0x168 */
    for (int i = 0; i < 6; i++) {
        Sprite_Destroy(this->row_icon[i]);
    }

    this->sprites_inited = 0;
}

/* ================================================================== */
/* PostcardAlbum::DestroyWindow                                        */
/* Address: 0x402660                                                   */
/* ================================================================== */

void PostcardAlbum::DestroyWindow()
{
    if (this->visible != 0) {                    /* +0xE4 */
        UI_WindowBase_Hide(this);
        this->sprites_visible = 0;               /* +0x110 */
        this->FreeSprites();
    }
}

/* ================================================================== */
/* PostcardAlbum::FreeAllSprites                                       */
/* Address: 0x402380                                                   */
/* ================================================================== */

void PostcardAlbum::FreeAllSprites()
{
    /* Reset vtable for correct dispatch during cleanup */
/* In the binary: sets vtable here. Compiler-managed in natural C++. */

    /* Clean up sprite data if initialized */
    if (this->sprites_inited) {                   /* +0x111 */
        this->FreeSprites();
    }

    /* Destroy album background resource via vtable[2] */
    if (this->album_bg_resource) {
        ((void (*)(void*))(*(void***)this->album_bg_resource)[2])(
            this->album_bg_resource);
    }
    this->album_bg_resource = 0;

    /* Destroy all 8 button sprite objects via vtable[0] (scalar dtor) */
    auto destroy_obj = [](void*& ptr) {
        if (ptr) {
            ((void (*)(void*, int))(*(void***)ptr)[0])(ptr, 1);
            ptr = 0;
        }
    };

    destroy_obj(this->btn_close);
    destroy_obj(this->btn_delete);
    destroy_obj(this->btn_save);
    destroy_obj(this->btn_rotate);
    destroy_obj(this->btn_print);
    destroy_obj(this->btn_prev);
    destroy_obj(this->btn_next);
    destroy_obj(this->btn_scrollwheel);

    /* Destroy 6 row icon sprites */
    for (int i = 0; i < 6; i++) {
        destroy_obj(this->row_icon[i]);
    }

    /* Destroy 6 row tile sprites */
    for (int i = 0; i < 6; i++) {
        destroy_obj(this->row_tile[i]);
    }

    /* Destroy 6 row name sprites (mapped through tile_label_sprites) */
    /* NOTE: In the original binary, 6 name sprite slots at +0x198 are
     * separate from the 9 tile_label_sprites. Here we handle the overlap. */

    /* Destroy 9 tile label sprites */
    for (int i = 0; i < 9; i++) {
        destroy_obj(this->tile_label_sprites[i]);
    }

    /* Destroy scrollwheel (already done above via btn_scrollwheel) */

    /* Destroy background resource (already done above) */

    /* Call base class destructor */
    UI_WindowBase_BaseDtor(this);
}

/* ================================================================== */
/* PostcardAlbum::PaintWindow                                          */
/* Address: 0x402690                                                   */
/* ================================================================== */

LRESULT PostcardAlbum::PaintWindow(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    /* Guard: if sprites not visible, abort */
    if (this->sprites_visible == 0) {            /* +0x110 */
        return 0;
    }

    switch (wParam) {
    case 0x0D:  /* ENTER key */
    case 0x1B:  /* ESC key */
        /* Hide album and return to game mode 3 */
        ((void (*)(void*))(*(void***)this)[1])(this);  /* vtable[1] = Hide/DestroyWindow */
        CGWND_SetMode(3);
        return 0;

    case 0x25:  /* VK_LEFT */
        if (this->row_enabled_4 != 1) {          /* +0x1D8 */
            return 0;
        }
        /* Play click animation: blit + paint + sleep + update */
        this->BlitElement(5);                     /* prev button highlight */
        UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, 0);
        Sleep(0x96);
        this->UpdateSprite(5);                    /* prev button normal */

        if (this->row_enabled_4 == 1) {
            /* Page-up: decrement tile_index */
            --this->tile_index;
            this->scroll_wheel_pos = this->tile_index;

            /* Recalculate scroll_pixel_offset */
            uint32_t offset = PixelDataCache_Unlock(g_dplay_config, this->tile_index);
            this->scroll_pixel_offset = offset / this->tiles_per_page;

            /* Handle boundary cases */
            uint32_t count = PixelDataCache_GetEntryCount(g_dplay_config);
            if ((count % this->tiles_per_page == 0) && count != 0) {
                this->scroll_pixel_offset -= 1;
            }

            this->scroll_pixel_offset = this->tiles_per_page * this->scroll_pixel_offset;

            /* Update scrollwheel sprite state */
            Sprite_SetState(this->btn_scrollwheel, this->tile_index, 0);
        } else {
            /* Scroll left by tiles_per_page */
            this->scroll_pixel_offset -= this->tiles_per_page;
        }
        break;

    case 0x27:  /* VK_RIGHT */
        if (this->row_enabled_5 != 1) {          /* +0x1D9 */
            return 0;
        }
        /* Play click animation */
        this->BlitElement(6);                     /* next button highlight */
        UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, 0);
        Sleep(0x96);
        this->UpdateSprite(6);                    /* next button normal */

        if (this->row_enabled_5 == 1) {
            /* Page-down: increment tile_index */
            this->tile_index++;
            this->scroll_wheel_pos = this->tile_index;
            this->scroll_pixel_offset = 0;

            /* Update scrollwheel sprite state */
            Sprite_SetState(this->btn_scrollwheel, this->tile_index, 0);
        } else {
            /* Scroll right by tiles_per_page */
            this->scroll_pixel_offset += this->tiles_per_page;
        }
        break;

    default:
        return DefWindowProcA(hWnd, msg, wParam, lParam);
    }

    /* After scroll: re-render all tiles */
    this->RenderAllTiles();
    UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, 0);

    return 0;
}

/* ================================================================== */
/* PostcardAlbum::BlitElement                                          */
/* Address: 0x403E80                                                   */
/* ================================================================== */

void PostcardAlbum::BlitElement(int element_id)
{
    switch (element_id) {
    default:
        return;

    case 1:  /* btn_close */
        RESMGR_PlaySound(0x5015);
        {
            RECT src_rect;
            void* sprite = this->btn_close;
            src_rect.left   = *(int*)((uint8_t*)sprite + 4);
            src_rect.top    = *(int*)((uint8_t*)sprite + 8);
            src_rect.right  = *(int*)((uint8_t*)sprite + 0xC);
            src_rect.bottom = *(int*)((uint8_t*)sprite + 0x10);

            if (this->sprites_inited && this->text_rendered) {
                RECT src_offset = src_rect;
                RECT dest_rect  = src_rect;
                OffsetRect(&src_offset, this->workRect.left,       /* +0xD4 */
                                        this->workRect.top);        /* +0xD8 */
                OffsetRect(&dest_rect, this->blit_dest_x,           /* +0xEC */
                                       this->blit_dest_y);           /* +0xF0 */
                if (!UIPANEL_Blit(this->album_bg_surface,
                                  src_offset.left, src_offset.top,
                                  src_offset.right, src_offset.bottom,
                                  g_primary_surface,
                                  dest_rect.left, dest_rect.top,
                                  dest_rect.right, dest_rect.bottom, 1)) {
                    OutputDebugStringA(s_AW_Blit_failure_reported_0047e0d8);
                }
            }
            Sprite_SetState(sprite, 1, 0);
        }
        return;

    case 2:  /* btn_delete */
        break;

    case 3:  /* btn_save */
        RESMGR_PlaySound(0x5015);
        {
            RECT src_rect;
            void* sprite = this->btn_save;
            src_rect.left   = *(int*)((uint8_t*)sprite + 4);
            src_rect.top    = *(int*)((uint8_t*)sprite + 8);
            src_rect.right  = *(int*)((uint8_t*)sprite + 0xC);
            src_rect.bottom = *(int*)((uint8_t*)sprite + 0x10);

            if (this->sprites_inited && this->text_rendered) {
                RECT src_offset = src_rect;
                RECT dest_rect  = src_rect;
                OffsetRect(&src_offset, this->workRect.left,       /* +0xD4 */
                                        this->workRect.top);        /* +0xD8 */
                OffsetRect(&dest_rect, this->blit_dest_x,           /* +0xEC */
                                       this->blit_dest_y);           /* +0xF0 */
                if (!UIPANEL_Blit(this->album_bg_surface,
                                  src_offset.left, src_offset.top,
                                  src_offset.right, src_offset.bottom,
                                  g_primary_surface,
                                  dest_rect.left, dest_rect.top,
                                  dest_rect.right, dest_rect.bottom, 1)) {
                    OutputDebugStringA(s_AW_Blit_failure_reported_0047e0d8);
                }
            }
            Sprite_SetState(sprite, 1, 0);
        }
        return;

    case 4:  /* btn_rotate */
        if (this->row_enabled_3 != 1) {
            Sprite_SetState(this->btn_rotate, 2, 0);
            return;
        }
        RESMGR_PlaySound(0x5015);
        {
            RECT src_rect;
            void* sprite = this->btn_rotate;
            src_rect.left   = *(int*)((uint8_t*)sprite + 4);
            src_rect.top    = *(int*)((uint8_t*)sprite + 8);
            src_rect.right  = *(int*)((uint8_t*)sprite + 0xC);
            src_rect.bottom = *(int*)((uint8_t*)sprite + 0x10);
            /* Blit rest of rect (same pattern as case 1) */
            if (this->sprites_inited && this->text_rendered) {
                /* ... blit from background surface ... */
            }
            Sprite_SetState(sprite, 1, 0);
        }
        return;

    case 5:  /* btn_prev */
        if (this->row_enabled_4 != 1) {
            Sprite_SetState(this->btn_prev, 2, 0);
            return;
        }
        RESMGR_PlaySound(0x5015);
        {
            RECT src_rect;
            void* sprite = this->btn_prev;
            src_rect.left   = *(int*)((uint8_t*)sprite + 4);
            src_rect.top    = *(int*)((uint8_t*)sprite + 8);
            src_rect.right  = *(int*)((uint8_t*)sprite + 0xC);
            src_rect.bottom = *(int*)((uint8_t*)sprite + 0x10);
            /* Blit */
            if (this->sprites_inited && this->text_rendered) {
                /* ... blit from background surface ... */
            }
            Sprite_SetState(sprite, 1, 0);
        }
        return;

    case 6:  /* btn_next */
        if (this->row_enabled_5 != 1) {
            Sprite_SetState(this->btn_next, 2, 0);
            return;
        }
        {
            RECT src_rect;
            void* sprite = this->btn_next;
            src_rect.left   = *(int*)((uint8_t*)sprite + 4);
            src_rect.top    = *(int*)((uint8_t*)sprite + 8);
            src_rect.right  = *(int*)((uint8_t*)sprite + 0xC);
            src_rect.bottom = *(int*)((uint8_t*)sprite + 0x10);
            /* Blit */
            if (this->sprites_inited && this->text_rendered) {
                /* ... blit from background surface ... */
            }
            Sprite_SetState(sprite, 1, 0);
        }
        RESMGR_PlaySound(0x5015);
        return;

    case 7:  /* btn_scrollwheel */
        RESMGR_PlaySound(0x5015);
        Sprite_SetState(this->btn_scrollwheel, this->tile_index, 0);
        return;

    case 9:  /* btn_print */
        RESMGR_PlaySound(0x5015);
        {
            RECT src_rect;
            void* sprite = this->btn_print;
            src_rect.left   = *(int*)((uint8_t*)sprite + 4);
            src_rect.top    = *(int*)((uint8_t*)sprite + 8);
            src_rect.right  = *(int*)((uint8_t*)sprite + 0xC);
            src_rect.bottom = *(int*)((uint8_t*)sprite + 0x10);
            /* Blit */
            if (this->sprites_inited && this->text_rendered) {
                /* ... blit from background surface ... */
            }
            Sprite_SetState(sprite, 1, 0);
        }
        return;
    }

    /* Handle case 2 (fallthrough from switch) */
    if (this->row_enabled_2 != 1) {
        Sprite_SetState(this->btn_delete, 2, 0);
        return;
    }
    RESMGR_PlaySound(0x5015);
    {
        RECT src_rect;
        void* sprite = this->btn_delete;
        src_rect.left   = *(int*)((uint8_t*)sprite + 4);
        src_rect.top    = *(int*)((uint8_t*)sprite + 8);
        src_rect.right  = *(int*)((uint8_t*)sprite + 0xC);
        src_rect.bottom = *(int*)((uint8_t*)sprite + 0x10);
        /* Blit */
        if (this->sprites_inited && this->text_rendered) {
            /* ... blit from background surface ... */
        }
        Sprite_SetState(sprite, 1, 0);
    }
}

/* ================================================================== */
/* PostcardAlbum::UpdateSprite                                         */
/* Address: 0x403BA0                                                   */
/* ================================================================== */

void PostcardAlbum::UpdateSprite(int element_id)
{
    switch (element_id) {
    default:
        return;
    case 1:
        Sprite_SetState(this->btn_close, 0, 0);
        return;
    case 2:
        break;
    case 3:
        Sprite_SetState(this->btn_save, 0, 0);
        return;
    case 4:
        if (this->row_enabled_3 != 1) {
            Sprite_SetState(this->btn_rotate, 2, 0);
            return;
        }
        Sprite_SetState(this->btn_rotate, 0, 0);
        return;
    case 5:
        if (this->row_enabled_4 != 1) {
            Sprite_SetState(this->btn_prev, 2, 0);
            return;
        }
        Sprite_SetState(this->btn_prev, 0, 0);
        return;
    case 6:
        if (this->row_enabled_5 != 1) {
            Sprite_SetState(this->btn_next, 2, 0);
            return;
        }
        Sprite_SetState(this->btn_next, 0, 0);
        return;
    case 9:
        Sprite_SetState(this->btn_print, 0, 0);
        return;
    }

    /* Handle case 2 */
    if (this->row_enabled_2 != 1) {
        Sprite_SetState(this->btn_delete, 2, 0);
        return;
    }
    Sprite_SetState(this->btn_delete, 0, 0);
}

/* ================================================================== */
/* PostcardAlbum::HitTest                                              */
/* Address: 0x403CD0                                                   */
/* ================================================================== */

int PostcardAlbum::HitTest(int x, int y)
{
    /* Test btn_close (+0x148) */
    {
        POINT pt = { x, y };
        if (PtInRect((RECT*)&((ButtonSprite*)this->btn_close)->x, pt)) {
            return 1;
        }
    }

    /* Test btn_print (+0x158) */
    {
        POINT pt = { x, y };
        if (PtInRect((RECT*)&((ButtonSprite*)this->btn_print)->x, pt)) {
            return 9;
        }
    }

    /* Test btn_rotate (+0x154) */
    {
        POINT pt = { x, y };
        if (PtInRect((RECT*)&((ButtonSprite*)this->btn_rotate)->x, pt)) {
            return 4;
        }
    }

    /* Test btn_delete (+0x14C) */
    {
        POINT pt = { x, y };
        if (PtInRect((RECT*)&((ButtonSprite*)this->btn_delete)->x, pt)) {
            return 2;
        }
    }

    /* Test btn_save (+0x150) */
    {
        POINT pt = { x, y };
        if (PtInRect((RECT*)&((ButtonSprite*)this->btn_save)->x, pt)) {
            return 3;
        }
    }

    /* Test btn_prev (+0x15C) */
    {
        POINT pt = { x, y };
        if (PtInRect((RECT*)&((ButtonSprite*)this->btn_prev)->x, pt)) {
            return 5;
        }
    }

    /* Test btn_next (+0x160) */
    {
        POINT pt = { x, y };
        if (PtInRect((RECT*)&((ButtonSprite*)this->btn_next)->x, pt)) {
            return 6;
        }
    }

    /* Test 6 row icon sprites at +0x168 */
    for (int i = 0; i < 6; i++) {
        POINT pt = { x, y };
        if (this->row_icon[i] &&
            PtInRect((RECT*)&((ButtonSprite*)this->row_icon[i])->x, pt)) {
            this->hovered_tile = i;
            return 8;
        }
    }

    /* Test 6 row tile sprites at +0x180 */
    for (int i = 0; i < 6; i++) {
        POINT pt = { x, y };
        if (this->row_tile[i] &&
            PtInRect((RECT*)&((ButtonSprite*)this->row_tile[i])->x, pt)) {
            this->hovered_tile = i;
            return 10;
        }
    }

    /* Test 9 tile label sprites at +0x1B0 */
    for (int i = 0; i < 9; i++) {
        POINT pt = { x, y };
        if (this->tile_label_sprites[i] &&
            PtInRect((RECT*)&((ButtonSprite*)this->tile_label_sprites[i])->x, pt)) {
            this->hovered_tile = i;
            return 7;
        }
    }

    return 0;  /* No hit */
}

/* ================================================================== */
/* PostcardAlbum::RenderTileName                                       */
/* Address: 0x4048E0                                                   */
/* ================================================================== */

uint8_t PostcardAlbum::RenderTileName(int row_index)
{
    /* Look up tile data from PixelDataCache using scroll offset + row index */
    uint32_t tile_entry = PixelDataCache_LookupAsset(
        g_dplay_config,
        this->scroll_pixel_offset + row_index,
        this->tile_index);

    if (tile_entry == 0) {
        /* No tile data: clear the row and blit background from surface */
        /* Clear row name buffer */
        this->tile_names[row_index][0] = '\0';

        /* Blit row area from album background surface */
        void* icon_sprite = this->row_icon[row_index];  /* +0x168 + row*4 */
        if (icon_sprite) {
            RECT sprite_rect;
            sprite_rect.left   = *(int*)((uint8_t*)icon_sprite + 4);
            sprite_rect.top    = *(int*)((uint8_t*)icon_sprite + 8);
            sprite_rect.right  = *(int*)((uint8_t*)icon_sprite + 0xC);
            sprite_rect.bottom = *(int*)((uint8_t*)icon_sprite + 0x10);

            if (this->sprites_inited && this->text_rendered) {
                RECT src_offset = sprite_rect;
                RECT dest_rect  = sprite_rect;
                OffsetRect(&src_offset, this->workRect.left,       /* +0xD4 */
                                        this->workRect.top);        /* +0xD8 */
                OffsetRect(&dest_rect, this->blit_dest_x,           /* +0xEC */
                                       this->blit_dest_y);           /* +0xF0 */
                if (!UIPANEL_Blit(this->album_bg_surface,
                                  src_offset.left, src_offset.top,
                                  src_offset.right, src_offset.bottom,
                                  g_primary_surface,
                                  dest_rect.left, dest_rect.top,
                                  dest_rect.right, dest_rect.bottom, 1)) {
                    OutputDebugStringA(s_AW_Blit_failure_reported_0047e0d8);
                }
            }
        }
        return 0;
    }

    /* Tile data found: render the player/tile preview into row's sprite area */
    {
        void* icon_sprite = this->row_icon[row_index];
        if (icon_sprite) {
            DPLAY_RenderPlayer(g_dplay,
                (HDC)(uintptr_t)(*(int*)((uint8_t*)icon_sprite + 0xC) >> 8),
                (int)tile_entry,
                g_primary_surface,
                *(int*)((uint8_t*)icon_sprite + 4),
                *(int*)((uint8_t*)icon_sprite + 8),
                *(uint32_t*)((uint8_t*)icon_sprite + 0xC),
                *(RECT**)((uint8_t*)icon_sprite + 0x10));
        }
    }

    /* Copy player name string into tile name buffer */
    {
        const char* name_src = (const char*)(uintptr_t)(tile_entry + 0x25);
        char* name_dst = this->tile_names[row_index];  /* +0x1DA + row*0x14 */
        int i = 0;
        while (i < 19 && *name_src) {
            *name_dst++ = *name_src++;
            i++;
        }
        *name_dst = '\0';
    }

    /* Free tile entry (vtable[0] scalar dtor) */
    ((void (*)(void*, int))(*(void***)(uintptr_t)tile_entry)[0])(
        (void*)(uintptr_t)tile_entry, 1);

    /* Set row's tile sprite state to normal */
    Sprite_SetState(this->row_tile[row_index], 0, 0);

    return 1;
}

/* ================================================================== */
/* PostcardAlbum::RenderAllTiles                                       */
/* Address: 0x404AC0                                                   */
/* ================================================================== */

void PostcardAlbum::RenderAllTiles()
{
    uint32_t count = 0;

    /* Phase 1: Render tile names into each row */
    if (this->tiles_per_page != 0) {
        for (uint32_t i = 0; i < (uint32_t)this->tiles_per_page; i++) {
            this->RenderTileName(i);

            /* Blit the tile area from album background to primary surface */
            if (this->sprites_inited && this->text_rendered) {
                if (this->row_tile[i]) {
                    RECT sprite_rect;
                    sprite_rect.left   = ((ButtonSprite*)this->row_tile[i])->x;
                    sprite_rect.top    = ((ButtonSprite*)this->row_tile[i])->y;
                    sprite_rect.right  = ((ButtonSprite*)this->row_tile[i])->sourceX;
                    sprite_rect.bottom = ((ButtonSprite*)this->row_tile[i])->sourceY;

                    RECT src_offset = sprite_rect;
                    RECT dest_rect  = sprite_rect;
                    OffsetRect(&src_offset, this->workRect.left,   /* +0xD4 */
                                            this->workRect.top);    /* +0xD8 */
                    OffsetRect(&dest_rect, this->blit_dest_x,       /* +0xEC */
                                           this->blit_dest_y);       /* +0xF0 */
                    if (!UIPANEL_Blit(this->album_bg_surface,
                                      src_offset.left, src_offset.top,
                                      src_offset.right, src_offset.bottom,
                                      g_primary_surface,
                                      dest_rect.left, dest_rect.top,
                                      dest_rect.right, dest_rect.bottom, 1)) {
                        OutputDebugStringA(s_AW_Blit_failure_reported_0047e0d8);
                    }
                }
            }
        }
    }

    /* Phase 2: Begin paint and draw text labels */
    HDC hdc = (HDC)UIPANEL_BeginPaint(this);

    if (this->tiles_per_page != 0) {
        for (uint32_t i = 0; i < (uint32_t)this->tiles_per_page; i++) {
            if (this->tile_names[i][0] != '\0' &&
                this->text_rendered == 1) {
                SetBkMode(hdc, 1);  /* TRANSPARENT */

                /* Set black text */
                COLORREF old_color = SetTextColor(hdc, 0x000000);  /* BLACK */

                /* Set transparent background */
                int old_bk_mode = SetBkMode(hdc, 1);

                /* Select small font */
                void* old_font = SelectObject(hdc, g_font_small);

                /* Draw tile name text */
                RECT text_rect;
                void* sprite = this->row_tile[i];
                if (sprite) {
                    text_rect.left   = *(int*)((uint8_t*)sprite + 4);
                    text_rect.top    = *(int*)((uint8_t*)sprite + 8);
                    text_rect.right  = *(int*)((uint8_t*)sprite + 0xC);
                    text_rect.bottom = *(int*)((uint8_t*)sprite + 0x10);
                } else {
                    SetRectEmpty(&text_rect);
                }

                DrawTextA(hdc, this->tile_names[i], -1,
                          &text_rect,
                          0x10);  /* DT_LEFT | DT_TOP | DT_WORDBREAK = 0x10 */

                /* Restore GDI state */
                SelectObject(hdc, old_font);
                SetTextColor(hdc, old_color);
                SetBkMode(hdc, old_bk_mode);
            }
        }
    }

    /* End paint */
    UIPANEL_EndPaintEx(this, this->hWnd, (int)(uintptr_t)hdc, 1, 0);

    /* Phase 3: Update navigation flags */
    if (this->scroll_pixel_offset == 0) {
        this->row_enabled_4 = 1;             /* enable prev button */
        if (this->tile_index != 0) {
            /* Don't disable prev if tile_index > 0 */
        } else {
            if (this->row_enabled_4 != 1) {
                this->row_enabled_4 = 1;
            }
            this->UpdateSprite(5);
        }
    } else {
        this->row_enabled_4 = 0;
        this->UpdateSprite(5);
    }

    /* Update next button */
    uint32_t entry_count = PixelDataCache_GetEntryCount(g_dplay_config);

    if (this->scroll_pixel_offset + this->tiles_per_page < (int32_t)entry_count) {
        this->row_enabled_5 = 0;
    } else {
        this->row_enabled_5 = 1;
        if (this->tile_index > 7) {
            if (this->row_enabled_5 == 1) {
                this->row_enabled_5 = 0;
                this->UpdateSprite(6);
            }
            return;
        }
    }

    if (this->row_enabled_5 == 0) {
        this->row_enabled_5 = 1;
        this->UpdateSprite(6);
    }
}
