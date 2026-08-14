/**
 * stubs/ddraw.h — Minimal DirectDraw type stubs
 *
 * Provides enough type definitions for the decompiled C++ code
 * to compile. No implementation — linking to ddraw.lib would be
 * needed for a working binary.
 */

#ifndef STUBS_DDRAW_H
#define STUBS_DDRAW_H

/* shared/types.h first: it sets LOCO_TYPES_DEFINED so windows.h's transitive
 * windows_types.h include skips its own (differently-shaped) RECT/DWORD/etc,
 * matching the guard windows_types.h already documents for this exact case. */
#include "../shared/types.h"
#include "windows.h"

/* ================================================================== */
/* IDirectDraw4 / IDirectDrawSurface4 — API-compatible COM interfaces  */
/*                                                                     */
/* loco.exe's PE timestamp (1998-10-06) and its IID_IDirectDraw4 GUID  */
/* (byte-verified in .rdata, absent any IDirectDraw7 GUID) place it in */
/* the DirectX 6.0 SDK window — see NOTE-directx-sdk.md. The real      */
/* interfaces (method names/signatures, not a concrete implementation) */
/* live in platform/ddraw_interfaces.h, shared with the host build's   */
/* SDL3-backed classes (graphics/sdl3_ddraw.h) so there's one          */
/* declaration instead of two independently-drifting copies. Nothing   */
/* in the _WIN32 typecheck build ever instantiates these — they're     */
/* only used as pointer/cast types here — so the pure-virtual           */
/* interface alone is sufficient; no concrete stub subclass is needed. */
/* ================================================================== */
#include "../platform/ddraw_interfaces.h"

#endif /* STUBS_DDRAW_H */
