/**
 * NameEntryPanel.cpp — NameEntryPanel implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

// Status: TRANSCRIBED

#include "NameEntryPanel.h"
#include "ButtonSprite.h"
#include "../network/DPlayManager.h"
#include "../network/Netman.h"
#include "../network/DirectPlay.h"       /* DirectPlayConnectionNode (GameConfig::m_providerList) */
#include "../game/GameConfig.h"          /* GameConfig singleton, _g_netman_data */
#include "../resources/ResourceObject.h" /* ResourceObject::Lock/Unlock (spriteTerminator) */
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void* __cdecl operator_new(size_t size);     /* 0x465CE0 */
    extern void  __cdecl GLOBAL_free(void* ptr);         /* 0x465CD0 */
    /* extern ResourceManager g_resmgr (resources/ResourceManager.h) is an
     * object, not a pointer — matches the canonical declaration now that
     * this file transitively includes it via network/Netman.h. Previously
     * declared here as `void*`, one of the 22 files flagged in PROGRESS.md's
     * "g_resmgr extern-type-mismatch" sweep; fixed alongside adding
     * on_create/window_proc/on_timer since the new include surfaced it as
     * a hard conflicting-declaration error rather than a silent mismatch. */

extern "C" {
    extern HBRUSH __stdcall CreateSolidBrush(COLORREF color);  /* 0x477070 */
    extern BOOL   __stdcall DeleteObject(HGDIOBJ hObject);     /* 0x477048 */
    extern HWND   __stdcall GetDesktopWindow(void);             /* 0x477364 */
    extern BOOL   __stdcall GetClientRect(HWND hWnd, void* lpRect); /* 0x477368 */
    extern HICON  __stdcall LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName); /* 0x47736C */
    extern LRESULT __stdcall DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam,
                                             LPARAM lParam);
    extern int    __stdcall SetBkMode(void* hdc, int mode);
    extern int    __stdcall SetTextColor(void* hdc, int color);
    extern void*  __stdcall SelectObject(void* hdc, void* hgdiobj);
    extern int32_t __stdcall DrawTextA(void* hdc, const char* lpchText, int32_t cchText,
                                        RECT* lprc, uint32_t format);
    extern int32_t __stdcall CopyRect(RECT* lprcDst, const RECT* lprcSrc);
    extern int32_t __stdcall OffsetRect(RECT* lprc, int32_t dx, int32_t dy);
    extern int32_t __stdcall IntersectRect(RECT* out, RECT* a, RECT* b);
    extern int32_t __stdcall PtInRect(const RECT* lprc, POINT pt);
}

/* Moved here 2026-08-17 from native/NETMAN_NetworkUI.c / native/NETMAN_SessionSettings.c
 * (hide()/show()/on_update()/on_lbutton_down()/on_key_down()/applyProviderModes()/
 * enumerateSessions()/getSessionInfo() below): real def resources/ResourceManager.h
 * (0x448D60), matching every other in-tree caller's `(int32_t resId)` shape
 * (ui/EditWindow.cpp, ui/HelpWnd.cpp, ui/TrainStationWindow.cpp, game/TrainStation.cpp,
 * core/Game.cpp) rather than network/Netman.h's own unreferenced/never-defined
 * `ResourceManager_LoadSoundResource` rename attempt. */
extern void  __cdecl    RESMGR_LoadSoundResource(int32_t resId);        /* 0x448D60 */

/* g_font_small — shared small UI font, canonical name/type from
 * network/NetworkPlayerList.cpp (real definition: shared/stubs_impl.cpp). */
extern void* g_font_small;                                              /* 0x4855F8 */

/* EDIT-control subclass WndProc (0x4417E0, not yet decompiled) — real
 * def: native/NETMAN_NetworkUI.c. Registered by enumerateSessions() via
 * SetWindowLongA. C++ linkage (native/*.c is compiled as C++, not C). */
extern LRESULT __stdcall NETMAN_EditControlSubclassProc(void* hWnd, uint32_t msg,
                                                          uint32_t wParam, uint32_t lParam);

/* Matches ui/UI_WindowBase.cpp's declaration for the same real function. */
extern void   WIN32_PostQuit(void);                        /* 0x463670 */

/* g_primary_surface, UIPANEL_EndPaintEx, UIPANEL_EndPaint are already
 * declared by network/DPlayManager.h / network/Netman.h, transitively
 * included above — reused as-is to avoid a duplicate/ambiguous
 * declaration (RenderConnectionPanel is this class's own sibling renderer,
 * so these are the same real functions). */
extern void*  __fastcall UIPANEL_BeginPaint(void* self);     /* 0x426B00 */
extern bool   __cdecl    UIPANEL_Blit(void* surface, uint32_t srcX, uint32_t srcY,
                                       int32_t srcW, uint32_t srcH, void* dstSurface,
                                       uint32_t dstX, uint32_t dstY, int32_t dstW,
                                       uint32_t dstH, uint32_t flags);      /* 0x42B050 */
extern void   __cdecl    UI_CenterWindow(RECT* outer, RECT* inner);        /* 0x425A50 */

    /* Resource manager */
    extern int    __thiscall RESMGR_GetById(void* resmgr, UINT id);      /* 0x4472B0 */
    extern void   __fastcall RESMGR_ReleaseSoundResource(int handle);    /* 0x448EE0 */

    /* UI_CreateFullWindow (UI_WindowBase::create_full_window, vtable[6]) */
    extern int    __thiscall UI_CreateFullWindow(void* self, int nCmdShow,
                                                  HWND hParent, int x, int y,
                                                  int nWidth, int nHeight,
                                                  HMENU hMenu, HICON hIcon,
                                                  UINT classStyle);       /* 0x425B70 */

    /* Inherited base destructor */
    extern void   __fastcall UI_WindowBase_BaseDtor(void* self);          /* 0x425910 */

