/**
 * GameSetupPanel.cpp — GameSetupPanel implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Renders a full-screen lobby for game setup / city selection. The render
 * pass blits a background, draws title text, a scrollable layout list, a
 * player grid, and overlays ButtonSprites. Supports two modes: scenario
 * list (titleList) and layout list (layoutList), toggled by a global flag.
 */

// Status: TRANSCRIBED

#include "GameSetupPanel.h"
#include "ButtonSprite.h"
#include "../network/Netman.h"
#include "../resources/ResourceManager.h"
#include "WndProcStream.h"

/* AssetMgr forward-declared (struct matches AssetMgr.h layout);
   AssetMgr_LoadFile declared in Netman.h */
/* TODO: include AssetMgr.h directly once OutputDebugStringA conflict is resolved */
struct AssetMgr;
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* Heap management */
extern void* __cdecl operator_new(size_t size);     /* 0x465CE0 */
extern void  __cdecl GLOBAL_free(void* ptr);         /* 0x465CD0 */

extern "C" {
    /* Win32 APIs (imported through IAT) */
    extern HWND   __stdcall GetDesktopWindow(void);             /* 0x477364 */
    extern BOOL   __stdcall GetClientRect(HWND, RECT* lpRect);  /* 0x477368 */
    extern HICON  __stdcall LoadIconA(HINSTANCE, LPCSTR);       /* 0x47736C */
    extern BOOL   __stdcall DeleteObject(HGDIOBJ);              /* 0x477048 */
    extern void*  __stdcall GetStockObject(int fnObject);       /* 0x477050 */
    extern int    __stdcall SetBkMode(HDC hdc, int mode);       /* 0x477064 */
    extern int    __stdcall SetBkColor(HDC hdc, int color);     /* 0x47706C */
    extern int    __stdcall SetTextColor(HDC hdc, int color);   /* 0x477060 */
    extern HGDIOBJ __stdcall SelectObject(HDC hdc, HGDIOBJ obj); /* 0x47703C */
    extern int    __stdcall DrawTextA(HDC hdc, const char* str,
                                      int len, RECT* rect, int format); /* 0x477348 */
    /* CopyRect, OffsetRect already declared in Netman.h with typed RECT params */
    extern BOOL   __stdcall KillTimer(HWND hWnd, UINT_PTR id);  /* 0x4772F0 */
    extern int    __stdcall wsprintfA(char* buf, const char* fmt, ...); /* 0x477370 */
}

/* CRT helpers */
extern int    __cdecl CRT_sprintf_buf(char* buf, const char* fmt, ...); /* 0x466D60 */
extern void   __cdecl CRT_exit(const char** msg, const char** fileLine); /* 0x466CE0 */
extern void   __cdecl CRT_free(void* ptr);                              /* 0x466C70 */

/* Resource manager — use g_resmgr.GetById() / g_resmgr.FormatResourceString() */
/* ReleaseSoundResource declared in ResourceManager.h (included above) */

/* Stream / WNDPROC helpers — TODO: decompile WndProcStream class (0x464490, 0x463810, etc.) */
extern void*  WIN32_StreamOpen(void* stream, const char* path, int mode, void* extra, int flag);
extern void   WIN32_StreamRead(void* stream, void* buf, int sz);        /* 0x463810 */
extern int*   WNDPROC_StreamFromMemory(void* stream, const char* data,
                                        int size, int mode);             /* 0x464490 */

/* UI_CreateFullWindow — use UI_WindowBase::create_full_window (static method) */
/* (declared in UI_WindowBase.h, included above) */

/* UIPANEL functions — TODO: decompile RenderSurface/UIPANEL render helpers */
extern void   UIPANEL_Blit(void* sprite, int dstX, int dstY,
                           int dstW, int dstH, void* surface,
                           int srcX, int srcY, int srcW,
                           int srcH, int mode);            /* 0x42B050 */
extern void*  UIPANEL_BeginPaint(void* self);              /* 0x426B00 */
extern void   UIPANEL_EndPaintEx(void* self, HWND hWnd,
                                  void* hdc, byte flags,
                                  void* rect);              /* 0x426B90 */

/* UI_CenterWindow — TODO: decompile 0x425A50 */
extern void   UI_CenterWindow(void* param1, void* param2); /* 0x425A50 */


/* ================================================================== */
/* Global variables                                                    */
/* ================================================================== */

/* _g_primary_surface now declared in shared/types.h */
/* Netman.h declares extern void* _g_netman at 0x4FD3AC.
   Re-declare with correct type here to enable typed field access.
   With -fpermissive, the last extern declaration in the TU wins.
   NOTE: Netman.h also declares _g_dplay_config at 0x4FD3AC (same address
   as _g_netman) — this is a known error in Netman.h. xref analysis confirms
   0x4FD3AC is the Netman singleton (written by GameLoop_Setup @ 0x406C9D).
   _g_dplay is at 0x4FD3A8 (written by GameLoop_Setup @ 0x406C6C). */
extern Netman* _g_netman;              /* 0x4FD3AC — network manager singleton */
/* g_resmgr now declared in ResourceManager.h (included above) */
/* g_asset_mgr declared as AssetMgr* in Netman.h; typed access available */
extern void* g_font_normal;           /* 0x4855F0 — normal UI font handle */
extern void* g_font_small;            /* 0x4855F4 — small UI font handle */
extern void* g_title_font;            /* 0x4855FC — title display font handle */
extern char  g_install_path[];        /* 0x4A99C8 — install directory path */
extern int   g_stream_open_mode;      /* 0x479190 — stream open mode flags for WIN32_StreamOpen */

/* The binary uses an empty string at fixed address 0x47E2C8 for
   DrawTextA DT_CALCRECT font measurement. The string content is
   the empty string — the address, not the content, matters to the
   binary. We use "" for standalone builds. */
static const char* const g_font_measure_string = "";

/* Sentinel value: (LayoutListNode*)-1 means "use cached currentList".
   The binary uses the -1 pointer sentinel pattern; preserved here
   with a typed constant to avoid void* comparison UB. */
static LayoutListNode* const CACHED_SENTINEL = (LayoutListNode*)-1;

/* ================================================================== */
/* GameSetupPanel Constructor                                          */
/* Address: 0x408AA0 (verified: Ghidra shows CGWND_GameSetup_Ctor      */
/*   0x408AA0-0x408AF7. EditWindow.cpp annotation 0x408A70 is inside   */
/*   CGWND_ReadRegistryString — no thunk at that address.)              */
/* ================================================================== */
GameSetupPanel::GameSetupPanel(HINSTANCE hInstance, UINT resId)
    : UI_WindowBase(hInstance, resId)
{
    /* Constructor body (in SEH frame) */
/* In the binary: sets vtable here. Compiler-managed in natural C++. */
    this->init();                                /* init fields + sprites */
}

/* ================================================================== */
/* GameSetupPanel::scalar deleting destructor (vtable[0])              */
/* Address: 0x408B00                                                   */
/* ================================================================== */
GameSetupPanel::~GameSetupPanel()
{
    this->base_destructor();
}

