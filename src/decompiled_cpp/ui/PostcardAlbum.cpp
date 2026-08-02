// Status: INTEGRATED
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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#include "ButtonSprite.h"
#include "../core/CGWND.h"
#include "../resources/ResourceManager.h"
#include "../graphics/PixelDataCache.h"
#include <new>
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* Memory */
    void*  operator_new(size_t size);               /* 0x465CE0 */
    void   GLOBAL_free(void* ptr);                   /* global free */

    /* UI window base */
    void   UI_WindowBase_BaseDtor(void* self);       /* 0x425910 — used only by the original;
                                                        C++ dtor chain handles base cleanup */

    /* CGWND helpers */
    void   CGWND_SetMode(int mode);                 /* 0x408130 */

    /* UIPANEL */
    bool   UIPANEL_Blit(void* renderer, uint32_t src_x, uint32_t src_y,
                        int src_w, uint32_t src_h,
                        void* dest_surface, uint32_t dest_x, uint32_t dest_y,
                        int dest_w, uint32_t dest_h, uint32_t flags);   /* 0x42B050 */
    HDC    UIPANEL_BeginPaint(void* self);           /* 0x426B00 */
    void   UIPANEL_EndPaintEx(void* self, int hdc, int unlock_param,
                              uint8_t unlock_flag, RECT* restrict_rect); /* 0x426B90 */

    /* Network / DPlay */
    void   DPLAY_RenderPlayer(void* dplay, void* hdc, int32_t player_data,
                              void* surface, int32_t x, int32_t y,
                              uint32_t w, RECT* rect);                  /* 0x4437C0 */

