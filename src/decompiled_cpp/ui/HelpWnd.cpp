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
 * Vtable layout (VTBL_HELPWND = 0x478428):
 *   [0] +0x00: scalar deleting destructor  (0x44F4F0)
 *   [1] +0x04: Hide                        (0x450AE0)
 *   [2] +0x08: Show                        (0x450240)
 *   [3] +0x0C: set_mode (Cursor dispatch)  (inherited, 0x414340)
 *   [4] +0x10: cleanup_sprites             (0x451440)
 *   [5] +0x14: Create                      (0x450CA0)
 *   [6] +0x18: init                        (0x451180)
 *   [7] +0x1C: on_show / update_anim       (0x450450)
 *
 * HelpPageNode (VTBL_HELPPAGE_NODE = 0x4783D8):
 *   [0] +0x00: scalar deleting destructor  (0x44F2A0)
 *   [1] +0x04: base destructor             (0x44F2C0)
 *
 * NOTE: find_page, find_page_scalar_dtor, and find_page_base_dtor
 * are actually HelpPageNode methods. TODO: move them to a separate
 * HelpPageNode class once class hierarchy is fully decompiled.
 */

#include "HelpWnd.h"
#include "ButtonSprite.h"
#include <cstdint>
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */

/* HelpPageNode vtable — standalone non-member class */
/* TODO: Move to HelpPageNode.h once HelpPageNode class is created */
#define VTBL_HELPPAGE_NODE ((void*)0x4783D8)

/* ==================================================================== */
/* External declarations                                                */
/* ==================================================================== */

/* Heap */
    extern void* __cdecl operator_new(size_t size);        /* 0x465CE0 */
    extern void  __cdecl GLOBAL_free(void* ptr);            /* 0x465CD0 */
    extern void  __cdecl CRT_free(void* ptr);               /* 0x466510? */
    extern int   __cdecl CRT_atoi(const char* str);         /* 0x466622 */
    extern int   __cdecl CRT_itoa(int val, char* buf, int radix);
    extern char* __cdecl CRT_strtok(char* str, const char* delimiters);

extern "C" {
    /* Win32 API */
    extern BOOL   __stdcall GetDesktopWindow(void);
    extern BOOL   __stdcall GetClientRect(HWND, void* lpRect);
    extern HICON  __stdcall LoadIconA(HINSTANCE, LPCSTR);
    extern BOOL   __stdcall SetRect(void* rect, int l, int t, int r, int b);
    extern BOOL   __stdcall OffsetRect(void* rect, int dx, int dy);
    extern BOOL   __stdcall CopyRect(void* dst, void* src);
    extern BOOL   __stdcall PtInRect(const void* rect, int x, int y);
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
}

    /* Game functions */
    extern void   Sprite_Init(void* sprite);                /* 0x454BF0 */
    extern void   Sprite_Destroy(void* sprite);             /* 0x454BC0 */
    extern void   Sprite_SetState(void* sprite, int state, void* surface); /* 0x454C30 */
    extern BOOL   UIPANEL_Blit(void* sprite, uint dstX, uint dstY,
                                int dstW, uint dstH, void* surface,
                                int srcX, int srcY, int srcW, int srcH, int mode); /* 0x42B050 */
    extern void   FormatResourceString(void* resmgr, UINT id,
                                        char* buf, int maxLen); /* 0x447330 */
    extern int    ResourceManager_GetStringById(void* resmgr, UINT id); /* 0x4472B0 */
    extern void   RESMGR_LoadSoundResource(int handle);       /* 0x448EE0 */
    extern void   RESMGR_ReleaseSoundResource(int handle);    /* 0x448EE0 */
    extern void   GameAudio_PlayResourceEx(void* audio, UINT resId, int* outChannel); /* 0x458DC0 */
    extern void   AudioChannel_Release(void* channel);        /* 0x458F40 */
    extern void   GameAudio_UpdateVolume(void* audio, char flag); /* 0x459110 */
    extern int    AudioChannel_IsActive(int channel);         /* 0x458F20 */
    extern int    Cursor_WaitForBlit(void* self);             /* 0x416380 — returns HDC */
    extern void   Cursor_Render(void* cursor, uintptr_t hWnd,
                                 int hdc, char flag);          /* 0x416420 */
    extern void   Cursor_SetCapture(void* cursor, int captured); /* 0x416B20 */
    extern void   Cursor_HandleWindowPaint(void* cursor, uintptr_t hWnd); /* 0x416C40 */
    extern BOOL   GameWindow_Create(void* self, int nCmdShow, HWND hParent,
                                     int x, int y, int nWidth, int nHeight,
                                     HMENU hMenu, HICON hIcon, UINT classStyle,
                                     int exStyle, int unused, uint8_t showCursor);
    extern void   GameWindow_Hide(void* self);                /* 0x413C10 */
    extern void   GameWindow_Show(void* self);                /* 0x413D10 */
    extern void   GameWindow_SetPosition(void* self, int x, int y); /* 0x413D90 */
    extern void   GameWindow_BaseDtor(void* self);            /* 0x413B70 */
    extern void   UI_CenterWindow(void* outerRect, void* innerRect); /* 0x425A50 */
    extern void   UI_SetWindowVisible(void* wnd, char visible); /* 0x425F20 */
    extern LRESULT UI_DefWndProc(HWND, UINT, WPARAM, LPARAM);  /* 0x422EA0 */
    extern void   CGWND_SetFullscreenMode(char flag);          /* 0x40BCF0 */
    extern void   CGWND_SetMode(void* mode);                   /* 0x40BD50 */
    extern int    Game_SetScreenMode(void* game, char a, char b, char c); /* 0x40EA10 */
    extern void   TileMap_InvalidateRect(void* tilemap, int l, int t,
                                          int r, int b);        /* 0x446330 */
    extern void   TileMap_InvalidateDirtyRects(void* tilemap, char flag); /* 0x446680 */
    extern void   PlaySound(int resId);                         /* 0x459930 */
    extern void   WIN32_StreamOpen(void* stream, int mode);
    extern void   WIN32_StreamOpenPath(void* stream, const char* path,
                                        int mode, int unknown);
    extern void   WIN32_StreamDestroy(void* stream);
    extern void   WIN32_StreamDestroyImmediate(void* stream);
    extern void   WNDPROC_CriticalSectionLock(int* stream, char* buf);
    extern void   WNDPROC_StreamCleanup(void* stream);
    extern void*  WNDPROC_StreamFromMemory(void* self, char* data, int size, int mode);
    extern void*  AssetMgr_LoadFile(void* mgr, const char* path, int* outSize);
    extern void   RESDATA_GameVehicle_Ctor(void* self, int resId);
    extern void   RESDATA_GameVehicle_BaseDtor(void* self);
    extern void   Vehicle_LoadSounds(void* vehicle, int* pageData, char flag);
    extern void   Vehicle_SetState(void* vehicle, int state);
    extern void   GameObject_Update(void* obj);                /* 0x436AE0 */
    extern int    CGWND_TrackPiece_SetZoom(void* obj, int zoom); /* 0x413A30 */
    extern void   Config_GetIniString(void* config, const char* section,
                                       const char* key, const char* def,
                                       char* out, int maxLen);
    extern void   Config_ReadInt(void* config, const char* section,
                                  const char* key, char* out);
    extern void   WIN32_PostQuit(void);                        /* 0x462560 */

