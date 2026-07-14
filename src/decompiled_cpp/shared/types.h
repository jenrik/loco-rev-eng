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

/* ================================================================== */
/* Windows type shims — match original MSVC 32-bit sizes               */
/* ================================================================== */
typedef uint8_t   BYTE;
typedef uint8_t   byte;
typedef uint16_t  WORD;
typedef uint32_t  DWORD;
typedef uint32_t  UINT;
typedef int32_t   BOOL;
typedef void*     HWND;
typedef void*     HINSTANCE;
typedef const char* LPCSTR;
typedef const wchar_t* LPCWSTR;

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
/* Forward declarations for cross-referenced types                     */
/* ================================================================== */
struct GameObject;
struct Entity;
struct RESDATA;
struct UIPANEL;
struct SURFACE;
struct FrameData;
struct UISprite;

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
/* RESDATA — resource definition loaded from ResourceManager            */
/* ================================================================== */
struct RESDATA {
    void     *vtable;           /* +0x00  vtable[1]=Lock/GetSurface         */
    int32_t   resource_id;      /* +0x04  numeric resource ID               */
    /* gap +0x08..+0x0F */
    uint32_t  flags;            /* +0x10  0 = no surface data               */
    UIPANEL  *ui_panel;         /* +0x10  (overlaps: sprite-sheet panel ptr)*/
    uint16_t  frame_width;      /* +0x14  sprite frame width in pixels      */
    uint16_t  frame_height;     /* +0x16  sprite frame height in pixels     */
    /* +0x18 */
    uint16_t  anim_count;       /* +0x1A  total animation entry count       */
    /* +0x1C */
    int16_t   default_anim;     /* +0x1E  default animation index (signed)  */
    FrameData *anim_table;      /* +0x20  array of FrameData entries        */
    /* +0x24..+0x31 */
    int16_t   offset_x;         /* +0x32  world X offset (added in SetPos)  */
    int16_t   offset_y;         /* +0x34  world Y offset                    */
    /* ... variable-sized fields follow ... */
    uint32_t  default_delay;    /* +0x164 timing/delay copied to GameObject */
};

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
extern HWND      g_main_window;         /* 0x4AA4A0 — main window handle       */
extern uint8_t   g_is_party_mode;       /* 0x48548C — 1 = party mode active    */
extern uint32_t  g_party_start_time;    /* 0x485490 — tick when party started  */
extern void     *g_config_ini;          /* 0x485484 — config/INI object handle  */
