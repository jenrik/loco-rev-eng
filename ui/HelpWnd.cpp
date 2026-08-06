/**
 * HelpWnd.cpp — Tutorial/help window implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * HelpWnd is the in-game tutorial/help window, subclassing GameWindow.
 * It manages up to 200 help pages with navigation, scrollable text,
 * narration audio, overlay animations, and clickable hotspots.
 *
 * Vtable layout (VTBL_HELPWND = 0x478428, overriding GameWindow vtable):
 *   [0] +0x00: scalar deleting destructor  (0x44F4F0) — overrides GameWindow::~GameWindow
 *   [1] +0x04: hide                        (0x450AE0) — overrides GameWindow::hide
 *   [2] +0x08: show                        (0x450240) — signature mismatch, see NOTE in HelpWnd.h
 *   [3] +0x0C: set_mode (inherited)        (0x414340) — from GameWindow
 *   [4] +0x10: cleanup_sprites             (0x451440) — overrides GameWindow::cleanup_sprites
 *   [5] +0x14: create                      (0x450CA0) — signature mismatch, see NOTE in HelpWnd.h
 *   [6] +0x18: init                        (0x451180) — overrides GameWindow::init
 *   [7] +0x1C: update_anim                 (0x450450) — overrides GameWindow::update_anim
 *
 * HelpPageNode (VTBL_HELPPAGE_NODE = 0x4783D8):
 *   [0] +0x00: scalar deleting destructor  (0x44F2A0)
 *   [1] +0x04: base destructor             (0x44F2C0)
 *
 * NOTE: find_page, find_page_scalar_dtor, and find_page_base_dtor
 * are actually HelpPageNode methods. TODO: move them to a separate
 * HelpPageNode class once class hierarchy is fully decompiled.
 */

// Status: TRANSCRIBED

#include "HelpWnd.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#include "ButtonSprite.h"
#include <cstdint>
#include <new>
#include <cstring>
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */

/* HelpPageNode extracted to ui/HelpPageNode.h / HelpPageNode.cpp */
/* ==================================================================== */
/* External declarations                                                */
/* ==================================================================== */

/* Heap */
    extern void* __cdecl operator_new(size_t size);        /* 0x465CE0 */
    extern void  __cdecl GLOBAL_free(void* ptr);            /* 0x465CD0 */
    extern void  __cdecl CRT_free(void* ptr);               /* 0x466510? */
    extern int   __cdecl CRT_atoi(const char* str);         /* 0x466622 */
    extern char* __cdecl CRT_itoa(int val, char* buf, int radix); /* 0x466622 */
    
extern "C" {
    /* Win32 API */
    extern HWND   __stdcall GetDesktopWindow(void);
    extern BOOL   __stdcall GetClientRect(HWND, void* lpRect);
    extern HICON  __stdcall LoadIconA(HINSTANCE, LPCSTR);
    extern BOOL   __stdcall SetRect(void* rect, int l, int t, int r, int b);
    extern BOOL   __stdcall OffsetRect(void* rect, int dx, int dy);
    extern BOOL   __stdcall CopyRect(void* dst, void* src);
    extern BOOL   __stdcall PtInRect(const void* rect, POINT pt);
    extern BOOL   __stdcall SetRectEmpty(void* rect);
    extern int    __stdcall SetBkMode(void* hdc, int mode);
    extern int    __stdcall SetTextColor(void* hdc, int color);
    extern void*  __stdcall SelectObject(void* hdc, void* obj);
    extern int    __stdcall DrawTextA(void* hdc, const char* str, int len,
                                       void* rect, int format);
    extern LRESULT __stdcall DefWindowProcA(HWND, UINT, WPARAM, LPARAM);
    extern BOOL   __stdcall KillTimer(HWND, UINT_PTR);
    extern UINT_PTR __stdcall SetTimer(HWND, UINT_PTR, UINT, void* proc);
    extern void   __stdcall GetWindowRect(HWND, void* rect);
    extern void   __stdcall Sleep(DWORD ms);

    /* Game functions (C-linkage) */
    extern void   WIN32_StreamOpen(void* stream, int mode);
    extern void   WIN32_StreamOpenPath(void* stream, const char* path, int mode, int unknown);
    extern void   WIN32_StreamDestroy(void* stream);
    extern void   WIN32_StreamDestroyImmediate(void* stream);
    extern void   WNDPROC_CriticalSectionLock(int* stream, char* buf);
    extern void   WNDPROC_StreamCleanup(void* stream);
    extern void*  WNDPROC_StreamFromMemory(void* self, char* data, int size, int mode);
    extern void*  AssetMgr_LoadFile(void* mgr, const char* path, int* outSize);

    /* Config_GetIniString/Config_ReadInt are the recovered C-ABI INI
     * helpers in game/ConfigIni.cpp (extern "C"). Without matching
     * linkage here, these calls silently bind to unrelated C++-mangled
     * no-op stubs elsewhere in the tree (or, for the mismatched
     * Config_ReadInt signature below, to no symbol at all — a null
     * call that crashed CGWND_EnterMode3(1)'s HelpWnd::play_narration
     * on the mode-1→mode-3 transition). */
    extern int Config_GetIniString(void* config, const char* section,
                                    const char* key, const char* def,
                                    char* out, int maxLen);
    extern int Config_ReadInt(void* config, const char* section,
                               const char* key, const char* value);
}

    /* Game functions (C++ linkage or typed APIs) */
    extern void   Sprite_Init(void* sprite);                /* 0x454BF0 */
    extern void   Sprite_Destroy(void* sprite);             /* 0x454BC0 */
    extern void   Sprite_SetState(void* sprite, int state, int* surface); /* 0x454C30 */
    extern BOOL   UIPANEL_Blit(void* sprite, uint dstX, uint dstY,
                                int dstW, uint dstH, void* surface,
                                int srcX, int srcY, int srcW, int srcH, int mode); /* 0x42B050 */
    extern void   FormatResourceString(void* resmgr, UINT id,
                                        char* buf, int maxLen); /* 0x447330 */
    extern int    ResourceManager_GetStringById(void* resmgr, UINT id); /* 0x4472B0 */
    extern void   LoadSoundResource(int handle);             /* loader, see ResourceManager */
    extern void   ReleaseSoundResource(int handle);           /* release refcounted sound */
    extern void   GameAudio_PlayResourceEx(void* audio, UINT resId, int* outChannel); /* 0x4131C0 */
    extern void   AudioChannel_Release(void* channel);  /* 0x40ECA0 */
    extern void   GameAudio_UpdateVolume(void* audio, char flag); /* 0x4135B0 */
    extern int    AudioChannel_IsActive(int channel); /* 0x40EEB0 */
    extern int    Cursor_WaitForBlit(void* self);           /* 0x414BB0 — returns HDC */
    extern void   Cursor_Render(void* cursor, int hWnd,
                                 int hdc, char flag);          /* 0x414C20 */
    extern void   Cursor_SetCapture(void* cursor, int captured); /* 0x414290 */
    extern int    Cursor_HandleWindowPaint(void* cursor, int hWnd); /* 0x414A80 */
    /* GameWindow method dispatch — now via qualified C++ calls
     * (e.g., this->GameWindow::hide(), this->GameWindow::create(...)) */
    extern void   UI_CenterWindow(int* outerRect, int* innerRect); /* 0x425A50 */
    extern void   UI_SetWindowVisible(void* wnd, char visible); /* 0x425F20 */
    extern LRESULT UI_DefWndProc(HWND, UINT, WPARAM, LPARAM);  /* 0x422EA0 */
    extern void   CGWND_SetFullscreenMode(char flag);          /* 0x40BCF0 */
    /* Real def: core/CGWND.cpp, void(int), address 0x408130 (the 0x40BD50
     * annotation here was bogus). Was declared void* here, mismatching the
     * real int param (call-0 landmine — silently bound to
     * shared/link_stubs.cpp's void* no-op stub). */
    extern void   CGWND_SetMode(int mode);                   /* 0x408130 */
    extern int    Game_SetScreenMode(void* game, char a, char b, char c); /* 0x40EA10 */
    extern void   TileMap_InvalidateRect(void* tilemap, int l, int t,
                                          int r, int b);        /* 0x446330 */
    extern void   TileMap_InvalidateDirtyRects(void* tilemap, char flag); /* 0x446680 */
    extern void   PlaySound(int resId);                         /* 0x459930 */
    /* Vehicle/GameObject methods — extracted to HelpPageNode.cpp */
    extern int    CGWND_TrackPiece_SetZoom(void* obj, int zoom); /* 0x413A30 */
    extern void   WIN32_PostQuit(void);                        /* 0x463670 —
                                    real body in core/CGWND.cpp (0x462560,
                                    this comment's old value, isn't a
                                    function at all) */

