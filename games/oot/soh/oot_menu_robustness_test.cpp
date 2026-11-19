/**
 * @file oot_menu_robustness_test.cpp
 * @brief Backport packet #1733: menu robustness tests (PRs 6228, 6352, 6817, 6771, 6705).
 *        Row OoTMenuRobustness (CTest label `redship`), dispatch `oot-menu-robustness`
 *        in src/common/test_runner.cpp. ROM-free and display-free.
 *
 *   (a) ExcludedLocations CVar string is serialized without a trailing comma while
 *       legacy trailing-comma configs parse to the exact same check set, and empty
 *       strings / empty sets round-trip cleanly.
 *   (b) Locations page refresh detects MQ setting/count changes and save loads, and
 *       IsQuestVisible + RandomizerCheckObjects::UpdateImGuiVisibility() expose MQ
 *       checks under Selection Only and All MQ while hiding vanilla/MQ properly.
 *   (c) BuildAvailableAudioBackendsMap filters offered Audio API entries strictly to
 *       the platform's available backend vector (no WASAPI when only SDL/NUL exist).
 *   (d) UIWidgets::GetVisibleLabel strips `#` / `##` ImGui ID suffixes for sliders.
 */

#include <cstdint>
#include <cstdio>
#include <exception>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <libultraship/bridge.h>
#include <libultraship/libultraship.h>

#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/settings.h"
#include "soh/Enhancements/randomizer/item_location.h"
#include "soh/Enhancements/randomizer/randomizer_check_objects.h"

extern "C" {
// entrance.cpp: ROM-free region graph + Rando::Context bring-up.
int Rando_InitRegionGraphForTest(void);
}

// SohMenuRandomizer.cpp: pure helpers for Locations menu serialization and refresh.
namespace SohGui {
int32_t PackMQDungeonSetting(int32_t mqDungeons, int32_t mqDungeonCount);
bool ShouldRefreshLocationsMenu(bool dirty, int32_t currMQSetting, int32_t prevMQSetting, bool saveLoaded);
std::string FormatExcludedLocations(const std::set<RandomizerCheck>& excluded);
std::set<RandomizerCheck> ParseExcludedLocations(const char* raw);
} // namespace SohGui

// Menu.cpp: pure helper filtering Audio API combobox entries to available backends.
namespace Ship {
std::map<Ship::AudioBackend, const char*>
BuildAvailableAudioBackendsMap(const std::vector<Ship::AudioBackend>& availableBackends);
} // namespace Ship

// UIWidgets.cpp: slider label suffix stripper.
namespace UIWidgets {
std::string GetVisibleLabel(const char* label);
} // namespace UIWidgets

namespace {

int sFailures = 0;

#define MR_CHECK(cond, msg)                                                                    \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            printf("[TEST] oot-menu-robustness: FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            sFailures++;                                                                       \
        }                                                                                      \
    } while (0)

struct SavedIntCVar {
    const char* key;
    bool existed;
    int32_t value;

    void Save(const char* cvarKey) {
        key = cvarKey;
        existed = CVarExists(cvarKey);
        value = CVarGetInteger(cvarKey, 0);
    }

    void Restore() const {
        if (existed) {
            CVarSetInteger(key, value);
        } else {
            CVarClear(key);
        }
    }
};

