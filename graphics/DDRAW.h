/**
 * DDRAW.h — DirectDraw rendering and sprite management for Lego Loco
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * This file covers four groups:
 *
 *   A) DDRAW_Building class  (vtable 0x478548, size ~0x5B0):
 *      Building/station/vehicle sprite manager. Manages selection popups,
 *      name labels, colored state dots, station lists, track arrows, and
 *      day/night animation. Associated with global g_ddraw_building at
 *      0x4A9EF0. Extends RESDATA (vtable 0x478274).
 *
 *   B) SpriteData struct  (size 0x10):
 *      Lightweight sprite resource descriptor. Holds a 4-ary tree pointer
 *      plus an allocated pixel buffer. Resource ID stored at +0x0C.
 *
 *   C) FileData struct  (size 0x10):
 *      Loaded game resource file descriptor. Holds file stream handle,
 *      list of decompressed sub-blocks, total size, and alloc'd filename.
 *
 *   D) C free functions:
 *      Surface management (create, unlock, release, restore),
 *      audio initialization (DirectSound),
 *      error string lookup tables,
 *      clipper management,
 *      file/resource loading helpers.
 *
 * Class hierarchy:
 *   RESDATA (vtable 0x478274)
 *     └─ DDRAW_Building  ← this class (vtable 0x478548)
 *
 * Vtable layout (0x478548):
 *   [0]  +0x00: scalar deleting destructor (DDRAW_SpriteScalarDtor, 0x458AD0)
 *   [1]  +0x04: Refresh/Hide/Show            (DDRAW_SelectBuilding dispatches)
 *   [2]  +0x08: HitTest dispatch              (used by tilemap/town)
 *   [3]  +0x0C: SetPosition                   (move sprite to x,y)
 *   [4]  +0x10: (unknown)
 *   [5]  +0x14: (unknown)
 *   [6]  +0x18: LoadChildResource             (resId, unknown, flags)
 *   [7]  +0x1C: HandleOneArgAction            (single int arg)
 *   [8]  +0x20: Refresh                       (re-render sprite)
 *   [9]  +0x24: SetName / SetResource         (text or resource)
 *   [10] +0x28: AnimUpdate                    (per-frame animation tick)
 *   [11] +0x2C: DispatchRender/DrawConnected  (render/clip subtree)
 *   [12] +0x30: DispatchRenderConnected       (render connected tiles)
 *   [13]+: More RESDATA-inherited slots
 */

#pragma once

#include "../shared/types.h"
#include "../core/GameObject.h"
#include "DDRAW_Building.h"


// Status: TRANSCRIBED
/* ================================================================== */
/* Forward declarations                                               */
/* ================================================================== */

struct GameAudio;
struct UIPANEL_Surface;
class  Building;

/* ================================================================== */
/* SpriteData — lightweight sprite resource descriptor                */
/* Size: 0x10 bytes                                                   */
/* ================================================================== */

struct SpriteData {
    void*     tree_node;         /* +0x00  AssetMgr 4-ary tree node      */
    void*     pixel_buffer;      /* +0x04  allocated pixel data buffer   */
    uint32_t  file_size;         /* +0x08  total file size in bytes      */
    uint16_t  resource_id;       /* +0x0C  numeric resource identifier   */
    /* total: 0x0E bytes + alignment padding to 0x10 */

    /**
     * SpriteData::SpriteData — constructor.
     * Address: 0x45CDF0, __thiscall
     *
     * Zeroes tree_node/pixel_buffer/file_size, stores resource_id.
     */
    explicit SpriteData(uint16_t res_id);

    /**
     * SpriteData::~SpriteData — destructor.
     * Address: 0x45CE10, __fastcall
     *
     * Detaches this node from the AssetMgr tree (AssetMgr_ReadFile,
     * despite the misleading Ghidra-inferred name — see resources/AssetMgr.h),
     * then frees pixel_buffer if allocated.
     */
    ~SpriteData();
};

/* ================================================================== */
/* FileData — loaded game resource file descriptor                    */
/* Size: 0x10 bytes                                                   */
/* Destroyed by DDRAW_FileData_Dtor (0x45CA20)                        */
/* ================================================================== */

struct FileData {
    void*     file_handle;       /* +0x00  opened file stream handle     */
    void*     block_list;        /* +0x04  linked list of decompressed   */
                                 /*        sub-blocks (+0x00=name, +0x04= */
                                 /*        size, +0x08=offset, +0x0C=next)*/
    int32_t   total_size;        /* +0x08  total decompressed size       */
    char*     file_name;         /* +0x0C  allocated file path string    */
};

/* DDRAW_Building's class declaration lives in DDRAW_Building.h (included
 * above) so translation units that can't include the rest of this header
 * (see the NOTE on g_tilemap/g_ddraw_building/g_primary_surface below)
 * can still call its methods directly. */

/* ================================================================== */
/* Global singleton                                                    */
/* ================================================================== */

/**
 * g_ddraw_building — global DDRAW_Building singleton.
 * Address: 0x4A9EF0
 *
 * Created in CGWND_InitMode1. Used as the active building selection
 * popup manager. Session to g_active_panel.
 */
