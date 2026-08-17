/**
 * PostcardAlbum.cpp — PostcardAlbum implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Implements every method against the canonical real-inheritance model in
 * ui/PostcardAlbum.h (PostcardAlbum : public UI_WindowBase). This is the
 * sole PostcardAlbum implementation — a competing flat, non-inheriting
 * class of the same name that used to live in graphics/LOCOBITMAP.h/.cpp
 * (and mangled identically to several methods here, a duplicate-symbol /
 * silent-misbind hazard) has been deleted; see ui/PostcardAlbum.h's own
 * top-of-file doc comment and graphics/LOCOBITMAP.h's doc comment for the
 * resolution.
 *
 * Methods implemented here (see ui/PostcardAlbum.h for full per-method
 * doc comments and field-offset documentation):
 *   PostcardAlbum::PostcardAlbum(HINSTANCE, UINT)   — 0x401F50
 *   PostcardAlbum::CreateFromResource(void*, HINSTANCE, UINT) — 0x401F50
 *   PostcardAlbum::InitFromResource()               — 0x401FD0
 *   PostcardAlbum::~PostcardAlbum()                 — 0x401FB0 (vtable[0])
 *   PostcardAlbum::FreeAllSprites()                 — 0x402380
 *   PostcardAlbum::hide()                           — 0x402660 (vtable[1])
 *   PostcardAlbum::show()                           — 0x402590 (vtable[2])
 *   PostcardAlbum::on_rbutton_down()                — 0x4055E0 (vtable[16])
 *   PostcardAlbum::on_key_down() / PaintWindow       — 0x402690 (vtable[21])
 *   PostcardAlbum::FreeSprites()                    — 0x404830
 *   PostcardAlbum::InitWindow(HWND)                 — 0x402520
 *   PostcardAlbum::InitWindowSurface()              — 0x404720
 *   PostcardAlbum::InitSprites()                    — 0x404770
 *   PostcardAlbum::on_create()                      — 0x4028B0 (vtable[7])
 *   PostcardAlbum::BlitElement(int)                 — 0x403E80
 *   PostcardAlbum::UpdateSprite(int)                — 0x403BA0
 *   PostcardAlbum::RenderTileName(int)               — 0x4048E0
 *   PostcardAlbum::RenderAllTiles()                  — 0x404AC0
 *   PostcardAlbum::HitTest(int,int)                  — 0x403CD0
 *   PostcardAlbum::BlitToSurface(ButtonSprite*)      — not its own address
 *                                                       (factored helper)
 *
 * FreeAllSprites (0x402380) is not one of the originally-assigned
 * addresses, but was added because ~PostcardAlbum() calls it directly.
 * album_bg_resource/photo_bg_resource are `void*` (not `ResourceObject*`),
 * matching town/Town.h, ui/PostcardPreviewWindow.h, and
 * ui/NameEntryPanel.h's identical resource-field pattern) because on_create
 * needs RESDATA*-typed frame_width/frame_height reads off the same resource,
 * which a field-level ResourceObject* type cannot provide without a
 * forbidden reinterpret_cast — see ui/PostcardAlbum.h's field comments.
 *
 * show() unconditionally divides by tiles_per_page (+0x11C), which
 * InitFromResource zeroes and which is set (to 4 or 6, by is_high_res) by
 * the vtable[7] on_create() override below. show() calls this->on_create()
 * (after InitSprites(), which satisfies on_create's own sprites_inited
 * guard) before reaching the division, so tiles_per_page is guaranteed
 * non-zero on every reachable path. See on_create()'s own comment below and
 * ui/PostcardAlbum.h's doc comment for the full per-resolution layout.
 */

// Status: INTEGRATED

#include <new>

#include "PostcardAlbum.h"
#include "ButtonSprite.h"
#include "../resources/ResourceObject.h"
#include "../resources/ResourceManager.h"
#include "../graphics/PixelDataCache.h"
#include "../network/DPlayManager.h"   /* complete type needed for `delete selected_postcard_player` */
#include "../network/NetworkPlayerList.h"   /* g_dplay, RenderPlayer() -- used by RenderTileName */
#include "../platform/ddraw_interfaces.h"   /* IDirectDrawSurface4 -- RenderPlayer's surface param */

/* g_screen_width / g_screen_height — shared globals used by the high-res
 * detection below. Canonical definition: shared/stubs_impl.cpp. Same
 * extern shape as ui/TrainStationWindow.cpp / core/CGWND.cpp. */
extern int32_t g_screen_width;   /* 0x4851D8 */
extern int32_t g_screen_height;  /* 0x485214 */

/* g_resmgr (the one canonical ResourceManager object, 0x4855E8) is already
 * declared by resources/ResourceManager.h, included above — same pattern
 * as ui/PostcardPreviewWindow.cpp / ui/GameSetupPanel.cpp. */

/* g_dplay_config — the LIVE, actually-populated PixelDataCache* singleton
 * at 0x4FD3B4 (constructed by GameLoop_Setup, core/GameLoop.cpp:272; typed
 * PixelDataCache* the same way core/CGWND.cpp's own extern already does).
 *
 * NOT the same storage as graphics/PixelDataCache.h's own declared
 * `g_pixel_cache`: that is a SEPARATE global at the same conceptual
 * address that is never assigned anywhere in
 * the tree (grepped: zero writes) — a permanently-null shadow, the same
 * landmine class as the historical `_g_dplay` shadow documented in
 * docs/landmine-sweep-worklist.md. `g_dplay_config` (despite its
 * misleading, historically-inherited name — it has nothing to do with
 * DirectPlay networking) is the one that is actually populated at runtime,
 * so it is used here rather than the correctly-named-but-dead
 * `g_pixel_cache`. Resolving that split-global landmine tree-wide is out of
 * scope for this pass; flagged in the final report. */
extern PixelDataCache* g_dplay_config;   /* 0x4FD3B4 */

/* Win32 API used by InitWindow/show/on_key_down below.
 *
 * Deliberately `extern "C"`, unlike several sibling ui/*.cpp files
 * (HelpWnd.cpp, NameEntryPanel.cpp, GameSetupPanel.cpp) that declare
 * LoadIconA etc. at plain C++ linkage -- verified those are the broken
 * ones: graphics/sdl3_window.h wraps ALL of these symbols (LoadIconA,
 * ShowWindow, GetClientRect, GetDesktopWindow, DefWindowProcA, SetFocus,
 * Sleep) in one `extern "C" { ... }` block spanning the entire header
 * (lines 26-337), and graphics/sdl3_window.cpp's real definitions inherit
 * that C linkage from the header declaration -- confirmed by inspecting
 * both files directly, not by matching the (wrong) sibling pattern.
 * shared/stubs_link001_batch1_crt_win32.cpp's own C++-linkage LoadIconA
 * stub exists specifically to catch callers with the sibling files' wrong
 * linkage. */
