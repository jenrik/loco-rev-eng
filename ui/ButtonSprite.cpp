/**
 * ButtonSprite.cpp — ButtonSprite implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

// Status: TRANSCRIBED

#include "ButtonSprite.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

extern void* __cdecl operator_new(size_t size);     /* 0x465CE0 — operator new */
    extern void  __cdecl GLOBAL_free(void* ptr);         /* 0x465CD0 */

    /* ResourceManager singleton */
    extern void* g_resmgr;                               /* 0x4855E8 */

    /* Resource lookup / release */
    extern void* __thiscall ResourceManager_GetById(void* resmgr, UINT id); /* 0x4472B0 */
    extern void  __fastcall RESMGR_ReleaseSoundResource(void* res);         /* 0x448EE0 */

    /* UIPANEL_Blit — blits a sprite frame to a surface (0x42B050).
     * Real def: ui/UIPANEL_Surface.cpp, bool(void*,uint32_t,uint32_t,
     * int32_t,uint32_t,void*,uint32_t,uint32_t,int32_t,uint32_t,uint32_t).
     * Was declared with a uniform `int`/`byte` shape that doesn't match the
     * real mixed uint32_t/int32_t parameter types — call-0 landmine. */
    extern bool __cdecl UIPANEL_Blit(void* srcSurface, uint32_t destX, uint32_t destY,
                                     int32_t destW, uint32_t destH, void* targetSurface,
                                     uint32_t srcX, uint32_t srcY, int32_t srcW, uint32_t srcH,
                                     uint32_t flags);

    /* Win32 GDI — OffsetRect (via IAT at 0x477374) */
extern "C" {
    extern void __stdcall OffsetRect(void* lprc, int dx, int dy);
}

/* Global primary surface reference */
extern void* _g_primary_surface;  /* 0x4FD3C4 */

namespace {
using PixelReleaseFunction = void (__fastcall *)(void*);
using PixelSurfaceFunction = void* (__fastcall *)(void*, int, int);

void release_pixel_data(void* pixel_data)
{
    const auto* pixel_header = reinterpret_cast<const uint32_t*>(pixel_data);
    if (pixel_header[4] != 0) {
        void** vtable = *reinterpret_cast<void***>(pixel_data);
        auto release = reinterpret_cast<PixelReleaseFunction>(vtable[2]);
        release(pixel_data);
    }
}
}

/* ================================================================== */
/* ButtonSprite Constructor                                            */
/* Address: 0x454B50                                                   */
/*                                                                     */
/* Lightweight constructor: sets vtable, zeroes pixelData/surface/     */
/* field_20, stores resource ID. Fields +0x04..+0x10 (position/size)  */
/* are left uninitialized — they must be set by caller after ctor.     */
/*                                                                     */
/* Called by: PanelA_Init, PanelB_Init, PostcardAlbum_InitFromResource, */
/*            Cursor_Init, HelpWnd_Init, Town_BaseCtor,                 */
/*            Town_DrawPostcardPreview                                  */
/* ================================================================== */
ButtonSprite::ButtonSprite(UINT resId)
{
/* In the binary: sets vtable here. Compiler-managed in natural C++. */
    this->pixelData  = NULL;                       /* +0x14 */
    this->surface    = NULL;                       /* +0x18 */
    this->resourceId = resId;                      /* +0x1C */
    this->field_20   = 0;                          /* +0x20 */
    /* NOTE: +0x04 (x), +0x08 (y), +0x0C (sourceX), +0x10 (sourceY)
       are NOT initialized — caller must set them after construction. */
}

/* ================================================================== */
/* ButtonSprite::scalar deleting destructor (vtable[0])                */
/* Address: 0x454B70                                                   */
/*                                                                     */
/* Releases child pixel data if refcounted, zeroes pixelData/surface,  */
/* and optionally frees the heap allocation when flags & 1.            */
/* ================================================================== */
ButtonSprite::~ButtonSprite()
{
    /* Reset vtable for correct dispatch during destruction */
/* In the binary: sets vtable here. Compiler-managed in natural C++. */

    /* Release child pixel data sub-object if refcounted */
    if (this->pixelData != NULL) {
        release_pixel_data(this->pixelData);
    }

    this->pixelData = NULL;  /* +0x14 */
    this->surface   = NULL;  /* +0x18 */
}