/* ================================================================== */
/* Global variables                                                     */
/* ================================================================== */

extern HelpWnd* g_audio_mgr;           /* 0x4FD2CC */
extern Town*    g_town;                /* 0x4FD2D0 */
class InputMgr;
extern InputMgr g_input_mgr;           /* 0x4A9990 — static InputMgr object */
extern PostcardAlbum* g_postcard;      /* 0x4FD2D4 */
extern Cursor*  g_cursor;              /* 0x4FD2D8 */
extern int      g_game_mode;           /* 0x4FD2E0 */
extern int      g_is_fullscreen;       /* 0x4FD2E4 */
extern TileMap* g_tilemap;             /* 0x4AAD08 — tilemap singleton */
extern ResourceManager* g_resmgr;      /* 0x4855E8 */
extern HFONT    g_font_small;          /* 0x4855F4 */
extern void*    g_primary_surface;     /* 0x4FD3C4 */
extern int      g_viewport_rect_left;   /* 0x4FD358 */
extern int      g_viewport_rect_top;    /* 0x4FD35C */
extern int      g_viewport_rect_right;  /* 0x4FD360 */
extern int      g_viewport_rect_bottom; /* 0x4FD364 */
extern int      g_screen_center_x;      /* game screen center X */
extern int      g_screen_center_y;     /* game screen center Y */
extern Game*    g_game;                 /* game singleton */
extern GameAudio* g_audio;             /* GameAudio singleton */
extern NetMan*  g_netman;              /* NetMan singleton */
/* g_demo_mode — declared in shared/types.h */
extern void*    g_player_config;       /* player config pointer */
/* g_config_ini — declared in shared/types.h, not redeclared here */
extern AssetMgr* g_asset_mgr;          /* asset manager */
extern char     g_install_path[];      /* game install path string */

/* ================================================================== */
/* Stream vtable helper — TODO: decompile stream class, replace with   */
/* proper C++ destructor dispatch.                                      */
/* ================================================================== */

/**
 * stream_vtable_scalar_dtor — Call the scalar deleting destructor
 * (vtable[1]) on a WNDPROC stream object.
 *
 * The stream class has a vtable at *streamObj; vtable[1] is the scalar
 * deleting destructor. Called with flag=1 to free memory.
 *
 * TODO: decompile stream class (WNDPROC stream) at 0x479190.
 * Once the class has a proper C++ destructor, replace this with
 * 'delete streamObj'.
 */
/* stream_vtable_scalar_dtor — moved to HelpWnd_stubs.cpp */
extern void stream_vtable_scalar_dtor(int* streamObj);

/**
 * Single-character test string for measure_text_height. 0x47f06c.
 */
extern const char s_measure_test_char[];

/**
 * Single-character string for render_scroll_down. 0x47f070.
 */
extern const char s_scroll_down_char[];

/* ================================================================== */
/* HelpPageNode methods — extracted to ui/HelpPageNode.cpp             */
/* ================================================================== */

/* ================================================================== */
/* HelpWnd constructor                                                 */
/* Address: 0x44F490                                                   */
/*                                                                      */
/* Chains to GameWindow constructor, then calls init()                 */
/* ================================================================== */
HelpWnd::HelpWnd(HINSTANCE hInstance, UINT resId)
    : GameWindow(hInstance, resId)
{
    /* NOTE: init() is called twice in the binary lifecycle: once here from
     * the constructor, and once via vtable[6] during create(). The second
     * call re-creates the ButtonSprites, leaking those from the first call.
     * This matches the original binary behavior.
     *
     * init() itself does no Win32 file I/O — it only allocates nine
     * ButtonSprite objects (a portable placement-new; ButtonSprite's own
     * ctor just zeroes pixelData/surface, see ui/ButtonSprite.cpp) and
     * zeroes index/state fields, including spritesInited. A previous
     * #ifndef _WIN32 skip here (justified by a "heavy Win32 file I/O"
     * comment that does not match this function's actual body) left the
     * host's g_audio_mgr permanently uninitialized: CGWND_EnterMode3(1)'s
     * unconditional HelpWnd::play_narration(5, 0) call then read garbage
     * heap memory through this->btnAnim in load_page and crashed. On the
     * host, create() (which would run init() a second time) is never
     * called for g_audio_mgr (see CGWND::InitAllSubsystems's #ifndef
     * _WIN32 branch), so this single call cannot double-leak sprites. */
    this->init();
}

/* ================================================================== */
/* HelpWnd::~HelpWnd — Scalar deleting destructor (vtable[0])          */
/* Address: 0x44F4F0                                                   */
/* ================================================================== */
HelpWnd::~HelpWnd()
{
    this->base_destructor();
}

/* ================================================================== */
/* HelpWnd::base_destructor — Base destructor body                     */
/* Address: 0x44F510                                                   */
/* ================================================================== */
void HelpWnd::base_destructor()
{
    /* In the binary: sets vtable here. Compiler-managed in natural C++. */
    this->cleanup_sprites();

    /* Chain to GameWindow base destructor */
    this->GameWindow::base_destructor();
}

/* ================================================================== */
/* HelpWnd::init — Initialize all HelpWnd subsystems (vtable[6])       */
/* Address: 0x451180                                                   */
/* ================================================================== */
void HelpWnd::init()
{
    int i;

    /* NOTE: Called twice per binary lifecycle — once from the constructor
     * and once via vtable[6] during create(). The second call leaks the
     * ButtonSprites from the first (9 × 0x24 = 0x144 bytes leaked).
     * Matches original binary behavior at 0x451180. */

    /* Zero initial state fields */
    this->hIcon = nullptr;               /* +0x154 */
    this->audioChannel = NULL;          /* +0x158 */
    this->spritesInited = 0;            /* +0x14E (byte) */
    this->active = 0;                   /* +0x14C (byte) */
    this->wasFullscreen = 0;            /* +0x14D (byte) — set by play_narration before show */
    this->nextBtnEnabled = 1;           /* +0x11C (byte) */
    this->prevBtnEnabled = 1;           /* +0x124 (byte) */
    this->closeBtnEnabled = 1;          /* +0x12C (byte) */
    this->scrollDownBtnEnabled = 0;     /* +0x144 (byte) */
    this->lineHeight = 1;               /* +0x3068 — set to 1, later overwritten with -1 before the dword loop; the dword loop ends at +0x304B, before +0x3068; matches binary */
    this->animTimerId = 0;              /* +0x3058 */

    /* Create 9 ButtonSprite objects for the help UI */
    /* NOTE: Order matches original — operator_new then placement new, stored in field order.
     * The original x86 body allocates a literal 0x24 bytes per sprite
     * (sizeof(ButtonSprite) on 32-bit: vtable ptr + 4 int32_t + 2 pointers +
     * UINT + int32_t, all 4-byte). On this 64-bit build ButtonSprite is
     * larger (8-byte vtable ptr and pointer members), so allocating the
     * literal 0x24 and placement-constructing into it overflowed the
     * block — corrupting the next heap chunk's header, surfaced later as
     * "free(): invalid size" when cleanup_sprites() deleted a sprite (or
     * an unrelated one) at HelpWnd teardown. Use sizeof(ButtonSprite) so
     * the allocation always matches this platform's real object size. */

    /* btnClose (+0x128) — Close button, res 0x3D01 */
    {
        void* mem = operator_new(sizeof(ButtonSprite));
        this->btnClose = (ButtonSprite*)(mem ? new ((void*)mem) ButtonSprite(0x3D01) : NULL);
    }

    /* btnNext (+0x118) — Next page, res 0x3CFF */
    {
        void* mem = operator_new(sizeof(ButtonSprite));
        this->btnNext = (ButtonSprite*)(mem ? new ((void*)mem) ButtonSprite(0x3CFF) : NULL);
    }

    /* btnPrevActual (+0x120) — Prev page, res 0x3D00 */
    {
        void* mem = operator_new(sizeof(ButtonSprite));
        this->btnPrevActual = (ButtonSprite*)(mem ? new ((void*)mem) ButtonSprite(0x3D00) : NULL);
    }

    /* btnAnim (+0x130) — Animation sprite, res 0x3CFD */
    {
        void* mem = operator_new(sizeof(ButtonSprite));
        this->btnAnim = (ButtonSprite*)(mem ? new ((void*)mem) ButtonSprite(0x3CFD) : NULL);
    }

    /* btnContent (+0x134) — Content sprite (dynamically set later) */
    {
        void* mem = operator_new(sizeof(ButtonSprite));
        this->btnContent = (ButtonSprite*)(mem ? new ((void*)mem) ButtonSprite(0) : NULL);
    }

    /* btnScrollBar (+0x148) — Scrollbar handle, res 0x3CFE */
    {
        void* mem = operator_new(sizeof(ButtonSprite));
        this->btnScrollBar = (ButtonSprite*)(mem ? new ((void*)mem) ButtonSprite(0x3CFE) : NULL);
    }

    /* btnTextArea (+0x138) — Text area (dynamically set later) */
    {
        void* mem = operator_new(sizeof(ButtonSprite));
        this->btnTextArea = (ButtonSprite*)(mem ? new ((void*)mem) ButtonSprite(0) : NULL);
    }

    /* btnTextArea2 (+0x13C) — Secondary text area */
    {
        void* mem = operator_new(sizeof(ButtonSprite));
        this->btnTextArea2 = (ButtonSprite*)(mem ? new ((void*)mem) ButtonSprite(0) : NULL);
    }

    /* btnTextArea3 (+0x140) — Tertiary text area */
    {
        void* mem = operator_new(sizeof(ButtonSprite));
        this->btnTextArea3 = (ButtonSprite*)(mem ? new ((void*)mem) ButtonSprite(0) : NULL);
    }

    /* Zero first byte of each page in 200-page array (marks page as unused) */
    for (i = 0; i < 200; i++) {
        *(char*)((uintptr_t)&this->pages + i * 0x3C) = 0;
    }

    /* Initialize all index/state fields to -1 */
    this->currentPageIdx = -1;     /* +0x3040 */
    this->field_305C = -1;         /* +0x305C */
    this->field_3060 = -1;         /* +0x3060 */
    this->scrollOffset = -1;       /* +0x3044 (initialized to -1, then 0 after first show) */
    this->nextPageIdx = -1;        /* +0x3048 */
    this->prevPageIdx = -1;        /* +0x304C */
    this->nextPageLinkIdx = -1;    /* +0x3050 */
    this->prevPageLinkIdx = -1;    /* +0x3054 */
    this->field_3064 = -1;         /* +0x3064 */
    this->lineHeight = -1;         /* +0x3068 overwritten with -1 */

    /* Clear help data loaded flag */
    this->helpDataLoaded = 0;      /* +0x303C (byte) */

    /* Initialize remaining fields */
    this->animFrameCount = 0;      /* +0x150 */
    this->returnGameMode = 3;      /* +0x3074 — default game mode 3 */

    /* Zero pages array and work buffer. The original x86 body does this via
     * raw `this + 0x15C` dword arithmetic (+0x15C..+0x303C, 3000 dwords);
     * those literal offsets assume the original 32-bit layout (4-byte
     * ButtonSprite* members) and do not correspond to any real field on
     * this 64-bit build (8-byte pointers shift everything after them). A
     * literal port of that arithmetic silently zeroed btnAnim/btnContent/
     * btnScrollBar/btnTextArea moments after they were allocated above.
     * Zero the actual named fields instead — same original effect
     * (pages[200] and workBuffer both cleared). */
    std::memset(this->pages, 0, sizeof(this->pages));
    std::memset(this->workBuffer, 0, sizeof(this->workBuffer));

    /* Set page resource type and context window mode to 0 */
    this->pageResourceType = 0;    /* +0x306C */
    this->windowMode = 0;          /* +0x3070 */
}

