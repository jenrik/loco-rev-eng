// Status: TRANSCRIBED
/**
 * EditWindow.cpp -- EditWindow (UI_MainMenu) implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

#include "EditWindow.h"
#include "UI_Utils.h"
#include "NameEntryPanel.h"
#include "GameSetupPanel.h"
#include "../core/CGWND.h"
#include "../game/PlayerConfig.h"
#include "../game/Train.h"
#include "../graphics/LOCOBITMAP.h"
#include "resource_manager_sdl3.h"
#include "sdl3_ddraw.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
#include <stdint.h>
#include <cstdint>
#include <cstring>

/* ================================================================== */
/* POINT structure for GDI hit-testing                                  */
/* ================================================================== */
struct POINT32 {
    int32_t x;
    int32_t y;
};

/* ================================================================== */
/* Windows API declarations                                            */
/* ================================================================== */

#ifdef _WIN32
extern "C" {

void* __stdcall GetDesktopWindow(void);
int   __stdcall GetClientRect(void* hWnd, void* lpRect);
void* __stdcall LoadIconA(void* hInstance, const char* lpIconName);
void* __stdcall CreateSolidBrush(uint32_t crColor);
void* __stdcall CreateHatchBrush(int fnStyle, uint32_t crColor);
BOOL  __stdcall DeleteObject(void* hObject);
void  __stdcall PostQuitMessage(int nExitCode);
void  __stdcall ReleaseCapture(void);
int   __stdcall ShowCursor(BOOL bShow);
void* __stdcall SetCapture(void* hWnd);
void* __stdcall SetTimer(void* hWnd, void* nIDEvent, UINT uElapse, void* lpTimerFunc);
BOOL  __stdcall KillTimer(void* hWnd, void* uIDEvent);
int   __stdcall ShowWindow(void* hWnd, int nCmdShow);
BOOL  __stdcall EnableWindow(void* hWnd, BOOL bEnable);
void* __stdcall SetWindowLongA(void* hWnd, int nIndex, LONG dwNewLong);
LONG  __stdcall GetWindowLongA(void* hWnd, int nIndex);
void* __stdcall CreateWindowExA(DWORD dwExStyle, const char* lpClassName,
                                const char* lpWindowName, DWORD dwStyle,
                                int X, int Y, int nWidth, int nHeight,
                                void* hWndParent, void* hMenu,
                                void* hInstance, void* lpParam);
void* __stdcall SetFocus(void* hWnd);
int   __stdcall GetWindowTextA(void* hWnd, char* lpString, int nMaxCount);
int   __stdcall SetWindowTextA(void* hWnd, const char* lpString);
int   __stdcall CallWindowProcA(LONG prevWndFunc, void* hWnd, UINT Msg,
                                void* wParam, void* lParam);
void  __stdcall DefWindowProcA(void* hWnd, UINT Msg, void* wParam, void* lParam);
void  __stdcall GetCursorPos(void* lpPoint);
void* __stdcall WindowFromPoint(void* Point);
BOOL  __stdcall PtInRect(const void* lprc, void* pt);
void  __stdcall CopyRect(void* lprcDst, const void* lprcSrc);
void  __stdcall OffsetRect(void* lprc, int dx, int dy);
BOOL  __stdcall GetClientRect(void* hWnd, void* lpRect);
void  __stdcall InvalidateRect(void* hWnd, const void* lpRect, BOOL bErase);
void* __stdcall BringWindowToTop(void* hWnd);
void* __stdcall PostMessageA(void* hWnd, UINT Msg, void* wParam, void* lParam);
LRESULT __stdcall SendMessageA(void* hWnd, UINT Msg, void* wParam, void* lParam);
void  __stdcall SetRect(void* lprc, int left, int top, int right, int bottom);
void  __stdcall wsprintfA(char* out, const char* fmt, ...);
BOOL  __stdcall PlaySoundA(const char* pszSound, void* hmod, DWORD fdwSound);

} /* extern "C" */
#endif /* _WIN32 */

#ifndef _WIN32
/* Non-Windows: use SDL3 window shim for implemented APIs,
 * plus inline stubs for functions not yet covered */
#include <SDL3/SDL.h>
#include "sdl3_window.h"
#include "sdl3_game_audio.h"
#include "host_test_events.h"

/* sdl3_window.h covers: GetDesktopWindow, GetClientRect, LoadIconA,
 *   CreateSolidBrush, DeleteObject, ShowWindow, EnableWindow,
 *   SetWindowLongA, GetWindowLongA, CreateWindowExA, SetWindowTextA,
 *   DefWindowProcA, PtInRect, SetRect, InvalidateRect, PostMessageA,
 *   PlaySoundA, SetTimer, KillTimer, wsprintfA.
 * Below: stubs for functions NOT in sdl3_window.h */

/* Inline stubs for functions not yet covered by sdl3_window.h */
static inline void* CreateHatchBrush(int, uint32_t) { return NULL; }
static inline void  ReleaseCapture(void) {}
// Win32 decrements a visibility counter on FALSE. Returning -1 after a hide
// preserves the caller's loop termination on the SDL host.
static inline int   ShowCursor(BOOL show) { return show ? 0 : -1; }
static inline void* SetCapture(void*) { return NULL; }
static inline int   GetWindowTextA(void*, char* buf, int max) {
    if (buf && max > 0) buf[0] = 0;
    return 0;
}
static inline int   CallWindowProcA(LONG, void*, UINT, void*, void*) { return 0; }
static inline void* WindowFromPoint(void*) { return NULL; }
static inline void* BringWindowToTop(void*) { return NULL; }
static inline LRESULT SendMessageA(void*, UINT, void*, void*) { return 0; }
#endif /* !_WIN32 */

/* CRT helpers */
void* __cdecl operator_new(size_t size);                    /* 0x465CE0 */
void  __cdecl GLOBAL_free(void* ptr);                       /* 0x465CD0 */

/* Game functions */
void  __fastcall NETMAN_CreateSession(void* panelA);            /* 0x43C8C0 */
void  __fastcall NETMAN_SetGameMode(void* netman, int mode);    /* 0x43CC50 */
void  __thiscall NETMAN_SendPacket(void* netman);               /* 0x43CDF0 */
void  __stdcall WIN32_ResumeThread(void* thread, int mode);     /* 0x466EA0 */
void  __fastcall CGWND_SetMode(void* mode);                     /* 0x408130 */
#ifndef _WIN32
// The host calls the reconstructed C++ implementation, not the legacy
// pointer-typed ABI bridge used by untranslated x86 call sites.
void  CGWND_SetMode(int mode);                                  /* 0x408130 */
#endif
void  __fastcall DDRAW_InitAudio(void);                         /* 0x4014C2 */
int   __thiscall Config_ReadInt(void* ini, const char* section,
                                const char* key, const char* def);  /* 0x448D50 */
void  __thiscall TileMap_Init(void** tilemap, byte flag);           /* 0x458380 */
int   __thiscall ResourceManager_GetStringById(void** mgr, int id); /* 0x460AA0 */
void  __thiscall RESMGR_ReleaseSoundResource(int res);              /* 0x44BB90 */
void  __thiscall RESMGR_LoadSoundResource(int res);                 /* 0x44B8E0 */
void  __fastcall UI_SetWindowVisible(void* self, byte visible);    /* 0x425F20 */
void  __stdcall RESDATA_FreeWindow(PopupWindow* window);           /* 0x460B70 */

