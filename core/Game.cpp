// Status: INTEGRATED
/**
 * Game.cpp — Game class implementation: main loop, cursor engine
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation (database locon).
 *
 * Global singleton: g_game at 0x4854C8.
 * Frame heartbeat: GameLoop_FrameUpdate (0x45C3C0, core/GameLoop.cpp)
 *                  calls Game::Update (0x410840) every frame.
 *
 * Every method below carries its original address; behavior was checked
 * against the decompiled and disassembled assembly for each basic block.
 */

#include "Game.h"

#include "../game/Building.h"
#include "../game/BuildingMgr.h"
#include "../game/ScriptedObject.h"
#include "../game/World.h"
#include "../world/tilemap.h"
#include "../resources/ResourceManager.h"
#include "../audio/GameAudio.h"
#include "../town/Town.h"
#include "../world/scriptengine.h"
#include "CGWND.h"

#include <cstring>   /* memmove — the original uses CRT_memmove (0x466EA0) */

/* ================================================================== */
/* Win32 API surface                                                   */
/*                                                                     */
/* The original binary calls user32.dll via its import table (IAT).    */
/* On Windows the real <windows.h> declarations are used; the verified  */
/* IAT slot addresses are documented below for reference.  On the host  */
/* build the stubs/windows.h declarations plus the shared/link_stubs    */
/* and sdl3_shims definitions provide the same C-linkage symbols.       */
/* ================================================================== */

#ifdef _WIN32
#include <windows.h>

/* IAT entries referenced by the Game methods (verified in disassembly):
 *   SetCursorPos    @ 0x00477250   (call [0x477250] in 0x410C21)
 *   ClientToScreen  @ 0x00477378   (call [0x477378] in 0x410A67)
 *   WindowFromPoint @ 0x004772E4   (call [0x4772E4] in 0x410A72)
 *   ReleaseCapture  @ 0x00477370   (call [0x477370] in 0x411F43)
 *   LoadCursorA     @ 0x004772CC   (call [0x4772CC] in 0x411F4C)
 *   SetCursor       @ 0x004772FC   (call [0x4772FC] in 0x411F58)
 */
#else

extern "C" {
/* Win32 APIs used by the Game methods.  On the host build these resolve
 * to the C-linkage definitions in shared/link_stubs.cpp and
 * sdl3_shims/sdl3_window.cpp (stubs/windows.h is not included here
 * because its extern "C" declarations conflict with the C++-linkage
 * declarations in world/tilemap.h). */
int32_t SystemParametersInfoA(UINT, UINT, void*, UINT);
BOOL    ClientToScreen(HWND, POINT*);
BOOL    SetCursorPos(int, int);
BOOL    ReleaseCapture(void);
BOOL    GetCursorPos(POINT*);
BOOL    ScreenToClient(HWND, POINT*);
HWND    SetCapture(HWND);
int     ShowCursor(int);
HCURSOR LoadCursorA(HINSTANCE, const char*);
void    SetCursor(HCURSOR);
HCURSOR LoadCursorFromFileA(const char*);
HWND    WindowFromPoint(POINT);
int     wsprintfA(char*, const char*, ...);
}
#endif

/* ================================================================== */
/* External declarations — globals (Ghidra addresses in comments)      */
/* ================================================================== */

extern void* operator_new(size_t size);              /* 0x00465CE0 */
extern void  GLOBAL_free(void* ptr);                 /* 0x00465CD0 */

/* Game mode / world state — g_game_mode, g_build_mode, g_is_town_mode,
 * g_placement_valid, g_world_width, g_player_id, g_viewport_x,
 * g_client_offset_x/y, g_town_view and g_allow_building_placement are
 * declared in world/tilemap.h. */
extern int32_t  g_demo_mode;           /* 0x004A9918 */
extern uint8_t  g_ddraw_active;        /* 0x004A9F78 */
extern uint8_t  g_town_click_valid;    /* 0x004AA664 */
extern int32_t  g_world_height;        /* 0x004AAD10 */
extern int32_t  g_viewport_y;          /* 0x004AAD28 viewport scroll Y */
extern char     g_install_path[];      /* 0x004A99C8 */
extern int32_t  g_object_count;        /* 0x004A99A0 object array count */
extern void**   g_object_array;        /* 0x004A9998 object array */

/* Subsystem singletons (g_player_color is declared in world/tilemap.h) */
/* g_scripted_object is canonically declared in world/scriptengine.h */
extern BuildingMgr*    g_building_mgr;     /* 0x00485448 — host-constructed singleton */
extern World*          g_world;            /* 0x004A98B0 — host-constructed singleton */

/* Overlay subsystems (each is an object embedded at its global) */
extern void*    g_town;                /* Town window object */
extern void*    g_cursor;              /* Cursor window object */
extern void*    g_postcard;            /* PostcardAlbum window object */
extern void*    g_postcard_send;       /* postcard-send window object */
extern void*    g_ui_main;             /* UI main window object */
extern void*    g_tooltip_mgr;         /* 0x004FD220 tooltip manager */
extern GameAudio* g_audio;             /* 0x004FD38C audio system */

/* Town-view overlays (hit-test objects embedded at their globals) */
extern uint8_t  g_town_overlay_bounds[];    /* 0x004AA730 */
extern uint8_t  g_second_overlay_bounds[];  /* 0x004AA818 */
extern int32_t  g_town_overlay_threshold;   /* 0x004AA744 */
extern uint8_t  g_has_town_overlay;         /* 0x004AA7B8 */
extern uint8_t  g_has_second_overlay;       /* 0x004AA8A0 */

/* DDRAW drag-rect hit-test object (embedded at its global) */
extern uint8_t  g_ddraw_drag_rect[];        /* 0x004A9FD0 */

/* Byte flags read by the cursor engine (names unconfirmed) */
extern uint8_t  g_mouse_capture;      /* 0x004855AE — byte flag cleared by
                                         SetScreenMode on capture release
                                         (previously an unnamed Ghidra global) */
extern uint8_t  g_flag_4A9F80;        /* 0x004A9F80 — read by UpdateInputState
                                         (0x411B08); semantic name unconfirmed */

/* Sorted placement lists — 18-byte BuildingCollection objects embedded
 * at their globals (game/BuildingMgr.h layout). */
extern BuildingCollection g_building_list;   /* 0x00485494 */
extern BuildingCollection g_vehicle_list;    /* 0x004854AC */

/* Free helpers used by the Game methods */
void  PlaySoundAt(int sound_id, int x, int y, int flags);   /* 0x004479D0 */
void  UI_CreateMessageBox(void* mgr, int32_t res_id, int32_t p2, char p3,
                          int32_t x, int32_t y, int32_t p7); /* 0x00423AB0 */
void  RESMGR_LoadSoundResource(void* res);                   /* 0x00448D60 */
void  RESMGR_ReleaseSoundResource(void* res);                /* 0x00448EE0 */

/* ================================================================== */
/* RESDATA field accessors                                             */
/*                                                                     */
/* shared/types.h names +0x04 resource_id, +0x14 frame_width, +0x1A    */
/* anim_count, +0x1E default_anim, +0x32 offset_x, +0x34 offset_y.     */
/* The type byte at +0x08, the animation index at +0x1C, the sound     */
/* override at +0x52E, and the play-area bounds at +0x169/+0x16D are    */
/* read as raw offsets here (same pattern as Building.cpp +0x08).      */
/* TODO: name these fields in shared/types.h.                          */
/* ================================================================== */

