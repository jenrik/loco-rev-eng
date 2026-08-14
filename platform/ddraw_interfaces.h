/**
 * platform/ddraw_interfaces.h — API-compatible DirectDraw 4 interfaces
 *
 * Declares IDirectDraw4, IDirectDrawSurface4, IDirectDrawPalette, and
 * IDirectDrawClipper as pure-virtual C++ classes, using loco.exe's real
 * DirectDraw method names and signatures. This is the same pattern already
 * proven for IDirectPlay4A (stubs/dplay.h) and AudioDirectSoundBuffer/
 * AudioDirectSoundDevice (audio/AudioChannel.h): one interface declared
 * once, so every consumer (real host implementation, MinGW typecheck stub,
 * decompiled call sites) sees the same method set.
 *
 * loco.exe's PE timestamp (1998-10-06) and its byte-verified IID_IDirectDraw4
 * GUID place it in the DirectX 6.0 SDK window, with no IDirectDraw7 GUID
 * present — see NOTE-directx-sdk.md for how that was determined and where
 * the real SDK headers can be sourced. The method names/signatures below
 * were written fresh against that SDK's documented API surface (consulted
 * for interface shape only); no original DirectDraw *implementation* code
 * (ddraw.dll behavior/disassembly, SDK sample source) was referenced or
 * ported — see NOTE-directx-sdk.md's sourcing guardrail.
 *
 * SCOPE: this shim targets API compatibility, not ABI compatibility. Method
 * order below is for readability (IUnknown first, then grouped by purpose),
 * not a slot-accurate replica of the real COM vtable — nothing in this tree
 * may reach a method through raw pointer-offset vtable indexing once it has
 * been migrated to call through these interfaces by name. Only the methods
 * some real caller in this tree needs (by name today, or identified by RE
 * evidence during the raw-vtbl-dispatch migration) are declared — there is
 * no requirement to pre-declare the real SDK's full, larger method list.
 */

#ifndef LOCO_DDRAW_INTERFACES_H
#define LOCO_DDRAW_INTERFACES_H

#include <cstdint>

/* RECT/DWORD/HRESULT — platform-neutral, used unconditionally by both the
 * host build and the _WIN32 typecheck build (shared/types.h itself has no
 * _WIN32 split; see stubs/windows_types.h's LOCO_TYPES_DEFINED guard for how
 * the two coexist when a TU also pulls in stubs/windows.h). */
#include "../shared/types.h"

/* =========================================================================
 * DirectDraw structs
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
 * layout parity is a non-goal for host-only builds. These structs have no
 * SDL dependency, so — unlike the DirectSound/DirectPlay types still split
 * across platform/sdl3_types.h's #ifndef _WIN32 section — they're declared
 * unconditionally here and shared by both the host build and the _WIN32
 * typecheck build (stubs/ddraw.h) instead of each declaring its own copy.
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

struct IDirectDrawSurface4;
struct IDirectDrawPalette;
struct IDirectDrawClipper;

/* =========================================================================
 * IDirectDrawSurface4
 * ========================================================================= */
struct IDirectDrawSurface4 {
    virtual ~IDirectDrawSurface4() = default;

    /* IUnknown */
    virtual int32_t QueryInterface(void* iid, void** object) = 0;
    virtual uint32_t AddRef() = 0;
    virtual uint32_t Release() = 0;

    /* Blit */
    virtual HRESULT Blt(RECT* dest_rect, IDirectDrawSurface4* src_surface,
                         RECT* src_rect, DWORD flags, DDBLTFX* fx) = 0;
    virtual HRESULT BltFast(DWORD dx, DWORD dy, IDirectDrawSurface4* src_surface,
                             RECT* src_rect, DWORD flags) = 0;

    /* CPU pixel access */
    virtual HRESULT Lock(RECT* rect, DDSURFACEDESC* desc, DWORD flags,
                          void* event_handle) = 0;
    virtual HRESULT Unlock(RECT* rect) = 0;

