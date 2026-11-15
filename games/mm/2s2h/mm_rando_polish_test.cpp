/**
 * ROM-free lock for the 2Ship "rando polish" backport (audit bundle M4): the
 * parts of it that are not drawing. CTest label "redship" (display-free); row
 * `mm-rando-polish` in src/common/test_runner.cpp (MMRandoPolish in
 * CMake/SingleExecutable.cmake).
 *
 *   T  2Ship cfd1116a4 (#1775): the Check Tracker heads Southern Swamp,
 *      Mountain Village, Goron Village, Path to Goron Village and Lone Peak
 *      Shrine without their scene state or "& Grottos"
 *      (Rando::CheckTracker::GetCheckTrackerSceneName), and a scene with no
 *      override keeps Ship_GetSceneName's words.
 *   C  The Combo Tracker's MM pane names the same scenes with the same words
 *      (MMSceneAreaName, through MM_TrackerAdapter_TestSceneAreaName).
 *   S  2Ship 77cbefd28 (#1478): Rando::Spoiler::ApplyToSaveContext refuses a
 *      spoiler whose check names an item no MM item answers to (string and
 *      object forms), keeps a real item (non-vacuity: the check name is one
 *      Apply reads), and keeps accepting the "<item> (Ocarina of Time)" label
 *      the writer gives a cross-game host, which resolves to RI_UNKNOWN by
 *      design (Spoiler/Generate.cpp).
 *
 * The rest of the bundle (the pot model, the soul models' segments, the
 * file-select plate colour, the grayscale souls and the failure toast) is
 * drawing or a live file creation, which no ROM-free row reaches.
 *
 * The S legs write the MM save's rando block; it is restored before return.
 */

#ifndef RSBS_SINGLE_EXECUTABLE
extern "C" int MM_RandoPolish_RunHeadless(void) {
    return 0;
}
#else

#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

#include "Rando/Rando.h"
#include "Rando/CheckTracker/CheckTracker.h"
#include "Rando/Spoiler/Spoiler.h"
#include "Rando/StaticData/StaticData.h"
#include "2s2h/ShipUtils.h"

extern "C" {
#include "variables.h"
}

extern "C" const char* MM_TrackerAdapter_TestSceneAreaName(int sceneId);