extern "C" {
    extern HWND    GetDesktopWindow(void);
    extern BOOL    GetClientRect(HWND hWnd, RECT* lpRect);
    extern HICON   LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName);
    extern BOOL    ShowWindow(HWND hWnd, int nCmdShow);
    extern HWND    SetFocus(HWND hWnd);
    extern LRESULT __stdcall DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
    extern void    Sleep(DWORD dwMilliseconds);

    /* Win32 GDI RECT helpers used by on_create() below (0x4028B0) --
     * same graphics/sdl3_window.h extern "C" block, same signatures as the
     * established precedent in ui/NameEntryPanel.cpp / ui/GameSetupPanel.cpp. */
    extern void SetRectEmpty(RECT* lprc);
    extern BOOL CopyRect(RECT* lprcDst, const RECT* lprcSrc);
    extern BOOL OffsetRect(RECT* lprc, int dx, int dy);

    /* Used by HitTest()/BlitToSurface()/RenderAllTiles() below -- same
     * graphics/sdl3_window.h extern "C" block (spans PtInRect at line 206,
     * OutputDebugStringA at line 216, and the GDI text-drawing group at
     * lines 196-201), matching the established precedent in
     * ui/GameSetupPanel.cpp / ui/NameEntryPanel.cpp for these same symbols. */
    extern BOOL     PtInRect(const RECT* lprc, POINT pt);
    extern void     OutputDebugStringA(const char* lpOutputString);
    extern HGDIOBJ  SelectObject(HDC hdc, HGDIOBJ hgdiobj);
    extern COLORREF SetTextColor(HDC hdc, COLORREF color);
    extern int      SetBkMode(HDC hdc, int mode);
    extern int      DrawTextA(HDC hdc, LPCSTR lpchText, int cchText,
                               RECT* lprc, UINT format);
}

/* CGWND_SetMode — core game-mode state machine (core/CGWND.h). Local
 * extern to avoid pulling in CGWND.h's much heavier dependency set, same
 * pattern as ui/HelpWnd.cpp. */
extern void CGWND_SetMode(int new_mode);   /* 0x408130 */

/* UIPANEL_Blit -- central blit dispatcher (ui/UIPANEL_Surface.cpp, 0x42B050,
 * 105+ callers tree-wide). C++ linkage (not extern "C" -- the real def is a
 * C++-mangled member-shaped free function; matching declaration already
 * used by ui/UI_WindowBase.cpp, copied verbatim here for the identical
 * mangled symbol). Used by BlitToSurface() below. */
extern bool __thiscall UIPANEL_Blit(void* renderer, uint32_t src_x, uint32_t src_y,
    int32_t dest_x, uint32_t dest_y, void* dest_surface, uint32_t clip_x, uint32_t clip_y,
    int32_t clip_w, uint32_t clip_h, uint32_t flags);                        /* 0x42B050 */

/* g_primary_surface -- DDraw primary surface, used as UIPANEL_Blit's
 * destination in BlitToSurface() and as RenderTileName's RenderPlayer
 * surface argument (cast to IDirectDrawSurface4* there). Same declaration
 * as ui/UI_WindowBase.cpp. */
extern void* g_primary_surface;   /* 0x4FD3C4 */

/* g_font_small -- shared small UI font handle, used by RenderAllTiles()'s
 * DrawTextA call. Address: 0x004855F4. */
extern void* g_font_small;        /* 0x4855F4 */

/* UI_CenterWindow — centers `inner` rect in place within `outer` rect
 * (outer left unmodified). Address: 0x425A50, __cdecl. Real definition:
 * shared/stubs_impl.cpp (canonical int*,int* body) plus a RECT*,RECT*
 * forwarding overload in shared/stubs_link001_batch5_ui_graphics.cpp --
 * same declaration style as ui/NameEntryPanel.cpp / network/DPlayManager.cpp. */
extern void __cdecl UI_CenterWindow(RECT* outer, RECT* inner);   /* 0x425A50 */

namespace {

/* Builds a RECT{left,top,right,bottom} view of a ButtonSprite's own
 * x/y/sourceX/sourceY fields -- the same dual-use-as-bounding-box read
 * ButtonSprite.h's own field comments document (NameEntryPanel::
 * on_lbutton_down uses the identical pattern). Not a stored field on
 * ButtonSprite itself -- see that header's comment for why. */
RECT sprite_rect(const ButtonSprite& sprite)
{
    RECT rect{};
    rect.left   = sprite.x;
    rect.top    = sprite.y;
    rect.right  = sprite.sourceX;
    rect.bottom = sprite.sourceY;
    return rect;
}

bool sprite_contains(const ButtonSprite* sprite, int x, int y)
{
    RECT rect = sprite_rect(*sprite);
    POINT pt{ x, y };
    return PtInRect(&rect, pt) != 0;
}

} // namespace

/* ================================================================== */
/* PostcardAlbum::PostcardAlbum                                        */
/* Address: 0x401F50                                                   */
/*                                                                     */
/* Chains to UI_WindowBase(hInstance, resId) (0x425870), then calls    */
/* InitFromResource() to allocate all album fields and sprites. In the */
/* binary, UI_WindowBase's constructor sets the vtable to 0x477C30,    */
/* then this constructor overwrites it to 0x4773F0 before calling      */
/* InitFromResource — both vtable pokes are compiler-managed here.     */
/* ================================================================== */
PostcardAlbum::PostcardAlbum(HINSTANCE hInstance, UINT resId)
    : UI_WindowBase(hInstance, resId)
{
    InitFromResource();
}

/* ================================================================== */
/* PostcardAlbum::CreateFromResource                                   */
/* Address: 0x401F50 (same address as the constructor — see the        */
/* header's doc comment: the original caller, CGWND_InitAllSubsystems  */
/* @ 0x4072B9, is simply `operator_new(0x254)` followed by a direct    */
/* call into this constructor with ECX = the allocated pointer; the    */
/* two-step "factory" shape is what the MSVC compiler already emits    */
/* for a plain `new PostcardAlbum(hInstance, resId)` expression, not a */
/* distinct function). This wrapper exists because core/CGWND.cpp's    */
/* faithful `_WIN32` branch folds `operator_new(...)` into the call's  */
/* own argument rather than using a `new` expression directly.         */
/* ================================================================== */
PostcardAlbum* PostcardAlbum::CreateFromResource(void* mem, HINSTANCE hInstance, UINT resId)
{
    return (mem != nullptr) ? new (mem) PostcardAlbum(hInstance, resId) : nullptr;
}

/* ================================================================== */
/* PostcardAlbum::InitFromResource                                     */
/* Address: 0x401FD0 (__fastcall, ECX = this)                          */
/*                                                                     */
/* Zeroes all album state, detects high-resolution mode, creates 8     */
/* button sprites (res 0x3C04-0x3C0F), 6 row groups (icon/tile/name     */
/* sprites), 9 tile label sprites, and sets all element enable flags   */
/* to 1. Every `new ButtonSprite(id)` below reproduces the original's  */
/* `operator_new(0x24)` + null-check + ButtonSprite_Ctor(id) sequence —*/
/* exactly what a plain `new ButtonSprite(id)` expression compiles to. */
/* ================================================================== */
void PostcardAlbum::InitFromResource()
{
    icon_handle           = nullptr;   /* +0xE8 */
    sprites_inited         = 0;         /* +0x111 */
    scroll_pixel_offset    = 0;         /* +0x114 */
    tile_index             = 0;         /* +0x118 */
    tiles_per_page         = 0;         /* +0x11C */
    hovered_tile           = 0;         /* +0x120 */
    tile_count_init        = 9;         /* +0x124 */
    scroll_wheel_pos        = 0;         /* +0x128 */
    scroll_wheel_enabled    = 1;         /* +0x12C */
    text_rendered          = 0;         /* +0x112 */
    active_flag            = 0;         /* +0x110 */

    /* High-res detection: low-res (0) iff width < 801 AND height < 601;
     * high-res (1) otherwise. Signed comparison (original uses JG). */
    if (g_screen_width < 0x321 && g_screen_height < 0x259) {
        is_high_res = 0;
    } else {
        is_high_res = 1;
    }

    btn_close       = new ButtonSprite(0x3C04);
    btn_delete      = new ButtonSprite(0x3C09);
    btn_save        = new ButtonSprite(0x3C05);
    btn_rotate      = new ButtonSprite(0x3C08);
    btn_print       = new ButtonSprite(0x3C0F);
    btn_prev        = new ButtonSprite(0x3C06);
    btn_next        = new ButtonSprite(0x3C07);
    btn_scrollwheel = new ButtonSprite(is_high_res ? 0x3C0D : 0x3C0C);

    /* 6 album rows: icon (res 0), tile preview (res 0x3C0E), name (res 0). */
    for (int row = 0; row < 6; ++row) {
        row_icon[row] = new ButtonSprite(0);
        row_tile[row] = new ButtonSprite(0x3C0E);
        row_name[row] = new ButtonSprite(0);
        tile_names[row][0] = '\0';
    }

    for (int i = 0; i < 9; ++i) {
        tile_label_sprites[i] = new ButtonSprite(0);
    }

    for (int i = 0; i < 6; ++i) {
        row_enabled[i] = 1;
    }

    album_bg_resource        = nullptr; /* +0x138 */
    selected_postcard_player = nullptr; /* +0x130 */
    window_surface_inited    = 0;       /* +0xFC */

    /* bg_blit_rect/layout_rect (+0xEC/+0x100) are never written by the
     * original 0x401FD0 — left uninitialized here too, matching that
     * (verified: no store to either offset anywhere in this function).
     * Both are unconditionally written by on_create() (0x4028B0) before
     * anything reads them, once sprites_inited is set — see on_create(). */
}