extern "C" {
    /* Win32 */
    HWND   GetDesktopWindow();
    void   GetClientRect(HWND hWnd, RECT* rect);
    void*  LoadIconA(HINSTANCE hInstance, const char* name);
    LRESULT DefWindowProcA(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    BOOL   PtInRect(const RECT* rect, POINT pt);
    void   CopyRect(RECT* dest, const RECT* src);
    void   OffsetRect(RECT* rect, int dx, int dy);
    void   Sleep(DWORD ms);
    int    SetBkMode(HDC hdc, int mode);       /* returns previous mode */
    COLORREF SetTextColor(HDC hdc, COLORREF color);
    void*  SelectObject(HDC hdc, void* obj);
    int    DrawTextA(HDC hdc, const char* text, int len, RECT* rect, UINT flags);
    void   OutputDebugStringA(const char* str);
    BOOL   ShowWindow(HWND hWnd, int nCmdShow);
    HWND   SetFocus(HWND hWnd);
}

/* ================================================================== */
/* Global variables                                                    */
/* ================================================================== */

extern int    g_screen_width;            /* 0x4851D8 */
extern int    g_screen_height;           /* 0x485214 */
extern void*  g_primary_surface;         /* 0x4FD3C4 — Primary DDraw surface */
extern void*  g_dplay;                   /* 0x4FD3B0 — DirectPlay */
extern void*  g_font_small;              /* 0x4855F4 — Small UI font handle */

extern const char s_AW_Blit_failure_reported_0047e0d8[]; /* debug string */

/* ================================================================== */
/* Resource ABI bridge                                                 */
/* ================================================================== */

namespace {

/**
 * Release a resource object obtained from g_resmgr.GetById().
 * The resource vtable is RESDATA (0x478274): [0] scalar dtor, [1] Lock,
 * [2] Unlock. The typed ResourceObject view in ResourceManager.h models
 * these slots; the objects returned by GetById are built by the original
 * binary, so we dispatch through the documented slots.
 */
void release_resource(void* resource)
{
    if (resource != nullptr) {
        /* ResourceObject::Unlock — vtable[2] */
        ResourceObject* obj = static_cast<ResourceObject*>(resource);
        obj->Unlock();
    }
}

/**
 * Scalar-delete a sprite object (ButtonSprite vtable 0x47851C, slot [0]).
 * Used for objects stored as ButtonSprite* where the pointer slot must
 * be cleared (the binary nulls each slot after freeing).
 */
void delete_sprite(ButtonSprite*& sprite)
{
    if (sprite != nullptr) {
        delete sprite;   /* compiler emits scalar deleting destructor */
    }
    sprite = nullptr;
}

/**
 * Scalar-delete the lazily-created +0x130 sprite slot, which is stored
 * as void* in this class (its exact type is not yet decompiled).
 */
void delete_tile_preview_sprite(void*& sprite)
{
    if (sprite != nullptr) {
        delete static_cast<ButtonSprite*>(sprite);
    }
    sprite = nullptr;
}

}  // namespace

/* ================================================================== */
/* PostcardAlbum::PostcardAlbum (constructor)                          */
/* Address: 0x401F50 (within PostcardAlbum_CreateFromResource)         */
/* ================================================================== */

PostcardAlbum::PostcardAlbum(HINSTANCE hInstance, UINT resId)
    : UI_WindowBase(hInstance, resId)
{
    /* In the binary: sets vtable to 0x4773F0 here. Compiler-managed in
     * natural C++ (derived vtable is installed before the ctor body). */
    this->InitFromResource();
}

/* ================================================================== */
/* PostcardAlbum::CreateFromResource — Factory                         */
/* Address: 0x401F50 (PostcardAlbum_CreateFromResource)                */
/* ================================================================== */

PostcardAlbum* PostcardAlbum::CreateFromResource(void* mem, HINSTANCE hInstance, UINT resId)
{
    if (mem == nullptr) {
        return nullptr;
    }
    return new (mem) PostcardAlbum(hInstance, resId);
}

/* ================================================================== */
/* PostcardAlbum::~PostcardAlbum — scalar deleting destructor          */
/* Address: 0x401FB0 (vtable[0], PostcardAlbum_DestroyFromResource)    */
/* ================================================================== */

PostcardAlbum::~PostcardAlbum()
{
    /* User cleanup only; the compiler emits the heap free. */
    this->FreeAllSprites();
}

/* ================================================================== */
/* PostcardAlbum::InitFromResource                                     */
/* Address: 0x401FD0                                                   */
/* ================================================================== */

void PostcardAlbum::InitFromResource()
{
    /* Zero-initialize album state fields */
    this->icon_handle = nullptr;                 /* +0xE8 */
    this->sprites_inited = 0;                    /* +0x111 */
    this->scroll_pixel_offset = 0;               /* +0x114 */
    this->tile_index = 0;                        /* +0x118 */
    this->tiles_per_page = 0;                    /* +0x11C */
    this->hovered_tile = 0;                      /* +0x120 */
    this->tile_count_init = 9;                   /* +0x124 — default 9 tiles */
    this->scroll_wheel_pos = 0;                  /* +0x128 */
    this->scroll_wheel_enabled = 1;              /* +0x12C */
    this->text_rendered = 0;                     /* +0x112 */
    this->active_flag = 0;                       /* +0x110 */

    /* Detect high-resolution mode */
    if (g_screen_width < 0x321 && g_screen_height < 0x259) {
        this->is_high_res = 0;                   /* +0x134 — 800x600 or smaller */
    } else {
        this->is_high_res = 1;                   /* +0x134 — 1024x768 or larger */
    }

    /* Create 8 button sprites (0x24 bytes each via ButtonSprite ctor) */

    /* btn_close  — res 0x3C04 (+0x148) */
    {
        void* mem = operator_new(sizeof(ButtonSprite));
        this->btn_close = mem ? new (mem) ButtonSprite(0x3C04) : nullptr;
    }

    /* btn_delete — res 0x3C09 (+0x14C) */
    {
        void* mem = operator_new(sizeof(ButtonSprite));
        this->btn_delete = mem ? new (mem) ButtonSprite(0x3C09) : nullptr;
    }

    /* btn_save   — res 0x3C05 (+0x150) */
    {
        void* mem = operator_new(sizeof(ButtonSprite));
        this->btn_save = mem ? new (mem) ButtonSprite(0x3C05) : nullptr;
    }

    /* btn_rotate — res 0x3C08 (+0x154) */
    {
        void* mem = operator_new(sizeof(ButtonSprite));
        this->btn_rotate = mem ? new (mem) ButtonSprite(0x3C08) : nullptr;
    }

    /* btn_print  — res 0x3C0F (+0x158) */
    {
        void* mem = operator_new(sizeof(ButtonSprite));
        this->btn_print = mem ? new (mem) ButtonSprite(0x3C0F) : nullptr;
    }

    /* btn_prev   — res 0x3C06 (+0x15C) */
    {
        void* mem = operator_new(sizeof(ButtonSprite));
        this->btn_prev = mem ? new (mem) ButtonSprite(0x3C06) : nullptr;
    }

    /* btn_next   — res 0x3C07 (+0x160) */
    {
        void* mem = operator_new(sizeof(ButtonSprite));
        this->btn_next = mem ? new (mem) ButtonSprite(0x3C07) : nullptr;
    }

    /* btn_scrollwheel — res 0x3C0C (low-res) or 0x3C0D (high-res) (+0x164) */
    {
        void* mem = operator_new(sizeof(ButtonSprite));
        if (this->is_high_res == 0) {
            this->btn_scrollwheel = mem ? new (mem) ButtonSprite(0x3C0C) : nullptr;
        } else {
            this->btn_scrollwheel = mem ? new (mem) ButtonSprite(0x3C0D) : nullptr;
        }
    }

    /* Create 6 row sprite groups: icons (+0x168), tiles (+0x180),
     * name sprites (+0x198). The binary allocates all three per row. */
    for (int row = 0; row < 6; row++) {
        /* Row icon sprite — res 0 (no default resource) */
        {
            void* mem = operator_new(0x24);
            this->row_icon[row] = mem ? new (mem) ButtonSprite(0) : nullptr;
        }

        /* Row tile sprite — res 0x3C0E */
        {
            void* mem = operator_new(0x24);
            this->row_tile[row] = mem ? new (mem) ButtonSprite(0x3C0E) : nullptr;
        }

        /* Row name sprite — res 0 (no default resource) */
        {
            void* mem = operator_new(0x24);
            this->row_name[row] = mem ? new (mem) ButtonSprite(0) : nullptr;
        }

        /* Clear the row name buffer's first dword (matches the binary's
         * 4-byte store at +0x1DA + row*0x14). */
        std::memset(this->tile_names[row], 0, 4);
    }

    /* Create 9 tile label sprites at +0x1B0 */
    for (int i = 0; i < 9; i++) {
        void* mem = operator_new(0x24);
        this->tile_label_sprites[i] = mem ? new (mem) ButtonSprite(0) : nullptr;
    }

    /* Set all element enable flags to 1 (+0x1D4..+0x1D9) */
    for (int i = 0; i < 6; i++) {
        this->row_enabled[i] = 1;
    }

    /* Clear scroll/selection state */
    this->tile_preview_sprite = nullptr;              /* +0x130 lazily-created sprite slot */
    this->album_bg_resource = nullptr;      /* +0x138 (binary sets +0x138=0 and +0x130=0) */
    this->window_surface_inited = 0;        /* +0xFC */
}

/* ================================================================== */
/* PostcardAlbum::InitWindow                                           */
/* Address: 0x402520 — shared with PostcardPreviewWindow in the binary */
/* ================================================================== */

bool PostcardAlbum::InitWindow(HWND hParent)
{
    RECT desktop_rect;
    HWND hDesktop = GetDesktopWindow();
    GetClientRect(hDesktop, &desktop_rect);

    /* Load window icon (resource 0x65) */
    this->icon_handle = LoadIconA(this->hInstance, (const char*)(uintptr_t)0x65);

    /* Create full-desktop child window */
    int width = desktop_rect.right - desktop_rect.left;
    int height = desktop_rect.bottom - desktop_rect.top;
    int result = this->create_full_window(
         0, hParent,
        desktop_rect.left, desktop_rect.top,
        width, height,
        (HMENU)0, (HICON)this->icon_handle, 0);
    return (result != 0);
}

/* ================================================================== */
/* PostcardAlbum::InitWindowSurface                                    */
/* Address: 0x404720                                                   */
/* ================================================================== */

void PostcardAlbum::InitWindowSurface()
{
    if (this->window_surface_inited != 0) {   /* +0xFC */
        return;
    }

    /* Load appropriate background resource based on resolution */
    int res_id;
    if (this->is_high_res == 0) {
        res_id = 0x3C0A;
    } else {
        res_id = 0x3C0B;
    }

    int32_t res = g_resmgr.GetById(res_id);   /* ResourceManager_GetById, 0x446EA0 */
    void* resource = reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(res)));
    this->album_bg_resource = resource;       /* +0x138 */

    /* Get surface via resource vtable[1] = ResourceObject::Lock(0, 0).
     * NOTE: the binary dereferences the GetById result unconditionally;
     * the null guard here only changes behavior where the original
     * would crash (resource 0x3C0A/0x3C0B always exist in the game). */
    if (resource != nullptr) {
        this->album_bg_surface = static_cast<ResourceObject*>(resource)->Lock(0, 0);
    }

    this->window_surface_inited = 1;          /* +0xFC */
}

