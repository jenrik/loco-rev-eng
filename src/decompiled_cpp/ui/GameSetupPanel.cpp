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
/* AssetMgr forward-declared (not including AssetMgr.h to avoid OutputDebugStringA
   conflict with Netman.h); AssetMgr_LoadFile declared as extern inline below */
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
    extern BOOL   __stdcall GetDesktopWindow(void);             /* 0x477364 */
    extern BOOL   __stdcall GetClientRect(HWND, void* lpRect);  /* 0x477368 */
    extern HICON  __stdcall LoadIconA(HINSTANCE, LPCSTR);       /* 0x47736C */
    extern BOOL   __stdcall DeleteObject(HGDIOBJ);              /* 0x477048 */
    extern void*  __stdcall GetStockObject(int fnObject);       /* 0x477050 */
    extern int    __stdcall SetBkMode(void* hdc, int mode);     /* 0x477064 */
    extern int    __stdcall SetBkColor(void* hdc, int color);   /* 0x47706C */
    extern int    __stdcall SetTextColor(void* hdc, int color); /* 0x477060 */
    extern void*  __stdcall SelectObject(void* hdc, void* obj); /* 0x47703C */
    extern int    __stdcall DrawTextA(void* hdc, const char* str,
                                      int len, void* rect, int format); /* 0x477348 */
    /* CopyRect, OffsetRect already declared in Netman.h with typed RECT params */
    extern BOOL   __stdcall KillTimer(HWND hWnd, UINT_PTR id);  /* 0x4772F0 */
    extern int    __stdcall wsprintfA(char* buf, const char* fmt, ...); /* 0x477370 */
}

    /* CRT helpers */
    extern int    __cdecl CRT_sprintf_buf(char* buf, const char* fmt, ...); /* 0x466D60 */
    extern void   __cdecl CRT_exit(const char** msg, const char** fileLine); /* 0x466CE0 */
    extern void   __cdecl CRT_free(void* ptr);                              /* 0x466C70 */

    /* Resource manager — now use g_resmgr.GetById() / g_resmgr.FormatResourceString() */
    /* (declared in ResourceManager.h, included above) */
    extern void   ReleaseSoundResource(int handle);          /* 0x448EE0 — non-member helper */

    /* Stream / WNDPROC helpers — TODO: decompile WndProcStream class (0x464490, 0x463810, etc.) */
    extern void*  WIN32_StreamOpen(void* stream, const char* path, int mode, void* extra, int flag);
    extern void   WIN32_StreamRead(void* stream, void* buf, int sz);        /* 0x463810 */
    extern void   WIN32_StreamDestroy(void* stream);                        /* 0x463A80 */
    extern int*   WNDPROC_StreamFromMemory(void* stream, const char* data,
                                            int size, int mode);             /* 0x464490 */
    extern void   WNDPROC_StreamCleanup(void* stream);                      /* 0x464620 */

    /* Asset manager (forward-declared; AssetMgr.h omitted to avoid OutputDebugStringA
       conflict with Netman.h) */
    extern uint8_t* AssetMgr_LoadFile(struct AssetMgr* self, uint8_t* filename,
                                       int32_t* out_size);                   /* 0x45CD00 */

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

    /* UI_CenterWindow */
    extern void   UI_CenterWindow(void* param1, void* param2); /* 0x425A50 */


/* ================================================================== */
/* Global variables                                                    */
/* ================================================================== */

extern void* _g_primary_surface;      /* 0x4FD3C4 — primary DirectDraw surface */
extern Netman* g_netman;              /* 0x4FD3AC — network manager singleton */
                                       /* NOTE: Netman.h declares _g_netman at same address;
                                                canonicalize to g_netman across codebase */
/* g_resmgr now declared in ResourceManager.h (included above) */
/* g_asset_mgr declared as void* in Netman.h; cast to AssetMgr* when calling AssetMgr_LoadFile */
extern void* g_font_normal;           /* 0x4855F0 — normal UI font handle */
extern void* g_font_small;            /* 0x4855F4 — small UI font handle */
extern void* g_title_font;            /* 0x4855FC — title display font handle */
extern char  g_install_path[];        /* 0x4A99C8 — install directory path */
extern int   g_stream_open_mode;      /* 0x479190 — stream open mode flags for WIN32_StreamOpen */

/* Named constant for font measurement string at 0x47E2C8 */
/* Binary uses this dummy string via DrawTextA to measure font height */
static const char* const g_font_measure_string = (const char*)0x47E2C8;

/* (void*)-1 sentinel: Binary uses (void*)-1 to mean "use cached currentList".
   Equivalent on x86 (32-bit) but non-portable on other architectures. */
#define LIST_SENTINEL_CACHED  ((void*)-1)

/* ================================================================== */
/* Minimal WndProcStream class — TODO: full decompilation with Ghidra */
/* ================================================================== */
/* LayoutListNode — linked list node for scenario/layout entries      */
/* 0x0C bytes per node: [0]=next, [4]=padding, [8]=name string       */
/* ================================================================== */
struct LayoutListNode {
    LayoutListNode* next;    // +0x00  next node pointer
    int32_t         _pad_04; // +0x04  padding/unused
    char*           name;    // +0x08  heap-allocated name string
};

