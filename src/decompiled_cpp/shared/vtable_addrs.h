/**
 * vtable_addrs.h — Vtable address constants for Lego Loco C++ decompilation
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * These are the concrete vtable pointers found in the .rdata section
 * and assigned during object construction. Each vtable is an array of
 * function pointers; slot 0 is always the scalar-deleting destructor
 * (MSVC convention).
 *
 * Naming: VTBL_<ClassName>[_<variant>]
 *   _BASE  = intermediate vtable set by base constructor
 *   _FULL  = final vtable set by most-derived constructor
 */

#pragma once

/* ================================================================== */
/* GameObject hierarchy                                                */
/* ================================================================== */
#define VTBL_GAMEOBJECT                0x00477820  /* GameObject root vtable      */
#define VTBL_GAMEOBJECT_SCALAR_DTOR    0x00477820  /* vtable[0] — scalar dtor     */

#define VTBL_ENTITY                    0x00477488  /* Entity vtable               */
#define VTBL_ENTITY_SCALAR_DTOR        0x00477488  /* vtable[0] — scalar dtor     */

/* ================================================================== */
/* CGWND — main game window                                            */
/* ================================================================== */
#define VTBL_CGWND                     0x004774C4  /* CGWND vtable                */

/* ================================================================== */
/* Building hierarchy                                                  */
/* ================================================================== */
#define VTBL_BUILDING_FULL             0x00477EB8  /* Building complete vtable    */
#define VTBL_BUILDING_BASE             0x00477F18  /* Building_BaseCtor vtable    */

/* ================================================================== */
/* BuildingComplex                                                     */
/* ================================================================== */
#define VTBL_BUILDING_COMPLEX          0x00478008  /* BuildingComplex vtable      */

/* ================================================================== */
/* Graphics — LOCOBITMAP variants                                      */
/* ================================================================== */
#define VTBL_LOCOBITMAP                0x004773E8  /* LOCOBITMAP vtable A         */
#define VTBL_LOCOBITMAP_ALT            0x004773F0  /* LOCOBITMAP vtable B         */

/* ================================================================== */
/* Additional vtables (addresses documented, class names TBD)          */
/* ================================================================== */
#define VTBL_0047703C                  0x0047703C
#define VTBL_00477044                  0x00477044
#define VTBL_00477048                  0x00477048
#define VTBL_00477058                  0x00477058
#define VTBL_00477060                  0x00477060
#define VTBL_00477064                  0x00477064
#define VTBL_00477070                  0x00477070
#define VTBL_00477078                  0x00477078
#define VTBL_004770A0                  0x004770A0
#define VTBL_004770C4                  0x004770C4
#define VTBL_0047726C                  0x0047726C
#define VTBL_00477290                  0x00477290
#define VTBL_004772A4                  0x004772A4
#define VTBL_004772A8                  0x004772A8
#define VTBL_004772AC                  0x004772AC
#define VTBL_004772D4                  0x004772D4
#define VTBL_004772F4                  0x004772F4
#define VTBL_004772F8                  0x004772F8
#define VTBL_004772FC                  0x004772FC
#define VTBL_00477300                  0x00477300
#define VTBL_00477340                  0x00477340
#define VTBL_00477348                  0x00477348
#define VTBL_0047734C                  0x0047734C
#define VTBL_00477354                  0x00477354
#define VTBL_00477358                  0x00477358
#define VTBL_00477364                  0x00477364
#define VTBL_00477368                  0x00477368
#define VTBL_0047736C                  0x0047736C
#define VTBL_00477374                  0x00477374
#define VTBL_00477394                  0x00477394
#define VTBL_004773AC                  0x004773AC
#define VTBL_004773B4                  0x004773B4
#define VTBL_00477564                  0x00477564
#define VTBL_00477894                  0x00477894
#define VTBL_00477A90                  0x00477A90
#define VTBL_00477B40                  0x00477B40
#define VTBL_00477BD0                  0x00477BD0
#define VTBL_00477D28                  0x00477D28
#define VTBL_00477F70                  0x00477F70
#define VTBL_00477F88                  0x00477F88
#define VTBL_00477FE0                  0x00477FE0
