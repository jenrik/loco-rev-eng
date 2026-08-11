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
}

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
    this->field_EC = 0;             /* +0xEC */
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
/* by spriteTerminator — repurposed by NETMAN_JoinSession as a         */
/* background-bitmap resource pointer before this runs), the 7         */
/* ButtonSprites' destination rects (each sprite's x/y/sourceX/sourceY */
/* dual-used as a RECT), panelRect, panelClickRect, and                */
/* editControlRect. Ends by calling NETMAN_EnumerateSessions(this).    */
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
    scrollBlock->left  = 0;
    scrollBlock->right  = bgResource->frame_width;
    scrollBlock->top   = 0;
    scrollBlock->bottom = bgResource->frame_height;
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

    NETMAN_EnumerateSessions(this);
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

    UIPANEL_EndPaintEx(this, this->hWnd,
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
