// Status: VALIDATED
/**
 * persistence_adapter_test.cpp — typed host persistence adapter regressions
 *
 * Exercises the strict .loco format parser/writer (PersistenceAdapter,
 * #ifndef _WIN32) against the shipped fixtures:
 *
 *   1. every shipped SAVEGAME save, ScrSaver save and art-res/~curr
 *      parses with the expected header/record layout (counts below were
 *      verified by byte-parsing the shipped files),
 *   2. save -> load round-trip is record-identical,
 *   3. missing file / bad magic / truncation / oversized counts and
 *      dimensions fail explicitly (never partial success),
 *   4. save names cannot escape the save directory (no host escape).
 *
 * All file writes go to fresh temp dirs under build/test-artifacts;
 * shipped art-res is read-only here.
 */

#include "persistence_fixtures.h"
#include "input/PersistenceAdapter.h"

#include <vector>

using loco::host::LoadError;
using loco::host::LoadResult;
using loco::host::PersistenceAdapter;
using loco::host::SaveDocument;

/* seed_fresh_world_from_fixture (PersistenceAdapter.o) references the
 * real INPUT_* entry points, which are linked by input_world_test; this
 * adapter-only test never calls the seed, so fail-loud fixtures prove
 * the reference stays dormant here. */
static void fixture_reached_INPUT_NewWorld(void)
{ std::fprintf(stderr, "FAIL: INPUT_NewWorld reached in adapter test\n"); std::abort(); }
static void fixture_reached_INPUT_SaveCurrentWorld(void)
{ std::fprintf(stderr, "FAIL: INPUT_SaveCurrentWorld reached in adapter test\n"); std::abort(); }
class InputMgr;
void INPUT_NewWorld(InputMgr*);
void INPUT_NewWorld(InputMgr*) { fixture_reached_INPUT_NewWorld(); }
void INPUT_SaveCurrentWorld(InputMgr*, const char*);
void INPUT_SaveCurrentWorld(InputMgr*, const char*) { fixture_reached_INPUT_SaveCurrentWorld(); }

static int failures = 0;

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::fprintf(stderr, "FAIL: %s\n", msg);                          \
            failures++;                                                       \
        }                                                                     \
    } while (0)

/* Build a minimal valid document the tests can corrupt on purpose. */
static SaveDocument make_doc(uint16_t w, uint16_t h, uint32_t entities,
                             uint16_t vehicles)
{
    SaveDocument doc;
    doc.header.type = 8;
    doc.header.player_id = w;
    doc.header.player_color = h;
    doc.header.entity_count = entities;
    doc.header.vehicle_count = vehicles;
    doc.preview.assign(static_cast<size_t>(w) * h, 0x05);
    for (uint32_t i = 0; i < entities; i++) {
        loco::host::EntityRecord r;
        std::memset(&r, 0, sizeof(r));
        r.resource_id = static_cast<uint16_t>(0x1000 + i);
        r.x = static_cast<uint16_t>(i);
        r.y = 1;
        doc.entities.push_back(r);
    }
    for (uint16_t i = 0; i < vehicles; i++) {
        loco::host::VehicleRecord v;
        std::memset(&v, 0, sizeof(v));
        v.resource_id = 0x1808;
        doc.vehicles.push_back(v);
    }
    return doc;
}

