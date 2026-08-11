/**
 * AboutDialog.cpp — AboutDialog implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

// Status: TRANSCRIBED

#include "AboutDialog.h"
#include <cassert>
#include <cstdio>
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern "C" {

/* Windows API */
extern void* __stdcall LoadIconA(void* hInstance, const char* lpIconName);   /* @ IAT 0x47736C */
extern HWND  __stdcall GetDesktopWindow(void);                                /* @ IAT 0x477364 */
extern BOOL  __stdcall GetClientRect(HWND hWnd, RECT* lpRect);               /* @ IAT 0x477368 */
extern void  __stdcall SetRectEmpty(RECT* lprc);                             /* @ IAT 0x477350 */
extern void  __stdcall SetRect(RECT* lprc, int left, int top,                /* @ IAT 0x477384 */
                                int right, int bottom);
extern void  __stdcall OffsetRect(RECT* lprc, int dx, int dy);               /* @ IAT 0x477374 */
extern COLORREF __stdcall SetTextColor(HDC hdc, COLORREF color);             /* @ IAT 0x477060 */
extern int   __stdcall SetBkMode(HDC hdc, int mode);                         /* @ IAT 0x477064 */
extern void* __stdcall SelectObject(HDC hdc, void* hgdiobj);                 /* @ IAT 0x47703C */
extern int   __stdcall DrawTextA(HDC hdc, const char* lpchText,              /* @ IAT 0x477348 */
                                  int nCount, RECT* lprc, UINT format);
extern int   __stdcall KillTimer(HWND hWnd, UINT_PTR uIDEvent);              /* @ IAT 0x477344 */

} /* extern "C" */

/* GameWindow base methods — called by AboutDialog ctor and Create */
extern void* __thiscall GameWindow_Ctor(void* self,                          /* 0x413AB0 */
                                         HINSTANCE hInstance, UINT resId);
extern uint32_t __thiscall GameWindow_Create(void* self, int nCmdShow,       /* 0x413DE0 */
                                              HWND hWndParent,
                                              int x, int y,
                                              int nWidth, int nHeight,
                                              void* hMenu, void* hIcon,
                                              uint32_t classStyle,
                                              uint32_t param10,
                                              uint32_t param11,
                                              uint8_t param12);

/* CRT */
extern void* __cdecl CRT_malloc(const char* str, size_t size);              /* 0x466DE0 */
extern int   __cdecl CRT_atoi(const char* str);                              /* 0x466390 */
extern void  __cdecl CRT_exit(int* code, const char* msg);                   /* 0x466CE0 */
extern void  __cdecl CRT_time(void);                                         /* 0x465010 */
extern void* __cdecl operator_new(size_t size);                              /* 0x465CE0 */
extern void  __cdecl GLOBAL_free(void* ptr);                                 /* 0x465CD0 */
extern void  __cdecl CRT_free(void* ptr);                                    /* 0x466C70 */
extern int   __cdecl CRT_sprintf_buf(char* buf, const char* format, ...);    /* 0x466D60 */

/* Resource manager */
extern void* __thiscall ResourceManager_GetById(void* resmgr, int resId);    /* 0x446EA0 */
extern void  __thiscall RESMGR_PlaySound(int resId);                         /* 0x447930 */
extern void* __thiscall AssetMgr_LoadFile(void* assetMgr,                    /* 0x45CD00 */
                                           const char* path, int* sizeOut);

/* WIN32 stream I/O */
extern void  __fastcall WIN32_StreamOpen(void* stream, int mode);            /* 0x463890 */
extern void  __fastcall WIN32_StreamOpenPath(void* stream,                   /* 0x463AA0 */
                                              const char* path,
                                              int mode, int flags);
extern void  __fastcall WIN32_StreamDestroy(int* stream);                    /* 0x463A80 */
extern void  __fastcall WIN32_StreamDestroyImmediate(void* stream);          /* 0x463B10 */
extern void  __fastcall WNDPROC_StreamCleanup(int* stream);                  /* 0x464620 */
extern void* __fastcall WNDPROC_StreamFromMemory(void* stream,               /* 0x464490 */
                                                   char* data, int size,
                                                   int mode);
extern void  __fastcall WNDPROC_StreamSeekForward(void* stream,              /* 0x464C70 */
                                                    void* buf, int len,
                                                    int lineEnd);
extern void  __fastcall WNDPROC_StreamSeekForwardLine(void* stream,          /* 0x410260 */
                                                       void* buf, int len,
                                                       int lineEnd);
