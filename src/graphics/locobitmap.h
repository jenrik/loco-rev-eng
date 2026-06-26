/*
 * Lego Loco (1998) - Decompiled and documented for Linux port
 * Subsystem: LOCOBITMAP (IDirectDrawSurface wrapper)
 * WIN32 -> LINUX: IDirectDrawSurface -> SDL_Surface or SDL_Texture
 */

#ifndef LOCOBITMAP_H
#define LOCOBITMAP_H

/* ── Platform detection ───────────────────────────────────────────────── */
#ifdef __linux__
#  ifndef LOCO_LINUX
#    define LOCO_LINUX 1
#  endif
#endif

#include <stdint.h>
#include <stddef.h>

#ifdef LOCO_LINUX
#  include <SDL2/SDL.h>
#  include <unistd.h>
#  include <stdio.h>
#  include <stdlib.h>
#  include <string.h>
#  include <errno.h>
#  include <ctype.h>
#else
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <ddraw.h>
#endif

/* ── Platform type aliases ────────────────────────────────────────────── */
#ifdef LOCO_LINUX
typedef SDL_Renderer   LocoRenderer;
typedef SDL_Surface    LocoSurface;
typedef SDL_Texture    LocoTexture;
typedef SDL_Window     LocoWindow;
typedef struct LocoRect { int left, top, right, bottom; } LocoRect;
typedef int            LocoResult;
#define LOCO_OK        0
#define LOCO_FAIL      (-1)
#else
typedef IDirectDraw4         LocoRenderer;
typedef IDirectDrawSurface   LocoSurface;
typedef IDirectDrawSurface   LocoTexture;
typedef HWND_                LocoWindow;
typedef RECT                 LocoRect;
typedef HRESULT              LocoResult;
#define LOCO_OK              S_OK
#define LOCO_FAIL            E_FAIL
#endif

/* ═══════════════════════════════════════════════════════════════════════
 * Constants from binary analysis
 * ═══════════════════════════════════════════════════════════════════════ */

/*
 * DDSURFACEDESC2 size written to dwSize before every GetSurfaceDesc /
 * Lock / CreateSurface call (DirectDraw ABI requirement).
 * LINUX: not needed; SDL_Surface exposes ->w/->h/->pitch directly.
 */
#define DDSURFACEDESC2_SIZE             0x7c    /* 124 bytes */

/*
 * GDI BITMAP struct size for GetObjectA queries.
 * LINUX: no equivalent; SDL_Surface->w/h are direct fields.
 */
#define BITMAP_STRUCT_SIZE              0x18    /* 24 bytes */

/*
 * DDSURFACEDESC2.dwFlags for surface creation:
 *   DDSD_CAPS(1) | DDSD_HEIGHT(2) | DDSD_WIDTH(4) = 7
 */
#define DDSD_CAPS_HEIGHT_WIDTH          0x07

/*
 * DDSCAPS combinations for offscreen surface allocation.
 *   0x4040 = DDSCAPS_VIDEOMEMORY | DDSCAPS_OFFSCREENPLAIN  (GPU memory)
 *   0x0840 = DDSCAPS_SYSTEMMEMORY | DDSCAPS_OFFSCREENPLAIN (CPU memory)
 * LINUX: SDL_TEXTUREACCESS_STATIC (video) vs SDL_Surface (system).
 */
#define DDSCAPS_OFFSCREENPLAIN_VIDEOMEM 0x4040u
#define DDSCAPS_OFFSCREENPLAIN_SYSMEM   0x0840u

/*
 * Win32 StretchBlt raster operation: SRCCOPY (direct pixel copy).
 * LINUX: SDL_BlitScaled with default blend mode.
 */
#define SRCCOPY_ROP                     0x00CC0020u

/*
 * DirectDraw surface-lost HRESULT observed in loco.exe.
 * Triggers IDirectDrawSurface::Restore() + blit retry.
 * LINUX: handle SDL_RENDER_TARGETS_RESET / SDL_RENDER_DEVICE_RESET events.
 */
#define DDERR_SURFACELOST_APPROX        0x88760042u

/*
 * Async/wait blit flag for the secondary Blt call in BlitToScreen.
 * LINUX: SDL_RenderFlush() if explicit GPU synchronisation is needed.
 */
#define DDBLT_ASYNC_FLAG                0x01000000u

/*
 * IDirectDrawSurface vtable byte offsets (IDirectDrawSurface1 layout).
 * byte offset / 4 = vtable index.
 *
 *   Offset  Idx  Method
 *   0x14     5   Blt                  <- LOCOBITMAP_BlitToScreen
 *   0x44    17   GetDC                <- LOCOBITMAP_BlitHBitmapToSurface
 *   0x58    22   GetSurfaceDesc       <- GetSurfaceDesc, BlitHBitmapToSurface
 *   0x64    25   Lock                 <- LOCOBITMAP_DarkenRect
 *   0x68    26   ReleaseDC            <- LOCOBITMAP_BlitHBitmapToSurface
 *   0x6c    27   Restore              <- BlitHBitmapToSurface, BlitToScreen
 *   0x80    32   Unlock               <- LOCOBITMAP_DarkenRect
 *
 * IDirectDraw4:
 *   0x18     6   CreateSurface        <- LOCOBITMAP_LoadFromFile
 */
#define VTBL_IDS_BLT            (0x14 / 4)
#define VTBL_IDS_GETDC          (0x44 / 4)
#define VTBL_IDS_GETSURFACEDESC (0x58 / 4)
#define VTBL_IDS_LOCK           (0x64 / 4)
#define VTBL_IDS_RELEASEDC      (0x68 / 4)
#define VTBL_IDS_RESTORE        (0x6c / 4)
#define VTBL_IDS_UNLOCK         (0x80 / 4)
#define VTBL_IDD4_CREATESURFACE (0x18 / 4)

/*
 * Per-channel isolation mask for 16-bit pixel right-shift darkening.
 * Stored at DAT_00485280 in the original binary.
 *
 * RGB565 layout:  RRRRR GGGGGG BBBBB  (bits 15-11, 10-5, 4-0)
 * After >> 1 without masking, carry bits bleed across channel boundaries.
 * 0x7BEF clears bits 15, 10, and 4:
 *   (pixel >> 1) & 0x7BEF  ->  each RGB channel halved independently.
 *
 * For RGB555: equivalent mask is 0x3DEF.
 * LINUX: SDL_SetTextureColorMod(tex, 128, 128, 128) for GPU-side darkening.
 */
