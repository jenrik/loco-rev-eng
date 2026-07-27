/**
 * types.h — Common type definitions for Lego Loco C++ decompilation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Provides Windows type shims (matching original MSVC sizes) and
 * struct definitions for shared data types used across subsystems.
 */

#pragma once

#include <stdint.h>

/* Prevent stubs/windows.h from redefining types already declared here */
#define LOCO_TYPES_DEFINED

/* ================================================================== */
/* Windows type shims — match original MSVC 32-bit sizes               */
/* ================================================================== */
typedef uint8_t   BYTE;
typedef uint8_t   byte;
typedef uint16_t  WORD;
typedef uint32_t  DWORD;
typedef uint32_t  UINT;
typedef int32_t   BOOL;
typedef int32_t   LONG;
typedef int32_t   LRESULT;
typedef uint32_t  WPARAM;
typedef int32_t   LPARAM;
typedef uint16_t  ATOM;

/* Handle types — void* on 32-bit; compatible with stubs/windows.h */
typedef void*     HANDLE;
typedef HANDLE    HWND;
typedef HANDLE    HINSTANCE;
typedef HANDLE    HMODULE;
typedef HANDLE    HMENU;
typedef HANDLE    HDC;
typedef HANDLE    HICON;
typedef HANDLE    HBRUSH;
typedef HANDLE    HCURSOR;
typedef HANDLE    HFONT;
typedef HANDLE    HPALETTE;
typedef HANDLE    HBITMAP;
typedef HANDLE    HRGN;
typedef HANDLE    HGDIOBJ;
typedef HANDLE    HPEN;

typedef const char*     LPCSTR;
typedef const wchar_t*  LPCWSTR;

/* ================================================================== */
/* RECT — matches Windows RECT (4 x int32_t = 16 bytes)                */
/* ================================================================== */
struct RECT {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
};

/* ================================================================== */
/* POINT — matches Windows POINT (2 x int32_t = 8 bytes)               */
/* ================================================================== */
struct POINT {
    int32_t x;
    int32_t y;
};

/* ================================================================== */
/* SIZE — matches Windows SIZE (2 x int32_t = 8 bytes)                 */
/* ================================================================== */
struct SIZE {
    int32_t cx;
    int32_t cy;
};

#ifndef _TAGMSG_DEFINED
#define _TAGMSG_DEFINED
/* ================================================================== */
/* tagMSG — Windows message structure (28 bytes)                       */
/* ================================================================== */
struct tagMSG {
    HWND     hwnd;
    UINT     message;
    WPARAM   wParam;
    LPARAM   lParam;
    DWORD    time;
    POINT    pt;
};
#endif /* _TAGMSG_DEFINED */

/* ================================================================== */
#ifndef _WNDCLASSA_DEFINED
#define _WNDCLASSA_DEFINED
/* WNDCLASSA — Window class structure (40 bytes)                       */
/* ================================================================== */
struct WNDCLASSA {
    UINT      style;
    LRESULT (*lpfnWndProc)(HWND, UINT, WPARAM, LPARAM);
    int       cbClsExtra;
    int       cbWndExtra;
    HINSTANCE hInstance;
    HICON     hIcon;
    HCURSOR   hCursor;
    HBRUSH    hbrBackground;
    LPCSTR    lpszMenuName;
    LPCSTR    lpszClassName;
};
#endif /* _WNDCLASSA_DEFINED */

/* ================================================================== */
#ifndef _WNDCLASSEXA_DEFINED
#define _WNDCLASSEXA_DEFINED
/* WNDCLASSEXA — Extended window class structure (48 bytes)            */
/* ================================================================== */
struct WNDCLASSEXA {
    UINT      cbSize;
    UINT      style;
    LRESULT (*lpfnWndProc)(HWND, UINT, WPARAM, LPARAM);
    int       cbClsExtra;
    int       cbWndExtra;
    HINSTANCE hInstance;
    HICON     hIcon;
    HCURSOR   hCursor;
    HBRUSH    hbrBackground;
    LPCSTR    lpszMenuName;
    LPCSTR    lpszClassName;
    HICON     hIconSm;
};
#endif /* _WNDCLASSEXA_DEFINED */

/* ================================================================== */
/* COLORREF — GDI color value (uint32_t)                                */
/* ================================================================== */
typedef uint32_t COLORREF;

typedef int32_t  HRESULT;