/* ================================================================== */
/* GameSetupPanel::init                                                */
/* Address: 0x408B20                                                   */
/* ================================================================== */
void GameSetupPanel::init()
{
    /* Zero all fields */
    this->hIcon = NULL;               /* +0x1B4 */
    this->spritesCreated = 0;         /* +0x21C (byte) */
    this->titleList = NULL;           /* +0xEC */
    this->layoutList = NULL;          /* +0xF0 */
    this->selectedEntry = 0;          /* +0xF4 */
    this->lineHeight = 0x10;          /* +0x100 */
    this->displayedCount = 0;         /* +0x104 */
    this->field_10C = 0;              /* +0x10C (byte) */
    this->field_E8 = 0;               /* +0xE8 (byte) */
    this->field_110 = 0;              /* +0x110 */
    this->field_F8 = 0;               /* +0xF8 */
    this->field_FC = 0;               /* +0xFC */
    this->currentList = NULL;         /* +0x108 */
    this->renderFlag = 0;             /* +0x1B8 (byte) */
    this->timerId1 = 0;               /* +0x118 */
    this->timerId2 = 0;               /* +0x11C */
    this->titleDrawnFlag = 0;         /* +0x114 */
    this->field_234 = NULL;           /* +0x234 */
    this->textAlignMode = 3;          /* +0x1B0 */
    this->backgroundSprite = NULL;    /* +0x238 — init to NULL; pixel data set by external caller */

    /* NOTE: spritesCreated (+0x21C) remains 0 after init(). It is set to 1 by
       external code (likely the OnCreate callback or EditWindow caller) after
       all sprite pixel data has been loaded. All blocks guarded by
       'if (this->spritesCreated != 0)' are dead code until that write occurs. */

    /* Create 5 main ButtonSprites */
    this->titleSprite = new ButtonSprite(0x42A);     /* +0x220 */
    this->field_224   = new ButtonSprite(0x42C);     /* +0x224 */
    this->field_228   = new ButtonSprite(0x429);     /* +0x228 */
    this->field_22C   = new ButtonSprite(0x42B);     /* +0x22C */
    this->field_230   = new ButtonSprite(0x42F);     /* +0x230 */

    /* field at +0x120 (byte, set to 0) */
    /* NOTE: Binary only zeroes titleText[0] — remaining 127 bytes are
   uninitialized. updateTitle() always writes a full null-terminated string
   via FormatResourceString before drawTitle() reads the buffer. */
    this->titleText[0] = '\0';        /* +0x120 (byte) */

    /* Create 9 layout slot ButtonSprites (res 0x43A..0x442) */
    for (int i = 0; i < 9; i++) {
        this->layoutSprite[i] = new ButtonSprite(0x43A + i);  /* +0x23C + i*4 */
    }
}

/* ================================================================== */
/* GameSetupPanel::base_destructor                                     */
/* Address: 0x408D10                                                   */
/* ================================================================== */
void GameSetupPanel::base_destructor()
{
/* In the binary: sets vtable here. Compiler-managed in natural C++. */

    /* Free title list linked list (+0xEC) */
    {
        LayoutListNode* node = this->titleList;
        while (node != NULL) {
            LayoutListNode* next = node->next;                /* node[0] = next pointer */
            if (node->name != NULL) {                          /* node[2] = name string */
                GLOBAL_free(node->name);
            }
            GLOBAL_free(node);
            this->titleList = next;
            node = next;
        }
    }

    /* Free layout list linked list (+0xF0) — same structure */
    {
        LayoutListNode* node = this->layoutList;
        while (node != NULL) {
            LayoutListNode* next = node->next;
            if (node->name != NULL) {
                GLOBAL_free(node->name);
            }
            GLOBAL_free(node);
            this->layoutList = next;
            node = next;
        }
    }

    /* If sprites have been created, destroy pixel data and free sprites */
    if (this->spritesCreated != 0) {  /* +0x21C */
        /* Destroy pixel data on 5 main sprites */
        this->titleSprite->destroy();    /* +0x220 */
        this->field_224->destroy();      /* +0x224 */
        this->field_228->destroy();      /* +0x228 */
        this->field_22C->destroy();      /* +0x22C */
        this->field_230->destroy();      /* +0x230 */

        /* Release sub-object at +0x234 via Release() (vtable[2]) */
        if (this->field_234 != NULL) {
            this->field_234->Release();
        }
        this->field_234 = NULL;

        /* Release background sprite at +0x238 via Release() (vtable[2]) */
        if (this->backgroundSprite != NULL) {
            this->backgroundSprite->Release();
        }
        this->backgroundSprite = NULL;

        /* Destroy pixel data and free all 9 layout sprites */
        for (int i = 0; i < 9; i++) {
            this->layoutSprite[i]->destroy();
            delete this->layoutSprite[i];
            this->layoutSprite[i] = NULL;
        }

        this->spritesCreated = 0;  /* +0x21C */
    }

    /* Release sound resource 0x5015 */
    {
        int sndHandle = g_resmgr.GetById(0x5015);
        if (sndHandle != 0) {
            /* NOTE: GetById returns int32_t; ReleaseSoundResource expects ResourceEntry*.
               On 32-bit x86 these are the same width — safe cast. */
            ReleaseSoundResource((ResourceEntry*)sndHandle);
        }
    }

    /* Free main sprites via scalar dtor in fixed order:
       +0x22C, +0x228, +0x224, +0x220, +0x230 */
    if (this->field_22C != NULL) {
        delete this->field_22C;
        this->field_22C = NULL;
    }
    if (this->field_228 != NULL) {
        delete this->field_228;
        this->field_228 = NULL;
    }
    if (this->field_224 != NULL) {
        delete this->field_224;
        this->field_224 = NULL;
    }
    if (this->titleSprite != NULL) {
        delete this->titleSprite;
        this->titleSprite = NULL;
    }
    if (this->field_230 != NULL) {
        delete this->field_230;
        this->field_230 = NULL;
    }

    /* Call UI_WindowBase base destructor */
    this->UI_WindowBase::base_destructor();
}

/* ================================================================== */
/* GameSetupPanel::create_window                                       */
/* Address: 0x408F00                                                   */
/* ================================================================== */
bool GameSetupPanel::create_window(HWND hWndParent)
{
    HWND hDesktop = GetDesktopWindow();
    RECT desktopRect;
    GetClientRect(hDesktop, &desktopRect);

    HICON icon = LoadIconA(this->hInstance, (LPCSTR)0x65);  /* +0x04 */
    this->hIcon = icon;  /* +0x1B4 */

    int result = UI_WindowBase::create_full_window(
        this,
        0,                              /* nCmdShow (SW_HIDE) */
        hWndParent,
        desktopRect.left,
        desktopRect.top,
        desktopRect.right - desktopRect.left,
        desktopRect.bottom - desktopRect.top,
        (HMENU)0,
        icon,
        0                               /* class style */
    );

    return (result != 0);
}

/* ================================================================== */
/* GameSetupPanel::render (vtable[8])                                   */
/* Address: 0x409280                                                    */
/*                                                                      */
/* Full render pass:                                                    */
/*   1. Blit background sprite to primary surface                       */
/*   2. Update title text from resource strings                         */
/*   3. Reset main button sprites to state 0                           */
/*   4. Choose active list (layoutList or titleList) based on global    */
/*   5. Draw layout list entries                                        */
/*   6. Draw player grid                                                */
/*   7. Set renderFlag                                                  */
/*   8. End paint operation                                             */
/* ================================================================== */
void GameSetupPanel::render(int unused)
{
    /* Step 1: Blit background sprite (at +0x238) to primary surface.
       Source rect: clipRect (+0x1CC), Destination rect: workRect (+0xD4) */
    UIPANEL_Blit(
        this->backgroundSprite,           /* +0x238 */
        this->workRect.left,              /* +0xD4 */
        this->workRect.top,               /* +0xD8 */
        this->workRect.right,             /* +0xDC */
        this->workRect.bottom,            /* +0xE0 */
        _g_primary_surface,               /* 0x4FD3C4 */
        this->clipRect.left,              /* +0x1CC */
        this->clipRect.top,               /* +0x1D0 */
        this->clipRect.right,             /* +0x1D4 */
        this->clipRect.bottom,            /* +0x1D8 */
        1                                 /* blit mode */
    );

    /* Step 2: Update title text */
    this->updateTitle();

    /* Step 3: Reset main button sprites to state 0 */
    this->field_224->setState(0, NULL);   /* +0x224 */
    this->field_228->setState(0, NULL);   /* +0x228 */
    this->field_22C->setState(0, NULL);   /* +0x22C */

    /* Step 4: Choose active list based on m_playerSlotCount flag */
    LayoutListNode* activeList;
    if (_g_netman->m_playerSlotCount == 0) {
        activeList = this->layoutList;    /* +0xF0 */
    } else {
        activeList = this->titleList;     /* +0xEC */
    }

    /* Step 5: Draw the layout/scenario list */
    this->drawLayoutList(activeList);

    /* Step 6: Draw the player grid */
    this->drawGrid();

    /* Step 7: Set render flag */
    this->renderFlag = 1;                 /* +0x1B8 */

    /* Step 8: End paint */
    UIPANEL_EndPaintEx(this, this->hWnd, static_cast<int>(0), 0, nullptr);
}