#define PIXEL_CHANNEL_MASK_RGB565       0x7BEFu
#define PIXEL_CHANNEL_MASK_RGB555       0x3DEFu

/*
 * Sentinel written by constructors to mean "uninitialised / empty".
 * Written to LOCOBITMAP::width by LOCOBITMAP_Construct.
 * Written to AlbIndex::currentSection by AlbIndex_FlushPage on reset.
 */
#define LOCOBITMAP_UNINIT               (-1)

/*
 * Byte size of each frame / index record: 6 x uint32_t = 24 bytes.
 * Shared by LOCOBITMAP frame-record arrays and AlbIndex entry buffers;
 * both call LOCOBITMAP_InsertFrameRecord (FUN_00401690) for insertion.
 */
#define FRAME_RECORD_SIZE               0x18    /* 24 bytes */

/*
 * AlbIndex letter-range to section-index mapping.
 * First character of the uppercased name field (at param1+0x25) selects
 * the section in InsertSorted and RemoveByID.
 */
#define ALBINDEX_SECTION_AC             0   /* A, B, C */
#define ALBINDEX_SECTION_DF             1   /* D, E, F */
#define ALBINDEX_SECTION_GJ             2   /* G, H, I, J */
#define ALBINDEX_SECTION_KM             3   /* K, L, M */
#define ALBINDEX_SECTION_NQ             4   /* N, O, P, Q */
#define ALBINDEX_SECTION_RT             5   /* R, S, T */
#define ALBINDEX_SECTION_UW             6   /* U, V, W */
#define ALBINDEX_SECTION_XZ             7   /* X, Y, Z */
#define ALBINDEX_SECTION_OTHER          8   /* non-alpha */
#define ALBINDEX_SECTION_COUNT          9
#define ALBINDEX_SECTION_NONE           (-1)

/* Byte offset of the ID field within a 24-byte AlbIndex entry. */
#define ALBINDEX_ENTRY_ID_OFFSET        0x14

/*
 * LocoBitmapChain slot-ID constants, dispatched by SetSlotState /
 * HitTest / DrawSlot.
 */
#define LBC_SLOTID_1                    1
#define LBC_SLOTID_2                    2
#define LBC_SLOTID_3                    3
#define LBC_SLOTID_4                    4
#define LBC_SLOTID_LEFT_ARROW           5
#define LBC_SLOTID_RIGHT_ARROW          6
#define LBC_SLOTID_TRACK                7   /* frame driven by trackPageIndex  */
#define LBC_SLOTID_THUMBNAIL            8   /* thumbnail grid zone             */
#define LBC_SLOTID_9                    9
#define LBC_SLOTID_TEXT_RECT            10  /* text-rect grid zone             */

/* LocoBitmapChain array dimensions */
#define LBC_FIXED_SLOT_COUNT            8
#define LBC_THUMBNAIL_COUNT             6
#define LBC_LABELBG_COUNT               6
#define LBC_TEXTRECT_COUNT              6
#define LBC_SCROLLDEST_COUNT            9
#define LBC_LABEL_COUNT                 6
#define LBC_LABEL_BUF_SIZE              0x14   /* 20 chars per text label      */
#define LBC_ENABLE_FLAG_COUNT           6

/* DrawText flags for RefreshGrid labels (Win32 DT_SINGLELINE|DT_CENTER|DT_VCENTER). */
#define LBC_DRAWTEXT_FLAGS              0x25

/* Window icon resource ID used by CreateWindow. */
#define LBC_WINDOW_ICON_ID              0x65

/* Draw-resource token passed to FUN_00447930 before each DrawSlot call. */
#define LBC_DRAW_RESOURCE_TOKEN         0x5015

/* Milliseconds to sleep between arrow un-draw and redraw in WndProc. */
#define LBC_ARROW_BLINK_MS              150

/* ═══════════════════════════════════════════════════════════════════════
 * Struct: LOCOBITMAP                                  vtable 0x004773e8
 * ═══════════════════════════════════════════════════════════════════════
 * Core LOCOBITMAP object.  Wraps an IDirectDrawSurface (accessed through
 * vtable dispatch on the embedded sub-object) and maintains an expandable
 * array of 24-byte animation frame records.
 *
 * Field layout (each field 4 bytes, 32-bit binary):
 *
 *   +0x00  void**  vtable       -> PTR_FUN_004773e8
 *
 *   +0x04  int     width        -1 = uninitialised (LOCOBITMAP_UNINIT).
 *                               When the object is used as a UI slot in
 *                               LocoBitmapChain, the four int fields at
 *                               +0x04..+0x10 are reinterpreted as a screen-
 *                               space RECT (left/top/right/bottom).  The
 *                               constructor sets all four to -1 as an
 *                               "uninitialised" sentinel regardless of
 *                               semantic use.
 *
 *   +0x08  int     frameBufPtr  Heap address of the frame-record array
 *                               (0 = no records allocated).
 *                               Also RECT.top in UI-slot context.
 *
 *   +0x0c  int     capacity     Byte capacity of the frame-record buffer.
 *                               Entry count = capacity / FRAME_RECORD_SIZE.
 *                               Also RECT.right in UI-slot context.
 *
 *   +0x10  int     frameCount   Next insertion index; updated by
 *                               LOCOBITMAP_InsertFrameRecord.
 *                               Also RECT.bottom in UI-slot context.
 *
 *   +0x14  int     savedCount   Snapshot of frameCount before the last
 *                               insert (for undo / rollback purposes).
 *
 * The IDirectDrawSurface* is held in a sub-object reached via vtable
 * dispatch; it is not a direct field in this struct.
 *
 * Naming note: earlier Ghidra analysis of this binary named this class
 * "LOCOAnimIndex" (treating the surface wrapper separately as "LOCOBitmap"
 * with vtable 0x004774c4).  The current analysis unifies them.
 *
 * Minimum object size: 0x18 bytes plus the vtable-dispatched surface.
 *
 * WIN32: IDirectDrawSurface* managed via COM vtable dispatch.
 * LINUX: Replace with SDL_Texture* + SDL_Rect per slot.
 */
typedef struct LOCOBITMAP {
    void  **vtable;     /* +0x00  -> PTR_FUN_004773e8                        */
    int     width;      /* +0x04  -1 = uninit; RECT.left in UI-slot context  */
    int     frameBufPtr;/* +0x08  heap ptr to frame-record array             */
                        /*        RECT.top in UI-slot context                */
    int     capacity;   /* +0x0c  byte capacity of frame-record buffer       */
                        /*        RECT.right in UI-slot context              */
    int     frameCount; /* +0x10  next insertion index                       */
                        /*        RECT.bottom in UI-slot context             */
    int     savedCount; /* +0x14  snapshot of frameCount before last insert  */
} LOCOBITMAP;