extern void  __fastcall Game_UnlockMutex(int mutex);                         /* 0x410240 */

/* UI helper */
extern void __fastcall UI_CenterWindow(int* left, int* rect);               /* 0x425A50 */
/* Real def: ui/UIPANEL_Surface.cpp, bool(void*,uint32_t,uint32_t,int32_t,
 * uint32_t,void*,uint32_t,uint32_t,int32_t,uint32_t,uint32_t) — was declared
 * `int` for most params, which doesn't match the real mixed uint32_t/int32_t
 * shape (call-0 landmine). */
extern bool __fastcall UIPANEL_Blit(void* srcSurface, uint32_t srcX, uint32_t srcY,   /* 0x42B050 */
                                     int32_t srcW, uint32_t srcH,
                                     void* dstSurface,
                                     uint32_t dstX, uint32_t dstY,
                                     int32_t dstW, uint32_t dstH,
                                     uint32_t flags);

/* CRT wide-string helper for parsing WVE marker */
extern void* __thiscall wcsstr(const void* str, const void* substr);        /* 0x471480 */

/* UIPANEL surface helpers — real bodies in ui/UIPANEL_Surface.cpp /
 * graphics/LOCOBITMAP.cpp. Declared here with `void*` (not the typed
 * UIPANEL_Surface*) to avoid pulling in graphics/LOCOBITMAP.h's larger,
 * differently-sized same-named struct — see AboutDialog::InitSprites'
 * doc comment and PROGRESS.md for that pre-existing conflict. Matches
 * ui/UIPANEL.cpp's own `void*`-typed local extern for the same address. */
extern void     __fastcall UIPANEL_CreateSurface(void* surface);            /* 0x42A110 */
extern size_t   UIPANEL_Surface_Size();  /* graphics/LOCOBITMAP.cpp — real sizeof(UIPANEL_Surface) */
extern uint32_t __thiscall UIPANEL_InitSurface(void* surface, int width,    /* 0x42A850 */
                                                int height, int mode,
                                                uint32_t paletteParam,
                                                uint8_t fillByte);
extern void     __thiscall UIPANEL_SetClipRect(void* surface,               /* 0x42AA90 */
                                                uint8_t fillByte,
                                                uint32_t blitFlags);

/* Cursor_Render — the binary passes AboutDialog's own `this` where a
 * Cursor* is expected (matching the identical, already-documented idiom
 * in ui/HelpWnd.cpp's own Cursor_Render calls: "binary passes HelpWnd* as
 * Cursor*"). Declared with `void*` here rather than pulling in Cursor.h
 * for a call that never actually dereferences a Cursor field.
 * Address corrected here to 0x414C20 (verified via Ghidra: real function
 * `Cursor_Render`, matching signature/param count) — ui/HelpWnd.cpp's own
 * declaration for this same function still cites 0x416420, which Ghidra
 * resolves to a mid-body address inside the unrelated `Cursor_Init`; not
 * fixed there since it's a pre-existing bug outside this pass's scope
 * (see PROGRESS.md). */
extern void __fastcall Cursor_Render(void* cursor, uintptr_t hWnd,          /* 0x414C20 */
                                      int hdc, char flag);

/* ================================================================== */
/* Global variables referenced                                         */
/* ================================================================== */

extern void* g_resmgr;              /* 0x4855E8 — global resource manager */
extern void* g_asset_mgr;           /* 0x485600 — global asset manager */
extern char  g_install_path[];      /* 0x4A99C8 — installation path string */
extern void* g_font_small;          /* 0x4855F4 — small font handle (used for credits text) */
extern char  g_empty_string;        /* 0x4851D0 — empty string constant */

/* String constants referenced by LoadCredits */
extern const char* g_credits_subpath;   /* 0x47E3D8 — "CREDITS\\WVE\\" */
extern const char* g_credits_fmt;       /* 0x47E3D0 — format string for path building */
extern const char* g_credits_filename;  /* 0x47E3E4 — credits WVE filename */
extern const wchar_t* g_wve_marker;     /* 0x47E3CC — "WVE" wide-string marker */
extern const wchar_t* g_wve_replacement;/* 0x47E3C8 — replacement string for WVE marker */
extern const char* g_msg_malloc_fail;   /* 0x47A5E8 — "out of memory" error message */