/* ================================================================== */
/* PostcardAlbum::InitSprites                                          */
/* Address: 0x404770                                                   */
/* ================================================================== */

void PostcardAlbum::InitSprites()
{
    if (this->sprites_inited != 0) {          /* +0x111 */
        return;
    }

    /* Initialize all 8 button sprites (ButtonSprite::init, 0x454BF0) */
    this->btn_close->init();                  /* +0x148 */
    this->btn_delete->init();                 /* +0x14C */
    this->btn_save->init();                   /* +0x150 */
    this->btn_rotate->init();                 /* +0x154 */
    this->btn_print->init();                  /* +0x158 */
    this->btn_prev->init();                   /* +0x15C */
    this->btn_next->init();                   /* +0x160 */
    this->btn_scrollwheel->init();            /* +0x164 */

    /* Initialize the 6 row TILE sprites at +0x180 (verified: the loop
     * in the binary walks +0x180, not the +0x168 icon array) */
    for (int i = 0; i < 6; i++) {
        this->row_tile[i]->init();
    }

    /* Load photo background resource (res 0x3CFA) */
    int32_t res = g_resmgr.GetById(0x3CFA);
    void* resource = reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(res)));
    this->photo_bg_resource = resource;       /* +0x140 */
    if (resource != nullptr) {
        this->photo_bg_surface = static_cast<ResourceObject*>(resource)->Lock(0, 0);
    }

    this->sprites_inited = 1;                 /* +0x111 */
}

/* ================================================================== */
/* PostcardAlbum::FreeSprites                                          */
/* Address: 0x404830                                                   */
/* ================================================================== */

