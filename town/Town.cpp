// Status: INTEGRATED
/**
 * Town.cpp — Town class implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Every method below is annotated with its original address and was
 * validated against the Ghidra disassembly/decompilation of loco.exe
 * (database "locon"). Field offsets used here are documented in Town.h.
 *
 * This file contains the Town main gameplay view implementation:
 *   - Building selection and tracking (select_building/deselect_building/
 *     track_building)
 *   - Viewport occupancy checks (check_occupied, check_occupied_ex,
 *     blit_viewport)
 *   - Viewport scroll rect computation (calc_scroll_rect, ..._reversed)
 *   - Postcard creation, sending, receiving, album management
 *   - Tile viewport rendering helpers (UIPANEL_Blit dispatch targets)
 *   - Train_HandleTrackBuild (remote track-build network message)
 */

#include "Town.h"
#include "../core/GameObject.h"
#include "../game/Building.h"
#include "../game/TrackPiece.h"
#include "../game/World.h"
#include "../game/Train.h"
#include "../game/PlayerConfig.h"
#include "../network/DPlayManager.h"
#include "../network/NetworkPlayerList.h"
#include "../ui/PostcardAlbum.h"
#include "../ui/UIPANEL_Surface.h"
#include "../ui/UI_ChildWindow.h"
#ifndef _WIN32
#include "sdl3_ddraw.h"   /* typed IDirectDrawSurface4 + DDSURFACEDESC bridge */
#include <cstdio>
#endif

/* ResourceManager_GetById is an internal C++ symbol (0x446EA0), not a
 * Win32 ABI import.  Keep C++ linkage so it resolves to the typed SDL bridge
 * on the host rather than an unresolved C name. */
void* ResourceManager_GetById(void* resmgr, int id);

/* ================================================================== */
/* External references (Win32 / cross-module C ABI)                    */
/* ================================================================== */

/* CRT memory — C++ linkage (real bodies malloc()/free()-based; these used
 * to sit inside the extern "C" block below with a `const void*` GLOBAL_free
 * parameter, which is a *different*, always-unresolved C-linkage symbol
 * from the real C++-mangled `GLOBAL_free(void*)` — every call in this file
 * was a null-pointer-call crash risk under the project's
 * --unresolved-symbols=ignore-all link policy). */
void*  operator_new(size_t size);               /* 0x465CE0 */
void   GLOBAL_free(void* ptr);                  /* 0x465CD0 */

extern "C" {
    /* Resource management */
    void*  RESDATA_CreateChildSprite(void* parent, void* res, int x, int y); /* 0x4546D0 */
    void   Sprite_Destroy(void* sprite);                             /* 0x454BC0 */
    void   Sprite_Init(void* sprite);                                /* 0x454BF0 */
    void   Sprite_SetState(void* sprite, int state, int* unk);      /* 0x454C30 */

    /* GameObject */
    void   GameObject_GetRelPos(void* obj, int* out, int x, int y);  /* 0x436A40 */
    /* GameObject_Draw — 0x405E60, __thiscall + 6 stack args (RET 0x18).
     * The codebase's 0-arg Panel.h declaration is a simplification; the
     * render_selection call site passes (0, extra, rect{4}) per the
     * disassembly at 0x42D40E..0x42D431. */
    void   GameObject_Draw(void* self, int a1, int a2, int a3, int a4,
                           int a5, int a6);

    /* World (game/World.h):
     *   void __stdcall World_GetObjectAt(void* obj)   — 0x44E800
     *   void __stdcall World_RenderAll(void* vehicle) — 0x44E683
     *   bool World::SaveToFile(uint, char, char)      — 0x44D8A0 */

    /* Panel */

    /* UI Window management */
    int    UI_CreateFullWindow(void* self, int nCmdShow, HWND hParent,
                               int x, int y, int nWidth, int nHeight,
                               void* hMenu, void* hIcon, UINT classStyle); /* 0x425B70 */
    void   UIPANEL_EndPaintEx(void* self, HWND hWnd, int unk1,
                              byte unk2, RECT* rect);                /* 0x42B2D0 */

    /* UIPANEL blit */
    void   UIPANEL_Blit(void* src_surface, uint32_t src_x, uint32_t src_y,
                        int32_t src_w, uint32_t src_h,
                        void* dest_surface, uint32_t dest_x, uint32_t dest_y,
                        int32_t dest_w, uint32_t dest_h, uint32_t flags); /* 0x42B050 */
    void*  UIPANEL_CreateSurface(void* obj);                         /* 0x42AF30 */

    /* Tile map */
    void   TileMap_InvalidateRect(void* tilemap, int left, int top,
                                  int right, int bottom);            /* 0x416FF0 */

    /* DDraw */
    /* Address corrected: 0x4412F0 disassembles to NameEntryPanel_CreateWindow,
     * not DDRAW_SelectBuilding; confirmed via Ghidra at 0x459180. This is the
     * (void*, void*) overload, a different mangled symbol from the
     * (void*, int) one used by world/tilemap.cpp — see graphics/DDRAW.cpp. */
    void   DDRAW_SelectBuilding(void* ddraw, void* building);       /* 0x459180 */
    void*  DDRAW_GetDdrawErrorString(int code);                     /* 0x45BBC0 */

    /* Audio */
    char   PlaySound(int sound_id);                                  /* 0x44A290 */
    int    HelpWnd_PlayNarration(void* audio_mgr, int page, uint flags); /* 0x44F560 */

    /* RESDATA */
    char   RESDATA_IsBuildingTile(void* tile_data);                  /* 0x44C4E0 */
    char   RESDATA_HitTestChildren(void* self, int x, int y);        /* 0x44B200 */

    /* Network */
    void*  NET_ResolveAddress(const char* hostname);                 /* 0x444C70 */
    void   NET_RegisterPlayer(void* dplay, void* data, int type, int unk); /* 0x4498E0 */
    int    DPLAY_GetMessageCount(void* dplay);                       /* 0x4510E0 */
    void   DPLAY_RenderPlayer(void* dplay, int param1, int param2,
                              void* param3, int param4, int param5,
                              uint32_t param6, RECT* param7);        /* 0x4437C0 */
    void   NET_GetFilePath(uint player_id, int type, char* buf);     /* 0x445510 */
    void   NET_GetAttFilePath(uint player_id, int type, char* buf);  /* 0x445400 */
    char*  _strrchr(const char* s, int c);                           /* 0x467E60 */

    /* Timer */
    UINT_PTR SetTimer(HWND hWnd, UINT_PTR id, UINT timeout, void* proc);
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
    /* strlen/strcpy/memset come from <cstring> via stubs/compat.h. */
}

/* ================================================================== */
/* Global variables referenced by Town functions                       */
/* ================================================================== */

extern void* g_resmgr;                  /* 0x4855E8 — resource manager */
extern void* g_tilemap;                 /* 0x4FD244 — tile map */
extern void* g_ddraw_building;          /* 0x4A9EF0 — DDraw building selection */
extern void* g_primary_surface;         /* 0x4FD3C4 — primary DirectDraw surface */
/* g_dplay's canonical NetworkPlayerList* declaration comes from
 * network/NetworkPlayerList.h (included above); Town.cpp used to shadow it
 * with a weaker `extern void* g_dplay` here, which is why the postcard
 * paths below used to call a fabricated free-function NET_UnregisterPlayer
 * instead of the real NetworkPlayerList::UnregisterPlayer method. */
extern void* g_netman;                  /* 0x4FD3AC — Netman* (see
                                          * network/Netman.h's canonical
                                          * _g_netman for the same object;
                                          * this file's own g_netman keeps
                                          * the void* storage type actually
                                          * defined in shared/stubs_impl.cpp). */
extern void* g_audio_mgr;               /* 0x4FD14C — audio manager */
extern char   g_ddraw_active;           /* 0x4A9F78 — 1 = DirectDraw building mode active */
extern int32_t g_demo_mode;             /* 0x4A9918 — 1 = demo mode */
extern char   g_game_mode;              /* 0x4852AC — current game mode */
extern int    g_cursor_world_x;         /* 0x4FD348 — cursor world X */
extern int    g_cursor_world_y;         /* 0x4FD34C — cursor world Y */
extern void*  g_active_panel;           /* 0x4FD3E0 — active panel override */
extern char   g_surface_lost;           /* 0x4FD218 — primary surface lost flag */
extern void*  g_main_window;            /* 0x4FD230 — main CGWND window */
extern void*  g_world;                  /* 0x4A98B0 — World singleton */

    /* Pixel format state, initialized by DDRAW_SetSurfaceFormat (0x45B9DB):
     * red-channel bit shift (10 for 555, 11 for 565). */
    extern int    g_surface_channel1;       /* 0x485278 — red channel shift */
    extern int    g_surface_red_mask;       /* 0x485288 — red channel mask (water check) */
    extern int    g_surface_blue_mask;      /* 0x485290 — blue channel mask (water check) */

/* Viewport rect globals (one RECT at 0x4AAD14) */
extern int g_viewport_rect_left;        /* 0x4AAD14 */
extern int g_viewport_rect_top;         /* 0x4AAD18 */
extern int g_viewport_rect_right;       /* 0x4AAD1C */
extern int g_viewport_rect_bottom;      /* 0x4AAD20 */

/* Dialog title constant "LEGO LOCO" (0x47E1C0) */
extern const char s_LEGO_LOCO_0047e1c0[];

/* Empty string constant (0x4851D0) */
extern char g_empty_string;