/* ================================================================== */
/**
 * WndProcStreamMetadata — header/metadata block for WNDPROC stream objects.
 * The first field of a WndProcStream points to an instance of this struct.
 * Index 4 (offset 0x10) stores a byte offset used to locate the stream's
 * validity flags field within the stream object.
 *
 * TODO: Verify layout in Ghidra and move to a proper header.
 */
struct WndProcStreamMetadata {
    int32_t field_00;        // +0x00
    int32_t field_04;        // +0x04
    int32_t field_08;        // +0x08
    int32_t field_0C;        // +0x0C
    int32_t flagsOffset;     // +0x10  byte offset to flags within WndProcStream
};

/**
 * WndProcStream — vtable-based stream object used for file I/O.
 * Vtable: [0] scalar deleting destructor, [4] returns flags offset info.
 *
 * TODO: Full decompilation — this is a minimal wrapper for GameSetupPanel.
 *       Move to a proper header once the complete class is decompiled in Ghidra.
 */
class WndProcStream {
public:
    virtual ~WndProcStream() {}      // [0] scalar deleting destructor
    // slots [1]-[3]: unknown
    // slot [4]: unknown (returns metadata about flags)

    WndProcStreamMetadata* metadata; // +0x00  pointer to metadata block
    int32_t  field_04;               // +0x04
    int32_t  streamLength;           // +0x08  total stream length in bytes

    /** Check if the stream is valid (flags & 5 == 0). */
    bool IsValid() const {
        int32_t offset = this->metadata->flagsOffset;
        int32_t* flagsPtr = (int32_t*)((uint8_t*)this + offset + 8);
        int32_t flags = *flagsPtr;
        return (flags & 5) == 0;
    }
};

/* ================================================================== */
/* GameSetupPanel Constructor                                          */
/* Address: 0x408AA0                                                   */
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

    /* Create 5 main ButtonSprites */
    this->titleSprite = new ButtonSprite(0x42A);     /* +0x220 */
    this->field_224   = new ButtonSprite(0x42C);     /* +0x224 */
    this->field_228   = new ButtonSprite(0x429);     /* +0x228 */
    this->field_22C   = new ButtonSprite(0x42B);     /* +0x22C */
    this->field_230   = new ButtonSprite(0x42F);     /* +0x230 */

    /* field at +0x120 (byte, set to 0) */
    this->titleText[0] = '\0';        /* +0x120 (byte) */

    /* NOTE: backgroundSprite (+0x238) is NOT initialized here.
       It is set by other code (likely create_window or render init) before use. */

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
        LayoutListNode* node = (LayoutListNode*)this->titleList;
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
        LayoutListNode* node = (LayoutListNode*)this->layoutList;
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
            ReleaseSoundResource(sndHandle);
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
    void* activeList;
    if (g_netman->m_playerSlotCount == 0) {
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
    UIPANEL_EndPaintEx(this, this->hWnd, 0, 0, NULL);
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
    /* g_netman->m_bFlag1 (+0x7C8) = network session active flag */
    /* g_netman->m_bInit  (+0x04)  = network mode flag */
    if (g_netman->m_bFlag1 != 0 && g_netman->m_bInit != 0) {
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
        if (g_netman->m_playerSlotCount == 0) {
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
void GameSetupPanel::drawLayoutList(void* list)
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
    if (list != LIST_SENTINEL_CACHED && list != NULL) {
        int lineStep = lineHeight + 4;
        LayoutListNode* node = (LayoutListNode*)list;
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
/* Player data is read from g_netman structure:                         */
/*   g_netman+0x08 = maxPlayers                                       */
/*   g_netman+0x0C = playerCount (columns)                             */
/*   g_netman+0x10 = currentPlayer (rows)                              */
/*   g_netman + row*0x4C + 0x518 = playerId (0 = empty)                */
/*   g_netman + row*0x4C + 0x51D = player name string                  */
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
    int numRows = g_netman->m_playerCols;    /* column count = rows to draw */
    int numCols = g_netman->m_playerRows;     /* row count = columns to draw */
    int maxPlayers = g_netman->m_playerSlotCount;

    int insertedCount = 0;  /* total players inserted across all rows */

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
                int playerId = g_netman->m_slots[row].dpId;

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
                    const char* playerName = g_netman->m_slots[row].layout_name;
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
        LayoutListNode* node = (LayoutListNode*)this->titleList;
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
    int* pData = NULL;        /* local_2c / loaded asset data */
    int dataSize = 0;         /* local_28 */
    int* streamResult = NULL; /* local_1c / stream result pointer */
    int loadedData = 0;       /* local_280 / data bytes */

    /* Step 3: Try AssetMgr first */
    if (g_asset_mgr != NULL) {
        pData = (int*)AssetMgr_LoadFile((AssetMgr*)g_asset_mgr, (uint8_t*)filePath, &dataSize);
        if (pData != NULL) {
            streamObj = (char*)operator_new(0x5C);
            if (streamObj != NULL) {
                streamResult = (int*)WNDPROC_StreamFromMemory(streamObj, (const char*)pData, dataSize, 1);
            }
        }
    }

    /* Step 4: Fall back to direct file open if AssetMgr failed */
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
            newNode->next = (LayoutListNode*)this->titleList;   /* node[0] = next pointer */
            this->titleList = (void*)newNode;

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
