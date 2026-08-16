// Status: INTEGRATED
/**
 * GameView.cpp — GameView lifecycle and building-selection/tracking
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation (database "loco").
 *
 * The compiler owns the GameView vtable; the original constructor,
 * destructor body, cleanup, and the building-selection/tracking family
 * below were verified instruction-by-instruction against the binary
 * (see GameView.h for the 22-slot vtable layout).
 *
 * select_building/track_building/deselect_building/update_selection/
 * render_selection/update_cursor_child were previously transcribed onto
 * town/Town.h/.cpp as Town:: methods. Every one of their call sites
 * loads ECX with the bare immediate 0x4852A0 (this class's global
 * instance), never a Town pointer-variable dereference — see
 * GameView.h's class doc comment for the full evidence trail. Moved
 * here with their field/method references corrected to this class's
 * real layout (game_object_sub-relative rects, inherited Entity::anim_index,
 * etc. — see GameView.h's field-layout comment).
 */

#include "GameView.h"
#include "../game/Building.h"
#include "../game/Vehicle.h"
#include "../game/World.h"
#include "../graphics/DDRAW_Building.h"
#include "../ui/UIPANEL_Surface.h"   /* UIPANEL_InitSurface, real C++ linkage */
/* graphics/LOCOBITMAP.h — real canonical UIPANEL_Surface definition.
 * town/Town.cpp/.h can't include this directly (LOCOBITMAP.h also
 * defines a conflicting, differently-shaped `class PostcardAlbum` that
 * collides with ui/PostcardAlbum.h, which Town.cpp includes), but this
 * file includes neither ui/PostcardAlbum.h nor anything that pulls it in
 * — checked before adding this — so the real type is used directly here
 * instead of Town.cpp's forward-declared-opaque-pointer workaround. */
#include "../graphics/LOCOBITMAP.h"
#include "../platform/ddraw_interfaces.h"   /* real IDirectDrawSurface4::Blt */

#ifndef _WIN32
#include <cassert>
#include <cstdio>
#include <unordered_set>
#endif

/* Panel-family helpers — 0x4544E0 (Panel base init) and 0x454630
 * (Panel partial destructor / RESDATA base cleanup). */
extern void __fastcall RESDATA_BaseInit(void* self);   /* 0x4544E0 */
extern void __fastcall RESDATA_DtorBase(void* self);   /* 0x454630 */

/* GameObject_GetRelPos (0x436A40) — declaration already visible via
 * core/GameView.h -> game/Panel.h's own `extern int __thiscall
 * GameObject_GetRelPos(...)`; not re-declared here (a second,
 * differently-typed local declaration would hard-conflict once both
 * are visible in the same TU, same issue town/Town.cpp hit and
 * resolved the same way). */

/* Tilemap */
extern void __thiscall TileMap_InvalidateRect(void* tilemap, int left, int top,
                                               int right, int bottom);     /* 0x455840 */

/* RESDATA_IsBuildingTile — real implementation world/tilemap.cpp/.h
 * (0x44BD30). Declared locally to avoid pulling world/tilemap.h's
 * conflicting local Win32/global declarations into this file. */
extern uint8_t RESDATA_IsBuildingTile(int32_t tile_obj);

/* UIPANEL_CreateSurface — real def graphics/LOCOBITMAP.cpp:0x42A110,
 * placement-style constructor for UIPANEL_Surface (see town/Town.cpp's
 * identical declaration/doc comment for the extern-"C" landmine this
 * avoids — kept as plain C++ linkage here too, matching the real symbol). */
extern void   UIPANEL_CreateSurface(UIPANEL_Surface* surface);   /* 0x42A110 */
extern size_t UIPANEL_Surface_Size();  /* graphics/LOCOBITMAP.cpp — real sizeof(UIPANEL_Surface) */
extern void*  operator_new(size_t size);   /* 0x465CE0 */

/* UIPANEL_Blit — real def ui/UIPANEL_Surface.cpp (0x42B050), C++-mangled. */
extern bool UIPANEL_Blit(void* renderer, uint32_t src_x, uint32_t src_y,
                          int32_t dest_x, uint32_t dest_y,
                          void* dest_surface, uint32_t clip_left, uint32_t clip_top,
                          int32_t clip_right, uint32_t clip_bottom, uint32_t flags); /* 0x42B050 */

/* Win32 — matches town/Town.cpp's own extern "C" declaration exactly
 * (same real symbol, avoid an ODR-mismatched second declaration).
 *
 * OffsetRect: the real definition (graphics/sdl3_window.cpp, declared
 * extern "C" in graphics/sdl3_window.h) returns BOOL and takes RECT*.
 * game/Panel.h's own `extern void __stdcall OffsetRect(void*, int, int)`
 * (visible transitively via this file's Panel.h include) has NO
 * extern "C" linkage and a different parameter type, so it is a distinct,
 * never-actually-linkable overload — the same latent mismatch
 * game/TrackPiece.cpp already routes around with its own local
 * `extern "C" void OffsetRect(void*, int, int)` declaration. Declared
 * correctly here (matching the real symbol) so the call below binds to
 * it; the return value is discarded, matching every other caller in this
 * tree. */
extern "C" {
    BOOL IntersectRect(RECT* dest, const RECT* src1, const RECT* src2);
    void OffsetRect(RECT* rect, int dx, int dy);
}

/* ================================================================== */
/* Global variables                                                    */
/* ================================================================== */

extern ResourceManager g_resmgr;        /* 0x4855E8 — object, not a pointer (see
                                          * town/Town.cpp's identical declaration/
                                          * doc comment for the historical void*
                                          * landmine this avoids). Used by
                                          * handle_tile_click via the real typed
                                          * ResourceManager::GetById (0x446EA0),
                                          * not the tree-wide inconsistent
                                          * `ResourceManager_GetById` free-function
                                          * shape (see game/Panel.h's own note). */
