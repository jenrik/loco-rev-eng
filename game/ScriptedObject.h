/**
 * ScriptedObject.h — Interactive scripted object class (extends Panel)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * Status: TRANSCRIBED
 *
 * ScriptedObject is the interactive panel object placed in the game world.
 * It manages an embedded Entity sub-object (sprite), a ScriptEngine (scripting),
 * and a ScrollPanel (scrollable UI). It supports drag-based movement,
 * tool click handling, hit testing, Enter/Escape key dispatch, and
 * loading .dat script files for child objects.
 *
 * The object has a 4-state mode machine at +0x740:
 *   0 = idle/hover (default, waiting for interaction)
 *   1 = in-world (build mode entry, bounds clamped)
 *   2 = exiting build mode (transitioning out)
 *   3 = placed/active (fully placed in world)
 *
 * Size: 0x74C bytes
 * Vtable: 0x4782A8 (VTBL_SCRIPTED_OBJECT)
 *
 * Class hierarchy:
 *   GameObject
 *     └─ Entity
 *          └─ Panel
 *               └─ ScriptedObject  ← this class
 *
 * Vtable layout (0x4782A8, 23 slots):
 *   [0]  +0x00: scalar deleting destructor    (0x4494C0)  compiler-generated
 *   [1]  +0x04: OnUpdateChild                 (0x454890)  Panel::UpdateChild
 *   [2]  +0x08: IsDragging                    (0x449CE0)  override
 *   [3]  +0x0C: MoveTo                        (0x449DC0)  override
 *   [4]  +0x10: HitTest                       (0x44A0C0)  override
 *   [5]  +0x14: (inherited from Panel)
 *   [6]  +0x18: Init                          (0x454680)  Panel::Init
 *   [7]  +0x1C: StopSound                     (0x405A20)  Entity::StopSound
 *   [8]  +0x20: SetVisible                    (0x4061B0)  Entity::SetVisible
 *   [9]  +0x24: Update                        (0x4497A0)  override
 *   [10] +0x28: Draw                          (0x454900)  Panel::Draw (override of Entity::Draw)
 *   [11] +0x2C: DrawConnected                 (0x405FD0)  Entity::DrawConnected
 *   [12] +0x30: SetName                       (0x405E20)  Entity::SetName
 *   [13] +0x34: SetAnimState                  (0x405A50)  Entity::SetAnimState
 *   [14] +0x38: Shutdown                      (0x4495B0)  override
 *   [15] +0x3C: InitState                     (0x44ADF0)  override
 *   [16] +0x40: HandleToolClick               (0x44A250)  override
 *   [17] +0x44: (unknown, shared by [18])     (0x44EF00)
 *   [18] +0x48: (unknown, shared by [17])     (0x44EF00)
 *   [19] +0x4C: UpdateToolState               (0x44AC20)  override
 *   [20] +0x50: GetDragOffset                 (0x449D80)  override
 *   [21] +0x54: CheckClick                    (0x449D00)  override
 *   [22] +0x58: (unknown)
 */

#pragma once

#include "../game/Panel.h"

class TrackPiece;
class ScriptedObject : public Panel {
public:
    /* ================================================================ */
    /* Fields (offsets from this)                                        */
    /* ================================================================ */

    /* Panel/Entity/GameObject fields (0x00..0xDF) — see Panel.h, Entity.h, GameObject.h */
    /* +0x24: visible       (inherited from Entity) */
    /* +0x40: resource      (inherited from Entity) */
    /* +0x48: audio_channel (inherited from Entity, repurposed as path_buf) */
    /* +0x90: drag_active   (inherited from Panel) */
    /* +0x94: drag_offset_x (inherited from Panel) */
    /* +0x98: drag_offset_y (inherited from Panel) */
    /* +0xA0: tooltip_handle (inherited from Panel) */
    /* +0xAD: dim_flag      (inherited from Panel) */
    /* +0xDC: child_ptr     (inherited from Panel, repurposed as escape_tool) */

    /* ================================================================ */
    /* Embedded Entity sub-object (+0xE0..+0x167, 0x88 bytes)            */
    /* ================================================================ */
    /* Constructed via Entity::Entity(-1, -1, 0, 0) at 0x405790.        */
    /* Access via entity_vmove()/entity_init() helpers in .cpp.          */
    /*                                                                   */
    /* Repurposed Entity fields (ScriptedObject overlays after init):    */
    /*   Entity::resource (+0x40) → frame_data_ptr (int16_t offsets)     */
    /*   Entity::name[6]  (+0x82) → loaded_flag                          */
    uint8_t  sub_entity[0x88];              // +0xE0  embedded Entity (88 bytes)

    /* ================================================================ */
    /* ScriptedObject fields after embedded Entity (+0x168..+0x177)      */
    /* ================================================================ */

    RECT     drag_rect;                     // +0x168  active drag rectangle (computed from screen_rect or sub_entity screen_rect)