void    __fastcall Town_BlitElement(void* src, int sx, int sy,
                                    int sw, int sh, void* dst,
                                    int dx, int dy, int dw, int dh,
                                    int flags);                    /* 0x42B050 */

void    __thiscall UIPANEL_InitSurface(void* surf, int w, int h,
                                       int a, int b, int c);    /* 0x426E40 */
void*   __thiscall UIPANEL_CreateSurface(void* buf);             /* 0x426E10 */
void*   UIPANEL_DestroySurface(UIPANEL_Surface* surface, uint8_t flags); /* 0x42A140 */

/* ================================================================== */
/* External game function references                                    */
/* ================================================================== */

extern void*   g_primary_surface;       /* 0x4FD3C4 */
extern void*   g_scripted_object;       /* 0x4AA5B8 */
extern PlayerConfig* g_player_config;   /* 0x485160 */
extern void*   g_config_ini;            /* 0x485484 */
extern void*   g_resmgr;                /* 0x4855E8 */
extern void**  g_tilemap;               /* 0x4855D0 */
extern int32_t g_screen_width;          /* 0x4AABE8 */
extern int32_t g_screen_height;         /* 0x4AABEC */

extern void*   _g_network_thread;       /* 0x4FD3A0 */
extern char*   _g_netman_state;         /* 0x4FD3A8 */
extern void*   g_netman;                /* 0x4FD3A4 */
extern void*   _g_train;                /* 0x4FD398 */
extern int32_t _g_network_queue;        /* 0x4FD3A0 */

extern void*   g_install_path;          /* 0x485480 */

/* ================================================================== */
/* EditWindow Constructor                                               */
/* Address: 0x4202F0                                                    */
/* ================================================================== */
EditWindow::EditWindow(HINSTANCE hInstance, UINT resourceId) :
    UI_WindowBase(hInstance, resourceId)
{
    /* Set EditWindow vtable */
/* In the binary: sets vtable here. Compiler-managed in natural C++. */

    /* Create GDI brushes */
    this->hbrSolid = CreateSolidBrush(0x5252E7);    /* +0x204 */
    this->hbrHatch = CreateHatchBrush(5, 0x0A5C0A); /* +0x208 */

    /* Initialize all EditWindow-specific fields to 0/NULL */
    this->field_F0       = 0;                        /* +0xF0 */
    this->dialogState    = 0;                        /* +0xE8 */
    this->pPopupWindow   = NULL;                     /* +0x210 */
    this->hasPopup       = 0;                        /* +0xF4 */
    this->previousState  = 0;                        /* +0xEC */
    this->icon           = NULL;                     /* +0xF8 (loaded later in Create) */
    this->spritesLoaded  = 0;                        /* +0x18C */
    this->pPanelA        = NULL;                     /* +0x21C */
    this->pPanelB        = NULL;                     /* +0x220 */

    /* Store global pointer */
    g_editwindow_ptr = this;
}

// Repeated binary sequence (base_destructor, hide, setState(7)).
// PopupWindow models the observed virtual destructor and HWND field.
static void destroy_popup_window(PopupWindow*& popup, LONG saved_wnd_proc)
{
    if (!popup) return;
    SetWindowLongA(popup->hWnd, -4, saved_wnd_proc);
    RESDATA_FreeWindow(popup);
    delete popup;
    popup = nullptr;
}

/* ================================================================== */
/* EditWindow::scalar deleting destructor (vtable[0])                   */
/* Address: 0x4203A0                                                    */
/* ================================================================== */
EditWindow::~EditWindow()
{
    this->base_destructor();
}

/* ================================================================== */
/* EditWindow::base_destructor                                          */
/* Address: 0x4203C0                                                    */
/* ================================================================== */
void EditWindow::base_destructor()
{
    /* Reset vtable for partial destruction */
/* In the binary: sets vtable here. Compiler-managed in natural C++. */

    // These are fully reconstructed UI classes: ordinary C++ ownership
    // replaces the binary scalar-deleting-destructor vtable dispatch.
    delete this->pPanelB;
    this->pPanelB = nullptr;
    delete this->pPanelA;
    this->pPanelA = nullptr;

    /* Delete GDI brushes */
    if (this->hbrSolid != NULL) {
        DeleteObject(this->hbrSolid);            /* +0x204 */
        this->hbrSolid = NULL;
    }
    if (this->hbrHatch != NULL) {
        DeleteObject(this->hbrHatch);            /* +0x208 */
        this->hbrHatch = NULL;
    }

    /* Cleanup sprites if loaded */
    if (this->spritesLoaded) {
        this->cleanupSprites();
    }

    destroy_popup_window(this->pPopupWindow, this->savedPopupWndProc);

    /* Release music resource 0x5015 */
    int musicRes = ResourceManager_GetStringById((void**)&g_resmgr, 0x5015);
    if (musicRes != 0) {
        RESMGR_ReleaseSoundResource(musicRes);
    }

    /* Call UI_WindowBase base destructor */
    this->UI_WindowBase::base_destructor();
}

/* ================================================================== */
/* EditWindow::create -- Creates the full-screen main menu window       */
/* Address: 0x4204D0                                                    */
/* ================================================================== */
int EditWindow::create(HWND hWndParent)
{
    std::fprintf(stderr, "[TRACE] EditWindow::create: begin\n");
    this->hWnd = NULL;

    /* Get desktop window dimensions for full-screen */
    HWND desktop = GetDesktopWindow();
    RECT desktopRect;
    GetClientRect(desktop, &desktopRect);

    /* Load app icon */
    this->icon = LoadIconA(this->hInstance, (const char*)0x65);  /* +0xF8 */

    /* Load sprites */
    std::fprintf(stderr, "[TRACE] EditWindow::create: load sprites\n");
    this->initSprites();
    std::fprintf(stderr, "[TRACE] EditWindow::create: sprites loaded\n");

    /* Create full-screen window */
    std::fprintf(stderr, "[TRACE] EditWindow::create: create window\n");
    int result = UI_WindowBase::create_full_window(
        this, 0, hWndParent,
        desktopRect.left, desktopRect.top,
        desktopRect.right - desktopRect.left,
        desktopRect.bottom - desktopRect.top,
        NULL,
        this->icon,
        0);

    if (result == 0) {
        return 0;
    }

    /* Construct the reconstructed child panels directly. */
    std::fprintf(stderr, "[TRACE] EditWindow::create: create NameEntryPanel\n");
    this->pPanelA = new NameEntryPanel(this->hInstance, 0x1F6);
    if (this->pPanelA) this->pPanelA->create_window(this->hWnd);
    std::fprintf(stderr, "[TRACE] EditWindow::create: create GameSetupPanel\n");
    this->pPanelB = new GameSetupPanel(this->hInstance, 0x1F9);
    if (this->pPanelB) this->pPanelB->create_window(this->hWnd);

    /* Create the player-name edit control (EDIT, WS_CHILD, ID 0x411) */
#ifndef _WIN32
    // UI_MainMenu_Create at 0x4204D0 creates and subclasses this native
    // control. The SDL canvas replacement is composed in hostRenderFrame;
    // do not fabricate a Win32 child window in the host process.
    this->hwndEdit = nullptr;
    this->prevEditWndProc = 0;
#else
    this->hwndEdit = CreateWindowExA(
        0x200,                                  /* WS_EX_CLIENTEDGE */
        "EDIT",
        "",                                     /* empty default text */
        0x40000000,                             /* WS_CHILD */
        this->editBoxRect.left,                 /* +0x15C */
        this->editBoxRect.top,                  /* +0x160 */
        this->editBoxRect.right - this->editBoxRect.left,   /* width */
        this->editBoxRect.bottom - this->editBoxRect.top,   /* height */
        this->hWnd,
        (HMENU)0x411,                           /* control ID */
        this->hInstance,
        NULL);

    /* Set edit control font and limit text length */
    PostMessageA(this->hwndEdit, 0x30, (void*)0x4855F8, 1);  /* WM_SETFONT */
    PostMessageA(this->hwndEdit, 0xC5, (void*)0x0B, 0);     /* EM_LIMITTEXT = 11 */

    /* Subclass edit control's WndProc to 0x420B20 */
    this->prevEditWndProc = SetWindowLongA(this->hwndEdit, -4, 0x420B20);

    /* Set focus to edit control */
    SetFocus(this->hwndEdit);
#endif

    return 1;
}