namespace {

inline const RESDATA* to_resdata(const void* res)
{
    return static_cast<const RESDATA*>(res);
}

inline uint8_t resource_type(const void* res)
{
    return *reinterpret_cast<const uint8_t*>(
        reinterpret_cast<const uint8_t*>(res) + 0x08);
}

inline int32_t resource_id(const void* res)
{
    return to_resdata(res)->resource_id;            /* +0x04 */
}

inline int16_t resource_anim_index(const void* res)
{
    return *reinterpret_cast<const int16_t*>(
        reinterpret_cast<const uint8_t*>(res) + 0x1C);
}

inline int16_t resource_sound_override(const void* res)
{
    return *reinterpret_cast<const int16_t*>(
        reinterpret_cast<const uint8_t*>(res) + 0x52E);
}

inline uint8_t resource_play_rows(const void* res)
{
    return *reinterpret_cast<const uint8_t*>(
        reinterpret_cast<const uint8_t*>(res) + 0x169);
}

inline uint8_t resource_low_y(const void* res)
{
    return *reinterpret_cast<const uint8_t*>(
        reinterpret_cast<const uint8_t*>(res) + 0x16D);
}

/* Main window handle — the binary reads the HWND at CGWND+0x08. */
inline HWND main_window_handle()
{
    return reinterpret_cast<CGWND*>(g_main_window)->hWnd;
}

/* HWND of an overlay subsystem object (stored at object +0x08). */
inline HWND window_handle_of(const void* obj)
{
    return *reinterpret_cast<const HWND*>(
        reinterpret_cast<const uint8_t*>(obj) + 0x08);
}

/* ================================================================== */
/* Inline TimerSlotList dispatch view                                  */
/*                                                                     */
/* Game embeds a TimerSlotList at +0x10C (16 bytes).  The shared       */
/* reconstruction (shared/TimerSlotList.h) models only the destructor  */
/* slot; Game dispatches the running-vtable (0x477758) slots below.    */
/* Slot numbers and target addresses are verified against the binary   */
/* vtable dump.  TODO: fold into TimerSlotList once the shared owner   */
/* canonicalizes its slot set.                                         */
/* ================================================================== */
struct TimerSlotsView {
    virtual void    Resize(int32_t capacity) = 0;   /* [0]  0x435D10 */
    virtual void    SlotDtor() = 0;                 /* [1]  0x4125C0 */
    virtual void    Slot2() = 0;                    /* [2]  0x424020 */
    virtual void    Slot3() = 0;                    /* [3]  0x4241E0 */
    virtual void    Slot4() = 0;                    /* [4]  0x4356E0 */
    virtual void    Stop() = 0;                     /* [5]  0x424250 */
    virtual void    Slot6() = 0;                    /* [6]  0x424270 */
    virtual void    Slot7() = 0;                    /* [7]  0x424530 */
    virtual void*   GetItem(int32_t index) = 0;     /* [8]  0x424030 */
    virtual void    Slot9() = 0;                    /* [9]  0x412140 */
    virtual void    Slot10() = 0;                   /* [10] 0x4124B0 */
    virtual int32_t GetCount() = 0;                 /* [11] 0x424000 */
    virtual void    Slot12() = 0;                   /* [12] 0x424760 */
    virtual void    Insert(void* item) = 0;         /* [13] 0x412440 */
    virtual void*   FindIndex(int32_t index) = 0;   /* [14] 0x412540 */

protected:
    ~TimerSlotsView() = default;   /* non-virtual: never destroyed through this view */
};

inline TimerSlotsView* game_timer_slots(Game* self)
{
    return reinterpret_cast<TimerSlotsView*>(&self->timer_sub_ptr);
}

/* ================================================================== */
/* Sorted building/vehicle list dispatch view                          */
/*                                                                     */
/* g_building_list (0x485494) / g_vehicle_list (0x4854AC) are the      */
/* global sorted collections of placed buildings/vehicles, embedded at */
/* their globals (18-byte BuildingCollection layout per                */
/* game/BuildingMgr.h: items@+4, capacity@+8, count@+0xC,              */
/* key_offset@+0x10, key_size@+0x14).  Game dispatches the five        */
/* running slots below; the slot semantics are verified against the    */
/* TimerList-family vtables (0x477AE8..0x477BD0): [7] InternalExtract,  */
/* [12] IsSlotFilled, [16] FindItem, [17] InsertAt, [18] Comparator.   */
/* The exact vtable variant of these two globals is not yet identified  */
/* (no initializer was located); TODO: fold into the shared            */
/* collections reconstruction (shared/collections.h).                  */
/* ================================================================== */
struct SortedListSlotsView {
    virtual void     Slot0() = 0;                                   /* [0] Resize */
    virtual void     Slot1() = 0;
    virtual void     Slot2() = 0;
    virtual void     Slot3() = 0;
    virtual void     Slot4() = 0;
    virtual void     Slot5() = 0;
    virtual void     Slot6() = 0;
    virtual void*    InternalExtract(int32_t index) = 0;            /* [7]  0x424530 */
    virtual void     Slot8() = 0;                                   /* [8]  GetItem */
    virtual void     Slot9() = 0;
    virtual void     Slot10() = 0;
    virtual void     Slot11() = 0;                                  /* [11] GetCount */
    virtual int32_t  IsSlotFilled(int32_t index) = 0;               /* [12] 0x424760 */
    virtual void     Slot13() = 0;
    virtual void     Slot14() = 0;
    virtual void     Slot15() = 0;
    virtual uint32_t FindItem(void* target, uint32_t low, uint32_t high) = 0; /* [16] 0x424820 */
    virtual void     InsertAt(int32_t index, void* item) = 0;       /* [17] 0x4248C0 */
    virtual int32_t  Comparator(void* a, void* b) = 0;              /* [18] 0x424960 */

protected:
    ~SortedListSlotsView() = default;  /* non-virtual: never destroyed through this view */
};

inline SortedListSlotsView* list_ops(BuildingCollection& list)
{
    return reinterpret_cast<SortedListSlotsView*>(&list);
}

/* ================================================================== */
/* DDRAW_Building hit-test view                                         */
/*                                                                     */
/* Game calls the two direct hit-test methods (0x459D60 / 0x45A740) on  */
/* the g_ddraw_building singleton (0x4A9EF0, declared void* in         */
/* world/tilemap.h).  graphics/DDRAW.h cannot be included here because  */
/* it redeclares g_tilemap / g_ddraw_building / g_primary_surface with */
/* different types than world/tilemap.h; this minimal view declares    */
/* only the two methods used.  TODO: reconcile the two headers' global  */
/* declarations so graphics/DDRAW.h can be used directly.              */
/* ================================================================== */
struct DDRAW_BuildingView {
    int32_t HitTest(int32_t x, int32_t y);        /* 0x00459D60 */
    uint8_t HitTestWithDrag(int32_t x, int32_t y);/* 0x0045A740 */
};

inline DDRAW_BuildingView* ddraw_building()
{
    return static_cast<DDRAW_BuildingView*>(g_ddraw_building);
}

/* The town view is an object embedded at the g_town_view global
 * (binary 0x4852A0); the binary passes its address (ECX = 0x4852A0)
 * to GameObject::PtInRect (0x436A10, vtable slot [2]) and to the
 * postcard click handler (0x42D670). */
inline GameObject* town_view_object()
{
    return reinterpret_cast<GameObject*>(&g_town_view);
}

/* Overlay-bounds and drag-rect objects are GameObject-family objects
 * embedded at their globals; the binary calls 0x436A10 (slot [2])
 * directly on them. */
inline bool bounds_hit(uint8_t* obj, int x, int y)
{
    return reinterpret_cast<GameObject*>(obj)->PtInRect(x, y) != 0;
}

} // namespace

