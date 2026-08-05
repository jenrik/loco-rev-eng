/**
 * TrainStationWindow.cpp — Train station dispatch dialog implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

// Status: TRANSCRIBED

#include "TrainStationWindow.h"

/* ================================================================== */
/* External references                                                  */
/* ================================================================== */

/* GameWindow base methods */
extern void* __thiscall GameWindow_Ctor(void* self, HINSTANCE hInstance, UINT resId);   /* 0x413AB0 */
extern uint32_t __thiscall GameWindow_Create(void* self, int nCmdShow, HWND hWndParent,  /* 0x413DE0 */
    int x, int y, int nWidth, int nHeight, HMENU hMenu, HICON hIcon,
    UINT classStyle, int unused1, int unused2, uint8_t showCursor);

/* Train sprite loading */
extern void __fastcall Train_LoadSprites(int thisPtr);                     /* sprite loader */

/* Window management */
extern void __cdecl UI_CenterWindow(int* outX, int* inOutY);               /* 0x425C70 */

/* Tooltip management */
extern void __fastcall TrainStationWindow_UpdateTooltip(int thisPtr);      /* tooltip update */
extern void __cdecl UI_DestroyTooltip(void* tooltipMgr, void* tooltip);    /* destroy tooltip */

/* Globals */
extern int      g_screen_width;        /* 0x4851D8 */
extern int      g_screen_height;       /* 0x485214 */
extern int      g_viewport_rect_left;  /* 0x4AAD14 */
extern int      g_viewport_rect_top;   /* 0x4AAD18 */
extern int      g_viewport_rect_right; /* 0x4AAD1C */
extern int      g_viewport_rect_bottom;/* 0x4AAD20 */
extern void*    g_tilemap;             /* 0x4AAD08 */
extern void*    g_tooltip_mgr;         /* 0x4FD220 */
extern void*    g_resmgr;              /* ResourceManager */
extern void*    g_audio;               /* 0x4FD3BC — GameAudio */

/* Resource manager */
extern int   __cdecl ResourceManager_GetStringById(void* resmgr, int id);
extern int   __cdecl RESMGR_LoadSoundResource(int strRes);
extern void  __cdecl RESMGR_ReleaseSoundResource(int strRes);

/* Tilemap invalidation */
extern void __cdecl TileMap_InvalidateRect(void* tilemap, int left, int top, int right, int bottom);
extern void __cdecl TileMap_InvalidateDirtyRects(void* tilemap, char flag);

/* Sound */
extern void __cdecl PlaySound(uint32_t resId);

/* Win32 API */
extern HWND  GetDesktopWindow(void);
extern BOOL  GetClientRect(HWND hWnd, RECT* lpRect);
extern HICON LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName);
extern BOOL  ShowWindow(HWND hWnd, int nCmdShow);
extern HWND  SetFocus(HWND hWnd);
extern UINT_PTR SetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, void* lpTimerFunc);
extern BOOL  KillTimer(HWND hWnd, UINT_PTR uIDEvent);
extern void  SetRect(RECT* lprc, int x1, int y1, int x2, int y2);
extern void  SetRectEmpty(RECT* lprc);
extern BOOL  UnionRect(RECT* lprcDst, const RECT* lprcSrc1, const RECT* lprcSrc2);
extern BOOL  IntersectRect(RECT* lprcDst, const RECT* lprcSrc1, const RECT* lprcSrc2);

namespace {
using SpriteReleaseFunction = void (*)(void*);

void release_sprite_resource(void* resource)
{
    void** vtable = *reinterpret_cast<void***>(resource);
    if (vtable != nullptr) {
        auto release = reinterpret_cast<SpriteReleaseFunction>(vtable[2]);
        release(resource);
    }
}

int legacy_this_pointer(const void* object)
{
    return static_cast<int>(reinterpret_cast<intptr_t>(object));
}
}

