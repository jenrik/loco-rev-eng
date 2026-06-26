/*
 * Lego Loco (1998) - Decompiled and documented for Linux port
 * Subsystem: UI (dialogs, main menu, settings, multiplayer lobby)
 * WIN32: CreateDialogParamA -> LINUX: SDL2 custom rendering
 *
 * Original binary: loco.exe (Win32, x86, MSVC-compiled)
 *
 * This header provides:
 *   - Struct definitions with documented field offsets for the full UI hierarchy:
 *       UIPanel (building picker), EditWindow (fullscreen dialog controller),
 *       PanelA (name-entry), PanelB (city/multiplayer selection), ButtonSprite,
 *       LOCOBITMAP, ScreenSaverCtx, SaveFileNode
 *   - Function declarations for all UI subsystem functions
 *   - Constants extracted from the decompiled binary:
 *       dialog resource IDs, language IDs, dialog state machine values,
 *       main-menu sprite resource IDs, multiplayer slot counts
 *   - Platform abstraction macros and type aliases
 *     (#ifdef LOCO_LINUX ... #else ... #endif blocks for SDL2 vs Win32 types)
 *
 * Build with -DLOCO_LINUX to compile the SDL2/POSIX port.
 * Without -DLOCO_LINUX the Win32 types and stubs are used.
 *
 * PORTING NOTES (summary):
 *   1.  HWND at UIPanel+0x08          -> SDL_Window*
 *   2.  IDirectDrawSurface::Blt       -> SDL_RenderCopy
 *   3.  IDirectDrawSurface::SetClipper-> SDL_RenderSetClipRect
 *   4.  IDirectDrawSurface::GetBltSt  -> SDL_RenderFlush (or no-op)
 *   5.  GetCursorPos                  -> SDL_GetGlobalMouseState (SDL 2.0.4+)
 *   6.  IntersectRect / UnionRect     -> SDL_IntersectRect / SDL_UnionRect
 *   7.  WndProc / WM_PAINT            -> SDL event loop / main render tick
 *   8.  DestroyWindow                 -> SDL_DestroyWindow
 *   9.  PostQuitMessage(0)            -> SDL_PushEvent(SDL_QUIT)
 *  10.  Sleep(ms)                     -> SDL_Delay(ms)
 *  11.  ExitProcess(1)                -> SDL_Quit(); exit(1)
 *  12.  EnterCriticalSection          -> SDL_LockMutex
 *  13.  LeaveCriticalSection          -> SDL_UnlockMutex
 *  14.  operator delete (MSVC)        -> free()
 *  15.  MSVC SEH ExceptionList        -> removed; use RAII / C++ try/catch
 *  16.  RECT {left,top,right,bottom}  -> LOCO_RECT; SDL_Rect uses {x,y,w,h}
 */

#ifndef UIPANEL_H
#define UIPANEL_H

/*---------------------------------------------------------------------------
 * Platform type definitions
 *--------------------------------------------------------------------------*/

#ifdef LOCO_LINUX
/*
 * Linux/SDL2 build.
 * SDL_Window* replaces HWND.
 * SDL_Renderer* / SDL_Texture* replace IDirectDrawSurface**.
 * SDL_mutex* replaces CRITICAL_SECTION / DirectDraw Lock/Unlock.
 */
#  include <SDL2/SDL.h>
#  include <stdint.h>
#  include <stdlib.h>
#  include <string.h>

/* Aliases that let shared code still name the Win32 types */
typedef SDL_Window  *HWND;
typedef uint32_t     UINT;
typedef intptr_t     WPARAM;
typedef intptr_t     LPARAM;
typedef intptr_t     LRESULT;
typedef uint32_t     DWORD;

#else /* Win32 build */
/*
 * Win32 build — use the Windows SDK types directly.
 */
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <ddraw.h>
#  include <stdint.h>
#  include <stdlib.h>
#  include <string.h>

#endif /* LOCO_LINUX */

#include <stdint.h>

/*===========================================================================
 * Constants from the decompiled binary
 *===========================================================================*/

/*
 * DDBLT_WAIT = 0x1000000
 * Passed as the 5th argument to every IDirectDrawSurface::Blt call in
 * Panel_Render (0x426b90) and Panel_DragUpdate (0x426eb0).
 * Instructs DirectDraw to busy-wait until hardware is ready rather than
 * returning DDERR_WASSTILLDRAWING.
 * LINUX: no SDL2 equivalent — SDL_RenderCopy is always synchronous.
 */
#define DDBLT_WAIT                  0x01000000u

/*
 * PANEL_TYPE = 0xc (12)
 * Object type identifier stored at UIPanel+0x04.
 * Set by UIPanel_Ctor (0x427370).
 * Used for runtime type identification in the game's object system.
 */
#define PANEL_TYPE                  0x0c

/*
 * DIRTY_RECT_MAX_DIM = 0x100 (256 pixels)
 * Coalescing threshold in Panel_DragUpdate (0x426eb0).
 * If the union of the old and new blit rects fits within 256x256 px,
 * both regions are combined into a single padded blit pass.
 */
#define DIRTY_RECT_MAX_DIM          0x100

/*
 * DIRTY_RECT_PADDING = 4 pixels
 * Border added to each edge of the coalesced dirty rect.
 * Prevents sub-pixel tearing when the cursor moves a short distance.
 */
#define DIRTY_RECT_PADDING          4

/*
 * BLIT_SYNC_TIMEOUT_ITERS = 1000
 * Maximum polling iterations in Panel_WaitBltSync (0x426b00).
 * At BLIT_SYNC_SLEEP_MS per iteration this equals a 10-second hard timeout.
 */
#define BLIT_SYNC_TIMEOUT_ITERS     1000

/*
 * BLIT_SYNC_SLEEP_MS = 10
 * Milliseconds between each IDirectDrawSurface::GetBltStatus poll in
 * Panel_WaitBltSync.
 * WIN32: Sleep(BLIT_SYNC_SLEEP_MS)
 * LINUX: SDL_Delay(BLIT_SYNC_SLEEP_MS) — only if retry logic is retained.
 */
#define BLIT_SYNC_SLEEP_MS          10

/*
 * CURSOR_INVALID_SENTINEL = 0xffffffff (-1 as signed int)
 * Written to UIPanel.last_cursor_x (+0x34) and UIPanel.last_cursor_y (+0x38)
 * when dragging stops or the cursor tracking is reset.
 * Distinguishes 'no previous blit position' from the valid coordinate (0,0).
 */
#define CURSOR_INVALID_SENTINEL     0xffffffffu

/*
 * IDirectDrawSurface COM vtable offsets.
 * The vtable layout follows the IUnknown + IDirectDrawSurface COM ABI:
 *   QueryInterface          = 0x00
 *   AddRef                  = 0x04
 *   Release                 = 0x08
 *   AddAttachedSurface      = 0x0c
 *   AddOverlayDirtyRect     = 0x10
 *   Blt                     = 0x14  <- DDRAW_VTBL_BLT
 *   ...
 *   GetBltStatus            = 0x44  <- DDRAW_VTBL_GETBLTSTATUS
 *   ...
 *   SetClipper              = 0x68  <- DDRAW_VTBL_SETCLIPPER
 *
 * These are used in Panel_Render (0x426b90), Panel_WaitBltSync (0x426b00),
 * and Panel_DragUpdate (0x426eb0).
 */
#define DDRAW_VTBL_BLT              0x14
#define DDRAW_VTBL_GETBLTSTATUS     0x44
#define DDRAW_VTBL_SETCLIPPER       0x68

/*
 * Panel mode constants (uint16 at UIPanel+0x49c).
 * The UIPanel state machine has 6 states (0-5).
 * Managed by FUN_004277d0 (not yet separately documented).
 */
#define PANEL_MODE_HIDDEN           0   /* inactive/hidden; global ptr -> null sentinel */
#define PANEL_MODE_MAIN             1   /* full building picker, all 4 tab buttons      */
#define PANEL_MODE_TAB2             2   /* tab 2 view                                   */
#define PANEL_MODE_TAB3             3   /* tab 3 view                                   */
#define PANEL_MODE_TAB4             4   /* tab 4 view                                   */
#define PANEL_MODE_TAB5             5   /* tab 5 view                                   */

/*
 * Building-picker tile array dimensions.
 * BUILDING_TILE_COUNT    = 6    (UIPanel+0x4c0..+0x4d4, 6 pointer slots)
 * BUILDING_TILE_SPACING  = 25   (vertical px spacing set per tile by FUN_00427580)
 */
#define BUILDING_TILE_COUNT         6
#define BUILDING_TILE_SPACING       0x19   /* 25 pixels */

/*
 * Panel content resource type IDs.
 * FUN_00427580 queries the resource registry (DAT_004855e8) with these IDs
 * via FUN_00446ea0 to look up panel content elements for the building picker.
 */
#define PANEL_CONTENT_BASE_ID       0x2c00
#define PANEL_CONTENT_MAX_ID        0x2c0f

/*===========================================================================
 * Portable RECT type
 *
 * Win32 uses RECT {left, top, right, bottom} (all LONG).
 * SDL2 uses SDL_Rect {x, y, w, h} (all int).
 *
 * On Linux we define LOCO_RECT to match the Win32 RECT layout so that
 * dirty-rect arithmetic in Panel_Render and Panel_DragUpdate compiles
 * unchanged.  Conversion helpers convert to/from SDL_Rect at blit sites.
 *===========================================================================*/

#ifndef LOCO_LINUX
/* Win32: RECT and LONG already defined by <windows.h> */
typedef RECT   LOCO_RECT;
typedef LONG   LOCO_LONG;