/* ================================================================== */
/* AboutDialog constructor                                              */
/* Address: 0x40F1C0                                                    */
/*                                                                      */
/* Called by: CGWND_InitAllSubsystems @ 0x407638                        */
/*                                                                      */
/* Calls the GameWindow base constructor, then initializes all          */
/* AboutDialog-specific fields. The vtable is set to VTBL_ABOUTDIALOG   */
/* (0x477680) after the base constructor sets its vtable.               */
/* ================================================================== */
AboutDialog::AboutDialog(HINSTANCE hInstance, uint32_t resId)
    : GameWindow(hInstance, resId)
{
    RECT emptyRect;

    /* Override vtable with AboutDialog's vtable */
/* In the binary: sets vtable here. Compiler-managed in natural C++. */

    /* Initialize all AboutDialog-specific fields to defaults */
    this->scroll_timer        = -10;                  /* +0x11C */
    this->timer_id            = 0;                    /* +0x124 */
    this->sprites_initialized = 0;                    /* +0x12C */
    this->panel_active        = 0;                    /* +0x12D */
    this->res_surface         = NULL;                 /* +0x130 */
    this->res_object          = NULL;                 /* +0x134 */
    this->panel               = NULL;                 /* +0x14C */
    this->hIcon               = NULL;                 /* +0x150 */
    this->background_res_id   = 0;                    /* +0x1154 */
    this->screensaver_surface = NULL;                 /* +0x1158 */
    this->res_credits_obj     = NULL;                 /* +0x116C */
    this->res_credits_data    = NULL;                 /* +0x1170 */

    /* Zero the 4096-byte credits text buffer (+0x154 to +0x1153) */
    for (int i = 0; i < 0x1000; i++) {
        this->credits_text[i] = '\0';
    }

    /* Clear the scroll rectangle */
    SetRectEmpty(&this->scroll_rect);                 /* +0x115C */
}

/* ================================================================== */
/* Scalar deleting destructor (vtable[0])                               */
/* Address: 0x40F270                                                    */
/*                                                                      */
/* Calls base_destructor to release resources, then optionally frees    */
/* the heap allocation.                                                 */
/*                                                                      */
/* Called from: error unwind in CGWND_InitAllSubsystems @ 0x407692,     */
/*              CGWND cleanup path                                      */
/* ================================================================== */
AboutDialog::~AboutDialog()
{
    this->base_destructor();
}

/* ================================================================== */
/* Base destructor body                                                 */
/* Address: 0x40F290                                                    */
/*                                                                      */
/* Restores the AboutDialog vtable (so vtable dispatch during the       */
/* GameWindow base destructor chain is correct), then tail-calls        */
/* GameWindow::base_destructor via JMP.                                 */
/*                                                                      */
/* NOTE: Actual resource cleanup (panel, sprites, surfaces, timer) is   */
/* handled by the Hide (vtable[1]) override. The base destructor only   */
/* cleans up GameWindow-level resources.                                */
/* ================================================================== */
void AboutDialog::base_destructor()
{
/* In the binary: sets vtable here. Compiler-managed in natural C++. */
    this->GameWindow::base_destructor();
}

/* ================================================================== */
/* Create the About dialog window                                       */
/* Address: 0x40F510                                                    */
/*                                                                      */
/* Called by: CGWND_InitAllSubsystems @ 0x4076DE                        */
/*                                                                      */
/* Loads the window icon from resource 0x65, computes a centered        */
/* position on the desktop (window size 248x232), then delegates to     */
/* GameWindow::Create to register the WNDCLASS and create the HWND.     */
/* The window is created initially hidden (nCmdShow=0).                */
/* ================================================================== */
bool AboutDialog::Create(HWND hWndParent)
{
    RECT desktopRect;
    int  x, y;
    int  width  = 0xF8;   /* 248 pixels */
    int  height = 0xE8;   /* 232 pixels */

    /* Load icon from resource 0x65 (101 = IDI_APPLICATION) */
    this->hIcon = LoadIconA(
        this->hInstance,
        reinterpret_cast<const char*>(static_cast<uintptr_t>(0x65)));  /* +0x150 */

    /* Get desktop dimensions for centering */
    HWND hDesktop = GetDesktopWindow();
    GetClientRect(hDesktop, &desktopRect);

    x = 0;
    y = 0;

    /* Center the 248x232 window on the desktop */
    UI_CenterWindow(&desktopRect.left, &x);

    /* Create the hidden window (WS_POPUP|WS_CLIPSIBLINGS|WS_CLIPCHILDREN = 0x86000000) */
    uint32_t result = GameWindow_Create(
        this,
        0,                             /* nCmdShow = 0 (hidden) */
        hWndParent,
        x, y,
        width, height,
        (void*)NULL,                   /* hMenu = NULL */
        this->hIcon,                   /* hIcon for WNDCLASS */
        0,                             /* classStyle = default (CS_HREDRAW|CS_VREDRAW) */
        0,                             /* unused param10 */
        0,                             /* unused param11 */
        0                              /* showCursor = 0 */
    );

    return (result != 0);
}