/* ================================================================== */
/* Additional Win32 types not in compat.h                               */
/* ================================================================== */
typedef LRESULT (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

/* ================================================================== */
/* Forward declarations for cross-referenced types                     */
/* ================================================================== */
struct GameObject;
struct Entity;
struct RESDATA;
struct UIPANEL;
struct SURFACE;
struct FrameData;
struct UISprite;
struct AboutDialog;    /* About/Credits dialog and screensaver class */

/* ================================================================== */
/* FrameData — per-frame animation metadata                            */
/* Size: 0x18 = 24 bytes                                               */
/* Stored at RESDATA+0x20, indexed by GameObject::anim_index           */
/* ================================================================== */
struct FrameData {
    uint16_t start_frame;       /* +0x00  first frame in animation range    */
    uint16_t end_frame;         /* +0x02  last frame in animation range     */
    uint16_t step_delay;        /* +0x04  ticks to wait between steps       */
    uint16_t _pad_06;           /* +0x06  (alignment)                       */
    int32_t  wait_time;         /* +0x08  pause duration at boundary (ticks)*/
    int16_t  sound_fx_index;    /* +0x0C  <0 = no-loop (stop at boundary)    */
    uint16_t audio_res_id;      /* +0x0E  audio resource ID for playback    */
    uint32_t audio_delay;       /* +0x10  delay mode (0=immediate, 1=random)*/
    uint16_t volume;            /* +0x14  audio playback volume             */
    uint8_t  flip_horizontal;   /* +0x16  1 = mirrored sprite               */
    /* Note: offset +0x17 is overloaded:
     *   is_connected: 1 = connected/multi-tile sprite
     *   step_mode:    1 = +2 per tick with even frames only
     */
};

/* ================================================================== */
#ifndef RESDATA_DEFINED
#define RESDATA_DEFINED
/* RESDATA — resource definition loaded from ResourceManager            */
/* Size: 0x1D8 bytes (from Ghidra: base size documented in RM.h)       */
/*                                                                     */
/* WARNING: This struct serves dual purposes:                           */
/*  - Sprite metadata mode (+0x04..+0x34): frame/animation fields      */
/*  - Save/load buffer mode (+0x04..+0xAF): entity+vehicle buffers     */
/*    (accessed via ResourceManager.cpp which #undefs RESDATA_DEFINED  */
/*     and provides its own layout with entity_buffer, vehicle_buffer,  */
/*     pixels, primary_stream, etc. at documented offsets.)            */
/* ================================================================== */
struct RESDATA {
    void     *vtable;           /* +0x00  vtable[1]=Lock/GetSurface         */
    int32_t   resource_id;      /* +0x04  numeric resource ID               */
    /* gap +0x08..+0x0F — padding / unknown */
    uint8_t   _pad_08[8];       /* +0x08  padding                           */
    uint32_t  flags;            /* +0x10  0 = no surface data               */
    /* Note: +0x10 also aliased as ui_panel (UIPANEL*) in some contexts    */
    uint16_t  frame_width;      /* +0x14  sprite frame width in pixels      */
    uint16_t  frame_height;     /* +0x16  sprite frame height in pixels     */
    uint8_t   _pad_18[2];       /* +0x18  padding                           */
    uint16_t  anim_count;       /* +0x1A  total animation entry count       */
    uint8_t   _pad_1C[2];       /* +0x1C  padding                           */
    int16_t   default_anim;     /* +0x1E  default animation index (signed)  */
    FrameData *anim_table;      /* +0x20  array of FrameData entries        */
    uint8_t   _pad_24[14];      /* +0x24..+0x31  padding                    */
    int16_t   offset_x;         /* +0x32  world X offset (added in SetPos)  */
    int16_t   offset_y;         /* +0x34  world Y offset                    */
    /* +0x36..+0x163: unknown fields — padding to reach default_delay     */
    /* Fields below carved out of this padding range; exact offsets TBD,  */
    /* consumers reference them by name only (see TrackPiece.cpp).        */
    uint16_t  frame_count;       /* +0x?? total frame count (entity_buffer +0x28) */
    uint16_t  frame_w;           /* +0x?? frame width (entity_buffer +0x24) */
    uint16_t  frame_h;           /* +0x?? frame height (entity_buffer +0x26) */
    int16_t   world_x;           /* +0x?? world X (entity_buffer +0x2a)     */
    int16_t   world_y;           /* +0x?? world Y (entity_buffer +0x2c)     */
    uint8_t   entity_buffer[0x24]; /* +0x?? raw entity save/load buffer     */
    uint8_t   _pad_36[0x100];   /* +0x36..+0x163 (reduced by fields above)  */
    uint32_t  default_delay;    /* +0x164 timing/delay copied to GameObject */
    /* +0x168..+0x1C3: padding — resource manager save/load fields        */
    uint8_t   _pad_168[0x5C];   /* +0x168..+0x1C3                           */
    /* The following fields are documented for ResourceManager save/load
     * operations. They live at +0x1C4..+0x1D7 in the full 0x1D8-byte
     * layout. ResourceManager.cpp #undefs RESDATA_DEFINED and provides
     * its own definition with these fields at the correct offsets.       */
    /* +0x1C4: pixels (void*)                                              */
    /* +0x1C8: primary_stream (void*)                                      */
    /* +0x1CC: secondary_stream (void*)                                    */
    /* +0x1D0: asset_data (void*)                                          */
    /* +0x1D4: asset_size (int32_t)                                        */
    uint8_t   _pad_1C4[0x14];   /* +0x1C4..+0x1D7                           */
};
#endif /* RESDATA_DEFINED */

/* ================================================================== */
/* UISprite — UI hit-test / render sprite descriptor                   */
/* ================================================================== */
struct UISprite {
    /* Fields documented as encountered during RE; full layout TBD */
    int32_t  x;                 /* +0x00  screen X position               */
    int32_t  y;                 /* +0x04  screen Y position               */
    int32_t  width;             /* +0x08  sprite width                    */
    int32_t  height;            /* +0x0C  sprite height                   */
};

/* ================================================================== */
/* Global variables — declared extern; defined in their owning module   */
/* ================================================================== */
extern uint32_t  g_game_time;           /* 0x4A99B4 — global game tick counter */
extern HWND      g_main_window;
extern void*     _g_primary_surface;   /* 0x4FD3C4 — primary DirectDraw surface */
extern uint8_t   g_is_party_mode;       /* 0x48548C — 1 = party mode active    */
extern uint32_t  g_party_start_time;    /* 0x485490 — tick when party started  */
extern void     *g_config_ini;          /* 0x485484 — config/INI object handle  */
extern int32_t    g_demo_mode;           /* 0x4A9918 — 1 = demo mode active      */