/* ================================================================== */
/* EditWindow::show -- Shows the main menu (vtable[2])                  */
/* Address: 0x4206B0                                                    */
/* ================================================================== */
void EditWindow::show()
{
    std::fprintf(stderr, "[TRACE] EditWindow::show: begin\n");
    /* Reset state */
    this->previousState = 0;        /* +0xEC */
    this->hasPopup = 0;             /* +0xF4 */

    /* Reload sprites */
    std::fprintf(stderr, "[TRACE] EditWindow::show: reload sprites\n");
    this->initSprites();

    /* Set window visible */
    UI_SetWindowVisible(this, 1);

    /* Initialize network panel */
    std::fprintf(stderr, "[TRACE] EditWindow::show: initialize network panel\n");
    this->netPanelInit();

    /* Create network session on PanelA */
    std::fprintf(stderr, "[TRACE] EditWindow::show: create session\n");
    NETMAN_CreateSession(this->pPanelA);

    /* Call base class Show (creates timer, captures mouse) */
    std::fprintf(stderr, "[TRACE] EditWindow::show: base show\n");
    this->UI_WindowBase::show();
    std::fprintf(stderr, "[TRACE] EditWindow::show: base shown\n");

    /* Bring window to top */
    BringWindowToTop(this->hWnd);
    std::fprintf(stderr, "[TRACE] EditWindow::show: window raised\n");

    /* Hide the OS cursor */
    int cursorVis = ShowCursor(FALSE);
    while (cursorVis >= 0) {
        cursorVis = ShowCursor(FALSE);
    }
    std::fprintf(stderr, "[TRACE] EditWindow::show: cursor hidden\n");

    /* Set focus to edit control and set player name */
#ifndef _WIN32
    // UI_MainMenu_Show (0x4206B0) reads PlayerConfig::name at +0x06.
    // The host has no native child HWND, so copy the same bounded name into
    // its canvas EDIT replacement and select it logically by focusing it.
    this->hostEditText[0] = '\0';
    if (g_player_config != nullptr) {
        const char* source = g_player_config->name;
        size_t length = 0;
        while (length < 11 && source[length] != '\0') {
            this->hostEditText[length] = source[length];
            ++length;
        }
        this->hostEditText[length] = '\0';
    }
    this->hostEditFocused = true;
#else
    SetFocus(this->hwndEdit);
    SetWindowTextA(this->hwndEdit, g_player_config->name);
    /* Send EM_SETSEL (0xB1) -- select end of text (start=0, end=-1) */
    SendMessageA(this->hwndEdit, 0xB1, 0, (void*)-1);
#endif
    std::fprintf(stderr, "[TRACE] EditWindow::show: player name set\n");
    std::fprintf(stderr, "[TRACE] EditWindow::show: edit selection set\n");

    /* Set NetMan game mode based on netman state */
    if (*(char*)(_g_netman_state + 7) == 0) {
        NETMAN_SetGameMode(g_netman, 0);     /* Single-player */
    } else {
        NETMAN_SetGameMode(g_netman, 3);     /* Multiplayer */
    }
    std::fprintf(stderr, "[TRACE] EditWindow::show: network mode set\n");

    /* Transition based on previousState */
    if (this->previousState == 0) {
        /* 0x42073E: inherited vtable[4] with a null surface. */
        this->set_render_surface(nullptr, 0, nullptr, 0, 1);
        ShowWindow(this->hwndEdit, SW_HIDE);
    } else {
        /* Return from game: go to state 7 */
        this->setState(7);
    }

    /* 0x420780 loads resource 0x5015 (sounds\toybox\clstray1) on menu entry. */
#ifdef _WIN32
    int musicRes = ResourceManager_GetStringById((void**)&g_resmgr, 0x5015);
    if (musicRes != 0) {
        RESMGR_LoadSoundResource(musicRes);
    }
#else
    // Host-only typed equivalent; it preserves the original preload without
    // routing through the 32-bit DirectSound resource-object ABI.
    SDL3_GameAudioPreloadResource(0x5015);
#endif
}

/* ================================================================== */
/* EditWindow::hide -- Hides the main menu (vtable[1])                  */
/* Address: 0x420860                                                    */
/* ================================================================== */
void EditWindow::hide()
{
    /* Call base class Hide */
    this->UI_WindowBase::hide();

    /* Clear popup flag */
    this->hasPopup = 0;                              /* +0xF4 */

    destroy_popup_window(this->pPopupWindow, this->savedPopupWndProc);

    /* Set state to hidden */
    this->setState(1);

    /* Cleanup sprites */
    this->cleanupSprites();

    /* Restore focus to main CGWND window */
    // `g_main_window` is declared as an opaque HWND in types.h, but the
    // binary stores a CGWND object pointer at 0x4AA4A0.
    HWND mainHwnd = static_cast<CGWND*>(g_main_window)->hWnd;  /* +0x08 */
    SetFocus(mainHwnd);
    InvalidateRect(mainHwnd, NULL, FALSE);
}

/* ================================================================== */
/* EditWindow::setState -- Main menu state machine                      */
/* Address: 0x4208F0                                                    */
/* ================================================================== */
void EditWindow::setState(int32_t state)
{
    const int32_t previous_state = this->dialogState;
    this->dialogState = state;

    switch (state) {
    case 1:
        PlaySoundA(nullptr, nullptr, 0);
        ShowWindow(this->hwndEdit, SW_HIDE);
        return;
    case 2:
        ShowWindow(this->hwndEdit, SW_HIDE);
        this->pPanelA->show();
        if (previous_state == 4 || previous_state == 5) this->pPanelB->hide();
        return;
    case 3:
        this->pPanelA->hide();
        if (*(char*)(_g_netman_state + 0x18) == 0) {
            this->dialogState = 4;
        } else {
            this->dialogState = 5;
        }
        this->pPanelB->show();
        return;
    case 4:
    case 5:
        ShowWindow(this->hwndEdit, SW_HIDE);
        this->pPanelB->show();
        return;
    case 6:
        this->pPanelB->hide();
        this->pPanelA->hide();
        if (*(char*)(_g_netman_state + 7) != 0) NETMAN_SetGameMode(g_netman, 1);
        WIN32_ResumeThread(_g_network_thread, 1);
        CGWND_SetMode((void*)1);
        return;
    case 7:
        if (this->pPopupWindow) {
            destroy_popup_window(this->pPopupWindow, this->savedPopupWndProc);
            this->previousState = 99;
        }
        if (previous_state == 0) DDRAW_InitAudio();
        if (previous_state == 0 || previous_state == 1) {
            char path[1284];
            wsprintfA(path, "%s\\video\\music.wav", &g_install_path);
            PlaySoundA(path, nullptr, 9);
        }
        this->pPanelB->hide();
        this->pPanelA->hide();
        return;
    default:
        return;
    }
}

