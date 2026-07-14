/**
 * Train.h — Train game object class
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Train extends Building, sharing the Building_BaseCtor for construction.
 * Trains move along tracks, transport minifigures between stations,
 * and have specialized movement and scheduling logic.
 *
 * The Train constructor calls Building::BaseCtor then sets its own vtable.
 */

#pragma once

#include "Building.h"

class Train : public Building {
public:
    /* Train-specific fields (beyond Building's 0xF4 bytes) */

    /**
     * Constructor — calls Building::BaseCtor, sets Train vtable.
     * Address: 0x4533D8
     */
    Train(int resource_id);

    /**
     * Scalar deleting destructor.
     */
    void* scalar_deleting_destructor(byte flags);

    /**
     * Deserialize train state from save data.
     * Address: 0x435E00 area
     */
    void Deserialize(void* data);
};