    /* Description / format / color key */
    virtual HRESULT GetSurfaceDesc(DDSURFACEDESC* desc) = 0;
    virtual HRESULT GetPixelFormat(DDPIXELFORMAT* fmt) = 0;
    virtual HRESULT SetColorKey(DWORD flags, const DDCOLORKEY* key) = 0;

    /* Lifecycle */
    virtual HRESULT Restore() = 0;
    virtual HRESULT IsLost() = 0;

    /* GDI interop */
    virtual HRESULT GetDC(void** hdc) = 0;
    virtual HRESULT ReleaseDC(void* hdc) = 0;

    /* Palette. (SetClipper has zero real callers in this tree today —
     * deferred until there's a real caller to shape it against.) */
    virtual HRESULT SetPalette(IDirectDrawPalette* palette) = 0;

    /* Multi-surface (backbuffer/z-buffer chains, overlays) */
    virtual HRESULT GetAttachedSurface(void* caps, IDirectDrawSurface4** out) = 0;
    virtual HRESULT EnumSurfaces(void* callback, void* context) = 0;

    /* Misc */
    virtual HRESULT GetCaps(void* caps) = 0;
    virtual HRESULT WaitForVerticalBlank(DWORD flags, void* event_handle) = 0;
};

/* =========================================================================
 * IDirectDraw4
 * ========================================================================= */
struct IDirectDraw4 {
    virtual ~IDirectDraw4() = default;

    /* IUnknown */
    virtual int32_t QueryInterface(void* iid, void** object) = 0;
    virtual uint32_t AddRef() = 0;
    virtual uint32_t Release() = 0;

    /* Surface/palette/clipper factories. CreatePalette/CreateClipper have
     * zero real callers in this tree today (this project bakes palettes
     * into RGBA at BMP-load time rather than swapping IDirectDrawPalette
     * objects at runtime, and clipper creation isn't reached from any live
     * path), but are declared for interface completeness now that minimal
     * concrete IDirectDrawPalette/Clipper implementations exist. */
    virtual HRESULT CreateSurface(DDSURFACEDESC* desc, IDirectDrawSurface4** out,
                                   void* unused) = 0;
    virtual HRESULT CreatePalette(DWORD flags, void* color_array,
                                   IDirectDrawPalette** out, void* unused) = 0;
    virtual HRESULT CreateClipper(DWORD flags, IDirectDrawClipper** out,
                                   void* unused) = 0;

    /* Device setup */
    virtual HRESULT SetCooperativeLevel(void* hwnd, DWORD flags) = 0;
    virtual HRESULT SetDisplayMode(DWORD width, DWORD height, DWORD bpp,
                                    DWORD refresh_rate, DWORD flags) = 0;
    virtual HRESULT GetDeviceIdentifier(void* identifier, DWORD flags) = 0;
};

/* =========================================================================
 * IDirectDrawPalette — minimal: no real caller passes actual color entries
 * today (this project bakes palettes into RGBA at BMP-load time, see
 * CLAUDE.md's "Palette handling" note), but the interface must exist for
 * CreatePalette/SetPalette call sites to type-check and behave safely.
 * ========================================================================= */
struct IDirectDrawPalette {
    virtual ~IDirectDrawPalette() = default;

    virtual int32_t QueryInterface(void* iid, void** object) = 0;
    virtual uint32_t AddRef() = 0;
    virtual uint32_t Release() = 0;

    virtual HRESULT SetEntries(DWORD flags, DWORD start, DWORD count,
                                void* entries) = 0;
    virtual HRESULT GetEntries(DWORD flags, DWORD start, DWORD count,
                                void* entries) = 0;
};

/* =========================================================================
 * IDirectDrawClipper — minimal: this tree only ever releases opaque
 * clipper objects today (native/ddraw_clippers.c), it never sets/queries a
 * real clip list.
 * ========================================================================= */
struct IDirectDrawClipper {
    virtual ~IDirectDrawClipper() = default;

    virtual int32_t QueryInterface(void* iid, void** object) = 0;
    virtual uint32_t AddRef() = 0;
    virtual uint32_t Release() = 0;
};

#endif /* LOCO_DDRAW_INTERFACES_H */
