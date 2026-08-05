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
#include <stddef.h>

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
/*  - Save/load buffer mode:                                           */
/*      entity record buffer  at +0x04 (RESMGR_LockResource, 0x447DB0) */
/*      vehicle record buffer at +0x84 (RESMGR_UnlockResource, 0x447DF0)*/
/*      save header region    at +0xB0 (SaveRegion, 0x114 bytes)       */
/*      pixels/streams        at +0x1C4..+0x1D7                        */
/* ================================================================== */

/* ================================================================== */
/* SaveRegion — the 0x114-byte .loco save header (file bytes +0x00..)  */
/*                                                                      */
/* Overlaid at RESDATA + 0xB0: RESMGR_LoadResource (0x447BA0) reads    */
/* exactly 0x114 file bytes there, and RESMGR_LoadResourceData         */
/* (0x447E30) writes the same region back out.  Field names follow the */
/* file layout (evidence: shipped art-res/SAVEGAME saves and ~curr):   */
/*   +0x00 uint16 type          — always 8 (RESMGR_IsSaveHeader)       */
/*   +0x02 uint16 player_id     — INPUT_SaveCurrentWorld writes        */
/*                                g_player_id; designer saves store the */
/*                                preview width/16 there instead       */
/*   +0x04 uint16 player_color  — same dual use (preview height/16)    */
/*   +0x06 uint16 pad           — always 0 in shipped saves            */
/*   +0x08 uint32 entity_count  — number of 0x80-byte entity records   */
/*   +0x0C uint16 vehicle_count — number of 0x2C-byte vehicle records  */
/*   +0x0E char  name[0x106]    — backdrop/save name ("BACKDROP" in    */
/*                                ~curr; INPUT_SaveCurrentWorld writes */
/*                                the empty BSS string at 0x4AA9FD)    */
/* ================================================================== */
struct SaveRegion {
    uint16_t type;              /* +0x00 */
    uint16_t player_id;         /* +0x02 */
    uint16_t player_color;      /* +0x04 */
    uint16_t pad_06;            /* +0x06 */
    uint32_t entity_count;      /* +0x08 */
    uint16_t vehicle_count;     /* +0x0C */
    char     name[0x106];       /* +0x0E */
};
static_assert(sizeof(SaveRegion) == 0x114, "SaveRegion must be exactly 0x114 bytes");

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
    /* +0x36..+0x163: unknown fields — padding to reach the save region.  */
    /* Fields below are the legacy carved names TrackPiece.cpp reads      */
    /* (offsets preserved from the pre-save-region layout; the Ghidra     */
    /* comments reference the record-buffer view at +0x04 + 0x24..0x30).  */
    uint16_t  frame_count;       /* +0x36 total frame count (entity_buffer +0x28) */
    uint16_t  frame_w;           /* +0x38 frame width (entity_buffer +0x24) */
    uint16_t  frame_h;           /* +0x3A frame height (entity_buffer +0x26) */
    int16_t   world_x;           /* +0x3C world X (entity_buffer +0x2a)     */
    int16_t   world_y;           /* +0x3E world Y (entity_buffer +0x2c)     */
    uint8_t   entity_buffer[0x24]; /* +0x40 raw entity save/load buffer     */
    uint8_t   _pad_64[0x4C];    /* +0x64..+0xAF                           */

    /* ---- Resource-manager save/load region (+0xB0..+0x1D7) ---------- */
    /* The entity/vehicle record buffers at +0x04/+0x84 live in the      */
    /* sprite-metadata padding above; RESMGR_LockResource (0x447DB0)     */
    /* reads 0x80 bytes into +0x04 and RESMGR_UnlockResource (0x447DF0)  */
    /* reads 0x2C bytes into +0x84.                                     */
    SaveRegion save;            /* +0xB0..+0x1C3  (0x114 bytes)           */
    void*     save_pixels;      /* +0x1C4  preview pixel buffer           */
    void*     primary_stream;   /* +0x1C8  entity/vehicle record stream   */
    void*     secondary_stream; /* +0x1CC  output stream (save writing)   */
    void*     asset_data;       /* +0x1D0  AssetMgr_LoadFile result       */
    int32_t   asset_size;       /* +0x1D4  asset_data byte count          */
    /* +0x1D8..0x1DB: tail — sizeof(RESDATA) is exactly 0x1D8 (i686).   */
#if UINTPTR_MAX != 0xffffffffu
    /* ---- Host-native record buffers (64-bit hosts only) ------------
     * The x86 RESDATA layout carries the entity/vehicle record buffers
     * at +0x04/+0x84 (RESMGR_LockResource 0x447DB0 / RESMGR_UnlockResource
     * 0x447DF0 read one 0x80/0x2C record there).  Those offsets are only
     * valid in the 32-bit layout: on a 64-bit host +0x04 lands inside the
     * pointer-width vtable member and +0x84 inside the sprite-metadata
     * fields, so writing them would corrupt host members.  The native
     * primitives (ResDataSave.cpp host branch) therefore read into these
     * typed-width buffers instead — no x86 offsets are ever written into
     * host RESDATA members (safe native layout, AGENTS.md).  The buffers
     * are not part of the x86 layout (guarded out on 32-bit). */
    alignas(8) uint8_t host_record_entity[0x80];   /* 0x80-byte entity record */
    alignas(8) uint8_t host_record_vehicle[0x2C];  /* 0x2C-byte vehicle record */
#endif
};
/* x86-layout assertions only — the shared struct carries native-width
 * pointers (vtable/anim_table), so 64-bit hosts have different sizes.
 * The save region itself (SaveRegion) is pure LE uint16/32 + char, so
 * its own size assertion is unconditional. */
#if UINTPTR_MAX == 0xffffffffu
static_assert(sizeof(RESDATA) == 0x1D8, "RESDATA size mismatch (expected 0x1D8)");
static_assert(offsetof(RESDATA, save) == 0xB0, "RESDATA save region offset mismatch");
static_assert(offsetof(RESDATA, save_pixels) == 0x1C4,
              "RESDATA save_pixels offset mismatch");
static_assert(offsetof(RESDATA, primary_stream) == 0x1C8,
              "RESDATA primary_stream offset mismatch");
static_assert(offsetof(RESDATA, secondary_stream) == 0x1CC,
              "RESDATA secondary_stream offset mismatch");
static_assert(offsetof(RESDATA, asset_data) == 0x1D0,
              "RESDATA asset_data offset mismatch");
static_assert(offsetof(RESDATA, asset_size) == 0x1D4,
              "RESDATA asset_size offset mismatch");
#endif
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
