/**
 * ROM-free lock for "the combo owns MM's paired start" (#879, operator ruling
 * 2026-10-09, items 1-2). CTest row MMPairedStart, label "rando" (it runs a
 * real paired generation, so it needs the display bring-up the rando rows use;
 * no archive is read). Dispatch `mm-paired-start` in src/common/test_runner.cpp.
 *
 * WHAT #879 FOUND. A paired world's MM half started with 2Ship's default
 * starting-items list, { RI_PROGRESSIVE_SWORD, RI_SHIELD_HERO, RI_OCARINA,
 * RI_SONG_TIME } (Rando::GetStartingItemsFromConfig, StartingItems.cpp), or with
 * whatever a `CVars.gRando.StartingItems` block in the player's config named
 * instead: a source the combo's settings surface never shows. The ruling: at
 * creation the paired MM start is the combo's own frozen decision, the Ocarina
 * of Time and the Song of Time and nothing else; the Kokiri Sword and the Hero's
 * Shield leave the start and go into MM's pool, as OoTMM does; 2Ship's config
 * block is never read for a paired world.
 *
 * WHAT THIS ROW LOCKS, with a 2Ship config block present that names a DIFFERENT
 * list (the old default plus the Bunny Hood, so a leak names itself):
 *
 *   1. The identity. The creation stamp (MM_Rando_ComputeProfileStamp, the one
 *      computation creation and every arrival run) is the same with and without
 *      the block: a config change after creation changes nothing, and is never
 *      a reason to refuse an arrival.
 *   2. The start. A real paired creation (a headless OoT generation, then MM's
 *      creation-time half through the real OnSaveInit chain) persists exactly
 *      { RI_OCARINA, RI_SONG_TIME } into the MM half, and the half holds the
 *      Ocarina of Time and the Song of Time, no sword (B empty), no shield and
 *      no Bunny Hood.
 *   3. The pool. GeneratePools over the half's own persisted rando info (the
 *      creation's input, unchanged) holds the Kokiri Sword as a third
 *      progressive sword beside the two Smithy rows, and the Hero's Shield; the
 *      Skull Kid roof's Ocarina and Song of Time are not in it.
 *   4. The world. The single bag placed the sword and the shield: every pool
 *      copy sits on an MM check or, crossed, on an OoT host.
 *
 * Every leg reports and the row fails at the end, so a red run names every
 * contract it breaks rather than the first.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <ship/Context.h>
#include <ship/config/Config.h>
#include <nlohmann/json.hpp>
#include <libultraship/bridge/consolevariablebridge.h>

#include "2s2h/Rando/Foreign.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/Logic/Logic.h"
#include "2s2h/Rando/StaticData/StaticData.h"

// src/common — the creation stamp, the coordinator's placement table.
#include "combo_mm_options_view.h"
#include "combo_logic.h"
#include "context.h"
#include "cvar_shared_keys.h"
#include "foreign_items.h"
#include "shared_items.h" // Combo_RecordSharedItemCrossing: the ocarina found on an OoT host (#1671)

#include "2s2h/Rando/Logic/ResetTimeGuard.h" // MmCanResetTime / MmCanResetTimeOnMoon (#1671)

extern "C" {
#include "functions.h"
#include "variables.h"
// 3drando/menu.cpp: the headless Generate button (freezes the pairing identity
// and the combo record, stops OoT's fill at its general pass).
int Rando_HeadlessSeedTest(const char* seedStr);
int OoT_ComboLogic_GeneralPassDeferred(void);
// mm_rando_gen_test.cpp: MM's creation-time half over that world, through the
// real OnSaveInit chain, without OoT's side of the tail.
int MM_Rando_HeadlessPairedHalf(void);
// GameExports_SingleExe.cpp: MM's arrival consumer and its two shared-resource
// shims; Rando/ForeignItemsSingleExe.cpp: the deferred give's queue (#1671).
void MM_ConsumeSharedItems(void);
void MM_HarvestSharedResources(void);
void MM_ApplySharedResources(void);
int MM_ForeignItem_FlushPending(void);
int MM_ForeignItem_TestPendingCount(void);
uint16_t MM_ForeignItem_TestPendingAt(int index);
void MM_ForeignItem_TestResetPending(void);
}

namespace {

constexpr const char* kStartSeed = "RSBSSINGLEBAG1"; // ComboSingleBag's pinned paired seed

int sFailures = 0;

// Ship::Config::SetBlock descends only through keys that already exist (a
// missing `CVars.gRando` would land the block at `CVars.StartingItems`, where
// 2Ship never looks), and it saves the config file. So the block is written at
// the deepest level that exists, and the key this row created is the one it
// erases afterwards.
std::string sCreatedKey;

void WriteStartingItemsBlock(const std::shared_ptr<Ship::Config>& config, const nlohmann::json& items) {
    const nlohmann::json nested = config->GetNestedJson();
    const bool hasCVars = nested.is_object() && nested.contains("CVars") && nested["CVars"].is_object();
    const bool hasRando = hasCVars && nested["CVars"].contains("gRando") && nested["CVars"]["gRando"].is_object();
    if (hasRando) {
        sCreatedKey = "CVars.gRando.StartingItems";
        config->SetBlock(sCreatedKey, items);
    } else if (hasCVars) {
        sCreatedKey = "CVars.gRando";
        config->SetBlock(sCreatedKey, nlohmann::json{ { "StartingItems", items } });
    } else {
        sCreatedKey = "CVars";
        config->SetBlock(sCreatedKey, nlohmann::json{ { "gRando", { { "StartingItems", items } } } });
    }
}

void EraseStartingItemsBlock(const std::shared_ptr<Ship::Config>& config) {
    if (!sCreatedKey.empty()) {
        config->EraseBlock(sCreatedKey);
        sCreatedKey.clear();
    }
}

void Check(bool ok, const char* what) {
    if (!ok) {
        sFailures++;
        printf("[TEST] FAIL: %s\n", what);
    }
}

std::string ItemNames(const std::vector<RandoItemId>& items) {
    std::string s;
    for (RandoItemId id : items) {
        if (!s.empty()) {
            s += ", ";
        }
        s += (id > RI_UNKNOWN && id < RI_MAX) ? Rando::StaticData::Items[id].spoilerName : "?";
    }
    return s.empty() ? std::string("(none)") : s;
}

int CountOf(const std::vector<RandoItemId>& items, RandoItemId id) {
    int n = 0;
    for (RandoItemId item : items) {
        n += item == id ? 1 : 0;
    }
    return n;
}

/** Copies of MM's @p id in the finished world: on an MM check, or crossed onto an
 *  OoT host by the single bag (the coordinator's table, still held). */