/* ================================================================== */
/* TrainStationWindow::TrainStationWindow — Constructor                 */
/* Address: 0x436B20                                                    */
/* ================================================================== */
TrainStationWindow::TrainStationWindow(HINSTANCE hInstance, UINT resId)
    : GameWindow(hInstance, resId)
{
    /* In the binary: sets vtable here. Compiler-managed in natural C++. */

    /* Zero all subclass fields */
    this->hIcon            = nullptr;     /* +0x128 */
    this->sprites_loaded   = 0;           /* +0x16C */
    this->sound_loaded     = 0;           /* +0x16D */
    this->field_1B8        = 0;           /* +0x1B8 */
    this->train_type       = 0;           /* +0x124 */
    this->field_120        = 0;           /* +0x120 */
    this->tooltip_active   = 0;           /* +0x1BC */
    this->tooltip_ptr      = nullptr;     /* +0x1C0 */

    /* Set animation state to inactive */
    this->frame_index      = -1;          /* +0x1A0 */
    this->anim_state       = -1;          /* +0x190 */
}


/* ================================================================== */
/* TrainStationWindow::~TrainStationWindow — Scalar deleting dtor      */
/* Address: 0x436B40                                                    */
/* ================================================================== */
TrainStationWindow::~TrainStationWindow()
{
    this->base_destructor();
}


/* ================================================================== */
/* TrainStationWindow::base_destructor — Release all resources          */
/* Address: 0x436B80                                                    */
/* ================================================================== */
void TrainStationWindow::base_destructor()
{
    /* In the binary: sets vtable here. Compiler-managed in natural C++. */

    /* Release sprites and tooltip via Hide logic */
    this->hide();

    /* Chain to GameWindow base destructor */
    GameWindow::base_destructor();
}


/* ================================================================== */
/* TrainStationWindow::Create — Load sprites and create window          */
/* Address: 0x436D00 (vtable[5])                                        */
/* ================================================================== */
bool TrainStationWindow::Create(HWND hWndParent)
{
    RECT  desktopRect;
    int   x = 0;
    int   y = 0;
    uint32_t winWidth;
    uint32_t winHeight;
    int    result;

    /* Load icon resource 0x65 */
    this->hIcon = LoadIconA(
        this->hInstance,
        reinterpret_cast<LPCSTR>(static_cast<uintptr_t>(0x65)));

    /* Load train sprites */
    Train_LoadSprites(legacy_this_pointer(this));

    /* Read window size from destination data resource */
    /* dest_data_res at +0x188; width at +0x14, height at +0x16 */
    const auto* destination_bytes = reinterpret_cast<const uint8_t*>(
        this->dest_data_res);
    winWidth  = static_cast<uint32_t>(
        *reinterpret_cast<const uint16_t*>(destination_bytes + 0x14));
    winHeight = static_cast<uint32_t>(
        *reinterpret_cast<const uint16_t*>(destination_bytes + 0x16));

    /* Set up desktop rect for centering */
    SetRectEmpty(&desktopRect);
    desktopRect.right  = g_screen_width;
    desktopRect.bottom = g_screen_height;

    /* If sprites were previously loaded, release them */
    if (this->sprites_loaded) {
        /* Release sprite resources via vtable[2] (Destroy/Release) */
        release_sprite_resource(this->sprite_ptr_1);
        this->sprite_ptr_1 = nullptr;

        release_sprite_resource(this->sprite_ptr_2);
        this->sprite_ptr_2 = nullptr;

        release_sprite_resource(this->dest_data_res);
        this->dest_data_res = nullptr;

        release_sprite_resource(this->sprite_ptr_3);
        this->sprite_ptr_3 = nullptr;

        this->sprites_loaded = 0;
    }

    /* Center the window on screen */
    UI_CenterWindow(&desktopRect.left, &x);

    /* Create the actual window via GameWindow::create */
    /* nCmdShow=0 (hidden), style=WS_EX_TOPMOST|WS_POPUP (0x86000000) */
    result = GameWindow_Create(this, 0, hWndParent, x, y,
                               winWidth - x, winHeight - y,
                               nullptr, this->hIcon, 0, 0x86000000, 0, 0);

    return static_cast<uint8_t>(result) != 0;
}