/* ================================================================== */
/* PostcardAlbum::~PostcardAlbum (vtable[0] deleting destructor)       */
/* Address: 0x401FB0                                                    */
/*                                                                     */
/* Original body: FreeAllSprites(this); if (flags & 1) GLOBAL_free(this);*/
/* return this; — the flag check and heap free are the MSVC scalar     */
/* deleting-destructor's own compiler-generated wrapper around the real */
/* destructor body (FreeAllSprites), reproduced here for free by a      */
/* plain `delete` expression / virtual destructor; only the real        */
/* cleanup (FreeAllSprites) is user code.                                */
/* ================================================================== */
PostcardAlbum::~PostcardAlbum()
{
    FreeAllSprites();
}

/* ================================================================== */
/* PostcardAlbum::FreeAllSprites                                        */
/* Address: 0x402380 (__fastcall, ECX = this)                           */
/*                                                                      */
/* Full destructor body. Order faithfully reproduces the original:      */
/*   1. FreeSprites() if sprites_inited (+0x111).                       */
/*   2. Release album_bg_resource via ResourceObject::Unlock() (the     */
/*      original's vtable-slot-2/no-argument call) and null it.         */
/*      album_bg_surface (+0x13C) is NOT cleared here — verified         */
/*      against the disassembly; a genuine original quirk.               */
/*   3. Scalar-delete the 7 individually-named button sprites.          */
/*   4. Scalar-delete all 18 row sprites (icon/name/tile order, per     */
/*      instruction order in the original: icon, then name, then tile). */
/*   5. Scalar-delete all 9 tile label sprites.                          */
/*   6. Scalar-delete btn_scrollwheel and selected_postcard_player       */
/*      (DPlayManager*) last, matching the original's tail ordering.     */
/* Base cleanup (UI_WindowBase's own destructor body) runs implicitly    */
/* via the compiler-generated base-destructor call after ~PostcardAlbum's*/
/* body completes — not reproduced here as an explicit call.             */
/*                                                                      */
/* UPDATE (this pass): FreeSprites() (0x404830) and InitSprites()        */
/* (0x404770) are now both defined below, so sprites_inited (+0x111) is  */
/* a live path -- InitSprites() sets it true, show()/InitWindow() reach  */
/* it via the ordinary construction/show flow, and this FreeSprites()    */
/* call is genuinely exercised, not merely declared-and-unreached.       */
/* ================================================================== */
void PostcardAlbum::FreeAllSprites()
{
    if (sprites_inited) {
        FreeSprites();
    }

    if (album_bg_resource != nullptr) {
        static_cast<ResourceObject*>(album_bg_resource)->Unlock();
        album_bg_resource = nullptr;
    }

    delete btn_close;  btn_close  = nullptr;
    delete btn_delete; btn_delete = nullptr;
    delete btn_save;   btn_save   = nullptr;
    delete btn_rotate; btn_rotate = nullptr;
    delete btn_print;  btn_print  = nullptr;
    delete btn_prev;   btn_prev   = nullptr;
    delete btn_next;   btn_next   = nullptr;

    for (int row = 0; row < 6; ++row) {
        delete row_icon[row]; row_icon[row] = nullptr;
        delete row_name[row]; row_name[row] = nullptr;
        delete row_tile[row]; row_tile[row] = nullptr;
    }

    for (int i = 0; i < 9; ++i) {
        delete tile_label_sprites[i];
        tile_label_sprites[i] = nullptr;
    }

    delete btn_scrollwheel;
    btn_scrollwheel = nullptr;

    delete selected_postcard_player;
    selected_postcard_player = nullptr;
}

/* ================================================================== */
/* PostcardAlbum::hide (vtable[1])                                     */
/* Address: 0x402660 (Ghidra name "PostcardAlbum_DestroyWindow" is      */
/* misleading — it never calls the real Win32 DestroyWindow API, only  */
/* UI_WindowBase::hide()).                                             */
/*                                                                     */
/* Guard is UI_WindowBase::visible (+0xE4) — verified via disassembly, */
/* NOT this class's own active_flag (+0x110).                          */
/* ================================================================== */
void PostcardAlbum::hide()
{
    if (this->visible) {
        UI_WindowBase::hide();
        this->text_rendered = 0;   /* +0x112 */
        FreeSprites();
    }
}