/* Global pointer to this NameEntryPanel instance */
extern NameEntryPanel* g_nameEntryPanel;  /* 0x485260 */

/* ================================================================== */
/* NameEntryPanel Constructor                                          */
/* Address: 0x440F20                                                   */
/*                                                                     */
/* Chains to UI_WindowBase base constructor, then sets vtable to       */
/* VTBL_NAMEENTRYPANEL and calls Init() to initialize all fields.     */
/*                                                                     */
/* Called by: UI_MainMenu_Create @ 0x42058D                            */
/*   parent calls: operator_new(0x1E4), then this(this, hInst, 0x1F6) */
/*                                                                     */
/* @param hInstance  Application instance handle                       */
/* @param resId      Resource ID (0x1F6 = 502 for name-entry panel)    */
/* ================================================================== */
NameEntryPanel::NameEntryPanel(HINSTANCE hInstance, UINT resId)
    : UI_WindowBase(hInstance, resId)
{
    /* Constructor body (in SEH frame) */
/* In the binary: sets vtable here. Compiler-managed in natural C++. */
    this->init();                                /* init fields + sprites */
}

/* ================================================================== */
/* NameEntryPanel::init                                                */
/* Address: 0x440FA0                                                   */
/*                                                                     */
/* Initializes all NameEntryPanel-specific fields:                     */
/*   1. Zeroes numeric fields                                          */
/*   2. Sets gameMode to 3 (max players count)                         */
/*   3. Creates a solid background brush (color 0xA8C4D8)             */
/*   4. Creates 7 ButtonSprite objects (resources 0x417..0x421)       */
/*   5. Stores this panel pointer in global @ 0x485260                 */
/* ================================================================== */
void NameEntryPanel::init()
{
    /* Zero fields */
    this->animationTimerId = 0;     /* +0xEC */
    this->field_E8 = 0;             /* +0xE8 (byte) */
    this->gameMode = 3;             /* +0x140 — default max players */
    this->textBuffer[0] = 0;        /* +0xF0 (null-terminate; buffer is 64 bytes) */
    this->sessionNameEditHwnd = nullptr;   /* +0x1D8 */
    this->iconHandle = nullptr;     /* +0x144 */
    this->hasSprites = 0;           /* +0x1AC (byte) */
    this->supportsTwoPlayerMode = 0;   /* +0x1E0 (byte) */
    this->supportsFourPlayerMode = 0;  /* +0x1E1 (byte) */

    /* Create background brush */
    this->backgroundBrush = CreateSolidBrush(0xA8C4D8);  /* +0x1D4 */

    /* Create 7 ButtonSprite objects */
    this->sprite0 = new ButtonSprite(0x419);  /* +0x1B0 */
    this->sprite1 = new ButtonSprite(0x41A);  /* +0x1B4 */
    this->sprite2 = new ButtonSprite(0x417);  /* +0x1B8 */
    this->sprite3 = new ButtonSprite(0x418);  /* +0x1BC */
    this->sprite4 = new ButtonSprite(0x41F);  /* +0x1C0 */
    this->sprite5 = new ButtonSprite(0x420);  /* +0x1C4 */
    this->sprite6 = new ButtonSprite(0x421);  /* +0x1C8 */
    this->spriteTerminator = NULL;             /* +0x1CC — array terminator */

    /* Store global reference to this panel */
    g_nameEntryPanel = this;  /* 0x485260 */
}

/* ================================================================== */
/* NameEntryPanel::scalar deleting destructor (vtable[0])              */
/* Address: 0x440F80                                                   */
/*                                                                     */
/* Calls base_destructor to release all sprites/resources, then        */
/* optionally frees heap memory if (flags & 1).                        */
/* ================================================================== */
NameEntryPanel::~NameEntryPanel()
{
    this->base_destructor();
}

/* ================================================================== */
/* NameEntryPanel::base_destructor                                     */
/* Address: 0x441190                                                   */
/*                                                                     */
/* Destructor body: resets vtable, then:                               */
/*   1. If hasSprites flag is set: destroy all 7 ButtonSprite objects  */
/*      via Sprite_Destroy (fastcall with pixelData vtable[2])         */
/*   2. Calls scalar dtor on each ButtonSprite pointer (vtable[0])     */
/*   3. Releases sound resource 0x5015                                 */
/*   4. Deletes the background brush                                   */
/*   5. Calls UI_WindowBase base destructor                            */
/* ================================================================== */
void NameEntryPanel::base_destructor()
{
/* In the binary: sets vtable here. Compiler-managed in natural C++. */

    /* If sprites have been allocated, destroy them */
    if (this->hasSprites) {  /* +0x1AC */
        /* Release child pixel data from each sprite (Sprite_Destroy) */
        if (this->sprite0 != NULL) this->sprite0->destroy();
        if (this->sprite1 != NULL) this->sprite1->destroy();
        if (this->sprite2 != NULL) this->sprite2->destroy();
        if (this->sprite3 != NULL) this->sprite3->destroy();
        if (this->sprite4 != NULL) this->sprite4->destroy();
        if (this->sprite5 != NULL) this->sprite5->destroy();
        if (this->sprite6 != NULL) this->sprite6->destroy();

        this->hasSprites = 0;  /* +0x1AC */
    }

    /* Free each ButtonSprite via its scalar deleting destructor (vtable[0]) */
    if (this->sprite0 != NULL) {
        delete this->sprite0;
        this->sprite0 = NULL;
    }
    if (this->sprite1 != NULL) {
        delete this->sprite1;
        this->sprite1 = NULL;
    }
    if (this->sprite2 != NULL) {
        delete this->sprite2;
        this->sprite2 = NULL;
    }
    if (this->sprite3 != NULL) {
        delete this->sprite3;
        this->sprite3 = NULL;
    }
    if (this->sprite4 != NULL) {
        delete this->sprite4;
        this->sprite4 = NULL;
    }
    if (this->sprite5 != NULL) {
        delete this->sprite5;
        this->sprite5 = NULL;
    }
    if (this->sprite6 != NULL) {
        delete this->sprite6;
        this->sprite6 = NULL;
    }

    /* Release sound resource 0x5015 */
    int sndHandle = RESMGR_GetById(&g_resmgr, 0x5015);
    if (sndHandle != 0) {
        RESMGR_ReleaseSoundResource(sndHandle);
    }

    /* Delete background brush */
    if (this->backgroundBrush != NULL) {  /* +0x1D4 */
        DeleteObject(this->backgroundBrush);
        this->backgroundBrush = NULL;
    }

    /* Call UI_WindowBase base destructor */
    UI_WindowBase_BaseDtor(this);
}