void CheckExcludedLocationsRoundTrip() {
    printf("[TEST] oot-menu-robustness: leg (a) ExcludedLocations round-trip\n");

    const std::set<RandomizerCheck> emptySet;
    const std::string emptyFormatted = SohGui::FormatExcludedLocations(emptySet);
    MR_CHECK(emptyFormatted.empty(), "empty excluded set must serialize to an empty string");
    MR_CHECK(SohGui::ParseExcludedLocations(emptyFormatted.c_str()).empty(),
             "empty formatted excluded string must parse back to empty set");
    MR_CHECK(SohGui::ParseExcludedLocations("").empty(), "empty literal string must parse to empty set");
    MR_CHECK(SohGui::ParseExcludedLocations(nullptr).empty(), "null pointer must parse to empty set");

    const std::set<RandomizerCheck> singleSet = { RC_KF_KOKIRI_SWORD_CHEST };
    const std::string singleFormatted = SohGui::FormatExcludedLocations(singleSet);
    const std::string singleExpected = std::to_string(RC_KF_KOKIRI_SWORD_CHEST);
    MR_CHECK(singleFormatted == singleExpected, "single excluded check must serialize without comma");
    MR_CHECK(!singleFormatted.empty() && singleFormatted.back() != ',',
             "single excluded check string must not end with trailing comma");
    const std::string singleLegacy = singleFormatted + ",";
    MR_CHECK(SohGui::ParseExcludedLocations(singleLegacy.c_str()) == singleSet,
             "legacy trailing-comma single excluded check must parse to the same set");

    const std::set<RandomizerCheck> multiSet = {
        RC_KF_KOKIRI_SWORD_CHEST,
        RC_DEKU_TREE_SLINGSHOT_CHEST,
        RC_DODONGOS_CAVERN_BOSS_ROOM_CHEST,
    };
    const std::string multiFormatted = SohGui::FormatExcludedLocations(multiSet);
    MR_CHECK(!multiFormatted.empty() && multiFormatted.back() != ',',
             "multi excluded list must serialize without a trailing comma");
    MR_CHECK(multiFormatted.front() != ',', "multi excluded list must not have a leading comma");
    MR_CHECK(SohGui::ParseExcludedLocations(multiFormatted.c_str()) == multiSet,
             "non-trailing-comma excluded list must round-trip cleanly");

    const std::string multiLegacy = multiFormatted + ",";
    MR_CHECK(SohGui::ParseExcludedLocations(multiLegacy.c_str()) == multiSet,
             "legacy trailing-comma multi excluded list must parse to the exact same set");

    // An empty field (a leading comma, a doubled comma) is skipped rather than
    // handed to std::stoi, which would throw out of the menu draw.
    const std::string emptyFields = "," + std::to_string(RC_KF_KOKIRI_SWORD_CHEST) + ",," +
                                    std::to_string(RC_DEKU_TREE_SLINGSHOT_CHEST) + ",," +
                                    std::to_string(RC_DODONGOS_CAVERN_BOSS_ROOM_CHEST) + ",";
    bool emptyFieldsParsed = false;
    try {
        emptyFieldsParsed = SohGui::ParseExcludedLocations(emptyFields.c_str()) == multiSet;
    } catch (const std::exception& e) {
        printf("[TEST] oot-menu-robustness: the parse threw on empty fields (%s)\n", e.what());
    }
    MR_CHECK(emptyFieldsParsed, "an excluded list with empty fields must parse to the same set without throwing");
}

