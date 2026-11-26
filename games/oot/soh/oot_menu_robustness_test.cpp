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
 *   (e) UIWidgets::WrappedLabel wraps a checkbox label's visible text only: a hidden
 *       `##` ID suffix no longer counts toward the line length, and reaches ImGui intact.
 *   (f) From source (RSBS_SOURCE_DIR): Menu::MenuDrawItem's two checkbox cases draw
 *       their label through WrappedLabel, not WrappedText.
 *   (g) #1819: a malformed field in the ExcludedLocations, EnabledTricks or
 *       EnabledGlitches list (empty, a word, "12abc", out of int range, outside the
 *       enum) is skipped without throwing: RandoIdList_ParseField (src/common), the
 *       Locations page's ParseExcludedLocations, the Generate button's
 *       Rando_ReadMenuExcludesAndTricks (over leg (b)'s bring-up) and the Tricks
 *       page's UpdateMenuTricks. Runs after (b), whose Rando::Context it reuses.
 */

#include <cstdint>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iterator>
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

#include "rando_id_list.h" // src/common — #1819: RandoIdList_ParseField

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
void UpdateMenuTricks(); // #1819: the Tricks page's read of EnabledTricks and EnabledGlitches
} // namespace SohGui

// randomizer.cpp (#982): the Generate button's read of the exclude and trick CVars.
void Rando_ReadMenuExcludesAndTricks(std::set<RandomizerCheck>& excludedLocations,
                                     std::set<RandomizerTrick>& enabledTricks);

// Menu.cpp: pure helper filtering Audio API combobox entries to available backends.
namespace Ship {
std::map<Ship::AudioBackend, const char*>
BuildAvailableAudioBackendsMap(const std::vector<Ship::AudioBackend>& availableBackends);
} // namespace Ship

// UIWidgets.cpp: slider label suffix stripper, and the checkbox label wrapper (UIWidgets.hpp).
namespace UIWidgets {
std::string GetVisibleLabel(const char* label);
std::string WrappedText(const char* text, unsigned int charactersPerLine);
std::string WrappedLabel(const char* label, unsigned int charactersPerLine);
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

struct SavedStringCVar {
    const char* key;
    bool existed;
    std::string value;

    void Save(const char* cvarKey) {
        key = cvarKey;
        existed = CVarExists(cvarKey);
        const char* current = CVarGetString(cvarKey, "");
        value = current != nullptr ? current : "";
    }

