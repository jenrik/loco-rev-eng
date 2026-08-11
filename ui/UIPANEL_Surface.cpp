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
 * Global counter: g_surface_alloc_counter tracks number of active surface allocations.
 * Global palette scratch: g_shared_palette_buffer + g_shared_palette_refcount for shared palette.
 */

// Status: TRANSCRIBED

#include "UIPANEL.h"
#include "UIPANEL_Surface.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
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
 * file casts to IDirectDrawSurface4* at each use. `palette_ptr` is
 * `uint16_t*` (a 256-entry lookup table) — no cast needed. */
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
    extern void* g_shared_palette_buffer;              /* 0x48524C — shared palette buffer */
    extern int   g_shared_palette_refcount;            /* 0x485250 — shared palette refcount */
    extern int   g_surface_alloc_counter;              /* 0x485254 — surface allocation counter */

    /* Memory stream / file helpers */
    int  WIN32_StreamOpen(void* stream, int mode);
    int  WIN32_StreamOpenPath(void* stream, LPCSTR path, int flags, int unk);
    extern size_t WIN32_Stream_Size(); /* resources/Win32Stream.cpp — real sizeof(WIN32_Stream) */
    void WIN32_StreamDestroy(void* stream);
    void WIN32_StreamDestroyImmediate(void* stream);
    /* Real def (shared/link_stubs.cpp) is extern "C" (plain, unmangled) —
     * a C++-linkage declaration here mangles to a distinct, unlinked
     * symbol regardless of matching parameter types. */
    extern "C" void WIN32_StreamRead(void* stream, void* buf, int32_t size);
    void Stream_BeginRead(void* stream, uint32_t offset, int mode);
    void* WNDPROC_StreamFromMemory(void* obj, char* data, int size, int mode);
    void WNDPROC_StreamCleanup(void* stream);
    void* AssetMgr_LoadFile(void* mgr, const char* path, int* out_size);
    /* Real def: graphics/sdl3_ddraw.cpp (host path, guarded #ifndef _WIN32);
     * first param is the same IDirectDrawSurface4* typed elsewhere in this
     * file (g_backbuffer/g_primary_surface), not int*. */
    int   DDRAW_RestoreSurfaces(IDirectDrawSurface4* surf, void* desc);
    /* Real def (shared/link_stubs.cpp) is extern "C" (plain, unmangled). */
    extern "C" void* DDRAW_GetDdrawErrorString(int code);
    HDC   DDRAW_LoadBmpToSurface(LPCSTR path, int bpp, int unk1, int unk2, char unk3);
    void  DDRAW_GetSurfaceWidthHeight(void* surface, uint16_t* out_h, uint16_t* out_w);

    /* Town tile rendering functions are UIPANEL_Surface methods
     * (graphics/LOCOBITMAP.h) implemented in town/TownTiles.cpp — see the
     * struct comment there for why they're not free functions here. The
     * `void*, int,int,int,int,void*,int,int,int,int,int` free-function
     * declarations that used to live here didn't match any real symbol
     * (only the typed methods exist), so every call site below was a
     * call-0 landmine before this fix. */

    /* UIPANEL surface helpers (forward-declared, defined later) */
    uint32_t __thiscall UIPANEL_ClearSurface(void* surface, int width, int height);
    uint32_t __thiscall UIPANEL_ReadPaletteFromBMP(void* surface, void* stream);
    uint32_t __thiscall UIPANEL_InitSurface(void* surface, int width, int height,
                                             int mode, uint32_t palette_param,
                                             uint8_t fill_byte);
    void __thiscall UIPANEL_SetClipRect(void* surface, uint8_t fill_byte,
                                         uint32_t blit_flags);
    uint8_t __thiscall UIPANEL_StretchBlit(void* surface, LPCSTR file_path,
                                            uint32_t param_2, int param_3,
                                            int param_4);
    bool __thiscall UIPANEL_Blit(void* renderer, uint32_t src_x, uint32_t src_y,
                                 int src_w, uint32_t src_h, void* dest_surface,
                                 uint32_t dest_x, uint32_t dest_y, int dest_w,
                                 uint32_t dest_h, uint32_t flags);

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
        if ((palette_param & 0xFFFF) <= (uint32_t)g_shared_palette_refcount) {
            s->palette_ptr = (uint16_t*)g_shared_palette_buffer;
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
        DDRAW_RestoreSurfaces(new_surf, &desc);
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

        asset_data = AssetMgr_LoadFile(g_asset_mgr, rel_path, &asset_size);
        if (asset_data != NULL) {
            /* WNDPROC_StreamFromMemory placement-constructs a WIN32_Stream
             * here (see resources/Win32Stream.cpp); 0x5C was the original
             * x86 sizeof(WIN32_Stream) — use the real host size instead,
             * since StreamObject's pointer fields (rdbuf, tied) widen the
             * class to 0x80 bytes on this 64-bit host. */
            mem_stream = operator_new(WIN32_Stream_Size());
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

        /* Allocate header copy struct for palette + info. 0x428 is a fixed
         * BMP-format buffer (BITMAPINFOHEADER, 0x28 bytes, + a 256-entry
         * 4-byte-per-color palette, 0x400 bytes = 0x428 total) — a raw byte
         * buffer, not a C++ object, so this is safe as-is regardless of
         * host pointer width. */
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
        uint16_t surf_w, surf_h;
        DDRAW_GetSurfaceWidthHeight(new_surf, &surf_h, &surf_w);
        s->width  = (int32_t)surf_w;
        s->height = (int32_t)surf_h;
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

    WIN32_StreamDestroy((void*)(intptr_t)(*(int*)((intptr_t)stream_buf + 8)));

    return result;
}uint32_t __thiscall UIPANEL_ReadPaletteFromBMP(void* surface, void* stream)
{
    /* Grounded: 0x42AF30 — read 256-color palette from BMP stream.
     * Disassembly: accesses +0x10 (has_palette), allocates +0x14 (palette)
     * as uint16_t[256], reads 1024 bytes of RGBQUAD, converts to 16-bit. */
    UIPANEL_Surface* s = (UIPANEL_Surface*)surface;

    uint16_t* palette;
    int has_own_palette = 0;

    if (s->has_palette == 0 || g_shared_palette_refcount != 0) {
        /* Private palette. 0x200 = 256 * sizeof(uint16_t) — a fixed-size
         * raw uint16_t[256] array, not a C++ object, so safe as-is on any
         * host. */
        s->has_palette = 1;
        palette = (uint16_t*)operator_new(0x200);     /* 512 bytes */
        s->palette_ptr = palette;
        if (palette == NULL) {
            GLOBAL_free(g_shared_palette_buffer);
            g_shared_palette_buffer = NULL;
            return 0;
        }
    } else {
        /* Shared global palette — same fixed-size uint16_t[256] array as
         * the private-palette branch above; safe as-is. */
        s->has_palette = 0;
        palette = (uint16_t*)operator_new(0x200);
        g_shared_palette_buffer = palette;
        if (palette == NULL) return 0;
        s->palette_ptr = palette;
        g_shared_palette_refcount++;
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
/* entire rendering subsystem. Ghidra's own decompilation of this      */
/* function (cross-checked instruction-by-instruction) shows it is a   */
/* SINGLE if/else-if/switch dispatch chain per call — exactly one Town */
/* tile-rendering method or DDraw Blt() call ever executes, never two. */
/* mode==1 (hardware DDraw surface) NEVER calls a Town_* tile method —  */
/* it always performs exactly one IDirectDrawSurface4::Blt() call, with */
/* the src/dest rects and DDBLT_KEYSRC flag varying by `flags`. The     */
/* Town_* tile dispatch below only ever runs when mode==0 (software     */
/* pixel buffer).                                                       */
/*                                                                     */
/* Flag dispatch (mode==0 path):                                       */
/*   0x00     = DrawTile (base tile drawing)                           */
/*   0x01/0x03 = InitTileCache (palette-init)                          */
/*   0x02     = DrawTiles16bpp_Strided (standard LTR 16bpp)            */
/*   0x04/0x84 = FlushTileCache (2x2 block expand)                     */
/*   0x05/0x85 = DrawCachedTile (2x2 block from cache)                 */
/*   0x10-0x1F = DrawTileEx (3x2 block expand)                         */
/*   0x20     = BlitTileSurface (right-to-left blit)                   */
/*   0x22     = DrawTiles16bpp_Reversed (H-mirror 16bpp)               */
/*   0x40     = scroll rect adjustment (CalcScrollRect), handled first  */
/*   0x102    = DrawTiles16bpp_Checker (checkerboard 16bpp)            */
/*   0x202    = DrawTiles16bpp_Staggered (staggered 16bpp)             */
/*   0x400/0x402 = DrawTileLine (alpha-blended line)                   */
/*   anything else = DrawTile (fallback)                                */
/*                                                                     */
/* Parameters: this (tile_map/renderer), src_x, src_y, dest_x, dest_y, */
/*   dest_surface, clip_left, clip_top, clip_right, clip_bottom, flags */
/* ================================================================== */
bool __thiscall UIPANEL_Blit(void* renderer,
    uint32_t src_x, uint32_t src_y, int dest_x, uint32_t dest_y,
    void* dest_surface, uint32_t clip_left, uint32_t clip_top,
    int clip_right, uint32_t clip_bottom, uint32_t flags)
{
    UIPANEL_Surface* surf = (UIPANEL_Surface*)renderer;

    /* Auto-detect scroll when source/dest rects differ and flag not set */
    if ((flags & 0xFFFFFFFB) != 0 && (flags & 0xFFFFFFEF) != 0) {
        if ((clip_right - clip_left) != (dest_x - (int)src_x) &&
            (clip_bottom - clip_top) != (dest_y - src_y)) {
            flags |= 0x80;  /* Enable DDraw Blt fallback */
        }
    }

    /* Handle scroll rect calculation */
    if ((flags & 0x40) != 0) {
        RECT rect;
        rect.left   = src_x;
        rect.top    = src_y;
        rect.right  = clip_right;
        rect.bottom = clip_bottom;
        if (surf->mode == 1) {
            surf->CalcScrollRect(&rect, dest_surface);
        } else if (surf->mode == 0) {
            surf->CalcScrollRect_Reversed(&rect, dest_surface);
        }
        src_x       = rect.left;
        src_y       = rect.top;
        clip_right  = rect.right;
        clip_bottom = rect.bottom;
        flags &= 0xFFFFFFBF;
    }

    /* Snapshot g_surface_lost at entry (matches the original's cVar2,
     * captured before either branch below can change it) — needed by the
     * tail unlock logic further down. */
    bool entry_surface_lost = g_surface_lost;

    if (surf->mode != 0) {
        if (surf->mode != 1) {
            return false;
        }
        /* Hardware DDraw path — always exactly one Blt() call. Never
         * dispatches to a Town_* tile method (those only run for
         * mode == 0, below). */
        if (g_surface_lost) {
            if (g_primary_surface != NULL) {
                HRESULT unlock_hr = g_primary_surface->Unlock(NULL);
                if (SUCCEEDED(unlock_hr)) {
                    g_surface_lost = 0;
                }
            }
        }

        /* dest_rect/src_rect roles are constant across every flags value;
         * only the DDBLT_KEYSRC flag bit varies. */
        RECT dest_rect = { (int)src_x, (int)src_y, dest_x, (int)dest_y };
        RECT src_rect  = { (int)clip_left, (int)clip_top, clip_right, (int)clip_bottom };
        IDirectDrawSurface4* dst_ddraw = (IDirectDrawSurface4*)dest_surface;
        IDirectDrawSurface4* src_ddraw = (IDirectDrawSurface4*)surf->ddraw_surf;

        uint32_t blt_flags;
        if (flags == 0) {
            blt_flags = DDBLT_WAIT | DDBLT_KEYSRC;
        } else if (flags == 1 || flags == 0x80) {
            blt_flags = DDBLT_WAIT;
        } else {
            blt_flags = ((flags & 1) == 0) ? (DDBLT_WAIT | DDBLT_KEYSRC) : DDBLT_WAIT;
        }

        HRESULT hr = dst_ddraw->Blt(&dest_rect, src_ddraw, &src_rect, blt_flags, NULL);
        return SUCCEEDED(hr);
    }

    /* Software buffer mode (mode == 0): lock pixels, dispatch to exactly
     * one Town_* tile-rendering method (single if/else chain, matching
     * the original — never more than one method per call). */
    DDSURFACEDESC desc;
    uint8_t* pixels;
    uint32_t pitch;

    if (dest_surface == (void*)g_primary_surface) {
        if (!g_surface_lost) {
            /* Check surface lost */
            memset(&desc, 0, sizeof(desc));
            desc.dwSize = sizeof(desc);
            HRESULT hr = g_primary_surface->Lock(NULL, &desc, DDLOCK_WAIT, NULL);
            if (SUCCEEDED(hr)) {
                g_surface_lost = 1;
            }
        }
        /* Use DDSURFACEDESC from globals for primary surface */
        pitch  = *(uint32_t*)0x4FD1AC;
        pixels = *(uint8_t**)0x4FD1C0;
    } else {
        memset(&desc, 0, sizeof(desc));
        desc.dwSize = sizeof(desc);
        IDirectDrawSurface4* lock_surf = (IDirectDrawSurface4*)dest_surface;
        HRESULT hr = lock_surf->Lock(NULL, &desc, DDLOCK_WAIT, NULL);
        if (FAILED(hr)) {
            return false;
        }
        pixels = (uint8_t*)desc.lpSurface;
        pitch  = desc.lPitch;
    }

    bool result;
    if (flags < 0x12) {
        if (flags > 0x0F) {
            result = surf->DrawTileEx(src_x, src_y, dest_x, dest_y,
                                      pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
        } else {
            switch (flags) {
            case 0:
                result = surf->DrawTile(src_x, src_y, dest_x, dest_y,
                                        pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
                break;
            case 1:
            case 3:
                result = surf->InitTileCache(src_x, src_y, dest_x, dest_y,
                                             pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
                break;
            case 2:
                result = surf->DrawTiles16bpp_Strided(src_x, src_y, dest_x, dest_y,
                                                      pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
                break;
            case 4:
                result = surf->FlushTileCache(src_x, src_y, dest_x, dest_y,
                                              pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
                break;
            case 5:
                result = surf->DrawCachedTile(src_x, src_y, dest_x, dest_y,
                                              pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
                break;
            default:
                result = surf->DrawTile(src_x, src_y, dest_x, dest_y,
                                        pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
                break;
            }
        }
    } else if (flags < 0x85) {
        if (flags == 0x84) {
            result = surf->FlushTileCache(src_x, src_y, dest_x, dest_y,
                                          pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
        } else if (flags == 0x20) {
            result = surf->BlitTileSurface(src_x, src_y, dest_x, dest_y,
                                           pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
        } else if (flags == 0x22) {
            result = surf->DrawTiles16bpp_Reversed(src_x, src_y, dest_x, dest_y,
                                                    pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
        } else {
            result = surf->DrawTile(src_x, src_y, dest_x, dest_y,
                                    pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
        }
    } else if (flags < 0x103) {
        if (flags == 0x102) {
            result = surf->DrawTiles16bpp_Checker(src_x, src_y, dest_x, dest_y,
                                                  pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
        } else if (flags == 0x85) {
            result = surf->DrawCachedTile(src_x, src_y, dest_x, dest_y,
                                          pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
        } else {
            result = surf->DrawTile(src_x, src_y, dest_x, dest_y,
                                    pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
        }
    } else {
        if (flags == 0x202) {
            result = surf->DrawTiles16bpp_Staggered(src_x, src_y, dest_x, dest_y,
                                                    pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
        } else if (flags == 0x400 || flags == 0x402) {
            result = surf->DrawTileLine(src_x, src_y, dest_x, dest_y,
                                        pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
        } else {
            result = surf->DrawTile(src_x, src_y, dest_x, dest_y,
                                    pixels, pitch, clip_left, clip_top, clip_right, clip_bottom);
        }
    }

    /* Unlock/return tail — matches the original's LAB_0042b8d2 block
     * exactly: custom surfaces always unlock and fail the whole call if
     * that unlock fails; the primary surface only unlocks if it *wasn't*
     * already lost at function entry (entry_surface_lost) and currently
     * IS lost, and only clears g_surface_lost if the unlock succeeds. */
    if (dest_surface != (void*)g_primary_surface) {
        /* Original: `if (iVar4==0) return uVar3; return false;` — exact
         * zero check (this vtable slot's own convention), not SUCCEEDED. */
        int unlock_result = ((IDirectDrawSurface4*)dest_surface)->Unlock(NULL);
        if (unlock_result == 0) {
            return result;
        }
        return false;
    }
    if (entry_surface_lost) {
        return result;
    }
    if (!g_surface_lost) {
        return result;
    }
    /* Original: `if (iVar4!=0) return uVar3;` (skip clearing on failure,
     * still return the tile-dispatch result either way) then clear. */
    int unlock_result = g_primary_surface->Unlock(NULL);
    if (unlock_result != 0) {
        return result;
    }
    g_surface_lost = 0;
    return result;
}

#pragma GCC diagnostic pop