/* ================================================================== */
/* Game constructor                                                     */
/* Address: 0x410510                                                    */
/* ================================================================== */
Game::Game()
{
    /* Entity(-1, -1, 0, 0) runs first as the compiler-managed base
     * construction (original: GameObject_BaseCtor at 0x405790). */

    /* Inline timer slot list at +0x10C.  The original transitions the
     * sub-object vtable from 0x477798 (init) to 0x477758 (running) —
     * a compiler-managed artifact in the C++ model (the shared
     * TimerSlotList reconstruction owns that lifecycle).  The item
     * array allocation and count below reproduce the recovered data
     * fields. */
    this->timer_array_ptr = 0;          /* +0x110 */
    this->timer_count = 0;              /* +0x114 */

    int32_t* timers = static_cast<int32_t*>(operator_new(0x28));
    this->timer_array_ptr =
        static_cast<int32_t>(reinterpret_cast<intptr_t>(timers));

    if (timers != nullptr) {
        for (int i = 10; i != 0; i--) {
            *timers = 0;
            timers++;
        }
        this->timer_count = 10;
    } else {
        this->timer_array_ptr = 0;
        this->timer_count = 0;
    }
    this->timer_edit = 0;               /* +0x118 */

    /* vtable = 0x477718 — compiler-managed. */
    this->visible = 1;                  /* +0x24 */
    this->SetScreenMode(0, 1, 0);

    this->cursor_sound_id = -1;         /* +0x88 */
    this->selected_object = nullptr;    /* +0xE8 */
    this->selected_visible = 0;         /* +0xEC */

    this->mouse_spi3[0] = 0;            /* +0xF0 */
    this->mouse_spi3[1] = 0;
    this->mouse_spi3[2] = 0;
    this->mouse_spi4[0] = 0;            /* +0xFC */
    this->mouse_spi4[1] = 0;
    this->mouse_spi4[2] = 0;
    SystemParametersInfoA(3, 0, this->mouse_spi3, 0);   /* SPI_GETMOUSE (3) */
    SystemParametersInfoA(4, 0, this->mouse_spi4, 0);   /* SPI_GETMOUSE (4) */

    this->left_click_flag = 0;          /* +0xA4 */
    this->right_click_flag = 0;         /* +0xB4 */
    this->mouse_move_flag = 0;          /* +0xC4 */
    this->mouse_drag_flag = 0;          /* +0xD4 */
    this->mouse_drag_mode = 0;          /* +0xE4 */
    this->mouse_drag_handled = 0;       /* +0xE5 */
    this->click_on_selected = 0;        /* +0xE6 */
    this->_pad_E7 = 0;
    this->busy_cursor_handle = 0;       /* +0x108 */
    this->cursor_disabled = 0;          /* +0x8D */

    this->Update();                     /* one initial tick */
}

/* ================================================================== */
/* Game destructor body                                                 */
/* Address: 0x410680 (body) / 0x410660 (scalar deleting wrapper)        */
/* ================================================================== */
Game::~Game()
{
    /* The original resets the inline timer sub-object vtable to its
     * dead marker vtable (0x477798), zeroes the
     * count, frees the item array, and runs the Entity base destructor
     * (GameObject_DtorBody).  The vtable reset is compiler-managed in
     * the C++ model; the Entity base destructor runs automatically
     * after this body. */
    this->timer_edit = 0;               /* +0x118 */
    this->timer_count = 0;              /* +0x114 */
    if (this->timer_array_ptr != 0) {
        GLOBAL_free(reinterpret_cast<void*>(
            static_cast<intptr_t>(this->timer_array_ptr)));
    }
    this->timer_array_ptr = 0;
}

/* ================================================================== */
/* Game::Update — MAIN per-frame game loop                              */
/* Address: 0x410840 — vtable [10]                                      */
/* ================================================================== */
void Game::Update()
{
    /* Skip unless the parent-active flag (+0x24) is set. */
    if (!this->visible) {
        return;
    }

    /* Step 1: animation state machine (0x405C40). */
    Entity::Update();

    /* Step 2: poll input flags. */
    bool has_event = (this->left_click_flag != 0) ||
                     (this->mouse_move_flag != 0) ||
                     (this->right_click_flag != 0) ||
                     (this->screensaver_active != 0);

    if (this->resource != nullptr) {    /* +0x40 parent resource */
        /* Step 3: screensaver timeout check. */
        if (this->screensaver_active != 0) {
            if (this->IsScreensaverActive() != 0) {
                this->ClearMouseMode();
                this->screensaver_active = 0;
            }
        }

        /* Step 4: click dispatch. */
        if (this->left_click_flag != 0) {
            this->HandleLeftClick();
        }
        if (this->right_click_flag != 0) {
            this->HandleRightClick();
        }

        /* Re-click on the selected object deselects it. */
        Building* sel = this->selected_object;
        if (sel != nullptr && this->selected_visible != 0 &&
            this->click_on_selected == 0) {
            if (sel->CheckPlacementCollision(sel->world_x,
                                             sel->world_y) != 0) {
                this->DeselectGameObject();
                if (this->mouse_drag_mode != 0) {
                    sel->next_action_time = 0;      /* +0xA0 */
                    this->selected_object = nullptr;
                }
            }
        }

        /* Mouse move conversion. */
        if (this->mouse_move_flag != 0) {
            int32_t wx, wy;
            this->ScreenToWorld(&wx, (int)(this->mouse_move_screen_pos & 0xFFFF),
                                (int)(this->mouse_move_screen_pos >> 16));
            this->mouse_move_world_x = wx;
            this->mouse_move_world_y = wy;
            this->mouse_move_flag = 0;
            TileMap_ClearInputProcessedFlag(g_tilemap);
            this->mouse_drag_mode = 0;
        }

        /* Mouse drag conversion. */
        if (this->mouse_drag_flag != 0) {
            int32_t wx, wy;
            this->ScreenToWorld(&wx, (int)(this->mouse_drag_screen_pos & 0xFFFF),
                                (int)(this->mouse_drag_screen_pos >> 16));
            this->mouse_drag_world_x = wx;
            this->mouse_drag_world_y = wy;
            this->right_click_flag = 0;
            this->mouse_drag_flag = 0;
            this->mouse_drag_handled = 0;
            TileMap_ClearInputProcessedFlag(g_tilemap);
        }
    }

    /* Step 5: mode-specific cursor feedback, plus fallback sound. */
    if (has_event) {
        this->UpdateCursorMode();
        if (this->initialized != 1) {
            this->PlaySound(0x1400);
        }
    }
}

/* ================================================================== */
/* Game::UpdateCursorMode                                               */
/* Address: 0x411760                                                    */
/* ================================================================== */
void Game::UpdateCursorMode()
{
    if (g_game_mode != 1) {
        if (g_game_mode == 3) {
            this->UpdateInputState();
        } else if (g_game_mode == 4) {
            this->HandleCursorHover();
            this->ClearMouseMode();
        } else {
            this->PlaySound(0x1400);
        }
    }
    if (this->initialized != 1) {
        this->PlaySound(0x1400);
    }
}

