/**
 * DDRAW_DimSurfaceRect — Dim (halve brightness) a rect on the primary surface
 * Address: 0x401540
 * Size: 213 bytes
 * Calling convention: __cdecl
 *
 * Locks the primary DirectDraw surface and divides every pixel in the
 * specified rect by 2 (right-shift by 1) with bitmasking, producing
 * a dimmed/half-bright region. Used for selection highlight feedback
 * and disabled-area overlays. Always returns 1.
 *
 * Called by: Town_DeselectBuilding, UIPANEL drawing code
 *
 * Lock/Unlock go through the typed IDirectDrawSurface4 interface. Ghidra
 * disassembly of 0x401540 confirms the original calls
 * `CALL dword ptr [ECX+0x64]` (Lock, byte offset 0x64 = COM vtable slot 25)
 * and `CALL dword ptr [ECX+0x80]` (Unlock, byte offset 0x80 = slot 32),
 * pushing the exact same (this, rect=NULL, &desc, flags=0, hEvent=NULL) /
 * (this, rect=NULL) argument shape as town/TownTiles.cpp's
 * Town_CheckOccupiedEx (0x42CA15-ish), which locks/reads this same primary
 * surface. This rewrite mirrors that already-INTEGRATED call shape instead
 * of hand-indexing the vtable.
 *
 * Three address/type bugs, all confirmed via Ghidra xrefs, are fixed here
 * (the raw-vtable-index version below was never correct on host regardless,
 * since IDirectDrawSurface4 there is an ordinary SDL3-backed C++ class —
 * graphics/sdl3_ddraw.h — not the original COM vtable layout):
 *   - g_primary_surface: was declared at 0x4FF0D8 (zero xrefs anywhere in
 *     the binary); the real global, read directly by this function's own
 *     disassembly, is at 0x4FD3C4 (128 xrefs across dozens of functions).
 *   - g_surface_bshift: was declared at 0x48527A (zero xrefs); the real
 *     global — referenced by this function's own AND-mask instruction and
 *     by every Town_DrawTiles16bpp_* variant — is at 0x485280.
 *   - The old `(*(void***)g_primary_surface)[0x64]` indexed a `void**`
 *     array with a literal byte offset (0x64), i.e. byte offset 0x190
 *     (0x64 * sizeof(void*)) — four times past the real Lock slot.
 *
 * @param left    Rect left (in surface pixels)
 * @param top     Rect top
 * @param right   Rect right
 * @param bottom  Rect bottom
 * @return        Always 1
 */
#include "../shared/types.h"
#include "../graphics/sdl3_ddraw.h"   /* typed IDirectDrawSurface4 + DDSURFACEDESC bridge */
#include "../game/Panel.h"            /* canonical DDRAW_DimSurfaceRect prototype */

/* Surface/pixel format globals — canonical addresses/types, matching
 * graphics/DDRAW.h, ui/UIPANEL_Surface.cpp, town/TownTiles.cpp. */
extern void* g_primary_surface;    /* 0x4FD3C4 */
extern char  g_surface_lost;       /* 0x4FD218 */
extern int   g_surface_bshift;     /* 0x485280 — half-bright mask */

namespace {
/* Persistent primary-surface lock state. The original keeps one
 * DDSURFACEDESC at 0x4FD19C; reproduced as file-static storage (own copy
 * per lock site) rather than a single cross-TU global, matching the
 * precedent town/TownTiles.cpp already established for the identical
 * idiom (its own g_primary_surface_desc, "moved here... along with
 * Town_CheckOccupiedEx, its only user") — nothing here depends on
 * observing another function's in-flight lock. */
DDSURFACEDESC g_dim_surface_desc = {};
} // namespace

int DDRAW_DimSurfaceRect(int left, int top, int right, int bottom)
{
    IDirectDrawSurface4* primary = static_cast<IDirectDrawSurface4*>(g_primary_surface);

    /* Lock the primary surface if not already locked. */
    if (g_surface_lost == 0) {
        DDSURFACEDESC& desc = g_dim_surface_desc;
        desc = DDSURFACEDESC();
        desc.dwSize = 0x7C;

        /* Lock(this, NULL, &desc, 0, NULL) — COM slot 25 (byte 0x64). */
        if (primary->Lock(nullptr, &desc, 0, nullptr) == 0) {
            g_surface_lost = 1;
        }
    }

    /* Dim pixels in the specified rect. Preserved bit-for-bit from the
     * original: byte-pitch halved (>>1) to get pixel-pitch for a 16bpp
     * surface, each pixel halved (>>1) and masked against the half-bright
     * mask. */
    uint32_t pitch_half = (static_cast<uint32_t>(g_dim_surface_desc.lPitch) >> 1) & 0xFFFF;
    uint16_t* pixels = reinterpret_cast<uint16_t*>(
        static_cast<uint8_t*>(g_dim_surface_desc.lpSurface) +
        (static_cast<uint32_t>(top) * pitch_half + static_cast<uint32_t>(left)) * 2);

    uint32_t width  = static_cast<uint32_t>(right - left) & 0xFFFF;
    uint32_t height = static_cast<uint32_t>(bottom - top) & 0xFFFF;

    if (height != 0) {
        do {
            uint32_t w = width;
            for (; w != 0; w--) {
                *pixels = static_cast<uint16_t>((*pixels >> 1) & g_surface_bshift);
                pixels++;
            }
            height--;
            pixels += pitch_half - width;
        } while (height != 0);
    }

    /* Unlock the surface if it was locked. */
    if (g_surface_lost != 0) {
        /* Unlock(NULL) — COM slot 32 (byte 0x80). */
        if (primary->Unlock(nullptr) == 0) {
            g_surface_lost = 0;
        }
    }

    return 1;
}