/* ================================================================== */
/* NameEntryPanel::create_window                                       */
/* Address: 0x4412F0                                                   */
/*                                                                     */
/* Creates a full-screen window covering the entire desktop.           */
/* Loads icon resource 0x65 and calls UI_CreateFullWindow.             */
/*                                                                     */
/* Called by: UI_MainMenu_Create @ 0x4205A9 (immediately after ctor)  */
/*                                                                     */
/* @param hWndParent  Parent window HWND                               */
/* @return            true on success, false on failure                */
/* ================================================================== */
bool NameEntryPanel::create_window(HWND hWndParent)
{
    /* Get desktop window dimensions */
    HWND hDesktop = GetDesktopWindow();
    RECT desktopRect;
    GetClientRect(hDesktop, &desktopRect);

    /* Load icon resource */
    HICON hIcon = LoadIconA(
        this->hInstance,
        reinterpret_cast<LPCSTR>(static_cast<uintptr_t>(0x65)));  /* +0x04 */
    this->iconHandle = hIcon;       /* +0x144 — store icon handle */

    /* Create full-screen window */
    int result = UI_CreateFullWindow(
        this,                           /* self */
        0,                              /* nCmdShow (SW_HIDE) */
        hWndParent,
        desktopRect.left,               /* x */
        desktopRect.top,                /* y */
        desktopRect.right - desktopRect.left,   /* width */
        desktopRect.bottom - desktopRect.top,   /* height */
        nullptr,                        /* hMenu = NULL */
        hIcon,                          /* icon */
        0                               /* class style */
    );

    return (result != 0);
}