/* ================================================================== */
/* GameSetupPanel::updateTitle                                         */
/* Address: 0x409360                                                   */
/*                                                                      */
/* Updates the title text buffer (+0x120) based on network state and    */
/* display mode. Options:                                               */
/*   - Network mode active  -> resource 0x71 ("Network Game")           */
/*   - titleList selected   -> resource 0x70 ("Select Layout")          */
/*   - layoutList selected  -> resource 0x6F ("Select Scenario")        */
/* Always refreshes the drawn title after updating the buffer.          */
/* ================================================================== */
void GameSetupPanel::updateTitle()
{
    /* Check if network mode is active */
    /* _g_netman->m_bFlag1 (+0x7C8) = network session active flag */
    /* _g_netman->m_bInit  (+0x04)  = network mode flag */
    if (_g_netman->m_bFlag1 != 0 && _g_netman->m_bInit != 0) {
        /* Network mode: hide the title sprite and use resource 0x71 */
        this->titleSprite->setState(0, NULL);
        g_resmgr.FormatResourceString(0x71, this->titleText, 0x80);
    } else {
        /* Single-player mode: remember title sprite position */
        RECT spriteRect;
        spriteRect.left   = this->titleSprite->x;
        spriteRect.top    = this->titleSprite->y;
        spriteRect.right  = this->titleSprite->sourceX;
        spriteRect.bottom = this->titleSprite->sourceY;

        /* Restore background behind the title sprite if sprites exist */
        if (this->spritesCreated != 0) {
            RECT workRectCopy, clipRectCopy;
            CopyRect(&workRectCopy, &spriteRect);
            CopyRect(&clipRectCopy, &spriteRect);
            OffsetRect(&workRectCopy, this->workRect.left, this->workRect.top);
            OffsetRect(&clipRectCopy, this->clipRect.left, this->clipRect.top);
            UIPANEL_Blit(
                this->backgroundSprite,
                workRectCopy.left, workRectCopy.top,
                workRectCopy.right, workRectCopy.bottom,
                _g_primary_surface,
                clipRectCopy.left, clipRectCopy.top,
                clipRectCopy.right, clipRectCopy.bottom,
                1
            );
        }

        /* Select resource string based on m_playerSlotCount flag */
        UINT resId;
        if (_g_netman->m_playerSlotCount == 0) {
            resId = 0x6F;  /* "Select Scenario" */
        } else {
            resId = 0x70;  /* "Select Layout" */
        }
        g_resmgr.FormatResourceString(resId, this->titleText, 0x80);
    }

    /* Draw the updated title */
    this->drawTitle();
}

/* ================================================================== */
/* GameSetupPanel::drawLayoutList                                      */
/* Address: 0x4094B0                                                   */
/*                                                                      */
/* Renders the scenario/layout list into the layoutListRect region.     */
/* Parameters:                                                          */
/*   list  - Pointer to the linked list to render (titleList or        */
/*           layoutList). Pass (void*)-1 to use cached currentList.    */
/*                                                                      */
/* Draws each entry's name from node[2] (offset +8 in 0xC-byte node).  */
/* Entries before selectedEntry are drawn in highlight color.           */
/* Empty list shows fallback text from resource 0x7F.                  */
/* ================================================================== */
void GameSetupPanel::drawLayoutList(LayoutListNode* list)
{
    /* Store the list pointer */
    this->currentList = list;                    /* +0x108 */

    /* Load layout list rect */
    RECT listRect;
    listRect.left   = this->layoutListRect.left;
    listRect.top    = this->layoutListRect.top;
    listRect.right  = this->layoutListRect.right;
    listRect.bottom = this->layoutListRect.bottom;

    /* Restore background if sprites exist */
    if (this->spritesCreated != 0) {
        RECT workRectCopy, clipRectCopy;
        CopyRect(&workRectCopy, &listRect);
        CopyRect(&clipRectCopy, &listRect);
        OffsetRect(&workRectCopy, this->workRect.left, this->workRect.top);
        OffsetRect(&clipRectCopy, this->clipRect.left, this->clipRect.top);
        UIPANEL_Blit(
            this->backgroundSprite,
            workRectCopy.left, workRectCopy.top,
            workRectCopy.right, workRectCopy.bottom,
            _g_primary_surface,
            clipRectCopy.left, clipRectCopy.top,
            clipRectCopy.right, clipRectCopy.bottom,
            1
        );
    }

    /* Begin GDI painting */
    void* hdc = UIPANEL_BeginPaint(this);

    /* Set up GDI state */
    int oldBkMode = SetBkMode(hdc, 1);            /* TRANSPARENT */
    int oldBkColor = SetBkColor(hdc, 0x2525DC);   /* dark blue background */
    int oldTextColor = SetTextColor(hdc, 0xFF5C00); /* orange text */
    void* oldFont = SelectObject(hdc, g_font_normal);

    /* Reset display counter */
    this->displayedCount = 0;                      /* +0x104 */

    /* Calculate text drawing rect (full list rect with DrawTextA) */
    RECT textRect;
    textRect.left   = this->layoutListRect.left;
    textRect.top    = this->layoutListRect.top;
    textRect.right  = this->layoutListRect.right;
    textRect.bottom = this->layoutListRect.bottom;

    /* Measure font height by drawing a dummy string */
    int lineHeight = DrawTextA(hdc, g_font_measure_string, -1, &textRect, 0x420);

    /* Adjust text rect for inner padding */
    textRect.right  = this->layoutListRect.right - 0x0C;
    textRect.left   = this->layoutListRect.left + 0x0C;
    textRect.top    = this->layoutListRect.top + 0x0C;
    textRect.bottom = this->layoutListRect.bottom - 0x0C;

    int drawnCount = 0;

    /* Enumerate the linked list */
    if (list != CACHED_SENTINEL && list != NULL) {
        int lineStep = lineHeight + 4;
        LayoutListNode* node = list;
        int idx = 0;

        do {
            /* Highlight selected entry with background color, others with text color */
            if (idx == this->selectedEntry) {
                SetTextColor(hdc, 0x2525DC);   /* highlight color */
            } else {
                SetTextColor(hdc, 0xFF5C00);   /* normal text color */
            }

            /* Draw entry name (node->name = name string at offset 8) */
            const char* nameStr = node->name;
            DrawTextA(hdc, nameStr, -1, &textRect, 0x20);

            /* Advance to next line */
            textRect.top += lineStep;
            textRect.bottom = textRect.top + lineStep;

            /* Bounds check */
            if (this->layoutListRect.bottom < textRect.bottom) {
                break;
            }

            /* Next node */
            node = node->next;
            idx++;
        } while (node != NULL);

        drawnCount = idx;
    }

    /* Store display count */
    this->displayedCount = drawnCount;             /* +0x104 */

    /* If nothing was drawn, show empty-list fallback */
    if (drawnCount == 0) {
        char buffer[512];  /* 0x200 bytes on stack */
        SetBkMode(hdc, 1);                        /* TRANSPARENT */
        g_resmgr.FormatResourceString(0x7F, buffer, 0x200);
        DrawTextA(hdc, buffer, -1, &textRect, 0x25);
    }

    /* Restore GDI state */
    SetBkMode(hdc, oldBkMode);
    SetBkColor(hdc, oldBkColor);
    SetTextColor(hdc, oldTextColor);
    SelectObject(hdc, oldFont);

    /* Store computed line height */
    this->lineHeight = lineHeight + 4;             /* +0x100 */

    /* End paint */
    UIPANEL_EndPaintEx(this, this->hWnd, (int)hdc, 1, NULL);
}

