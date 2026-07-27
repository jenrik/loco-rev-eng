/**
 * GameWindow.h — Base class for DirectDraw-backed game windows
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * GameWindow is a separate base class from UI_WindowBase. It is used by
 * AboutDialog, TrainStationWindow, and AudioMgr (which embeds HelpWnd).
 * Unlike UI_WindowBase (which uses the main game backbuffer), GameWindow
 * creates its OWN DirectDraw offscreen surface for its content and blits
 * it to the primary backbuffer during Show/Hide.
 *
 * Key differences from UI_WindowBase:
 *   - Creates its own DDraw offscreen plain surface for rendering
 *   - Manages 2 auxiliary DDraw sub-objects (released in base dtor)
 *   - Has a direct Create method that registers WNDCLASS + creates HWND
 *   - Stores window title at +0xA8 (50 chars) used as WNDCLASS class name
 *   - Supports repositioning via SetPosition
 *   - Manages shared cursor backbuffer via ref-count at +0x5C
 *
 * Subclasses: AboutDialog (vtable 0x477680), TrainStationWindow (vtable 0x478130),
 *   AudioMgr / HelpWnd (vtable 0x478428)
 *
 * Size: ~0x118 bytes (280 bytes base)
 * Vtable address in loco.exe: 0x477898
 *
 * Class hierarchy:
 *   GameWindow  ← this class (root)
 *     ├─ AboutDialog               (vtable 0x477680)
 *     ├─ TrainStationWindow        (vtable 0x478130)
 *     └─ AudioMgr / HelpWnd        (vtable 0x478428)
 *
 * Vtable layout (8 entries, 0x20 bytes):
 *   [0] +0x00: ~GameWindow (scalar deleting dtor) (0x413B50)
 *   [1] +0x04: hide                            (0x413C10)
 *   [2] +0x08: show                            (0x413D10)
 *   [3] +0x0C: set_mode                        (default: Cursor_SetMode,    0x414340)
 *   [4] +0x10: cleanup_sprites                 (default: stub at 0x426130)
 *   [5] +0x14: create                          (0x413DE0)
 *   [6] +0x18: init                            (default: Cursor_UpdateClientRect, 0x4140A0)
 *   [7] +0x1C: update_anim                     (default: stub at 0x426130)
 */

// Status: TRANSCRIBED

#pragma once

#include "../shared/types.h"
/* vtable addresses in vtable_addrs.h — compiler manages vtables via virtual methods */

/* Forward declaration: DDraw surface type used for backbuffer and aux pointers */
struct IDirectDrawSurface4;

/* ================================================================== */
/* GameWindow class                                                     */
/* ================================================================== */

class GameWindow {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */
/* vtable at +0x00 is compiler-managed */
    HINSTANCE  hInstance;              // +0x04  application instance handle
    HWND       hWnd;                   // +0x08  this window's HWND (set by Create)
    HWND       hWndParent;             // +0x0C  parent window HWND (stored during Create)

    UINT       resourceId;             // +0x10  window/resource ID (for title string loading)
    int32_t    field_14;               // +0x14  zeroed in ctor; used by subclasses as state

    /* Cached window rectangle (set during Create, updated by SetPosition) */
    int32_t    rectLeft;               // +0x18  window rect left
    int32_t    rectTop;                // +0x1C  window rect top
    int32_t    rectRight;              // +0x20  window rect right
    int32_t    rectBottom;             // +0x24  window rect bottom

    int32_t    field_28;               // +0x28  zeroed in create() when backbufferSurface is NULL
    int32_t    field_2C;               // +0x2C  zeroed in create() when backbufferSurface is NULL

    int32_t    width;                  // +0x30  window width  (from Create nWidth param)
    int32_t    height;                 // +0x34  window height (from Create nHeight param)

    IDirectDrawSurface4* backbufferSurface; // +0x38  own offscreen surface (DDraw backbuffer)
                                       //         released via vtable[2] in base dtor

    int32_t    defaultWidth;           // +0x3C  default width  (set to 0x20 in ctor)
    int32_t    defaultHeight;          // +0x40  default height (set to 0x20 in ctor)

