/**
 * UIPANEL_Surface_lifecycle.cpp — UIPANEL_Surface construction/destruction/copy
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Deliberately split out of graphics/LOCOBITMAP.cpp (which also implements
 * the large, unrelated PostcardAlbum class) into its own minimal-dependency
 * translation unit: several narrow test executables and
 * resources/sprite_uipanel_adapter.cpp need UIPANEL_Surface's constructor
 * without pulling in PostcardAlbum's much larger Win32/sprite/pixel-cache
 * dependency graph.
 */

#include "LOCOBITMAP.h"
#include "../platform/ddraw_interfaces.h"
#include <cstring>

/* C++ allocation helpers */
void* operator_new(size_t size);                               /* @0x465CE0 */
void  GLOBAL_free(void* ptr);                                  /* @0x465CD0 */

extern "C" {
    void OutputDebugStringA(const char* msg);                  /* @0x477090 - indirect */
}

extern int32_t g_ref_count;                 /* 0x00485254 -- global UIPANEL_Surface count */
extern void*   g_ddraw;                     /* 0x485440 -- IDirectDraw4* */

/* ────────────────────────────────────────────────────────────────── */
/* UIPANEL_Surface::UIPANEL_Surface — default ctor                    */
/* Address: 0x42A110 — __fastcall (ECX=this)                          */
/* ────────────────────────────────────────────────────────────────── */
UIPANEL_Surface::UIPANEL_Surface()
    : mode(0), width(0), height(0), has_palette(0), flags(0),
      palette_ptr(nullptr), pixels(nullptr), ddraw_surf(nullptr)
{
/* In the binary: this->vtable = VTBL_*. Compiler-managed in natural C++. */
    g_ref_count++;  /* 0x00485254 */
}

/* ────────────────────────────────────────────────────────────────── */
/* UIPANEL_Surface::UIPANEL_Surface(const&) — deep-copy ctor, a.k.a.   */
/* UIPANEL_CopySurface. Disassembly-verified control flow (the naive   */
/* decompilation mis-showed the HRESULT==0/success branch as a failure */
/* branch and lost the retry's argument count):                       */
/*   1. Zero-init + increment g_ref_count, same as the default ctor.  */
/*   2. Copy mode/width/height/has_palette/flags verbatim.            */
/*   3. Palette: new 0x200-byte allocation + memcpy when owned        */
/*      (has_palette==1), otherwise share the source's pointer.       */
/*   4. Pixels: new width*height allocation + memcpy when present.    */
/*   5. ddraw_surf: when the source has one, call                     */
/*      IDirectDraw4::CreateSurface (retrying once on failure --      */
/*      dwCaps gets re-set because the failed attempt clobbers it);   */
/*      on double failure, log exactly the original's                 */
/*      OutputDebugStringA string and leave ddraw_surf null; on       */
/*      success, Blt the full source surface into the new one.        */
/* Address: 0x42A1C0 — __thiscall (ECX=this, stack arg=&other)        */
/* ────────────────────────────────────────────────────────────────── */
UIPANEL_Surface::UIPANEL_Surface(const UIPANEL_Surface& other)
    : mode(other.mode), width(other.width), height(other.height),
      has_palette(other.has_palette), flags(other.flags),
      palette_ptr(nullptr), pixels(nullptr), ddraw_surf(nullptr)
{
    g_ref_count++;  /* 0x00485254 */

    if (has_palette == 1 && other.palette_ptr != nullptr) {
        palette_ptr = static_cast<uint16_t*>(operator_new(0x200));
        std::memcpy(palette_ptr, other.palette_ptr, 0x200);
    } else {
        palette_ptr = other.palette_ptr;   /* shared, unowned reference */
    }

    if (other.pixels != nullptr) {
        const size_t pixel_bytes =
            static_cast<size_t>(width) * static_cast<size_t>(height);
        pixels = static_cast<uint8_t*>(operator_new(pixel_bytes));
        std::memcpy(pixels, other.pixels, pixel_bytes);
    }

    if (other.ddraw_surf != nullptr) {
        DDSURFACEDESC desc;
        desc.dwSize    = 0x7c;
        desc.dwFlags   = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
        desc.dwHeight  = static_cast<uint32_t>(height);
        desc.dwWidth   = static_cast<uint32_t>(width);
        desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;

        IDirectDrawSurface4* new_surf = nullptr;
        HRESULT hr = static_cast<IDirectDraw4*>(g_ddraw)->CreateSurface(
            &desc, &new_surf, nullptr);
        if (hr != 0) {
            desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
            hr = static_cast<IDirectDraw4*>(g_ddraw)->CreateSurface(
                &desc, &new_surf, nullptr);
        }
        if (hr != 0) {
            OutputDebugStringA(
                "LOCOBITMAP COPY CONSTRUCTOR - failed to create surface");
        } else {
            ddraw_surf = new_surf;
            RECT dest_rect{0, 0, width, height};
            RECT src_rect{0, 0, other.width, other.height};
            new_surf->Blt(&dest_rect,
                          static_cast<IDirectDrawSurface4*>(other.ddraw_surf),
                          &src_rect, DDBLT_WAIT, nullptr);
        }
    }
}

/* ────────────────────────────────────────────────────────────────── */
/* UIPANEL_Surface::~UIPANEL_Surface — scalar dtor, user cleanup only  */
/* (vtable[0] at 0x477D28). The scalar-deleting-destructor flag and    */
/* its conditional operator-delete call are compiler-generated, not    */
/* reproduced here.                                                    */
/* Address: 0x42A140 — __thiscall (ECX=this)                          */
/* ────────────────────────────────────────────────────────────────── */
UIPANEL_Surface::~UIPANEL_Surface()
{
    /* Free palette allocation (0x200 bytes) when owned */
    if (has_palette == 1 && palette_ptr != nullptr) {
        GLOBAL_free(palette_ptr);           /* @0x465CD0 */
        palette_ptr = nullptr;
        has_palette = 0;
    }

    /* Free pixel buffer */
    if (pixels != nullptr) {
        GLOBAL_free(pixels);
        pixels = nullptr;
    }

    /* Release DDraw surface via IDirectDrawSurface4::Release */
    if (ddraw_surf != nullptr) {
        static_cast<IDirectDrawSurface4*>(ddraw_surf)->Release();
        ddraw_surf = nullptr;
    }

    g_ref_count--;  /* 0x00485254 */
}

UIPANEL_Surface* UIPANEL_Surface_New()
{
    return new UIPANEL_Surface();
}