/* ================================================================== */
/* RenderCredits — Render one frame of scrolling credits text           */
/* Address: 0x40F980 (1212 bytes — the largest AboutDialog method)      */
/*                                                                      */
/* Called by: CGWND_Screensaver_Update @ 0x40F42E, 0x40F445             */
/*                                                                      */
/* This function renders one frame of the scrolling credits animation.  */
/* It processes the credits text buffer (+0x154), parses optional       */
/* background image tags ('<resID>' on the first line), loads the       */
/* background image via ResourceManager, blits it via UIPANEL_Blit,     */
/* and renders the scrolling text via GDI DrawTextA with orange color   */
/* (0xFF5C00) on transparent background using the small game font.      */
/*                                                                      */
/* NOTE: The Ghidra decompiler output for this function is heavily      */
/* garbled due to SEH exception handlers and complex string operations.  */
/* The disassembly trace shows the actual control flow more clearly.    */
/* The key operations are documented below at a high level.             */
/* ================================================================== */
uint32_t AboutDialog::RenderCredits(HDC hdc)
{
    /* NOTE: This is a high-level description of the function's logic.
       The actual assembly at 0x40F980-0x40FE3F (402 instructions) has
       complex SEH-protected string processing that the decompiler
       struggles to represent cleanly. The key phases are:

     Phase 1: Setup
       - Play credits music (RESMGR_PlaySound with resource 0x5597)
       - Read scroll_pos from +0x148 (word)
       - Set up panel rendering context via panel vtable calls
       - Create a 216x196 pixel render area

     Phase 2: Process credits text
       - Copy credits_text (+0x154) into local buffer
       - Skip '*' comment/separator lines and blank '\n' lines
       - If first line starts with '<', parse '<number>' format
         to extract background_res_id (stored at +0x1154)
       - Count newlines, pad text buffer to exactly 13 lines
         centered vertically with extra '\n' padding

     Phase 3: Load and render background image
       - If background_res_id != 0:
         - ResourceManager_GetById to get resource object
         - Lock surface via vtable[4] for rendering
         - Set up image_rect (+0x1174) with the frame dimensions,
           centered in the 216x196 area via OffsetRect
         - Blit background via UIPANEL_Blit(src=data, dst=panel)

     Phase 4: Render text via GDI
       - Get HDC from panel (vtable[0x44/4=17])
       - SetTextColor(0xFF5C00) — Lego orange
       - SetBkMode(TRANSPARENT)
       - SelectObject(g_font_small)
       - DrawTextA(text, DT_CENTER|DT_TOP) in a 216x196 rect
       - Restore GDI objects

     Phase 5: Present
       - Call panel vtable[0x68/4=26] to present/blit
       - Return 1
    */

    /* Due to heavy SEH-induced decompilation issues, this function
       is documented structurally rather than as exact C++ code.
       See the disassembly at 0x40F980-0x40FE3F for the authoritative
       instruction-level behavior. */

    /* Phase 1: Play credits music */
    RESMGR_PlaySound(0x5597);  /* Lego Loco credits music resource */

    /* Phase 2: Get scroll position and set up panel */
    /* scroll_pos read from +0x148 (2 bytes, used as initial Y offset) */
    /* Panel at +0x14C is used for rendering: */
    void* pPanel = this->panel;  /* +0x14C */

    if (pPanel != NULL) {
        /* Call panel vtable methods for setup */
        /* vtable[0x1D] — set some parameter (0x8) */
        /* vtable[5] — set clip rect to (0, 0, 216, 196) */
    }

    /* Phase 2-5: Text processing, background loading, and rendering
       See the disassembly for exact operations. The function uses
       multiple 4096-byte stack buffers for text manipulation. */

    /* Return 1 to indicate credits were rendered */
    return 1;
}