/* ==================================================================== */
/* HelpWnd::create — Create the help window (vtable[5])                  */
/* Address: 0x450CA0                                                    */
/* ==================================================================== */
int HelpWnd::create(HWND hWndParent)
{
    /* Load window icon */
    this->hIcon = LoadIconA(this->hInstance, (LPCSTR)0x65);

    /* Get desktop dimensions */
    HWND hDesktop = GetDesktopWindow();
    RECT desktopRect;
    GetClientRect(hDesktop, &desktopRect);

    /* Window size: 0x240 x 0x170 (576 x 368) */
    int winLeft = 0;
    int winTop = 0;
    int winRight = 0x240;
    int winBottom = 0x170;

    /* Center on desktop */
    UI_CenterWindow(reinterpret_cast<int*>(&desktopRect), reinterpret_cast<int*>(&winLeft));

    /* Offset by -0x30 in both axes */
    OffsetRect(&desktopRect, -0x30, -0x30);

    /* Create via GameWindow::Create */
    int result = this->GameWindow::create(
        0,                              /* nCmdShow = SW_HIDE initially */
        hWndParent,
        winLeft, winTop,
        winRight - winLeft,
        winBottom - winTop,
        (HMENU)0,
        this->hIcon,
        0,                              /* classStyle */
        0x86000000,                     /* exStyle (WS_EX_APPWINDOW|TOPMOST|TOOLWINDOW) */
        0,                              /* unused */
        1                               /* showCursor */
    );

    return result;
}

/* ==================================================================== */
/* HelpWnd::show — Display the help window (vtable[2])                   */
/* Address: 0x450240                                                    */
/* ==================================================================== */
void HelpWnd::show(int pageTarget)
{
    /* Save current game mode */
    g_game_mode = this->returnGameMode;

    /* Handle fullscreen toggle */
    if (g_is_fullscreen == 1) {
        CGWND_SetFullscreenMode(1);
    }

    /* Reset screensaver mouse mode and invalidate tilemap */
    Game_SetScreenMode(g_game, 0, 0, 0);
    TileMap_InvalidateRect(g_tilemap,
        g_viewport_rect_left, g_viewport_rect_top,
        g_viewport_rect_right, g_viewport_rect_bottom);
    TileMap_InvalidateDirtyRects(g_tilemap, 0);

    /* Set game mode to 8 (help window active) */
    g_game_mode = 8;

    /* Load help page data on first show */
    if (this->helpDataLoaded == 0) {
        this->reset_pages();
        this->helpDataLoaded = 1;
    }

    /* Measure text line height */
    this->lineHeight = this->measure_text_height();

    /* Load the requested page */
    this->load_page(pageTarget);

    /* Update audio volume */
    if (g_audio != NULL) {
        GameAudio_UpdateVolume(g_audio, 1);
    }

    /* Initialize sprites if not already done */
    if (this->spritesInited != 1) {
        Sprite_Init(this->btnNext);
        Sprite_Init(this->btnPrevActual);
        Sprite_Init(this->btnClose);
        Sprite_Init(this->btnAnim);
        Sprite_Init(this->btnScrollBar);
        this->spritesInited = 1;
    }

    /* Call vtable[6] = init callback (Update client rect) */
    this->init();

    /* Reset scroll offset */
    this->scrollOffset = 0;

    /* Create 10ms animation timer (ID 0x54 = 84) */
    this->animTimerId = SetTimer(this->hWnd, 0x54, 10, NULL);

    /* Clear flags */
    this->active = 0;
    this->field_3064 = 0;

    /* Position window at center screen */
    this->GameWindow::set_position( g_screen_center_x - 0x120, g_screen_center_y - 0xB8);

    /* Update navigation button visibility */
    this->nextBtnEnabled = (this->nextPageIdx != -1) ? 1 : 0;
    this->prevBtnEnabled = (this->prevPageIdx != -1) ? 1 : 0;

    /* Scroll down button mirrors prev button state */
    this->scrollDownBtnEnabled = (this->prevBtnEnabled != 0) ? 1 : 0;

    /* Initialize animation frame count from sprite data */
    {
        void* animPixelData = this->btnAnim->pixelData;
        if (animPixelData != NULL) {
            /* NOTE: Raw offset access into ButtonSprite pixel data structure.
             * +0x20 = frame table pointer, +0x1E = default animation index.
             * TODO: Add named fields to pixel data structure. */
            void* frameTable = *(void**)((uintptr_t)animPixelData + 0x20);
            short frameIdx = *(short*)((uintptr_t)animPixelData + 0x1E);
            this->animFrameCount = (uint)*(unsigned short*)((uintptr_t)frameTable + frameIdx * 0x18);
        }
    }

    /* Call base GameWindow::Show */
    this->GameWindow::show();

    /* Dispatch visibility to context sub-window */
    int mode = this->windowMode;
    if (mode == 1) {
        UI_SetWindowVisible(g_town, 1);
    } else if (mode == 2) {
        UI_SetWindowVisible(g_postcard, 1);
    } else if (mode == 3) {
        UI_SetWindowVisible(g_cursor, 1);
    }
}

