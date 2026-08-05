/**
 * LOCOBITMAP.cpp — PostcardAlbum class and DDRAW_PresentRect implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * SEE LOCOBITMAP.h for naming disambiguation.
 *
 * This file now contains:
 *   A) PostcardAlbum class methods (Entity-derived, vtable 0x4773F0)
 *      - Lifecycle: CreateFromResource, DestroyFromResource, InitFromResource
 *      - Window: InitWindow, DestroyWindow, PaintWindow
 *      - Sprites: FreeAllSprites, FreeSprites, InitSprites, InitWindowSurface
 *      - Surface hit-test/update/blit: UpdateSprite, HitTest, BlitToSurface, BlitElement
 *      - Tile rendering: RenderTileName, RenderAllTiles
 *   B) DDRAW_PresentRect — DDraw blit helper (free function, 0x401280)
 *   C) UIPANEL_Surface management functions
 */

// Status: TRANSCRIBED

#include "LOCOBITMAP.h"
#include "PixelDataCache.h"
#include <new>

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* C++ allocation helpers */
void* operator_new(size_t size);                               /* @0x465CE0 */
void  GLOBAL_free(void* ptr);                                  /* @0x465CD0 */

extern "C" {
    void  DDRAW_GetDdrawErrorString(int32_t error);            /* @0x45BBC0 */
    void  UI_WindowBase_Ctor(void* self, HINSTANCE hInst, uint32_t resId);     /* @0x425870 */
    void  UI_WindowBase_BaseDtor(void* self);                               /* @0x4258D0 */
    BOOL  IsRectEmpty(const RECT* rect);                       /* @0x45B940 */
    void  OffsetRect(RECT* rect, int32_t dx, int32_t dy);     /* @0x45B960 - indirect via 0x477378 */
    void  ClientToScreen(HWND hWnd, POINT* pt);               /* @0x45B980 - indirect via 0x477374 */
    void  GetWindowRect(HWND hWnd, RECT* rect);               /* @0x45B990 - indirect via 0x47737C */
    BOOL  IntersectRect(RECT* dst, const RECT* src1, const RECT* src2); /* @0x45B940 - via 0x47726C */
    void  TileMap_InvalidateRect(void* self, int32_t left, int32_t top,
                                  int32_t right, int32_t bottom);           /* @0x455840 */
    void  TileMap_ScrollRect(int32_t left, int32_t top,
                              int32_t right, int32_t bottom);               /* @0x4553E0 */
    int32_t DDRAW_RestoreSurfaces(void* backbuffer, int32_t* desc);                       /* @0x456550 */

    /* Sprite management */
    ButtonSprite* RESDATA_CreateSpriteObject(void* mem, uint32_t resource_id); /* @0x454B50 */
    void  Sprite_SetState(void* sprite, int32_t state, void* surface);      /* @0x454C30 */
    bool  Sprite_Init(void* sprite);                                        /* @0x454BF0 - __fastcall */
    void  Sprite_Destroy(void* sprite);                                     /* @0x454BC0 - __fastcall */
    void* ResourceManager_GetById(void* resmgr, uint32_t res_id);           /* @0x446EA0 */

    /* Sound */
    void  RESMGR_PlaySound(UINT sound_id);                                 /* @0x447930 */

    /* UI helpers */
    void* UI_CreateFullWindow(void* self, int32_t param, HWND parent,
                               int32_t x, int32_t y, int32_t w, int32_t h,
                               void* menu, void* icon, int32_t flags);      /* @0x425150 */
    void  UI_WindowBase_Hide(void* self);                                   /* @0x4258C0 - approximate */
    void  UIPANEL_EndPaintEx(void* self, HWND hWnd, int32_t param1,
                              uint8_t flag, RECT* rect);                     /* @0x426B90 */
    bool  UIPANEL_Blit(void* panel, uint32_t src_left, uint32_t src_top,
                       int32_t src_right, uint32_t src_bottom,
                       void* dest_surface, uint32_t dst_left, uint32_t dst_top,
                       int32_t dst_right, uint32_t dst_bottom,
                       uint32_t flags);                                      /* @0x42B050 */
    void  UIPANEL_BeginPaint(void* self);                                   /* @0x426B00 */

    void  CGWND_SetMode(int mode);                                         /* @0x408130 */

    /* Rendering helpers */
    bool  CopyRect(RECT* dst, const RECT* src);                             /* USER32 */
    BOOL  PtInRect(const RECT* rect, int32_t x, int32_t y);                 /* USER32 */
    void  OutputDebugStringA(const char* msg);                              /* @0x477090 - indirect */
    void  DrawTextA(void* hdc, const char* str, int32_t count,
                    RECT* rect, uint32_t format);                            /* USER32 */

    /* DPLAY rendering */
    void  DPLAY_RenderPlayer(void* dplay, void* hdc, void* asset_entry,
                             void* surface, int32_t x, int32_t y,
                             int32_t w, void* h);                            /* @0x459F20 */

    /* Win32 API */
    HICON  LoadIconA(HINSTANCE hInst, LPCSTR lpIconName);                 /* @0x45B800 - indirect */
    HWND   GetDesktopWindow(void);                                         /* @0x45B920 - indirect */
    void   GetClientRect(HWND hWnd, RECT* rect);                          /* @0x45B960 - indirect */
    LRESULT DefWindowProcA(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void   Sleep(uint32_t dwMilliseconds);                                 /* @0x45BD30 - indirect */
    int32_t SetBkMode(void* hdc, int32_t mode);                            /* GDI32 */
    int32_t SetTextColor(void* hdc, int32_t color);                        /* GDI32 */
    void*  SelectObject(void* hdc, void* obj);                             /* GDI32 */
    void   SetTextAlign(void* hdc, uint32_t align);                        /* GDI32 */
}

/* Global variables referenced */
extern PixelDataCache* g_pixel_cache;       /* PixelDataCache singleton at 0x4FD3B4 */
extern int32_t g_ref_count;                 /* 0x00485254 -- global UIPANEL_Surface count */
extern uint32_t g_shared_palette[128];      /* 0x0048524C -- shared palette allocation */
extern int32_t g_screen_width;              /* 0x004851D8 */
extern int32_t g_screen_height;             /* 0x00485214 */
extern void*   g_backbuffer;                /* 0x004FD3C0 -- DDraw backbuffer surface */
extern void*   g_primary_surface;           /* 0x004FD3C4 -- DDraw primary surface */
extern void*   g_ddraw;                     /* 0x004FD190 -- IDirectDraw4* */
extern uint8_t g_surface_lost;              /* 0x004FD198 -- DDraw surface lost flag */
extern void*   g_font_small;                /* 0x004855F4 -- small font handle */
extern void*   g_dplay_config;              /* 0x004FD390 -- DPLAY config */
extern void*   g_dplay;                     /* 0x004FD394 -- DPLAY handle */
extern uint8_t g_resmgr;                    /* 0x004855E8 -- ResourceManager singleton */

/* Tilemap & viewport globals for DDRAW_PresentRect */
extern void*   g_tilemap;                                          /* @0x004FD0C8 */
extern int32_t g_viewport_rect_left;    /* 0x004851D0 -- viewport rect */
extern int32_t g_viewport_rect_top;     /* 0x004851D4 */
extern int32_t g_viewport_rect_right;   /* 0x004851DC */
extern int32_t g_viewport_rect_bottom;  /* 0x004851E0 */

namespace {

/* ResourceData's recovered COM-like table has a destructor at slot 0,
 * surface acquisition at slot 1, and release at slot 2.  This interface
 * gives those calls typed C++ dispatch without open-coded vtable indexing. */
struct ResourceDataView {
    virtual void* destroy(uint8_t flags) = 0;
    virtual void* get_surface(int flags, int mode) = 0;
    virtual void release_surface() = 0;

protected:
    ~ResourceDataView() = default;
};

struct PixelEntryView {
    virtual void* destroy(int flags) = 0;

protected:
    ~PixelEntryView() = default;
};

/* IDirectDrawSurface's COM slots used by the recovered present path. */
/* DirectDrawSurface4's vtable is a COM ABI table.  Every entry is
 * __stdcall; omitting it makes the x86 callee/caller stack contract wrong.
 * Slots 5 and 27 are the only non-IUnknown entries used by this file. */
struct DirectDrawSurfaceView {
    virtual HRESULT __stdcall query_interface(const void* riid,
                                               void** object) = 0; // slot 0
    virtual uint32_t __stdcall add_reference() = 0;               // slot 1
    virtual uint32_t __stdcall release() = 0;                     // slot 2
    virtual HRESULT __stdcall slot3() = 0;
    virtual HRESULT __stdcall slot4() = 0;
    virtual HRESULT __stdcall blt(RECT* dst_rect, void* source,
                                  RECT* src_rect, DWORD flags,
                                  void* fx) = 0;                   // slot 5
    virtual HRESULT __stdcall slot6() = 0;
    virtual HRESULT __stdcall slot7() = 0;
    virtual HRESULT __stdcall slot8() = 0;
    virtual HRESULT __stdcall slot9() = 0;
    virtual HRESULT __stdcall slot10() = 0;
    virtual HRESULT __stdcall slot11() = 0;
    virtual HRESULT __stdcall slot12() = 0;
    virtual HRESULT __stdcall slot13() = 0;
    virtual HRESULT __stdcall slot14() = 0;
    virtual HRESULT __stdcall slot15() = 0;
    virtual HRESULT __stdcall slot16() = 0;
    virtual HRESULT __stdcall slot17() = 0;
    virtual HRESULT __stdcall slot18() = 0;
    virtual HRESULT __stdcall slot19() = 0;
    virtual HRESULT __stdcall slot20() = 0;
    virtual HRESULT __stdcall slot21() = 0;
    virtual HRESULT __stdcall slot22() = 0;
    virtual HRESULT __stdcall slot23() = 0;
    virtual HRESULT __stdcall slot24() = 0;
    virtual HRESULT __stdcall slot25() = 0;
    virtual HRESULT __stdcall slot26() = 0;
    virtual HRESULT __stdcall restore() = 0;                      // slot 27

protected:
    ~DirectDrawSurfaceView() = default;
};

static RECT sprite_rect(const ButtonSprite& sprite)
{
    RECT rect{};
    rect.left = sprite.x;
    rect.top = sprite.y;
    rect.right = sprite.sourceX;
    rect.bottom = sprite.sourceY;
    return rect;
}

static bool sprite_contains(const ButtonSprite* sprite, int x, int y)
{
    RECT rect = sprite_rect(*sprite);
    return PtInRect(&rect, x, y) != 0;
}

static void destroy_allocated_sprite(ButtonSprite* sprite)
{
    if (sprite != nullptr) {
        sprite->destroy();
        GLOBAL_free(sprite);
    }
}

static void destroy_resource(void* resource)
{
    if (resource != nullptr) {
        reinterpret_cast<ResourceDataView*>(resource)->destroy(1);
    }
}

static void* resource_surface(void* resource)
{
    return resource == nullptr
        ? nullptr
        : reinterpret_cast<ResourceDataView*>(resource)->get_surface(0, 0);
}

} // namespace

/* ================================================================== */
/* BlitToSurface — apply scroll offsets and blit from background       */
/* panel to primary surface. Repeating pattern in BlitElement,        */
/* RenderTileName, and RenderAllTiles.                                 */
/* ================================================================== */
bool PostcardAlbum::BlitToSurface(ButtonSprite* sprite)
{
    /* ButtonSprite stores the recovered destination rectangle at +0x04..+0x10. */
    RECT src_rect = sprite_rect(*sprite);

    /* Skip blit if paint or window not ready */
    if (this->paint_inited == 0 || this->window_visible == 0) {
        return true;
    }

    RECT src_offset, dst_offset;
    CopyRect(&src_offset, &src_rect);
    CopyRect(&dst_offset, &src_rect);

    /* Apply scroll offsets for source and destination */
    OffsetRect(&src_offset, this->scroll_src_x, this->scroll_src_y);
    OffsetRect(&dst_offset, this->scroll_dst_x, this->scroll_dst_y);

    bool result = UIPANEL_Blit(
        this->background_ui_panel,  /* +0x13C */
        src_offset.left, src_offset.top,
        src_offset.right, src_offset.bottom,
        g_primary_surface,
        dst_offset.left, dst_offset.top,
        dst_offset.right, dst_offset.bottom,
        1  /* flags = 1 (no color key, DDBLT_WAIT) */
    );

    if (!result) {
        OutputDebugStringA("AW_Blit failure reported");
    }

    return result;
}

/* ================================================================== */
/* PostcardAlbum::UpdateSprite                                          */
/* Address: 0x403BA0                                                   */
/*                                                                     */
/* Called by: PostcardAlbum_PaintWindow (VK_LEFT/VK_RIGHT handlers),   */
/*            GFX_RenderAllTiles (after scroll toggle changes),        */
/*            unnamed callers in 0x404EBB-0x4053B5 range              */
/* ================================================================== */
void PostcardAlbum::UpdateSprite(int surface_type)
{
    switch (surface_type) {
    case 1:
        /* Main button: always state 0 */
        Sprite_SetState(this->sprite_main, 0, nullptr);
        return;

    case 2: {
        /* Toggle A: check toggle flag at +0x1D6 */
        uint8_t* toggle = &this->toggle_2;
        if (*toggle == 1) {
            Sprite_SetState(this->sprite_toggle_a, 0, nullptr);
        } else {
            Sprite_SetState(this->sprite_toggle_a, 2, nullptr);
        }
        return;
    }

    case 3:
        /* Button B: always state 0 */
        Sprite_SetState(this->sprite_button_b, 0, nullptr);
        return;

    case 4: {
        /* Toggle B: check toggle flag at +0x1D7 */
        uint8_t* toggle = &this->toggle_4;
        if (*toggle == 1) {
            Sprite_SetState(this->sprite_toggle_b, 0, nullptr);
        } else {
            Sprite_SetState(this->sprite_toggle_b, 2, nullptr);
        }
        return;
    }

    case 5: {
        /* Toggle C (up arrow): check toggle flag at +0x1D4 (toggle_up) */
        if (this->toggle_up != 1) {
            Sprite_SetState(this->sprite_toggle_c, 2, nullptr);
        } else {
            Sprite_SetState(this->sprite_toggle_c, 0, nullptr);
        }
        return;
    }

    case 6: {
        /* Toggle D (down arrow): check toggle flag at +0x1D5 (toggle_down) */
        if (this->toggle_down != 1) {
            Sprite_SetState(this->sprite_toggle_d, 2, nullptr);
        } else {
            Sprite_SetState(this->sprite_toggle_d, 0, nullptr);
        }
        return;
    }

    case 9:
        /* Help button: always state 0 */
        Sprite_SetState(this->sprite_help, 0, nullptr);
        return;

    default:
        /* Types 0, 7, 8, 10: no-op */
        return;
    }
}

/* ================================================================== */
/* PostcardAlbum::HitTest                                               */
/* Address: 0x403CD0                                                   */
/*                                                                     */
/* Called by: PostcardAlbum_PaintWindow (dispatch via UI event handler */
/*            at 0x404F88) — only external xref                       */
/* ================================================================== */
int PostcardAlbum::HitTest(int x, int y)
{
    /* Phase 1: Test individual sprites in priority order */

    /* Type 1: main button at +0x148 */
    if (sprite_contains(this->sprite_main, x, y)) {
        return 1;
    }

    /* Type 9: help sprite at +0x158 (checked 2nd!) */
    if (sprite_contains(this->sprite_help, x, y)) {
        return 9;
    }

    /* Type 4: toggle_b at +0x154 */
    if (sprite_contains(this->sprite_toggle_b, x, y)) {
        return 4;
    }

    /* Type 2: toggle_a at +0x14C */
    if (sprite_contains(this->sprite_toggle_a, x, y)) {
        return 2;
    }

    /* Type 3: button_b at +0x150 */
    if (sprite_contains(this->sprite_button_b, x, y)) {
        return 3;
    }

    /* Type 5: toggle_c (up) at +0x15C */
    if (sprite_contains(this->sprite_toggle_c, x, y)) {
        return 5;
    }

    /* Type 6: toggle_d (down) at +0x160 */
    if (sprite_contains(this->sprite_toggle_d, x, y)) {
        return 6;
    }

    /* Phase 2: Row-based tile items (6 rows, 2 sprites each) */
    for (uint16_t i = 0; i < 6; i++) {
        /* Type 8: tile_left[i] at +0x168 + i*4 */
        if (sprite_contains(this->tile_left[i], x, y)) {
            this->hit_index = i;
            return 8;
        }

        /* Type 10: tile_right[i] at +0x198 + i*4 */
        if (sprite_contains(this->tile_right[i], x, y)) {
            this->hit_index = i;
            return 10;
        }
    }

    /* Phase 3: Thumbnail sprites (9 sprites at +0x1B0) */
    for (uint16_t i = 0; i < 9; i++) {
        /* Type 7: thumb_sprites[i] at +0x1B0 + i*4 */
        if (sprite_contains(this->thumb_sprites[i], x, y)) {
            this->hit_index = i;
            return 7;
        }
    }

    return 0; /* No hit */
}

/* ================================================================== */
/* PostcardAlbum::BlitElement                                           */
/* Address: 0x403E80                                                   */
/*                                                                     */
/* Blit one UI element with click sound and optional UIPANEL blit.     */
/* Toggle types (2,4,5,6) check their flag first. Only sprites that    */
/* have a valid toggle value (==1) get blitted and play sound.         */
/*                                                                     */
/* Called by: PostcardAlbum_PaintWindow (VK_LEFT/VK_RIGHT handlers),   */
/*            unnamed callers in 0x404FB6-0x405352 range              */
/* ================================================================== */
void PostcardAlbum::BlitElement(int surface_type)
{
    switch (surface_type) {
    case 1:
        /* Main button — play sound, blit, set state 1 */
        RESMGR_PlaySound(0x5015);
        this->BlitToSurface(this->sprite_main);
        Sprite_SetState(this->sprite_main, 1, nullptr);
        return;

    case 2: {
        /* Toggle A — check flag at +0x1D6 */
        uint8_t* toggle = &this->toggle_2;
        if (*toggle != 1) {
            Sprite_SetState(this->sprite_toggle_a, 2, nullptr);
            return;
        }
        RESMGR_PlaySound(0x5015);
        this->BlitToSurface(this->sprite_toggle_a);
        Sprite_SetState(this->sprite_toggle_a, 1, nullptr);
        return;
    }

    case 3:
        /* Button B — play sound, blit, set state 1 */
        RESMGR_PlaySound(0x5015);
        this->BlitToSurface(this->sprite_button_b);
        Sprite_SetState(this->sprite_button_b, 1, nullptr);
        return;

    case 4: {
        /* Toggle B — check flag at +0x1D7 */
        uint8_t* toggle = &this->toggle_4;
        if (*toggle != 1) {
            Sprite_SetState(this->sprite_toggle_b, 2, nullptr);
            return;
        }
        RESMGR_PlaySound(0x5015);
        this->BlitToSurface(this->sprite_toggle_b);
        Sprite_SetState(this->sprite_toggle_b, 1, nullptr);
        return;
    }

    case 5: {
        /* Toggle C (up arrow) — check toggle_up (+0x1D4) */
        if (this->toggle_up != 1) {
            Sprite_SetState(this->sprite_toggle_c, 2, nullptr);
            return;
        }
        RESMGR_PlaySound(0x5015);
        this->BlitToSurface(this->sprite_toggle_c);
        Sprite_SetState(this->sprite_toggle_c, 1, nullptr);
        return;
    }

    case 6: {
        /* Toggle D (down arrow) — check toggle_down (+0x1D5) */
        if (this->toggle_down != 1) {
            Sprite_SetState(this->sprite_toggle_d, 2, nullptr);
            return;
        }
        /* Type 6 unique: blit WITHOUT sound first, then play sound AFTER */
        this->BlitToSurface(this->sprite_toggle_d);
        Sprite_SetState(this->sprite_toggle_d, 1, nullptr);
        RESMGR_PlaySound(0x5015);
        return;
    }

    case 7:
        /* Indicator sprite — play sound, set state from sprite_state_value (+0x128) */
        RESMGR_PlaySound(0x5015);
        Sprite_SetState(this->sprite_indicator, this->sprite_state_value, nullptr);
        return;

    case 9:
        /* Help button — play sound, blit, set state 1 */
        RESMGR_PlaySound(0x5015);
        this->BlitToSurface(this->sprite_help);
        Sprite_SetState(this->sprite_help, 1, nullptr);
        return;

    default:
        /* Types 0, 8, 10: no-op */
        return;
    }
}

/* ================================================================== */
/* PostcardAlbum::InitWindowSurface (was GFX_InitWindow)               */
/* Address: 0x404720                                                   */
/*                                                                     */
/* Lazy-init the window background surface. Loads resource 0x3C0A      */
/* (normal) or 0x3C0B (hi-res) based on high_res flag.                 */
/*                                                                     */
/* Called by: CGWND_InitMode1 (0x408457, 0x408595) for both town       */
/*            view and album view                                      */
/* ================================================================== */
void PostcardAlbum::InitWindowSurface()
{
    if (this->background_loaded != 0) {
        return; /* Already initialized */
    }

    /* Choose resource based on resolution */
    uint32_t res_id;
    if (this->high_res == 0) {
        res_id = 0x3C0A;
    } else {
        res_id = 0x3C0B;
    }

    /* Load background RESDATA and create surface */
    void* resdata = ResourceManager_GetById(static_cast<void*>(&g_resmgr), res_id);
    this->background_resdata = resdata;                 /* +0x138 */

    /* Call the typed RESDATA surface accessor. */
    void* surface = resource_surface(resdata);
    this->background_ui_panel = surface;                 /* +0x13C */

    this->background_loaded = 1;                         /* +0xFC */
}

/* ================================================================== */
/* PostcardAlbum::InitSprites (was GFX_InitSprites)                    */
/* Address: 0x404770                                                   */
/*                                                                     */
/* Init all tile-grid sprites and load paint resource 0x3CFA.          */
/* Sets paint_inited (+0x111) = 1.                                     */
/*                                                                     */
/* Called by: PostcardAlbum_InitWindow (via CGWND flow at 0x402595)    */
/* ================================================================== */
void PostcardAlbum::InitSprites()
{
    if (this->paint_inited != 0) {
        return; /* Already initialized */
    }

    /* Initialize 8 individual sprites */
    Sprite_Init(this->sprite_main);
    Sprite_Init(this->sprite_toggle_a);
    Sprite_Init(this->sprite_button_b);
    Sprite_Init(this->sprite_toggle_b);
    Sprite_Init(this->sprite_help);
    Sprite_Init(this->sprite_toggle_c);
    Sprite_Init(this->sprite_toggle_d);
    Sprite_Init(this->sprite_indicator);

    /* Initialize 6 tile_mid sprites (at +0x180..+0x194) */
    for (int i = 0; i < 6; i++) {
        Sprite_Init(this->tile_mid[i]);
    }

    /* Load paint resource 0x3CFA and create surface */
    void* resdata = ResourceManager_GetById(static_cast<void*>(&g_resmgr), 0x3CFA);
    this->paint_resdata = resdata;                       /* +0x140 */

    void* surface = resource_surface(resdata);
    this->paint_surface = surface;                       /* +0x144 */

    this->paint_inited = 1;                               /* +0x111 */
}

/* ================================================================== */
/* PostcardAlbum::FreeSprites (was GFX_FreeSprites)                    */
/* Address: 0x404830                                                   */
/*                                                                     */
/* Tear down all tile-grid sprite objects and release paint resource.  */
/* Clears paint_inited (+0x111) = 0.                                   */
/*                                                                     */
/* Called by: PostcardAlbum_DestroyWindow, PostcardAlbum_FreeAllSprites*/
/* ================================================================== */
void PostcardAlbum::FreeSprites()
{
    if (this->paint_inited == 0) {
        return;
    }

    /* Release paint RESDATA through its typed destructor entry. */
    if (this->paint_resdata != nullptr) {
        destroy_resource(this->paint_resdata);
        this->paint_resdata = nullptr;                    /* +0x140 */
    }

    /* Destroy 8 individual sprites */
    Sprite_Destroy(this->sprite_main);
    Sprite_Destroy(this->sprite_toggle_a);
    Sprite_Destroy(this->sprite_button_b);
    Sprite_Destroy(this->sprite_toggle_b);
    Sprite_Destroy(this->sprite_help);
    Sprite_Destroy(this->sprite_toggle_c);
    Sprite_Destroy(this->sprite_toggle_d);
    Sprite_Destroy(this->sprite_indicator);

    /* Destroy 6 tile_mid sprites */
    for (int i = 0; i < 6; i++) {
        Sprite_Destroy(this->tile_mid[i]);
    }

    this->paint_inited = 0;                               /* +0x111 */
}

/* ================================================================== */
/* PostcardAlbum::RenderTileName (was GFX_RenderTileName)               */
/* Address: 0x4048E0                                                   */
/*                                                                     */
/* Render a single tile's display name onto the primary surface.       */
/* Looks up pixel-format entry by asset index, renders name, copies    */
/* string to tile_text_buf[tile_index].                                 */
/*                                                                     */
/* Called by: RenderAllTiles (0x404AE8)                                */
/* ================================================================== */
void PostcardAlbum::RenderTileName(int tile_index)
{
    /* Look up pixel format entry for this tile from the pixel data cache */
    void* entry = g_pixel_cache->LookupAsset(
        this->tile_offset + tile_index,
        this->tile_shown_count
    );

    if (entry == nullptr) {
        /* No entry — clear name flag and blit empty background */
        this->tile_text_buf[tile_index][0] = 0;  /* +0x1DA + tile_index*0x14 */

        /* Get the tile_left rect for this row and blit it */
        this->BlitToSurface(this->tile_left[tile_index]);

        /* Clear the name flag at start of text buffer (checks for empty/not-rendered) */
        this->tile_text_buf[tile_index][0] = '\0';
        return;
    }

    /* Get the tile_left sprite for the blit rect */
    ButtonSprite* sprite = this->tile_left[tile_index];   /* +0x168 + tile_index*4 */

    /* Render player name via DPLAY */
    const RECT tile_rect = sprite_rect(*sprite);
    int32_t rect_bottom = tile_rect.bottom;
    int32_t rect_width  = tile_rect.right;
    int32_t rect_top    = tile_rect.top;
    int32_t rect_left   = tile_rect.left;

    /* DPLAY_RenderPlayer(global_dplay, DC, entry, primary, x, y, w, ???); */
    DPLAY_RenderPlayer(
        g_dplay,
        reinterpret_cast<void*>(static_cast<intptr_t>(sprite->sourceX)),  /* hDC derived from width */
        entry,
        g_primary_surface,
        rect_left,
        rect_top,
        rect_width,
        reinterpret_cast<void*>(static_cast<intptr_t>(rect_bottom))
    );

    /* Copy the entry name to tile_text_buf[tile_index] (max 20 bytes) */
    /* Name is at entry + 0x25 */
    const char* src_name = reinterpret_cast<const char*>(
        static_cast<const uint8_t*>(entry) + 0x25);
    char* dst_buf = this->tile_text_buf[tile_index];  /* +0x1DA + tile_index*0x14 */

    /* Copy full string with strlen + memcpy (no 19-char limit) */
    size_t len = 0;
    while (src_name[len] != '\0') {
        dst_buf[len] = src_name[len];
        len++;
    }
    dst_buf[len] = '\0';

    /* Mark entry as used through its typed release entry. */
    reinterpret_cast<PixelEntryView*>(entry)->destroy(1);

    /* Set tile_mid sprite state to 0 (default/visible) */
    Sprite_SetState(this->tile_mid[tile_index], 0, nullptr);
}

/* ================================================================== */
/* PostcardAlbum::RenderAllTiles (was GFX_RenderAllTiles)               */
/* Address: 0x404AC0                                                   */
/*                                                                     */
/* Full tile-grid render: blit each tile, draw debug text, then update */
/* scroll toggle flags (up/down arrows).                               */
/*                                                                     */
/* Called by: PostcardAlbum_PaintWindow (0x402848),                    */
/*            CGWND_GameSetup_RenderPlayerSlots (0x4055BA),           */
/*            and unnamed callers in 0x404F18-0x4053AC range          */
/* ================================================================== */
void PostcardAlbum::RenderAllTiles()
{
    /* Phase 1: Render all visible tile names and blit backgrounds */
    if (this->tile_total_count != 0) {
        for (uint32_t i = 0; i < static_cast<uint32_t>(this->tile_total_count); i++) {
            this->RenderTileName(static_cast<int>(i));

            /* Blit the tile_right rect for this row */
            this->BlitToSurface(this->tile_right[i]);
        }
    }

    /* Phase 2: Begin paint (create HDC), render debug text if enabled */
    UIPANEL_BeginPaint(this);
    void* hdc = nullptr;  /* placeholder — HDC from BeginPaint */
    /* TODO: UIPANEL_BeginPaint returns void; HDC obtained via UI surface */

    if (this->tile_total_count != 0 && this->show_debug_text == 1) {
        for (uint32_t i = 0; i < static_cast<uint32_t>(this->tile_total_count); i++) {
            SetBkMode(hdc, 1);  /* TRANSPARENT */

            SetTextColor(hdc, 0);  /* Black text */

            int32_t old_mode = SetBkMode(hdc, 1);
            void* old_font = SelectObject(hdc, g_font_small);

            /* Get tile_right rect for text placement */
            RECT text_rect;
            ButtonSprite* sprite = this->tile_right[i];
            text_rect = sprite_rect(*sprite);

            DrawTextA(hdc, this->tile_text_buf[i], -1, &text_rect, 0x25);

            SelectObject(hdc, old_font);
            SetTextColor(hdc, 0);
            SetBkMode(hdc, old_mode);
            SetBkMode(hdc, old_mode);
        }
    }

    /* End paint */
    UIPANEL_EndPaintEx(this, this->hWnd,
                       static_cast<int32_t>(reinterpret_cast<intptr_t>(hdc)),
                       0x01, nullptr);

    /* Phase 3: Update scroll toggle flags */
    if (this->tile_offset == 0) {
        /* At top of list — enable up arrow, check if we can scroll down */
        this->scroll_up_visible = 1;

        if (this->tile_shown_count != 0) {
            /* Items exist above, show up arrow */
            if (this->toggle_up != 1) {
                this->toggle_up = 1;
                this->UpdateSprite(5);
            }
        } else {
            /* No items, hide up arrow */
            if (this->toggle_up == 1) {
                this->toggle_up = 0;
                this->UpdateSprite(5);
            }
        }
    } else {
        /* Not at top — hide up arrow, check scroll down */
        this->scroll_up_visible = 0;

        if (this->toggle_up != 0) {
            /* Up arrow is visible but shouldn't be */
            this->toggle_up = 0;
            this->UpdateSprite(5);
        }
    }

    /* Update down arrow based on DPLAY config entry count */
    int32_t total_entries = static_cast<PixelDataCache*>(g_dplay_config)->GetEntryCount();

    if (static_cast<uint32_t>(this->tile_offset + this->tile_total_count) <
        static_cast<uint32_t>(total_entries)) {
        /* More items exist below — hide down arrow (wait, this is confusing) */
        /* BUG or INTENT: When tile_offset + tile_total_count < total_entries,
           scroll_down_visible is set to 0 (can see more below — should be 1).
           But the flag name might mean "reached bottom" not "can scroll down". */
        this->scroll_down_visible = 0;
    } else {
        /* At bottom — show down arrow */
        this->scroll_down_visible = 1;

        if (this->tile_shown_count > 7) {
            /* Showing many items, check down arrow state */
            if (this->toggle_down == 1) {
                /* Was shown but shouldn't be — hide it */
                this->toggle_down = 0;
                this->UpdateSprite(6);
                return;
            }
        }
        /* else: not enough items, keep down arrow as-is */
    }

    if (this->toggle_down == 0) {
        /* Down arrow hidden but should be shown — show it */
        this->toggle_down = 1;
        this->UpdateSprite(6);
    }
}

/* ================================================================== */
/* PostcardAlbum::CreateFromResource                                   */
/* Address: 0x401F50                                                   */
/* ================================================================== */
PostcardAlbum* PostcardAlbum::CreateFromResource(void* mem, HINSTANCE hInst, uint32_t resId)
{
    return mem != nullptr ? new (mem) PostcardAlbum(hInst, resId) : nullptr;
}

/**
 * PostcardAlbum::PostcardAlbum
 * Address: 0x401F50
 *
 * Placement construction preserves the binary's caller-owned allocation while
 * allowing C++ to install the class vtable.
 */
PostcardAlbum::PostcardAlbum(HINSTANCE hInst, uint32_t resId)
{
    UI_WindowBase_Ctor(this, hInst, resId);  /* @0x425870 — init Entity/GameObject chain */
    InitFromResource();
}

/* ================================================================== */
/* PostcardAlbum::InitFromResource                                      */
/* Address: 0x401FD0                                                   */
/* ================================================================== */
void PostcardAlbum::InitFromResource()
{
    /* SEH prologue */

    /* Initialize fields */
    this->field_0E8 = nullptr;              /* +0xE8 */
    this->paint_inited = 0;                  /* +0x111 */
    this->tile_offset = 0;                   /* +0x114 */
    this->tile_shown_count = 0;              /* +0x118 */
    this->tile_total_count = 0;              /* +0x11C */
    this->hit_index = 0;                     /* +0x120 */
    this->box_count = 9;                     /* +0x124 */
    this->sprite_state_value = 0;            /* +0x128 */
    this->show_debug_text = 1;               /* +0x12C */
    this->window_visible = 0;                /* +0x112 */
    this->destroyed = 0;                     /* +0x110 */

    /* Detect high-resolution mode (>= 800x600, stored at 0x4851D8/0x485214) */
    if ((g_screen_width < 0x321) && (g_screen_height < 0x258)) {
        this->high_res = 0;                  /* +0x134 — low resolution */
    } else {
        this->high_res = 1;                  /* +0x134 — high resolution */
    }

    /* Create 8 individual UI sprites (resources 0x3C04-0x3C0F) */
    this->sprite_main       = RESDATA_CreateSpriteObject(operator_new(sizeof(ButtonSprite)), 0x3C04);
    this->sprite_toggle_a   = RESDATA_CreateSpriteObject(operator_new(sizeof(ButtonSprite)), 0x3C09);
    this->sprite_button_b   = RESDATA_CreateSpriteObject(operator_new(sizeof(ButtonSprite)), 0x3C05);
    this->sprite_toggle_b   = RESDATA_CreateSpriteObject(operator_new(sizeof(ButtonSprite)), 0x3C08);
    this->sprite_help       = RESDATA_CreateSpriteObject(operator_new(sizeof(ButtonSprite)), 0x3C0F);
    this->sprite_toggle_c   = RESDATA_CreateSpriteObject(operator_new(sizeof(ButtonSprite)), 0x3C06);
    this->sprite_toggle_d   = RESDATA_CreateSpriteObject(operator_new(sizeof(ButtonSprite)), 0x3C07);

    /* 8th sprite — resource varies by resolution */
    if (this->high_res == 0) {
        this->sprite_indicator = RESDATA_CreateSpriteObject(operator_new(sizeof(ButtonSprite)), 0x3C0C);
    } else {
        this->sprite_indicator = RESDATA_CreateSpriteObject(operator_new(sizeof(ButtonSprite)), 0x3C0D);
    }

    /* Create 6 rows of 3 sprites each (18 sprites total) */
    /* Interleaved storage at +0x168 (tile_left), +0x180 (tile_mid), +0x198 (tile_right) */
    for (int row = 0; row < 6; row++) {
        /* tile_left (surface_type 8) */
        this->tile_left[row] = RESDATA_CreateSpriteObject(operator_new(sizeof(ButtonSprite)), 0);
        /* tile_mid (background for name text) */
        this->tile_mid[row]  = RESDATA_CreateSpriteObject(operator_new(sizeof(ButtonSprite)), 0x3C0E);
        /* tile_right (surface_type 10) */
        this->tile_right[row] = RESDATA_CreateSpriteObject(operator_new(sizeof(ButtonSprite)), 0);
        /* Initialize text buffer byte to 0 (at +0x1DA + row*0x14) */
        this->tile_text_buf[row][0] = 0;
    }

    /* Create 9 thumbnail sprites (at +0x1B0..+0x1CC) */
    for (int i = 0; i < 9; i++) {
        this->thumb_sprites[i] = RESDATA_CreateSpriteObject(operator_new(sizeof(ButtonSprite)), 0);
    }

    /* Set all toggle flags to 1 (enabled by default) */
    this->toggle_up           = 1;   /* +0x1D4 */
    this->toggle_down         = 1;   /* +0x1D5 */
    this->toggle_2            = 1;   /* +0x1D6 */
    this->toggle_4            = 1;   /* +0x1D7 */
    this->scroll_up_visible   = 1;   /* +0x1D8 */
    this->scroll_down_visible = 1;   /* +0x1D9 */

    /* Init remaining state */
    this->background_resdata  = nullptr;  /* +0x138 */
    this->extra_sprite        = nullptr; /* +0x130 */
    this->background_loaded   = 0;        /* +0xFC */
}

/* ================================================================== */
/* PostcardAlbum::DestroyFromResource (vtable[0])                      */
/* Address: 0x401FB0                                                   */
/* ================================================================== */
void* PostcardAlbum::DestroyFromResource(uint8_t flags)
{
    this->FreeAllSprites();
    return this;
}

/* ================================================================== */
/* PostcardAlbum::FreeAllSprites (misnamed "LoadPalette" in Ghidra)     */
/* Address: 0x402380                                                   */
/* ================================================================== */
void PostcardAlbum::FreeAllSprites()
{
    /* SEH prologue */

    /* C++ keeps the class vtable active while resources are released. */

    /* Step 1: Free all sprites if initialized */
    if (this->paint_inited != 0) {            /* +0x111 */
        this->FreeSprites();                   /* @0x404830 */
    }

    /* Step 2: Free 8 individual sprites (+0x148..+0x164). */
    ButtonSprite* sprites[8] = {
        this->sprite_main,
        this->sprite_toggle_a,
        this->sprite_button_b,
        this->sprite_toggle_b,
        this->sprite_help,
        this->sprite_toggle_c,
        this->sprite_toggle_d,
        this->sprite_indicator
    };
    for (ButtonSprite* sprite : sprites) {
        destroy_allocated_sprite(sprite);
    }
    this->sprite_main = nullptr;
    this->sprite_toggle_a = nullptr;
    this->sprite_button_b = nullptr;
    this->sprite_toggle_b = nullptr;
    this->sprite_help = nullptr;
    this->sprite_toggle_c = nullptr;
    this->sprite_toggle_d = nullptr;
    this->sprite_indicator = nullptr;

    /* Step 3: Free 6 rows x 3 sprites. */
    for (int row = 0; row < 6; row++) {
        destroy_allocated_sprite(this->tile_left[row]);
        destroy_allocated_sprite(this->tile_mid[row]);
        destroy_allocated_sprite(this->tile_right[row]);
        this->tile_left[row] = nullptr;
        this->tile_mid[row] = nullptr;
        this->tile_right[row] = nullptr;
    }

    /* Step 4: Free 9 thumbnail sprites. */
    for (ButtonSprite*& sprite : this->thumb_sprites) {
        destroy_allocated_sprite(sprite);
        sprite = nullptr;
    }

    /* Step 5: Free extra sprite at +0x130. */
    destroy_allocated_sprite(this->extra_sprite);
    this->extra_sprite = nullptr;

    /* Step 6: Resdata cleanup — field_0E8 and others. */
    if (this->field_0E8 != nullptr) {
        destroy_resource(this->field_0E8);
        this->field_0E8 = nullptr;
    }

    /* Step 7: Destroy base window (UI_WindowBase_BaseDtor) */
    UI_WindowBase_BaseDtor(this);           /* @0x4258D0 */

    /* SEH epilogue */
}

/* ================================================================== */
/* PostcardAlbum::InitWindow                                           */
/* Address: 0x402520                                                   */
/* ================================================================== */
bool PostcardAlbum::InitWindow(HWND hWndParent)
{
    RECT desktop_rect;
    HWND desktop = GetDesktopWindow();            /* @0x45B920 (indirect) */
    GetClientRect(desktop, &desktop_rect);        /* @0x45B960 (indirect) */

    /* Load icon resource 0x65 */
    HICON icon = LoadIconA(
        this->hInstance,
        reinterpret_cast<LPCSTR>(static_cast<uintptr_t>(0x65)));  /* @0x45B800 */

    /* Store icon handle */
    this->field_0E8 = static_cast<void*>(icon);  /* +0xE8 */

    /* Create the full-screen child window */
    void* result = UI_CreateFullWindow(
        this,                                     /* self */
        0,                                        /* param */
        hWndParent,                               /* parent HWND */
        desktop_rect.left,                        /* x */
        desktop_rect.top,                         /* y */
        desktop_rect.right - desktop_rect.left,   /* width */
        desktop_rect.bottom - desktop_rect.top,   /* height */
        nullptr,                                  /* menu */
        icon,                                     /* icon */
        0                                         /* flags */
    );                                            /* @0x425150 */

    return result != nullptr;
}

/* ================================================================== */
/* PostcardAlbum::DestroyWindow (vtable[1])                             */
/* Address: 0x402660                                                   */
/* ================================================================== */
void PostcardAlbum::DestroyWindow()
{
    /* Check if window has been created (+0xE4 is likely a "has_window" flag) */
    if (this->has_window != 0) {
        UI_WindowBase_Hide(this);                 /* @0x4258C0 (approximate) */
        this->window_visible = 0;                 /* +0x112 */
        this->FreeSprites();                      /* @0x404830 */
    }
}

/* ================================================================== */
/* PostcardAlbum::PaintWindow — WndProc for keyboard navigation         */
/* Address: 0x402690                                                   */
/* ================================================================== */
LRESULT PostcardAlbum::PaintWindow(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    /* If destroyed, return 0 immediately */
    if (this->destroyed != 0) {                    /* +0x110 */
        return 0;
    }

    switch (wParam) {
    case 0x0D:  /* VK_RETURN */
    case 0x1B:  /* VK_ESCAPE */
        /* Virtual dispatch to DestroyWindow */
        this->DestroyWindow();
        CGWND_SetMode(3);                         /* @0x408130 — return to game mode */
        return 0;

    case 0x25:  /* VK_LEFT — previous page */
        if (this->toggle_up != 1) {                /* +0x1D4 */
            return 0;
        }
        this->BlitElement(5);                      /* @0x403E80 */
        UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);  /* @0x426B90 */
        Sleep(0x96);                               /* 150ms delay */
        this->UpdateSprite(5);                     /* @0x403BA0 */

        if (this->scroll_up_visible == 1) {        /* +0x1D8 (wrap-around mode) */
            /* Wrap around — decrement sprite_state_value */
            this->sprite_state_value--;             /* +0x128 */
            this->tile_shown_count = this->sprite_state_value;  /* +0x118 */

            /* Recalculate tile_offset from album data via global pixel cache */
            this->tile_offset = g_pixel_cache->Unlock(this->sprite_state_value)
                               / this->tile_total_count;

            uint32_t total = g_pixel_cache->GetEntryCount();
            if ((total % static_cast<uint32_t>(this->tile_total_count) == 0) &&
                (total != 0)) {
                this->tile_offset--;
            }
            this->tile_offset *= this->tile_total_count;

            Sprite_SetState(this->sprite_indicator, this->sprite_state_value, nullptr);  /* @0x454C30 */
            goto render;
        }

        /* Normal — move to previous tiles */
        this->tile_offset -= this->tile_total_count;
        break;

    case 0x27:  /* VK_RIGHT — next page */
        if (this->toggle_down != 1) {              /* +0x1D5 */
            return 0;
        }
        this->BlitElement(6);                      /* @0x403E80 */
        UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);  /* @0x426B90 */
        Sleep(0x96);                               /* 150ms delay */
        this->UpdateSprite(6);                     /* @0x403BA0 */

        if (this->scroll_down_visible == 1) {      /* +0x1D9 (wrap-around mode) */
            /* Wrap around — increment sprite_state_value */
            this->sprite_state_value++;             /* +0x128 */
            this->tile_shown_count = this->sprite_state_value;  /* +0x118 */
            this->tile_offset = 0;                  /* +0x114 */
            Sprite_SetState(this->sprite_indicator, this->sprite_state_value, nullptr);
            goto render;
        }

        /* Normal — move to next tiles */
        this->tile_offset += this->tile_total_count;
        break;

    default:
        return DefWindowProcA(hWnd, msg, wParam, lParam);
    }

    /* Update tile_offset */
    this->tile_offset = this->tile_offset;