int PlacedInWorld(RandoItemId id, int* outOnOoT) {
    int onMM = 0;
    for (int rc = 0; rc < (int)RC_MAX; ++rc) {
        const RandoSaveCheck& check = RANDO_SAVE_CHECKS[rc];
        onMM += (check.shuffled && !check.skipped && check.randoItemId == id) ? 1 : 0;
    }
    int onOoT = 0;
    ComboLogicPlacement p;
    for (int i = 0; i < Combo_Logic_PlacementCount(GAME_OOT); ++i) {
        if (Combo_Logic_PlacementAt(GAME_OOT, i, &p) && p.item.originGame == (uint8_t)GAME_MM &&
            p.item.id == (uint16_t)id) {
            onOoT++;
        }
    }
    *outOnOoT = onOoT;
    return onMM + onOoT;
}

} // namespace

extern "C" int MM_PairedStart_RunHeadless(void) {
    printf("[TEST] mm-paired-start: the combo owns MM's paired start (Ocarina + Song of Time); the Kokiri Sword and "
           "the Hero's Shield are MM pool items; 2Ship's StartingItems block is never read (#879)\n");
    sFailures = 0;

    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetConfig() == nullptr) {
        printf("[TEST] FAIL: no Ship::Context config: the 2Ship config block cannot be written, so every leg below "
               "would be vacuous\n");
        return 1;
    }
    auto config = ctx->GetConfig();

    // The block a player's config may carry: 2Ship's old default plus the Bunny
    // Hood, so any item of it reaching the paired world names the leak.
    const std::vector<RandoItemId> block = { RI_PROGRESSIVE_SWORD, RI_SHIELD_HERO, RI_OCARINA, RI_SONG_TIME,
                                             RI_MASK_BUNNY };
    auto blockJson = nlohmann::json::array();
    for (RandoItemId id : block) {
        blockJson.push_back(Rando::StaticData::Items[id].spoilerName);
    }

    // ------------------------------------------------------------------------
    // 1. The identity ignores the block.
    // ------------------------------------------------------------------------
    const uint32_t stampWithout = MM_Rando_ComputeProfileStamp();
    WriteStartingItemsBlock(config, blockJson);
    // Non-vacuity: 2Ship's own accessor reads the block this row wrote.
    const std::vector<RandoItemId> fromConfig = Rando::GetStartingItemsFromConfig();
    printf("[TEST] mm-paired-start: (1) 2Ship's config accessor now reads: %s\n", ItemNames(fromConfig).c_str());
    if (CountOf(fromConfig, RI_MASK_BUNNY) != 1) {
        printf("[TEST] FAIL: 2Ship's accessor did not read the block this row wrote; the legs would be vacuous\n");
        EraseStartingItemsBlock(config);
        return 1;
    }
    const uint32_t stampWith = MM_Rando_ComputeProfileStamp();
    printf("[TEST] mm-paired-start: (1) creation stamp without the block %08X, with it %08X (expected equal)\n",
           (unsigned)stampWithout, (unsigned)stampWith);
    Check(stampWith == stampWithout,
          "(1) the paired MM identity folds 2Ship's StartingItems config block: a config change after creation "
          "would refuse the world's arrival (#879)");

    // ------------------------------------------------------------------------
    // 2. A real paired creation with the block present.
    // ------------------------------------------------------------------------
    int rc = Rando_HeadlessSeedTest(kStartSeed);
    if (rc != 0 || !Combo_ForeignPairingActive() || !Combo_ComboSettingsFrozen() ||
        !OoT_ComboLogic_GeneralPassDeferred()) {
        printf("[TEST] FAIL: the paired OoT generation for %s did not leave a frozen paired world at its general "
               "pass (rc=%d)\n",
               kStartSeed, rc);
        EraseStartingItemsBlock(config);
        return 1;
    }
    rc = MM_Rando_HeadlessPairedHalf();
    if (rc != 0) {
        printf("[TEST] FAIL: MM's creation-time half did not produce a rando world (rc=%d)\n", rc);
        EraseStartingItemsBlock(config);
        return 1;
    }

    const std::vector<RandoItemId> persisted = Rando::GetStartingItemsFromSave(gSaveContext.save.shipSaveInfo.rando);
    printf("[TEST] mm-paired-start: (2) the MM half's frozen start list: %s (expected RI_OCARINA, RI_SONG_TIME)\n",
           ItemNames(persisted).c_str());
    Check(persisted.size() == 2 && CountOf(persisted, RI_OCARINA) == 1 && CountOf(persisted, RI_SONG_TIME) == 1,
          "(2) the paired MM start is not exactly the Ocarina of Time and the Song of Time (#879)");

    const bool ocarina = INV_CONTENT(ITEM_OCARINA_OF_TIME) == ITEM_OCARINA_OF_TIME;
    const bool songOfTime = CHECK_QUEST_ITEM(QUEST_SONG_TIME) != 0;
    const int sword = (int)GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
    const int shield = (int)GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD);
    const int bButton = (int)BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B);
    const bool bunny = INV_CONTENT(ITEM_MASK_BUNNY) == ITEM_MASK_BUNNY;
    printf("[TEST] mm-paired-start: (2) the MM half holds: ocarina=%d songOfTime=%d sword=%d shield=%d B=0x%02X "
           "bunnyHood=%d (expected 1/1/%d/%d/0x%02X/0)\n",
           ocarina ? 1 : 0, songOfTime ? 1 : 0, sword, shield, bButton & 0xFF, bunny ? 1 : 0,
           (int)EQUIP_VALUE_SWORD_NONE, (int)EQUIP_VALUE_SHIELD_NONE, (int)ITEM_NONE & 0xFF);
    Check(ocarina, "(2) the paired MM half does not hold the Ocarina of Time");
    Check(songOfTime, "(2) the paired MM half does not hold the Song of Time");
    Check(sword == EQUIP_VALUE_SWORD_NONE && bButton == ITEM_NONE,
          "(2) the paired MM half starts with a sword: the Kokiri Sword is a pool item (#879)");
    Check(shield == EQUIP_VALUE_SHIELD_NONE,
          "(2) the paired MM half starts with a shield: the Hero's Shield is a pool item (#879)");
    Check(!bunny, "(2) 2Ship's StartingItems config block reached the paired world (the Bunny Hood was granted)");

    // ------------------------------------------------------------------------
    // 3. The pool the creation generated, from the half's own persisted input.
    // ------------------------------------------------------------------------
    auto info = std::make_unique<RandoSaveInfo>();
    memcpy(info.get(), &gSaveContext.save.shipSaveInfo.rando, sizeof(RandoSaveInfo));
    std::vector<RandoCheckId> checkPool;
    std::vector<RandoItemId> itemPool;
    Rando::Logic::GeneratePools(*info, checkPool, itemPool);
    const int swords = CountOf(itemPool, RI_PROGRESSIVE_SWORD);
    const int shields = CountOf(itemPool, RI_SHIELD_HERO);
    const int ocarinas = CountOf(itemPool, RI_OCARINA);
    const int songs = CountOf(itemPool, RI_SONG_TIME);
    printf("[TEST] mm-paired-start: (3) MM's pool: %zu items over %zu checks; progressive swords %d, Hero's Shield "
           "%d, Ocarina %d, Song of Time %d (expected 3/1/0/0)\n",
           itemPool.size(), checkPool.size(), swords, shields, ocarinas, songs);
    Check(swords == 3,
          "(3) MM's pool does not hold three progressive swords (the Kokiri Sword beside the two Smithy rows)");
    Check(shields == 1, "(3) MM's pool does not hold the Hero's Shield");
    Check(ocarinas == 0 && songs == 0,
          "(3) MM's pool still holds the Skull Kid roof's Ocarina or Song of Time, which the world starts with");

    // ------------------------------------------------------------------------
    // 4. The world holds every pool copy.
    // ------------------------------------------------------------------------
    int swordsOnOoT = 0;
    int shieldsOnOoT = 0;
    const int swordsPlaced = PlacedInWorld(RI_PROGRESSIVE_SWORD, &swordsOnOoT);
    const int shieldsPlaced = PlacedInWorld(RI_SHIELD_HERO, &shieldsOnOoT);
    printf("[TEST] mm-paired-start: (4) placed in the world: progressive swords %d (%d crossed onto OoT hosts), "
           "Hero's Shield %d (%d crossed) (expected %d/%d)\n",
           swordsPlaced, swordsOnOoT, shieldsPlaced, shieldsOnOoT, swords, shields);
    Check(swordsPlaced == swords, "(4) not every progressive sword of MM's pool was placed in the world");
    Check(shieldsPlaced == shields, "(4) the Hero's Shield of MM's pool was not placed in the world");

    // Leave the config file as this row found it: the block it created goes.
    EraseStartingItemsBlock(config);

    if (sFailures != 0) {
        printf("[TEST] FAIL: %d contract(s) broken\n", sFailures);
        return 1;
    }
    printf("[TEST] PASS: a paired MM half starts with exactly the Ocarina of Time and the Song of Time whatever "
           "2Ship's config says; the Kokiri Sword and the Hero's Shield are placed as MM pool items (#879)\n");
    return 0;
}