void PostcardAlbum::FreeSprites()
{
    if (this->sprites_inited == 0) {          /* +0x111 */
        return;
    }

    /* Release photo background resource (vtable[2] = Unlock) */
    if (this->photo_bg_resource != nullptr) { /* +0x140 */
        static_cast<ResourceObject*>(this->photo_bg_resource)->Unlock();
    }
    this->photo_bg_resource = nullptr;        /* +0x140 */
    this->photo_bg_surface = nullptr;         /* +0x144 */

    /* Destroy all 8 button sprites (ButtonSprite::destroy, 0x454BC0) */
    this->btn_close->destroy();
    this->btn_delete->destroy();
    this->btn_save->destroy();
    this->btn_rotate->destroy();
    this->btn_print->destroy();
    this->btn_prev->destroy();
    this->btn_next->destroy();
    this->btn_scrollwheel->destroy();

    /* Destroy the 6 row TILE sprites at +0x180 (verified) */
    for (int i = 0; i < 6; i++) {
        this->row_tile[i]->destroy();
    }

    this->sprites_inited = 0;                 /* +0x111 */
}

/* ================================================================== */
/* PostcardAlbum::hide — Hide/destroy the album window                 */
/* Address: 0x402660 (vtable[1], PostcardAlbum_DestroyWindow)          */
/* ================================================================== */

void PostcardAlbum::hide()
{
    if (this->visible != 0) {                 /* +0xE4 */
        this->UI_WindowBase::hide();          /* 0x425990 */
        this->text_rendered = 0;              /* +0x112 */
        this->FreeSprites();
    }
}

/* ================================================================== */
/* PostcardAlbum::show — Show the album (mode 6 entry)                 */
/* Address: 0x402590 (vtable[2])                                       */
/*                                                                     */
/* Verified from raw x86 bytes; the only not-yet-decompiled step is    */
/* the vtable[7] dispatch (0x4028B0), which UI_WindowBase does not     */
/* expose as a C++ virtual.                                            */
/* ================================================================== */

void PostcardAlbum::show()
{
    this->InitSprites();                      /* 0x404770 */

    /* In the binary: vtable slot [7] (0x4028B0), the album's
     * on_create/init-layout override, is dispatched with `this`. Not yet
     * decompiled (undefined in the Ghidra DB) and UI_WindowBase has no
     * C++ virtual for slot [7], so it is not dispatched here.
     * TODO: decompile 0x4028B0. */

    this->UI_WindowBase::show();              /* 0x4259C0 */

    ShowWindow(this->hWnd, 3);                /* SW_MAXIMIZE */
    SetFocus(this->hWnd);

    /* Free the lazily-created +0x130 sprite if any */
    delete_tile_preview_sprite(this->tile_preview_sprite);   /* +0x130 */

    /* vtable[3] = set_mode with the child-object slots (+0x60, +0x64) */
    this->set_mode(this->childCount0, this->childObj0, 0, 1);

    this->active_flag = 0;                    /* +0x110 */

    /* Restore scroll position from the PixelDataCache temp fields
     * (insert_index +0x10 / saved_album_index +0x14), then reset them.
     * The binary uses unsigned divl by tiles_per_page; a zero
     * tiles_per_page faults in the original as well. */
    PixelDataCache* cache = g_pixel_cache;
    uint32_t edi = static_cast<uint32_t>(cache->insert_index);
    int32_t  ecx = cache->saved_album_index;
    uint32_t rem = edi % static_cast<uint32_t>(this->tiles_per_page);
    int32_t  scroll = static_cast<int32_t>(edi - rem);

    if (scroll >= 0 && ecx >= 0) {
        if (scroll != this->scroll_pixel_offset ||
            ecx != this->scroll_wheel_pos) {
            this->scroll_wheel_pos = ecx;     /* +0x128 */
            this->tile_index = ecx;           /* +0x118 */
            this->scroll_pixel_offset = 0;    /* +0x114 */
            this->RenderAllTiles();
            this->scroll_pixel_offset = scroll;
        }
    }
    cache->insert_index = -1;                 /* +0x10 */
    cache->saved_album_index = -1;            /* +0x14 */
}

/* ================================================================== */
/* PostcardAlbum::FreeAllSprites                                       */
/* Address: 0x402380                                                   */
/* ================================================================== */

void PostcardAlbum::FreeAllSprites()
{
    /* Clean up sprite data if initialized */
    if (this->sprites_inited) {               /* +0x111 */
        this->FreeSprites();
    }

    /* Release album background resource (vtable[2] = Unlock) */
    if (this->album_bg_resource != nullptr) { /* +0x138 */
        static_cast<ResourceObject*>(this->album_bg_resource)->Unlock();
    }
    this->album_bg_resource = nullptr;

    /* Destroy all 8 button sprite objects (scalar deleting dtor) */
    delete_sprite(this->btn_close);
    delete_sprite(this->btn_delete);
    delete_sprite(this->btn_save);
    delete_sprite(this->btn_rotate);
    delete_sprite(this->btn_print);
    delete_sprite(this->btn_prev);
    delete_sprite(this->btn_next);

    /* Destroy 6 row icon sprites (+0x168), 6 row name sprites (+0x198),
     * 6 row tile sprites (+0x180) — the binary frees all three arrays */
    for (int i = 0; i < 6; i++) {
        delete_sprite(this->row_icon[i]);
        delete_sprite(this->row_name[i]);
        delete_sprite(this->row_tile[i]);
    }

    /* Destroy 9 tile label sprites (+0x1B0) */
    for (int i = 0; i < 9; i++) {
        delete_sprite(this->tile_label_sprites[i]);
    }

    /* Destroy the scrollwheel sprite (+0x164) */
    delete_sprite(this->btn_scrollwheel);

    /* Destroy the lazily-created +0x130 sprite slot */
    delete_tile_preview_sprite(this->tile_preview_sprite);

    /* Base cleanup runs through the C++ destructor chain
     * (UI_WindowBase::~UI_WindowBase -> base_destructor). The binary
     * calls UI_WindowBase_BaseDtor explicitly here; doing so in C++
     * would double-run the base cleanup. */
}

