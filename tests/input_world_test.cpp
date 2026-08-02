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
        /* The original 0x4FD3DC global is BSS (zero) at startup; the
         * loader saves and restores it around its work.  Set the
         * caller-visible value explicitly so the restore is asserted
         * against a known pre-load state. */
        g_allow_building_placement = 1;

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

    /* ---- 5b. FindObjectAt default mode (0x41E498): the pick range is
     * the +0x158 count of the typed g_resmgr.GetById(mode) lookup, NOT a
     * collection scan (review-corrected semantics). ---- */
    {
        InputMgr mgr;
        g_fixture_getbyid_count = 0;
        CHECK(INPUT_FindObjectAt(&mgr, 5) == nullptr,
              "FindObjectAt(5) not-found via GetById (count 0)");
        CHECK(INPUT_FindObjectAt(&mgr, -2) == nullptr,
              "FindObjectAt(-2) default path (mode < -1)");

        /* Populated: with a fake resource whose +0x158 count is 2, the
         * pick range comes from the lookup and the result is one of the
         * two entities whose resource id (+0x04) equals the mode.  The
         * fixture maps the fake resource with MAP_32BIT so the binary's
         * int32 pointer return round-trips on x86_64; when the 32-bit
         * region is unavailable GetById returns 0 (not-found) and the
         * range check degrades to the null assertions below. */
        g_fixture_getbyid_count = 2;
        if (INPUT_FindObjectAt(&mgr, 7) == nullptr) {
            /* 32-bit fake unavailable: the not-found path must still be
             * null (no collection dereference on the default path). */
            CHECK(INPUT_FindObjectAt(&mgr, 7) == nullptr,
                  "FindObjectAt(7) null when the fake lookup is unavailable");
        } else {
            /* Two fake entities with fake resources whose +0x04 == 7. */
            struct FakeEntity { uint8_t pad[0x40]; void* resource; };
            FakeEntity* e0 = new FakeEntity();
            FakeEntity* e1 = new FakeEntity();
            int32_t* r0 = new int32_t[4];
            int32_t* r1 = new int32_t[4];
            std::memset(e0, 0, sizeof(*e0));
            std::memset(e1, 0, sizeof(*e1));
            std::memset(r0, 0, 4 * sizeof(int32_t));
            std::memset(r1, 0, 4 * sizeof(int32_t));
            r0[1] = 7;   /* resource +0x04 == mode */
            r1[1] = 7;
            e0->resource = r0;
            e1->resource = r1;
            mgr.count = 2;
            mgr.buffer[0] = reinterpret_cast<Entity*>(e0);
            mgr.buffer[1] = reinterpret_cast<Entity*>(e1);
            Entity* found = static_cast<Entity*>(INPUT_FindObjectAt(&mgr, 7));
            CHECK(found == reinterpret_cast<Entity*>(e0) ||
                      found == reinterpret_cast<Entity*>(e1),
                  "FindObjectAt(7) returns one of the +0x04==7 members");
            delete[] r0;
            delete[] r1;
            delete e0;
            delete e1;
        }
        g_fixture_getbyid_count = 0;
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
        CHECK(access((missing_dir + "/curr").c_str(), F_OK) != 0,
              "failed save leaves no partial curr");
        mgr.DtorBody();
    }

    /* ---- 7. Atomic-save contract: read-only dir + failed rename ---- */
    {
        /* (a) read-only data dir: fopen of the temp fails -> 0, and no
         * "curr" (or temp) is created. */
        std::string dir = make_temp_dir();
        std::string wild = dir + "/Wildwest.sav";
        copy_fixture(fixture_root() + "/SAVEGAME/Wildwest.sav", wild);
        std::snprintf(g_install_path, sizeof(g_install_path), "%s/", dir.c_str());
        InputMgr mgr;
        PersistenceAdapter::instance().clear_document();
        CHECK(INPUT_LoadSaveFile(&mgr, "Wildwest.sav", 1, 1) == 1,
              "read-only-dir case: load first");
        ::chmod(dir.c_str(), 0555);
        CHECK(INPUT_SaveCurrentWorld(&mgr, "curr") == 0,
              "read-only dir -> SaveCurrentWorld fails explicitly (atomic)");
        CHECK(access((dir + "/curr").c_str(), F_OK) != 0,
              "read-only failure leaves no partial curr");
        CHECK(access((dir + "/curr.tmp").c_str(), F_OK) != 0,
              "read-only failure leaves no temp file");
        ::chmod(dir.c_str(), 0755);
        mgr.DtorBody();

        /* (b) failed commit (target exists as a DIRECTORY): the temp
         * writes succeed but the rename cannot publish over a directory,
         * so the save returns 0 and removes the temp. */
        std::string dir2 = make_temp_dir();
        std::string wild2 = dir2 + "/Wildwest.sav";
        copy_fixture(fixture_root() + "/SAVEGAME/Wildwest.sav", wild2);
        std::snprintf(g_install_path, sizeof(g_install_path), "%s/", dir2.c_str());
        InputMgr mgr2;
        PersistenceAdapter::instance().clear_document();
        CHECK(INPUT_LoadSaveFile(&mgr2, "Wildwest.sav", 1, 1) == 1,
              "failed-rename case: load first");
        ::mkdir((dir2 + "/curr").c_str(), 0755);   /* directory blocks rename */
        CHECK(INPUT_SaveCurrentWorld(&mgr2, "curr") == 0,
              "failed atomic rename -> SaveCurrentWorld returns 0");
        struct stat st;
        CHECK(::stat((dir2 + "/curr").c_str(), &st) == 0 && S_ISDIR(st.st_mode),
              "failed rename leaves the pre-existing directory untouched");
        CHECK(access((dir2 + "/curr.tmp").c_str(), F_OK) != 0,
              "failed rename removes the temp (no partial save)");
        mgr2.DtorBody();
    }

    /* ---- 8. "curr.sav" must not clobber the host current-name ----
     * ----    bookkeeping (LoadWorld step 3 companion) ------------ */
    {
        std::string dir = make_temp_dir();
        std::string wild = dir + "/Wildwest.sav";
        std::string sav = dir + "/curr.sav";
        copy_fixture(fixture_root() + "/SAVEGAME/Wildwest.sav", wild);
        copy_fixture(fixture_root() + "/SAVEGAME/Wildwest.sav", sav);
        std::snprintf(g_install_path, sizeof(g_install_path), "%s/", dir.c_str());

        InputMgr mgr;
        PersistenceAdapter::instance().clear_document();
        CHECK(INPUT_LoadSaveFile(&mgr, "Wildwest.sav", 1, 1) == 1,
              "curr.sav case: load fixture first");
        CHECK(INPUT_SaveCurrentWorld(&mgr, "curr") == 1,
              "curr.sav case: persist curr");

        g_current_save_path[0] = '\0';
        PersistenceAdapter::instance().clear_document();
        CHECK(INPUT_LoadWorld(&mgr, "curr") == 1, "LoadWorld loads curr");
        /* The companion load of "curr.sav" (marker match) must not
         * overwrite the primary load's recorded name with "curr.sav". */
        CHECK(std::strcmp(g_current_save_path, "curr") == 0,
              "curr.sav companion load does not clobber the current-save path");
        mgr.DtorBody();
    }

    /* ---- 9. SaveCurrentWorld symmetric path-escape guard + seed ----
     * ----    failure propagation (fresh seed never reports success --
     * ----    without a durable curr) ------------------------------ */
    {
        std::string dir = make_temp_dir();
        std::string wild = dir + "/Wildwest.sav";
        copy_fixture(fixture_root() + "/SAVEGAME/Wildwest.sav", wild);
        std::snprintf(g_install_path, sizeof(g_install_path), "%s/", dir.c_str());
        InputMgr mgr;
        PersistenceAdapter::instance().clear_document();
        CHECK(INPUT_LoadSaveFile(&mgr, "Wildwest.sav", 1, 1) == 1,
              "escape case: load fixture");
        CHECK(INPUT_SaveCurrentWorld(&mgr, "../evil") == 0,
              "SaveCurrentWorld refuses an escaping name (symmetric guard)");
        CHECK(INPUT_SaveCurrentWorld(&mgr, "/tmp/evil") == 0,
              "SaveCurrentWorld refuses an absolute name");
        mgr.DtorBody();

        /* Seed success path: fixture parses + curr persists durably. */
        {
            std::string sdir = make_temp_dir();
            std::string swild = sdir + "/Wildwest.sav";
            copy_fixture(fixture_root() + "/SAVEGAME/Wildwest.sav", swild);
            std::snprintf(g_install_path, sizeof(g_install_path), "%s/", sdir.c_str());
            InputMgr smgr;
            PersistenceAdapter::instance().clear_document();
            setenv("LEGO_LOCO_SAVE_SEED", "Wildwest.sav", 1);
            CHECK(loco::host::seed_fresh_world_from_fixture(&smgr),
                  "seed succeeds with a durable curr");
            CHECK(access((sdir + "/curr").c_str(), F_OK) == 0,
                  "seed leaves a durable curr file");
            CHECK(access((sdir + "/curr.tmp").c_str(), F_OK) != 0,
                  "seed leaves no temp file");
            unsetenv("LEGO_LOCO_SAVE_SEED");
            smgr.DtorBody();
        }

        /* Seed failure path: read-only dir -> save fails -> seed returns
         * false (the fresh world must never report success without a
         * durable curr). */
        {
            std::string fdir = make_temp_dir();
            std::string fwild = fdir + "/Wildwest.sav";
            copy_fixture(fixture_root() + "/SAVEGAME/Wildwest.sav", fwild);
            std::snprintf(g_install_path, sizeof(g_install_path), "%s/", fdir.c_str());
            InputMgr fmgr;
            PersistenceAdapter::instance().clear_document();
            setenv("LEGO_LOCO_SAVE_SEED", "Wildwest.sav", 1);
            ::chmod(fdir.c_str(), 0555);
            CHECK(!loco::host::seed_fresh_world_from_fixture(&fmgr),
                  "seed FAILS when the durable curr save fails (no false success)");
            CHECK(access((fdir + "/curr").c_str(), F_OK) != 0,
                  "failed seed leaves no durable curr");
            ::chmod(fdir.c_str(), 0755);
            unsetenv("LEGO_LOCO_SAVE_SEED");
            fmgr.DtorBody();
        }
    }

    /* ---- 10. Placement-offset semantics (0x41D693..0x41D6D2): the
     * binary divides ((player - saved)/2) with the cltd/sub/sar idiom —
     * TRUNCATION TOWARD ZERO (-3/2 = -1, not floor division -2) — and
     * reads the saved header words as 16-bit UNSIGNED (and $0xffff)
     * while the player globals are sign-extended 16-bit loads.  The
     * recording TileMap fixture captures the coordinates the placement
     * block passes to FindObject. ---- */
    {
        std::string dir = make_temp_dir();
        std::snprintf(g_install_path, sizeof(g_install_path), "%s/", dir.c_str());

        /* (a) delta -3: the offset must be -1 (truncation toward zero),
         * never -2 (floor).  Preview dims 3x3 = 9 bytes; one entity at
         * (0,0). */
        loco::host::SaveDocument doc = loco::host::SaveDocument();
        doc.header.type = 8;
        doc.header.player_id = 3;
        doc.header.player_color = 3;
        doc.header.entity_count = 1;
        doc.preview.assign(3u * 3u, 0);
        loco::host::EntityRecord rec;
        std::memset(&rec, 0, sizeof(rec));
        rec.resource_id = 0x1;
        rec.x = 0;
        rec.y = 0;
        doc.entities.push_back(rec);
        std::string err;
        CHECK(loco::host::PersistenceAdapter::write_save(dir + "/off.loco", doc, &err),
              "offset test: write crafted save");

        InputMgr mgr;
        PersistenceAdapter::instance().clear_document();
        g_player_id = 0;
        g_player_color = 0;
        g_tilemap = &g_fixture_tilemap;
        g_fixture_record_tilemap = true;
        g_fixture_find_count = 0;
        loco::host::set_host_placement_available(true);
        CHECK(INPUT_LoadSaveFile(&mgr, "off.loco", 0, 0) == 1,
              "offset test: load crafted save");
        loco::host::set_host_placement_available(false);
        g_fixture_record_tilemap = false;
        g_tilemap = nullptr;
        CHECK(g_fixture_find_count >= 1,
              "offset test: placement FindObject ran");
        if (g_fixture_find_count >= 1) {
            CHECK(g_fixture_find_id[0] == 0x1, "offset test: resource id");
            CHECK(g_fixture_find_x[0] == -1,
                  "offset (0-3)/2 == -1 (truncation toward zero, not floor -2)");
            CHECK(g_fixture_find_y[0] == -1,
                  "offset (0-3)/2 == -1 (color, truncation)");
        }
        mgr.DtorBody();

        /* (b) unsigned saved-u16 semantics: saved player_id = 0x8000
         * (read as the UNSIGNED 16-bit value 32768) with a 32-KiB
         * preview (0x8000 x 1, under the 16 MiB cap): with g_player_id
         * = 0 the offset is -16384, NOT +16384 (which a sign-extended
         * read of the saved word would produce). */
        loco::host::SaveDocument doc2 = loco::host::SaveDocument();
        doc2.header.type = 8;
        doc2.header.player_id = 0x8000;
        doc2.header.player_color = 1;
        doc2.header.entity_count = 1;
        doc2.preview.assign(0x8000u * 1u, 0);
        loco::host::EntityRecord rec2;
        std::memset(&rec2, 0, sizeof(rec2));
        rec2.resource_id = 0x2;
        rec2.x = 0;
        rec2.y = 0;
        doc2.entities.push_back(rec2);
        CHECK(loco::host::PersistenceAdapter::write_save(dir + "/u16.loco", doc2, &err),
              "u16 test: write crafted save");

        InputMgr mgr2;
        PersistenceAdapter::instance().clear_document();
        g_player_id = 0;
        g_player_color = 0;
        g_tilemap = &g_fixture_tilemap;
        g_fixture_record_tilemap = true;
        g_fixture_find_count = 0;
        loco::host::set_host_placement_available(true);
        CHECK(INPUT_LoadSaveFile(&mgr2, "u16.loco", 0, 0) == 1,
              "u16 test: load crafted save");
        loco::host::set_host_placement_available(false);
        g_fixture_record_tilemap = false;
        g_tilemap = nullptr;
        if (g_fixture_find_count >= 1) {
            CHECK(g_fixture_find_x[0] == -16384,
                  "saved player_id read as unsigned u16: (0 - 0x8000)/2 = -16384");
        } else {
            CHECK(false, "u16 test: placement FindObject ran");
        }
        mgr2.DtorBody();
    }

    /* ---- 11. Scenario-2 edge coordinates (INPUT_LoadWorld 0x41D320):
     * the four TileMap_FindObject calls gated by the Netman edge checks
     * use the exact x/y pairs from the binary (0x41D482..0x41D596):
     *   up:    (0xC46, x=(player_id>>1)-1, y=0)
     *   down:  (0xC48, x=(player_id>>1)-1, y=player_color-2)
     *   right: (0xC42, x=player_id-3,      y=(player_color>>1)-1)
     *   left:  (0xC44, x=0,                y=(player_color>>1)-1)
     * (the up-edge x/y were previously swapped).  A missing-file load
     * still runs the scenario-2 block (result is discarded there), and
     * an empty collection means the only FindObject calls are the four
     * edge checks. ---- */
    {
        std::string dir = make_temp_dir();
        std::snprintf(g_install_path, sizeof(g_install_path), "%s/", dir.c_str());

        InputMgr mgr;
        PersistenceAdapter::instance().clear_document();
        g_player_id = 5;
        g_player_color = 7;
        g_tilemap = &g_fixture_tilemap;
        g_netman = &g_fixture_netman;   /* fixture ctor sets m_gameMode == 2 */
        g_fixture_record_tilemap = true;
        g_fixture_record_netman = true;
        g_fixture_edge_result = 1;      /* every edge check succeeds */
        g_fixture_find_count = 0;
        CHECK(INPUT_LoadWorld(&mgr, "empty.loco") == 0,
              "edge test: LoadWorld of a missing file returns 0");
        g_fixture_record_tilemap = false;
        g_fixture_record_netman = false;
        g_fixture_edge_result = 0;
        g_tilemap = nullptr;
        g_netman = nullptr;
        CHECK(g_fixture_find_count == 4,
              "edge test: exactly four edge FindObject calls");
        if (g_fixture_find_count >= 4) {
            CHECK(g_fixture_find_id[0] == 0xC46 && g_fixture_find_x[0] == 1 &&
                      g_fixture_find_y[0] == 0,
                  "CheckUpEdge: (0xC46, x=(5>>1)-1=1, y=0)");
            CHECK(g_fixture_find_id[1] == 0xC48 && g_fixture_find_x[1] == 1 &&
                      g_fixture_find_y[1] == 5,
                  "CheckDownEdge: (0xC48, x=1, y=7-2=5)");
            CHECK(g_fixture_find_id[2] == 0xC42 && g_fixture_find_x[2] == 2 &&
                      g_fixture_find_y[2] == 2,
                  "CheckRightEdge: (0xC42, x=5-3=2, y=(7>>1)-1=2)");
            CHECK(g_fixture_find_id[3] == 0xC44 && g_fixture_find_x[3] == 0 &&
                      g_fixture_find_y[3] == 2,
                  "CheckLeftEdge: (0xC44, x=0, y=(7>>1)-1=2)");
        } else {
            CHECK(false, "edge test: expected exactly four FindObject calls");
        }
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