/**
 * mm-paired-start-shuffle-ocarina (#1671 lane B1; the operator's 2026-11-16
 * request for "shuffle ocarina as an MM option"). Majora's Mask's frozen Shuffle
 * Ocarina option (RO_SHUFFLE_OCARINA, off by default) takes the Ocarina of Time
 * out of the paired start, so the Clock Tower roof check's vanilla RI_OCARINA
 * stays in MM's pool; the Song of Time stays in the start; the moon crash is then
 * always New Cycle.
 *
 *   1. The IDENTITY list (PairedStartingItems) never moves, and the creation
 *      stamp moves only through the option's own appended term: key cleared and
 *      key 0 stamp alike, key 1 differently.
 *   2. The WORLD list (PairedStartingItemsForWorld) reads the frozen option from
 *      the half (RANDO_SAVE_OPTIONS): off, the Ocarina and the Song of Time; on,
 *      the Song of Time; on with the Shared Song of Time rule, nothing; on under
 *      Vanilla logic, the Song of Time (no Vanilla exception).
 *   3. The force (MM_Rando_FrozenMoonCrash): a randomizer half with the option on
 *      plays New Cycle over a frozen Last Save byte; off, it plays its byte. The
 *      identity folds the AUTHORED moon crash, never the force.
 *   4. Real paired creations with the key on, at the default (proving) rung and
 *      at the No Logic rung: generation succeeds; the half starts with the Song
 *      of Time alone and without the ocarina; the frozen byte is the authored Last
 *      Save while the half plays New Cycle; RI_OCARINA is in MM's pool once (twice
 *      under Plentiful) and every copy is placed. At the proving rung the
 *      reset-time guard keeps the full first cycle (MmCanResetTime) and the Moon's
 *      reset term is false until the ocarina is found. Under Vanilla logic the
 *      roof check and RI_OCARINA are still in the pool.
 *   5. The ocarina found on an OoT host: its crossing is redeemed at the next MM
 *      arrival and the first gameplay frame's give puts ITEM_OCARINA_OF_TIME in
 *      its slot; a harvest/apply round trip keeps it.
 *
 * Every leg reports and the row fails at the end.
 */
