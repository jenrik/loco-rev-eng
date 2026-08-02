/**
 * PersistenceAdapter.h — typed host persistence adapter (#ifndef _WIN32)
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 *
 * The SDL host cannot yet run the full original placement pipeline for a
 * shipped .loco save: the typed entity constructors (Entity::InitBase
 * 0x405900, ResourceGameObject 0x4580A0, GameVehicle 0x412870, ...) read
 * original-RESDATA metadata (object type +0x08, animation table +0x20,
 * member limit +0x522, tile-state byte +0x63A, ...) from the resource
 * objects ResourceManager_GetById returns.  On the host those objects are
 * SDL SpriteResource values that do NOT carry the original x86 RESDATA
 * layout, so invoking the typed constructors against them would be
 * out-of-bounds reads.  Per AGENTS.md and the persistence-milestone
 * requirement, this adapter therefore:
 *
 *   1. represents the recovered save records with the strongly typed
 *      structs below (evidence: shipped art-res/SAVEGAME saves and
 *      art-res/~curr byte layouts, verified against the INPUT_LoadSaveFile
 *      0x41D5C0 / INPUT_SaveCurrentWorld 0x41D9B0 disassembly),
 *   2. parses and writes the .loco format strictly (missing file, bad
 *      magic, truncation, oversized counts/dimensions and path escapes
 *      all fail explicitly — no partial-success concealment, no OOB),
 *   3. reports the placement coverage explicitly: the number of records
 *      whose entity classes the host object graph can place into the
 *      world (0 today) and the number carried as records (all of them
 *      until the resource-RESDATA milestone lands).
 *
 * The adapter is the ONLY host component that owns save-record state;
 * INPUT_SaveCurrentWorld (host path) persists the adapter's recovered
 * record set, so fixture -> records -> curr round-trips exactly.
 */

#pragma once

#include "../shared/types.h"

#include <cstdint>
#include <string>
#include <vector>

class InputMgr;

namespace loco {
namespace host {

/* ================================================================== */
/* Typed recovered save records (0x80-byte entities, 0x2C vehicles)    */
/* ================================================================== */

/** One 0x14-byte child (occupant) record inside an entity record. */
struct ChildRecord {
    uint16_t resource_id;   /* +0x00  -> parent vtable[15] FindChild/CreateMember */
    uint16_t pad_02;        /* +0x02 */
    uint32_t value;         /* +0x04  -> child->+0x94 */
    char     name[12];      /* +0x08  wcsstr(name,"PARTY") -> child SetName */
};
static_assert(sizeof(ChildRecord) == 0x14, "ChildRecord must be 0x14 bytes");

/** One 0x80-byte entity record. */
struct EntityRecord {
    uint16_t resource_id;   /* +0x00 */
    uint16_t x;             /* +0x02 */
    uint16_t y;             /* +0x04 */
    uint16_t pad_06;        /* +0x06 */
    uint32_t anim_state;    /* +0x08  -> entity vtable[7] SetAnimState */
    uint32_t dest;          /* +0x0C  -> entity->+0xBC */
    char     name[12];      /* +0x10  -> entity vtable[13] SetName/SetCustomName */
    ChildRecord children[5]; /* +0x1C */
};
static_assert(sizeof(EntityRecord) == 0x80, "EntityRecord must be 0x80 bytes");
static_assert(offsetof(EntityRecord, children) == 0x1C,
              "EntityRecord children offset mismatch");

/** One 0x2C-byte vehicle record. */
struct VehicleRecord {
    uint32_t resource_id;       /* +0x00  -> World_LoadFromFile vehicle_init[0] */
    uint32_t route[3];          /* +0x04  -> vehicle_init[1..3] */
    uint32_t fields_10_1C[4];   /* +0x10  preserved verbatim (unknown in the
                                 *         original; only [0..3] and +0x20 are
                                 *         consumed by the load path) */
    char     name[12];          /* +0x20  -> editors[0]->SetName */
};
static_assert(sizeof(VehicleRecord) == 0x2C, "VehicleRecord must be 0x2C bytes");

/** A complete parsed .loco save document. */
struct SaveDocument {
    SaveRegion             header;     /* 0x114-byte header (typed) */
    std::vector<uint8_t>   preview;    /* header.player_id * player_color bytes */
    std::vector<EntityRecord>  entities;
    std::vector<VehicleRecord> vehicles;
};

/* ================================================================== */
/* Strict load/write results                                          */
/* ================================================================== */

enum class LoadError {
    None,
    Missing,     /* file does not exist or cannot be opened */
    BadMagic,    /* header type word != 8 */
    Truncated,   /* file ends before the declared records complete */
    Oversized,   /* count/dimension values are implausible (OOB guard) */
    Io,          /* read/write error */
    PathEscape,  /* name resolves outside the save directory */
};

struct LoadResult {
    bool       ok = false;
    LoadError  error = LoadError::None;
    std::string detail;
    /* Placement coverage: how many records the host object graph could
     * place into the world (INPUT_PlaceObject + TileMap placement) and
     * how many are carried as records instead.  0 placed until the host
     * resource objects carry the original RESDATA metadata — see the
     * file header. */
    size_t     placed_entities = 0;
    size_t     carried_entities = 0;
    size_t     carried_vehicles = 0;
};

/* ================================================================== */
/* PersistenceAdapter                                                 */
/* ================================================================== */

class PersistenceAdapter {
public:
    /** Strict, bounded read of a .loco save into typed records.
     *  Never mutates the shipped fixtures; caller supplies the path. */
    static LoadResult read_save(const std::string& path, SaveDocument* out);