render:
    this->RenderAllTiles();                        /* @0x404AC0 */
    UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);  /* @0x426B90 */
    return 0;
}

/* ================================================================== */
/* DDRAW_PresentRect — DDraw present/blit helper (free function)       */
/* Address: 0x401280                                                   */
/* ================================================================== */
void __cdecl DDRAW_PresentRect(const RECT* rect, HWND hWnd, int32_t offset_xy[2],
                               uint8_t use_color_key)
{
    BOOL result;
    RECT blit_rect;
    RECT window_rect;
    RECT clipped_rect;
    POINT screen_offset;
    int32_t blit_flags;

    /* Save original rect dimensions for DDraw blit source */
    RECT src_rect;
    src_rect.left   = rect->left;     /* +0x00 */
    src_rect.top    = rect->top;      /* +0x04 */
    src_rect.right  = rect->right;    /* +0x08 */
    src_rect.bottom = rect->bottom;   /* +0x0C */

    /* Return early if rect is empty */
    result = IsRectEmpty(rect);                  /* @0x45B940 (via 0x477268) */
    if (result != 0) {
        return;
    }

    /* Copy the rect for blitting, apply optional offset */
    blit_rect.left   = rect->left;
    blit_rect.top    = rect->top;
    blit_rect.right  = rect->right;
    blit_rect.bottom = rect->bottom;

    if (offset_xy != nullptr) {
        OffsetRect(&blit_rect, -offset_xy[0], -offset_xy[1]);  /* @0x45B960 (via 0x477378) */
    }

    /* Convert client coordinates to screen coordinates */
    screen_offset.x = 0;
    screen_offset.y = 0;
    ClientToScreen(hWnd, &screen_offset);        /* @0x45B980 (via 0x477374) */
    OffsetRect(&blit_rect, screen_offset.x, screen_offset.y);

    /* Get the window rect and clip the blit rect against it */
    GetWindowRect(hWnd, &window_rect);           /* @0x45B990 (via 0x47737C) */
    result = IntersectRect(&clipped_rect, &blit_rect, &window_rect);  /* @0x45B940 (via 0x47726C) */
    if (result == 0) {
        return;  /* Nothing visible — rect is outside window */
    }

    /* Determine blit flags */
    if (use_color_key == 0) {
        blit_flags = 0x1000000;  /* DDBLT_WAIT */
    } else {
        blit_flags = 0x200;      /* DDBLT_KEYSRC */
    }

    /* Perform the Blt from primary surface to backbuffer. */
    DirectDrawSurfaceView* backbuffer =
        reinterpret_cast<DirectDrawSurfaceView*>(g_backbuffer);
    int32_t blt_result = backbuffer->blt(
        &blit_rect, g_primary_surface, &src_rect, blit_flags, nullptr);

    if (blt_result == 0x887601C2) {  /* DDERR_SURFACELOST */
        /* Surface was lost — restore it and retry */
        int32_t restore_result = backbuffer->restore(); /* slot 27 = Restore */

        if (restore_result != 0) {
            goto error;  /* Surface restore failed */
        }

        /* Retry the blit with DDBLT_WAIT. */
        blt_result = backbuffer->blt(
            &blit_rect, g_primary_surface, &src_rect, 0x1000000, nullptr);

        if (blt_result == 0) {
            /* Success — invalidate viewport for full redraw */
            TileMap_InvalidateRect(g_tilemap,
                g_viewport_rect_left, g_viewport_rect_top,
                g_viewport_rect_right, g_viewport_rect_bottom);  /* @0x455840 */
        }
    } else if (use_color_key != 0 && blt_result != 0) {
        /* With color key and non-SURFACELOST error, retry with DDBLT_WAIT. */
        blt_result = backbuffer->blt(
            &blit_rect, g_primary_surface, &src_rect, 0x1000000, nullptr);

        if (blt_result == 0x887601C2) {
            /* Surface lost again on retry */
            int32_t restore_result = backbuffer->restore();
            if (restore_result != 0) {
                goto error;
            }
            blt_result = backbuffer->blt(
                &blit_rect, g_primary_surface, &src_rect, 0x1000000, nullptr);
        }
    }

    if (blt_result == 0) {
        return;  /* Success */
    }

error:
    DDRAW_GetDdrawErrorString(blt_result);       /* @0x45BBC0 */
}