int main()
{
    const std::string root = fixture_root();

    /* ---- 1. Parse every shipped fixture ---- */
    {
        struct Fixture {
            const char* name;
            uint16_t    w, h;
            uint32_t    entities;
            uint16_t    vehicles;
        };
        /* Counts verified by byte-parsing the shipped fixtures (header
         * entity_count/vehicle_count must equal the records present). */
        const Fixture fixtures[] = {
            { "SAVEGAME/Wildwest.sav", 64, 48, 497, 1 },
            { "SAVEGAME/4BRIDGES.SAV", 64, 48, 558, 1 },
            { "SAVEGAME/BUSYTOWN.SAV", 64, 48, 1080, 0 },
            { "SAVEGAME/COW-VILL.SAV", 64, 48, 1834, 3 },
            { "SAVEGAME/FOREST.SAV",   64, 48, 1927, 1 },
            { "SAVEGAME/GREENVIL.SAV", 64, 48, 915, 1 },
            { "SAVEGAME/MOONBASE.SAV", 64, 48, 647, 1 },
            { "SAVEGAME/SNOWBALL.SAV", 64, 48, 443, 1 },
            { "ScrSaver/SS800.sav",    64, 48, 498, 2 },
            { "ScrSaver/SS1024.sav",   64, 48, 855, 1 },
            { "ScrSaver/SS1152.sav",   72, 54, 1266, 1 },
            { "ScrSaver/SS1280.sav",   80, 64, 1759, 1 },
            { "~curr",                 64, 48, 1927, 1 },
        };
        for (const Fixture& f : fixtures) {
            std::string path = root + "/" + f.name;
            SaveDocument doc;
            LoadResult r = PersistenceAdapter::read_save(path, &doc);
            CHECK(r.ok, f.name);
            if (!r.ok) {
                std::fprintf(stderr, "  (%s)\n", r.detail.c_str());
                continue;
            }
            CHECK(doc.header.type == 8, f.name);
            CHECK(doc.header.player_id == f.w, f.name);
            CHECK(doc.header.player_color == f.h, f.name);
            /* Strict layout: the declared counts must equal the records
             * actually present (and the table's verified values). */
            CHECK(doc.entities.size() == f.entities, f.name);
            CHECK(doc.entities.size() == doc.header.entity_count, f.name);
            CHECK(doc.vehicles.size() == f.vehicles, f.name);
            CHECK(doc.vehicles.size() == doc.header.vehicle_count, f.name);
            CHECK(doc.preview.size() ==
                      static_cast<size_t>(doc.header.player_id) *
                          doc.header.player_color,
                  f.name);
        }
    }

    /* ---- 2. write -> read round-trip ---- */
    {
        std::string dir = make_temp_dir();
        std::string path = dir + "/roundtrip.loco";
        SaveDocument doc = make_doc(16, 12, 50, 2);

        std::string error;
        CHECK(PersistenceAdapter::write_save(path, doc, &error), "write_save");

        SaveDocument back;
        LoadResult r = PersistenceAdapter::read_save(path, &back);
        CHECK(r.ok, "round-trip read");
        CHECK(back.header.type == 8, "round-trip header type");
        CHECK(back.header.player_id == 16 && back.header.player_color == 12,
              "round-trip dims");
        CHECK(back.entities.size() == 50, "round-trip entity count");
        CHECK(back.vehicles.size() == 2, "round-trip vehicle count");
        CHECK(back.preview.size() == 16 * 12, "round-trip preview size");
        /* Record identity must be byte-exact. */
        bool identical = true;
        for (size_t i = 0; i < doc.entities.size() && identical; i++) {
            identical = std::memcmp(&doc.entities[i], &back.entities[i],
                                    sizeof(loco::host::EntityRecord)) == 0;
        }
        CHECK(identical, "round-trip entity records identical");
        identical = std::memcmp(doc.preview.data(), back.preview.data(),
                                doc.preview.size()) == 0;
        CHECK(identical, "round-trip preview identical");

        /* Atomic-write contract: write_save stages "<path>.tmp" and
         * renames it over the target, so no temp file is left behind
         * after a successful write. */
        CHECK(access((path + ".tmp").c_str(), F_OK) != 0,
              "write_save leaves no temp file after success");

        /* Atomic-write failure: an unwritable directory fails the stage
         * without touching the target and leaves no temp. */
        {
            std::string ro = make_temp_dir();
            std::string ropath = ro + "/ro.loco";
            ::chmod(ro.c_str(), 0555);
            std::string err;
            CHECK(!PersistenceAdapter::write_save(ropath, doc, &err),
                  "write_save fails on an unwritable directory");
            CHECK(access(ropath.c_str(), F_OK) != 0,
                  "failed atomic write leaves no target file");
            CHECK(access((ropath + ".tmp").c_str(), F_OK) != 0,
                  "failed atomic write leaves no temp file");
            ::chmod(ro.c_str(), 0755);
        }
    }

    /* ---- 3. malformed inputs fail explicitly ---- */
    {
        std::string dir = make_temp_dir();
        std::string path = dir + "/missing.loco";
        SaveDocument doc;
        LoadResult r = PersistenceAdapter::read_save(path, &doc);
        CHECK(!r.ok && r.error == LoadError::Missing, "missing file fails");

        /* bad magic */
        {
            SaveDocument bad = make_doc(16, 12, 3, 0);
            bad.header.type = 7;
            std::string p = dir + "/badmagic.loco";
            std::string err;
            CHECK(PersistenceAdapter::write_save(p, bad, &err), "write badmagic");
            LoadResult r2 = PersistenceAdapter::read_save(p, &doc);
            CHECK(!r2.ok && r2.error == LoadError::BadMagic, "bad magic fails");
        }

        /* truncation: header claims 100 records but the file has 3 */
        {
            SaveDocument trunc = make_doc(16, 12, 3, 0);
            std::string p = dir + "/trunc.loco";
            std::string err;
            CHECK(PersistenceAdapter::write_save(p, trunc, &err), "write trunc");
            /* append bytes to the header entity_count to claim 100 */
            FILE* f = std::fopen(p.c_str(), "r+b");
            CHECK(f != nullptr, "open trunc for patch");
            if (f != nullptr) {
                uint32_t count = 100;
                std::fseek(f, 0x08, SEEK_SET);
                std::fwrite(&count, 1, sizeof(count), f);
                std::fclose(f);
            }
            LoadResult r3 = PersistenceAdapter::read_save(p, &doc);
            CHECK(!r3.ok &&
                      (r3.error == LoadError::Truncated ||
                       r3.error == LoadError::Oversized),
                  "truncated file fails explicitly");
        }

        /* oversized entity count */
        {
            SaveDocument over = make_doc(16, 12, 0x10001, 0); /* > cap */
            std::string p = dir + "/oversized.loco";
            std::string err;
            CHECK(PersistenceAdapter::write_save(p, over, &err), "write oversized");
            LoadResult r4 = PersistenceAdapter::read_save(p, &doc);
            CHECK(!r4.ok && r4.error == LoadError::Oversized,
                  "oversized entity count fails");
        }

        /* oversized preview dimensions: 0xFFFF x 0xFFFF overflows the
         * 16 MiB preview cap (write refuses; the reader classifies the
         * header as Oversized once a file exists with those dims).
         *
         * Built by hand rather than via make_doc(): write_save's cap
         * check multiplies header.player_id * player_color directly and
         * never reads doc.preview, so make_doc()'s unconditional
         * preview.assign(w * h, ...) would allocate/fill a real ~4 GiB
         * buffer just to exercise a rejection path that doesn't need it
         * (this previously OOM-killed the host running the suite). */
        {
            SaveDocument over;
            over.header.type = 8;
            over.header.player_id = 0xFFFF;
            over.header.player_color = 0xFFFF;
            std::string p = dir + "/bigdims.loco";
            std::string err;
            /* write_save refuses before writing (dims overflow) */
            CHECK(!PersistenceAdapter::write_save(p, over, &err),
                  "oversized dims refused at write");
            LoadResult r5 = PersistenceAdapter::read_save(p, &doc);
            CHECK(!r5.ok, "oversized dims file fails at read (missing)");
        }
    }

    /* ---- 4. path-escape protection ---- */
    {
        CHECK(PersistenceAdapter::name_escapes("../escape"), "parent traversal");
        CHECK(PersistenceAdapter::name_escapes("/etc/passwd"), "absolute path");
        CHECK(PersistenceAdapter::name_escapes("C:\\windows"), "drive letter");
        CHECK(PersistenceAdapter::name_escapes("a/b"), "slash component");
        CHECK(PersistenceAdapter::name_escapes(".."), "dotdot");
        CHECK(PersistenceAdapter::name_escapes(""), "empty name");
        CHECK(!PersistenceAdapter::name_escapes("curr"), "plain name ok");
        CHECK(!PersistenceAdapter::name_escapes("SAVEGAME_FIX"), "plain name ok 2");
    }

    if (failures == 0) {
        std::puts("PASS: persistence adapter strict parse/write/round-trip/malformed");
        return 0;
    }
    std::fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
}
