/**
 * PersistenceAdapter.cpp — typed host persistence adapter (#ifndef _WIN32)
 *
 * See PersistenceAdapter.h for the design and the explicit placement
 * limitation.  This file implements the strict .loco format parser and
 * writer over the typed record structs, plus the fresh-world seed that
 * backs the SDL host's single-player start.
 */

#ifndef _WIN32

#include "PersistenceAdapter.h"
#include "InputMgr.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

extern char  g_install_path[];     /* 0x4A99C8 — "<data>/art-res/" on the host */
extern void* operator_new(size_t); /* 0x465CE0 */
extern void  GLOBAL_free(void*);   /* 0x465CD0 */

namespace loco {
namespace host {

namespace {

/* Hard bounds for hostile/corrupt headers.  The largest shipped fixture
 * (~curr) is 250,048 bytes; these caps only prevent absurd allocations
 * while leaving every real save far below the limit. */
constexpr size_t kMaxSaveBytes    = 64u * 1024u * 1024u; /* 64 MiB */
constexpr size_t kMaxPreviewBytes = 16u * 1024u * 1024u; /* 16 MiB */
constexpr uint32_t kMaxEntityCount = 0x10000u;           /* 65536 */
constexpr uint32_t kMaxVehicleCount = 0x1000u;           /* 4096 */

bool read_whole_file(const std::string& path, std::vector<uint8_t>* out,
                     std::string* error)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) {
        if (error) *error = "cannot open file";
        return false;
    }
    if (std::fseek(f, 0, SEEK_END) != 0) {
        std::fclose(f);
        if (error) *error = "seek failed";
        return false;
    }
    long size = std::ftell(f);
    if (size < 0 || static_cast<unsigned long>(size) > kMaxSaveBytes) {
        std::fclose(f);
        if (error) *error = "file too large";
        return false;
    }
    std::rewind(f);
    out->resize(static_cast<size_t>(size));
    size_t got = std::fread(out->data(), 1, out->size(), f);
    std::fclose(f);
    if (got != out->size()) {
        if (error) *error = "short read";
        return false;
    }
    return true;
}

bool check_layout(const std::vector<uint8_t>& bytes, const SaveRegion& header,
                  size_t* preview_bytes, size_t* entities_bytes,
                  size_t* vehicles_bytes, std::string* error)
{
    if (header.type != 8) {
        if (error) *error = "bad magic (type != 8)";
        return false;
    }
    /* Preview dimensions: the game writes its own player id/color here,
     * which can be (0,0) — then the preview is empty (0 bytes), a
     * legitimate game-saved layout.  The 16 MiB cap rejects hostile
     * headers; uint16_t fields cannot overflow size_t. */
    uint32_t w = header.player_id;
    uint32_t h = header.player_color;
    size_t preview = static_cast<size_t>(w) * h;
    if (preview > kMaxPreviewBytes) {
        if (error) *error = "preview dimensions oversized";
        return false;
    }
    if (header.entity_count > kMaxEntityCount) {
        if (error) *error = "entity count oversized";
        return false;
    }
    if (header.vehicle_count > kMaxVehicleCount) {
        if (error) *error = "vehicle count oversized";
        return false;
    }
    size_t entities = static_cast<size_t>(header.entity_count) * sizeof(EntityRecord);
    size_t vehicles = static_cast<size_t>(header.vehicle_count) * sizeof(VehicleRecord);
    size_t total = sizeof(SaveRegion) + preview + entities + vehicles;
    if (bytes.size() != total) {
        if (error) {
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                          "size mismatch: file %zu vs declared %zu", bytes.size(), total);
            *error = buf;
        }
        return false;
    }
    *preview_bytes = preview;
    *entities_bytes = entities;
    *vehicles_bytes = vehicles;
    return true;
}

}  // namespace