    void Restore() const {
        if (existed) {
            CVarSetString(key, value.c_str());
        } else {
            CVarClear(key);
        }
    }
};

// RandoIdList_ParseField on one NUL-terminated field: the id, -1000 when rejected,
// -2000 when a rejection wrote the output anyway.
int32_t MrParseField(const char* field, int32_t minId, int32_t maxIdExclusive) {
    int32_t out = -1000;
    const int ok = RandoIdList_ParseField(field, std::string(field).size(), minId, maxIdExclusive, &out);
    if (ok == 0 && out != -1000) {
        return -2000;
    }
    return ok != 0 ? out : -1000;
}

void CheckMalformedIdLists() {
    printf("[TEST] oot-menu-robustness: leg (g) malformed excluded-location and trick lists (#1819)\n");

    // (g1) The strict field parser, with the bounds both readers use.
    const int32_t rcMin = RC_UNKNOWN_CHECK + 1;
    const int32_t rcMax = RC_MAX;
    const std::string rcLast = std::to_string(RC_MAX - 1);
    const std::string rcEnd = std::to_string(RC_MAX);
    const std::string rtLast = std::to_string(RT_MAX - 1);
    const std::string rtEnd = std::to_string(RT_MAX);
    MR_CHECK(MrParseField("", rcMin, rcMax) == -1000, "an empty field is rejected");
    MR_CHECK(MrParseField("   ", rcMin, rcMax) == -1000, "a field of spaces is rejected");
    MR_CHECK(MrParseField("abc", rcMin, rcMax) == -1000, "a word is rejected");
    MR_CHECK(MrParseField("1a", 0, 1000000) == -1000, "a letter is rejected even where its code would be in range");
    MR_CHECK(MrParseField("99999999999", rcMin, rcMax) == -1000, "a number beyond int range is rejected");
    MR_CHECK(MrParseField("4294967297", rcMin, rcMax) == -1000, "a number that wraps a 32-bit int is rejected");
    MR_CHECK(MrParseField("-3", rcMin, rcMax) == -1000, "a negative id is rejected");
    MR_CHECK(MrParseField("+12", rcMin, rcMax) == -1000, "a signed id is rejected");
    MR_CHECK(MrParseField("12abc", rcMin, rcMax) == -1000, "trailing garbage is rejected (std::stoi read 12)");
    MR_CHECK(MrParseField("1 2", rcMin, rcMax) == -1000, "a space inside the digits is rejected");
    MR_CHECK(MrParseField(" 12", rcMin, rcMax) == 12, "a leading space is tolerated, as std::stoi did");
    MR_CHECK(MrParseField("12 ", rcMin, rcMax) == 12, "a trailing space is tolerated, as std::stoi did");
    MR_CHECK(MrParseField("\t12\t", rcMin, rcMax) == 12, "surrounding tabs are tolerated");
    MR_CHECK(MrParseField("007", rcMin, rcMax) == 7, "leading zeros read as std::stoi read them");
    MR_CHECK(MrParseField("0", rcMin, rcMax) == -1000, "RC_UNKNOWN_CHECK is not an excludable check");
    MR_CHECK(MrParseField("1", rcMin, rcMax) == RC_LINKS_POCKET, "the lowest real check id is accepted");
    MR_CHECK(MrParseField(rcLast.c_str(), rcMin, rcMax) == RC_MAX - 1, "the highest check id is accepted");
    MR_CHECK(MrParseField(rcEnd.c_str(), rcMin, rcMax) == -1000, "RC_MAX itself is rejected");
    MR_CHECK(MrParseField("0", 0, RT_MAX) == RT_VISIBLE_COLLISION, "trick id 0 is a real trick and is accepted");
    MR_CHECK(MrParseField(rtLast.c_str(), 0, RT_MAX) == RT_MAX - 1, "the highest trick id is accepted");
    MR_CHECK(MrParseField(rtEnd.c_str(), 0, RT_MAX) == -1000, "RT_MAX itself is rejected");
    int32_t sliced = -1;
    MR_CHECK(RandoIdList_ParseField("123,456", 3, 0, 1000, &sliced) == 1 && sliced == 123,
             "the parser reads only the field's own length");

    // (g2) The Locations page's reader keeps the good ids around bad fields and does not throw.
    const std::set<RandomizerCheck> good = { RC_DEKU_TREE_SLINGSHOT_CHEST, RC_DODONGOS_CAVERN_BOSS_ROOM_CHEST };
    const std::string goodA = std::to_string(RC_DEKU_TREE_SLINGSHOT_CHEST);
    const std::string goodB = std::to_string(RC_DODONGOS_CAVERN_BOSS_ROOM_CHEST);
    const std::string wellFormed = goodA + "," + goodB;
    const std::string malformed = goodA + ",abc,,12abc,-3,99999999999,0," + rcEnd + "," + goodB + ",";
    const std::string oneBad = goodA + ",abc," + goodB;
    bool pageThrew = false;
    try {
        MR_CHECK(SohGui::ParseExcludedLocations(wellFormed.c_str()) == good, "page: a well-formed list parses whole");
        MR_CHECK(SohGui::ParseExcludedLocations(oneBad.c_str()) == good,
                 "page: one bad field between two good ones keeps both good ids");
        MR_CHECK(SohGui::ParseExcludedLocations(malformed.c_str()) == good,
                 "page: every malformed field is skipped and only the good ids remain");
        MR_CHECK(SohGui::ParseExcludedLocations("abc").empty(), "page: a list of one word parses to the empty set");
    } catch (const std::exception& e) {
        pageThrew = true;
        printf("[TEST] oot-menu-robustness: the Locations page parse threw (%s)\n", e.what());
    }
    MR_CHECK(!pageThrew, "page: the Locations page parse must not throw on a malformed list");

    // (g3) The generation reader (shared with the golden harness), over the bring-up leg (b) made.
    if (Rando::Context::GetInstance() == nullptr) {
        MR_CHECK(false, "leg (g) needs the Rando::Context leg (b) brought up");
        return;
    }
    SavedStringCVar savedExcludes;
    SavedStringCVar savedTricks;
    SavedStringCVar savedGlitches;
    SavedIntCVar savedMQDungeons;
    SavedIntCVar savedMQCount;
    savedExcludes.Save(CVAR_RANDOMIZER_SETTING("ExcludedLocations"));
    savedTricks.Save(CVAR_RANDOMIZER_SETTING("EnabledTricks"));
    savedGlitches.Save(CVAR_RANDOMIZER_SETTING("EnabledGlitches"));
    savedMQDungeons.Save(CVAR_RANDOMIZER_SETTING("MQDungeons"));
    savedMQCount.Save(CVAR_RANDOMIZER_SETTING("MQDungeonCount"));
    // Leg (b) proves both checks visible with no MQ dungeons, so the visibility filter keeps them.
    CVarSetInteger(CVAR_RANDOMIZER_SETTING("MQDungeons"), RO_MQ_DUNGEONS_NONE);
    CVarSetInteger(CVAR_RANDOMIZER_SETTING("MQDungeonCount"), 0);

    const std::set<RandomizerTrick> goodTricks = { RT_GROTTOS_WITHOUT_AGONY, RT_FEWER_TUNIC_REQUIREMENTS };
    const std::string tricksWellFormed =
        std::to_string(RT_GROTTOS_WITHOUT_AGONY) + "," + std::to_string(RT_FEWER_TUNIC_REQUIREMENTS);
    const std::string tricksMalformed = "," + std::to_string(RT_GROTTOS_WITHOUT_AGONY) + ",abc,,-1,99999999999," +
                                        rtEnd + ",2x," + std::to_string(RT_FEWER_TUNIC_REQUIREMENTS);
    const std::string excludesMalformed = "," + malformed;
    bool genThrew = false;
    try {
        std::set<RandomizerCheck> excl;
        std::set<RandomizerTrick> tricks;
        CVarSetString(CVAR_RANDOMIZER_SETTING("ExcludedLocations"), wellFormed.c_str());
        CVarSetString(CVAR_RANDOMIZER_SETTING("EnabledTricks"), tricksWellFormed.c_str());
        Rando_ReadMenuExcludesAndTricks(excl, tricks);
        MR_CHECK(excl == good, "generation: a well-formed exclude list reads whole");
        MR_CHECK(tricks == goodTricks, "generation: a well-formed trick list reads whole");

        excl.clear();
        tricks.clear();
        CVarSetString(CVAR_RANDOMIZER_SETTING("ExcludedLocations"), excludesMalformed.c_str());
        CVarSetString(CVAR_RANDOMIZER_SETTING("EnabledTricks"), tricksMalformed.c_str());
        Rando_ReadMenuExcludesAndTricks(excl, tricks);
        MR_CHECK(excl == good, "generation: malformed exclude fields are skipped, the good ids kept");
        MR_CHECK(tricks == goodTricks, "generation: malformed trick fields are skipped, the good ids kept");
    } catch (const std::exception& e) {
        genThrew = true;
        printf("[TEST] oot-menu-robustness: Rando_ReadMenuExcludesAndTricks threw (%s)\n", e.what());
    }
    MR_CHECK(!genThrew, "generation: the exclude and trick read must not throw on a malformed list");

    // (g4) The Tricks page's reader (EnabledTricks and EnabledGlitches) does not throw.
    bool tricksPageThrew = false;
    try {
        CVarSetString(CVAR_RANDOMIZER_SETTING("EnabledTricks"), tricksMalformed.c_str());
        CVarSetString(CVAR_RANDOMIZER_SETTING("EnabledGlitches"), tricksMalformed.c_str());
        SohGui::UpdateMenuTricks();
    } catch (const std::exception& e) {
        tricksPageThrew = true;
        printf("[TEST] oot-menu-robustness: the Tricks page parse threw (%s)\n", e.what());
    }
    MR_CHECK(!tricksPageThrew, "tricks page: the Tricks page parse must not throw on a malformed list");

    savedExcludes.Restore();
    savedTricks.Restore();
    savedGlitches.Restore();
    savedMQDungeons.Restore();
    savedMQCount.Restore();
    SohGui::UpdateMenuTricks();
    RandomizerCheckObjects::UpdateImGuiVisibility();
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

    // Live UpdateImGuiVisibility() over Rando::Context + Rando::StaticData, the
    // bring-up oot_tracker_soh_fixes_test.cpp uses. It reads the MQ keys from the
    // CVar store and the seed options from the Context, so it needs no
    // Settings::CreateOptions(): that one asks OTRGlobals::Instance whether both
    // quests' archives are loaded, and this row has no OTRGlobals.
    if (Rando_InitRegionGraphForTest() != 0 || Rando::Context::GetInstance() == nullptr) {
        MR_CHECK(false, "Rando_InitRegionGraphForTest failed");
        return;
    }
    auto randoCtx = Rando::Context::GetInstance();
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

void CheckCheckboxLabelWrap() {
    printf("[TEST] oot-menu-robustness: leg (e) UIWidgets::WrappedLabel ignores a hidden ID suffix\n");

    // Menu::DrawElement draws a page's widgets at 90 / columnCount characters per line.
    constexpr unsigned int kMrThreeColumnWidth = 90 / 3;

    // Premise: WrappedText counts the hidden suffix, so without WrappedLabel this name wraps early.
    MR_CHECK(UIWidgets::WrappedText("Shuffle Master Sword##Randomizer", kMrThreeColumnWidth) ==
                 "Shuffle Master\nSword##Randomizer",
             "premise: WrappedText counts the ##Randomizer suffix toward the line length");

    // Real checkbox names (Randomizer options carry ##Randomizer since #1768; Enhancements and
    // Minigames rows carry their own suffix): each wraps as its visible name alone does.
    struct MrLabelCase {
        const char* name;
        const char* suffix;
    };
    const MrLabelCase cases[] = {
        { "Shuffle Master Sword", "##Randomizer" },    { "Shuffle Kokiri Sword", "##Randomizer" },
        { "Shuffle Deku Stick Bag", "##Randomizer" },  { "Shuffle Gerudo Membership Card", "##Randomizer" },
        { "All Locations Reachable", "##Randomizer" }, { "Time Travel Requires Master Sword", "##Randomizer" },
        { "Skip Child Stealth", "##Enhancement" },     { "Blue Fire Arrows", "##Enhancement" },
        { "Sunlight Arrows", "##Enhancement" },        { "Unlimited Playback Time", "##LostWoods" },
    };
    for (const MrLabelCase& c : cases) {
        const std::string full = std::string(c.name) + c.suffix;
        const std::string wrapped = UIWidgets::WrappedLabel(full.c_str(), kMrThreeColumnWidth);
        const std::string expected = UIWidgets::WrappedText(c.name, kMrThreeColumnWidth) + c.suffix;
        if (wrapped != expected) {
            printf("[TEST] oot-menu-robustness: \"%s\" wrapped as \"%s\"\n", full.c_str(), wrapped.c_str());
        }
        MR_CHECK(wrapped == expected, "a suffixed checkbox label must wrap exactly as its visible name alone");
        const size_t idAt = wrapped.find("##");
        MR_CHECK(idAt != std::string::npos && wrapped.substr(idAt) == c.suffix,
                 "the hidden ID suffix must reach ImGui unchanged");
        MR_CHECK(UIWidgets::WrappedLabel(c.name, kMrThreeColumnWidth) ==
                     UIWidgets::WrappedText(c.name, kMrThreeColumnWidth),
                 "a label without ## must come out exactly as WrappedText gives it");
    }

    // Hand-walked results at 30 characters per line.
    MR_CHECK(UIWidgets::WrappedLabel("Shuffle Master Sword##Randomizer", kMrThreeColumnWidth) ==
                 "Shuffle Master Sword##Randomizer",
             "Shuffle Master Sword stays on one line");
    MR_CHECK(UIWidgets::WrappedLabel("Skip Child Stealth##Enhancement", kMrThreeColumnWidth) ==
                 "Skip Child Stealth##Enhancement",
             "Skip Child Stealth stays on one line");
    MR_CHECK(UIWidgets::WrappedLabel("Time Travel Requires Master Sword##Randomizer", kMrThreeColumnWidth) ==
                 "Time Travel Requires Master\nSword##Randomizer",
             "a long name still wraps at the same space with its suffix");
    MR_CHECK(UIWidgets::WrappedLabel("Time Travel Requires Master Sword", kMrThreeColumnWidth) ==
                 "Time Travel Requires Master\nSword",
             "a long name without a suffix wraps as WrappedText wraps it");
    // A single '#' is drawn by ImGui, so it stays in the visible part and counts.
    MR_CHECK(UIWidgets::WrappedLabel("Shuffle Master Sword #1 Of Many##X", kMrThreeColumnWidth) ==
                 "Shuffle Master Sword #1 Of\nMany##X",
             "a single # is visible text and counts toward the line length");
    MR_CHECK(UIWidgets::WrappedLabel("Shuffle Master Sword###Id", kMrThreeColumnWidth) == "Shuffle Master Sword###Id",
             "a ### suffix is split at its first ## and kept whole");
    MR_CHECK(UIWidgets::WrappedLabel("##Hidden label whose id is longer than one line", kMrThreeColumnWidth) ==
                 "##Hidden label whose id is longer than one line",
             "a label that is all ID is never wrapped");
    MR_CHECK(UIWidgets::WrappedLabel(nullptr, kMrThreeColumnWidth).empty(), "nullptr label returns empty string");
}

std::string MrReadSource(const char* sourceDir, const char* relPath) {
    std::ifstream in(std::string(sourceDir) + "/" + relPath, std::ios::binary);
    if (!in) {
        return std::string();
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    // CRLF checkouts read as LF so the needles below stay one spelling.
    std::string normalized;
    normalized.reserve(text.size());
    for (const char c : text) {
        if (c != '\r') {
            normalized += c;
        }
    }
    return normalized;
}

// The text from the one occurrence of `start` in `src` to the first `end` after it.
// Empty when `start` is missing or not unique, or `end` does not follow it.
std::string MrSliceBetween(const std::string& src, const char* start, const char* end) {
    const size_t at = src.find(start);
    if (at == std::string::npos || src.find(start, at + 1) != std::string::npos) {
        return std::string();
    }
    const size_t close = src.find(end, at);
    if (close == std::string::npos) {
        return std::string();
    }
    return src.substr(at, close - at);
}

void CheckCheckboxLabelCallSites(const char* sourceDir) {
    printf("[TEST] oot-menu-robustness: leg (f) the checkbox cases draw through WrappedLabel (source)\n");
    if (sourceDir == nullptr || sourceDir[0] == '\0') {
        MR_CHECK(false, "the source directory was not passed in");
        return;
    }
    const std::string menu = MrReadSource(sourceDir, "games/oot/soh/SohGui/Menu.cpp");
    MR_CHECK(!menu.empty(), "games/oot/soh/SohGui/Menu.cpp must be readable under RSBS_SOURCE_DIR");
    const std::string drawItem = MrSliceBetween(menu, "\nvoid Menu::MenuDrawItem(", "\n}\n");
    MR_CHECK(!drawItem.empty(), "Menu::MenuDrawItem must be found once in Menu.cpp");
    const std::string checkbox = MrSliceBetween(drawItem, "case WIDGET_CHECKBOX: {", "} break;");
    const std::string cvarCheckbox = MrSliceBetween(drawItem, "case WIDGET_CVAR_CHECKBOX: {", "} break;");
    MR_CHECK(
        checkbox.find("UIWidgets::Checkbox(UIWidgets::WrappedLabel(widget.name.c_str(), width).c_str(), pointer,") !=
            std::string::npos,
        "WIDGET_CHECKBOX must draw its label through UIWidgets::WrappedLabel");
    MR_CHECK(cvarCheckbox.find("UIWidgets::CVarCheckbox(UIWidgets::WrappedLabel(widget.name.c_str(), width).c_str(), "
                               "widget.cVar,") != std::string::npos,
             "WIDGET_CVAR_CHECKBOX must draw its label through UIWidgets::WrappedLabel");
    MR_CHECK(!drawItem.empty() && drawItem.find("WrappedText(widget.name") == std::string::npos,
             "no widget name may reach WrappedText in Menu::MenuDrawItem");
}

} // namespace

extern "C" int OoT_MenuRobustness_RunHeadless(const char* sourceDir) {
    sFailures = 0;
    CheckExcludedLocationsRoundTrip();
    CheckMQLocationsVisibility();
    CheckMalformedIdLists();
    CheckAudioBackendFiltering();
    CheckSliderVisibleLabels();
    CheckCheckboxLabelWrap();
    CheckCheckboxLabelCallSites(sourceDir);
    printf("[TEST] oot-menu-robustness: completed with %d failure(s)\n", sFailures);
    return sFailures == 0 ? 0 : 1;
}