/* ================================================================== */
/* GameSetupPanel::drawTitle                                           */
/* Address: 0x409770                                                   */
/*                                                                      */
/* Draws the title text at the title area (+0x1FC). Selects title       */
/* font, draws with DrawTextA (DT_CENTER|DT_VCENTER), then applies     */
/* alignment post-processing based on textAlignMode:                    */
/*   0 = right-align                                                    */
/*   1 = left-align                                                     */
/*   2 = bottom-align                                                   */
/*   else = default (centered vertically)                                */
/* ================================================================== */
void GameSetupPanel::drawTitle()
{
    /* Set the title-drawn flag */
    this->titleDrawnFlag = 1;

    /* Get title rectangle */
    RECT titleRect;
    titleRect.left   = this->titleRect.left;
    titleRect.top    = this->titleRect.top;
    titleRect.right  = this->titleRect.right;
    titleRect.bottom = this->titleRect.bottom;

    /* Restore background if sprites exist */
    if (this->spritesCreated != 0) {
        RECT workRectCopy, clipRectCopy;
        CopyRect(&workRectCopy, &titleRect);
        CopyRect(&clipRectCopy, &titleRect);
        OffsetRect(&workRectCopy, this->workRect.left, this->workRect.top);
        OffsetRect(&clipRectCopy, this->clipRect.left, this->clipRect.top);
        UIPANEL_Blit(
            this->backgroundSprite,
            workRectCopy.left, workRectCopy.top,
            workRectCopy.right, workRectCopy.bottom,
            _g_primary_surface,
            clipRectCopy.left, clipRectCopy.top,
            clipRectCopy.right, clipRectCopy.bottom,
            1
        );
    }

    /* Begin GDI painting */
    void* hdc = UIPANEL_BeginPaint(this);
    void* oldFont = SelectObject(hdc, g_title_font);  /* 0x4855FC */

    /* Copy title rect to draw rect */
    RECT* drawRect = &this->titleDrawRect;
    drawRect->left   = titleRect.left;
    drawRect->top    = titleRect.top;
    drawRect->right  = titleRect.right;
    drawRect->bottom = titleRect.bottom;

    /* Draw the title text */
    int textHeight = DrawTextA(hdc, this->titleText, -1, drawRect, 0x420);

    /* Restore font */
    SelectObject(hdc, oldFont);

    /* End paint */
    UIPANEL_EndPaintEx(this, this->hWnd, (int)hdc, 1, NULL);

    /* Adjust bottom of draw rect for alignment calculation */
    drawRect->bottom = textHeight - 4 + drawRect->top;

    /* Center the window (align title area) */
    UI_CenterWindow(&this->titleRect, drawRect);

    /* Apply alignment adjustment based on textAlignMode (+0x1B0) */
    int alignMode = this->textAlignMode;
    if (alignMode == 0) {
        /* Right-align: offset to match right edge */
        OffsetRect(drawRect, this->titleRect.right - drawRect->left, 0);
    } else if (alignMode == 1) {
        /* Left-align: offset to match left edge */
        OffsetRect(drawRect, this->titleRect.left - drawRect->right, 0);
    } else if (alignMode == 2) {
        /* Bottom-align: offset to match bottom edge */
        OffsetRect(drawRect, 0, this->titleRect.bottom - drawRect->top);
    } else {
        /* Default: center vertically */
        OffsetRect(drawRect, 0, this->titleRect.top - drawRect->bottom);
    }
}

/* ================================================================== */
/* GameSetupPanel::drawGrid                                            */
/* Address: 0x409980                                                   */
/*                                                                      */
/* Draws the player/scenario selection grid in the gridRect area.       */
/* Each cell is 0xA5 x 0x7B pixels, with row spacing 0x7C.            */
/* Layout sprites (at +0x23C) are positioned and set to state:         */
/*   state 0 = hidden (beyond maxPlayers)                              */
/*   state 1 = empty slot                                              */
/*   state 2 = occupied slot (+ player name text)                      */
/*                                                                      */
/* Player data is read from _g_netman structure:                       */
/*   _g_netman+0x08 = maxPlayers (m_playerSlotCount)                  */
/*   _g_netman+0x0C = playerCount / numCols (m_playerRows)            */
/*   _g_netman+0x10 = currentPlayer / numRows (m_playerCols)          */
/*   _g_netman + row*0x4C + 0x530 = dpId (m_slots[row].dpId,          */
/*                                  PlayerSlot+0x18)                   */
/*   _g_netman + row*0x4C + 0x52A = layout_name (m_slots[row].        */
/*                                  layout_name, PlayerSlot+0x12)     */
/* ================================================================== */
void GameSetupPanel::drawGrid()
{
    /* Get grid rectangle */
    RECT gridRect;
    gridRect.left   = this->gridRect.left;
    gridRect.top    = this->gridRect.top;
    gridRect.bottom = gridRect.top + 0x7B;

    /* Full grid area for background restore */
    RECT gridArea;
    gridArea.left   = this->gridRect.left;
    gridArea.top    = this->gridRect.top;
    gridArea.right  = this->gridRect.right;
    gridArea.bottom = this->gridRect.bottom;

    /* Restore background if sprites exist */
    if (this->spritesCreated != 0) {
        RECT workRectCopy, clipRectCopy;
        CopyRect(&workRectCopy, &gridArea);
        CopyRect(&clipRectCopy, &gridArea);
        OffsetRect(&workRectCopy, this->workRect.left, this->workRect.top);
        OffsetRect(&clipRectCopy, this->clipRect.left, this->clipRect.top);
        UIPANEL_Blit(
            this->backgroundSprite,
            workRectCopy.left, workRectCopy.top,
            workRectCopy.right, workRectCopy.bottom,
            _g_primary_surface,
            clipRectCopy.left, clipRectCopy.top,
            clipRectCopy.right, clipRectCopy.bottom,
            1
        );
    }

    /* Get network manager data */
    /* NOTE: Binary transposes grid axes — numRows comes from m_playerCols,
             numCols from m_playerRows. The grid draws numRows rows of
             numCols columns per row. */
    int numRows = _g_netman->m_playerCols;    /* column count = rows to draw */
    int numCols = _g_netman->m_playerRows;     /* row count = columns to draw */
    int maxPlayers = _g_netman->m_playerSlotCount;

    /* NOTE: insertedCount is incremented but never read after the loop.
   The binary also computes this value without using it — dead code preserved
   for behavioral fidelity. */
    int insertedCount = 0;  /* total players inserted across all rows (unused) */

    /* Calculate first cell rect OUTSIDE the row loop so that the
       per-row OffsetRect(0, 0x7C) at the end of each iteration
       actually advances the Y position for the next row. */
    RECT cellRect = gridRect;
    cellRect.right = cellRect.left + 0xA4;

    /* Iterate rows */
    for (int row = 0; row < numRows; row++) {
        /* Track layout sprite pointer (3 per row for the 9-sprite grid) */
        ButtonSprite** spritePtr = &this->layoutSprite[row * 3];

        /* Iterate columns */
        for (int col = 0; col < numCols; col++) {
            ButtonSprite* sprite = spritePtr[col];

            /* Position sprite at current cell */
            sprite->x       = cellRect.left;
            sprite->y       = cellRect.top;
            sprite->sourceX = cellRect.right;
            sprite->sourceY = cellRect.bottom;

            if (row < maxPlayers) {
                /* Check if this slot has a player */
                int playerId = _g_netman->m_slots[row].dpId;

                if (playerId == 0) {
                    /* Empty slot: show empty state */
                    sprite->setState(1, NULL);
                } else {
                    /* Occupied slot: show filled state + player name */
                    sprite->setState(2, NULL);

                    /* Begin paint to draw player name */
                    void* hdc = UIPANEL_BeginPaint(this);
                    int oldBkMode = SetBkMode(hdc, 1);             /* TRANSPARENT */
                    int oldTextColor = SetTextColor(hdc, 0);      /* black */
                    void* oldFont = SelectObject(hdc, g_font_small);

                    /* Player name from the layout_name field (Netman::PlayerSlot +0x12) */
                    const char* playerName = _g_netman->m_slots[row].layout_name;
                    DrawTextA(hdc, playerName, -1, &cellRect, 0x25);

                    /* Restore GDI state */
                    SelectObject(hdc, oldFont);
                    SetBkMode(hdc, oldBkMode);
                    SetTextColor(hdc, oldTextColor);
                    UIPANEL_EndPaintEx(this, this->hWnd, (int)hdc, 1, NULL);
                }
                insertedCount++;
            } else {
                /* Beyond max players: hidden state */
                sprite->setState(0, NULL);
            }

            /* Advance to next column */
            OffsetRect(&cellRect, 0xA5, 0);
        }

        /* Advance to next row: reset X, move Y down by 0x7C */
        cellRect.left = gridRect.left;
        cellRect.right = cellRect.left + 0xA4;
        OffsetRect(&cellRect, 0, 0x7C);
    }
}