/* ================================================================== */
/* PersistenceAdapter::read_save                                      */
/* ================================================================== */
LoadResult PersistenceAdapter::read_save(const std::string& path,
                                         SaveDocument* out)
{
    LoadResult result;
    std::string error;

    std::vector<uint8_t> bytes;
    if (!read_whole_file(path, &bytes, &error)) {
        result.error = LoadError::Missing;
        result.detail = "missing/unreadable: " + error;
        return result;
    }
    if (bytes.size() < sizeof(SaveRegion)) {
        result.error = LoadError::Truncated;
        result.detail = "file shorter than the 0x114-byte header";
        return result;
    }

    SaveDocument doc;
    std::memcpy(&doc.header, bytes.data(), sizeof(SaveRegion));

    size_t preview_bytes = 0, entities_bytes = 0, vehicles_bytes = 0;
    if (!check_layout(bytes, doc.header, &preview_bytes, &entities_bytes,
                      &vehicles_bytes, &error)) {
        /* Distinguish "bad magic" from truncation/oversize for the tests. */
        if (doc.header.type != 8) {
            result.error = LoadError::BadMagic;
        } else if (error.find("oversized") != std::string::npos) {
            result.error = LoadError::Oversized;
        } else {
            result.error = LoadError::Truncated;
        }
        result.detail = error;
        return result;
    }

    /* Preview pixels. */
    doc.preview.assign(bytes.begin() + sizeof(SaveRegion),
                       bytes.begin() + sizeof(SaveRegion) + preview_bytes);

    /* Entity records. */
    const uint8_t* p = bytes.data() + sizeof(SaveRegion) + preview_bytes;
    doc.entities.resize(doc.header.entity_count);
    if (entities_bytes > 0) {
        std::memcpy(doc.entities.data(), p, entities_bytes);
    }
    p += entities_bytes;

    /* Vehicle records. */
    doc.vehicles.resize(doc.header.vehicle_count);
    if (vehicles_bytes > 0) {
        std::memcpy(doc.vehicles.data(), p, vehicles_bytes);
    }

    result.ok = true;
    result.error = LoadError::None;
    result.carried_entities = doc.entities.size();
    result.carried_vehicles = doc.vehicles.size();
    /* placed_entities stays 0 — see the file header for the limitation. */
    if (out != nullptr) {
        *out = std::move(doc);
    }
    return result;
}

/* ================================================================== */
/* PersistenceAdapter::write_save                                     */
/* ================================================================== */
bool PersistenceAdapter::write_save(const std::string& path,
                                    const SaveDocument& doc,
                                    std::string* error)
{
    size_t preview_bytes = static_cast<size_t>(doc.header.player_id) *
                           doc.header.player_color;
    size_t entities_bytes = doc.entities.size() * sizeof(EntityRecord);
    size_t vehicles_bytes = doc.vehicles.size() * sizeof(VehicleRecord);
    size_t total = sizeof(SaveRegion) + preview_bytes + entities_bytes + vehicles_bytes;
    if (total > kMaxSaveBytes) {
        if (error) *error = "document too large";
        return false;
    }

    std::vector<uint8_t> out;
    out.reserve(total);
    const uint8_t* header_bytes = reinterpret_cast<const uint8_t*>(&doc.header);
    out.insert(out.end(), header_bytes, header_bytes + sizeof(SaveRegion));
    out.insert(out.end(), doc.preview.begin(), doc.preview.end());
    if (!doc.entities.empty()) {
        const uint8_t* e = reinterpret_cast<const uint8_t*>(doc.entities.data());
        out.insert(out.end(), e, e + entities_bytes);
    }
    if (!doc.vehicles.empty()) {
        const uint8_t* v = reinterpret_cast<const uint8_t*>(doc.vehicles.data());
        out.insert(out.end(), v, v + vehicles_bytes);
    }

    /* Atomic write: stage into "<path>.tmp" in the same directory, then
     * rename over the target (POSIX atomic).  A failed stage leaves no
     * file at all; a failed rename removes the temp — a partial save is
     * never visible.  (The documented atomic contract in
     * PersistenceAdapter.h is implemented here for real.) */
    const std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (f == nullptr) {
        if (error) *error = "cannot create file";
        return false;
    }
    size_t written = std::fwrite(out.data(), 1, out.size(), f);
    bool flush_ok = (std::fflush(f) == 0);
    std::fclose(f);
    if (written != out.size() || !flush_ok) {
        std::remove(tmp.c_str());   /* never leave a partial save */
        if (error) *error = "write failed";
        return false;
    }
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        std::remove(tmp.c_str());
        if (error) *error = "rename failed";
        return false;
    }
    return true;
}

