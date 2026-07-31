/**
 * Town.cpp — Town class implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * This file contains the Town main gameplay view implementation:
 *   - Building selection and tracking (SelectBuilding/DeselectBuilding/TrackBuilding)
 *   - Viewport occupancy checks (CheckOccupied, CheckOccupiedEx, BlitViewport)
 *   - Viewport scroll rect computation (CalcScrollRect, CalcScrollRectReversed)
 *   - Postcard creation, sending, receiving, album management
 *   - Tile viewport rendering helpers
 *   - Network session management for multiplayer postcards
 *
 * NOTE: The PostcardPreviewWindow and GameView class implementations
 * have been moved to their own files:
 *   - PostcardPreviewWindow: src/decompiled_cpp/ui/PostcardPreviewWindow.cpp
 *   - GameView:              src/decompiled_cpp/core/GameView.cpp
 */

#include "Town.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
#include "../core/GameObject.h"

/* ================================================================== */
/* External references (C-linkage from Win32 and other modules)        */
/* ================================================================== */

extern "C" {
    /* CRT memory */
    void*  operator_new(size_t size);               /* 0x465CE0 — operator new */
    void   GLOBAL_free(const void* ptr);             /* global free */

    /* Resource management */
    void*  ResourceManager_GetById(void* resmgr, int id);           /* 0x44CB40 */
    void*  RESDATA_CreateSpriteObject(void* obj, int res_id);       /* 0x44B6B0 */
    void*  RESDATA_CreateChildSprite(void* parent, void* res, int x, int y); /* 0x44B910 */
    void   Sprite_Destroy(void* sprite);                             /* 0x44AE90 */
    void   Sprite_Init(void* sprite);                                /* 0x44ADA0 */
    void   Sprite_SetState(void* sprite, int state, int* unk);      /* 0x44AE20 */
    char   UI_IsBitmapReady(int res_id);                             /* 0x448810 */
    void*  ButtonSprite_Ctor(void* obj, int res_id);                 /* 0x44AEA0 */

    /* RESDATA base init / dtor */
    void   RESDATA_BaseInit(void* self);                             /* 0x4544E0 */
    void   RESDATA_DtorBase(void* self);                             /* 0x454630 */

    /* GameObject */
    void   GameObject_BaseCtor(void* obj, int x, int y, int w, int h); /* 0x405790 */
    void   GameObject_DtorBody(void* obj);                            /* 0x405870 */

    /* Panel */
    void   Panel_DtorBody(void* obj);                                 /* 0x4545A0 */

    /* UI Window management */
    int    UI_CreateFullWindow(void* self, int nCmdShow, HWND hParent,
                               int x, int y, int nWidth, int nHeight,
                               void* hMenu, void* hIcon, UINT classStyle); /* 0x425B70 */
    void   UI_WindowBase_Hide(void* self);                           /* 0x425990 */
    void   UI_WindowBase_BaseDtor(void* self);                       /* 0x425910 */
    void*  UI_WindowBase_Ctor(void* self, HINSTANCE hInstance, UINT resId); /* 0x425880 */
    void   UIPANEL_EndPaintEx(void* self, HWND hWnd, int unk1,
                              byte unk2, RECT* rect);                /* 0x42B2D0 */

    /* UIPANEL blit */
    void   UIPANEL_Blit(void* src_surface, uint32_t src_x, uint32_t src_y,
                        int32_t src_w, uint32_t src_h,
                        void* dest_surface, uint32_t dest_x, uint32_t dest_y,
                        int32_t dest_w, uint32_t dest_h, uint32_t flags); /* 0x42B050 */
    void*  UIPANEL_CreateSurface(void* obj);                         /* 0x42AF30 */
    void   UIPANEL_InitSurface(void* surface, int w, int h,
                               byte mode, int unk1, int unk2);     /* 0x42AF70 */

    /* Tile map */
    void   TileMap_InvalidateRect(void* tilemap, int left, int top,
                                  int right, int bottom);            /* 0x416FF0 */

    /* DDraw */
    void   DDRAW_SelectBuilding(void* ddraw, void* building);       /* 0x4412F0 */
    void*  DDRAW_GetDdrawErrorString(int code);                     /* 0x45BBC0 */

    /* Track piece */
    int    CGWND_TrackPiece_SetZoom(void* track, int zoom);         /* 0x452E80 */

    /* Audio */
    char   RESMGR_PlaySound(int sound_id);                           /* 0x44A290 */
    void*  AudioMgr_PlayEvent(void* audio_mgr, int event_id, int unk); /* 0x446220 */

    /* RESDATA */
    void   RESDATA_DtorBase(void* self);                             /* 0x44B580 */
    char   RESDATA_IsBuildingTile(void* tile_data);                  /* 0x44C4E0 */
    char   RESDATA_HitTestChildren(void* self, int x, int y);        /* 0x44B200 */

    /* Network */
    char*  NET_GetHostName(int type, int index);                     /* 0x449AA0 */
    void*  NET_ResolveAddress(const char* hostname);                 /* 0x449A40 */
    void   NET_RegisterPlayer(void* dplay, void* data, int type, int unk); /* 0x4498E0 */
    void   NET_UnregisterPlayer(void* dplay, const char* hostname);  /* 0x449C00 */
    short  NET_UpdatePlayerList(void);                               /* 0x447AB0 */
    void   NETMAN_CheckTimeout(void* netman, uint32_t active);      /* 0x4415F0 */
    int    DPLAY_GetMessageCount(void* dplay);                       /* 0x4510E0 */
    void   DPLAY_RenderPlayer(void* dplay, char flag, void* player,
                              void* surface, int x, int y,
                              uint32_t extra, RECT* rect);           /* 0x4510A0 */
    void   NETMAN_SendAck(void* netman);                             /* 0x4415C0 */
    void   NET_GetFilePath(void* mgr, int type, int player_id, char* buf); /* 0x445510 */
    void   NET_GetAttFilePath(void* mgr, int type, int player_id, char* buf); /* 0x445400 */
    void   NET_DownloadAsset(void* mgr, int player_id, int type, char* buf); /* 0x445A40 */
    char*  CRT_malloc(void* ptr, size_t size);                       /* 0x466DE0 */
    char*  _strrchr(const char* s, int c);                           /* 0x467E60 */

    /* Timer */
    void   KillTimer(HWND hWnd, UINT_PTR id);

    /* String */
    void   FormatResourceString(void* resmgr, int id, char* buf, int max_len); /* 0x447330 */

    /* Win32 */
    HWND   GetDesktopWindow();
    void   GetClientRect(HWND hWnd, RECT* rect);
    void*  LoadIconA(HINSTANCE hInstance, const char* name);
    void   SetFocus(HWND hWnd);
    int    ShowCursor(int show);
    void   SetRect(RECT* rect, int left, int top, int right, int bottom);
    void   SetRectEmpty(RECT* rect);
    void   CopyRect(RECT* dest, const RECT* src);
    void   OffsetRect(RECT* rect, int dx, int dy);
    BOOL   PtInRect(const RECT* rect, int x, int y);
    BOOL   IntersectRect(RECT* dest, const RECT* src1, const RECT* src2);
    BOOL   IsRectEmpty(const RECT* rect);
    void   SetTimer(HWND hWnd, UINT_PTR id, UINT timeout, void* proc);
    void   EnableWindow(HWND hWnd, BOOL enable);
    void   PostMessageA(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT DefWindowProcA(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void   PostQuitMessage(int exit_code);
    void   Sleep(DWORD ms);
    int    GetLastError();
    BOOL   CreateDirectoryA(const char* path, void* security);
    HANDLE CreateFileA(const char* name, DWORD access, DWORD share,
                       void* security, DWORD creation, DWORD flags, HANDLE tmpl);
    BOOL   ReadFile(HANDLE hFile, void* buf, DWORD n, DWORD* read, void* ovlp);
    BOOL   WriteFile(HANDLE hFile, const void* buf, DWORD n,
                     DWORD* written, void* ovlp);
    BOOL   CloseHandle(HANDLE h);
    BOOL   DeleteFileA(const char* name);
    BOOL   CopyFileA(const char* src, const char* dest, BOOL fail_if_exists);
    int    MessageBoxA(HWND hWnd, const char* text, const char* title, UINT type);
    void   LocalFree(void* ptr);
    BOOL   GetSaveFileNameA(void* ofn);
    void   SetCursorPos(int x, int y);
    void   FormatMessageA(DWORD flags, void* source, DWORD msg_id, DWORD lang,
                          char* buf, DWORD size, void* args);
}

/* ================================================================== */
/* Global variables referenced by Town functions                       */
/* ================================================================== */

extern void* g_resmgr;                  /* 0x4FD228 — Resource manager */
extern void* g_tilemap;                 /* 0x4FD244 — Tile map pointer */
extern void* g_ddraw_building;          /* 0x4FD128 — DDraw building selection */
extern void* g_primary_surface;         /* 0x4FD164 — Primary DirectDraw surface */
extern void* g_dplay;                   /* 0x4FD1F4 — DirectPlay interface */
extern void* g_netman;                  /* 0x4FD33C — Network manager */
extern void* g_audio_mgr;               /* 0x4FD14C — Audio manager */
extern char   g_ddraw_active;           /* 0x485268 — 1 = DirectDraw active */
extern int32_t g_demo_mode;              /* 0x4A9918 — 1 = demo mode */
extern char   g_game_mode;              /* 0x4852AC — current game mode */
extern int    g_cursor_world_x;         /* 0x4FD348 — cursor world X */
extern int    g_cursor_world_y;         /* 0x4FD34C — cursor world Y */
extern void*  g_active_panel;           /* 0x4A9EF0 — active panel override */
extern char   g_surface_lost;           /* 0x4FD218 — surface lost flag */
extern void*  g_main_window;            /* 0x4FD230 — main CGWND window pointer */
extern int    g_surface_bshift;         /* 0x485280 — bit shift for half-bright */
extern char   g_empty_string;           /* 0x4851D0 — empty string constant */

/* Tile check globals for CheckOccupiedEx */
extern uint32_t DAT_004fd19c;           /* 0x4FD19C — DDSURFACEDESC */
extern uint32_t DAT_004fd1ac;           /* 0x4FD1AC — surface pitch */
extern uint32_t DAT_004fd1c0;           /* 0x4FD1C0 — surface pixel data */
extern int      DAT_00485288;           /* 0x485288 — pixel mask 1 */
extern int      DAT_00485278;           /* 0x485278 — pixel shift */
extern int      DAT_00485290;           /* 0x485290 — pixel mask 2 */

/* Viewport rect globals */
extern int g_viewport_rect_left;        /* 0x4FD0F0 */
extern int g_viewport_rect_top;         /* 0x4FD0F4 */
extern int g_viewport_rect_right;       /* 0x4FD0F8 */
extern int g_viewport_rect_bottom;      /* 0x4FD0FC */

/* Resource string constants (used by save_postcard_as) */
extern const char s_LEGO_LOCO_0047e1c0[]; /* 0x47E1C0 — "LEGO LOCO" dialog title */
extern const char DAT_0047e4d4[];        /* 0x47E4D4 — filter string template */
extern const char DAT_0047e4e8[];        /* 0x47E4E8 — file extension ".bmp" */
extern const char DAT_0047e954[];        /* 0x47E954 — default name template */
extern const char DAT_0047e956[];        /* 0x47E956 — */
extern void* g_net_cache_mgr;            /* 0x4FD3B0 — network cache manager */

/* Win32 API indirect call targets */
extern void (*g_SetRect)(RECT*, int, int, int, int);       /* 0x477384 */
extern BOOL (*g_IntersectRect)(RECT*, const RECT*, const RECT*); /* 0x47726C */
extern BOOL (*g_IsRectEmpty)(const RECT*);                  /* 0x477268 */

/* Missing Win32 API constants (not in compat.h yet) */
#ifndef FORMAT_MESSAGE_FROM_SYSTEM
#define FORMAT_MESSAGE_FROM_SYSTEM     0x00001000
#endif
#ifndef LANG_NEUTRAL
#define LANG_NEUTRAL                   0x00
#endif
#ifndef SUBLANG_DEFAULT
#define SUBLANG_DEFAULT                0x01
#endif
#ifndef MAKELANGID
#define MAKELANGID(p, s)               ((((WORD)(s)) << 10) | (WORD)(p))
#endif
#ifndef MB_ICONERROR
#define MB_ICONERROR                    MB_ICONSTOP
#endif

/* Hook procedure for OPENFILENAME custom dialog */
extern LRESULT SaveAsDlgHook(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
/* 0x419FD0 — custom hook for GetSaveFileNameA */

/* ================================================================== */
/* Town::Town — Constructor                                            */
/* Address: 0x42E900                                                   */
/*                                                                     */
/* Called by: CGWND_InitAllSubsystems @ 0x407054                       */
/* ================================================================== */
Town::Town(HINSTANCE hInstance, UINT resId)
    : UI_WindowBase(hInstance, resId)
{
    /* SEH prologue omitted */

    /* Override base-class vtable with Town vtable */
/* In the binary: sets vtable here. Compiler-managed in natural C++. */

    /* Initialize all Town-specific fields and create postcard sprites */
    this->base_ctor();                         /* Town_BaseCtor @ 0x42E980 */

    /* SEH epilogue omitted */
}

/* ================================================================== */
/* Town::base_ctor — ID: 0x42E980                                      */
/*                                                                     */
/* Initialize all Town-specific fields, create 8 postcard sprites.     */
/* Called from Town::Town after base class init.                       */
/* ================================================================== */
void Town::base_ctor()
{
    /* Zero all Town-specific state fields */
    this->icon_handle = nullptr;         /* +0x5F4 */
    this->postcard_active = 0;           /* +0x5F8 */
    this->sprites_initialized = 0;       /* +0x5F9 */
    this->overlay_initialized = 0;       /* +0x5FA */
    this->timer_active = 0;              /* +0x5FC */
    this->frame_counter = 0;             /* +0x600 */
    this->selected_player = nullptr;    /* +0x608 */
    this->postcard_data = nullptr;       /* +0x60C */
    this->player_count_flag = 1;         /* +0x610 (default to 1) */
    this->timer_counter = 0;             /* +0x5F0 */
    this->net_update_flag = 0;           /* +0x604 */
    this->repaint_requested = 0;         /* +0x605 */
    this->flag_E8 = 0;                   /* +0xE8 */

    /* Create 8 postcard sprites (0x24 bytes each, UISprite objects)     */
    /* Resources: 0x3CF0-0x3CF9 (with 0x3cac for inbox preview)          */

    /* Button sprites 1-4 : res 0x3CF0, 0x3CF1, 0x3CF2, 0x3CF3       */
    void* obj;
    obj = operator_new(0x24);
    this->sprite_btn_close = (obj) ? RESDATA_CreateSpriteObject(obj, 0x3CF0) : nullptr;

    obj = operator_new(0x24);
    this->sprite_btn_options = (obj) ? RESDATA_CreateSpriteObject(obj, 0x3CF1) : nullptr;

    obj = operator_new(0x24);
    this->sprite_btn_rotate = (obj) ? RESDATA_CreateSpriteObject(obj, 0x3CF2) : nullptr;

    obj = operator_new(0x24);
    this->sprite_btn_save = (obj) ? RESDATA_CreateSpriteObject(obj, 0x3CF3) : nullptr;

    /* Inbox preview sprite : res 0x3CAC                              */
    obj = operator_new(0x24);
    this->sprite_inbox = (obj) ? RESDATA_CreateSpriteObject(obj, 0x3CAC) : nullptr;

    /* Inbox counter sprite : res 0x3CF5                              */
    obj = operator_new(0x24);
    this->sprite_inbox_counter = (obj) ? RESDATA_CreateSpriteObject(obj, 0x3CF5) : nullptr;

    /* Outbox counter sprite : res 0x3CF6                             */
    obj = operator_new(0x24);
    this->sprite_outbox_counter = (obj) ? RESDATA_CreateSpriteObject(obj, 0x3CF6) : nullptr;

    /* Send button sprite : res 0x3CF9                                */
    obj = operator_new(0x24);
    this->sprite_send = (obj) ? RESDATA_CreateSpriteObject(obj, 0x3CF9) : nullptr;

    /* Initialize sprite state look-up tables                          */
    this->inbox_state_lut[0] = 0;
    this->inbox_state_lut[1] = 1;
    this->inbox_state_lut[2] = 3;
    this->inbox_state_lut[3] = 5;
    this->inbox_state_lut[4] = 7;

    this->outbox_state_lut[0] = 0;
    this->outbox_state_lut[1] = 1;
    this->outbox_state_lut[2] = 2;
    this->outbox_state_lut[3] = 3;
    this->outbox_state_lut[4] = 4;

    /* Clear network update flag                                       */
    this->net_update_flag = 0;

    /* Clear frame counter                                             */
    this->timer_counter = 0;

    /* Clear audio playing flag                                        */
    this->audio_playing = 0;
}

/* ================================================================== */
/* Town::scalar deleting destructor — vtable[0]                        */
/* Address: 0x42E960                                                   */
/*                                                                     */
/* Called by: CGWND_InitAllSubsystems error rollback, Town cleanup     */
/* ================================================================== */
Town::~Town()
{
    this->destroy();
}

/* ================================================================== */
/* Town::destroy — Full destructor body                                 */
/* Address: 0x42EC10                                                   */
/*                                                                     */
/* Releases all Town-owned resources. Called from scalar deleter.      */
/* ================================================================== */
void Town::destroy()
{
    /* Reset vtable for correct dispatch during destruction           */
/* In the binary: sets vtable here. Compiler-managed in natural C++. */

    /* Destroy notification/overlay window if initialized              */
    if (this->overlay_initialized) {                        /* +0x5FA */
        void* overlay = this->overlay_resource;              /* +0x644 */
        if (overlay) {
            ((void (*)(void*))(*(void***)overlay)[2])(overlay);
        }
        this->overlay_initialized = 0;
    }

    /* Destroy postcard sprites if initialized                         */
    if (this->sprites_initialized) {                        /* +0x5F9 */
        Sprite_Destroy(this->sprite_btn_close);             /* +0x6A4 */
        Sprite_Destroy(this->sprite_btn_options);           /* +0x6A8 */
        Sprite_Destroy(this->sprite_btn_rotate);            /* +0x6AC */
        Sprite_Destroy(this->sprite_btn_save);              /* +0x6B0 */
        Sprite_Destroy(this->sprite_inbox);                 /* +0x6B4 */
        Sprite_Destroy(this->sprite_outbox_counter);        /* +0x6B8 */
        Sprite_Destroy(this->sprite_inbox_counter);         /* +0x6BC */
        Sprite_Destroy(this->sprite_send);                  /* +0x6C0 */

        /* Destroy 3 child sub-window resources via vtable[2]         */
        if (this->button_strip_resource) {                  /* +0x664 */
            ((void (*)(void*))(*(void***)this->button_strip_resource)[2])(
                this->button_strip_resource);
        }
        if (this->background_resource) {                    /* +0x64C */
            ((void (*)(void*))(*(void***)this->background_resource)[2])(
                this->background_resource);
        }
        if (this->send_confirm_resource) {                  /* +0x69C */
            ((void (*)(void*))(*(void***)this->send_confirm_resource)[2])(
                this->send_confirm_resource);
        }

        this->sprites_initialized = 0;
    }

    /* Free individual sprite objects via vtable[0] if non-null        */
    if (this->sprite_btn_close) {
        ((void (*)(void*, int))(*(void***)this->sprite_btn_close)[0])(
            this->sprite_btn_close, 1);
        this->sprite_btn_close = nullptr;
    }
    if (this->sprite_btn_options) {
        ((void (*)(void*, int))(*(void***)this->sprite_btn_options)[0])(
            this->sprite_btn_options, 1);
        this->sprite_btn_options = nullptr;
    }
    if (this->sprite_btn_rotate) {
        ((void (*)(void*, int))(*(void***)this->sprite_btn_rotate)[0])(
            this->sprite_btn_rotate, 1);
        this->sprite_btn_rotate = nullptr;
    }
    if (this->sprite_btn_save) {
        ((void (*)(void*, int))(*(void***)this->sprite_btn_save)[0])(
            this->sprite_btn_save, 1);
        this->sprite_btn_save = nullptr;
    }
    if (this->sprite_inbox) {
        ((void (*)(void*, int))(*(void***)this->sprite_inbox)[0])(
            this->sprite_inbox, 1);
        this->sprite_inbox = nullptr;
    }
    if (this->sprite_outbox_counter) {
        ((void (*)(void*, int))(*(void***)this->sprite_outbox_counter)[0])(
            this->sprite_outbox_counter, 1);
        this->sprite_outbox_counter = nullptr;
    }
    if (this->sprite_inbox_counter) {
        ((void (*)(void*, int))(*(void***)this->sprite_inbox_counter)[0])(
            this->sprite_inbox_counter, 1);
        this->sprite_inbox_counter = nullptr;
    }
    if (this->sprite_send) {
        ((void (*)(void*, int))(*(void***)this->sprite_send)[0])(
            this->sprite_send, 1);
        this->sprite_send = nullptr;
    }

    /* Call base class destructor                                       */
    UI_WindowBase_BaseDtor(this);
}

/* ================================================================== */
/* Town::init_sprites — Create the Town child window                    */
/* Address: 0x42EDB0                                                   */
/*                                                                     */
/* Called by: CGWND_InitAllSubsystems during startup                   */
/* ================================================================== */
bool Town::init_sprites(HWND hParent)
{
    RECT desktop_rect;
    HWND hDesktop = GetDesktopWindow();
    GetClientRect(hDesktop, &desktop_rect);

    /* Load window icon (resource 0x65)                                 */
    this->icon_handle = LoadIconA(this->hInstance, (const char*)0x65);  /* +0x5F4 */

    /* Create full-desktop window via UI_CreateFullWindow                */
    int width = desktop_rect.right - desktop_rect.left;
    int height = desktop_rect.bottom - desktop_rect.top;
    int result = UI_CreateFullWindow(this, 0, hParent,
                                     desktop_rect.left, desktop_rect.top,
                                     width, height,
                                     (void*)0, this->icon_handle, 0);
    return (result != 0);
}

/* ================================================================== */
/* Town::handle_tile_click — Create placement cursor sprites (MISNAMED) */
/* Address: 0x42CE10                                                   */
/*                                                                     */
/* Creates 3 cursor indicator sprites (valid=0x3807, invalid=0x3808,    */
/* hover=0x3806) and an overlay UIPANEL surface for placement feedback. */
/* Stores sprites at +0x170, +0x174, +0x178 (overloading track_piece),  */
/* with duplicate references at +0xD8/+0xDC.                           */
/* Also initializes backup_surface rect at +0x180 via SetRect.          */
/* ================================================================== */
char Town::handle_tile_click()
{
    /* Create valid-placement indicator sprite (res 0x3807)             */
    /* Stored at +0x170 (cursor_valid_sprite) and +0xD8 (cursor_valid_dup) */
    void* res = ResourceManager_GetById(g_resmgr, 0x3807);
    if (res && UI_IsBitmapReady(static_cast<int>(reinterpret_cast<intptr_t>(res)))) {
        void* sprite = RESDATA_CreateChildSprite(this, res, 0, 0);
        this->cursor_valid_sprite = sprite;          /* +0x170 */
        this->cursor_valid_dup = sprite;             /* +0xD8 */
    }

    /* Create invalid-placement indicator sprite (res 0x3808)           */
    /* Stored at +0x174 (cursor_invalid_sprite) and +0xDC (cursor_invalid_dup) */
    res = ResourceManager_GetById(g_resmgr, 0x3808);
    if (res && UI_IsBitmapReady(static_cast<int>(reinterpret_cast<intptr_t>(res)))) {
        void* sprite = RESDATA_CreateChildSprite(this, res, 0, 0);
        this->cursor_invalid_sprite = sprite;        /* +0x174 */
        this->cursor_invalid_dup = sprite;           /* +0xDC */
    }

    /* Create hover/empty cursor sprite (res 0x3806)                    */
    /* Stored at +0x178 — overloads track_piece field                    */
    res = ResourceManager_GetById(g_resmgr, 0x3806);
    if (res && UI_IsBitmapReady(static_cast<int>(reinterpret_cast<intptr_t>(res)))) {
        this->track_piece = RESDATA_CreateChildSprite(this, res, 0, 0); /* +0x178 */
    }

    /* Load animation resources 0x3805 and 0x3804 via vtable[6]        */
    char loaded;
    loaded = ((char (*)(void*, int, int, int))(*(void***)this)[6])(
                 this, 0x3805, -1, 0);
    if (loaded) {
        loaded = ((char (*)(void*, int, int, int))(*(void***)this->child_panel)[6])(
                     this->child_panel, 0x3804, -1, 0);
        if (loaded) {
            /* Create overlay UIPANEL surface for placement feedback   */
            void* surface_obj = operator_new(0x20);
            this->overlay_panel = surface_obj ? UIPANEL_CreateSurface(surface_obj) : nullptr; /* +0x17C */

            if (this->overlay_panel) {
                void* pgfx = *(void**)(*(intptr_t*)this->panel_graphics + 0x10); /* +0x124 */
                UIPANEL_InitSurface(this->overlay_panel,
                    *(int*)((uint8_t*)pgfx + 8),    /* width */
                    *(int*)((uint8_t*)pgfx + 0xc),  /* height */
                    1, 0, 0);

                /* Initialize backup_surface rect using overlay_panel dimensions */
                SetRect((RECT*)&this->backup_surface, 0, 0,    /* +0x180 */
                    *(int*)((uint8_t*)this->overlay_panel + 8)  /* Panel/UIPANEL +0x08: width */,   /* width */
                    *(int*)((uint8_t*)this->overlay_panel + 0xc) /* Panel/UIPANEL +0x0C: height */);/* height */
            }
            return 1;
        }
    }
    return 0;
}

/* ================================================================== */
/* Town::is_valid_placement — Static check for buildable tile          */
/* Address: 0x42CF90 (__cdecl)                                         */
/* ================================================================== */
bool Town::is_valid_placement(void* entity)
{
    if (entity == nullptr || *(char*)((intptr_t)entity + 0x18) != 1) {
        return false;
    }

    uint8_t tile_type = 0;
    if (*(void**)((intptr_t)entity + 0x40) != nullptr) {
        tile_type = *(uint8_t*)(*(intptr_t*)((intptr_t)entity + 0x40) + 8);
    }

    if (tile_type == 0) {
        return false;
    }

    /* Tile type 0x07 = always buildable                             */
    if (tile_type == 0x07) {
        return true;
    }

    /* Tile types 0x08, 0x02, 0x06 = must be visible                  */
    if (tile_type == 0x08 || tile_type == 0x02 || tile_type == 0x06) {
        return *(char*)((intptr_t)entity + 0x24) == 1;
    }

    /* Tile type 0x04 = connected tile                                 */
    if (tile_type == 0x04 &&
        *(char*)(*(intptr_t*)((intptr_t)entity + 0x40) + 0x62c) == 1) {
        return true;
    }

    /* Tile type 0x03 = building tile                                  */
    if (tile_type == 0x03 &&
        RESDATA_IsBuildingTile(*(void**)((intptr_t)entity + 0x40))) {
        return true;
    }

    /* Tile type 0x0C = large ID tiles (ID > 0x300F)                  */
    if (tile_type == 0x0C) {
        int id = -1;
        if (*(void**)((intptr_t)entity + 0x40) != nullptr) {
            id = *(int*)(*(intptr_t*)((intptr_t)entity + 0x40) + 4);
        }
        if (id > 0x300F) {
            return true;
        }
    }

    return false;
}

/* ================================================================== */
/* Town::select_building — Select/focus a building in the town view     */
/* Address: 0x42D040                                                   */
/*                                                                     */
/* Called by: UI building click handlers, Town_SendPostcard lifecycle   */
/* ================================================================== */
byte Town::select_building(void* building)
{
    if (building != nullptr && g_game_mode == 3) {
        bool valid = Town::is_valid_placement(building);
        if (valid && g_demo_mode != 1) {
            this->selection_active = 1;                         /* +0x88 */

            uint16_t tile_type = 0;
            if (*(void**)((intptr_t)building + 0x40) != nullptr) {
                tile_type = *(uint8_t*)(*(intptr_t*)((intptr_t)building + 0x40) + 8);
            }
            this->selected_building_type = tile_type;            /* +0x16C */

            if (g_ddraw_active == 0) {
                g_active_panel = this;
            }

            this->selected_building = building;                  /* +0xE0 */

            int center_x = (*(int*)((intptr_t)building + 0x10) -
                           *(int*)((intptr_t)building + 8)) / 2 +
                           *(int*)((intptr_t)building + 8);
            int center_y = (*(int*)((intptr_t)building + 0x14) -
                           *(int*)((intptr_t)building + 0xc)) / 2 +
                           *(int*)((intptr_t)building + 0xc);
            ((void (*)(void*, int, int))(*(void***)this)[3])(this, center_x, center_y);

            short zoom = (this->selected_building_type == 6) ? 1 : 3;
            CGWND_TrackPiece_SetZoom(this->track_piece, zoom);   /* +0x178 */

            ((void (*)(void*))(*(void***)this->track_piece)[8])(
                this->track_piece);

            TileMap_InvalidateRect(&g_tilemap,
                *(int*)((intptr_t)this + 8),
                *(int*)((intptr_t)this + 0xc),
                *(int*)((intptr_t)this + 0x10),
                *(int*)((intptr_t)this + 0x14));

            DDRAW_SelectBuilding(&g_ddraw_building, this->selected_building);

            return this->selection_active;
        }
    }

    this->selection_active = 0;                                  /* +0x88 */
    this->selected_building_type = 0;                            /* +0x16C */

    g_active_panel = (void*)(uintptr_t)(~(uint32_t)(g_ddraw_active != 1) & 0x4A9EF0);

    ((void (*)(void*))(*(void***)this)[1])(this);

    if (this->child_panel) {
        ((void (*)(void*))(*(void***)this->child_panel)[1])(this->child_panel);
    }

    return this->selection_active;
}

/* ================================================================== */
/* Town::deselect_building — Remove building selection overlay          */
/* Address: 0x42D280                                                   */
/* ================================================================== */
void Town::deselect_building()
{
    RECT clip_rect;
    int inset_div;

    if (this->selected_building_type == 7) {
        inset_div = this->overlay_dest_right - 0x90;           /* +0x11C */
        clip_rect.right = inset_div;
        inset_div = this->overlay_dest_bottom - 0x8c;          /* +0x120 */
        clip_rect.bottom = inset_div;
    } else {
        clip_rect.right = this->overlay_dest_right / 4;        /* +0x11C */
        clip_rect.bottom = this->overlay_dest_bottom / 4;      /* +0x120 */
    }

    clip_rect.left  = this->viewport_inset_left + clip_rect.right;    /* +0xEC */
    clip_rect.right = this->viewport_inset_right - clip_rect.right;   /* +0xF4 */
    clip_rect.top   = this->viewport_inset_top + clip_rect.bottom;    /* +0xF0 */
    clip_rect.bottom = this->viewport_inset_bottom - clip_rect.bottom; /* +0xF8 */

    RECT viewport_rect;
    viewport_rect.left   = g_viewport_rect_left;
    viewport_rect.top    = g_viewport_rect_top;
    viewport_rect.right  = g_viewport_rect_right;
    viewport_rect.bottom = g_viewport_rect_bottom;
    IntersectRect(&clip_rect, &clip_rect, &viewport_rect);

    int* backing_surface = *(int**)(*(intptr_t*)(this->overlay_panel) + 0x1c);
    ((void (*)(void*, uint32_t, void*, RECT*, uint32_t, int))(
        *(void***)backing_surface)[5])(
        backing_surface,
        this->backup_surface,     /* +0x180 */
        g_primary_surface,
        &clip_rect,
        0x1000000,
        0);

    uint32_t panel_flags = 0;
    if (*(char*)(uintptr_t)(*(int*)((intptr_t)this->panel_graphics + 0x20) + 0x16 +
                 this->selected_building_type * 0x18) == 1) {
        panel_flags = 0x20;
    }

    void* panel_surface = *(void**)(*(intptr_t*)this->panel_graphics + 0x10);
    UIPANEL_Blit(
        panel_surface,
        this->backup_surface,     /* +0x180 — source surface handle */
        this->backup_x,           /* +0x184 — source X */
        this->backup_y,           /* +0x188 — source Y */
        this->backup_width,       /* +0x18C — source width */
        backing_surface,
        this->backup_surface,     /* +0x180 — dest X */
        this->backup_x,           /* +0x184 — dest Y */
        this->backup_y,           /* +0x188 — dest width */
        this->backup_width,       /* +0x18C — dest height */
        panel_flags);
}

/* ================================================================== */
/* Town::track_building — Per-frame tracking of selected building       */
/* Address: 0x42D1A0                                                   */
/* ================================================================== */
void Town::track_building()
{
    if (!this->selection_active) {                               /* +0x88 */
        return;
    }

    if (this->selected_building_type == 6 &&                      /* +0x16C */
        !*(char*)((intptr_t)this->selected_building + 0x24)) {
        this->select_building(nullptr);
    }

    void* building = this->selected_building;                      /* +0xE0 */
    int cx = (*(int*)((intptr_t)building + 0x10) -
              *(int*)((intptr_t)building + 8)) / 2 +
              *(int*)((intptr_t)building + 8);
    int cy = (*(int*)((intptr_t)building + 0x14) -
              *(int*)((intptr_t)building + 0xc)) / 2 +
              *(int*)((intptr_t)building + 0xc);

    if (this->building_center_x != cx ||                          /* +0x190 */
        this->building_center_y != cy) {                          /* +0x194 */
        ((void (*)(void*, int, int))(*(void***)this)[3])(this, cx, cy);
        this->building_center_x = cx;                              /* +0x190 */
        this->building_center_y = cy;                              /* +0x194 */
    }

    for (void* child = this->children_list; child != nullptr;      /* +0xD0 */
         child = *(void**)((intptr_t)child + 0x28)) {
        ((void (*)(void*, void*))(*(void***)this)[0x14])(this, child);
    }

    if (this->child_panel) {                                       /* +0xE4 */
        ((void (*)(void*))(*(void***)this->child_panel)[1])(
            this->child_panel);
    }
}

/* ================================================================== */
/* Town::update_selection — Blit selection overlay to primary surface   */
/* Address: 0x42D3A0                                                   */
/* ================================================================== */
void Town::update_selection()
{
    UIPANEL_Blit(
        this->overlay_panel,          /* +0x17C */
        this->viewport_inset_left,    /* +0xEC */
        this->viewport_inset_top,     /* +0xF0 */
        this->viewport_inset_right,   /* +0xF4 */
        this->viewport_inset_bottom,  /* +0xF8 */
        g_primary_surface,
        this->overlay_dest_left,      /* +0x114 */
        this->overlay_dest_top,       /* +0x118 */
        this->overlay_dest_right,     /* +0x11C */
        this->overlay_dest_bottom,    /* +0x120 */
        0x40);
}

/* ================================================================== */
/* Town::render_selection — Draw selection for a single tile            */
/* Address: 0x42D400                                                   */
/* ================================================================== */
void Town::render_selection()
{
    if (this->selection_active) {                                /* +0x88 */
        ((void (*)(void*))(*(void***)this)[0x0A])(this);
    }
}

/* ================================================================== */
/* Town::check_occupied — Scan byte buffer for non-empty tiles         */
/* Address: 0x42C950 (__thiscall)                                      */
/* ================================================================== */
uint8_t Town::check_occupied(int x1, int y1, int x2, int y2)
{
    if (*(int*)((intptr_t)this + 4) != 0) {
        return check_occupied_ex(x1, y1, x2, y2);
    }

    int stride = *(int*)((intptr_t)this + 8);
    uint8_t* buf = (uint8_t*)(uintptr_t)(*(int*)((intptr_t)this + 0x18));
    int width = x2 - x1;
    int height = y2 - y1;

    if (width <= 0 || height <= 0) {
        return 0;
    }

    uint8_t* row = buf + y1 * stride + x1;
    for (int row_idx = 0; row_idx < height; row_idx++) {
        for (int col = 0; col < width; col++) {
            if (row[col] != 0) {
                return 1;
            }
        }
        row += stride;
    }

    return 0;
}

/* ================================================================== */
/* Town::check_occupied_ex — Extended tile occupancy via DDraw surface  */
/* Address: 0x42C9F0 (__stdcall)                                       */
/* ================================================================== */
uint8_t Town::check_occupied_ex(int x1, int y1, int x2, int y2)
{
    uint8_t result = 0;

    if (g_surface_lost == 0) {
        uint32_t* ddsd = &DAT_004fd19c;
        for (int i = 0x1f; i != 0; i--) {
            *ddsd++ = 0;
        }
        DAT_004fd19c = 0x7c;

        int lock_result = ((int (*)(void*, int, void*, int, int))(
            *(void***)g_primary_surface)[0x64])(
            g_primary_surface, 0, &DAT_004fd19c, 0, 0);

        if (lock_result != 0) {
            g_surface_lost = 1;
        }
    }

    uint32_t pitch = DAT_004fd1ac >> 1 & 0xFFFF;
    uint32_t height = (y2 - y1) & 0xFFFF;
    uint32_t width = (x2 - x1) & 0xFFFF;

    uint16_t* pixels = (uint16_t*)(uintptr_t)(DAT_004fd1c0 + (pitch * y1 + x1) * 2);

    for (uint32_t row = 0; row < height; row++) {
        if (width != 0) {
            for (uint32_t col = 0; col < width; col++) {
                uint16_t pixel = pixels[col];
                int channel1 = (DAT_00485288 & pixel) >> ((uint8_t)DAT_00485278 & 0x1f);
                int channel2 = DAT_00485290 & pixel;

                if (channel1 != 0x1f && channel2 != 0x1f) {
                    result = 1;
                    break;
                }
            }
        }

        pixels += pitch - width;

        if (result != 0) {
            break;
        }
    }

    if (g_surface_lost != 0) {
        int unlock_result = ((int (*)(void*, int))(
            *(void***)g_primary_surface)[0x80])(
            g_primary_surface, 0);
        if (unlock_result == 0) {
            g_surface_lost = 0;
        }
    }

    return result;
}

/* ================================================================== */
/* Town::blit_viewport — Viewport occupancy check for collision        */
/* Address: 0x42CB10 (__thiscall)                                      */
/* ================================================================== */
uint32_t Town::blit_viewport(int x1, int y1, int x2, int y2, int x, int y)
{
    if (x < x1 || x2 < x || y < y1 || y2 < y) {
        return 1;
    }

    if (*(int*)((intptr_t)this + 4) != 1) {
        uint8_t* buf = *(uint8_t**)((intptr_t)this + 0x18);
        int stride = *(int*)((intptr_t)this + 8);
        uint8_t val = buf[stride * y + x];
        return (val == 0) ? 1 : 0;
    }

    int ddsd_buf[31];
    for (int i = 0; i < 31; i++) {
        ddsd_buf[i] = 0;
    }
    ddsd_buf[0] = 0x7c;

    void* surface = *(void**)((intptr_t)this + 0x1C);
    int lock_result = ((int (*)(void*, int, void*, int, int))(
        *(void***)surface)[0x64])(
        surface, 0, ddsd_buf, 1, 0);

    if (lock_result != 0) {
        DDRAW_GetDdrawErrorString(1);
        return 0;
    }

    uint32_t pitch = ddsd_buf[4];
    uint32_t* pixel_base = (uint32_t*)(uintptr_t)ddsd_buf[12];
    uint16_t pixel = *(uint16_t*)((uint8_t*)pixel_base + pitch * y + x * 2);

    uint32_t channel1 = (DAT_00485288 & (uint32_t)pixel) >> ((uint8_t)DAT_00485278 & 0x1f);
    uint32_t channel2 = DAT_00485290 & (uint32_t)pixel;

    uint8_t result = 1;
    if (channel1 != 0x1f || channel2 != 0x1f) {
        result = 0;
    }

    ((int (*)(void*, int))(
        *(void***)surface)[0x80])(
        surface, 0);

    return result;
}

/* ================================================================== */
/* Town::calc_scroll_rect — Calculate visible tile rect from scroll     */
/* Address: 0x42C590 (__thiscall)                                      */
/*                                                                     */
/* Ghidra decompilation of this function is garbled due to stack reuse  */
/* (DDSURFACEDESC overlaps with local rect variables). The core logic   */
/* has been reconstructed below. Returns 0 (always FALSE — BUG?).       */
/* ================================================================== */
uint32_t Town::calc_scroll_rect(RECT* pClipRect, void* surface)
{
    /* NOTE: The original binary allocates a DDSURFACEDESC (0x7C bytes)
     * on the stack and calls vtable[0x58] on 'surface' to fill it.
     * This gets the pixel dimensions of the source surface.
     *
     * Then it calls SetRect twice to build two RECT structures:
     *   surface_rect(0, 0, surface_width, surface_height)
     *   viewport_rect(0, 0, this->width, this->height) -- from this+0x08/+0x0C
     *
     * Then it compensates for negative scroll offsets in pClipRect
     * (since clipped regions can extend above/left of the origin),
     * creates a candidate rect from the adjusted origin + destination
     * dimensions, intersects it with surface_rect, and outputs the
     * result back through param_2/pClipRect.
     *
     * The function ALWAYS returns FALSE (0) — a likely bug where the
     * return value should indicate whether the computed rect is non-empty.
     */
    return 0;  /* Always returns FALSE — POSSIBLE BUG */
}

/* ================================================================== */
/* Town::calc_scroll_rect_reversed — Reversed scroll rect calculation   */
/* Address: 0x42C700 (__thiscall)                                      */
/* ================================================================== */
uint32_t Town::calc_scroll_rect_reversed(RECT* pClipRect, void* surface)
{
    /* Same DDSURFACEDESC setup as calc_scroll_rect, but the order
     * of operations differs:
     *   1. First IntersectRect clips viewportRect against pClipRect
     *   2. Returns immediately if the intersected rect is empty
     *   3. Then builds the source rect offset using the adjusted scroll origin
     *   4. Returns TRUE (1) on success
     * NOTE: Ghidra decompilation is garbled due to same stack issues.
     */
    return 1;
}

/* ================================================================== */
/* Town::postcard_init_list — Initialize postcard dialog (vtable[8])   */
/* Address: 0x42E420                                                   */
/* ================================================================== */
void Town::postcard_init_list()
{
    if (!this->sprites_initialized) {                            /* +0x5F9 */
        return;
    }

    if (!this->postcard_active) {                                /* +0x5F8 */
        this->postcard_active = 1;
    }

    this->postcard_send_handler(0);

    this->postcard_update_ui(2);
    this->postcard_update_ui(3);
    this->postcard_update_ui(4);
    this->postcard_update_ui(9);
    this->postcard_update_ui(7);
    this->postcard_update_ui(5);
    this->postcard_update_ui(6);
    this->postcard_update_ui(8);

    this->clear_postcard_ui();
    UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);

    SetFocus(this->hWnd);

    void* result = AudioMgr_PlayEvent(g_audio_mgr, 1, 0);
    if ((char)(intptr_t)result != 0) {
        this->audio_playing = 1;                                 /* +0x5ED */
    }
}

/* ================================================================== */
/* Town::postcard_send_handler — Render postcard overlay to primary     */
/* Address: 0x42E5E0                                                   */
/* ================================================================== */
void Town::postcard_send_handler(char full_render)
{
    if (!this->sprites_initialized || !this->postcard_active) {
        return;
    }

    RECT dest_rect;

    if (full_render == 0) {
        SetRectEmpty(&dest_rect);
        dest_rect.left   = this->postcard_origin_x;              /* +0x618 */
        dest_rect.top    = this->postcard_origin_y;              /* +0x61C */
        dest_rect.right  = this->postcard_width;                  /* +0x620 */
        dest_rect.bottom = this->postcard_height;                 /* +0x624 */
    } else {
        CopyRect(&dest_rect, &this->preview_rect);               /* +0x68C */
        OffsetRect(&dest_rect, this->postcard_origin_x,
                               this->postcard_origin_y);

        UIPANEL_Blit(
            this->overlay_surface,        /* +0x648 */
            this->preview_rect.left,      /* +0x68C */
            this->preview_rect.top,       /* +0x690 */
            this->preview_rect.right,     /* +0x694 */
            this->preview_rect.bottom,    /* +0x698 */
            g_primary_surface,
            dest_rect.left, dest_rect.top,
            dest_rect.right, dest_rect.bottom,
            1);

        CopyRect(&dest_rect, (RECT*)&this->send_rect_left);       /* +0x654 */
        OffsetRect(&dest_rect, this->postcard_origin_x,
                               this->postcard_origin_y);
    }

    UIPANEL_Blit(
        this->overlay_surface,              /* +0x648 */
        *(uint32_t*)((intptr_t)this + 0xD4), /* stale field offset */
        *(uint32_t*)((intptr_t)this + 0xD8),
        *(int32_t*)((intptr_t)this + 0xDC),
        *(uint32_t*)((intptr_t)this + 0xE0),
        g_primary_surface,
        dest_rect.left, dest_rect.top,
        dest_rect.right, dest_rect.bottom,
        1);
}

/* ================================================================== */
/* Town::postcard_update_ui — Postcard idle/release UI handler          */
/* Address: 0x42DE70                                                   */
/* ================================================================== */
void Town::postcard_update_ui(int action_id)
{
    if (!this->postcard_active) {                                /* +0x5F8 */
        return;
    }

    switch (action_id) {
    case 2:
        Sprite_SetState(this->sprite_btn_close, 0, nullptr);
        return;

    case 3:
        Sprite_SetState(this->sprite_btn_options, 0, nullptr);
        return;

    case 4:
        Sprite_SetState(this->sprite_btn_rotate, 0, nullptr);
        return;

    case 5:
        Sprite_SetState(this->sprite_btn_save, 0, nullptr);
        return;

    case 6:
        {
            RECT src_rect;
            CopyRect(&src_rect, (RECT*)((intptr_t)this->sprite_inbox + 4));
            OffsetRect(&src_rect, this->postcard_origin_x,
                                   this->postcard_origin_y);

            int sprite = (intptr_t)this->sprite_inbox;
            UIPANEL_Blit(
                this->overlay_surface,        /* +0x648 */
                *(uint32_t*)(uintptr_t)(sprite + 4),
                *(uint32_t*)(uintptr_t)(sprite + 8),
                *(int32_t*)(uintptr_t)(sprite + 0xC),
                *(uint32_t*)(uintptr_t)(sprite + 0x10),
                g_primary_surface,
                src_rect.left, src_rect.top,
                src_rect.right, src_rect.bottom,
                1);

            int msg_count = 0;
            if (!this->is_host) {
                msg_count = (int)DPLAY_GetMessageCount(g_dplay);
            } else {
                void* host = NET_GetHostName(1, 0);
                while (host) {
                    void* next = *(void**)((intptr_t)host + 0x504);
                    msg_count++;
                    GLOBAL_free(host);
                    host = next;
                }
                if (msg_count == 0) {
                    this->has_remote_players = 0;
                } else {
                    this->has_remote_players = 1;
                }
            }

            if (msg_count != 0) {
                Sprite_SetState(this->sprite_inbox, 0, nullptr);
                return;
            }
        }
        break;

    case 7:
        {
            uint16_t msg_count = 0;
            if (!this->is_host) {
                msg_count = (uint16_t)DPLAY_GetMessageCount(g_dplay);
            } else {
                void* host = NET_GetHostName(1, 0);
                while (host) {
                    void* next = *(void**)((intptr_t)host + 0x504);
                    msg_count++;
                    GLOBAL_free(host);
                    host = next;
                }
                if (msg_count == 0) {
                    this->has_remote_players = 0;
                } else {
                    this->has_remote_players = 1;
                }
            }

            int lut_idx = (msg_count < 5) ? msg_count : 4;
            Sprite_SetState(this->sprite_inbox_counter,
                            this->inbox_state_lut[lut_idx], nullptr);
        }
        return;

    case 8:
        {
            int msg_count = (int)DPLAY_GetMessageCount(g_dplay);
            int lut_idx = (msg_count < 5) ? msg_count : 4;
            Sprite_SetState(this->sprite_outbox_counter,
                            this->outbox_state_lut[lut_idx], nullptr);
        }
        return;

    case 9:
        Sprite_SetState(this->sprite_send, 0, nullptr);
        return;
    }
}

/* ================================================================== */
/* Town::postcard_dlg_proc — Postcard UI press/click handler            */
/* Address: 0x42E150                                                   */
/* ================================================================== */
void Town::postcard_dlg_proc(int action_id)
{
    if (!this->postcard_active) {
        return;
    }

    switch (action_id) {
    case 2:
        RESMGR_PlaySound(0x5015);
        Sprite_SetState(this->sprite_btn_close, 1, nullptr);
        return;

    case 3:
        RESMGR_PlaySound(0x5015);
        Sprite_SetState(this->sprite_btn_options, 1, nullptr);
        return;

    case 4:
        RESMGR_PlaySound(0x5015);
        Sprite_SetState(this->sprite_btn_rotate, 1, nullptr);
        return;

    case 5:
        RESMGR_PlaySound(0x5015);
        Sprite_SetState(this->sprite_btn_save, 1, nullptr);
        return;

    case 6:
        RESMGR_PlaySound(0x5015);
        {
            int sprite = (intptr_t)this->sprite_inbox;
            UIPANEL_Blit(
                this->overlay_surface,
                *(uint32_t*)(uintptr_t)(sprite + 4),
                *(uint32_t*)(uintptr_t)(sprite + 8),
                *(int32_t*)(uintptr_t)(sprite + 0xC),
                *(uint32_t*)(uintptr_t)(sprite + 0x10),
                g_primary_surface,
                *(uint32_t*)(uintptr_t)(sprite + 4) + this->postcard_origin_x,
                *(uint32_t*)(uintptr_t)(sprite + 8) + this->postcard_origin_y,
                *(int32_t*)(uintptr_t)(sprite + 0xC) + this->postcard_origin_x,
                *(uint32_t*)(uintptr_t)(sprite + 0x10) + this->postcard_origin_y,
                1);
            Sprite_SetState(this->sprite_inbox, 1, nullptr);
        }
        return;

    case 7:
        RESMGR_PlaySound(0x5015);
        {
            uint16_t msg_count = 0;
            if (!this->is_host) {
                msg_count = (uint16_t)DPLAY_GetMessageCount(g_dplay);
            } else {
                void* host = NET_GetHostName(1, 0);
                while (host) {
                    void* next = *(void**)((intptr_t)host + 0x504);
                    msg_count++;
                    GLOBAL_free(host);
                    host = next;
                }
                if (msg_count == 0) {
                    this->has_remote_players = 0;
                } else {
                    this->has_remote_players = 1;
                }
            }

            int lut_idx = (msg_count < 5) ? msg_count : 4;
            Sprite_SetState(this->sprite_inbox_counter,
                            this->inbox_state_lut[lut_idx] + 1, nullptr);
        }
        return;

    case 8:
        {
            int msg_count = (int)DPLAY_GetMessageCount(g_dplay);
            int lut_idx = (msg_count < 5) ? msg_count : 4;
            Sprite_SetState(this->sprite_outbox_counter,
                            this->outbox_state_lut[lut_idx], nullptr);
        }
        return;

    case 9:
        Sprite_SetState(this->sprite_send, 1, nullptr);
        return;
    }
}

/* ================================================================== */
/* Town::postcard_update_buttons — Blit button strip to primary surface */
/* Address: 0x42E4E0                                                   */
/* ================================================================== */
void Town::postcard_update_buttons()
{
    RECT dest_rect;

    if (this->repaint_requested) {                              /* +0x605 */
        SetRectEmpty(&dest_rect);
        dest_rect.right  = this->button_src_right - this->button_src_left;
        dest_rect.bottom = this->button_src_bottom - this->button_src_top;
        OffsetRect(&dest_rect, dest_rect.right, 0);
    } else {
        SetRectEmpty(&dest_rect);
        dest_rect.right  = this->button_src_right - this->button_src_left;
        dest_rect.bottom = this->button_src_bottom - this->button_src_top;
    }

    UIPANEL_Blit(
        this->button_strip_surface,        /* +0x668 */
        this->button_src_left,             /* +0x66C */
        this->button_src_top,              /* +0x670 */
        this->button_src_right,            /* +0x674 */
        this->button_src_bottom,           /* +0x678 */
        g_primary_surface,
        dest_rect.left, dest_rect.top,
        dest_rect.right, dest_rect.bottom,
        1);
}

/* ================================================================== */
/* Town::hit_test_buttons — Hit-test postcard overlay buttons           */
/* Address: 0x430090                                                   */
/* ================================================================== */
byte Town::hit_test_buttons(int32_t x, int32_t y)
{
    if (PtInRect((RECT*)((intptr_t)this->sprite_btn_close + 4), x, y)) {
        return 2;
    }
    if (PtInRect((RECT*)((intptr_t)this->sprite_btn_options + 4), x, y)) {
        return 3;
    }
    if (PtInRect((RECT*)((intptr_t)this->sprite_btn_rotate + 4), x, y)) {
        return 4;
    }
    if (PtInRect((RECT*)((intptr_t)this->sprite_btn_save + 4), x, y)) {
        return 5;
    }
    if (PtInRect((RECT*)((intptr_t)this->sprite_inbox + 4), x, y)) {
        return 6;
    }
    if (PtInRect((RECT*)((intptr_t)this->sprite_inbox_counter + 4), x, y)) {
        return 7;
    }
    if (PtInRect(&this->button_hit_rect_send, x, y)) {          /* +0x67C */
        return 9;
    }
    if (PtInRect((RECT*)((intptr_t)this->sprite_outbox_counter + 4), x, y)) {
        return 8;
    }
    return 0;
}

/* ================================================================== */
/* Town::hit_test — Postcard paint-throttle message handler (vtable[9])*/
/* Address: 0x42FFF0                                                   */
/* ================================================================== */
void Town::hit_test(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (this->timer_active == 0) {                               /* +0x5FC */
        goto forward_msg;
    }

    this->frame_counter++;                                      /* +0x600 */

    if (this->repaint_requested) {                              /* +0x605 */
        this->repaint_requested = 0;
        this->frame_counter = 0;
        this->postcard_update_buttons();
        UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
    } else if (this->frame_counter > 19) {
        this->repaint_requested = 1;
        this->postcard_update_buttons();
        UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
    }

    if (this->timer_counter != 0) {                              /* +0x5F0 */
        this->timer_counter--;
    }

forward_msg:
    DefWindowProcA(hWnd, msg, wParam, lParam);
}

/* ================================================================== */
/* Town::postcard_click_handler — Left-click on postcard overlay        */
/* Address: 0x42D670                                                   */
/* ================================================================== */
char Town::postcard_click_handler(int x, int y)
{
    if (this->selection_active != 1) {                          /* +0x88 */
        return 0;
    }

    if (this->postcard_click_flag) {                            /* +0x90 */
        this->postcard_click_flag = 0;
        return 1;
    }

    return RESDATA_HitTestChildren(this, x, y);
}

/* ================================================================== */
/* Town::postcard_command_handler — WM_COMMAND for postcard controls    */
/* Address: 0x42D6B0                                                   */
/* ================================================================== */
int Town::postcard_command_handler(void* control, uint32_t wParam, uint32_t lParam)
{
    if (control == nullptr || *(char*)((intptr_t)control + 0x56) == 0) {
        return 0;
    }

    char handled = ((char (*)(void*, uint32_t, uint32_t))(
        *(void***)control)[2])(control, wParam, lParam);
    if (handled == 0) {
        return 0;
    }

    int res_id = *(int*)(uintptr_t)(*(int*)((uint8_t*)control + 0x44) + 4);
    short timer_val;

    if (res_id == 0x3806) {
        timer_val = *(short*)((uint8_t*)control + 0x48);
        if (timer_val == 1) {
            CGWND_TrackPiece_SetZoom(control, 2);
            *(short*)((uint8_t*)control + 0x54) = 6;
        }
    } else if (res_id == 0x3807) {
        if (*(short*)((uint8_t*)control + 0x48) == 1) {
            DDRAW_SelectBuilding(&g_ddraw_building, this->selected_building);
            return 1;
        }
        DDRAW_SelectBuilding(&g_ddraw_building, nullptr);
        return 1;
    } else if (res_id == 0x3808) {
        timer_val = *(short*)((uint8_t*)control + 0x48);
        if (timer_val == 1) {
            CGWND_TrackPiece_SetZoom(control, 2);
            *(short*)((uint8_t*)control + 0x54) = 6;
        }
    }

    return 1;
}

/* ================================================================== */
/* Town::send_postcard — Postcard sending lifecycle handler             */
/* Address: 0x42D770                                                   */
/* ================================================================== */
byte Town::send_postcard(void* track_piece)
{
    if (track_piece == nullptr) {
        return 0;
    }

    if (*(short*)((intptr_t)track_piece + 0x54) >= 0) {
        (*(short*)((intptr_t)track_piece + 0x54))--;
    }

    int res_id = *(int*)(uintptr_t)(*(int*)((uint8_t*)track_piece + 0x44) + 4);

    if (res_id == 0x3806) {
        if (*(short*)((uint8_t*)track_piece + 0x54) == 0 &&
            *(short*)((uint8_t*)track_piece + 0x48) == 2) {
            CGWND_TrackPiece_SetZoom(track_piece, 1);

            void* world = *(void**)((intptr_t)this->selected_building + 0x44C);
            if (world) {
                this->select_building(nullptr);
                DDRAW_SelectBuilding(&g_ddraw_building, nullptr);
            }
        }
    } else if (res_id == 0x3807) {
        if (*(short*)((intptr_t)track_piece + 0x54) >= 0) {
            DDRAW_SelectBuilding(&g_ddraw_building, this->selected_building);
            *(short*)((intptr_t)track_piece + 0x54) = -1;
        }

        if (g_ddraw_active) {
            CGWND_TrackPiece_SetZoom(track_piece, 2);
        } else {
            CGWND_TrackPiece_SetZoom(track_piece, 1);
        }
        return 1;
    } else if (res_id - 0x3808 == 0) {
        if (*(short*)((intptr_t)track_piece + 0x54) == 0 &&
            *(short*)((intptr_t)track_piece + 0x48) == 2) {
            CGWND_TrackPiece_SetZoom(track_piece, 1);
            this->select_building(nullptr);
            return 1;
        }
    }

    return 1;
}

/* ================================================================== */
/* Town::clear_postcard_ui — Reset the postcard UI after closing        */
/* Address: 0x42E760                                                   */
/* ================================================================== */
void Town::clear_postcard_ui()
{
    if (!this->postcard_active) {                                /* +0x5F8 */
        return;
    }

    if (this->selected_player) {                                 /* +0x608 */
        if (*(short*)((intptr_t)this->selected_player + 0x3a) == 0) {
            RECT restore_rect;
            CopyRect(&restore_rect, &this->preview_rect);
            OffsetRect(&restore_rect, this->postcard_origin_x,
                                      this->postcard_origin_y);

            UIPANEL_Blit(
                this->overlay_surface,
                this->preview_rect.left,
                *(uint32_t*)((intptr_t)&this->preview_rect + 4),
                this->preview_rect.right,
                *(uint32_t*)((intptr_t)&this->preview_rect + 0xc),
                g_primary_surface,
                restore_rect.left, restore_rect.top,
                restore_rect.right, restore_rect.bottom,
                0);
        }

        DPLAY_RenderPlayer(g_dplay, (char)this->player_count_flag,
                           this->selected_player, g_primary_surface,
                           this->render_param_x, this->render_param_y,
                           this->render_extra, (RECT*)this->player_rect);

        RECT send_area;
        SetRectEmpty(&send_area);
        send_area.right  = this->send_rect_right - this->send_rect_left;
        send_area.bottom = this->send_rect_bottom - this->send_rect_top;

        UIPANEL_Blit(
            this->overlay_surface,
            this->send_rect_left,
            this->send_rect_top,
            this->send_rect_right,
            this->send_rect_bottom,
            g_primary_surface,
            send_area.left, send_area.top,
            send_area.right, send_area.bottom,
            0);

        this->postcard_update_ui(6);
        return;
    }

    this->postcard_send_handler(1);
    this->postcard_update_ui(8);
    this->postcard_update_ui(6);
}

/* ================================================================== */
/* Town::init_postcard_ui — De-initialize/reset the postcard UI         */
/* Address: 0x42F6C0 (MISNAMED: this is cleanup, not init)              */
/* ================================================================== */
void Town::init_postcard_ui()
{
    if (!this->visible) {                                        /* +0xE4 */
        goto update_network;
    }

    UI_WindowBase_Hide(this);
    {
        int i = ShowCursor(0);
        while (i >= 0) {
            i = ShowCursor(0);
        }
    }

    TileMap_InvalidateRect(&g_tilemap,
                           g_viewport_rect_left, g_viewport_rect_top,
                           g_viewport_rect_right, g_viewport_rect_bottom);

    if (this->selected_player) {
        ((void (*)(void*, int))(*(void***)this->selected_player)[0])(
            this->selected_player, 1);
        this->selected_player = nullptr;
        this->postcard_data = nullptr;
    }

    if (this->sprites_initialized) {
        Sprite_Destroy(this->sprite_btn_close);
        Sprite_Destroy(this->sprite_btn_options);
        Sprite_Destroy(this->sprite_btn_rotate);
        Sprite_Destroy(this->sprite_btn_save);
        Sprite_Destroy(this->sprite_inbox);
        Sprite_Destroy(this->sprite_outbox_counter);
        Sprite_Destroy(this->sprite_inbox_counter);
        Sprite_Destroy(this->sprite_send);

        if (this->button_strip_resource) {
            ((void (*)(void*))(*(void***)this->button_strip_resource)[2])(
                this->button_strip_resource);
        }
        if (this->background_resource) {
            ((void (*)(void*))(*(void***)this->background_resource)[2])(
                this->background_resource);
        }
        if (this->send_confirm_resource) {
            ((void (*)(void*))(*(void***)this->send_confirm_resource)[2])(
                this->send_confirm_resource);
        }

        this->sprites_initialized = 0;
    }

    if (this->timer_active) {
        KillTimer(this->hWnd, 0x4D);
    }
    this->timer_active = 0;
    this->postcard_active = 0;

update_network:
    short active_players = NET_UpdatePlayerList();
    NETMAN_CheckTimeout(g_netman, (uint32_t)(active_players != 0));

    this->net_update_flag = 0;
}

/* ================================================================== */
/* Town::init_overlay_sprite — One-shot postcard overlay init           */
/* Address: 0x42FDF0                                                   */
/* ================================================================== */
void Town::init_overlay_sprite()
{
    if (this->overlay_initialized) {                             /* +0x5FA */
        return;
    }

    void* res = ResourceManager_GetById(&g_resmgr, 0x3CF7);
    this->overlay_resource = res;                                /* +0x644 */
    if (res) {
        this->overlay_surface =                                  /* +0x648 */
            ((void* (*)(void*, int, int))(*(void***)res)[1])(res, 0, 0);
    }
    this->overlay_initialized = 1;
}

/* ================================================================== */
/* Town::init_postcard_sprites — Initialize postcard overlay sprites    */
/* Address: 0x42FE30                                                   */
/* ================================================================== */
void Town::init_postcard_sprites()
{
    if (this->sprites_initialized) {
        return;
    }

    Sprite_Init(this->sprite_btn_close);
    Sprite_Init(this->sprite_btn_options);
    Sprite_Init(this->sprite_btn_rotate);
    Sprite_Init(this->sprite_btn_save);
    Sprite_Init(this->sprite_inbox);
    Sprite_Init(this->sprite_inbox_counter);
    Sprite_Init(this->sprite_outbox_counter);
    Sprite_Init(this->sprite_send);

    void* res = ResourceManager_GetById(&g_resmgr, 0x3CF8);
    this->background_resource = res;                             /* +0x64C */
    this->background_surface =                                    /* +0x650 */
        ((void* (*)(void*, int, int))(*(void***)res)[1])(res, 0, 0);

    res = ResourceManager_GetById(&g_resmgr, 0x3CFB);
    this->button_strip_resource = res;                            /* +0x664 */
    this->button_strip_surface =                                   /* +0x668 */
        ((void* (*)(void*, int, int))(*(void***)res)[1])(res, 0, 0);

    res = ResourceManager_GetById(&g_resmgr, 0x3CFA);
    this->send_confirm_resource = res;                             /* +0x69C */
    this->send_confirm_surface =                                    /* +0x6A0 */
        ((void* (*)(void*, int, int))(*(void***)res)[1])(res, 0, 0);

    this->sprites_initialized = 1;                                 /* +0x5F9 */
}

/* ================================================================== */
/* Town::load_background — WM_SYSCOMMAND handler (vtable[11])           */
/* Address: 0x42EE20                                                   */
/* ================================================================== */
void Town::load_background(HWND hWnd, UINT msg, uint32_t wParam, LPARAM lParam)
{
    if (msg == 0x112) {                                          /* WM_SYSCOMMAND */
        if ((wParam & 0xFFF0) == 0xF140) {                       /* SC_SCREENSAVE */
            PostQuitMessage(0);
            if (this->timer_active) {
                KillTimer(this->hWnd, 0x4D);
                this->timer_active = 0;
            }
        }
    } else if (msg == 0x5F5) {                                   /* WM_USER+0x1F5 */
        EnableWindow(this->hWnd, 1);
    }

    DefWindowProcA(hWnd, msg, wParam, lParam);
}

/* ================================================================== */
/* Town::postcard_draw_preview — Preview overlay dialog proc (vtable[10])*/
/* Address: 0x42F810                                                   */
/* ================================================================== */
int32_t Town::postcard_draw_preview(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (this->postcard_active && !this->flag_E8 && !this->audio_playing) {
        if (wParam == 0x1B || wParam == 0x51) {   /* ESC or Q */
            this->postcard_dlg_proc(2);
            UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
            Sleep(0x96);
            RESMGR_PlaySound(0x5015);

            ((void (*)(void*))(*(void***)this)[1])(this);
            return 0;
        }
        return DefWindowProcA(hWnd, msg, wParam, lParam);
    }

    return 0;
}

/* ================================================================== */
/* Town::upload_postcard — Write postcard to files for remote players   */
/* Address: 0x4309B0                                                   */
/* ================================================================== */
void Town::upload_postcard()
{
    if (!this->selected_player) {
        return;
    }

    int type = this->is_host ? 1 : 2;
    const char* hostname = NET_GetHostName(type, 0);

    while (hostname) {
        void* addr = NET_ResolveAddress(hostname);
        if (addr && *(int*)((intptr_t)addr + 0xC) ==
                    *(int*)((intptr_t)this->selected_player + 0xC)) {
            NET_UnregisterPlayer(g_dplay, hostname);
            HANDLE hFile = CreateFileA(hostname,
                                       0x40000000,
                                       1,
                                       nullptr,
                                       2,
                                       0x8000000,
                                       nullptr);
            if (hFile != (HANDLE)-1) {
                DWORD written;
                WriteFile(hFile, (void*)((intptr_t)this->selected_player + 4),
                          0x398, &written, nullptr);
                CloseHandle(hFile);
            }
        }
        if (addr) {
            ((void (*)(void*, int))(*(void***)addr)[0])(addr, 1);
        }
        const char* next = *(const char**)((intptr_t)hostname + 0x504);
        GLOBAL_free(hostname);
        hostname = next;
    }
}

/* ================================================================== */
/* Town::receive_postcard — Process incoming network postcard           */
/* Address: 0x42D8A0                                                   */
/* ================================================================== */
void Town::receive_postcard()
{
    if (!this->postcard_data) {                                  /* +0x60C */
        return;
    }

    int type = this->is_host ? 1 : 2;
    const char* hostname = NET_GetHostName(type, 0);

    while (hostname) {
        void* addr = NET_ResolveAddress(hostname);
        if (this->selected_player &&
            addr && *(int*)((intptr_t)addr + 0xC) ==
                    *(int*)((intptr_t)this->selected_player + 0xC)) {
            NET_UnregisterPlayer(g_dplay, hostname);
        }
        if (addr) {
            ((void (*)(void*, int))(*(void***)addr)[0])(addr, 1);
        }
        const char* next = *(const char**)((intptr_t)hostname + 0x504);
        GLOBAL_free(hostname);
        hostname = next;
    }

    if (*(short*)((intptr_t)this->postcard_data + 0x3A) != 0) {
        *(short*)((intptr_t)this->postcard_data + 0x3A) = 0;
    }

    NET_RegisterPlayer(g_dplay, this->postcard_data, 0, 0);

    if (this->selected_player) {
        ((void (*)(void*, int))(*(void***)this->selected_player)[0])(
            this->selected_player, 1);
    }
    this->selected_player = nullptr;

    this->list_postcards();

    int count = 0;
    if (this->is_host) {
        const char* p = NET_GetHostName(1, 0);
        while (p) {
            const char* next = *(const char**)((intptr_t)p + 0x504);
            count++;
            GLOBAL_free(p);
            p = next;
        }
        this->has_remote_players = (count > 0) ? 1 : 0;
    } else {
        DPLAY_GetMessageCount(g_dplay);
    }
}

/* ================================================================== */
/* Town::list_postcards — Cycle to next .crd entry                     */
/* Address: 0x42DD50                                                   */
/* ================================================================== */
void Town::list_postcards()
{
    int type = this->is_host ? 1 : 2;
    const char* first_hostname = NET_GetHostName(type, 0);

    if (!first_hostname) {
        return;
    }

    void* first_addr = NET_ResolveAddress(first_hostname);
    if (!this->selected_player) {
        this->selected_player = first_addr;
        const char* p = first_hostname;
        while (p) {
            const char* next = *(const char**)((intptr_t)p + 0x504);
            GLOBAL_free(p);
            p = next;
        }
        return;
    }

    void* next_player = nullptr;
    const char* p = first_hostname;
    while (p) {
        void* addr = NET_ResolveAddress(p);

        if (!next_player &&
            addr && *(int*)((intptr_t)addr + 0xC) ==
                    *(int*)((intptr_t)this->selected_player + 0xC) &&
            *(const char**)((intptr_t)p + 0x504)) {
            next_player = NET_ResolveAddress(
                *(const char**)((intptr_t)p + 0x504));
        }

        if (addr) {
            ((void (*)(void*, int))(*(void***)addr)[0])(addr, 1);
        }
        const char* next = *(const char**)((intptr_t)p + 0x504);
        GLOBAL_free(p);
        p = next;
    }

    if (!next_player) {
        if (this->selected_player) {
            ((void (*)(void*, int))(*(void***)this->selected_player)[0])(
                this->selected_player, 1);
        }
        this->selected_player = first_addr;
        return;
    }

    if (first_addr) {
        ((void (*)(void*, int))(*(void***)first_addr)[0])(first_addr, 1);
    }
    if (this->selected_player) {
        ((void (*)(void*, int))(*(void***)this->selected_player)[0])(
            this->selected_player, 1);
    }
    this->selected_player = next_player;
}

/* ================================================================== */
/* Town::save_postcard — Client-side session re-registration            */
/* Address: 0x42DA10                                                   */
/* ================================================================== */
void Town::save_postcard()
{
    if (!this->postcard_data || this->is_host) {
        return;
    }

    const char* hostname = NET_GetHostName(2, 0);
    while (hostname) {
        void* addr = NET_ResolveAddress(hostname);
        if (this->selected_player &&
            addr && *(int*)((intptr_t)addr + 0xC) ==
                    *(int*)((intptr_t)this->selected_player + 0xC)) {
            NET_UnregisterPlayer(g_dplay, hostname);
        }
        if (addr) {
            ((void (*)(void*, int))(*(void***)addr)[0])(addr, 1);
        }
        const char* next = *(const char**)((intptr_t)hostname + 0x504);
        GLOBAL_free(hostname);
        hostname = next;
    }

    NET_RegisterPlayer(g_dplay, this->postcard_data, 1, 0);

    if (this->selected_player) {
        ((void (*)(void*, int))(*(void***)this->selected_player)[0])(
            this->selected_player, 1);
    }
    this->selected_player = nullptr;

    this->list_postcards();

    int count = 0;
    if (this->is_host) {
        const char* p = NET_GetHostName(1, 0);
        while (p) {
            const char* next = *(const char**)((intptr_t)p + 0x504);
            count++;
            GLOBAL_free(p);
            p = next;
        }
        this->has_remote_players = (count > 0) ? 1 : 0;
    } else {
        DPLAY_GetMessageCount(g_dplay);
    }
}

/* ================================================================== */
/* Town::load_postcard — Host-side session re-registration              */
/* Address: 0x42DB30                                                   */
/* ================================================================== */
void Town::load_postcard()
{
    if (!this->is_host || !this->postcard_data) {
        return;
    }

    const char* hostname = NET_GetHostName(1, 0);
    while (hostname) {
        void* addr = NET_ResolveAddress(hostname);
        if (this->selected_player &&
            addr && *(int*)((intptr_t)addr + 0xC) ==
                    *(int*)((intptr_t)this->selected_player + 0xC)) {
            NET_UnregisterPlayer(g_dplay, hostname);
        }
        if (addr) {
            ((void (*)(void*, int))(*(void***)addr)[0])(addr, 1);
        }
        const char* next = *(const char**)((intptr_t)hostname + 0x504);
        GLOBAL_free(hostname);
        hostname = next;
    }

    NET_RegisterPlayer(g_dplay, this->postcard_data, 2, 0);

    if (this->selected_player) {
        ((void (*)(void*, int))(*(void***)this->selected_player)[0])(
            this->selected_player, 1);
    }
    this->selected_player = nullptr;

    this->list_postcards();

    int count = 0;
    if (this->is_host) {
        const char* p = NET_GetHostName(1, 0);
        while (p) {
            const char* next = *(const char**)((intptr_t)p + 0x504);
            count++;
            GLOBAL_free(p);
            p = next;
        }
        this->has_remote_players = (count > 0) ? 1 : 0;
    } else {
        DPLAY_GetMessageCount(g_dplay);
    }
}

/* ================================================================== */
/* Town::delete_postcard — Delete a .crd postcard file                  */
/* Address: 0x42DC50                                                   */
/* ================================================================== */
void Town::delete_postcard()
{
    if (!this->selected_player) {
        return;
    }

    int type = this->is_host ? 1 : 2;
    const char* hostname = NET_GetHostName(type, 0);

    while (hostname) {
        void* addr = NET_ResolveAddress(hostname);
        if (this->selected_player &&
            addr && *(int*)((intptr_t)addr + 0xC) ==
                    *(int*)((intptr_t)this->selected_player + 0xC)) {
            NET_UnregisterPlayer(g_dplay, hostname);
            if (this->selected_player) {
                ((void (*)(void*, int))(*(void***)this->selected_player)[0])(
                    this->selected_player, 1);
            }
            this->selected_player = nullptr;
        }
        if (addr) {
            ((void (*)(void*, int))(*(void***)addr)[0])(addr, 1);
        }
        const char* next = *(const char**)((intptr_t)hostname + 0x504);
        GLOBAL_free(hostname);
        hostname = next;
    }

    int count = 0;
    if (this->is_host) {
        const char* p = NET_GetHostName(1, 0);
        while (p) {
            const char* next = *(const char**)((intptr_t)p + 0x504);
            count++;
            GLOBAL_free(p);
            p = next;
        }
        this->has_remote_players = (count > 0) ? 1 : 0;
    } else {
        DPLAY_GetMessageCount(g_dplay);
    }
}

/* ================================================================== */
/* Town::save_postcard_as — Show "Save As" dialog for received postcard */
/* Address: 0x42EEA0                                                   */
/*                                                                     */
/* Shows GetSaveFileName dialog, handles file-exists overwrite and     */
/* directory-not-found prompts. Returns 0=cancelled/error, 1=OK,       */
/* 2=create-dir-needed.                                                */
/*                                                                     */
/* NOTE: Uses the +0xE9..+0x5ED buffer region as the filename buffer   */
/* (passed as OPENFILENAME.lpstrFile with nMaxFile=0x504). This buffer */
/* overlaps with viewport/overlay fields but is only called during     */
/* postcard UI transitions when those fields are not in use.           */
/* ================================================================== */
byte Town::save_postcard_as()
{
    /* Step 1: Download the postcard attachment from network cache       */
    /* Uses selected_player->player_id as the asset identifier          */
    uint16_t player_id = *(uint16_t*)((intptr_t)this->selected_player + 0x3A);
    NET_DownloadAsset(g_net_cache_mgr, player_id, 5,
                      (char*)this + 0xE9);  /* buffer at +0xE9 */

    /* Step 2: Format dialog title string (resource 0x6a)               */
    char title_buf[256];
    FormatResourceString(g_resmgr, 0x6a, title_buf, sizeof(title_buf));

    /* Step 3: Set up OPENFILENAME structure                            */
    OPENFILENAMEA ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize       = sizeof(ofn);                   /* 0x4c */
    ofn.hwndOwner         = this->hWnd;
    ofn.hInstance         = this->hInstance;
    ofn.lpstrFilter       = "Bitmap Files\0*.bmp\0All Files\0*.*\0";
    ofn.nFilterIndex      = 1;
    ofn.lpstrFile         = (char*)this + 0xE9;            /* +0xE9 filename buffer */
    ofn.nMaxFile          = 0x504;                          /* 1284 bytes */
    ofn.lpstrFileTitle    = title_buf;
    ofn.nMaxFileTitle     = 0x104;
    ofn.lpstrTitle        = title_buf;
    ofn.Flags             = 0x80024;  /* OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_EXPLORER */
    ofn.lpfnHook          = (LPOFNHOOKPROC)&SaveAsDlgHook; /* 0x419FD0 custom hook */

    /* Step 4: Show the dialog — enable window, set flag, reposition    */
    PostMessageA(this->hWnd, 0x5F5, 0, 0);  /* WM_USER+0x1F5: enable town window */
    this->flag_E8 = 1;                       /* +0xE8 dialog guard */

    /* Reposition cursor near the button area (from postcard overlay)   */
    SetCursorPos(
        *(int*)((intptr_t)this->overlay_panel + 4),    /* overlay X */
        *(int*)((intptr_t)this->overlay_panel + 0x10) + 0x14); /* overlay bottom + offset */

    /* Center viewport via vtable[3]                                    */
    ((void (*)(void*, int, int, int, int))(*(void***)this)[3])(
        this,
        *(int*)((intptr_t)this + 0x60),   /* child_panel[0] area */
        *(int*)((intptr_t)this + 0x64),   /* child_panel[4] area */
        0, 1);

    /* Step 5: Show GetSaveFileNameA dialog                             */
    BOOL result = GetSaveFileNameA(&ofn);
    this->flag_E8 = 0;                       /* +0xE8 dialog guard cleared */

    if (result != 0) {
        /* Step 6: Check if chosen file already exists                  */
        HANDLE hFile = CreateFileA(ofn.lpstrFile, GENERIC_READ | GENERIC_WRITE,
                                   FILE_SHARE_READ, NULL,
                                   OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, NULL);

        if (hFile != INVALID_HANDLE_VALUE) {
            /* File exists — prompt for overwrite                       */
            char msg_buf[256];
            FormatResourceString(g_resmgr, 0x6b, msg_buf, sizeof(msg_buf));
            int choice = MessageBoxA(this->hWnd, msg_buf,
                                     s_LEGO_LOCO_0047e1c0,
                                     MB_YESNO | MB_ICONQUESTION);
            CloseHandle(hFile);

            if (choice == IDYES) {       /* Yes = overwrite */
                return 1;
            }
            return 0;                     /* No = cancel */
        }

        /* Step 7: Handle error codes                                   */
        DWORD err = GetLastError();

        if (err == ERROR_FILE_NOT_FOUND) {
            /* File doesn't exist — OK to create new one                */
            return 1;
        }

        if (err == ERROR_PATH_NOT_FOUND) {
            /* Path doesn't exist — ask about creating directory        */
            char msg_buf[256];
            FormatResourceString(g_resmgr, 0x6d, msg_buf, sizeof(msg_buf));
            int choice = MessageBoxA(this->hWnd, msg_buf,
                                     s_LEGO_LOCO_0047e1c0,
                                     MB_YESNO | MB_ICONWARNING);
            /* Returns 2 if YES (create dir), 0 if NO                   */
            return (choice != IDYES) ? 0 : 2;
        }

        /* Other errors — show error dialog                             */
        {
            char msg_buf[256];
            FormatResourceString(g_resmgr, 0x6d, msg_buf, sizeof(msg_buf));
            MessageBoxA(this->hWnd, msg_buf,
                        s_LEGO_LOCO_0047e1c0,
                        MB_OK | MB_ICONSTOP);
        }
    }

    return 0;  /* Cancelled or error */
}

/* ================================================================== */
/* Town::save_received_postcard — Download and save a received postcard */
/* Address: 0x42F250                                                   */
/*                                                                     */
/* Opens cached postcard file from network, reads payload into          */
/* the +0xE9 buffer, shows Save As dialog, copies to user path,        */
/* cleans up temp files, and notifies peers.                            */
/* ================================================================== */
void Town::save_received_postcard(uint32_t player_id)
{
    /* Step 1: Get cached file path and open for reading                */
    char cache_path[1284];
    NET_GetFilePath(g_net_cache_mgr, player_id, 5, cache_path);

    HANDLE hFile = CreateFileA(cache_path, GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        /* Failed to open cache file                                    */
        DWORD err = GetLastError();
        char* msg_buf = NULL;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                       NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       (char*)&msg_buf, 0, NULL);

        MessageBoxA(this->hWnd, msg_buf, s_LEGO_LOCO_0047e1c0, MB_ICONERROR);
        LocalFree(msg_buf);

        /* Clear need_connect flag                                      */
        *(uint16_t*)((intptr_t)this->selected_player + 0x3A) = 0;

        this->clear_postcard_ui();
        UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
        return;
    }

    /* Step 2: Read postcard payload into +0xE9 buffer                  */
    char* payload_buf = (char*)this + 0xE9;
    DWORD bytes_read;
    BOOL read_ok = ReadFile(hFile, payload_buf, 0x504, &bytes_read, NULL);

    if (!read_ok) {
        DWORD err = GetLastError();
        CloseHandle(hFile);

        char* msg_buf = NULL;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                       NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       (char*)&msg_buf, 0, NULL);

        MessageBoxA(this->hWnd, msg_buf, s_LEGO_LOCO_0047e1c0, MB_ICONERROR);
        LocalFree(msg_buf);

        *(uint16_t*)((intptr_t)this->selected_player + 0x3A) = 0;
        this->clear_postcard_ui();
        UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
        return;
    }

    CloseHandle(hFile);

    /* Step 3: Show Save As dialog — the chosen path overwrites the     */
    /* +0xE9 buffer with the user's selected filename.                  */
    byte save_result = this->save_postcard_as();
    if (save_result == 0) {
        return;  /* User cancelled */
    }

    /* Step 4: If directory needs creating, extract dir and create it   */
    if (save_result == 2) {
        char dir_path[1284];
        strcpy(dir_path, payload_buf);  /* payload_buf now has the filename from GetSaveFileName */

        /* Find last backslash and truncate to directory path           */
        char* last_slash = _strrchr(dir_path, '\\');
        if (last_slash != NULL) {
            *last_slash = '\0';
        }

        if (!CreateDirectoryA(dir_path, NULL)) {
            DWORD err = GetLastError();
            char* msg_buf = NULL;
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                           NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           (char*)&msg_buf, 0, NULL);

            MessageBoxA(this->hWnd, msg_buf, s_LEGO_LOCO_0047e1c0, MB_ICONERROR);
            LocalFree(msg_buf);
            return;
        }
    }

    /* Step 5: Copy from attachment cache path to user's chosen path    */
    uint16_t pid = *(uint16_t*)((intptr_t)this->selected_player + 0x3A);
    char att_path[1284];
    NET_GetAttFilePath(g_net_cache_mgr, pid, 5, att_path);

    BOOL copy_ok = CopyFileA(att_path, payload_buf, FALSE);
    if (!copy_ok) {
        DWORD err = GetLastError();
        char* msg_buf = NULL;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                       NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       (char*)&msg_buf, 0, NULL);

        MessageBoxA(this->hWnd, msg_buf, s_LEGO_LOCO_0047e1c0, MB_ICONERROR);
        LocalFree(msg_buf);
    }

    /* Step 6: Delete cache files                                       */
    BOOL del_ok = DeleteFileA(att_path);
    if (!del_ok) {
        DWORD err = GetLastError();
        char* msg_buf = NULL;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                       NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       (char*)&msg_buf, 0, NULL);

        MessageBoxA(this->hWnd, msg_buf, s_LEGO_LOCO_0047e1c0, MB_ICONERROR);
        LocalFree(msg_buf);
    } else {
        /* Also delete the main cache file                              */
        NET_GetFilePath(g_net_cache_mgr, pid, 5, cache_path);
        BOOL del2_ok = DeleteFileA(cache_path);
        if (!del2_ok) {
            DWORD err = GetLastError();
            char* msg_buf = NULL;
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                           NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           (char*)&msg_buf, 0, NULL);

            MessageBoxA(this->hWnd, msg_buf, s_LEGO_LOCO_0047e1c0, MB_ICONERROR);
            LocalFree(msg_buf);
        }

        /* Clear need_connect flag                                      */
        *(uint16_t*)((intptr_t)this->selected_player + 0x3A) = 0;

        /* Step 7: Notify peers                                          */
        this->upload_postcard();
    }

    /* Step 8: Clean up postcard UI                                     */
    this->clear_postcard_ui();
    UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
}