/* ================================================================== */
/* PostcardAlbum::show (vtable[2])                                     */
/* Address: 0x402590 (dispatched by CGWND_SetMode(6) @ 0x408216)       */
/*                                                                     */
/* RESOLVED (former hard blocker, see ui/PostcardAlbum.h's top banner):  */
/* this method's division by tiles_per_page (+0x11C) is unconditional   */
/* in the original too, and the original's tiles_per_page is guaranteed */
/* non-zero by the time show() runs because the vtable[7] on_create     */
/* override (0x4028B0, ported below) has already set it to 4 or 6 -- via*/
/* the `this->on_create()` dispatch immediately below. InitSprites()    */
/* (called first, on the line above) sets sprites_inited before         */
/* on_create() runs, satisfying on_create's own sprites_inited guard,   */
/* so tiles_per_page is genuinely non-zero on every reachable call.     */
/* ================================================================== */
void PostcardAlbum::show()
{
    InitSprites();

    /* vtable[7] dispatch -- sets tiles_per_page (+0x11C), see on_create(). */
    this->on_create();

    UI_WindowBase::show();
    ShowWindow(this->hWnd, 3);   /* SW_SHOWMAXIMIZED */
    SetFocus(this->hWnd);

    if (this->selected_postcard_player != nullptr) {   /* +0x130 */
        delete this->selected_postcard_player;
        this->selected_postcard_player = nullptr;
    }

    /* Original: `(**(*this+0xC))(param_1[0x18], param_1[0x19], 0, 1)` --
     * vtable[3] = set_mode(), receiving this base object's own child-slot-0
     * pair (childCount0/childObj0, +0x60/+0x64) as its first two arguments.
     * Preserved verbatim; not a defect introduced by this port. */
    this->set_mode(this->childCount0, this->childObj0, 0, 1);

    /* Original: `*(undefined1 *)(param_1 + 0x44) = 0;` where param_1 is
     * `int*` -- Ghidra's own pointer-arithmetic stride (x4) makes this byte
     * offset 0x110, i.e. active_flag, NOT UI_WindowBase's own +0x44 field.
     * Confirmed against this class's header field layout. */
    this->active_flag = 0;   /* +0x110 */

    /* Restore scroll position from PixelDataCache's (g_dplay_config) temp
     * fields. See ui/PostcardAlbum.cpp's own top banner for why
     * g_dplay_config (not graphics/PixelDataCache.h's g_pixel_cache) is the
     * correct, live global to read here. */
    int32_t savedAlbumIndex = g_dplay_config->saved_album_index;               /* +0x14 */
    uint32_t insertIndex = static_cast<uint32_t>(g_dplay_config->insert_index); /* +0x10 */

    /* tiles_per_page is non-zero here (set to 4 or 6 by on_create() above);
     * this unsigned modulo is safe on every reachable path. This line
     * executes unconditionally in the original too (the `>= 0` guard below
     * only gates what happens AFTER the division, not the division itself)
     * -- preserved faithfully. */
    int32_t newScrollOffset = static_cast<int32_t>(
        insertIndex - insertIndex % static_cast<uint32_t>(this->tiles_per_page));

    if (newScrollOffset >= 0 && savedAlbumIndex >= 0) {
        if (this->scroll_pixel_offset != newScrollOffset ||
            this->scroll_wheel_pos != savedAlbumIndex) {
            this->scroll_wheel_pos = savedAlbumIndex;   /* +0x128 */
            this->tile_index = savedAlbumIndex;          /* +0x118 */
            this->scroll_pixel_offset = 0;               /* +0x114 */
            RenderAllTiles();
            this->scroll_pixel_offset = newScrollOffset;
        }
        g_dplay_config->insert_index = -1;
        g_dplay_config->saved_album_index = -1;
    }
}

/* ================================================================== */
/* PostcardAlbum::on_rbutton_down (vtable[16])                         */
/* Address: 0x4055E0 (__thiscall, RET 0x10 -- confirms all 4 base       */
/* on_rbutton_down(HWND,UINT,WPARAM,LPARAM) stack args are genuinely    */
/* declared by the original, even though the body ignores every one).   */
/*                                                                      */
/* Ghidra auto-named this "GameObject_OnTimerTick" with a misleading     */
/* plate comment ("vtable[0x30] for GameObject... calls GameObject_     */
/* Update"); both were wrong. Verified: its sole DATA xref is this       */
/* class's own vtable slot 16 (0x477430), and dispatch_message (0x426140)*/
/* shows vtable+0x40 is reached ONLY via WM_RBUTTONDOWN (0x204). Renamed */
/* in Ghidra to PostcardAlbum_OnRButtonDown.                             */
/* ================================================================== */
LRESULT PostcardAlbum::on_rbutton_down(HWND /*hWnd*/, UINT /*msg*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
    if (this->selected_postcard_player != nullptr) {   /* +0x130 */
        delete this->selected_postcard_player;
        this->selected_postcard_player = nullptr;
        /* Identical call to the one in show() above. */
        this->set_mode(this->childCount0, this->childObj0, 0, 1);
    }
    return 0;
}

/* ================================================================== */
/* PostcardAlbum::on_key_down (vtable[21], "PaintWindow" in Ghidra)     */
/* Address: 0x402690 (__thiscall)                                       */
/*                                                                      */
/* Switches on wParam (virtual key code), not msg -- msg is only used   */
/* in the default/DefWindowProcA fallthrough, whose result is RETURNED  */
/* directly (unlike UI_WindowBase's default message slots, which always */
/* return 0 regardless of DefWindowProcA's result -- a real, verified    */
/* difference from the base class, preserved here).                     */
/* ================================================================== */
LRESULT PostcardAlbum::on_key_down(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (this->active_flag) {   /* +0x110 -- album busy, ignore input */
        return 0;
    }

    switch (wParam) {
    case 0x0D:   /* VK_RETURN */
    case 0x1B: { /* VK_ESCAPE */
        this->hide();          /* vtable[1] */
        CGWND_SetMode(3);
        return 0;
    }

    case 0x25: {  /* VK_LEFT */
        if (this->row_enabled[0] != 1) {   /* +0x1D4 */
            return 0;
        }
        BlitElement(5);
        this->EndPaintEx(nullptr, false, nullptr);
        Sleep(0x96);
        UpdateSprite(5);

        if (this->row_enabled[4] == 1) {   /* +0x1D8 scroll_up_visible */
            int32_t newScrollWheelPos = this->scroll_wheel_pos - 1;
            this->scroll_wheel_pos = newScrollWheelPos;   /* +0x128 */
            this->tile_index = newScrollWheelPos;          /* +0x118 */

            uint32_t entryCount = static_cast<uint32_t>(g_dplay_config->Unlock(newScrollWheelPos));
            this->scroll_pixel_offset =
                static_cast<int32_t>(entryCount / static_cast<uint32_t>(this->tiles_per_page));

            uint32_t totalEntries = static_cast<uint32_t>(g_dplay_config->GetEntryCount());
            if (totalEntries % static_cast<uint32_t>(this->tiles_per_page) == 0 &&
                g_dplay_config->GetEntryCount() != 0) {
                this->scroll_pixel_offset -= 1;
            }
            this->scroll_pixel_offset = this->tiles_per_page * this->scroll_pixel_offset;

            this->btn_scrollwheel->setState(this->scroll_wheel_pos, nullptr);
        } else {
            this->scroll_pixel_offset -= this->tiles_per_page;
        }
        break;
    }

    case 0x27: {  /* VK_RIGHT */
        if (this->row_enabled[1] != 1) {   /* +0x1D5 */
            return 0;
        }
        BlitElement(6);
        this->EndPaintEx(nullptr, false, nullptr);
        Sleep(0x96);
        UpdateSprite(6);

        if (this->row_enabled[5] == 1) {   /* +0x1D9 scroll_down_visible */
            int32_t newScrollWheelPos = this->scroll_wheel_pos + 1;
            this->scroll_wheel_pos = newScrollWheelPos;   /* +0x128 */
            this->tile_index = newScrollWheelPos;          /* +0x118 */
            this->scroll_pixel_offset = 0;                 /* +0x114 */
            this->btn_scrollwheel->setState(newScrollWheelPos, nullptr);
        } else {
            this->scroll_pixel_offset += this->tiles_per_page;
        }
        break;
    }

    default:
        return DefWindowProcA(hWnd, msg, wParam, lParam);
    }

    RenderAllTiles();
    this->EndPaintEx(nullptr, false, nullptr);
    return 0;
}

