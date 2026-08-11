/**
 * stubs_link001_batch3_resource_audio.cpp
 *
 * LINK-001 call-0 landmine sweep — batch 3: Resource manager / RESDATA /
 * sound family.
 *
 * Every symbol below was verified unresolved by cross-referencing the
 * caller's OWN forward declaration (not a demangled guess) against either
 * a real, already-implemented function/method elsewhere in the tree, or
 * against nm on the current build/lego_loco.p object files to confirm no
 * definition exists anywhere. See the per-symbol comments for the exact
 * evidence.
 *
 * This file only ADDS missing linker symbols. It never edits an existing
 * caller's declaration, even when that declaration is itself the true bug
 * (see the RESDATA_Lock/Unlock, ResourceManager_GetById, ResourceManager_
 * LoadResource/ReleaseResource, and PlaySound(int) sections below) — those
 * are flagged as `caller-declaration-is-wrong` in the session report
 * instead, per this pass's scope restriction (single new file only).
 */

// Status: TRANSCRIBED

#include <cstdint>
#include <cstdio>
#include <cassert>

#include "../resources/ResourceManager.h"
#include "../audio/AudioChannel.h"
#include "../audio/GameAudio.h"

/* ================================================================== */
/* RESDATA_Lock / RESDATA_Unlock                                       */
/* Address: 0x449410 / 0x449420                                        */
/*                                                                     */
/* caller-declaration-is-wrong: world/scriptengine.h (line ~479-517)   */
/* wraps the real, already-implemented bodies (world/scriptengine.cpp,  */
/* same two addresses) in `extern "C"`, giving them the plain unmangled */
/* linker symbols "RESDATA_Lock"/"RESDATA_Unlock" (confirmed via        */
/* `nm build/lego_loco.p/world_scriptengine.cpp.o` -> `T RESDATA_Lock`, */
/* `T RESDATA_Unlock`, no leading underscore/mangling). game/           */
/* BuildingMgr.cpp (my assigned caller — UpdateAll/CompactCollections/  */
/* RemoveObject) declares `extern void RESDATA_Lock(void*);` /          */
/* `RESDATA_Unlock(void*)` at plain (default C++) linkage instead,      */
/* needing the MANGLED symbols `_Z12RESDATA_LockPv` /                   */
/* `_Z14RESDATA_UnlockPv` (confirmed via                                */
/* `nm build/lego_loco.p/game_BuildingMgr.cpp.o` -> both listed `U`).    */
/* Cannot edit BuildingMgr.cpp (caller) or world/scriptengine.h (would   */
/* also require touching its many other extern "C" sibling decls) per   */
/* this pass's file-scope restriction, so bind a bridge here instead.   */
/*                                                                      */
/* SHOULD_BE_FIXED_AT: world/scriptengine.h:479/504/517 — RESDATA_Lock/ */
/* RESDATA_Unlock should not be inside the `extern "C" { ... }` block   */
/* there; every other in-tree caller (game/BuildingMgr.cpp, game/       */
/* BuildingComplex.cpp, graphics/DDRAW.cpp) already expects plain C++   */
/* linkage, so scriptengine.h's extern "C" wrapping is the outlier.     */
/* ================================================================== */

/* Bind directly to the real (unmangled, extern "C") linker symbols
 * without redeclaring "RESDATA_Lock"/"RESDATA_Unlock" in this TU (which
 * would conflict: same signature, different language linkage cannot be
 * declared twice under one name in one scope). */
extern "C" uint8_t RESDATA_Lock_real(void* ptr) asm("RESDATA_Lock");
extern "C" uint8_t RESDATA_Unlock_real(void* ptr) asm("RESDATA_Unlock");

void RESDATA_Lock(void* ptr)
{
    RESDATA_Lock_real(ptr);
}

void RESDATA_Unlock(void* ptr)
{
    RESDATA_Unlock_real(ptr);
}