/* ================================================================== */
/* NOTE: PostcardPreviewWindow and GameView implementations have been   */
/* moved to their own class files:                                      */
/*   - PostcardPreviewWindow: src/decompiled_cpp/ui/PostcardPreviewWindow.cpp */
/*   - GameView:              src/decompiled_cpp/core/GameView.cpp       */
/* ================================================================== */

/* ================================================================== */
/* Train_HandleTrackBuild — Process remote track-build network message  */
/* Address: 0x43CE10                                                   */
/*                                                                     */
/* Creates a local vehicle with random type, inits routes for each     */
/* track piece, copies local player info, downloads missing assets,    */
/* pops one car from the pending list, updates player info.            */
/* ================================================================== */
void Train_HandleTrackBuild(void* msg)
{
    /* Handles message type 0x3EC by creating a vehicle with
       random type (from 3 variants starting at res 0x1804),
       initializing routes, updating DPLAY player info.
       Full implementation TBD. */
}

/* ================================================================== */
/* Town::blit_element — Extract element surface and forward to Blit    */
/* Address: 0x42B960                                                   */
/*                                                                     */
/* Thin wrapper that extracts a DirectDraw surface pointer from the    */
/* element struct (element+0x1C) and passes it as the dest surface     */
/* to UIPANEL_Blit. All other parameters pass through unchanged.       */
/*                                                                     */
/* NOTE: `this` in all tile rendering methods is the tile cache         */
/* context (UIPANEL surface context), NOT the Town instance itself.    */
/* The methods are called by the UIPANEL_Blit dispatch mechanism.      */
/*                                                                     */
/* Called by: EditWindow_render, Cursor_InitBackground,                */
/*            CGWND_TrackPiece_Render, UIPANEL_DrawButton              */
/* ================================================================== */
void Town::BlitElement(
    int dest_x, int dest_y, int dest_w, int dest_h,
    void* element, int clip_left, int clip_top,
    int clip_right, int clip_bottom, uint32_t flags)
{
    /* Extract DirectDraw surface pointer from element+0x1C */
    void* ddraw_surface = *(void**)((intptr_t)element + 0x1C);

    /* Delegate to the main blit dispatcher with the element's surface */
    UIPANEL_Blit(
        this,                        /* +0x00  source surface context */
        dest_x, dest_y,              /* +0x04,+0x08  source position */
        dest_w, dest_h,              /* +0x0C,+0x10  source dimensions */
        ddraw_surface,               /* from element+0x1C */
        clip_left, clip_top,         /* +0x18,+0x1C  dest position */
        clip_right, clip_bottom,     /* +0x20,+0x24  dest dimensions */
        flags);                      /* +0x28  blit operation flags */
}

