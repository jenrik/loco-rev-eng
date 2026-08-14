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

/* DirectDraw structs (DDSURFACEDESC, DDBLTFX, DDPIXELFORMAT, DDCOLORKEY,
 * DDSCAPS2) and the real IDirectDraw4/IDirectDrawSurface4/
 * IDirectDrawPalette/IDirectDrawClipper interfaces now live in
 * ddraw_interfaces.h — platform-neutral (no SDL dependency), shared by both
 * this host header and stubs/ddraw.h's _WIN32 typecheck path, so both sides
 * see the exact same declarations instead of three independent copies. */
#include "ddraw_interfaces.h"

#ifndef _WIN32
/* Host: these structs mirror real dsound.h/dplay.h types that MinGW
 * provides natively; must stay excluded under _WIN32 to avoid redefinition. */

/* All basic Win32 types (HWND, HDC, DWORD, RECT, POINT, etc.)
 * are provided by shared/types.h, included above. */

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
