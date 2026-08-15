/**
 * TrackPiece.cpp — TrackPiece implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

// Status: TRANSCRIBED

#include "TrackPiece.h"
#include "../resources/ResourceObject.h"
/* vtable_addrs.h removed — compiler manages vtables via virtual methods */

namespace {

struct TrackResourceGeometry {
    uint8_t prefix_00_27[0x28];
    uint16_t frame_width;
    uint16_t frame_height;
    uint8_t prefix_2c[2];
    int16_t world_x;
    int16_t world_y;
};

struct TownTrackFields {
    void* viewport;
    uint8_t prefix_04_07[4];
    int32_t scroll_x;
    int32_t scroll_y;
    uint8_t prefix_10_9b[0x8C];
    void* zoom_sub_object;
    uint8_t prefix_a0_a3[4];
    int32_t camera_x;
    int32_t camera_limit;
};

struct TrackViewportFields {
    uint8_t prefix_00_2f[0x30];
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
    void* surface_object;
};

struct SurfaceFields {
    uint8_t prefix_00_0f[0x10];
    void* surface;
};

#if UINTPTR_MAX == 0xffffffffu
static_assert(offsetof(TrackResourceGeometry, frame_width) == 0x28);
static_assert(offsetof(TrackResourceGeometry, world_x) == 0x2E);
static_assert(offsetof(TownTrackFields, zoom_sub_object) == 0x9C);
static_assert(offsetof(TownTrackFields, camera_x) == 0xA4);
static_assert(offsetof(TrackViewportFields, surface_object) == 0x40);
#endif

} // namespace
/* ================================================================== */
/* External references                                                 */
/* ================================================================== */

void   GLOBAL_free(void* ptr);                                      /* 0x465CD0 */

extern "C" {
    void   SetRect(void* rect, int left, int top, int right, int bottom); /* USER32 */
    void   OffsetRect(void* rect, int dx, int dy);                       /* USER32 */
}


/* TileMap */
struct TileMap { int dummy; };
extern TileMap* g_tilemap;
extern void   TileMap_InvalidateRect(TileMap* tm, int left, int top, int right, int bottom);

/* Town rendering */
extern void   Town_BlitElement(void* panel, int dst_l, int dst_t, int dst_r, int dst_b,
                                void* surface, int src_l, int src_t, int src_r, int src_b,
                                uint32_t flags);

/* ================================================================== */
/* TrackPiece constructor                                              */
/* Address: 0x40CFA0                                                   */
/*                                                                     */
/* Called by:                                                          */
/*   RESDATA_CreateChildSprite (0x45476C, 0x4547DE)                    */
/*   RESMGR_SoundObject_Ctor (0x448F5E)                                */
/*                                                                     */
/* Stack layout (thiscall):                                            */
/*   [ESP+0x18] = param1: town  (void*)                                */
/*   [ESP+0x1C] = param2: res   (RESDATA*)                             */
/*   [ESP+0x20] = param3: flags (uint16_t)                             */
/*                                                                     */
/* 1. Calls GameObject_Ctor() to init base fields                      */
/* 2. Sets vtable to 0x477568 (VTBL_TRACK_PIECE)                      */
/* 3. Zeros TrackPiece-specific fields:                                */
/*    - sub_resource (+0x28) = 0                                       */
/*    - flags (+0x2C) = 0 (word)                                       */
/*    - current_frame (+0x4C) = 0                                       */
/*    - anim_tick (+0x50) = 0                                           */
/*    - field_30 (+0x30) = 0                                            */
/* 4. Sets type = 7 (+0x04)                                            */
/* 5. Sets prev_frame = 0xFFFF (+0x54)                                 */
/* 6. Sets render_enabled = 1 (+0x56)                                   */
/* 7. Calls Init(this, town, res, flags) directly                       */
/*                                                                     */
/* NOTE: resource (+0x44) is NOT zeroed here — it's set by Init().     */
/* ================================================================== */
TrackPiece::TrackPiece(void* town, RESDATA* res, uint16_t flags)
{
    /* Base GameObject constructor called automatically by C++ */

    /* Vtable managed by compiler */

    /* Zero all TrackPiece-specific fields (exact order as assembly) */
    this->sub_resource = 0;            /* +0x28 */
    this->flags = 0;                   /* +0x2C (word zero, matches MOV word ptr) */
    this->current_frame = 0;           /* +0x4C */
    this->anim_tick = 0;               /* +0x50 */
    this->field_30 = 0;                /* +0x30 */

    /* Set type marker to 7 (TrackPiece-specific type) */
    this->type = 7;                    /* +0x04 */

    /* Set default values */
    this->prev_frame = 0xFFFF;         /* +0x54 — -1 (uninitialized) */
    this->render_enabled = 1;          /* +0x56 — render by default */

    /* Initialize geometry from params (direct call, not vtable dispatch) */
    this->Init(town, res, flags);
}