/* ================================================================== */
/* Town::init_tile_cache — Copy 8bpp to 16bpp via palette lookup       */
/* Address: 0x42B9C0                                                   */
/*                                                                     */
/* Copies 8-bit indexed pixel data from this->pixels through           */
/* this->palette (byte-to-uint16_t lookup) to a 16-bit dest surface.   */
/* All source indices are treated as colors (no transparency logic).   */
/*                                                                     */
/* Tile cache context fields used:                                     */
/*   +0x08: stride — bytes per row in 8bpp source buffer               */
/*   +0x14: palette — uint16_t[256] color lookup table                 */
/*   +0x18: pixels — uint8_t* 8-bit indexed tile pixel data            */
/*                                                                     */
/* Called by: UIPANEL_Blit (flags=0x01, 0x03), UIPANEL_UnlockSurface   */
/* ================================================================== */
uint8_t Town::init_tile_cache(
    int dest_x, int dest_y, int dest_w, int dest_h,
    uintptr_t dest_base, uint32_t dest_pitch,
    int clip_left, int clip_top, int clip_right, int clip_bottom)
{
    uint8_t*  src_pixels = *(uint8_t**)((intptr_t)this + 0x18);    /* +0x18 */
    uint16_t* pal        = *(uint16_t**)((intptr_t)this + 0x14);   /* +0x14 */

    /* Guard: must have pixel data and palette */
    if (src_pixels == nullptr || pal == nullptr) {
        return 0;
    }

    int      src_stride  = *(int*)((intptr_t)this + 0x08);        /* +0x08 */
    uint32_t width       = (uint32_t)(clip_right - clip_left) & 0xFFFF;
    uint32_t height      = (uint32_t)(clip_bottom - clip_top) & 0xFFFF;
    uint32_t half_pitch  = (dest_pitch >> 1) & 0xFFFF;             /* dest pitch in uint16_t units */

    /* Source pointer: start of clipped region in the 8-bit tile cache */
    uint8_t* src = src_pixels + clip_top * src_stride + clip_left;

    /* Destination pointer: 16-bit at (dest_x, dest_y) on the surface */
    uint16_t* dest = (uint16_t*)(dest_base + (half_pitch * (uint32_t)dest_y + (uint32_t)dest_x) * 2);

    /* Row advancement: skip remaining stride after copying width pixels */
    int32_t src_advance  = src_stride - (int32_t)width;
    int32_t dest_advance = (int32_t)(half_pitch - width) * 2;       /* in bytes */

    /* Clip-space end-of-buffer marker for pointer-based loop */
    uint8_t* buf_end = src + width + (height - 1) * src_stride - 1;

    if (src >= buf_end) {
        return 1;  /* empty region */
    }

    /* Iterate rows — pointer comparison exit */
    while (src < buf_end) {
        uint8_t* row_end = src + width;

        /* Iterate columns — pointer comparison */
        while (src < row_end) {
            *dest = pal[*src];       /* palette lookup: byte index -> uint16_t color */
            dest++;
            src++;
        }

        /* Advance to next row */
        src  += src_advance;
        dest  = (uint16_t*)((uint8_t*)dest + dest_advance);
    }

    return 1;
}