/* ================================================================== */
/* Game::HandleCursorHover — build-mode cursor sound engine             */
/* Address: 0x4117B0                                                    */
/* ================================================================== */
void Game::HandleCursorHover()
{
    if (g_placement_valid == 1) {
        this->PlaySound(0x1404);
        this->StopSound(2);
        return;
    }

    if (g_scripted_object->GetDragOffset(this->mouse_world_x,
                                        this->mouse_world_y) != 0) {
        this->PlaySound(0x1404);
        this->StopSound(0);
        g_town_click_valid = 0;
        return;
    }

    if (g_scripted_object->IsDragging(this->mouse_world_x,
                                     this->mouse_world_y) == 0) {
        g_town_click_valid = 0;
    } else if (g_town_click_valid == 0) {
        this->PlaySound(0x1400);
        return;
    }

    if (g_has_town_overlay != 0) {
        if (bounds_hit(g_town_overlay_bounds, this->mouse_world_x,
                       this->mouse_world_y)) {
            if (this->mouse_world_y <= g_town_overlay_threshold - 0x34) {
                if (g_placement_valid == 0 && 0 < this->cursor_sound_id) {
                    this->PlaySound(this->cursor_sound_id);
                    return;
                }
                this->PlaySound(0x1404);
                return;
            }
            goto play_generic;
        }
    }

    if (g_has_second_overlay != 0) {
        if (bounds_hit(g_second_overlay_bounds, this->mouse_world_x,
                       this->mouse_world_y)) {
            goto play_generic;
        }
    }

    if (g_build_mode == 1) {
        this->cursor_sound_id = -1;
        this->PlaySound(0x1402);
        return;
    }

    {
        int32_t sound = this->cursor_sound_id;
        if (sound >= 0) {
            const void* parent = this->resource;
            if (parent != nullptr && resource_type(parent) != 0) {
                /* Horizontal/vertical edge-of-world cursor IDs. */
                if (sound == 0xC26 || sound == 0xC28 ||
                    sound == 0xC2A || sound == 0xC2C) {
                    int32_t mx = this->mouse_world_x;
                    if ((mx < 0x10) && (0x10 < this->mouse_world_y) &&
                        (this->mouse_world_y < g_world_height - 0x50)) {
                        this->cursor_sound_id = 0xC2C;   /* left edge */
                    }
                    if ((g_world_width - 0x50 < mx) &&
                        (0x10 < this->mouse_world_y) &&
                        (this->mouse_world_y < g_world_height - 0x50)) {
                        this->cursor_sound_id = 0xC26;   /* right edge */
                    }
                    if ((this->mouse_world_y < 0x11) &&
                        (0x10 < mx) && (mx < g_world_width - 0x50)) {
                        this->cursor_sound_id = 0xC28;   /* top edge */
                    }
                    if ((g_world_height - 0x40 < this->mouse_world_y) &&
                        (0x10 < mx) && (mx < g_world_width - 0x50)) {
                        this->cursor_sound_id = 0xC2A;   /* bottom edge */
                    }
                }
                /* Drag variants of the edge cursors. */
                sound = this->cursor_sound_id;
                if (sound == 0xC46 || sound == 0xC48 ||
                    sound == 0xC42 || sound == 0xC44) {
                    int32_t mx = this->mouse_world_x;
                    if ((mx < 0x10) && (0x10 < this->mouse_world_y) &&
                        (this->mouse_world_y < g_world_height - 0x50)) {
                        this->cursor_sound_id = 0xC44;
                    }
                    if ((g_world_width - 0x50 < mx) &&
                        (0x10 < this->mouse_world_y) &&
                        (this->mouse_world_y < g_world_height - 0x50)) {
                        this->cursor_sound_id = 0xC42;
                    }
                    if ((this->mouse_world_y < 0x11) &&
                        (0x10 < mx) && (mx < g_world_width - 0x50)) {
                        this->cursor_sound_id = 0xC46;
                    }
                    if ((g_world_height - 0x40 < this->mouse_world_y) &&
                        (0x10 < mx) && (mx < g_world_width - 0x50)) {
                        this->cursor_sound_id = 0xC48;
                    }
                }
                this->PlaySound(this->cursor_sound_id);
                return;
            }
        }
    }

play_generic:
    this->PlaySound(0x1400);
}

/* ================================================================== */
/* Game::UpdateInputState — town-mode cursor input handler              */
/* Address: 0x411AE0                                                    */
/* ================================================================== */
void Game::UpdateInputState()
{
    if (((this->selected_object != nullptr && this->selected_visible != 0) ||
         g_placement_valid != 0) ||
        g_flag_4A9F80 != 0) {
        this->PlaySound(0x1404);
        this->StopSound(2);
        return;
    }

    if (g_scripted_object->GetDragOffset(this->mouse_world_x,
                                        this->mouse_world_y) == 0) {
    if (g_ddraw_active != 0) {
            if (bounds_hit(g_ddraw_drag_rect, this->mouse_world_x,
                           this->mouse_world_y)) {
                goto play_action;
            }
        }
        if (g_scripted_object->IsDragging(this->mouse_world_x,
                                         this->mouse_world_y) != 0) {
            this->PlaySound(0x1400);
            return;
        }
        if (g_ddraw_active != 0) {
            if (ddraw_building()->HitTest(this->mouse_world_x,
                                          this->mouse_world_y) != 0) {
                this->PlaySound(0x1400);
                return;
            }
        }
        if (g_is_town_mode != 0) {
            if (town_view_object()->PtInRect(this->mouse_world_x,
                                             this->mouse_world_y) != 0) {
                this->PlaySound(0x1400);
                return;
            }
        }
        if (this->selected_object != nullptr &&
            this->selected_visible == 0) {
            this->PlaySound(0x1405);
            return;
        }
        this->PlaySound(0x1400);
        return;
    }

play_action:
    this->PlaySound(0x1404);
    this->StopSound(0);
}

/* ================================================================== */
/* Game::HandleLeftClick — left-click world dispatch                    */
/* Address: 0x411000                                                    */
/* ================================================================== */
void Game::HandleLeftClick()
{
    int32_t wx, wy;
    this->ScreenToWorld(&wx, (int)(this->left_click_screen_pos & 0xFFFF),
                        (int)(this->left_click_screen_pos >> 16));
    this->left_click_world_x = wx;
    this->left_click_world_y = wy;

    /* 0x1402 resources restart their default animation on click. */
    const void* parent = this->resource;
    int32_t parent_id = (parent != nullptr) ? resource_id(parent) : -1;
    if (parent_id == 0x1402 &&
        this->anim_index != to_resdata(parent)->default_anim) {
        this->StopSound(to_resdata(parent)->default_anim);
    }

    /* Town mode: the postcard overlay claims the click first. */
    if (g_is_town_mode != 0 &&
        town_view_object()->PtInRect(wx, wy) != 0) {
        if (reinterpret_cast<Town*>(&g_town_view)
                ->postcard_click_handler(wx, wy) != 0) {
            PlaySoundAt(0x5015, wx, wy, 4);
            this->left_click_flag = 0;
            return;
        }
        this->left_click_flag = 0;
        return;
    }

    /* Build chain (LAB_004110a0): DDRAW → scripted object → selection
     * placement → BuildingMgr/TileMap. */
    if (g_ddraw_active != 0 && ddraw_building()->HitTest(wx, wy) != 0) {
        if (ddraw_building()->HitTestWithDrag(wx, wy) != 0) {
            PlaySoundAt(0x5015, wx, wy, 4);
            this->left_click_flag = 0;
            return;
        }
        this->left_click_flag = 0;
        return;
    }

    if (g_scripted_object->CheckClick(wx, wy) != 0) {
        if (g_scripted_object->HitTest(wx, wy) != 0) {
            PlaySoundAt(0x5015, wx, wy, 4);
            this->left_click_flag = 0;
            return;
        }
        this->left_click_flag = 0;
        return;
    }

    if (this->selected_object == nullptr ||
        this->selected_visible != 0) {
        /* BuildingMgr / TileMap chain. */
        if (!g_building_mgr->FindAndNotify(wx, wy)) {
            TileMap_HandleClick(g_tilemap, this->mouse_world_x,
                                this->mouse_world_y);
        }
    } else {
        /* Clicking the map while an object is picked places it. */
        void* obj = TileMap_FindObjectByPos(g_tilemap, wx, wy);
        if (obj != nullptr) {
            uint8_t level = this->selected_object->occupation_level;
            if (level < 7) {
                this->selected_object->occupation_level = level + 1;
            }
            this->selected_object->OnOccupantReady(
                static_cast<Entity*>(obj));
            UI_CreateMessageBox(&g_tooltip_mgr, 0x386D, 0, 'W',
                                wx, wy, 1);
            this->selected_object->next_action_time = 0;
            this->SelectGameObject(nullptr);
            this->left_click_flag = 0;
            return;
        }
    }
    this->left_click_flag = 0;
}