/* ================================================================== */
/* PostcardAlbum::FreeSprites                                           */
/* Address: 0x404830 (__fastcall, ECX = this)                           */
/*                                                                      */
/* Releases photo_bg_resource via ResourceObject::Unlock() (vtable[2]), */
/* calls ButtonSprite::destroy() (NOT delete -- these sprites remain    */
/* owned/alive, only their sub-resource is released) on the 8 named     */
/* button sprites and the 6 row_tile sprites. Clears sprites_inited.    */
/* photo_bg_surface (+0x144) is NOT cleared here -- verified against    */
/* the disassembly; the same stale-pointer-after-release quirk already  */
/* documented for album_bg_surface in FreeAllSprites, preserved as-is.  */
/* ================================================================== */
void PostcardAlbum::FreeSprites()
{
    if (!this->sprites_inited) {   /* +0x111 */
        return;
    }

    static_cast<ResourceObject*>(this->photo_bg_resource)->Unlock();   /* vtable[2] */
    this->photo_bg_resource = nullptr;   /* +0x140 */

    this->btn_close->destroy();
    this->btn_delete->destroy();
    this->btn_save->destroy();
    this->btn_rotate->destroy();
    this->btn_print->destroy();
    this->btn_prev->destroy();
    this->btn_next->destroy();
    this->btn_scrollwheel->destroy();

    for (int row = 0; row < 6; ++row) {
        this->row_tile[row]->destroy();
    }

    this->sprites_inited = 0;
}

/* ================================================================== */
/* PostcardAlbum::InitWindow                                            */
/* Address: 0x402520 (__thiscall)                                       */
/*                                                                      */
/* See ui/PostcardAlbum.h's own doc comment for the VERIFIED note that  */
/* the binary calls this exact address on both PostcardAlbum and        */
/* PostcardPreviewWindow objects (core/CGWND.cpp's host port reproduces */
/* this via an explicit cast at its own call site).                     */
/* ================================================================== */
bool PostcardAlbum::InitWindow(HWND hParent)
{
    HWND desktop = GetDesktopWindow();
    RECT desktopRect;
    GetClientRect(desktop, &desktopRect);

    /* ABI_BOUNDARY: MAKEINTRESOURCE(0x65) -- Win32 resource-ID-as-pointer
     * convention for LoadIconA's lpIconName parameter. */
    this->icon_handle = LoadIconA(this->hInstance, reinterpret_cast<LPCSTR>(0x65));

    int created = this->create_full_window(
        0, hParent,
        desktopRect.left, desktopRect.top,
        desktopRect.right - desktopRect.left,
        desktopRect.bottom - desktopRect.top,
        nullptr, this->icon_handle, 0);

    return created != 0;
}

/* ================================================================== */
/* PostcardAlbum::InitWindowSurface                                     */
/* Address: 0x404720 (__fastcall, ECX = this)                           */
/*                                                                      */
/* One-shot (guarded by window_surface_inited, +0xFC): loads the album  */
/* background resource (0x3C0A low-res / 0x3C0B high-res) and its       */
/* Lock(0,0) surface.                                                   */
/* ================================================================== */
void PostcardAlbum::InitWindowSurface()
{
    if (this->window_surface_inited) {   /* +0xFC */
        return;
    }

    int32_t resId = (this->is_high_res == 0) ? 0x3C0A : 0x3C0B;

    /* ABI_BOUNDARY: ResourceManager::GetById returns the resource address
     * encoded as int32_t (an original x86 ABI artifact -- see the
     * established precedent in ui/PostcardPreviewWindow.cpp's
     * init_background()/init_sprites(), same idiom). */
    int32_t res = g_resmgr.GetById(resId);
    void* resource = reinterpret_cast<void*>(
        static_cast<uintptr_t>(static_cast<uint32_t>(res)));

    this->album_bg_resource = resource;          /* +0x138 */
    this->album_bg_surface = static_cast<ResourceObject*>(resource)->Lock(0, 0);   /* +0x13C, vtable[1] */
    this->window_surface_inited = 1;
}

/* ================================================================== */
/* PostcardAlbum::InitSprites                                           */
/* Address: 0x404770 (__fastcall, ECX = this)                           */
/*                                                                      */
/* One-shot (guarded by sprites_inited, +0x111): calls ButtonSprite::    */
/* init() on the 8 named button sprites and the 6 row_tile sprites,      */
/* then loads the photo overlay resource (0x3CFA) and its Lock(0,0)      */
/* surface.                                                              */
/* ================================================================== */
void PostcardAlbum::InitSprites()
{
    if (this->sprites_inited) {   /* +0x111 */
        return;
    }

    this->btn_close->init();
    this->btn_delete->init();
    this->btn_save->init();
    this->btn_rotate->init();
    this->btn_print->init();
    this->btn_prev->init();
    this->btn_next->init();
    this->btn_scrollwheel->init();

    for (int row = 0; row < 6; ++row) {
        this->row_tile[row]->init();
    }

    /* ABI_BOUNDARY: see InitWindowSurface() above for why GetById's result
     * needs this int-to-pointer conversion. */
    int32_t res = g_resmgr.GetById(0x3CFA);
    void* resource = reinterpret_cast<void*>(
        static_cast<uintptr_t>(static_cast<uint32_t>(res)));

    this->photo_bg_resource = resource;          /* +0x140 */
    this->photo_bg_surface = static_cast<ResourceObject*>(resource)->Lock(0, 0);   /* +0x144, vtable[1] */
    this->sprites_inited = 1;
}