    int32_t    field_44;               // +0x44  zeroed in ctor
    int32_t    field_48;               // +0x48  zeroed in ctor

    UINT_PTR   timerId;                // +0x4C  window timer ID (set by Show, killed by Hide)

    int32_t    field_50;               // +0x50  NOT zeroed in ctor
    int32_t    field_54;               // +0x54  NOT zeroed in ctor

    uint8_t    visible;                // +0x58  byte — visible flag, set to 1 by Show
    uint8_t    _pad_59[3];             // +0x59  padding

    int32_t    cursorRefcount;         // +0x5C  shared cursor backbuffer reference count
                                       //         incremented by Cursor_InitSprites,
                                       //         decremented by base dtor

    int32_t    field_60;               // +0x60  NOT zeroed in ctor
    int32_t    field_64;               // +0x64  NOT zeroed in ctor

    int32_t    reserved_work[8];        // +0x68  scratch fields; zeroed in ctor,
                                       //         cleared by set_mode(resetPos=1).
                                       //         Indices: [0]=+0x68, [1]=+0x6C, [2]=+0x70,
                                       //         [3]=+0x74, [4]=+0x78, [5]=+0x7C,
                                       //         [6]=+0x80, [7]=+0x84

    uint8_t    captureFlag;            // +0x88  byte — mouse capture flag
                                       //         checked in Hide for ReleaseCapture
    uint8_t    _pad_89[7];             // +0x89  padding

    /* Auxiliary DirectDraw sub-objects (released in base dtor) */
    int32_t    ddrawAuxCount1;         // +0x90  aux object 1 count/flag (zeroed in ctor)
    int32_t    ddrawAuxField1;         // +0x94  aux object 1 field  (zeroed in ctor)
    IDirectDrawSurface4* ddrawAuxPtr1; // +0x98  aux DDraw surface 1 (NOT zeroed in ctor)

    int32_t    ddrawAuxCount2;         // +0x9C  aux object 2 count/flag (zeroed in ctor)
    int32_t    ddrawAuxField2;         // +0xA0  aux object 2 field  (zeroed in ctor)
    IDirectDrawSurface4* ddrawAuxPtr2; // +0xA4  aux DDraw surface 2 (NOT zeroed in ctor)

    char       title[50];              // +0xA8  window title string (50 bytes)
                                       //         loaded via FormatResourceString in ctor
                                       //         used as WNDCLASS.lpszClassName in Create

    uint8_t    _pad_DA;                 // +0xDA  padding byte
    uint8_t    createdFlag;            // +0xDB  byte — 1 = window has been created
                                       //         set in Create before calling vtable[6] init callback

    int32_t    windowX;                // +0xDC  window left X position (stored from Create x param)
    int32_t    windowY;                // +0xE0  window top  Y position (stored from Create y param)

    int32_t    windowWidth;            // +0xE4  window width (from Create nWidth, overwritten
                                       //         by init() with GetClientRect result)
    int32_t    windowHeight;           // +0xE8  window height (from Create nHeight, overwritten
                                       //         by init() with GetClientRect result)

    int32_t    workWidth;              // +0xEC  working rect width  (set by init())
    int32_t    workHeight;             // +0xF0  working rect height (set by init())

    /* Temporary rect used by init() for GetClientRect output */
    int32_t    tempLeft;               // +0xF4  temporary rect left
    int32_t    tempTop;                // +0xF8  temporary rect top
    int32_t    tempRight;              // +0xFC  temporary rect right
    int32_t    tempBottom;             // +0x100 temporary rect bottom

    /* Client rectangle (copied from temp rect by init()).
       also used as width/height offsets by SetPosition. */
    int32_t    clientLeft;             // +0x104 cached client rect left
    int32_t    clientTop;              // +0x108 cached client rect top
    int32_t    clientWidth;            // +0x10C cached client rect right (= width)
                                       //         also: width offset used by SetPosition
    int32_t    clientHeight;           // +0x110 cached client rect bottom (= height)
                                       //         also: height offset used by SetPosition

