// Status: INTEGRATED
/**
 * GameView.cpp — GameView lifecycle (viewport scrolling helper)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation (database locon).
 *
 * The compiler owns the GameView vtable; the original constructor,
 * destructor body, and cleanup were verified instruction-by-instruction
 * against the binary (see GameView.h for the 20-slot vtable layout).
 */

#include "GameView.h"

/* Panel-family helpers — 0x4544E0 (Panel base init) and 0x454630
 * (Panel partial destructor / RESDATA base cleanup). */
extern void __fastcall RESDATA_BaseInit(void* self);   /* 0x4544E0 */
extern void __fastcall RESDATA_DtorBase(void* self);   /* 0x454630 */

/**
 * GameView::GameView — constructor
 * Address: 0x42CCE0
 *
 * The binary runs RESDATA_BaseInit (0x4544E0) and the embedded Entity
 * constructor (0x405790) before this class's own field writes.  In
 * natural C++ the Panel base and the Entity member are constructed
 * first (compiler-managed vtables), so those two calls are represented
 * by the base/member construction and the body below writes exactly the
 * recovered fields: type=0x0E, scroll_x=0, +0xAD active flag=1,
 * child_resource=nullptr.
 */
GameView::GameView() : game_object_sub(-1, -1, 0, 0)
{
    this->type = 0x0E;            /* +0x04 (GameObject::type) */
    this->scroll_x = 0;           /* +0xE0 */
    this->dim_flag = 1;           /* +0xAD active/dim flag */
    this->child_resource = nullptr;   /* +0x17C */
}

/**
 * GameView::~GameView — destructor body
 * Address: 0x42CD80 (scalar deleting wrapper at 0x42CD60 is
 * compiler-generated and not reconstructed here)
 *
 * The binary destroys the embedded Entity (GameObject_DtorBody at
 * +0xE4) then runs Panel_DtorBody (0x4545A0).  In natural C++ the
 * Panel base destructor runs after this body and the Entity member is
 * destroyed after the base chain; the resources are independent, so
 * the order swap is behavior-neutral.
 */
GameView::~GameView()
{
    /* Panel::~Panel → Panel::DtorBody (0x4545A0) → GameObject_DtorBody;
     * the embedded Entity member is destroyed afterwards. */
}

/**
 * GameView::cleanup
 * Address: 0x42CDD0 — vtable [15] (+0x3C)
 *
 * 1. Destroys the child resource via vtable[0] with flag 1 (the
 *    original does NOT clear the pointer afterwards).
 * 2. Resets the embedded Entity via its vtable[6] (InitBase, 0x405900)
 *    with (0, -1, 0).
 * 3. Resets self via vtable[6] (Panel::Init, 0x454680) with (0, -1, 0).
 * 4. Runs RESDATA_DtorBase (0x454630).
 */
void GameView::cleanup()
{
    if (this->child_resource != nullptr) {
        this->child_resource->Destroy(1);
    }

    this->game_object_sub.InitBase(0, -1, false);
    this->Init(0, -1, false);
    RESDATA_DtorBase(this);
}