/* ================================================================== */
/* PostcardAlbum::PaintWindow                                          */
/* Address: 0x402690 (vtable[21])                                      */
/* ================================================================== */

LRESULT PostcardAlbum::on_key_down(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    /* Guard: if the album is busy/active (+0x110), ignore the message */
    if (this->active_flag != 0) {             /* +0x110 */
        return 0;
    }

    switch (wParam) {
    case 0x0D:  /* ENTER key */
    case 0x1B:  /* ESC key */
        /* Hide album and return to game mode 3 (typed vtable[1]) */
        this->hide();                         /* 0x402660 */
        CGWND_SetMode(3);
        return 0;

    case 0x25:  /* VK_LEFT */
        if (this->row_enabled[0] != 1) {      /* +0x1D4 */
            return 0;
        }
        /* Play click animation: blit + paint + sleep + update */
        this->BlitElement(5);                 /* prev button highlight */
        UIPANEL_EndPaintEx(this, (int)(intptr_t)this->hWnd, 0, 0, nullptr);
        Sleep(0x96);
        this->UpdateSprite(5);                /* prev button normal */

        if (this->row_enabled[4] == 1) {      /* +0x1D8 */
            /* Page-up: decrement scroll position */
            this->scroll_wheel_pos -= 1;      /* +0x128 */
            this->tile_index = this->scroll_wheel_pos;  /* +0x118 */

            /* Recalculate scroll_pixel_offset */
            uint32_t offset = g_pixel_cache->Unlock(this->tile_index);
            this->scroll_pixel_offset = static_cast<int32_t>(
                offset / static_cast<uint32_t>(this->tiles_per_page));

            /* Handle boundary cases */
            uint32_t count = g_pixel_cache->GetEntryCount();
            if ((count % static_cast<uint32_t>(this->tiles_per_page) == 0) &&
                count != 0) {
                this->scroll_pixel_offset -= 1;
            }
            this->scroll_pixel_offset = this->tiles_per_page * this->scroll_pixel_offset;

            /* Update scrollwheel sprite state from scroll_wheel_pos */
            this->btn_scrollwheel->setState(this->scroll_wheel_pos, nullptr);
            break;   /* re-render below */
        }
        /* Scroll left by tiles_per_page */
        this->scroll_pixel_offset -= this->tiles_per_page;
        break;

    case 0x27:  /* VK_RIGHT */
        if (this->row_enabled[1] != 1) {      /* +0x1D5 */
            return 0;
        }
        /* Play click animation */
        this->BlitElement(6);                 /* next button highlight */
        UIPANEL_EndPaintEx(this, (int)(intptr_t)this->hWnd, 0, 0, nullptr);
        Sleep(0x96);
        this->UpdateSprite(6);                /* next button normal */

        if (this->row_enabled[5] == 1) {      /* +0x1D9 */
            /* Page-down: increment scroll position */
            this->scroll_wheel_pos += 1;      /* +0x128 */
            this->tile_index = this->scroll_wheel_pos;  /* +0x118 */
            this->scroll_pixel_offset = 0;    /* +0x114 */

            /* Update scrollwheel sprite state */
            this->btn_scrollwheel->setState(this->scroll_wheel_pos, nullptr);
            break;   /* re-render below */
        }
        /* Scroll right by tiles_per_page */
        this->scroll_pixel_offset += this->tiles_per_page;
        break;

    default:
        return DefWindowProcA(hWnd, msg, wParam, lParam);
    }

    /* After scroll: re-render all tiles */
    this->RenderAllTiles();
    UIPANEL_EndPaintEx(this, (int)(intptr_t)this->hWnd, 0, 0, nullptr);

    return 0;
}

/* ================================================================== */
/* PostcardAlbum::BlitElement                                          */
/* Address: 0x403E80                                                   */
/* ================================================================== */

