// Status: VALIDATED
/**
 * input_world_test.cpp — INPUT_* world new/load/save regressions
 *
 * Drives the real persistence entry points (InputMgr.o + ResDataSave.o +
 * PersistenceAdapter.o, host path) against copies of the shipped
 * fixtures in a fresh temp dir:
 *
 *   INPUT_NewWorld        0x41E120  fresh-world init + jingle
 *   INPUT_LoadSaveFile    0x41D5C0  strict load, record carry, failure
 *                                   modes (missing/bad magic/truncated/
 *                                   escape)
 *   INPUT_LoadWorld       0x41D320  load + current-save recording
 *   INPUT_SaveCurrentWorld 0x41D9B0 persist the recovered record set
 *   INPUT_FindObjectAt    0x41E1F0  typed collection scan (LoadSaveFile's
 *                                   vehicle-loop callee)
 *
 * INPUT_PlaceObject (0x41DD80) is an editor-only loud deferred stub and
 * is NOT exercised here (the persistence path never calls it).
 *
 * The link is honest: every symbol InputMgr.o/ResDataSave.o reference is
 * provided by persistence_fixtures.h (fail-loud where the gated host
 * path must never reach it).  Shipped art-res is never written.
 */

#include "persistence_fixtures.h"
#include "../src/decompiled_cpp/input/InputMgr.h"
#include "../src/decompiled_cpp/input/PersistenceAdapter.h"
#include "../src/decompiled_cpp/resources/ResourceManager.h"

#include <vector>

using loco::host::PersistenceAdapter;

static int failures = 0;

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::fprintf(stderr, "FAIL: %s\n", msg);                          \
            failures++;                                                       \
        }                                                                     \
    } while (0)