/* ================================================================== */
/* ButtonSprite::init                                                  */
/* Address: 0x454BF0                                                   */
/*                                                                     */
/* Loads pixel data from ResourceManager by resource ID. Stores the    */
/* pixel data pointer and queries vtable[1] to get the surface.        */
/*                                                                     */
/* @return  true if resource found and surface obtained                */
/* ================================================================== */
bool ButtonSprite::init()
{
    void* data = ResourceManager_GetById(&g_resmgr, this->resourceId);  /* +0x1C */
    this->pixelData = data;                                              /* +0x14 */

    if (data == NULL) {
        return false;
    }

    /* Query vtable[1] of pixel data to get surface pointer */
    void** vtable = *reinterpret_cast<void***>(data);
    auto get_surface = reinterpret_cast<PixelSurfaceFunction>(vtable[1]);
    void* surf = get_surface(data, 0, 0);
    this->surface = surf;  /* +0x18 */

    return (surf != NULL);
}

/* ================================================================== */
/* ButtonSprite::destroy                                               */
/* Address: 0x454BC0                                                   */
/*                                                                     */
/* Releases the pixel data sub-object if refcounted, zeroes fields.    */
/* Does NOT free the ButtonSprite itself (unlike scalar dtor).         */
/* ================================================================== */
void ButtonSprite::destroy()
{
    if (this->pixelData != NULL) {
        release_pixel_data(this->pixelData);
    }

    this->pixelData = NULL;  /* +0x14 */
    this->surface   = NULL;  /* +0x18 */
}

/* ================================================================== */
/* ButtonSprite::setState                                              */
/* Address: 0x454C30                                                   */
/*                                                                     */
/* Renders the sprite at the given frame index to the target surface.  */
/* Reads pixel dimension from the pixel data header. Supports multi-   */
/* frame sprites by offsetting the source rectangle horizontally.      */
/*                                                                     */
/* @param frameIndex      Frame index (0 = first frame)                */
/* @param targetSurface   Target surface (NULL = primary screen)       */
/* ================================================================== */
void ButtonSprite::setState(int frameIndex, void* targetSurface)
{
    if (this->surface == NULL) {  /* +0x18 — no surface loaded */
        return;
    }

    if (targetSurface == NULL) {
        targetSurface = _g_primary_surface;
    }

    /* Read pixel dimensions from the pixel data header */
    const auto* pixelHdr = reinterpret_cast<const uint8_t*>(this->pixelData); /* +0x14 */
    uint16_t frameWidth  = *reinterpret_cast<const uint16_t*>(pixelHdr + 0x14);  /* width  at pixelData+0x14 */
    uint16_t frameHeight = *reinterpret_cast<const uint16_t*>(pixelHdr + 0x16);  /* height at pixelData+0x16 */

    /* Build source rectangle */
    RECT srcRect;
    srcRect.left   = 0;
    srcRect.top    = 0;
    srcRect.right  = frameWidth;
    srcRect.bottom = frameHeight;

    /* For multi-frame sprites, offset source X by frameIndex * width */
    if (frameIndex != 0) {
        OffsetRect(&srcRect, frameIndex * frameWidth, 0);
    }

    /* Blit to target surface at stored position/dimensions */
    UIPANEL_Blit(
        this->surface,           /* +0x18: source surface (pixel data) */
        this->x,                 /* +0x04: dest X position */
        this->y,                 /* +0x08: dest Y position */
        this->sourceX,           /* +0x0C: dest width */
        this->sourceY,           /* +0x10: dest height */
        targetSurface,           /* target DirectDraw surface */
        srcRect.left,            /* source X */
        srcRect.top,             /* source Y */
        srcRect.right,           /* source right */
        srcRect.bottom,          /* source bottom */
        0                        /* blit flags */
    );
}
