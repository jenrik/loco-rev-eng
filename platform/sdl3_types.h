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
#include "shared/types.h"

#ifndef _WIN32
/* Host: these structs mirror real ddraw.h/dsound.h/dplay.h types that
 * MinGW provides natively; must stay excluded under _WIN32 to avoid
 * redefinition. */

/* All basic Win32 types (HWND, HDC, DWORD, RECT, POINT, etc.)
 * are provided by shared/types.h, included above. */

/* =========================================================================
 * DirectDraw types
 *
 * Field order/names below match the real DirectX 6.0 SDK layout (see
 * NOTE-directx-sdk.md for how that SDK version was determined and where its
 * headers can be sourced) — consulted for the documented struct shape only,
 * never for ddraw.dll's actual implementation. This project's shim targets
 * API compatibility (correct names, signatures, field order for by-name
 * access), not ABI compatibility: absolute byte offsets and sizeof() are NOT
 * preserved host-side (e.g. lpSurface is a real pointer — 8 bytes on a
 * 64-bit host vs. 4 in the original 32-bit struct — so every later offset
 * legitimately differs from the original binary). Per CLAUDE.md, exact x86
 * layout parity is a non-goal for host-only builds.
 * ========================================================================= */

/* Pixel format description — matches DDPIXELFORMAT from ddraw.h */
struct DDPIXELFORMAT {
    uint32_t dwSize = 0;
    uint32_t dwFlags = 0;
    uint32_t dwFourCC = 0;
    uint32_t dwRGBBitCount = 0;
    uint32_t dwRBitMask = 0;
    uint32_t dwGBitMask = 0;
    uint32_t dwBBitMask = 0;
    uint32_t dwRGBAlphaBitMask = 0;
};

/* Surface capabilities — matches DDSCAPS2 from ddraw.h */
struct DDSCAPS2 {
    uint32_t dwCaps = 0;
    uint32_t dwCaps2 = 0;
    uint32_t dwCaps3 = 0;
    uint32_t dwCaps4 = 0;
};

/* Color key — matches DDCOLORKEY from ddraw.h */
struct DDCOLORKEY {
    uint32_t dwColorSpaceLowValue = 0;
    uint32_t dwColorSpaceHighValue = 0;
};

/* Surface description — matches DDSURFACEDESC2 from ddraw.h (the shape
 * IDirectDraw4/IDirectDrawSurface4 actually use; kept named "DDSURFACEDESC"
 * because every caller in this tree already spells it that way). Height
 * precedes width, matching the real SDK — the opposite order this struct
 * used to have was an in-repo landmine (see NOTE-directx-sdk.md /
 * PROGRESS.md). `dwSize` is intentionally left unpopulated by the default
 * constructor: this shim's GetSurfaceDesc/CreateSurface do not validate it
 * against a fixed byte count (there isn't a single correct one once the
 * struct isn't x86-sized), so callers that set it explicitly (e.g. to the
 * original DDSD_SIZE constant) are not contradicted, and callers that don't
 * set it are not silently miscompared. */
struct DDSURFACEDESC {
    uint32_t      dwSize = 0;
    uint32_t      dwFlags = 0;
    uint32_t      dwHeight = 0;
    uint32_t      dwWidth = 0;
    int32_t       lPitch = 0;
    uint32_t      dwBackBufferCount = 0;
    uint32_t      dwMipMapCount = 0;
    uint32_t      dwAlphaBitDepth = 0;
    uint32_t      dwReserved = 0;
    void*         lpSurface = nullptr;
    DDCOLORKEY    ddckCKDestOverlay;
    DDCOLORKEY    ddckCKDestBlt;
    DDCOLORKEY    ddckCKSrcOverlay;
    DDCOLORKEY    ddckCKSrcBlt;
    DDPIXELFORMAT ddpfPixelFormat;
    DDSCAPS2      ddsCaps;
    uint32_t      dwTextureStage = 0;
};

/* Blit effects — matches DDBLTFX from ddraw.h. The fill-color/pattern/
 * z-buffer/alpha-const slots are real unions in the SDK; this shim only
 * ever needs the DWORD interpretation (dwFillColor etc.), so they're plain
 * fields here rather than reproducing the union — API-compatible by name,
 * not byte-identical. */
struct DDBLTFX {
    uint32_t   dwSize = 0;
    uint32_t   dwDDFX = 0;
    uint32_t   dwROP = 0;
    uint32_t   dwDDROP = 0;
    uint32_t   dwRotationAngle = 0;
    uint32_t   dwZBufferOpCode = 0;
    uint32_t   dwZBufferLow = 0;
    uint32_t   dwZBufferHigh = 0;
    uint32_t   dwZBufferBaseDest = 0;
    uint32_t   dwZDestConstBitDepth = 0;
    uint32_t   dwZDestConst = 0;
    uint32_t   dwZSrcConstBitDepth = 0;
    uint32_t   dwZSrcConst = 0;
    uint32_t   dwAlphaEdgeBlendBitDepth = 0;
    uint32_t   dwAlphaEdgeBlend = 0;
    uint32_t   dwReserved = 0;
    uint32_t   dwAlphaDestConstBitDepth = 0;
    uint32_t   dwAlphaDestConst = 0;
    uint32_t   dwAlphaSrcConstBitDepth = 0;
    uint32_t   dwAlphaSrcConst = 0;
    uint32_t   dwFillColor = 0;
    DDCOLORKEY ddckDestColorkey;
    DDCOLORKEY ddckSrcColorkey;
};

/* DDSURFACEDESC flags */
#define DDSD_CAPS         0x00000001
#define DDSD_HEIGHT       0x00000002
#define DDSD_WIDTH        0x00000004
#define DDSD_PITCH        0x00000008
#define DDSD_LPSURFACE    0x00000800

/* Surface capabilities (DDSCAPS2.dwCaps) */
#define DDSCAPS_OFFSCREENPLAIN 0x00000040
#define DDSCAPS_SYSTEMMEMORY   0x00000800

/* Blt flags */
#define DDBLT_WAIT        0x01000000
#define DDBLT_COLORFILL   0x00000400
#define DDBLT_KEYSRC      0x00008000

/* Lock flags */
#define DDLOCK_WAIT       0x00000001

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

#endif /* _WIN32 */

#endif /* LOCO_SDL3_TYPES_H */