/*
 * FrameRecord - one 24-byte entry in a LOCOBITMAP frame-record array.
 * Exact field semantics require cross-referencing animation callers.
 * Likely encodes a sprite-frame descriptor (source rect + metadata).
 */
typedef struct FrameRecord {
    uint32_t  frameId;  /* [+0x00] frame identifier or sprite-sheet index    */
    uint32_t  x;        /* [+0x04] left / x position                        */
    uint32_t  y;        /* [+0x08] top  / y position                        */
    uint32_t  w;        /* [+0x0c] width  (or right coord)                  */
    uint32_t  h;        /* [+0x10] height (or bottom coord)                 */
    uint32_t  flags;    /* [+0x14] flags, byte offset, or extra metadata    */
} FrameRecord;

/* ═══════════════════════════════════════════════════════════════════════
 * Struct: AlbIndex
 * ═══════════════════════════════════════════════════════════════════════
 * In-memory index for the PostBag album subsystem.  Partitions album
 * entries into nine letter-range sections and demand-loads each section
 * from disk.
 *
 * Disk path format:
 *   <DataPath>\PostBag\AlbIndex\<album%03d>\<section%04d>.ind
 *
 * Only one section is resident in memory at a time.  Switching sections
 * first calls AlbIndex_FlushPage (write + evict) then AlbIndex_LoadPage
 * (read new).  Each index entry is 24 bytes (6 x DWORD); the ID field
 * that uniquely identifies the entry is at byte offset ALBINDEX_ENTRY_ID_OFFSET
 * (= 0x14) within each entry.
 *
 * AlbIndex shares the FRAME_RECORD_SIZE = 24 record format and buffer
 * management helpers (LOCOBITMAP_InsertFrameRecord / AlbIndex_RemoveAt)
 * with the LOCOBITMAP frame-record subsystem.
 *
 *   +0x00  void**    vtable
 *   +0x04  int       currentSection   -1 (ALBINDEX_SECTION_NONE) when empty
 *   +0x08  uint8_t*  buffer           heap buffer for active section data
 *   +0x0c  uint32_t  bufferBytes      byte count (entries x FRAME_RECORD_SIZE)
 *   +0x10  int       albumNum         album number for .ind path building
 *
 * WIN32: file I/O via CreateFileA / ReadFile / WriteFile.
 * LINUX: replace with fopen/fread/fwrite or open/read/write.
 */
typedef struct AlbIndex {
    void     **vtable;          /* +0x00                                     */
    int        currentSection;  /* +0x04  -1 = no page loaded               */
    uint8_t   *buffer;          /* +0x08  heap buffer for active section     */
    uint32_t   bufferBytes;     /* +0x0c  byte count (entries x 0x18)       */
    int        albumNum;        /* +0x10  album number for .ind path         */
} AlbIndex;

/* ═══════════════════════════════════════════════════════════════════════
 * Struct: LocoBitmapChain                             vtable 0x004773f0
 * ═══════════════════════════════════════════════════════════════════════
 * Full-screen DirectDraw-backed 2-D carousel UI.  Manages a scrollable
 * grid of world/train item thumbnails with label overlays and keyboard-
 * driven navigation.
 *
 * Fixed slot array (slots[0..7]):
 *   Index  Slot-ID  Description
 *     [0]    1      generic UI element
 *     [1]    2      generic UI element
 *     [2]    3      generic UI element
 *     [3]    4      generic UI element
 *     [4]    9      generic UI element
 *     [5]    5      left-arrow navigation button
 *     [6]    6      right-arrow navigation button
 *     [7]    7      track/carousel slot; frame index = trackPageIndex
 *
 * enableFlags[] layout:
 *   [0] prev-page      [1] next-page
 *   [2] unknown        [3] unknown
 *   [4] prev-page-alt  [5] next-page-alt
 *
 * Total object size: ~0x252 bytes.
 *
 * WIN32: IDirectDrawSurface blits; WM_KEYDOWN message loop.
 * LINUX: SDL_RenderCopy; SDL_PollEvent + SDL_KEYDOWN.
 */