/* ================================================================== */
/* TrackPiece destructor (body) — 0x40D040                              */
/*                                                                     */
/* Uses SEH prologue/epilogue (exception handler at 0x4750C8).         */
/*                                                                     */
/* 1. Marks object dead via GameObject_MarkDead (0x436A00)              */
/* 2. If sub_resource (+0x28) is non-null:                             */
/*    - Calls its scalar deleting destructor (vtable[0]) with flags=1   */
/*      to free the sub-resource                                       */
/*    - Sets sub_resource to NULL                                       */
/* 3. Re-marks dead (SEH cleanup path)                                  */
/*                                                                     */
/* The scalar deleting destructor at vtable[0] wraps this body and     */
/* conditionally calls operator delete (compiler-generated).           */
/* ================================================================== */
TrackPiece::~TrackPiece()
{
    /* SEH prologue: push -1, push 0x4750C8, push fs:[0], mov fs:[0],esp */

    /* Mark object dead in manager */
    this->MarkDead();

    /* Release sub-resource at +0x28 if non-null. Real ResourceObject
     * virtual destructor (resources/ResourceObject.h) -- `delete`
     * reproduces the original's scalar-deleting-destructor-with-flag-1
     * exactly (see CLAUDE.md's "Scalar/vector deleting-destructor
     * flags... -> remove; keep only user cleanup"). */
    if (this->sub_resource != 0) {
        // ABI_BOUNDARY: sub_resource is a 32-bit handle (the original x86
        // pointer field's width, preserved so this class's own layout stays
        // offset-compatible on a 64-bit host -- same pointer_from_handle-
        // style convention resources/ResourceManager.cpp uses), not a
        // reconstructed-class round-trip.
        delete reinterpret_cast<ResourceObject*>(
            static_cast<uintptr_t>(static_cast<uint32_t>(this->sub_resource)));
        this->sub_resource = 0;
    }

    /* Re-mark dead (try level = -1, cleanup in case of SEH unwind) */
    this->MarkDead();

    /* SEH epilogue */
}


/* ================================================================== */
/* TrackPiece::Init — Initialize track piece geometry (NON-VIRTUAL)    */
/* Address: 0x40D0B0                                                   */
/*                                                                     */
/* Called from constructor only (direct call, not vtable dispatch).    */
/*                                                                     */
/* Stack layout (thiscall):                                            */
/*   [ESP+0x04] = town    (void*) — before PUSH ESI                    */
/*   [ESP+0x08] = res     (RESDATA*) — before PUSH ESI                 */
/*   [ESP+0x0C] = flags   (uint16_t) — before PUSH ESI                 */
/*                                                                     */
/* 1. Stores town_ptr (+0x24) and resource (+0x44)                     */
/* 2. If either is NULL: sets initialized=0, returns                    */
/* 3. Computes screen_rect from resource world position                 */
/*    (resource+0x2e=X, resource+0x30=Y, resource+0x28=W,              */
/*     resource+0x2a=H)                                                 */
/* 4. Computes source_rect from resource frame dimensions               */
/*    (starts at 0,0, width=resource+0x28, height=resource+0x2a)       */
/* 5. Sets zoom_level=1, stores flags, sets prev_frame=0xFFFF          */
/* 6. If flags & 2 (edge piece):                                       */
/*    - Computes tile_x = world_x/57 - 2 (signed divide by 0x39)      */
/*    - If town->camera_x_offset (+0xA8) < tile_x: sets it to tile_x   */
/* ================================================================== */
void TrackPiece::Init(void* town, RESDATA* res, uint16_t new_flags)
{
    /* Store pointers */
    this->town_ptr = town;             /* +0x24 */
    this->resource = res;              /* +0x44 */

    /* If either is null, mark as uninitialized and bail */
    if (town == nullptr || res == nullptr) {
        this->initialized = 0;         /* +0x18 */
        return;
    }

    /* Read resource geometry fields */
    const TrackResourceGeometry* geometry =
        reinterpret_cast<const TrackResourceGeometry*>(res);
    int16_t world_x = geometry->world_x;
    int16_t world_y = geometry->world_y;
    uint16_t frame_w = geometry->frame_width;
    uint16_t frame_h = geometry->frame_height;

    /* Set screen_rect from resource's world coords and frame dimensions */
    SetRect(&this->screen_rect,
            static_cast<int>(world_x),
            static_cast<int>(world_y),
            static_cast<int>(frame_w) + static_cast<int>(world_x),
            static_cast<int>(frame_h) + static_cast<int>(world_y));

    /* Set source_rect from resource frame dimensions (starts at origin) */
    SetRect(&this->source_rect,
            0,
            0,
            static_cast<int>(frame_w),
            static_cast<int>(frame_h));

    /* Initialize zoom and flags */
    this->zoom_level = 1;              /* +0x48 */
    this->flags = new_flags;           /* +0x2C */
    this->prev_frame = 0xFFFF;         /* +0x54 */

    /* If edge piece (flags & 2), adjust camera X offset in town manager */
    if ((new_flags & 2) != 0) {
        /* Signed divide world_x by 0x39 (57 px/tile), then subtract 2 */
        int tile_x = static_cast<int>(world_x) / 0x39 - 2;
        int32_t* manager_cam_x = &reinterpret_cast<TownTrackFields*>(town)->camera_limit;

        if (*manager_cam_x < tile_x) {
            *manager_cam_x = tile_x;
        }
    }
}


