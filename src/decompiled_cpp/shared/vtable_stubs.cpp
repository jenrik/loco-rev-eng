/**
 * vtable_stubs.cpp — Out-of-line method definitions for missing vtable entries
 *
 * Uses forward declarations with the EXACT class names from the binary
 * to generate correct C++ mangled names. One method per class, defined
 * out-of-line to avoid full class redefinition.
 *
 * Win32/DirectX shims excluded — handled by sdl3_shims.
 */

#include <cstdint>

/* Forward declare classes with just enough to define the methods */
/* These names MUST match the binary / existing headers exactly. */

namespace {
    /* Placeholder types for method params */
    typedef void* HWND;
    struct RECT { int32_t left, top, right, bottom; };
}

/* =========================================================== */
/* VehicleEditor methods                                        */
/* =========================================================== */
class VehicleEditor {
public:
    void CalcAngle();
    void CheckEdgeBounds(void*);
    void CheckEditBounds1(void*);
    void CheckEditBounds2(void*);
    void CheckVehicleAttach(void*);
};

void VehicleEditor::CalcAngle() {}
void VehicleEditor::CheckEdgeBounds(void*) {}
void VehicleEditor::CheckEditBounds1(void*) {}
void VehicleEditor::CheckEditBounds2(void*) {}
void VehicleEditor::CheckVehicleAttach(void*) {}

/* =========================================================== */
/* GameSetupPanel methods                                       */
/* =========================================================== */
class GameSetupPanel {
public:
    void HandleMapClick(int, int);
    void SelectLayoutEntry(int);
    void SendScenarioSelect(int);
    void ConnectToNetworkGame(int);
};

void GameSetupPanel::HandleMapClick(int, int) {}
void GameSetupPanel::SelectLayoutEntry(int) {}
void GameSetupPanel::SendScenarioSelect(int) {}
void GameSetupPanel::ConnectToNetworkGame(int) {}

/* =========================================================== */
/* HelpWnd stubs removed — now defined in ui/HelpWnd.cpp       */
/* with TODO: decompile annotations and address tracking.      */
/* =========================================================== */

/* =========================================================== */
/* RESDATA_ScriptedObject method                                */
/* =========================================================== */
class RESDATA_ScriptedObject {
public:
    void EnterBuildMode(unsigned char);
};

void RESDATA_ScriptedObject::EnterBuildMode(unsigned char) {}

/* =========================================================== */
/* Building::Building(int) — constructor                        */
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

Building::Building(int) {}

/* =========================================================== */
/* TrainEntity::TrainEntity(int) — constructor                  */
/* =========================================================== */
class TrainEntity {
public:
    TrainEntity(int);
    virtual ~TrainEntity() {}
};

TrainEntity::TrainEntity(int) {}

/* =========================================================== */
/* Collection::GetAt(int)                                       */
/* =========================================================== */
class Collection {
public:
    virtual ~Collection() {}
    virtual void* GetAt(int);
};

void* Collection::GetAt(int) { return 0; }

/* =========================================================== */
/* SortedCollection::Compare + SortRange                        */
/* =========================================================== */
class SortedCollection : Collection {
public:
    virtual int Compare(void*, void*);
    virtual void SortRange(int, int);
};

int SortedCollection::Compare(void*, void*) { return 0; }
void SortedCollection::SortRange(int, int) {}

/* =========================================================== */
/* UI_WindowBase — ctor + dtor + typeinfo + vtable              */
/*   This MUST be a complete enough class to generate typeinfo   */
/* =========================================================== */
class UI_WindowBase {
public:
    UI_WindowBase(void*, unsigned int);
    virtual ~UI_WindowBase();
};

UI_WindowBase::UI_WindowBase(void*, unsigned int) {}
UI_WindowBase::~UI_WindowBase() {}

/* =========================================================== */
/* UIEntity — vtable only                                       */
/* =========================================================== */
class UIEntity {
public:
    virtual ~UIEntity();
};
UIEntity::~UIEntity() {}

/* =========================================================== */
/* IDirectDrawSurface4 — constructor + destructor                */
/* =========================================================== */
class IDirectDrawSurface4 {
public:
    IDirectDrawSurface4();
    virtual ~IDirectDrawSurface4();
};

/* =========================================================== */
/* ScriptedObject — vtable generation                           */
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
/* =========================================================== */
/* Building virtual method stubs (Building.cpp is broken)       */
/* =========================================================== */
void Building::CheckTimeout() {}
void Building::HandleAction(int) {}
void Building::OnOccupantReady(int) {}
void Building::PartyModeUpdate(void*) {}
int Building::IsActionComplete() { return 0; }
void Building::StepToward(int, int) {}
void Building::TeleportTo(int, int) {}
void Building::PostMoveDispatch() {}



IDirectDrawSurface4::IDirectDrawSurface4() {}
IDirectDrawSurface4::~IDirectDrawSurface4() {}
/* ScriptedObject */
void ScriptedObject::LoadFromStream(void*) {}

/* Building (more stubs) */
void Building::BaseCleanup() {}
uint8_t Building::CheckPlacementCollision(int, int) { return 1; }
uint32_t Building::FindNearestConnectionNode(void*, unsigned int) { return 0; }



/* =========================================================== */
/* Netman stub                                                 */
/* =========================================================== */
class Netman {
public:
    void ReceivePing(int, unsigned char, unsigned int, int, int);
    void SendFileTransfer(void*);
    void CheckTimeout(int);
    void HandleTimeout(void*);
    void ResetNetworkState();
};
void Netman::ReceivePing(int, unsigned char, unsigned int, int, int) {}


void Netman::SendFileTransfer(void*) {}

void Netman::CheckTimeout(int) {}


void ScriptedObject::Init(int, int, unsigned char) {}
void ScriptedObject::OnUpdateChild() {}
void Netman::HandleTimeout(void*) {}

void Netman::ResetNetworkState() {}