typedef struct LocoBitmapChain {
    /* base / identity (+0x000 to +0x00b) */
    void      **vtable;                     /* +0x000  PTR_FUN_004773f0      */
    HINSTANCE   hInst;                      /* +0x004                        */
    HWND        hParent;                    /* +0x008  for blit-to-screen    */

    uint8_t     _pad_0x00c[0xc8];          /* +0x00c  base-class / unknown  */

    /* blit geometry (+0x0d4 to +0x0f3) */
    int         srcOffsetX;    /* +0x0d4  added to src RECT before blit     */
    int         srcOffsetY;    /* +0x0d8                                     */
    uint8_t     _pad_0x0dc[0x08]; /* +0x0dc  unknown                        */
    BOOL        drawActive;    /* +0x0e4  FlushDrawList early-exit guard     */
    HICON       hIcon;         /* +0x0e8                                     */
    int         dstOffsetX;    /* +0x0ec  added to dst RECT before blit     */
    int         dstOffsetY;    /* +0x0f0                                     */
    uint8_t     _pad_0x0f4[0x1c]; /* +0x0f4  unknown                        */

    /* control flags (+0x110 to +0x112) */
    uint8_t     shuttingDown;  /* +0x110  WndProc early-exit guard           */
    uint8_t     initialized;   /* +0x111  draw-data resident flag            */
    uint8_t     drawListActive;/* +0x112  blit pass currently active         */
    uint8_t     _pad_0x113;

    /* scroll state (+0x114 to +0x11f) */
    int         currentCol;    /* +0x114  leftmost visible column index      */
    int         currentRow;    /* +0x118  current row index                  */
    int         pageSize;      /* +0x11c  number of visible columns          */

    /* hit-test result (+0x120 to +0x123) */
    int         hitIndex;      /* +0x120  item index set by HitTest          */

    uint8_t     _pad_0x124[0x04]; /* +0x124  unknown                        */

    /* track / text (+0x128 to +0x133) */
    int         trackPageIndex;/* +0x128  frame index for track slot (ID 7)  */
    BOOL        textFlag;      /* +0x12c  enables DrawTextA label rendering  */

    uint8_t     _pad_0x130[0x04]; /* +0x130  unknown                        */

    /* display mode (+0x134 to +0x147) */
    int         resolutionFlag;/* +0x134  1 if desktop >= 801x601           */

    uint8_t     _pad_0x138[0x04]; /* +0x138  unknown                        */

    IDirectDrawSurface *pBlitSurface; /* +0x13c  source for AW blits        */
    void               *pDDObject;   /* +0x140  released via vtable[2]      */

    uint8_t     _pad_0x144[0x04]; /* +0x144  alignment padding              */

    /*
     * LOCOBITMAP slot arrays (+0x148 to +0x1d3).
     * Each LOCOBITMAP's four int fields (width/frameBufPtr/capacity/frameCount)
     * are used as a screen-space RECT (left/top/right/bottom) for blit
     * positioning.  Access via the LBC_SLOT_* macros below.
     *
     * LINUX: replace with SDL_Texture* + SDL_Rect pairs per slot.
     */
    LOCOBITMAP *slots[LBC_FIXED_SLOT_COUNT];         /* +0x148 to +0x164    */
    LOCOBITMAP *thumbnails[LBC_THUMBNAIL_COUNT];     /* +0x168 to +0x17c    */
    LOCOBITMAP *labelBg[LBC_LABELBG_COUNT];          /* +0x180 to +0x194    */
    LOCOBITMAP *textRects[LBC_TEXTRECT_COUNT];       /* +0x198 to +0x1ac    */
    LOCOBITMAP *scrollDest[LBC_SCROLLDEST_COUNT];    /* +0x1b0 to +0x1d0    */

    /* enable flags (+0x1d4 to +0x1d9) */
    uint8_t     enableFlags[LBC_ENABLE_FLAG_COUNT];  /* +0x1d4               */

    /*
     * Text label buffers (+0x1da to +0x251).
     * Six 20-char NUL-terminated strings, one per visible grid column.
     * Populated from world-data name fields (data+0x25) by DrawItem.
     * Rendered with DrawTextA / SDL_ttf by RefreshGrid.
     */
    char        labels[LBC_LABEL_COUNT][LBC_LABEL_BUF_SIZE]; /* +0x1da       */
} LocoBitmapChain;

/* ═══════════════════════════════════════════════════════════════════════
 * Extern globals
 * ═══════════════════════════════════════════════════════════════════════ */
#ifdef LOCO_LINUX
extern SDL_Renderer  *g_pRenderer;       /* replaces IDirectDraw* 0x00485440  */
extern SDL_Texture   *g_pPrimaryTexture; /* replaces g_pPrimarySurface 0x004fd3c0 */
extern SDL_Surface   *g_pBackSurface;    /* CPU back-buffer                   */
extern SDL_Texture   *g_pBackTexture;    /* GPU back-buffer 0x004fd3c4        */
#else
extern void          *g_pDirectDraw;     /* IDirectDraw*         0x00485440   */
extern void          *g_pPrimarySurface; /* IDirectDrawSurface*  0x004fd3c0   */
extern void          *g_pBackBuffer;     /* IDirectDrawSurface*  0x004fd3c4   */
#endif

/* Pointer to locked back-buffer pixel data (uint16_t*, 16-bit RGB565). */
extern uint16_t      *g_pSurfaceBits;    /* DAT_004fd1c0                      */

/* Surface byte stride per row (DDSURFACEDESC2.lPitch or SDL_Surface->pitch). */
extern uint32_t       g_nSurfacePitch;   /* DAT_004fd1ac                      */

/* 1 = back-buffer is currently locked (prevents redundant Lock calls). */
extern char           g_bSurfaceLocked;  /* DAT_004fd218                      */

/* Per-channel boundary mask; 0x7BEF (RGB565) or 0x3DEF (RGB555). */
extern uint16_t       g_nPixelChannelMask; /* DAT_00485280                    */

/* Font handle used by LocoBitmapChain_RefreshGrid for DrawTextA. */
extern HANDLE         g_labelFont;       /* DAT_004855f4                      */

/*
 * Application context struct (partial layout).
 *   +0x0c  HMODULE / void*  hModule      for LoadImageA on Win32
 *   +0x18  int              albumNumber  for PostBag .ind path building
 */
typedef struct AppContext {
    uint8_t   _pad0[0x0c];
#ifdef LOCO_LINUX
    void     *hModule;
#else
    HMODULE   hModule;
#endif
    uint8_t   _pad1[0x08];
    int       albumNumber;
} AppContext;

extern AppContext     *g_pAppInstance;   /* DAT_004aa4a0                      */

/*
 * Dirty-rectangle tracking state (partial layout).
 * Passed to FUN_00455840 after each successful blit in BlitToScreen.
 * Fields used: +0x00, +0x0c (x), +0x10 (y), +0x14 (w), +0x18 (h).
 */
typedef struct DirtyRectState {
    int       field_0x00;
    uint8_t   _pad[0x0c];
    int       x;           /* +0x0c */
    int       y;           /* +0x10 */
    int       w;           /* +0x14 */
    int       h;           /* +0x18 */
} DirtyRectState;

extern DirtyRectState *g_pDirtyRectState; /* DAT_004aad08                    */

/* ═══════════════════════════════════════════════════════════════════════
 * Batch 1 - LOCOBITMAP core functions          0x00401000 to 0x004016ff
 * ═══════════════════════════════════════════════════════════════════════ */

/*
 * LOCOBITMAP_LoadFromFile                                      0x00401000
 *
 * Load a BMP from disk via LoadImageA, create an IDirectDraw offscreen
 * surface (video or system memory), blit the bitmap onto it, and return
 * the owning LOCOBITMAP object.  Tries video memory (DDSCAPS 0x4040)
 * when useVideoMem is 1; falls back to system memory (0x0840) on failure
 * and logs the error via FUN_0045bbc0.
 *
 * WIN32: GetFileAttributesA, LoadImageA, GetObjectA, DeleteObject,
 *        OutputDebugStringA; IDirectDraw::CreateSurface (vtable 0x18).
 * LINUX: access(F_OK) + SDL_LoadBMP + SDL_SetColorKey +
 *        SDL_CreateTextureFromSurface / SDL_Surface.
 */
void *LOCOBITMAP_LoadFromFile(void *param_1, const char *filePath,
                               int desiredWidth, int desiredHeight,
                               int useVideoMem);