/* ==================================================================== */
/* HelpWnd::hide — Hide the help window (vtable[1])                      */
/* Address: 0x450AE0                                                    */
/* ==================================================================== */
void HelpWnd::hide()
{
    /* Call base GameWindow::Hide */
    this->GameWindow::hide();

    /* Clear active flag */
    this->active = 0;

    /* Restore cursor capture */
    Cursor_SetCapture((void*)this, 1);  /* binary passes HelpWnd* as Cursor* */

    /* Destroy sprite pixel data if initialized */
    if (this->spritesInited != 0) {
        Sprite_Destroy(this->btnNext);
        Sprite_Destroy(this->btnPrevActual);
        Sprite_Destroy(this->btnClose);
        Sprite_Destroy(this->btnAnim);
        if (this->btnContent->resourceId != 0) {
            Sprite_Destroy(this->btnContent);
        }
        Sprite_Destroy(this->btnScrollBar);
        this->spritesInited = 0;
    }

    /* Kill animation timer */
    KillTimer(this->hWnd, this->animTimerId);

    /* Release audio narration channel */
    if (this->audioChannel != NULL) {
        AudioChannel_Release(this->audioChannel);
    }

    /* Also release via GameAudio if available */
    if (g_audio != NULL && this->audioChannel != NULL) {
        /* NOTE: +0x38 = sound resource ID in AudioChannel.
         * TODO: Add named field to AudioChannel. */
        UINT sndId = *(UINT*)((uintptr_t)this->audioChannel + 0x38);
        if (sndId != 0) {
            int handle = ResourceManager_GetStringById(g_resmgr, sndId);
            ReleaseSoundResource(handle);
        }
        AudioChannel_Release(this->audioChannel);
    }

    /* Restore fullscreen mode if needed */
    if (this->wasFullscreen == 1) {
        CGWND_SetFullscreenMode(0);
    }

    /* Dispatch hide visibility to context sub-window */
    int mode = this->windowMode;
    if (mode == 1) {
        UI_SetWindowVisible(g_town, 0);
        /* NOTE: +0x5ED = Town flag field. TODO: Add named field to Town. */
        *(char*)((uintptr_t)g_town + 0x5ED) = 0;
    } else if (mode == 2) {
        UI_SetWindowVisible(g_postcard, 0);
        /* NOTE: +0x110 = PostcardAlbum destroyed flag. TODO: Add named field. */
        *(char*)((uintptr_t)g_postcard + 0x110) = 0;
    } else if (mode == 3) {
        UI_SetWindowVisible(g_cursor, 0);
        /* NOTE: +0xEC = Cursor flag. TODO: Add named field to Cursor. */
        *(int*)((uintptr_t)g_cursor + 0xEC) = 1;
    } else {
        /* Default mode: restore game mode 4 */
        g_game_mode = 4;
        TileMap_InvalidateRect(g_tilemap,
            g_viewport_rect_left, g_viewport_rect_top,
            g_viewport_rect_right, g_viewport_rect_bottom);
        TileMap_InvalidateDirtyRects(g_tilemap, 0);

        /* Check for new world creation */
        if (this->windowMode == 7 && this->pageResourceType == 0x2407) {
            extern void INPUT_NewWorld(InputMgr* mgr);   /* 0x41E120 */
            INPUT_NewWorld(&g_input_mgr);
        }
        g_game_mode = 8;
    }
}

/* ==================================================================== */
/* HelpWnd::wnd_proc — Windows message handler                           */
/* Address: 0x4518B0                                                    */
/* ==================================================================== */
LRESULT HelpWnd::wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    /* Handle WM_SYSCOMMAND (0x112) with SC_CLOSE (0xF140) */
    if (msg == 0x112 && (wParam & 0xFFF0) == 0xF140) {
        /* Call vtable[1] = hide() */
        this->hide();

        /* Determine mode to restore */
        int restoreMode;
        int mode = this->windowMode;
        if (mode == 1) {
            restoreMode = 5;
        } else if (mode == 2) {
            restoreMode = 6;
        } else if (mode == 3) {
            restoreMode = 7;
        } else {
            restoreMode = this->returnGameMode;
        }
        CGWND_SetMode(restoreMode);

        /* Post quit to break message loop */
        WIN32_PostQuit();
        return 0;
    }

    /* Default: pass through */
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

/* ==================================================================== */
/* HelpWnd::handle_click — Process mouse click                           */
/* Address: 0x451540                                                    */
/* ==================================================================== */
LRESULT HelpWnd::handle_click(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    /* Only handle clicks when active */
    if (this->active == 0) {
        return UI_DefWndProc(hWnd, msg, wParam, lParam);
    }

    int x = (short)(lParam & 0xFFFF);
    int y = (short)((uint)lParam >> 16);
    byte hitId = this->hit_test(x, y);

    switch (hitId) {
    case 1: /* Next page */
        this->highlight_button(1);
        Cursor_Render((void*)this, /* binary passes HelpWnd* as Cursor* */ (int)this->hWnd, 0, 0);
        Sleep(0x96);  /* 150ms */
        this->update_button_states(1);
        this->go_next_page();
        Cursor_Render((void*)this, /* binary passes HelpWnd* as Cursor* */ (int)this->hWnd, 0, 0);
        if (g_audio != NULL) {
            PlaySound(0x5015);
        }
        return 0;

    case 2: /* Prev page */
        this->highlight_button(2);
        Cursor_Render((void*)this, /* binary passes HelpWnd* as Cursor* */ (int)this->hWnd, 0, 0);
        Sleep(0x96);
        this->update_button_states(2);
        if (g_audio != NULL) {
            PlaySound(0x5015);
        }
        this->go_prev_page();
        Cursor_Render((void*)this, /* binary passes HelpWnd* as Cursor* */ (int)this->hWnd, 0, 0);
        return 0;

    case 3: /* Close button */
        this->highlight_button(3);
        Cursor_Render((void*)this, /* binary passes HelpWnd* as Cursor* */ (int)this->hWnd, 0, 0);
        Sleep(0x96);
        this->update_button_states(3);
        if (g_audio != NULL) {
            PlaySound(0x5015);
        }
        Cursor_Render((void*)this, /* binary passes HelpWnd* as Cursor* */ (int)this->hWnd, 0, 0);
        /* Call vtable[1] = hide() */
        this->hide();
        /* Restore game mode */
        {
            int mode = this->windowMode;
            if (mode == 1) {
                CGWND_SetMode(5);
            } else if (mode == 2) {
                CGWND_SetMode(6);
            } else if (mode == 3) {
                CGWND_SetMode(7);
            } else {
                CGWND_SetMode(this->returnGameMode);
            }
        }
        return 0;

    case 7: /* Content sprite click */
        {
            int contentResId = *(int*)((uintptr_t)(*(int*)(this->btnContent->pixelData)) + 4);
            if (contentResId == 0x3D01 || contentResId == 0x3D06) {
                /* Close/exit or continue — hide and restore mode */
                this->hide();
                int mode = this->windowMode;
                if (mode == 1) {
                    CGWND_SetMode(5);
                } else if (mode == 2) {
                    CGWND_SetMode(6);
                } else if (mode == 3) {
                    CGWND_SetMode(7);
                } else {
                    CGWND_SetMode(this->returnGameMode);
                }
            }
        }
        return 0;

    default:
        return 0;
    }
}

/* ==================================================================== */
/* HelpWnd::handle_mouse_move — Process mouse move                       */
/* Address: 0x4517B0                                                    */
/* ==================================================================== */
LRESULT HelpWnd::handle_mouse_move(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (this->active == 0) {
        return 0;
    }

    /* Let cursor system handle window paint */
    Cursor_HandleWindowPaint((void*)this, /* binary passes HelpWnd* as Cursor* */ (int)hWnd);

    /* Hit test */
    int x = (short)(lParam & 0xFFFF);
    int y = (short)(lParam >> 16);
    byte hitId = this->hit_test(x, y);

    switch (hitId) {
    case 1: /* Next page */
        if (this->nextBtnEnabled == 1) {
            /* Use cursor set 1 (hand/pointer cursor) — dispatch via vtable[3] */
            this->set_mode(&this->ddrawAuxCount1, &this->ddrawAuxPtr1, 0, 1);
            return 0;
        }
        break;
    case 2: /* Prev page */
        break;
    case 3: /* Close button */
        if (this->closeBtnEnabled == 1) {
            this->set_mode(&this->ddrawAuxCount2, &this->ddrawAuxPtr2, 0, 1);
            return 0;
        }
        break;
    }

    /* Check other interactive regions */
    if (this->prevBtnEnabled == 1 || this->closeBtnEnabled == 1 || this->scrollDownBtnEnabled == 1) {
        /* Use cursor set 2 (highlight/pointer cursor) via vtable[3] */
        this->set_mode(&this->ddrawAuxCount2, &this->ddrawAuxPtr2, 0, 1);
        return 0;
    }

    /* Default cursor */
    this->set_mode(&this->ddrawAuxCount1, &this->ddrawAuxPtr1, 0, 1);
    return 0;
}

