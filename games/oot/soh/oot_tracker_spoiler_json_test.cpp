/**
 * @file oot_tracker_spoiler_json_test.cpp
 * Locks for #1748:
 *  - OoTSpoilerJsonSections (`redship --test oot-spoiler-json-sections`):
 *    SoH 29948f27a (upstream PR 6918) const-reference JSON parser signatures,
 *    missing-section tolerance via .value(..., nlohmann::json()), and required
 *    "seed" lookup via .at("seed").
 *  - OoTItemTrackerGatedSections (`redship --test oot-item-tracker-gated-sections`):
 *    SoH 18f8770ea (upstream PR 7151) GetGatedSectionDisplay gating of Greg,
 *    Jabber Nuts, and Fishing Pole sections in UpdateVectors().
 *
 * Rando-tier rows using TrackerReachabilityBringUp (same bring-up as
 * OoTLogicGates), dispatched in src/common/test_runner.cpp.
 */

#include <z64.h>
#include "variables.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/dungeon.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/randomizer_item_tracker.h"
#include "soh/Enhancements/randomizer/settings.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/trial.h"
#include "soh/cvar_prefixes.h"

#include <libultraship/bridge/consolevariablebridge.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

// Non-static globals from games/oot/soh/Enhancements/randomizer/randomizer_item_tracker.cpp.
void UpdateVectors();
extern bool shouldUpdateVectors;
extern std::vector<ItemTrackerItem> mainWindowItems;
extern std::vector<ItemTrackerItem> jabbernutItems;
extern std::vector<ItemTrackerItem> fishingPoleItems;
extern std::vector<ItemTrackerItem> gregItems;

namespace {

int sTsjFailures = 0;

void TsjCheck(const char* leg, bool cond, const char* detail) {
    if (!cond) {
        std::printf("[TEST] FAIL (%s): %s\n", leg, detail);
        ++sTsjFailures;
        return;
    }
    std::printf("[TEST]   ok (%s): %s\n", leg, detail);
}

std::shared_ptr<Rando::Context> TsjEnsureContext() {
    auto randoCtx = Rando::Context::GetInstance();
    if (!randoCtx) {
        randoCtx = Rando::Context::CreateInstance();
        randoCtx->InitStaticData();
        Rando::Settings::GetInstance()->AssignContext(randoCtx);
        Rando::StaticData::InitItemTable();
        Rando::StaticData::InitLocationTable();
    }
    return randoCtx;
}

size_t TsjCountSectionIds(const std::vector<ItemTrackerItem>& section) {
    size_t count = 0;
    for (const ItemTrackerItem& item : section) {
        const bool found = std::any_of(mainWindowItems.begin(), mainWindowItems.end(),
                                       [&item](const ItemTrackerItem& entry) { return entry.id == item.id; });
        if (found) {
            ++count;
        }
    }
    return count;
}

void TsjRunUpdate() {
    shouldUpdateVectors = true;
    UpdateVectors();
}

} // namespace

/**
 * @return 0 when every leg holds, 1 otherwise.
 */