/* ================================================================== */
/* PostcardAlbum::on_create (vtable[7])                                */
/* Address: 0x4028B0 (__fastcall, ECX = this)                          */
/*                                                                     */
/* Per-resolution absolute sprite-rect layout. See ui/PostcardAlbum.h's*/
/* on_create() doc comment for the full step-by-step description; the */
/* helper lambdas below each reproduce one repeated shape from the      */
/* decompiled 0x4028B0 (button/name/tile rects are all "take a copy of */
/* layout_rect, offset it by a fixed (dx,dy), then size it" -- the      */
/* per-sprite literal offsets are transcribed directly from the         */
/* decompile/disassembly, not re-derived).                              */
/*                                                                      */
/* Ghidra auto-named this function "PostcardAlbum_OnCreate" already      */
/* correctly (not a misleading name) -- no rename needed in the         */
/* database, only a decompiler comment (see end of this file's diffs). */
/*                                                                      */
/* PRECONDITION (see ui/PostcardAlbum.h's on_create() doc comment for   */
/* the full trace): album_bg_resource (+0x138) is dereferenced          */
/* unconditionally below. It is populated by InitWindowSurface(), gated */
/* on window_surface_inited (+0xFC) -- a DIFFERENT flag from this       */
/* method's own sprites_inited (+0x111) guard, and show() never calls   */
/* InitWindowSurface() itself. The non-null guarantee instead comes     */
/* from game-mode ordering (CGWND_InitMode1, 0x408350, always calls     */
/* PostcardAlbum_InitWindowSurface(g_postcard) before the game can ever  */
/* reach mode 3/town, and mode 6 -- the only path to show()/on_create() */
/* -- is only reachable from mode 3) -- verified via CGWND_SetMode's     */
/* (0x408130) call graph, not assumed.                                  */
/* ================================================================== */
void PostcardAlbum::on_create()
{
    if (!this->sprites_inited) {   /* +0x111 -- matches disasm: JZ past the
                                     * entire function body, not even the
                                     * base on_create() call runs. */
        return;
    }

    UI_WindowBase::on_create();   /* populates clientRect/workRect, +0xC4/+0xD4 */

    if (this->is_high_res != 0 && this->is_high_res != 1) {
        return;   /* UNREACHABLE: is_high_res is only ever 0 or 1 (set by
                    * InitFromResource's g_screen_width/height check); this
                    * mirrors the original's `if (is_high_res != 1) return`
                    * in its is_high_res-nonzero branch verbatim. */
    }

    /* Design-resolution reference rect, then centered in place within the
     * actual window client rect (workRect) -- accounts for a real desktop
     * larger than the assumed 800x600/1024x768 design resolution. */
    if (this->is_high_res == 0) {
        this->layout_rect = RECT{0, 0, 800, 600};
    } else {
        this->layout_rect = RECT{0, 0, 1024, 768};
    }
    UI_CenterWindow(&this->workRect, &this->layout_rect);

    /* Background-image bounding rect (its own pixel dimensions), used to
     * center the window rect within the loaded album background bitmap. */
    RECT imageRect;
    SetRectEmpty(&imageRect);
    imageRect.right  = static_cast<RESDATA*>(this->album_bg_resource)->frame_width;
    imageRect.bottom = static_cast<RESDATA*>(this->album_bg_resource)->frame_height;

    CopyRect(&this->bg_blit_rect, &this->workRect);
    UI_CenterWindow(&imageRect, &this->bg_blit_rect);

    /* Places `sprite`'s own rect (x/y/sourceX/sourceY, read elsewhere as
     * left/top/right/bottom -- see BlitElement/HitTest) sized from its
     * pixel-data frame dimensions, offset by (dx,dy) from layout_rect. */
    auto placeButtonRect = [this](ButtonSprite* sprite, int32_t dx, int32_t dy) {
        RECT r = this->layout_rect;
        r.right  = r.left + static_cast<RESDATA*>(sprite->pixelData)->frame_width;
        r.bottom = r.top  + static_cast<RESDATA*>(sprite->pixelData)->frame_height;
        OffsetRect(&r, dx, dy);
        sprite->x       = r.left;
        sprite->y       = r.top;
        sprite->sourceX = r.right;
        sprite->sourceY = r.bottom;
    };

    /* Places `sprite`'s rect at a fixed (dx,dy) offset from layout_rect with
     * an explicit (width,height) -- used for the row_icon/row_tile arrays. */
    auto placeFixedRect = [this](ButtonSprite* sprite, int32_t dx, int32_t dy,
                                  int32_t width, int32_t height) {
        RECT r = this->layout_rect;
        OffsetRect(&r, dx, dy);
        r.right  = r.left + width;
        r.bottom = r.top + height;
        sprite->x       = r.left;
        sprite->y       = r.top;
        sprite->sourceX = r.right;
        sprite->sourceY = r.bottom;
    };

    /* Same as placeFixedRect, plus a second vertical offset (extraDy) --
     * used for the row_name array (icon/tile rect, then dropped down onto
     * the name-label strip beneath it). */
    auto placeNameRect = [this](ButtonSprite* sprite, int32_t dx, int32_t dy,
                                 int32_t extraDy, int32_t width, int32_t height) {
        RECT r = this->layout_rect;
        OffsetRect(&r, dx, dy);
        OffsetRect(&r, 0, extraDy);
        r.right  = r.left + width;
        r.bottom = r.top + height;
        sprite->x       = r.left;
        sprite->y       = r.top;
        sprite->sourceX = r.right;
        sprite->sourceY = r.bottom;
    };

    /* Places the scrollwheel sprite (sized from its own frame dimensions,
     * offset by dx,dy) plus the 9 tile_label_sprites stacked beneath it,
     * each 30px wide and 1/9th of the scrollwheel's own height tall. */
    auto placeScrollwheelAndLabels = [this](int32_t dx, int32_t dy) {
        RECT r = this->layout_rect;
        OffsetRect(&r, dx, dy);
        const auto* frame = static_cast<RESDATA*>(this->btn_scrollwheel->pixelData);
        int32_t right  = r.left + frame->frame_width;
        int32_t bottom = r.top  + frame->frame_height;

        this->btn_scrollwheel->x       = r.left;
        this->btn_scrollwheel->y       = r.top;
        this->btn_scrollwheel->sourceX = right;
        this->btn_scrollwheel->sourceY = bottom;

        int32_t sliceHeight = (bottom - r.top) / 9;
        int32_t labelLeft  = r.left;
        int32_t labelRight = r.left + 0x1e;
        int32_t labelTop   = r.top;
        for (int i = 0; i < 9; ++i) {
            this->tile_label_sprites[i]->x       = labelLeft;
            this->tile_label_sprites[i]->y       = labelTop;
            this->tile_label_sprites[i]->sourceX = labelRight;
            this->tile_label_sprites[i]->sourceY = labelTop + sliceHeight;
            labelTop += sliceHeight;
        }
    };

    if (this->is_high_res == 0) {
        /* ---- 800x600 layout (tiles_per_page = 4) ---- */
        placeButtonRect(this->btn_close,  0x26c, 0x20f);
        placeButtonRect(this->btn_delete, 0x69,  0x20f);
        placeButtonRect(this->btn_save,   0x19,  0x20f);
        placeButtonRect(this->btn_rotate, 0xb9,  0x20f);
        placeButtonRect(this->btn_print,  0x108, 0x20f);
        placeButtonRect(this->btn_prev,   0x21e, 0x20f);
        placeButtonRect(this->btn_next,   700,   0x20f);

        placeScrollwheelAndLabels(0x2f5, 0xf);

        this->tiles_per_page = 4;   /* +0x11C */

        placeFixedRect(this->row_icon[0], 0x2d,  0x22,  300, 200);
        placeFixedRect(this->row_icon[1], 0x2d,  0x106, 300, 200);
        placeFixedRect(this->row_icon[2], 0x1be, 0x22,  300, 200);
        placeFixedRect(this->row_icon[3], 0x1be, 0x106, 300, 200);

        placeNameRect(this->row_name[0], 0x2d,  0x22,  0xcb, 300, 0x19);
        placeNameRect(this->row_name[1], 0x2d,  0x106, 0xcb, 300, 0x19);
        placeNameRect(this->row_name[2], 0x1be, 0x22,  0xcb, 300, 0x19);
        placeNameRect(this->row_name[3], 0x1be, 0x106, 0xcb, 300, 0x19);

        placeFixedRect(this->row_tile[0], 0x2b,  0x20,  300, 200);
        placeFixedRect(this->row_tile[1], 0x2b,  0x104, 300, 200);
        placeFixedRect(this->row_tile[2], 0x1bc, 0x20,  300, 200);
        placeFixedRect(this->row_tile[3], 0x1bc, 0x104, 300, 200);
    } else {
        /* ---- 1024x768 layout (tiles_per_page = 6) ---- */
        placeButtonRect(this->btn_close,  0x397, 0x145);
        placeButtonRect(this->btn_delete, 0x397, 0x5e);
        placeButtonRect(this->btn_save,   0x397, 0x11);
        placeButtonRect(this->btn_rotate, 0x397, 0xab);
        placeButtonRect(this->btn_print,  0x397, 0xf8);
        placeButtonRect(this->btn_prev,   0x22,  0x279);
        placeButtonRect(this->btn_next,   0x397, 0x279);

        placeScrollwheelAndLabels(0x366, 8);

        this->tiles_per_page = 6;   /* +0x11C */

        placeFixedRect(this->row_icon[0], 0x9d,  0x1b,  300, 200);
        placeFixedRect(this->row_icon[1], 0x9d,  0xff,  300, 200);
        placeFixedRect(this->row_icon[2], 0x9d,  0x1e3, 300, 200);
        placeFixedRect(this->row_icon[3], 0x22e, 0x1b,  300, 200);
        placeFixedRect(this->row_icon[4], 0x22e, 0xff,  300, 200);
        placeFixedRect(this->row_icon[5], 0x22e, 0x1e3, 300, 200);

        placeNameRect(this->row_name[0], 0x9d,  0x1b,  200, 300, 0x19);
        placeNameRect(this->row_name[1], 0x9d,  0xff,  200, 300, 0x19);
        placeNameRect(this->row_name[2], 0x9d,  0x1e3, 200, 300, 0x19);
        placeNameRect(this->row_name[3], 0x22e, 0x1b,  200, 300, 0x19);
        placeNameRect(this->row_name[4], 0x22e, 0xff,  200, 300, 0x19);
        placeNameRect(this->row_name[5], 0x22e, 0x1e3, 200, 300, 0x19);

        placeFixedRect(this->row_tile[0], 0x9b,  0x19,  300, 200);
        placeFixedRect(this->row_tile[1], 0x9b,  0xfd,  300, 200);
        placeFixedRect(this->row_tile[2], 0x9b,  0x1e1, 300, 200);
        placeFixedRect(this->row_tile[3], 0x22c, 0x19,  300, 200);
        placeFixedRect(this->row_tile[4], 0x22c, 0xfd,  300, 200);
        placeFixedRect(this->row_tile[5], 0x22c, 0x1e1, 300, 200);
    }
}

