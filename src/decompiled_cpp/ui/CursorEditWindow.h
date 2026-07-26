/**
 * CursorEditWindow.h — Cursor editing/tool window
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * CursorEditWindow is a child window that loads cursor data (a .dat file
 * containing cursor metrics + a .bmp file containing cursor pixel data)
 * from the game installation directory or AssetMgr archive. It is created
 * as a resource-manager child window (via ResourceManager_AddString) and
 * renders the loaded cursor via UI_ChildWindow_Render.
 *
 * The cursor filename is derived from the resource string parameter.
 * For example, resource ID 0x1863 might correspond to "ArrowCursor",
 * yielding "ArrowCursor.dat" and "ArrowCursor.bmp".
 *
 * Inheritance:
 *   ChildWindow (0x168 bytes, vtable 0x477C18)
 *     +-- CursorEditWindow (+0x644 bytes subclass data)
 *
 * Size: 0x7AC bytes
 * Vtable: 0x477610
 *
 * Vtable layout (extends ChildWindow vtable):
 *   [0] +0x00: scalar deleting destructor (CGWND_CursorEditWindow_Dtor, 0x40E660)
 *   [1] +0x04: unknown (inherited or overridden)
 *   [2] +0x08: unknown (inherited or overridden)
 *   [3] +0x0C: loadCursorData (virtual — processes a stream; returns byte success)
 *
 * Called by: ResourceManager_AddString @ 0x446A55 (alloc 0x7AC, ctor)
 */

#pragma once

#include "../shared/types.h"
/* vtable addresses in vtable_addrs.h — compiler manages vtables via virtual methods */
class CursorEditWindow {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */

    /* ---- Inherited from ChildWindow (0x00..0x167) ---- */
/* vtable at +0x00 is compiler-managed */
    int32_t    resourceId;             // +0x04  Resource ID (set by ChildWindow::Create)
    int32_t    resourceType;           // +0x08  Resource type byte (set by ChildWindow::Create)
    void*      streamData;             // +0x0C  Stream data object (used by Cleanup)
    int32_t    childObj;               // +0x10  Sub-object pointer (released via vtable[0] in dtor)
    int16_t    field_14;               // +0x14  (init 0)
    int16_t    field_16;               // +0x16  (init 0)
    uint8_t    field_18;               // +0x18  (byte, init 0, set to 1 for some resource types)
    int16_t    field_1A;               // +0x1A  (init 0)
    int16_t    field_1C;               // +0x1C  (init 0)
    int16_t    field_1E;               // +0x1E  (init 0)
    void*      heapBuf;                // +0x20  Heap-allocated buffer (freed in dtor)
    int32_t    field_24;               // +0x24  Sub-object pointer (released via vtable[0] in dtor)
    int16_t    field_28;               // +0x28  (init 0)
    int16_t    field_2A;               // +0x2A  (init 0)
    int16_t    field_2C;               // +0x2C  (init 0)
    /* +0x2E..+0x37: gap/padding */
    int32_t    field_38;               // +0x38  (init 0 in Create)
    int32_t    field_3C;               // +0x3C  (init 0 in Create)
    int32_t    field_40;               // +0x40  (init -1 in Create)
    int32_t    field_44;               // +0x44  (init -1 in Create)

    /* Cursor paths: built by Create and Init */
    char       bmpPath[280];           // +0x48  .bmp file path buffer (sprintf'd during init)

    /* +0x160..+0x167: ChildWindow flags */
    int16_t    field_160;              // +0x160 (init 1 in Create)
    uint8_t    loaded;                 // +0x162 Cursor data loaded flag (0=failed, 1=loaded)
    uint8_t    field_163;              // +0x163 (init 1 in Create)
    int32_t    field_164;              // +0x164 (init 0 in Create)

    /* ---- CursorEditWindow-specific fields (0x168..0x7AB) ---- */
    /* (Mostly unknown, large gap — the init sets only two shorts at
        +0x7A8 and +0x7AA to 0) */

    int16_t    field_7A8;              // +0x7A8  (short, init 0 by Init)
    int16_t    field_7AA;              // +0x7AA  (short, init 0 by Init)

    /* Total: 0x7AC bytes */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * CursorEditWindow constructor.
     * Address: 0x40E600
     *
     * Chains to ChildWindow constructor (UI_CreateChildWindow), sets
     * vtable to 0x477610, then calls Init() to load cursor data.
     * Allocation size: 0x7AC bytes.
     *
     * Called by: ResourceManager_AddString @ 0x446A55
     *
     * @param resourceId  Resource ID (unique ID for this cursor)
     * @param nameParam   Non-zero = load cursor data from disk/asset mgr
     */
    CursorEditWindow(uint32_t resourceId, int32_t nameParam);

    /**
     * Scalar deleting destructor (vtable[0]).
     * Address: 0x40E660
     *
     * Calls BaseDtor to release resources, then optionally frees
     * the heap allocation.
     *
     * @param flags  Delete flag (bit 0 = free heap memory)
     */
    virtual ~CursorEditWindow();

    /* ================================================================ */
    /* Methods                                                           */
    /* ================================================================ */

    /**
     * Base destructor — Release ChildWindow resources.
     * Address: 0x40E680
     *
     * Resets vtable to 0x477610 and calls UI_ChildWindow_Dtor to
     * release sub-objects at +0x10, buffer at +0x20, and sub-object
     * at +0x24.
     */
    void base_destructor();

    /**
     * Init — Initialize and load cursor data.
     * Address: 0x40E690
     *
     * Builds filenames from the resource name parameter:
     *   sprintf(bmpPath, "%s\\%s.bmp", install_path, nameParam)
     *   sprintf(datPath, "%s\\%s.dat", install_path, nameParam)
     *
     * Attempts to load the .dat file via AssetMgr first, then falls
     * back to direct file open. Validates the data via vtable[3]
     * (loadCursorData), then renders via UI_ChildWindow_Render.
     * Stores success/failure in loaded flag (+0x162).
     *
     * @param resourceId  Resource ID
     * @param nameParam   Non-zero to load; zero for no-op initialization
     */
    void init(uint32_t resourceId, int32_t nameParam);

    /**
     * cleanup — Clean up stream resources.
     * Address: 0x40E8B0
     *
     * Destroys the stream at +0x0C and cleans up the WNDPROC stream
     * state. Called externally to release stream resources without
     * destroying the entire window.
     */
    void cleanup();

    /**
     * loadCursorData — Virtual method (vtable[3]) for loading data
     *                  from a stream.
     * Address: (varies per class, dispatched through vtable)
     */
    virtual byte loadCursorData(void* stream);
};

/* Vtable address */
#define VTBL_CURSOREDITWINDOW 0x00477610