/* ================================================================== */
/* TrainStationWindow::show — Display the train station dialog          */
/* Address: 0x436EC0 (vtable[2])                                        */
/* ================================================================== */
void TrainStationWindow::show(int train_type, int context)
{
    int   strRes;
    int   loadResult;

    /* Chain to GameWindow base show (sets capture, timer, visible flags) */
    GameWindow::show();

    /* Store train type */
    this->train_type = train_type;     /* +0x124 */

    /* Load train sprites */
    Train_LoadSprites(legacy_this_pointer(this));

    /* Fire vtable[6] — update_client_rect */
    this->init();

    /* Show window and set focus */
    ShowWindow(this->hWnd, 1);   /* SW_SHOWNORMAL */
    SetFocus(this->hWnd);

    /* Create 200ms timer (ID 1) */
    this->timer_id = SetTimer(this->hWnd, 1, 200, nullptr);

    /* Store context parameter */
    this->field_11C = context;         /* +0x11C */

    /* Activate tooltip */
    this->tooltip_active = 1;          /* +0x1BC */
    TrainStationWindow_UpdateTooltip(legacy_this_pointer(this));

    /* Load and play sound 0x50F8 if not already loaded */
    if (g_audio != nullptr && this->sound_loaded == 0) {
        strRes = ResourceManager_GetStringById(&g_resmgr, 0x50F8);
        loadResult = RESMGR_LoadSoundResource(strRes);
        if (static_cast<uint8_t>(loadResult) != 0) {
            auto* sound_resource = reinterpret_cast<uint8_t*>(
                static_cast<uintptr_t>(static_cast<uint32_t>(strRes)));
            sound_resource[8] = 1;   /* mark as active */
            this->sound_loaded = 1;
        }
    }

    PlaySound(0x50F8);
}


/* ================================================================== */
/* TrainStationWindow::hide — Dismiss the train station dialog          */
/* Address: 0x436F70 (vtable[1])                                        */
/* ================================================================== */
void TrainStationWindow::hide()
{
    RECT   spriteRect;
    RECT   unionRect;
    RECT   intersectRect;
    int    strRes;

    /* Chain to GameWindow base hide (saves backbuffer, kills base timer, hides HWND) */
    GameWindow::hide();

    /* Release sprites if loaded */
    if (this->sprites_loaded) {
        release_sprite_resource(this->sprite_ptr_1);
        this->sprite_ptr_1 = nullptr;

        release_sprite_resource(this->sprite_ptr_2);
        this->sprite_ptr_2 = nullptr;

        release_sprite_resource(this->dest_data_res);
        this->dest_data_res = nullptr;

        release_sprite_resource(this->sprite_ptr_3);
        this->sprite_ptr_3 = nullptr;

        this->sprites_loaded = 0;
    }

    /* Kill the 200ms timer */
    KillTimer(this->hWnd, this->timer_id);

    /* Reset animation state */
    this->field_120    = 0;
    this->anim_state   = -1;         /* +0x190 */
    this->frame_index  = -1;         /* +0x1A0 */
    this->tooltip_active = 0;        /* +0x1BC */

    /* Destroy tooltip with tilemap invalidation */
    if (this->tooltip_ptr != nullptr) {
        /* Build rectangle from window position (GameWindow fields at +0xDC..+0xE8) */
        SetRect(&spriteRect,
                this->windowX,
                this->windowY,
                this->windowX + this->windowWidth,
                this->windowY + this->windowHeight);

        /* Union with tooltip's stored rect (at tooltip_ptr + 0x08) */
        UnionRect(&unionRect,
                  reinterpret_cast<const RECT*>(
                      reinterpret_cast<const uint8_t*>(this->tooltip_ptr) + 8),
                  &spriteRect);

        /* Intersect with viewport bounds */
        IntersectRect(&intersectRect,
                      &unionRect,
                      reinterpret_cast<const RECT*>(&g_viewport_rect_left));

        /* Destroy tooltip */
        UI_DestroyTooltip(&g_tooltip_mgr, this->tooltip_ptr);
        this->tooltip_ptr = nullptr;

        /* Invalidate affected tilemap regions */
        TileMap_InvalidateRect(g_tilemap,
                               intersectRect.left, intersectRect.top,
                               intersectRect.right, intersectRect.bottom);
        TileMap_InvalidateDirtyRects(g_tilemap, 0);
    }

    /* Release sound resource if loaded */
    if (g_audio != nullptr && this->sound_loaded != 0) {
        strRes = ResourceManager_GetStringById(&g_resmgr, 0x50F8);
        auto* sound_resource = reinterpret_cast<uint8_t*>(
            static_cast<uintptr_t>(static_cast<uint32_t>(strRes)));
        sound_resource[8] = 0;   /* mark as inactive */
        RESMGR_ReleaseSoundResource(strRes);
        this->sound_loaded = 0;
    }
}
