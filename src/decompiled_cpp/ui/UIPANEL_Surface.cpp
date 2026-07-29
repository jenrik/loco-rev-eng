/**
 * UIPANEL_Surface.cpp — UIPANEL_Surface (LOCOBITMAP) DDraw wrapper implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * UIPANEL_Surface is a 0x20-byte struct wrapping either a software 8bpp pixel
 * buffer or a DirectDraw offscreen surface. The struct has its own vtable
 * (VTBL_UIPANEL_SURFACE, 0x477D28) for its scalar deleting destructor.
 *
 * Struct layout (0x20 bytes):
 *   +0x00  vtable (void**)              VTBL_UIPANEL_SURFACE
 *   +0x04  mode (int32_t)               0=software 8bpp buffer, 1=DDraw surface
 *   +0x08  width (int32_t)              Surface width in pixels
 *   +0x0C  height (int32_t)             Surface height in pixels
 *   +0x10  has_palette (uint8_t)        1 if palette is owned, 0 if shared
 *   +0x11  (uint8_t)                    padding
 *   +0x14  palette (uint16_t*)          uint16_t[256] color lookup table
 *   +0x18  pixels (uint8_t*)            8bpp pixel buffer (mode=0) or NULL (mode=1)
 *   +0x1C  ddraw_surf (IDirectDrawSurface4*)  DDraw surface (mode=1)
 *
 * Global counter: DAT_00485254 tracks number of active surface allocations.
 * Global palette scratch: DAT_0048524c + DAT_00485250 for shared palette.
 */

#include "UIPANEL.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* ================================================================== */
/* UIPANEL_Surface struct — 0x20 bytes, grounded in Ghidra disassembly */
/*                                                                     */
/* Address references:                                                 */
/*   UIPANEL_CreateSurface:  0x42A110 (ctor)                          */
/*   UIPANEL_DestroySurface: 0x42A140 (dtor, vtable[0])               */
/*   UIPANEL_CopySurface:    0x42A1C0                                 */
/*   UIPANEL_LockSurface:    0x42A370 (mismamed — releases resources) */
/*   UIPANEL_UnlockSurface:  0x42A3D0 (converts sw→DDraw)             */
/*   UIPANEL_BlitSurface:    0x42A540 (collision detection)           */
/*   UIPANEL_FreeAllSurfaces: 0x42A5F0                                */
/*   UIPANEL_FillRect:       0x42A610                                 */
/*   UIPANEL_InitSurface:    0x42A850                                 */
/*   UIPANEL_ClearSurface:   0x42A980                                 */
/*   UIPANEL_SetClipRect:    0x42AA90                                 */
/*   UIPANEL_StretchBlit:    0x42AB10 (BMP loader)                    */
/*   UIPANEL_ReadPaletteFromBMP: 0x42AF30                             */
/*   UIPANEL_Blit:           0x42B050 (central blit dispatcher, 105+ callers) */
/*                                                                     */
/* Vtable: VTBL_UIPANEL_SURFACE (0x477D28)                            */
/*   [0] scalar deleting destructor (UIPANEL_DestroySurface)          */
/* ================================================================== */

/* DDraw stub types for host build (the real SDL3 bridge is in
 * src/sdl3_shims/sdl3_ddraw.h; files that need the full bridge
 * include it directly.) */
struct DDSCAPS2 { DWORD dwCaps; DWORD dwCaps2; DWORD dwCaps3; DWORD dwCaps4; };
struct DDSURFACEDESC { DWORD dwSize; DWORD dwFlags; DWORD dwHeight; DWORD dwWidth; LONG lPitch; void* lpSurface; DWORD dwBackBufferCount; DWORD dwMipMapCount; DWORD dwAlphaBitDepth; DWORD dwReserved; DDSCAPS2 ddsCaps; };
struct IDirectDrawSurface4;
struct IDirectDraw4 { void* vtable; int Release() { return 0; } int CreateSurface(void*, IDirectDrawSurface4**, void*) { return 0; } };
struct IDirectDrawSurface4 { void* vtable; int Release() { return 0; } int Blt(void*, void*, void*, int, void*) { return 0; } int Lock(void*, void*, int, int) { return 0; } int Unlock(void*) { return 0; } };
struct DDBLTFX { DWORD dwSize; DWORD dwFillColor; DWORD dwDDFX; };

/* UIPANEL_Surface is defined in graphics/LOCOBITMAP.h (pulled in transitively
 * via UIPANEL.h above). Its `ddraw_surf` member is a raw void* there, so this
 * file casts to IDirectDrawSurface4* at each use, and its palette pointer is
 * `palette_ptr` (uint32_t*), cast to uint16_t* since the data is a
 * uint16_t[256] lookup table. */