void PostcardAlbum::BlitElement(int element_id)
{
    /* Blit the sprite's rect from the album background to the primary
     * surface; the sprite rect is its x/y/sourceX/sourceY fields. */
    auto blit_sprite = [this](ButtonSprite* sprite) {
        RECT src_rect;
        src_rect.left   = sprite->x;
        src_rect.top    = sprite->y;
        src_rect.right  = sprite->sourceX;
        src_rect.bottom = sprite->sourceY;

        if (this->sprites_inited && this->text_rendered) {
            RECT src_offset = src_rect;
            RECT dest_rect  = src_rect;
            OffsetRect(&src_offset, this->workRect.left,     /* +0xD4 */
                                    this->workRect.top);      /* +0xD8 */
            OffsetRect(&dest_rect, this->blit_dest_x,        /* +0xEC */
                                   this->blit_dest_y);        /* +0xF0 */
            if (!UIPANEL_Blit(this->album_bg_surface,
                              static_cast<uint32_t>(src_offset.left),
                              static_cast<uint32_t>(src_offset.top),
                              src_offset.right, static_cast<uint32_t>(src_offset.bottom),
                              g_primary_surface,
                              static_cast<uint32_t>(dest_rect.left),
                              static_cast<uint32_t>(dest_rect.top),
                              dest_rect.right, static_cast<uint32_t>(dest_rect.bottom),
                              1)) {
                OutputDebugStringA(s_AW_Blit_failure_reported_0047e0d8);
            }
        }
        sprite->setState(1, nullptr);
    };

    switch (element_id) {
    default:
        return;

    case 1:  /* btn_close */
        PlaySound(0x5015);
        blit_sprite(this->btn_close);
        return;

    case 2:  /* btn_delete */
        break;   /* handled after the switch */

    case 3:  /* btn_save */
        PlaySound(0x5015);
        blit_sprite(this->btn_save);
        return;

    case 4:  /* btn_rotate */
        if (this->row_enabled[3] != 1) {      /* +0x1D7 */
            this->btn_rotate->setState(2, nullptr);
            return;
        }
        PlaySound(0x5015);
        blit_sprite(this->btn_rotate);
        return;

    case 5:  /* btn_prev */
        if (this->row_enabled[0] != 1) {      /* +0x1D4 */
            this->btn_prev->setState(2, nullptr);
            return;
        }
        PlaySound(0x5015);
        blit_sprite(this->btn_prev);
        return;

    case 6:  /* btn_next — the binary plays the sound AFTER the blit */
        if (this->row_enabled[1] != 1) {      /* +0x1D5 */
            this->btn_next->setState(2, nullptr);
            return;
        }
        blit_sprite(this->btn_next);
        PlaySound(0x5015);
        return;

    case 7:  /* btn_scrollwheel — state from scroll_wheel_pos (+0x128) */
        PlaySound(0x5015);
        this->btn_scrollwheel->setState(this->scroll_wheel_pos, nullptr);
        return;

    case 9:  /* btn_print */
        PlaySound(0x5015);
        blit_sprite(this->btn_print);
        return;
    }

    /* Handle case 2 (btn_delete) */
    if (this->row_enabled[2] != 1) {          /* +0x1D6 */
        this->btn_delete->setState(2, nullptr);
        return;
    }
    PlaySound(0x5015);
    blit_sprite(this->btn_delete);
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
        this->btn_close->setState(0, nullptr);
        return;
    case 2:
        break;   /* handled after the switch */
    case 3:
        this->btn_save->setState(0, nullptr);
        return;
    case 4:
        if (this->row_enabled[3] != 1) {      /* +0x1D7 */
            this->btn_rotate->setState(2, nullptr);
            return;
        }
        this->btn_rotate->setState(0, nullptr);
        return;
    case 5:
        if (this->row_enabled[0] != 1) {      /* +0x1D4 */
            this->btn_prev->setState(2, nullptr);
            return;
        }
        this->btn_prev->setState(0, nullptr);
        return;
    case 6:
        if (this->row_enabled[1] != 1) {      /* +0x1D5 */
            this->btn_next->setState(2, nullptr);
            return;
        }
        this->btn_next->setState(0, nullptr);
        return;
    case 9:
        this->btn_print->setState(0, nullptr);
        return;
    }

    /* Handle case 2 (btn_delete) */
    if (this->row_enabled[2] != 1) {          /* +0x1D6 */
        this->btn_delete->setState(2, nullptr);
        return;
    }
    this->btn_delete->setState(0, nullptr);
}

/* ================================================================== */
/* PostcardAlbum::HitTest                                              */
/* Address: 0x403CD0 (non-virtual helper)                              */
/* ================================================================== */