/* ================================================================== */
/* NameEntryPanel::on_create (vtable[7])                                */
/* Address: 0x441360                                                   */
/*                                                                     */
/* Calls the inherited UI_WindowBase::on_create() first. Then, only    */
/* when sprites have been allocated, lays out the panel's fixed        */
/* 800x600 window rect (centered on workRect), the child-surface       */
/* blit scroll offsets (a second centering pass of a copy of workRect  */
/* within a {0,0,width,height} rect sized from the resource pointed to */
/* by spriteTerminator — repurposed by show() as a background-bitmap   */
/* resource pointer before this runs), the 7 ButtonSprites' destination*/
/* rects (each sprite's x/y/sourceX/sourceY dual-used as a RECT),      */
/* panelRect, panelClickRect, and editControlRect. Ends by calling     */
/* enumerateSessions().                                                */
/* ================================================================== */
void NameEntryPanel::on_create()
{
    UI_WindowBase::on_create();

    if (!this->hasSprites) {
        return;
    }

    /* panelWindowRect = {0,0,800,600}, centered within workRect */
    this->panelWindowRect.left = 0;
    this->panelWindowRect.right = 800;
    this->panelWindowRect.top = 0;
    this->panelWindowRect.bottom = 600;
    UI_CenterWindow(&this->workRect, &this->panelWindowRect);

    /* Second centering pass: a copy of workRect, centered within a rect
     * sized from spriteTerminator's repurposed resource pointer, produces
     * the child-surface blit's scroll offset pair + dest size. scrollOffsetX2/
     * Y2/blitDestWidth/blitDestHeight are laid out contiguously exactly like
     * a RECT (left,top,right,bottom), matching the original's reuse of that
     * 16-byte span for both purposes. */
    RECT scrolledWorkRect = this->workRect;
    RECT* scrollBlock = reinterpret_cast<RECT*>(&this->scrollOffsetX2);
    auto* bgResource = static_cast<RESDATA*>(this->spriteTerminator);
    /* The original (0x441360) dereferences spriteTerminator unconditionally
     * here, matching show()'s (0x441870) own unconditional Lock() dispatch
     * on the same resource pointer -- the original game assumes resource
     * 0x439 always resolves and never null-checks it. show() was
     * unreachable from any live NameEntryPanel vtable dispatch until this
     * 2026-08-17 pass wired it in as a real override, so this path is now
     * exercised by real GUI flows for the first time. Host resource lookup
     * (resources/resource_manager_sdl3.cpp) is not guaranteed to have
     * every original resource ID loadable in every test/tool configuration
     * the way the original install always did -- degrade to a zero-size
     * background rect instead of dereferencing null, rather than preserving
     * a crash the original binary would only have hit under equally-total
     * asset corruption. */
    uint16_t bgWidth = 0;
    uint16_t bgHeight = 0;
    if (bgResource != nullptr) {
        bgWidth = bgResource->frame_width;
        bgHeight = bgResource->frame_height;
    }
    scrollBlock->left  = 0;
    scrollBlock->right  = bgWidth;
    scrollBlock->top   = 0;
    scrollBlock->bottom = bgHeight;
    UI_CenterWindow(scrollBlock, &scrolledWorkRect);
    scrollBlock->left   = scrolledWorkRect.left;
    scrollBlock->top    = scrolledWorkRect.top;
    scrollBlock->right  = scrolledWorkRect.right;
    scrollBlock->bottom = scrolledWorkRect.bottom;

    /* sprite6 (res 0x421) dest rect, anchored 0x18/0x24 into panelWindowRect */
    this->sprite6AnchorRect.left = this->panelWindowRect.left + 0x18;
    this->sprite6AnchorRect.top  = this->panelWindowRect.top + 0x24;
    this->sprite6AnchorRect.right =
        static_cast<RESDATA*>(this->sprite6->pixelData)->frame_width +
        this->panelWindowRect.left + 0x18;
    this->sprite6AnchorRect.bottom =
        static_cast<RESDATA*>(this->sprite6->pixelData)->frame_height +
        this->sprite6AnchorRect.top;
    this->sprite6->x = this->sprite6AnchorRect.left;
    this->sprite6->y = this->sprite6AnchorRect.top;
    this->sprite6->sourceX = this->sprite6AnchorRect.right;
    this->sprite6->sourceY = this->sprite6AnchorRect.bottom;

    /* sprite5 (res 0x420) dest rect / panelRect, anchored off sprite6 */
    this->panelRect.left = this->sprite6AnchorRect.left + 1;
    this->panelRect.right =
        static_cast<RESDATA*>(this->sprite5->pixelData)->frame_width +
        this->sprite6AnchorRect.left + 1;
    this->panelRect.top = this->sprite6AnchorRect.bottom + 6;
    this->panelRect.bottom =
        static_cast<RESDATA*>(this->sprite5->pixelData)->frame_height +
        this->panelRect.top;
    this->sprite5->x = this->panelRect.left;
    this->sprite5->y = this->panelRect.top;
    this->sprite5->sourceX = this->panelRect.right;
    this->sprite5->sourceY = this->panelRect.bottom;

    /* panelClickRect, anchored off panelWindowRect's right edge and
     * sprite6AnchorRect's bottom edge */
    this->panelClickRect.left   = this->panelWindowRect.right - 0x78;
    this->panelClickRect.right  = this->panelWindowRect.right - 0x14;
    this->panelClickRect.top    = this->sprite6AnchorRect.bottom - 0x28;
    this->panelClickRect.bottom = this->sprite6AnchorRect.bottom + 0x50;

    /* sprite1 (res 0x41A) dest rect, right-anchored 200px past sprite6 */
    {
        int32_t rightEdge = this->sprite6AnchorRect.right + 200;
        this->sprite1->x = rightEdge -
            static_cast<RESDATA*>(this->sprite1->pixelData)->frame_width;
        this->sprite1->y = this->sprite6AnchorRect.bottom + 0x43;
        this->sprite1->sourceX = rightEdge;
        this->sprite1->sourceY = this->sprite1->y +
            static_cast<RESDATA*>(this->sprite1->pixelData)->frame_height;
    }

    /* sprite0 (res 0x419) dest rect, left-anchored off sprite1 */
    this->sprite0->x = this->sprite1->x -
        static_cast<RESDATA*>(this->sprite0->pixelData)->frame_width;
    this->sprite0->y = this->sprite1->y + 1;
    this->sprite0->sourceX = this->sprite1->x;
    this->sprite0->sourceY =
        static_cast<RESDATA*>(this->sprite0->pixelData)->frame_height +
        this->sprite1->y + 1;

    /* sprite4 (res 0x41F) dest rect: a {0,-height,width,0} rect centered
     * within sprite6AnchorRect, then offset by (-3, 0x46) */
    {
        RECT sprite4Rect;
        sprite4Rect.left = 0;
        sprite4Rect.right = static_cast<RESDATA*>(this->sprite4->pixelData)->frame_width;
        sprite4Rect.bottom = 0;
        sprite4Rect.top = -static_cast<int32_t>(
            static_cast<RESDATA*>(this->sprite4->pixelData)->frame_height);
        UI_CenterWindow(&this->sprite6AnchorRect, &sprite4Rect);
        OffsetRect(&sprite4Rect, -3, 0x46);
        this->sprite4->x = sprite4Rect.left;
        this->sprite4->y = sprite4Rect.top;
        this->sprite4->sourceX = sprite4Rect.right;
        this->sprite4->sourceY = sprite4Rect.bottom;
    }

    /* editControlRect, anchored off sprite4's dest rect */
    this->editControlRect.left   = this->sprite4->x + 0x48;
    this->editControlRect.top    = this->sprite4->y + 6;
    this->editControlRect.right  = this->sprite4->sourceX - 6;
    this->editControlRect.bottom = this->sprite4->sourceY - 10;

    /* sprite2 (res 0x417) dest rect, aligned under sprite4's X */
    {
        int32_t sprite4X = this->sprite4->x;
        int32_t rowTop = (sprite4X - this->sprite6AnchorRect.left) +
            this->sprite6AnchorRect.top;
        this->sprite2->x = sprite4X;
        this->sprite2->y = rowTop;
        this->sprite2->sourceX =
            static_cast<RESDATA*>(this->sprite2->pixelData)->frame_width + sprite4X;
        this->sprite2->sourceY =
            static_cast<RESDATA*>(this->sprite2->pixelData)->frame_height + rowTop;
    }

    /* sprite3 (res 0x418) dest rect, right-anchored to sprite4's sourceX,
     * top-anchored to sprite2's Y */
    {
        RECT sprite3Rect;
        sprite3Rect.top = this->sprite2->y;
        sprite3Rect.bottom = static_cast<RESDATA*>(this->sprite3->pixelData)->frame_height +
            sprite3Rect.top;
        sprite3Rect.right = this->sprite4->sourceX;
        sprite3Rect.left = sprite3Rect.right -
            static_cast<RESDATA*>(this->sprite3->pixelData)->frame_width;
        this->sprite3->x = sprite3Rect.left;
        this->sprite3->y = sprite3Rect.top;
        this->sprite3->sourceX = sprite3Rect.right;
        this->sprite3->sourceY = sprite3Rect.bottom;
    }

    this->enumerateSessions();
}