/* ================================================================== */
/* GameSetupPanel::cleanup                                             */
/* Address: 0x409DB0                                                   */
/*                                                                      */
/* Lightweight cleanup: resets state flags, hides window, destroys      */
/* sprite pixel data, releases field_234 sub-object, kills timers.      */
/* Does NOT free ButtonSprite objects or title/layout lists.            */
/* ================================================================== */
void GameSetupPanel::cleanup()
{
    /* Reset state flags */
    this->field_E8 = 0;              /* +0xE8 */
    this->titleDrawnFlag = 0;        /* +0x114 */
    this->field_10C = 0;             /* +0x10C */

    /* Hide the window */
    this->hide();

    /* Clear current list pointer */
    this->currentList = NULL;        /* +0x108 */

    /* Destroy sprite pixel data if sprites were created */
    if (this->spritesCreated != 0) {
        /* Destroy 5 main sprite pixel data */
        this->titleSprite->destroy();           /* +0x220 */
        this->field_224->destroy();             /* +0x224 */
        this->field_228->destroy();             /* +0x228 */
        this->field_22C->destroy();             /* +0x22C */
        this->field_230->destroy();             /* +0x230 */

        /* Release sub-object at +0x234 via Release() (vtable[2]) */
        if (this->field_234 != NULL) {
            this->field_234->Release();
        }
        this->field_234 = NULL;                     /* +0x234 */

        /* Destroy all 9 layout sprite pixel data */
        for (int i = 0; i < 9; i++) {
            this->layoutSprite[i]->destroy();   /* +0x23C + i*4 */
        }

        this->spritesCreated = 0;                   /* +0x21C */
    }

    /* Kill both timers */
    KillTimer(this->hWnd, this->timerId1);          /* +0x118 */
    KillTimer(this->hWnd, this->timerId2);          /* +0x11C */
}

/* ================================================================== */
/* GameSetupPanel::loadLayouts                                         */
/* Address: 0x409E70                                                   */
/*                                                                      */
/* Loads layout index from "install_dir\\Layouts\\index.lay" and       */
/* parses it into titleList linked list. Falls back from AssetMgr to   */
/* direct file open. Optionally connects to network game after load.   */
/* ================================================================== */
void GameSetupPanel::loadLayouts(bool connectToNetwork)
{
    /* Step 1: Free any existing titleList */
    {
        LayoutListNode* node = this->titleList;
        while (node != NULL) {
            LayoutListNode* next = node->next;
            if (node->name != NULL) {
                GLOBAL_free(node->name);
            }
            GLOBAL_free(node);
            this->titleList = next;
            node = next;
        }
    }

    /* Step 2: Build path to index.lay */
    char filePath[1285];  /* 0x505 bytes on stack */
    wsprintfA(filePath, "%s\\Layouts\\index.lay", g_install_path);

    char* streamObj = NULL;   /* local_18 / stream object */
    uint8_t* pData = NULL;     /* local_2c / loaded asset data */
    int dataSize = 0;         /* local_28 */
    int* streamResult = NULL; /* local_1c / stream result pointer */

    /* Step 3: Try AssetMgr first */
    if (g_asset_mgr != NULL) {
        pData = AssetMgr_LoadFile(g_asset_mgr, (uint8_t*)filePath, &dataSize);
        if (pData != NULL) {
            streamObj = (char*)operator_new(0x5C);
            if (streamObj != NULL) {
                streamResult = (int*)WNDPROC_StreamFromMemory(streamObj, (const char*)pData, dataSize, 1); /* pData is uint8_t*, cast to const char* for StreamFromMemory */
            }
        }
    }

    /* Step 4: Fall back to direct file open if AssetMgr failed.
       NOTE: If the AssetMgr path allocated streamObj but WNDPROC_StreamFromMemory
       returned NULL, streamObj is overwritten here without freeing the first
       allocation. The binary has this leak — preserved for behavioral fidelity. */
    if (streamResult == NULL) {
        streamObj = (char*)operator_new(0x5C);
        if (streamObj != NULL) {
            streamResult = (int*)WIN32_StreamOpen(streamObj, filePath, 0xA0, &g_stream_open_mode, 1);
        }
    }

    /* Step 5: Validate stream using typed WndProcStream wrapper */
    if (streamResult == NULL) {
        const char* errMsg = "Failed to open stream to data";
        CRT_exit(&errMsg, (const char**)0x47A5E8);
    }

    /* Check stream validity flags via WndProcStream::IsValid() */
    {
        WndProcStream* stream = (WndProcStream*)streamResult;
        if (!stream->IsValid()) {
            const char* errMsg = "Invalid stream";
            CRT_exit(&errMsg, (const char**)0x47A5E8);
        }
    }

    /* Step 6: Allocate text buffer (0x2000 bytes) and zero it */
    uint32_t* textBuffer = (uint32_t*)operator_new(0x2000);
    if (textBuffer == NULL) {
        const char* errMsg = "couldn't allocate buffer";
        CRT_exit(&errMsg, (const char**)0x47A5E8);
    }

    /* Zero the buffer (0x800 dwords = 0x2000 bytes) */
    for (int i = 0; i < 0x800; i++) {
        textBuffer[i] = 0;
    }

    /* Step 7: Read file content */
    WIN32_StreamRead(streamResult, textBuffer, 0x2000);

    /* Step 8: Parse lines */
    WndProcStream* stream = (WndProcStream*)streamResult;
    int streamLen = stream->streamLength;  /* WNDPROC stream length */
    uint32_t* buf = textBuffer;
    int pos = 0;
    int lineStart = 0;

    while (pos < streamLen) {
        char ch = *((char*)buf + pos);

        /* Skip newlines */
        if (ch == '\n') {
            lineStart = pos + 1;
            pos++;
            continue;
        }

        /* Check for end of line (CR, NUL) with minimum 5-char content */
        if ((ch == '\r' || ch == '\0') && (pos - lineStart > 4)) {
            /* Null-terminate this line */
            ((char*)buf)[pos] = '\0';

            /* Allocate a new list node (0xC bytes) */
            LayoutListNode* newNode = (LayoutListNode*)operator_new(0x0C);
            newNode->next = this->titleList;                    /* node[0] = next pointer */
            newNode->_pad_04 = 0;                                /* +0x04 padding, zero-initialized */
            this->titleList = newNode;

            /* Allocate name buffer (0x100 bytes) and copy line content */
            char* nameBuf = (char*)operator_new(0x100);
            newNode->name = nameBuf;                             /* node[2] = name string */

            /* Copy the line text with explicit bounds check (max 255 chars + NUL).
               Binary has no bounds check; assumes line < 0x100 chars. */
            const char* src = (const char*)buf + lineStart;
            char* dst = nameBuf;
            int copyCount = 0;
            while (*src != '\0' && copyCount < 0xFF) {
                *dst++ = *src++;
                copyCount++;
            }
            *dst = '\0';

            lineStart = pos + 1;
        }

        pos++;
    }

    /* Step 9: Clean up resources */
    if (pData != NULL) {
        CRT_free(pData);
    }
    if (streamResult != NULL) {
        /* Destroy the WNDPROC stream via its virtual destructor.
           The binary calls vtable[0](this, 1) — scalar deleting destructor. */
        WndProcStream* typedStream = (WndProcStream*)streamResult;
        delete typedStream;
    }
    if (textBuffer != NULL) {
        GLOBAL_free(textBuffer);
    }

    /* Step 10: Optionally connect to network game */
    if (connectToNetwork) {
        this->selectedEntry = 0;
        this->ConnectToNetworkGame(0);
    }
}

