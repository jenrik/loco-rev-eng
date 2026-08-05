/**
 * Compatibility header for the retired Ghidra name "BuildingComplex".
 * loco_v8 proves that 0x434500 constructs BuildingMgr itself; there is no
 * BuildingComplex base/derived type in this hierarchy.
 */
#pragma once
#include "BuildingMgr.h"

// Status: TRANSCRIBED
using BuildingComplex = BuildingMgr;