/* ================================================================== */
/* NameEntryPanel::window_proc (vtable[11])                             */
/* Address: 0x442150                                                   */
/*                                                                     */
/* WM_SYSCOMMAND (0x112) with the low nibble masked off == 0xF140       */
/* (a custom system-menu command) quits the app. WM_CTLCOLORSTATIC     */
/* (0x133) for the session-name edit control recolors it (orange text, */
/* transparent background) and returns the panel's background brush.  */
/* Everything else falls through to DefWindowProcA.                    */
/* ================================================================== */
LRESULT NameEntryPanel::window_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == 0x112) {                              /* WM_SYSCOMMAND */
        if ((wParam & 0xFFF0) == 0xF140) {
            WIN32_PostQuit();
        }
    } else if (msg == 0x133 &&                        /* WM_CTLCOLORSTATIC */
               reinterpret_cast<HWND>(lParam) == this->sessionNameEditHwnd) {
        void* hdc = reinterpret_cast<void*>(wParam);
        SetTextColor(hdc, 0xFF5C00);
        SetBkMode(hdc, 1);                             /* TRANSPARENT */
        return reinterpret_cast<LRESULT>(this->backgroundBrush);
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

/* ================================================================== */
/* NameEntryPanel::on_timer (vtable[12])                                */
/* Address: 0x4423D0                                                   */
/*                                                                     */
/* Gated on field_E8 (text-dirty flag, set by RenderConnectionPanel)    */
/* and wParam == 0x50 (the 50ms animation timer NETMAN_JoinSession       */
/* starts); anything else delegates to the inherited window_proc().    */
/* When the gate passes: optionally blits the child surface (when      */
/* hasSprites), advances textDrawRect by one scroll step in the         */
/* direction selected by gameMode's 4-state cycle, draws textBuffer     */
/* clipped to the intersection with panelRect, sets sprite5's frame,    */
/* then advances the 4-state cycle whenever the new textDrawRect        */
/* position has scrolled past the corresponding panelRect boundary —    */
/* calling RenderConnectionPanel(this) only on that transition — before */
/* always finishing with UIPANEL_EndPaint.                              */
/* ================================================================== */
LRESULT NameEntryPanel::on_timer(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (!this->field_E8 || wParam != 0x50) {
        return UI_WindowBase::window_proc(hWnd, msg, wParam, lParam);
    }

    RECT panelRectCopy = this->panelRect;

    if (this->hasSprites) {
        RECT srcRect = panelRectCopy;
        RECT dstRect = panelRectCopy;
        OffsetRect(&srcRect, this->workRect.left, this->workRect.top);
        OffsetRect(&dstRect, this->scrollOffsetX2, this->scrollOffsetY2);
        UIPANEL_Blit(this->childSurface,
                     srcRect.left, srcRect.top, srcRect.right, srcRect.bottom,
                     g_primary_surface,
                     dstRect.left, dstRect.top, dstRect.right, dstRect.bottom,
                     1);
    }

    void* hdc = UIPANEL_BeginPaint(this);

    /* Scroll direction/step selected by the 4-state cycle */
    int32_t dx;
    int32_t dy;
    switch (this->gameMode) {
        case 0:  dx = -1; dy = 0;  break;
        case 1:  dx = 1;  dy = 0;  break;
        case 2:  dx = 0;  dy = -1; break;
        default: dx = 0;  dy = 1;  break;
    }

    RECT* textRect = &this->textDrawRect;
    OffsetRect(textRect, dx, dy);
    RECT clippedRect;
    IntersectRect(&clippedRect, textRect, &panelRectCopy);

    void* hFont = *reinterpret_cast<void**>(0x4855FC);  /* matches DPlayManager.cpp's font */
    void* oldFont = SelectObject(hdc, hFont);
    int oldBkMode = SetBkMode(hdc, 1);                   /* TRANSPARENT */
    int oldTextColor = SetTextColor(hdc, 0x32C8FA);

    uint32_t format;
    if (textRect->left < panelRectCopy.left) {
        format = 0x22;                                   /* DT_RIGHT | DT_SINGLELINE */
    } else if (this->textDrawRect.top < panelRectCopy.top) {
        format = 0x28;                                   /* DT_BOTTOM | DT_SINGLELINE */
    } else {
        format = 0x20;                                   /* DT_SINGLELINE */
    }
    DrawTextA(hdc, this->textBuffer, -1, &clippedRect, format);

    SetBkMode(hdc, oldBkMode);
    SelectObject(hdc, oldFont);
    SetTextColor(hdc, oldTextColor);

    UIPANEL_EndPaintEx(this,
                       static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)),  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)
                       static_cast<int32_t>(reinterpret_cast<intptr_t>(hdc)), 1, nullptr);
    this->sprite5->setState(0, nullptr);

    /* Advance the 4-state cycle whenever the corresponding boundary was
     * crossed; RenderConnectionPanel only fires on a real transition. */
    bool boundaryHit = true;
    if (textRect->left < panelRectCopy.right || this->gameMode != 1) {
        if (panelRectCopy.top < textRect->bottom || this->gameMode != 2) {
            if (panelRectCopy.left < textRect->right || this->gameMode != 0) {
                if (this->textDrawRect.top < panelRectCopy.bottom || this->gameMode != 3) {
                    boundaryHit = false;
                } else {
                    this->gameMode = 1;
                }
            } else {
                this->gameMode = 3;
            }
        } else {
            this->gameMode = 0;
        }
    } else {
        this->gameMode = 2;
    }

    if (boundaryHit) {
        RenderConnectionPanel(this);
    }
    UIPANEL_EndPaint(this);
    return 0;
}