/* ================================================================== */
/* RESDATA_CreateChildSprite (extern "C" shape)                        */
/* Address: 0x4546D0                                                   */
/*                                                                     */
/* town/Town.cpp declares this INSIDE an `extern "C" { ... }` block     */
/* (town/Town.cpp:115-117), needing the plain unmangled symbol          */
/* "RESDATA_CreateChildSprite" (confirmed via `nm build/lego_loco.p/    */
/* town_Town.cpp.o` -> `U RESDATA_CreateChildSprite`, no mangling).      */
/* shared/defsym_stubs.cpp already stubs a C++-*mangled* overload of    */
/* this same name/shape (`_Z25RESDATA_CreateChildSpritePvS_ii`) — a      */
/* DIFFERENT linker symbol, so it does not satisfy Town.cpp's need;      */
/* this is not a duplicate.                                             */
/*                                                                       */
/* That existing C++ overload's doc comment independently verified zero */
/* real callers of Town::handle_tile_click() anywhere in the tree; a     */
/* fresh grep here confirms the same for the extern "C" shape (the only */
/* two hits for "handle_tile_click" outside town/Town.cpp itself are     */
/* comments in ui/UI_ChildWindow.cpp and shared/defsym_stubs.cpp, no     */
/* actual call site). Mirrors that file's own precedent: loud stub,      */
/* assert(0), not a silent nullptr — a future caller must fail loudly.   */
/* ================================================================== */
extern "C" void* RESDATA_CreateChildSprite(void* parent, void* res, int x, int y)
{
    (void)parent;
    (void)res;
    (void)x;
    (void)y;
    std::fprintf(stderr,
        "STUB: RESDATA_CreateChildSprite(void*, void*, int, int) [extern \"C\" shape] "
        "reached at %s:%d — 0x4546D0, verified unreachable today (Town::handle_tile_click "
        "has zero callers in this tree), but must not silently return garbage if that "
        "changes.\n", __FILE__, __LINE__);
    assert(0 && "stub reached — RESDATA_CreateChildSprite (extern \"C\" shape), 0x4546D0, "
                "verified unreachable via Town::handle_tile_click");
    return nullptr;
}

/* ================================================================== */
/* RESDATA_ScriptedObject_AddChild (extern "C" shape)                  */
/* Address: 0x44B190                                                   */
/*                                                                     */
/* resources/ResourceManager.cpp declares this INSIDE an `extern "C"`  */
/* block (line ~137-140), needing the plain symbol                     */
/* "RESDATA_ScriptedObject_AddChild" (confirmed via `nm build/          */
/* lego_loco.p/resources_ResourceManager.cpp.o` -> `U                  */
/* RESDATA_ScriptedObject_AddChild`). shared/defsym_stubs.cpp only       */
/* stubs the C++-mangled overload (`_Z31RESDATA_ScriptedObject_        */
/* AddChildPvii`) — a different symbol, not a duplicate.                */
/*                                                                     */
/* Called from ResourceManager::AddString's resourceType==3, odd-resId */
/* branch, which is reachable any time a scripted-object resource ID   */
/* of that class is first requested via ResourceManager::GetById — a   */
/* live path (GetById is called constantly for resource loading), so   */
/* this warns once + returns a benign default rather than asserting.   */
/* The caller already allocates a 0x63C-byte buffer via operator_new   */
/* before calling this; returning nullptr here just leaves that buffer */
/* unconstructed (a leak, not a crash) and this specific resource type  */
/* fails to load — no worse than the pre-existing call-0 in terms of    */
/* functionality, but no longer a hard fault.                          */
/* TODO: decompile 0x44B190 (ScriptedObject-derived child constructor). */
/* ================================================================== */
extern "C" void* RESDATA_ScriptedObject_AddChild(void* obj, int32_t resId, int32_t strPtr)
{
    (void)obj;
    (void)resId;
    (void)strPtr;
    static bool warned = false;
    if (!warned) {
        std::fprintf(stderr,
            "STUB: RESDATA_ScriptedObject_AddChild(void*, int, int) not implemented "
            "(TODO: decompile 0x44B190) — scripted-object resource load dropped, "
            "preallocated buffer left unconstructed\n");
        warned = true;
    }
    return nullptr;
}

