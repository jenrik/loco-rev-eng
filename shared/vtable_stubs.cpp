/**
 * vtable_stubs.cpp — Out-of-line stub definitions for missing vtable entries
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Status: STUB — TRANSITIONAL
 * Partial class definitions provide vtable entries not yet declared in
 * canonical headers. Each class will be removed once its canonical header
 * is complete and the corresponding TU compiles. Tracked in PROGRESS.md.
 *
 * TODO (CLASS-001): Remove duplicate partial class definitions when
 * canonical headers declare all needed virtual methods.
 */

#include <cstdint>
#include <cstdio>
#include <cassert>

/* =========================================================== */
/* VehicleEditor — methods not yet in canonical header         */
/* =========================================================== */
class VehicleEditor {
public:
    /* CalcAngle() removed (LINK-001): real body now in
     * core/VehicleEditor.cpp (0x?), same signature — collided. */
    void CheckEdgeBounds(void*);
    void CheckEditBounds1(void*);
    void CheckEditBounds2(void*);
    void CheckVehicleAttach(void*);
};

void VehicleEditor::CheckEdgeBounds(void*) {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — VehicleEditor::CheckEdgeBounds");
}
void VehicleEditor::CheckEditBounds1(void*) {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — VehicleEditor::CheckEditBounds1");
}
void VehicleEditor::CheckEditBounds2(void*) {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — VehicleEditor::CheckEditBounds2");
}
void VehicleEditor::CheckVehicleAttach(void*) {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — VehicleEditor::CheckVehicleAttach");
}

/* =========================================================== */
/* RESDATA_ScriptedObject — not yet in canonical header        */
/* =========================================================== */
class RESDATA_ScriptedObject {
public:
    void EnterBuildMode(unsigned char);
};

void RESDATA_ScriptedObject::EnterBuildMode(unsigned char) {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — RESDATA_ScriptedObject::EnterBuildMode");
}

/* =========================================================== */
/* Building / TrainEntity — REMOVED (CLASS-001).                */
/*                                                                */
/* These blocks used to be duplicate shadow `class Building {   */
/* ... };` / `class TrainEntity { ... };` definitions providing  */
/* assert-stub bodies for the ctor and most methods. Same ODR    */
/* hazard as UIEntity above: mangled names don't encode field    */
/* layout, so these collided with the real, complete             */
/* `Building : public Entity` (game/Building.h/.cpp) and          */
/* `TrainEntity` (game/Train.h/.cpp) — both fully integrated,      */
/* every method here now has a real body there (LINK-001).       */
/* =========================================================== */

/* =========================================================== */
/* Collection — GetAt                                          */
/* =========================================================== */
class Collection {
public:
    virtual ~Collection() {}
    virtual void* GetAt(int);
};

void* Collection::GetAt(int) {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — Collection::GetAt");
    return nullptr;
}

/* =========================================================== */
/* SortedCollection — Compare + SortRange                       */
/* =========================================================== */
class SortedCollection : Collection {
public:
    virtual int Compare(void*, void*);
    virtual void SortRange(int, int);
};

int SortedCollection::Compare(void*, void*) {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — SortedCollection::Compare");
    return 0;
}
void SortedCollection::SortRange(int, int) {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — SortedCollection::SortRange");
}

/* =========================================================== */
/* UI_WindowBase — REMOVED (CLASS-001), same ODR hazard as       */
/* UIEntity below: the real ctor/dtor are in ui/UI_WindowBase.cpp */
/* (LINK-001).                                                    */
/* =========================================================== */

/* =========================================================== */
/* UIEntity — REMOVED (CLASS-001).                              */
/*                                                                */
/* This block used to be a duplicate `class UIEntity { virtual   */
/* ~UIEntity(); };` shadow definition providing an assert-stub   */
/* destructor. It was an ODR violation: ui/UIEntity.h already    */
/* declares the real, complete `UIEntity : public Entity` class, */
/* and because C++ name mangling does not encode a class's field */
/* layout, both definitions produced the exact same mangled      */
/* `UIEntity::~UIEntity()` symbol — whichever translation unit   */
/* the linker picked "won" for every caller, regardless of which */
/* one they meant. The real destructor is now implemented for    */
/* real in ui/UIEntity.cpp (address 0x423500 body / 0x4234E0     */
/* scalar-deleting wrapper; see ui/UIEntity.h). Removed rather    */
/* than left stubbed, per this file's own CLASS-001 policy.      */
/* =========================================================== */

/* =========================================================== */
/* ScriptedObject — vtable stubs                                */
/* =========================================================== */
class ScriptedObject {
public:
    virtual ~ScriptedObject() {}
    virtual void InitSubObjects() {}
    virtual void HandleAction(int) {}
    virtual void Update() {}
    void Init(int, int, unsigned char);
    /* LoadFromStream(void*)/OnUpdateChild() removed (LINK-001): real
     * bodies now in game/ScriptedObject.cpp (0-arg OnUpdateChild, and
     * LoadFromStream(void*) — collided, same ODR hazard as UIEntity). */
};

void ScriptedObject::Init(int, int, unsigned char) {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — ScriptedObject::Init");
}

/* =========================================================== */
/* Netman — stubs                                               */
/* =========================================================== */
class Netman {
public:
    void SendFileTransfer(void*);
    void HandleTimeout(void*);
    /* ReceivePing/CheckTimeout/ResetNetworkState removed (LINK-001): real
     * bodies now in network/Netman.cpp -- collided, same ODR hazard as
     * UIEntity. SendFileTransfer(void*)/HandleTimeout(void*) below don't
     * collide (real Netman.cpp takes TrainMessage or InboundTrainNode
     * pointers, a different mangled overload) but are almost certainly
     * equally dead; left alone, out of LINK-001's scope. */
};
void Netman::SendFileTransfer(void*) {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — Netman::SendFileTransfer");
}
void Netman::HandleTimeout(void*) {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — Netman::HandleTimeout");
}