#ifndef _WIN32
#include "EditWindow.h"
#include "../../sdl3_shims/host_test_events.h"
#include "../../sdl3_shims/sdl3_game_audio.h"

/* ================================================================== */
/* GameSetupPanel::hostRenderFrame — SDL3 host composition              */
/* Assembly basis: GameSetupPanel::render (0x409280), drawGrid          */
/* (0x409980), and Sprite_SetState (0x454C30).                          */
/* ================================================================== */

#include <SDL3/SDL.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SYNTHESIS_H
#include "../../sdl3_shims/sdl3_ddraw.h"

// Deliberately avoid including resource_manager_sdl3.h here: this translated
// file retains an incompatible legacy ResourceManager_Init declaration.
namespace loco::assets {
class SpriteResource;
class SpriteBitmap;
SpriteResource* host_get_sprite_by_id(uint32_t resource_id);
SpriteBitmap* sprite_bitmap(SpriteResource* resource);
SDL_Surface* bitmap_surface(const SpriteBitmap* bitmap);
void release_sprite(SpriteResource* resource);
}

// sdl3_window.h cannot be included here because this translation unit retains
// the original unconditional Win32 declarations above.
// SDL3_GetRenderer is exported with C linkage by sdl3_window.cpp. Without
// this linkage, the permissive host linker leaves a mangled call unresolved,
// which lands in the __stack_chk_fail PLT slot after the player grid renders.
extern "C" SDL_Renderer* SDL3_GetRenderer(void);

namespace {
constexpr uint32_t kLobbyBackdropResource = 0x439;  // startup\apback.bmp
constexpr uint32_t kExitResource = 0x42C;             // startup\apExit.bmp
constexpr uint32_t kSearchResource = 0x429;           // startup\apsearch.bmp
constexpr uint32_t kOptionsResource = 0x42B;          // startup\apoption.bmp
constexpr uint32_t kFirstPlayerResource = 0x43A;      // startup\aplayer0.bmp
constexpr int kMaximumGridColumns = 3;
constexpr int kMaximumGridRows = 3;
constexpr int kGridCellWidth = 0xA4;
constexpr int kGridCellHeight = 0x7B;
constexpr int kGridStrideX = 0xA5;
constexpr int kGridStrideY = 0x7C;

// CGWND_GameSetup_Show at 0x408F70 centers an 800x600 working area. The
// fixed SDL primary is 1280x1024, so retain that original coordinate space.
constexpr int kWorkingAreaWidth = 800;
constexpr int kWorkingAreaHeight = 600;
constexpr int kWorkingLeft = (SDL3_PRIMARY_CANVAS_WIDTH - kWorkingAreaWidth) / 2;
constexpr int kWorkingTop = (SDL3_PRIMARY_CANVAS_HEIGHT - kWorkingAreaHeight) / 2;
constexpr int kGridLeft = kWorkingLeft + 0x1B;
// CGWND_GameSetup_Show at 0x408F70 is not yet a Ghidra function, but its
// instruction stream is recovered in the raw binary.  0x40902E..0x4090F5
// derives these rectangles from the centered 800x600 working area:
// grid = {work + 0x1B, work + 0x1B, +0x1EE, +0x173};
// list = {grid.right + 0x11, grid.top, work.right - 0x18,
//         grid.bottom - 0x1E}.  drawLayoutList at 0x409635..0x409642 then
// applies its 12px inner padding before every DrawTextA call.
constexpr int kGridTop = kWorkingTop + 0x1B;
constexpr int kLayoutListLeft = kGridLeft + 0x1EE + 0x11;
constexpr int kLayoutListTop = kGridTop;
constexpr int kLayoutListRight = kWorkingLeft + kWorkingAreaWidth - 0x18;
constexpr int kLayoutListBottom = kGridTop + 0x173 - 0x1E;
constexpr int kLayoutTextLeft = kLayoutListLeft + 0x0C;
constexpr int kLayoutTextTop = kLayoutListTop + 0x0C;
constexpr int kLayoutTextRight = kLayoutListRight - 0x0C;
constexpr int kLayoutTextBottom = kLayoutListBottom - 0x0C;

// 0x4090FB..0x4091EB derives these button rectangles from gridRect and the
// decoded frame dimensions.  They are the state-0 Exit and Options controls
// issued by GameSetupPanel::render at 0x4092C4 and 0x4092E9.
constexpr int kExitLeft = kWorkingLeft + kWorkingAreaWidth - 208;
constexpr int kExitTop = kGridTop + 0x1C0;
constexpr int kOptionsLeft = kGridLeft + 0x20C;
constexpr int kOptionsTop = kGridTop + 0x162;

struct HostGridLayout {
    const char* label;
    int32_t display_columns;
    int32_t display_rows;
};

constexpr HostGridLayout kHostGridLayouts[] = {
    {"3x3", 3, 3}, {"2x2", 2, 2}, {"2x1", 2, 1},
    {"3x1", 3, 1}, {"3x2", 3, 2},
};
constexpr int kHostGridLayoutCount = static_cast<int>(sizeof(kHostGridLayouts) / sizeof(kHostGridLayouts[0]));
// drawLayoutList (0x4094B0) sets these COLORREF values before each
// DrawTextA call. COLORREF is 0x00BBGGRR, hence this SDL RGB order.
constexpr SDL_Color kOriginalListTextColor = {0x00, 0x5c, 0xff, 0xff};
constexpr SDL_Color kOriginalSelectedListTextColor = {0xdc, 0x25, 0x25, 0xff};
constexpr int kOriginalNormalFontHeight = 14;
constexpr int kOriginalListLineStep = kOriginalNormalFontHeight + 4;

// ResourceManager_Init (0x44611A..0x44613A) assigns g_font_normal from
// CreateFontA(14, 0, 0, 0, 700, ..., "Arial"). FreeType rasterizes the
// Fontconfig-selected Arial-compatible face; emboldening retains weight 700
// when the host only ships a regular fallback.
struct HostNormalFont {
    FT_Library library = nullptr;
    FT_Face face = nullptr;