/* ================================================================== */
/* Town::draw_tile — Draw indexed tile with palette remap + transp.    */
/* Address: 0x42BA90                                                   */
/*                                                                     */
/* For each pixel, saves the original destination value to palette[0], */
/* reads the source 8-bit index, and writes palette[index] to dest.    */
/* Source index 0 reads palette[0] = original dest = transparency.     */
/*                                                                     */
/* This avoidsa per-pixel conditional branch: index 0 automatically    */
/* becomes transparent by reading back the just-saved destination.     */
/*                                                                     */
/* Called by: UIPANEL_Blit (flags=0x00, fallback/default)              */
/* ================================================================== */
uint8_t Town::draw_tile(
    int dest_x, int dest_y, int dest_w, int dest_h,
    intptr_t dest_base, uint32_t dest_pitch,
    int clip_left, int clip_top, int clip_right, int clip_bottom)
{
    uint8_t*  src_pixels = *(uint8_t**)((intptr_t)this + 0x18);    /* +0x18 */
    uint16_t* palette    = *(uint16_t**)((intptr_t)this + 0x14);   /* +0x14 */

    /* Guard: must have pixel data and palette */
    if (src_pixels == nullptr || palette == nullptr) {
        return 0;
    }

    uint32_t width       = (uint32_t)(clip_right - clip_left) & 0xFFFF;
    uint32_t height      = (uint32_t)(clip_bottom - clip_top) & 0xFFFF;
    uint32_t half_pitch  = (dest_pitch >> 1) & 0xFFFF;

    /* Early out: empty region */
    if (width == 0 || height == 0) {
        return 1;
    }

    int src_stride = *(int*)((intptr_t)this + 0x08);              /* +0x08 */

    /* Destination pointer: 16-bit at (dest_x, dest_y) */
    uint16_t* dest = (uint16_t*)(dest_base + (half_pitch * (uint32_t)dest_y + (uint32_t)dest_x) * 2);

    /* Source pointer: 8-bit at (clip_left, clip_top) in tile cache */
    uint8_t* src = src_pixels + clip_top * src_stride + clip_left;

    int src_advance  = src_stride - (int32_t)width;
    int dest_advance = (int32_t)(half_pitch - width) * 2;         /* in bytes */

    /* Iterate rows */
    do {
        uint32_t remaining = width;

        /* Iterate columns */
        do {
            uint16_t saved_dest = *dest;           /* save original dest pixel */
            uint8_t  src_idx    = *src;            /* read source index byte */
            src++;
            remaining--;

            /* KEY TRANSPARENCY TRICK: Overwrite palette[0] with the original
             * destination pixel value. Source index 0 will then read back
             * palette[0] = the saved original = transparent pass-through. */
            palette[0] = saved_dest;               /* +0x14[0] = dest pixel */
            *dest = palette[src_idx];              /* +0x14[src_idx] -> dest */
            dest++;
        } while (remaining != 0);

        /* Advance to next row */
        src  += src_advance;
        dest  = (uint16_t*)((uint8_t*)dest + dest_advance);
        height--;
    } while (height != 0);

    return 1;
}