extern char    g_game_mode;             /* 0x4852AC — current game mode */
extern int32_t g_demo_mode;             /* 0x4A9918 — 1 = demo mode */
extern char    g_ddraw_active;          /* 0x4A9F78 — 1 = DirectDraw building mode active */
extern void*   g_active_panel;          /* 0x4FD3E0 — active panel override */
extern void*   g_tilemap;               /* 0x4FD244 — tile map */
extern void*   g_primary_surface;       /* 0x4FD3C4 — primary DirectDraw surface */
extern int     g_cursor_world_x;        /* 0x4FD348 — cursor world X */
extern int     g_cursor_world_y;        /* 0x4FD34C — cursor world Y */
extern void*   g_world;                 /* 0x4A98B0 — World singleton (void* storage;
                                          * see game/Vehicle.h/game/World.h call sites
                                          * for the same static_cast<World*> convention
                                          * already used by town/Town.cpp). */

/* Canonical typed declaration: graphics/DDRAW.h (avoided here directly —
 * that header's g_tilemap/g_primary_surface globals conflict in type
 * with this file's own local extern void* declarations above, the same
 * conflict graphics/DDRAW_Building.h's own doc comment describes). */
extern DDRAW_Building* g_ddraw_building; /* 0x4A9EF0 */

/* Viewport rect globals (one RECT at 0x4AAD14) */
extern int g_viewport_rect_left;        /* 0x4AAD14 */
extern int g_viewport_rect_top;         /* 0x4AAD18 */
extern int g_viewport_rect_right;       /* 0x4AAD1C */
extern int g_viewport_rect_bottom;      /* 0x4AAD20 */

/* Scroll offset applied to the viewport rect (immediately following it
 * in memory, but a separate pair of globals — not part of the RECT
 * above). Read by center_on_point (0x42D440). */
extern int g_viewport_x;                /* 0x4AAD24 */
extern int g_viewport_y;                /* 0x4AAD28 */

/* Client-area metrics, consumed by center_on_point (0x42D440) as four
 * consecutive dwords forming a RECT (left/top/right/bottom), then offset
 * by g_viewport_x/g_viewport_y above via OffsetRect. The names look like
 * scalar width/height/offset metrics but the disassembly stores all four
 * into one contiguous stack RECT and passes &rect to OffsetRect — treat
 * them as that RECT's four fields, not as independent scalars. */
extern int g_client_width;              /* 0x485220 — RECT.left   */
extern int g_client_height;             /* 0x485224 — RECT.top    */
extern int g_client_offset_x;           /* 0x485228 — RECT.right  */
extern int g_client_offset_y;           /* 0x48522C — RECT.bottom */

/* ================================================================== */
/* Host-only mirror view for foreign object layouts                    */
/*                                                                      */
/* Originally a verbatim copy of town/Town.cpp's own UIPANEL_SurfaceView/  */
/* view() helper (pre-existing, documented technical debt). This class's   */
/* own `overlay_panel` field is `ResourceObject*` (a generic resource-     */
/* handle type, matching the same idiom used tree-wide for resource-       */
/* shaped objects — see GameView.h's field doc), not the real              */
/* UIPANEL_Surface* itself, so this mirror still stands in for that one    */
/* specific field. The genuinely UIPANEL_Surface*-typed case (sizing the   */
/* overlay surface handle_tile_click constructs, via PanelGraphicsView's   */
/* `surface` member below) now uses the real type directly instead of a    */
/* second mirror overload — this file includes graphics/LOCOBITMAP.h       */
/* directly (see the include comment above) since, unlike Town.cpp, it     */
/* doesn't also include the conflicting ui/PostcardAlbum.h.                 */
/* ================================================================== */

namespace {

struct UIPANEL_SurfaceView {
    void*    vtable;       // +0x00
    int32_t  mode;          // +0x04
    int32_t  width;          // +0x08
    int32_t  height;         // +0x0C
    uint8_t  has_palette;   // +0x10
    uint8_t  flags;          // +0x11
    uint8_t  _pad_12[2];
    uint16_t* palette_ptr; // +0x14
    uint8_t*  pixels;       // +0x18
    void*     ddraw_surf;   // +0x1C
};

UIPANEL_SurfaceView* view(ResourceObject* surface)
{
    return reinterpret_cast<UIPANEL_SurfaceView*>(surface);
}

/* `game_object_sub.resource`'s concrete class is unidentified (mirrors
 * only the two Ghidra-confirmed fields — same caveat as Town.h's
 * `panel_graphics` field comment). */
struct PanelGraphicsView {
    uint8_t          _pad_00[0x10];
    UIPANEL_Surface* surface;      // +0x10
    uint8_t          _pad_14[0x0C];
    void*            anim_table;   // +0x20 — array of 0x18-byte entries
};

PanelGraphicsView* panel_graphics_view(void* panel_graphics)
{
    return reinterpret_cast<PanelGraphicsView*>(panel_graphics);
}

}  // namespace

/**
 * GameView::GameView — constructor
 * Address: 0x42CCE0
 *
 * The binary runs RESDATA_BaseInit (0x4544E0) and the embedded Entity
 * constructor (0x405790) before this class's own field writes.  In
 * natural C++ the Panel base and the Entity member are constructed
 * first (compiler-managed vtables), so those two calls are represented
 * by the base/member construction and the body below writes exactly the
 * recovered fields: type=0x0E, selected_building=nullptr, +0xAD active
 * flag=1, overlay_panel=nullptr.
 */
GameView::GameView() : game_object_sub(-1, -1, 0, 0)
{
    this->type = 0x0E;                  /* +0x04 (GameObject::type) */
    this->selected_building = nullptr;  /* +0xE0 */
    this->dim_flag = 1;                 /* +0xAD active/dim flag */
    this->overlay_panel = nullptr;      /* +0x17C */
}