/* ==================================================================== */
/* HelpWnd::cleanup_sprites — Destroy all 9 ButtonSprite objects          */
/* Address: 0x451440                                                    */
/* ==================================================================== */
void HelpWnd::cleanup_sprites()
{
    /* Destroy each ButtonSprite via delete (C++ handles scalar deleting dtor) */
    if (this->btnClose != NULL) {
        delete this->btnClose;
        this->btnClose = NULL;
    }
    if (this->btnNext != NULL) {
        delete this->btnNext;
        this->btnNext = NULL;
    }
    if (this->btnPrevActual != NULL) {
        delete this->btnPrevActual;
        this->btnPrevActual = NULL;
    }
    if (this->btnAnim != NULL) {
        delete this->btnAnim;
        this->btnAnim = NULL;
    }
    if (this->btnContent != NULL) {
        delete this->btnContent;
        this->btnContent = NULL;
    }
    if (this->btnScrollBar != NULL) {
        delete this->btnScrollBar;
        this->btnScrollBar = NULL;
    }
    if (this->btnTextArea != NULL) {
        delete this->btnTextArea;
        this->btnTextArea = NULL;
    }
    if (this->btnTextArea2 != NULL) {
        delete this->btnTextArea2;
        this->btnTextArea2 = NULL;
    }
    if (this->btnTextArea3 != NULL) {
        delete this->btnTextArea3;
        this->btnTextArea3 = NULL;
    }

    this->spritesInited = 0;
}

/* ==================================================================== */
/* HelpWnd::serialize_pages — Map mode/resourceType to page index        */
/* Address: 0x44F9A0                                                    */
/*                                                                      */
/* Returns a page index based on current windowMode and                  */
/* pageResourceType. Used to determine which help page to show.          */
/* ==================================================================== */
int HelpWnd::serialize_pages()
{
    switch (this->windowMode) {
    case 0:
    case 5:
        return 1;

    case 1:
        return 0x14;  /* 20 */

    case 2:
        return 0x1B;  /* 27 */

    case 3:
        return 0x1C;  /* 28 */

    case 4:
        switch (this->pageResourceType) {
        case 0x848:
            return 0x22;  /* 34 */
        case 0x818:
            return 0x23;  /* 35 */
        case 0xC54:
        case 0xC56:
        case 0xC58:
        case 0xC5A:
            return 0x24;  /* 36 */
        case 0xC5C:
            return 0x20;  /* 32 */
        }
        break;

    case 6:
        return 4;

    case 7:
        switch (this->pageResourceType) {
        case 0x2406: return 8;
        case 0x2407: return 9;
        case 0x2408: return 10;
        case 0x2409: return 0x10;  /* 16 */
        case 0x240A: return 0x0C;  /* 12 */
        case 0x240B: return 0x0B;  /* 11 */
        case 0x240D: return 0x0D;  /* 13 */
        }
        break;

    case 8:
        return 5;

    case 9:
        return 0x0F;  /* 15 */

    case 0xB:
        return 0x1D;  /* 29 */

    case 0xC:
        return 0x1E;  /* 30 */
    }

    return -1;
}

/* ==================================================================== */
/* HelpWnd::load_page_data — Build page data key string                  */
/* Address: 0x44F750                                                    */
/*                                                                      */
/* Builds a concatenated key string from windowMode + pageResourceType   */
/* into the output buffer. Used for INI-style key generation.            */
/* ==================================================================== */
void HelpWnd::load_page_data(char* outBuf)
{
    /* Build key: itoa(mode) + '#' + itoa(resourceType) */
    if (this->pageResourceType != 0) {
        char buf[20];

        /* First part: mode number */
        CRT_itoa(this->windowMode, buf, 10);

        /* Build: mode + "#" + resourceType */
        int outIdx = 0;

        /* Copy mode string */
        const char* src = buf;
        while (*src) {
            outBuf[outIdx++] = *src++;
        }

        /* Copy separator */
        src = "#";
        while (*src) {
            outBuf[outIdx++] = *src++;
        }

        /* Copy resource type string */
        CRT_itoa(this->pageResourceType, buf, 10);
        src = buf;
        while (*src) {
            outBuf[outIdx++] = *src++;
        }

        outBuf[outIdx] = '\0';
    } else {
        /* Just mode number */
        CRT_itoa(this->windowMode, outBuf, 10);
    }
}

/* HelpWnd::play_narration — Play narration audio and show help window   */
/* Address: 0x44F560                                                    */
/*                                                                      */
/* Plays narration audio for the current help page context and shows    */
/* the help window. Handles tutorial state tracking via INI settings.   */
/* ==================================================================== */
uint HelpWnd::play_narration(int windowMode, uint pageResourceType)
{
    this->windowMode = windowMode;
    this->pageResourceType = pageResourceType;

    /* Skip if multiplayer scenario 2 or demo mode */
    if (g_netman != NULL && *(int*)((uintptr_t)g_netman + 0x5C) == 2) {
        return pageResourceType & 0xFFFFFF00;
    }
    if (g_demo_mode == 1) {
        return pageResourceType & 0xFFFFFF00;
    }

    /* Serialize pages to get starting page index */
    int pageIdx = this->serialize_pages();
    this->currentPageIdx = pageIdx;

    /* Save fullscreen state */
    this->wasFullscreen = (uint8_t)g_is_fullscreen;

    /* Check tutorial settings */
    char iniBuf[1024];
    Config_GetIniString(g_config_ini, "TUTORIAL",
        (LPCSTR)((uintptr_t)g_player_config + 6), "", iniBuf, 0x400);

    /* Build search key from this->windowMode / this->pageResourceType */
    char keyBuf[20];
    this->load_page_data(keyBuf);

    /* Search for key in TUTORIAL string */
    char* found = strstr(iniBuf, keyBuf);
    if (found == NULL && this->currentPageIdx != -1) {
        if (windowMode != 0) {
            /* Save this tutorial as "watched" */
            Config_GetIniString(g_config_ini, "TUTORIAL",
                (LPCSTR)((uintptr_t)g_player_config + 6), "", iniBuf, 0x400);

            /* Build key again */
            char keyBuf2[20];
            this->load_page_data(keyBuf2);

            /* Append key to TUTORIAL string */
            if (strlen(iniBuf) > 0) {
                strcat(iniBuf, ",");
            }
            strcat(iniBuf, keyBuf2);

            Config_ReadInt(g_config_ini, "TUTORIAL",
                (LPCSTR)((uintptr_t)g_player_config + 6), iniBuf);
        }
    } else if (windowMode != 0) {
        /* Already watched — skip */
        return (uint)(uintptr_t)found & 0xFFFFFF00;
    }

    /* Show help window */
    this->returnGameMode = g_game_mode;
    CGWND_SetMode(8);
    this->load_page(this->currentPageIdx);
    this->show(this->currentPageIdx);

    return (this->currentPageIdx & 0xFFFFFF00) | 1;
}

/* ==================================================================== */
/* HelpWnd::reset_pages — Reset all page index/scroll state              */
/* Address: 0x44FB10                                                    */
/*                                                                      */
/* Loads help page data from help script files and resets navigation.   */
/* ==================================================================== */
char HelpWnd::reset_pages()
{
    char result = 0;
    int stream[0x41] = {0};
    int* loadedData = NULL;

    /* Reset all page indices */
    this->currentPageIdx = -1;
    this->scrollOffset = 0;
    this->nextPageIdx = -1;
    this->prevPageIdx = -1;
    this->nextPageLinkIdx = -1;
    this->prevPageLinkIdx = -1;
    this->lineHeight = 0;
    this->field_305C = -1;
    this->field_3060 = -1;
    this->field_3064 = -1;

    /* Disable navigation buttons */
    this->nextBtnEnabled = 0;
    this->prevBtnEnabled = 0;

    /* Open stream for reading */
    WIN32_StreamOpen(stream, 1);  /* mode 1 = read */

    /* Build help data file path */
    char fileBuf[0x200];
    {
        char installPath[256];
        strcpy(installPath, g_install_path);
        strcat(installPath, "\\help\\");
        strcat(installPath, "help.ini");
        strcpy(fileBuf, installPath);
    }

    /* Try loading from asset manager first */
    if (g_asset_mgr != NULL) {
        int fileSize = 0;
        loadedData = (int*)AssetMgr_LoadFile(g_asset_mgr, fileBuf, &fileSize);

        if (loadedData != NULL) {
            /* Create memory stream from loaded data */
            void* memStream = operator_new(0x5C);
            if (memStream != NULL) {
                int* streamObj = WNDPROC_StreamFromMemory(memStream, (char*)loadedData, fileSize, 1);
                if (streamObj != NULL) {
                    result = (char)this->load_help_data(streamObj);
                    /* Release stream via vtable[1] scalar deleting destructor.
                     * TODO: decompile stream class — replace with 'delete streamObj'
                     * once the stream class has a proper C++ destructor. */
                    stream_vtable_scalar_dtor(streamObj);
                }
            }
            CRT_free(loadedData);
        }
    }

    /* Fallback: load directly from file */
    if (result == 0) {
        WIN32_StreamOpenPath(stream, fileBuf, 0x20, 0x479190);
        result = (char)this->load_help_data(stream);
        WIN32_StreamDestroyImmediate(stream);
    }

    /* Cleanup */
    WIN32_StreamDestroy(stream);
    WNDPROC_StreamCleanup(stream);

    return result;
}