/* ================================================================== */
/* TrackPiece::SetZoom — Change display zoom level                     */
/* Address: 0x40D170                                                   */
/*                                                                     */
/* Only valid if initialized && zoom differs from current value.       */
/*                                                                     */
/* Zoom dispatch via SetFrame (vtable[6]):                             */
/*   1: SetFrame(0)                                                    */
/*   2: SetFrame(1) if count<3, else SetFrame(count-2)                 */
/*   3: SetFrame(count-1) — last frame                                  */
/*   4: Recalcs coords by subtracting tile (0x39 offset), recurses     */
/*      SetZoom(2) on sub-object (town+0x9c), then UpdateAnim          */
/* ================================================================== */
void TrackPiece::SetZoom(short zoom)
{
    if (this->initialized != 1) {
        return;
    }

    if (this->zoom_level == zoom) {
        return;
    }

    this->zoom_level = zoom;

    switch (zoom) {
    case 1:
        /* First frame */
        this->SetFrame(0);
        return;

    case 2:
        {
            uint16_t frame_count = this->resource->frame_count  /* entity_buffer +0x28 */;
            if (frame_count < 3) {
                this->SetFrame(1);
            } else {
                this->SetFrame(frame_count - 2);
            }
        }
        return;

    case 3:
        {
            uint16_t frame_count = this->resource->frame_count  /* entity_buffer +0x28 */;
            this->SetFrame(frame_count - 1);
        }
        return;

    case 4:
        {
            /* Get sub-object from manager at town+0x9c */
            TownTrackFields* town_fields = reinterpret_cast<TownTrackFields*>(this->town_ptr);
            TrackPiece* sub_piece = reinterpret_cast<TrackPiece*>(town_fields->zoom_sub_object);

            /* Copy this object's screen_rect into sub_piece's rect */
            sub_piece->screen_rect = this->screen_rect;

            /* Get camera offset from town manager at +0xa4 */
            int cam_x_offset = town_fields->camera_x;
            sub_piece->screen_rect.left += cam_x_offset * -0x39;
            sub_piece->screen_rect.right += cam_x_offset * -0x39;

            /* Recurse: set sub-object zoom to 2 */
            sub_piece->SetZoom(2);

            /* Dispatch Render on sub-object (vtable[8] at byte offset 0x20) */
            sub_piece->Render();
        }
        return;
    }
}