/**
 * GameView::~GameView — destructor body
 * Address: 0x42CD80 (scalar deleting wrapper at 0x42CD60 is
 * compiler-generated and not reconstructed here)
 *
 * The binary destroys the embedded Entity (GameObject_DtorBody at
 * +0xE4) then runs Panel_DtorBody (0x4545A0).  In natural C++ the
 * Panel base destructor runs after this body and the Entity member is
 * destroyed after the base chain; the resources are independent, so
 * the order swap is behavior-neutral.
 */
GameView::~GameView()
{
    /* Panel::~Panel → Panel::DtorBody (0x4545A0) → GameObject_DtorBody;
     * the embedded Entity member is destroyed afterwards.
     *
     * NOTE: track_building() (0x42D1A0) walks Panel's own +0xD0 child
     * list as a TrackPiece* chain, but Panel::~Panel only owns/frees the
     * head pointer at +0xD0 today (see game/Panel.h/.cpp) — it does not
     * walk the rest of the chain. On the original binary this was the
     * same shape (RESDATA_DtorBase frees the head the same way). Whether
     * that's a real original leak or the list is single-element in
     * practice is unconfirmed; it is not exercised on host regardless,
     * because g_town_view's GameView is placement-new'd into static
     * storage (town/sdl3_town_mode3.cpp) and never explicitly destroyed
     * — this destructor has zero call sites on host today. Flagging here
     * rather than guessing at a fix so it isn't silently forgotten if
     * GameView destruction is ever wired up for real. */
}

/**
 * GameView::cleanup
 * Address: 0x42CDD0 — vtable [15] (+0x3C)
 *
 * 1. Destroys the overlay panel (the original's vtable[0] scalar deleting
 *    destructor with flag 1 -- ordinary `delete` reproduces this exactly;
 *    see ResourceObject.h. The original does NOT clear the pointer
 *    afterwards, so this doesn't either.)
 * 2. Resets the embedded Entity via its vtable[6] (InitBase, 0x405900)
 *    with (0, -1, 0).
 * 3. Resets self via vtable[6] (Panel::Init, 0x454680) with (0, -1, 0).
 * 4. Runs RESDATA_DtorBase (0x454630).
 */
void GameView::cleanup()
{
    if (this->overlay_panel != nullptr) {
        delete this->overlay_panel;
    }

    this->game_object_sub.InitBase(0, -1, false);
    this->Init(0, -1, false);
    RESDATA_DtorBase(this);
}

/**
 * GameView::center_on_point — vtable[3] (+0x0C). Address: 0x42D440.
 *
 * Verified against raw vtable bytes: GameView's vtable (0x477D30) slot 3
 * is this function itself (0x42D440), overriding Entity::MoveTo's slot
 * ([3] on Entity's own vtable 0x477488 is 0x405C00). The two repositions
 * of `this` below are therefore `this->Entity::MoveTo(...)` (explicitly
 * scoped, matching the original's hardcoded `CALL 0x00405C00` — a plain
 * virtual `this->MoveTo(...)` would recurse into center_on_point). The
 * two repositions of game_object_sub are ordinary `MoveTo(...)` calls —
 * genuinely virtual in the original too, but unambiguous since
 * game_object_sub's dynamic type is exactly Entity.
 *
 * See GameView.h's doc comment for the full field/geometry breakdown.
 */
void GameView::center_on_point(int center_x, int center_y)
{
    /* Client rect, consumed as {left, top, right, bottom} (see the
     * globals' declarations above for why these look like scalar
     * metrics but are read as one RECT). */
    RECT client_rect;
    client_rect.left   = g_client_width;
    client_rect.top    = g_client_height;
    client_rect.right  = g_client_offset_x;
    client_rect.bottom = g_client_offset_y;
    OffsetRect(&client_rect, g_viewport_x, g_viewport_y);

    /* No null check in the original — both resources are dereferenced
     * unconditionally, matching this reconstruction. */
    RESDATA* own_res = static_cast<RESDATA*>(this->resource);                 /* +0x40 */
    RESDATA* sub_res  = static_cast<RESDATA*>(this->game_object_sub.resource); /* +0x124 */
    int deadzone = own_res->frame_width + (sub_res->frame_width >> 1);

    if (center_x < 0) {
        center_x = 0;
    }

    if (this->dim_flag) {                                                     /* +0xAD */
        if (client_rect.right - deadzone < center_x) {
            this->dim_flag = 0;
            this->StopSound(0);            /* vtable[7], generic state-change notify */

            /* Reposition every child TrackPiece for the "anchored left"
             * side, matching track_building's own +0x28-as-next-pointer
             * chain-walk idiom (game/TrackPiece.h's `sub_resource` is a
             * genuine linked-list "next" pointer despite its int32_t
             * declared type — see track_building's identical comment). */
            for (TrackPiece* child = static_cast<TrackPiece*>(this->child_surface); /* +0xD0 */
                 child != nullptr;
                 child = reinterpret_cast<TrackPiece*>(
                     static_cast<uintptr_t>(static_cast<uint32_t>(child->sub_resource)))) {
                RESDATA* child_res = child->resource;
                child->screen_rect.left  = own_res->frame_width - child_res->frame_w - child_res->world_x;
                child->screen_rect.right = own_res->frame_width - child_res->world_x;
                child->Render();                                              /* vtable[8] */
            }

            /* Notification still runs even with an empty child list —
             * the original jumps directly here, not around it. */
            this->game_object_sub.StopSound(this->dim_flag);
        } else {
            this->Entity::MoveTo(((sub_res->frame_width >> 1) - 3) + center_x,
                                  center_y - (own_res->frame_height >> 1));
            this->game_object_sub.MoveTo(center_x - (sub_res->frame_width >> 1),
                                          center_y - (sub_res->frame_height >> 1));
            return;
        }
    }

    if (center_x < client_rect.left + deadzone) {
        this->dim_flag = 1;
        this->StopSound(1);

        /* Reposition every child TrackPiece for the "anchored right" side. */
        for (TrackPiece* child = static_cast<TrackPiece*>(this->child_surface);
             child != nullptr;
             child = reinterpret_cast<TrackPiece*>(
                 static_cast<uintptr_t>(static_cast<uint32_t>(child->sub_resource)))) {
            RESDATA* child_res = child->resource;
            child->screen_rect.left  = child_res->world_x;
            child->screen_rect.right = child_res->frame_w + child_res->world_x;
            child->Render();
        }

        this->game_object_sub.StopSound(this->dim_flag);

        this->Entity::MoveTo(((sub_res->frame_width >> 1) - 3) + center_x,
                              center_y - (own_res->frame_height >> 1));
        this->game_object_sub.MoveTo(center_x - (sub_res->frame_width >> 1),
                                      center_y - (sub_res->frame_height >> 1));
        return;
    }

    this->Entity::MoveTo((center_x - deadzone) + 3,
                          center_y - (own_res->frame_height >> 1));
    this->game_object_sub.MoveTo(center_x - (sub_res->frame_width >> 1),
                                  center_y - (sub_res->frame_height >> 1));
}