/* Save-dialog custom hook (0x419FD0) */
extern LRESULT SaveAsDlgHook(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/* ================================================================== */
/* Train/network helpers used by Train_HandleTrackBuild               */
/* ================================================================== */

/* Vehicle / route / editor helpers (see game/Train_network.cpp): */
extern void* __thiscall Vehicle_Ctor(void* obj, int resource_id, int type,
                                      uint8_t flag, int unknown);   /* 0x44BE50 */
extern void  __thiscall Vehicle_InitRoute(void* obj, int resource_id,
                                           int type, uint8_t flag);  /* 0x44C220 */
extern void  __thiscall VehicleEditor_SetDPlayData(void* vehicle,
                                                    int data);       /* 0x40D770 */
extern void  __fastcall Train_SendPlayerInfo(void* subsystem);       /* 0x43CCC0 */
extern uint32_t __cdecl CRT_rand(void);                              /* 0x466150 */
extern void  __cdecl DirectPlay_Close(void* peer);                   /* 0x45FC30 */
extern void* g_dplay_peer;               /* 0x48525C */
extern PlayerConfig* g_player_config;     /* canonical singleton */
extern void  NETMAN_QueueMessage(TrainMessage* message);             /* 0x43F140 */

/* NETMAN_CheckTimeout — Netman::CheckTimeout (0x440820) call-site adapter,
 * defined in network/Netman.cpp. Town.cpp can't include Netman.h directly
 * (its own local Win32-API/global declarations below conflict with
 * Netman.h's), so this thin forwarder is the only way to reach the one
 * real implementation from here; it has no logic of its own. Town.cpp's
 * previous declaration here (a fabricated `NETMAN_CheckTimeout(void*,
 * uint32_t)` citing address 0x4415F0, which does not exist as a function)
 * dangled with no body anywhere — a crash risk on every Town::hide(). */
extern void  NETMAN_CheckTimeout(void* netman, int32_t timeoutVal);  /* 0x440820 */

/* Window-proc helpers used by the postcard window procs: */
extern void  CGWND_SetMode(int mode);                                 /* 0x408130 */
extern void  Cursor_Show(void* c);                                    /* 0x4164F0 */
extern void* g_cursor;                /* 0x4FD384 — Cursor object */
extern void* g_postcard;              /* 0x4FD388 — PostcardAlbum object */

/* ================================================================== */
/* Host-only view structs for foreign object layouts                   */
/* ================================================================== */

namespace {

/* Panel-like objects dispatch a resource load at vtable slot 6 (byte
 * offset 0x18); used by handle_tile_click (0x42CEDA / 0x42CEF6). */
char panel_load_resource(void* obj, int res_id, int a, int b)
{
    return (char)((char (*)(void*, int, int, int))(*(void***)obj)[6])(
        obj, res_id, a, b);
}

/* RESDATA child resource — released via vtable[2] (Destroy/Release).
 * Used for overlay/background/button-strip/send-confirm resources. */
void release_sprite_resource(void* resource)
{
    if (resource != nullptr) {
        void** vtable = *reinterpret_cast<void***>(resource);
        reinterpret_cast<void (*)(void*)>(vtable[2])(resource);
    }
}

/* Player/session record (+0x0C player id, +0x3A need-connect flag). */
int32_t record_player_id(const void* record)
{
    return *reinterpret_cast<const int32_t*>(
        reinterpret_cast<const uint8_t*>(record) + 0xC);
}

uint16_t record_need_connect(const void* record)
{
    return *reinterpret_cast<const uint16_t*>(
        reinterpret_cast<const uint8_t*>(record) + 0x3A);
}

void record_set_need_connect(void* record, uint16_t value)
{
    *reinterpret_cast<uint16_t*>(
        reinterpret_cast<uint8_t*>(record) + 0x3A) = value;
}

/* UI_CenterWindow @ 0x425A50 — center `inner` rect within `outer`. */
void center_window_rect(const RECT* outer, RECT* inner)
{
    int old_left = inner->left;
    int left = ((outer->right - outer->left) / 2 -
                (inner->right - inner->left) / 2) + outer->left;
    inner->left = left;
    inner->right = (inner->right - old_left) + left;

    int old_top = inner->top;
    int top = ((outer->bottom - outer->top) / 2 -
               (inner->bottom - inner->top) / 2) + outer->top;
    inner->top = top;
    inner->bottom = (inner->bottom - old_top) + top;
}

} // namespace

namespace {

/* Persistent primary-surface lock state — the original keeps a global
 * DDSURFACEDESC at 0x4FD19C that persists across calls; reproduced as
 * file-static storage so a failed re-lock still scans the previous
 * lock's data exactly like the original global. */
DDSURFACEDESC g_primary_surface_desc = {};

} // namespace

/* ================================================================== */
/* Town::Town — Constructor                                            */
/* Address: 0x42E900                                                   */
/*                                                                     */
/* Calls UI_WindowBase(hInstance, 0x1F5), sets the vtable to 0x477D88     */
/* (0x477D88), then base_ctor to init fields + create 8 ButtonSprites. */
/* Called by: CGWND_InitAllSubsystems @ 0x407054.                      */
/* ================================================================== */
Town::Town(HINSTANCE hInstance, UINT resId)
    : UI_WindowBase(hInstance, resId)
{
    /* In the binary: sets vtable here. Compiler-managed in natural C++. */
    this->base_ctor();                         /* Town_BaseCtor @ 0x42E980 */
}

/* ================================================================== */
/* Town::base_ctor — ID: 0x42E980                                      */
/*                                                                     */
/* Initialize all Town-specific fields, create 8 ButtonSprite objects. */
/* Called from Town::Town after the base class init.                   */
/* ================================================================== */
void Town::base_ctor()
{
    /* Zero all Town-specific state fields (offsets from disassembly). */
    this->icon_handle = nullptr;         /* +0x5F4 */
    this->flag_E8 = 0;                   /* +0xE8  */
    this->sprites_initialized = 0;       /* +0x5F9 */
    this->overlay_initialized = 0;       /* +0x5FA */
    this->postcard_active = 0;           /* +0x5F8 */
    this->selected_player = nullptr;     /* +0x608 */
    this->postcard_data = nullptr;       /* +0x60C */
    this->player_count_flag = 1;         /* +0x610 */
    this->overlay_resource = nullptr;    /* +0x644 */
    this->overlay_surface = nullptr;     /* +0x648 */
    this->timer_active = 0;              /* +0x5FC */
    this->frame_counter = 0;             /* +0x600 */
    this->repaint_requested = 0;         /* +0x605 */

    /* Create 8 postcard button sprites (0x24 bytes each). The binary
     * uses ButtonSprite_Ctor @ 0x454B50 on operator_new(0x24) blocks
     * (verified at 0x42EA09 / 0x42EA40 / ...). */
    this->sprite_btn_close      = new ButtonSprite(0x3CF0);   /* +0x6A4 */
    this->sprite_btn_options    = new ButtonSprite(0x3CF1);   /* +0x6A8 */
    this->sprite_btn_rotate     = new ButtonSprite(0x3CF2);   /* +0x6AC */
    this->sprite_btn_save       = new ButtonSprite(0x3CF3);   /* +0x6B0 */
    this->sprite_inbox          = new ButtonSprite(0x3CAC);   /* +0x6B4 */
    /* NOTE: the counter sprites are stored out of numeric order in the
     * original: res 0x3CF5 -> +0x6BC, res 0x3CF6 -> +0x6B8. */
    this->sprite_inbox_counter  = new ButtonSprite(0x3CF5);   /* +0x6BC */
    this->sprite_outbox_counter = new ButtonSprite(0x3CF6);   /* +0x6B8 */
    this->sprite_send           = new ButtonSprite(0x3CF9);   /* +0x6C0 */

    /* Initialize sprite state look-up tables (verified words at
     * +0x6C4..+0x6D6). */
    this->inbox_state_lut[0] = 0;        /* +0x6C4 */
    this->inbox_state_lut[1] = 1;        /* +0x6C6 */
    this->inbox_state_lut[2] = 3;        /* +0x6C8 */
    this->inbox_state_lut[3] = 5;        /* +0x6CA */
    this->inbox_state_lut[4] = 7;        /* +0x6CC */

    this->outbox_state_lut[0] = 0;       /* +0x6CE */
    this->outbox_state_lut[1] = 1;       /* +0x6D0 */
    this->outbox_state_lut[2] = 2;       /* +0x6D2 */
    this->outbox_state_lut[3] = 3;       /* +0x6D4 */
    this->outbox_state_lut[4] = 4;       /* +0x6D6 */

    /* Clear network update flag / frame counter / audio flag. */
    this->net_update_flag = 0;           /* +0x604 */
    this->timer_counter = 0;             /* +0x5F0 */
    this->audio_playing = 0;             /* +0x5ED */
}

/* ================================================================== */
/* Town::~Town — scalar deleting destructor (vtable[0])                */
/* Address: 0x42E960                                                   */
/*                                                                     */
/* Calls destroy() then GLOBAL_free if flags & 1. The compiler emits   */
/* the deleting wrapper in natural C++.                                */
/* ================================================================== */
Town::~Town()
{
    this->destroy();
}

/* ================================================================== */
/* Town::destroy — Full destructor body                                */
/* Address: 0x42EC10                                                   */
/*                                                                     */
/* Releases all Town-owned resources.                                  */
/* ================================================================== */
void Town::destroy()
{
    /* In the binary: resets vtable. Compiler-managed in natural C++. */

    /* Destroy the overlay resource if initialized (+0x5FA). */
    if (this->overlay_initialized) {
        release_sprite_resource(this->overlay_resource);   /* +0x644 */
        this->overlay_initialized = 0;
    }

    /* Destroy the 8 postcard sprites + 3 child resources (+0x5F9). */
    if (this->sprites_initialized) {
        Sprite_Destroy(this->sprite_btn_close);
        Sprite_Destroy(this->sprite_btn_options);
        Sprite_Destroy(this->sprite_btn_rotate);
        Sprite_Destroy(this->sprite_btn_save);
        Sprite_Destroy(this->sprite_inbox);
        Sprite_Destroy(this->sprite_outbox_counter);
        Sprite_Destroy(this->sprite_inbox_counter);
        Sprite_Destroy(this->sprite_send);

        release_sprite_resource(this->button_strip_resource);   /* +0x664 */
        release_sprite_resource(this->background_resource);     /* +0x64C */
        release_sprite_resource(this->send_confirm_resource);   /* +0x69C */

        this->sprites_initialized = 0;
    }

    /* Free individual sprite objects (vtable[0] deleting dtor). */
    delete this->sprite_btn_close;
    this->sprite_btn_close = nullptr;
    delete this->sprite_btn_options;
    this->sprite_btn_options = nullptr;
    delete this->sprite_btn_rotate;
    this->sprite_btn_rotate = nullptr;
    delete this->sprite_btn_save;
    this->sprite_btn_save = nullptr;
    delete this->sprite_inbox;
    this->sprite_inbox = nullptr;
    delete this->sprite_outbox_counter;
    this->sprite_outbox_counter = nullptr;
    delete this->sprite_inbox_counter;
    this->sprite_inbox_counter = nullptr;
    delete this->sprite_send;
    this->sprite_send = nullptr;

    /* Call base class destructor body. */
    this->base_destructor();
}

/* ================================================================== */
/* Town::show — Show Town window + postcard UI (vtable[2])             */
/* Address: 0x42F5E0                                                   */
/* ================================================================== */
void Town::show()
{
    int i = ShowCursor(0);
    while (i >= 0) {
        i = ShowCursor(0);
    }

    UI_WindowBase::show();                          /* 0x4259C0 */

    this->flag_E8 = 0;                              /* +0xE8 */
    this->init_postcard_sprites();                  /* 0x42FE30 */

    /* vtable[7] -> on_create: postcard geometry layout. */
    this->on_create();                              /* 0x42F8B0 */

    SetFocus(this->hWnd);

    /* vtable[3] -> set_mode(base child pair). */
    this->set_mode(this->childCount0, this->childObj0, 0, 1);

    this->is_host = 1;                              /* +0x606 */

    /* Count connected hosts to refresh has_remote_players. */
    short count = 0;
    PostBagFileNode* host = NET_GetHostName(1, 0);
    while (host != nullptr) {
        PostBagFileNode* next = host->next;
        count++;
        GLOBAL_free(host);
        host = next;
    }
    this->has_remote_players = (count == 0) ? 0 : 1;   /* +0x607 */

    /* Arm the 0x4D / 200 ms postcard refresh timer. */
    if (this->timer_active == 0) {                  /* +0x5FC */
        this->timer_active = (int)SetTimer(this->hWnd, 0x4D, 200, nullptr);
    }

    this->list_postcards();                         /* 0x42DD50 */
    this->audio_playing = 0;                        /* +0x5ED */
}

/* ================================================================== */
/* Town::hide — Hide Town + de-init postcard UI (vtable[1])            */
/* Address: 0x42F6C0                                                   */
/*                                                                     */
/* MISNAMED in the original source as "DeinitPostcardUI"; it is the    */
/* vtable[1] Hide override.                                            */
/* ================================================================== */
void Town::hide()
{
    if (this->visible) {                            /* +0xE4 */
        UI_WindowBase::hide();                      /* 0x425990 */
        {
            int i = ShowCursor(0);
            while (i >= 0) {
                i = ShowCursor(0);
            }
        }

        TileMap_InvalidateRect(&g_tilemap,
                               g_viewport_rect_left, g_viewport_rect_top,
                               g_viewport_rect_right, g_viewport_rect_bottom);

        if (this->selected_player) {                /* +0x608 */
            /* Release the player record (vtable[0] deleting dtor). */
            delete (DPlayManager*)this->selected_player;
            this->selected_player = nullptr;
            this->postcard_data = nullptr;          /* +0x60C */
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

            release_sprite_resource(this->button_strip_resource);
            release_sprite_resource(this->background_resource);
            release_sprite_resource(this->send_confirm_resource);

            this->sprites_initialized = 0;
        }

        if (this->timer_active) {
            KillTimer(this->hWnd, 0x4D);
        }
        this->timer_active = 0;
        this->postcard_active = 0;
    }

    /* update_network label */
    short active_players = NET_UpdatePlayerList();
    NETMAN_CheckTimeout(g_netman, active_players != 0 ? 1 : 0);

    this->net_update_flag = 0;
}

/* ================================================================== */
/* Town::on_create — Postcard geometry layout (vtable[7])              */
/* Address: 0x42F8B0                                                   */
/* ================================================================== */
void Town::on_create()
{
    if (this->sprites_initialized == 0) {           /* +0x5F9 */
        return;
    }

    UI_WindowBase::on_create();                     /* 0x425D30 */

    /* Center the postcard overlay rect (+0x614..+0x620) within the
     * working rect; width/height come from the overlay resource. */
    RECT overlay;
    overlay.left   = 0;
    overlay.top    = 0;
    overlay.right  = (uint16_t)(*(uint16_t*)((intptr_t)this->overlay_resource + 0x14));
    overlay.bottom = (uint16_t)(*(uint16_t*)((intptr_t)this->overlay_resource + 0x16));
    center_window_rect(&this->workRect, &overlay);
    this->postcard_rect_left   = overlay.left;      /* +0x614 */
    this->postcard_rect_top    = overlay.top;       /* +0x618 */
    this->postcard_rect_right  = overlay.right;     /* +0x61C */
    this->postcard_rect_bottom = overlay.bottom;    /* +0x620 */

    /* Player render rect {0, 0, 800, 600}, centered within the working
     * rect; it is the origin for the sprite geometry below. */
    RECT player_rect;
    player_rect.left   = 0;                         /* +0x634 */
    player_rect.top    = 0;
    player_rect.right  = 800;                       /* +0x63C */
    player_rect.bottom = 600;                       /* +0x640 */
    center_window_rect(&this->workRect, &player_rect);
    this->player_rect = player_rect;

    /* Position each of the 8 button sprites relative to the player rect. */
    this->layout_postcard_sprite(this->sprite_btn_close,        0x2C, 0x22);
    this->layout_postcard_sprite(this->sprite_btn_options,      0x1A7, 0x196);
    this->layout_postcard_sprite(this->sprite_btn_rotate,       0x22, 0x106);
    this->layout_postcard_sprite(this->sprite_btn_save,         0x29E, 0x1C3);
    this->layout_postcard_sprite(this->sprite_inbox,            0x2CB, 0x12E);
    this->layout_postcard_sprite(this->sprite_inbox_counter,    0xA9, 0x15E);
    this->layout_postcard_sprite(this->sprite_outbox_counter,   0x21C, 0xB);
    this->layout_postcard_sprite(this->sprite_send,            0x16D, 0x1A0);

    /* Send button hit rect, offset by the player rect origin. */
    SetRect(&this->button_hit_rect_send, 0x14B, 0x1A3, 0x1AB, 0x1FB);
    OffsetRect(&this->button_hit_rect_send, this->player_rect.left,
               this->player_rect.top);

    /* Send animation source rect (right/bottom = background size). */
    CopyRect((RECT*)&this->send_rect_left, &this->player_rect);
    this->send_rect_right =
        (uint16_t)(*(uint16_t*)((intptr_t)this->background_resource + 0x14)) +
        this->send_rect_left;
    this->send_rect_bottom =
        this->send_rect_top +
        (uint16_t)(*(uint16_t*)((intptr_t)this->background_resource + 0x16));
    OffsetRect((RECT*)&this->send_rect_left, 0x14C, 0xB0);

    /* Button strip source rect (right/bottom = strip size). */
    CopyRect((RECT*)&this->button_src_left, &this->player_rect);
    this->button_src_right =
        (uint16_t)(*(uint16_t*)((intptr_t)this->button_strip_resource + 0x14)) +
        this->button_src_left;
    this->button_src_bottom =
        this->button_src_top +
        (uint16_t)(*(uint16_t*)((intptr_t)this->button_strip_resource + 0x16));
    OffsetRect((RECT*)&this->button_src_left, 0x100, 0xD2);

    /* Player render area rect: player_rect offset by (0x199, 0xB2).
     * render_extra/render_rect_ptr are computed from the player rect
     * BEFORE the offset (binary order at 0x42FD8E/0x42FD98). */
    this->render_extra = this->player_rect.left + 300;
    this->render_rect_ptr =
        (void*)(uintptr_t)(this->player_rect.top + 200);
    CopyRect((RECT*)&this->render_param_x, &this->player_rect);
    OffsetRect((RECT*)&this->render_param_x, 0x199, 0xB2);

    /* Postcard preview area, offset by the render-area origin. */
    SetRect(&this->preview_rect, 0x14, -0xB, 0x2D, 0x3B);
    OffsetRect(&this->preview_rect, this->render_param_x,
               this->render_param_y);
}

/* ================================================================== */
/* Town::layout_postcard_sprite — position one button sprite           */
/* (inline helper used by on_create; no original address — it is the   */
/* repeated sprite-positioning block at 0x42F9E0..0x42FD90).           */
/* ================================================================== */
void Town::layout_postcard_sprite(ButtonSprite* sprite, int dx, int dy)
{
    RECT r;
    r.left   = this->player_rect.left;
    r.top    = this->player_rect.top;
    r.right  = r.left +
        (uint16_t)(*(uint16_t*)((intptr_t)sprite->pixelData + 0x14));
    r.bottom = r.top +
        (uint16_t)(*(uint16_t*)((intptr_t)sprite->pixelData + 0x16));
    OffsetRect(&r, dx, dy);

    sprite->x = r.left;
    sprite->y = r.top;
    sprite->sourceX = r.right;
    sprite->sourceY = r.bottom;
}

/* ================================================================== */
/* Town::init_sprites — Create the Town child window                   */
/* Address: 0x42EDB0                                                   */
/* ================================================================== */
bool Town::init_sprites(HWND hParent)
{
    RECT desktop_rect;
    HWND hDesktop = GetDesktopWindow();
    GetClientRect(hDesktop, &desktop_rect);

    /* Load window icon (resource 0x65). */
    this->icon_handle = (HICON)LoadIconA(this->hInstance, (const char*)0x65);

    int width  = desktop_rect.right - desktop_rect.left;
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
/* Creates 3 TrackPiece child sprites (valid=0x3807 -> +0x170/+0xD8,   */
/* invalid=0x3808 -> +0x174/+0xDC, hover=0x3806 -> +0x178), loads      */
/* animation resources 0x3805 (self) + 0x3804 (child_panel) via        */
/* vtable[6], creates the overlay UIPANEL surface at +0x17C and        */
/* initializes the backup rect at +0x180. Returns 1 on success.        */
/* ================================================================== */
char Town::handle_tile_click()
{
    void* res = ResourceManager_GetById(&g_resmgr, 0x3807);
    if (res && UI_IsBitmapReady((int)(intptr_t)res)) {
        TrackPiece* sprite = (TrackPiece*)RESDATA_CreateChildSprite(this, res, 0, 0);
        this->cursor_valid_sprite = sprite;          /* +0x170 */
        this->cursor_valid_dup = sprite;             /* +0xD8 */
    }

    res = ResourceManager_GetById(&g_resmgr, 0x3808);
    if (res && UI_IsBitmapReady((int)(intptr_t)res)) {
        TrackPiece* sprite = (TrackPiece*)RESDATA_CreateChildSprite(this, res, 0, 0);
        this->cursor_invalid_sprite = sprite;        /* +0x174 */
        this->cursor_invalid_dup = sprite;           /* +0xDC */
    }

    res = ResourceManager_GetById(&g_resmgr, 0x3806);
    if (res && UI_IsBitmapReady((int)(intptr_t)res)) {
        this->track_piece =                          /* +0x178 */
            (TrackPiece*)RESDATA_CreateChildSprite(this, res, 0, 0);
    }

    /* Load animation resources 0x3805 (self) / 0x3804 (child_panel) via
     * vtable slot 6 (byte offset 0x18). Faithful to the binary — the
     * slot is the inherited create_full_window entry, called here with
     * a resource-id argument set (original source quirk). */
    char loaded = panel_load_resource(this, 0x3805, -1, 0);
    if (loaded) {
        loaded = panel_load_resource(this->child_panel, 0x3804, -1, 0);
        if (loaded) {
            void* surface_obj = operator_new(0x20);
            this->overlay_panel = surface_obj ? UIPANEL_CreateSurface(surface_obj) : nullptr;

            if (this->overlay_panel) {
                void* pgfx = *(void**)((intptr_t)this->panel_graphics + 0x10);
                UIPANEL_InitSurface(this->overlay_panel,
                                    *(int*)((intptr_t)pgfx + 8),
                                    *(int*)((intptr_t)pgfx + 0xC),
                                    1, 0, 0);

                SetRect((RECT*)&this->backup_surface, 0, 0,
                        *(int*)((intptr_t)this->overlay_panel + 8),
                        *(int*)((intptr_t)this->overlay_panel + 0xC));
            }
            return 1;
        }
    }
    return 0;
}

/* ================================================================== */
/* Town::is_valid_placement — Static buildable-tile check (__cdecl)    */
/* Address: 0x42CF90                                                   */
/* ================================================================== */
bool Town::is_valid_placement(Building* entity)
{
    uint8_t tile_type = 0;
    if (entity != nullptr && entity->initialized == 1) {      /* +0x18 */
        if (entity->resource != nullptr) {                    /* +0x40 */
            tile_type = *(uint8_t*)((intptr_t)entity->resource + 8);
        }
    }

    if (tile_type == 0) {
        return false;
    }
    if (tile_type == 0x07) {        /* always buildable */
        return true;
    }
    if (tile_type == 0x08 || tile_type == 0x02 || tile_type == 0x06) {
        return entity->visible == 1;                    /* +0x24 */
    }
    if (tile_type == 0x04 &&
        *(uint8_t*)((intptr_t)entity->resource + 0x62C) == 1) {
        return true;                /* connected tile */
    }
    if (tile_type == 0x03 &&
        RESDATA_IsBuildingTile(entity->resource)) {
        return true;                /* building tile */
    }
    if (tile_type == 0x0C) {        /* large-ID tile (ID > 0x300F) */
        int32_t id = -1;
        if (entity->resource != nullptr) {
            id = ((RESDATA*)entity->resource)->resource_id;     /* +0x04 */
        }
        if (id > 0x300F) {
            return true;
        }
    }
    return false;
}

/* ================================================================== */
/* Town::select_building — Select/focus a building (NULL = deselect)   */
/* Address: 0x42D040                                                   */
/* ================================================================== */
byte Town::select_building(Building* building)
{
    if (building != nullptr && g_game_mode == 3) {
        bool valid = Town::is_valid_placement(building);
        if (valid && g_demo_mode != 1) {
            this->selection_active = 1;                         /* +0x88 */

            uint16_t tile_type = 0;
            if (building->resource != nullptr) {
                tile_type = *(uint8_t*)((intptr_t)building->resource + 8);
            }
            this->selected_building_type = tile_type;           /* +0x16C */

            if (g_ddraw_active == 0) {
                g_active_panel = this;
            }

            this->selected_building = building;                 /* +0xE0 */

            int center_x = (building->screen_rect.right -
                            building->screen_rect.left) / 2 +
                           building->screen_rect.left;
            int center_y = (building->screen_rect.bottom -
                            building->screen_rect.top) / 2 +
                           building->screen_rect.top;
            /* vtable[3] dispatch; the binary pushes only the two
             * center coords (0x42D0D3..0x42D0DB) leaving the trailing
             * set_mode args undefined — reproduced with zeros. */
            this->set_mode(center_x, (void*)(intptr_t)center_y, 0, 0);

            short zoom = (this->selected_building_type == 6) ? 1 : 3;
            this->track_piece->SetZoom(zoom);                   /* 0x40D170 */

            this->track_piece->Render();                        /* vtable[8] */

            /* Binary reads this+0x08..+0x14 as the invalidate rect
             * (0x42D10C..0x42D122); kept as raw base-slot access. */
            TileMap_InvalidateRect(&g_tilemap,
                *(int*)((intptr_t)this + 8),
                *(int*)((intptr_t)this + 0xC),
                *(int*)((intptr_t)this + 0x10),
                *(int*)((intptr_t)this + 0x14));

            DDRAW_SelectBuilding(&g_ddraw_building, this->selected_building);

            return this->selection_active;
        }
    }

    /* Deselect path */
    this->selection_active = 0;                                 /* +0x88 */
    this->selected_building_type = 0;                           /* +0x16C */

    /* In the binary: g_active_panel = (g_ddraw_active == 1)
     * ? &g_ddraw_building : 0  (0x42D156..0x42D16C). */
    g_active_panel = (g_ddraw_active == 1) ? (void*)&g_ddraw_building : nullptr;

    this->hide();                                               /* vtable[1] */

    if (this->child_panel) {
        /* child_panel vtable[1] (Hide) */
        ((void (*)(void*))(*(void***)this->child_panel)[1])(this->child_panel);
    }

    return this->selection_active;
}

/* ================================================================== */
/* Town::deselect_building — Remove the building selection overlay     */
/* Address: 0x42D280                                                   */
/* ================================================================== */
void Town::deselect_building()
{
    RECT clip_rect;
    int right_inset, bottom_inset;

    if (this->selected_building_type == 7) {
        right_inset  = this->overlay_dest_right - 0x90;
        bottom_inset = this->overlay_dest_bottom - 0x8C;
    } else {
        right_inset  = this->overlay_dest_right >> 2;
        bottom_inset = this->overlay_dest_bottom >> 2;
    }

    clip_rect.left   = this->viewport_inset_left + right_inset;
    clip_rect.right  = this->viewport_inset_right - right_inset;
    clip_rect.top    = this->viewport_inset_top + bottom_inset;
    clip_rect.bottom = this->viewport_inset_bottom - bottom_inset;

    RECT viewport_rect;
    viewport_rect.left   = g_viewport_rect_left;
    viewport_rect.top    = g_viewport_rect_top;
    viewport_rect.right  = g_viewport_rect_right;
    viewport_rect.bottom = g_viewport_rect_bottom;
    IntersectRect(&clip_rect, &clip_rect, &viewport_rect);

    /* overlay_panel + 0x1C = backing surface (no extra dereference). */
    void* backing_surface = *(void**)((intptr_t)this->overlay_panel + 0x1C);

    /* backing_surface vtable slot 5: (surface, &backup, primary, rect,
     * 0x1000000, 0) — restore the cached background. */
    ((void (*)(void*, uint32_t, void*, RECT*, uint32_t, int))(
        *(void***)backing_surface)[5])(
        backing_surface,
        this->backup_surface,
        g_primary_surface,
        &clip_rect,
        0x1000000,
        0);

    /* Panel anim-table flag at +0x20 + index*0x18 + 0x16; the index is
     * the base timerId slot (+0x28), repurposed as panel index. */
    uint32_t panel_flags = 0;
    int panel_index = static_cast<int>(this->timerId);           /* +0x28 */
    if (*(uint8_t*)(*(intptr_t*)((intptr_t)this->panel_graphics + 0x20) +
                    panel_index * 0x18 + 0x16) == 1) {
        panel_flags = 0x20;
    }

    void* panel_surface = *(void**)((intptr_t)this->panel_graphics + 0x10);
    UIPANEL_Blit(
        panel_surface,
        this->backup_surface,
        this->backup_x,
        this->backup_y,
        this->backup_width,
        backing_surface,
        this->backup_surface,
        this->backup_x,
        this->backup_y,
        this->backup_width,
        panel_flags);
}

/* ================================================================== */
/* Town::track_building — Per-frame tracking of the selected building  */
/* Address: 0x42D1A0                                                   */
/* ================================================================== */
void Town::track_building()
{
    if (!this->selection_active) {                               /* +0x88 */
        return;
    }

    if (this->selected_building_type == 6 &&                     /* +0x16C */
        this->selected_building->visible == 0) {                 /* +0x24 */
        this->select_building(nullptr);
    }

    Building* building = this->selected_building;                /* +0xE0 */
    int cx = (building->screen_rect.right - building->screen_rect.left) / 2 +
             building->screen_rect.left;
    int cy = (building->screen_rect.bottom - building->screen_rect.top) / 2 +
             building->screen_rect.top;

    if (this->building_center_x != cx ||                         /* +0x190 */
        this->building_center_y != cy) {                         /* +0x194 */
        /* vtable[3] dispatch; binary pushes only (cx, cy). */
        this->set_mode(cx, (void*)(intptr_t)cy, 0, 0);
        this->building_center_x = cx;
        this->building_center_y = cy;
    }

    int local_8[2];
    GameObject_GetRelPos(this, local_8, g_cursor_world_x, g_cursor_world_y);

    /* Update each child sprite (vtable[20] dispatch per child). */
    for (TrackPiece* child = this->children_list;                /* +0xD0 */
         child != nullptr;
         child = (TrackPiece*)*(void**)((intptr_t)child + 0x28)) {
        this->on_mouse_move(this->hWnd, 0x200, 0, (LPARAM)(intptr_t)child);
    }

    if (this->child_panel) {                                     /* +0xE4 */
        ((void (*)(void*))(*(void***)this->child_panel)[1])(this->child_panel);
    }
}

/* ================================================================== */
/* Town::update_selection — Blit the selection overlay to primary      */
/* Address: 0x42D3A0                                                   */
/* ================================================================== */
void Town::update_selection()
{
    UIPANEL_Blit(
        this->overlay_panel,              /* +0x17C */
        this->viewport_inset_left,        /* +0xEC */
        this->viewport_inset_top,         /* +0xF0 */
        this->viewport_inset_right,       /* +0xF4 */
        this->viewport_inset_bottom,      /* +0xF8 */
        g_primary_surface,
        this->overlay_dest_left,          /* +0x114 */
        this->overlay_dest_top,           /* +0x118 */
        this->overlay_dest_right,         /* +0x11C */
        this->overlay_dest_bottom,        /* +0x120 */
        0x40);
}

/* ================================================================== */
/* Town::render_selection — Draw the selection highlight for one tile  */
/* Address: 0x42D400 (__thiscall, 5 stack args, RET 0x14)              */
/*                                                                     */
/* The binary calls GameObject_Draw (0x405E60) directly with           */
/* (0, arg5, rect{arg1..arg4}).                                        */
/* ================================================================== */
void Town::render_selection(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                            int32_t extra)
{
    if (this->selection_active) {                                /* +0x88 */
        RECT rect;
        rect.left   = x1;
        rect.top    = y1;
        rect.right  = x2;
        rect.bottom = y2;
        /* Binary: PUSH 0; PUSH arg5; RECT on stack; CALL 0x405E60. */
        GameObject_Draw(this, 0, extra,
                        rect.left, rect.top, rect.right, rect.bottom);
    }
}

/* ================================================================== */
/* Town::check_occupied — Scan tile buffer for non-empty tiles         */
/* Address: 0x42C950 (__thiscall)                                      */
/* ================================================================== */
uint8_t Town::check_occupied(int x1, int y1, int x2, int y2)
{
    if (*(int*)((intptr_t)this + 4) != 0) {          /* mode flag */
        return check_occupied_ex(x1, y1, x2, y2);
    }

    int stride = *(int*)((intptr_t)this + 8);
    uint8_t* buf = (uint8_t*)(uintptr_t)(*(int*)((intptr_t)this + 0x18));
    int width  = x2 - x1;
    int height = y2 - y1;

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
/* Town::check_occupied_ex — Extended tile occupancy via surface lock  */
/* Address: 0x42C9F0 (__stdcall, 4 stack args, RET 0x10)               */
/*                                                                     */
/* Locks the global primary surface (COM slot 25 = vtable byte offset  */
/* 0x64), scans 16-bit pixels; occupied when (red>>shift) != 0x1f AND  */
/* blue != 0x1f. Unlock via COM slot 32 (byte offset 0x80).            */
/* ================================================================== */
uint8_t Town::check_occupied_ex(int x1, int y1, int x2, int y2)
{
    uint8_t result = 0;
    IDirectDrawSurface4* primary = (IDirectDrawSurface4*)g_primary_surface;

    if (g_surface_lost == 0) {
        /* NOTE: the binary sets the flag when Lock SUCCEEDS (returns 0)
         * — "lost" is actually the locked state here (0x42CA33..0x42CA37). */
        DDSURFACEDESC& desc = g_primary_surface_desc;
        desc = DDSURFACEDESC();
        desc.dwSize = 0x7C;

        /* Lock(this, NULL, &desc, 0, 0) — COM slot 25 (byte 0x64). */
        if (primary->Lock(nullptr, &desc, 0, 0) == 0) {
            g_surface_lost = 1;
        }
    }

    uint32_t pitch = (uint32_t)g_primary_surface_desc.lPitch;
    uint32_t height = (y2 - y1) & 0xFFFF;
    uint32_t width  = (x2 - x1) & 0xFFFF;

    uint16_t* pixels = (uint16_t*)((uintptr_t)g_primary_surface_desc.lpSurface +
                                   ((pitch >> 1) * (uint32_t)y1 +
                                    (uint32_t)x1) * 2);

    for (uint32_t row = 0; row < height; row++) {
        if (width != 0) {
            for (uint32_t col = 0; col < width; col++) {
                uint16_t pixel = pixels[col];
                int channel1 = (g_surface_red_mask & pixel) >>
                               ((uint8_t)g_surface_channel1 & 0x1f);
                int channel2 = g_surface_blue_mask & pixel;

                if (channel1 != 0x1f && channel2 != 0x1f) {
                    result = 1;
                    break;
                }
            }
        }

        pixels += (pitch >> 1) - width;

        if (result != 0) {
            break;
        }
    }

    if (g_surface_lost != 0) {
        /* Unlock(NULL) — COM slot 32 (byte 0x80). */
        if (primary->Unlock(nullptr) == 0) {
            g_surface_lost = 0;
        }
    }

    return result;
}

/* ================================================================== */
/* Town::blit_viewport — Viewport occupancy check for collision        */
/* Address: 0x42CB10 (__thiscall, 6 stack args, RET 0x18)              */
/*                                                                     */
/* NOTE: faithful to the binary, the bound tests use the parameters    */
/* as (x < x1 || y2 < x || y < x2 || x < y) — y1 is never read.        */
/* ================================================================== */
uint32_t Town::blit_viewport(int x1, int y1, int x2, int y2, int x, int y)
{
    /* Outside bounds -> passable. Parameter usage matches the binary
     * (0x42CB31..0x42CB61): y1 is never read. */
    if (x < x1 || y2 < x || y < x2 || x < y) {
        return 1;
    }

    if (*(int*)((intptr_t)this + 4) != 1) {
        uint8_t* buf = *(uint8_t**)((intptr_t)this + 0x18);
        int stride = *(int*)((intptr_t)this + 8);
        uint8_t val = buf[stride * y + x];
        return (val == 0) ? 1 : 0;
    }

    /* Mode 1: lock the surface at this+0x1C and check the pixel. */
    IDirectDrawSurface4* surface = *(IDirectDrawSurface4**)((intptr_t)this + 0x1C);
    DDSURFACEDESC desc = {};
    desc.dwSize = 0x7C;

    /* Lock(this, NULL, &desc, 1, 0) — COM slot 25 (byte 0x64). */
    int lock_result = surface->Lock(nullptr, &desc, 1, 0);
    if (lock_result != 0) {
        DDRAW_GetDdrawErrorString(1);
        return 0;
    }

    uint32_t pitch = (uint32_t)desc.lPitch;
    uint8_t* base = (uint8_t*)desc.lpSurface;    /* ddsd + 0x24 */
    uint16_t pixel = *(uint16_t*)(base + pitch * (uint32_t)y + (uint32_t)x * 2);

    uint32_t channel1 = (g_surface_red_mask & (uint32_t)pixel) >>
                        ((uint8_t)g_surface_channel1 & 0x1f);
    uint32_t channel2 = g_surface_blue_mask & (uint32_t)pixel;

    uint8_t result = 1;
    if (channel1 != 0x1f || channel2 != 0x1f) {
        result = 0;
    }

    /* Unlock(NULL) — COM slot 32 (byte 0x80). */
    surface->Unlock(nullptr);

    return result;
}

/* ================================================================== */
/* Town::calc_scroll_rect — Visible tile rect from scroll position     */
/* Address: 0x42C590 (__thiscall)                                      */
/*                                                                     */
/* DDSURFACEDESC setup, surface + viewport rects, negative clip        */
/* compensation, intersection with the surface bounds. Returns 0 (the  */
/* value is unused by callers).                                        */
/* ================================================================== */
uint32_t Town::calc_scroll_rect(RECT* pClipRect, void* surface)
{
    /* Get surface dimensions via IDirectDrawSurface4::GetSurfaceDesc
     * (COM slot 22 = vtable byte offset 0x58). */
    IDirectDrawSurface4* surf = static_cast<IDirectDrawSurface4*>(surface);
    DDSURFACEDESC desc = {};
    desc.dwSize = 0x7C;
    surf->GetSurfaceDesc(&desc);

    RECT surface_rect;
    SetRect(&surface_rect, 0, 0,
            (int32_t)desc.dwWidth, (int32_t)desc.dwHeight);

    RECT viewport_rect;
    SetRect(&viewport_rect, 0, 0, *(int*)((intptr_t)this + 8),
            *(int*)((intptr_t)this + 0xC));

    /* Adjust the scroll origin by the (negative) clip offsets. */
    RECT* clip = pClipRect;
    int scroll_x = clip->left - ((clip->left < 0) ? clip->left : 0);
    int scroll_y = clip->top - ((clip->top < 0) ? clip->top : 0);

    RECT candidate;
    IntersectRect(&candidate, clip, clip);

    if (IsRectEmpty(&candidate) == 0) {
        RECT src_rect;
        src_rect.left   = scroll_x;
        src_rect.top    = scroll_y;
        src_rect.right  = scroll_x + (candidate.right - candidate.left);
        src_rect.bottom = scroll_y + (candidate.bottom - candidate.top);

        RECT out_rect;
        IntersectRect(&out_rect, &src_rect, &surface_rect);

        if (IsRectEmpty(&out_rect) == 0) {
            if (scroll_x > 0) scroll_x = 0;
            if (scroll_y > 0) scroll_y = 0;

            SetRect(clip, out_rect.left, out_rect.top,
                    (out_rect.right - out_rect.left) + out_rect.left,
                    (out_rect.bottom - out_rect.top) + out_rect.top);

            SetRect(pClipRect,
                    clip->left - scroll_x, clip->top - scroll_y,
                    (clip->left - scroll_x) +
                        (out_rect.right - out_rect.left),
                    (clip->top - scroll_y) +
                        (out_rect.bottom - out_rect.top));
        }
    }

    /* The original always returns FALSE (0); kept faithful. */
    return 0;
}

/* ================================================================== */
/* Town::calc_scroll_rect_reversed — Reversed scroll rect calculation  */
/* Address: 0x42C700 (__thiscall)                                      */
/* ================================================================== */
uint32_t Town::calc_scroll_rect_reversed(RECT* pClipRect, void* surface)
{
    /* Get surface dimensions via IDirectDrawSurface4::GetSurfaceDesc
     * (COM slot 22 = vtable byte offset 0x58). */
    IDirectDrawSurface4* surf = static_cast<IDirectDrawSurface4*>(surface);
    DDSURFACEDESC desc = {};
    desc.dwSize = 0x7C;
    surf->GetSurfaceDesc(&desc);

    RECT surface_rect;
    SetRect(&surface_rect, 0, 0,
            (int32_t)desc.dwWidth, (int32_t)desc.dwHeight);

    RECT viewport_rect;
    SetRect(&viewport_rect, 0, 0, *(int*)((intptr_t)this + 8),
            *(int*)((intptr_t)this + 0xC));

    int scroll_x = pClipRect->left - ((pClipRect->left < 0) ? pClipRect->left : 0);
    int scroll_y = pClipRect->top - ((pClipRect->top < 0) ? pClipRect->top : 0);

    RECT clip_rect;
    IntersectRect(&clip_rect, &viewport_rect, pClipRect);

    if (IsRectEmpty(&clip_rect) != 0) {
        return 0;
    }

    RECT src_rect;
    src_rect.left   = scroll_x;
    src_rect.top    = scroll_y;
    src_rect.right  = scroll_x + (clip_rect.right - clip_rect.left);
    src_rect.bottom = scroll_y + (clip_rect.bottom - clip_rect.top);

    RECT out_rect;
    IntersectRect(&out_rect, &src_rect, &surface_rect);

    if (IsRectEmpty(&out_rect) != 0) {
        return 0;
    }

    if (scroll_x > 0) scroll_x = 0;
    if (scroll_y > 0) scroll_y = 0;

    SetRect(pClipRect,
            clip_rect.left - scroll_x, clip_rect.top - scroll_y,
            (clip_rect.left - scroll_x) + (out_rect.right - out_rect.left),
            (clip_rect.top - scroll_y) + (out_rect.bottom - out_rect.top));

    return 1;
}

/* ================================================================== */
/* Town::postcard_init_list — Initialize postcard dialog (vtable[8])   */
/* Address: 0x42E420                                                   */
/* ================================================================== */
void Town::on_update(int32_t /* param */)
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

    /* HelpWnd_PlayNarration @ 0x44F560 (audio event 1). */
    int result = HelpWnd_PlayNarration(g_audio_mgr, 1, 0);
    if (result != 0) {
        this->audio_playing = 1;                                 /* +0x5ED */
    }
}

/* ================================================================== */
/* Town::postcard_send_handler — Render the postcard overlay           */
/* Address: 0x42E5E0                                                   */
/* ================================================================== */
void Town::postcard_send_handler(char full_render)
{
    if (!this->sprites_initialized || !this->postcard_active) {
        return;
    }

    RECT dest_rect;
    uint32_t src_x, src_y;
    int32_t  src_w;
    uint32_t src_h;

    if (full_render == 0) {
        /* Dest = postcard overlay rect (+0x614..+0x620); source =
         * base workRect (+0xD4..+0xE0). */
        dest_rect.left   = this->postcard_rect_left;
        dest_rect.top    = this->postcard_rect_top;
        dest_rect.right  = this->postcard_rect_right;
        dest_rect.bottom = this->postcard_rect_bottom;

        src_x = (uint32_t)this->workRect.left;
        src_y = (uint32_t)this->workRect.top;
        src_w = this->workRect.right;
        src_h = (uint32_t)this->workRect.bottom;
    } else {
        CopyRect(&dest_rect, &this->preview_rect);               /* +0x68C */
        OffsetRect(&dest_rect, this->postcard_rect_left,
                               this->postcard_rect_top);

        UIPANEL_Blit(
            this->overlay_surface,
            this->preview_rect.left,
            (uint32_t)this->preview_rect.top,
            this->preview_rect.right,
            (uint32_t)this->preview_rect.bottom,
            g_primary_surface,
            dest_rect.left, dest_rect.top,
            dest_rect.right, dest_rect.bottom,
            1);

        CopyRect(&dest_rect, (RECT*)&this->send_rect_left);      /* +0x654 */
        OffsetRect(&dest_rect, this->postcard_rect_left,
                               this->postcard_rect_top);

        src_x = (uint32_t)this->send_rect_left;
        src_y = (uint32_t)this->send_rect_top;
        src_w = this->send_rect_right;
        src_h = (uint32_t)this->send_rect_bottom;
    }

    UIPANEL_Blit(
        this->overlay_surface,
        src_x, src_y, src_w, src_h,
        g_primary_surface,
        dest_rect.left, dest_rect.top,
        dest_rect.right, dest_rect.bottom,
        1);
}

/* ================================================================== */
/* Town::postcard_update_ui — Postcard idle/release UI handler         */
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
            OffsetRect(&src_rect, this->postcard_rect_left,
                                   this->postcard_rect_top);

            ButtonSprite* sprite = this->sprite_inbox;           /* +0x6B4 */
            UIPANEL_Blit(
                this->overlay_surface,
                (uint32_t)sprite->x,
                (uint32_t)sprite->y,
                sprite->sourceX,
                (uint32_t)sprite->sourceY,
                g_primary_surface,
                src_rect.left, src_rect.top,
                src_rect.right, src_rect.bottom,
                1);

            int msg_count = 0;
            if (!this->is_host) {
                msg_count = (int)DPLAY_GetMessageCount(g_dplay);
            } else {
                PostBagFileNode* host = NET_GetHostName(1, 0);
                while (host) {
                    PostBagFileNode* next = host->next;
                    msg_count++;
                    GLOBAL_free(host);
                    host = next;
                }
                this->has_remote_players = (msg_count == 0) ? 0 : 1;
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
                PostBagFileNode* host = NET_GetHostName(1, 0);
                while (host) {
                    PostBagFileNode* next = host->next;
                    msg_count++;
                    GLOBAL_free(host);
                    host = next;
                }
                this->has_remote_players = (msg_count == 0) ? 0 : 1;
            }

            int lut_idx = (msg_count < 5) ? msg_count : 4;
            Sprite_SetState(this->sprite_inbox_counter,          /* +0x6B8 */
                            this->inbox_state_lut[lut_idx], nullptr);
        }
        return;

    case 8:
        {
            uint16_t msg_count = (uint16_t)DPLAY_GetMessageCount(g_dplay);
            int lut_idx = (msg_count < 5) ? msg_count : 4;
            Sprite_SetState(this->sprite_outbox_counter,         /* +0x6BC */
                            this->outbox_state_lut[lut_idx], nullptr);
        }
        return;

    case 9:
        Sprite_SetState(this->sprite_send, 0, nullptr);
        return;
    }
}