extern DDRAW_Building* g_ddraw_building;  /* 0x4A9EF0 */

/* ================================================================== */
/* Global state                                                        */
/* ================================================================== */

extern int g_viewport_x;            /* 0x4AAD24 — viewport scroll X */
extern int g_viewport_y;            /* 0x4AAD28 — viewport scroll Y */

class UI_Manager;
extern UI_Manager* g_tooltip_mgr;   /* 0x4FD220 — points at the real UI_Manager
                                      * singleton (DDRAW.cpp defines the backing
                                      * `static UI_Manager g_tooltip_mgr_instance`
                                      * — see that file's doc comment for the
                                      * CRT static-initializer evidence) */
extern void* g_world_state;         /* 0x4A98B0 — World state struct */
extern void* g_tilemap;             /* 0x4AAD08 — tilemap global */

/* ================================================================== */
/* C free functions — Surface management                               */
/* ================================================================== */

/**
 * DDRAW_GetSurface — master DirectDraw surface initialization.
 * Address: 0x45B500, __cdecl
 *
 * Creates DirectDraw object, sets cooperative level, creates primary
 * surface + backbuffer, detects 16-bit pixel format (555 vs 565),
 * sets color key, attaches clipper to HWND. Sets global pixel-format
 * vars at 0x485274-0x485290.
 *
 * Called by: CGWND_InitMode1 (init sequence)
 *
 * @return  1 on success, error code on failure
 */
uint32_t __cdecl DDRAW_GetSurface(void);

/**
 * DDRAW_UnlockPrimary — unlock and flip the primary surface.
 * Address: 0x45B940, __cdecl
 *
 * Unlocks the backbuffer, performs flip/blit to primary surface.
 * Handles surface loss (DDERR_SURFACELOST) by restoring surfaces.
 *
 * Called by: GameLoop_FrameUpdate (end of frame)
 */
void __cdecl DDRAW_UnlockPrimary(void);

/**
 * DDRAW_SetSurfaceFormat — detect and set surface pixel format.
 * Address: 0x45B9B0, __cdecl
 *
 * Calls IDirectDrawSurface::GetPixelFormat on the surface and
 * detects 5-5-5 RGB (0x7C00) vs 5-6-5 RGB (0xF800) format.
 * Sets global pixel-format variables.
 *
 * Called by: DDRAW_GetSurface
 *
 * @param surface  IDirectDrawSurface4*
 * @param fmt      DDPIXELFORMAT structure to fill
 */
void __cdecl DDRAW_SetSurfaceFormat(int* surface, int* fmt);

/**
 * DDRAW_RestoreSurfaces — restore lost DirectDraw surfaces.
 * Address: 0x45BA50, __cdecl
 *
 * Calls IDirectDrawSurface4::Restore on the primary surface and
 * re-sets pixel format. Called after display mode change or Alt+Tab.
 *
 * Called by: DDRAW_UnlockPrimary (on surface loss)
 *
 * @param surface  IDirectDrawSurface4* for primary surface
 * @param param2   (undefined)
 */
void __cdecl DDRAW_RestoreSurfaces(int* surface, uint32_t param2);

/**
 * DDRAW_ReleaseSurfaces — release all DirectDraw surfaces and object.
 * Address: 0x45BAA0, __cdecl
 *
 * Releases backbuffer, primary surface, clippers, DirectDraw object,
 * and the DDRAW DLL handle. Counterpart to DDRAW_GetSurface.
 *
 * Called by: CGWND_InitMode1 (cleanup path)
 */
void __cdecl DDRAW_ReleaseSurfaces(void);

/* ================================================================== */
/* C free functions — Audio management                                 */
/* ================================================================== */

/**
 * DDRAW_InitAudio — initialize DirectSound.
 * Address: 0x45B7E0, __cdecl
 *
 * Creates GameAudio object, initializes DirectSound with cooperative
 * level, creates primary buffer, sets format to 22050Hz 16-bit mono.
 * Configures volume bounds from LOCO.INI or defaults (0x4B/0x4E).
 *
 * Called by: CGWND_InitMode1 (init sequence)
 *
 * @return  1 on success, 0 on failure
 */
uint32_t __cdecl DDRAW_InitAudio(void);

/**
 * DDRAW_DestroyAudio — shut down DirectSound.
 * Address: 0x45BB20, __cdecl
 *
 * Saves volume levels to LOCO.INI, calls GameAudio_Cleanup, destroys
 * GameAudio object.
 *
 * Called by: CGWND_Cleanup
 */
void __cdecl DDRAW_DestroyAudio(void);

/* ================================================================== */
/* C free functions — Error strings                                    */
/* ================================================================== */

/**
 * DDRAW_GetDdrawErrorString — DirectDraw error code to string.
 * Address: 0x45BBC0, __cdecl
 *
 * Large switch over common DDERR_* and D3DERR_* error codes.
 * Returns human-readable string. Unknown codes return generic message.
 *
 * @param hresult  DirectDraw/D3D HRESULT error code
 * @return         pointer to error string literal
 */