/* ================================================================== */
/* Global variables                                                     */
/* ================================================================== */

extern HelpWnd* g_audio_mgr;           /* 0x4FD2CC */
extern Town*    g_town;                /* 0x4FD2D0 */
extern PostcardAlbum* g_postcard;      /* 0x4FD2D4 */
extern Cursor*  g_cursor;              /* 0x4FD2D8 */
extern int      g_game_mode;           /* 0x4FD2E0 */
extern int      g_is_fullscreen;       /* 0x4FD2E4 */
extern TileMap* g_tilemap;             /* 0x4AAD08 — tilemap singleton */
extern ResourceManager* g_resmgr;      /* 0x4855E8 */
extern void*    g_font_small;          /* 0x4855F4 */
extern void*    g_primary_surface;     /* 0x4FD3C4 */
extern int      g_viewport_rect_left;   /* 0x4FD358 */
extern int      g_viewport_rect_top;    /* 0x4FD35C */
extern int      g_viewport_rect_right;  /* 0x4FD360 */
extern int      g_viewport_rect_bottom; /* 0x4FD364 */
extern int      g_screen_center_x;      /* game screen center X */
extern int      g_screen_center_y;     /* game screen center Y */
extern void*    g_game;                 /* game singleton */
extern GameAudio* g_audio;             /* GameAudio singleton */
extern void*    g_netman;              /* NetMan singleton */
extern int      g_demo_mode;           /* demo mode flag */
extern void*    g_player_config;       /* player config pointer */
extern void*    g_config_ini;          /* INI config */
extern AssetMgr* g_asset_mgr;          /* asset manager */
extern char     g_install_path[];      /* game install path string */

/* String constants used by help system */
extern const char s_tutorial_section[];    /* "TUTORIAL" — 0x47f038 */
extern const char s_format_help_data_path[]; /* format string for help data path — 0x47e3d0 */
extern const char s_newline_separator[];   /* "\n" page separator — 0x47e3cc */
extern const char s_space_delimiter[];     /* space " " delimiter — 0x47f044 */
extern const char s_page_delimiter[];      /* page delimiter — 0x47f048 */
extern const char s_hash_prefix;           /* '#' prefix — 0x47f04c */
extern const char g_empty_string;          /* "" — kept as-is */

/**
 * Single-character test string for measure_text_height. 0x47f06c.
 * A single character used to measure the height of one line of text.
 */
extern const char s_measure_test_char[];   /* 0x47f06c */

/**
 * Single-character string for render_scroll_down. 0x47f070.
 * Displayed as scroll-down indicator arrow.
 */
extern const char s_scroll_down_char[];    /* 0x47f070 */

/* ================================================================== */
/* HelpPageNode methods — temporarily in HelpWnd                       */
/* TODO: Move to separate HelpPageNode class (vtable 0x4783D8)        */
/* ================================================================== */

/**
 * find_page — Initialize a page-list node (HelpPageNode constructor).
 * Address: 0x44F210
 *
 * Initializes a page node with the given resource ID (0xC42-0xC48 range).
 * Calls RESDATA_GameVehicle_Ctor on the node then configures page-specific fields.
 * Sets vtable to VTBL_HELPPAGE_NODE (0x4783D8).
 *
 * NOTE: The binary manually sets the vtable via *(void**)this = VTBL_HELPPAGE_NODE.
 * In natural C++, HelpPageNode would be its own class and the compiler would
 * manage the vtable. This is deferred until the full class hierarchy is decompiled.
 */
HelpPageData* HelpWnd::find_page(int resourceId)
{
    /* Call RESDATA base ctor */
    RESDATA_GameVehicle_Ctor(this, resourceId);

    /* In the binary: *(void**)this = VTBL_HELPPAGE_NODE.
     * In natural C++, HelpPageNode would be its own class — compiler manages vtable. */
    *(void**)this = (void*)VTBL_HELPPAGE_NODE;  /* FIXME: HelpPageNode class needed */

    /* Configure page-specific fields.
     * NOTE: These offsets are HelpPageNode fields, not HelpWnd fields.
     * TODO: Map to named HelpPageNode fields once class layout is decompiled. */
    *(int*)((uintptr_t)this + 0x04) = 5;        /* next page count / offset */
    *(int*)((uintptr_t)this + 0x10C) = 3;       /* page state / type */
    *(int*)((uintptr_t)this + 0x11C) = 0;       /* field_11c = 0 */

    /* Set overlay-related fields based on resource ID */
    switch (resourceId) {
    case 0xC42:
    case 0xC44:
    case 0xC46:
    case 0xC48:
        *(int*)((uintptr_t)this + 0x120) = 1;   /* overlay flag */
        *(int*)((uintptr_t)this + 0x124) = 0;   /* overlay counter */
        break;
    default:
        *(int*)((uintptr_t)this + 0x120) = 0;
        *(int*)((uintptr_t)this + 0x124) = 0;
        break;
    }

    return (HelpPageData*)this;
}

/**
 * find_page_scalar_dtor — Scalar deleting destructor for HelpPageNode (vtable[0]).
 * Address: 0x44F2A0
 *
 * Calls base destructor, then optionally frees memory.
 * TODO: Move to HelpPageNode class — this is compiler-generated in C++.
 */
void* HelpWnd::find_page_scalar_dtor(byte flags)
{
    /* Call HelpPageNode::base_destructor (vtable[1]).
     * In natural C++, this is the base destructor body; delete handles memory. */
    this->find_page_base_dtor();

    if (flags & 1) {
        GLOBAL_free(this);
    }
    return this;
}

/**
 * find_page_base_dtor — Base destructor for HelpPageNode (vtable[1]).
 * Address: 0x44F2C0
 *
 * Resets vtable to base, frees linked list at field[0x49] (offset +0x124),
 * chains to RESDATA_GameVehicle_BaseDtor.
 * TODO: Move to HelpPageNode class.
 */
void HelpWnd::find_page_base_dtor()
{
    /* In the binary: sets vtable to VTBL_HELPPAGE_NODE.
     * FIXME: HelpPageNode class needed — compiler manages vtables. */
    *(void**)this = (void*)VTBL_HELPPAGE_NODE;

    /* Free linked list nodes at offset +0x124.
     * NOTE: These offsets are HelpPageNode fields, not HelpWnd fields. */
    int next = *(int*)((uintptr_t)this + 0x124);
    while (next != 0) {
        int tmp = *(int*)(next + 4);
        GLOBAL_free((void*)next);
        next = tmp;
    }

    /* Chain to RESDATA_GameVehicle_BaseDtor */
    RESDATA_GameVehicle_BaseDtor(this);
}

/* ================================================================== */
/* HelpWnd constructor                                                 */
/* Address: 0x44F490                                                   */
/*                                                                      */
/* Chains to GameWindow constructor, then calls init()                 */
/* ================================================================== */
HelpWnd::HelpWnd(HINSTANCE hInstance, UINT resId)
    : GameWindow(hInstance, resId)
{
    /* In the binary: sets vtable to VTBL_HELPWND. Compiler-managed in natural C++. */
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
    GameWindow_BaseDtor(this);
}

