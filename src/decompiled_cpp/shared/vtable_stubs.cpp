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
    void CalcAngle();
    void CheckEdgeBounds(void*);
    void CheckEditBounds1(void*);
    void CheckEditBounds2(void*);
    void CheckVehicleAttach(void*);
};

void VehicleEditor::CalcAngle() {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — VehicleEditor::CalcAngle");
}
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
/* Building — methods not yet in canonical header              */
/* =========================================================== */
class Building {
public:
    Building(int);
    virtual ~Building() {}
    void CheckTimeout();
    void HandleAction(int);
    void OnOccupantReady(int);
    void PartyModeUpdate(void*);
    int IsActionComplete();
    void StepToward(int, int);
    void TeleportTo(int, int);
    void PostMoveDispatch();
    void BaseCleanup();
    uint8_t CheckPlacementCollision(int, int);
    uint32_t FindNearestConnectionNode(void*, unsigned int);
};

Building::Building(int) {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — Building::Building(int)");
}
void Building::CheckTimeout() {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — Building::CheckTimeout");
}
void Building::HandleAction(int) {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — Building::HandleAction");
}
void Building::OnOccupantReady(int) {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — Building::OnOccupantReady");
}
void Building::PartyModeUpdate(void*) {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — Building::PartyModeUpdate");
}
int Building::IsActionComplete() {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — Building::IsActionComplete");
    return 0;
}
void Building::StepToward(int, int) {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — Building::StepToward");
}
void Building::TeleportTo(int, int) {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — Building::TeleportTo");
}
void Building::PostMoveDispatch() {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — Building::PostMoveDispatch");
}
void Building::BaseCleanup() {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — Building::BaseCleanup");
}
uint8_t Building::CheckPlacementCollision(int, int) {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — Building::CheckPlacementCollision");
    return 1;
}
uint32_t Building::FindNearestConnectionNode(void*, unsigned int) {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — Building::FindNearestConnectionNode");
    return 0;
}

/* =========================================================== */
/* TrainEntity — constructor                                   */
/* =========================================================== */
class TrainEntity {
public:
    TrainEntity(int);
    virtual ~TrainEntity() {}
};

TrainEntity::TrainEntity(int) {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — TrainEntity::TrainEntity(int)");
}

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
/* UI_WindowBase — ctor + dtor for typeinfo generation         */
/* =========================================================== */
class UI_WindowBase {
public:
    UI_WindowBase(void*, unsigned int);
    virtual ~UI_WindowBase();
};

UI_WindowBase::UI_WindowBase(void*, unsigned int) {
    /* Host: minimal init — vtable managed by compiler */
}
UI_WindowBase::~UI_WindowBase() {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — UI_WindowBase::~UI_WindowBase");
}

/* =========================================================== */
/* UIEntity — dtor for vtable                                  */
/* =========================================================== */
class UIEntity {
public:
    virtual ~UIEntity();
};
UIEntity::~UIEntity() {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — UIEntity::~UIEntity");
}

/* =========================================================== */
/* ScriptedObject — vtable stubs                                */
/* =========================================================== */
class ScriptedObject {
public:
    virtual ~ScriptedObject() {}
    virtual void InitSubObjects() {}
    virtual void HandleAction(int) {}
    virtual void Update() {}
    void LoadFromStream(void*);
    void Init(int, int, unsigned char);
    void OnUpdateChild();
};

void ScriptedObject::LoadFromStream(void*) {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — ScriptedObject::LoadFromStream");
}
void ScriptedObject::Init(int, int, unsigned char) {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — ScriptedObject::Init");
}
void ScriptedObject::OnUpdateChild() {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — ScriptedObject::OnUpdateChild");
}

/* =========================================================== */
/* Netman — stubs                                               */
/* =========================================================== */
class Netman {
public:
    void ReceivePing(int, unsigned char, unsigned int, int, int);
    void SendFileTransfer(void*);
    void CheckTimeout(int);
    void HandleTimeout(void*);
    void ResetNetworkState();
};
void Netman::ReceivePing(int, unsigned char, unsigned int, int, int) {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — Netman::ReceivePing");
}
void Netman::SendFileTransfer(void*) {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — Netman::SendFileTransfer");
}
void Netman::CheckTimeout(int) {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — Netman::CheckTimeout");
}
void Netman::HandleTimeout(void*) {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — Netman::HandleTimeout");
}
void Netman::ResetNetworkState() {
    fprintf(stderr, "STUB: %s at %s:%d\n", __func__, __FILE__, __LINE__);
    assert(0 && "stub reached — Netman::ResetNetworkState");
}