#else  /* LOCO_LINUX */

typedef int32_t  LOCO_LONG;

typedef struct LOCO_RECT {
    LOCO_LONG left;
    LOCO_LONG top;
    LOCO_LONG right;
    LOCO_LONG bottom;
} LOCO_RECT;

/*
 * LINUX: convert LOCO_RECT (l/t/r/b) to SDL_Rect (x/y/w/h).
 * Use at every IDirectDrawSurface::Blt -> SDL_RenderCopy call site.
 */
static inline SDL_Rect loco_rect_to_sdl(const LOCO_RECT *r)
{
    SDL_Rect s;
    s.x = (int)r->left;
    s.y = (int)r->top;
    s.w = (int)(r->right  - r->left);
    s.h = (int)(r->bottom - r->top);
    return s;
}

/*
 * LINUX: convert SDL_Rect (x/y/w/h) back to LOCO_RECT (l/t/r/b).
 */
static inline LOCO_RECT sdl_rect_to_loco(const SDL_Rect *s)
{
    LOCO_RECT r;
    r.left   = (LOCO_LONG)s->x;
    r.top    = (LOCO_LONG)s->y;
    r.right  = (LOCO_LONG)(s->x + s->w);
    r.bottom = (LOCO_LONG)(s->y + s->h);
    return r;
}

/*
 * LINUX: IntersectRect replacement.
 * WIN32: BOOL IntersectRect(LPRECT lprcDst, const RECT *lprcSrc1,
 *                            const RECT *lprcSrc2)
 * LINUX: SDL_IntersectRect (SDL 2.0.4+) returns SDL_TRUE when non-empty.
 * Returns 1 if intersection is non-empty, 0 otherwise (matches Win32 BOOL).
 */
static inline int loco_intersect_rect(LOCO_RECT *out,
                                      const LOCO_RECT *a,
                                      const LOCO_RECT *b)
{
    SDL_Rect sa = loco_rect_to_sdl(a);
    SDL_Rect sb = loco_rect_to_sdl(b);
    SDL_Rect sr;
    if (SDL_IntersectRect(&sa, &sb, &sr) == SDL_TRUE)
    {
        *out = sdl_rect_to_loco(&sr);
        return 1;
    }
    out->left = out->top = out->right = out->bottom = 0;
    return 0;
}

/*
 * LINUX: UnionRect replacement.
 * WIN32: BOOL UnionRect(LPRECT lprcDst, const RECT *lprcSrc1,
 *                        const RECT *lprcSrc2)
 * LINUX: SDL_UnionRect (SDL 2.0.4+)
 */
static inline void loco_union_rect(LOCO_RECT *out,
                                   const LOCO_RECT *a,
                                   const LOCO_RECT *b)
{
    SDL_Rect sa = loco_rect_to_sdl(a);
    SDL_Rect sb = loco_rect_to_sdl(b);
    SDL_Rect sr;
    SDL_UnionRect(&sa, &sb, &sr);
    *out = sdl_rect_to_loco(&sr);
}

#endif /* LOCO_LINUX */

/*===========================================================================
 * Forward declarations
 *===========================================================================*/

typedef struct UIPanel       UIPanel;
typedef struct UIScrollPos   UIScrollPos;
typedef struct UIHotspotList UIHotspotList;
typedef struct UIElement     UIElement;
typedef struct UIButton      UIButton;
typedef struct UIBuildTile   UIBuildTile;
typedef struct UIPanelVtable UIPanelVtable;

/*===========================================================================
 * UIScrollPos — 16-byte sub-object at UIPanel+0x3f0
 *
 * Initialised by FUN_00405790(-1, -1, 0, 0).
 * Destroyed by FUN_00405870.
 *
 * Default scroll_x = -1, scroll_y = -1 is the "no scroll" sentinel state.
 *===========================================================================*/
struct UIScrollPos {
    int scroll_x;   /* +0x00  -1 = no scroll (sentinel) */
    int scroll_y;   /* +0x04  -1 = no scroll            */
    int width;      /* +0x08  0 = uninitialised         */
    int height;     /* +0x0c  0 = uninitialised         */
};
/* Expected size: 16 bytes */

/*===========================================================================
 * UIHotspotList — variable-size sub-object at UIPanel+0x478
 *
 * Opaque dynamic array of button/hotspot entries.
 * Initialised by FUN_0042a110.
 * Destroyed by FUN_0042a370.
 *
 * Each entry is a UIButton with:
 *   vtable+0x0c = SetPosition(btn, x, y)
 *   vtable+0x20 = Refresh(btn)
 *   btn+0x56    = visibility flag (char)
 *===========================================================================*/
struct UIHotspotList {
    void **entries;   /* pointer to dynamically allocated button array */
    int    count;     /* number of entries in use                      */
    int    capacity;  /* allocated slot count                          */
};

/*===========================================================================
 * UIElement — linked-list node, size >= 0x8b4 bytes
 *
 * Child elements stored in the linked list rooted at UIPanel+0x4d8.
 * next-pointer at element+0x22c (= element + 0x8b * 4).
 *
 * Each element has a vtable whose slot [0] is its destructor.
 * A string field is accessed via FUN_004490d0.
 *===========================================================================*/
struct UIElement {
    void  **vtable;        /* +0x000  vtable[0] = destructor(elem, delete_flag) */
    char    _pad[0x22b];   /* +0x004..+0x22b  internal fields (not fully mapped) */
    void   *next;          /* +0x22c  next sibling in UIPanel+0x4d8 linked list  */
    /* fields beyond +0x22d not mapped in this analysis */
};

/*===========================================================================
 * UIButton — accessed via vtable; stored as pointers in UIPanel
 *
 * Known vtable methods (called in FUN_004277d0 state machine):
 *   vtable+0x0c = SetPosition(btn, x, y)
 *   vtable+0x20 = Refresh(btn)
 * Known field:
 *   btn+0x56 = visibility flag (char; 1=visible, 0=hidden)
 *===========================================================================*/
struct UIButton {
    void **vtable;     /* +0x00 */
    char   _opaque[0x55];
    char   visible;   /* +0x56  visibility flag */
};

/*===========================================================================
 * UIBuildTile — one building-picker slot in UIPanel+0x4c0..+0x4d4
 *
 * 6 slots total (BUILDING_TILE_COUNT).
 * Tiles are spaced BUILDING_TILE_SPACING (25 px) apart vertically.
 * Populated by FUN_00427580 using resource IDs 0x2c02..0x2c0c.
 *===========================================================================*/
struct UIBuildTile {
    void **vtable;   /* +0x00  virtual dispatch table */
    /* internal layout opaque */
};

/*===========================================================================
 * UIPanelVtable — virtual dispatch table for UIPanel
 * Original address: 0x00477cc8  (PTR_FUN_00477cc8)
 *
 * Set at UIPanel+0x00 in UIPanel_Ctor (0x427370).
 * Re-set at the start of UIPanel_Dtor body (FUN_00427460) before cleanup.
 *
 * Documented slots (0-indexed, each slot is one function pointer = 4 bytes):
 *   [0]  vtable+0x00 = destructor(self, delete_flag)
 *   [1]  vtable+0x04 = (not documented in this analysis)
 *   [2]  vtable+0x08 = (not documented)
 *   [3]  vtable+0x0c = (not documented)
 *   [4]  vtable+0x10 = (not documented)
 *   [5]  vtable+0x14 = (not documented)
 *   [6]  vtable+0x18 = release/close (called in destructor on self and sub-obj)
 *===========================================================================*/
struct UIPanelVtable {
    void (*destructor)(UIPanel *self, int delete_flag);   /* slot 0  vtable+0x00 */
    void (*slot1)(UIPanel *self);                         /* slot 1  vtable+0x04 */
    void (*slot2)(UIPanel *self);                         /* slot 2  vtable+0x08 */
    void (*slot3)(UIPanel *self);                         /* slot 3  vtable+0x0c */
    void (*slot4)(UIPanel *self);                         /* slot 4  vtable+0x10 */
    void (*slot5)(UIPanel *self);                         /* slot 5  vtable+0x14 */
    void (*release)(UIPanel *self);                       /* slot 6  vtable+0x18 */
    /* additional undocumented slots follow */
};

/*===========================================================================
 * UIPanel — main panel object
 *
 * Original vtable pointer: PTR_FUN_00477cc8 at 0x00477cc8
 * Object size: at least 0x4e0 (1248) bytes (full derived object).
 *
 * Field offsets are listed as hex inline comments.
 * All offsets are from the start of the UIPanel object (from `this`).
 *
 * Sub-object embedding:
 *   UIScrollPos   at +0x3f0  (16 bytes)
 *   UIHotspotList at +0x478  (variable)
 *===========================================================================*/
struct UIPanel {

    /* ---- Virtual dispatch + type identity -------------------------------- */
    UIPanelVtable  *vtable;              /* +0x00  PTR_FUN_00477cc8               */
    int             type;                /* +0x04  PANEL_TYPE = 0xc               */

    /* ---- OS window / surface handle ------------------------------------- */
#ifdef LOCO_LINUX
    SDL_Window     *sdl_window;          /* +0x08  Linux: SDL_Window*             */
                                         /*        (replaces Win32 HWND)          */
#else
    HWND            hwnd;                /* +0x08  Win32: window handle           */
#endif

    /* ---- Panel hierarchy ------------------------------------------------ */
    int             sibling_count;       /* +0x0c  0 = last panel -> post quit    */
    int             _pad_10;             /* +0x10  reserved                       */