/* ================================================================== */
/* Game::HandleRightClick — right-click world dispatch                  */
/* Address: 0x411230                                                    */
/* ================================================================== */
void Game::HandleRightClick()
{
    int32_t wx, wy;
    this->ScreenToWorld(&wx, (int)(this->right_click_screen_pos & 0xFFFF),
                        (int)(this->right_click_screen_pos >> 16));
    this->right_click_world_x = wx;
    this->right_click_world_y = wy;

    const void* parent = this->resource;
    if (parent == nullptr) {
        return;
    }

    if (this->cursor_sound_id == resource_id(parent)) {
        /* Same resource type: cycle animation or apply override. */
        if (resource_sound_override(parent) == 0) {
            uint16_t anim_count = to_resdata(parent)->anim_count;   /* +0x1A */
            if (this->anim_index + 1 < (int)anim_count) {
                this->StopSound(static_cast<int16_t>(this->anim_index + 1));
            } else {
                this->StopSound(0);
            }
        } else {
            if (this->cursor_sound_id != resource_id(parent) ||
                resource_sound_override(parent) < 1) {
                goto world_chain;
            }
            this->cursor_sound_id = resource_sound_override(parent);
        }
        PlaySoundAt(0x502C, wx, wy, 4);
    } else {
world_chain:
        /* BuildingMgr → World → TileMap dispatch. */
        if (g_building_mgr->FindAndNotify(wx, wy) == 0) {
            if (g_world->ProcessAudio(wx, wy) == 0) {
                if (TileMap_HandleClick(g_tilemap, wx, wy) == 0) {
                    goto done;
                }
            }
        }
        PlaySoundAt(0x5015, wx, wy, 4);
    }

done:
    if (this->selected_visible == 0) {
        this->SelectGameObject(nullptr);
    }
    this->right_click_flag = 0;
}

/* ================================================================== */
/* Game::ClearMouseMode — mouse-button release (build mode)             */
/* Address: 0x410D20                                                    */
/* ================================================================== */
void Game::ClearMouseMode()
{
    TimerSlotsView* timer = game_timer_slots(this);
    const void* parent = this->resource;

    /* Phase 1: clear the timer slot-list selection state. */
    int32_t count = timer->GetCount();
    for (int32_t i = 0; i < count; i++) {
        void* item = timer->GetItem(i);
        if (item != nullptr) {
            /* Locate the entry in the global object array. */
            int32_t found = -1;
            for (int32_t j = 0; j < g_object_count; j++) {
                if (g_object_array[j] == item) {
                    found = j;
                    break;
                }
            }
            Entity* obj = static_cast<Entity*>(static_cast<GameObject*>(item));
            if (found >= 0 && obj->initialized == 1) {
                obj->blit_flags = 0;                 /* +0x2C */
                obj->InvalidateRect();
            }
        }
    }
    timer->Stop();
    this->blit_flags = 0;                            /* +0x2C */

    if (g_game_mode != 4 || g_build_mode == 0) {
        return;
    }

    /* Phase 2: dispatch the release to the scripted object or the
     * building footprint tiles. */
    if (g_scripted_object->CheckClick(this->mouse_world_x,
                                     this->mouse_world_y)) {
        if (g_build_mode != 2) {
            return;
        }
        int32_t parent_id = (parent != nullptr) ? resource_id(parent) : -1;
        if (parent_id != this->cursor_sound_id) {
            return;
        }
        if (this->mouse_drag_mode == 1) {
            this->mouse_drag_mode = 0;
            this->click_on_selected = 0;
            this->blit_flags = 0x400;                /* redraw signal */
            return;
        }
    } else {
        if (this->mouse_drag_mode == 1) {
            this->left_click_flag = 1;
            this->left_click_screen_pos = this->packed_mouse_pos;
        }

        if (g_build_mode == 1) {
            /* Road placement: touch the tile under the cursor. */
            int16_t tx = static_cast<int16_t>(
                (this->mouse_world_x < 0) ? -1 : (this->mouse_world_x >> 4));
            int16_t ty = static_cast<int16_t>(
                (this->mouse_world_y < 0) ? -1 : (this->mouse_world_y >> 4));
            short layer = 0;
            void* obj = TileMap_GetObjectAtEx(g_tilemap, tx, ty, &layer);
            if (obj != nullptr) {
                Building* tile = static_cast<Building*>(obj);
                if (tile->initialized == 1 &&
                    static_cast<uint8_t>(tile->track_node_id) != 0) {
                    tile->blit_flags = 0x400;
                    tile->InvalidateRect();
                    timer->Insert(tile);
                }
            }
        }

        if (g_build_mode != 2) {
            return;
        }
        int32_t parent_id = (parent != nullptr) ? resource_id(parent) : -1;
        if (parent_id != this->cursor_sound_id) {
            return;
        }

        /* Object placement: touch every footprint tile of the resource
         * (rows at RESDATA+0x168, columns at +0x169, footprint mask at
         * +0x16E with 7-byte column stride and 0x3F-byte row stride).
         * The row/column counts are cached; the resource metadata is
         * not modified inside the loop. */
        if (parent != nullptr && resource_play_rows(parent) != 0) {
            const uint8_t* base = reinterpret_cast<const uint8_t*>(parent);
            uint8_t rows = base[0x168];
            uint8_t cols = base[0x169];
            const uint8_t* row_mask = base + 0x16E;
            for (uint32_t row = 0; row < rows; row++) {
                for (uint32_t col = 0; col < cols; col++) {
                    if (row_mask[col * 7] != 0) {
                        int16_t tx = static_cast<int16_t>(
                            (this->mouse_world_x < 0) ?
                                -1 : (this->mouse_world_x >> 4));
                        int16_t ty = static_cast<int16_t>(
                            (this->mouse_world_y < 0) ?
                                -1 : (this->mouse_world_y >> 4));
                        void* obj = TileMap_GetObjectAt(
                            g_tilemap,
                            static_cast<int16_t>(tx + (int16_t)row),
                            static_cast<int16_t>(ty + (int16_t)col), 0);
                        if (obj != nullptr) {
                            Building* tile = static_cast<Building*>(obj);
                            if (tile->initialized == 1) {
                                if (g_allow_building_placement == 1) {
                                    tile->blit_flags = 0x400;
                                    tile->InvalidateRect();
                                }
                                timer->Insert(tile);
                            }
                        }
                    }
                }
                row_mask += 0x3F;
            }
        }

        if (g_allow_building_placement == 1) {
            return;
        }
        if (timer->GetCount() == 0) {
            return;
        }
    }

    this->blit_flags = 0x400;
}

/* ================================================================== */
/* Game::SetScreenMode — capture/cursor mode state machine              */
/* Address: 0x411DC0                                                    */
/* ================================================================== */
void Game::SetScreenMode(uint8_t capture, uint8_t show, uint8_t custom)
{
    if (((this->visible != capture) || (this->cursor_disabled != custom)) &&
        g_demo_mode != 1) {
        this->visible = capture;
        TileMap_InvalidateRect(g_tilemap,
                               this->screen_rect.left,
                               this->screen_rect.top,
                               this->screen_rect.right,
                               this->screen_rect.bottom);
        if (capture == 0) {
            /* Release: restore the arrow cursor. */
            g_mouse_capture = 0;
            ReleaseCapture();
            if (this->cursor_disabled != 0) {
                /* IDC_ARROW = 0x7F00. */
                SetCursor(LoadCursorA(nullptr, reinterpret_cast<const char*>(
                    static_cast<intptr_t>(0x7F00))));
                this->cursor_disabled = 0;
            }
            if (show != 0) {
                while (ShowCursor(1) < 0) { /* keep showing */ }
            }
        } else {
            if (custom == 0) {
                /* Capture with standard hidden cursor. */
                while (ShowCursor(0) >= 0) { /* keep hiding */ }
                this->cursor_disabled = 0;
            } else {
                /* Capture with the busy.ani cursor (build-mode scroll). */
                if (this->cursor_disabled == 0) {
                    if (this->busy_cursor_handle == 0) {
                        char path[260];
                        wsprintfA(path, "%s\\CURSORS\\%s",
                                  g_install_path, "busy.ani");
                        this->busy_cursor_handle = static_cast<int32_t>(
                            reinterpret_cast<intptr_t>(
                                LoadCursorFromFileA(path)));
                    }
                    this->cursor_disabled = 1;
                }
                while (ShowCursor(1) < 0) { /* keep showing */ }
                SetCursor(reinterpret_cast<HCURSOR>(
                    static_cast<intptr_t>(this->busy_cursor_handle)));
            }
            SetCapture(main_window_handle());
            POINT pt;
            if (GetCursorPos(&pt) != 0) {
                this->screensaver_active = 1;
                ScreenToClient(main_window_handle(), &pt);
                this->packed_mouse_pos =
                    static_cast<uint32_t>((pt.y << 16) | (pt.x & 0xFFFF));
                if (this->IsScreensaverActive() != 0) {
                    this->ClearMouseMode();
                    this->screensaver_active = 0;
                    return;
                }
            }
        }
    }
}