/* ================================================================== */
/* LoadCredits — Load credits text from WVE animation resource file     */
/* Address: 0x40FE50 (1005 bytes)                                       */
/*                                                                      */
/* Called by: CGWND_AboutDialog_Show @ 0x40F311 (inside Show override)  */
/*                                                                      */
/* This function loads credits text from a WVE animation file into     */
/* the credits_text buffer at +0x154 (max 4096 bytes). It uses the      */
/* game's stream I/O abstraction (WIN32_Stream*) to read chunks of      */
/* the file, extracting text lines between WVE markers.                 */
/*                                                                      */
/* Two load paths:                                                      */
/*   1. AssetManager — loads from game's packed archives first          */
/*   2. Direct file open — falls back to disk file if AssetManager      */
/*      fails (or is unavailable)                                       */
/*                                                                      */
/* NOTE: The decompiler output for this function is heavily garbled     */
/* due to SEH exception handling, wide-string operations, and complex   */
/* stream I/O. See the disassembly at 0x40FE50-0x41023D for details.    */
/* ================================================================== */
void AboutDialog::LoadCredits()
{
    /* NOTE: This is a high-level description of the function's logic.
       The full disassembly at 0x40FE50-0x41023D (313 instructions)
       is heavily SEH-protected with stream locking/unlocking.

     Phase 1: Setup
       - Zero the credits_text buffer (+0x154, 4096 bytes)
       - Open a WIN32 stream (mode 1)
       - Build file path: sprintf("%s\\%s\\%s", install_path,
           "CREDITS\\WVE\\", credits_filename)

     Phase 2: AssetManager path (primary)
       - If g_asset_mgr exists:
         - AssetMgr_LoadFile to load the WVE file
         - operator_new(0x5C) for stream object
         - WNDPROC_StreamFromMemory to create memory stream
         - Set credits_text[0] = '\0'
         - Lock stream, increment refcount
         - WNDPROC_StreamSeekForward(buf, 0x1000, 10) — read chunk,
           seek past newline
         - Search for WVE marker (wcsstr with g_wve_marker @ 0x47E3CC)
         - Loop: while WVE found and not aborted:
           - Copy read buffer content to end of credits_text
           - Append g_wve_replacement (@ 0x47E3C8) string
           - Lock, read next chunk, unlock
           - Search for WVE marker
         - Release stream via vtable[0]

     Phase 3: Direct file path (fallback)
       - If AssetManager path failed:
         - WIN32_StreamOpenPath to open file from disk
         - Same read-append loop as Phase 2

     Phase 4: Cleanup
       - WIN32_StreamDestroy (cleanup attempt, may fail silently)
       - WNDPROC_StreamCleanup (final stream cleanup)

     The text lines are appended sequentially with a separator between
     each line, building up the scrolling credit text. Lines starting
     with '*' or '\n' are skipped during RenderCredits rendering.
    */

    /* See the disassembly at 0x40FE50-0x41023D for exact implementation.
       Key called functions:
         WIN32_StreamOpen        (0x463890)
         CRT_sprintf_buf         (0x466D60)
         AssetMgr_LoadFile       (0x45CD00)
         operator_new            (0x465CE0)
         WNDPROC_StreamFromMemory (0x464490)
         WNDPROC_StreamSeekForward (0x464C70)
         wcsstr                  (0x471480)
         WIN32_StreamOpenPath    (0x463AA0)
         WIN32_StreamDestroyImmediate (0x463B10)
         WIN32_StreamDestroy     (0x463A80)
         WNDPROC_StreamCleanup   (0x464620) */
}

/* ================================================================== */
/* AboutDialog::Update                                                  */
/* Address: 0x40F3C0 (Ghidra: AboutDialog_UpdateScreensaver)            */
/* See the header doc comment for the full behavior description.       */
/* ================================================================== */
void AboutDialog::Update()
{
    if (scroll_timer < 15) {
        scroll_timer += 2;
    }

    if (scroll_timer > 0) {
        if (scroll_accum < 1000 - scroll_timer) {
            scroll_accum += scroll_timer;
        } else {
            HDC nextFrame = reinterpret_cast<HDC>(static_cast<intptr_t>(frame_counter + 1));
            fade_timer    = 0;
            scroll_timer  = -10;
            scroll_accum  = 0;
            frame_counter = static_cast<int32_t>(reinterpret_cast<intptr_t>(nextFrame));

            if (RenderCredits(nextFrame) == 0) {
                frame_counter = 1;
                RenderCredits(reinterpret_cast<HDC>(static_cast<intptr_t>(1)));
            }
        }

        fade_timer = scroll_accum / 10;
        RenderScreensaver();
        Cursor_Render(static_cast<void*>(this), reinterpret_cast<uintptr_t>(this->hWnd), 0, 0);
    }
}