    /* ---- Bitmap content ------------------------------------------------- */
    void           *content_bitmap;      /* +0x14  LOCOBITMAP*; NULL if empty     */
    int             panel_width;         /* +0x18  frame width in pixels          */
    int             panel_height;        /* +0x1c  frame height in pixels         */

    /* ---- Animated sprite strip ------------------------------------------ */
    int             frame_count;         /* +0x20  1=static; >=2=animated strip   */
    int             current_frame;       /* +0x24  0-based frame index            */
    int             _pad_28;             /* +0x28  reserved                       */

    /* ---- Screen position ------------------------------------------------ */
    int             screen_origin_x;     /* +0x2c  panel left edge (screen coords)*/
    int             screen_origin_y;     /* +0x30  panel top edge (screen coords) */

    /* ---- Last-known cursor position (panel-local coords) ----------------- */
    int             last_cursor_x;       /* +0x34  -1 = CURSOR_INVALID_SENTINEL   */
    int             last_cursor_y;       /* +0x38  -1 = CURSOR_INVALID_SENTINEL   */

    /* ---- Render control flags ------------------------------------------- */
    char            render_flags;        /* +0x3c  non-zero skips cursor-rel path */
    char            _pad_3d[7];          /* +0x3d..+0x43 alignment padding        */

    /* ---- Drag-and-drop state -------------------------------------------- */
    char            drag_active;         /* +0x44  non-zero enables DragUpdate    */
    char            _pad_45[3];          /* +0x45..+0x47 alignment padding        */

    /* ---- DirectDraw surface (Win32) / SDL texture (Linux) --------------- */
#ifdef LOCO_LINUX
    SDL_Texture    *sdl_texture;         /* +0x48  Linux: SDL_Texture*            */
                                         /*        (replaces IDirectDrawSurface**)*/
#else
    void          **dd_surface;          /* +0x48  IDirectDrawSurface** (vtable   */
                                         /*        calls at vtable+0x14/0x44/0x68)*/
#endif

    /* ---- Blit synchronisation ------------------------------------------- */
    int             blit_sync_result;    /* +0x4c  result stored by WaitBltSync   */

    /* ---- Dirty rect — tracks last blitted region (RECT format) ---------- */
    LOCO_LONG       dirty_left;          /* +0x50  RECT.left                      */
    LOCO_LONG       dirty_top;           /* +0x54  RECT.top                       */
    LOCO_LONG       dirty_right;         /* +0x58  RECT.right                     */
    LOCO_LONG       dirty_bottom;        /* +0x5c  RECT.bottom                    */

    char            _pad_60[0x28];       /* +0x60..+0x87  gap                     */

    /* ---- Visibility flags ----------------------------------------------- */
    char            is_visible;          /* +0x88  1 = panel shown                */
    char            _pad_89[0x21];       /* +0x89..+0xaa padding                  */
    char            is_alive;            /* +0xab  cleared on destroy (guard)     */
    char            _pad_ac;             /* +0xac  padding                        */
    char            visible_flag2;       /* +0xad  set by timer check             */
    char            _pad_ae[0x26];       /* +0xae..+0xd3 padding                  */

    /* ---- Viewport / clip rectangle ------------------------------------- */
    int             clip_left;           /* +0xd4  viewport left / x_scroll       */
    int             clip_top;            /* +0xd8  viewport top  / active btn ptr */
    int             clip_right;          /* +0xdc  viewport right                 */
    int             clip_bottom;         /* +0xe0  viewport bottom / aux flag     */

    char            _pad_e4[0x30c];      /* +0xe4..+0x3ef  gap to scroll sub-obj  */

    /* ---- Sub-objects ---------------------------------------------------- */
    UIScrollPos     scroll_pos;          /* +0x3f0  UIScrollPos (16 bytes)        */
                                         /*  scroll_x=-1, scroll_y=-1, w=0, h=0  */
                                         /*  ends at +0x400                       */

    char            _pad_400[0x78];      /* +0x400..+0x477  gap to hotspot list   */

    UIHotspotList   hotspot_list;        /* +0x478  dynamic button array          */
                                         /*  init FUN_0042a110 / dtor FUN_0042a370*/

    char            _pad_490[0xc];       /* +0x490..+0x49b  padding               */

    /* ---- Panel state machine -------------------------------------------- */
    uint16_t        panel_mode;          /* +0x49c  PANEL_MODE_* (uint16)         */
    char            _pad_49e[2];         /* +0x49e..+0x49f  alignment             */

    /* ---- Tab button pointers -------------------------------------------- */
    UIButton       *btn_tab1;            /* +0x4a0  tab 1 button                  */
    UIButton       *btn_tab2;            /* +0x4a4  tab 2 button                  */
    UIButton       *btn_tab3;            /* +0x4a8  tab 3 button                  */
    UIButton       *btn_tab4;            /* +0x4ac  tab 4 button                  */
    UIButton       *btn_content;         /* +0x4b0  content area button           */
    void           *_unused_4b4;         /* +0x4b4  unused slot                   */
    void           *_unused_4b8;         /* +0x4b8  unused slot                   */
    char           *label_text;          /* +0x4bc  city chooser / label string   */

    /* ---- Building-picker tile slots (BUILDING_TILE_COUNT = 6) ----------- */
    UIBuildTile    *building_tiles[BUILDING_TILE_COUNT];
                                         /* +0x4c0..+0x4d4  6 tile pointers       */
                                         /*  BUILDING_TILE_SPACING = 25 px vert   */

    /* ---- Content linked list -------------------------------------------- */
    void           *list_ptr;            /* +0x4d4  UIElement* list pointer       */
    void           *content_list_head;   /* +0x4d8  head of resource element list */
    void           *aux_list;            /* +0x4dc  UIElement* auxiliary list     */

    /* ---- Tail padding to reach aux_flag at +0x2ea ----------------------- */
    /* (Actual object may be allocated at full 0x4e0 bytes from the ctor)    */
    char            aux_flag;            /* +0x2ea  auxiliary flag (zeroed ctor)  */
};

/*===========================================================================
 * Global variable extern declarations
 * Defined in ui.c; original PE data-section addresses noted.
 *===========================================================================*/

/*
 * 0x004fd3c4  g_primary_renderer / DAT_004fd3c4
 *
 * Global primary DirectDraw surface.  Final destination for all screen blits.
 * GetBltStatus is polled on this surface in Panel_WaitBltSync.
 *
 * WIN32: IDirectDrawSurface** (COM interface double-pointer)
 *        vtable+0x14 = Blt, vtable+0x44 = GetBltStatus, vtable+0x68 = SetClipper
 * LINUX: SDL_Renderer* g_primary_renderer  (main renderer for the game window)
 *        SDL_Texture*  g_primary_texture   (optional render target)
 */
#ifdef LOCO_LINUX
extern SDL_Renderer *g_primary_renderer;   /* DAT_004fd3c4 equivalent */
extern SDL_Texture  *g_primary_texture;
#else
extern void *DAT_004fd3c4;
#endif

/*
 * 0x004fd3c0  g_secondary_texture / DAT_004fd3c0
 *
 * Global secondary DirectDraw surface (back buffer / offscreen buffer).
 * Used by Panel_DragUpdate to restore background pixels under the old cursor
 * position before drawing at the new position.
 *
 * WIN32: IDirectDrawSurface** — second COM interface object
 * LINUX: SDL_Texture* g_secondary_texture
 *        Holds a snapshot of the background region saved at drag start.
 *        Restored via SDL_RenderCopy to the old rect before drawing the sprite.
 */
#ifdef LOCO_LINUX
extern SDL_Texture *g_secondary_texture;   /* DAT_004fd3c0 equivalent */
#else
extern void *DAT_004fd3c0;
#endif

/*
 * 0x004fd3e0  g_active_panel / DAT_004fd3e0
 *
 * Pointer to the currently active/focused UIPanel object.
 * Set to `this` when a panel activates (modes 1-5).
 * Reset to &g_null_panel_sentinel (DAT_004aa5b8) when mode is set to
 * PANEL_MODE_HIDDEN (0).
 * LINUX: plain C pointer; no Win32 dependency.
 */
extern UIPanel *g_active_panel;   /* DAT_004fd3e0 */

/*
 * 0x004aad0c  g_game_timer / DAT_004aad0c
 *
 * Game timer or frame counter.
 * Read in FUN_004277d0 to compute elapsed time since panel+0x38 and set the
 * panel_visible_flag2 at panel+0xad.
 * LINUX: replace reads with SDL_GetTicks() at call sites, or maintain a
 *        global monotonic frame counter.
 */
extern int g_game_timer;   /* DAT_004aad0c */

/*
 * 0x004aa5c8  g_visibility_threshold / DAT_004aa5c8
 *
 * Visibility elapsed-time threshold constant.
 * Compared against the timer delta in the panel timer check in FUN_004277d0.
 * LINUX: plain int; no Win32 dependency.
 */
extern int g_visibility_threshold;   /* DAT_004aa5c8 */

/*
 * 0x00477cc8  g_uipanel_vtable / PTR_FUN_00477cc8
 *
 * UIPanel virtual dispatch table.
 * Assigned to UIPanel+0x00 in UIPanel_Ctor.
 * Re-assigned at the start of the destructor body before virtual cleanup.
 * LINUX: implement as a file-static UIPanelVtable struct with function pointers.
 */
extern UIPanelVtable g_uipanel_vtable;   /* PTR_FUN_00477cc8 */