/* ================================================================== */
/* UIPANEL_Surface management functions                                */
/* ================================================================== */

/* ────────────────────────────────────────────────────────────────── */
/* UIPANEL_CreateSurface — Constructor for UIPANEL_Surface            */
/* Address: 0x42A110 — __fastcall (ECX=this)                         */
/* ────────────────────────────────────────────────────────────────── */
void UIPANEL_CreateSurface(UIPANEL_Surface* surface)
{
/* In the binary: surface->vtable = VTBL_*. Compiler-managed in natural C++. */
    surface->mode        = 0;                         /* +0x04 */
    surface->width       = 0;                         /* +0x08 */
    surface->height      = 0;                         /* +0x0C */
    surface->has_palette = 0;                         /* +0x10 */
    surface->flags       = 0;                         /* +0x11 */
    surface->palette_ptr = nullptr;                   /* +0x14 */
    surface->pixels      = nullptr;                   /* +0x18 */
    surface->ddraw_surf  = nullptr;                   /* +0x1C */

    g_ref_count++;  /* 0x00485254 */
}

/* ────────────────────────────────────────────────────────────────── */
/* UIPANEL_DestroySurface — Scalar dtor (vtable[0] at 0x477D28)     */
/* Address: 0x42A140 — __thiscall (ECX=this)                        */
/* ────────────────────────────────────────────────────────────────── */
void* UIPANEL_DestroySurface(UIPANEL_Surface* surface, uint8_t flags)
{
    /* Restore vtable */
/* In the binary: surface->vtable = VTBL_*. Compiler-managed in natural C++. */

    /* Free palette allocation (0x200 bytes, 128 uint32 entries) */
    if ((surface->has_palette == 1) && (surface->palette_ptr != nullptr)) {
        GLOBAL_free(surface->palette_ptr);           /* @0x465CD0 */
        surface->palette_ptr = nullptr;
        surface->has_palette = 0;
    }

    /* Free pixel buffer */
    if (surface->pixels != nullptr) {
        GLOBAL_free(surface->pixels);
        surface->pixels = nullptr;
    }

    /* Release DDraw surface via IDirectDrawSurface::Release */
    if (surface->ddraw_surf != nullptr) {
        uint32_t release_result =
            reinterpret_cast<DirectDrawSurfaceView*>(surface->ddraw_surf)->release();
        (void)release_result;
        surface->ddraw_surf = nullptr;
    }

    /* Decrement global ref count */
    g_ref_count--;  /* 0x00485254 */

    /* Optionally free heap memory (scalar deleting dtor flag) */
    if ((flags & 1) != 0) {
        GLOBAL_free(surface);
    }

    return surface;
}
