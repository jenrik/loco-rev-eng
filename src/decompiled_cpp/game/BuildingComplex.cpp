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
/* BuildingComplex::DispatchTimers — Dispatch timer events             */
/* Address: 0x434690  (size: 143 bytes)                                */
/*                                                                     */
/* Iterates timer collections at +0x4C and +0x64, calling vtable[0x40] */
/* on each item with fields at +0xA8 and +0xAC as arguments.           */
/* Only runs in game mode 3.                                           */
/* ================================================================== */
void BuildingComplex::DispatchTimers()
{
    extern int g_game_mode;
    if (g_game_mode != 3) return;

    auto dispatchCollection = [](void* timer_coll) {
        void** vt = *(void***)timer_coll;
        auto getCount = (int(__thiscall*)())vt[0x2C / 4];
        auto getItem  = (void*(__thiscall*)(int))vt[0x20 / 4];

        int count = getCount();
        for (int i = 0; i < count; i++) {
            void* item = getItem(i);
            /* vtable[0x40] = Dispatch(arg_a, arg_b) */
            void** ivt = *(void***)item;
            ((void(__thiscall*)(int,int))ivt[0x40 / 4])(
                *(int*)((uint8_t*)item + 0xA8),
                *(int*)((uint8_t*)item + 0xAC));
            count = getCount();
        }
    };

    dispatchCollection((uint8_t*)this + 0x4C);
    dispatchCollection((uint8_t*)this + 0x64);
}