/*
 * 0x004855e8  g_resource_registry / DAT_004855e8
 *
 * Resource registry / resource table.
 * Searched by UIPanel_LoadContent (FUN_00427580) via FUN_00446ea0 using
 * type IDs in the range PANEL_CONTENT_BASE_ID (0x2c00) to PANEL_CONTENT_MAX_ID.
 * LINUX: pointer into the game's internal resource system.
 *        The resource system (FUN_00446ea0) requires its own porting.
 */
extern void *g_resource_registry;   /* DAT_004855e8 */

/*===========================================================================
 * Function declarations
 *===========================================================================*/

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------
 * Panel_WndProc  /  Panel_OnExposed
 * Original address: 0x00426900
 *
 * WIN32: Panel_WndProc(self, hwnd, msg, wparam, lparam)
 *   Window procedure for the panel HWND.
 *   If hwnd == self->hwnd: acquires render mutex, calls Panel_DragUpdate(1),
 *   releases mutex.  Falls through to DefWindowProcA.
 *   Acts as WM_PAINT / WM_TIMER callback keeping the panel bitmap refreshed.
 *
 * LINUX: Panel_OnExposed(self)
 *   Called from the SDL event loop on SDL_WINDOWEVENT_EXPOSED or from the
 *   main game-loop render tick.
 *   Calls Panel_DragUpdate(1) under the render mutex, then SDL_RenderPresent.
 *   DefWindowProcA is dropped entirely.
 *
 * WIN32 APIs:  DefWindowProcA
 * LINUX equiv: SDL_RenderPresent(g_primary_renderer)
 *---------------------------------------------------------------------------*/
#ifndef LOCO_LINUX
LRESULT Panel_WndProc(UIPanel *self, HWND hwnd, UINT msg,
                       WPARAM wparam, LPARAM lparam);
#else
void Panel_OnExposed(UIPanel *self);
#endif

/*---------------------------------------------------------------------------
 * Panel_Destroy
 * Original address: 0x00426a90
 *
 * Panel shutdown sequence:
 *   1. Clears is_alive at +0xab (re-entrant destroy guard).
 *   2. Destroys the OS window (HWND at +0x08).
 *   3. If sibling_count (+0x0c) == 0 (last panel), posts application quit.
 *
 * WIN32: DestroyWindow(self->hwnd), PostQuitMessage(0)
 * LINUX: SDL_DestroyWindow(self->sdl_window)
 *        SDL_Event ev={.type=SDL_QUIT}; SDL_PushEvent(&ev)
 *---------------------------------------------------------------------------*/
void Panel_Destroy(UIPanel *self);

/*---------------------------------------------------------------------------
 * Panel_WaitBltSync
 * Original address: 0x00426b00
 *
 * Synchronisation wait for a pending DirectDraw blit to complete.
 * Polls IDirectDrawSurface::GetBltStatus (vtable+0x44) on DAT_004fd3c4.
 * Stores result at self->blit_sync_result (+0x4c).
 * Retries up to BLIT_SYNC_TIMEOUT_ITERS times with BLIT_SYNC_SLEEP_MS delay.
 * On timeout calls FUN_00463600 (fatal error log) then ExitProcess(1).
 * Returns the final status value (0 = complete).
 *
 * WIN32: Sleep, ExitProcess, IDirectDrawSurface::GetBltStatus (vtable+0x44)
 * LINUX: SDL2 blits are synchronous — this function becomes a no-op.
 *        Optionally call SDL_RenderFlush(g_primary_renderer) and return 0.
 *---------------------------------------------------------------------------*/
int Panel_WaitBltSync(UIPanel *self);

/*---------------------------------------------------------------------------
 * Panel_UpdateWrapper
 * Original address: 0x00426b70
 *
 * Thin convenience wrapper: triggers a full panel repaint by calling
 * Panel_Render with use_clipper=0, special_mode=0, dirty_rect=NULL.
 * A null dirty_rect instructs Panel_Render to repaint the entire panel bounds.
 *
 * WIN32: none (all Win32 calls are inside Panel_Render)
 * LINUX: direct delegation to Panel_Render with a null SDL_Rect*
 *---------------------------------------------------------------------------*/
void Panel_UpdateWrapper(UIPanel *self, void *unused_param);

/*---------------------------------------------------------------------------
 * Panel_Render
 * Original address: 0x00426b90
 *
 * Core panel blit and dirty-rect engine.
 *
 * Parameters:
 *   use_clipper  — non-zero: call SetClipper/SDL_RenderSetClipRect first
 *   special_mode — non-zero: alternative render path (not normal update)
 *   dirty_rect   — if non-NULL: intersect with stored dirty rect before blit;
 *                  NULL = repaint full panel bounds
 *
 * Normal path (special_mode == 0):
 *   1. Optional: IDirectDrawSurface::SetClipper (vtable+0x68) / SDL_RenderSetClipRect
 *   2. Acquire render mutex
 *   3. GetCursorPos / SDL_GetGlobalMouseState -> local coords (subtract +0x2c/+0x30)
 *   4. Clip local cursor to viewport at +0xd4..+0xe0
 *   5. Sprite strip: src_x += current_frame (+0x24) * panel_width (+0x18)
 *   6. IntersectRect / SDL_IntersectRect to skip blit if dirty_rect misses
 *   7. UnionRect / SDL_UnionRect to merge old and new dirty rects
 *   8. IDirectDrawSurface::Blt (vtable+0x14, DDBLT_WAIT) / SDL_RenderCopy
 *   9. FUN_00401280 invalidate merged rect / no-op on Linux
 *  10. Second Blt on DAT_004fd3c4 to push to primary / SDL_RenderPresent
 *  11. Update dirty rect at +0x50..+0x5c
 *  12. Release render mutex
 *
 * WIN32: GetCursorPos, IntersectRect, UnionRect,
 *        IDirectDrawSurface::Blt (vtable+0x14),
 *        IDirectDrawSurface::SetClipper (vtable+0x68)
 * LINUX: SDL_GetGlobalMouseState, SDL_IntersectRect, SDL_UnionRect,
 *        SDL_RenderCopy, SDL_RenderSetClipRect
 *---------------------------------------------------------------------------*/
void Panel_Render(UIPanel *self, int use_clipper, int special_mode,
                  const LOCO_RECT *dirty_rect);

/*---------------------------------------------------------------------------
 * Panel_DragUpdate
 * Original address: 0x00426eb0
 *
 * Drag-and-drop cursor tracking with incremental dirty-rect repaint.
 * Guards on self->drag_active (+0x44); returns immediately if not set.
 *
 * Parameters:
 *   flag — non-zero: restore background at old cursor position before drawing
 *           at the new position (background restore via DAT_004fd3c0)
 *
 * Behaviour:
 *   1. GetCursorPos / SDL_GetGlobalMouseState -> panel-local coords
 *   2. Clip to viewport at +0xd4..+0xe0
 *   3. UnionRect / SDL_UnionRect of old (+0x50..+0x5c) and new rects
 *   4. If union < DIRTY_RECT_MAX_DIM (256 px): coalesced path with
 *      DIRTY_RECT_PADDING (4 px) expansion
 *   5. If flag != 0 and last_cursor valid: restore background from
 *      DAT_004fd3c0 at old position; reset last_cursor to
 *      CURSOR_INVALID_SENTINEL (0xffffffff)
 *   6. Update dirty rect (+0x50..+0x5c) to new position
 *   7. Blit dragged bitmap via FUN_0042b050
 *   8. Push to screen via Blt on DAT_004fd3c0 / SDL_RenderCopy
 *
 * WIN32: GetCursorPos, UnionRect,
 *        IDirectDrawSurface::Blt (vtable+0x14 on DAT_004fd3c0)
 * LINUX: SDL_GetGlobalMouseState, SDL_UnionRect,
 *        SDL_RenderCopy of saved background texture to old rect;
 *        SDL_RenderCopy of dragged sprite texture to new rect
 *---------------------------------------------------------------------------*/
void Panel_DragUpdate(UIPanel *self, int flag);

/*---------------------------------------------------------------------------
 * UIPanel_Ctor
 * Original address: 0x00427370
 *
 * Constructor for UIPanel.  Called after the object is allocated on the heap.
 * Returns self.
 *
 * Initialisation sequence:
 *   1. [WIN32 only] Set up MSVC SEH frame via ExceptionList (TIB FS:[0])
 *   2. Call base-class constructor FUN_004544e0
 *   3. Initialise UIScrollPos at +0x3f0 via FUN_00405790(-1,-1,0,0)
 *   4. Initialise UIHotspotList at +0x478 via FUN_0042a110
 *   5. Set vtable = PTR_FUN_00477cc8 at +0x00
 *   6. Set type = PANEL_TYPE (0xc) at +0x04
 *   7. Zero all button pointers, tile array, list heads, flag bytes
 *   8. [WIN32 only] Tear down SEH frame
 *
 * WIN32: MSVC SEH via ExceptionList TIB field (FS:[0] write)
 * LINUX: Remove the SEH setup/teardown entirely.
 *        If exception safety is needed, use C++ RAII or try/catch.
 *        ExceptionList is a Windows TIB field with no POSIX analogue.
 *---------------------------------------------------------------------------*/
UIPanel *UIPanel_Ctor(UIPanel *self);

