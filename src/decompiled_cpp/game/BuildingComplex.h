/**
 * BuildingComplex.h — Complex/multi-tile building extension
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * BuildingComplex extends Building for buildings that span multiple
 * tiles (e.g., large stations, multi-part structures). It adds
 * timer dispatch for coordinating animations across tiles.
 *
 * Vtable: 0x478008
 */

#pragma once

#include "Building.h"

class BuildingComplex : public Building {
public:
    /* Additional fields beyond Building's 244 bytes */

    /**
     * Base destructor.
     * Address: 0x437E60
     */
    void BaseDtor();

    /**
     * Full constructor.
     * Address: 0x437EA0
     */
    BuildingComplex(int resource_id);

    /**
     * Scalar deleting destructor.
     * Address: 0x437E80
     */
    void* scalar_deleting_destructor(byte flags);

    /**
     * Dispatch timer events across all tiles.
     * Address: 0x438070
     */
    void DispatchTimers();
};