/*
 * LOCOBITMAP_BlitHBitmapToSurface                             0x00401170
 *
 * Copy an HBITMAP into a DirectDraw surface using a temporary GDI memory
 * DC and StretchBlt, bracketed by IDirectDrawSurface::GetDC/ReleaseDC.
 * Restores a lost surface first.  Returns the HRESULT from GetDC (0=OK).
 *
 * WIN32: CreateCompatibleDC, SelectObject, GetObjectA, StretchBlt, DeleteDC,
 *        OutputDebugStringA; IDirectDrawSurface::Restore (vtable 0x6c),
 *        GetSurfaceDesc (0x58), GetDC (0x44), ReleaseDC (0x68).
 * LINUX: SDL_BlitScaled(srcSurface, NULL, dstSurface, &dstRect).
 */
int LOCOBITMAP_BlitHBitmapToSurface(void *pSurface, void *hBitmap);

/*
 * LOCOBITMAP_BlitToScreen                                     0x00401280
 *
 * Blit a scroll-adjusted, window-clipped rectangle from the offscreen
 * back-buffer to the primary (screen) surface, recovering transparently
 * from DDERR_SURFACELOST (0x88760042) by calling Restore and retrying.
 *
 * WIN32: IsRectEmpty, OffsetRect, ClientToScreen, GetWindowRect,
 *        IntersectRect; IDirectDrawSurface::Blt (0x14), Restore (0x6c).
 * LINUX: SDL_RenderCopy(renderer, backTexture, &srcRect, &dstRect).
 */
void LOCOBITMAP_BlitToScreen(LocoRect *pSrcRect, void *hWnd,
                              int *pScrollOrigin, int bAsync);

/*
 * LOCOBITMAP_GetSurfaceDesc                                   0x004014e0
 *
 * Call IDirectDrawSurface::GetSurfaceDesc on a surface and return the
 * pixel dimensions (width/height) to the caller via out-pointers.
 * Sets DDSURFACEDESC2.dwSize = 0x7c before the call.
 *
 * WIN32: IDirectDrawSurface::GetSurfaceDesc (vtable 0x58).
 * LINUX: SDL_QueryTexture / SDL_Surface->w, ->h.
 */
void LOCOBITMAP_GetSurfaceDesc(void *pSurface, int *outW, int *outH);

/*
 * LOCOBITMAP_DarkenRect                                       0x00401540
 *
 * Darken a rectangle of 16-bit pixels on the back-buffer by 50%.
 * Per pixel: pixel = (pixel >> 1) & g_nPixelChannelMask
 * g_nPixelChannelMask (DAT_00485280) = 0x7BEF for RGB565.
 * Lazily locks the surface on the first call; unlocks after the pass.
 *
 * WIN32: IDirectDrawSurface::Lock (vtable 0x64), Unlock (vtable 0x80).
 * LINUX: SDL_LockSurface + pixel walk + SDL_UnlockSurface,
 *        or SDL_SetTextureColorMod(tex, 128, 128, 128).
 */
void LOCOBITMAP_DarkenRect(int x1, int y1, int x2, int y2);

/*
 * LOCOBITMAP_Construct                                        0x00401620
 *
 * __fastcall constructor.  Sets vtable to PTR_FUN_004773e8, sets width
 * to -1 (LOCOBITMAP_UNINIT), and initialises the embedded animation-frame
 * list: frameBufPtr=0, capacity=0, frameCount=0, savedCount=-1.
 *
 * WIN32 APIs: none.
 */
void __fastcall LOCOBITMAP_Construct(LOCOBITMAP *self);

/*
 * LOCOBITMAP_Destructor                                       0x00401650
 *
 * __thiscall destructor.  Re-seats the vtable (re-entry guard), releases
 * the DirectDraw surface via vtable dispatch, frees the frame-record
 * heap buffer, then calls operator-delete on self if bit 0 of deleteBit
 * is set.
 *
 * WIN32 APIs: none (custom allocator FUN_00465cd0).
 * LINUX: free(); SDL_DestroyTexture / SDL_FreeSurface for the surface.
 */
void LOCOBITMAP_Destructor(LOCOBITMAP *self, int deleteBit);

/*
 * LOCOBITMAP_InsertFrameRecord                                0x00401690
 *
 * Insert a 24-byte (6-DWORD) frame record at position index in the
 * LOCOBITMAP's dynamic frame array, growing the heap buffer by one slot
 * and shifting existing records up.
 *
 * Also invoked as AlbIndex_InsertAt for AlbIndex entry buffers: both
 * subsystems use identical 24-byte records and the same buffer-management
 * field layout at +0x08 / +0x0c / +0x10 / +0x14.
 *
 * WIN32 APIs: none (FUN_00465ce0 = malloc / FUN_00465cd0 = free).
 * LINUX: malloc + memmove + free.
 */
void LOCOBITMAP_InsertFrameRecord(LOCOBITMAP *self, int index,
                                   const FrameRecord *record);

/* ═══════════════════════════════════════════════════════════════════════
 * Batch 2 - AlbIndex functions                 0x00401760 to 0x00401ef0
 * ═══════════════════════════════════════════════════════════════════════ */

/*
 * AlbIndex_RemoveAt                                           0x00401760
 *
 * Remove the 24-byte entry at index from the in-memory section buffer.
 * If exactly one entry exists (bufferBytes == FRAME_RECORD_SIZE) the
 * buffer is freed entirely and both pointer and size are zeroed.
 * Otherwise allocates a buffer one entry smaller, copies the surviving
 * entries (skipping the gap at index), frees the old allocation, and
 * decrements bufferBytes by FRAME_RECORD_SIZE.
 *
 * WIN32 APIs: none (FUN_00465ce0 / FUN_00465cd0).
 * LINUX: malloc + memmove + free.
 */
void AlbIndex_RemoveAt(AlbIndex *self, int index);

/*
 * AlbIndex_GetCount                                           0x00401810
 *
 * __fastcall, non-method helper.  Returns the number of 24-byte entries:
 *   *(uint32_t*)((uint8_t*)param1 + 0x0C) / FRAME_RECORD_SIZE
 * param1 is the AlbIndex* (bufferBytes is at +0x0C).
 *
 * WIN32 APIs: none.
 */
int __fastcall AlbIndex_GetCount(const void *param1);

/*
 * AlbIndex_EnsurePageGetCount                                 0x00401820
 *
 * If currentSection (+0x04) differs from section, calls AlbIndex_LoadPage
 * to switch pages.  Returns the entry count for the now-active page
 * (bufferBytes / FRAME_RECORD_SIZE).
 *
 * WIN32 APIs: none directly (AlbIndex_LoadPage performs file I/O).
 * LINUX: same logic; file I/O replaced by POSIX fopen/fread.
 */