/*---------------------------------------------------------------------------
 * UIPanel_Dtor
 * Original address: 0x00427440
 *
 * Destructor for UIPanel.  Uses MSVC bit-0 delete-flag convention.
 *   bit 0 set   -> destruct + free heap memory (heap object)
 *   bit 0 clear -> destruct in place only (stack / embedded object)
 *
 * Destructor body (FUN_00427460):
 *   1. Walk child linked list at +0x4d8 via next-pointers at element+0x22c
 *      Call vtable[0](child, 1) on each child (destruct + free)
 *   2. Reset vtable to PTR_FUN_00477cc8
 *   3. Call vtable[6] (release/close) on self and sub-obj at +0xfc
 *   4. Call FUN_00454630 (base destructor)
 *   5. Call FUN_0042a370 (UIHotspotList dtor at +0x478)
 *   6. Call FUN_00405870 (UIScrollPos dtor at +0x3f0)
 *   7. Call FUN_004545a0 (root base destructor)
 *   If delete_flag & 1: call FUN_00465cd0 (operator delete) to free memory
 *
 * WIN32: FUN_00465cd0 wraps HeapFree / operator delete
 * LINUX: free(self) when delete_flag & 1
 *---------------------------------------------------------------------------*/
void UIPanel_Dtor(UIPanel *self, int delete_flag);

/*---------------------------------------------------------------------------
 * UIPanel_New  (synthesis helper)
 *
 * Allocates a UIPanel on the heap and calls UIPanel_Ctor.
 * Equivalent to `new UIPanel()` in the original MSVC binary.
 * Returns the constructed object, or NULL on allocation failure.
 *
 * LINUX: calloc(1, sizeof(UIPanel)) + UIPanel_Ctor
 *---------------------------------------------------------------------------*/
UIPanel *UIPanel_New(void);

/*---------------------------------------------------------------------------
 * UIPanel_Delete  (synthesis helper)
 *
 * Destructs and frees a heap-allocated UIPanel.
 * Equivalent to `delete panel` in the original MSVC binary.
 * Calls UIPanel_Dtor with delete_flag=1.
 *
 * LINUX: UIPanel_Dtor(panel, 1) -> free(panel) inside Dtor
 *---------------------------------------------------------------------------*/
void UIPanel_Delete(UIPanel *panel);

#ifdef __cplusplus
}  /* extern "C" */
#endif

/*===========================================================================
 * WIN32 -> Linux/SDL2 replacement table  (quick reference)
 *
 *  Win32 API / concept                  Linux/SDL2 replacement
 *  ------------------------------------ -----------------------------------------
 *  HWND                                 SDL_Window*
 *  DefWindowProcA                       (dropped — SDL event loop handles events)
 *  WM_PAINT / WndProc                   SDL_WINDOWEVENT_EXPOSED / render tick
 *  DestroyWindow(hwnd)                  SDL_DestroyWindow(sdl_window)
 *  PostQuitMessage(0)                   SDL_Event e={SDL_QUIT}; SDL_PushEvent(&e)
 *  GetCursorPos(&pt)                    SDL_GetGlobalMouseState(&x, &y)
 *  IDirectDrawSurface::Blt (vtbl+0x14) SDL_RenderCopy(renderer, tex, &src, &dst)
 *  IDirectDrawSurface::SetClipper       SDL_RenderSetClipRect(renderer, &rect)
 *  IDirectDrawSurface::GetBltStatus     SDL_RenderFlush(renderer)  (or no-op)
 *  IntersectRect(&r, &a, &b)            SDL_IntersectRect(&a, &b, &r)
 *  UnionRect(&r, &a, &b)               SDL_UnionRect(&a, &b, &r)
 *  Sleep(ms)                            SDL_Delay(ms)
 *  ExitProcess(1)                       SDL_Quit(); exit(1)
 *  EnterCriticalSection / DDraw Lock    SDL_LockMutex(sdl_mutex)
 *  LeaveCriticalSection / DDraw Unlock  SDL_UnlockMutex(sdl_mutex)
 *  operator delete / FUN_00465cd0       free()
 *  MSVC SEH ExceptionList (FS:[0])      (removed; use RAII or try/catch)
 *  RECT {left,top,right,bottom}         LOCO_RECT; convert -> SDL_Rect {x,y,w,h}
 *  DAT_004fd3c4 (primary surface)       g_primary_renderer (SDL_Renderer*)
 *  DAT_004fd3c0 (secondary surface)     g_secondary_texture (SDL_Texture*)
 *  DDBLT_WAIT (0x1000000)               (dropped — SDL_RenderCopy is synchronous)
 *===========================================================================*/

/*===========================================================================
 * Section 2: UI Dialog System (EditWindow, PanelA, PanelB, ButtonSprite)
 *
 * These classes form the fullscreen dialog controller and all main-menu panels.
 * They are entirely distinct from the UIPanel building-picker documented above.
 *
 * CLASS HIERARCHY
 * ---------------
 *   WindowBase  (vtable at +0x00 per subclass, shared wndproc LAB_004272f0)
 *     +0x04  HINSTANCE
 *     +0x08  HWND  own window (created via FUN_00425b70 / WindowBase_create)
 *     +0x0c  HWND  parent window
 *     +0x78  char[] window class name (used in RegisterClassA)
 *     +0xab  byte  window-created flag
 *     +0xac..+0xb8  int x, y, w, h
 *   Window style: 0x87000000 (WS_POPUP|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN)
 *
 *   EditWindow  <-- WindowBase  (vtable PTR_FUN_004779f8, ~0x230 bytes)
 *   PanelA      <-- WindowBase  (vtable PTR_FUN_004781d0,  0x1e4 bytes)
 *   PanelB      <-- WindowBase  (vtable PTR_FUN_004774d0,  0x260 bytes)
 *===========================================================================*/

/*---------------------------------------------------------------------------
 * Dialog and UI resource IDs
 *
 * Passed to ResourceManager_lookup (0x00446ea0 / RESMGR_GetResource).
 * IDs in [100, 500] are localized: a per-language string-table offset is
 * applied before calling LoadStringA to resolve the asset name.
 *--------------------------------------------------------------------------*/

/* Win32 dialog resources */
#define RES_SPLASH_DIALOG       0x0071  /* 675x450 centered splash (WinMain)  */
#define RES_APP_ICON            0x0065  /* Application icon loaded into HICON */

/* Rendering subsystem init tags */
#define RES_DDRAW_MANAGER       0x01f8  /* CDirectDrawManager (0x224 bytes)   */
#define RES_DSOUND_MANAGER      0x01f5  /* CDirectSoundManager (0x6e0 bytes)  */
#define RES_BUILDING_RENDERER   0x01f7  /* Building renderer (0x2c4 bytes)    */
#define RES_MINIFIG_RENDERER    0x01fc  /* Minifig renderer (0x1d4 bytes)     */
#define RES_ENVIRONMENT         0x01fb  /* Environment renderer (0x254 bytes) */
#define RES_VEHICLE_RENDERER    0x01fa  /* Vehicle/train renderer (0x740 bytes)*/
#define RES_SCENE_MANAGER       0x01fe  /* Scene manager (0x3078 bytes)       */
#define RES_PARTICLE_FX         0x01fd  /* Particle/FX renderer (0x1184 bytes)*/
#define RES_PANEL_A             0x01f6  /* PanelA (name-entry) init tag       */
#define RES_PANEL_B             0x01f9  /* PanelB (city-select) init tag      */

/* Main-menu button sprites (12 total; 0x40d is skipped) */
#define RES_BTN_MAINMENU_FIRST  0x0403
#define RES_BTN_MAINMENU_LAST   0x040f
#define RES_BTN_MAINMENU_SKIP   0x040d  /* deliberately skipped               */
#define RES_BTN_MAINMENU_COUNT  12

/* Main-menu layout background sprites (blitted by MainMenu_layoutSprites) */
#define RES_LAYOUT_BACKGROUND   0x0413  /* blit at (0, 0)                     */
#define RES_LAYOUT_PANEL_1      0x0444  /* blit at (0xf4, 0x1d6)              */
#define RES_LAYOUT_PANEL_2      0x0445  /* blit at (0x204, 0xf9)              */
#define RES_LAYOUT_PANEL_3      0x0446  /* blit at (0x11a, 0xf0)              */
#define RES_LAYOUT_PANEL_4      0x0443  /* blit at (0x20b, 0x2a8)             */

/* PanelA animated button sprites */
#define RES_PANEL_A_BTN_0       0x0419
#define RES_PANEL_A_BTN_1       0x041a
#define RES_PANEL_A_BTN_2       0x0417
#define RES_PANEL_A_BTN_3       0x0418
#define RES_PANEL_A_BTN_4       0x041f
#define RES_PANEL_A_BTN_5       0x0420
#define RES_PANEL_A_BTN_6       0x0421

/* PanelB named button sprites */
#define RES_PANEL_B_BTN_0       0x042a
#define RES_PANEL_B_BTN_1       0x042c
#define RES_PANEL_B_BTN_2       0x0429
#define RES_PANEL_B_BTN_3       0x042b
#define RES_PANEL_B_BTN_4       0x042f

/* PanelB city-selection sprites (9 contiguous entries) */
#define RES_PANEL_B_CITY_FIRST  0x043a
#define RES_PANEL_B_CITY_LAST   0x0442
#define RES_PANEL_B_CITY_COUNT  9

/* Child edit-control ID and max player-name length */
#define EDIT_CONTROL_ID         0x0411
#define EDIT_CONTROL_MAX_CHARS  11      /* EM_SETLIMITTEXT value              */

/* Screensaver password DLL capability check resource */
#define RES_PASSWORD_CHECK      0x5015