/* ================================================================== */
/* EditWindow::HandleClick -- Compute button RECTs and centering offsets*/
/* Address: 0x421200 (vtable[7])                                        */
/* ================================================================== */
void EditWindow::HandleClick()
{
    /* Update client rect */
    this->UI_WindowBase::on_create();

    /* Only proceed if sprites are loaded */
    if (!this->spritesLoaded) {
        return;
    }

    /* Compute centering offsets from surface vs screen size */
    if (this->pMainSurface != NULL) {
        const int32_t surfW = this->pMainSurface->width;    /* +0x08 */
        const int32_t surfH = this->pMainSurface->height;   /* +0x0C */

        this->centerOffsetX = (surfW - g_screen_width) / 2;
        this->centerOffsetY = (surfH - g_screen_height) / 2;
        this->surfaceRight  = g_screen_width + this->centerOffsetX;
        this->surfaceBottom = g_screen_height + this->centerOffsetY;
    }

    /* --- Compute Option Button 1 RECT (+0x13C) --- */
    {
        SetRect(&this->btnOption1Rect, 0x387, 0x2A5, 0, 0);
        this->btnOption1Rect.right  = this->btnOption1Rect.left +
            loco::assets::sprite_width(this->sprite_405.resource);
        this->btnOption1Rect.bottom = this->btnOption1Rect.top +
            loco::assets::sprite_height(this->sprite_405.resource);
        OffsetRect(&this->btnOption1Rect,
                   -this->centerOffsetX, -this->centerOffsetY);
    }

    /* --- Compute Option Button 2 RECT (+0x14C) --- */
    {
        SetRect(&this->btnOption2Rect, 0x18B, 0x2A5, 0, 0);
        this->btnOption2Rect.right = this->btnOption2Rect.left +
            loco::assets::sprite_width(this->sprite_403.resource);
        this->btnOption2Rect.bottom = this->btnOption2Rect.top +
            loco::assets::sprite_height(this->sprite_403.resource);
        OffsetRect(&this->btnOption2Rect,
                   -this->centerOffsetX, -this->centerOffsetY);
    }

    /* --- Compute Play Button RECT (+0xFC) --- */
    {
        SetRect(&this->btnPlayRect, 0x212, 0x1EA, 0, 0);
        this->btnPlayRect.right = this->btnPlayRect.left +
            loco::assets::sprite_width(this->sprite_407.resource);
        this->btnPlayRect.bottom = this->btnPlayRect.top +
            loco::assets::sprite_height(this->sprite_407.resource);
        OffsetRect(&this->btnPlayRect,
                   -this->centerOffsetX, -this->centerOffsetY);
    }

    /* --- Compute Scenario Button RECT (+0x10C) --- */
    {
        SetRect(&this->btnScenarioRect, 0x2C9, 0x1EA, 0, 0);
        this->btnScenarioRect.right = this->btnScenarioRect.left +
            loco::assets::sprite_width(this->sprite_409.resource);
        this->btnScenarioRect.bottom = this->btnScenarioRect.top +
            loco::assets::sprite_height(this->sprite_409.resource);
        OffsetRect(&this->btnScenarioRect,
                   -this->centerOffsetX, -this->centerOffsetY);
    }

    /* --- Compute Exit Button RECT (+0x11C) --- */
    {
        SetRect(&this->btnExitRect, 0x387, 0x1BD, 0, 0);
        this->btnExitRect.right = this->btnExitRect.left +
            loco::assets::sprite_width(this->sprite_40B.resource);
        this->btnExitRect.bottom = this->btnExitRect.top +
            loco::assets::sprite_height(this->sprite_40B.resource);
        OffsetRect(&this->btnExitRect,
                   -this->centerOffsetX, -this->centerOffsetY);
    }

    /* --- Compute Text Button RECT (+0x12C) --- */
    {
        SetRect(&this->btnTextRect, 0x387, 0x231, 0, 0);
        this->btnTextRect.right = this->btnTextRect.left +
            loco::assets::sprite_width(this->sprite_40E.resource);
        this->btnTextRect.bottom = this->btnTextRect.top +
            loco::assets::sprite_height(this->sprite_40E.resource);
        OffsetRect(&this->btnTextRect,
                   -this->centerOffsetX, -this->centerOffsetY);
    }

    /* --- Compute Backdrop RECT (+0x17C) --- */
    SetRect(&this->backdropRect, 300, 0xAC, 0x3D4, 0x354);
    OffsetRect(&this->backdropRect, -this->centerOffsetX, -this->centerOffsetY);

    /* --- Compute Edit Box RECT (+0x15C) --- */
    SetRect(&this->editBoxRect, 0x232, 0x2CC, 0x34D, 0x2ED);
    OffsetRect(&this->editBoxRect, -this->centerOffsetX, -this->centerOffsetY);
}

/* ================================================================== */
/* EditWindow::netPanelWndProc -- Network panel WndProc (vtable[20])   */
/* Address: 0x422D80                                                    */
/* ================================================================== */
int EditWindow::netPanelWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    /* If not yet initialized, delegate to UIPANEL_WindowProc */
    if (this->field_14 == NULL) {  /* pInitGuard at +0x14 */
        // 0x422D9D passes the EditWindow instance in ECX before the four
        // WndProc stack arguments.  The prior four-argument declaration
        // dropped that required receiver.
        extern LRESULT __thiscall UIPANEL_WindowProc(void*, HWND, UINT, WPARAM, LPARAM);
        return static_cast<int>(UIPANEL_WindowProc(this, hwnd, msg, wParam, lParam));
    }

    /* Extract mouse coordinates from lParam */
    int mouseY = (lParam >> 16) & 0xFFFF;
    int mouseX = lParam & 0xFFFF;

    POINT32 pt = { mouseX, mouseY };

    /* Original hit-test order and state gates (0x422DC6..0x422E4B). */
    if (PtInRect(&this->btnOption1Rect, &pt))
        goto highlight_btn;
    if (PtInRect(&this->btnOption2Rect, &pt))
        goto highlight_btn;

    /* Play is active only for single-player. */
    if (PtInRect(&this->btnPlayRect, &pt) && _g_netman_state[7] == 0)
        goto highlight_btn;

    /* Scenario is active only for multiplayer when scenario data exists. */
    if (PtInRect(&this->btnScenarioRect, &pt) &&
        *reinterpret_cast<const int32_t*>(_g_netman_state + 0x10) != 0 &&
        _g_netman_state[7] != 0)
        goto highlight_btn;

    /* The two lower controls are mutually exclusive by alternate-menu state. */
    if (PtInRect(&this->btnExitRect, &pt) && _g_netman_state[8] == 0)
        goto highlight_btn;
    if (PtInRect(&this->btnTextRect, &pt) && _g_netman_state[8] != 0)
        goto highlight_btn;

    /* No button hit -- restore the normal animation through vtable[3]. */
    this->set_mode(this->childCount0, this->childObj0, 0, 1);
    return 0;