#define FAILED(hr) ((int)(hr) < 0)
#define SUCCEEDED(hr) ((int)(hr) >= 0)
#define DDBLT_WAIT 0x00000010
#define DDBLT_COLORFILL 0x00000400
#define DDBLT_KEYSRC 0x00008000
#define DDSD_CAPS 0x00000001
#define DDSD_WIDTH 0x00000004
#define DDSD_HEIGHT 0x00000002
#define DDSCAPS_OFFSCREENPLAIN 0x00000040
#define DDSCAPS_SYSTEMMEMORY 0x00000800
#define DDLOCK_WAIT 0x00000001

#define VTBL_UIPANEL_SURFACE ((void*)0x477D28)

/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

/* operator_new, GLOBAL_free already declared via compat.h */
    void*  GLOBAL_malloc(size_t size);
    void   GLOBAL_free_sized(void* ptr, size_t size);
    void   CRT_free(void* ptr);
    void   WIN32_FatalError(void);
extern "C" {
    int    __stdcall OutputDebugStringA(LPCSTR lpOutputString);
}

    /* DirectDraw globals */
    extern IDirectDraw4*      g_ddraw;               /* 0x4FD394 */
    extern IDirectDrawSurface4* g_primary_surface;   /* 0x4FD3C4 */
    extern IDirectDrawSurface4* g_backbuffer;        /* 0x4FD3C0 */
    extern void*              g_asset_mgr;           /* 0x4FD3CC */
    extern char               g_surface_lost;         /* 0x4FD218 */

    /* Pixel format globals */
    extern int  g_surface_bshift;                    /* 0x485280 */
    extern int  g_surface_channel1;                  /* 0x485278 */
    extern int  g_surface_channel2;                  /* 0x48527C */
    extern int  g_surface_bpp;                       /* 0x485274 */
    extern int  g_pixel_format_mask;                 /* 0x485248 */
    extern int  g_pixel_format_ch1;                  /* 0x48528C */
    extern int  g_pixel_format_ch2;                  /* 0x485290 */

    /* Global palette scratch area */
    extern void* DAT_0048524c;                       /* 0x48524C — shared palette buffer */
    extern int   DAT_00485250;                       /* 0x485250 — shared palette refcount */
    extern int   DAT_00485254;                       /* 0x485254 — surface allocation counter */

    /* Memory stream / file helpers */
    int  WIN32_StreamOpen(void* stream, int mode);
    int  WIN32_StreamOpenPath(void* stream, LPCSTR path, int flags, int unk);
    void WIN32_StreamDestroy(int stream);
    void WIN32_StreamDestroyImmediate(void* stream);
    void WIN32_StreamRead(void* stream, void* buf, uint32_t size);
    void Stream_BeginRead(void* stream, uint32_t offset, int mode);
    void* WNDPROC_StreamFromMemory(void* obj, char* data, int size, int mode);
    void WNDPROC_StreamCleanup(void* stream);
    void* AssetMgr_LoadFile(void* mgr, void* path, int* out_size);
    int   DDRAW_RestoreSurfaces(int* surf, void* desc);
    void* DDRAW_GetDdrawErrorString(int code);
    HDC   DDRAW_LoadBmpToSurface(LPCSTR path, int bpp, int unk1, int unk2, char unk3);
    void  DDRAW_GetSurfaceWidthHeight(int* out_w, int* out_h);

    /* Town tile rendering functions */
    bool __thiscall Town_InitTileCache(
        void* self, int src_x, int src_y, int dest_x, int dest_y,
        uint8_t* dest_surface, uint32_t dest_pitch,
        int clip_left, int clip_top, int clip_right, int clip_bottom);
    void __thiscall Town_DrawTile(void*, int, int, int, int, void*, int, int, int, int, int);
    void __thiscall Town_DrawTiles16bpp_Strided(void*, int, int, int, int, void*, int, int, int, int, int);
    void __thiscall Town_FlushTileCache(void*, int, int, int, int, void*, int, int, int, int, int);
    void __thiscall Town_DrawCachedTile(void*, int, int, int, int, void*, int, int, int, int, int);
    void __thiscall Town_DrawTileEx(void*, int, int, int, int, void*, int, int, int, int, int);
    void __thiscall Town_BlitTileSurface(void*, int, int, int, int, void*, int, int, int, int, int);
    void __thiscall Town_DrawTiles16bpp_Reversed(void*, int, int, int, int, void*, int, int, int, int, int);
    void __thiscall Town_DrawTiles16bpp_Checker(void*, int, int, int, int, void*, int, int, int, int, int);
    void __thiscall Town_DrawTiles16bpp_Staggered(void*, int, int, int, int, void*, int, int, int, int, int);
    void __thiscall Town_DrawTileLine(void*, int, int, int, int, void*, int, int, int, int, int);

    /* UIPANEL surface helpers (forward-declared, defined later) */
    uint32_t __thiscall UIPANEL_ClearSurface(void* surface, int width, int height);
    uint32_t __thiscall UIPANEL_ReadPaletteFromBMP(void* surface, void* stream);

    uint32_t __thiscall UIPANEL_InitSurface(void* surface,
    int width, int height, int mode, uint32_t palette_param, uint8_t fill_byte)
{
    /* Grounded: 0x42A850 — initialize/reinit UIPANEL_Surface.
     * Disassembly: stores mode at +0x04, clears +0x10 (has_palette),
     * frees old +0x18 (pixels), allocates new buffer (mode=0) or
     * creates DDraw surface (mode=1). */
    UIPANEL_Surface* s = (UIPANEL_Surface*)surface;

    /* Set mode */
    s->mode = mode;
    s->has_palette = 0;

    /* Free existing pixel buffer */
    if (s->pixels != NULL) {
        GLOBAL_free(s->pixels);
        s->pixels = NULL;
    }

    if (mode == 0) {
        /* Software buffer mode */
        UIPANEL_ClearSurface(surface, width, height);  /* allocates pixel buffer */

        /* Share global palette if small enough */
        if ((palette_param & 0xFFFF) <= (uint32_t)DAT_00485250) {
            s->palette_ptr = (uint32_t*)DAT_0048524c;
        }

        /* Fill pixel buffer with fill_byte pattern */
        if (s->pixels != NULL) {
            int buf_size = height * width;
            uint8_t fill_pattern = fill_byte;
            uint32_t fill_dword = (fill_pattern << 24) | (fill_pattern << 16) |
                                  (fill_pattern << 8) | fill_pattern;
            uint32_t* pw = (uint32_t*)s->pixels;
            for (int i = buf_size / 4; i > 0; i--) {
                *pw++ = fill_dword;
            }
            uint8_t* pb = (uint8_t*)pw;
            for (int i = buf_size & 3; i > 0; i--) {
                *pb++ = fill_pattern;
            }
        }
    }

    if (mode == 1) {
        /* DDraw surface mode */
        UIPANEL_ClearSurface(surface, width, height);  /* creates DDraw surface */

        /* If previous mode was 0 (had pixel buffer), try DDraw Blt color-fill */
        if (s->mode == 0 && s->pixels != NULL) {
            int buf_size = width * height;
            memset(s->pixels, 0, buf_size);
        } else if (s->ddraw_surf != NULL) {
            DDBLTFX bltFx;
            bltFx.dwSize = sizeof(DDBLTFX);
            bltFx.dwFillColor = 0;
            ((IDirectDrawSurface4*)s->ddraw_surf)->Blt(NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &bltFx);
        }
    }

    return 1;
}uint32_t __thiscall UIPANEL_ClearSurface(void* surface, int width, int height)
{
    /* Grounded: 0x42A980 — reallocate storage for new dimensions.
     * Disassembly: reads old mode from +0x04, stores new w/h at +0x08/+0x0C,
     * frees+reallocs +0x18 (pixels) for mode=0, or releases+recreates
     * +0x1C (ddraw_surf) for mode=1. */
    UIPANEL_Surface* s = (UIPANEL_Surface*)surface;
    int old_mode = s->mode;

    /* Update dimensions */
    s->width  = width;
    s->height = height;

    if (old_mode == 0) {
        /* Free old pixel buffer and allocate new */
        if (s->pixels != NULL) {
            GLOBAL_free(s->pixels);
            s->pixels = NULL;
        }

        void* new_pixels = operator_new(height * width);
        s->pixels = (uint8_t*)new_pixels;
        if (new_pixels == NULL) {
            return 0;
        }
    } else if (old_mode == 1) {
        /* Release old DDraw surface */
        if (s->ddraw_surf != NULL) {
            ((IDirectDrawSurface4*)s->ddraw_surf)->Release();
            s->ddraw_surf = NULL;
        }

        /* Create new DDraw offscreen surface */
        DDSURFACEDESC desc;
        memset(&desc, 0, sizeof(desc));
        desc.dwSize = sizeof(desc);
        desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
        desc.dwHeight = height;
        desc.dwWidth = width;
        desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;

        IDirectDrawSurface4* new_surf = NULL;
        HRESULT hr = g_ddraw->CreateSurface(&desc, &new_surf, NULL);
        if (FAILED(hr)) {
            DDRAW_GetDdrawErrorString(1);
            return 0;
        }

        s->ddraw_surf = new_surf;
        DDRAW_RestoreSurfaces((int*)new_surf, (int*)&desc);
    }

    return 1;
}void __thiscall UIPANEL_SetClipRect(void* surface, uint8_t fill_byte, uint32_t blit_flags)
{
    /* Grounded: 0x42AA90 — fill surface with solid color pattern.
     * Disassembly: reads mode from +0x04. Mode=0: memset +0x18 (pixels).
     * Mode=1: DDraw Blt color-fill on +0x1C (ddraw_surf). */
    UIPANEL_Surface* s = (UIPANEL_Surface*)surface;

    if (s->mode == 0) {
        /* Software mode: fill pixel buffer */
        if (s->pixels != NULL) {
            int buf_size = s->height * s->width;
            uint8_t fill = fill_byte;
            uint32_t fill_dword = (fill << 24) | (fill << 16) | (fill << 8) | fill;
            uint32_t* pw = (uint32_t*)s->pixels;
            for (int i = buf_size / 4; i > 0; i--) {
                *pw++ = fill_dword;
            }
            uint8_t* pb = (uint8_t*)pw;
            for (int i = buf_size & 3; i > 0; i--) {
                *pb++ = fill;
            }
        }
        return;
    }

    /* DDraw mode: color-fill via Blt */
    if (s->ddraw_surf != NULL) {
        DDBLTFX bltFx;
        bltFx.dwSize = sizeof(DDBLTFX);
        bltFx.dwFillColor = blit_flags;
        ((IDirectDrawSurface4*)s->ddraw_surf)->Blt(NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &bltFx);
    }
}uint8_t __thiscall UIPANEL_StretchBlit(void* surface, LPCSTR file_path,
    uint32_t param_2, int param_3, int param_4)
{
    /* Grounded: 0x42AB10 — load BMP file onto UIPANEL_Surface.
     * Disassembly: frees old +0x18 (pixels), reads BMP header, palette,
     * loads pixels bottom-up (mode=0, 8-bit) or uses DDRAW_LoadBmpToSurface
     * (mode=1, hi-color). Stores result at +0x1C (ddraw_surf) or +0x18 (pixels). */
    UIPANEL_Surface* s = (UIPANEL_Surface*)surface;

    void* stream_buf[6];         /* WIN32_Stream struct */
    void* asset_data = NULL;
    int asset_size = 0;
    void* mem_stream = NULL;
    void* stream = NULL;
    uint8_t result = 1;

    /* Initialize stream */
    WIN32_StreamOpen(stream_buf, 1);

    /* Free existing pixel buffer */
    if (s->pixels != NULL) {
        GLOBAL_free(s->pixels);
        s->pixels = NULL;
    }

    /* Try loading from asset manager first */
    if (g_asset_mgr != NULL) {
        int path_len = strlen(file_path);
        int install_len = strlen((LPCSTR)0x4852B8);  /* g_install_path */
        LPCSTR rel_path = file_path + install_len;
        if (*rel_path == '\\') rel_path++;

        asset_data = AssetMgr_LoadFile(g_asset_mgr, (void*)rel_path, &asset_size);
        if (asset_data != NULL) {
            mem_stream = operator_new(0x5C);
            if (mem_stream != NULL) {
                stream = WNDPROC_StreamFromMemory(mem_stream, (char*)asset_data, asset_size, 1);
            }
        }
    }

    /* Fallback to disk file if asset manager didn't provide it */
    if (stream == NULL) {
        WIN32_StreamOpenPath(stream_buf, file_path, 0xA0, 0x479190);
        if (*(int*)((uintptr_t)(*(int*)((intptr_t)stream_buf + 4)) + 0x4C) != -1) {
            stream = stream_buf;
        }
        if (stream == NULL) {
            goto cleanup;
        }
    }

    /* Read BMP header */
    struct {
        uint16_t bfType;
        uint32_t bfSize;
        uint32_t reserved;
        uint32_t bfOffBits;
    } bmp_file_header;

    struct {
        uint32_t biSize;
        int32_t  biWidth;
        int32_t  biHeight;
        uint16_t biPlanes;
        uint16_t biBitCount;
        uint32_t biCompression;
        uint32_t biSizeImage;
        int32_t  biXPelsPerMeter;
        int32_t  biYPelsPerMeter;
        uint32_t biClrUsed;
        uint32_t biClrImportant;
    } bmp_info_header;

    WIN32_StreamRead(stream, &bmp_file_header, sizeof(bmp_file_header));
    if (bmp_file_header.bfType != 0x4D42) { /* 'BM' */
        goto cleanup;
    }

    WIN32_StreamRead(stream, &bmp_info_header, sizeof(bmp_info_header));

    /* Path A: 8-bit indexed BMP with palette (software buffer mode) */
    if (bmp_info_header.biBitCount == 8 &&
        s->mode != 1 &&
        param_3 == 0 && param_4 == 0) {

        s->mode = 0;

        /* Allocate header copy struct for palette + info */
        void* header_copy = operator_new(0x428);
        if (header_copy == NULL) goto cleanup;
        memcpy(header_copy, &bmp_info_header, sizeof(bmp_info_header));

        /* Read palette from BMP */
        if (!UIPANEL_ReadPaletteFromBMP(surface, stream)) {
            GLOBAL_free(header_copy);
            goto cleanup;
        }

        /* Set dimensions */
        s->width  = bmp_info_header.biWidth;
        s->height = bmp_info_header.biHeight;

        /* Allocate pixel buffer */
        int buf_size = bmp_info_header.biWidth * bmp_info_header.biHeight;
        void* pixels = operator_new(buf_size);
        s->pixels = (uint8_t*)pixels;
        if (pixels == NULL) goto cleanup;
        memset(pixels, 0, buf_size);

        /* Read pixel rows (BMP is bottom-up) */
        int row_stride = bmp_info_header.biWidth;
        if ((row_stride & 3) != 0) {
            row_stride = (row_stride & ~3) + 4;  /* align to 4 bytes */
        }

        uint8_t* dest_row = s->pixels + (bmp_info_header.biHeight - 1) * bmp_info_header.biWidth;
        int padding = row_stride - bmp_info_header.biWidth;

        for (int y = 0; y < bmp_info_header.biHeight; y++) {
            WIN32_StreamRead(stream, dest_row, bmp_info_header.biWidth);
            dest_row -= bmp_info_header.biWidth;
            if (padding > 0) {
                Stream_BeginRead(stream, padding, 1);
            }
        }
    } else {
        /* Path B: hi-color or DDraw surface — use DDRAW helper */
        WIN32_StreamDestroyImmediate(stream_buf);

        IDirectDrawSurface4* new_surf = DDRAW_LoadBmpToSurface(file_path, 16, param_3, param_4, 0);
        s->ddraw_surf = new_surf;
        s->mode = 1;

        /* Get surface dimensions */
        uint32_t surf_w, surf_h;
        DDRAW_GetSurfaceWidthHeight(&surf_w, &surf_h);
        s->width  = (int32_t)(surf_w & 0xFFFF);
        s->height = (int32_t)(surf_h & 0xFFFF);
    }

cleanup:
    if (mem_stream != NULL) {
        void (**dtor)(void*) = *(void***)mem_stream;
        (*dtor)(mem_stream);
    }

    if (stream != NULL && stream != stream_buf) {
        WIN32_StreamDestroyImmediate(stream);
    }

    if (asset_data != NULL) {
        CRT_free(asset_data);
    }

    if (*(int*)((uintptr_t)(*(int*)((intptr_t)stream_buf + 4)) + 0x4C) != -1) {
        WIN32_StreamDestroyImmediate(stream_buf);
    }

    WIN32_StreamDestroy(*(int*)((intptr_t)stream_buf + 8));

    return result;
}uint32_t __thiscall UIPANEL_ReadPaletteFromBMP(void* surface, void* stream)
{
    /* Grounded: 0x42AF30 — read 256-color palette from BMP stream.
     * Disassembly: accesses +0x10 (has_palette), allocates +0x14 (palette)
     * as uint16_t[256], reads 1024 bytes of RGBQUAD, converts to 16-bit. */
    UIPANEL_Surface* s = (UIPANEL_Surface*)surface;

    uint16_t* palette;
    int has_own_palette = 0;

    if (s->has_palette == 0 || DAT_00485250 != 0) {
        /* Private palette */
        s->has_palette = 1;
        palette = (uint16_t*)operator_new(0x200);     /* 512 bytes */
        s->palette_ptr = (uint32_t*)palette;
        if (palette == NULL) {
            GLOBAL_free(DAT_0048524c);
            DAT_0048524c = NULL;
            return 0;
        }
    } else {
        /* Shared global palette */
        s->has_palette = 0;
        palette = (uint16_t*)operator_new(0x200);
        DAT_0048524c = palette;
        if (palette == NULL) return 0;
        s->palette_ptr = (uint32_t*)palette;
        DAT_00485250++;
    }

    /* Read 256 RGBQUAD entries (1024 bytes) from stream */
    uint8_t rgb_quads[1024];
    WIN32_StreamRead(stream, rgb_quads, 1024);

    /* Convert each RGBQUAD to 16-bit RGB 5-6-5 */
    for (int i = 0; i < 256; i++) {
        uint8_t* quad = rgb_quads + i * 4;
        uint8_t b = quad[0];
        uint8_t g = quad[1];
        uint8_t r = quad[2];

        /* Convert to 16-bit: R[4:0] G[5:0] B[4:0] */
        uint16_t color = (uint16_t)(
            ((r >> 3) << 11) |
            ((g >> 2) << 5)  |
            (b >> 3)
        );
        palette[i] = color;
    }

    return 1;
}/* ================================================================== */
/* UIPANEL_Blit — Main blit dispatcher for all UIPANEL rendering       */
/* Address: 0x42B050                                                   */
/*                                                                     */
/* This is the central blit function with 105+ callers across the      */
/* entire rendering subsystem. It dispatches to specific tile drawing   */
/* functions based on a flags parameter.                                */
/*                                                                     */
/* Flag dispatch:                                                      */
/*   0x00     = Town_DrawTile (base tile drawing)                      */
/*   0x01/0x03 = Town_InitTileCache (palette-init)                     */
/*   0x02     = Town_DrawTiles16bpp_Strided (standard LTR 16bpp)       */
/*   0x04/0x84 = Town_FlushTileCache (2x2 block expand)               */
/*   0x05/0x85 = Town_DrawCachedTile (2x2 block from cache)           */
/*   0x0F-0x1F = Town_DrawTileEx (3x2 block expand)                   */
/*   0x20     = Town_BlitTileSurface (right-to-left blit)              */
/*   0x22     = Town_DrawTiles16bpp_Reversed (H-mirror 16bpp)          */
/*   0x40     = scroll rect adjustment (Town_CalcScrollRect)           */
/*   0x80     = DDraw Blt (direct blit)                                */
/*   0x100    = Town_DrawTiles16bpp_Checker (checkerboard 16bpp)       */
/*   0x200    = Town_DrawTiles16bpp_Staggered (staggered 16bpp)        */
/*   0x400/0x402 = Town_DrawTileLine (alpha-blended line)              */
/*                                                                     */
/* Parameters: this (tile_map/renderer), src_x, src_y, dest_x, dest_y, */
/*   dest_surface, clip_left, clip_top, clip_right, clip_bottom, flags */
/* ================================================================== */
bool __thiscall UIPANEL_Blit(void* renderer,
    uint32_t src_x, uint32_t src_y, int dest_x, uint32_t dest_y,
    void* dest_surface, uint32_t clip_left, uint32_t clip_top,
    int clip_right, uint32_t clip_bottom, uint32_t flags)
{
    /* Auto-detect scroll when source/dest rects differ and flag not set */
    if ((flags & 0xFFFFFFFB) != 0 && (flags & 0xFFFFFFEF) != 0) {
        if ((clip_right - clip_left) != (dest_x - (int)src_x) &&
            (clip_bottom - clip_top) != (dest_y - src_y)) {
            flags |= 0x80;  /* Enable DDraw Blt fallback */
        }
    }

    /* Handle scroll rect calculation */
    if ((flags & 0x40) != 0) {
        int mode = *(int*)((intptr_t)renderer + 4);  /* +0x04 */
        if (mode == 1) {
            /* Forward scroll rect */
            RECT rect;
            rect.left   = src_x;
            rect.top    = src_y;
            rect.right  = clip_right;
            rect.bottom = clip_bottom;
            void Town_CalcScrollRect(void* r, RECT* clip, void* surf);
            Town_CalcScrollRect(renderer, &rect, dest_surface);
            src_x       = rect.left;
            src_y       = rect.top;
            clip_right  = rect.right;
            clip_bottom = rect.bottom;
        } else if (mode == 0) {
            /* Reversed scroll rect */
            RECT rect;
            rect.left   = src_x;
            rect.top    = src_y;
            rect.right  = clip_right;
            rect.bottom = clip_bottom;
            void Town_CalcScrollRect_Reversed(void* r, RECT* clip, void* surf);
            Town_CalcScrollRect_Reversed(renderer, &rect, dest_surface);
            src_x       = rect.left;
            src_y       = rect.top;
            clip_right  = rect.right;
            clip_bottom = rect.bottom;
        }
        flags &= 0xFFFFFFBF;
    }

    /* Determine surface type */
    int surf_mode = *(int*)((intptr_t)renderer + 4);  /* +0x04 */

    if (surf_mode == 1) {
        DDSURFACEDESC desc;
        uint8_t* locked_pixels = NULL;
        uint32_t pitch = 0;

        /* Handle surface lost */
        if (g_surface_lost) {
            IDirectDrawSurface4* primary = *(IDirectDrawSurface4**)0x4FD3C4;
            if (primary != NULL) {
                HRESULT hr = primary->Unlock(NULL);
                if (SUCCEEDED(hr)) {
                    g_surface_lost = 0;
                }
            }
        }

        /* DDraw surface mode: lock surface and blit */
        memset(&desc, 0, sizeof(desc));
        desc.dwSize = sizeof(desc);

        HRESULT hr;
        if (flags == 0 || flags == 1) {
            /* Simple Blt via DDraw */
            RECT src_rect;
            src_rect.left   = clip_left;
            src_rect.top    = clip_top;
            src_rect.right  = clip_right;
            src_rect.bottom = clip_bottom;

            RECT dest_rect;
            dest_rect.left   = src_x;
            dest_rect.top    = src_y;
            dest_rect.right  = dest_x;
            dest_rect.bottom = dest_y;

            IDirectDrawSurface4* src_ddraw = (IDirectDrawSurface4*)((UIPANEL_Surface*)renderer)->ddraw_surf;
            IDirectDrawSurface4* dst_ddraw = (IDirectDrawSurface4*)dest_surface;

            uint32_t blt_flags = (flags == 0) ? DDBLT_WAIT : (DDBLT_WAIT | DDBLT_KEYSRC);
            hr = dst_ddraw->Blt(&dest_rect, src_ddraw, &src_rect, blt_flags, NULL);
            return SUCCEEDED(hr);
        } else if (flags == 0x80) {
            /* Direct Blt using dest surface */
            RECT src_r = { (int)src_x, (int)src_y, dest_x, (int)dest_y };
            RECT clip_r = { (int)clip_left, (int)clip_top, clip_right, (int)clip_bottom };
            IDirectDrawSurface4* src_ddraw = (IDirectDrawSurface4*)((UIPANEL_Surface*)renderer)->ddraw_surf;
            hr = ((IDirectDrawSurface4*)dest_surface)->Blt(&clip_r, src_ddraw, &src_r, DDBLT_WAIT, NULL);
            return SUCCEEDED(hr);
        } else {
            /* Lock surface for pixel-level blit */
            RECT lock_rect = { (int)clip_left, (int)clip_top, clip_right, (int)clip_bottom };
            IDirectDrawSurface4* surf = (IDirectDrawSurface4*)dest_surface;
            hr = surf->Lock(&lock_rect, &desc, DDLOCK_WAIT, NULL);
            if (FAILED(hr)) {
                return false;
            }
            locked_pixels = (uint8_t*)desc.lpSurface;
            pitch = desc.lPitch;
        }

        /* Dispatch to tile rendering function */
        bool success = true;

        switch (flags & 0xFF) {
        case 0x00:
            Town_DrawTile(renderer, src_x, src_y, dest_x, dest_y,
                         locked_pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            break;
        case 0x01:
        case 0x03:
            Town_InitTileCache(renderer, src_x, src_y, dest_x, dest_y,
                             locked_pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            break;
        case 0x02:
            Town_DrawTiles16bpp_Strided(renderer, src_x, src_y, dest_x, dest_y,
                                       locked_pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            break;
        case 0x04:
            Town_FlushTileCache(renderer, src_x, src_y, dest_x, dest_y,
                              locked_pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            break;
        case 0x05:
            Town_DrawCachedTile(renderer, src_x, src_y, dest_x, dest_y,
                              locked_pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            break;
        default:
            if (flags >= 0x10 && flags < 0x20) {
                Town_DrawTileEx(renderer, src_x, src_y, dest_x, dest_y,
                              locked_pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            } else {
                Town_DrawTile(renderer, src_x, src_y, dest_x, dest_y,
                            locked_pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            }
            break;
        }

        if (flags >= 0x20) {
            if (flags == 0x20) {
                Town_BlitTileSurface(renderer, src_x, src_y, dest_x, dest_y,
                                   locked_pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            } else if (flags == 0x22) {
                Town_DrawTiles16bpp_Reversed(renderer, src_x, src_y, dest_x, dest_y,
                                           locked_pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            } else if (flags == 0x84 || flags == 0x04) {
                Town_FlushTileCache(renderer, src_x, src_y, dest_x, dest_y,
                                  locked_pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            } else if (flags == 0x85 || flags == 0x05) {
                Town_DrawCachedTile(renderer, src_x, src_y, dest_x, dest_y,
                                  locked_pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            } else if (flags == 0x102) {
                Town_DrawTiles16bpp_Checker(renderer, src_x, src_y, dest_x, dest_y,
                                          locked_pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            } else if (flags == 0x202) {
                Town_DrawTiles16bpp_Staggered(renderer, src_x, src_y, dest_x, dest_y,
                                            locked_pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            } else if (flags == 0x400 || flags == 0x402) {
                Town_DrawTileLine(renderer, src_x, src_y, dest_x, dest_y,
                                locked_pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            }
        }

        /* Unlock the surface */
        if (locked_pixels != NULL) {
            ((IDirectDrawSurface4*)dest_surface)->Unlock(NULL);
        }

        return success;
    }

    /* Software buffer mode */
    if (dest_surface == (void*)g_primary_surface) {
        if (!g_surface_lost) {
            /* Check surface lost */
            DDSURFACEDESC desc;
            memset(&desc, 0, sizeof(desc));
            desc.dwSize = sizeof(desc);
            HRESULT hr = g_primary_surface->Lock(NULL, &desc, DDLOCK_WAIT, NULL);
            if (SUCCEEDED(hr)) {
                g_surface_lost = 1;
            }
        }
        /* Use DDSURFACEDESC from globals for primary surface */
        uint32_t pitch = *(uint32_t*)0x4FD1AC;
        uint8_t* pixels = *(uint8_t**)0x4FD1C0;

        /* Dispatch to appropriate tile function */
        if (flags < 0x12) {
            if (flags > 0x0F) {
                Town_DrawTileEx(renderer, src_x, src_y, dest_x, dest_y,
                              pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            } else {
                switch (flags) {
                case 0:
                    Town_DrawTile(renderer, src_x, src_y, dest_x, dest_y,
                                pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
                    break;
                case 1:
                case 3:
                    Town_InitTileCache(renderer, src_x, src_y, dest_x, dest_y,
                                     pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
                    break;
                case 2:
                    Town_DrawTiles16bpp_Strided(renderer, src_x, src_y, dest_x, dest_y,
                                              pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
                    break;
                case 4:
                    Town_FlushTileCache(renderer, src_x, src_y, dest_x, dest_y,
                                      pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
                    break;
                case 5:
                    Town_DrawCachedTile(renderer, src_x, src_y, dest_x, dest_y,
                                      pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
                    break;
                default:
                    Town_DrawTile(renderer, src_x, src_y, dest_x, dest_y,
                                pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
                    break;
                }
            }
        } else {
            if (flags == 0x20) {
                Town_BlitTileSurface(renderer, src_x, src_y, dest_x, dest_y,
                                   pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            } else if (flags == 0x22) {
                Town_DrawTiles16bpp_Reversed(renderer, src_x, src_y, dest_x, dest_y,
                                           pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            } else if (flags == 0x84 || flags == 0x04) {
                Town_FlushTileCache(renderer, src_x, src_y, dest_x, dest_y,
                                  pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            } else if (flags == 0x85 || flags == 0x05) {
                Town_DrawCachedTile(renderer, src_x, src_y, dest_x, dest_y,
                                  pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            } else if (flags == 0x102) {
                Town_DrawTiles16bpp_Checker(renderer, src_x, src_y, dest_x, dest_y,
                                          pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            } else if (flags == 0x202) {
                Town_DrawTiles16bpp_Staggered(renderer, src_x, src_y, dest_x, dest_y,
                                            pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            } else if (flags == 0x400 || flags == 0x402) {
                Town_DrawTileLine(renderer, src_x, src_y, dest_x, dest_y,
                                pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            } else {
                Town_DrawTile(renderer, src_x, src_y, dest_x, dest_y,
                            pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            }
        }

        /* Unlock primary surface if needed */
        if (g_surface_lost) {
            g_primary_surface->Unlock(NULL);
            g_surface_lost = 0;
        }
    } else {
        /* Lock the custom surface */
        DDSURFACEDESC desc;
        memset(&desc, 0, sizeof(desc));
        desc.dwSize = sizeof(desc);
        IDirectDrawSurface4* surf = (IDirectDrawSurface4*)dest_surface;
        HRESULT hr = surf->Lock(NULL, &desc, DDLOCK_WAIT, NULL);
        if (FAILED(hr)) {
            return false;
        }
        uint8_t* pixels = (uint8_t*)desc.lpSurface;
        uint32_t pitch = desc.lPitch;

        /* Dispatch to appropriate tile function (same pattern as above) */
        if (flags < 0x12) {
            if (flags > 0x0F) {
                Town_DrawTileEx(renderer, src_x, src_y, dest_x, dest_y,
                              pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            } else {
                switch (flags) {
                case 0:
                    Town_DrawTile(renderer, src_x, src_y, dest_x, dest_y,
                                pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
                    break;
                case 1: case 3:
                    Town_InitTileCache(renderer, src_x, src_y, dest_x, dest_y,
                                     pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
                    break;
                case 2:
                    Town_DrawTiles16bpp_Strided(renderer, src_x, src_y, dest_x, dest_y,
                                              pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
                    break;
                case 4:
                    Town_FlushTileCache(renderer, src_x, src_y, dest_x, dest_y,
                                      pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
                    break;
                case 5:
                    Town_DrawCachedTile(renderer, src_x, src_y, dest_x, dest_y,
                                      pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
                    break;
                default:
                    Town_DrawTile(renderer, src_x, src_y, dest_x, dest_y,
                                pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
                    break;
                }
            }
        } else {
            if (flags == 0x20) {
                Town_BlitTileSurface(renderer, src_x, src_y, dest_x, dest_y,
                                   pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            } else if (flags == 0x22) {
                Town_DrawTiles16bpp_Reversed(renderer, src_x, src_y, dest_x, dest_y,
                                           pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            } else if (flags == 0x84 || flags == 0x04) {
                Town_FlushTileCache(renderer, src_x, src_y, dest_x, dest_y,
                                  pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            } else if (flags == 0x85 || flags == 0x05) {
                Town_DrawCachedTile(renderer, src_x, src_y, dest_x, dest_y,
                                  pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            } else if (flags == 0x102) {
                Town_DrawTiles16bpp_Checker(renderer, src_x, src_y, dest_x, dest_y,
                                          pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            } else if (flags == 0x202) {
                Town_DrawTiles16bpp_Staggered(renderer, src_x, src_y, dest_x, dest_y,
                                            pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            } else if (flags == 0x400 || flags == 0x402) {
                Town_DrawTileLine(renderer, src_x, src_y, dest_x, dest_y,
                                pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            } else {
                Town_DrawTile(renderer, src_x, src_y, dest_x, dest_y,
                            pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
            }
        }

        surf->Unlock(NULL);
    }

    return true;
}