    /* ================================================================ */
    /* Embedded ScriptEngine (+0x178..+0x1B3, 0x3C bytes core)          */
    /* Total reserved: 0x88 bytes (+0x178..+0x1FF).                      */
    /* ================================================================ */
    /* ScriptEngine_Init at 0x44E8D0 initializes 0x3C bytes.            */
    /* ScriptedObject repurposes ScriptEngine::field_38 (at SE+0x38)    */
    /* as script_engine_offset.                                          */
    /* Key vtable slots: [15]=Call(shutdown), [21]=HandleDrag(void*,int) */
    uint8_t  script_engine_prefix[0x38];   // +0x178  ScriptEngine fields 0x00..0x37
    int32_t  script_engine_offset;          // +0x1B0  SE child sprite offset (overlays SE::field_38)
    uint8_t  script_engine_suffix[4];       // +0x1B4  remaining SE bytes
    uint8_t  _pad_1B8[0x48];                // +0x1B8..+0x1FF
    uint8_t  script_engine_active;          // +0x200  SE sub-object visible flag
    uint8_t  _pad_201[0x5F];                // +0x201..+0x25F

    /* ================================================================ */
    /* Embedded ScrollPanel (+0x260..+0x2E7, 0x88 bytes)                 */
    /* ================================================================ */
    /* UIPANEL_InitScrollPanel at 0x427370 initializes full 0x88 bytes.  */
    /* ScriptedObject repurposes UIPANEL bytes at +0x38 as offset.       */
    /* Key vtable slots: [15]=destructor, [21]=HandleDrag(void*,int)    */
    uint8_t  scroll_panel_prefix[0x38];    // +0x260  UIPANEL fields 0x00..0x37
    int32_t  scroll_panel_offset;           // +0x298  SP child sprite offset (overlays UIPANEL field)
    uint8_t  scroll_panel_suffix[0x4C];     // +0x29C  remaining UIPANEL bytes
    uint8_t  scroll_panel_active;           // +0x2E8  SP sub-object visible flag
    uint8_t  _pad_2E9[0x347];               // +0x2E9..+0x62F

    /* ================================================================ */
    /* Child/Script fields (+0x630..+0x73F)                              */
    /* ================================================================ */

    void*    child_script_ptr;              // +0x630  child ScriptedObject pointer
    uint8_t  _pad_634[6];                   // +0x634..+0x639
    uint8_t  unk_flag;                      // +0x63A
    char     script_bitmap_path[0x104];     // +0x63B  generated scene-relative BMP path
    uint8_t  _pad_73F;                       // +0x73F

    /* ================================================================ */
    /* Mode / trailing fields (+0x740..+0x74B)                           */
    /* ================================================================ */

    int16_t  mode;                          // +0x740  0=idle, 1=in-world, 2=exiting, 3=placed
    uint8_t  _pad_742[2];                   // +0x742..+0x743
    int32_t  field_744;                     // +0x744
    int32_t  field_748;                     // +0x748

    /* Total size: 0x74C bytes */

    /* ================================================================ */
    /* Constructor / Destructor                                          */
    /* ================================================================ */

    /**
     * ScriptedObject constructor.
     * Address: 0x449430
     */
    ScriptedObject();

    /**
     * Virtual destructor (vtable[0]).
     * Address: 0x4494C0 (body), scalar deleting wrapper compiler-generated.
     */
    virtual ~ScriptedObject();

    /* ================================================================ */
    /* Virtual methods — vtable overrides                                */
    /* ================================================================ */

    /** OnUpdateChild — vtable[1], address 0x454890 (Panel::UpdateChild) */
    virtual void OnUpdateChild();

    /** IsDragging — vtable[2], address 0x449CE0 */
    virtual int IsDragging(int x, int y);

    /** MoveTo — vtable[3], address 0x449DC0 */
    virtual void MoveTo(int x, int y) override;

    /** HitTest — vtable[4], address 0x44A0C0 */
    virtual int HitTest(int x, int y);

    /** Update — vtable[9], address 0x4497A0 */
    virtual void Update() override;

    /** Shutdown — vtable[14], address 0x4495B0 */
    virtual void Shutdown();

    /** InitState — vtable[15], address 0x44ADF0 */
    virtual int InitState();

    /** HandleToolClick — vtable[16], address 0x44A250 */
    virtual uint32_t HandleToolClick(int* tool, int x, int y);

    /** UpdateToolState — vtable[19], address 0x44AC20 */
    virtual uint32_t UpdateToolState(TrackPiece* tool);

    /** GetDragOffset — vtable[20], address 0x449D80 */
    virtual int GetDragOffset(int x, int y);

    /** CheckClick — vtable[21], address 0x449D00 */
    virtual bool CheckClick(int x, int y);

    /* ================================================================ */
    /* Non-virtual methods                                               */
    /* ================================================================ */

    /** InitSubObjects — Tear down all sub-objects. Address: 0x4494E0 */
    void InitSubObjects();

    /** HandleEvent — Load and parse a .dat script file. Address: 0x44B290 */
    void HandleEvent(uint32_t resource_id, const char* name_suffix);

    /** LoadFromStream — Init from parsed script stream. */
    virtual char LoadFromStream(void* stream);

    /** AddChild — Construct child ScriptedObject. Address: 0x44B190 */
    ScriptedObject* AddChild(uint32_t resource_id, const char* name_suffix);

    /** RemoveChild — Destroy child ScriptedObject. Address: 0x44B220 */
    void RemoveChild();

    /** EnterBuildMode — Enter/exit build mode. Address: 0x44A9D0 */
    void EnterBuildMode(uint8_t enter);
};

/* External global at 0x479190 */
extern int g_stream_open_flags;