/* ================================================================== */
/* Town::flush_tile_cache — 2x2 block expansion with transparency      */
/* Address: 0x42BB90                                                   */
/*                                                                     */
/* Reads each 8-bit source byte once, writes 4 identical 16-bit pixels */
/* in a 2x2 block. Palette index 0 = skip (preserves dest behind it).  */
/*                                                                     */
/* Source buffer: stride = (clip_right - clip_left), inner loop runs   */
/* `clip_right` columns per row (NOT clip_right - clip_left).          */
/* Outer loop runs `clip_bottom` rows directly (not a delta).          */
/*                                                                     */
/* Called by: UIPANEL_Blit (flags=0x04, 0x84)                          */
/* ================================================================== */
uint8_t Town::flush_tile_cache(
    int dest_x, int dest_y, int dest_w, int dest_h,
    uintptr_t dest_base, uint32_t dest_pitch,
    int clip_left, int clip_top, int clip_right, uint32_t clip_bottom)
{
    uint32_t width       = (uint32_t)(clip_right - clip_left) & 0xFFFF;
    uint32_t half_pitch  = (dest_pitch >> 1) & 0xFFFF;
    uint32_t full_width  = (width + (uint32_t)clip_left) & 0xFFFF;  /* inner loop count */

    /* Destination pointer: 16-bit at (dest_x, dest_y) */
    uint16_t* dest = (uint16_t*)(dest_base + (half_pitch * (uint32_t)dest_y + (uint32_t)dest_x) * 2);

    uint8_t*  src_pixels = *(uint8_t**)((intptr_t)this + 0x18);    /* +0x18 */
    uint16_t* palette    = *(uint16_t**)((intptr_t)this + 0x14);   /* +0x14 */

    uint32_t row = 0;

    if (clip_bottom == 0) {
        return 1;
    }

    /* Iterate rows */
    do {
        uint32_t col = 0;

        if (full_width != 0) {
            do {
                /* Read source byte */
                uint8_t index = *(src_pixels + width * (row & 0xFFFF) + col);

                if (index != 0) {
                    /* Expand to 2x2 block */
                    uint16_t color = palette[index];            /* +0x14 */

                    dest[0] = color;                /* top-left */
                    dest[1] = color;                /* top-right */
                    dest[half_pitch] = dest[0];     /* bottom-left */
                    dest[half_pitch + 1] = dest[0]; /* bottom-right */
                }

                dest += 2;   /* advance 2 dest pixels per source pixel */
                col++;
            } while ((col & 0xFFFF) < full_width);
        }

        row++;
        /* Advance dest to next row: (half_pitch - width) * 2 uint16_t units */
        dest = dest + (half_pitch - width) * 2;

    } while ((row & 0xFFFF) < clip_bottom);

    return 1;
}