/*---------------------------------------------------------------------------
 * Dialog state machine values  (EditWindow::dialogState, offset +0xe8)
 *
 * States are set via EditWindow_setState (0x004208f0) / IntroMenu_animation_state.
 *
 *   0  initial / uninitialised
 *   1  hidden: PlaySoundA(NULL), ShowWindow(edit, SW_HIDE)
 *   2  deactivated: hide edit, stop PanelA; stop PanelB if prev was 4/5
 *   3  show panels: show PanelA, resolve to 4/5, show PanelB
 *   4  Panel B singleplayer: hide edit, show PanelB
 *   5  Panel B multiplayer:  hide edit, show PanelB
 *   6  full shutdown: hide panels, transition to gameplay via GameState_machine(1)
 *   7  close child dialog: restore wndproc, destroy dialog, play music.wav
 *--------------------------------------------------------------------------*/
#define UI_STATE_INITIAL        0
#define UI_STATE_HIDDEN         1
#define UI_STATE_DEACTIVATED    2
#define UI_STATE_SHOW_PANELS    3
#define UI_STATE_PANEL_B_SINGLE 4
#define UI_STATE_PANEL_B_MULTI  5
#define UI_STATE_SHUTDOWN       6
#define UI_STATE_CLOSE_CHILD    7

/*---------------------------------------------------------------------------
 * Language IDs and string-table offsets
 *
 * Stored at CResourceMgr+0x241B8 (alias: DAT_004851f4 language context).
 * Applied by ResourceManager_lookup (0x00446ea0) and LocalizedString_load
 * (0x00447330) for string IDs in [100, 500].
 *
 * Ground-truth offsets extracted from binary; language name labels from
 * ResourceManager_lookup (Batch 1).  Batch 2 uses slightly different label
 * names for the same offsets — the offsets are authoritative.
 *--------------------------------------------------------------------------*/
#define LANG_ID_ENGLISH         1
#define LANG_ID_DANISH          2
#define LANG_ID_DUTCH           4
#define LANG_ID_SPANISH         5
#define LANG_ID_FRENCH          6
#define LANG_ID_GERMAN          7
#define LANG_ID_ITALIAN         8
#define LANG_ID_PORTUGUESE      9

#define LANG_STRTBL_ENGLISH     0x6cfc
#define LANG_STRTBL_DANISH      0x652c
#define LANG_STRTBL_DUTCH       0x6338
#define LANG_STRTBL_SPANISH     0x6144
#define LANG_STRTBL_FRENCH      0x6914
#define LANG_STRTBL_GERMAN      0x6720
#define LANG_STRTBL_ITALIAN     0x6ef0
#define LANG_STRTBL_PORTUGUESE  0x6b08

#define LOCALIZED_STR_ID_FIRST  100
#define LOCALIZED_STR_ID_LAST   500

/* Localized error string IDs shown by Display_capability_check */
#define STR_DISPLAY_ERROR_A     0x007a
#define STR_DISPLAY_ERROR_B     0x007b

/*---------------------------------------------------------------------------
 * Main-menu surface and layout dimensions
 *--------------------------------------------------------------------------*/
#define MAINMENU_SURFACE_WIDTH   1280
#define MAINMENU_SURFACE_HEIGHT  1024

/*---------------------------------------------------------------------------
 * Multiplayer timing constants
 *--------------------------------------------------------------------------*/
#define MP_SLOT_COUNT_LOBBY     0x1e    /* 30 polling slots; lobby not active */
#define MP_SLOT_COUNT_ACTIVE    0x32    /* 50 polling slots; multiplayer on   */
#define MP_POLL_TIMER_MS_IDLE   500     /* idle session polling period        */
#define MP_POLL_TIMER_MS_MATCH  20      /* fast polling when session matched  */

/*---------------------------------------------------------------------------
 * ButtonSprite  --  leaf UI widget (0x24 bytes)
 * vtable: PTR_FUN_0047851c
 *   [0] destructor (flags & 1 = free heap)
 *   [1] hide / deactivate
 *   [2] show / activate
 *
 * WIN32: allocated by FUN_00465ce0; freed via vtable[0](1)
 * LINUX: malloc + free; replace vtable dispatch with function pointer table
 *--------------------------------------------------------------------------*/
typedef struct ButtonSprite {
    void    **vtable;       /* +0x00  PTR_FUN_0047851c                        */
    uint8_t   _pad[0x10];   /* +0x04..+0x13  reserved base-class fields       */
    int32_t   posX;         /* +0x14  x position on surface                   */
    int32_t   posY;         /* +0x18  y position on surface                   */
    int32_t   resourceId;   /* +0x1c  resource ID passed to ButtonSprite_ctor */
    int32_t   stateFlags;   /* +0x20  animation / visibility state            */
} ButtonSprite;             /* total: 0x24 bytes                              */

/*---------------------------------------------------------------------------
 * SpriteSlot  --  one of EditWindow's 12 main-menu button entries
 * Each entry occupies 8 bytes at EditWindow+0x1b0..+0x1e8.
 *--------------------------------------------------------------------------*/
typedef struct SpriteSlot {
    void *pResource;        /* +0x00  CResourceBase* from ResourceManager_lookup */
    void *pSurface;         /* +0x04  surface handle (from vtable[1] on resource) */
} SpriteSlot;

/*---------------------------------------------------------------------------
 * LOCOBITMAP  --  DirectDraw offscreen surface wrapper (0x20 bytes)
 * vtable: PTR_FUN_00477d28
 * Global ref count: DAT_00485254 (g_locobitmapRefCount)
 *
 * Pixel pipeline:
 *   1. rawpixelPtr: 8-bit indexed pixels (width*height bytes)
 *   2. palettePtr:  128 RGB565 entries (0x200 bytes = 0x80 uint32 slots)
 *   3. LOCOBITMAP_DDraw_upload (0x0042a3d0): create DirectDraw SYSTEMMEMORY
 *      surface and blit via LOCOBITMAP_indexed_to_16bit_blit (0x0042b9c0)
 *   4. After upload: CPU buffers freed, convertedFlag = 1
 *
 * WIN32: pDDSurface = IDirectDrawSurface* (DDSCAPS_SYSTEMMEMORY, flags=0x840)
 * LINUX: pDDSurface = SDL_Texture* (SDL_TEXTUREACCESS_STATIC, RGB565)
 *--------------------------------------------------------------------------*/
typedef struct LOCOBITMAP {
    void      **vtable;         /* +0x00  PTR_FUN_00477d28                   */
    int32_t     convertedFlag;  /* +0x04  1 = DDraw surface ready; CPU bufs  */
                                /*         freed                              */
    int32_t     width;          /* +0x08  pixel width                        */
    int32_t     height;         /* +0x0c  pixel height                       */
    uint8_t     flags0;         /* +0x10  1 = palette is valid               */
    uint8_t     flags1;         /* +0x11                                     */
    uint8_t     _pad[2];        /* +0x12  alignment                          */
    uint16_t   *palettePtr;     /* +0x14  128-entry RGB565 palette           */
    uint8_t    *rawpixelPtr;    /* +0x18  8-bit indexed pixel data           */
    void       *pDDSurface;     /* +0x1c  WIN32: IDirectDrawSurface*         */
                                /*         LINUX: SDL_Texture*               */
} LOCOBITMAP;                   /* total: 0x20 bytes                         */

/*---------------------------------------------------------------------------
 * PanelA  --  Main-menu / name-entry animated panel (0x1e4 bytes)
 * vtable: PTR_FUN_004781d0
 * Ctor:   PanelA_ctor (0x00440f20)
 * Init:   PanelA_init (0x00440fa0)
 * Window: PanelA_createWindow (0x004412f0)
 * Global: DAT_00485260 (g_pPanelA)
 *
 * Background brush: CreateSolidBrush(0xa8c4d8) light blue
 *   COLORREF 0xa8c4d8 -> RGB(0xd8, 0xc4, 0xa8)
 * LINUX: SDL_Color { R=0xD8, G=0xC4, B=0xA8, A=0xFF }
 *--------------------------------------------------------------------------*/
typedef struct PanelA {
    void      **vtable;             /* +0x000  PTR_FUN_004781d0              */
    uint8_t     _base[0x13c];       /* +0x004  WindowBase fields             */
    int32_t     languageSlotCount;  /* +0x140  default 3                     */
    void       *hIcon;              /* +0x144  HICON for resource 0x65       */
    uint8_t     _pad[0x6b];         /* +0x148..+0x1af                        */
    ButtonSprite *pSprite[7];       /* +0x1b0..+0x1c8  animated sprites      */
    uint8_t     _pad2[0x0b];        /* +0x1cc..+0x1d3                        */
    void       *hbrLightBlue;       /* +0x1d4  HBRUSH; LINUX: SDL_Color      */
} PanelA;                           /* total: 0x1e4 bytes                    */

/*---------------------------------------------------------------------------
 * PanelB  --  City / multiplayer selection panel (0x260 bytes)
 * vtable: PTR_FUN_004774d0
 * Ctor:   PanelB_ctor (0x00408aa0)
 * Init:   PanelB_init (0x00408b20)
 * Window: PanelB_createWindow (0x00408f00)
 *--------------------------------------------------------------------------*/
typedef struct PanelB {
    void      **vtable;             /* +0x000  PTR_FUN_004774d0              */
    uint8_t     _base[0x1ac];       /* +0x004  WindowBase fields             */
    int32_t     languageSlotCount;  /* +0x1b0  default 3                     */
    void       *hIcon;              /* +0x1b4  HICON for resource 0x65       */
    uint8_t     _pad[0x69];         /* +0x1b8..+0x21f                        */
    ButtonSprite *pNamedBtn[5];     /* +0x220..+0x230  5 named button ptrs   */
    uint8_t     _pad2[0x0b];        /* +0x234..+0x23b                        */
    void       *pCitySprite[9];     /* +0x23c..+0x25f  city-selection ptrs   */
} PanelB;                           /* total: 0x260 bytes                    */