/* ================================================================== */
/* ResourceManager_GetById (extern "C" shape)                          */
/* Address: 0x446EA0                                                   */
/*                                                                     */
/* caller-declaration-is-wrong: game/BuildingPanel.cpp                 */
/* (BuildingPanel::init_sprites), graphics/LOCOBITMAP.cpp               */
/* (PostcardAlbum::InitWindowSurface / InitSprites), and                */
/* ui/UI_ChildWindow.cpp (ChildWindow::IsBitmapReady) all declare this   */
/* INSIDE an `extern "C" { ... }` block, needing the plain unmangled    */
/* symbol "ResourceManager_GetById" (confirmed via nm on all three      */
/* corresponding .o files -> `U ResourceManager_GetById`, no mangling). */
/* The only real implementations in the tree are the four C++-mangled   */
/* overloads in resources/resource_manager_sdl3.cpp (`(void*,int32_t)`, */
/* `(void*,uint32_t)`, `(void**,int32_t)`, `(void**,uint32_t)` — see     */
/* `nm build/lego_loco.p/resources_resource_manager_sdl3.cpp.o`), none   */
/* of which is unmangled. Cannot edit any of the three caller files nor */
/* resources/resource_manager_sdl3.cpp per this pass's restriction, so   */
/* bridge to the real `(void*, int32_t)` overload here.                 */
/*                                                                       */
/* SHOULD_BE_FIXED_AT: game/BuildingPanel.cpp:89, graphics/LOCOBITMAP.cpp:34-52 */
/* (the enclosing extern "C" block), and ui/UI_ChildWindow.cpp:20-23 —   */
/* ResourceManager_GetById should not be declared inside `extern "C"`   */
/* there; it is a C++-mangled free function everywhere it is actually   */
/* defined.                                                             */
/* ================================================================== */
extern void* ResourceManager_GetById_real(void* resmgr, int resource_id)
    asm("_Z23ResourceManager_GetByIdPvi");

extern "C" void* ResourceManager_GetById(void* resmgr, int id)
{
    return ResourceManager_GetById_real(resmgr, id);
}

/* ================================================================== */
/* ResourceManager_LoadResource(void*, char const*) /                  */
/* ResourceManager_ReleaseResource(void*)                               */
/* Address: 0x447BA0 / 0x447B90                                        */
/*                                                                     */
/* caller-declaration-is-wrong: network/Netman.h (lines 250-251)       */
/* declares these two names — annotated "was RESMGR_LoadResource" /     */
/* "was RESMGR_ReleaseResource" — for what is really the already-       */
/* implemented `RESMGR_LoadResource`/`RESMGR_ReleaseResource` in         */
/* resources/ResDataSave.cpp (exact same two addresses; see that file's  */
/* own header comment). The rename was made in the declaration but the   */
/* real functions were never renamed/aliased to match, so                */
/* Netman::ProcessPlayerData's calls bind to nothing (confirmed via nm   */
/* on build/lego_loco.p/network_Netman.cpp.o -> both `U`).               */
/* Cannot edit network/Netman.h (caller) or resources/ResDataSave.cpp    */
/* per this pass's restriction, so bridge (by name, not asm-label —      */
/* the names genuinely differ, so no linkage-collision workaround is     */
/* needed here).                                                        */
/*                                                                       */
/* SHOULD_BE_FIXED_AT: network/Netman.h:250-251 — call the real           */
/* RESMGR_LoadResource/RESMGR_ReleaseResource (resources/ResourceManager.h) */
/* directly instead of declaring renamed-but-undefined aliases.          */
/* ================================================================== */
uint8_t ResourceManager_LoadResource(void* resdata, const char* path)
{
    return static_cast<uint8_t>(
        RESMGR_LoadResource(reinterpret_cast<RESDATA*>(resdata), path));
}

void ResourceManager_ReleaseResource(void* resdata)
{
    RESMGR_ReleaseResource(reinterpret_cast<RESDATA*>(resdata));
}

/* ================================================================== */
/* ResourceManager_GetStringById(void*, int)                            */
/* Address: 0x4472B0                                                   */
/*                                                                     */
/* ui/TrainStationWindow.cpp declares `extern int __cdecl               */
/* ResourceManager_GetStringById(void* resmgr, int id);` at plain C++    */
/* linkage (needs `_Z29ResourceManager_GetStringByIdPvi`, confirmed via  */
/* nm on build/lego_loco.p/ui_TrainStationWindow.cpp.o -> `U`). The      */
/* real, already fully-implemented body is the member method            */
/* ResourceManager::GetStringById(UINT) (resources/ResourceManager.cpp,  */
/* same address 0x4472B0) — g_resmgr is the sole ResourceManager         */
/* instance every caller passes as the first (void*) argument. Thin ABI  */
/* bridge, matching the free-function calling convention the original   */
/* binary's callers use.                                                */
/* ================================================================== */
int ResourceManager_GetStringById(void* resmgr, int id)
{
    if (resmgr == nullptr) {
        return 0;
    }
    return reinterpret_cast<ResourceManager*>(resmgr)->GetStringById(static_cast<UINT>(id));
}