/* ==================================================================== */
/* HelpWnd::load_help_data — Parse help page data from script-file stream*/
/* Address: 0x44FC80                                                    */
/*                                                                      */
/* Reads lines from a script-file stream via WNDPROC_CriticalSectionLock,*/
/* parsing each line into HelpPageData entries. Each page data line      */
/* contains: text resource ID, sound ID, overlay flag, and two RECTs.   */
/* ==================================================================== */
int HelpWnd::load_help_data(void* stream)
{
    char buf[0x100];  /* 256-byte line buffer */

    /* Lock stream and read first line */
    WNDPROC_CriticalSectionLock((int*)stream, buf);
    char* pageData = strstr(buf, "\n");  /* look for newline/separator */

    int pageIdx = 0;

    if (pageData != NULL) {
        /* Process pages while stream has data */
        while ((*(byte*)((uintptr_t)(*(int*)stream + 8 + (uintptr_t)stream)) & 1) == 0) {
            /* Read page ID */
            pageIdx = CRT_atoi(buf);
            WNDPROC_CriticalSectionLock((int*)stream, buf);

            /* Check for backslash line terminator (safety check) */
            if (buf[strlen(buf) - 2] != '\\') {
                return 0;  /* corrupt data */
            }

            WNDPROC_CriticalSectionLock((int*)stream, buf);

            /* Parse field_10: if line starts with '\', read relative offset */
            if (buf[0] == '\\') {
                this->pages[pageIdx].field_10 = CRT_atoi(buf + 1);
                WNDPROC_CriticalSectionLock((int*)stream, buf);
            } else {
                this->pages[pageIdx].field_10 = 0xAF;  /* default */
            }

            /* Parse soundResId: if line starts with '@', read explicit sound */
            if (buf[0] == '@') {
                this->pages[pageIdx].soundResId = CRT_atoi(buf + 1);
                WNDPROC_CriticalSectionLock((int*)stream, buf);
            } else {
                this->pages[pageIdx].soundResId = 0x50F8;  /* default narration */
            }

            /* Parse hasOverlay: '#' prefix = 1 */
            if (buf[0] == '#') {
                this->pages[pageIdx].hasOverlay = 1;
                WNDPROC_CriticalSectionLock((int*)stream, buf);
            } else {
                this->pages[pageIdx].hasOverlay = 0;
            }

            /* Parse text resource ID */
            this->pages[pageIdx].pageId = CRT_atoi(buf);
            WNDPROC_CriticalSectionLock((int*)stream, buf);

            /* Check for backslash terminator */
            if (buf[strlen(buf) - 2] != '\\') {
                return 0;
            }

            WNDPROC_CriticalSectionLock((int*)stream, buf);

            /* Parse textResId */
            this->pages[pageIdx].textResId = CRT_atoi(buf);
            WNDPROC_CriticalSectionLock((int*)stream, buf);

            /* Check for backslash terminator */
            if (buf[strlen(buf) - 2] != '\\') {
                return 0;
            }

            WNDPROC_CriticalSectionLock((int*)stream, buf);

            /* Parse nextPageId / link data */
            this->pages[pageIdx].nextPageId = CRT_atoi(buf);
            WNDPROC_CriticalSectionLock((int*)stream, buf);

            /* Check for backslash terminator */
            if (buf[strlen(buf) - 2] != '\\') {
                return 0;
            }

            /* Mark page as present — writes 1 to pages[pageIdx + 6].pageId
             * byte 0. The +6 stride means the validity marker is stored 6 pages
             * ahead. Verified at 0x44FC80:
             *   *(undefined1 *)(this + (iVar4 + 6) * 0x3c) = 1; */
            *(char*)((uintptr_t)&this->pages + (pageIdx + 6) * 0x3C) = 1;

            WNDPROC_CriticalSectionLock((int*)stream, buf);

            /* If line starts with '\', it has additional click rect + overlay rect data */
            if (buf[0] == '\\') {
                /* Copy line to temporary buffer with space terminator handling */
                char tmpBuf[0x100];
                strcpy(tmpBuf, buf);
                tmpBuf[0] = ' ';  /* Replace leading backslash with space */

                /* Parse clickRect from space-separated values */
                char* token = strtok(tmpBuf, " ");
                token = strtok(NULL, " ");
                this->pages[pageIdx].clickRect.left = CRT_atoi(token);

                token = strtok(NULL, " ");
                this->pages[pageIdx].clickRect.top = CRT_atoi(token);

                token = strtok(NULL, " ");
                this->pages[pageIdx].clickRect.right = CRT_atoi(token);

                token = strtok(NULL, " ");
                this->pages[pageIdx].clickRect.bottom = CRT_atoi(token);

                WNDPROC_CriticalSectionLock((int*)stream, buf);

                /* Parse overlayRect from next line */
                strcpy(tmpBuf, buf);
                tmpBuf[0] = ' ';

                token = strtok(tmpBuf, " ");
                token = strtok(NULL, " ");
                this->pages[pageIdx].overlayRect.left = CRT_atoi(token);

                token = strtok(NULL, " ");
                this->pages[pageIdx].overlayRect.top = CRT_atoi(token);

                token = strtok(NULL, " ");
                this->pages[pageIdx].overlayRect.right = CRT_atoi(token);

                token = strtok(NULL, " ");
                this->pages[pageIdx].overlayRect.bottom = CRT_atoi(token);

                WNDPROC_CriticalSectionLock((int*)stream, buf);
            } else {
                /* No rect data — set both RECTs empty */
                SetRectEmpty(&this->pages[pageIdx].clickRect);
                SetRectEmpty(&this->pages[pageIdx].overlayRect);
            }

            /* Check for next page separator (\n pattern) */
            if (strstr(buf, "\n") == NULL) {
                return 1;  /* Success — end of data */
            }
        }
    }

    return 1;  /* Success */
}

/* ==================================================================== */
/* HelpWnd::update_scroll — Update scroll position and nav links         */
/* Address: 0x4500A0                                                    */
/* ==================================================================== */
void HelpWnd::update_scroll()
{
    /* Step 1: Determine nextPageIdx based on scrollOffset */
    if (this->scrollOffset < 1) {
        /* At top — next page is the previous sequential page */
        this->nextPageLinkIdx = -1;
        this->nextPageIdx = this->currentPageIdx - 1;

        /* Skip pages whose validity marker byte is 0.
         * NOTE: (nextPageIdx + 5) * 0x3C accesses pages[nextPageIdx + 5].pageId
         * byte 0. The +5 offset means we look 5 entries ahead in the page array.
         * Verified against binary at 0x4500A0. */
        while (this->nextPageIdx >= 0 &&
               *(char*)((uintptr_t)&this->pages + (this->nextPageIdx + 5) * 0x3C) == 0) {
            this->nextPageIdx--;
            if (this->nextPageIdx < 1) {
                this->nextPageIdx = -1;
                break;
            }
        }
    } else {
        /* Scrolled down — next page is current (stay on same page) */
        this->nextPageIdx = this->currentPageIdx;
        this->nextPageLinkIdx = this->scrollOffset - 1;
    }

    /* Step 2: Measure text lines to determine total line count */
    int hdc = Cursor_WaitForBlit((void*)this)  /* binary passes HelpWnd* as Cursor* */;
    int lineCount = 0;

    if (this->helpDataLoaded == 0) {
        lineCount = 1;
    } else {
        int result = this->draw_text(0, &hdc);
        if (result >= 0) {
            while (lineCount < 200) {
                lineCount++;
                unsigned int nextResult = this->draw_text(lineCount, &hdc);
                if ((int)nextResult < 0) break;
            }
        }
    }

    Cursor_Render((void*)this, /* binary passes HelpWnd* as Cursor* */ (int)this->hWnd, hdc, 1);

    /* Step 3: Determine prevPageIdx based on total lines */
    int curPage = this->currentPageIdx;
    if (this->scrollOffset < lineCount - 1) {
        /* More text below — prev page is current page (scroll up) */
        this->prevPageIdx = curPage;
        this->prevPageLinkIdx = this->scrollOffset + 1;
    } else {
        /* At text end — prev page is next sequential page */
        this->prevPageLinkIdx = -1;
        this->prevPageIdx = curPage + 1;

        /* Skip pages with validity marker byte == 0.
         * NOTE: (prevPageIdx + 7) * 0x3C accesses pages[prevPageIdx + 7].pageId
         * byte 0. The +7 offset looks 7 entries ahead. Verified at 0x4500A0. */
        while (this->prevPageIdx <= 199 &&
               *(char*)((uintptr_t)&this->pages + (this->prevPageIdx + 7) * 0x3C) == 0) {
            this->prevPageIdx++;
            if (this->prevPageIdx > 199) {
                this->prevPageIdx = -1;
                break;
            }
        }
    }

    /* Step 4: Disable next/prev if page has overlay and windowMode != 0.
     * Verified at 0x4500A0. hasOverlay is at pageData[].hasOverlay (+0x14). */
    if (this->nextPageIdx != -1 && this->windowMode != 0 &&
        this->nextPageIdx != curPage &&
        this->pages[this->nextPageIdx].hasOverlay == 1) {
        this->nextPageIdx = -1;
    }
    if (this->prevPageIdx != -1 && this->windowMode != 0 &&
        this->prevPageIdx != curPage &&
        this->pages[curPage].hasOverlay == 1) {
        this->prevPageIdx = -1;
    }
}