/* ================================================================== */
/* NameEntryPanel::applyProviderModes                                  */
/* Address: 0x4419C0 (formerly "NETMAN_CreateSession")                 */
/*                                                                     */
/* Set supportsTwoPlayerMode/supportsFourPlayerMode from the network   */
/* provider list. Not a vtable slot — an ordinary public method,       */
/* called from show() and directly by EditWindow::show()               */
/* (ui/EditWindow.cpp).                                                */
/* ================================================================== */
void NameEntryPanel::applyProviderModes()
{
#ifndef _WIN32
    /* Host-only: _g_netman_data is constructed unconditionally by
     * GameLoop_Setup on both platforms today, but any narrowly-linked
     * test/tool binary that doesn't run it still sees a null singleton
     * here — this guard keeps that configuration safe. On Windows this
     * branch cannot be taken. */
    if (_g_netman_data == nullptr) {
        return;
    }
#endif
    for (DirectPlayConnectionNode* provider = _g_netman_data->m_providerList;
         provider != nullptr; provider = provider->next) {
        if (provider->type == 2) {
            this->supportsFourPlayerMode = 1;
        } else if (provider->type == 4) {
            this->supportsTwoPlayerMode = 1;
        }
    }
}

/* ================================================================== */
/* NameEntryPanel::enumerateSessions (private)                         */
/* Address: 0x441720 (formerly "NETMAN_EnumerateSessions")             */
/*                                                                     */
/* Create the session-name EDIT child control. Called by on_create()   */
/* only.                                                                */
/* ================================================================== */
void NameEntryPanel::enumerateSessions()
{
    if (this->sessionNameEditHwnd != nullptr) return;  /* Already created */

    void* hWnd = CreateWindowExA(
        0x200,                          /* WS_EX_CLIENTEDGE */
        "EDIT",                         /* 0x47E464 — "EDIT" window class name */
        &g_empty_string,
        0x40000080,                     /* WS_CHILD | WS_VISIBLE */
        this->editControlRect.left,
        this->editControlRect.top,
        this->editControlRect.right - this->editControlRect.left,   /* width */
        this->editControlRect.bottom - this->editControlRect.top,   /* height */
        this->hWnd,                      /* parent HWND — this panel's own window,
                                          * of which the edit control is a child */
        reinterpret_cast<void*>(static_cast<uintptr_t>(0x41F)),  /* HMENU = control ID */
        this->hInstance,
        nullptr
    );

    this->sessionNameEditHwnd = hWnd;

    if (hWnd != nullptr) {
        PostMessageA(hWnd, 0x30 /* WM_SETFONT */,
                     static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_font_small)), 1);
        PostMessageA(hWnd, 0xC5 /* EM_LIMITTEXT */, 0x40, 0);

        GameConfig* const config = _g_netman_data;
#ifndef _WIN32
        if (config != nullptr)
#endif
        {
            SetWindowTextA(hWnd, config->m_sessionName);
        }

        /* Subclass the edit control */
        void* oldWndProc = SetWindowLongA(hWnd, -4 /* GWL_WNDPROC */,
                                           reinterpret_cast<void*>(&NETMAN_EditControlSubclassProc));  // ABI_BOUNDARY: function pointer marshaled through SetWindowLongA's void* WNDPROC parameter, a genuine Win32 callback-registration boundary
        this->originalEditWndProc = oldWndProc;
    }
}

/* ================================================================== */
/* NameEntryPanel::getSessionInfo (private)                            */
/* Address: 0x441B40 (formerly "NETMAN_GetSessionInfo")                */
/*                                                                     */
/* Refresh sprite visibility based on session mode flags. Called by    */
/* on_update() and on_lbutton_down() (after toggling a player-count    */
/* mode).                                                               */
/* ================================================================== */
void NameEntryPanel::getSessionInfo()
{
    GameConfig* const config = _g_netman_data;

    this->sprite6->setState(0, nullptr);

#ifndef _WIN32
    if (config == nullptr) {
        return;  /* Host-only: see applyProviderModes()'s comment above. */
    }
#endif

    if (config->m_hostMode == 0) {
        /* Host mode */
        if (this->supportsTwoPlayerMode != 0) {
            if (config->m_hostPlayerCount == 4) {
                this->sprite2->setState(1, nullptr);
                ShowWindow(this->sessionNameEditHwnd, 0);
            } else {
                this->sprite2->setState(0, nullptr);
            }
        }
        if (this->supportsFourPlayerMode == 0) return;

        /* 4-player mode */
        if (config->m_hostPlayerCount == 2) {
            this->sprite4->setState(0, nullptr);
            this->sprite3->setState(1, nullptr);
            ShowWindow(this->sessionNameEditHwnd, 5);  /* SW_SHOW */
            SetFocus(this->sessionNameEditHwnd);
            return;
        }
        ShowWindow(this->sessionNameEditHwnd, 0);
    } else {
        /* Client mode */
        ShowWindow(this->sessionNameEditHwnd, 0);
        if (this->supportsTwoPlayerMode != 0) {
            this->sprite2->setState(
                static_cast<int32_t>(config->m_clientPlayerCount == 4), nullptr);
        }
        if (this->supportsFourPlayerMode == 0) return;
        if (config->m_clientPlayerCount == 2) {
            this->sprite3->setState(1, nullptr);
            return;
        }
    }
    this->sprite3->setState(0, nullptr);
}