char* __cdecl DDRAW_GetDdrawErrorString(int32_t hresult);

/**
 * DDRAW_GetDsoundErrorString — DirectSound error code to string.
 * Address: 0x45C2E0, __cdecl
 *
 * Smaller switch over DSERR_* error codes.
 *
 * @param hresult  DirectSound HRESULT error code
 * @return         pointer to error string literal
 */
char* __cdecl DDRAW_GetDsoundErrorString(int32_t hresult);

/* ================================================================== */
/* C free functions — Clipper management                               */
/* ================================================================== */

/**
 * DDRAW_Init — high-level DDRAW initialization.
 * Address: 0x45C8A0, __cdecl
 *
 * Creates thumbpal bitmap via UIPANEL_StretchBlit from resource
 * "smisc/thumbpal.bmp". Returns success/failure.
 *
 * Called by: CGWND_InitAllSubsystems (init sequence)
 *
 * @return  TRUE if thumbpal bitmap loaded successfully
 */
bool __cdecl DDRAW_Init(void);

/**
 * DDRAW_ReleaseClippers — release all DirectDraw clipper objects.
 * Address: 0x45C970, __cdecl
 *
 * Releases 7 clipper objects stored at 0x4FF0F8-0x4FF110
 * (including UIPANEL_Surface at 0x4FF110).
 *
 * Called by: DDRAW_ReleaseSurfaces
 */
void __cdecl DDRAW_ReleaseClippers(void);

/**
 * DDRAW_FreeClipper — zero-initialize a clipper struct entry.
 * Address: 0x45CA10, __fastcall
 *
 * Sets vtable and reference fields to 0. Thin clear helper.
 *
 * @param clipper  pointer to clipper struct (4 dwords)
 */
void __fastcall DDRAW_FreeClipper(void* clipper);

/* ================================================================== */
/* C functions — File/resource data helpers                            */
/* ================================================================== */

/**
 * DDRAW_FileData_Dtor — destructor for FileData struct.
 * Address: 0x45CA20, __fastcall
 *
 * Frees file buffer and decompressed block linked list.
 *
 * @param data  FileData* to destroy
 */
void __fastcall DDRAW_FileData_Dtor(FileData* data);

/**
 * DDRAW_LoadFile — load a game resource file into FileData struct.
 * Address: 0x45CAA0, __thiscall
 *
 * Opens file via WIN32_StreamOpen or AssetMgr, reads into allocated
 * buffer, handling compressed (Huffman) and uncompressed files.
 * Stores decompressed sub-blocks in a linked list. Replaces extension
 * to load accompanying data file.
 *
 * @param path  file path to load
 * @return      1 on success, 0 on failure
 */
uint32_t __thiscall DDRAW_LoadFile(FileData* self, const char* path);

/* ================================================================== */
/* Global state — DirectDraw globals                                   */
/*                                                                     */
/* Interface version confirmed as IDirectDraw4/IDirectDrawSurface4     */
/* (not 7): search_bytes against loco.exe finds IID_IDirectDraw,       */
/* IDirectDraw2, IDirectDraw4, IDirectDrawSurface, Surface2/3/4 in a   */
/* contiguous GUID table (0x4785C8-0x478628) but no IID_IDirectDraw7   */
/* or IDirectDrawSurface7 anywhere in the binary. Consistent with the  */
/* DirectX 6.0 SDK (Aug 1998) determination in NOTE-directx-sdk.md.    */
/* ================================================================== */

extern int*    g_ddraw;               /* 0x4A9908  IDirectDraw4*            */
extern int*    g_primary_surface;     /* 0x4FF0D8  IDirectDrawSurface4*     */
extern int*    g_backbuffer;          /* 0x4FF0DC  IDirectDrawSurface4*     */
extern int16_t g_surface_bpp;         /* 0x485274  0x22B=555, 0x235=565    */
extern int16_t g_surface_bshift;      /* 0x48527A  bit-shift mask           */

/* Game mode / difficulty globals */
extern uint8_t  g_is_town_mode;       /* 0x485328  1 = in-game town mode    */
extern uint8_t  g_ddraw_active;       /* 0x4A9F78  1 = DDraw active         */
extern uint16_t g_game_difficulty;    /* 0x4AA288  1=easy, 2=normal, 3=hard*/

extern void* g_active_panel;          /* 0x4FD3E0  active UI panel override
                                        * (corrected: disassembly of
                                        * DDRAW_SelectBuilding/0x459180 stores
                                        * through [0x4fd3e0]; 0x4852A0 below is
                                        * g_town_view, not this global)         */
/* g_tooltip_mgr declared once, above (see that declaration for the real
 * UI_Manager backing object and construction evidence). */
extern void* g_town_view;             /* 0x4852A0  town view object
                                        * (corrected: matches game/World.cpp and
                                        * world/tilemap.cpp's g_town_view decls,
                                        * and the literal AND-mask operand read
                                        * at DDRAW_SelectBuilding/0x459180.
                                        * 0x4A99C8 was g_active_panel's mistaken
                                        * address, swapped with this entry.)     */
extern int    g_demo_mode;            /* demo mode flag                     */