/* ================================================================== */
/* TrackPiece::SetFrame (vtable[6]) — Set current animation frame      */
/* Address: 0x40D2A0                                                   */
/*                                                                     */
/* Called by UpdateAnim (to advance animation) and SetZoom (to change  */
/* zoom level).                                                        */
/*                                                                     */
/* 1. Store frame index at +0x4C (current_frame)                       */
/* 2. Read frame width from resource+0x28                              */
/* 3. Set source_rect.left = frame * frameWidth                        */
/* 4. Set source_rect.right = (frame + 1) * frameWidth                 */
/* 5. Call Render (vtable[8]) to blit the updated frame                */
/* 6. Reset anim_tick (+0x50) to 0                                     */
/* ================================================================== */
void TrackPiece::SetFrame(int frame)
{
    if (this->initialized != 1) {
        return;
    }

    this->current_frame = frame;         /* +0x4C */

    /* Update source_rect X offsets based on frame index */
    uint16_t frame_w = this->resource->frame_w  /* entity_buffer[0x24]: frame_w */;
    this->source_rect.left = frame * static_cast<int>(frame_w);
    this->source_rect.right = (frame + 1) * static_cast<int>(frame_w);

    /* Call Render to blit updated frame */
    this->Render();

    /* Reset animation tick counter */
    this->anim_tick = 0;                 /* +0x50 */
}


/* ================================================================== */
/* TrackPiece::UpdateAnim (vtable[7]) — Advance animation tick         */
/* Address: 0x40D2F0                                                   */
/*                                                                     */
/* Increments anim_tick unconditionally (even if zoom != 1).           */
/* Only advances frames when ALL conditions are met:                   */
/*   - zoom_level == 1                                                 */
/*   - resource frame count >= 4                                        */
/*   - anim_tick >= 3 (after increment)                                 */
/*                                                                     */
/* Frame advancement wraps at (frame_count - 3):                       */
/*   0, 1, 2, ..., frame_count-4, 0, ...                               */
/*                                                                     */
/* When frame changes, calls SetFrame(vtable[6]) with the new index.   */
/*                                                                     */
/* Return value (8-bit):                                               */
/*   1 = zoom_level == 1 (animation is running/ticked)                  */
/*   0 = zoom_level != 1 or uninitialized                              */
/* ================================================================== */
uint8_t TrackPiece::UpdateAnim()
{
    if (this->initialized != 1) {
        return 0;
    }

    /* Increment tick counter — happens regardless of zoom level */
    int tick = this->anim_tick + 1;
    this->anim_tick = tick;

    /* Only animate for zoom level 1 */
    if (this->zoom_level == 1) {
        uint16_t frame_count = this->resource->frame_count  /* entity_buffer +0x28 */;

        if (frame_count >= 4) {
            /* Advance frame only when tick exceeds threshold */
            if (tick >= 3) {
                int cur_frame = this->current_frame;     /* +0x4C */
                int next_frame;

                if (cur_frame < frame_count - 3) {
                    next_frame = cur_frame + 1;
                } else {
                    next_frame = 0;  /* wrap around */
                }

                /* Call SetFrame if frame needs to change */
                if (cur_frame != next_frame) {
                    this->SetFrame(next_frame);
                }
            }
        }
        /* Return 1 — animation state is active (zoom == 1) */
        return 1;
    }

    /* Zoom != 1 — no animation */
    return 0;
}