/* ================================================================== */
/* NameEntryPanel::hide (vtable[1])                                    */
/* Address: 0x441A00 (formerly "NETMAN_LeaveSession")                  */
/*                                                                     */
/* Kill the animation timer, destroy the 7 sprites and unlock the      */
/* child-surface resource, then chain to the inherited                 */
/* UI_WindowBase::hide().                                              */
/* ================================================================== */
void NameEntryPanel::hide()
{
    /* animationTimerId (+0xEC), NOT the inherited UI_WindowBase::timerId
     * (+0x28, UI_WindowBase::show()'s own separate 120ms timer) — see
     * animationTimerId's header doc comment for the ×4-scaled-offset
     * evidence distinguishing the two. */
    KillTimer(this->hWnd, this->animationTimerId);

    if (this->hasSprites) {
        /* res->Unlock() (ResourceObject vtable slot 2 — Lock() is slot 1,
         * confirmed via show()'s own Lock() call on this same resource
         * pointer). Ghidra shows this called with no explicit arguments
         * (unlike show()'s Lock() call, which does pass explicit (0, 0)
         * args per ui/AboutDialog.cpp's convention). */
        if (this->spriteTerminator != nullptr) {
            static_cast<ResourceObject*>(this->spriteTerminator)->Unlock();
        }

        this->sprite0->destroy();
        this->sprite1->destroy();
        this->sprite2->destroy();
        this->sprite3->destroy();
        this->sprite4->destroy();
        this->sprite5->destroy();
        this->sprite6->destroy();
        this->hasSprites = 0;
    }

    /* The original calls UI_WindowBase_Hide directly (not through the
     * vtable) — this function IS the vtable[1] override, so this is a
     * non-virtual base-class chain-up, not a virtual re-dispatch. */
    UI_WindowBase::hide();
}

/* ================================================================== */
/* NameEntryPanel::show (vtable[2])                                    */
/* Address: 0x441870 (formerly "NETMAN_JoinSession")                   */
/*                                                                     */
/* Initialize and show the join-session UI panel.                      */
/* ================================================================== */
void NameEntryPanel::show()
{
    /* Mark paint-ready flag as false initially. */
    this->paintReadyFlag = 0;

    if (!this->hasSprites) {
        /* Allocate and initialize 7 sprites, plus a resource-backed child
         * surface (unrelated to the 7 ButtonSprites). spriteTerminator/
         * childSurface are repurposed here per their header documentation. */
        void* res = ResourceManager_GetById(&g_resmgr, 0x439);
        this->spriteTerminator = res;

        /* Real ResourceObject::Lock(0, 0) virtual dispatch, matching
         * ui/AboutDialog.cpp's identical pattern for the same
         * ResourceManager_GetById-sourced resource. */
        if (res != nullptr) {
            this->childSurface = static_cast<ResourceObject*>(res)->Lock(0, 0);
        }

        this->sprite0->init();
        this->sprite1->init();
        this->sprite2->init();
        this->sprite3->init();
        this->sprite4->init();
        this->sprite5->init();
        this->sprite6->init();

        this->hasSprites = 1;
    }

    /* vtable slot [7]: on_create() — a real, genuinely-overridable virtual
     * call. */
    this->on_create();

    /* Set 2-player/4-player mode-availability flags from the provider
     * list (the original inlines an identical copy of this loop here;
     * folded into the one shared applyProviderModes() implementation). */
    this->applyProviderModes();

    /* The original calls UI_WindowBase_Show directly (not through the
     * vtable) — same non-virtual base-class chain-up as hide() above. */
    UI_WindowBase::show();
    SetFocus(this->hWnd);

    /* vtable slot [3]: set_mode() — a real, genuinely-overridable virtual
     * call, inherited from UI_WindowBase. */
    this->set_mode(this->childCount0, this->childObj0, 0, 1);

    /* Load and play sound resource */
    {
        int32_t soundId = ResourceManager_GetStringById(&g_resmgr, 0x5015);
        if (soundId != 0) {
            RESMGR_LoadSoundResource(soundId);
        }
    }

    /* Start animation timer (50ms interval). Stored in animationTimerId
     * (+0xEC), NOT the inherited UI_WindowBase::timerId (+0x28) which
     * UI_WindowBase::show() (called above) already set to its own,
     * separate 120ms timer (id 0x43) — overwriting timerId here would
     * make hide()'s KillTimer(timerId) kill the wrong timer twice and
     * leak UI_WindowBase::show()'s own. See animationTimerId's header
     * doc comment for the ×4-scaled-offset evidence. */
    this->animationTimerId = static_cast<UINT_PTR>(reinterpret_cast<uintptr_t>(
        SetTimer(this->hWnd, 0x50, 0x32, nullptr)));
    this->gameMode = 2;  /* initial marquee-scroll state */

    FormatResourceString(&g_resmgr, 0x79, this->textBuffer, sizeof(this->textBuffer));
    RenderConnectionPanel(this);
}

/* ================================================================== */
/* NameEntryPanel::on_update (vtable[8])                                */
/* Address: 0x441A90 (formerly "NETMAN_UpdateSessionInfo")             */
/*                                                                     */
/* Blit child surface, update sprite states, refresh session info, end */
/* paint.                                                               */
/* ================================================================== */
void NameEntryPanel::on_update(int32_t /*param*/)
{
    UIPANEL_Blit(
        this->childSurface,
        static_cast<uint32_t>(this->workRect.left),    /* srcX */
        static_cast<uint32_t>(this->workRect.top),      /* srcY */
        this->workRect.right,                            /* srcW */
        static_cast<uint32_t>(this->workRect.bottom),   /* srcH */
        g_primary_surface,
        static_cast<uint32_t>(this->scrollOffsetX2),     /* dstX */
        static_cast<uint32_t>(this->scrollOffsetY2),     /* dstY */
        this->blitDestWidth,                              /* dstW */
        static_cast<uint32_t>(this->blitDestHeight),     /* dstH */
        1
    );

    this->sprite6->setState(0, nullptr);
    this->sprite0->setState(0, nullptr);
    this->sprite1->setState(0, nullptr);

    this->getSessionInfo();

    UIPANEL_EndPaintEx(this, static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)), 0, 0, nullptr);
    this->paintReadyFlag = 1;
}