void CheckMQLocationsVisibility() {
    printf("[TEST] oot-menu-robustness: leg (b) MQ Dungeons Locations visibility\n");

    const int32_t packedNone = SohGui::PackMQDungeonSetting(RO_MQ_DUNGEONS_NONE, 0);
    const int32_t packedSelection = SohGui::PackMQDungeonSetting(RO_MQ_DUNGEONS_SELECTION, 0);
    const int32_t packedSetZero = SohGui::PackMQDungeonSetting(RO_MQ_DUNGEONS_SET_NUMBER, 0);
    const int32_t packedSetOne = SohGui::PackMQDungeonSetting(RO_MQ_DUNGEONS_SET_NUMBER, 1);

    MR_CHECK(packedNone != packedSelection, "None and Selection Only must produce distinct packed MQ keys");
    MR_CHECK(packedSetZero != packedSetOne, "MQ dungeon count changes must change packed MQ key");

    MR_CHECK(!SohGui::ShouldRefreshLocationsMenu(false, packedNone, packedNone, false),
             "Locations menu should not refresh when clean, unchanged, and no save loaded");
    MR_CHECK(SohGui::ShouldRefreshLocationsMenu(true, packedNone, packedNone, false),
             "Locations menu should refresh when dirty");
    MR_CHECK(SohGui::ShouldRefreshLocationsMenu(false, packedSelection, packedNone, false),
             "Locations menu should refresh when MQ setting changes");
    MR_CHECK(SohGui::ShouldRefreshLocationsMenu(false, packedSetOne, packedSetZero, false),
             "Locations menu should refresh when MQ dungeon count changes");
    MR_CHECK(SohGui::ShouldRefreshLocationsMenu(false, packedNone, packedNone, true),
             "Locations menu should refresh when save is loaded");

    // Pure predicate checks across None / Selection Only / Set Number (0, 5, 12)
    MR_CHECK(RandomizerCheckObjects::IsQuestVisible(RCQUEST_VANILLA, RO_MQ_DUNGEONS_NONE, 0),
             "None: vanilla check visible");
    MR_CHECK(!RandomizerCheckObjects::IsQuestVisible(RCQUEST_MQ, RO_MQ_DUNGEONS_NONE, 0), "None: MQ check hidden");
    MR_CHECK(RandomizerCheckObjects::IsQuestVisible(RCQUEST_BOTH, RO_MQ_DUNGEONS_NONE, 0), "None: Both check visible");

    MR_CHECK(RandomizerCheckObjects::IsQuestVisible(RCQUEST_VANILLA, RO_MQ_DUNGEONS_SELECTION, 0),
             "Selection Only: vanilla check visible");
    MR_CHECK(RandomizerCheckObjects::IsQuestVisible(RCQUEST_MQ, RO_MQ_DUNGEONS_SELECTION, 0),
             "Selection Only: MQ check visible");
    MR_CHECK(RandomizerCheckObjects::IsQuestVisible(RCQUEST_BOTH, RO_MQ_DUNGEONS_SELECTION, 0),
             "Selection Only: Both check visible");

    MR_CHECK(!RandomizerCheckObjects::IsQuestVisible(RCQUEST_VANILLA, RO_MQ_DUNGEONS_SET_NUMBER, 12),
             "Set Number 12 (All): vanilla check hidden");
    MR_CHECK(RandomizerCheckObjects::IsQuestVisible(RCQUEST_MQ, RO_MQ_DUNGEONS_SET_NUMBER, 12),
             "Set Number 12 (All): MQ check visible");
    MR_CHECK(RandomizerCheckObjects::IsQuestVisible(RCQUEST_BOTH, RO_MQ_DUNGEONS_SET_NUMBER, 12),
             "Set Number 12 (All): Both check visible");

    MR_CHECK(RandomizerCheckObjects::IsQuestVisible(RCQUEST_VANILLA, RO_MQ_DUNGEONS_SET_NUMBER, 0),
             "Set Number 0: vanilla check visible");
    MR_CHECK(!RandomizerCheckObjects::IsQuestVisible(RCQUEST_MQ, RO_MQ_DUNGEONS_SET_NUMBER, 0),
             "Set Number 0: MQ check hidden");

    MR_CHECK(RandomizerCheckObjects::IsQuestVisible(RCQUEST_VANILLA, RO_MQ_DUNGEONS_SET_NUMBER, 5),
             "Set Number 5: vanilla check visible");
    MR_CHECK(RandomizerCheckObjects::IsQuestVisible(RCQUEST_MQ, RO_MQ_DUNGEONS_SET_NUMBER, 5),
             "Set Number 5: MQ check visible");

    // Live UpdateImGuiVisibility() over Rando::Context + Rando::StaticData
    auto randoCtx = Rando::Context::GetInstance();
    if (!randoCtx) {
        randoCtx = Rando::Context::CreateInstance();
        randoCtx->InitStaticData();
    }
    if (Rando_InitRegionGraphForTest() != 0 || Rando::Context::GetInstance() == nullptr) {
        MR_CHECK(false, "Rando_InitRegionGraphForTest failed");
        return;
    }
    Rando::Settings::GetInstance()->CreateOptions();
    if (Rando::StaticData::GetLocation(RC_DEKU_TREE_SLINGSHOT_CHEST)->GetName().empty()) {
        Rando::StaticData::InitLocationTable();
    }

    SavedIntCVar savedMQDungeons;
    SavedIntCVar savedMQCount;
    savedMQDungeons.Save(CVAR_RANDOMIZER_SETTING("MQDungeons"));
    savedMQCount.Save(CVAR_RANDOMIZER_SETTING("MQDungeonCount"));

    CVarSetInteger(CVAR_RANDOMIZER_SETTING("MQDungeons"), RO_MQ_DUNGEONS_NONE);
    CVarSetInteger(CVAR_RANDOMIZER_SETTING("MQDungeonCount"), 0);
    RandomizerCheckObjects::UpdateImGuiVisibility();
    MR_CHECK(randoCtx->GetItemLocation(RC_DEKU_TREE_SLINGSHOT_CHEST)->IsVisible(),
             "UpdateImGuiVisibility None: vanilla Deku chest visible");
    MR_CHECK(!randoCtx->GetItemLocation(RC_DEKU_TREE_MQ_SLINGSHOT_CHEST)->IsVisible(),
             "UpdateImGuiVisibility None: MQ Deku chest hidden");
    MR_CHECK(randoCtx->GetItemLocation(RC_DODONGOS_CAVERN_BOSS_ROOM_CHEST)->IsVisible(),
             "UpdateImGuiVisibility None: Both check visible");

    CVarSetInteger(CVAR_RANDOMIZER_SETTING("MQDungeons"), RO_MQ_DUNGEONS_SELECTION);
    CVarSetInteger(CVAR_RANDOMIZER_SETTING("MQDungeonCount"), 0);
    RandomizerCheckObjects::UpdateImGuiVisibility();
    MR_CHECK(randoCtx->GetItemLocation(RC_DEKU_TREE_SLINGSHOT_CHEST)->IsVisible(),
             "UpdateImGuiVisibility Selection: vanilla Deku chest visible");
    MR_CHECK(randoCtx->GetItemLocation(RC_DEKU_TREE_MQ_SLINGSHOT_CHEST)->IsVisible(),
             "UpdateImGuiVisibility Selection: MQ Deku chest visible");
    MR_CHECK(randoCtx->GetItemLocation(RC_DODONGOS_CAVERN_BOSS_ROOM_CHEST)->IsVisible(),
             "UpdateImGuiVisibility Selection: Both check visible");

    CVarSetInteger(CVAR_RANDOMIZER_SETTING("MQDungeons"), RO_MQ_DUNGEONS_SET_NUMBER);
    CVarSetInteger(CVAR_RANDOMIZER_SETTING("MQDungeonCount"), 12);
    RandomizerCheckObjects::UpdateImGuiVisibility();
    MR_CHECK(!randoCtx->GetItemLocation(RC_DEKU_TREE_SLINGSHOT_CHEST)->IsVisible(),
             "UpdateImGuiVisibility All(12): vanilla Deku chest hidden");
    MR_CHECK(randoCtx->GetItemLocation(RC_DEKU_TREE_MQ_SLINGSHOT_CHEST)->IsVisible(),
             "UpdateImGuiVisibility All(12): MQ Deku chest visible");
    MR_CHECK(randoCtx->GetItemLocation(RC_DODONGOS_CAVERN_BOSS_ROOM_CHEST)->IsVisible(),
             "UpdateImGuiVisibility All(12): Both check visible");

    savedMQDungeons.Restore();
    savedMQCount.Restore();
    RandomizerCheckObjects::UpdateImGuiVisibility();
}