extern "C" int OoT_SpoilerJsonSections_RunHeadless(void) {
    sTsjFailures = 0;

    const std::shared_ptr<Rando::Context> randoCtx = TsjEnsureContext();
    const nlohmann::json emptyObj = nlohmann::json::object();

    // (a) Dungeons: missing masterQuestDungeons clears MQ without throwing; named dungeon sets MQ.
    {
        const std::shared_ptr<Rando::Dungeons> dungeons = randoCtx->GetDungeons();
        const std::array<Rando::DungeonInfo*, 12> list = dungeons->GetDungeonList();
        std::array<bool, 12> savedMq = {};
        for (size_t i = 0; i < list.size(); ++i) {
            savedMq[i] = list[i]->IsMQ();
        }

        list[0]->SetMQ();
        TsjCheck("SJ-a0", list[0]->IsMQ(), "dungeon 0 marked MQ before empty ParseJson");

        bool threw = false;
        try {
            dungeons->ParseJson(emptyObj);
        } catch (...) {
            threw = true;
        }
        TsjCheck("SJ-a1", !threw, "Dungeons::ParseJson on empty const json does not throw");
        TsjCheck("SJ-a2", !list[0]->IsMQ(), "Dungeons::ParseJson on empty object clears MQ flag");

        const nlohmann::json mqDoc =
            nlohmann::json::object({ { "masterQuestDungeons", nlohmann::json::array({ list[0]->GetName() }) } });
        threw = false;
        try {
            dungeons->ParseJson(mqDoc);
        } catch (...) {
            threw = true;
        }
        TsjCheck("SJ-a3", !threw && list[0]->IsMQ(), "Dungeons::ParseJson with dungeon 0 name sets MQ");

        for (size_t i = 0; i < list.size(); ++i) {
            if (savedMq[i]) {
                list[i]->SetMQ();
            } else {
                list[i]->ClearMQ();
            }
        }
    }

    // (b) Trials: missing requiredTrials marks every trial skipped without throwing.
    {
        const std::shared_ptr<Rando::Trials> trials = randoCtx->GetTrials();
        const std::vector<Rando::TrialInfo*> list = trials->GetTrialList();
        std::vector<bool> savedReq(list.size(), false);
        for (size_t i = 0; i < list.size(); ++i) {
            savedReq[i] = list[i]->IsRequired();
        }

        list[0]->SetAsRequired();
        TsjCheck("SJ-b0", list[0]->IsRequired(), "trial 0 marked required before empty ParseJson");

        bool threw = false;
        try {
            trials->ParseJson(emptyObj);
        } catch (...) {
            threw = true;
        }
        bool allSkipped = true;
        for (const Rando::TrialInfo* trial : list) {
            if (!trial->IsSkipped()) {
                allSkipped = false;
                break;
            }
        }
        TsjCheck("SJ-b1", !threw, "Trials::ParseJson on empty const json does not throw");
        TsjCheck("SJ-b2", allSkipped, "Trials::ParseJson on empty object skips every trial");

        for (size_t i = 0; i < list.size(); ++i) {
            if (savedReq[i]) {
                list[i]->SetAsRequired();
            } else {
                list[i]->SetAsSkipped();
            }
        }
    }

    // (c) Settings: seed/finalSeed succeed with missing optional sections; missing "seed" throws.
    {
        Rando::Settings* settings = Rando::Settings::GetInstance();
        settings->AssignContext(randoCtx);
        const std::string savedSeedString = randoCtx->GetSeedString();
        const uint32_t savedSeed = randoCtx->GetSeed();

        const nlohmann::json seedDoc = nlohmann::json::object({ { "seed", "rsbs-1748" }, { "finalSeed", 7 } });
        bool threw = false;
        try {
            settings->ParseJson(seedDoc);
        } catch (...) {
            threw = true;
        }
        TsjCheck("SJ-c1", !threw, "Settings::ParseJson with seed and finalSeed only does not throw");
        TsjCheck("SJ-c2", randoCtx->GetSeedString() == "rsbs-1748" && randoCtx->GetSeed() == 7,
                 "Settings::ParseJson sets seed string and finalSeed");

        const nlohmann::json missingSeedDoc = nlohmann::json::object({ { "finalSeed", 7 } });
        bool missingThrew = false;
        try {
            settings->ParseJson(missingSeedDoc);
        } catch (...) {
            missingThrew = true;
        }
        TsjCheck("SJ-c3", missingThrew, "Settings::ParseJson without \"seed\" throws");

        randoCtx->SetSeedString(savedSeedString);
        randoCtx->SetSeed(savedSeed);
    }

    // (d) Context JSON section parsers accept an empty const object without throwing.
    {
        bool locThrew = false;
        try {
            randoCtx->ParseItemLocationsJson(emptyObj);
        } catch (...) {
            locThrew = true;
        }
        TsjCheck("SJ-d1", !locThrew, "Context::ParseItemLocationsJson on empty const json does not throw");

        bool tricksThrew = false;
        try {
            randoCtx->ParseTricksJson(emptyObj);
        } catch (...) {
            tricksThrew = true;
        }
        TsjCheck("SJ-d2", !tricksThrew, "Context::ParseTricksJson on empty const json does not throw");

        bool hashThrew = false;
        try {
            randoCtx->ParseHashIconIndexesJson(emptyObj);
        } catch (...) {
            hashThrew = true;
        }
        TsjCheck("SJ-d3", !hashThrew, "Context::ParseHashIconIndexesJson on empty const json does not throw");
    }

    if (sTsjFailures != 0) {
        std::printf("[TEST] FAIL (oot-spoiler-json-sections): %d failure(s)\n", sTsjFailures);
        return 1;
    }
    std::printf("[TEST] PASS: oot-spoiler-json-sections\n");
    return 0;
}

/**
 * @return 0 when every leg holds, 1 otherwise.
 */