    uint8_t    visible2;               // +0x114  byte — secondary visible flag
                                       //         set to 1 by Show, cleared to 0 by Hide

    /* Total base size: 0x118 bytes */
    /* Subclass-specific fields begin at +0x118 */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * GameWindow constructor.
     * Address: 0x413AB0
     *
     * Initializes the base window object:
     *   - C++ construction installs the GameWindow vtable (0x477898)
     *   - Stores hInstance at +0x04, resourceId at +0x10
     *   - Sets default width/height to 0x20
     *   - Zeroes most fields (cursorRefcount, backbuffer, DDraw aux objects, etc.)
     *   - Some fields are left uninitialized (+0x50, +0x54, +0x60, +0x64, +0x98, +0xA4)
     *   - Loads window title from string resources via FormatResourceString
     *     into +0xA8 (max 50 chars)
     *
     * Called by: AboutDialog_Ctor @ 0x40F1E9,
     *            TrainStationWindow_Ctor @ 0x436B2F,
     *            AudioMgr_Ctor @ 0x44F4B9
     *
     * @param hInstance    Application instance handle
     * @param resId        Window resource ID (used to load title string)
     */
    GameWindow(HINSTANCE hInstance, UINT resId);

    /**
     * Scalar deleting destructor (vtable[0]).
     * Address: 0x413B50
     *
     * Calls base_destructor() to release all resources, then
     * optionally frees the heap allocation via GLOBAL_free if flags & 1.
     *
     * @param flags  Delete flag (bit 0 = free heap memory)
     * @return       This pointer (after dtor body)
     */
    virtual ~GameWindow();

    /**
     * update_anim — vtable[7] callback. Fired after window is shown.
     * Address: 0x426130 (default stub)
     * Overridden by HelpWnd as update_anim (0x450450).
     */
    virtual void update_anim(int param);

    /**
     * Base destructor body (called by scalar deleting destructor).
     * Address: 0x413B70
     *
     * Release sequence:
     *   1. Enters destruction with the compiler-managed GameWindow vtable
     *   2. Releases two auxiliary DDraw sub-objects (pairs at
     *      +0x90/+0x98, +0x9C/+0xA4) via vtable[2] if non-null
     *   3. Decrements global cursor backbuffer refcount (+0x5C).
     *      If refcount reaches 0, releases the global cursor backbuffer
     *      (_g_cursor_back @ 0x4FD3CC) via vtable[2]
     *   4. Releases own backbuffer surface (+0x38) via vtable[2]
     *
     * Called by: GameWindow_Dtor, AboutDialog_BaseDtor, TrainStationWindow_BaseDtor,
     *            AudioMgr_Dtor, and various exception-handling unwinds
     */
    void base_destructor();

    /**
     * set_mode — vtable[3] callback. Set cursor/animation state.
     * Address: 0x414340 (default impl: Cursor_SetMode)
     *
     * If stateId matches current, no-op (unless stateId=0 → early return).
     * Updates field_14/field_44/field_48. If resetPos, zeroes reserved_work
     * block. If forceRedraw and !visible, triggers UnlockPrimary →
     * UpdateDirtyRect → UnlockAllSurfaces → RenderWithViewport.
     *
     * @param stateId      New state ID (0 = hide cursor)
     * @param resdata      Resource data pointer (stored at +0x44)
     * @param resetPos     If non-zero, zero the reserved_work block (+0x68..+0x84)
     * @param forceRedraw  If non-zero, force a dirty-rect render cycle
     */
    virtual void set_mode(int32_t stateId, void* resdata, uint8_t resetPos, uint8_t forceRedraw);

    /**
     * cleanup_sprites — vtable[4] callback.
     * Address: 0x426130 (default stub)
     * Overridden by HelpWnd as cleanup_sprites (0x451440).
     */
    virtual void cleanup_sprites();

    /**
     * init — vtable[6] callback.
     * Address: 0x4140A0 (default: Cursor_UpdateClientRect)
     * Overridden by HelpWnd as init (0x451180).
     */
    virtual void init();