/* ================================================================== */
/* Game::SetCursorByResourceId — draw the game cursor                   */
/* Address: 0x411C50                                                    */
/* ================================================================== */
void Game::SetCursorByResourceId(int left, int top, int right, int bottom,
                                 int enable_scroll)
{
    if (this->visible != 0 && this->cursor_disabled == 0) {
        if (this->selected_object != nullptr &&
            this->selected_visible != 0) {
            this->selected_object->Draw(RECT{left, top, right, bottom},
                                        enable_scroll, 0);
        }
        this->Draw(RECT{left, top, right, bottom},
                   enable_scroll, this->blit_flags);            /* 0x405E60 */
        this->DrawConnected(RECT{left, top, right, bottom},
                            enable_scroll, this->blit_flags);   /* 0x405FD0 */
    }
}

/* ================================================================== */
/* Game::ResetCursor — redraw the cursor in the game's own rect        */
/* Address: 0x411D10                                                    */
/* ================================================================== */
void Game::ResetCursor()
{
    if (this->visible != 0) {
        if (this->selected_object != nullptr &&
            this->selected_visible != 0) {
            this->selected_object->Draw(this->screen_rect, 1, 0);
        }
        this->Draw(this->screen_rect, 1, this->blit_flags);
        this->DrawConnected(this->screen_rect, 1, this->blit_flags);
    }
}

/* ================================================================== */
/* Game::SelectGameObject — pick up a building/vehicle                  */
/* Address: 0x4113A0                                                    */
/* ================================================================== */
int Game::SelectGameObject(Building* obj)
{
    uint8_t result = 0;

    if (this->selected_object != obj) {
        this->DeselectGameObject();
        this->selected_object = nullptr;       /* +0xE8 */
    }

    if (obj != nullptr) {
        this->selected_object = obj;
        obj->OnOccupantReady(nullptr);         /* vtable[17] (0x44) */

        uint8_t type = (obj->resource != nullptr)
                           ? resource_type(obj->resource)
                           : 0;
        if (type == 7) {                       /* building */
            uint32_t idx = 0xFFFFFFFF;
            if (g_building_list.count != 0) {
                idx = list_ops(g_building_list)->FindItem(
                    obj, 0,
                    static_cast<uint32_t>(g_building_list.count - 1));
            }
            void* removed =
                list_ops(g_building_list)->InternalExtract(idx);
            if (removed != nullptr) {
                if (idx <
                    static_cast<uint32_t>(g_building_list.count - 1)) {
                    memmove(&g_building_list.items[idx],
                            &g_building_list.items[idx + 1],
                            static_cast<size_t>(
                                g_building_list.count - 1 - idx) *
                                sizeof(void*));
                }
                g_building_list.items[g_building_list.count - 1] = nullptr;
                g_building_list.count--;
            }
        } else if (type == 8) {                /* vehicle */
            uint32_t idx = 0xFFFFFFFF;
            if (g_vehicle_list.count != 0) {
                idx = list_ops(g_vehicle_list)->FindItem(
                    obj, 0,
                    static_cast<uint32_t>(g_vehicle_list.count - 1));
            }
            void* removed =
                list_ops(g_vehicle_list)->InternalExtract(idx);
            if (removed != nullptr) {
                if (idx <
                    static_cast<uint32_t>(g_vehicle_list.count - 1)) {
                    memmove(&g_vehicle_list.items[idx],
                            &g_vehicle_list.items[idx + 1],
                            static_cast<size_t>(
                                g_vehicle_list.count - 1 - idx) *
                                sizeof(void*));
                }
                g_vehicle_list.items[g_vehicle_list.count - 1] = nullptr;
                g_vehicle_list.count--;
            }
        }

        result = 1;
        if (this->mouse_move_flag == 0) {      /* +0xC4 */
            this->selected_visible = 1;        /* +0xEC */
        }
        UI_CreateMessageBox(&g_tooltip_mgr, 0x386D, 0, 'W',
                            obj->world_x, obj->world_y, 1);

        /* Selection sound from the resource (+0x174). */
        if (obj->resource != nullptr) {
            uint32_t sound = *reinterpret_cast<const uint32_t*>(
                reinterpret_cast<const uint8_t*>(obj->resource) + 0x174);
            if (sound != 0 && g_audio != nullptr) {
                g_audio->PlayResource(sound);
            }
        }
    }
    return result;
}

/* ================================================================== */
/* Game::DeselectGameObject — re-insert into the sorted list            */
/* Address: 0x411580                                                    */
/* ================================================================== */
void Game::DeselectGameObject()
{
    Building* sel = this->selected_object;
    if (sel != nullptr && sel->initialized == 1) {
        uint8_t type = (sel->resource != nullptr)
                           ? resource_type(sel->resource)
                           : 0;
        if (type == 7) {                           /* building */
            int32_t idx = -1;
            if (g_building_list.count != 0) {
                idx = static_cast<int32_t>(list_ops(g_building_list)->FindItem(
                    sel, 0,
                    static_cast<uint32_t>(g_building_list.count - 1)));
            }
            if (idx == -1) {
                /* Insert at the sorted position. */
                uint32_t pos = static_cast<uint32_t>(g_building_list.count);
                if (g_building_list.key_size != 0 &&
                    g_building_list.count != 0) {
                    pos = 0;
                    while (list_ops(g_building_list)->IsSlotFilled(pos) != 0) {
                        void* item =
                            list_ops(g_building_list)->InternalExtract(pos);
                        int32_t cmp =
                            list_ops(g_building_list)->Comparator(sel, item);
                        if (cmp < 1) {
                            break;
                        }
                        pos++;
                        if (static_cast<uint32_t>(g_building_list.count) <= pos) {
                            break;
                        }
                    }
                }
                list_ops(g_building_list)->InsertAt(static_cast<int32_t>(pos),
                                                    sel);
            }

            /* Track the tile-overlap count (+0x88). */
            int16_t tile_x = static_cast<int16_t>(
                (sel->world_x < 0) ? -1 : (sel->world_x >> 4));
            int16_t tile_y = static_cast<int16_t>(
                (sel->world_y < 0) ? -1 : (sel->world_y >> 4));
            void* under = TileMap_GetObjectAt(g_tilemap, tile_x, tile_y, 0);
            uint8_t level = sel->occupation_level;
            if (under == nullptr) {
                if (level != 0) {
                    sel->occupation_level = level - 1;
                }
            } else if (level < 7) {
                sel->occupation_level = level + 1;
            }
        }

        type = (sel->resource != nullptr) ? resource_type(sel->resource) : 0;
        if (type == 8) {                           /* vehicle */
            int32_t idx = -1;
            if (g_vehicle_list.count != 0) {
                idx = static_cast<int32_t>(list_ops(g_vehicle_list)->FindItem(
                    sel, 0,
                    static_cast<uint32_t>(g_vehicle_list.count - 1)));
            }
            if (idx == -1) {
                uint32_t pos = static_cast<uint32_t>(g_vehicle_list.count);
                if (g_vehicle_list.key_size != 0 &&
                    g_vehicle_list.count != 0) {
                    pos = 0;
                    while (list_ops(g_vehicle_list)->IsSlotFilled(pos) != 0) {
                        void* item =
                            list_ops(g_vehicle_list)->InternalExtract(pos);
                        int32_t cmp =
                            list_ops(g_vehicle_list)->Comparator(sel, item);
                        if (cmp < 1) {
                            break;
                        }
                        pos++;
                        if (static_cast<uint32_t>(g_vehicle_list.count) <= pos) {
                            break;
                        }
                    }
                }
                list_ops(g_vehicle_list)->InsertAt(static_cast<int32_t>(pos),
                                                   sel);
            }
        }

        sel->InvalidateRect();                     /* vtable[1] (0x04) */
    }
    this->selected_visible = 0;                /* +0xEC */
}