/**
 * GameView::is_valid_placement — static buildable-tile check.
 * Address: 0x42CF90.
 *
 * Validates entity initialized (+0x18) and tile type byte at
 * resource+8: 0x07 always valid; 0x08/0x02/0x06 must be visible;
 * 0x04 must be connected (+0x62C); 0x03 must be a building tile;
 * 0x0C valid when resource id > 0x300F.
 */
bool GameView::is_valid_placement(Building* entity)
{
    uint8_t tile_type = 0;
    if (entity != nullptr && entity->initialized == 1) {      /* +0x18 */
        if (entity->resource != nullptr) {                    /* +0x40 */
            tile_type = static_cast<RESDATA*>(entity->resource)->object_type;
        }
    }

    if (tile_type == 0) {
        return false;
    }
    if (tile_type == 0x07) {        /* always buildable */
        return true;
    }
    if (tile_type == 0x08 || tile_type == 0x02 || tile_type == 0x06) {
        return entity->visible == 1;                    /* +0x24 */
    }
    if (tile_type == 0x04 &&
        /* +0x62C is well past RESDATA's documented/asserted 0x1D8 size —
         * this resource type's data extends beyond the common RESDATA
         * header into fields RESDATA doesn't model; kept as a raw offset
         * read rather than forced through RESDATA's (too-small) type. */
        *reinterpret_cast<uint8_t*>(
            reinterpret_cast<intptr_t>(entity->resource) + 0x62C) == 1) {
        return true;                /* connected tile */
    }
    if (tile_type == 0x03 &&
        RESDATA_IsBuildingTile(static_cast<int32_t>(
            reinterpret_cast<intptr_t>(entity->resource)))) {
        return true;                /* building tile */
    }
    if (tile_type == 0x0C) {        /* large-ID tile (ID > 0x300F) */
        int32_t id = -1;
        if (entity->resource != nullptr) {
            id = static_cast<RESDATA*>(entity->resource)->resource_id;
        }
        if (id > 0x300F) {
            return true;
        }
    }
    return false;
}

/**
 * GameView::select_building — select/focus a building (nullptr = deselect).
 * Address: 0x42D040.
 */
uint8_t GameView::select_building(Building* building)
{
    if (building != nullptr && g_game_mode == 3) {
        if (is_valid_placement(building) && g_demo_mode != 1) {
            this->update_child_flags = 1;                       /* +0x88 */

            uint8_t tile_type = (building->resource != nullptr)
                ? static_cast<RESDATA*>(building->resource)->object_type
                : 0;
            this->selected_building_type = tile_type;           /* +0x16C */

            if (g_ddraw_active == 0) {
                g_active_panel = this;
            }
            this->selected_building = building;                 /* +0xE0 */

            int center_x = (building->screen_rect.right - building->screen_rect.left) / 2 +
                            building->screen_rect.left;
            int center_y = (building->screen_rect.bottom - building->screen_rect.top) / 2 +
                            building->screen_rect.top;
            this->center_on_point(center_x, center_y);          /* vtable[3] */

            short zoom = (this->selected_building_type == 6) ? 1 : 3;
            this->track_piece->SetZoom(zoom);                   /* 0x40D170 */
            this->track_piece->Render();                        /* vtable[8] */

            /* Reads this+0x08..+0x14: GameObject::screen_rect, inherited
             * DIRECTLY on `this` (not game_object_sub — see GameView.h's
             * field-layout comment on the two distinct +0x28-shaped
             * accesses for the analogous distinction). */
            TileMap_InvalidateRect(g_tilemap,
                this->screen_rect.left, this->screen_rect.top,
                this->screen_rect.right, this->screen_rect.bottom);

            g_ddraw_building->SelectBuilding(this->selected_building);

            return this->update_child_flags;
        }
    }

    /* Deselect path */
    this->update_child_flags = 0;                               /* +0x88 */
    this->selected_building_type = 0;                            /* +0x16C */

    g_active_panel = (g_ddraw_active == 1) ? g_ddraw_building : nullptr;

    this->UpdateChild();                     /* vtable[1] on `this` (Panel::UpdateChild) */
    this->game_object_sub.InvalidateRect();  /* vtable[1] on embedded Entity */

    return this->update_child_flags;
}

/**
 * GameView::track_building — per-frame tracking of the selected building.
 * Address: 0x42D1A0.
 */