    /**
     * Hide the window (vtable[1]).
     * Address: 0x413C10
     *
     * Saves screen content behind window into the game backbuffer via Blt,
     * kills the timer, releases mouse capture, hides the HWND, and clears
     * the visible2 flag.
     *
     * Called by: HelpWnd_Hide @ 0x450AE4, TrainStationWindow_Hide @ 0x436F78,
     *            CGWND_Screensaver_Hide @ 0x40F48D
     */
    virtual void hide();

    /**
     * Show the window (vtable[2]).
     * Address: 0x413D10
     *
     * Sets capture, creates a 190ms timer (ID 0x43), shows the HWND,
     * sets visible flags (visible + visible2), fires vtable[7] callback,
     * restores saved backbuffer content via Blt from this window's surface
     * to the primary backbuffer.
     *
     * Called by: HelpWnd_Show @ 0x4503F9, TrainStationWindow_Show @ 0x436EC4,
     *            AboutDialog (via vtable dispatch @ 0x40F2AA)
     */
    virtual void show();

    /**
     * SetPosition — Reposition the window.
     * Address: 0x413D90
     *
     * Updates the cached window rectangle and calls SetWindowPos to
     * physically reposition the HWND. Uses stored client area dimensions
     * at +0x10C/+0x110 to compute the window size for SetWindowPos.
     *
     * Called by: HelpWnd_Show @ 0x450389
     *
     * @param x  New left X position in screen coordinates
     * @param y  New top Y position in screen coordinates
     */
    void set_position(int x, int y);

    /**
     * Create — Register WNDCLASS, create HWND, allocate DDraw surface
     * (vtable[5]). Called directly (not via vtable) from subclass Create
     * methods.
     * Address: 0x413DE0
     *
     * Full window creation sequence:
     *   1. Stores layout parameters (x, y, width, height, hWndParent)
     *   2. Registers a WNDCLASS with title string as class name and a
     *      shared WndProc (0x415900)
     *   3. Creates the HWND via CreateWindowExA (style = WS_POPUP |
     *      WS_CLIPSIBLINGS | WS_CLIPCHILDREN). The 'this' pointer is
     *      passed as the CREATESTRUCT lpCreateParams.
     *   4. Sets createdFlag = 1
     *   5. Fires vtable[6] (init callback)
     *   6. Calls Cursor_InitSprites to set up cursor overlay
     *   7. Creates an offscreen DDraw surface (if not already present)
     *      via IDirectDraw4::CreateSurface with DDSD_CAPS|DDSD_WIDTH|
     *      DDSD_HEIGHT and dwCaps=DDSCAPS_OFFSCREENPLAIN (0x840)
     *   8. Caches window rectangle and dimensions
     *   9. Shows the window via ShowWindow + UpdateWindow
     *  10. Stores capture flag from param
     *
     * Called by: AboutDialog_Create @ 0x40F5A1,
     *            TrainStationWindow_Create @ 0x436D49,
     *            HelpWnd_Create @ 0x450D40
     *
     * @param nCmdShow      Initial show command (SW_* flags)
     * @param hWndParent    Parent window HWND
     * @param x             Window X position
     * @param y             Window Y position
     * @param nWidth        Window width
     * @param nHeight       Window height
     * @param hMenu         Menu handle (or NULL)
     * @param hIcon         Window icon handle (for WNDCLASS.hIcon)
     * @param classStyle    WNDCLASS.style override (0 = CS_HREDRAW|CS_VREDRAW)
     * @param unused1       Unused parameter (passed by callers for compatibility)
     * @param unused2       Unused parameter (passed by callers for compatibility)
     * @param showCursor    Byte: 0 = don't capture cursor on close, 1 = capture
     * @return              1 on success, 0 on failure
     */
    virtual int create(int nCmdShow, HWND hWndParent, int x, int y,
               int nWidth, int nHeight, HMENU hMenu, HICON hIcon,
               UINT classStyle, int unused1, int unused2, uint8_t showCursor);
};

static_assert(sizeof(GameWindow) == 0x118, "GameWindow size must be 0x118 bytes");
