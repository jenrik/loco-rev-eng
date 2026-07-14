/**
 * Train.cpp — Train class implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 */

#include "Train.h"

extern void GLOBAL_free(void* ptr);


/* ================================================================== */
/* Train::Train — Constructor                                          */
/* Address: 0x4533D8                                                    */
/*                                                                     */
/* Calls Building::BaseCtor (shared with Building), then sets its own  */
/* vtable.                                                             */
/* ================================================================== */
Train::Train(int resource_id)
{
    /* Shared base constructor — same as Building */
    this->BaseCtor(resource_id);

    /* Override vtable to Train-specific vtable */
    /* (vtable address TBD from Ghidra — stored in .rdata) */
    this->vtable = (void**)0x00480B80;  /* Train vtable */
}


/* ================================================================== */
/* Train::scalar_deleting_destructor                                   */
/* ================================================================== */
void* Train::scalar_deleting_destructor(byte flags)
{
    /* Call base destructor for cleanup */
    this->BaseDtor();

    if (flags & 1) {
        GLOBAL_free(this);
    }

    return this;
}


/* ================================================================== */
/* Train::Deserialize — Deserialize from save data                     */
/* Address: ~0x435E00 area                                             */
/* ================================================================== */
void Train::Deserialize(void* data)
{
    /* See Train-related decompilation for full implementation */
}
