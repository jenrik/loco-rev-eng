/**
 * ButtonSprite.h — Lightweight UI button sprite wrapper
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * ButtonSprite is a small (0x24 bytes) sprite-object wrapper used throughout
 * the UI system as buttons and image elements. It manages a single resource
 * from the ResourceManager, providing init/destroy/setState lifecycle.
 *
 * It does NOT inherit from any class — it is a standalone leaf class.
 * It is allocated via operator_new(0x24) and initialized with a resource ID.
 * After construction, the caller must call Init() to load the pixel data,
 * and SetState() to render the sprite at a given position.
 *
 * Field layout:
 *   +0x00: vtable pointer (VTBL_BUTTONSPRITE, 0x47851C)
 *   +0x04: x position (int32)
 *   +0x08: y position (int32)
 *   +0x0C: source_x / width (int32) — used in UIPANEL_Blit
 *   +0x10: source_y / height (int32) — used in UIPANEL_Blit
 *   +0x14: pixel_data* — frame data from ResourceManager (vtable-based)
 *   +0x18: surface* — target DirectDraw surface for rendering
 *   +0x1C: resource_id (UINT)
 *   +0x20: field_20 (int32, flags or state, zeroed by ctor)
 *
 * Size: 0x24 bytes
 * Vtable: 0x47851C (VTBL_BUTTONSPRITE)
 *
 * Class hierarchy: (none — standalone class)
 *
 * Vtable layout (0x47851C):
 *   [0] +0x00: scalar deleting destructor (Sprite_ScalarDeletingDtor, 0x454B70)
 */
#pragma once

#include "../shared/types.h"

// Status: TRANSCRIBED
/* vtable addresses in vtable_addrs.h — compiler manages vtables via virtual methods */
class ButtonSprite {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */
/* vtable at +0x00 is compiler-managed */
    int32_t    x;                      // +0x04  X position (set externally after ctor)
    int32_t    y;                      // +0x08  Y position (set externally after ctor)
    int32_t    sourceX;                // +0x0C  Source offset / width (used in blit)
    int32_t    sourceY;                // +0x10  Source offset / height (used in blit)
    void*      pixelData;              // +0x14  Pixel data from ResourceManager (frame data)
    void*      surface;                // +0x18  Target rendering surface
    UINT       resourceId;             // +0x1C  Resource ID for this sprite
    int32_t    field_20;               // +0x20  Unknown flags or state (zeroed by ctor)

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * ButtonSprite constructor.
     * Address: 0x454B50
     *
     * Sets vtable to VTBL_BUTTONSPRITE (0x47851C), zeroes pixelData (+0x14),
     * surface (+0x18), and field_20 (+0x20), and stores the resource ID.
     * Fields +0x04..+0x10 (position/size) are NOT initialized and must be
     * set by the caller after construction.
     *
     * Called by: PanelA_Init @ 0x441025, PanelB_Init @ 0x408BD0,
     *            PostcardAlbum_InitFromResource, Cursor_Init, etc.
     *
     * @param resId  Resource ID to associate with this sprite
     */
    ButtonSprite(UINT resId);

    /**
     * Scalar deleting destructor (vtable[0]).
     * Address: 0x454B70
     *
     * Releases the child pixel data object (if present and refcounted),
     * zeroes pixelData/surface, then optionally frees heap memory.
     *
     * @param flags  Delete flag (bit 0 = free heap allocation)
     */
    virtual ~ButtonSprite();

    /* ================================================================ */
    /* Methods                                                           */
    /* ================================================================ */

    /**
     * Init — Load pixel data from the ResourceManager.
     * Address: 0x454BF0
     *
     * Looks up the resource by resourceId (stored at +0x1C), stores the
     * result in pixelData (+0x14), and queries vtable[1] of the pixel
     * data for the surface pointer stored in surface (+0x18).
     *
     * @return  true if resource was found and surface was obtained
     */
    bool init();

    /**
     * Destroy — Release pixel data without freeing the ButtonSprite.
     * Address: 0x454BC0
     *
     * Calls Sprite_Destroy: if pixelData (+0x14) is non-null and the
     * refcount at pixelData[4] is non-zero, calls pixelData->vtable[2]()
     * to release the sub-resource. Then zeroes pixelData and surface.
     */
    void destroy();

    /**
     * SetState — Render this sprite at the specified state/frame.
     * Address: 0x454C30
     *
     * Blits the sprite image to the target surface. If surface (+0x18) is
     * non-null, reads pixelData (+0x14) for frame dimensions (ushort at
     * pixelData+0x14 = width, pixelData+0x16 = height). If param_1 is
     * non-zero, offsets the source rectangle horizontally by param_1 * width
     * (for multi-frame sprites). Then calls UIPANEL_Blit to draw.
     *
     * @param frameIndex   Frame index for multi-frame sprites (0 = first frame)
     * @param targetSurface  Target DirectDraw surface (NULL = primary surface)
     */
    void setState(int frameIndex, void* targetSurface);
};

/* ================================================================== */
/* Global helper: Sprite_Destroy (C-linkage free function)             */
/* Calls pixelData vtable[2] to release child, zeroes fields.          */
/* Used internally by ButtonSprite::destroy(). Address: 0x454BC0.     */
/* ================================================================== */
extern "C" void __fastcall Sprite_Destroy(void* buttonSprite);