/* ==================================================================== */
/* HelpWnd::update_anim — Animation frame tick (vtable[7])              */
/* Address: 0x450450                                                    */
/* ==================================================================== */
void HelpWnd::update_anim(int param)
{
    (void)param;  /* unused — binary passes only 'this' in ECX */

    if (this->audioChannel == NULL) return;

    bool needsRender = false;
    this->field_3064++;
    if (this->field_3064 == 100) this->field_3064 = 0;

    if ((this->field_3064 & 1) == 0) {
        if (AudioChannel_IsActive((int)this->audioChannel) == 0) {
            void* pd = this->btnAnim->pixelData;
            if (pd != NULL) {
                short fi = *(short*)((uintptr_t)pd + 0x1E);
                void* ft = *(void**)((uintptr_t)pd + 0x20);
                unsigned short mf = *(unsigned short*)((uintptr_t)ft + fi * 0x18 + 2);
                if (this->animFrameCount < (int)mf)
                    this->animFrameCount++;
                else
                    this->animFrameCount = *(unsigned short*)((uintptr_t)ft + fi * 0x18);
            }
        } else {
            void* pd = this->btnAnim->pixelData;
            if (pd != NULL) {
                short fi = *(short*)((uintptr_t)pd + 0x1E);
                void* ft = *(void**)((uintptr_t)pd + 0x20);
                unsigned short hf = *(unsigned short*)((uintptr_t)ft + fi * 0x18 + 0x18);
                if (this->animFrameCount == hf) goto skip_render;
                this->animFrameCount = hf;
            }
        }
        this->update_button_states(6);
        needsRender = true;
    }
skip_render:
    if (needsRender)
        Cursor_Render((Cursor*)this, (uintptr_t)this->hWnd, 0, 0);
}

/* ==================================================================== */
/* HelpWnd::load_page — Load and display a specific help page           */
/* Address: 0x450520                                                    */
/* ==================================================================== */
void HelpWnd::load_page(int pageIdx)
{
    if (this->spritesInited != 0) {
        void* pd = this->btnAnim->pixelData;
        if (pd != NULL) {
            short fi = *(short*)((uintptr_t)pd + 0x1E);
            void* ft = *(void**)((uintptr_t)pd + 0x20);
            this->animFrameCount = *(unsigned short*)((uintptr_t)ft + fi * 0x18);
        }
    }
    this->currentPageIdx = pageIdx;
    if (this->btnContent->resourceId != 0)
        Sprite_Destroy(this->btnContent);
    this->btnContent->resourceId = this->pages[pageIdx].pageId;
    Sprite_Init(this->btnContent);

    void* cpd = this->btnContent->pixelData;
    if (cpd == NULL || *(int*)((uintptr_t)cpd + 0x14) == 0 || *(int*)((uintptr_t)cpd + 0x18) == 0) {
        RECT taRect;
        CopyRect(&taRect, (RECT*)((uintptr_t)this->btnTextArea + 4));
        taRect.bottom = ((taRect.top + 0x96) -
            ((taRect.top + 0x96) - taRect.top) % this->lineHeight) - 1;
        *(RECT*)((uintptr_t)this->btnTextArea + 4) = taRect;
    } else {
        RECT cr;
        SetRect(&cr, 0, 0,
            *(unsigned short*)((uintptr_t)(*(int*)((uintptr_t)cpd + 0x14)) + 0x14),
            *(unsigned short*)((uintptr_t)(*(int*)((uintptr_t)cpd + 0x14)) + 0x16));
        OffsetRect(&cr, 0xE8, 0);
        OffsetRect(&cr, 0x96, 0xB2);
        void* sd = *(void**)((uintptr_t)cpd + 0x14);
        OffsetRect(&cr,
            -(int)(*(unsigned short*)((uintptr_t)sd + 0x14) >> 1),
            -(int)(*(unsigned short*)((uintptr_t)sd + 0x16) >> 1));
        CopyRect((RECT*)((uintptr_t)this->btnTextArea + 4), &cr);
    }

    this->update_scroll();
    this->update_button_states(1);
    this->update_button_states(2);
    this->update_button_states(3);
    this->update_button_states(7);
    this->update_button_states(4);
    this->update_button_states(8);
    this->update_button_states(9);
    this->update_button_states(5);
}

/* ==================================================================== */
/* HelpWnd::go_next_page — Navigate to next help page                   */
/* Address: 0x451920                                                    */
/* ==================================================================== */
void HelpWnd::go_next_page()
{
    int nextIdx = this->nextPageIdx;
    if (nextIdx == -1) return;

    if (nextIdx == this->currentPageIdx) {
        this->scrollOffset = this->nextPageLinkIdx;
        this->load_page(this->currentPageIdx);
        if (g_audio != NULL) {
            UINT sndId = this->pages[this->currentPageIdx].soundResId;
            if (this->audioChannel != NULL) {
                UINT oid = *(UINT*)((uintptr_t)this->audioChannel + 0x38);
                if (oid != 0) ReleaseSoundResource(ResourceManager_GetStringById(g_resmgr, oid));
                AudioChannel_Release(this->audioChannel);
            }
            if (sndId != 0) {
                LoadSoundResource(ResourceManager_GetStringById(g_resmgr, sndId));
                GameAudio_PlayResourceEx(g_audio, sndId, (int*)&this->audioChannel);
            }
        }
        if (this->active != 0) {
            for (int i = 1; i <= 9; i++) this->update_button_states(i);
            KillTimer(this->hWnd, this->animTimerId);
            this->animTimerId = SetTimer(this->hWnd, 0x54, 10, NULL);
        }
    } else {
        this->load_page(nextIdx);
        int hdc = Cursor_WaitForBlit((Cursor*)this);
        int lc = 0;
        if (this->helpDataLoaded == 0) { lc = 1; }
        else {
            int r = this->draw_text(0, &hdc);
            if (r >= 0) {
                while (lc < 200) { lc++; if ((int)this->draw_text(lc, &hdc) < 0) break; }
            }
        }
        Cursor_Render((Cursor*)this, (uintptr_t)this->hWnd, hdc, 1);
        if (lc > 1) {
            this->scrollOffset = lc - 1;
            this->load_page(this->currentPageIdx);
            if (g_audio != NULL) {
                UINT sndId = this->pages[this->currentPageIdx].soundResId;
                if (this->audioChannel != NULL) {
                    UINT oid = *(UINT*)((uintptr_t)this->audioChannel + 0x38);
                    if (oid != 0) ReleaseSoundResource(ResourceManager_GetStringById(g_resmgr, oid));
                    AudioChannel_Release(this->audioChannel);
                }
                if (sndId != 0) {
                    LoadSoundResource(ResourceManager_GetStringById(g_resmgr, sndId));
                    GameAudio_PlayResourceEx(g_audio, sndId, (int*)&this->audioChannel);
                }
            }
            if (this->active != 0) {
                for (int i = 1; i <= 9; i++) this->update_button_states(i);
                KillTimer(this->hWnd, this->animTimerId);
                this->animTimerId = SetTimer(this->hWnd, 0x54, 10, NULL);
            }
        }
    }
}