/* ================================================================== */
/* Game::IsScreensaverActive — core mouse-movement check                */
/* Address: 0x410A40                                                    */
/* ================================================================== */
int Game::IsScreensaverActive()
{
    uint32_t packed = this->packed_mouse_pos;  /* +0x90 */

    if ((int)(uint16_t)packed <= g_client_offset_x &&
        (int)(packed >> 16) <= g_client_offset_y) {
        POINT screen_pt;
        screen_pt.x = (int)(packed & 0xFFFF);
        screen_pt.y = (int)(packed >> 16);
        ClientToScreen(main_window_handle(), &screen_pt);
        this->mouse_screen_x = screen_pt.x;    /* +0x94 */
        this->mouse_screen_y = screen_pt.y;    /* +0x98 */

        HWND under = WindowFromPoint(screen_pt);
        if (under == main_window_handle() || this->visible == 0) {
            int32_t wx = 0, wy = 0;
            this->ScreenToWorld(&wx, (int)(packed & 0xFFFF),
                                (int)(packed >> 16));

            bool moved = (wx != this->mouse_world_x) ||
                         (wy != this->mouse_world_y);
            if (moved) {
                if (g_game_mode == 4 && g_build_mode == 2) {
                    g_scripted_object->CheckClick(wx, wy);
                }
                if (this->click_on_selected != 0) {
                    this->mouse_drag_mode = 1; /* +0xE4 */
                }

                const void* parent = this->resource;
                int32_t parent_id = (parent != nullptr)
                                        ? resource_id(parent)
                                        : -1;
                if (parent_id == this->cursor_sound_id &&
                    this->mouse_drag_mode == 1) {
                    /* Drag-scrolling: keep the cursor over the world
                     * point that is being scrolled. */
                    if (this->cursor_sound_id == 0xC1C ||
                        this->cursor_sound_id == 0x3408) {
                        this->mouse_screen_y +=
                            (this->mouse_world_y - wy);
                        SetCursorPos(this->mouse_screen_x,
                                     this->mouse_screen_y);
                        wy = this->mouse_world_y;
                    }
                    if (this->cursor_sound_id == 0xC1A ||
                        this->cursor_sound_id == 0x3409) {
                        this->mouse_screen_x +=
                            (this->mouse_world_x - wx);
                        SetCursorPos(this->mouse_screen_x,
                                     this->mouse_screen_y);
                        wx = this->mouse_world_x;
                    }
                }

                /* Keep the selected object glued to the cursor. */
                if (this->selected_object != nullptr &&
                    this->selected_visible != 0) {
                    uint16_t fw =
                        (this->selected_object->resource != nullptr)
                            ? to_resdata(this->selected_object->resource)
                                  ->frame_width
                            : 0;
                    this->selected_object->MoveTo(wx - (fw >> 1),
                                                  wy - (fw >> 2));
                }

                this->mouse_world_x = wx;
                this->mouse_world_y = wy;

                /* Notify the parent resource of the hover position. */
                if (parent != nullptr) {
                    if (resource_type(parent) == 5) {
                        this->MoveTo(this->mouse_world_x -
                                         to_resdata(parent)->offset_x,
                                     this->mouse_world_y -
                                         to_resdata(parent)->offset_y);
                        return 1;
                    }
                    this->MoveTo(this->mouse_world_x,
                                 this->mouse_world_y -
                                     resource_low_y(parent));
                    return 1;
                }
            }
            return 0;    /* cursor did not move over game content */
        }

        /* Cursor is over one of the overlay windows — restore arrow. */
        if ((g_town != nullptr && under == window_handle_of(g_town)) ||
            (g_cursor != nullptr && under == window_handle_of(g_cursor)) ||
            (g_postcard != nullptr &&
             under == window_handle_of(g_postcard)) ||
            (g_postcard_send != nullptr &&
             under == window_handle_of(g_postcard_send)) ||
            (g_ui_main != nullptr && under == window_handle_of(g_ui_main))) {
            this->SetScreenMode(0, 0, 0);
            return 0;
        }
    }

    this->SetScreenMode(0, 1, 0);
    return 0;
}

/* ================================================================== */
/* Game::ScreenToWorld — screen pixel → world coordinates               */
/* Address: 0x412060                                                    */
/* ================================================================== */
void Game::ScreenToWorld(int32_t* out_xy, int screen_x, int screen_y)
{
    uint32_t x = static_cast<uint32_t>(screen_x + g_viewport_x);
    uint32_t y = static_cast<uint32_t>(screen_y + g_viewport_y);

    if ((int32_t)x < 0) {
        x = 0;
    }
    if ((g_player_id + 1) * 0x10 <= (int32_t)x) {
        x = static_cast<uint32_t>(g_player_id * 0x10 + 0xF);
    }
    if ((int32_t)y < 0) {
        y = 0;
    }
    if ((g_player_color + 1) * 0x10 <= (int32_t)y) {
        y = static_cast<uint32_t>(g_player_color * 0x10 + 0xF);
    }

    const void* parent = this->resource;
    if (parent != nullptr && resource_type(parent) != 5) {
        /* Clamp to the resource play area. */
        uint32_t w = static_cast<uint32_t>(g_world_width) -
                     to_resdata(parent)->frame_width;
        if ((int32_t)w < (int32_t)x) {
            x = w;
        }
        if ((int32_t)y < resource_low_y(parent)) {
            y = resource_low_y(parent);
        }
        uint32_t h = static_cast<uint32_t>(g_world_height) -
                     resource_play_rows(parent) * 0x10;
        if ((int32_t)h < (int32_t)y) {
            y = h;
        }
        /* Snap to the 16px grid when the scripted object does not claim
         * the point (assembly: x - ((x^s) - s & 0xF ^ s) - s, which is
         * signed truncation to a multiple of 16). */
        if (g_scripted_object->CheckClick(static_cast<int32_t>(x),
                                         static_cast<int32_t>(y)) == 0) {
            int32_t sx = static_cast<int32_t>(x);
            int32_t sy = static_cast<int32_t>(y);
            x = static_cast<uint32_t>(sx - (sx % 16));
            y = static_cast<uint32_t>(sy - (sy % 16));
        }
    }

    out_xy[0] = static_cast<int32_t>(x);
    out_xy[1] = static_cast<int32_t>(y);
}

