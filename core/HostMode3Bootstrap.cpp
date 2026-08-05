/**
 * HostMode3Bootstrap.cpp — SDL-host construction of the mode-3 object cone
 *
 * Lego Loco (loco.exe, 1998, MSVC x86) — host-only deviation (#ifndef _WIN32)
 *
 * The original constructs the embedded singletons (g_world, g_game,
 * g_building_mgr, g_input_mgr, g_scripted_object) via CRT static-init
 * thunks (0x45C560..0x45C650) and initializes the TileMap via
 * TileMap_Init in GameLoop_Setup.  g_input_mgr is now a typed static
 * object (InputMgr g_input_mgr; — 0x4A9990, ctor 0x41D250 via the C++
 * static-init equivalent of thunk 0x45C620), so BootstrapMode3Core does
 * not construct it.  The SDL host keeps the other decompiled void*
 * storage in shared/stubs_impl.cpp and constructs the real C++ objects
 * here in GameLoop_Setup order, so the mode-1 loading worker
 * (original 0x45DE40) and CGWND_EnterMode3 common tail can run against
 * real objects instead of nullptr.
 *
 * WIN32_QueueAsyncTask: the original spawns a worker thread whose proc
 * (0x461890) calls callback(param).  The SDL pump is single-threaded, so
 * the host records the pending (callback, param) pair and runs it once
 * from the pump before the per-frame tick (see CGWND_sdl3.cpp).
 */

// Status: TRANSCRIBED

#ifndef _WIN32

#include "../input/InputMgr.h"
#include "../input/PersistenceAdapter.h"
#include "../core/Game.h"
#include "../core/CGWND.h"
#include "../game/World.h"
#include "../game/BuildingMgr.h"
#include "../world/scriptengine.h"
#include "../world/tilemap.h"
#include "../audio/GameAudio.h"
#include "sdl3_town_mode3.h"
/* sdl3_tile_placement.h: host placement helpers (see that file) */

#include <cstdio>
#include <cstring>
#include <new>

extern void* operator_new(size_t size);              /* 0x465CE0 */
extern void  GLOBAL_free(void* ptr);                 /* 0x465CD0 */

/* Singleton storage (defined in shared/stubs_impl.cpp) — file scope so
 * these bind to the same C++ symbols as every other decompiled TU.
 * g_scripted_object and g_tilemap are canonically declared as pointers by
 * world/scriptengine.h and world/tilemap.h. */
extern void* g_game;             /* 0x4854C8 */
extern void* g_world;            /* 0x4A98B0 */
extern void* g_building_mgr;     /* 0x485448 */
extern void* g_audio;            /* 0x4FD3BC */
extern void CGWND_SetMode(int mode);   /* 0x408130 */

/* Odr-use the tilemap.h inline wrappers so out-of-line definitions are
 * emitted for TUs (e.g. CGWND.cpp) that declare the free forms without
 * including tilemap.h. */
void (*const host_emit_tilemap_updateall)(TileMap*) = &TileMap_UpdateAll;
void (*const host_emit_tilemap_invalidaterect)(TileMap*, int, int, int, int) =
    &TileMap_InvalidateRect;