    HostNormalFont()
    {
        if (FT_Init_FreeType(&library) != 0 ||
            FT_New_Face(library, LOCO_HOST_UI_FONT_FILE, 0, &face) != 0 ||
            FT_Set_Pixel_Sizes(face, 0, kOriginalNormalFontHeight) != 0) {
            if (face != nullptr) FT_Done_Face(face);
            if (library != nullptr) FT_Done_FreeType(library);
            face = nullptr;
            library = nullptr;
        }
    }

    ~HostNormalFont()
    {
        if (face != nullptr) FT_Done_Face(face);
        if (library != nullptr) FT_Done_FreeType(library);
    }
};

bool host_draw_text(SDL_Renderer* renderer, int x, int y, const char* text,
                    const SDL_Color& color)
{
    if (renderer == nullptr || text == nullptr) return false;
    static const HostNormalFont font;
    if (font.face == nullptr) return false;

    const int baseline = y + static_cast<int>(font.face->size->metrics.ascender >> 6);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    bool rendered = true;
    for (const unsigned char* character = reinterpret_cast<const unsigned char*>(text);
         *character != '\0'; ++character) {
        if (FT_Load_Char(font.face, *character, FT_LOAD_DEFAULT) != 0) {
            rendered = false;
            continue;
        }
        FT_GlyphSlot_Embolden(font.face->glyph);
        if (FT_Render_Glyph(font.face->glyph, FT_RENDER_MODE_NORMAL) != 0) {
            rendered = false;
            continue;
        }

        const FT_GlyphSlot glyph = font.face->glyph;
        const FT_Bitmap& bitmap = glyph->bitmap;
        for (unsigned int row = 0; row < bitmap.rows; ++row) {
            const unsigned char* coverage = bitmap.buffer + row * bitmap.pitch;
            for (unsigned int column = 0; column < bitmap.width; ++column) {
                if (coverage[column] == 0) continue;
                SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, coverage[column]);
                const SDL_FRect pixel = {
                    static_cast<float>(x + glyph->bitmap_left + static_cast<int>(column)),
                    static_cast<float>(baseline - glyph->bitmap_top + static_cast<int>(row)),
                    1.0f, 1.0f};
                rendered = SDL_RenderFillRect(renderer, &pixel) && rendered;
            }
        }
        x += static_cast<int>(glyph->advance.x >> 6);
    }
    return rendered;
}

void host_apply_layout(GameSetupPanel& panel, int index)
{
    if (index < 0 || index >= kHostGridLayoutCount) return;

    const HostGridLayout& layout = kHostGridLayouts[index];
    panel.hostLayoutIndex = static_cast<uint8_t>(index);
    // The host bootstrap may not construct Netman while DirectPlay is absent.
    // When it does exist, preserve the original field update; otherwise the
    // selected profile is the provider value consumed below.
    if (_g_netman != nullptr) {
        _g_netman->m_playerRows = layout.display_columns;
        _g_netman->m_playerCols = layout.display_rows;
        _g_netman->m_playerSlotCount = layout.display_columns * layout.display_rows;
    }
    loco::host_test::emit_layout_selected(layout.display_columns, layout.display_rows,
                                          layout.display_columns * layout.display_rows);
}

int host_layout_at(float canvas_x, float canvas_y)
{
    // drawLayoutList (0x409635..0x409642) applies 12px inner padding then
    // advances each row by the measured font height + 4. PtInRect-style
    // right/bottom exclusion preserves the original list bounds.
    const float left = static_cast<float>(kLayoutTextLeft);
    const float top = static_cast<float>(kLayoutTextTop);
    const float right = static_cast<float>(kLayoutTextRight);
    const float bottom = static_cast<float>(kLayoutTextBottom);
    if (canvas_x < left || canvas_x >= right || canvas_y < top || canvas_y >= bottom) return -1;
    const int index = static_cast<int>((canvas_y - top) / kOriginalListLineStep);
    return index >= 0 && index < kHostGridLayoutCount ? index : -1;
}

bool host_blit_resource(uint32_t resource_id, int x, int y) {
    auto* resource = loco::assets::host_get_sprite_by_id(resource_id);
    auto* bitmap = loco::assets::sprite_bitmap(resource);
    SDL_Surface* surface = loco::assets::bitmap_surface(bitmap);
    const bool rendered = surface && SDL3_BlitSurfaceToPrimary(surface, x, y);
    loco::assets::release_sprite(resource);
    return rendered;
}

bool host_blit_frame(uint32_t resource_id, int frame, int frame_width, int frame_height,
                     int x, int y) {
    auto* resource = loco::assets::host_get_sprite_by_id(resource_id);
    auto* bitmap = loco::assets::sprite_bitmap(resource);
    SDL_Surface* surface = loco::assets::bitmap_surface(bitmap);
    // Sprite_SetState (0x454C30) obtains frame N by offsetting source.left by
    // N * resource-frame-width; retain that source-rectangle convention.
    const SDL_Rect source = {frame * frame_width, 0, frame_width, frame_height};
    const bool rendered = surface && SDL3_BlitSurfaceRectToPrimary(surface, source, x, y);
    loco::assets::release_sprite(resource);
    return rendered;
}
}  // namespace

enum class HostLobbyControl : uint8_t {
    None,
    Exit,
    Search,
    Options,
};

constexpr uint64_t kLobbyPressDurationMs = 150;  // Sleep(0x96) at 0x40A591 etc.

HostLobbyControl host_pressed_control(const GameSetupPanel& panel)
{
    return static_cast<HostLobbyControl>(panel.hostPressedControl);
}

void host_complete_lobby_control(GameSetupPanel& panel, HostLobbyControl control)
{
    switch (control) {
    case HostLobbyControl::Exit:
        // 0x40A65C..0x40A709: reset session state, then state 7.
        if (g_editwindow_ptr != nullptr) {
            g_editwindow_ptr->setState(7);
            panel.titleDrawnFlag = 0;
        }
        return;

    case HostLobbyControl::Search:
        // 0x40A70C..0x40A82A starts a client search. The SDL DirectPlay
        // boundary is deliberately empty, so retain the completed empty scan.
        if (g_editwindow_ptr != nullptr && g_editwindow_ptr->dialogState == 5) {
            panel.hostSearchCompleted = true;
            loco::host_test::emit_search_completed(0);
        }
        return;

    case HostLobbyControl::Options:
        // 0x40A873..0x40A908 returns through state 2. NameEntryPanel has no
        // SDL compositor, so state 7 remains the explicit host presentation
        // fallback after preserving the recovered state-2 transition.
        if (g_editwindow_ptr != nullptr) {
            g_editwindow_ptr->setState(2);
            g_editwindow_ptr->setState(7);
        }
        return;

    case HostLobbyControl::None:
        return;
    }
}

bool host_lobby_contains(int left, int top, int width, int height,
                         float x, float y)
{
    // PtInRect in GAMESTATE_HandleClick (0x40A4E0) includes left/top and
    // excludes right/bottom edges.
    return x >= left && x < left + width && y >= top && y < top + height;
}