/* ================================================================== */
/* PersistenceAdapter::name_escapes                                   */
/* ================================================================== */
bool PersistenceAdapter::name_escapes(const std::string& name)
{
    if (name.empty()) return true;
    /* Absolute paths, drive letters, and any path separator escape the
     * save directory; ".." components climb out of it. */
    if (name[0] == '/' || name[0] == '\\') return true;
    if (name.size() >= 2 && name[1] == ':') return true;
    if (name.find('/') != std::string::npos ||
        name.find('\\') != std::string::npos) return true;
    if (name == "." || name == "..") return true;
    return false;
}

/* ================================================================== */
/* PersistenceAdapter::instance                                       */
/* ================================================================== */
PersistenceAdapter& PersistenceAdapter::instance()
{
    static PersistenceAdapter adapter;
    return adapter;
}

/* ================================================================== */
/* seed_fresh_world_from_fixture                                      */
/* ================================================================== */
bool seed_fresh_world_from_fixture(InputMgr* mgr)
{
    /* 1. Real new-game init (INPUT_NewWorld, 0x41E120): build mode, the
     *    new-game sound 0x5026, World_Init, viewport scroll over the
     *    (empty) fresh entity list. */
    INPUT_NewWorld(mgr);

    /* 2. Read the shipped scenario fixture into the typed record set.
     *    SAVEGAME/Wildwest.sav is a shipped town (header name "ARRID",
     *    497 entity records + 1 vehicle); override with
     *    LEGO_LOCO_SAVE_SEED. */
    const char* seed = std::getenv("LEGO_LOCO_SAVE_SEED");
    const char* fixture = (seed != nullptr && *seed != '\0')
        ? seed : "SAVEGAME/Wildwest.sav";
    std::string path = std::string(g_install_path) + fixture;

    SaveDocument doc;
    LoadResult result = PersistenceAdapter::read_save(path, &doc);
    if (!result.ok) {
        std::fprintf(stderr,
            "[HOST] seed_fresh_world: cannot read fixture '%s' (%s); "
            "starting with the empty fresh world\n",
            fixture, result.detail.c_str());
        std::fflush(stderr);
        return false;
    }
    PersistenceAdapter::instance().document() = std::move(doc);

    std::fprintf(stderr,
        "[HOST] seed_fresh_world: seeded %zu entity + %zu vehicle records "
        "from '%s' (placement coverage: %zu placed / %zu carried — host "
        "resources lack original RESDATA metadata; see PersistenceAdapter.h)\n",
        PersistenceAdapter::instance().document().entities.size(),
        PersistenceAdapter::instance().document().vehicles.size(),
        fixture, result.placed_entities, result.carried_entities);
    std::fflush(stderr);

    /* 3. Persist the recovered records to "curr" through the real save
     *    machinery (INPUT_SaveCurrentWorld, 0x41D9B0 — atomic temp+
     *    rename on the host, so a durable curr exists iff this succeeds).
     *    The save result is PROPAGATED: a fresh seed must never report
     *    success without a durable curr (the loading worker then skips
     *    the load-back and the host logs the failure loudly). */
    if (INPUT_SaveCurrentWorld(mgr, "curr") == 0) {
        std::fprintf(stderr,
            "[HOST] seed_fresh_world: INPUT_SaveCurrentWorld(\"curr\") "
            "failed — fresh world NOT persisted (no durable curr); "
            "continuing with the empty fresh world\n");
        std::fflush(stderr);
        return false;
    }
    return true;
}

}  // namespace host
}  // namespace loco

#endif /* _WIN32 */