int PostcardAlbum::HitTest(int x, int y)
{
    POINT pt = { x, y };

    auto hit = [&pt](ButtonSprite* sprite) {
        RECT r = { sprite->x, sprite->y, sprite->sourceX, sprite->sourceY };
        return PtInRect(&r, pt) != FALSE;
    };

    /* Test btn_close (+0x148) -> 1 */
    if (hit(this->btn_close)) return 1;

    /* Test btn_print (+0x158) -> 9 */
    if (hit(this->btn_print)) return 9;

    /* Test btn_rotate (+0x154) -> 4 */
    if (hit(this->btn_rotate)) return 4;

    /* Test btn_delete (+0x14C) -> 2 */
    if (hit(this->btn_delete)) return 2;

    /* Test btn_save (+0x150) -> 3 */
    if (hit(this->btn_save)) return 3;

    /* Test btn_prev (+0x15C) -> 5 */
    if (hit(this->btn_prev)) return 5;

    /* Test btn_next (+0x160) -> 6 */
    if (hit(this->btn_next)) return 6;

    /* Test 6 row icon sprites (+0x168) -> 8, then 6 row name sprites
     * (+0x198) -> 10. Verified: the binary tests +0x198 for the second
     * loop, not the +0x180 tile array. */
    for (int i = 0; i < 6; i++) {
        if (hit(this->row_icon[i])) {
            this->hovered_tile = i;           /* +0x120 */
            return 8;
        }
        if (hit(this->row_name[i])) {
            this->hovered_tile = i;           /* +0x120 */
            return 10;
        }
    }

    /* Test 9 tile label sprites (+0x1B0) -> 7 */
    for (int i = 0; i < 9; i++) {
        if (hit(this->tile_label_sprites[i])) {
            this->hovered_tile = i;           /* +0x120 */
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
    void* tile_entry = g_pixel_cache->LookupAsset(
        this->scroll_pixel_offset + row_index,   /* +0x114 + row */
        this->tile_index);                       /* +0x118 */

    if (tile_entry == nullptr) {
        /* No tile data: clear the row and blit background from surface */
        this->tile_names[row_index][0] = '\0';   /* +0x1DA + row*0x14 */

        /* Blit row area from album background surface */
        ButtonSprite* icon_sprite = this->row_icon[row_index];  /* +0x168 + row*4 */
        RECT sprite_rect;
        sprite_rect.left   = icon_sprite->x;
        sprite_rect.top    = icon_sprite->y;
        sprite_rect.right  = icon_sprite->sourceX;
        sprite_rect.bottom = icon_sprite->sourceY;

        if (this->sprites_inited && this->text_rendered) {
            RECT src_offset = sprite_rect;
            RECT dest_rect  = sprite_rect;
            OffsetRect(&src_offset, this->workRect.left,       /* +0xD4 */
                                    this->workRect.top);        /* +0xD8 */
            OffsetRect(&dest_rect, this->blit_dest_x,          /* +0xEC */
                                   this->blit_dest_y);          /* +0xF0 */
            if (!UIPANEL_Blit(this->album_bg_surface,
                              static_cast<uint32_t>(src_offset.left),
                              static_cast<uint32_t>(src_offset.top),
                              src_offset.right, static_cast<uint32_t>(src_offset.bottom),
                              g_primary_surface,
                              static_cast<uint32_t>(dest_rect.left),
                              static_cast<uint32_t>(dest_rect.top),
                              dest_rect.right, static_cast<uint32_t>(dest_rect.bottom),
                              1)) {
                OutputDebugStringA(s_AW_Blit_failure_reported_0047e0d8);
            }
        }
        return 0;
    }

    /* Tile data found: render the player/tile preview into the row's
     * icon sprite area. The HDC argument packs the sprite's +0x0C field
     * shifted right by 8 with scroll_wheel_enabled (+0x12C) as the top
     * byte (verified from raw x86). */
    ButtonSprite* icon_sprite = this->row_icon[row_index];
    DPLAY_RenderPlayer(g_dplay,
        reinterpret_cast<void*>(
            ((static_cast<uint32_t>(icon_sprite->sourceX) >> 8) & 0xFFFFFF) |
            (static_cast<uint32_t>(this->scroll_wheel_enabled) << 24)),
        static_cast<int32_t>(reinterpret_cast<intptr_t>(tile_entry)),
        g_primary_surface,
        icon_sprite->x,
        icon_sprite->y,
        static_cast<uint32_t>(icon_sprite->sourceX),
        reinterpret_cast<RECT*>(static_cast<uintptr_t>(icon_sprite->sourceY)));

    /* Copy player name string into tile name buffer.
     * The binary uses an unbounded strlen+copy; the 19-char cap here is a
     * safety deviation that only changes behavior for names that would
     * overflow the 20-byte buffer (and corrupt the object in the x86
     * original). */
    {
        const char* name_src = reinterpret_cast<const char*>(
            reinterpret_cast<const uint8_t*>(tile_entry) + 0x25);
        char* name_dst = this->tile_names[row_index];  /* +0x1DA + row*0x14 */
        int i = 0;
        while (i < 19 && *name_src) {
            *name_dst++ = *name_src++;
            i++;
        }
        *name_dst = '\0';
    }

    /* Free the tile entry via its scalar deleting destructor (vtable[0]).
     * The tile entry type is not yet decompiled, so this is a documented
     * ABI dispatch. */
    {
        void** vtable = *reinterpret_cast<void***>(tile_entry);
        using ScalarDtor = void (*)(void*, int);
        ScalarDtor dtor = reinterpret_cast<ScalarDtor>(vtable[0]);
        dtor(tile_entry, 1);
    }

    /* Set row's tile sprite state to normal */
    this->row_tile[row_index]->setState(0, nullptr);

    return 1;
}

/* ================================================================== */
/* PostcardAlbum::RenderAllTiles                                       */
/* Address: 0x404AC0                                                   */
/* ================================================================== */

void PostcardAlbum::RenderAllTiles()
{
    /* Phase 1: Render tile names and blit each row's name-sprite area
     * (+0x198 — verified) from the album background to the primary. */
    if (this->tiles_per_page != 0) {          /* +0x11C */
        for (uint32_t i = 0; i < static_cast<uint32_t>(this->tiles_per_page); i++) {
            this->RenderTileName(static_cast<int>(i));

            if (this->sprites_inited && this->text_rendered) {
                ButtonSprite* sprite = this->row_name[i];   /* +0x198 + i*4 */
                RECT sprite_rect;
                sprite_rect.left   = sprite->x;
                sprite_rect.top    = sprite->y;
                sprite_rect.right  = sprite->sourceX;
                sprite_rect.bottom = sprite->sourceY;

                RECT src_offset = sprite_rect;
                RECT dest_rect  = sprite_rect;
                OffsetRect(&src_offset, this->workRect.left,   /* +0xD4 */
                                        this->workRect.top);    /* +0xD8 */
                OffsetRect(&dest_rect, this->blit_dest_x,      /* +0xEC */
                                       this->blit_dest_y);      /* +0xF0 */
                if (!UIPANEL_Blit(this->album_bg_surface,
                                  static_cast<uint32_t>(src_offset.left),
                                  static_cast<uint32_t>(src_offset.top),
                                  src_offset.right, static_cast<uint32_t>(src_offset.bottom),
                                  g_primary_surface,
                                  static_cast<uint32_t>(dest_rect.left),
                                  static_cast<uint32_t>(dest_rect.top),
                                  dest_rect.right, static_cast<uint32_t>(dest_rect.bottom),
                                  1)) {
                    OutputDebugStringA(s_AW_Blit_failure_reported_0047e0d8);
                }
            }
        }
    }

    /* Phase 2: Begin paint and draw text labels (gated by
     * scroll_wheel_enabled +0x12C — verified). Draws into the name
     * sprite rects with DT_SINGLELINE|DT_VCENTER|DT_CENTER (0x25). */
    HDC hdc = UIPANEL_BeginPaint(this);

    if (this->tiles_per_page != 0) {
        for (uint32_t i = 0; i < static_cast<uint32_t>(this->tiles_per_page); i++) {
            if (this->scroll_wheel_enabled == 1) {      /* +0x12C */
                SetBkMode(hdc, 1);                      /* TRANSPARENT */

                /* Set black text */
                COLORREF old_color = SetTextColor(hdc, 0x000000);  /* BLACK */

                /* Set transparent background */
                int old_bk_mode = SetBkMode(hdc, 1);

                /* Select small font */
                void* old_font = SelectObject(hdc, g_font_small);

                /* Draw tile name text into the name sprite's rect */
                RECT text_rect;
                ButtonSprite* sprite = this->row_name[i];
                text_rect.left   = sprite->x;
                text_rect.top    = sprite->y;
                text_rect.right  = sprite->sourceX;
                text_rect.bottom = sprite->sourceY;

                DrawTextA(hdc, this->tile_names[i], -1,
                          &text_rect, 0x25);   /* DT_SINGLELINE|DT_VCENTER|DT_CENTER */

                /* Restore GDI state (the binary emits the final
                 * SetBkMode twice — kept for byte-level fidelity) */
                SelectObject(hdc, old_font);
                SetTextColor(hdc, old_color);
                SetBkMode(hdc, old_bk_mode);
                SetBkMode(hdc, old_bk_mode);
            }
        }
    }

    /* End paint — (this, hWnd, hdc, flag=1, null) per the decompile */
    UIPANEL_EndPaintEx(this, (int)(intptr_t)this->hWnd,
                       (int)(intptr_t)hdc, 1, nullptr);

    /* Phase 3: Update navigation flags.
     * row_enabled[4] mirrors "at scroll top"; row_enabled[0] mirrors
     * "prev button enabled" (transition-triggered sprite update). */
    if (this->scroll_pixel_offset == 0) {
        this->row_enabled[4] = 1;             /* +0x1D8 */
        if (this->tile_index == 0) {          /* +0x118 */
            if (this->row_enabled[0] == 1) {  /* +0x1D4 */
                this->row_enabled[0] = 0;
                this->UpdateSprite(5);
            }
        } else {
            if (this->row_enabled[0] == 0) {
                this->row_enabled[0] = 1;
                this->UpdateSprite(5);
            }
        }
    } else {
        this->row_enabled[4] = 0;             /* +0x1D8 */
        if (this->row_enabled[0] == 0) {
            this->row_enabled[0] = 1;
            this->UpdateSprite(5);
        }
    }

    /* Update next button: row_enabled[5] mirrors "more pages remain";
     * row_enabled[1] mirrors "next button enabled". */
    uint32_t entry_count = g_pixel_cache->GetEntryCount();

    if (static_cast<uint32_t>(this->scroll_pixel_offset + this->tiles_per_page) < entry_count) {
        this->row_enabled[5] = 0;             /* +0x1D9 */
        if (this->row_enabled[1] == 0) {      /* +0x1D5 */
            this->row_enabled[1] = 1;
            this->UpdateSprite(6);
        }
    } else {
        this->row_enabled[5] = 1;             /* +0x1D9 */
        if (static_cast<uint32_t>(this->tile_index) > 7) {
            if (this->row_enabled[1] == 1) {
                this->row_enabled[1] = 0;
                this->UpdateSprite(6);
            }
            return;
        }
        if (this->row_enabled[1] == 0) {
            this->row_enabled[1] = 1;
            this->UpdateSprite(6);
        }
    }
}

#pragma GCC diagnostic pop