    /** Exact write: 0x114 header + preview + entities*0x80 + vehicles*0x2C.
     *  Returns false (with detail) on any I/O error; writes atomically
     *  through a temp file + rename so a failed save never leaves a
     *  partial file behind. */
    static bool write_save(const std::string& path, const SaveDocument& doc,
                           std::string* error);

    /** True when the name would escape the save directory (host
     *  hardening; the original concatenates caller strings verbatim). */
    static bool name_escapes(const std::string& name);

    /** The adapter's recovered record set — the persisted host world. */
    SaveDocument& document() { return document_; }
    const SaveDocument& document() const { return document_; }
    void clear_document() { document_ = SaveDocument(); }

    /* Singleton: the host world has one persistence state. */
    static PersistenceAdapter& instance();

private:
    PersistenceAdapter() = default;
    SaveDocument document_;
};

/* ================================================================== */
/* Host placement capability (test hook)                               */
/* ================================================================== */

/** True when the SDL host may place entities into the world (default
 *  false — host resources lack original RESDATA metadata; see the file
 *  header).  Component tests that supply proper-layout resource objects
 *  set this to true.  Defined in InputMgr.cpp (host branch). */
#ifndef _WIN32
bool host_placement_available();
void set_host_placement_available(bool available);

/** Host atomic-save commit (defined in resources/ResDataSave.cpp).
 *  Flushes and renames the RESDATA secondary (write) stream's temp file
 *  over its target path; INPUT_SaveCurrentWorld (0x41D9B0) calls this
 *  after every record write succeeds and only reports success when it
 *  returns true, so a failed save never leaves a partial "curr". */
bool host_save_commit(RESDATA* resdata);
#endif

/* ================================================================== */
/* Host fresh-world seeding                                            */
/* ================================================================== */

/** Seed a fresh single-player world from a shipped scenario fixture and
 *  persist it to "curr".
 *
 *  Flow (documented host deviation, #ifndef _WIN32 only):
 *    1. INPUT_NewWorld(&g_input_mgr)  — real new-game init (0x41E120):
 *       build mode, new-game sound, World_Init, viewport scroll.
 *    2. Read the shipped fixture SAVEGAME/Wildwest.sav (evidence-backed
 *       shipped town; override with LEGO_LOCO_SAVE_SEED) into the typed
 *       record set.
 *    3. INPUT_SaveCurrentWorld(&g_input_mgr, "curr")  — persist the
 *       recovered records to <data>/art-res/curr through the real save
 *       machinery.
 *
 *  The seed is a real, dynamic world at the persistence layer: the
 *  records are parsed from the shipped fixture and round-trip exactly
 *  through curr.  Entity-graph placement of those records into the
 *  mode-3 world is gated by the RESDATA-metadata limitation (see the
 *  file header) and tracked in PROGRESS.md.
 *
 *  Returns true only when the fixture parsed AND INPUT_SaveCurrentWorld
 *  durably persisted "curr" (atomic host save); on any failure the
 *  reason is logged loudly and false is returned (the host still enters
 *  mode 3 with the empty fresh world state INPUT_NewWorld produced, but
 *  never reports a seeded world that was not durably saved). */
bool seed_fresh_world_from_fixture(InputMgr* mgr);

}  // namespace host
}  // namespace loco