int AlbIndex_EnsurePageGetCount(AlbIndex *self, int section);

/*
 * AlbIndex_InsertSorted                                       0x00401850
 *
 * Insert a record into the alphabetically-partitioned PostBag index in
 * sorted order.  Uppercases the name field (param1+0x25) via _strupr
 * (FUN_00474c70) to select the target section (A-C=0 ... X-Z=7, other=8),
 * loads that section via AlbIndex_LoadPage, then linearly scans the
 * sorted name array byte-by-byte to find the correct insertion position,
 * and calls LOCOBITMAP_InsertFrameRecord (FUN_00401690) to insert.
 *
 * WIN32 APIs: none directly (FUN_00474c70 = _strupr).
 * LINUX: toupper loop replaces _strupr; otherwise same logic.
 */
void AlbIndex_InsertSorted(AlbIndex *self, void *param1);

/*
 * AlbIndex_RemoveByID                                         0x00401aa0
 *
 * Remove the index entry whose ID field (entry + ALBINDEX_ENTRY_ID_OFFSET)
 * matches the id at param1+0x0C.  Uses the same letter-to-section mapping
 * as InsertSorted; loads the correct section via AlbIndex_LoadPage.
 * Walks the buffer comparing ID values at stride FRAME_RECORD_SIZE.
 * Returns 1 on match + removal, 0 if not found.
 *
 * WIN32 APIs: none.
 */
int AlbIndex_RemoveByID(AlbIndex *self, void *param1);

/*
 * AlbIndex_FindResource                                       0x00401c10
 *
 * Load section via AlbIndex_LoadPage, then iterate entries starting at
 * byte offset (startIndex x FRAME_RECORD_SIZE).  For each entry: read
 * the ID at entry + ALBINDEX_ENTRY_ID_OFFSET, pass it to FUN_00445930
 * (path-builder writing into a 1284-byte CHAR buffer), then call
 * FUN_00444c70 (resource-lookup).  Returns the first non-NULL pointer,
 * or NULL if none found.
 *
 * WIN32 APIs: none directly.
 */
void *AlbIndex_FindResource(AlbIndex *self, int startIndex, int section);

/*
 * AlbIndex_FlushPage                                          0x00401c90
 *
 * Write the currently resident section buffer back to disk and evict it.
 * Path: <DataPath>\PostBag\AlbIndex\<album%03d>\<section%04d>.ind
 * Deletes the old file, recreates it with CreateFileA(OPEN_ALWAYS, R/W),
 * calls WriteFile, then CloseHandle.  On failure: GetLastError +
 * FormatMessageA + LocalFree.  On success: frees the heap buffer and
 * resets buffer/bufferBytes/currentSection to 0/0/-1.
 *
 * WIN32: wsprintfA, DeleteFileA, CreateFileA, WriteFile, CloseHandle,
 *        GetLastError, FormatMessageA, LocalFree.
 * LINUX: snprintf + unlink + fopen("wb") + fwrite + fclose + strerror.
 */
void AlbIndex_FlushPage(AlbIndex *self);

/*
 * AlbIndex_LoadPage                                           0x00401df0
 *
 * Save the currently loaded section (AlbIndex_FlushPage) then load
 * section param_section from disk.  Same path format as FlushPage.
 * Opens with CreateFileA(OPEN_ALWAYS, R/W); if GetFileSize returns 0 or
 * INVALID_FILE_SIZE the buffer is set to NULL/0 and the file is closed.
 * Otherwise allocates a heap buffer, ReadFile fills it, and sets
 * currentSection = param_section.
 *
 * WIN32: wsprintfA, CreateFileA, GetFileSize, ReadFile, CloseHandle,
 *        GetLastError, FormatMessageA, LocalFree.
 * LINUX: snprintf + fopen + fseek(SEEK_END) + ftell + fread + fclose.
 */
void AlbIndex_LoadPage(AlbIndex *self, int section);

/* ═══════════════════════════════════════════════════════════════════════
 * Batch 3 - LocoBitmapChain functions          0x00401f50 to 0x00404ac0
 * ═══════════════════════════════════════════════════════════════════════ */

/*
 * LocoBitmapChain_Ctor                                        0x00401f50
 *
 * Constructor.  Calls base ctor (FUN_00425870), seats vtable
 * (PTR_FUN_004773f0), then calls LocoBitmapChain_Init.
 *
 * WIN32: none.
 */
void LocoBitmapChain_Ctor(LocoBitmapChain *self);

/*
 * LocoBitmapChain_Dtor                                        0x00401fb0
 *
 * Destructor.  Calls LocoBitmapChain_Release to free all LOCOBITMAP slots
 * and associated resources.  If bit 0 of deleteBit is set, frees self.
 *
 * WIN32: none (custom allocator).
 * LINUX: free().
 */
void LocoBitmapChain_Dtor(LocoBitmapChain *self, int deleteBit);

/*
 * LocoBitmapChain_Init                                        0x00401fd0
 *
 * Initialise all state fields to zero, set resolutionFlag to 1 if the
 * desktop is >= 801x601, allocate 7 fixed LOCOBITMAP slots at IDs
 * 0x3c04..0x3c0f into +0x148..+0x164, allocate the 6 thumbnail, 6
 * label-bg, 6 text-rect, and 9 scroll-destination slot arrays, zero all
 * 6 label buffers, and set all 6 enable flags to 1.
 *
 * WIN32: GetSystemMetrics (SM_CXSCREEN / SM_CYSCREEN).
 * LINUX: SDL_GetCurrentDisplayMode; SDL_CreateTexture per slot.
 */
void LocoBitmapChain_Init(LocoBitmapChain *self);

/*
 * LocoBitmapChain_Release                                     0x00402380
 *
 * Release all DirectDraw and LOCOBITMAP resources.  Frees the object at
 * +0x140 via vtable[2], calls FUN_00454bc0 (LOCOBITMAP dtor/free) on all
 * 8 fixed slots (+0x148..+0x164), 6 thumbnail (+0x168[0..5]), 6 label-bg
 * (+0x180[0..5]) via a combined loop, and 9 scroll-dest (+0x1b0[0..8]).
 * Calls base-class destructor FUN_00425910.
 *
 * WIN32: IDirectDrawSurface::Release via vtable.
 * LINUX: SDL_DestroyTexture per slot; SDL_DestroyRenderer / SDL_DestroyWindow.
 */
