/**
 * EditorState.h — Per-endpoint track editor state machine
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Status: TRANSCRIBED
 *
 * EditorState tracks the position of one endpoint of a track segment during
 * vehicle route editing. It manages world position, track index, parent
 * building link, and the placement state machine used by VehicleEditor.
 *
 * Size: 0x20 bytes
 * Vtable: 0x477564
 *
 * Vtable layout:
 *   [0] +0x00: scalar deleting destructor (EditorState::~EditorState, 0x40B550)
 */

#pragma once

#include "../shared/types.h"

/* Forward declarations */
class Vehicle;
class VehicleEditor;
class Building;
class Entity;

/**
 * EditorState — Per-endpoint track placement state
 *
 * Tracks one end of a track segment being placed. Owns:
 *   - Position and track index on the parent building
 *   - Reference to the parent building (with refcounting at +0x114)
 *   - Placement state machine controlling track-follow, bounds-wrap,
 *     and edit-mode transitions
 *
 * Two EditorState objects are owned by VehicleEditor (end_a at +0x430,
 * end_b at +0x434), one for each end of the track segment.
 */
class EditorState {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */

    /* vtable at +0x00 is compiler-managed via virtual destructor */
    int32_t   direction;          // +0x04  0 = backward, 1 = forward
    int32_t   track_pos;          // +0x08  index into building control point array
    int32_t   pos_x;              // +0x0C  world X position (pixels), snapped to track
    int32_t   pos_y;              // +0x10  world Y position (pixels), snapped to track
    Building* building;           // +0x14  pointer to associated Building/track owner
    int32_t   move_state;         // +0x18  0=idle, 1=track-follow, 2=bounds-wrap, 4=edit-mode
    int32_t   edit_state;         // +0x1C  0=normal, 1=bridge-edge, 2=wrap, 4=edit-edit, 5=edit-edge

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    EditorState(char viewport_side);                     /* 0x40B500 */
    virtual ~EditorState();                              /* 0x40B550 (vtable[0]) */

    /* ================================================================ */
    /* Core methods (Addresses: 0x40B5A0–0x40B610)                       */
    /* ================================================================ */

    void     Detach();                                   /* 0x40B5A0 */
    void     Copy(const EditorState* src);               /* 0x40B5D0 */
    int      FindTrackPosition(int pixel_x, int pixel_y); /* 0x40B610 */
    uint32_t InitTrackAtPosition(int pixel_x, int pixel_y); /* 0x40B740 */

    /* ================================================================ */
    /* Track connectivity (Address: 0x40B880)                            */
    /* ================================================================ */
    Building* FindAdjacentTrack();                       /* 0x40B880 — NULL on success */

    /* ================================================================ */
    /* Placement algorithms (Addresses: 0x40BBD0–0x40C580)               */
    /* ================================================================ */
    uint32_t UpdateVehiclePlacement(Vehicle* vehicle);   /* 0x40BBD0 */
    uint32_t UpdatePosition(Vehicle* vehicle, VehicleEditor* ve); /* 0x40C580 */

    /* ================================================================ */
    /* Vehicle attachment (Addresses: 0x40C3D0–0x40C460)                 */
    /* ================================================================ */
    uint8_t  TryAttach(Vehicle* vehicle);                /* 0x40C3D0 */
    uint8_t  HandleDirection(Vehicle* vehicle, Building* train); /* 0x40C460 */

    /* ================================================================ */
    /* Edge scrolling & bounds (Addresses: 0x40CB10–0x40CC90)            */
    /* ================================================================ */
    uint32_t ScrollEdge();                               /* 0x40CB10 */
    void     CheckBounds();                              /* 0x40CC20 */
    void     CheckBounds2();                             /* 0x40CC90 */

    /* ================================================================ */
    /* Edit mode state machine (Address: 0x40CD60)                       */
    /* ================================================================ */
    void     UpdateEditMode(Vehicle* vehicle);            /* 0x40CD60 */
};
