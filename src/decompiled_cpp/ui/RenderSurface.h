/**
 * RenderSurface.h — Minimal typed base class for offscreen surface objects
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * RenderSurface is a base class for vtable-based offscreen render surfaces
 * used throughout the UI subsystem. The binary releases these objects via
 * vtable[2] (Release). This header provides a minimal typed declaration
 * so that GameSetupPanel, UI_WindowBase, and other UI classes can use
 * typed pointers instead of void*.
 *
 * Vtable layout (partial — only documented slots):
 *   [0] +0x00: scalar deleting destructor
 *   [1] +0x04: (unknown)
 *   [2] +0x08: Release — releases resources / decrements refcount
 *
 * TODO: Full decompilation of the surface class. Currently only the Release
 *       pattern (vtable[2]) is confirmed. Additional virtual methods and
 *       field layout need Ghidra verification.
 */

// Status: STUB — minimal typed declaration for anti-pattern removal

#pragma once

#include "../shared/types.h"

class RenderSurface {
public:
    virtual ~RenderSurface() {}      // [0] scalar deleting destructor
    virtual void Unknown1() {}        // [1] placeholder — real method unknown
    virtual void Release() = 0;       // [2] release resources / decrement refcount
};