void LocoBitmapChain_Release(LocoBitmapChain *self);

/*
 * LocoBitmapChain_CreateWindow                                0x00402520
 *
 * Create the full-screen carousel window.  Gets desktop client rect,
 * loads icon resource LBC_WINDOW_ICON_ID (0x65), calls FUN_00425b70 to
 * create the window sized to the desktop.  Returns TRUE on success.
 *
 * WIN32: GetDesktopWindow + GetClientRect + LoadIconA + CreateWindowExA.
 * LINUX: SDL_CreateWindow(SDL_WINDOW_FULLSCREEN_DESKTOP).
 */
BOOL LocoBitmapChain_CreateWindow(LocoBitmapChain *self);

/*
 * LocoBitmapChain_FlushDrawList                               0x00402660
 *
 * If drawActive (+0xe4) is set, calls FUN_00425990 (present/flip), then
 * clears drawListActive (+0x112) to 0, and calls FreeSlotData.
 *
 * WIN32: IDirectDrawSurface::Flip or Blt to primary.
 * LINUX: SDL_RenderPresent; slot textures destroyed.
 */
void LocoBitmapChain_FlushDrawList(LocoBitmapChain *self);

/*
 * LocoBitmapChain_WndProc                                     0x00402690
 *
 * Window procedure for keyboard navigation.  Guards on shuttingDown
 * (+0x110).  WM_KEYDOWN dispatch (wParam values):
 *
 *   0x1B (ESC) / 0x0D (Enter)  -> PostQuitMessage(0)
 *   0x25 (VK_LEFT)  -> un-draw left-arrow slot (FUN_00403e80, slotId=5),
 *                       Sleep(LBC_ARROW_BLINK_MS), re-draw (FUN_00403ba0,5),
 *                       scroll left one page or decrement track index
 *   0x27 (VK_RIGHT) -> mirror of VK_LEFT for right-arrow (slotId=6)
 *
 * After any scroll: calls RefreshGrid then FUN_00426b90 (blit to screen).
 *
 * WIN32: WM_KEYDOWN, PostQuitMessage, Sleep.
 * LINUX: SDL_KEYDOWN + SDL_SCANCODE_*, SDL_Delay.
 */
LRESULT CALLBACK LocoBitmapChain_WndProc(HWND hWnd, UINT msg,
                                          WPARAM wParam, LPARAM lParam);

/*
 * LocoBitmapChain_SetSlotState                                0x00403ba0
 *
 * Set the visual state (normal/idle vs. grayed/disabled) of one fixed UI
 * slot.  Calls FUN_00454c30(slot, 0, NULL) for normal frame, or
 * FUN_00454c30(slot, 2, NULL) for grayed frame when the corresponding
 * enable flag is clear.
 *
 * Slot-ID to slots[] index and enableFlags[] index:
 *   ID 1  -> slots[0]  (no enable flag)
 *   ID 2  -> slots[1]  (no enable flag)
 *   ID 3  -> slots[2]  (no enable flag)
 *   ID 4  -> slots[3]  (no enable flag)
 *   ID 9  -> slots[4]  enableFlags[2]
 *   ID 5  -> slots[5]  enableFlags[0]  (left-arrow)
 *   ID 6  -> slots[6]  enableFlags[1]  (right-arrow)
 *
 * WIN32: none directly.
 * LINUX: SDL_SetTextureColorMod(tex, 128, 128, 128) for grayed state.
 */
void LocoBitmapChain_SetSlotState(LocoBitmapChain *self, int slotId);

/*
 * LocoBitmapChain_HitTest                                     0x00403cd0
 *
 * Hit-test screen point (x, y) against all LOCOBITMAP slot RECTs.
 * The slot RECT is read from slot->width / frameBufPtr / capacity /
 * frameCount as left / top / right / bottom via the LBC_SLOT_* macros.
 * Sets self->hitIndex when a thumbnail, text-rect, or scroll-dest is hit.
 *
 * Return values (zone IDs):
 *   0         miss
 *   1,2,3,4   fixed buttons (no hitIndex update)
 *   5         left-arrow
 *   6         right-arrow
 *   7         scroll-dest  (hitIndex = matched scrollDest[] index)
 *   8         thumbnail    (hitIndex = matched thumbnails[] index)
 *   9         fixed button (no hitIndex update)
 *   10        text-rect    (hitIndex = matched textRects[] index)
 *
 * WIN32: PtInRect (replaced by RECT boundary comparison).
 * LINUX: manual SDL_Rect containment check.
 */
int LocoBitmapChain_HitTest(LocoBitmapChain *self, int x, int y);

/*
 * LocoBitmapChain_DrawSlot                                    0x00403e80
 *
 * Draw one LOCOBITMAP slot to the back-buffer.  Acquires a draw resource
 * token via FUN_00447930(LBC_DRAW_RESOURCE_TOKEN).  Reads the slot's RECT
 * from slot +0x04..+0x10.  If both initialized (+0x111) and drawListActive
 * (+0x112) are set: applies (srcOffsetX, srcOffsetY) to the source RECT
 * and (dstOffsetX, dstOffsetY) to the destination RECT, then calls
 * FUN_0042b050 (AW blit) from g_pBackSurface/g_pBackBuffer to
 * pBlitSurface (+0x13c).  Marks the slot visible via
 * FUN_00454c30(slot, 1, NULL).  Slot ID 7 uses trackPageIndex as the
 * frame index.  Disabled slots call FUN_00454c30(slot, 2, NULL) and skip.
 *
 * WIN32: IDirectDrawSurface::Blt via FUN_0042b050.
 * LINUX: SDL_RenderCopy(renderer, slotTexture, &srcRect, &dstRect).
 */
void LocoBitmapChain_DrawSlot(LocoBitmapChain *self, int slotId);

/*
 * LocoBitmapChain_FreeSlotData                                0x00404830
 *
 * Free all LOCOBITMAP objects currently loaded into every slot.  Guards on
 * initialized (+0x111).  Releases pDDObject (+0x140) via vtable[2], then
 * calls FUN_00454bc0 (LOCOBITMAP dtor/free) on all 8 fixed, 6 thumbnail,
 * 6 label-bg (combined loop), and 9 scroll-dest slots.  Clears initialized.
 *
 * WIN32: IDirectDrawSurface::Release via vtable.
 * LINUX: SDL_DestroyTexture per slot.
 */