int main()
{
    /* ---- 1. INPUT_NewWorld (0x41E120) on a fresh manager ---- */
    {
        InputMgr mgr;
        g_in_build_mode = 0;
        g_last_play_sound_id = 0;
        INPUT_NewWorld(&mgr);
        CHECK(g_in_build_mode == 1, "NewWorld sets g_in_build_mode (0x4FD199)");
        /* The original plays the new-game jingle via PlaySound(0x5026)
         * (0x41E123); on the host the sound-resource loading chain is not
         * reconstructed, so the guarded host path logs the jingle loudly
         * instead of invoking the broken PlaySound (documented deviation
         * in InputMgr.cpp).  The fixture's PlaySound recorder must stay
         * untouched. */
        CHECK(g_last_play_sound_id == 0,
              "NewWorld jingle is a guarded host skip (PlaySound not invoked)");
        /* g_game/g_tilemap null: the scroll block is a loud host skip;
         * entity_count is 0 so the scroll loop never runs. */
        CHECK(mgr.entity_count == 0, "NewWorld leaves a fresh manager empty");
        mgr.DtorBody();
    }

    /* ---- 2. INPUT_LoadSaveFile on a real shipped fixture ---- */
    {
        std::string dir = make_temp_dir();
        std::string wild = dir + "/Wildwest.sav";
        CHECK(copy_fixture(fixture_root() + "/SAVEGAME/Wildwest.sav", wild),
              "copy Wildwest fixture");
        std::snprintf(g_install_path, sizeof(g_install_path), "%s/", dir.c_str());

        InputMgr mgr;
        PersistenceAdapter::instance().clear_document();

        char result = INPUT_LoadSaveFile(&mgr, "Wildwest.sav", 1, 1);
        CHECK(result == 1, "LoadSaveFile loads Wildwest.sav");
        CHECK(g_in_build_mode == 1, "LoadSaveFile sets g_in_build_mode at tail");
        CHECK(g_allow_building_placement == 1,
              "LoadSaveFile restores placement flag");
        CHECK(PersistenceAdapter::instance().document().entities.size() == 497,
              "Wildwest: 497 entity records carried");
        CHECK(PersistenceAdapter::instance().document().vehicles.size() == 1,
              "Wildwest: 1 vehicle record carried");
        mgr.DtorBody();
    }

    /* ---- 3. Save -> load round-trip through the REAL functions ---- */
    {
        std::string dir = make_temp_dir();
        std::string wild = dir + "/Wildwest.sav";
        copy_fixture(fixture_root() + "/SAVEGAME/Wildwest.sav", wild);
        std::snprintf(g_install_path, sizeof(g_install_path), "%s/", dir.c_str());

        InputMgr mgr;
        PersistenceAdapter::instance().clear_document();
        CHECK(INPUT_LoadSaveFile(&mgr, "Wildwest.sav", 1, 1) == 1,
              "load before save");

        /* Persist the recovered records to "curr" (host path). */
        INPUT_SaveCurrentWorld(&mgr, "curr");
        CHECK(PersistenceAdapter::name_escapes("curr") == false,
              "curr is a safe save name");
        std::string curr = dir + "/curr";
        FILE* f = std::fopen(curr.c_str(), "rb");
        CHECK(f != nullptr, "SaveCurrentWorld wrote <dir>/curr");
        if (f != nullptr) std::fclose(f);

        /* Read curr back through the strict adapter: records must match
         * the fixture's records exactly. */
        loco::host::SaveDocument doc;
        auto r = PersistenceAdapter::read_save(curr, &doc);
        CHECK(r.ok, "curr parses");
        if (r.ok) {
            CHECK(doc.entities.size() == 497, "curr entity count matches fixture");
            CHECK(doc.vehicles.size() == 1, "curr vehicle count matches fixture");
            CHECK(doc.header.entity_count == 497, "curr header entity count");
            /* The header player fields now hold g_player_id/color (the
             * original writer's behaviour) — record identity is the
             * round-trip invariant. */
            bool identical = true;
            const auto& src = PersistenceAdapter::instance().document().entities;
            for (size_t i = 0; i < src.size() && identical; i++) {
                identical = std::memcmp(&src[i], &doc.entities[i],
                                        sizeof(loco::host::EntityRecord)) == 0;
            }
            CHECK(identical, "curr entity records identical to the fixture's");
        }

        /* LoadWorld("curr") through the real entry point. */
        g_current_save_path[0] = '\0';
        PersistenceAdapter::instance().clear_document();
        CHECK(INPUT_LoadWorld(&mgr, "curr") == 1, "LoadWorld loads curr");
        CHECK(std::strcmp(g_current_save_path, "curr") == 0,
              "LoadWorld records the current-save path (0x4AA8F8)");
        CHECK(PersistenceAdapter::instance().document().entities.size() == 497,
              "curr records carried after LoadWorld");
        mgr.DtorBody();
    }

    /* ---- 4. Failure modes fail explicitly ---- */
    {
        std::string dir = make_temp_dir();
        std::string wild = dir + "/Wildwest.sav";
        copy_fixture(fixture_root() + "/SAVEGAME/Wildwest.sav", wild);
        std::snprintf(g_install_path, sizeof(g_install_path), "%s/", dir.c_str());

        InputMgr mgr;

        /* missing */
        PersistenceAdapter::instance().clear_document();
        CHECK(INPUT_LoadSaveFile(&mgr, "does-not-exist.loco", 1, 1) == 0,
              "missing file -> 0");

        /* bad magic */
        {
            std::string bad = dir + "/bad.loco";
            loco::host::SaveDocument d = loco::host::SaveDocument();
            d.header.type = 7;
            std::string err;
            PersistenceAdapter::write_save(bad, d, &err);
            CHECK(INPUT_LoadSaveFile(&mgr, "bad.loco", 1, 1) == 0,
                  "bad magic -> 0");
        }

        /* truncated: declare more entities than the file holds */
        {
            std::string trunc = dir + "/trunc.loco";
            loco::host::SaveDocument d = loco::host::SaveDocument();
            d.header.type = 8;
            d.header.player_id = 16;
            d.header.player_color = 12;
            d.preview.assign(16 * 12, 0);
            std::string err;
            PersistenceAdapter::write_save(trunc, d, &err);
            /* Patch the header to claim 1000 entities. */
            FILE* f = std::fopen(trunc.c_str(), "r+b");
            CHECK(f != nullptr, "open trunc for patch");
            if (f != nullptr) {
                uint32_t count = 1000;
                std::fseek(f, 0x08, SEEK_SET);
                std::fwrite(&count, 1, sizeof(count), f);
                std::fclose(f);
            }
            CHECK(INPUT_LoadSaveFile(&mgr, "trunc.loco", 1, 1) == 0,
                  "truncated record area -> 0 (no partial success)");
        }

        /* path escape */
        PersistenceAdapter::instance().clear_document();
        CHECK(INPUT_LoadSaveFile(&mgr, "../etc/passwd", 1, 1) == 0,
              "path-escape name -> 0");
        CHECK(PersistenceAdapter::instance().document().entities.empty(),
              "path-escape does not carry records");

        mgr.DtorBody();
    }

    /* ---- 5. INPUT_FindObjectAt (0x41E1F0) on the collection ---- */
    {
        InputMgr mgr;
        CHECK(INPUT_FindObjectAt(&mgr, -1) == nullptr, "FindObjectAt(-1) empty");
        CHECK(INPUT_FindObjectAt(&mgr, 0) == nullptr, "FindObjectAt(0) empty");
        CHECK(INPUT_FindObjectAt(&mgr, 1) == nullptr, "FindObjectAt(1) empty");
        CHECK(INPUT_FindObjectAt(&mgr, 2) == nullptr, "FindObjectAt(2) empty");
        CHECK(INPUT_FindObjectAt(&mgr, 3) == nullptr, "FindObjectAt(3) empty");
        CHECK(INPUT_FindObjectAt(&mgr, 4) == nullptr, "FindObjectAt(4) empty");
        CHECK(INPUT_FindObjectAt(&mgr, 0x1000) == nullptr, "FindObjectAt(resId) empty");

        /* mode -1: random entity from a non-empty list (identity only). */
        mgr.entity_count = 2;
        Entity* a = reinterpret_cast<Entity*>(static_cast<uintptr_t>(0x100));
        Entity* b = reinterpret_cast<Entity*>(static_cast<uintptr_t>(0x200));
        mgr.buffer[0] = a;
        mgr.buffer[1] = b;
        mgr.count = 2;
        Entity* picked = static_cast<Entity*>(INPUT_FindObjectAt(&mgr, -1));
        CHECK(picked == a || picked == b, "FindObjectAt(-1) picks a member");
        mgr.entity_count = 0;
        mgr.DtorBody();
    }

    /* ---- 6. INPUT_SaveCurrentWorld failure path ---- */
    {
        /* An unwritable/absent output directory makes LoadResourceData
         * fail explicitly; the function must return 0 (not silently
         * succeed) and leave no partial file. */
        std::string dir = make_temp_dir();
        std::string missing_dir = dir + "/no-such-subdir";
        std::snprintf(g_install_path, sizeof(g_install_path), "%s/", missing_dir.c_str());
        InputMgr mgr;
        PersistenceAdapter::instance().clear_document();
        CHECK(INPUT_SaveCurrentWorld(&mgr, "curr") == 0,
              "SaveCurrentWorld fails explicitly when the output cannot be opened");
        mgr.DtorBody();
    }

    if (failures == 0) {
        std::puts("PASS: INPUT_NewWorld/LoadSaveFile/LoadWorld/SaveCurrentWorld/"
                  "FindObjectAt");
        return 0;
    }
    std::fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
}
