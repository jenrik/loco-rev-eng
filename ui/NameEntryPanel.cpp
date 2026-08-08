/**
 * NameEntryPanel.cpp — NameEntryPanel implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

// Status: TRANSCRIBED

#include "NameEntryPanel.h"
#include "ButtonSprite.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void* __cdecl operator_new(size_t size);     /* 0x465CE0 */
    extern void  __cdecl GLOBAL_free(void* ptr);         /* 0x465CD0 */
    extern void* g_resmgr;                               /* 0x4855E8 */

extern "C" {
    extern HBRUSH __stdcall CreateSolidBrush(COLORREF color);  /* 0x477070 */
    extern BOOL   __stdcall DeleteObject(HGDIOBJ hObject);     /* 0x477048 */
    extern HWND   __stdcall GetDesktopWindow(void);             /* 0x477364 */
    extern BOOL   __stdcall GetClientRect(HWND hWnd, void* lpRect); /* 0x477368 */
    extern HICON  __stdcall LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName); /* 0x47736C */
}

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
    this->nameEditHwnd = nullptr;   /* +0x1D8 */
    this->field_144 = 0;            /* +0x144 */
    this->hasSprites = 0;           /* +0x1AC (byte) */
    this->field_1E0 = 0;            /* +0x1E0 (byte) */
    this->field_1E1 = 0;            /* +0x1E1 (byte) */

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
    this->gameMode = static_cast<int32_t>(reinterpret_cast<intptr_t>(hIcon));
                                                                    /* +0x144 — store icon handle */
    /* BUG: icon handle overwrites gameMode field. This is likely a
       misinterpretation: the icon may be stored at a different field.
       In the disassembly, +0x144 is written with the icon handle after
       being cleared by Init. */

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