/* ================================================================== */
/* NameEntryPanel::on_lbutton_down (vtable[14])                        */
/* Address: 0x441C80 (formerly "NETMAN_SetSessionInfo")                */
/*                                                                     */
/* Handle UI click/hit-test on session panel sprites.                  */
/* ================================================================== */
LRESULT NameEntryPanel::on_lbutton_down(HWND /*hWnd*/, UINT /*msg*/, WPARAM /*wParam*/,
                                         LPARAM lParam)
{
    if (this->paintReadyFlag == 0) return 0;

    POINT pt;
    pt.x = static_cast<int32_t>(lParam & 0xFFFF);
    pt.y = static_cast<int32_t>(lParam >> 16);

    RECT sprite0Rect{ this->sprite0->x, this->sprite0->y,
                       this->sprite0->sourceX, this->sprite0->sourceY };
    if (PtInRect(&sprite0Rect, pt)) {
        /* Hit-test sprite0 (btn_back/cancel) */
        this->sprite0->setState(1, nullptr);
        PlaySound(0x5015);
        this->EndPaint();
        Sleep(0x96);
        this->set_render_surface(nullptr, 0, nullptr, 0, 1);

        GameConfig* const config = _g_netman_data;
#ifndef _WIN32
        if (config != nullptr)
#endif
        {
            GetWindowTextA(this->sessionNameEditHwnd, config->m_sessionName,
                            sizeof(config->m_sessionName));
            if (config->m_hostMode == 0) {
                config->m_clientAutoFlag = 1;
            } else {
                config->m_hostFlagAuto = 1;
            }
            NETMAN_SendPacket(config);
        }
        UI_MainMenu_SetState(g_ui_main, 3);
        return 0;
    }

    RECT sprite1Rect{ this->sprite1->x, this->sprite1->y,
                       this->sprite1->sourceX, this->sprite1->sourceY };
    if (PtInRect(&sprite1Rect, pt)) {
        /* Hit-test sprite1 (btn_join/ok) */
        this->sprite1->setState(1, nullptr);
        PlaySound(0x5015);
        this->EndPaint();
        Sleep(0x96);
        this->set_render_surface(nullptr, 0, nullptr, 0, 1);
        UI_MainMenu_SetState(g_ui_main, 7);
        return 0;
    }

    RECT sprite2Rect{ this->sprite2->x, this->sprite2->y,
                       this->sprite2->sourceX, this->sprite2->sourceY };
    if (PtInRect(&sprite2Rect, pt) && this->supportsTwoPlayerMode != 0) {
        /* Hit-test sprite2 (2-player button) */
        GameConfig* const config = _g_netman_data;
#ifndef _WIN32
        if (config != nullptr)
#endif
        {
            if (config->m_hostMode == 0) {
                config->m_hostPlayerCount = 4;
            } else {
                config->m_clientPlayerCount = 4;
            }
        }
        this->getSessionInfo();
        PlaySound(0x5015);
        UIPANEL_EndPaintEx(this, static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)), 0, 0, nullptr);
        return 0;
    }

    RECT sprite3Rect{ this->sprite3->x, this->sprite3->y,
                       this->sprite3->sourceX, this->sprite3->sourceY };
    if (PtInRect(&sprite3Rect, pt) && this->supportsFourPlayerMode != 0) {
        /* Hit-test sprite3 (4-player button) */
        GameConfig* const config = _g_netman_data;
#ifndef _WIN32
        if (config != nullptr)
#endif
        {
            if (config->m_hostMode == 0) {
                config->m_hostPlayerCount = 2;
            } else {
                config->m_clientPlayerCount = 2;
            }
        }
        this->getSessionInfo();
        PlaySound(0x5015);
        UIPANEL_EndPaintEx(this, static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)), 0, 0, nullptr);
        return 0;
    }

    /* Hit-test panel background (panelClickRect) for random ambience sound */
    if (PtInRect(&this->panelClickRect, pt)) {
        int32_t rnd = CRT_rand();
        PlaySoundAt(rnd / 0x1FFF + 0x500F, pt.x, pt.y, 4);
    }

    return 0;
}

/* ================================================================== */
/* NameEntryPanel::on_key_down (vtable[21])                             */
/* Address: 0x441F80 (formerly "NETMAN_DestroySession",                */
/* native/NETMAN_SessionSettings.c)                                     */
/*                                                                     */
/* Handle ENTER (confirm/join) and ESC (cancel) key presses.           */
/* ================================================================== */
LRESULT NameEntryPanel::on_key_down(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (this->paintReadyFlag == 0) {
        return DefWindowProcA(hWnd, msg, wParam, lParam);
    }

    if (wParam != 0x0D && wParam != 0x1B) {   /* VK_RETURN / VK_ESCAPE */
        return DefWindowProcA(hWnd, msg, wParam, lParam);
    }

    if (wParam == 0x1B) {
        /* ESC pressed — cancel/back */
        this->sprite1->setState(1, nullptr);
        UIPANEL_EndPaintEx(this, static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)), 0, 0, nullptr);  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)
        Sleep(0x96);
        this->set_render_surface(nullptr, 0, nullptr, 0, 1);
        UI_MainMenu_SetState(g_ui_main, 7);
        return 0;
    }

    /* wParam == 0x0D: ENTER pressed — confirm/join */
    this->sprite0->setState(1, nullptr);
    UIPANEL_EndPaintEx(this, static_cast<int32_t>(reinterpret_cast<intptr_t>(this->hWnd)), 0, 0, nullptr);  // ABI_BOUNDARY: opaque OS HWND round-tripped through the original function's int hdc param (matches ui/UIPANEL.cpp's UIPANEL_EndPaint wrapper)
    Sleep(0x96);
    this->set_render_surface(nullptr, 0, nullptr, 0, 1);

    GameConfig* const config = _g_netman_data;
#ifndef _WIN32
    if (config != nullptr)
#endif
    {
        GetWindowTextA(this->sessionNameEditHwnd, config->m_sessionName, sizeof(config->m_sessionName));
        if (config->m_hostMode == 0) {
            config->m_clientAutoFlag = 1;
        } else {
            config->m_hostFlagAuto = 1;
        }
        NETMAN_SendPacket(config);
    }
    UI_MainMenu_SetState(g_ui_main, 3);
    return 0;
}