void CheckAudioBackendFiltering() {
    printf("[TEST] oot-menu-robustness: leg (c) Audio API backend filtering\n");

    const std::vector<Ship::AudioBackend> posixBackends = { Ship::AudioBackend::SDL, Ship::AudioBackend::NUL };
    const auto posixOffered = Ship::BuildAvailableAudioBackendsMap(posixBackends);
    MR_CHECK(posixOffered.size() == 2, "POSIX backend set should offer exactly 2 entries");
    MR_CHECK(!posixOffered.contains(Ship::AudioBackend::WASAPI),
             "POSIX backend set must never offer unavailable WASAPI backend");
    MR_CHECK(posixOffered.contains(Ship::AudioBackend::SDL), "POSIX backend set should offer SDL");
    MR_CHECK(posixOffered.contains(Ship::AudioBackend::NUL), "POSIX backend set should offer Null");

    const std::vector<Ship::AudioBackend> winBackends = {
        Ship::AudioBackend::WASAPI,
        Ship::AudioBackend::SDL,
        Ship::AudioBackend::NUL,
    };
    const auto winOffered = Ship::BuildAvailableAudioBackendsMap(winBackends);
    MR_CHECK(winOffered.size() == 3, "Windows backend set should offer all 3 entries");
    MR_CHECK(winOffered.contains(Ship::AudioBackend::WASAPI), "Windows backend set should offer WASAPI");

    const std::vector<Ship::AudioBackend> nullOnly = { Ship::AudioBackend::NUL };
    const auto nullOffered = Ship::BuildAvailableAudioBackendsMap(nullOnly);
    MR_CHECK(nullOffered.size() == 1 && nullOffered.contains(Ship::AudioBackend::NUL),
             "Null-only platform set should offer only Null");
    MR_CHECK(!nullOffered.contains(Ship::AudioBackend::WASAPI) && !nullOffered.contains(Ship::AudioBackend::SDL),
             "Null-only platform set must omit WASAPI and SDL");

    const std::vector<Ship::AudioBackend> noBackends;
    const auto emptyOffered = Ship::BuildAvailableAudioBackendsMap(noBackends);
    MR_CHECK(emptyOffered.empty(), "Empty available backends list should produce empty offered map");
}

void CheckSliderVisibleLabels() {
    printf("[TEST] oot-menu-robustness: leg (d) UIWidgets::GetVisibleLabel suffix stripping\n");

    MR_CHECK(UIWidgets::GetVisibleLabel("Medallion Count##Randomizer") == "Medallion Count",
             "double-hash Randomizer suffix should be stripped");
    MR_CHECK(UIWidgets::GetVisibleLabel("Count#Unique") == "Count", "single-hash suffix should be stripped");
    MR_CHECK(UIWidgets::GetVisibleLabel("Plain Label") == "Plain Label", "plain label without hash stays intact");
    MR_CHECK(UIWidgets::GetVisibleLabel("##Hidden") == "", "leading double-hash strips entire visible text");
    MR_CHECK(UIWidgets::GetVisibleLabel("") == "", "empty label returns empty string");
    MR_CHECK(UIWidgets::GetVisibleLabel(nullptr) == "", "nullptr label returns empty string safely");
}

} // namespace

extern "C" int OoT_MenuRobustness_RunHeadless(void) {
    sFailures = 0;
    CheckExcludedLocationsRoundTrip();
    CheckMQLocationsVisibility();
    CheckAudioBackendFiltering();
    CheckSliderVisibleLabels();
    printf("[TEST] oot-menu-robustness: completed with %d failure(s)\n", sFailures);
    return sFailures == 0 ? 0 : 1;
}