/* ================================================================== */
/* ResourceManager::FormatResourceString(unsigned int, char*, int)      */
/* Address: 0x447330                                                   */
/*                                                                     */
/* Declared in resources/ResourceManager.h:428 but never defined         */
/* anywhere in the tree (confirmed via nm on                            */
/* build/lego_loco.p/resources_ResourceManager.cpp.o — no symbol at all, */
/* and ui/GameSetupPanel.cpp needs it: `U                                */
/* ResourceManager::FormatResourceString(unsigned int, char*, int)`).     */
/* The header's own doc comment says it is called by 30+ callers         */
/* (WinMain, CGWND_InitGame, GameWindow_Ctor, UI_WindowBase_Ctor,         */
/* GameSetupPanel, HelpWnd, NETMAN_ProcessMessage, ...) — this is a      */
/* hot, reachable path (window titles/labels), not something safe to     */
/* assert on. A member function can be defined out-of-line in any        */
/* translation unit once the class is visible, so this stays a single-   */
/* file addition without touching resources/ResourceManager.cpp.         */
/* TODO: decompile 0x447330 for real (localized string-table lookup +    */
/* language-offset logic per the header's doc comment).                  */
/* ================================================================== */
void ResourceManager::FormatResourceString(UINT resId, char* outBuf, int32_t bufSize)
{
    static bool warned = false;
    if (!warned) {
        std::fprintf(stderr,
            "STUB: ResourceManager::FormatResourceString(unsigned int, char*, int) not "
            "implemented (TODO: decompile 0x447330) — window titles/labels left blank\n");
        warned = true;
    }
    if (outBuf != nullptr && bufSize > 0) {
        outBuf[0] = '\0';
    }
    (void)resId;
}

/* ================================================================== */
/* LoadSoundResource(int) / ReleaseSoundResource(int)                   */
/*                                                                     */
/* ui/HelpWnd.cpp declares `extern void LoadSoundResource(int handle);`  */
/* and `extern void ReleaseSoundResource(int handle);` at plain C++       */
/* linkage (lines ~120-121) — a distinct pair of names from the           */
/* RESMGR_-prefixed / ResourceEntry*-typed families used everywhere else  */
/* in the tree (grep-confirmed: no other file declares exactly these      */
/* two unprefixed, int-typed names). No real implementation exists        */
/* anywhere for either shape. go_next_page/go_prev_page/hide are          */
/* reachable any time the player uses the in-game help window, so this    */
/* warns once + no-ops rather than asserting.                             */
/* TODO: identify the real address (likely the same conceptual operation  */
/* as RESMGR_LoadSoundResource/ReleaseSoundResource at 0x448D60/0x448EE0, */
/* but HelpWnd.cpp's own address annotations for neighboring calls in      */
/* this cluster are inconsistent with those in other files — needs a      */
/* fresh disassembly pass of HelpWnd's own call sites, not a guess here). */
/* ================================================================== */
void LoadSoundResource(int handle)
{
    (void)handle;
    static bool warned = false;
    if (!warned) {
        std::fprintf(stderr,
            "STUB: LoadSoundResource(int) not implemented (ui/HelpWnd.cpp caller; real "
            "address not yet identified) — sound resource load dropped\n");
        warned = true;
    }
}

void ReleaseSoundResource(int handle)
{
    (void)handle;
    static bool warned = false;
    if (!warned) {
        std::fprintf(stderr,
            "STUB: ReleaseSoundResource(int) not implemented (ui/HelpWnd.cpp caller; real "
            "address not yet identified) — sound resource release dropped\n");
        warned = true;
    }
}