/*---------------------------------------------------------------------------
 * EditWindow  --  Fullscreen dialog controller (~0x230 bytes)
 * vtable: PTR_FUN_004779f8
 * Ctor:   EditWindow_ctor    (0x004202f0)
 * Dtor:   EditWindow_dtor    (0x004203a0)
 * Cleanup:EditWindow_cleanup (0x004203c0)
 * Global: DAT_00485240 (g_pEditWindow)
 *
 * Field map (offsets from object base):
 *   +0x000  void**   vtable          PTR_FUN_004779f8
 *   +0x004..+0x0e7  WindowBase fields (HWND at +0x08, parent at +0x0c)
 *   +0x0e8  int32_t  dialogState     UI_STATE_* constants
 *   +0x0ec  int32_t  prevState
 *   +0x0f0  uint8_t  flag0
 *   +0x0f4  uint8_t  flag1           cleared on destroy
 *   +0x0f8  HICON    hAppIcon        resource 0x65
 *   +0x13c..+0x17c  8 RECT hit-rects for buttons (set by MainMenu_recalcLayout)
 *   +0x16c  int32_t  centerOffsetX   (surface_w - screen_w) / 2
 *   +0x170  int32_t  centerOffsetY   (surface_h - screen_h) / 2
 *   +0x18c  BOOL     spritesInitialized
 *   +0x1b0..+0x1e7  SpriteSlot[12]  main-menu button sprite pairs
 *   +0x1f0  void*    pMainSurface    1280x1024 DirectDraw / SDL_Texture*
 *   +0x204  HBRUSH   hbrSolid        CreateSolidBrush(0x5252e7) blue-purple
 *   +0x208  HBRUSH   hbrHatch        CreateHatchBrush(0x000a5c0a) dark olive
 *   +0x20c  HWND     hwndEdit        child edit control (ID 0x411, max 11 ch)
 *   +0x210  void*    pChildDialog    active child dialog or NULL
 *   +0x214  WNDPROC  savedWndProc    original wndproc before subclassing
 *   +0x21c  PanelA*  pPanelA         name-entry panel
 *   +0x220  PanelB*  pPanelB         city/multiplayer panel
 *
 * WIN32: Edit control subclassed to wndproc at 0x420b20.
 *        Cursor hidden via while(ShowCursor(FALSE)>=0){} loop.
 * LINUX: SDL_StartTextInput(); SDL_ShowCursor(SDL_DISABLE);
 *        PanelA/B rendered as SDL_RenderSetViewport sub-regions.
 *--------------------------------------------------------------------------*/
typedef struct EditWindow {
    void      **vtable;
    uint8_t     _base[0xe4];        /* WindowBase fields */
    int32_t     dialogState;        /* +0x0e8  UI_STATE_* */
    int32_t     prevState;          /* +0x0ec */
    uint8_t     flag0;              /* +0x0f0 */
    uint8_t     _pad0[3];
    uint8_t     flag1;              /* +0x0f4 */
    uint8_t     _pad1[3];
    void       *hAppIcon;           /* +0x0f8  HICON, resource 0x65 */
    uint8_t     _pad2[0x8f];        /* +0x0fc..+0x18b hit-rects + offsets */
    int32_t     spritesInitialized; /* +0x18c */
    uint8_t     _pad3[0x23];        /* +0x190..+0x1b2 */
    SpriteSlot  menuSprite[12];     /* +0x1b0..+0x1e7 */
    void       *pMainSurface;       /* +0x1f0  1280x1024 DDraw / SDL_Texture */
    uint8_t     _pad4[0x10];        /* +0x1f4..+0x203 */
    void       *hbrSolid;           /* +0x204  0x5252e7 blue-purple */
    void       *hbrHatch;           /* +0x208  0x000a5c0a dark olive */
    void       *hwndEdit;           /* +0x20c  HWND child edit control */
    void       *pChildDialog;       /* +0x210 */
    void       *savedWndProc;       /* +0x214  WNDPROC before subclass */
    uint8_t     _pad5[4];           /* +0x218..+0x21b */
    PanelA     *pPanelA;            /* +0x21c */
    PanelB     *pPanelB;            /* +0x220 */
    uint8_t     _pad6[0x0f];        /* +0x224..+0x22f */
} EditWindow;                       /* total: ~0x230 bytes */

/*---------------------------------------------------------------------------
 * ScreenSaverCtx  --  Screensaver dialog state block (~0x78 bytes)
 * Initialized by ScreenSaverDialog_init (0x00448040)
 * Destroyed by   ScreenSaverDialog_destroy (0x00448080)
 *
 * WIN32: hbrLightBlue = CreateSolidBrush(0xa8c4d8)
 * LINUX: no GDI; use uint32_t ARGB = 0xD8C4A8FF
 *--------------------------------------------------------------------------*/
typedef struct ScreenSaverCtx {
    void       *hwnd;               /* [0]   screensaver window handle       */
    uint32_t    posX;               /* [1]                                   */
    uint32_t    posY;               /* [2]                                   */
    uint32_t    width;              /* [3]                                   */
    uint32_t    height;             /* [4]                                   */
    uint32_t    capacity;           /* [5]   set to 0x400 in init            */
    void       *hbrLightBlue;       /* [6]   HBRUSH; LINUX: SDL_Color        */
    uint8_t     _pad[0x47];         /* [7]...[0x1d]                          */
    void       *hPasswordDll;       /* [0x1e] HMODULE: password.cpl          */
    void       *pfnGetPasswordStatus;   /* at offset +0x70 within block      */
    void       *pfnVerifyScreenSavePwd; /* at offset +0x74 within block      */
} ScreenSaverCtx;

/*---------------------------------------------------------------------------
 * SaveFileNode  --  One entry from SaveFileEnum_list (0x00448390)
 * Allocated: 0x508 bytes per file.  Singly linked list.
 * param_1 == 0 -> "*.sav" save-game files
 * param_1 == 1 -> "*.scr" screensaver layout files
 *
 * WIN32: FindFirstFileA / FindNextFileA
 * LINUX: opendir / readdir / closedir (POSIX dirent.h)
 *--------------------------------------------------------------------------*/
typedef struct SaveFileNode {
    char              filename[0x504]; /* null-terminated matched filename   */
    struct SaveFileNode *next;         /* next node; NULL = end of list      */
} SaveFileNode;                        /* total: 0x508 bytes                 */

/*---------------------------------------------------------------------------
 * Global singletons for the dialog subsystem
 *--------------------------------------------------------------------------*/
extern EditWindow *g_pEditWindow;   /* DAT_00485240 */
extern PanelA     *g_pPanelA;       /* DAT_00485260 */
extern void       *g_pConfigMgr;    /* DAT_004fd3a8  CConfigManager (0xb0 b) */
extern void       *g_pNpcMgr;       /* DAT_004fd3ac  CEventQueue (0x804 b)   */
extern void       *g_pDDrawRenderer;/* DAT_004fd378  CDirectDrawManager      */
extern void       *g_pUserProfile;  /* DAT_004aa4a8  CUserProfile (0x124 b)  */
extern int         g_screensaverMode;  /* DAT_004a9918  0=normal, 1=kiosk    */
extern int         g_seasonOverride;   /* DAT_00485230  SEASON_* values       */
extern int32_t     g_locobitmapRefCount; /* DAT_00485254                     */

/*---------------------------------------------------------------------------
 * Function declarations: EditWindow lifecycle
 *--------------------------------------------------------------------------*/

/* EditWindow_ctor (0x004202f0) — creates brushes, zeros fields, sets global */
EditWindow *EditWindow_ctor(EditWindow *self);

/* EditWindow_dtor (0x004203a0) — calls cleanup; frees heap if flags & 1 */
void EditWindow_dtor(EditWindow *self, int flags);

/* EditWindow_cleanup (0x004203c0) — destroys child dialog, frees sprites,
 * deletes GDI brushes, calls base cleanup FUN_00425910 */
void EditWindow_cleanup(EditWindow *self);

/* EditWindow_WM_INITDIALOG (0x004204d0) — creates panels, edit control */
int  EditWindow_WM_INITDIALOG(EditWindow *self);

/* EditWindow_activate (0x004206b0) — loads sprites, hides cursor, shows UI */
void EditWindow_activate(EditWindow *self);

/* EditWindow_setState (0x004208f0) — state machine; see UI_STATE_* constants */
void EditWindow_setState(EditWindow *self, int newState);

/* EditWindow_destroy (0x00420860) — hides UI, frees sprites, restores focus */
void EditWindow_destroy(EditWindow *self);

/*---------------------------------------------------------------------------
 * Function declarations: PanelA / PanelB lifecycle
 *--------------------------------------------------------------------------*/

PanelA *PanelA_ctor(PanelA *self);           /* 0x00440f20 */
void    PanelA_init(PanelA *self);           /* 0x00440fa0 */
int     PanelA_createWindow(PanelA *self);   /* 0x004412f0 */

PanelB *PanelB_ctor(PanelB *self);           /* 0x00408aa0 */
void    PanelB_init(PanelB *self);           /* 0x00408b20 */
int     PanelB_createWindow(PanelB *self);   /* 0x00408f00 */

/*---------------------------------------------------------------------------
 * Function declarations: ButtonSprite
 *--------------------------------------------------------------------------*/

/* ButtonSprite_ctor (0x00454b50) — sets vtable, resourceId, zeros state */
ButtonSprite *ButtonSprite_ctor(ButtonSprite *self, int32_t resourceId);

/*---------------------------------------------------------------------------
 * Function declarations: Main-menu sprite management
 *--------------------------------------------------------------------------*/