/* ==================================================================== */
/* HelpWnd::go_prev_page — Navigate to previous help page               */
/* Address: 0x451C60                                                    */
/* ==================================================================== */
void HelpWnd::go_prev_page()
{
    int prevIdx = this->prevPageIdx;
    if (prevIdx == -1) return;

    if (prevIdx == this->currentPageIdx) {
        this->scrollOffset = this->prevPageLinkIdx;
        this->load_page(this->currentPageIdx);
        if (g_audio != NULL) {
            UINT sndId = this->pages[this->currentPageIdx].soundResId;
            if (this->audioChannel != NULL) {
                UINT oid = *(UINT*)((uintptr_t)this->audioChannel + 0x38);
                if (oid != 0) ReleaseSoundResource(ResourceManager_GetStringById(g_resmgr, oid));
                AudioChannel_Release(this->audioChannel);
            }
            if (sndId != 0) {
                LoadSoundResource(ResourceManager_GetStringById(g_resmgr, sndId));
                GameAudio_PlayResourceEx(g_audio, sndId, (int*)&this->audioChannel);
            }
        }
        if (this->active != 0) {
            for (int i = 1; i <= 9; i++) this->update_button_states(i);
            KillTimer(this->hWnd, this->animTimerId);
            this->animTimerId = SetTimer(this->hWnd, 0x54, 10, NULL);
        }
    } else {
        this->scrollOffset = 0;
        this->load_page(prevIdx);
        if (g_audio != NULL) {
            UINT sndId = this->pages[this->currentPageIdx].soundResId;
            if (this->audioChannel != NULL) {
                UINT oid = *(UINT*)((uintptr_t)this->audioChannel + 0x38);
                if (oid != 0) ReleaseSoundResource(ResourceManager_GetStringById(g_resmgr, oid));
                AudioChannel_Release(this->audioChannel);
            }
            if (sndId != 0) {
                LoadSoundResource(ResourceManager_GetStringById(g_resmgr, sndId));
                GameAudio_PlayResourceEx(g_audio, sndId, (int*)&this->audioChannel);
            }
        }
        if (this->active != 0) {
            for (int i = 1; i <= 9; i++) this->update_button_states(i);
            KillTimer(this->hWnd, this->animTimerId);
            this->animTimerId = SetTimer(this->hWnd, 0x54, 10, NULL);
        }
    }
}

/* ==================================================================== */
/* HelpWnd::highlight_button — Set button sprite to highlighted state   */
/* Address: 0x4527B0                                                    */
/* ==================================================================== */
void HelpWnd::highlight_button(int buttonId)
{
    switch (buttonId) {
    case 1:
        if (this->nextBtnEnabled == 1)
            Sprite_SetState(this->btnNext, 1, reinterpret_cast<int*>(this->backbufferSurface));
        break;
    case 2:
        if (this->prevBtnEnabled == 1)
            Sprite_SetState(this->btnPrevActual, 1, reinterpret_cast<int*>(this->backbufferSurface));
        break;
    case 3:
        if (this->closeBtnEnabled == 1)
            Sprite_SetState(this->btnClose, 1, reinterpret_cast<int*>(this->backbufferSurface));
        break;
    case 6:
        Sprite_SetState(this->btnAnim, 0, reinterpret_cast<int*>(this->backbufferSurface));
        break;
    case 7: {
        void* pd = this->btnContent->pixelData;
        if (pd != NULL && *(int*)((uintptr_t)pd + 0x14) != 0 && *(int*)((uintptr_t)pd + 0x18) != 0)
            Sprite_SetState(this->btnContent, 0, reinterpret_cast<int*>(this->backbufferSurface));
        break;
    }
    case 8:
        Sprite_SetState(this->btnScrollBar, 0, reinterpret_cast<int*>(this->backbufferSurface));
        break;
    case 9:
        if (this->scrollDownBtnEnabled == 1) {
            int hdc = Cursor_WaitForBlit((Cursor*)this);
            this->render_scroll_down(&hdc);
            Cursor_Render((Cursor*)this, (uintptr_t)this->hWnd, hdc, 1);
        }
        break;
    }
}

/* ==================================================================== */
/* HelpWnd::update_button_states — Update all button sprite states      */
/* Address: 0x451FB0                                                    */
/* ==================================================================== */
void HelpWnd::update_button_states(int buttonId)
{
    switch (buttonId) {
    case 1:
        Sprite_SetState(this->btnNext, this->nextBtnEnabled == 1 ? 0 : 2, reinterpret_cast<int*>(this->backbufferSurface));
        break;
    case 2:
        Sprite_SetState(this->btnPrevActual, this->prevBtnEnabled == 1 ? 0 : 2, reinterpret_cast<int*>(this->backbufferSurface));
        break;
    case 3:
        Sprite_SetState(this->btnClose, this->closeBtnEnabled == 1 ? 0 : 2, reinterpret_cast<int*>(this->backbufferSurface));
        break;
    case 4: {
        int hdc = Cursor_WaitForBlit((Cursor*)this);
        this->render_page(&hdc);
        break;
    }
    case 5: {
        int hdc = Cursor_WaitForBlit((Cursor*)this);
        this->render_scroll_up(&hdc);
        Cursor_Render((Cursor*)this, (uintptr_t)this->hWnd, hdc, 1);
        break;
    }
    case 6:
        this->update_anim_sprite(this->animFrameCount);
        break;
    case 7: {
        void* pd = this->btnContent->pixelData;
        if (pd != NULL && *(int*)((uintptr_t)pd + 0x14) != 0 && *(int*)((uintptr_t)pd + 0x18) != 0) {
            Sprite_SetState(this->btnContent, 0, reinterpret_cast<int*>(this->backbufferSurface));
            void* sp = *(void**)((uintptr_t)pd + 0x14);
            if (sp != NULL && *(char*)((uintptr_t)sp + 0x17) == 1)
                Sprite_SetState(this->btnContent, 1, reinterpret_cast<int*>(this->backbufferSurface));
        }
        break;
    }
    case 8:
        this->draw_scroll_indicator();
        break;
    case 9:
        if (this->scrollDownBtnEnabled != 0) {
            int hdc = Cursor_WaitForBlit((Cursor*)this);
            this->render_scroll_down(&hdc);
            Cursor_Render((Cursor*)this, (uintptr_t)this->hWnd, hdc, 1);
        }
        break;
    }
}

/* ==================================================================== */
/* HelpWnd::draw_text — Draw one line of help text for scroll position  */
/* Address: 0x450850                                                    */
/*                                                                      */
/* TODO: decompile 0x450850 — full word-wrap/DrawTextA logic.           */
/* Stub returns -1 (EOF). Tracked in PROGRESS.md.                       */
/* ==================================================================== */
int HelpWnd::draw_text(int lineIdx, int* hdc_p)
{
    (void)lineIdx;
    (void)hdc_p;
    return -1;
}

/* ==================================================================== */
/* HelpWnd::measure_text_height — Measure height of one line of text    */
/* Address: 0x452170                                                    */
/* ==================================================================== */
int HelpWnd::measure_text_height()
{
    int hdc = Cursor_WaitForBlit((Cursor*)this);
    int oldColor = SetTextColor((void*)(uintptr_t)hdc, 0xFF5C00);
    int oldMode = SetBkMode((void*)(uintptr_t)hdc, 1);
    void* oldFont = SelectObject((void*)(uintptr_t)hdc, g_font_small);

    RECT mr;
    SetRect(&mr, 0, 0, 0xD9, 0x96);
    OffsetRect(&mr, 0x2A, 0x23);

    int result = DrawTextA((void*)(uintptr_t)hdc, s_measure_test_char, 1, &mr, 0x18C10);

    SelectObject((void*)(uintptr_t)hdc, oldFont);
    SetBkMode((void*)(uintptr_t)hdc, oldMode);
    SetTextColor((void*)(uintptr_t)hdc, oldColor);
    Cursor_Render((Cursor*)this, (uintptr_t)this->hWnd, hdc, 1);
    return result;
}

/* ==================================================================== */
/* HelpWnd::hit_test — Determine which button was clicked/hovered        */
/* Address: 0x451E90                                                    */
/* ==================================================================== */
byte HelpWnd::hit_test(int x, int y)
{
    POINT pt{ x, y };
    if (this->nextBtnEnabled != 0 &&
        PtInRect((RECT*)((uintptr_t)this->btnNext + 4), pt)) return 1;
    if (this->prevBtnEnabled != 0 &&
        PtInRect((RECT*)((uintptr_t)this->btnPrevActual + 4), pt)) return 2;
    if (PtInRect((RECT*)((uintptr_t)this->btnClose + 4), pt)) return 3;
    if (PtInRect((RECT*)((uintptr_t)this->btnContent + 4), pt)) return 7;
    if (PtInRect((RECT*)((uintptr_t)this->btnAnim + 4), pt)) return 6;
    if (PtInRect((RECT*)((uintptr_t)this->btnTextArea + 4), pt)) return 4;
    if (PtInRect((RECT*)((uintptr_t)this->btnTextArea2 + 4), pt)) return 5;
    if (PtInRect((RECT*)((uintptr_t)this->btnScrollBar + 4), pt)) return 8;
    return 0;
}

#pragma GCC diagnostic pop