/* ================================================================== */
/* ReleaseSoundResource(ResourceEntry*)                                 */
/* Address: 0x448EE0                                                   */
/*                                                                     */
/* Declared in resources/ResourceManager.h:828 but never defined         */
/* anywhere (confirmed via nm on build/lego_loco.p/ui_GameSetupPanel.cpp.o */
/* -> `U _Z20ReleaseSoundResourceP13ResourceEntry`). GameSetupPanel's own */
/* destructor calls it, so this is reachable whenever a GameSetupPanel   */
/* is torn down. The header's doc comment fully documents the real        */
/* behavior (decrement refcount at +0x120; when it reaches 0 and a        */
/* DirectSound buffer exists at +0x0C with flag bit0 clear, stop+release  */
/* it) — the refcount bookkeeping is implemented for real below (it's a   */
/* plain field decrement on an already-typed struct); the DirectSound     */
/* buffer stop/release side effect is deferred (would require adapting    */
/* AudioDirectSoundDevice/AudioChannel's typed interfaces to a raw         */
/* ResourceEntry::buffer pointer, which is real feature work beyond a      */
/* single stub function) and only warns once instead of performing it.    */
/* TODO: decompile 0x448EE0's buffer-release call fully.                  */
/* ================================================================== */
int32_t ReleaseSoundResource(ResourceEntry* entry)
{
    if (entry == nullptr) {
        return 1;
    }
    entry->refcount--;
    if (entry->refcount <= 0 && entry->buffer != nullptr && (entry->flags & 1) == 0) {
        static bool warned = false;
        if (!warned) {
            std::fprintf(stderr,
                "STUB: ReleaseSoundResource(ResourceEntry*) — DirectSound buffer stop/"
                "release not implemented (TODO: decompile 0x448EE0 buffer-release path); "
                "refcount bookkeeping only, buffer left allocated\n");
            warned = true;
        }
    }
    return 1;
}

/* ================================================================== */
/* PlaySound(int)                                                       */
/* Address: 0x447930                                                    */
/*                                                                      */
/* caller-declaration-is-wrong: game/BuildingMgr.cpp                    */
/* (BuildingMgr::HandleClick), ui/HelpWnd.cpp (HelpWnd::handle_click),   */
/* and ui/UIPANEL.cpp (UIPANEL::HandleDrag) all declare/call             */
/* `PlaySound(int)` (mangled `_Z9PlaySoundi`, confirmed via nm on all    */
/* three .o files -> `U PlaySound(int)`). The one real, already-         */
/* implemented body is `void __cdecl PlaySound(UINT soundId)`            */
/* (resources/ResourceManager.h:498 / ResourceManager.cpp:1342, address   */
/* 0x447930) — mangled `_Z9PlaySoundj`, a DIFFERENT symbol purely         */
/* because `int` vs `UINT` mangle differently even though they are the    */
/* same 32-bit width. resources/ResourceManager.h's own doc comment       */
/* explicitly lists "HelpWnd_HandleClick" and "UI_PaintWindow" among       */
/* PlaySound's 66+ real callers, confirming this is the correct target    */
/* for all three of my assigned callers despite each of their own local   */
/* declarations citing a different (almost certainly stale/copy-pasted)   */
/* address comment (HelpWnd.cpp says 0x459930; UIPANEL.cpp says            */
/* 0x44A290) — resources/ResourceManager.h's fully-decompiled, address-    */
/* and-caller-list-documented 0x447930 is treated as ground truth here.   */
/* Cannot edit any of the three caller files nor                          */
/* resources/ResourceManager.cpp per this pass's restriction.             */
/*                                                                        */
/* SHOULD_BE_FIXED_AT: game/BuildingMgr.cpp:20, ui/HelpWnd.cpp:146,        */
/* ui/UIPANEL.cpp:105 — each should declare `PlaySound(unsigned int)`     */
/* (or `UINT`) to match the real 0x447930 implementation, and their        */
/* address comments (0x459930, 0x44A290) should be corrected to 0x447930  */
/* or removed if they were never verified.                                */
/* ================================================================== */
void PlaySound(int soundId)
{
    PlaySound(static_cast<UINT>(soundId));
}

