/**
 * DDRAW_GetSurfaceWidthHeight — Get surface dimensions via GetSurfaceDesc
 * Address: 0x4014E0
 * Size: 82 bytes (0x4014E0 — 0x401532)
 * Calling convention: __cdecl
 *
 * Allocates a DDSURFACEDESC2 (31 dwords, dwSize=0x7C) on the stack,
 * zero-fills it via REP STOSD, then calls IDirectDrawSurface7::GetSurfaceDesc
 * at vtable slot 22 (byte offset 0x58). Extracts dwWidth and dwHeight as
 * 16-bit values and writes them to the caller-provided output pointers.
 *
 * @param surface     IDirectDrawSurface7* (nullable — stores skipped if null)
 * @param out_height  uint16_t* — receives dwHeight from DDSURFACEDESC2
 * @param out_width   uint16_t* — receives dwWidth from DDSURFACEDESC2
 *
 * Called by (5 callers): DDRAW_DimSurfaceRect, UIPANEL surface query paths.
 * All callers push 3 stack arguments.
 *
 * Vtable note: CALL [ECX + 0x58] is byte offset 0x58 = vtable slot 22
 * (22 * 4 = 0x58), NOT C array index 0x58 (which would be slot 88 at
 * byte offset 0x160).
 */
#include "../shared/types.h"

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
    int ddsd_buf[31];        /* DDSURFACEDESC2 (124 bytes = 31 dwords) */
    int i;

    /* Zero-fill via REP STOSD (ECX=0x1F) */
    for (i = 0; i < 31; i++) {
        ddsd_buf[i] = 0;
    }
    ddsd_buf[0] = 0x7C;     /* dwSize = sizeof(DDSURFACEDESC2) */

    if (surface != nullptr) {
        /* Call vtable slot 22 — GetSurfaceDesc (byte offset 0x58) */
        /* Original: CALL [ECX + 0x58] where ECX = *surface (vtable ptr) */
        (reinterpret_cast<int (*)(void*, int*)>((*static_cast<void***>(surface))[22]))(
            surface, ddsd_buf);

        /* dwHeight at DDSURFACEDESC2 offset +0x0C (ddsd_buf[3]) */
        /* Original: MOV CX, [ESP + 0x10]; MOV [EAX], CX */
        *out_height = static_cast<uint16_t>(ddsd_buf[3]);

        /* dwWidth at DDSURFACEDESC2 offset +0x08 (ddsd_buf[2]) */
        /* Original: MOV AX, [ESP + 0x0c]; MOV [EDX], AX */
        *out_width = static_cast<uint16_t>(ddsd_buf[2]);
    }
}