void GameView::track_building()
{
    if (!this->update_child_flags) {                             /* +0x88 */
        return;
    }

    if (this->selected_building_type == 6 &&                     /* +0x16C */
        this->selected_building->visible == 0) {                 /* +0x24 */
        this->select_building(nullptr);
    }

    Building* building = this->selected_building;                /* +0xE0 */
    int cx = (building->screen_rect.right - building->screen_rect.left) / 2 +
             building->screen_rect.left;
    int cy = (building->screen_rect.bottom - building->screen_rect.top) / 2 +
             building->screen_rect.top;

    if (this->building_center_x != cx ||                         /* +0x190 */
        this->building_center_y != cy) {                         /* +0x194 */
        this->center_on_point(cx, cy);                            /* vtable[3] */
        this->building_center_x = cx;
        this->building_center_y = cy;
    }

    /* Result unused by the original — kept for the (possibly side-
     * effecting) call itself, matching the binary exactly. */
    int unused_rel_pos[2];
    GameObject_GetRelPos(this, unused_rel_pos, g_cursor_world_x, g_cursor_world_y);

    /* Update each child sprite (vtable[20] dispatch per child). Chained
     * via TrackPiece::sub_resource (+0x28) — Ghidra-confirmed as a real
     * linked-list "next" pointer, despite game/TrackPiece.h currently
     * declaring it `int32_t` (a pre-existing, wider landmine — see
     * town/Town.cpp's identical comment on this same field). Matching
     * that established workaround here rather than reading the full 8
     * bytes as `*(void**)`, which would pull in 4 bytes of the adjacent
     * flags/_pad_2E fields as garbage upper address bits on this 64-bit
     * host. */
    for (TrackPiece* child = static_cast<TrackPiece*>(this->child_surface);  /* +0xD0 */
         child != nullptr;
         child = reinterpret_cast<TrackPiece*>(
             static_cast<uintptr_t>(static_cast<uint32_t>(child->sub_resource)))) {
        this->update_cursor_child(child);                         /* vtable[20] */
    }

    this->game_object_sub.InvalidateRect();                       /* vtable[1] on embedded Entity */
}

/**
 * GameView::deselect_building — remove the building-selection overlay.
 * Address: 0x42D280.
 */
void GameView::deselect_building(int32_t /*unused1*/, int32_t /*unused2*/,
                                  int32_t /*unused3*/, int32_t /*unused4*/)
{
    RECT clip_rect;
    int right_inset, bottom_inset;

    if (this->selected_building_type == 7) {
        right_inset  = this->game_object_sub.source_rect.right - 0x90;
        bottom_inset = this->game_object_sub.source_rect.bottom - 0x8C;
    } else {
        right_inset  = this->game_object_sub.source_rect.right >> 2;
        bottom_inset = this->game_object_sub.source_rect.bottom >> 2;
    }

    clip_rect.left   = this->game_object_sub.screen_rect.left + right_inset;
    clip_rect.right  = this->game_object_sub.screen_rect.right - right_inset;
    clip_rect.top    = this->game_object_sub.screen_rect.top + bottom_inset;
    clip_rect.bottom = this->game_object_sub.screen_rect.bottom - bottom_inset;

    RECT viewport_rect;
    viewport_rect.left   = g_viewport_rect_left;
    viewport_rect.top    = g_viewport_rect_top;
    viewport_rect.right  = g_viewport_rect_right;
    viewport_rect.bottom = g_viewport_rect_bottom;
    IntersectRect(&clip_rect, &clip_rect, &viewport_rect);

#ifndef _WIN32
    /* The original (0x42D280) never null-checks overlay_panel here: on
     * Windows, nothing reaches deselect_building() before handle_tile_click
     * has already run at least once and set it. On this host build,
     * handle_tile_click's only caller (GameLoop_PostSetupBootstrap) has no
     * definition anywhere in the tree yet, so overlay_panel is always null
     * — guard rather than dereference a null resource pointer, matching
     * the same host-reachable-but-uninitialized idiom already established
     * elsewhere (core/GameObject.cpp's Entity::Update null-resource guard). */
    if (this->overlay_panel == nullptr) {
        static std::unordered_set<const void*> warned_null;
        if (warned_null.insert(this).second) {
            std::fprintf(stderr,
                "[HOST] GameView::deselect_building: skipping overlay "
                "restore for %p -- overlay_panel is null (handle_tile_click "
                "never ran; see PROGRESS.md)\n",
                static_cast<void*>(this));
            std::fflush(stderr);
        }
        return;
    }
#endif

    /* overlay_panel's own ddraw_surf field (no extra dereference). */
    IDirectDrawSurface4* backing_surface =
        static_cast<IDirectDrawSurface4*>(view(this->overlay_panel)->ddraw_surf);

    /* Restore the cached background. Real disassembly (0x42D280) confirms
     * the dest-rect argument is `puVar1 = (uint*)(param_1 + 0x180)` — the
     * ADDRESS of the four backup_* fields treated as one RECT{left, top,
     * right, bottom} (same layout `handle_tile_click`'s own `SetRect`
     * call already establishes for these fields) — not their value. The
     * previous transcription here passed `this->backup_surface` (the
     * value, always 0 per `handle_tile_click`'s own zero-init), which
     * would hand `Blt` a null dest_rect instead of the real backup
     * region — a real, live bug, fixed alongside converting this call
     * from raw vtable-slot-5 dispatch to the typed interface (the same
     * conversion already applied to this exact COM slot elsewhere:
     * `ui/UIPANEL_Surface.cpp`, `world/tilemap.cpp`,
     * `graphics/LOCOBITMAP.cpp`). */
    backing_surface->Blt(
        reinterpret_cast<RECT*>(&this->backup_surface),  // ABI_BOUNDARY: real x86 layout reinterpreted as RECT{left,top,right,bottom}
        static_cast<IDirectDrawSurface4*>(g_primary_surface),
        &clip_rect,
        0x1000000,
        nullptr);

    /* Panel anim-table flag at +0x20 + index*0x18 + 0x16; the index is
     * Entity::anim_index (+0x28), INHERITED DIRECTLY on `this` via
     * Panel : Entity : GameObject — NOT any field of game_object_sub
     * (see GameView.h's field-layout comment). */
    uint32_t panel_flags = 0;
    int panel_index = static_cast<int>(this->anim_index);         /* +0x28 */
    PanelGraphicsView* pgfx = panel_graphics_view(this->game_object_sub.resource);
    const uint8_t* anim_entry = reinterpret_cast<const uint8_t*>(pgfx->anim_table) +
                                 panel_index * 0x18;
    if (anim_entry[0x16] == 1) {
        panel_flags = 0x20;
    }

    void* panel_surface = pgfx->surface;
    UIPANEL_Blit(
        panel_surface,
        this->backup_surface,
        this->backup_x,
        this->backup_y,
        this->backup_width,
        backing_surface,
        this->backup_surface,
        this->backup_x,
        this->backup_y,
        this->backup_width,
        panel_flags);
}