/* ================================================================== */
/* PostcardAlbum::BlitToSurface                                         */
/* NOT its own original address -- see ui/PostcardAlbum.h's doc comment */
/* for this method. Factored out of the identical inline pattern in     */
/* BlitElement (0x403E80), RenderTileName (0x4048E0), and RenderAllTiles */
/* (0x404AC0): read the sprite's rect, guard on sprites_inited/          */
/* text_rendered, copy the rect twice, offset one copy by                */
/* workRect.left/top (source) and the other by bg_blit_rect.left/top     */
/* (destination), then UIPANEL_Blit from album_bg_surface to             */
/* g_primary_surface with flags=1 (matches every one of the three        */
/* original call sites' identical literal `1`).                          */
/* ================================================================== */
bool PostcardAlbum::BlitToSurface(ButtonSprite* sprite)
{
    RECT rect = sprite_rect(*sprite);

    /* Guard verified identical at all three original inline sites:
     * `if (sprites_inited != 0 && text_rendered != 0) { ...blit... }`
     * -- when not ready to paint, every call site just falls through
     * without blitting (and without treating it as failure). */
    if (this->sprites_inited == 0 || this->text_rendered == 0) {
        return true;
    }

    RECT srcRect = rect;
    RECT dstRect = rect;
    OffsetRect(&srcRect, this->workRect.left, this->workRect.top);
    OffsetRect(&dstRect, this->bg_blit_rect.left, this->bg_blit_rect.top);

    bool ok = UIPANEL_Blit(
        this->album_bg_surface,                                   /* +0x13C */
        srcRect.left, srcRect.top, srcRect.right, srcRect.bottom,
        g_primary_surface,
        dstRect.left, dstRect.top, dstRect.right, dstRect.bottom,
        1);

    if (!ok) {
        OutputDebugStringA("AW_Blit failure reported");
    }

    return ok;
}

/* ================================================================== */
/* PostcardAlbum::UpdateSprite                                          */
/* Address: 0x403BA0 (__thiscall)                                       */
/*                                                                      */
/* See ui/PostcardAlbum.h's doc comment for the verified single-real-    */
/* argument ABI evidence (RET 8, but the second stack dword is dead at   */
/* every real call site).                                                */
/* ================================================================== */
void PostcardAlbum::UpdateSprite(int elementId)
{
    switch (elementId) {
    case 1:
        this->btn_close->setState(0, nullptr);
        return;

    case 2:
        if (this->row_enabled[2] == 1) {
            this->btn_delete->setState(0, nullptr);
        } else {
            this->btn_delete->setState(2, nullptr);
        }
        return;

    case 3:
        this->btn_save->setState(0, nullptr);
        return;

    case 4:
        if (this->row_enabled[3] == 1) {
            this->btn_rotate->setState(0, nullptr);
        } else {
            this->btn_rotate->setState(2, nullptr);
        }
        return;

    case 5:
        if (this->row_enabled[0] == 1) {
            this->btn_prev->setState(0, nullptr);
        } else {
            this->btn_prev->setState(2, nullptr);
        }
        return;

    case 6:
        if (this->row_enabled[1] == 1) {
            this->btn_next->setState(0, nullptr);
        } else {
            this->btn_next->setState(2, nullptr);
        }
        return;

    case 9:
        this->btn_print->setState(0, nullptr);
        return;

    default:
        /* 0, 7, 8, 10, and anything else: no-op (no visual state to
         * update for the scrollwheel or row sprites here). */
        return;
    }
}

/* ================================================================== */
/* PostcardAlbum::HitTest                                                */
/* Address: 0x403CD0 (__thiscall, NON-virtual helper)                    */
/* ================================================================== */
int PostcardAlbum::HitTest(int x, int y)
{
    if (sprite_contains(this->btn_close, x, y))  return 1;
    if (sprite_contains(this->btn_print, x, y))  return 9;
    if (sprite_contains(this->btn_rotate, x, y)) return 4;
    if (sprite_contains(this->btn_delete, x, y)) return 2;
    if (sprite_contains(this->btn_save, x, y))   return 3;
    if (sprite_contains(this->btn_prev, x, y))   return 5;
    if (sprite_contains(this->btn_next, x, y))   return 6;

    for (int i = 0; i < 6; ++i) {
        if (sprite_contains(this->row_icon[i], x, y)) {
            this->hovered_tile = i;   /* +0x120 */
            return 8;
        }
        if (sprite_contains(this->row_name[i], x, y)) {
            this->hovered_tile = i;
            return 10;
        }
    }

    for (int i = 0; i < 9; ++i) {
        if (sprite_contains(this->tile_label_sprites[i], x, y)) {
            this->hovered_tile = i;
            return 7;
        }
    }

    return 0;
}

/* ================================================================== */
/* PostcardAlbum::BlitElement                                           */
/* Address: 0x403E80 (__thiscall)                                        */
/*                                                                       */
/* See ui/PostcardAlbum.h's doc comment for the verified single-real-     */
/* argument ABI evidence.                                                 */
/* ================================================================== */
void PostcardAlbum::BlitElement(int elementId)
{
    switch (elementId) {
    case 1:
        PlaySound(0x5015);
        this->BlitToSurface(this->btn_close);
        this->btn_close->setState(1, nullptr);
        return;

    case 2:
        if (this->row_enabled[2] != 1) {
            this->btn_delete->setState(2, nullptr);
            return;
        }
        PlaySound(0x5015);
        this->BlitToSurface(this->btn_delete);
        this->btn_delete->setState(1, nullptr);
        return;

    case 3:
        PlaySound(0x5015);
        this->BlitToSurface(this->btn_save);
        this->btn_save->setState(1, nullptr);
        return;

    case 4:
        if (this->row_enabled[3] != 1) {
            this->btn_rotate->setState(2, nullptr);
            return;
        }
        PlaySound(0x5015);
        this->BlitToSurface(this->btn_rotate);
        this->btn_rotate->setState(1, nullptr);
        return;

    case 5:
        if (this->row_enabled[0] != 1) {
            this->btn_prev->setState(2, nullptr);
            return;
        }
        PlaySound(0x5015);
        this->BlitToSurface(this->btn_prev);
        this->btn_prev->setState(1, nullptr);
        return;

    case 6:
        if (this->row_enabled[1] != 1) {
            this->btn_next->setState(2, nullptr);
            return;
        }
        /* Unique ordering: blit + state change BEFORE the sound, verified
         * against the disassembly (every other case plays PlaySound first). */
        this->BlitToSurface(this->btn_next);
        this->btn_next->setState(1, nullptr);
        PlaySound(0x5015);
        return;

    case 7:
        PlaySound(0x5015);
        this->btn_scrollwheel->setState(this->scroll_wheel_pos, nullptr);   /* +0x128 */
        return;

    case 9:
        PlaySound(0x5015);
        this->BlitToSurface(this->btn_print);
        this->btn_print->setState(1, nullptr);
        return;

    default:
        /* 0, 8, 10, and anything else: no-op. */
        return;
    }
}

