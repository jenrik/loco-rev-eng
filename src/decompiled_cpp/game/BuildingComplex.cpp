/**
 * BuildingComplex.cpp — Complex building class implementation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * See src/decompiled/buildingcomplex_*.c for full C decompilations.
 */

#include "BuildingComplex.h"
#include "../shared/vtable_addrs.h"

extern void GLOBAL_free(void* ptr);
extern void Building_BaseDtor_raw(void* building);         /* 0x432740 */


/* ================================================================== */
/* BuildingComplex::BuildingComplex — Constructor                      */
/* Address: 0x437EA0                                                   */
/*                                                                     */
/* Calls Building::BaseCtor, then overrides vtable to 0x478008.       */
/* See src/decompiled/buildingcomplex_ctor.c for full details.         */
/* ================================================================== */
BuildingComplex::BuildingComplex(int resource_id)
{
    /* Call shared Building base constructor */
    this->BaseCtor(resource_id);

    /* Override vtable to BuildingComplex (0x478008) */
    this->vtable = (void**)VTBL_BUILDING_COMPLEX;
}


/* ================================================================== */
/* BuildingComplex::BaseDtor — Base destructor                         */
/* Address: 0x437E60                                                   */
/*                                                                     */
/* Restores vtable, then delegates to Building::BaseDtor.              */
/* See src/decompiled/buildingcomplex_basedtor.c for full details.     */
/* ================================================================== */
void BuildingComplex::BaseDtor()
{
    this->vtable = (void**)VTBL_BUILDING_COMPLEX;
    Building_BaseDtor_raw(this);                    /* 0x432740 */
}


/* ================================================================== */
/* BuildingComplex::scalar_deleting_destructor — Vtable[0]             */
/* Address: 0x437E80                                                   */
/*                                                                     */
/* Calls BaseDtor, then frees memory if flags & 1.                     */
/* See src/decompiled/buildingcomplex_dtor.c for full details.         */
/* ================================================================== */
void* BuildingComplex::scalar_deleting_destructor(byte flags)
{
    this->BaseDtor();
    if (flags & 1) {
        GLOBAL_free(this);
    }
    return this;
}


/* ================================================================== */
/* BuildingComplex::DispatchTimers — Timer dispatch across tiles       */
/* Address: 0x438070                                                   */
/*                                                                     */
/* See src/decompiled/buildingcomplex_dispatchtimers.c for full details */
/* ================================================================== */
void BuildingComplex::DispatchTimers()
{
    /* See src/decompiled/buildingcomplex_dispatchtimers.c (0x438070) */
}