/**
 * GameView::update_selection — blit the selection overlay to primary.
 * Address: 0x42D3A0.
 */
void GameView::update_selection(int32_t /*unused1*/, int32_t /*unused2*/,
                                 int32_t /*unused3*/, int32_t /*unused4*/)
{
    UIPANEL_Blit(
        this->overlay_panel,
        this->game_object_sub.screen_rect.left,
        this->game_object_sub.screen_rect.top,
        this->game_object_sub.screen_rect.right,
        this->game_object_sub.screen_rect.bottom,
        g_primary_surface,
        this->game_object_sub.source_rect.left,
        this->game_object_sub.source_rect.top,
        this->game_object_sub.source_rect.right,
        this->game_object_sub.source_rect.bottom,
        0x40);
}

/**
 * GameView::render_selection — draw the selection highlight for one tile.
 * Address: 0x42D400 (vtable[21], +0x54).
 *
 * Confirmed via disassembly that ECX is never reloaded between the
 * +0x88 check and the CALL 0x00405E60 — the receiver is `this` (the
 * inherited base Entity through Panel : Entity), not game_object_sub.
 * The six pushed args (rect{4}, extra, 0) are exactly
 * Entity::Draw(RECT, int, uint32_t) as core/Entity.h already declares
 * it; no signature mismatch.
 *
 * This must call Entity::Draw() explicitly, NOT through virtual
 * dispatch: Panel::Draw (0x454900, game/Panel.h/.cpp — Ghidra's own
 * label for this address, "DispatchEvent", was a naming artifact, fixed
 * 2026-08-14) is a real override of this exact vtable slot (confirmed
 * via decompile — same (RECT, int, uint32_t) signature) whose body
 * calls Entity::Draw() and THEN dims overlapping child rects. The
 * original's raw, non-virtual `CALL 0x00405E60` deliberately bypasses
 * that extra dimming for this specific redraw; a plain `this->Draw(...)`
 * virtual call would incorrectly reintroduce it.
 */
void GameView::render_selection(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                                 int32_t extra)
{
    if (this->update_child_flags) {                              /* +0x88 */
        RECT rect;
        rect.left   = x1;
        rect.top    = y1;
        rect.right  = x2;
        rect.bottom = y2;
        this->Entity::Draw(rect, extra, 0);
    }
}

/**
 * GameView::update_cursor_child — per-child cursor-sprite animation tick.
 * Address: 0x42D770 (vtable[20], +0x50).
 *
 * Dispatched only from track_building's own child loop. Recognizes the
 * three handle_tile_click cursor resource IDs: 0x3806 (hover/zoom
 * control — on frame-0 zoom-2, fires the world save), 0x3807 (valid
 * cursor — selects/deselects and adjusts zoom based on DDraw building
 * mode), 0x3808 (invalid cursor — on frame-0 zoom-2, deselects).
 */