/* ================================================================== */
/* HelpWnd::init — Initialize all HelpWnd subsystems (vtable[6])       */
/* Address: 0x451180                                                   */
/* ================================================================== */
void HelpWnd::init()
{
    int i;

    /* Zero initial state fields */
    this->hIcon = 0;                    /* +0x154 */
    this->audioChannel = NULL;          /* +0x158 */
    this->spritesInited = 0;            /* +0x14E (byte) */
    this->active = 0;                   /* +0x14C (byte) */
    this->nextBtnEnabled = 1;           /* +0x11C (byte) */
    this->prevBtnEnabled = 1;           /* +0x124 (byte) */
    this->closeBtnEnabled = 1;          /* +0x12C (byte) */
    this->scrollDownBtnEnabled = 0;     /* +0x144 (byte) */
    this->lineHeight = 1;               /* +0x3068 */
    this->animTimerId = 0;              /* +0x3058 */

    /* Create 9 ButtonSprite objects for the help UI */
    /* NOTE: Order matches original — operator_new then placement new, stored in field order */

    /* btnClose (+0x128) — Close button, res 0x3D01 */
    {
        void* mem = operator_new(0x24);
        this->btnClose = (ButtonSprite*)(mem ? new (mem) ButtonSprite(0x3D01) : NULL);
    }

    /* btnNext (+0x118) — Next page, res 0x3CFF */
    {
        void* mem = operator_new(0x24);
        this->btnNext = (ButtonSprite*)(mem ? new (mem) ButtonSprite(0x3CFF) : NULL);
    }

    /* btnPrevActual (+0x120) — Prev page, res 0x3D00 */
    {
        void* mem = operator_new(0x24);
        this->btnPrevActual = (ButtonSprite*)(mem ? new (mem) ButtonSprite(0x3D00) : NULL);
    }

    /* btnAnim (+0x130) — Animation sprite, res 0x3CFD */
    {
        void* mem = operator_new(0x24);
        this->btnAnim = (ButtonSprite*)(mem ? new (mem) ButtonSprite(0x3CFD) : NULL);
    }

    /* btnContent (+0x134) — Content sprite (dynamically set later) */
    {
        void* mem = operator_new(0x24);
        this->btnContent = (ButtonSprite*)(mem ? new (mem) ButtonSprite(0) : NULL);
    }

    /* btnScrollBar (+0x148) — Scrollbar handle, res 0x3CFE */
    {
        void* mem = operator_new(0x24);
        this->btnScrollBar = (ButtonSprite*)(mem ? new (mem) ButtonSprite(0x3CFE) : NULL);
    }

    /* btnTextArea (+0x138) — Text area (dynamically set later) */
    {
        void* mem = operator_new(0x24);
        this->btnTextArea = (ButtonSprite*)(mem ? new (mem) ButtonSprite(0) : NULL);
    }

    /* btnTextArea2 (+0x13C) — Secondary text area */
    {
        void* mem = operator_new(0x24);
        this->btnTextArea2 = (ButtonSprite*)(mem ? new (mem) ButtonSprite(0) : NULL);
    }

    /* btnTextArea3 (+0x140) — Tertiary text area */
    {
        void* mem = operator_new(0x24);
        this->btnTextArea3 = (ButtonSprite*)(mem ? new (mem) ButtonSprite(0) : NULL);
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

    /* Zero the large work buffer at +0x15C (3000 dwords = 0x2EE0 bytes) */
    {
        int32_t* buf = (int32_t*)((uintptr_t)this + 0x15C);
        for (i = 0; i < 3000; i++) {
            buf[i] = 0;
        }
    }

    /* Set page resource type and context window mode to 0 */
    this->pageResourceType = 0;    /* +0x306C */
    this->windowMode = 0;          /* +0x3070 */
}

/* ==================================================================== */
/* HelpWnd::create — Create the help window (vtable[5])                  */
/* Address: 0x450CA0                                                    */
/* ==================================================================== */
bool HelpWnd::create(HWND hWndParent)
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
    UI_CenterWindow(&desktopRect, &winLeft);

    /* Offset by -0x30 in both axes */
    OffsetRect(&desktopRect, -0x30, -0x30);

    /* Create via GameWindow::Create */
    int result = GameWindow_Create(
        this,
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

    return (result != 0);
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
    GameWindow_SetPosition(this, g_screen_center_x - 0x120, g_screen_center_y - 0xB8);

    /* Update navigation button visibility */
    this->nextBtnEnabled = (this->nextPageIdx != -1) ? 1 : 0;
    this->prevBtnEnabled = (this->prevPageIdx != -1) ? 1 : 0;

    /* Scroll down button mirrors prev button state */
    this->scrollDownBtnEnabled = (this->prevBtnEnabled != 0) ? 1 : 0;

    /* Initialize animation frame count from sprite data */
    {
        void* animPixelData = this->btnAnim->pixelData;
        if (animPixelData != NULL) {
            void* frameTable = *(void**)((uintptr_t)animPixelData + 0x20);
            short frameIdx = *(short*)((uintptr_t)animPixelData + 0x1E);
            this->animFrameCount = (uint)*(unsigned short*)((uintptr_t)frameTable + frameIdx * 0x18);
        }
    }

    /* Call base GameWindow::Show */
    GameWindow_Show(this);

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
    GameWindow_Hide(this);

    /* Clear active flag */
    this->active = 0;

    /* Restore cursor capture */
    Cursor_SetCapture(this, 1);

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
        UINT sndId = *(UINT*)((uintptr_t)this->audioChannel + 0x38);
        if (sndId != 0) {
            int handle = ResourceManager_GetStringById(g_resmgr, sndId);
            RESMGR_ReleaseSoundResource(handle);
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
        *(char*)((uintptr_t)g_town + 0x5ED) = 0;
    } else if (mode == 2) {
        UI_SetWindowVisible(g_postcard, 0);
        *(char*)((uintptr_t)g_postcard + 0x110) = 0;
    } else if (mode == 3) {
        UI_SetWindowVisible(g_cursor, 0);
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
            extern void __cdecl INPUT_NewWorld(int param);
            INPUT_NewWorld(0x4A9990);
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
        void* restoreMode;
        int mode = this->windowMode;
        if (mode == 1) {
            restoreMode = (void*)5;
        } else if (mode == 2) {
            restoreMode = (void*)6;
        } else if (mode == 3) {
            restoreMode = (void*)7;
        } else {
            restoreMode = (void*)(uintptr_t)this->returnGameMode;
        }
        CGWND_SetMode(restoreMode);

        /* Post quit to break message loop */
        WIN32_PostQuit();
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
        Cursor_Render(this, (uintptr_t)this->hWnd, 0, 0);
        Sleep(0x96);  /* 150ms */
        this->update_button_states(1);
        this->go_next_page();
        Cursor_Render(this, (uintptr_t)this->hWnd, 0, 0);
        if (g_audio != NULL) {
            PlaySound(0x5015);
        }
        return 0;

    case 2: /* Prev page */
        this->highlight_button(2);
        Cursor_Render(this, (uintptr_t)this->hWnd, 0, 0);
        Sleep(0x96);
        this->update_button_states(2);
        if (g_audio != NULL) {
            PlaySound(0x5015);
        }
        this->go_prev_page();
        Cursor_Render(this, (uintptr_t)this->hWnd, 0, 0);
        return 0;

    case 3: /* Close button */
        this->highlight_button(3);
        Cursor_Render(this, (uintptr_t)this->hWnd, 0, 0);
        Sleep(0x96);
        this->update_button_states(3);
        if (g_audio != NULL) {
            PlaySound(0x5015);
        }
        Cursor_Render(this, (uintptr_t)this->hWnd, 0, 0);
        /* Call vtable[1] = hide() */
        this->hide();
        /* Restore game mode */
        {
            int mode = this->windowMode;
            if (mode == 1) {
                CGWND_SetMode((void*)5);
            } else if (mode == 2) {
                CGWND_SetMode((void*)6);
            } else if (mode == 3) {
                CGWND_SetMode((void*)7);
            } else {
                CGWND_SetMode((void*)(uintptr_t)this->returnGameMode);
            }
        }
        return 0;

    case 7: /* Content sprite click */
        {
            int contentResId = *(int*)(*(int*)(this->btnContent->pixelData) + 4);
            if (contentResId == 0x3D01 || contentResId == 0x3D06) {
                /* Close/exit or continue — hide and restore mode */
                this->hide();
                int mode = this->windowMode;
                if (mode == 1) {
                    CGWND_SetMode((void*)5);
                } else if (mode == 2) {
                    CGWND_SetMode((void*)6);
                } else if (mode == 3) {
                    CGWND_SetMode((void*)7);
                } else {
                    CGWND_SetMode((void*)(uintptr_t)this->returnGameMode);
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
    Cursor_HandleWindowPaint(this, (uintptr_t)hWnd);

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
            goto use_cursor_set_2;
        }
        break;
    }

    /* Check other interactive regions */
    if (this->prevBtnEnabled != 1 && this->closeBtnEnabled != 1 && this->scrollDownBtnEnabled != 1) {
        /* Default cursor */
        this->set_mode(&this->ddrawAuxCount1, &this->ddrawAuxPtr1, 0, 1);
        return 0;
    }

use_cursor_set_2:
    /* Use cursor set 2 (highlight/pointer cursor) via vtable[3] */
    this->set_mode(&this->ddrawAuxCount2, &this->ddrawAuxPtr2, 0, 1);
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

/* ==================================================================== */
/* HelpWnd::set_page — Process page linkage event                        */
/* Address: 0x44F340                                                    */
/*                                                                      */
/* Called as part of page sequencing. Loads the linked game object      */
/* from the page data queue and sets its animation state.               */
/* ==================================================================== */
void HelpWnd::set_page()
{
    /* Update the base GameObject state */
    GameObject_Update(this);

    /* Check if there is pending page data and update flag is clear.
     * NOTE: Offsets +0x124 (linked list head) and +0x11C (update flag)
     * are HelpPageNode fields. TODO: Map to named HelpPageNode fields. */
    void* pageData = *(void**)((uintptr_t)this + 0x124);  /* linked list head */
    if (pageData != NULL && *(int*)((uintptr_t)this + 0x11C) == 0) {
        *(int*)((uintptr_t)this + 0x11C) = 1;  /* Set update flag */

        /* Pop first node from list */
        void* vehicle = *(void**)pageData;
        *(int*)((uintptr_t)this + 0x124) = *(int*)((uintptr_t)pageData + 4);
        GLOBAL_free(pageData);

        /* Load sounds and set vehicle state */
        Vehicle_LoadSounds(vehicle, (int*)this, 0);
        Vehicle_SetState(vehicle, 2);
        *(int*)((uintptr_t)vehicle + 0x60) = 4;  /* action_state = 4 */
    }
}

/* ==================================================================== */
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
    CGWND_SetMode((void*)8);
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
                    /* Release stream via vtable[?] (index 3 = destructor).
                     * NOTE: Literal vtable dispatch on stream object — stream class
                     * not yet decompiled. */
                    (*(void (**)(int))(**(int**)(*(int*)(*streamObj + 4) + (int)streamObj)))(1);
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
        while ((*(byte*)(*(int*)stream + 8 + (int)stream) & 1) == 0) {
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

            /* Mark page as present (hasOverlay style flag at next offset).
             * NOTE: The binary writes a marker byte into the page array with an
             * offset stride. The exact page validity marker mechanism needs
             * deeper Ghidra analysis. */
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
         * NOTE: This accesses pages[nextPageIdx + 5].pageId's first byte
         * as a validity flag. Needs deeper Ghidra analysis to map correctly. */
        while (this->nextPageIdx >= 0 &&
               *(char*)((uintptr_t)this + (this->nextPageIdx + 5) * 0x3C) == 0) {
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
    int hdc = Cursor_WaitForBlit(this);
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

    Cursor_Render(this, (uintptr_t)this->hWnd, hdc, 1);

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

        /* Skip pages with validity marker byte == 0 */
        while (this->prevPageIdx <= 199 &&
               *(char*)((uintptr_t)this + (this->prevPageIdx + 7) * 0x3C) == 0) {
            this->prevPageIdx++;
            if (this->prevPageIdx > 199) {
                this->prevPageIdx = -1;
                break;
            }
        }
    }

    /* Check nextPageIdx: if same as current and overlay present, disable */
    if (this->nextPageIdx != -1 &&
        this->windowMode != 0 &&
        this->nextPageIdx != curPage &&
        *(char*)((uintptr_t)this + this->nextPageIdx * 0x3C + 0x170) == 1) {
        this->nextPageIdx = -1;
    }

    /* Check prevPageIdx: if same as current and overlay present, disable */
    if (this->prevPageIdx != -1 &&
        this->windowMode != 0 &&
        this->prevPageIdx != curPage &&
        *(char*)((uintptr_t)this + curPage * 0x3C + 0x170) == 1) {
        this->prevPageIdx = -1;
    }
}

/* ==================================================================== */
/* HelpWnd::load_page — Load and render a specific help page             */
/* Address: 0x450520                                                    */
/*                                                                      */
/* Loads the content sprite for the given page, positions the text      */
/* area, applies clickable RECTs from page data, then updates scroll    */
/* state and navigation button visibility.                              */
/* ==================================================================== */
void HelpWnd::load_page(int pageIdx)
{
    /* Initialize animation frame count from sprite data if sprites active */
    if (this->spritesInited != 0) {
        void* pixelData = this->btnAnim->pixelData;           /* +0x130 -> +0x14 */
        if (pixelData != NULL) {
            void* frameTable = *(void**)((uintptr_t)pixelData + 0x20);  /* FrameData array */
            short frameIdx = *(short*)((uintptr_t)pixelData + 0x1E);    /* default animation index */
            this->animFrameCount = (uint)*(unsigned short*)((uintptr_t)frameTable + frameIdx * 0x18); /* +0x150 */
        }
    }

    /* Set current page index */
    this->currentPageIdx = pageIdx;                            /* +0x3040 */

    /* Destroy existing content sprite pixel data if any */
    if (this->btnContent->resourceId != 0) {                   /* +0x134 -> +0x1C */
        Sprite_Destroy(this->btnContent);                      /* 0x454BC0 */
    }

    /* Load new content sprite resource from page data */
    /* pages[pageIdx].pageId is stored as btnContent->resourceId */
    this->btnContent->resourceId = this->pages[pageIdx].pageId; /* +0x134 -> +0x1C = pages[pageIdx]+0x00 */
    Sprite_Init(this->btnContent);                             /* 0x454BF0 */

    /* Position content sprite and text area */
    if (this->btnContent->pixelData != NULL &&                 /* +0x134 -> +0x14 */
        this->btnContent->surface != NULL) {                   /* +0x134 -> +0x18 */
        /* Content sprite loaded: position it centered */
        int spriteW = *(unsigned short*)(this->btnContent->pixelData + 0x14);  /* frame_width */
        int spriteH = *(unsigned short*)(this->btnContent->pixelData + 0x16);  /* frame_height */

        /* Build sprite bounding rect and center at (0xE8+0x96, 0+B2) = (382, 178) */
        RECT spriteRect;
        SetRect(&spriteRect, 0, 0, spriteW, spriteH);
        OffsetRect(&spriteRect, 0xE8, 0);
        OffsetRect(&spriteRect, 0x96, 0xB2);
        OffsetRect(&spriteRect, -(spriteW >> 1), -(spriteH >> 1));

        /* Store sprite position and size to btnContent's rect fields */
        this->btnContent->x = spriteRect.left;                 /* +0x134 -> +0x04 */
        this->btnContent->y = spriteRect.top;                  /* +0x134 -> +0x08 */
        this->btnContent->sourceX = spriteRect.right;           /* +0x134 -> +0x0C */
        this->btnContent->sourceY = spriteRect.bottom;          /* +0x134 -> +0x10 */

        /* Position text area below the content sprite.
         * ButtonSprite.x/.y/.sourceX/.sourceY overlay as a RECT:
         *   (+0x04)=left, (+0x08)=top, (+0x0C)=right, (+0x10)=bottom */
        {
            RECT textRect;
            CopyRect(&textRect, (RECT*)&this->btnTextArea->x);  /* save original text rect */

            /* Compute new bottom:
             * (spriteRect.top - 10) - ((spriteRect.top - 10) - textRect.top) % lineHeight - 1
             * This aligns the text area bottom to a line-height boundary below sprite top. */
            int adjTop = spriteRect.top - 10;
            int newBottom = adjTop - ((adjTop - textRect.top) % this->lineHeight) - 1;

            /* Store all 4 rect fields back, with adjusted bottom */
            *((int*)&this->btnTextArea->x + 0) = textRect.left;    /* +0x04 */
            *((int*)&this->btnTextArea->x + 1) = textRect.top;     /* +0x08 */
            *((int*)&this->btnTextArea->x + 2) = textRect.right;   /* +0x0C */
            *((int*)&this->btnTextArea->x + 3) = newBottom;        /* +0x10 */
        }
    } else {
        /* No content sprite: text area fills the full content region */
        RECT textRect;
        CopyRect(&textRect, (RECT*)&this->btnTextArea->x);

        /* Compute new bottom: (top + 150) - (150 % lineHeight) - 1 */
        int newBottom = (textRect.top + 0x96) - ((textRect.top + 0x96 - textRect.top) % this->lineHeight) - 1;

        /* Store all 4 rect fields back, with adjusted bottom */
        *((int*)&this->btnTextArea->x + 0) = textRect.left;        /* +0x04 */
        *((int*)&this->btnTextArea->x + 1) = textRect.top;         /* +0x08 */
        *((int*)&this->btnTextArea->x + 2) = textRect.right;       /* +0x0C */
        *((int*)&this->btnTextArea->x + 3) = newBottom;            /* +0x10 */
    }

    /* If page defines clickable/overlay RECTs (larger than 60x20), apply them */
    HelpPageData* page = &this->pages[pageIdx];
    int clickW  = page->clickRect.right   - page->clickRect.left;
    int clickH  = page->clickRect.bottom  - page->clickRect.top;
    int overlayW = page->overlayRect.right  - page->overlayRect.left;
    int overlayH = page->overlayRect.bottom - page->overlayRect.top;

    if (clickW > 0x3C && clickH > 0x14 && overlayW > 0x3C && overlayH > 0x14) {
        /* Apply clickRect to btnTextArea's rect */
        RECT adjustedClickRect;
        CopyRect(&adjustedClickRect, &page->clickRect);
        /* Adjust bottom to line-height boundary:
         * clickRect.bottom - ((clickRect.bottom - clickRect.top) % lineHeight) - 1 */
        adjustedClickRect.bottom = page->clickRect.bottom -
            ((page->clickRect.bottom - page->clickRect.top) % this->lineHeight) - 1;

        *((int*)&this->btnTextArea->x + 0) = adjustedClickRect.left;   /* +0x04 */
        *((int*)&this->btnTextArea->x + 1) = adjustedClickRect.top;    /* +0x08 */
        *((int*)&this->btnTextArea->x + 2) = adjustedClickRect.right;  /* +0x0C */
        *((int*)&this->btnTextArea->x + 3) = adjustedClickRect.bottom; /* +0x10 */

        /* Apply overlayRect to btnContent's rect */
        *((int*)&this->btnContent->x + 0) = page->overlayRect.left;    /* +0x04 */
        *((int*)&this->btnContent->x + 1) = page->overlayRect.top;     /* +0x08 */
        *((int*)&this->btnContent->x + 2) = page->overlayRect.right;   /* +0x0C */
        *((int*)&this->btnContent->x + 3) = page->overlayRect.bottom;  /* +0x10 */
    }

    /* Update scroll state */
    this->update_scroll();

    /* Update navigation button visibility based on next/prev page indices */
    this->nextBtnEnabled = (this->nextPageIdx != -1) ? 1 : 0;     /* +0x11C */
    this->prevBtnEnabled = (this->prevPageIdx != -1) ? 1 : 0;     /* +0x124 */
    this->scrollDownBtnEnabled = (this->prevBtnEnabled != 0) ? 1 : 0; /* +0x144 */
}

/* ==================================================================== */
/* HelpWnd::go_next_page — Navigate to the next help page                */
/* Address: 0x451920                                                    */
/* ==================================================================== */
void HelpWnd::go_next_page()
{
    int nextIdx = this->nextPageIdx;
    if (nextIdx == -1) return;

    if (nextIdx == this->currentPageIdx) {
        /* Wrap-around: use scroll link */
        this->scrollOffset = this->nextPageLinkIdx;
        this->load_page(this->currentPageIdx);

        /* Play narration audio */
        if (g_audio != NULL) {
            this->play_page_audio_common();
        }

        /* Refresh all button states and reset timer */
        if (this->active != 0) {
            this->refresh_all_buttons();
        }
    } else {
        /* Normal next page */
        this->load_page(nextIdx);

        /* Measure text for scroll adjustment */
        int hdc = Cursor_WaitForBlit(this);
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

        Cursor_Render(this, (uintptr_t)this->hWnd, hdc, 1);

        if (lineCount > 1) {
            this->scrollOffset = lineCount - 1;
            this->load_page(this->currentPageIdx);

            if (this->active != 0) {
                this->refresh_all_buttons();
            }
        }

        /* Play narration audio */
        if (g_audio != NULL) {
            this->play_page_audio_common();
        }

        /* Refresh all button states */
        if (this->active != 0) {
            this->refresh_all_buttons();
        }
    }
}

/* ==================================================================== */
/* HelpWnd::go_prev_page — Navigate to the previous help page            */
/* Address: 0x451C60                                                    */
/* ==================================================================== */
void HelpWnd::go_prev_page()
{
    int prevIdx = this->prevPageIdx;
    if (prevIdx == -1) return;

    if (prevIdx == this->currentPageIdx) {
        /* Wrap-around: use scroll link */
        this->scrollOffset = this->prevPageLinkIdx;
        this->load_page(this->currentPageIdx);

        /* Play narration audio */
        if (g_audio != NULL) {
            this->play_page_audio_common();
        }

        if (this->active != 0) {
            this->refresh_all_buttons();
        }
    } else {
        /* Normal prev page */
        this->scrollOffset = 0;
        this->load_page(prevIdx);

        /* Play narration audio */
        if (g_audio != NULL) {
            this->play_page_audio_common();
        }

        if (this->active != 0) {
            this->refresh_all_buttons();
        }
    }
}

/* ==================================================================== */
/* Helper: play narration audio for current page                          */
/* ==================================================================== */
void HelpWnd::play_page_audio_common()
{
    if (g_audio == NULL) return;

    int soundResId = this->pages[this->currentPageIdx].soundResId;

    /* Release existing audio channel */
    if (this->audioChannel != NULL) {
        UINT sndId = *(UINT*)((uintptr_t)this->audioChannel + 0x38);
        if (sndId != 0) {
            int handle = ResourceManager_GetStringById(g_resmgr, sndId);
            RESMGR_ReleaseSoundResource(handle);
        }
        AudioChannel_Release(this->audioChannel);
    }

    /* Play new sound */
    if (soundResId != 0) {
        int handle = ResourceManager_GetStringById(g_resmgr, soundResId);
        RESMGR_LoadSoundResource(handle);
        GameAudio_PlayResourceEx(g_audio, soundResId, (int*)&this->audioChannel);
    }
}

/* ==================================================================== */
/* Helper: refresh all button states and reset animation timer           */
/* ==================================================================== */
void HelpWnd::refresh_all_buttons()
{
    this->update_button_states(6);  /* Animation */
    this->update_button_states(8);  /* Scrollbar */
    this->update_button_states(1);  /* Next */
    this->update_button_states(2);  /* Prev */
    this->update_button_states(3);  /* Close */
    this->update_button_states(7);  /* Content */
    this->update_button_states(4);  /* Text */
    this->update_button_states(9);  /* Scroll down */
    this->update_button_states(5);  /* Scroll up */

    /* Reset animation timer */
    KillTimer(this->hWnd, this->animTimerId);
    this->animTimerId = SetTimer(this->hWnd, 0x54, 10, NULL);
}

/* ==================================================================== */
/* HelpWnd::update_anim — Update animation frame tick (vtable[7] cb)     */
/* Address: 0x450450                                                    */
/*                                                                      */
/* Called from the timer callback. Tracks a frame counter and dispatches */
/* update_button_states(6) when frame boundary is reached.              */
/* ==================================================================== */
void HelpWnd::update_anim()
{
    if (this->audioChannel == NULL) {
        return;
    }

    bool frameChanged = false;

    /* Advance frame counter */
    this->field_3064++;
    if (this->field_3064 == 100) {
        this->field_3064 = 0;
    }

    /* Check if this is an even frame boundary (frame_count & 1 == 0) */
    if ((this->field_3064 & 1) == 0) {
        /* Check if audio is still playing */
        char isActive = AudioChannel_IsActive((uintptr_t)this->audioChannel);

        if (isActive == 0) {
            /* Audio not playing: advance to next frame */
            void* pixelData = this->btnAnim->pixelData;
            if (pixelData != NULL) {
                unsigned short* frameInfo = (unsigned short*)(
                    *(uintptr_t*)((uintptr_t)pixelData + 0x20) +
                    *(short*)((uintptr_t)pixelData + 0x1E) * 0x18);

                if (this->animFrameCount < (int)frameInfo[1]) {
                    /* Advance to next frame */
                    this->animFrameCount++;
                } else {
                    /* Loop back to start frame */
                    this->animFrameCount = frameInfo[0];
                }
            }
        } else {
            /* Audio playing: stay on the audio-frame */
            unsigned short audioFrame = *(unsigned short*)(
                *(uintptr_t*)(*(uintptr_t*)((uintptr_t)this->btnAnim->pixelData) + 0x20) + 0x18);
            if (this->animFrameCount != audioFrame) {
                this->animFrameCount = audioFrame;
            } else {
                goto skip_render;
            }
        }

        this->update_button_states(6);
        frameChanged = true;
    }

skip_render:
    if (frameChanged) {
        Cursor_Render(this, (uintptr_t)this->hWnd, 0, 0);
    }
}

/* ==================================================================== */
/* HelpWnd::draw_text — Draw one line of help text at scroll position    */
/* Address: 0x450850                                                    */
/*                                                                      */
/* Uses DrawTextA with DT_WORDBREAK|DT_EDITCONTROL to word-wrap         */
/* text. Returns character offset of next line start, or -1 at EOF.     */
/* ==================================================================== */
int HelpWnd::draw_text(int lineIdx, int* hdc_p)
{
    if (this->helpDataLoaded == 0) {
        return -1;
    }

    int hdc = *hdc_p;

    /* Set up GDI state */
    int oldColor = SetTextColor((void*)(uintptr_t)hdc, 0xA0C0D1);
    int oldBkMode = SetBkMode((void*)(uintptr_t)hdc, 1);  /* TRANSPARENT */
    void* oldFont = SelectObject((void*)(uintptr_t)hdc, g_font_small);

    /* Work buffers */
    char lineBuf[0x200];
    char compareBuf[0x200];
    char fullText[0x200];
    lineBuf[0] = 0;
    compareBuf[0] = 0;
    fullText[0] = 0;

    /* Initialize buffers */
    memset(lineBuf + 1, 0, sizeof(lineBuf) - 1);
    memset(compareBuf, 0, sizeof(compareBuf));
    memset(fullText + 1, 0, sizeof(fullText) - 1);

    /* Load page text resource */
    if (this->currentPageIdx >= 0) {
        int textResId = this->pages[this->currentPageIdx].textResId;
        FormatResourceString(g_resmgr, textResId, fullText, 0x200);
    }

    /* Walk through the text counting word-wrapped lines until we reach lineIdx */
    int charPos = 0;
    int currentLineIdx = 0;

    if (lineIdx > 0) {
        while (currentLineIdx < 200) {
            if (currentLineIdx >= lineIdx) break;

            /* Copy remainder of text to both lineBuf and compareBuf */
            RECT textRect;
            CopyRect(&textRect, (RECT*)&this->btnTextArea->x);

            const char* src = fullText + charPos;
            int len = (int)strlen(src);

            /* Copy to lineBuf */
            if (len > (int)sizeof(lineBuf) - 1) len = sizeof(lineBuf) - 1;
            memcpy(lineBuf, src, len);
            lineBuf[len] = '\0';

            /* Copy to compareBuf */
            memcpy(compareBuf, src, len);
            compareBuf[len] = '\0';

            /* Measure wrapped line */
            int textLen = (int)strlen(lineBuf);
            if (textLen > 0) {
                DrawTextA((void*)(uintptr_t)hdc, lineBuf, textLen - 1, &textRect, 0x18810);
            }

            /* Compare lineBuf and compareBuf to find where word-wrap broke */
            int wordBreak = 0;
            if (lineBuf[0] == compareBuf[0]) {
                /* Find divergence point */
                for (wordBreak = 0; wordBreak < textLen - 1; wordBreak++) {
                    if (lineBuf[wordBreak] != compareBuf[wordBreak]) break;
                }

                if (wordBreak >= textLen - 1) {
                    /* End of text */
                    SelectObject((void*)(uintptr_t)hdc, oldFont);
                    SetBkMode((void*)(uintptr_t)hdc, oldBkMode);
                    SetTextColor((void*)(uintptr_t)hdc, oldColor);
                    return -1;
                }
            }

            /* Scan backward from wordBreak to find last space */
            char c = lineBuf[wordBreak];
            while (c != ' ' && wordBreak >= 0) {
                wordBreak--;
                c = lineBuf[wordBreak];
            }
            if (wordBreak < 0) wordBreak = 0;

            currentLineIdx++;
            charPos = charPos + 1 + wordBreak;
        }
    }

    /* Restore GDI */
    SelectObject((void*)(uintptr_t)hdc, oldFont);
    SetBkMode((void*)(uintptr_t)hdc, oldBkMode);
    SetTextColor((void*)(uintptr_t)hdc, oldColor);

    return charPos;
}

/* ====================================================================== */
/* HelpWnd::measure_text_height — Measure height of one line of help text */
/* Address: 0x452170                                                      */
/*                                                                        */
/* Gets an HDC, sets up the small font, calls DrawTextA with DT_CALCRECT  */
/* on a single-character test string, and returns the measured line       */
/* height in pixels. Used to calculate text layout for scrolling pages.   */
/*                                                                        */
/* Called by: HelpWnd::show @ 0x4502D0                                     */
/* ====================================================================== */
int HelpWnd::measure_text_height()
{
    /* Get HDC via cursor blit wait */
    int hdc = Cursor_WaitForBlit(this);

    /* Set up GDI state for text measurement */
    int oldColor = SetTextColor((void*)(uintptr_t)hdc, 0xFF5C00);
    int oldBkMode = SetBkMode((void*)(uintptr_t)hdc, 1);       /* TRANSPARENT */
    void* oldFont = SelectObject((void*)(uintptr_t)hdc, g_font_small);

    /* Set measurement rect: 0xD9 x 0x96 (217 x 150), offset by 0x2A x 0x23 (42 x 35) */
    RECT measureRect;
    SetRect(&measureRect, 0, 0, 0xD9, 0x96);
    OffsetRect(&measureRect, 0x2A, 0x23);

    /* Measure one line of text using a single-character test string */
    /* DT_CALCRECT | DT_EDITCONTROL | DT_WORDBREAK = 0x18C10 */
    int lineHeight = DrawTextA(
        (void*)(uintptr_t)hdc,
        s_measure_test_char,   /* 0x47f06c — single char test string */
        1,                     /* length = 1 character */
        &measureRect,
        0x18C10);              /* DT_CALCRECT | DT_WORDBREAK | DT_EDITCONTROL */

    /* Restore GDI state */
    SelectObject((void*)(uintptr_t)hdc, oldFont);
    SetBkMode((void*)(uintptr_t)hdc, oldBkMode);
    SetTextColor((void*)(uintptr_t)hdc, oldColor);

    /* Render cursor to commit the blit */
    Cursor_Render(this, (uintptr_t)this->hWnd, hdc, 1);

    return lineHeight;
}

/* ====================================================================== */
/* HelpWnd::update_button_states — Update sprite visibility/state          */
/* Address: 0x451FB0                                                      */
/*                                                                        */
/* buttonId mapping:                                                      */
/*   1 = Next page button      (btnNext, +0x118)                          */
/*   2 = Prev page button      (btnPrevActual, +0x120)                    */
/*   3 = Close button          (btnClose, +0x128)                         */
/*   4 = Render page text      (calls render_page)                        */
/*   5 = Render scroll-up      (calls render_scroll_up)                   */
/*   6 = Update anim sprite    (calls update_anim_sprite)                 */
/*   7 = Render content sprite (btnContent, +0x134)                       */
/*   8 = Scroll bar handle     (btnScrollBar, +0x148)                     */
/*   9 = Render scroll-down    (calls render_scroll_down)                 */
/*                                                                        */
/* State values passed to Sprite_SetState:                                */
/*   0 = normal (enabled) state                                           */
/*   1 = highlighted (hover) state                                        */
/*   2 = disabled (dimmed) state                                          */
/* ====================================================================== */
void HelpWnd::update_button_states(int buttonId)
{
    void* backbuffer = this->backbufferSurface;  /* +0x38 */

    switch (buttonId) {
    case 1: /* Next page button */
        if (this->nextBtnEnabled == 1) {
            Sprite_SetState(this->btnNext, 0, backbuffer);   /* normal */
        } else {
            Sprite_SetState(this->btnNext, 2, backbuffer);   /* disabled */
        }
        return;

    case 2: /* Prev page button */
        if (this->prevBtnEnabled == 1) {
            Sprite_SetState(this->btnPrevActual, 0, backbuffer);
        } else {
            Sprite_SetState(this->btnPrevActual, 2, backbuffer);
        }
        return;

    case 3: /* Close button */
        if (this->closeBtnEnabled == 1) {
            Sprite_SetState(this->btnClose, 0, backbuffer);
        }
        return;

    case 4: /* Render page text */
        {
            int hdc = Cursor_WaitForBlit(this);
            this->render_page(&hdc);
        }
        return;

    case 5: /* Render scroll-up indicator */
        {
            int hdc = Cursor_WaitForBlit(this);
            this->render_scroll_up(&hdc);
            Cursor_Render(this, (uintptr_t)this->hWnd, hdc, 1);
        }
        return;

    case 6: /* Update animation sprite */
        this->update_anim_sprite(this->animFrameCount);
        return;

    case 7: /* Content sprite */
        if (this->btnContent->pixelData != NULL &&
            this->btnContent->surface != NULL) {
            Sprite_SetState(this->btnContent, 0, backbuffer);
            /* Check if the content sprite's animation has step_mode flag.
             * NOTE: pixelData offset +0x20 and +0x17 are from an
             * un-decompiled pixel data structure. */
            void* animTable = (this->btnContent->pixelData != NULL)
                ? *(void**)((uint8_t*)this->btnContent->pixelData + 0x20) : NULL;
            if (animTable != NULL && *(char*)((uintptr_t)animTable + 0x17) == 1) {
                Sprite_SetState(this->btnContent, 1, backbuffer);  /* highlighted */
                return;
            }
        }
        return;

    case 8: /* Scroll bar handle */
        Sprite_SetState(this->btnScrollBar, 0, backbuffer);
        return;

    case 9: /* Render scroll-down indicator */
        if (this->scrollDownBtnEnabled == 1) {
            int hdc = Cursor_WaitForBlit(this);
            this->render_scroll_down(&hdc);
            Cursor_Render(this, (uintptr_t)this->hWnd, hdc, 1);
        }
        return;
    }
}

/* ====================================================================== */
/* HelpWnd::highlight_button — Visually highlight a navigation button      */
/* Address: 0x4527B0                                                      */
/*                                                                        */
/* buttonId mapping (same as update_button_states):                       */
/*   1 = Next page button      (btnNext, +0x118)                          */
/*   2 = Prev page button      (btnPrevActual, +0x120)                    */
/*   3 = Close button          (btnClose, +0x128)                         */
/*   6 = Animation sprite      (btnAnim, +0x130) — set to normal (0)      */
/*   7 = Content sprite        (btnContent, +0x134) — set to normal (0)   */
/*   8 = Scroll bar handle     (btnScrollBar, +0x148) — set to normal (0) */
/*   9 = Scroll-down indicator (button renders directly)                  */
/*                                                                        */
/* Note: Cases 6, 7, 8 set state=0 (normal) rather than highlighted.     */
/* ====================================================================== */
void HelpWnd::highlight_button(int buttonId)
{
    void* backbuffer = this->backbufferSurface;  /* +0x38 */

    switch (buttonId) {
    case 1: /* Next page — highlight */
        if (this->nextBtnEnabled == 1) {
            Sprite_SetState(this->btnNext, 1, backbuffer);   /* state=1 = highlighted */
        }
        return;

    case 2: /* Prev page — highlight */
        if (this->prevBtnEnabled == 1) {
            Sprite_SetState(this->btnPrevActual, 1, backbuffer);
        }
        return;

    case 3: /* Close button — highlight */
        if (this->closeBtnEnabled == 1) {
            Sprite_SetState(this->btnClose, 1, backbuffer);
        }
        return;

    case 6: /* Animation sprite — set normal (not highlighted) */
        Sprite_SetState(this->btnAnim, 0, backbuffer);       /* state=0 = normal */
        return;

    case 7: /* Content sprite — set normal if pixelData and surface exist */
        if (this->btnContent->pixelData != NULL &&
            this->btnContent->surface != NULL) {
            Sprite_SetState(this->btnContent, 0, backbuffer);
        }
        return;

    case 8: /* Scroll bar handle — set normal */
        Sprite_SetState(this->btnScrollBar, 0, backbuffer);
        return;

    case 9: /* Scroll-down indicator — render directly */
        if (this->scrollDownBtnEnabled == 1) {
            int hdc = Cursor_WaitForBlit(this);
            this->render_scroll_down(&hdc);
            Cursor_Render(this, (uintptr_t)this->hWnd, hdc, 1);
        }
        return;
    }
}

/* ====================================================================== */
/* HelpWnd::hit_test — Determine which button/region was clicked at (x,y)  */
/* Address: 0x451E90                                                      */
/*                                                                        */
/* Tests the click point against the bounding rects of each button sprite */
/* in priority order. Returns the button ID if a hit is found, or 0 if   */
/* no button was hit.                                                     */
/*                                                                        */
/* Return value mapping:                                                  */
/*   0 = no hit                                                           */
/*   1 = Next page button      (btnNext, +0x118)                          */
/*   2 = Prev page button      (btnPrevActual, +0x120)                    */
/*   3 = Close button          (btnClose, +0x128)                         */
/*   4 = Text area             (btnTextArea, +0x138)                      */
/*   5 = Text area 2           (btnTextArea2, +0x13C)                     */
/*   6 = Animation sprite      (btnAnim, +0x130)                          */
/*   7 = Content sprite        (btnContent, +0x134)                       */
/*   8 = Scroll bar            (btnScrollBar, +0x148)                     */
/* ====================================================================== */
byte HelpWnd::hit_test(int x, int y)
{
    /* Test next page button — only if enabled */
    if (this->nextBtnEnabled != 0) {
        if (PtInRect((RECT*)&this->btnNext->x, x, y)) {
            return 1;
        }
    }

    /* Test prev page button — only if enabled */
    if (this->prevBtnEnabled != 0) {
        if (PtInRect((RECT*)&this->btnPrevActual->x, x, y)) {
            return 2;
        }
    }

    /* Test close button */
    if (PtInRect((RECT*)&this->btnClose->x, x, y)) {
        return 3;
    }

    /* Test content sprite */
    if (PtInRect((RECT*)&this->btnContent->x, x, y)) {
        return 7;
    }

    /* Test animation sprite */
    if (PtInRect((RECT*)&this->btnAnim->x, x, y)) {
        return 6;
    }

    /* Test primary text area */
    if (PtInRect((RECT*)&this->btnTextArea->x, x, y)) {
        return 4;
    }

    /* Test secondary text area */
    if (PtInRect((RECT*)&this->btnTextArea2->x, x, y)) {
        return 5;
    }

    /* Test scroll bar — return 8 if hit, 0 otherwise */
    if (PtInRect((RECT*)&this->btnScrollBar->x, x, y)) {
        return 8;
    }

    return 0;
}

/* ====================================================================== */
/* Stub methods — deferred decompilation                                  */
/* ====================================================================== */

/**
 * set_mode — vtable[3] cursor dispatch. Inherited from GameWindow.
 * Address: 0x414340
 *
 * TODO: Decompile Cursor_SetMode implementation from 0x414340.
 * Temporarily stubbed — this virtual method is dispatched through
 * the vtable and calls into Cursor_SetMode.
 */
void HelpWnd::set_mode(void* countPtr, void* dataPtr, int modeA, int modeB)
{
    /* Stub: the real implementation at 0x414340 manages auxiliary
     * DDraw object references. TODO: decompile 0x414340 */
}

/**
 * render_page — Render current page text content.
 * Address: 0x452230
 *
 * TODO: decompile 0x452230
 */
void HelpWnd::render_page(int* /* hdc_p */)
{
    /* TODO: decompile 0x452230 */
}

/**
 * render_scroll_up — Render scroll-up indicator.
 * Address: 0x452570
 *
 * TODO: decompile 0x452570
 */
void HelpWnd::render_scroll_up(int* /* hdc_p */)
{
    /* TODO: decompile 0x452570 */
}

/**
 * render_scroll_down — Render scroll-down indicator.
 * Address: 0x4526B0
 *
 * TODO: decompile 0x4526B0
 */
void HelpWnd::render_scroll_down(int* /* hdc_p */)
{
    /* TODO: decompile 0x4526B0 */
}

/**
 * draw_scroll_indicator — Blit the scroll indicator to surface.
 * Address: 0x452B00
 *
 * TODO: decompile 0x452B00
 */
void HelpWnd::draw_scroll_indicator()
{
    /* TODO: decompile 0x452B00 */
}

/**
 * update_anim_sprite — Render animation sprite at frame offset.
 * Address: 0x452C00
 *
 * TODO: decompile 0x452C00
 */
void HelpWnd::update_anim_sprite(int /* frameOffset */)
{
    /* TODO: decompile 0x452C00 */
}