/* ================================================================== */
/* PostcardAlbum::RenderTileName                                        */
/* Address: 0x4048E0 (__thiscall)                                        */
/*                                                                       */
/* See ui/PostcardAlbum.h's doc comment for why this returns void (the    */
/* original's return value is dead at its one caller, RenderAllTiles,    */
/* and is not even deterministic on the not-found path).                 */
/* ================================================================== */
void PostcardAlbum::RenderTileName(int rowIndex)
{
    DPlayManager* entry = g_dplay_config->LookupAsset(
        this->scroll_pixel_offset + rowIndex,   /* +0x114 */
        this->tile_index);                      /* +0x118 */

    if (entry == nullptr) {
        this->tile_names[rowIndex][0] = '\0';
        this->BlitToSurface(this->row_icon[rowIndex]);
        return;
    }

    ButtonSprite* iconSprite = this->row_icon[rowIndex];
    RECT iconRect = sprite_rect(*iconSprite);

    /* NetworkPlayerList::RenderPlayer (0x4437C0) -- real 9-arg ABI, see
     * network/NetworkPlayerList.h. `scroll_wheel_enabled` (+0x12C) is
     * passed as the `highlighted` flag, matching the original's read of
     * the same byte at this call site. */
    g_dplay->RenderPlayer(
        this->scroll_wheel_enabled != 0,
        entry,
        static_cast<IDirectDrawSurface4*>(g_primary_surface),
        iconRect.left, iconRect.top, iconRect.right, iconRect.bottom,
        this->hWnd,
        nullptr);

    /* Copy the resolved player's name (m_sessionBlk2, a postcard-flavored
     * reuse of the session-name byte range -- see network/DPlayManager.h's
     * own field documentation) into tile_names[rowIndex]. Per-byte
     * copy (not a bulk reinterpret_cast of the whole array) to avoid
     * treating the modeled DPlayManager field as an opaque byte blob.
     *
     * BUG (original, preserved): this copy is unbounded -- both
     * m_sessionBlk2 and tile_names[row] are fixed 20-byte buffers, and the
     * original never clamps the copy length, so a source string with no
     * NUL within its 20 bytes would overrun tile_names[row] by reading
     * past m_sessionBlk2's own bounds. Reproduced faithfully rather than
     * silently clamped, matching the original's real (if unsafe) behavior. */
    size_t len = 0;
    while (entry->m_sessionBlk2[len] != 0) {
        this->tile_names[rowIndex][len] = static_cast<char>(entry->m_sessionBlk2[len]);
        ++len;
    }
    this->tile_names[rowIndex][len] = '\0';

    /* Matches the original's vtable[0] dispatch with flags=1 (DPlayManager's
     * real scalar deleting destructor frees `this` when bit 0 is set). */
    delete entry;

    this->row_tile[rowIndex]->setState(0, nullptr);   /* +0x180 */
}

/* ================================================================== */
/* PostcardAlbum::RenderAllTiles                                        */
/* Address: 0x404AC0 (__fastcall, ECX = this)                            */
/*                                                                       */
/* See ui/PostcardAlbum.h's doc comment for the full phase breakdown and  */
/* the verified unsigned-comparison evidence for tile_index's Phase-3     */
/* bounds check.                                                          */
/* ================================================================== */
void PostcardAlbum::RenderAllTiles()
{
    /* Phase 1: render each visible row's name text, then blit its
     * row_name background area. */
    if (this->tiles_per_page != 0) {
        for (int32_t i = 0; i < this->tiles_per_page; ++i) {
            this->RenderTileName(i);
            this->BlitToSurface(this->row_name[i]);
        }
    }

    /* Phase 2: GDI text pass (debug/name labels), bracketed by
     * BeginPaint/EndPaintEx like every other UI_WindowBase-derived paint
     * path in this codebase. */
    HDC hdc = this->BeginPaint();

    if (this->tiles_per_page != 0) {
        for (int32_t i = 0; i < this->tiles_per_page; ++i) {
            if (this->scroll_wheel_enabled == 1) {   /* +0x12C */
                SetBkMode(hdc, 1);   /* TRANSPARENT */
                COLORREF oldColor = SetTextColor(hdc, 0);   /* black */
                int oldMode = SetBkMode(hdc, 1);
                HGDIOBJ oldFont = SelectObject(hdc, g_font_small);

                RECT nameRect = sprite_rect(*this->row_name[i]);
                DrawTextA(hdc, this->tile_names[i], -1, &nameRect,
                          0x25 /* DT_SINGLELINE | DT_VCENTER | DT_CENTER */);

                SelectObject(hdc, oldFont);
                SetTextColor(hdc, oldColor);
                SetBkMode(hdc, oldMode);
                /* Disassembly calls SetBkMode(hdc, oldMode) twice back to
                 * back here (same restored value); the second call is a
                 * provable no-op on a real GDI DC (idempotent state set),
                 * reproduced verbatim rather than silently dropped. */
                SetBkMode(hdc, oldMode);
            }
        }
    }

    /* unlockOnly=true: only release the HDC, no present -- the visible
     * blitting already happened via BlitToSurface() in Phase 1. */
    this->EndPaintEx(hdc, true, nullptr);

    /* Phase 3a: prev-button (element 5) / "can page up" enable state. */
    if (this->scroll_pixel_offset == 0) {   /* +0x114 */
        this->row_enabled[4] = 1;
        uint8_t wasEnabled = this->row_enabled[0];
        if (this->tile_index != 0) {   /* +0x118 */
            if (wasEnabled == 0) {
                this->row_enabled[0] = 1;
                this->UpdateSprite(5);
            }
        } else {
            if (wasEnabled == 1) {
                this->row_enabled[0] = 0;
                this->UpdateSprite(5);
            }
        }
    } else {
        uint8_t wasEnabled = this->row_enabled[0];
        this->row_enabled[4] = 0;
        if (wasEnabled == 0) {
            this->row_enabled[0] = 1;
            this->UpdateSprite(5);
        }
    }

    /* Phase 3b: next-button (element 6) / "can page down" enable state. */
    uint32_t totalEntries = static_cast<uint32_t>(g_dplay_config->GetEntryCount());
    if (static_cast<uint32_t>(this->scroll_pixel_offset + this->tiles_per_page) < totalEntries) {
        uint8_t wasEnabled = this->row_enabled[1];
        this->row_enabled[5] = 0;
        if (wasEnabled == 0) {
            this->row_enabled[1] = 1;
            this->UpdateSprite(6);
        }
    } else {
        this->row_enabled[5] = 1;
        uint8_t wasEnabled = this->row_enabled[1];
        /* VERIFIED unsigned comparison -- see this method's header doc
         * comment. */
        if (static_cast<uint32_t>(this->tile_index) > 7) {
            if (wasEnabled == 1) {
                this->row_enabled[1] = 0;
                this->UpdateSprite(6);
            }
            return;
        }
        if (wasEnabled == 0) {
            this->row_enabled[1] = 1;
            this->UpdateSprite(6);
        }
    }
}