uint8_t GameView::update_cursor_child(TrackPiece* child)
{
    if (child == nullptr) {
        return 0;
    }

    if (child->prev_frame >= 0) {
        child->prev_frame--;
    }

    int res_id = child->resource->resource_id;             /* +0x44 -> +4 */

    if (res_id == 0x3806) {
        if (child->prev_frame == 0 && child->zoom_level == 2) {
            child->SetZoom(1);

            /* BLOCKED — two prerequisite model fixes in OTHER subsystems,
             * NOT an unresolved mystery about this binary. Re-derived and
             * fully resolved this session (previous pass's "dual
             * Building-or-VehicleEditor identity, possibly dead/OOB" framing
             * was INCOMPLETE and its "+0x44C is VehicleEditor::
             * target_building, a building" citation was itself wrong — see
             * below):
             *
             * 1. Re-disassembled 0x42D822..0x42D897 (the real res_id==0x3806
             *    block; Ghidra's JZ table dispatches 0x3806->0x42D822,
             *    0x3807->0x42D7DB, 0x3808 falls through at 0x42D7AC — the
             *    addresses in the old comment, 0x42D839/0x42D83F, were the
             *    right instructions, just under a wrong theory of what they
             *    mean). Confirmed:
             *      0x42D839  MOV EDX, [EDI + 0xE0]   ; EDX = this->selected_building
             *      0x42D83F  MOV ESI, [EDX + 0x44C]  ; ESI = *(EDX + 0x44C)
             *    EDI is `this` (GameView) throughout this function (set at
             *    0x42D776, `MOV EDI, ECX`) — confirmed not some other
             *    register/object.
             *
             * 2. this->selected_building (+0xE0) is NOT exclusively
             *    `Building*`. select_building (0x42D0B7) is the only
             *    instruction that ever writes this slot, but its own
             *    callers pass at least three distinct runtime types
             *    (Building* / a Train-collection object / VehicleEditor*)
             *    — see the full evidence trail on the field's own
             *    declaration in GameView.h. In THIS branch specifically,
             *    the value is a VehicleEditor* (0x450 bytes, operator_new
             *    at 0x44BF05 in Vehicle::Vehicle/0x44BE50): +0x44C is
             *    genuinely in-bounds there (unlike the old comment's
             *    "0xF4-byte Building, OOB" theory), and holds a `Vehicle*`
             *    backref to the owning vehicle — written by
             *    `Vehicle::Vehicle` at 0x44BF6D (`MOV [ECX+0x44C], ESI`,
             *    ECX = editors[0], ESI = the Vehicle being constructed)
             *    and by `Vehicle::InitRoute` at 0x44C2C1. (The field is
             *    currently misdeclared `void* target_building` in
             *    core/VehicleEditor.h as if it held a Building — that
             *    label is wrong; see report/PROGRESS.md, not fixed here.)
             *    This is confirmed independently by every consumer below:
             *    ESI->+0x78/+0x7A match Vehicle::slot_index/network_id
             *    (game/Vehicle.h), and the calls are World_GetObjectAt/
             *    World_RenderAll/World::SaveToFile — all Vehicle*-typed
             *    (game/World.h). There is no out-of-bounds read and no
             *    original-binary defect: this branch is real, reachable
             *    "vehicle route/track-editor cursor completed, tear down
             *    the editor and remove the vehicle" logic.
             *
             * The real sequence still to implement (captures below happen
             * BEFORE the World calls because SaveToFile deletes the
             * vehicle — order matters):
             *   this->select_building(nullptr);                   0x42D84D
             *   g_ddraw_building->SelectBuilding(nullptr);         0x42D859
             *   uint8_t slot_index  = vehicle->slot_index;  /+0x78/ 0x42D85E
             *   uint16_t network_id = vehicle->network_id;  /+0x7A/ 0x42D861
             *   World_GetObjectAt(vehicle);                        0x42D86F
             *   World_RenderAll(vehicle);                          0x42D87A
             *   g_world->SaveToFile(network_id, <player_id>, 1);   0x42D892
             * `<player_id>` at 0x42D87F (`MOV ECX,[ESP+0xC]`) is a real
             * third argument (SaveToFile's char player_id, game/World.h)
             * — NOT yet re-derived against the intervening push sequence
             * in THIS pass; do that before implementing the call.
             *
             * Cannot legally implement any of this without reinterpret_cast
             * until both of the following land (tracked in PROGRESS.md,
             * out of scope for this class):
             *   (a) GameView::selected_building retyped Building* -> Entity*
             *       (forces a matching retype of
             *       DDRAW_Building::SelectBuilding(Building*) in
             *       graphics/DDRAW_Building.h+DDRAW.cpp, since
             *       select_building unconditionally forwards this same
             *       value there at 0x42D12C) — only then is
             *       `static_cast<VehicleEditor*>(this->selected_building)`
             *       a legal same-base downcast instead of a cross-cast.
             *   (b) VehicleEditor::target_building retyped void* ->
             *       Vehicle* (core/VehicleEditor.h) — a separate,
             *       multi-file landmine sweep: game/Vehicle.cpp has 7
             *       write sites already relying on the "Vehicle* backref"
             *       meaning (e.g. its own comment on the assignment
             *       `editor->target_building = this;`, annotated "backref"),
             *       while core/VehicleEditor.cpp's
             *       destructor (~VehicleEditor, body 0x40D680) already
             *       casts the SAME field to `Building*` and reads
             *       `occupation_level` — confirmed wrong against
             *       0x40D6E4 (`CMP byte[EAX+0x88],0`, EAX = target_building
             *       = a Vehicle*, so +0x88 is Vehicle::init_flag/
             *       process_delay per game/Vehicle.h, not
             *       Building::occupation_level). Not fixed here; reported
             *       separately.
             *
             * Still true, but now secondary: handle_tile_click's sole
             * caller GameLoop_PostSetupBootstrap (0x45DF32) is unimplemented
             * in this tree today, so no TrackPiece with resource_id 0x3806
             * can exist on this host build regardless of the above.
             */
#ifndef _WIN32
            fprintf(stderr,
                    "STUB: GameView::update_cursor_child (0x42D770) res_id "
                    "0x3806 zoom-complete branch reached at %s:%d — blocked on "
                    "two prerequisite model fixes in other subsystems "
                    "(GameView::selected_building retyped Entity*, "
                    "VehicleEditor::target_building retyped Vehicle*; see "
                    "comment above for full evidence and the exact call "
                    "sequence to implement). Not an original-binary bug and "
                    "not unreachable-by-design: this path is only unreachable "
                    "on THIS host build today because handle_tile_click's sole "
                    "caller (GameLoop_PostSetupBootstrap) is unimplemented.\n",
                    __FILE__, __LINE__);
            assert(false &&
                   "GameView::update_cursor_child (0x42D770) res_id 0x3806 "
                   "branch: blocked on selected_building/Entity* and "
                   "VehicleEditor::target_building/Vehicle* retypes in other "
                   "subsystems; see core/GameView.cpp's comment");
#endif
        }
    } else if (res_id == 0x3807) {
        if (child->prev_frame >= 0) {
            g_ddraw_building->SelectBuilding(this->selected_building);
            child->prev_frame = 0xFFFF;
        }

        if (g_ddraw_active) {
            child->SetZoom(2);
        } else {
            child->SetZoom(1);
        }
        return 1;
    } else if (res_id == 0x3808) {
        if (child->prev_frame == 0 && child->zoom_level == 2) {
            child->SetZoom(1);
            this->select_building(nullptr);
            return 1;
        }
    }

    return 1;
}

/**
 * GameView::handle_tile_click — create placement cursor indicator sprites.
 * Address: 0x42CE10 (MISNAMED: not a click handler).
 *
 * Moved here from town/Town.cpp: its sole caller,
 * GameLoop_PostSetupBootstrap (0x45DF32), loads ECX with the bare
 * immediate 0x4852A0 before `CALL 0x0042CE10` — the same receiver
 * evidence as every other method in this class, and (per a tree-wide
 * grep) GameLoop_PostSetupBootstrap is itself not implemented anywhere in
 * this codebase today, so this method has zero real callers on the host
 * regardless of this move.
 *
 * See this class's header doc for the full field/dispatch breakdown.
 */