highlight_btn:
    /* Button highlighted -- select the highlighted animation. */
    this->set_mode(this->childCount1, this->childObj1, 0, 1);
    return 0;
}

/* ================================================================== */
/* EditWindow::onPlayerNameChanged -- Handles player name edit box      */
/* Address: 0x422660                                                    */
/* ================================================================== */
void EditWindow::onPlayerNameChanged()
{
#ifdef _WIN32
    // Preserve the original x86 stack layout; its 0x10-byte local allocation
    // supplies the 13th byte accepted by GetWindowTextA at 0x422680.
    char nameBuf[12] = {};
#else
    // The SDL host has no adjacent x86 spill slot. Match GetWindowTextA's
    // cchMax=13 contract with a real 13-byte local buffer, preventing a
    // terminating NUL from corrupting the host frame.
    char nameBuf[13] = {};
#endif

    /* 0x422667..0x422675 pushes (nullptr, 0, nullptr, false, true). */
    this->set_render_surface(nullptr, 0, nullptr, 0, 1);

    /* Read the edited name from the edit control (max 12 chars + null = 13) */
#ifdef _WIN32
    GetWindowTextA(this->hwndEdit, nameBuf, 13);
#else
    // Host path: read from the SDL-backed text buffer (same 11-char limit as
    // the native EDIT control's EM_LIMITTEXT at 0x420A56).
    std::memcpy(nameBuf, this->hostEditText, sizeof(this->hostEditText));
    nameBuf[sizeof(this->hostEditText) - 1] = '\0';
#endif

    /* Check for empty name (strlen-like countdown from -1) */
    int len = -1;
    char* p = nameBuf;
    while (*p++ != 0) len--;
    if (len == -2) {
        return;     /* empty string */
    }

    /* Reject names containing illegal characters */
    if (strpbrk(nameBuf, ",. /\\[]{}|!@#$%^&*()_+-=~`'\"<>?;:") != NULL) {
        return;
    }

    /* Require at least one alphabetic character */
    if (strpbrk(nameBuf, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ") == NULL) {
        return;
    }

    /* Save name to PlayerConfig */
    g_player_config->SetName(nameBuf);
    g_player_config->Save();

    /* Read screen mode setting (discard result, normalizes formatting) */
    Config_ReadInt(g_config_ini, "Control", "ScreenMode", nameBuf);

    /* Restore the name from config (normalizes formatting) */
    SetWindowTextA(this->hwndEdit, g_player_config->name);
#ifndef _WIN32
    // Mirror the canonical config name back into the host edit buffer so
    // subsequent hostRenderFrame frames draw the normalized text.
    std::memcpy(this->hostEditText, g_player_config->name,
                sizeof(this->hostEditText));
    this->hostEditText[sizeof(this->hostEditText) - 1] = '\0';
#endif

    /* Send network packet */
    NETMAN_SendPacket(_g_netman_state);

    /* Transition based on network state */
    if (*(char*)(_g_netman_state + 7) == 0) {
        /* Single-player mode */
        TileMap_Init(&g_tilemap, 1);

        if (*(char*)(_g_netman_state + 8) == 0) {
            /* Standard single-player or scenario */
            if (*(char*)(_g_netman_state + 0x24) != 0) {
                const int gameMode = *(int*)(_g_netman_state + 0x28);
                if ((gameMode == 4 && this->pPanelA->field_1E0 != 0) ||
                    (gameMode == 2 && this->pPanelA->field_1E1 != 0)) {
                    this->setState(4);     /* Single-player */
                    return;
                }
            }
        } else {
            /* Scenario select screen */
            if (*(char*)(_g_netman_state + 0x18) != 0) {
                const int gameMode = *(int*)(_g_netman_state + 0x1C);
                if ((gameMode == 4 && this->pPanelA->field_1E0 != 0) ||
                    (gameMode == 2 && this->pPanelA->field_1E1 != 0)) {
                    this->setState(5);     /* Multiplayer */
                    return;
                }
            }
        }

        /* Default: go to loading state */
        this->setState(2);
        return;
    }

    /* Multiplayer mode */
    TileMap_Init(&g_tilemap, 0);
    if (*(char*)(_g_netman_state + 7) != 0) {
        NETMAN_SetGameMode(g_netman, 1);
    }

    WIN32_ResumeThread(_g_network_thread, 1);
    CGWND_SetMode((void*)1);
}

/* ================================================================== */
/* EditWindow::netPanelInit -- Initialize network subsystem              */
/* Address: 0x422820                                                    */
/* ================================================================== */
void EditWindow::netPanelInit()
{
    std::fprintf(stderr, "[TRACE] EditWindow::netPanelInit: config=%p train=%p\n", static_cast<void*>(_g_netman_state), _g_train);
    /* Set polling interval based on game mode */
    if (*(char*)(_g_netman_state + 7) == 0) {
        *(int*)(_g_netman_state + 0x0C) = 0x1E;    /* 30ms for single-player */
    } else {
        *(int*)(_g_netman_state + 0x0C) = 0x32;    /* 50ms for multiplayer */
    }

    /* Skip if train subsystem already exists */
    if (_g_train != NULL) {
        return;
    }

    /* Zero the network queue flag */
    _g_network_queue = 0;

    /* Create TrainSubsystem (0x38 bytes in the original x86 object). */
#ifdef _WIN32
    // TODO: transcribe the original thread-creation path below after the
    // DirectPlay platform layer is available on this target.
#else
    // TrainSubsystem_Ctor @ 0x438BC0 receives the two panel context values.
    // C++ new performs the allocation + constructor sequence without a raw
    // function-pointer call; DirectPlay enumeration is isolated in its host
    // platform branch in Train_network.cpp.
    _g_train = new TrainSubsystem(
        static_cast<int>(this->resourceId),
        static_cast<int>(reinterpret_cast<intptr_t>(this->hWndParent)));
    _g_network_thread = NULL;
    return;
#endif

    /* Create network thread (0x41C bytes) */
    typedef void* (__thiscall* WIN32CreateThread)(void* buf);
    extern WIN32CreateThread WIN32_CreateThread;    /* 0x466DC0 */

    void* threadBuf = operator_new(0x41C);
    if (threadBuf != NULL) {
        _g_network_thread = WIN32_CreateThread(threadBuf);
    } else {
        _g_network_thread = NULL;
    }

    /* Queue async task: Train_ProcessMessages */
    extern int __stdcall WIN32_QueueAsyncTask(void* thread, void* func, void* param);
    extern void Train_ProcessMessages(void* train);    /* 0x439240 */

    int result = WIN32_QueueAsyncTask(_g_network_thread,
                                       (void*)&Train_ProcessMessages,
                                       _g_train);
    if (result != 1) {
        /* 0x4228ED dispatches the base no-op failure hook (vtable[5]). */
        this->on_async_task_failure(0);
        return;
    }

    /* Start the thread */
    WIN32_ResumeThread(_g_network_thread, 1);
}

/* ================================================================== */
/* EditWindow::initSprites -- Load 12 menu button resources             */
/* Address: 0x421500                                                    */
/* ================================================================== */
void EditWindow::initSprites()
{
    if (this->spritesLoaded) return;

    const auto load_sprite = [](MenuSpriteSlot& slot, uint32_t resource_id) {
        slot.resource = loco::assets::host_resource_manager().get_sprite_by_id(resource_id);
        slot.bitmap = loco::assets::sprite_bitmap(slot.resource);
    };
    load_sprite(this->sprite_403, 0x403);
    load_sprite(this->sprite_404, 0x404);
    load_sprite(this->sprite_405, 0x405);
    load_sprite(this->sprite_406, 0x406);
    load_sprite(this->sprite_407, 0x407);
    load_sprite(this->sprite_408, 0x408);
    load_sprite(this->sprite_409, 0x409);
    load_sprite(this->sprite_40A, 0x40A);
    load_sprite(this->sprite_40B, 0x40B);
    load_sprite(this->sprite_40C, 0x40C);
    load_sprite(this->sprite_40E, 0x40E);
    load_sprite(this->sprite_40F, 0x40F);

    this->render();
    this->spritesLoaded = 1;
}

/* ================================================================== */
/* EditWindow::cleanupSprites -- Release all 12 sprites and surface     */
/* Address: 0x421AE0                                                    */
/* ================================================================== */
void EditWindow::cleanupSprites()
{
    if (!this->spritesLoaded) return;

    const auto release_sprite = [](MenuSpriteSlot& slot) {
        loco::assets::release_sprite(slot.resource);
        slot.resource = nullptr;
        slot.bitmap = nullptr;
    };
    release_sprite(this->sprite_403);
    release_sprite(this->sprite_404);
    release_sprite(this->sprite_405);
    release_sprite(this->sprite_406);
    release_sprite(this->sprite_407);
    release_sprite(this->sprite_408);
    release_sprite(this->sprite_409);
    release_sprite(this->sprite_40A);
    release_sprite(this->sprite_40B);
    release_sprite(this->sprite_40C);
    release_sprite(this->sprite_40E);
    release_sprite(this->sprite_40F);

    if (this->pMainSurface) UIPANEL_DestroySurface(this->pMainSurface, 1);
    this->pMainSurface = nullptr;
    this->spritesLoaded = 0;
}

/* ================================================================== */
/* EditWindow::render -- Composite menu backdrop onto offscreen surface */
/* Address: 0x4216F0                                                    */
/* ================================================================== */
void EditWindow::render()
{
#ifndef _WIN32
    // 0x421758..0x421ABF loads and blits these five resources, in this
    // exact order, into a 1280x1024 menu surface. The host has no safe
    // x86 UIPANEL_Surface/COM adapter, so its typed primary target is the
    // composition boundary. Presentation remains the caller's job, just as
    // the original function only prepares its offscreen surface.
    if (!SDL3_ClearPrimarySurface(0x002850)) return;

    struct BackdropElement {
        uint32_t resource_id;
        int x;
        int y;
    };
    constexpr BackdropElement backdrop[] = {
        {0x413, 0x000, 0x000},
        {0x444, 0x0F4, 0x1D6},
        {0x445, 0x204, 0x0F9},
        {0x446, 0x11A, 0x0F0},
        {0x443, 0x20B, 0x2A8},
    };

    for (const BackdropElement& element : backdrop) {
        auto* resource = loco::assets::host_resource_manager().get_sprite_by_id(element.resource_id);
        auto* bitmap = loco::assets::sprite_bitmap(resource);
        const bool blitted = bitmap != nullptr &&
            SDL3_BlitSurfaceToPrimary(loco::assets::bitmap_surface(bitmap), element.x, element.y);
        loco::assets::release_sprite(resource);
        if (!blitted) return;
    }
#else
    void* surface_memory = operator_new(0x20);
    this->pMainSurface = surface_memory
        ? static_cast<UIPANEL_Surface*>(UIPANEL_CreateSurface(surface_memory)) : nullptr;
    if (this->pMainSurface) UIPANEL_InitSurface(this->pMainSurface, 0x500, 0x400, 1, 0, 0);

    const auto blit_backdrop = [this](uint32_t resource_id, int x, int y) {
        auto* resource = loco::assets::host_resource_manager().get_sprite_by_id(resource_id);
        auto* bitmap = loco::assets::sprite_bitmap(resource);
        if (bitmap) {
#ifndef _WIN32
            SDL3_BlitSurfaceToPrimary(loco::assets::bitmap_surface(bitmap), x, y);
#else
            const int width = static_cast<int>(loco::assets::bitmap_width(bitmap));
            const int height = static_cast<int>(loco::assets::bitmap_height(bitmap));
            Town_BlitElement(bitmap, 0, 0, width, height, this->pMainSurface, x, y, width, height, 0);
#endif
        }
        loco::assets::release_sprite(resource);
    };
    blit_backdrop(0x413, 0, 0);
    blit_backdrop(0x444, 0xF4, 0x1D6);
    blit_backdrop(0x445, 0x204, 0xF9);
    blit_backdrop(0x446, 0x11A, 0xF0);
    blit_backdrop(0x443, 0x20B, 0x2A8);
#endif
}

#ifndef _WIN32
namespace {
enum HostMenuButton {
    kHostNoButton = -1,
    kHostOptionOne,
    kHostQuit,
    kHostSinglePlayer,
    kHostMultiplayer,
    kHostGame,
    kHostJoinGame,
};

bool host_point_in_rect(const RECT& rect, float x, float y)
{
    // Win32 PtInRect uses left/top inclusive and right/bottom exclusive edges.
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

void host_blit_menu_sprite(const MenuSpriteSlot& slot, const RECT& destination)
{
    if (!slot.bitmap) return;
    SDL3_BlitSurfaceToPrimary(loco::assets::bitmap_surface(slot.bitmap),
                              destination.left, destination.top);
}

void host_set_menu_rects(EditWindow& menu)
{
    // EditWindow_HandleClick (0x421200) derives each size from the loaded
    // resource and then subtracts the source-surface crop offset. The SDL
    // host always renders the whole logical surface, so that offset is zero.
    const auto set_sprite_rect = [](RECT& rect, int left, int top,
                                    const MenuSpriteSlot& sprite) {
        rect.left = left;
        rect.top = top;
        rect.right = left + static_cast<int>(loco::assets::sprite_width(sprite.resource));
        rect.bottom = top + static_cast<int>(loco::assets::sprite_height(sprite.resource));
    };
    set_sprite_rect(menu.btnOption1Rect, 0x387, 0x2A5, menu.sprite_403);
    set_sprite_rect(menu.btnOption2Rect, 0x18B, 0x2A5, menu.sprite_405);
    set_sprite_rect(menu.btnPlayRect, 0x212, 0x1EA, menu.sprite_407);
    set_sprite_rect(menu.btnScenarioRect, 0x2C9, 0x1EA, menu.sprite_409);
    set_sprite_rect(menu.btnExitRect, 0x387, 0x1BD, menu.sprite_40B);
    set_sprite_rect(menu.btnTextRect, 0x387, 0x231, menu.sprite_40E);

    // 0x4214BA..0x4214EA: SetRect(0x232, 0x2CC, 0x34D, 0x2ED), then
    // OffsetRect by the negative source-surface crop. The host crop is zero.
    menu.editBoxRect = {0x232, 0x2CC, 0x34D, 0x2ED};
}

HostMenuButton host_button_at(const EditWindow& menu, float x, float y)
{
    // Exact enabled-control ordering from EditWindow_netPanelWndProc
    // (0x422D80). Keep menu composition and availability tied to the same
    // recovered configuration fields as the known-good host revision.
    const char* const state = _g_netman_state;
    const bool multiplayer = state && state[7] != 0;
    const bool alternate_menu = state && state[8] != 0;
    const bool has_scenario = state && *reinterpret_cast<const int32_t*>(state + 0x10) != 0;

    if (host_point_in_rect(menu.btnOption1Rect, x, y)) return kHostOptionOne;
    if (host_point_in_rect(menu.btnOption2Rect, x, y)) return kHostQuit;
    if (!multiplayer && host_point_in_rect(menu.btnPlayRect, x, y)) return kHostSinglePlayer;
    if (multiplayer && has_scenario && host_point_in_rect(menu.btnScenarioRect, x, y)) return kHostMultiplayer;
    if (!multiplayer && !alternate_menu && host_point_in_rect(menu.btnExitRect, x, y)) return kHostGame;
    if (!multiplayer && alternate_menu && host_point_in_rect(menu.btnTextRect, x, y)) return kHostJoinGame;
    return kHostNoButton;
}
}  // namespace

/**
 * Host SDL frame composition.
 *
 * Assembly basis: the state-0/state-7 branch at 0x421C31 composes the
 * prepared 0x500x0x400 backdrop, invokes EditWindow_drawButtons (0x422010),
 * and overlays resources 0x403 and 0x405 in the two option rectangles.
 * The host re-composes the fixed canvas rather than performing unsafe x86
 * surface blits; this method is excluded from the original Windows build.
 */
void EditWindow::hostRenderFrame()
{
    if (!this->visible || !this->spritesLoaded) return;

    // States 3 (check-config), 4 (single-player), and 5 (network) show
    // GameSetupPanel after UI_MainMenu_SetState (0x4208F0) hides the edit
    // control. The SDL composition is intentionally isolated from the
    // original Win32 executable path.
    if (this->dialogState == 3 || this->dialogState == 4 ||
        this->dialogState == 5) {
#ifdef _WIN32
        // The original executable continues through the UIPANEL/GDI path.
        return;
#else
        if (this->pPanelB != nullptr) this->pPanelB->hostRenderFrame();
        return;
#endif
    }

    // States 0 (initial) and 7 (return-from-game) compose the main menu.
    if (this->dialogState != 0 && this->dialogState != 7) return;

    host_set_menu_rects(*this);
    this->render();

    const char* const state = _g_netman_state;
    const bool multiplayer = state && state[7] != 0;
    const bool alternate_menu = state && state[8] != 0;
    const bool has_scenario = state && *reinterpret_cast<const int32_t*>(state + 0x10) != 0;

    // Original 0x421C9B -> 0x422010 selection and visibility branches.
    // In particular, retain the known-good right-hand 0x409 path rather than
    // replacing it with a synthetic always-visible menu.
    if (multiplayer) {
        host_blit_menu_sprite(this->sprite_408, this->btnPlayRect);
        if (has_scenario) host_blit_menu_sprite(this->sprite_409, this->btnScenarioRect);
    } else {
        host_blit_menu_sprite(this->sprite_407, this->btnPlayRect);
        if (has_scenario) host_blit_menu_sprite(this->sprite_40A, this->btnScenarioRect);
        host_blit_menu_sprite(alternate_menu ? this->sprite_40C : this->sprite_40B,
                              this->btnExitRect);
        host_blit_menu_sprite(alternate_menu ? this->sprite_40E : this->sprite_40F,
                              this->btnTextRect);
    }

    // 0x42298A and 0x422AC3 use 0x404 and 0x406 while the corresponding
    // action is pressed. Unlike selection toggles, these actions call
    // Sleep(0x96), so retain their artwork until host completion below.
    const int active_button = this->hostPressedButton != kHostNoButton
        ? this->hostPressedButton : this->hostHoveredButton;
    host_blit_menu_sprite(active_button == kHostOptionOne ? this->sprite_404 : this->sprite_403,
                          this->btnOption1Rect);
    host_blit_menu_sprite(active_button == kHostQuit ? this->sprite_406 : this->sprite_405,
                          this->btnOption2Rect);

    // UI_MainMenu_Create (0x4204D0) owns a native EDIT child at this RECT.
    // The SDL host composes its equivalent into the same logical canvas.
    SDL3_DrawPrimaryTextInput(this->editBoxRect.left, this->editBoxRect.top,
                              this->editBoxRect.right, this->editBoxRect.bottom,
                              this->hostEditText, this->hostEditFocused);

    // 0x422A72..0x422AB2 and 0x422BCD..0x422C4C play click resource
    // 0x5015, expose their pressed artwork, then Sleep(0x96). Complete the
    // same action after the SDL frame has presented that artwork.
    const int pressed_button = this->hostPressedButton;
    if (pressed_button != kHostNoButton &&
        SDL_GetTicks() >= this->hostPressedUntilMs) {
        this->hostPressedButton = kHostNoButton;
        this->hostPressedUntilMs = 0;
        this->hostHoveredButton = kHostNoButton;
        if (pressed_button == kHostOptionOne) {
            this->hostCommitPlayerName();
        } else if (pressed_button == kHostQuit) {
            CGWND_SetMode(10);
        }
    }
}

/**
 * Host pointer adapter for EditWindow_netPanelWndProc (0x422D80) and the
 * click state changes at 0x422C60..0x422D2E. SDL display coordinates are
 * inverted through the same canvas projection used by SDL3_PresentPrimarySurface.
 */
void EditWindow::hostHandlePointer(float display_x, float display_y, bool pressed)
{
    // GAMESTATE_HandleClick (0x40A4E0) owns mouse input while states 3/4/5
    // show GameSetupPanel.  Do not send those events through the dormant
    // main-menu hit-test rectangles.
    if (this->dialogState == 3 || this->dialogState == 4 ||
        this->dialogState == 5) {
        if (this->pPanelB != nullptr) {
            this->pPanelB->hostHandlePointer(display_x, display_y, pressed);
        }
        return;
    }

    float canvas_x = 0.0f;
    float canvas_y = 0.0f;
    const HostMenuButton button = SDL3_DisplayToPrimaryCanvas(display_x, display_y,
                                                                &canvas_x, &canvas_y)
        ? host_button_at(*this, canvas_x, canvas_y) : kHostNoButton;
    this->hostHoveredButton = button;
    if (!pressed) return;

    if (host_point_in_rect(this->editBoxRect, canvas_x, canvas_y)) {
        this->hostEditFocused = true;
        this->hostHoveredButton = kHostNoButton;
        return;
    }
    this->hostEditFocused = false;

    // The option controls are the two delayed button paths. 0x42298A and
    // 0x422AC3 restore their background, draw 0x404/0x406, play 0x5015,
    // repaint, and Sleep(0x96) before accepting or invoking mode 10.
    if (button == kHostOptionOne || button == kHostQuit) {
        if (this->hostPressedButton == kHostNoButton) {
            SDL3_GameAudioPlayResource(0x5015);
            this->hostPressedButton = button;
            this->hostPressedUntilMs = SDL_GetTicks() + 150;
        }
        return;
    }

    // 0x422C60..0x422D66 is a mode selector, not a direct game-start
    // action. Resource strings identify 0x407/0x408 as singleup/singledown
    // and 0x409/0x40A as multipleup/multipledown. The byte at DPlayConfig+7
    // is therefore the selected-single-player flag: 1 after the left click,
    // 0 after the enabled right click.
    bool changed = false;
    switch (button) {
    case kHostSinglePlayer:
        _g_netman_state[7] = 1;
        NETMAN_SetGameMode(g_netman, 3);
        changed = true;
        break;
    case kHostMultiplayer:
        _g_netman_state[7] = 0;
        NETMAN_SetGameMode(g_netman, 0);
        changed = true;
        break;
    case kHostGame:
        // 0x40B/0x40C are hostup/hostdown; DPlayConfig+8 is the
        // selected-host-game flag used by the subsequent Go action.
        _g_netman_state[8] = 1;
        changed = true;
        break;
    case kHostJoinGame:
        // 0x40E/0x40F are joinup/joindown and clear the host selection.
        _g_netman_state[8] = 0;
        changed = true;
        break;
    default:
        break;  // Popup-producing option handlers remain on the Win32 path.
    }

    // The original handler has no Sleep on this path. Draw the changed sprite
    // selection on the next SDL frame and play the same click resource at
    // 0x422D57.
    if (changed) SDL3_GameAudioPlayResource(0x5015);
}

void EditWindow::hostCommitPlayerName()
{
    const char* const illegal = ",. /\\[]{}|!@#$%^&*()_+-=~`'\"<>?;:";
    bool has_alpha = false;
    bool legal = this->hostEditText[0] != '\0';
    for (const char* ch = this->hostEditText; legal && *ch; ++ch) {
        if (std::strchr(illegal, *ch) != nullptr) legal = false;
        if ((*ch >= 'a' && *ch <= 'z') || (*ch >= 'A' && *ch <= 'Z')) has_alpha = true;
    }

    // The original click branch calls OnPlayerNameChanged at 0x422AB2; the
    // edit-subclass Enter branch reaches the same commit flow at 0x420D57.
    // Preserve the recovered validation while avoiding the unported 32-bit
    // mode-1 bootstrap used by the native handler.
    if (legal && has_alpha && g_player_config != nullptr) {
        std::memcpy(g_player_config->name, this->hostEditText,
                    sizeof(this->hostEditText));
        loco::host_test::emit_player_name_committed(g_player_config->name);
        // 0x422722 branches on DPlayConfig+7. With the selected-single flag
        // set, the original takes the local startup path at 0x4227DA and
        // never enters the multiplayer panel. The SDL host cannot yet run
        // that full game-start path, so its guarded presentation fallback is
        // the existing single-player setup (state 4). The cleared flag is
        // the multiplayer selection and maps to the host network lobby
        // (state 5).
        this->setState(_g_netman_state[7] != 0 ? 4 : 5);
    }
}

bool EditWindow::hostHandleKey(int32_t key_code)
{
    if (!this->hostEditFocused) return false;

    // The unlabelled edit subclass at 0x420B20 forwards only Enter and
    // Escape WM_KEYDOWN messages to the parent. Its parent handler at
    // 0x420D57 commits the name for Enter; 0x420C19 takes the quit path
    // for Escape. Backspace is handled by the native EDIT default proc.
    if (key_code == 13) {
        this->hostCommitPlayerName();
        return true;
    }
    if (key_code == 27) {
        CGWND_SetMode(10);
        return true;
    }
    if (key_code == 8) {
        const size_t length = std::strlen(this->hostEditText);
        if (length != 0) this->hostEditText[length - 1] = '\0';
        return true;
    }
    return false;
}

void EditWindow::hostHandleTextInput(const char* utf8_text)
{
    if (!this->hostEditFocused || !utf8_text) return;

    size_t length = std::strlen(this->hostEditText);
    // The native ANSI EDIT control uses EM_LIMITTEXT(11). Preserve that byte
    // bound and retain only printable single-byte input accepted by its font.
    for (const unsigned char* ch = reinterpret_cast<const unsigned char*>(utf8_text);
         *ch != '\0' && length < 11; ++ch) {
        if (*ch >= 0x20 && *ch < 0x7f) this->hostEditText[length++] = static_cast<char>(*ch);
    }
    this->hostEditText[length] = '\0';
}
#endif  // !_WIN32

/* ================================================================== */
/* EditWindow::drawButtons -- Draw all main menu buttons                */
/* Address: 0x422010                                                    */
/* ================================================================== */
void EditWindow::drawButtons()
{
    // 0x422010 selects exactly these resources from the DPlay state bytes.
    const auto blit_button = [this](const MenuSpriteSlot& sprite, RECT& destination) {
        this->updateButton(&destination);
        if (sprite.bitmap == nullptr) return;

        const int width = static_cast<int>(loco::assets::sprite_width(sprite.resource));
        const int height = static_cast<int>(loco::assets::sprite_height(sprite.resource));
        Town_BlitElement(sprite.bitmap,
                         destination.left, destination.top,
                         destination.right, destination.bottom,
                         g_primary_surface, 0, 0, width, height, 0);
    };

    if (_g_netman_state[7] == 0) {
        blit_button(this->sprite_407, this->btnPlayRect);
        if (*reinterpret_cast<const int32_t*>(_g_netman_state + 0x10) != 0) {
            blit_button(this->sprite_40A, this->btnScenarioRect);
        }

        if (_g_netman_state[8] == 0) {
            blit_button(this->sprite_40B, this->btnExitRect);
            this->drawText(&this->btnTextRect, 0,
                           this->sprite_40F.resource, this->sprite_40F.bitmap);
            return;
        }

        blit_button(this->sprite_40C, this->btnExitRect);
        blit_button(this->sprite_40E, this->btnTextRect);
        return;
    }

    blit_button(this->sprite_408, this->btnPlayRect);
    if (*reinterpret_cast<const int32_t*>(_g_netman_state + 0x10) != 0) {
        blit_button(this->sprite_409, this->btnScenarioRect);
    }

    // Multiplayer restores the inactive exit region from the menu surface,
    // then restores the text region (0x4223C6..0x42243D).
    this->updateButton(&this->btnExitRect);
    this->updateButton(&this->btnTextRect);
}

/* ================================================================== */
/* EditWindow::drawText -- Draw a single character from font sheet      */
/* Address: 0x422440                                                    */
/* ================================================================== */
void EditWindow::drawText(RECT* rect, int charIndex,
                           loco::assets::SpriteResource* fontResource,
                           loco::assets::SpriteBitmap* fontBitmap)
{
    if (rect == nullptr) return;

    // 0x422440 first restores the destination from pMainSurface.
    this->updateButton(rect);

    if (fontResource == nullptr || fontBitmap == nullptr) return;

    const int width = static_cast<int>(loco::assets::sprite_width(fontResource));
    const int height = static_cast<int>(loco::assets::sprite_height(fontResource));
    RECT source = {0, 0, width, height};
    if (charIndex != 0) {
        // The original OffsetRect uses a sheet-column offset of charIndex * frameWidth.
        OffsetRect(&source, charIndex * width, 0);
    }

    Town_BlitElement(fontBitmap,
                     rect->left, rect->top, rect->right, rect->bottom,
                     g_primary_surface,
                     source.left, source.top, source.right, source.bottom, 0);
}

/* ================================================================== */
/* EditWindow::updateButton -- Restore button background from temp surf */
/* Address: 0x422570                                                    */
/* ================================================================== */
void EditWindow::updateButton(RECT* rect)
{
    if (this->pMainSurface == NULL || rect == NULL) return;

    RECT offR;
    CopyRect(&offR, rect);
    OffsetRect(&offR, this->centerOffsetX, this->centerOffsetY);

    Town_BlitElement(this->pMainSurface,
        rect->left, rect->top, rect->right, rect->bottom,
        g_primary_surface,
        offR.left, offR.top, offR.right, offR.bottom, 0);
}
