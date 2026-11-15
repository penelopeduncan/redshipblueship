/**
 * @file oot_older_save_meta_test.cpp
 * ROM-free, display-free lock for the SoH #7434 port ("Older saves cannot be
 * read"). CTest label "redship", row OoTOlderSaveMeta in
 * CMake/SingleExecutable.cmake, dispatch "oot-older-save-meta" in
 * src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN. The file select's meta read (SaveManager::
 * StartupCheckAndInitMetaImpl) indexed keys that older fileN.sav files do not
 * have, or have under another name or type: isMagicAcquired and
 * isDoubleMagicAcquired (called magicAcquired and doubleMagic before SoH
 * 5.1.0), isMasterQuest (absent before base version 2, a number rather than a
 * bool in older files), and the whole sohStats section (absent in older files).
 * Each one threw a json type_error, and the caller quarantined a readable save
 * as corrupt.
 *
 * THE LEGS, on a fresh SoH SaveManager through ReadMetaForTest (the meta read
 * without the quarantine, whose popup needs the GUI):
 *   1. control: a current-format vanilla file reads every field;
 *   2. magicAcquired / doubleMagic instead of the is* names;
 *   3. isMasterQuest stored as the number 1 means a Master Quest file;
 *   4. no isMasterQuest at all means a vanilla-quest file;
 *   5. no sohStats section: build version 0.0.0 and an empty string;
 *   6. a sohStats section whose "data" is null: the same.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "SaveManager.h"
#include "Enhancements/game-interactor/GameInteractor.h"
#include "test_scratch.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <string>

extern "C" void OoT_SaveManager_SetSavDirectoryForTest(const char* dir);

namespace {

#define OSM_ASSERT(cond, ...)                         \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            failures++;                               \
        }                                             \
    } while (0)

// A current-format vanilla fileN.sav, cut down to what the meta read looks at.
nlohmann::json CurrentFormatSave() {
    nlohmann::json items = nlohmann::json::array();
    for (int i = 0; i < 24; i++) {
        items.push_back(0xFF);
    }
    nlohmann::json base = {
        { "deaths", 3 },
        { "playerName", { 0x15, 0x2C, 0x2D, 0x2E, 0x3E, 0x3E, 0x3E, 0x3E } },
        { "healthCapacity", 0x50 },
        { "inventory",
          { { "questItems", 0x1234 },
            { "items", items },
            { "equipment", 0x1111 },
            { "upgrades", 0x2222 },
            { "gsTokens", 7 },
            { "defenseHearts", 0 } } },
        { "isMagicAcquired", 1 },
        { "isDoubleMagicAcquired", 1 },
        { "rupees", 42 },
        { "isDoubleDefenseAcquired", 0 },
        { "health", 0x30 },
        { "isMasterQuest", false },
    };
    nlohmann::json stats = {
        { "buildVersionMajor", 7 },
        { "buildVersionMinor", 8 },
        { "buildVersionPatch", 9 },
        { "buildVersion", "Test Build" },
    };
    return {
        { "version", 1 },
        { "fileType", 0 },
        { "sections",
          { { "base", { { "version", 4 }, { "data", base } } },
            { "sohStats", { { "version", 1 }, { "data", stats } } } } },
    };
}

// Run the meta read on @p save; false (with the reason printed) when it threw.
bool ReadMeta(SaveManager* mgr, nlohmann::json save, std::string* error) {
    std::memset(&mgr->fileMetaInfo[0], 0xAB, sizeof(SaveFileMetaInfo));
    try {
        mgr->ReadMetaForTest(0, save);
    } catch (const std::exception& e) {
        *error = e.what();
        return false;
    }
    return true;
}

} // namespace

extern "C" int OoT_OlderSaveMeta_RunHeadless(void) {
    printf("[TEST] oot-older-save-meta: the file select reads older OoT saves (old magic names, numeric or absent "
           "isMasterQuest, no sohStats) instead of throwing (SoH #7434)\n");
    int failures = 0;

    const std::filesystem::path dir = rsbs::test::ScratchDir("rsbs_oot_older_save_meta_test");
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    OoT_SaveManager_SetSavDirectoryForTest(dir.string().c_str());
    struct SavDirectoryReset {
        ~SavDirectoryReset() {
            OoT_SaveManager_SetSavDirectoryForTest(nullptr);
        }
    } savDirectoryReset;

    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    SaveManager* mgr = new SaveManager(); // kept alive, like every other *::Instance singleton
    const SaveFileMetaInfo& meta = mgr->fileMetaInfo[0];
    std::string error;

    // ---- 1. control: the current format -------------------------------------
    {
        const bool ok = ReadMeta(mgr, CurrentFormatSave(), &error);
        printf("[TEST] leg 1 OBSERVED: read %s, valid %d, magic %d/%d, MQ %u original %u, build %d.%d.%d \"%s\"\n",
               ok ? "ok" : error.c_str(), meta.valid, meta.isMagicAcquired, meta.isDoubleMagicAcquired,
               meta.requiresMasterQuest, meta.requiresOriginal, meta.buildVersionMajor, meta.buildVersionMinor,
               meta.buildVersionPatch, ok ? meta.buildVersion : "");
        OSM_ASSERT(ok && meta.valid == 1, "leg 1: a current-format save did not read (%s)", error.c_str());
        OSM_ASSERT(ok && meta.isMagicAcquired == 1 && meta.isDoubleMagicAcquired == 1 && meta.rupees == 42 &&
                       meta.deaths == 3,
                   "leg 1: a current-format save's fields were not read");
        OSM_ASSERT(ok && meta.requiresMasterQuest == 0 && meta.requiresOriginal == 1,
                   "leg 1: a bool false isMasterQuest did not read as a vanilla-quest file");
        OSM_ASSERT(ok && meta.buildVersionMajor == 7 && meta.buildVersionMinor == 8 && meta.buildVersionPatch == 9 &&
                       std::strcmp(meta.buildVersion, "Test Build") == 0,
                   "leg 1: the sohStats build version was not read");
    }

    // ---- 2. pre-5.1.0 magic names -------------------------------------------
    {
        nlohmann::json save = CurrentFormatSave();
        nlohmann::json& base = save["sections"]["base"]["data"];
        base.erase("isMagicAcquired");
        base.erase("isDoubleMagicAcquired");
        base["magicAcquired"] = 1;
        base["doubleMagic"] = 1;
        error.clear();
        const bool ok = ReadMeta(mgr, save, &error);
        printf("[TEST] leg 2 OBSERVED: magicAcquired/doubleMagic: read %s, magic %d/%d\n", ok ? "ok" : error.c_str(),
               meta.isMagicAcquired, meta.isDoubleMagicAcquired);
        OSM_ASSERT(ok, "leg 2: a save with the pre-5.1.0 magic names threw (%s)", error.c_str());
        OSM_ASSERT(ok && meta.isMagicAcquired == 1 && meta.isDoubleMagicAcquired == 1,
                   "leg 2: magicAcquired/doubleMagic were not read as the magic flags");
    }

    // ---- 3. isMasterQuest as a number ---------------------------------------
    {
        nlohmann::json save = CurrentFormatSave();
        save["sections"]["base"]["data"]["isMasterQuest"] = 1;
        error.clear();
        const bool ok = ReadMeta(mgr, save, &error);
        printf("[TEST] leg 3 OBSERVED: isMasterQuest 1: read %s, MQ %u original %u\n", ok ? "ok" : error.c_str(),
               meta.requiresMasterQuest, meta.requiresOriginal);
        OSM_ASSERT(ok, "leg 3: a numeric isMasterQuest threw (%s)", error.c_str());
        OSM_ASSERT(ok && meta.requiresMasterQuest == 1 && meta.requiresOriginal == 0,
                   "leg 3: a numeric isMasterQuest of 1 did not read as a Master Quest file");
    }

    // ---- 4. no isMasterQuest (base version 1) -------------------------------
    {
        nlohmann::json save = CurrentFormatSave();
        save["sections"]["base"]["data"].erase("isMasterQuest");
        error.clear();
        const bool ok = ReadMeta(mgr, save, &error);
        printf("[TEST] leg 4 OBSERVED: no isMasterQuest: read %s, MQ %u original %u\n", ok ? "ok" : error.c_str(),
               meta.requiresMasterQuest, meta.requiresOriginal);
        OSM_ASSERT(ok, "leg 4: a save without isMasterQuest threw (%s)", error.c_str());
        OSM_ASSERT(ok && meta.requiresMasterQuest == 0 && meta.requiresOriginal == 1,
                   "leg 4: a save without isMasterQuest did not read as a vanilla-quest file");
    }

    // ---- 5. no sohStats section ---------------------------------------------
    {
        nlohmann::json save = CurrentFormatSave();
        save["sections"].erase("sohStats");
        error.clear();
        const bool ok = ReadMeta(mgr, save, &error);
        printf("[TEST] leg 5 OBSERVED: no sohStats: read %s, valid %d, build %d.%d.%d \"%s\"\n",
               ok ? "ok" : error.c_str(), meta.valid, meta.buildVersionMajor, meta.buildVersionMinor,
               meta.buildVersionPatch, ok ? meta.buildVersion : "");
        OSM_ASSERT(ok, "leg 5: a save without a sohStats section threw (%s)", error.c_str());
        OSM_ASSERT(ok && meta.valid == 1 && meta.buildVersionMajor == 0 && meta.buildVersionMinor == 0 &&
                       meta.buildVersionPatch == 0 && meta.buildVersion[0] == '\0',
                   "leg 5: a save without sohStats did not read as build 0.0.0 with an empty build string");
    }

    // ---- 6. sohStats with a null "data" -------------------------------------
    {
        nlohmann::json save = CurrentFormatSave();
        save["sections"]["sohStats"]["data"] = nullptr;
        error.clear();
        const bool ok = ReadMeta(mgr, save, &error);
        printf("[TEST] leg 6 OBSERVED: sohStats data null: read %s, build %d.%d.%d\n", ok ? "ok" : error.c_str(),
               meta.buildVersionMajor, meta.buildVersionMinor, meta.buildVersionPatch);
        OSM_ASSERT(ok, "leg 6: a sohStats section with null data threw (%s)", error.c_str());
        OSM_ASSERT(ok && meta.buildVersionMajor == 0 && meta.buildVersionMinor == 0 && meta.buildVersionPatch == 0 &&
                       meta.buildVersion[0] == '\0',
                   "leg 6: a sohStats section with null data did not read as build 0.0.0");
    }

    std::filesystem::remove_all(dir, ec);
    printf("[TEST] oot-older-save-meta: %s (%d failure(s))\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures;
}

#endif // RSBS_SINGLE_EXECUTABLE