/* ================================================================== */
/* Town::postcard_dlg_proc — Postcard press/click handler              */
/* Address: 0x42E150                                                   */
/* ================================================================== */
void Town::postcard_dlg_proc(int action_id)
{
    if (!this->postcard_active) {
        return;
    }

    switch (action_id) {
    case 2:
        PlaySound(0x5015);
        Sprite_SetState(this->sprite_btn_close, 1, nullptr);
        return;

    case 3:
        PlaySound(0x5015);
        Sprite_SetState(this->sprite_btn_options, 1, nullptr);
        return;

    case 4:
        PlaySound(0x5015);
        Sprite_SetState(this->sprite_btn_rotate, 1, nullptr);
        return;

    case 5:
        PlaySound(0x5015);
        Sprite_SetState(this->sprite_btn_save, 1, nullptr);
        return;

    case 6:
        PlaySound(0x5015);
        {
            ButtonSprite* sprite = this->sprite_inbox;           /* +0x6B4 */
            UIPANEL_Blit(
                this->overlay_surface,
                (uint32_t)sprite->x,
                (uint32_t)sprite->y,
                sprite->sourceX,
                (uint32_t)sprite->sourceY,
                g_primary_surface,
                (uint32_t)sprite->x + this->postcard_rect_left,
                (uint32_t)sprite->y + this->postcard_rect_top,
                sprite->sourceX + this->postcard_rect_left,
                (uint32_t)sprite->sourceY + this->postcard_rect_top,
                1);
            Sprite_SetState(this->sprite_inbox, 1, nullptr);
        }
        return;

    case 7:
        PlaySound(0x5015);
        {
            uint16_t msg_count = 0;
            if (!this->is_host) {
                msg_count = (uint16_t)DPLAY_GetMessageCount(g_dplay);
            } else {
                PostBagFileNode* host = NET_GetHostName(1, 0);
                while (host) {
                    PostBagFileNode* next = host->next;
                    msg_count++;
                    GLOBAL_free(host);
                    host = next;
                }
                this->has_remote_players = (msg_count == 0) ? 0 : 1;
            }

            int lut_idx = (msg_count < 5) ? msg_count : 4;
            Sprite_SetState(this->sprite_inbox_counter,
                            this->inbox_state_lut[lut_idx] + 1, nullptr);
        }
        return;

    case 8:
        {
            uint16_t msg_count = (uint16_t)DPLAY_GetMessageCount(g_dplay);
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
/* Town::postcard_update_buttons — Blit the postcard button strip      */
/* Address: 0x42E4E0                                                   */
/* ================================================================== */
void Town::postcard_update_buttons()
{
    RECT dest_rect;
    SetRectEmpty(&dest_rect);
    dest_rect.right  = this->button_src_right - this->button_src_left;
    dest_rect.bottom = this->button_src_bottom - this->button_src_top;

    if (this->repaint_requested) {                               /* +0x605 */
        OffsetRect(&dest_rect, dest_rect.right, 0);
    }

    UIPANEL_Blit(
        this->button_strip_surface,
        (uint32_t)this->button_src_left,
        (uint32_t)this->button_src_top,
        this->button_src_right,
        (uint32_t)this->button_src_bottom,
        g_primary_surface,
        dest_rect.left, dest_rect.top,
        dest_rect.right, dest_rect.bottom,
        1);
}

/* ================================================================== */
/* Town::hit_test_buttons — Hit-test postcard overlay buttons          */
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
    if (PtInRect(&this->button_hit_rect_send, x, y)) {           /* +0x67C */
        return 9;
    }
    if (PtInRect((RECT*)((intptr_t)this->sprite_outbox_counter + 4), x, y)) {
        return 8;
    }
    return 0;
}

/* ================================================================== */
/* Town::hit_test — Postcard paint-throttle window proc (vtable[12])   */
/* Address: 0x42FFF0                                                   */
/* ================================================================== */
LRESULT Town::on_timer(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (this->timer_active != 0) {                               /* +0x5FC */
        this->frame_counter++;                                   /* +0x600 */

        if (this->repaint_requested) {                           /* +0x605 */
            this->repaint_requested = 0;
            this->frame_counter = 0;
            this->postcard_update_buttons();
            UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
        } else if (this->frame_counter > 19) {
            this->repaint_requested = 1;
            this->postcard_update_buttons();
            UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
        }

        if (this->timer_counter != 0) {                          /* +0x5F0 */
            this->timer_counter--;
        }
    }

    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

/* ================================================================== */
/* Town::postcard_click_handler — Left-click on the postcard overlay   */
/* Address: 0x42D670                                                   */
/* ================================================================== */
char Town::postcard_click_handler(int x, int y)
{
    if (this->selection_active != 1) {                           /* +0x88 */
        return 0;
    }

    if (this->postcard_click_flag) {                             /* +0x90 */
        this->postcard_click_flag = 0;
        return 1;
    }

    return RESDATA_HitTestChildren(this, x, y);
}

/* ================================================================== */
/* Town::postcard_command_handler — WM_COMMAND for postcard controls   */
/* Address: 0x42D6B0                                                   */
/* ================================================================== */
int Town::postcard_command_handler(TrackPiece* control, uint32_t wParam,
                                   uint32_t lParam)
{
    if (control == nullptr || control->render_enabled == 0) {    /* +0x56 */
        return 0;
    }

    char handled = ((char (*)(void*, uint32_t, uint32_t))(
        *(void***)control)[2])(control, wParam, lParam);
    if (handled == 0) {
        return 0;
    }

    /* control->resource (+0x44) -> resource_id (+4). */
    int res_id = control->resource->resource_id;
    short timer_val;

    if (res_id == 0x3806) {
        timer_val = control->zoom_level;                         /* +0x48 */
        if (timer_val == 1) {
            control->SetZoom(2);
            control->prev_frame = 6;                             /* +0x54 */
        }
    } else if (res_id == 0x3807) {
        if (control->zoom_level == 1) {
            DDRAW_SelectBuilding(&g_ddraw_building, this->selected_building);
            return 1;
        }
        DDRAW_SelectBuilding(&g_ddraw_building, nullptr);
        return 1;
    } else if (res_id == 0x3808) {
        timer_val = control->zoom_level;
        if (timer_val == 1) {
            control->SetZoom(2);
            control->prev_frame = 6;
        }
    }

    return 1;
}

/* ================================================================== */
/* Town::send_postcard — Postcard sending lifecycle handler            */
/* Address: 0x42D770                                                   */
/* ================================================================== */
byte Town::send_postcard(TrackPiece* track_piece)
{
    if (track_piece == nullptr) {
        return 0;
    }

    if (track_piece->prev_frame >= 0) {                          /* +0x54 */
        track_piece->prev_frame--;
    }

    int res_id = track_piece->resource->resource_id;             /* +0x44 -> +4 */

    if (res_id == 0x3806) {
        if (track_piece->prev_frame == 0 &&
            track_piece->zoom_level == 2) {
            track_piece->SetZoom(1);

            /* world pointer at building+0x44C */
            void* world = *(void**)((intptr_t)this->selected_building + 0x44C);
            if (world) {
                this->select_building(nullptr);
                DDRAW_SelectBuilding(&g_ddraw_building, nullptr);

                /* Save the world (0x43CF47/0x43CF53/0x43CF5D). The
                 * binary passes &g_world (0x4A98B0 = World storage);
                 * the host uses the g_world value — same singleton. */
                char player_id = *(char*)((intptr_t)world + 0x78);
                uint16_t resource_id = *(uint16_t*)((intptr_t)world + 0x7A);
                World_GetObjectAt(world);
                World_RenderAll(world);
                static_cast<World*>(g_world)->SaveToFile(resource_id,
                                                         player_id, 1);
            }
        }
    } else if (res_id == 0x3807) {
        if (track_piece->prev_frame >= 0) {
            DDRAW_SelectBuilding(&g_ddraw_building, this->selected_building);
            track_piece->prev_frame = -1;
        }

        if (g_ddraw_active) {
            track_piece->SetZoom(2);
        } else {
            track_piece->SetZoom(1);
        }
        return 1;
    } else if (res_id - 0x3808 == 0) {
        if (track_piece->prev_frame == 0 &&
            track_piece->zoom_level == 2) {
            track_piece->SetZoom(1);
            this->select_building(nullptr);
            return 1;
        }
    }

    return 1;
}

/* ================================================================== */
/* Town::clear_postcard_ui — Clear/reset the postcard UI               */
/* Address: 0x42E760                                                   */
/* ================================================================== */
void Town::clear_postcard_ui()
{
    if (!this->postcard_active) {                                /* +0x5F8 */
        return;
    }

    if (this->selected_player) {                                 /* +0x608 */
        if (record_need_connect(this->selected_player) == 0) {
            RECT restore_rect;
            CopyRect(&restore_rect, &this->preview_rect);
            OffsetRect(&restore_rect, this->postcard_rect_left,
                                      this->postcard_rect_top);

            UIPANEL_Blit(
                this->overlay_surface,
                this->preview_rect.left,
                (uint32_t)this->preview_rect.top,
                this->preview_rect.right,
                (uint32_t)this->preview_rect.bottom,
                g_primary_surface,
                restore_rect.left, restore_rect.top,
                restore_rect.right, restore_rect.bottom,
                0);
        }

        /* DPLAY_RenderPlayer(dplay, flag(+0x610), player(+0x608),
         * primary, +0x624, +0x628, +0x62C, *(void**)(+0x630)). */
        DPLAY_RenderPlayer(g_dplay,
                           (int)this->player_count_flag,
                           (int)(intptr_t)this->selected_player,
                           g_primary_surface,
                           this->render_param_x,
                           this->render_param_y,
                           this->render_extra,
                           (RECT*)this->render_rect_ptr);

        RECT send_area;
        SetRectEmpty(&send_area);
        send_area.right  = this->send_rect_right - this->send_rect_left;
        send_area.bottom = this->send_rect_bottom - this->send_rect_top;

        UIPANEL_Blit(
            this->overlay_surface,
            (uint32_t)this->send_rect_left,
            (uint32_t)this->send_rect_top,
            this->send_rect_right,
            (uint32_t)this->send_rect_bottom,
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
/* Town::init_overlay_sprite — One-shot overlay sprite initialization  */
/* Address: 0x42FDF0                                                   */
/* ================================================================== */
void Town::init_overlay_sprite()
{
    if (this->overlay_initialized) {                             /* +0x5FA */
        return;
    }

    void* res = ResourceManager_GetById(&g_resmgr, 0x3CF7);
    this->overlay_resource = res;                                /* +0x644 */
    this->overlay_surface = ((void* (*)(void*, int, int))(
        *(void***)res)[1])(res, 0, 0);                           /* +0x648 */
    this->overlay_initialized = 1;
}

/* ================================================================== */
/* Town::init_postcard_sprites — Initialize the 8 postcard sprites     */
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
    Sprite_Init(this->sprite_outbox_counter);
    Sprite_Init(this->sprite_inbox_counter);
    Sprite_Init(this->sprite_send);

    void* res = ResourceManager_GetById(&g_resmgr, 0x3CF8);
    this->background_resource = res;                             /* +0x64C */
    this->background_surface = ((void* (*)(void*, int, int))(
        *(void***)res)[1])(res, 0, 0);                           /* +0x650 */

    res = ResourceManager_GetById(&g_resmgr, 0x3CFB);
    this->button_strip_resource = res;                           /* +0x664 */
    this->button_strip_surface = ((void* (*)(void*, int, int))(
        *(void***)res)[1])(res, 0, 0);                           /* +0x668 */

    res = ResourceManager_GetById(&g_resmgr, 0x3CFA);
    this->send_confirm_resource = res;                           /* +0x69C */
    this->send_confirm_surface = ((void* (*)(void*, int, int))(
        *(void***)res)[1])(res, 0, 0);                           /* +0x6A0 */

    this->sprites_initialized = 1;                               /* +0x5F9 */
}

/* ================================================================== */
/* Town::load_background — Town window proc (vtable[11])               */
/* Address: 0x42EE20                                                   */
/* ================================================================== */
LRESULT Town::window_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
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

    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

/* ================================================================== */
/* Town::postcard_draw_preview — Preview overlay dialog proc (vtable[21]) */
/* Address: 0x42F810                                                   */
/* ================================================================== */
LRESULT Town::on_key_down(HWND hWnd, UINT msg, WPARAM wParam,
                                    LPARAM lParam)
{
    if (this->postcard_active && !this->flag_E8 && !this->audio_playing) {
        if (wParam == 0x1B || wParam == 0x51) {   /* ESC or Q */
            this->postcard_dlg_proc(2);
            UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
            Sleep(0x96);
            PlaySound(0x5015);

            this->hide();                           /* vtable[1] */
            CGWND_SetMode(3);
            return 0;
        }
        return DefWindowProcA(hWnd, msg, wParam, lParam);
    }

    return 0;
}

/* ================================================================== */
/* Town::postcard_mouse_handler — Postcard button mouse dispatch        */
/* Address: 0x430800 (vtable[20])                                      */
/*                                                                     */
/* Guards on audio_playing/flag_E8/postcard_active, hit-tests the       */
/* buttons, refreshes has_remote_players for the counter buttons and    */
/* re-renders the postcard overlay for the pressed buttons. Also        */
/* dispatched from the track_building child loop with the child pointer */
/* (the original treats it as packed mouse coords).                     */
/* ================================================================== */
LRESULT Town::on_mouse_move(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)hWnd; (void)msg; (void)wParam;
    const int32_t packed_xy = static_cast<int32_t>(lParam);
    int32_t x = packed_xy & 0xFFFF;
    int32_t y = (packed_xy >> 16) & 0xFFFF;
    int sVar3 = 0;

    if (this->audio_playing) {                   /* +0x5ED */
        return 0;
    }
    if (this->flag_E8) {                         /* +0xE8 */
        /* LAB_00430861: re-render the overlay pair. */
        this->set_mode(this->childCount1, this->childObj1, 0, 1);
        return 0;
    }
    if (!this->postcard_active) {                /* +0x5F8 */
        return 0;
    }

    switch (this->hit_test_buttons(x, y)) {
    case 0:
        if (this->postcard_data) {               /* +0x60C */
            return 0;
        }
        goto set_mode_pair;
    case 2:
    case 3:
    case 4:
        /* Binary: falls into the button-set-mode path (caseD_2). */
        goto button_set_mode;
    case 5:
        sVar3 = 0;
        if (this->is_host) {
            PostBagFileNode* host = NET_GetHostName(1, 0);
            while (host) {
                PostBagFileNode* next = host->next;
                sVar3++;
                GLOBAL_free(host);
                host = next;
            }
        } else {
            sVar3 = DPLAY_GetMessageCount(g_dplay);
        }
        break;
    case 6:
    case 7:
        sVar3 = 0;
        if (!this->is_host) {
            sVar3 = DPLAY_GetMessageCount(g_dplay);
        } else {
            PostBagFileNode* host = NET_GetHostName(1, 0);
            while (host) {
                PostBagFileNode* next = host->next;
                sVar3++;
                GLOBAL_free(host);
                host = next;
            }
        }
        break;
    case 9:
        if (this->postcard_data) {
            return 0;
        }
        goto set_mode_pair_again;
    default:
        return 0;
    }

    this->has_remote_players = (sVar3 == 0) ? 0 : 1;    /* +0x607 */

    if (sVar3 != 0) {
    button_set_mode:
        if (this->postcard_data == 0) {
        set_mode_pair_again:
            this->set_mode(this->childCount1, this->childObj1, 0, 1);
        }
    }
    return 0;

set_mode_pair:
    /* vtable[3]: set_mode(childCount1, childObj1, 0, 1) — the binary
     * pushes the +0x68/+0x6C base pair. */
    this->set_mode(this->childCount1, this->childObj1, 0, 1);
    return 0;
}

/* ================================================================== */
/* Town::postcard_wnd_proc — Postcard overlay window proc (vtable[14]) */
/* Address: 0x430190                                                   */
/*                                                                     */
/* Handles mouse clicks on the postcard preview/button areas: save a    */
/* received postcard, select a player, toggle the inbox preview, and    */
/* the close/options/rotate/save/send button actions. The argument is   */
/* the packed mouse position (x in low 16 bits, y in high 16 bits).     */
/* ================================================================== */
LRESULT Town::on_lbutton_down(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)hWnd; (void)msg; (void)wParam;
    const int32_t packed_xy = static_cast<int32_t>(lParam);
    int32_t x = packed_xy & 0xFFFF;
    int32_t y = (packed_xy >> 16) & 0xFFFF;

    if (!this->postcard_active || this->flag_E8 || this->audio_playing) {
        return 0;
    }

    /* Click on the preview area of a player that still needs the
     * connection handshake: save the received postcard. */
    if (this->selected_player &&
        record_need_connect(this->selected_player) != 0 &&
        PtInRect(&this->preview_rect, x, y)) {
        PlaySound(0x5464);
        if (!this->is_host) {
            return 0;
        }
        this->save_received_postcard((uint32_t)y);
        return 0;
    }

    /* Click on the player render area: select the player. */
    if (this->selected_player &&
        PtInRect((RECT*)&this->render_param_x, x, y)) {
        PlaySound(0x5015);
        this->postcard_data = this->selected_player;      /* +0x60C */
        /* vtable[3] with (send_confirm_surface, send_confirm_resource,
         * 0, 1) — binary passes the +0x6A0/+0x69C pair. */
        this->set_mode((int32_t)(intptr_t)this->send_confirm_surface,
                       this->send_confirm_resource, 0, 1);
        return 0;
    }

    /* Click on the button strip area: toggle the inbox preview. */
    if (PtInRect((RECT*)&this->button_src_left, x, y)) {
        if (this->timer_counter) {                         /* +0x5F0 */
            return 0;
        }
        this->timer_counter = 8;
        PlaySound(0x5114);
        this->frame_counter = 0x15;                        /* +0x600 */
        if (this->repaint_requested == 1) {                /* +0x605 */
            this->repaint_requested = 0;
            this->frame_counter = 0;
            this->postcard_update_buttons();
            UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
        } else {
            this->repaint_requested = 1;
            this->postcard_update_buttons();
            UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
        }
    }

    switch (this->hit_test_buttons(x, y)) {
    case 0:
        if (this->postcard_data) {
            return 0;
        }
        goto set_mode_base_pair;

    case 2:
        if (this->postcard_data == 0) {
            this->postcard_dlg_proc(2);
            UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
            Sleep(0x96);
            PlaySound(0x5015);
            this->hide();                        /* vtable[1] */
            return 0;
        }
        goto clear_pending;

    case 3:
        if (this->postcard_data == 0) {
            this->postcard_dlg_proc(3);
            UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
            Sleep(0x96);
            /* g_postcard vtable[2] (PostcardAlbum show). */
            static_cast<PostcardAlbum*>(g_postcard)->show();
            this->hide();                        /* vtable[1] */
            return 0;
        }
        this->set_mode(this->childCount1, this->childObj1, 0, 1);
        this->receive_postcard();
        this->postcard_data = 0;
        this->clear_postcard_ui();
        if (!this->is_host && this->selected_player == 0) {
            this->postcard_update_ui(8);
        }
        this->postcard_update_ui(7);
        UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
        return 0;

    case 4:
        if (this->postcard_data) {
            this->set_mode(this->childCount1, this->childObj1, 0, 1);
        }
        this->postcard_dlg_proc(4);
        UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
        Sleep(0x96);
        Cursor_Show(g_cursor);
        if (this->postcard_data) {
            this->postcard_data = 0;
            this->selected_player = nullptr;
        }
        this->hide();                        /* vtable[1] */
        this->postcard_active = 0;           /* +0x5F8 */
        return 0;

    case 5:
        if (this->postcard_data) {
            this->postcard_data = 0;
            this->set_mode(this->childCount1, this->childObj1, 0, 1);
        }
        if (!this->has_remote_players) {     /* +0x607 */
            return 0;
        }
        this->postcard_dlg_proc(5);
        UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
        Sleep(0x96);
        this->postcard_update_ui(5);
        this->delete_postcard();
        this->list_postcards();
        this->postcard_send_handler(0);
        this->postcard_update_ui(8);
        this->postcard_update_ui(2);
        this->postcard_update_ui(3);
        this->postcard_update_ui(4);
        this->postcard_update_ui(7);
        this->postcard_update_ui(6);
        this->clear_postcard_ui();
        if (!this->is_host && this->selected_player == 0) {
            this->postcard_update_ui(8);
        }
        break;

    case 6:
        if (this->postcard_data) {
            this->postcard_data = 0;
            this->set_mode(this->childCount1, this->childObj1, 0, 1);
            return 0;
        }
        if (!this->has_remote_players) {
            return 0;
        }
        this->postcard_dlg_proc(6);
        UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
        Sleep(0x96);
        this->postcard_update_ui(6);
        /* Toggle the player-count flag (+0x610). */
        this->player_count_flag = (this->player_count_flag == 0) ? 1 : 0;
        this->clear_postcard_ui();
        break;

    case 7:
        if (this->postcard_data) {
            this->set_mode(this->childCount1, this->childObj1, 0, 1);
            this->save_postcard();
            this->postcard_data = 0;
            this->clear_postcard_ui();
            if (!this->is_host && this->selected_player == 0) {
                this->postcard_update_ui(8);
                UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
                return 0;
            }
            goto end_paint_ex;
        }
        if (!this->has_remote_players) {
            goto case_8_path;
        }
        this->postcard_dlg_proc(7);
        UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
        Sleep(0x96);
        this->list_postcards();
        this->postcard_update_ui(7);
        this->postcard_update_ui(8);
        this->postcard_update_ui(6);
        this->clear_postcard_ui();
        break;

    case_8_path:
    case 8:
        if (this->postcard_data == 0) {
            return 0;
        }
        this->set_mode(this->childCount1, this->childObj1, 0, 1);
        this->load_postcard();
        this->postcard_data = 0;
        this->postcard_update_ui(8);
        this->postcard_update_ui(7);
        this->clear_postcard_ui();
    end_paint_ex:
        UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
        return 0;

    case 9:
        if (this->postcard_data) {
            goto clear_pending;
        }
        if (this->timer_counter) {
            return 0;
        }
        this->timer_counter = 8;
        this->postcard_dlg_proc(9);
        UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
        Sleep(0x96);
        this->postcard_update_ui(9);
        PlaySound(0x5276);
        break;
    }

    UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
    return 0;

set_mode_base_pair:
    this->set_mode(this->childCount1, this->childObj1, 0, 1);
    return 0;

clear_pending:
    this->postcard_data = 0;
    this->set_mode(this->childCount1, this->childObj1, 0, 1);
    return 0;
}

/* ================================================================== */
/* Town::upload_postcard — Write postcard payload to remote players    */
/* Address: 0x4309B0                                                   */
/* ================================================================== */
void Town::upload_postcard()
{
    if (!this->selected_player) {                                /* +0x608 */
        return;
    }

    int type = this->is_host ? 1 : 2;
    PostBagFileNode* hostname = NET_GetHostName(type, 0);

    while (hostname) {
        void* addr = NET_ResolveAddress(hostname->path);
        if (addr && record_player_id(addr) ==
                    record_player_id(this->selected_player)) {
            g_dplay->UnregisterPlayer(hostname->path);
            HANDLE hFile = CreateFileA(hostname->path,
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
            delete (DPlayManager*)addr;
        }
        PostBagFileNode* next = hostname->next;
        GLOBAL_free(hostname);
        hostname = next;
    }
}

/* ================================================================== */
/* Town::receive_postcard — Process an incoming network postcard       */
/* Address: 0x42D8A0                                                   */
/* ================================================================== */
void Town::receive_postcard()
{
    if (!this->postcard_data) {                                  /* +0x60C */
        return;
    }

    int type = this->is_host ? 1 : 2;
    PostBagFileNode* hostname = NET_GetHostName(type, 0);

    while (hostname) {
        void* addr = NET_ResolveAddress(hostname->path);
        if (this->selected_player &&
            addr && record_player_id(addr) ==
                    record_player_id(this->selected_player)) {
            g_dplay->UnregisterPlayer(hostname->path);
        }
        if (addr) {
            delete (DPlayManager*)addr;
        }
        PostBagFileNode* next = hostname->next;
        GLOBAL_free(hostname);
        hostname = next;
    }

    /* If the sender still needs the connection handshake, remove the
     * Att_In temp file and clear the flag. */
    if (record_need_connect(this->postcard_data) != 0) {
        char att_path[1284];
        NET_GetAttFilePath(record_player_id(this->postcard_data) & 0xFFFF,
                           5, att_path);
        DeleteFileA(att_path);
        record_set_need_connect(this->postcard_data, 0);
    }

    NET_RegisterPlayer(g_dplay, this->postcard_data, 0, 0);

    if (this->selected_player) {
        delete (DPlayManager*)this->selected_player;
    }
    this->selected_player = nullptr;

    this->list_postcards();

    int count = 0;
    if (this->is_host) {
        PostBagFileNode* p = NET_GetHostName(1, 0);
        while (p) {
            PostBagFileNode* next = p->next;
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
/* Town::list_postcards — Cycle selected_player to the next entry      */
/* Address: 0x42DD50                                                   */
/* ================================================================== */
void Town::list_postcards()
{
    int type = this->is_host ? 1 : 2;
    PostBagFileNode* first_hostname = NET_GetHostName(type, 0);

    if (!first_hostname) {
        return;
    }

    void* first_addr = NET_ResolveAddress(first_hostname->path);
    if (!this->selected_player) {
        this->selected_player = first_addr;
        PostBagFileNode* p = first_hostname;
        while (p) {
            PostBagFileNode* next = p->next;
            GLOBAL_free(p);
            p = next;
        }
        return;
    }

    void* next_player = nullptr;
    PostBagFileNode* p = first_hostname;
    while (p) {
        void* addr = NET_ResolveAddress(p->path);

        if (!next_player &&
            addr && record_player_id(addr) ==
                    record_player_id(this->selected_player) &&
            p->next) {
            next_player = NET_ResolveAddress(p->next->path);
        }

        if (addr) {
            delete (DPlayManager*)addr;
        }
        PostBagFileNode* next = p->next;
        GLOBAL_free(p);
        p = next;
    }

    if (!next_player) {
        if (this->selected_player) {
            delete (DPlayManager*)this->selected_player;
        }
        this->selected_player = first_addr;
        return;
    }

    if (first_addr) {
        delete (DPlayManager*)first_addr;
    }
    if (this->selected_player) {
        delete (DPlayManager*)this->selected_player;
    }
    this->selected_player = next_player;
}

/* ================================================================== */
/* Town::save_postcard — Client-side session re-registration           */
/* Address: 0x42DA10                                                   */
/* ================================================================== */
void Town::save_postcard()
{
    if (!this->postcard_data || this->is_host) {
        return;
    }

    PostBagFileNode* hostname = NET_GetHostName(2, 0);
    while (hostname) {
        void* addr = NET_ResolveAddress(hostname->path);
        if (this->selected_player &&
            addr && record_player_id(addr) ==
                    record_player_id(this->selected_player)) {
            g_dplay->UnregisterPlayer(hostname->path);
        }
        if (addr) {
            delete (DPlayManager*)addr;
        }
        PostBagFileNode* next = hostname->next;
        GLOBAL_free(hostname);
        hostname = next;
    }

    NET_RegisterPlayer(g_dplay, this->postcard_data, 1, 0);

    if (this->selected_player) {
        delete (DPlayManager*)this->selected_player;
    }
    this->selected_player = nullptr;

    this->list_postcards();

    int count = 0;
    if (this->is_host) {
        PostBagFileNode* p = NET_GetHostName(1, 0);
        while (p) {
            PostBagFileNode* next = p->next;
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
/* Town::load_postcard — Host-side session re-registration             */
/* Address: 0x42DB30                                                   */
/* ================================================================== */
void Town::load_postcard()
{
    if (!this->is_host || !this->postcard_data) {
        return;
    }

    PostBagFileNode* hostname = NET_GetHostName(1, 0);
    while (hostname) {
        void* addr = NET_ResolveAddress(hostname->path);
        if (this->selected_player &&
            addr && record_player_id(addr) ==
                    record_player_id(this->selected_player)) {
            g_dplay->UnregisterPlayer(hostname->path);
        }
        if (addr) {
            delete (DPlayManager*)addr;
        }
        PostBagFileNode* next = hostname->next;
        GLOBAL_free(hostname);
        hostname = next;
    }

    NET_RegisterPlayer(g_dplay, this->postcard_data, 2, 0);

    if (this->selected_player) {
        delete (DPlayManager*)this->selected_player;
    }
    this->selected_player = nullptr;

    this->list_postcards();

    int count = 0;
    if (this->is_host) {
        PostBagFileNode* p = NET_GetHostName(1, 0);
        while (p) {
            PostBagFileNode* next = p->next;
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
/* Town::delete_postcard — Delete a player's .crd postcard file        */
/* Address: 0x42DC50                                                   */
/* ================================================================== */
void Town::delete_postcard()
{
    if (!this->selected_player) {
        return;
    }

    int type = this->is_host ? 1 : 2;
    PostBagFileNode* hostname = NET_GetHostName(type, 0);

    while (hostname) {
        void* addr = NET_ResolveAddress(hostname->path);
        if (this->selected_player &&
            addr && record_player_id(addr) ==
                    record_player_id(this->selected_player)) {
            g_dplay->UnregisterPlayer(hostname->path);
            if (this->selected_player) {
                delete (DPlayManager*)this->selected_player;
            }
            this->selected_player = nullptr;
        }
        if (addr) {
            delete (DPlayManager*)addr;
        }
        PostBagFileNode* next = hostname->next;
        GLOBAL_free(hostname);
        hostname = next;
    }

    int count = 0;
    if (this->is_host) {
        PostBagFileNode* p = NET_GetHostName(1, 0);
        while (p) {
            PostBagFileNode* next = p->next;
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
/* Town::save_postcard_as — Show the "Save As" dialog                  */
/* Address: 0x42EEA0 (__thiscall, 1 stack arg: unused)                 */
/*                                                                     */
/* NOTE: the OFN field values below were verified against the          */
/* disassembly (0x42F092..0x42F0D6). The string-buffer assembly        */
/* (filter fragment "*" / "c:" initial dir / extension dance) is       */
/* reconstructed from the verified outward fields; the exact internal  */
/* buffer gymnastics are cosmetic and were garbled in the decompiler.  */
/* ================================================================== */
byte Town::save_postcard_as()
{
    /* Step 1: default filename buffer lives at this+0xE9 (0x504 bytes,
     * overlapping the viewport/panel fields during the dialog). */
    char* filename_buf = (char*)this + 0xE9;

    /* File title + initial-dir buffers for the dialog. */
    char file_title_buf[0x100] = {0};
    char initial_dir_buf[0x100] = {0};

    /* Build the initial directory string: the binary starts it with the
     * embedded word "c:" (0x3A63 at 0x42F0D6 -> lpstrInitialDir). */
    memcpy(initial_dir_buf, "c:", 2);

    /* Download the postcard attachment from the network cache using the
     * sender player id (selected_player + 0x3A). */
    NET_DownloadAsset(record_need_connect(this->selected_player),
                      5, filename_buf);

    /* Step 2: format the dialog title (resource 0x6a). */
    char title_buf[0x100] = {0};
    FormatResourceString(g_resmgr, 0x6a, title_buf, sizeof(title_buf));

    /* Step 3: set up OPENFILENAMEA (all fields verified against the
     * disassembly writes at 0x42F092..0x42F0D6). */
    struct OFN {
        uint32_t lStructSize;       /* +0x00 */
        void*    hwndOwner;         /* +0x04 */
        void*    hInstance;         /* +0x08 */
        const char* lpstrFilter;    /* +0x0C */
        char*    lpstrCustomFilter; /* +0x10 */
        uint32_t nMaxCustFilter;    /* +0x14 */
        uint32_t nFilterIndex;      /* +0x18 */
        char*    lpstrFile;         /* +0x1C */
        uint32_t nMaxFile;          /* +0x20 */
        char*    lpstrFileTitle;    /* +0x24 */
        uint32_t nMaxFileTitle;     /* +0x28 */
        const char* lpstrInitialDir;/* +0x2C */
        const char* lpstrTitle;     /* +0x30 */
        uint32_t Flags;             /* +0x34 */
        uint16_t nFileOffset;       /* +0x38 */
        uint16_t nFileExtension;    /* +0x3A */
        const char* lpstrDefExt;    /* +0x3C */
        void*    lCustData;         /* +0x40 */
        void*    lpfnHook;          /* +0x44 */
        const char* lpTemplateName; /* +0x48 */
    } ofn = {};

    ofn.lStructSize    = sizeof(OFN);                       /* 0x4C */
    ofn.hwndOwner      = this->hWnd;
    ofn.hInstance      = this->hInstance;
    ofn.lpstrFilter    = &g_empty_string;                   /* "" — binary 0x4851D0 */
    ofn.nFilterIndex   = 1;
    ofn.lpstrFile      = filename_buf;                      /* this+0xE9 */
    ofn.nMaxFile       = 0x504;
    ofn.lpstrFileTitle = file_title_buf;
    ofn.nMaxFileTitle  = 0x104;
    ofn.lpstrInitialDir = initial_dir_buf;
    ofn.lpstrTitle     = title_buf;
    ofn.Flags          = 0x80024;  /* OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT|OFN_EXPLORER */
    ofn.lpfnHook       = (void*)&SaveAsDlgHook;             /* 0x419FD0 */

    /* Step 4: show the dialog — enable window, set guard, reposition. */
    PostMessageA(this->hWnd, 0x5F5, 0, 0);      /* WM_USER+0x1F5: enable town window */
    this->flag_E8 = 1;                           /* +0xE8 dialog guard */

    /* Reposition the cursor onto the send button sprite. */
    SetCursorPos(this->sprite_send->x,
                 this->sprite_send->sourceY + 0x14);

    /* Center the viewport via vtable[3]. */
    this->set_mode(this->childCount0, this->childObj0, 0, 1);

    /* Step 5: GetSaveFileNameA */
    BOOL result = GetSaveFileNameA(&ofn);
    this->flag_E8 = 0;

    if (result != 0) {
        /* Check if the chosen file already exists. */
        HANDLE hFile = CreateFileA(ofn.lpstrFile,
                                   0xC0000000,     /* GENERIC_READ|GENERIC_WRITE */
                                   0,              /* no sharing (binary: share=0) */
                                   nullptr,
                                   3,              /* OPEN_EXISTING */
                                   0x80,           /* FILE_ATTRIBUTE_NORMAL */
                                   nullptr);

        if (hFile != (HANDLE)-1) {
            /* File exists — prompt for overwrite (MB_YESNO = 4). */
            char msg_buf[0x100] = {0};
            FormatResourceString(g_resmgr, 0x6b, msg_buf, sizeof(msg_buf));
            int choice = MessageBoxA(this->hWnd, msg_buf,
                                     s_LEGO_LOCO_0047e1c0,
                                     4);            /* MB_YESNO */
            CloseHandle(hFile);

            if (choice == 6) {       /* IDYES = overwrite */
                return 1;
            }
            return 0;                /* No = cancel */
        }

        /* Step 6: handle error codes. */
        DWORD err = GetLastError();

        if (err == 2) {              /* ERROR_FILE_NOT_FOUND */
            return 1;
        }

        if (err == 3) {              /* ERROR_PATH_NOT_FOUND */
            char msg_buf[0x100] = {0};
            FormatResourceString(g_resmgr, 0x6d, msg_buf, sizeof(msg_buf));
            int choice = MessageBoxA(this->hWnd, msg_buf,
                                     s_LEGO_LOCO_0047e1c0,
                                     0x24);          /* MB_YESNO|MB_ICONQUESTION */
            return (choice != 6) ? 0 : 2;            /* YES -> 2 (create dir) */
        }

        /* Other errors — error dialog (MB_OK|MB_ICONSTOP = 0x30). */
        {
            char msg_buf[0x100] = {0};
            FormatResourceString(g_resmgr, 0x6d, msg_buf, sizeof(msg_buf));
            MessageBoxA(this->hWnd, msg_buf,
                        s_LEGO_LOCO_0047e1c0,
                        0x30);
        }
    }

    return 0;  /* Cancelled or error */
}

/* ================================================================== */
/* Town::save_received_postcard — Download and save a received postcard */
/* Address: 0x42F250 (__thiscall, 1 stack arg: unused_arg)             */
/* ================================================================== */
void Town::save_received_postcard(uint32_t unused_arg)
{
    /* The player id always comes from selected_player + 0x3A;
     * unused_arg is present in the original signature but never read. */
    uint16_t player_id = record_need_connect(this->selected_player);

    /* Step 1: open the cached .dat payload file. */
    char cache_path[1284];
    NET_GetFilePath(player_id, 5, cache_path);

    HANDLE hFile = CreateFileA(cache_path, 0x80000000,   /* GENERIC_READ */
                               3,           /* FILE_SHARE_READ|FILE_SHARE_WRITE */
                               nullptr,
                               3,           /* OPEN_EXISTING */
                               0x80,        /* FILE_ATTRIBUTE_NORMAL */
                               nullptr);

    if (hFile == (HANDLE)-1) {
        DWORD err = GetLastError();
        char* msg_buf = nullptr;
        FormatMessageA(0x1100, nullptr, err, 0x400,
                       (char*)&msg_buf, 0, nullptr);
        MessageBoxA(this->hWnd, msg_buf, s_LEGO_LOCO_0047e1c0, 0x10);
        LocalFree(msg_buf);

        record_set_need_connect(this->selected_player, 0);
        this->clear_postcard_ui();
        UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
        return;
    }

    /* Step 2: read the 0x504-byte payload into the +0xE9 buffer. */
    char* payload_buf = (char*)this + 0xE9;
    DWORD bytes_read;
    BOOL read_ok = ReadFile(hFile, payload_buf, 0x504, &bytes_read, nullptr);

    if (!read_ok) {
        DWORD err = GetLastError();
        CloseHandle(hFile);

        char* msg_buf = nullptr;
        FormatMessageA(0x1100, nullptr, err, 0x400,
                       (char*)&msg_buf, 0, nullptr);
        MessageBoxA(this->hWnd, msg_buf, s_LEGO_LOCO_0047e1c0, 0x10);
        LocalFree(msg_buf);

        record_set_need_connect(this->selected_player, 0);
        this->clear_postcard_ui();
        UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
        return;
    }

    CloseHandle(hFile);

    /* Step 3: show the Save As dialog — on success the +0xE9 buffer
     * holds the user's chosen filename. */
    byte save_result = this->save_postcard_as();
    if (save_result == 0) {
        return;  /* User cancelled */
    }

    /* Step 4: if a new directory is needed, extract it and create it. */
    if (save_result == 2) {
        char dir_path[1284];
        strcpy(dir_path, payload_buf);

        char* last_slash = _strrchr(dir_path, '\\');
        if (last_slash != nullptr) {
            *last_slash = '\0';
        }

        if (!CreateDirectoryA(dir_path, nullptr)) {
            DWORD err = GetLastError();
            char* msg_buf = nullptr;
            FormatMessageA(0x1100, nullptr, err, 0x400,
                           (char*)&msg_buf, 0, nullptr);
            MessageBoxA(this->hWnd, msg_buf, s_LEGO_LOCO_0047e1c0, 0x10);
            LocalFree(msg_buf);
            return;
        }
    }

    /* Step 5: copy from the attachment cache path to the chosen path. */
    char att_path[1284];
    NET_GetAttFilePath(player_id, 5, att_path);

    BOOL copy_ok = CopyFileA(att_path, payload_buf, FALSE);
    if (!copy_ok) {
        DWORD err = GetLastError();
        char* msg_buf = nullptr;
        FormatMessageA(0x1100, nullptr, err, 0x400,
                       (char*)&msg_buf, 0, nullptr);
        MessageBoxA(this->hWnd, msg_buf, s_LEGO_LOCO_0047e1c0, 0x10);
        LocalFree(msg_buf);
    }

    /* Step 6: delete the cache files. */
    BOOL del_ok = DeleteFileA(att_path);
    if (!del_ok) {
        DWORD err = GetLastError();
        char* msg_buf = nullptr;
        FormatMessageA(0x1100, nullptr, err, 0x400,
                       (char*)&msg_buf, 0, nullptr);
        MessageBoxA(this->hWnd, msg_buf, s_LEGO_LOCO_0047e1c0, 0x10);
        LocalFree(msg_buf);
    } else {
        /* Also delete the main cache .dat file. */
        NET_GetFilePath(player_id, 5, cache_path);
        BOOL del2_ok = DeleteFileA(cache_path);
        if (!del2_ok) {
            DWORD err = GetLastError();
            char* msg_buf = nullptr;
            FormatMessageA(0x1100, nullptr, err, 0x400,
                           (char*)&msg_buf, 0, nullptr);
            MessageBoxA(this->hWnd, msg_buf, s_LEGO_LOCO_0047e1c0, 0x10);
            LocalFree(msg_buf);
        }

        /* Clear the need_connect flag and notify peers. */
        record_set_need_connect(this->selected_player, 0);
        this->upload_postcard();
    }

    /* Step 7: clean up the postcard UI. */
    this->clear_postcard_ui();
    UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, nullptr);
}

/* ================================================================== */
/* Train_HandleTrackBuild — Process a remote track-build message       */
/* Address: 0x43CE10 (__thiscall: ECX = TrainSubsystem, 1 stack arg)   */
/*                                                                     */
/* Called from Train_ProcessMessages for message type 0x3EC.           */
/* ================================================================== */
void Train_HandleTrackBuild(void* subsystem, int msg)
{
    uintptr_t sub = (uintptr_t)subsystem;
    uintptr_t data = (uintptr_t)msg;

    /* Reset the pending-timeout field on every pending vehicle node. */
    for (void* node = *(void**)(sub + 0x14); node != nullptr;
         node = *(void**)((intptr_t)node + 0x70)) {
        *(uint16_t*)((intptr_t)node + 0x74) = 32000;
    }

    if (*(int*)(data + 0xC) != 0) {
        /* Create a local vehicle with a random type from 3 variants
         * starting at resource 0x1804. */
        void* mem = operator_new(0x94);
        void* vehicle = nullptr;
        if (mem != nullptr) {
            vehicle = Vehicle_Ctor(mem,
                                   (CRT_rand() % 3) * 2 + 0x1804,
                                   1, 1, 1);
        }

        if (vehicle != nullptr) {
            /* vehicle->editors[0] (+0x10) vtable slot 13: set the
             * player-name string (0x43CEA9..0x43CEAD). */
            void* editor = *(void**)((intptr_t)vehicle + 0x10);
            ((void (*)(void*, const char*))(*(void***)editor)[13])(
                editor, s_LEGO_LOCO_0047e1c0);

            /* Clear the vehicle's saved-track fields. */
            *(uint16_t*)((intptr_t)vehicle + 0x74) = 0;
            *(uint16_t*)((intptr_t)vehicle + 0x76) = 0;
            *(uint16_t*)((intptr_t)vehicle + 0x7E) = 0;
            *(uint16_t*)((intptr_t)vehicle + 0x80) = 0;
            *(uint8_t*)((intptr_t)vehicle + 0x82) = 0;
            *(uint16_t*)((intptr_t)vehicle + 0x84) = 0;
            *(uint16_t*)((intptr_t)vehicle + 0x86) = 0;

            /* For each session record in the message: create a slot,
             * copy the local player name, init the route, attach the
             * slot to the vehicle editor and download missing assets. */
            const uint8_t* record = (const uint8_t*)(data + 0x10);
            int count = *(int*)(data + 0xC);
            int index = 0;
            while (index < count) {
                DPlayManager* session = DPlayManager::EnumerateSessions(
                    (const DPLAY_SessionData*)record);
                /* Binary: no null check before use (0x43CF53..0x43CF5D). */
                const char* name = (const char*)g_player_config + 6;
                size_t len = strlen(name);
                memcpy((char*)((intptr_t)session + 0x10), name, len + 1);
                *(uint16_t*)((intptr_t)session + 0x3A) = 0;

                Vehicle_InitRoute(vehicle, 0x1871, 4, 1);
                VehicleEditor_SetDPlayData(
                    *(void**)((intptr_t)vehicle + 0x14 + index * 4),
                    (int)(intptr_t)session);
                static_cast<TrainSubsystem*>(subsystem)
                    ->DownloadMissingAssets(session);
                if (session != nullptr) {
                    delete session;
                }
                record += 0xE4;
                index++;
            }

            /* Queue a type-0xF TrainMessage carrying the vehicle. */
            TrainMessage* tm = new TrainMessage();
            tm->type = 0xF;
            tm->data_ptr = vehicle;
            *(uint8_t*)((intptr_t)vehicle + 0x88) = 0;
            NETMAN_QueueMessage(tm);
        }
    }

    /* Move the first pending vehicle node (if any) into the network
     * queue. */
    void* head = *(void**)(sub + 0x14);
    if (head != nullptr) {
        *(void**)(sub + 0x14) = *(void**)((intptr_t)head + 0x70);
        *(void**)((intptr_t)head + 0x70) = nullptr;

        TrainMessage* tm = new TrainMessage();
        tm->type = 0xF;
        tm->data_ptr = head;
        *(uint8_t*)((intptr_t)head + 0x88) = 0;
        NETMAN_QueueMessage(tm);
    }

    if (*(void**)(sub + 0x14) == nullptr) {
        if (*(int*)(sub + 0x34) == 0) {
            DirectPlay_Close(g_dplay_peer);
        }

        /* Drain any remaining pending vehicles into the queue. */
        while (*(void**)(sub + 0x14) != nullptr) {
            uintptr_t node = (uintptr_t)*(void**)(sub + 0x14);
            TrainMessage* tm = new TrainMessage();
            tm->type = 0xF;
            tm->data_ptr = (void*)node;
            *(uint8_t*)(node + 0x88) = 0;
            *(void**)(sub + 0x14) = *(void**)(node + 0x70);
            *(void**)(node + 0x70) = nullptr;
            NETMAN_QueueMessage(tm);
        }
    } else {
        Train_SendPlayerInfo((void*)sub);
    }
}