/* ================================================================== */
/* Game::PlaySound                                                      */
/* Address: 0x411FB0                                                    */
/* ================================================================== */
void Game::PlaySound(int sound_id)
{
    const void* parent = this->resource;
    int32_t parent_id = (parent != nullptr) ? resource_id(parent) : -1;
    if (parent_id == sound_id) {
        return;
    }

    int32_t res = g_resmgr.GetById(sound_id);
    if (res != 0) {
        /* vtable[6] — start playback on this entity. */
        this->InitBase(sound_id,
                       resource_anim_index(
                           reinterpret_cast<const void*>(
                               static_cast<intptr_t>(res))),
                       false);

        if (this->initialized != 0) {
            parent = this->resource;
            if (resource_type(parent) == 5) {
                this->SetWorldPos(this->mouse_world_x -
                                      to_resdata(parent)->offset_x,
                                  this->mouse_world_y -
                                      to_resdata(parent)->offset_y);
                return;
            }
            this->cursor_sound_id =
                (parent != nullptr) ? resource_id(parent) : -1;
            this->SetWorldPos(this->mouse_world_x,
                              this->mouse_world_y -
                                  resource_low_y(parent));
        }
    }
}

/* ================================================================== */
/* Game::Shutdown — separate cleanup function (NOT the destructor)      */
/* Address: 0x410700                                                    */
/* ================================================================== */
void Game::Shutdown()
{
    /* Restore the mouse speed settings (SPI_SETMOUSE setting 4). */
    SystemParametersInfoA(4, 0, this->mouse_spi3, 0);

    /* Stop the inline timer sub-object (vtable[5]). */
    game_timer_slots(this)->Stop();

    this->SetScreenMode(0, 1, 0);

    /* Release resources via vtable[6] (InitBase(0, -1, 0)). */
    this->InitBase(0, -1, false);
}

/* ================================================================== */
/* Game::LoadIntroSounds — preload the four intro/title sounds          */
/* Address: 0x410750                                                    */
/* ================================================================== */
bool Game::LoadIntroSounds()
{
    /* The original reuses the +0x110 timer-array slot as scratch and
     * clears it after each resource; the C++ port keeps the scratch in a
     * local and reproduces the final cleared field state.  The
     * intermediate writes are invisible to other code (single-threaded
     * startup path). */
    int32_t s1 = g_resmgr.GetStringById(0x5015);
    if (s1 != 0) {
        void* r1 = reinterpret_cast<void*>(static_cast<intptr_t>(s1));
        RESMGR_LoadSoundResource(r1);
        *reinterpret_cast<uint8_t*>(static_cast<intptr_t>(s1) + 8) = 1; /* keep-alive */
        RESMGR_ReleaseSoundResource(r1);
    }
    int32_t s2 = g_resmgr.GetStringById(0x5014);
    if (s2 != 0) {
        void* r2 = reinterpret_cast<void*>(static_cast<intptr_t>(s2));
        RESMGR_LoadSoundResource(r2);
        *reinterpret_cast<uint8_t*>(static_cast<intptr_t>(s2) + 8) = 1;
        RESMGR_ReleaseSoundResource(r2);
    }
    int32_t s3 = g_resmgr.GetStringById(0x501A);
    if (s3 != 0) {
        void* r3 = reinterpret_cast<void*>(static_cast<intptr_t>(s3));
        RESMGR_LoadSoundResource(r3);
        *reinterpret_cast<uint8_t*>(static_cast<intptr_t>(s3) + 8) = 1;
        RESMGR_ReleaseSoundResource(r3);
    }
    bool all_first_three = (s3 != 0) && (s2 != 0) && (s1 != 0);

    int32_t s4 = g_resmgr.GetStringById(0x501B);
    if (s4 == 0) {
        this->timer_array_ptr = 0;   /* original scratch slot ends at 0 */
        return false;
    }
    void* r4 = reinterpret_cast<void*>(static_cast<intptr_t>(s4));
    RESMGR_LoadSoundResource(r4);
    *reinterpret_cast<uint8_t*>(static_cast<intptr_t>(s4) + 8) = 1;
    RESMGR_ReleaseSoundResource(r4);

    this->timer_array_ptr = 0;       /* original scratch slot ends at 0 */
    return all_first_three;
}

/* ================================================================== */
/* Game::CheckScreensaverTimeout                                        */
/* Address: 0x410A20                                                    */
/* ================================================================== */
void Game::CheckScreensaverTimeout()
{
    if (this->IsScreensaverActive() != 0) {
        this->ClearMouseMode();
        this->screensaver_active = 0;
    }
}

/* ================================================================== */
/* Game_CheckTimeInRange — time window check                            */
/* Address: 0x412710                                                    */
/* __cdecl.  Structs are {second@0, minute@4, hour@8}; -1 sentinel =    */
/* inactive.  Overnight wrap when end < start.                          */
/* ================================================================== */
int Game_CheckTimeInRange(int* current, int* start, int* end)
{
    if (start[1] == -1) {            /* minutes */
        return 0;
    }
    if (end[1] == -1) {
        return 0;
    }
    if (start[2] == 0xFFFFFFFF) {    /* hours — unsigned -1 check */
        return 0;
    }
    if (end[2] == -1) {
        return 0;
    }

    int s = start[1] + start[2] * 60;
    int c = current[1] + current[2] * 60;
    int e = end[1] + end[2] * 60;

    if (e < s) {
        /* Overnight wrap. */
        if (s <= c || c <= e) {
            return 1;
        }
    } else if (s <= c && c <= e) {
        return 1;
    }
    return 0;
}

/* ================================================================== */
/* Game_IsPositionBetween — date/time range check                       */
/* Address: 0x412790                                                    */
/* __cdecl.  Structs are {second@0, minute@4, hour@8, day@0xC,          */
/* month@0x10}.  Day-of-year uses the month-day table at 0x47E410;      */
/* -1 sentinel fields match anything.  Called by INPUT_EditSetFocus.    */
/* ================================================================== */
int Game_IsPositionBetween(int* current, int* start, int* end)
{
    /* Cumulative days before each month (non-leap), from 0x47E410. */
    static const uint16_t kMonthDays[12] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };

    uint32_t s_day = 0, c_day = 0, e_day = 0;
    if (start[4] >= 0 && end[4] >= 0) {       /* month at +0x10 */
        s_day = kMonthDays[start[4]];
        c_day = kMonthDays[current[4]];
        e_day = kMonthDays[end[4]];
    }

    int32_t c_pos = static_cast<int32_t>(c_day) + current[3];   /* +0xC */
    int32_t s_pos = static_cast<int32_t>(s_day) + start[3];
    int32_t e_pos = static_cast<int32_t>(e_day) + end[3];

    bool day_in_range;
    if (e_pos < s_pos) {
        /* Overnight wrap. */
        day_in_range = !(c_pos < s_pos && e_pos < c_pos);
    } else {
        day_in_range = !(c_pos < s_pos || e_pos < c_pos);
    }

    if (day_in_range &&
        start[1] != -1 && end[1] != -1 &&     /* minutes */
        start[2] != 0xFFFFFFFF && end[2] != -1) {   /* hours */
        int s = start[1] + start[2] * 60;
        int c = current[1] + current[2] * 60;
        int e = end[1] + end[2] * 60;
        if (e < s) {
            if (s <= c || c <= e) {
                return 1;
            }
        } else if (s <= c && c <= e) {
            return 1;
        }
    }
    return 0;
}

/* ================================================================== */
/* Typed wrappers for TileMap::ProcessRect's Game dispatch calls.       */
/* Declared in world/tilemap.h; implemented here (not in tilemap.cpp)  */
/* to avoid pulling this file's own headers into that one. Real         */
/* signatures verified against each callee's RET immediate.            */
/* ================================================================== */
extern void* g_game; /* 0x4854C8 */

void Game_SetCursorByResourceId(int left, int top, int right, int bottom,
                                int enable_scroll)
{
    static_cast<Game*>(g_game)->SetCursorByResourceId(left, top, right,
                                                       bottom, enable_scroll);
}

void Game_ResetCursor(void)
{
    static_cast<Game*>(g_game)->ResetCursor();
}