namespace {

int sFailures = 0;

void Check(bool ok, const char* what) {
    if (!ok) {
        printf("[TEST] FAIL: %s\n", what);
        sFailures++;
    }
}

struct SceneName {
    SceneId scene;
    const char* name;
};

const SceneName kOverrides[] = {
    { SCENE_20SICHITAI, "Southern Swamp" },   { SCENE_10YUKIYAMANOMURA, "Mountain Village" },
    { SCENE_11GORONNOSATO, "Goron Village" }, { SCENE_17SETUGEN, "Path to Goron Village" },
    { SCENE_KAKUSIANA, "Lone Peak Shrine" },
};

void RunSceneNames() {
    for (const SceneName& row : kOverrides) {
        const std::string native = Rando::CheckTracker::GetCheckTrackerSceneName(row.scene);
        const char* pane = MM_TrackerAdapter_TestSceneAreaName((int)row.scene);
        printf("[TEST] T/C: scene %d: scene name '%s', Check Tracker '%s', combo pane '%s'\n", (int)row.scene,
               Ship_GetSceneName(row.scene), native.c_str(), pane != nullptr ? pane : "(null)");
        Check(native == row.name, "T: the Check Tracker does not head an overridden scene with its plain name");
        Check(pane != nullptr && std::string(pane) == row.name,
              "C: the combo pane does not name an overridden scene as the Check Tracker heads it");
    }
    const std::string field = Rando::CheckTracker::GetCheckTrackerSceneName(SCENE_00KEIKOKU);
    const char* fieldPane = MM_TrackerAdapter_TestSceneAreaName((int)SCENE_00KEIKOKU);
    Check(field == Ship_GetSceneName(SCENE_00KEIKOKU), "T: a scene with no override lost Ship_GetSceneName's name");
    Check(fieldPane != nullptr && field == fieldPane, "C: a scene with no override is named differently in the pane");
}

/** Apply @p spoiler; @return the exception's text, or "" when it applied. */
std::string ApplyAndCatch(const nlohmann::json& spoiler) {
    try {
        Rando::Spoiler::ApplyToSaveContext(spoiler);
    } catch (const std::exception& e) {
        return std::string(e.what()).empty() ? std::string("(empty exception)") : std::string(e.what());
    }
    return "";
}

nlohmann::json SpoilerWithCheck(const char* checkName, const nlohmann::json& entry) {
    nlohmann::json spoiler;
    spoiler["finalSeed"] = 1u;
    spoiler["options"] = nlohmann::json::object(); // every option at its default (Apply's missing-key rule)
    spoiler["startingItems"] = nlohmann::json::array();
    spoiler["checks"] = nlohmann::json::object();
    spoiler["checks"][checkName] = entry;
    return spoiler;
}

void RunSpoilerItems() {
    RandoCheckId checkId = RC_UNKNOWN;
    const char* checkName = nullptr;
    for (auto& [id, staticCheck] : Rando::StaticData::Checks) {
        if (staticCheck.randoCheckId != RC_UNKNOWN && staticCheck.name != nullptr && staticCheck.name[0] != '\0') {
            checkId = id;
            checkName = staticCheck.name;
            break;
        }
    }
    Check(checkName != nullptr, "S: MM's static check table has no named check");
    if (checkName == nullptr) {
        return;
    }
    const char* realItem = Rando::StaticData::Items[RI_SWORD_KOKIRI].spoilerName;
    nlohmann::json shop = nlohmann::json::object();

    // A real item applies (the leg reaches the check Apply reads).
    std::string error = ApplyAndCatch(SpoilerWithCheck(checkName, realItem));
    printf("[TEST] S: '%s' = '%s' -> '%s', item %d\n", checkName, realItem, error.c_str(),
           (int)RANDO_SAVE_CHECKS[checkId].randoItemId);
    Check(error.empty() && RANDO_SAVE_CHECKS[checkId].randoItemId == RI_SWORD_KOKIRI,
          "S: a spoiler naming a real item did not apply it");

    // An item no MM item answers to is refused, in both of the checks map's forms.
    error = ApplyAndCatch(SpoilerWithCheck(checkName, "Not An Item"));
    printf("[TEST] S: string 'Not An Item' -> '%s'\n", error.c_str());
    Check(error.find("Unknown item in spoiler: Not An Item") != std::string::npos,
          "S: a spoiler naming an unknown item (string form) was not refused");
    shop["randoItemId"] = "Not An Item";
    shop["price"] = 10;
    error = ApplyAndCatch(SpoilerWithCheck(checkName, shop));
    printf("[TEST] S: object 'Not An Item' -> '%s'\n", error.c_str());
    Check(error.find("Unknown item in spoiler: Not An Item") != std::string::npos,
          "S: a spoiler naming an unknown item (object form) was not refused");

    // A cross-game host's label still applies, as RI_UNKNOWN for the foreign section to normalize.
    error = ApplyAndCatch(SpoilerWithCheck(checkName, "Kokiri Sword (Ocarina of Time)"));
    printf("[TEST] S: foreign label -> '%s', item %d\n", error.c_str(), (int)RANDO_SAVE_CHECKS[checkId].randoItemId);
    Check(error.empty() && RANDO_SAVE_CHECKS[checkId].randoItemId == RI_UNKNOWN,
          "S: a cross-game host's \"(Ocarina of Time)\" label was refused");
    shop["randoItemId"] = "Kokiri Sword (Ocarina of Time)";
    error = ApplyAndCatch(SpoilerWithCheck(checkName, shop));
    Check(error.empty(), "S: a cross-game shop host's \"(Ocarina of Time)\" label was refused");
}

} // namespace

extern "C" int MM_RandoPolish_RunHeadless(void) {
    printf("[TEST] mm-rando-polish: plain Check Tracker scene names (2Ship #1775) and spoiler item refusal "
           "(2Ship #1478)\n");
    sFailures = 0;

    auto saved = std::make_unique<uint8_t[]>(sizeof(gSaveContext));
    memcpy(saved.get(), &gSaveContext, sizeof(gSaveContext));

    RunSceneNames();
    RunSpoilerItems();

    memcpy(&gSaveContext, saved.get(), sizeof(gSaveContext));

    if (sFailures != 0) {
        printf("[TEST] mm-rando-polish: FAIL (%d)\n", sFailures);
        return sFailures;
    }
    printf("[TEST] mm-rando-polish: PASS\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
