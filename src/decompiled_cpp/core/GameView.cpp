/**
 * GameView.cpp — GameView lifecycle and viewport helper
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 *
 * The compiler owns the GameView vtable.  The local ABI views below are
 * deliberately typed descriptions of the recovered foreign vtables; they
 * contain no executable vtable reads or writes.
 */

#include "GameView.h"
#include "Entity.h"

/* Original helper entry points.  Their conventions are determined by the
 * calls at 0x42CCFD, 0x42CD18, 0x42CDBB, and 0x42CE07 respectively. */
extern void __fastcall RESDATA_BaseInit(void* self);              /* 0x4544E0 */
extern void __thiscall GameObject_BaseCtor(void* self, int x, int y,
                                            int width, int height); /* 0x405790 */
extern void __fastcall Panel_DtorBody(void* self);                /* 0x4545A0 */
extern void __fastcall RESDATA_DtorBase(void* self);               /* 0x454630 */

namespace {

/* ResourceData slots 0..2, as recovered from the resource calls in
 * Entity::~Entity and Entity::InitBase. */
struct ResourceObjectView {
    virtual void* destroy(uint8_t flags) = 0;       /* slot 0 */
    virtual void* acquire_surface(int x, int y) = 0;/* slot 1 */
    virtual void release_surface() = 0;             /* slot 2 */

protected:
    ~ResourceObjectView() = default;
};

/* The embedded object at GameView+0xE4 is constructed by 0x405790 and has
 * the Entity/GameObject slot layout.  Only slot 6 is used by Cleanup(). */
struct EmbeddedEntityView {
    virtual void* destroy(uint8_t flags) = 0;       /* slot 0 */
    virtual void invalidate_rect() = 0;             /* slot 1 */
    virtual uint8_t hit_test(int, int) = 0;         /* slot 2 */
    virtual void move_to(int, int) = 0;             /* slot 3 */
    virtual BOOL callback_one(int, int) = 0;        /* slot 4 */
    virtual BOOL callback_two(int, int) = 0;        /* slot 5 */
    virtual int initialize(int, int, bool) = 0;     /* slot 6 */

protected:
    ~EmbeddedEntityView() = default;
};

/* GameView is a Panel-derived object in the binary.  Its cleanup path calls
 * the inherited Init slot at +0x18 (vtable slot 6), after the embedded
 * Entity has been reset. */
struct GameViewDispatchView {
    virtual void* destroy(uint8_t flags) = 0;       /* slot 0 */
    virtual void slot1() = 0;
    virtual uint8_t slot2(int, int) = 0;
    virtual void slot3(int, int) = 0;
    virtual uint8_t slot4(int, int) = 0;
    virtual void slot5() = 0;
    virtual int initialize(int, int, bool) = 0;     /* slot 6 */

protected:
    ~GameViewDispatchView() = default;
};

static EmbeddedEntityView* embedded_entity(GameView* view)
{
    return reinterpret_cast<EmbeddedEntityView*>(view->game_object_sub);
}

static ResourceObjectView* child_resource_view(GameView* view)
{
    return reinterpret_cast<ResourceObjectView*>(view->child_resource);
}

} // namespace

/**
 * GameView::GameView — constructor
 * Address: 0x42CCE0
 *
 * Ghidra shows RESDATA_BaseInit, the Entity constructor 0x405790 at +0xE4,
 * type 0x0E, +0xE0 = 0, +0xAD = 1, and +0x17C = 0.  C++ emits the final
 * GameView vptr instead of reproducing the original literal store.
 */
GameView::GameView()
{
    RESDATA_BaseInit(this);
    GameObject_BaseCtor(this->game_object_sub, -1, -1, 0, 0);

    this->type = 0x0E;
    this->scroll_x = 0;
    this->scroll_active_flag = 1;
    this->child_resource = nullptr;
}

/**
 * GameView::~GameView — destructor body
 * Address: 0x42CD80
 *
 * The scalar deleting wrapper at 0x42CD60 is compiler-generated and is not
 * reconstructed here.  This body releases the embedded Entity and then the
 * Panel base cleanup, matching the two calls in the original function.
 */
GameView::~GameView()
{
    /* The embedded object was constructed by the recovered Entity
     * constructor at 0x405790; invoke its real C++ destructor body rather
     * than spelling the scalar-deleting slot and its flag. */
    reinterpret_cast<Entity*>(this->game_object_sub)->~Entity();
    Panel_DtorBody(this);
}

/**
 * GameView::cleanup
 * Address: 0x42CDD0
 *
 * The original calls the child scalar-destructor slot with flag 1, resets
 * both the embedded Entity and the GameView through their typed Init slots,
 * then calls RESDATA_DtorBase.
 */
void GameView::cleanup()
{
    if (this->child_resource != nullptr) {
        child_resource_view(this)->destroy(1);
        this->child_resource = nullptr;
    }

    embedded_entity(this)->initialize(0, -1, false);
    reinterpret_cast<GameViewDispatchView*>(this)->initialize(0, -1, false);
    RESDATA_DtorBase(this);
}