namespace loco {
namespace host {

/* ================================================================== */
/* BootstrapMode3Core — construct the embedded-singleton cone           */
/*                                                                      */
/* Construction order mirrors the original static-init thunks: Game     */
/* first (TileMap::FullReset calls Game_DeselectGameObject), then World, */
/* BuildingMgr, ScriptedObject, TileMap (+Init), GameAudio.             */
/* ================================================================== */
void BootstrapMode3Core()
{
    /* g_input_mgr is a typed static object (InputMgr g_input_mgr; —
     * 0x4A9990) whose no-arg ctor (0x41D250) ran at static init via the
     * C++ equivalent of the CRT thunk 0x45C620; its embedded entity
     * collection is ready.  The tooltip manager (g_tooltip_mgr, 0x4FD220,
     * UI_Ctor 0x4238C0 via thunk 0x45C680) stays unconstructed on the host:
     * there is no typed UI-Manager reconstruction yet, and the raw-C
     * native/ui_manager.c is in NATIVE_BROKEN (evidence documented in
     * input/InputMgr.h). */
    std::fprintf(stderr,
        "[HOST] BootstrapMode3Core: g_input_mgr static object at %p "
        "(ctor 0x41D250, capacity=%d, count=%d); tooltip mgr deferred\n",
        static_cast<void*>(&g_input_mgr),
        static_cast<int>(g_input_mgr.capacity),
        static_cast<int>(g_input_mgr.count));
    std::fflush(stderr);

    /* TileMap first: Game::Game() calls SetScreenMode → TileMap_InvalidateRect,
     * which needs a non-null TileMap.  The original TileMap was a BSS object,
     * so its dimensions and occupancy pointer were zero before its constructor
     * called FullReset().  Host operator_new storage is indeterminate: zero it
     * before placement construction or FullReset can treat garbage dimensions
     * as an allocated occupancy bitmap.  The ctor otherwise only touches empty
     * host stubs (Game_DeselectGameObject/World_Init/tooltips), so it is safe
     * ahead of Game. */
    if (g_tilemap == nullptr) {
        void* tilemap_storage = operator_new(sizeof(TileMap));
        if (tilemap_storage != nullptr) {
            std::memset(tilemap_storage, 0, sizeof(TileMap));
            g_tilemap = ::new (tilemap_storage) TileMap;
            static_cast<TileMap*>(g_tilemap)->Init(0);
            std::fprintf(stderr,
                "[HOST] BootstrapMode3Core: TileMap constructed+Init (%p)\n",
                g_tilemap);
            std::fflush(stderr);
        } else {
            std::fprintf(stderr,
                "[HOST] BootstrapMode3Core: TileMap allocation failed; "
                "mode-3 bootstrap cannot continue\n");
            std::fflush(stderr);
        }
    }

    if (g_game == nullptr) {
        /* The original g_game lived in BSS (zeroed) before its ctor ran, so
         * fields the ctor reads before writing (e.g. screen_rect) were zero.
         * operator_new is malloc-like on the host; zero first for parity. */
        void* gm = operator_new(sizeof(Game));
        if (gm != nullptr) {
            std::memset(gm, 0, sizeof(Game));
            g_game = ::new (gm) Game;
        }

        /* Host-only deviation — Game has no animation resource.
         *
         * Assembly: Game_Update (0x410840) calls GameObject_Update
         * (0x405C40) on the Game object every frame while visible. That
         * function returns only when +0x18 (initialized) != 1, then reads
         * [resource+0x20]. The Game's resource (+0x40) is NULL forever
         * (verified: no code ever writes g_game+0x40) while the generic
         * GameObject ctor (0x4369D0) leaves initialized = 1, so the
         * original binary dereferences [0x20] and page-faults at 0x405C57
         * (mov 0x20(%ecx),%edx, ECX=0) on the first mode-3 frame — this
         * was reproduced live under Wine and is exactly the crash the SDL
         * host hits in Entity::Update. The Game has no animation frames,
         * so initialized must be 0: Entity::Update's guard then returns
         * early, the same state Entity::InitBase (0x405900) produces for a
         * failed resource load. Revisit if a host world-file load ever
         * gives the Game a resource. */
        static_cast<Game*>(g_game)->initialized = 0;
        std::fprintf(stderr, "[HOST] BootstrapMode3Core: Game constructed (%p)\n", g_game);
        std::fflush(stderr);
    }

    if (g_world == nullptr) {
        /* World has no constructor in the binary; the original instance was
         * BSS-zeroed. Zero the allocation so empty vehicle/sub-object slots
         * are NULL before any method reads them. */
        void* wm = operator_new(sizeof(World));
        if (wm != nullptr) {
            std::memset(wm, 0, sizeof(World));
            g_world = ::new (wm) World;
        }
        std::fprintf(stderr, "[HOST] BootstrapMode3Core: World constructed (%p)\n", g_world);
        std::fflush(stderr);
    }

    if (g_building_mgr == nullptr) {
        /* Original embedded at 0x485448 in BSS; zero first like World so the
         * embedded lock/collection fields the ctor does not reach are NULL. */
        void* bm = operator_new(sizeof(BuildingMgr));
        if (bm != nullptr) {
            std::memset(bm, 0, sizeof(BuildingMgr));
            g_building_mgr = ::new (bm) BuildingMgr;
        }
        std::fprintf(stderr, "[HOST] BootstrapMode3Core: BuildingMgr constructed (%p)\n", g_building_mgr);
        std::fflush(stderr);
    }

    if (g_scripted_object == nullptr) {
        void* mem = operator_new(sizeof(RESDATA_ScriptedObject));
        if (mem != nullptr) {
            std::memset(mem, 0, sizeof(RESDATA_ScriptedObject));
            /* Host: skip Ctor() — the x86_64 layout does not match the
             * original binary.  operator_new zeroes the allocation; the
             * host main-menu path does not exercise ScriptedObject
             * behavior. */
            g_scripted_object = mem;
            std::fprintf(stderr, "[HOST] BootstrapMode3Core: ScriptedObject raw alloc (%p)\n", g_scripted_object);
            std::fflush(stderr);
        }
    }

    /* g_tilemap was constructed first (see above). */

    std::fprintf(stderr, "[HOST] BootstrapMode3Core: about to construct GameAudio\n");
    std::fflush(stderr);
    if (g_audio == nullptr) {
        /* GameAudio::Init() talks to DirectSound ordinals; the SDL host
         * substitutes SDL3_GameAudio, so construct the object (field init)
         * without the DSound device bootstrap. */
        g_audio = new GameAudio;
        std::fprintf(stderr, "[HOST] BootstrapMode3Core: GameAudio new done\n");
        std::fflush(stderr);
        std::fprintf(stderr, "[HOST] BootstrapMode3Core: GameAudio constructed (%p) "
                     "(DSound Init deferred to SDL3_GameAudio)\n", g_audio);
        std::fflush(stderr);
    }
    std::fprintf(stderr, "[HOST] BootstrapMode3Core: returning\n");
    std::fflush(stderr);
}

/* ================================================================== */
/* Pending async-task slot (replaces the original worker thread)         */
/* ================================================================== */

static void*    s_pending_callback = nullptr;
static intptr_t s_pending_param    = 0;

bool HasPendingAsyncTask() { return s_pending_callback != nullptr; }

void QueuePendingAsyncTask(void* callback, intptr_t param)
{
    std::fprintf(stderr, "[HOST] async task queued: %p(param=%ld)\n",
                 callback, static_cast<long>(param));
    std::fflush(stderr);
    s_pending_callback = callback;
    s_pending_param = param;
}

void RunPendingAsyncTask()
{
    if (s_pending_callback == nullptr) return;
    void* callback = s_pending_callback;
    intptr_t param = s_pending_param;
    s_pending_callback = nullptr;
    s_pending_param = 0;

    using TaskFn = void (*)(void*);
    std::fprintf(stderr, "[HOST] async task %p(param=%ld) running\n",
                 callback, static_cast<long>(param));
    std::fflush(stderr);
    reinterpret_cast<TaskFn>(callback)(reinterpret_cast<void*>(param));
}

/* ================================================================== */
/* HostLoadingSequence — the mode-1 loading worker (original 0x45DE40)  */
/*                                                                      */
/* Original: init Game state, load/create the world, init TileMap/      */
/* ScriptedObject/DDRAW building/town, then wait for mode 1 to change.  */
/* Host: the cone is constructed in BootstrapMode3Core (g_input_mgr is  */
/* the typed static object).  A fresh single-player world is started    */
/* through the real INPUT_NewWorld (0x41E120), seeded from a shipped    */
/* scenario fixture and persisted to "curr" via INPUT_SaveCurrentWorld  */
/* (0x41D9B0) — see loco::host::seed_fresh_world_from_fixture (the      */
/* typed PersistenceAdapter; the record-set model and its explicit      */
/* placement limitation are documented in PersistenceAdapter.h).        */
/* ================================================================== */
void HostLoadingSequence(void* /*param*/)
{
    /* A fresh single-player host world: INPUT_NewWorld (real game init),
     * then seed the typed record set from a shipped fixture and persist
     * it to "curr" through the real save machinery.  INPUT_LoadWorld is
     * the global declared by InputMgr.h — no local extern here: a block
     * extern inside namespace loco::host would declare the undefined
     * loco::host::INPUT_LoadWorld instead. */
    bool seeded = loco::host::seed_fresh_world_from_fixture(&g_input_mgr);

    /* The host then loads "curr" back through the real INPUT_LoadWorld
     * (0x41D320) so the persisted world is the one the mode-3 loop
     * operates on.  seed_fresh_world_from_fixture returns false when the
     * fixture parse OR the durable "curr" save failed (atomic host
     * save — see PersistenceAdapter.cpp); the load-back is then skipped
     * and the failure is logged loudly, so a fresh seed NEVER reports
     * success without a durable curr. */
    if (seeded) {
        char loaded = INPUT_LoadWorld(&g_input_mgr, "curr");
        std::fprintf(stderr,
            "[HOST] LoadingSequence: fresh world seeded + persisted to "
            "curr (load-back result %d); entering mode 3\n",
            static_cast<int>(loaded));
        std::fflush(stderr);

        /* Host: the existing INPUT_LoadWorld → INPUT_LoadSaveFile path
         * already places records when host_placement_available() returns
         * true.  Enable that gate now that TileMap, ResourceManager, and
         * tile predicates are available. */
        loco::host::set_host_placement_available(true);
    } else {
        std::fprintf(stderr,
            "[HOST] LoadingSequence: fresh-world seed FAILED (no durable "
            "curr) — entering mode 3 with the empty fresh world\n");
        std::fflush(stderr);
    }

    /* The original queues WM_USER+0x406 after mode 3 and reaches Town::show
     * only after its post-load worker completes.  The SDL host has no typed
     * Town presentation/resource-object adapter yet, so preserve the stable
     * mode-3 world rather than synchronously entering the x86 town UI path. */
    CGWND_SetMode(3);
}

/* ================================================================== */
/* HostPostLoadWorker — the mode-3 post-load worker (original 0x42CC60) */
/*                                                                      */
/* Original: enumerate tilemap post-load assets + BuildingMgr cleanup,  */
/* then loop while mode stays 3.  The SDL pump already loops, so the    */
/* host runs the one-shot portion once.                                 */
/* ================================================================== */
void HostPostLoadWorker(void* /*param*/)
{
    /* Host-only deviation: World_EnumeratePostLoadAssets (0x457380)
     * consumes the unported TileMap asset-loader results.  The SDL
     * persistence adapter deliberately carries records without native
     * placement, so do not dispatch that original worker until typed tile
     * metadata and object placement are available. */
    if (g_tilemap != nullptr) {
        std::fprintf(stderr,
            "[HOST] PostLoadWorker: tile asset enumeration deferred "
            "(SDL tile metadata adapter unavailable)\n");
    }
    if (g_building_mgr != nullptr) {
        /* The host persistence adapter carries records but does not create
         * native Building instances until ResourceObject tile metadata is
         * reconstructed.  The recovered cleanup traverses those x86-managed
         * collections, so dispatching it against the empty host adapter is
         * neither useful nor safe.  Keep it deferred rather than executing
         * an x86 vtable walk over host state. */
        std::fprintf(stderr,
            "[HOST] PostLoadWorker: BuildingMgr cleanup deferred "
            "(no native placed Building instances)\n");
    }
    std::fprintf(stderr, "[HOST] PostLoadWorker: completed supported cleanup\n");
    std::fflush(stderr);
}

}  // namespace host
}  // namespace loco

/* ================================================================== */
/* WIN32_QueueAsyncTask — host runtime (single-threaded pump)           */
/*                                                                      */
/* Signature matches the declarations in InitMode1.cpp / CGWND.cpp.     */
/* The original spawns a worker thread; the host defers the callback    */
/* to the next SDL pump iteration (loco::host::RunPendingAsyncTask).    */
/* ================================================================== */
void WIN32_QueueAsyncTask(void* queue, void* callback, int param)
{
    (void)queue;
    if (callback == nullptr) return;
    loco::host::QueuePendingAsyncTask(callback, param);
}

#endif /* _WIN32 */