/* ================================================================== */
/* TrackPiece::Render (vtable[8]) — Draw track piece sprite to screen  */
/* Address: 0x40D340                                                   */
/*                                                                     */
/* Called by SetFrame after updating source_rect. Also called          */
/* externally during the render loop.                                  */
/*                                                                     */
/* 1. Bail if not initialized or render_enabled == 0                   */
/* 2. Copy screen_rect to local onscreen rect                          */
/* 3. If edge piece (flags & 2): shift left by (cam_x * -57)           */
/* 4. Get town viewport bounds from town+0x30..0x3c                    */
/* 5. Clip: bail if onscreen is outside viewport                       */
/* 6. Blit via Town_BlitElement with source_rect as source             */
/* 7. Offset onscreen by town scroll and invalidate tilemap rect       */
/* ================================================================== */
void TrackPiece::Render()
{
    /* Skip if not initialized or render is disabled */
    if (this->initialized != 1 || this->render_enabled == 0) {
        return;
    }

    /* Compute on-screen rect from screen_rect */
    RECT onscreen;
    onscreen.left   = this->screen_rect.left;
    onscreen.top    = this->screen_rect.top;
    onscreen.right  = this->screen_rect.right;
    onscreen.bottom = this->screen_rect.bottom;

    TownTrackFields* town_fields = reinterpret_cast<TownTrackFields*>(this->town_ptr);

    /* Apply edge offset if this is an edge piece (flags & 2) */
    if ((this->flags & 2) != 0) {
        int cam_x = town_fields->camera_x;                    /* +0xa4 */
        onscreen.left   += cam_x * -0x39;
        onscreen.right  += cam_x * -0x39;
    }

    /* Get town viewport bounds */
    TrackViewportFields* town_view =
        reinterpret_cast<TrackViewportFields*>(town_fields->viewport);
    int view_left   = town_view->left;
    int view_top    = town_view->top;
    int view_right  = town_view->right;
    int view_bottom = town_view->bottom;

    /* Check visibility against viewport.
     * Binary gates with four && conditions (all must pass to render):
     *   view_left <= onscreen.left + view_left  →  onscreen.left >= 0
     *   view_top <= onscreen.top
     *   onscreen.right + view_left <= view_right
     *   onscreen.bottom <= view_bottom
     * We negate each for early-return (any true = bail). */
    if (onscreen.left + view_left < view_left ||
        view_top > onscreen.top ||
        onscreen.right + view_left > view_right ||
        view_bottom < onscreen.bottom) {
        return;
    }

    /* Blit via Town_BlitElement */
    Town_BlitElement(
        *reinterpret_cast<void**>(this->resource->entity_buffer + 0x20),
        /* UI panel from resource */
        onscreen.left + view_left,
        onscreen.top,
        onscreen.right + view_left,
        onscreen.bottom,
        reinterpret_cast<SurfaceFields*>(town_view->surface_object)->surface,
        /* blit surface */
        this->source_rect.left,
        this->source_rect.top,
        this->source_rect.right,
        this->source_rect.bottom,
        1
    );

    /* Offset onscreen rect by town scroll position */
    OffsetRect(&onscreen,
               town_fields->scroll_x,                          /* +0x08 */
               town_fields->scroll_y);                         /* +0x0c */

    /* Invalidate tilemap rect for dirty rect tracking */
    TileMap_InvalidateRect(
        g_tilemap,
        onscreen.left, onscreen.top,
        onscreen.right, onscreen.bottom
    );
}


/* ================================================================== */
/* TrackPiece::RecalcRect — Recalculate screen rect from resource pos  */
/* Address: 0x40D470                                                   */
/*                                                                     */
/* Converts resource grid coordinates to pixel positions:               */
/* - Multiplies by 0x39 (57 = tile width in pixels)                    */
/* - Applies fixed offsets: -0x32 (X) and -0x28 (Y)                    */
/* - Updates screen_rect accordingly                                    */
/* - If flags & 2, adjusts camera offset in town manager               */
/* ================================================================== */
void TrackPiece::RecalcRect()
{
    /* Only recalc if resource Y coords are < 4 (valid grid position) */
    if (this->resource->world_y /* entity_buffer +0x2c, Ghidra: *(short*)(res+0x30) */ >= 4) {
        return;
    }

    /* Convert grid coords to pixel positions */
    int16_t world_x = this->resource->world_x    /* entity_buffer +0x2a, Ghidra: *(short*)(res+0x2e) */;
    int16_t world_y = this->resource->world_y /* entity_buffer +0x2c, Ghidra: *(short*)(res+0x30) */;

    world_x = world_x * 0x39 - 0x32;  /* tile to pixel, with offset */
    world_y = world_y * 0x39 - 0x28;

    this->resource->world_x = world_x;
    this->resource->world_y /* entity_buffer +0x2c, Ghidra: *(short*)(res+0x30) */ = world_y;

    /* Update screen_rect from updated resource coords */
    uint16_t frame_w = this->resource->frame_w  /* entity_buffer[0x24]: frame_w */;
    uint16_t frame_h = this->resource->frame_h  /* entity_buffer[0x26]: frame_h */;

    SetRect(&this->screen_rect,
            static_cast<int>(world_x),
            static_cast<int>(world_y),
            static_cast<int>(frame_w) + static_cast<int>(world_x),
            static_cast<int>(frame_h) + static_cast<int>(world_y));

    /* If edge piece, adjust camera offset */
    if ((this->flags & 2) != 0) {
        int tile_x = static_cast<int>(world_x) / 0x39 - 2;
        int32_t* manager_cam_x =
            &reinterpret_cast<TownTrackFields*>(this->town_ptr)->camera_limit;

        if (*manager_cam_x < tile_x) {
            *manager_cam_x = tile_x;
        }
    }
}
