/**
 * ResourceObject.h — typed view of the common RESDATA resource vtable
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * RESDATA (shared/types.h, vtable 0x478274) has exactly 3 slots:
 *   [0] scalar deleting destructor (ResourceData_Dtor, 0x447B60)
 *   [1] Lock/GetSurface (returns pixel data surface)
 *   [2] Unlock/ReleaseSurface
 * Slot 0 is a real virtual destructor here, not a hand-rolled `Destroy(flags)`
 * method — the "flags" argument in the original is the MSVC scalar-deleting-
 * destructor's own compiler-generated "also call operator delete" bit, which
 * plain C++ `delete` already reproduces (see CLAUDE.md's "Scalar/vector
 * deleting-destructor flags... -> remove; keep only user cleanup"). RESDATA
 * itself stays a plain, unmodeled struct (see shared/types.h) — it is never
 * the runtime type behind a resource pointer on this host-only-executing
 * build; concrete resource classes (e.g. loco::assets::SpriteResource,
 * resources/resource_manager_sdl3.cpp) derive from ResourceObject instead.
 *
 * Split out of ResourceManager.h so callers that only need this vtable
 * bridge (including a plain .c file, native/NETMAN_NetworkUI.c) don't have
 * to pull in ResourceManager.h's much heavier game/TrackPiece.h dependency.
 */
#pragma once

#include <stdint.h>

class ResourceObject {
public:
    virtual void* Lock(int32_t flags, int32_t mode) = 0;
    virtual void Unlock() = 0;
    virtual ~ResourceObject() = default;
};
