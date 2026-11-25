/**
 * @file oot_trick_text_1746_test.cpp
 * Lock for #1746: OoT randomizer trick display names, Hard Ground Jumps
 * description, cross-referenced trick description, and spoiler-log enabledTricks
 * lookup after porting SoH c021a0e8a (upstream PR 6225) and 692ef416e
 * (upstream PR 6391).
 *
 * Rando-tier row `oot-trick-text-1746` (CTest OoTTrickText1746), dispatch in
 * src/common/test_runner.cpp, same bring-up as MasterSwordRespawn
 * (OTRGlobals' randomizer context, required by Settings::CreateOptions).
 */

#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/settings.h"
#include "soh/OTRGlobals.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

namespace {

int sTk1746Failures = 0;

void Tk1746CheckInt(const char* leg, int got, int expected, const char* detail) {
    if (got != expected) {
        std::printf("[TEST] FAIL (%s): %s (expected %d, got %d)\n", leg, detail, expected, got);
        ++sTk1746Failures;
        return;
    }
    std::printf("[TEST]   ok (%s): %s\n", leg, detail);
}

void Tk1746CheckStr(const char* leg, const std::string& got, const std::string& expected, const char* detail) {
    if (got != expected) {
        std::printf("[TEST] FAIL (%s): %s\n  expected: \"%s\"\n  got:      \"%s\"\n", leg, detail, expected.c_str(),
                    got.c_str());
        ++sTk1746Failures;
        return;
    }
    std::printf("[TEST]   ok (%s): %s\n", leg, detail);
}

void Tk1746CheckBool(const char* leg, bool cond, const char* detail) {
    if (!cond) {
        std::printf("[TEST] FAIL (%s): %s\n", leg, detail);
        ++sTk1746Failures;
        return;
    }
    std::printf("[TEST]   ok (%s): %s\n", leg, detail);
}

void Tk1746ReadTricks(const std::shared_ptr<Rando::Context>& randoCtx, const nlohmann::json& names) {
    nlohmann::json spoiler;
    spoiler["enabledTricks"] = names;
    Rando::Settings::GetInstance()->ParseEnabledTricksJson(spoiler["enabledTricks"]);
    randoCtx->ParseTricksJson(spoiler);
}

struct Tk1746RenameRow {
    RandomizerTrick trick;
    const char* oldName;
    const char* newName;
};

const Tk1746RenameRow kTk1746JumpslashRenames[] = {
    { RT_KAK_TOWER_GS, "Kakariko Tower GS with Jump Slash", "Kakariko Tower GS with Jumpslash" },
    { RT_KAK_CHILD_WINDMILL_POH, "Windmill PoH as Child with Precise Jump Slash",
      "Windmill PoH as Child with Precise Jumpslash" },
    { RT_DMT_JS_LOWER_GS, "Death Mountain Trail Lower Red Rock GS with Jump Slash",
      "Death Mountain Trail Lower Red Rock GS with Jumpslash" },
    { RT_LH_LAB_WALL_GS, "Lake Hylia Lab Wall GS with Jump Slash", "Lake Hylia Lab Wall GS with Jumpslash" },
    { RT_DEKU_BASEMENT_GS, "Deku Tree Basement Vines GS with Jump Slash",
      "Deku Tree Basement Vines GS with Jumpslash" },
    { RT_FOREST_OUTSIDE_BACKDOOR, "Forest Temple Outside Backdoor with Jump Slash",
      "Forest Temple Outside Backdoor with Jumpslash" },
    { RT_FOREST_MQ_JS_HALLWAY_SWITCH, "Forest Temple MQ Twisted Hallway Switch with Jump Slash",
      "Forest Temple MQ Twisted Hallway Switch with Jumpslash" },
};

} // namespace

/**
 * @return 0 when every leg holds, 1 otherwise.
 */