/* MainMenu_loadSprites (0x00421500) — loads 12 button sprites; calls layout */
void MainMenu_loadSprites(EditWindow *self);

/* MainMenu_layoutSprites (0x004216f0) — creates 1280x1024 surface; blits 5
 * background sprites at hardcoded positions */
void MainMenu_layoutSprites(EditWindow *self);

/* MainMenu_recalcLayout (0x00421200) — recomputes 8 button hit-rects with
 * centering offsets (surface size vs screen size) */
void MainMenu_recalcLayout(EditWindow *self);

/* MainMenu_freeSprites (0x00421ae0) — releases all 12 sprites + main surface */
void MainMenu_freeSprites(EditWindow *self);

/*---------------------------------------------------------------------------
 * Function declarations: Resource / window infrastructure
 *--------------------------------------------------------------------------*/

/* ResourceManager_lookup (0x00446ea0) — central UI resource factory;
 * applies per-language string-table offset for IDs in [100, 500].
 * WIN32: GetModuleHandleA + LoadStringA  LINUX: g_StringTable_Lookup */
void *ResourceManager_lookup(void *pResMgr, int resourceId);

/* WindowBase_create (0x00425b70) — registers WNDCLASS, creates fullscreen
 * WS_POPUP window, calls vtable[7] callback.
 * WIN32: RegisterClassA, CreateWindowExA  LINUX: SDL_CreateWindow */
int WindowBase_create(void *self, void *hwndParent,
                      int x, int y, int w, int h);

/* Surface_blitSprite (0x0042b960) — blits sprite widget onto DDraw surface.
 * WIN32: IDirectDrawSurface::BltFast  LINUX: SDL_RenderCopy */
void Surface_blitSprite(void *pTargetSurface, void *pSprite, int x, int y);

/*---------------------------------------------------------------------------
 * Function declarations: LOCOBITMAP
 *--------------------------------------------------------------------------*/

/* LOCOBITMAP_copy_ctor (0x0042a1c0) — deep-copies palette, pixels, surface */
LOCOBITMAP *LOCOBITMAP_copy_ctor(LOCOBITMAP *dst, const LOCOBITMAP *src);

/* LOCOBITMAP_DDraw_upload (0x0042a3d0) — lazy-converts to DirectDraw surface;
 * frees CPU buffers after upload.
 * WIN32: IDirectDraw::CreateSurface  LINUX: SDL_CreateTexture + SDL_UpdateTexture */
void LOCOBITMAP_DDraw_upload(LOCOBITMAP *self, void *pDirectDraw);

/* LOCOBITMAP_indexed_to_16bit_blit (0x0042b9c0) — pixel-by-pixel 8-bit
 * indexed -> 16-bit RGB565 conversion into locked surface */
void LOCOBITMAP_indexed_to_16bit_blit(LOCOBITMAP *self,
                                       int dstX, int dstY,
                                       void *pDstDesc,
                                       int stride,
                                       int regionW, int regionH,
                                       int srcX, int srcY);

/*---------------------------------------------------------------------------
 * Function declarations: Multiplayer lobby
 *--------------------------------------------------------------------------*/

/* Lobby_create (0x00422820) — allocates SessionScheduler + NetworkManager;
 * starts event dispatch thread.  No-op if already created. */
void Lobby_create(void *pDDrawRenderer);

/* MultiplayerLobby_show (0x00448350) — sets mp flag, creates lobby, starts
 * session polling, raises thread priority, initializes audio */
void MultiplayerLobby_show(void);

/* LobbyState_machine (0x0043d2b0) — controls lobby polling state.
 * State 1: 500ms timer.  State 2: scan sessions, 20ms timer, post msg. */
void LobbyState_machine(void *pNpcMgr, int state);

/*---------------------------------------------------------------------------
 * Function declarations: Audio and screensaver
 *--------------------------------------------------------------------------*/

/* MainMenu_showAfterSetup (0x004480c0) — password DLL check; plays music.wav
 * WIN32: PlaySoundA(path, NULL, SND_ASYNC|SND_FILENAME)
 * LINUX: Mix_LoadMUS + Mix_PlayMusic */
int MainMenu_showAfterSetup(int *pSetupPhaseCounter);

/* Audio_init (0x0045b7e0) — allocates audio manager, opens device, reads
 * [Sound] volume levels from INI (defaults 75/75/78)
 * LINUX: Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) */
int Audio_init(void);

/* ScreenSaverPassword_loadDLL (0x004487f0) — Windows-only; loads password.cpl.
 * LINUX: no-op */
void ScreenSaverPassword_loadDLL(ScreenSaverCtx *ctx);

/* ScreenSaverDialog_init (0x00448040) — zeros context, creates light-blue brush */
void *ScreenSaverDialog_init(ScreenSaverCtx *ctx);

/* ScreenSaverDialog_destroy (0x00448080) — DeleteObject, DestroyWindow, FreeLibrary */
void ScreenSaverDialog_destroy(ScreenSaverCtx *ctx);

/* ScreenSaverTimer_tick (0x00448120) — cycles 3 screensaver slots every 2047 ticks */
void ScreenSaverTimer_tick(void *pMsgParam, int tickType);

/* SaveFileEnum_list (0x00448390) — enumerates *.sav or *.scr files into linked list
 * WIN32: FindFirstFileA  LINUX: opendir/readdir */
SaveFileNode *SaveFileEnum_list(int fileType);

/* ScreenSaver_layout_picker (0x004481b0) — reads INI [ScreenSaver]/Random;
 * picks a random or named layout file into outBuf as 'ScrSaver\[name]' */
void ScreenSaver_layout_picker(char *outBuf);

/* LocalizedString_load (0x00447330) — LoadStringA with per-language offset
 * LINUX: g_StringTable_Lookup with LANG_STRTBL_* offset */
void LocalizedString_load(void *pResMgr, unsigned int baseId,
                           char *outBuf, int bufLen);

/* GameState_machine (0x00408130) — global engine state machine.
 * WIN32: PostMessageA on state 10  LINUX: SDL_PushEvent(SDL_QUIT) */
void GameState_machine(int newState);

/*===========================================================================
 * STARTUP SEQUENCE  (FUN_00462e90 = WinMain, 0x00462e90)
 *
 *  1. CreateDialogParamA(resource 0x71)   — 675x450 centered splash dialog
 *  2. InstallPath_and_INI_init (0x4068d0) — registry + lego.ini; resource paths
 *  3. CmdLine_parse (0x406790)            — holiday themes; screensaver flag
 *  4. MainMenu_showAfterSetup (0x4480c0)  — password.cpl check; plays music.wav
 *  5. Window_and_INI_setup (0x406480)     — screen dims; [BALANCING] FPS limits
 *  6. Display_capability_check (0x406680) — palette/depth/resolution guards
 *  7. FindWindowA("LEGO LOCO")            — single-instance guard
 *  8. GameSubsystems_init (0x406ba0)      — all subsystems + timer
 *  9. if g_screensaverMode == 0:
 *         MultiplayerLobby_show() -> GameState_machine(2)   [normal menu]
 *     else:
 *         GameState_machine(1)                               [screensaver]
 * 10. ShowWindow, destroy splash, Win32 message loop (28ms multimedia timer)
 *
 * INI KEY SECTIONS  (lego.ini)
 * ----------------------------
 *   [DIRECTORIES]   ResFile (resource base), Remote (remote base)
 *   [WINDOW_ATTRIBUTES]  RectLeft/Top/Right/Bottom (clamped)
 *   [BALANCING]    MinVehicleFPS(20), MinBuildingFPS(18), MinMinifigFPS(16),
 *                  MinFlyingFPS(14)
 *   [PROCESS]      CleanExit (read then cleared to detect crashes)
 *   [MOUSE]        Setting1/2/3 (mouse button mappings)
 *   [Sound]        VolumeLow(75), VolumeMed(75), VolumeHigh(78)
 *   [ScreenSaver]  Sound(0=play), Layout(filename), Random(1=pick random)
 *   [LoadEvents]   000,001,... scripted load events
 *   [TimeEvents]   000,001,... scripted time events
 *   [EasterEggs]   1,2,3,4 entity IDs with +0x163 easter-egg flag
 *
 * RENDERING SUBSYSTEM ALLOCATION ORDER  (0x00406f90)
 * ---------------------------------------------------
 *  1. DAT_004fd378  FUN_004202f0  0x224 b  tag 0x1f8  CDirectDrawManager
 *     NOTE: address 0x004202f0 also appears as EditWindow_ctor (Batch 1).
 *     Size mismatch (0x224 vs ~0x230) and different resource IDs suggest
 *     distinct classes; address collision may be Ghidra analysis artefact.
 *  2. DAT_004fd37c  FUN_0042e900  0x6e0 b  tag 0x1f5  CDirectSoundManager
 *  3. DAT_004fd388  FUN_00430a90  0x2c4 b  tag 0x1f7  Building renderer
 *  4. DAT_00485258  FUN_00436b20  0x1d4 b  tag 0x1fc  Minifig renderer
 *  5. DAT_004fd384  FUN_00401f50  0x254 b  tag 0x1fb  Environment renderer
 *  6. DAT_004fd380  FUN_00415980  0x740 b  tag 0x1fa  Vehicle/train renderer
 *  7. DAT_004fd38c  FUN_0044f490  0x3078 b tag 0x1fe  Scene manager
 *  8. DAT_004fd390  FUN_0040f1c0  0x1184 b tag 0x1fd  Particle/FX renderer
 *===========================================================================*/

#endif /* UIPANEL_H */