/* ================================================================== */
/* Town::draw_cached_tile — 2x2 block expansion, no transparency skip  */
/* Address: 0x42BC80                                                   */
/*                                                                     */
/* Same 2x2 block expansion as FlushTileCache but WITHOUT the index 0  */
/* transparent skip. All source indices are drawn. Used for regions    */
/* that were pre-cleared or have no transparency in the cache.         */
/*                                                                     */
/* Called by: UIPANEL_Blit (flags=0x05, 0x85)                          */
/* ================================================================== */
uint8_t Town::draw_cached_tile(
    int dest_x, int dest_y, int dest_w, int dest_h,
    uintptr_t dest_base, uint32_t dest_pitch,
    int clip_left, int clip_top, int clip_right, uint32_t clip_bottom)
{
    uint32_t width       = (uint32_t)(clip_right - clip_left) & 0xFFFF;
    uint32_t half_pitch  = (dest_pitch >> 1) & 0xFFFF;
    uint32_t full_width  = (width + (uint32_t)clip_left) & 0xFFFF;

    /* Destination pointer: 16-bit at (dest_x, dest_y) */
    uint16_t* dest = (uint16_t*)(dest_base + (half_pitch * (uint32_t)dest_y + (uint32_t)dest_x) * 2);

    uint8_t*  src_pixels = *(uint8_t**)((intptr_t)this + 0x18);    /* +0x18 */
    uint16_t* palette    = *(uint16_t**)((intptr_t)this + 0x14);   /* +0x14 */

    uint32_t row = 0;

    if (clip_bottom == 0) {
        return 1;
    }

    /* Iterate rows */
    do {
        uint32_t col = 0;

        if (full_width != 0) {
            do {
                /* Read source byte (no transparency check) */
                uint8_t index = *(src_pixels + width * (row & 0xFFFF) + col);

                /* Always expand to 2x2 block */
                uint16_t color = palette[index];            /* +0x14 */

                dest[0] = color;                /* top-left */
                dest[1] = color;                /* top-right */
                dest[half_pitch] = dest[0];     /* bottom-left */
                dest[half_pitch + 1] = dest[0]; /* bottom-right */

                dest += 2;   /* advance 2 dest pixels per source pixel */
                col++;
            } while ((col & 0xFFFF) < full_width);
        }

        row++;
        /* Advance dest to next row */
        dest = dest + (half_pitch - width) * 2;

    } while ((row & 0xFFFF) < clip_bottom);

    return 1;
}