extern "C" int OoT_TrickText1746Test_Run(void) {
    if (OTRGlobals::Instance == nullptr || OTRGlobals::Instance->gRandoContext == nullptr) {
        std::printf("[TEST] FAIL: oot-trick-text-1746 needs OTRGlobals' randomizer context\n");
        return 1;
    }

    sTk1746Failures = 0;

    const std::shared_ptr<Rando::Settings> settings = Rando::Settings::GetInstance();
    settings->CreateOptions();
    const std::shared_ptr<Rando::Context> randoCtx = OTRGlobals::Instance->gRandoContext;

    // TK0: anti-vacuity — the trick name table is populated.
    Tk1746CheckInt("TK0", settings->GetRandomizerTrickByName("Ground Jump"), RT_GROUND_JUMP,
                   "Ground Jump maps to RT_GROUND_JUMP");

    // TK1: c021a0e8a — King Dodongo Bombchus trick name.
    const std::string kdNewName = "Dodongo's Cavern Dodongo with Only Bombchus";
    const std::string kdOldName = "Dodongo's Cavern Dodongo without Bombchus";
    Tk1746CheckStr("TK1", settings->GetTrickOption(RT_DC_DODONGO_CHU).GetName(), kdNewName,
                   "RT_DC_DODONGO_CHU GetName matches upstream post-image");
    Tk1746CheckInt("TK1", settings->GetRandomizerTrickByName(kdNewName), RT_DC_DODONGO_CHU,
                   "new KD Bombchus name maps to RT_DC_DODONGO_CHU");
    Tk1746CheckInt("TK1", settings->GetRandomizerTrickByName(kdOldName), -1, "old KD Bombchus name maps to -1");

    // TK2: 692ef416e — Hard Ground Jumps description matches upstream's two-line text exactly.
    const std::string expectedHardGroundJumpDesc =
        "Enables ground jumps which require some precision outside of setting up jump,\n"
        "such as needing extra height with jumpslash or jumping while running with hover boots.";
    Tk1746CheckStr("TK2", settings->GetTrickOption(RT_GROUND_JUMP_HARD).GetDescription(), expectedHardGroundJumpDesc,
                   "RT_GROUND_JUMP_HARD description matches upstream post-image");

    // TK3: 692ef416e — all seven "Jump Slash" -> "Jumpslash" trick renames.
    for (const Tk1746RenameRow& row : kTk1746JumpslashRenames) {
        Tk1746CheckStr("TK3", settings->GetTrickOption(row.trick).GetName(), row.newName, row.newName);
        Tk1746CheckInt("TK3", settings->GetRandomizerTrickByName(row.newName), row.trick, row.newName);
        Tk1746CheckInt("TK3", settings->GetRandomizerTrickByName(row.oldName), -1, row.oldName);
    }

    // TK4: 692ef416e — Boomerang hallway trick quotes the updated backdoor trick name, and that quoted name exists.
    const std::string quotedBackdoorName = "Forest Temple Outside Backdoor with Jumpslash";
    const std::string quotedBackdoorToken = "\"" + quotedBackdoorName + "\"";
    const std::string& rangHallwayDesc = settings->GetTrickOption(RT_FOREST_MQ_RANG_HALLWAY_SWITCH).GetDescription();
    Tk1746CheckBool("TK4", rangHallwayDesc.find(quotedBackdoorToken) != std::string::npos,
                    "RT_FOREST_MQ_RANG_HALLWAY_SWITCH description quotes updated backdoor trick name");
    Tk1746CheckInt("TK4", settings->GetRandomizerTrickByName(quotedBackdoorName), RT_FOREST_OUTSIDE_BACKDOOR,
                   "quoted backdoor trick name maps to RT_FOREST_OUTSIDE_BACKDOOR");

    // TK5: spoiler-log enabledTricks lookup switches RT_KAK_TOWER_GS on with the new name, leaves it off with the old.
    const uint8_t savedVisible = randoCtx->GetTrickOption(RT_VISIBLE_COLLISION).Get();
    const uint8_t savedKakTower = randoCtx->GetTrickOption(RT_KAK_TOWER_GS).Get();

    randoCtx->GetTrickOption(RT_VISIBLE_COLLISION).Set(0);
    randoCtx->GetTrickOption(RT_KAK_TOWER_GS).Set(0);
    Tk1746ReadTricks(randoCtx, nlohmann::json::array({ "Kakariko Tower GS with Jump Slash" }));
    Tk1746CheckInt("TK5", randoCtx->GetTrickOption(RT_KAK_TOWER_GS).Get(), 0,
                   "spoiler with old Kakariko Tower GS name leaves RT_KAK_TOWER_GS off");
    Tk1746CheckInt("TK5", randoCtx->GetTrickOption(RT_VISIBLE_COLLISION).Get(), 0,
                   "spoiler with old Kakariko Tower GS name leaves trick 0 off");

    randoCtx->GetTrickOption(RT_VISIBLE_COLLISION).Set(0);
    randoCtx->GetTrickOption(RT_KAK_TOWER_GS).Set(0);
    Tk1746ReadTricks(randoCtx, nlohmann::json::array({ "Kakariko Tower GS with Jumpslash" }));
    Tk1746CheckInt("TK5", randoCtx->GetTrickOption(RT_KAK_TOWER_GS).Get(), 1,
                   "spoiler with new Kakariko Tower GS name switches RT_KAK_TOWER_GS on");
    Tk1746CheckInt("TK5", randoCtx->GetTrickOption(RT_VISIBLE_COLLISION).Get(), 0,
                   "spoiler with new Kakariko Tower GS name leaves trick 0 off");

    settings->GetTrickOption(RT_KAK_TOWER_GS).SetContextIndex(RO_GENERIC_OFF);
    randoCtx->GetTrickOption(RT_VISIBLE_COLLISION).Set(savedVisible);
    randoCtx->GetTrickOption(RT_KAK_TOWER_GS).Set(savedKakTower);

    if (sTk1746Failures != 0) {
        std::printf("[TEST] FAIL (oot-trick-text-1746): %d failure(s)\n", sTk1746Failures);
        return 1;
    }
    std::printf("[TEST] PASS: oot-trick-text-1746\n");
    return 0;
}
