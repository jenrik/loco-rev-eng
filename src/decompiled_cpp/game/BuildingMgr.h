/**
 * BuildingMgr.h — Building Manager singleton
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * BuildingMgr manages all Building objects in the game world:
 * creation from resources, per-frame dispatch, rendering overlays,
 * click handling, and collection maintenance.
 *
 * Singleton at g_building_mgr (0x485448).
 */

#pragma once

#include "../shared/types.h"

class Building;

class BuildingMgr {
public:
    /* Collection fields */
    void*  building_list;       /* dynamic array/list of Building* */
    int    building_count;
    int    building_capacity;
    /* Additional collection management fields */

    /**
     * Blit overlap indicators for selected buildings.
     * Address: 0x434AB0
     */
    void BlitOverlaps();

    /**
     * Compact building collections (remove gaps from removals).
     * Address: 0x434870
     */
    void CompactCollections();

    /**
     * Create a Building from a resource definition.
     * Address: 0x4348F0
     *
     * Allocates Building object (operator_new(0xF4)), calls
     * Building::Building() constructor with the given resource ID.
     */
    Building* CreateFromResource(int resource_id);

    /**
     * Destroy all buildings and free memory.
     * Address: 0x434E50
     */
    void DestroyAll();

    /**
     * Dispatch per-frame update to all buildings.
     * Address: 0x434DC0
     */
    void DispatchAll();

    /**
     * Find a building at position and notify it of click.
     * Address: (in buildingmgr_findandnotify.c)
     */
    void FindAndNotify(int x, int y);

    /**
     * Handle a mouse click at the given position.
     * Address: (in buildingmgr_handleclick.c)
     */
    void HandleClick(int x, int y);

    /**
     * Invalidate rectangles for all visible buildings.
     * Address: (in buildingmgr_invalidaterects.c)
     */
    void InvalidateRects();

    /**
     * Remove buildings with no occupants.
     * Address: (in buildingmgr_removeempty.c)
     */
    void RemoveEmpty();

    /**
     * Remove a specific building object.
     * Address: (in buildingmgr_removeobject.c)
     */
    void RemoveObject(void* building);

    /**
     * Update all buildings (comprehensive update cycle).
     * Address: (in buildingmgr_updateall.c)
     */
    void UpdateAll();
};