extern "C" int MM_PairedStartShuffleOcarina_RunHeadless(void) {
    printf("[TEST] mm-paired-start-shuffle-ocarina: MM's frozen Shuffle Ocarina takes the Ocarina out of the paired "
           "start and into the pool, keeps the Song of Time, and forces New Cycle (#1671)\n");
    sFailures = 0;
    const char* const optionKey = Rando::StaticData::Options[RO_SHUFFLE_OCARINA].cvar;
    const std::string ruleKey = std::string(RSBS::kComboIdentityKeyPrefix) + "SharedSongTime";
    const uint32_t kSharedSongTimeBit = 0x04000000u; // the rule's record bit (.redsave format)
    struct PSOClearKeys {
        const char* option;
        std::string rule;
        ~PSOClearKeys() {
            CVarClear(option);
            CVarClear(rule.c_str());
            CVarClear(RSBS_CVAR_MM_MOON_CRASH);
            CVarClear(RSBS_CVAR_COMBO_RANDO_LOGIC);
        }
    } clearKeys{ optionKey, ruleKey };
    CVarClear(optionKey);
    CVarClear(ruleKey.c_str());
    CVarClear(RSBS_CVAR_MM_MOON_CRASH);
    CVarClear(RSBS_CVAR_COMBO_RANDO_LOGIC);

    // ------------------------------------------------------------------------
    // 1. The identity list and the stamp.
    // ------------------------------------------------------------------------
    const std::vector<RandoItemId> identity = Rando::Foreign::PairedStartingItems();
    printf("[TEST] mm-paired-start-shuffle-ocarina: (1) identity list: %s (expected Ocarina, Song of Time)\n",
           ItemNames(identity).c_str());
    Check(identity.size() == 2 && CountOf(identity, RI_OCARINA) == 1 && CountOf(identity, RI_SONG_TIME) == 1,
          "(1) the identity start list moved: every existing pair's MM profile digest would move");
    const uint32_t stampCleared = MM_Rando_ComputeProfileStamp();
    CVarSetInteger(optionKey, 0);
    const uint32_t stampOff = MM_Rando_ComputeProfileStamp();
    CVarSetInteger(optionKey, 1);
    const uint32_t stampOn = MM_Rando_ComputeProfileStamp();
    CVarClear(optionKey);
    printf("[TEST] mm-paired-start-shuffle-ocarina: (1) creation stamp cleared %08X, off %08X, on %08X (expected the "
           "first two equal, the third different)\n",
           (unsigned)stampCleared, (unsigned)stampOff, (unsigned)stampOn);
    Check(stampOff == stampCleared, "(1) the option at 0 moved MM's creation stamp: every option-off world would move");
    Check(stampOn != stampCleared, "(1) the option at 1 did not move MM's creation stamp: it is no part of the world");

    // ------------------------------------------------------------------------
    // 2. The world list reads the frozen option from the half.
    // ------------------------------------------------------------------------
    {
        const uint32_t savedOption = RANDO_SAVE_OPTIONS[RO_SHUFFLE_OCARINA];
        const uint32_t savedLogic = RANDO_SAVE_OPTIONS[RO_LOGIC];
        struct Case {
            uint32_t option;
            bool rule;
            uint32_t logic;
            const char* what;
            int ocarinas;
            int songs;
        };
        const Case kCases[] = {
            { RO_GENERIC_NO, false, RO_LOGIC_GLITCHLESS, "option off", 1, 1 },
            { RO_GENERIC_YES, false, RO_LOGIC_GLITCHLESS, "option on", 0, 1 },
            { RO_GENERIC_YES, true, RO_LOGIC_GLITCHLESS, "option on + Shared Song of Time", 0, 0 },
            { RO_GENERIC_YES, false, RO_LOGIC_VANILLA, "option on under Vanilla logic", 0, 1 },
        };
        for (const Case& c : kCases) {
            ComboContext_Init();
            gComboCtx.sourceIsRando = true;
            gComboCtx.sharedRandoSeed = 0x1671517Au;
            gComboCtx.sharedRandoSettingsHash = 0x1671CA5Eu;
            gComboCtx.mmProfileDigest = 0x1671D16Eu;
            ComboSettingsRecord rec;
            Combo_ComboSettingsDefaults(&rec);
            ComboItemRulesRecord rules;
            memset(&rules, 0, sizeof(rules));
            rules.crossItemFlags = c.rule ? kSharedSongTimeBit : 0u;
            Combo_FreezeComboSettingsEx(&rec, &rules);
            RANDO_SAVE_OPTIONS[RO_SHUFFLE_OCARINA] = c.option;
            RANDO_SAVE_OPTIONS[RO_LOGIC] = c.logic;
            const std::vector<RandoItemId> world = Rando::Foreign::PairedStartingItemsForWorld();
            printf("[TEST] mm-paired-start-shuffle-ocarina: (2) %s: world start %s (expected Ocarina %d, Song of "
                   "Time %d)\n",
                   c.what, ItemNames(world).c_str(), c.ocarinas, c.songs);
            Check(CountOf(world, RI_OCARINA) == c.ocarinas && CountOf(world, RI_SONG_TIME) == c.songs &&
                      (int)world.size() == c.ocarinas + c.songs,
                  "(2) the paired world start does not follow the frozen Shuffle Ocarina option (#1671)");
        }
        RANDO_SAVE_OPTIONS[RO_SHUFFLE_OCARINA] = savedOption;
        RANDO_SAVE_OPTIONS[RO_LOGIC] = savedLogic;
        ComboContext_Init();
    }

    // ------------------------------------------------------------------------
    // 3. The force, and an identity that folds the authored value only.
    // ------------------------------------------------------------------------
    {
        const SaveType savedSaveType = gSaveContext.save.shipSaveInfo.saveType;
        const uint8_t savedByte = gSaveContext.save.shipSaveInfo.rando.randoSaveMoonCrash;
        const uint32_t savedOption = RANDO_SAVE_OPTIONS[RO_SHUFFLE_OCARINA];
        ComboContext_Init(); // no Shared Song of Time rule
        gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
        struct Case {
            uint32_t option;
            uint8_t byte;
            int expected;
            const char* what;
        };
        const Case kCases[] = {
            { RO_GENERIC_YES, (uint8_t)RSBS_MM_MOON_CRASH_RESET, RSBS_MM_MOON_CRASH_CYCLE, "on, byte Last Save" },
            { RO_GENERIC_NO, (uint8_t)RSBS_MM_MOON_CRASH_RESET, RSBS_MM_MOON_CRASH_RESET, "off, byte Last Save" },
            { RO_GENERIC_NO, (uint8_t)RSBS_MM_MOON_CRASH_CYCLE, RSBS_MM_MOON_CRASH_CYCLE, "off, byte New Cycle" },
        };
        for (const Case& c : kCases) {
            RANDO_SAVE_OPTIONS[RO_SHUFFLE_OCARINA] = c.option;
            gSaveContext.save.shipSaveInfo.rando.randoSaveMoonCrash = c.byte;
            const int mode = MM_Rando_FrozenMoonCrash();
            printf("[TEST] mm-paired-start-shuffle-ocarina: (3) %s: plays %d (expected %d)\n", c.what, mode,
                   c.expected);
            Check(mode == c.expected, "(3) the frozen Shuffle Ocarina option does not decide the moon crash as "
                                      "designed (it forces New Cycle when on, and only then)");
        }
        gSaveContext.save.shipSaveInfo.saveType = savedSaveType;
        gSaveContext.save.shipSaveInfo.rando.randoSaveMoonCrash = savedByte;
        RANDO_SAVE_OPTIONS[RO_SHUFFLE_OCARINA] = savedOption;

        std::vector<uint32_t> values(RO_MAX, 0u);
        values[RO_SHUFFLE_OCARINA] = RO_GENERIC_YES;
        CVarSetInteger(RSBS_CVAR_MM_MOON_CRASH, RSBS_MM_MOON_CRASH_RESET);
        const std::string authoredReset = Rando::Foreign::TestFoldIdentityStringResolvedMoonCrash(values.data());
        CVarSetInteger(RSBS_CVAR_MM_MOON_CRASH, RSBS_MM_MOON_CRASH_CYCLE);
        const std::string authoredCycle = Rando::Foreign::TestFoldIdentityStringResolvedMoonCrash(values.data());
        CVarClear(RSBS_CVAR_MM_MOON_CRASH);
        const bool resetFoldsNoMoon = authoredReset.find("|moonCrash:") == std::string::npos;
        const bool cycleFoldsMoon = authoredCycle.find("|moonCrash:1") != std::string::npos;
        const bool bothFoldOption = authoredReset.find("|RO_SHUFFLE_OCARINA:1") != std::string::npos &&
                                    authoredCycle.find("|RO_SHUFFLE_OCARINA:1") != std::string::npos;
        printf("[TEST] mm-paired-start-shuffle-ocarina: (3) identity with the option on: authored Last Save folds a "
               "moon term %d, authored New Cycle %d, both fold the option %d (expected 0/1/1)\n",
               resetFoldsNoMoon ? 0 : 1, cycleFoldsMoon ? 1 : 0, bothFoldOption ? 1 : 0);
        Check(resetFoldsNoMoon && cycleFoldsMoon && bothFoldOption,
              "(3) the identity folds the forced moon crash instead of the authored one, or misses the option");
    }

    // ------------------------------------------------------------------------
    // 4. Real paired creations with the key on, at two rungs.
    // ------------------------------------------------------------------------
    struct RungCase {
        int rungKey; // -1: the key cleared (the default rung)
        const char* what;
    };
    const RungCase kRungs[] = {
        { -1, "default rung" },
        { (int)RSBS_COMBO_RUNG_NONE, "No Logic rung" },
    };
    for (const RungCase& r : kRungs) {
        CVarClear(ruleKey.c_str());
        CVarSetInteger(optionKey, 1);
        // Last Save authored, so the half's byte shows the force is the option's.
        CVarSetInteger(RSBS_CVAR_MM_MOON_CRASH, RSBS_MM_MOON_CRASH_RESET);
        if (r.rungKey >= 0) {
            CVarSetInteger(RSBS_CVAR_COMBO_RANDO_LOGIC, r.rungKey);
        } else {
            CVarClear(RSBS_CVAR_COMBO_RANDO_LOGIC);
        }
        int rc = Rando_HeadlessSeedTest(kStartSeed);
        if (rc != 0 || !Combo_ForeignPairingActive() || !Combo_ComboSettingsFrozen() ||
            !OoT_ComboLogic_GeneralPassDeferred()) {
            printf("[TEST] FAIL: (4) %s: the paired OoT generation for %s did not leave a frozen paired world at its "
                   "general pass (rc=%d)\n",
                   r.what, kStartSeed, rc);
            return 1;
        }
        const bool proving = Combo_LogicRungProves(gComboCtx.comboSettings.logicRung);
        rc = MM_Rando_HeadlessPairedHalf();
        if (rc != 0) {
            printf("[TEST] FAIL: (4) %s: MM's creation-time half did not produce a rando world (rc=%d)\n", r.what, rc);
            return 1;
        }
        const std::vector<RandoItemId> persisted =
            Rando::GetStartingItemsFromSave(gSaveContext.save.shipSaveInfo.rando);
        const bool ocarina = INV_CONTENT(ITEM_OCARINA_OF_TIME) == ITEM_OCARINA_OF_TIME;
        const bool songOfTime = CHECK_QUEST_ITEM(QUEST_SONG_TIME) != 0;
        printf("[TEST] mm-paired-start-shuffle-ocarina: (4) %s (proving %d): frozen option %u, start %s; ocarina=%d "
               "songOfTime=%d; moon crash byte %u, plays %d (expected 1, Song of Time, 0, 1, %d, %d)\n",
               r.what, proving ? 1 : 0, (unsigned)RANDO_SAVE_OPTIONS[RO_SHUFFLE_OCARINA], ItemNames(persisted).c_str(),
               ocarina ? 1 : 0, songOfTime ? 1 : 0, (unsigned)gSaveContext.save.shipSaveInfo.rando.randoSaveMoonCrash,
               MM_Rando_FrozenMoonCrash(), RSBS_MM_MOON_CRASH_RESET, RSBS_MM_MOON_CRASH_CYCLE);
        Check(RANDO_SAVE_OPTIONS[RO_SHUFFLE_OCARINA] == RO_GENERIC_YES,
              "(4) the creation did not freeze Shuffle Ocarina from its key into the MM half");
        Check(persisted.size() == 1 && CountOf(persisted, RI_SONG_TIME) == 1,
              "(4) the paired MM half's start is not the Song of Time alone under Shuffle Ocarina");
        Check(!ocarina, "(4) the paired MM half starts with the Ocarina of Time under Shuffle Ocarina");
        Check(songOfTime, "(4) the paired MM half does not know the Song of Time (it stays in the start)");
        Check(gSaveContext.save.shipSaveInfo.rando.randoSaveMoonCrash == RSBS_MM_MOON_CRASH_RESET,
              "(4) the half's frozen moon crash byte is not the authored Last Save: the force leaked into it");
        Check(MM_Rando_FrozenMoonCrash() == RSBS_MM_MOON_CRASH_CYCLE,
              "(4) the half does not play New Cycle under Shuffle Ocarina");

        auto info = std::make_unique<RandoSaveInfo>();
        memcpy(info.get(), &gSaveContext.save.shipSaveInfo.rando, sizeof(RandoSaveInfo));
        std::vector<RandoCheckId> checkPool;
        std::vector<RandoItemId> itemPool;
        Rando::Logic::GeneratePools(*info, checkPool, itemPool);
        const int expectedOcarinas = info->randoSaveOptions[RO_PLENTIFUL_ITEMS] == RO_GENERIC_YES ? 2 : 1;
        const int ocarinas = CountOf(itemPool, RI_OCARINA);
        int onOoT = 0;
        const int placed = PlacedInWorld(RI_OCARINA, &onOoT);
        printf("[TEST] mm-paired-start-shuffle-ocarina: (4) %s: MM's pool holds %d Ocarina (expected %d), placed %d "
               "(%d crossed onto OoT hosts)\n",
               r.what, ocarinas, expectedOcarinas, placed, onOoT);
        Check(ocarinas == expectedOcarinas, "(4) MM's pool does not hold the Ocarina the world no longer starts with");
        Check(placed == ocarinas, "(4) not every Ocarina copy of MM's pool was placed in the world");

        if (proving) {
            const bool resets = Rando::Logic::MmCanResetTime();
            const bool resetsOnMoon = Rando::Logic::MmCanResetTimeOnMoon();
            printf("[TEST] mm-paired-start-shuffle-ocarina: (4) %s: MmCanResetTime %d, MmCanResetTimeOnMoon %d "
                   "(expected 1/0)\n",
                   r.what, resets ? 1 : 0, resetsOnMoon ? 1 : 0);
            Check(resets, "(4) with no ocarina the reset-time guard shuts the first cycle: New Cycle is not forced");
            Check(!resetsOnMoon, "(4) the Moon's reset term holds before the ocarina is found");
        }

        if (r.rungKey < 0) {
            // Under Vanilla logic the start is the same list and the roof check
            // keeps its Ocarina in the pool (no Vanilla exception, B1.1).
            auto vanilla = std::make_unique<RandoSaveInfo>();
            memcpy(vanilla.get(), &gSaveContext.save.shipSaveInfo.rando, sizeof(RandoSaveInfo));
            vanilla->randoSaveOptions[RO_LOGIC] = RO_LOGIC_VANILLA;
            std::vector<RandoCheckId> vanillaChecks;
            std::vector<RandoItemId> vanillaItems;
            Rando::Logic::GeneratePools(*vanilla, vanillaChecks, vanillaItems);
            const bool roof = std::find(vanillaChecks.begin(), vanillaChecks.end(), RC_CLOCK_TOWER_ROOF_OCARINA) !=
                              vanillaChecks.end();
            const bool roofHoldsOcarina =
                vanilla->randoSaveChecks[RC_CLOCK_TOWER_ROOF_OCARINA].randoItemId == RI_OCARINA;
            const int vanillaOcarinas = CountOf(vanillaItems, RI_OCARINA);
            printf("[TEST] mm-paired-start-shuffle-ocarina: (4) Vanilla logic: roof check pooled %d, holds the "
                   "Ocarina %d, pool Ocarina %d (expected 1/1/>=1)\n",
                   roof ? 1 : 0, roofHoldsOcarina ? 1 : 0, vanillaOcarinas);
            Check(roof && roofHoldsOcarina && vanillaOcarinas >= 1,
                  "(4) under Vanilla logic the Clock Tower roof check no longer pools the Ocarina");
        }
    }

    // ------------------------------------------------------------------------
    // 5. The ocarina found on an OoT host, then a round trip.
    // ------------------------------------------------------------------------
    {
        auto savedSave = std::make_unique<uint8_t[]>(sizeof(gSaveContext));
        memcpy(savedSave.get(), &gSaveContext, sizeof(gSaveContext));
        auto savedCombo = std::make_unique<uint8_t[]>(sizeof(gComboCtx));
        memcpy(savedCombo.get(), &gComboCtx, sizeof(gComboCtx));
        PlayState* const savedPlay = MM_gPlayState;

        ComboContext_Init();
        Combo_ClearSharedItemOutbox();
        MM_ForeignItem_TestResetPending();
        gSaveContext.fileNum = 0xFF; // a cross-game MM session's "no flash slot" sentinel
        gSaveContext.gameMode = GAMEMODE_NORMAL;
        const bool heldBefore = INV_CONTENT(ITEM_OCARINA_OF_TIME) == ITEM_OCARINA_OF_TIME;
        const int recorded = Combo_RecordSharedItemCrossing(GAME_MM, (uint16_t)RI_OCARINA);
        MM_gPlayState = nullptr; // the arrival runs before MM_gPlayState is set (z_play.c)
        MM_ConsumeSharedItems();
        const int pending = MM_ForeignItem_TestPendingCount();
        const uint16_t queued = MM_ForeignItem_TestPendingAt(0);
        int given = 0;
        {
            static PlayState sPSOStandIn;
            memset(&sPSOStandIn, 0, sizeof(sPSOStandIn));
            MM_gPlayState = &sPSOStandIn;
            given = MM_ForeignItem_FlushPending();
            MM_gPlayState = nullptr;
        }
        const bool heldAfterGive = INV_CONTENT(ITEM_OCARINA_OF_TIME) == ITEM_OCARINA_OF_TIME;
        MM_HarvestSharedResources(); // the suspend
        MM_ApplySharedResources();   // the next arrival
        const bool heldAfterRoundTrip = INV_CONTENT(ITEM_OCARINA_OF_TIME) == ITEM_OCARINA_OF_TIME;
        printf("[TEST] mm-paired-start-shuffle-ocarina: (5) held before %d; crossing slot %d; queued %d (id %u, "
               "expected %u); given %d; held after the give %d, after a round trip %d (expected 0, >=0, 1, 1, 1, 1)\n",
               heldBefore ? 1 : 0, recorded, pending, (unsigned)queued, (unsigned)RI_OCARINA, given,
               heldAfterGive ? 1 : 0, heldAfterRoundTrip ? 1 : 0);
        Check(!heldBefore, "(5) the half already held the ocarina: the legs below would be vacuous");
        Check(recorded >= 0, "(5) the ocarina's crossing was not recorded");
        Check(pending == 1 && queued == (uint16_t)RI_OCARINA,
              "(5) the next MM arrival did not queue exactly the ocarina's give");
        Check(given == 1 && heldAfterGive,
              "(5) the first gameplay frame's give did not put the Ocarina of Time in its slot");
        Check(heldAfterRoundTrip, "(5) a harvest/apply round trip took the ocarina away");

        MM_ForeignItem_TestResetPending();
        Combo_ClearSharedItemOutbox();
        memcpy(&gComboCtx, savedCombo.get(), sizeof(gComboCtx));
        MM_gPlayState = savedPlay;
        memcpy(&gSaveContext, savedSave.get(), sizeof(gSaveContext));
    }

    if (sFailures != 0) {
        printf("[TEST] FAIL: %d contract(s) broken\n", sFailures);
        return 1;
    }
    printf("[TEST] PASS: under Shuffle Ocarina MM's paired start is the Song of Time alone, the Ocarina is placed from "
           "MM's pool and credited when found, the moon crash plays New Cycle, and the option-off identity is "
           "unchanged (#1671)\n");
    return 0;
}