void LocoBitmapChain_FreeSlotData(LocoBitmapChain *self);

/*
 * LocoBitmapChain_DrawItem                                    0x004048e0
 *
 * Draw one item cell in the scrollable grid.  Looks up the world-data
 * entry at (currentCol + col, currentRow) via AlbIndex_FindResource
 * (FUN_00401c10).  If null: blits background from thumbnails[col] rect
 * and clears the label buffer for col.  If non-null: calls FUN_004437c0
 * to render the item bitmap into the slot rect, copies the name string
 * (data+0x25) into labels[col], and calls
 * FUN_00454c30(labelBg[col], 0, NULL) to show the label overlay.
 *
 * WIN32: none directly.
 * LINUX: SDL_RenderCopy for thumbnails; SDL_ttf for label text.
 */
void LocoBitmapChain_DrawItem(LocoBitmapChain *self, int col);

/*
 * LocoBitmapChain_RefreshGrid                                 0x00404ac0
 *
 * Full item-grid refresh.  Calls DrawItem for each of the 6 visible
 * columns (0..5).  For each column i: if textFlag (+0x12c) is set,
 * reads the text-rect slot RECT from textRects[i] and calls DrawTextA
 * with g_labelFont, LBC_DRAWTEXT_FLAGS (0x25), and labels[i].  Blits to
 * screen via FUN_00426b90.  Updates prev-page enable flags
 * (enableFlags[0] / [4]) when currentCol == 0 && currentRow == 0.
 * Updates next-page enable flags (enableFlags[1] / [5]) when
 * (currentCol + pageSize) >= totalItems.  Calls SetSlotState to refresh
 * left/right arrow visual states.
 *
 * WIN32: DrawTextA (g_labelFont, DT_SINGLELINE|DT_CENTER|DT_VCENTER).
 * LINUX: SDL_ttf TTF_RenderText_Blended + SDL_RenderCopy.
 */
void LocoBitmapChain_RefreshGrid(LocoBitmapChain *self);

/* ═══════════════════════════════════════════════════════════════════════
 * Helper macros
 * ═══════════════════════════════════════════════════════════════════════ */

/* Number of frame records currently held in a LOCOBITMAP's buffer. */
#define LOCOBITMAP_FRAME_COUNT(bm) \
    ((bm) ? ((uint32_t)(bm)->capacity / FRAME_RECORD_SIZE) : 0u)

/* Pointer to FrameRecord at index idx within a LOCOBITMAP buffer. */
#define LOCOBITMAP_FRAME_AT(bm, idx) \
    ((bm) ? ((FrameRecord *)(uintptr_t)(bm)->frameBufPtr + (idx)) : NULL)

/* Number of index entries currently in an AlbIndex buffer. */
#define ALBINDEX_ENTRY_COUNT(ai) \
    ((ai) ? ((ai)->bufferBytes / FRAME_RECORD_SIZE) : 0u)

/* Raw uint32_t pointer to AlbIndex entry at index (6 dwords per entry). */
#define ALBINDEX_ENTRY_RAW(ai, idx) \
    ((ai) ? ((uint32_t *)(ai)->buffer + (idx) * (FRAME_RECORD_SIZE / 4)) : NULL)

/* ID field of an AlbIndex entry (at dword 5 = offset +0x14). */
#define ALBINDEX_ENTRY_ID(ai, idx) \
    (ALBINDEX_ENTRY_RAW(ai, idx) ? ALBINDEX_ENTRY_RAW(ai, idx)[5] : 0u)

/*
 * Read the blit RECT from a LOCOBITMAP used as a LocoBitmapChain UI slot.
 * The LOCOBITMAP fields width/frameBufPtr/capacity/frameCount at
 * +0x04/+0x08/+0x0c/+0x10 are reused as RECT left/top/right/bottom.
 */
#define LBC_SLOT_LEFT(s)    ((s)->width)
#define LBC_SLOT_TOP(s)     ((s)->frameBufPtr)
#define LBC_SLOT_RIGHT(s)   ((s)->capacity)
#define LBC_SLOT_BOTTOM(s)  ((s)->frameCount)

/* Point-in-RECT test using the slot-RECT field aliases above. */
#define LBC_SLOT_HIT(s, px, py) \
    ((px) >= LBC_SLOT_LEFT(s)  && (px) < LBC_SLOT_RIGHT(s) && \
     (py) >= LBC_SLOT_TOP(s)   && (py) < LBC_SLOT_BOTTOM(s))

/* ═══════════════════════════════════════════════════════════════════════
 * Linux / SDL2 port stubs
 * ═══════════════════════════════════════════════════════════════════════ */
#ifdef LOCO_LINUX

/*
 * LINUX_LOCOBITMAP_LoadFromFile - SDL2 replacement for LOCOBITMAP_LoadFromFile.
 *
 *   SDL_Surface *s = SDL_LoadBMP(path);
 *   SDL_SetColorKey(s, SDL_TRUE, SDL_MapRGB(s->format, 255, 0, 255));
 *   if (useVideoMem) {
 *       SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);
 *       SDL_FreeSurface(s);
 *       return t;   // GPU path
 *   }
 *   return s;       // CPU path; caller casts to SDL_Surface*
 */
static inline void *LINUX_LOCOBITMAP_LoadFromFile(const char *path,
                                                   int useVideoMem)
{
    (void)path; (void)useVideoMem;
    return NULL; /* TODO: implement as described above */
}

/*
 * LINUX_LOCOBITMAP_DarkenRect - SDL2 replacement for LOCOBITMAP_DarkenRect.
 *
 * CPU path (SDL_Surface*):
 *   SDL_LockSurface(surf);
 *   uint16_t *px = (uint16_t*)surf->pixels + y1 * (surf->pitch / 2);
 *   for each row, each pixel: px[x] = (px[x] >> 1) & PIXEL_CHANNEL_MASK_RGB565;
 *   SDL_UnlockSurface(surf);
 *
 * GPU path (SDL_Texture*):
 *   SDL_SetTextureColorMod(tex, 128, 128, 128);
 *   SDL_RenderCopy(renderer, tex, &srcRect, &dstRect);
 *   SDL_SetTextureColorMod(tex, 255, 255, 255);
 */
static inline void LINUX_LOCOBITMAP_DarkenRect(SDL_Surface *surf, SDL_Rect *r)
{
    (void)surf; (void)r;
    /* TODO: implement CPU or GPU darkening path as described above */
}

#endif /* LOCO_LINUX */

#endif /* LOCOBITMAP_H */