/* ================================================================== */
/* PlaySoundFile(char const*, void*, void*, int)                        */
/* Address: 0x447A70 (documented, not yet implemented)                  */
/*                                                                      */
/* network/Netman.h:277 declares `int32_t PlaySoundFile(const char*      */
/* path, void* x, void* y, int32_t flags);` — the `void* x, void* y`      */
/* shape matches Netman.h's OWN (also incorrect, but out of this batch's  */
/* scope — see note below) redeclaration of g_listener_x/g_listener_y as  */
/* `void*` (network/Netman.h:225-226) rather than the real `int32_t`       */
/* globals (shared/link_stubs.cpp:790). resources/ResourceManager.h:533    */
/* declares a differently-shaped, ALSO-undefined                          */
/* `void PlaySoundFile(const char*, int32_t, int32_t, uint32_t)` for the   */
/* same documented address — neither shape has a real body anywhere in    */
/* the tree (confirmed via nm on                                          */
/* build/lego_loco.p/resources_ResourceManager.cpp.o — no PlaySoundFile    */
/* symbol at all). Genuinely missing, not a mismatch with something        */
/* real. Netman::ReceivePlayerName is reachable during ordinary            */
/* multiplayer session join, so this warns once + returns a benign 0       */
/* rather than asserting.                                                  */
/*                                                                         */
/* NOTE (out of my assigned-symbol scope, flagged for awareness only):      */
/* network/Netman.h:225-226 declaring g_listener_x/g_listener_y as void*   */
/* while shared/link_stubs.cpp defines them as int32_t is a real ODR-      */
/* violating type mismatch (differs in size on a 64-bit host) — not fixed  */
/* here since neither symbol is in my assigned list and Netman.h is a      */
/* caller file I must not edit in this pass.                               */
/* TODO: decompile 0x447A70.                                               */
/* ================================================================== */
int32_t PlaySoundFile(const char* path, void* x, void* y, int32_t flags)
{
    (void)path;
    (void)x;
    (void)y;
    (void)flags;
    static bool warned = false;
    if (!warned) {
        std::fprintf(stderr,
            "STUB: PlaySoundFile(char const*, void*, void*, int) not implemented "
            "(TODO: decompile 0x447A70) — external WAV playback dropped\n");
        warned = true;
    }
    return 0;
}

/* ================================================================== */
/* AudioChannel_Release(void*)                                          */
/* Address: 0x40ECA0                                                    */
/*                                                                      */
/* ui/HelpWnd.cpp declares `extern void AudioChannel_Release(void*       */
/* channel);` at plain C++ linkage (line 123, outside any extern "C"     */
/* block). The real, already fully-implemented body is the member         */
/* method AudioChannel::Release() (audio/AudioChannel.cpp:134, same        */
/* address) — graphics/DDRAW.cpp also has a same-named `static` free       */
/* function wrapping the same logic, but `static` gives it internal        */
/* linkage only to that one translation unit, so it cannot satisfy any      */
/* other caller. Thin ABI bridge to the real member method.                */
/* ================================================================== */
void AudioChannel_Release(void* channel)
{
    if (channel != nullptr) {
        reinterpret_cast<AudioChannel*>(channel)->Release();
    }
}

/* ================================================================== */
/* GameAudio_Init(void*, int, void*)                                    */
/* Address: 0x412C50                                                    */
/*                                                                      */
/* native/ddraw_audio_init.c (the real, already-implemented body of      */
/* DDRAW_InitAudio, 0x45B7E0) declares and calls `extern uint32_t          */
/* __cdecl GameAudio_Init(void* audio, int channels, void* hwnd);` —       */
/* confirmed via nm on build/lego_loco.p/native_ddraw_audio_init.c.o ->    */
/* `U GameAudio_Init(void*, int, void*)`, with a detailed file-header       */
/* comment explaining that disassembly of 0x412C50 shows two real          */
/* __thiscall stack params (num_channels, hwnd) with `RET 0x8`, but the     */
/* in-tree GameAudio::Init() (audio/GameAudio.cpp:97) is currently           */
/* zero-arg and hardcodes num_channels=16 / hwnd=nullptr instead of using    */
/* the real caller-supplied values (SetCooperativeLevel(nullptr, 2) at       */
/* line 132, num_channels=16 at line 151). This bridges to that real but     */
/* parameter-incomplete method — it is reachable (ui/EditWindow.cpp calls    */
/* DDRAW_InitAudio() directly on first name-entry), so this must not          */
/* assert; it forwards the real (degraded) result and warns once about the   */
/* dropped parameters rather than silently pretending they were honored.     */
/*                                                                            */
/* SHOULD_BE_FIXED_AT: audio/GameAudio.h:84 and audio/GameAudio.cpp:97-160 —  */
/* GameAudio::Init() should take (int32_t num_channels, void* hwnd) and       */
/* thread them into SetCooperativeLevel/max_channels instead of the           */
/* hardcoded nullptr/16 (this is a GameAudio.cpp validation task, not         */
/* fixable from a stub file per this pass's scope).                          */
/* ================================================================== */
uint32_t GameAudio_Init(void* audio, int channels, void* hwnd)
{
    static bool warned = false;
    if (!warned) {
        std::fprintf(stderr,
            "STUB: GameAudio_Init(void*, int, void*) bridges to the real but "
            "parameter-incomplete GameAudio::Init() (0x412C50) — channels=%d, hwnd=%p "
            "are dropped (GameAudio::Init() currently hardcodes num_channels=16 and "
            "passes nullptr as hwnd; TODO: thread both real params through, see "
            "audio/GameAudio.h:84 / audio/GameAudio.cpp:97)\n", channels, hwnd);
        warned = true;
    }
    if (audio == nullptr) {
        return 0;
    }
    return reinterpret_cast<GameAudio*>(audio)->Init();
}