HostLobbyControl host_lobby_control_at(float canvas_x, float canvas_y)
{
    // The first three PtInRect checks in 0x40A4E0 use ButtonSprite rects
    // for Go (0x42A), Exit (0x42C), and Search (0x429).  Go is only drawn
    // after a DirectPlay session is available; the SDL DirectPlay boundary
    // currently reports no sessions, so only the three controls composed in
    // hostRenderFrame() are candidates here.
    if (host_lobby_contains(kExitLeft, kExitTop, 144, 112, canvas_x, canvas_y)) {
        return HostLobbyControl::Exit;
    }
    if (host_lobby_contains(kOptionsLeft + 79, kOptionsTop, 72, 72,
                            canvas_x, canvas_y)) {
        return HostLobbyControl::Search;
    }
    if (host_lobby_contains(kOptionsLeft, kOptionsTop, 72, 72,
                            canvas_x, canvas_y)) {
        return HostLobbyControl::Options;
    }
    return HostLobbyControl::None;
}

void GameSetupPanel::hostRenderFrame()
{
    // 0x409280 starts with a full background blit.  Resource 0x439 is the
    // exact background asset (startup\apback.bmp) used by this panel family.
    if (!host_blit_resource(kLobbyBackdropResource, 0, 0)) {
        SDL3_ClearPrimarySurface(0x003050);
    }

    // render (0x409280) resets Exit, Search, and Options to state 0 after
    // the backdrop.  Each source BMP is two horizontal frames, exactly as
    // Sprite_SetState (0x454C30) expects.  Go (0x42A) is not drawn here: the
    // original only exposes it after network/session selection updates it.
    const HostLobbyControl pressed_control = host_pressed_control(*this);
    host_blit_frame(kExitResource, pressed_control == HostLobbyControl::Exit ? 1 : 0,
                    144, 112, kExitLeft, kExitTop);
    host_blit_frame(kSearchResource, pressed_control == HostLobbyControl::Search ? 1 : 0,
                    72, 72, kOptionsLeft + 79, kOptionsTop);
    host_blit_frame(kOptionsResource, pressed_control == HostLobbyControl::Options ? 1 : 0,
                    72, 72, kOptionsLeft, kOptionsTop);

    // drawGrid (0x409980) advances x by 0xA5 and y by 0x7C. Keep its
    // dimensions authoritative: SyncGameState normally fills these Netman
    // fields from the host packet; host_apply_layout fills them locally while
    // SDL DirectPlay has no provider.
    const HostGridLayout& host_layout = kHostGridLayouts[
        this->hostLayoutIndex < kHostGridLayoutCount ? this->hostLayoutIndex : 0];
    const int grid_columns = _g_netman != nullptr
        ? _g_netman->m_playerRows : host_layout.display_columns;
    const int grid_rows = _g_netman != nullptr
        ? _g_netman->m_playerCols : host_layout.display_rows;
    const int visible_columns = SDL_clamp(grid_columns, 1, kMaximumGridColumns);
    const int visible_rows = SDL_clamp(grid_rows, 1, kMaximumGridRows);
    for (int row = 0; row < visible_rows; ++row) {
        for (int column = 0; column < visible_columns; ++column) {
            // The original sprite table is three entries per outer/Y row.
            const int slot = row * kMaximumGridColumns + column;
            host_blit_frame(kFirstPlayerResource + slot, 1, kGridCellWidth, kGridCellHeight,
                            kGridLeft + column * kGridStrideX,
                            kGridTop + row * kGridStrideY);
        }
    }

    // updateTitle (0x409360) selects resource 0x71 while network mode is
    // active; drawLayoutList (0x4094B0) falls back to resource 0x7F when
    // the DirectPlay session list is empty. The guarded primary glyph path
    // below follows drawLayoutList's recovered list colours and row cadence.
    if (SDL_Renderer* renderer = SDL3_GetRenderer()) {
        SDL_SetRenderTarget(renderer, SDL3_GetPrimarySurface()->texture);
        const bool network_lobby = g_editwindow_ptr != nullptr &&
                                   g_editwindow_ptr->dialogState == 5;
        if (network_lobby) {
            for (int index = 0; index < kHostGridLayoutCount; ++index) {
                const HostGridLayout& layout = kHostGridLayouts[index];
                const SDL_Color& color = index == this->hostLayoutIndex
                    ? kOriginalSelectedListTextColor : kOriginalListTextColor;
                host_draw_text(renderer, kLayoutTextLeft,
                               kLayoutTextTop + index * kOriginalListLineStep,
                               layout.label, color);
            }
            if (this->hostSearchCompleted) {
                host_draw_text(renderer, kLayoutTextLeft,
                               kLayoutTextTop + kHostGridLayoutCount * kOriginalListLineStep,
                               "NO NETWORK GAMES FOUND", kOriginalListTextColor);
            }
        } else {
            host_draw_text(renderer, kLayoutTextLeft, kLayoutTextTop, "NO LAYOUTS AVAILABLE",
                           kOriginalListTextColor);
        }
        SDL_SetRenderTarget(renderer, nullptr);
    }

    // The original handler plays 0x5015, draws source frame 1, then sleeps
    // 0x96 ms before its state transition. Render frame 1 until that exact
    // interval has elapsed; this preserves feedback without blocking SDL.
    if (pressed_control != HostLobbyControl::None &&
        SDL_GetTicks() >= this->hostPressedUntilMs) {
        this->hostPressedControl = static_cast<uint8_t>(HostLobbyControl::None);
        this->hostPressedUntilMs = 0;
        host_complete_lobby_control(*this, pressed_control);
    }
}

/**
 * GameSetupPanel host SDL pointer adapter.
 *
 * Assembly basis: GAMESTATE_HandleClick (0x40A4E0).  It checks the Go,
 * Exit, Search, and Options ButtonSprite rectangles in that order.  Go and
 * grid selection are unreachable on this host until DirectPlay reports a
 * session (the SDL DirectPlay boundary deliberately reports none); the three
 * controls rendered above retain their original click ordering/actions.
 */
void GameSetupPanel::hostHandlePointer(float display_x, float display_y, bool pressed)
{
    float canvas_x = 0.0f;
    float canvas_y = 0.0f;
    if (!pressed || !SDL3_DisplayToPrimaryCanvas(display_x, display_y,
                                                  &canvas_x, &canvas_y)) {
        return;
    }

    // The original receives these dimensions through NETMAN_SyncGameState
    // (0x43FC50). This is the explicit host-provider substitute: select a
    // layout-list row, update the same Netman fields, and let drawGrid's
    // original dimensions control the next frame.
    if (g_editwindow_ptr != nullptr && g_editwindow_ptr->dialogState == 5) {
        const int layout_index = host_layout_at(canvas_x, canvas_y);
        if (layout_index >= 0) {
            if (layout_index != this->hostLayoutIndex) {
                SDL3_GameAudioPlayResource(0x5015);
                host_apply_layout(*this, layout_index);
            }
            return;
        }
    }

    const HostLobbyControl control = host_lobby_control_at(canvas_x, canvas_y);
    if (control == HostLobbyControl::None ||
        this->hostPressedControl != static_cast<uint8_t>(HostLobbyControl::None)) {
        return;
    }

    // Every actionable control and valid grid/list selection in
    // GAMESTATE_HandleClick calls PlaySound(0x5015), sets Sprite_SetState(1),
    // and blocks for Sleep(0x96). The host uses the archive-backed sound and
    // defers the recovered action until hostRenderFrame has shown frame 1.
    SDL3_GameAudioPlayResource(0x5015);
    this->hostPressedControl = static_cast<uint8_t>(control);
    this->hostPressedUntilMs = SDL_GetTicks() + kLobbyPressDurationMs;
}
#endif  // !_WIN32
