/**
 * DDRAW_GetSurfaceWidthHeight — Get surface dimensions via GetSurfaceDesc
 * Address: 0x4014E0
 * Size: 82 bytes (0x4014E0 — 0x401532)
 * Calling convention: __cdecl
 *
 * Calls IDirectDrawSurface4::GetSurfaceDesc (real ABI vtable slot 22,
 * byte offset 0x58 — dispatched by name through platform/ddraw_interfaces.h
 * here, not by raw slot, since this shim is API- not ABI-compatible, see
 * CLAUDE.md) and extracts dwWidth/dwHeight as 16-bit values into the
 * caller-provided output pointers.
 *
 * @param surface     IDirectDrawSurface4* (nullable — stores skipped if null)
 * @param out_height  uint16_t* — receives dwHeight from DDSURFACEDESC
 * @param out_width   uint16_t* — receives dwWidth from DDSURFACEDESC
 *
 * Called by (5 callers): DDRAW_DimSurfaceRect, UIPANEL surface query paths.
 * All callers push 3 stack arguments.
 */
#include "../platform/ddraw_interfaces.h"

/* Forward declaration (STRICT=2 -Wmissing-declarations). Self-contained
 * rather than pulled from a header: graphics/sdl3_ddraw.h declares a
 * *different*, 2-argument DDRAW_GetSurfaceWidthHeight (the real host
 * SDL3-backed overload, operating on the primary surface singleton
 * directly) — this file's 3-argument free function is the original
 * x86-COM-vtable-shaped overload, a distinct symbol (see commit
 * message; landmine doc already tracks resolving each call site to the
 * right overload as open work, out of scope here). */
void __cdecl DDRAW_GetSurfaceWidthHeight(void* surface, uint16_t* out_height, uint16_t* out_width);

void __cdecl DDRAW_GetSurfaceWidthHeight(void* surface, uint16_t* out_height, uint16_t* out_width)
{
    if (surface != nullptr) {
        DDSURFACEDESC desc;
        static_cast<IDirectDrawSurface4*>(surface)->GetSurfaceDesc(&desc);
        *out_height = static_cast<uint16_t>(desc.dwHeight);
        *out_width = static_cast<uint16_t>(desc.dwWidth);
    }
}