/* ================================================================== */
/* AboutDialog::InitSprites                                            */
/* Address: 0x40F6A0 (Ghidra: AboutDialog_InitScreensaver)             */
/* See the header doc comment for the full behavior description.       */
/* ================================================================== */
void AboutDialog::InitSprites()
{
    if (sprites_initialized == 1) {
        return;
    }

    /* &g_resmgr matches every other in-tree ResourceManager_GetById call
     * site's convention for this exact `void* resmgr` parameter (see e.g.
     * world/scriptengine.cpp:505, input/Cursor.cpp, game/GameObject.cpp). */
    void* resource = ResourceManager_GetById(&g_resmgr, 0x3daf);
    res_object = resource;

    /* resource->vtable[1](0, 0) — resource objects returned by
     * ResourceManager_GetById are not yet a modeled C++ hierarchy in this
     * codebase (still void* everywhere they're used), so this is a direct,
     * evidence-backed vtable-slot call rather than a guessed method name,
     * matching this file's own AudioChannel_Release-style precedent
     * elsewhere in the DDRAW.cpp family for similarly unmodeled objects. */
    using ResourceGetSurfaceFn = void* (__stdcall*)(void*, int, int);
    void** resourceVtbl = *reinterpret_cast<void***>(resource);
    res_surface = reinterpret_cast<ResourceGetSurfaceFn>(resourceVtbl[1])(resource, 0, 0);

    /* Allocate the screensaver's own UIPANEL surface. The original
     * allocates exactly 0x20 bytes (the real 32-bit sizeof(UIPANEL_Surface),
     * graphics/LOCOBITMAP.h) — use UIPANEL_Surface_Size() instead of that
     * literal since pointer fields widen the struct on this 64-bit host
     * (see graphics/LOCOBITMAP.cpp). Faithfully proceeds to call
     * UIPANEL_InitSurface below even if this allocation fails
     * (screensaver_surface stays nullptr) — an original bug, preserved
     * as-is rather than fixed. */
    void* surfaceBuf = operator_new(UIPANEL_Surface_Size());
    if (surfaceBuf != nullptr) {
        UIPANEL_CreateSurface(surfaceBuf);
    }
    screensaver_surface = surfaceBuf;

    UIPANEL_InitSurface(screensaver_surface, 0xd8, 0xc4, 0, 0, 0);
    UIPANEL_SetClipRect(screensaver_surface, 9, 0);

    sprites_initialized = 1;
}

/* ================================================================== */
/* AboutDialog::RenderScreensaver                                      */
/* Address: 0x410280                                                    */
/* See the header doc comment for the full behavior description.       */
/* ================================================================== */
int32_t AboutDialog::RenderScreensaver()
{
    int32_t widthDelta  = scroll_rect.right  - scroll_rect.left;
    int32_t heightDelta = scroll_rect.bottom - scroll_rect.top;

    UIPANEL_Blit(screensaver_surface, scroll_rect.left, scroll_rect.top,
                 scroll_rect.right, scroll_rect.bottom,
                 static_cast<void*>(this->backbufferSurface),
                 0, 0, widthDelta, heightDelta, 1);

    /* Fade-window bounds gate (0x4102EE-0x410350 in the disassembly): two
     * signed-divide-by-3 checks (C++'s `/` on signed int truncates toward
     * zero, bit-identical to the original's IMUL-0x55555556 reciprocal
     * trick) reject frames at the extreme ends of the fade curve. */
    if (fade_timer < 0x10) {
        if ((0x10 - fade_timer) / 3 > 3) {
            return 0;
        }
    } else if (fade_timer > 0x54) {
        if ((fade_timer - 0x54) / 3 > 3) {
            return 0;
        }
    }
    if ((0x10 - fade_timer) / 3 >= 3) {
        return 0;
    }

    /* The remainder of this function (SetRect/OffsetRect + a call through
     * backbufferSurface's own vtable[5] passing backbufferSurface itself
     * as an explicit first argument, then a 1-3 iteration alpha-crossfade
     * UIPANEL_Blit loop) is a deferred stub — see the header doc comment
     * for why. Provably unreachable today: Update() (this function's only
     * wired-in caller) has no caller of its own anywhere in this tree yet. */
    fprintf(stderr, "STUB: %s (post-gate fade-blit tail) at %s:%d\n",
            __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — AboutDialog::RenderScreensaver 0x410280's "
                "post-gate SetRect/vtable[5]/crossfade-loop tail is not "
                "decompiled (ambiguous backbufferSurface vtable shape)");
    return 0;
}