extern "C" int OoT_ItemTrackerGatedSections_RunHeadless(void) {
    sTsjFailures = 0;

    const std::shared_ptr<Rando::Context> randoCtx = TsjEnsureContext();
    const auto savedQuestId = gSaveContext.ship.quest.id;
    const uint8_t savedShuffleSpeak = randoCtx->GetOption(RSK_SHUFFLE_SPEAK).Get();
    const uint8_t savedShuffleFishingPole = randoCtx->GetOption(RSK_SHUFFLE_FISHING_POLE).Get();

    const char* kJabberNutsKey = CVAR_TRACKER_ITEM("DisplayType.JabberNuts");
    const char* kFishingPoleKey = CVAR_TRACKER_ITEM("DisplayType.FishingPole");
    const char* kGregKey = CVAR_TRACKER_ITEM("DisplayType.Greg");

    // Start from cleared keys so unrelated sections do not interfere.
    CVarClear(kJabberNutsKey);
    CVarClear(kFishingPoleKey);
    CVarClear(kGregKey);

    // 1. Jabber Nuts: gated by IS_RANDO && RSK_SHUFFLE_SPEAK.
    CVarSetInteger(kJabberNutsKey, 1);
    gSaveContext.ship.quest.id = QUEST_NORMAL;
    randoCtx->GetOption(RSK_SHUFFLE_SPEAK).Set(1);
    TsjRunUpdate();
    TsjCheck("IT-j1", TsjCountSectionIds(jabbernutItems) == 0, "Jabber Nuts absent in vanilla file");

    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    randoCtx->GetOption(RSK_SHUFFLE_SPEAK).Set(0);
    TsjRunUpdate();
    TsjCheck("IT-j2", TsjCountSectionIds(jabbernutItems) == 0,
             "Jabber Nuts absent in rando file when RSK_SHUFFLE_SPEAK is 0");

    randoCtx->GetOption(RSK_SHUFFLE_SPEAK).Set(1);
    TsjRunUpdate();
    TsjCheck("IT-j3", TsjCountSectionIds(jabbernutItems) == jabbernutItems.size(),
             "Jabber Nuts present in full in rando file when RSK_SHUFFLE_SPEAK is 1");

    CVarClear(kJabberNutsKey);
    TsjRunUpdate();
    TsjCheck("IT-j4", TsjCountSectionIds(jabbernutItems) == 0,
             "Jabber Nuts absent when DisplayType.JabberNuts is cleared");

    // 2. Fishing Pole: gated by IS_RANDO && RSK_SHUFFLE_FISHING_POLE.
    CVarSetInteger(kFishingPoleKey, 1);
    gSaveContext.ship.quest.id = QUEST_NORMAL;
    randoCtx->GetOption(RSK_SHUFFLE_FISHING_POLE).Set(1);
    TsjRunUpdate();
    TsjCheck("IT-f1", TsjCountSectionIds(fishingPoleItems) == 0, "Fishing Pole absent in vanilla file");

    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    randoCtx->GetOption(RSK_SHUFFLE_FISHING_POLE).Set(0);
    TsjRunUpdate();
    TsjCheck("IT-f2", TsjCountSectionIds(fishingPoleItems) == 0,
             "Fishing Pole absent in rando file when RSK_SHUFFLE_FISHING_POLE is 0");

    randoCtx->GetOption(RSK_SHUFFLE_FISHING_POLE).Set(1);
    TsjRunUpdate();
    TsjCheck("IT-f3", TsjCountSectionIds(fishingPoleItems) == fishingPoleItems.size(),
             "Fishing Pole present in full in rando file when RSK_SHUFFLE_FISHING_POLE is 1");

    CVarClear(kFishingPoleKey);
    TsjRunUpdate();
    TsjCheck("IT-f4", TsjCountSectionIds(fishingPoleItems) == 0,
             "Fishing Pole absent when DisplayType.FishingPole is cleared");

    // 3. Greg: gated by IS_RANDO alone.
    CVarSetInteger(kGregKey, 1);
    gSaveContext.ship.quest.id = QUEST_NORMAL;
    TsjRunUpdate();
    TsjCheck("IT-g1", TsjCountSectionIds(gregItems) == 0, "Greg absent in vanilla file");

    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    TsjRunUpdate();
    TsjCheck("IT-g2", TsjCountSectionIds(gregItems) == gregItems.size(), "Greg present in rando file");

    CVarClear(kGregKey);
    TsjRunUpdate();
    TsjCheck("IT-g3", TsjCountSectionIds(gregItems) == 0, "Greg absent when DisplayType.Greg is cleared");

    // Restore: clear keys (never store), restore options and quest id, mark vectors dirty.
    CVarClear(kJabberNutsKey);
    CVarClear(kFishingPoleKey);
    CVarClear(kGregKey);
    randoCtx->GetOption(RSK_SHUFFLE_SPEAK).Set(savedShuffleSpeak);
    randoCtx->GetOption(RSK_SHUFFLE_FISHING_POLE).Set(savedShuffleFishingPole);
    gSaveContext.ship.quest.id = savedQuestId;
    shouldUpdateVectors = true;

    if (sTsjFailures != 0) {
        std::printf("[TEST] FAIL (oot-item-tracker-gated-sections): %d failure(s)\n", sTsjFailures);
        return 1;
    }
    std::printf("[TEST] PASS: oot-item-tracker-gated-sections\n");
    return 0;
}