/**
 * mm-paired-start-shared-song-time (#1069; OoTMM's sharedSongTime). With the
 * Shared Song of Time rule frozen on, the pair has ONE Song of Time and MM's
 * paired start no longer holds it (OoTMM replaces both games' copies with the
 * shared song and removes the second); with the rule off the start keeps it.
 *
 *   1. The IDENTITY list (PairedStartingItems, folded into MM's profile stamp)
 *      is rule-independent: the stamp runs before the combo freeze, so a list
 *      that followed the rule would refuse every arrival. The stamp is the same
 *      with the rule's key on and off.
 *   2. The WORLD list (PairedStartingItemsForWorld) follows the FROZEN rule:
 *      off, the Ocarina and the Song of Time; on, the Ocarina alone.
 *   3. A real paired creation with the rule's key on: the rule freezes on, the MM
 *      half's persisted start is the Ocarina alone, the half does not know the
 *      Song of Time, and MM's pool holds the Song of Time (the one copy the bag
 *      pairs with OoT's).
 */
extern "C" int MM_PairedStartSharedSongTime_RunHeadless(void) {
    printf("[TEST] mm-paired-start-shared-song-time: under the frozen Shared Song of Time rule MM's paired start is "
           "the Ocarina alone and the song is a pool item; the identity list never moves (#1069)\n");
    sFailures = 0;
    const std::string key = std::string(RSBS::kComboIdentityKeyPrefix) + "SharedSongTime";
    const uint32_t kBit = 0x04000000u; // the rule's record bit (.redsave format)
    CVarClear(key.c_str());
    struct ClearKey {
        std::string key;
        ~ClearKey() {
            CVarClear(key.c_str());
        }
    } clearKey{ key };

    // ------------------------------------------------------------------------
    // 1. The identity list, and the stamp, ignore the rule.
    // ------------------------------------------------------------------------
    const std::vector<RandoItemId> identity = Rando::Foreign::PairedStartingItems();
    printf("[TEST] mm-paired-start-shared-song-time: (1) identity list: %s (expected Ocarina, Song of Time)\n",
           ItemNames(identity).c_str());
    Check(identity.size() == 2 && CountOf(identity, RI_OCARINA) == 1 && CountOf(identity, RI_SONG_TIME) == 1,
          "(1) the identity start list moved: every existing pair's MM profile digest would move");
    const uint32_t stampOff = MM_Rando_ComputeProfileStamp();
    CVarSetInteger(key.c_str(), 1);
    const uint32_t stampOn = MM_Rando_ComputeProfileStamp();
    CVarClear(key.c_str());
    printf("[TEST] mm-paired-start-shared-song-time: (1) creation stamp off %08X, on %08X (expected equal)\n",
           (unsigned)stampOff, (unsigned)stampOn);
    Check(stampOff == stampOn, "(1) the rule moved MM's creation stamp: every arrival of such a world is refused");

    // ------------------------------------------------------------------------
    // 2. The world list follows the frozen rule.
    // ------------------------------------------------------------------------
    for (int on = 0; on < 2; on++) {
        ComboContext_Init();
        gComboCtx.sourceIsRando = true;
        gComboCtx.sharedRandoSeed = 0x1069517Au;
        gComboCtx.sharedRandoSettingsHash = 0x1069CA5Eu;
        gComboCtx.mmProfileDigest = 0x1069D16Eu;
        ComboSettingsRecord rec;
        Combo_ComboSettingsDefaults(&rec);
        ComboItemRulesRecord rules;
        memset(&rules, 0, sizeof(rules));
        rules.crossItemFlags = on != 0 ? kBit : 0u;
        Combo_FreezeComboSettingsEx(&rec, &rules);
        const std::vector<RandoItemId> world = Rando::Foreign::PairedStartingItemsForWorld();
        printf("[TEST] mm-paired-start-shared-song-time: (2) rule %s: world start %s (expected %s)\n",
               on != 0 ? "on" : "off", ItemNames(world).c_str(), on != 0 ? "Ocarina" : "Ocarina, Song of Time");
        if (on != 0) {
            Check(world.size() == 1 && CountOf(world, RI_OCARINA) == 1,
                  "(2) with the rule frozen on the paired start still holds the Song of Time: the pair would have "
                  "two");
        } else {
            Check(world.size() == 2 && CountOf(world, RI_OCARINA) == 1 && CountOf(world, RI_SONG_TIME) == 1,
                  "(2) with the rule off the paired start is not the Ocarina and the Song of Time (#879)");
        }
    }
    ComboContext_Init();

    // ------------------------------------------------------------------------
    // 3. A real paired creation with the rule's key on.
    // ------------------------------------------------------------------------
    CVarSetInteger(key.c_str(), 1);
    int rc = Rando_HeadlessSeedTest(kStartSeed);
    if (rc != 0 || !Combo_ForeignPairingActive() || !Combo_ComboSettingsFrozen() ||
        !OoT_ComboLogic_GeneralPassDeferred()) {
        printf("[TEST] FAIL: the paired OoT generation for %s did not leave a frozen paired world at its general "
               "pass (rc=%d)\n",
               kStartSeed, rc);
        return 1;
    }
    printf("[TEST] mm-paired-start-shared-song-time: (3) frozen rules %08X (expected the %08X bit)\n",
           (unsigned)gComboCtx.comboItemRules.crossItemFlags, (unsigned)kBit);
    Check(Combo_ComboCrossItemRule(kBit), "(3) the creation did not freeze the Shared Song of Time rule from its key");
    rc = MM_Rando_HeadlessPairedHalf();
    if (rc != 0) {
        printf("[TEST] FAIL: MM's creation-time half did not produce a rando world (rc=%d)\n", rc);
        return 1;
    }
    const std::vector<RandoItemId> persisted = Rando::GetStartingItemsFromSave(gSaveContext.save.shipSaveInfo.rando);
    const bool ocarina = INV_CONTENT(ITEM_OCARINA_OF_TIME) == ITEM_OCARINA_OF_TIME;
    const bool songOfTime = CHECK_QUEST_ITEM(QUEST_SONG_TIME) != 0;
    printf("[TEST] mm-paired-start-shared-song-time: (3) the MM half's frozen start %s; ocarina=%d songOfTime=%d "
           "(expected Ocarina, 1, 0)\n",
           ItemNames(persisted).c_str(), ocarina ? 1 : 0, songOfTime ? 1 : 0);
    Check(persisted.size() == 1 && CountOf(persisted, RI_OCARINA) == 1,
          "(3) the paired MM half's start is not the Ocarina alone under the Shared Song of Time rule");
    Check(ocarina, "(3) the paired MM half does not hold the Ocarina of Time (the ocarina stays MM's start)");
    Check(!songOfTime, "(3) the paired MM half starts knowing the Song of Time: the pair's one copy is a pool item");

    auto info = std::make_unique<RandoSaveInfo>();
    memcpy(info.get(), &gSaveContext.save.shipSaveInfo.rando, sizeof(RandoSaveInfo));
    std::vector<RandoCheckId> checkPool;
    std::vector<RandoItemId> itemPool;
    Rando::Logic::GeneratePools(*info, checkPool, itemPool);
    const int songs = CountOf(itemPool, RI_SONG_TIME);
    printf("[TEST] mm-paired-start-shared-song-time: (3) MM's pool holds %d Song of Time (expected 1)\n", songs);
    Check(songs == 1, "(3) MM's pool does not hold the Song of Time the world no longer starts with");

    if (sFailures != 0) {
        printf("[TEST] FAIL: %d contract(s) broken\n", sFailures);
        return 1;
    }
    printf("[TEST] PASS: under the Shared Song of Time rule MM's paired start is the Ocarina alone, the song is an "
           "MM pool item, and MM's identity never moves (#1069)\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