/* ================================================================== */
/* GameAudio_UpdateVolume(void*, unsigned char)                         */
/* Address: 0x4135B0                                                    */
/*                                                                      */
/* game/ScriptedObject.cpp declares `extern void GameAudio_UpdateVolume( */
/* void* audio, uint8_t mute);` at plain C++ linkage (line 72, outside    */
/* any extern "C" block). The real, already fully-implemented body is     */
/* the member method GameAudio::UpdateVolume(uint8_t) (audio/            */
/* GameAudio.cpp:496, same address). Thin ABI bridge.                     */
/* ================================================================== */
void GameAudio_UpdateVolume(void* audio, unsigned char mute)
{
    if (audio != nullptr) {
        reinterpret_cast<GameAudio*>(audio)->UpdateVolume(mute);
    }
}

/* ================================================================== */
/* Ordinal_1(int, void*)                                                */
/* Address: unknown — DLL ordinal import, not a game function          */
/*                                                                      */
/* audio/GameAudio.cpp:42 declares `int32_t Ordinal_1(int32_t, void*);`  */
/* (plain C++ linkage; commented "DirectSoundCreate / Enum") and calls    */
/* it unconditionally from GameAudio::Init() on both branches            */
/* (lines 112, 124: `Ordinal_1(0, &this->ds_device)`). network/           */
/* DirectPlay.cpp's file header independently documents that this exact   */
/* 2-arg overload is "genuinely missing" (see docs/landmine-sweep-        */
/* worklist.md) and unrelated to DirectPlay.cpp's own 3-arg               */
/* `Ordinal_1(const GUID*, void**, int32_t)` (different mangled name,     */
/* no ODR conflict). This is ordinal-1 of dsound.dll (DirectSoundCreate)  */
/* loaded via GetProcAddress on real Windows — a genuine thin OS/DLL      */
/* wrapper per CLAUDE.md's stub exception #1, not original game logic.    */
/*                                                                        */
/* Because GameAudio_Init above now bridges to the real GameAudio::Init(), */
/* this becomes reachable on every audio-init attempt; a real host-backed   */
/* DirectSound device already exists (audio/sdl3_dsound.cpp's               */
/* DirectSoundCreate/IDirectSound), but it implements a different,           */
/* concrete, SDL3-backed struct — not the abstract COM-style                 */
/* AudioDirectSoundDevice vtable interface GameAudio::ds_device expects       */
/* (audio/AudioChannel.h:84), and no adapter class bridging the two exists    */
/* yet. Building that adapter is real feature work beyond a single stub      */
/* function, so this returns a documented failure code instead: both real    */
/* call sites in GameAudio::Init() already check `if (result != 0) return    */
/* result & 0xFFFFFF00;`, so a nonzero return here makes audio init fail      */
/* gracefully (no audio device) rather than dereferencing an uninitialized    */
/* ds_device pointer.                                                        */
/* ================================================================== */
int32_t Ordinal_1(int32_t provider, void* out_object)
{
    (void)provider;
    (void)out_object;
    static bool warned = false;
    if (!warned) {
        std::fprintf(stderr,
            "STUB: Ordinal_1(int32_t, void*) not implemented — dsound.dll ordinal-1 "
            "(DirectSoundCreate) used by GameAudio::Init() (0x412C50); no adapter exists "
            "yet from the real AudioDirectSoundDevice vtable interface to the host's SDL3 "
            "IDirectSound (audio/sdl3_dsound.cpp). Returning a failure code so "
            "GameAudio::Init() takes its documented graceful-failure path.\n");
        warned = true;
    }
    return -1;
}