char GameView::handle_tile_click()
{
    int32_t res = g_resmgr.GetById(0x3807);
    if (res != 0 && UI_IsBitmapReady(res)) {
        TrackPiece* sprite = static_cast<TrackPiece*>(this->CreateChildSprite(res, 0, 0));
        this->cursor_valid_sprite = sprite;   /* +0x170 */
        this->enter_zoom_child = sprite;      /* +0xD8, inherited Panel field —
                                                * consumed by Panel::HandleKey's
                                                * Enter (0x0D) shortcut */
    }

    res = g_resmgr.GetById(0x3808);
    if (res != 0 && UI_IsBitmapReady(res)) {
        TrackPiece* sprite = static_cast<TrackPiece*>(this->CreateChildSprite(res, 0, 0));
        this->cursor_invalid_sprite = sprite; /* +0x174 */
        this->escape_zoom_child = sprite;     /* +0xDC, inherited Panel field —
                                                * consumed by Panel::HandleKey's
                                                * Escape (0x1B) shortcut */
    }

    res = g_resmgr.GetById(0x3806);
    if (res != 0 && UI_IsBitmapReady(res)) {
        this->track_piece =                   /* +0x178 */
            static_cast<TrackPiece*>(this->CreateChildSprite(res, 0, 0));
    }

    /* Load animation resources: 0x3805 on `this` (Panel::Init, vtable[6]),
     * 0x3804 on the embedded game_object_sub (Entity::InitBase, its own
     * vtable[6]). The original disassembly confirms the second call is a
     * genuine virtual dispatch on the embedded sub-object, not a pointer-
     * field dereference: `LEA ECX,[this+0xE4]` (the sub-object's own
     * address, used as the receiver) paired with `MOV EDX,[this+0xE4]`
     * (its vtable pointer, one dereference) — the same embedded-object
     * idiom already established by center_on_point/cleanup for
     * game_object_sub elsewhere in this class. */
    bool loaded = this->Init(0x3805, -1, false) != 0;
    if (loaded) {
        loaded = this->game_object_sub.InitBase(0x3804, -1, false) != 0;
    }
    if (!loaded) {
        return 0;
    }

    /* Placement-construct the selection-overlay UIPANEL_Surface. Real
     * sizeof(UIPANEL_Surface) via UIPANEL_Surface_Size() (the original
     * x86 size, 0x20, is irrelevant on this host's layout). Cast the raw
     * allocation to each needed pointer type from `void*` directly
     * (rather than between UIPANEL_Surface* and ResourceObject*
     * themselves) so this stays two ordinary void*-sourced static_casts,
     * matching the ABI-boundary mirror technique this file's `view()`/
     * `PanelGraphicsView` helpers already use for the same object. */
    void* raw_surface = operator_new(UIPANEL_Surface_Size());
    if (raw_surface != nullptr) {
        UIPANEL_CreateSurface(static_cast<UIPANEL_Surface*>(raw_surface));
    }
    this->overlay_panel = static_cast<ResourceObject*>(raw_surface);  /* +0x17C */

    if (this->overlay_panel != nullptr) {
        UIPANEL_Surface* pgfx_surface =
            panel_graphics_view(this->game_object_sub.resource)->surface;
        UIPANEL_InitSurface(this->overlay_panel,
                             pgfx_surface->width,
                             pgfx_surface->height,
                             1, 0, 0);

        /* Backup rect — see GameView.h's field doc for why these four
         * fields (despite their "surface"-shaped names, inherited as-is
         * from the earlier Town.h attribution) are really a RECT
         * {left, top, right, bottom} shared with deselect_building's own
         * UIPANEL_Blit calls. Written field-by-field (matching this
         * class's own established idiom elsewhere) rather than through a
         * RECT* reinterpret_cast over the four members. */
        this->backup_surface = 0;                                /* +0x180 left   */
        this->backup_x       = 0;                                /* +0x184 top    */
        this->backup_y       = view(this->overlay_panel)->width;  /* +0x188 right  */
        this->backup_width   = view(this->overlay_panel)->height; /* +0x18C bottom */
    }

    return 1;
}

/**
 * GameView::HitTestChild — per-child hit-test/zoom callback.
 * Address: 0x42D6B0 (vtable[17], +0x44). Overrides Panel::HitTestChild.
 *
 * Moved here from town/Town.cpp ("postcard_command_handler" — a doubly
 * wrong name: no postcards, no WM_COMMAND). This function's sole xref in
 * the entire binary is the DATA reference at this class's own vtable
 * slot [17] (0x477D74) — it has zero direct call sites of its own,
 * proving it is dispatched purely through Panel::HitTestChildren's
 * (game/Panel.cpp) per-child loop, which calls `this->HitTestChild(child,
 * x, y)` for every child in the +0xD0 list. `x`/`y` (formerly transcribed
 * as `wParam`/`lParam`, a WM_COMMAND-shaped misnomer) are screen
 * coordinates, matching `control->PtInRect(x, y)` below.
 */
uint8_t GameView::HitTestChild(TrackPiece* control, int x, int y)
{
    if (control == nullptr || control->render_enabled == 0) {   /* +0x56 */
        return 0;
    }

    if (!control->PtInRect(x, y)) {
        return 0;
    }

    int res_id = control->resource->resource_id;                /* +0x44 -> +4 */

    if (res_id == 0x3806) {
        if (control->zoom_level == 1) {                          /* +0x48 */
            control->SetZoom(2);
            control->prev_frame = 6;                             /* +0x54 */
        }
    } else if (res_id == 0x3807) {
        if (control->zoom_level == 1) {
            g_ddraw_building->SelectBuilding(this->selected_building);
        } else {
            g_ddraw_building->SelectBuilding(nullptr);
        }
        return 1;
    } else if (res_id == 0x3808) {
        if (control->zoom_level == 1) {
            control->SetZoom(2);
            control->prev_frame = 6;
        }
    }

    return 1;
}
