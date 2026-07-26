/**
 * sdl3_types.h — Shared type definitions for SDL3 DirectX shims
 *
 * This header provides SDL3-forwarding type definitions that mirror
 * the DirectX structs. Include this BEFORE the shim headers.
 *
 * NOT part of the Lego Loco reverse-engineering project.
 * This is a separate portability layer.
 */

#ifndef LOCO_SDL3_TYPES_H
#define LOCO_SDL3_TYPES_H

#include <SDL3/SDL.h>
#include <cstdint>
#include <cstring>

/* Pull in Win32 type definitions from decompiled code */
#include "../decompiled_cpp/shared/types.h"

/* All basic Win32 types (HWND, HDC, DWORD, RECT, POINT, etc.)
 * are provided by shared/types.h, included above. */

/* =========================================================================
 * DirectDraw types
 * ========================================================================= */

/* Surface description — matches DDSURFACEDESC from ddraw.h */
struct DDSURFACEDESC {
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwWidth;
    uint32_t dwHeight;
    int32_t  lPitch;
    uint32_t dwBackBufferCount;
    uint32_t dwMipMapCount;
    uint32_t dwAlphaBitDepth;
    uint32_t dwReserved;
    void*    lpSurface;
    uint32_t dwCKDestBltColorSpaceLowValue;
    uint32_t dwCKDestBltColorSpaceHighValue;
    uint32_t dwCKSrcBltColorSpaceLowValue;
    uint32_t dwCKSrcBltColorSpaceHighValue;

    DDSURFACEDESC() { std::memset(this, 0, sizeof(*this)); dwSize = sizeof(*this); }
};

/* Blit effects — matches DDBLTFX from ddraw.h */
struct DDBLTFX {
    uint32_t dwSize;
    uint32_t dwDDFX;
    uint32_t dwROP;
    uint32_t dwDDROP;
    uint32_t dwRotationAngle;
    uint32_t dwZBufferOpCode;
    uint32_t dwZBufferLow;
    uint32_t dwZBufferHigh;
    uint32_t dwZBufferBaseDest;
    uint32_t dwZDestConstBitDepth;
    uint32_t dwZSrcConstBitDepth;
    uint32_t dwAlphaEdgeBlendBitDepth;
    uint32_t dwAlphaEdgeBlend;
    uint32_t dwReserved;
    uint32_t dwAlphaDestConstBitDepth;
    uint32_t dwAlphaSrcConstBitDepth;
    uint32_t dwFillColor;
    uint32_t dwDDDestColorBitDepth;

    DDBLTFX() { std::memset(this, 0, sizeof(*this)); dwSize = sizeof(*this); }
};

/* Color key — matches DDCOLORKEY from ddraw.h */
struct DDCOLORKEY {
    uint32_t dwColorSpaceLowValue;
    uint32_t dwColorSpaceHighValue;
};

/* DDSURFACEDESC flags */
#define DDSD_WIDTH        0x00000004
#define DDSD_HEIGHT       0x00000002
#define DDSD_PITCH        0x00000008
#define DDSD_LPSURFACE    0x00000800

/* Blt flags */
#define DDBLT_WAIT        0x01000000

/* Color key flags */
#define DDCKEY_SRCBLT     0x00000001

/* =========================================================================
 * DirectSound types
 * ========================================================================= */

struct WAVEFORMATEX {
    uint16_t wFormatTag;
    uint16_t nChannels;
    uint32_t nSamplesPerSec;
    uint32_t nAvgBytesPerSec;
    uint16_t nBlockAlign;
    uint16_t wBitsPerSample;
    uint16_t cbSize;
};

struct DSBUFFERDESC {
    uint32_t      dwSize;
    uint32_t      dwFlags;
    uint32_t      dwBufferBytes;
    uint32_t      dwReserved;
    WAVEFORMATEX* lpwfxFormat;
};

struct DSBCAPS {
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwBufferBytes;
    uint32_t dwUnlockTransferRate;
    uint32_t dwPlayCpuOverhead;
};

/* DSBUFFERDESC flags */
#define DSBPLAY_LOOPING   0x00000001

/* Return codes */
#define DS_OK             0
#define DSERR_INVALIDPARAM 1

/* =========================================================================
 * DirectPlay types (minimal stub definitions)
 * ========================================================================= */

typedef uint32_t DPID;

#define DPID_SERVERPLAYER  1

#endif /* LOCO_SDL3_TYPES_H */
