/**
 * @file oot_chest_silver_paired_test.cpp
 * @brief #1530: Paired-world generation, home-only confinement, shopkeeper single-give,
 *        and default-world invariance for Shuffle Chest Minigame (#1364 / PR #1515) and
 *        Shuffle Silver Rupees (#1363 / PR #1512).
 *
 * Row OoTChestSilverPaired (CTest label `rando`, TIMEOUT 2400, skipped by `--test all`),
 * dispatch `oot-chest-silver-paired` in src/common/test_runner.cpp.
 *
 * Finishes eight paired creations on one fixed seed (Rando_HeadlessSeedTest ->
 * MM_Rando_HeadlessPairedHalf -> OoT_Creation_FinishPairedHalf):
 *   1. OFF       — all options cleared (default baseline)
 *   2. CG_KEYS   — Shuffle Chest Minigame On, Key Rings Off (6 small keys) + real
 *                  EnTakaraMan func_80B17A6C H0..H3 check over the paired world's item
 *   3. CG_RING   — Shuffle Chest Minigame On, Key Rings Selection + Chest Minigame
 *                  Keyring On (1 key ring)
 *   4. SR_ON     — Shuffle Silver Rupees On (80 silver rupee items across 16 vanilla
 *                  puzzles)
 *   5. SR_WALLET — Shuffle Silver Rupees Wallet (16 silver rupee pouches)
 *   6. SR_START  — Shuffle Silver Rupees Start With (80 locations shuffled, 0 silver
 *                  items in pool, 22 silver pouches in StartingInventory)
 *   7. BOTH      — Shuffle Chest Minigame On + Shuffle Silver Rupees On together
 *                  (6 small keys + 80 silver rupee items)
 *   8. OFF_AGAIN — all options cleared again: settingsHash, placementDigest, location
 *                  set, placed OoT items and crossing tables equal OFF's, so the
 *                  generations with the options on leave no residue behind. A
 *                  two-run self-diff, NOT a pin against main: a default world that
 *                  moved would move in both runs. Default-world invariance against
 *                  main is carried by the existing goldens (GoldenSeedDigestDefault
 *                  and the other seed-digest rows) staying green.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <set>
#include <tuple>
#include <vector>

#include <libultraship/bridge.h>

#include "soh/OTRGlobals.h"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/settings.h"
#include "soh/Enhancements/randomizer/item_location.h"
#include "soh/Enhancements/randomizer/3drando/starting_inventory.hpp"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

#include "combo_logic.h"
#include "combo_single_bag.h"
#include "context.h"
#include "crossing_store.h"
#include "game.h"

// At file scope, outside extern "C" (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
extern "C" {
#include "src/overlays/actors/ovl_En_Takara_Man/z_en_takara_man.h"
extern SaveContext gSaveContext;
extern PlayState* OoT_gPlayState;
void func_80B17A6C(EnTakaraMan* thisx, PlayState* play);
void func_80B17AC4(EnTakaraMan* thisx, PlayState* play);
int OoT_Rando_TestTakeQueuedChecks(uint16_t* out, int cap);
}

extern "C" int Rando_HeadlessSeedTest(const char* seedStr);
extern "C" int OoT_ComboLogic_GeneralPassDeferred(void);
extern "C" int MM_Rando_HeadlessPairedHalf(void);
extern "C" int OoT_Creation_FinishPairedHalf(int writeSpoiler);
extern "C" int OoT_ComboLogic_TestHostAcceptsForeign(uint16_t rc);

void RegisterShuffleTreasureChestGame();
void RandomizerOnFlagSetHandler(int16_t flagType, int16_t flag);

namespace {

constexpr const char* kSeed = "RSBSCHESTSILVER1";

const char* const kChestShuffleKey = CVAR_RANDOMIZER_SETTING("ShuffleChestMinigame");
const char* const kKeyRingsModeKey = CVAR_RANDOMIZER_SETTING("ShuffleKeyRings");
const char* const kChestKeyringKey = CVAR_RANDOMIZER_SETTING("ShuffleKeyRingsChestGame");
const char* const kSilverShuffleKey = CVAR_RANDOMIZER_SETTING("ShuffleSilver");

int sCspFailures = 0;

void CspFail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[TEST] oot-chest-silver-paired: FAIL: ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    sCspFailures++;
}

#define CSP_CHECK(cond, ...)      \
    do {                          \
        if (!(cond)) {            \
            CspFail(__VA_ARGS__); \
        }                         \
    } while (0)

/** Swap all four GameInteractor hook maps aside for a scope so COND_VB_SHOULD
 *  (functionsForID) and COND_ID_HOOK leave nothing behind. */
template <typename H> struct CspHooksAside {
    decltype(GameInteractor::RegisteredGameHooks<H>::functions) functions;
    decltype(GameInteractor::RegisteredGameHooks<H>::functionsForID) forId;
    decltype(GameInteractor::RegisteredGameHooks<H>::functionsForPtr) forPtr;
    decltype(GameInteractor::RegisteredGameHooks<H>::functionsForFilter) filters;
    CspHooksAside() {
        functions.swap(GameInteractor::RegisteredGameHooks<H>::functions);
        forId.swap(GameInteractor::RegisteredGameHooks<H>::functionsForID);
        forPtr.swap(GameInteractor::RegisteredGameHooks<H>::functionsForPtr);
        filters.swap(GameInteractor::RegisteredGameHooks<H>::functionsForFilter);
    }
    ~CspHooksAside() {
        functions.swap(GameInteractor::RegisteredGameHooks<H>::functions);
        forId.swap(GameInteractor::RegisteredGameHooks<H>::functionsForID);
        forPtr.swap(GameInteractor::RegisteredGameHooks<H>::functionsForPtr);
        filters.swap(GameInteractor::RegisteredGameHooks<H>::functionsForFilter);
    }
};

struct CspSavedKey {
    const char* key;
    bool existed;
    int32_t value;
    void Save(const char* k) {
        key = k;
        existed = CVarExists(k);
        value = CVarGetInteger(k, 0);
    }
    void Restore() const {
        if (existed) {
            CVarSetInteger(key, value);
        } else {
            CVarClear(key);
        }
    }
};

bool CspIsSilverItem(RandomizerGet rg) {
    return rg >= RG_SHADOW_SILVER_BLADES && rg <= RG_GANONS_CASTLE_MQ_SILVER_SHADOW;
}

bool CspIsMqSilverItem(RandomizerGet rg) {
    return rg >= RG_DODONGOS_CAVERN_MQ_SILVER && rg <= RG_GANONS_CASTLE_MQ_SILVER_SHADOW;
}

bool CspIsHomeConfinedOoTItem(RandomizerGet rg) {
    return rg == RG_TREASURE_GAME_SMALL_KEY || rg == RG_TREASURE_GAME_KEY_RING || CspIsSilverItem(rg);
}

using CrossingTuple = std::tuple<uint16_t, uint8_t, uint16_t>;

struct CspRun {
    bool generated = false;
    int mmHalf = -1;
    int bagStatus = -1;
    bool goalProven = false;
    int crossings = -1;
    uint32_t settingsHash = 0u;
    uint32_t placementDigest = 0u;
    int confinedRows = 0;
    int homeOnlyRows = 0;

    uint8_t chestOption = 0;
    uint8_t chestKeyringOption = 0;
    uint8_t silverOption = 0;

    std::set<RandomizerCheck> locations;
    std::vector<uint16_t> placedItems;
    std::vector<CrossingTuple> ootCrossings;
    std::vector<CrossingTuple> mmCrossings;

    int chestLocations = 0;
    int chestEmpty = 0;
    int chestMixed = 0;
    int chestAcceptsForeign = 0;
    int chestCrossed = 0;

    int silverLocations = 0;
    int silverEmpty = 0;
    int silverMixed = 0;
    int silverAcceptsForeign = 0;
    int silverCrossed = 0;

    int smallKeysOnOoT = 0;
    int keyRingOnOoT = 0;
    int silverItemsOnOoT = 0;
    int mqSilverItemsOnOoT = 0;
    int startingSilverItems = 0;

    int leakedInMmPlacements = 0;
    int leakedInMmCrossings = 0;
};

struct CspConfig {
    const char* name;
    int chestShuffle; // -1 = clear
    int keyRingsMode; // -1 = clear
    int chestKeyring; // -1 = clear
    int silverMode;   // -1 = clear
};

void CspApplyKey(const char* key, int val) {
    if (val < 0) {
        CVarClear(key);
    } else {
        CVarSetInteger(key, val);
    }
}

CspRun CspRunProfile(const CspConfig& cfg) {
    CspRun r;
    Combo_SingleBag_Forget();
    ComboContext_Init();

    CspApplyKey(kChestShuffleKey, cfg.chestShuffle);
    CspApplyKey(kKeyRingsModeKey, cfg.keyRingsMode);
    CspApplyKey(kChestKeyringKey, cfg.chestKeyring);
    CspApplyKey(kSilverShuffleKey, cfg.silverMode);

    const int gen = Rando_HeadlessSeedTest(kSeed);
    r.generated = gen == 0 && OoT_ComboLogic_GeneralPassDeferred() != 0;
    auto world = Rando::Context::GetInstance();
    if (r.generated && world != nullptr) {
        r.settingsHash = gComboCtx.sharedRandoSettingsHash;
        r.chestOption = world->GetOption(RSK_SHUFFLE_CHEST_MINIGAME).Get();
        r.chestKeyringOption = world->GetOption(RSK_KEYRINGS_CHEST_GAME).Get();
        r.silverOption = world->GetOption(RSK_SHUFFLE_SILVER).Get();
        for (RandomizerCheck rc : world->allLocations) {
            r.locations.insert(rc);
        }
        for (RandomizerGet item : StartingInventory) {
            if (CspIsSilverItem(item)) {
                r.startingSilverItems++;
            }
        }
    }

    r.mmHalf = r.generated ? MM_Rando_HeadlessPairedHalf() : -1;
    const ComboSingleBagReport* bag = Combo_SingleBag_LastReport();
    r.bagStatus = bag->status;
    r.goalProven = bag->fill.goalProven;
    r.placementDigest = bag->fill.placementDigest;
    r.confinedRows = bag->confinedRows;
    r.homeOnlyRows = bag->homeOnlyRows;

    const int mmPlacementCount = Combo_Logic_PlacementCount(GAME_MM);
    for (int i = 0; i < mmPlacementCount; ++i) {
        ComboLogicPlacement p = {};
        if (Combo_Logic_PlacementAt(GAME_MM, i, &p) && p.item.originGame == (uint8_t)GAME_OOT &&
            CspIsHomeConfinedOoTItem((RandomizerGet)p.item.id)) {
            r.leakedInMmPlacements++;
        }
    }

    const bool proven = r.mmHalf == 0 && r.bagStatus == RSBS_COMBO_LOGIC_OK && r.goalProven;
    if (proven) {
        r.crossings = OoT_Creation_FinishPairedHalf(0);
    }

    if (r.crossings >= 0 && world != nullptr) {
        r.placedItems.assign((size_t)RC_MAX, 0u);
        for (int c = 1; c < (int)RC_MAX; ++c) {
            const RandomizerCheck rc = (RandomizerCheck)c;
            const RandomizerGet placed = world->GetItemLocation(rc)->GetPlacedRandomizerGet();
            r.placedItems[(size_t)c] = (uint16_t)placed;
            if (placed == RG_TREASURE_GAME_SMALL_KEY) {
                r.smallKeysOnOoT++;
            } else if (placed == RG_TREASURE_GAME_KEY_RING) {
                r.keyRingOnOoT++;
            } else if (CspIsSilverItem(placed)) {
                r.silverItemsOnOoT++;
                if (CspIsMqSilverItem(placed)) {
                    r.mqSilverItemsOnOoT++;
                }
            }
        }

        for (RandomizerCheck rc : r.locations) {
            Rando::Location* loc = Rando::StaticData::GetLocation(rc);
            const RandomizerCheckType type = loc->GetRCType();
            const RandomizerGet placed = world->GetItemLocation(rc)->GetPlacedRandomizerGet();
            const bool accepts = OoT_ComboLogic_TestHostAcceptsForeign((uint16_t)rc) != 0;
            const bool crossed = Combo_Crossings_Lookup(GAME_OOT, (uint16_t)rc) != nullptr;
            if (type == RCTYPE_CHEST_GAME) {
                r.chestLocations++;
                r.chestEmpty += (placed == RG_NONE) ? 1 : 0;
                r.chestMixed += (placed != RG_NONE && placed != loc->GetVanillaItem()) ? 1 : 0;
                r.chestAcceptsForeign += accepts ? 1 : 0;
                r.chestCrossed += crossed ? 1 : 0;
            } else if (type == RCTYPE_SILVER) {
                r.silverLocations++;
                r.silverEmpty += (placed == RG_NONE) ? 1 : 0;
                r.silverMixed += (placed != RG_NONE && placed != loc->GetVanillaItem()) ? 1 : 0;
                r.silverAcceptsForeign += accepts ? 1 : 0;
                r.silverCrossed += crossed ? 1 : 0;
            }
        }

        const int ootCrossCount = Combo_Crossings_Count(GAME_OOT);
        for (int i = 0; i < ootCrossCount; ++i) {
            ComboCrossing c = {};
            if (Combo_Crossings_At(GAME_OOT, i, &c)) {
                r.ootCrossings.emplace_back(c.hostCheck, c.item.originGame, c.item.id);
            }
        }
        const int mmCrossCount = Combo_Crossings_Count(GAME_MM);
        for (int i = 0; i < mmCrossCount; ++i) {
            ComboCrossing c = {};
            if (Combo_Crossings_At(GAME_MM, i, &c)) {
                r.mmCrossings.emplace_back(c.hostCheck, c.item.originGame, c.item.id);
                if (c.item.originGame == (uint8_t)GAME_OOT && CspIsHomeConfinedOoTItem((RandomizerGet)c.item.id)) {
                    r.leakedInMmCrossings++;
                }
            }
        }
    }

    printf("[TEST] %-9s: gen=%d mm=%d bag=%s proven=%d fin=%d hash=%08X digest=%08X locs=%zu (cg=%d, sr=%d) "
           "ootPlaced(keys=%d, ring=%d, sr=%d, startSr=%d) bag(conf=%d, home=%d) mmLeak(pl=%d, cr=%d)\n",
           cfg.name, r.generated ? 1 : 0, r.mmHalf, Combo_Logic_StatusName(r.bagStatus), r.goalProven ? 1 : 0,
           r.crossings, (unsigned)r.settingsHash, (unsigned)r.placementDigest, r.locations.size(), r.chestLocations,
           r.silverLocations, r.smallKeysOnOoT, r.keyRingOnOoT, r.silverItemsOnOoT, r.startingSilverItems,
           r.confinedRows, r.homeOnlyRows, r.leakedInMmPlacements, r.leakedInMmCrossings);
    return r;
}

void CspCheckFinished(const char* name, const CspRun& r) {
    CSP_CHECK(r.generated && r.mmHalf == 0 && r.bagStatus == RSBS_COMBO_LOGIC_OK && r.goalProven && r.crossings >= 0,
              "%s: paired creation did not finish cleanly (gen=%d mm=%d bag=%s proven=%d crossings=%d)", name,
              r.generated ? 1 : 0, r.mmHalf, Combo_Logic_StatusName(r.bagStatus), r.goalProven ? 1 : 0, r.crossings);
    CSP_CHECK(r.leakedInMmPlacements == 0 && r.leakedInMmCrossings == 0,
              "%s: %d chest-game/silver item(s) landed in MM placements and %d in MM crossings", name,
              r.leakedInMmPlacements, r.leakedInMmCrossings);
}

// ---- Shopkeeper single-give over the finished paired CG_KEYS world -----------------------

struct CspTakaraStage {
    PlayState* play = nullptr;
    Player* player = nullptr;
    EnTakaraMan* takaraMan = nullptr;
};

void CspArmTakaraFrame(CspTakaraStage& s, bool taken) {
    std::memset(s.play, 0, sizeof(PlayState));
    std::memset(s.player, 0, sizeof(Player));
    s.play->sceneNum = SCENE_TREASURE_BOX_SHOP;
    s.play->actorCtx.actorLists[ACTORCAT_PLAYER].head = &s.player->actor;
    s.player->actor.id = ACTOR_PLAYER;
    s.player->getItemId = GI_NONE;
    s.takaraMan->actor.id = ACTOR_EN_TAKARA_MAN;
    s.takaraMan->actor.xzDistToPlayer = 20.0f;
    s.takaraMan->actor.yDistToPlayer = 0.0f;
    s.takaraMan->actor.parent = taken ? &s.player->actor : nullptr;
    s.takaraMan->actionFunc = func_80B17A6C;
}

void CspCheckPairedShopkeeper(Rando::Context& world) {
    if (OTRGlobals::Instance == nullptr || OTRGlobals::Instance->gRandomizer == nullptr) {
        CspFail("H: OTRGlobals::Instance->gRandomizer is null");
        return;
    }
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    Rando::ItemLocation* shopkeeper = world.GetItemLocation(RC_MARKET_TREASURE_CHEST_GAME_SHOPKEEPER);
    const RandomizerGet placed = shopkeeper->GetPlacedRandomizerGet();
    const bool shopCrossed =
        Combo_Crossings_Lookup(GAME_OOT, (uint16_t)RC_MARKET_TREASURE_CHEST_GAME_SHOPKEEPER) != nullptr;
    CSP_CHECK(placed != RG_NONE && !shopCrossed, "H: paired CG_KEYS shopkeeper check holds placed=%d, crossed=%d",
              (int)placed, shopCrossed ? 1 : 0);

    CspTakaraStage s;
    s.play = (PlayState*)calloc(1, sizeof(PlayState));
    s.player = (Player*)calloc(1, sizeof(Player));
    s.takaraMan = (EnTakaraMan*)calloc(1, sizeof(EnTakaraMan));
    if (s.play == nullptr || s.player == nullptr || s.takaraMan == nullptr) {
        CspFail("H: failed to allocate stub PlayState/Player/EnTakaraMan");
        free(s.play);
        free(s.player);
        free(s.takaraMan);
        return;
    }

    std::unique_ptr<SaveContext> savedSave = std::make_unique<SaveContext>();
    std::memcpy(savedSave.get(), &gSaveContext, sizeof(SaveContext));
    const RandomizerCheckStatus priorStatus = shopkeeper->GetCheckStatus();
    const uint8_t priorOption = world.GetOption(RSK_SHUFFLE_CHEST_MINIGAME).Get();
    PlayState* const priorPlay = OoT_gPlayState;
    const GameId priorGame = Context_GetCurrentGame();
    Context_SetCurrentGame(GAME_OOT);

    std::memset(&gSaveContext, 0, sizeof(SaveContext));
    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    shopkeeper->SetCheckStatus(RCSHOW_UNCHECKED);
    const GetItemEntry seedEntry = world.GetFinalGIEntry(RC_MARKET_TREASURE_CHEST_GAME_SHOPKEEPER, true, GI_DOOR_KEY);
    OoT_gPlayState = s.play;
    (void)OoT_Rando_TestTakeQueuedChecks(nullptr, 0);
    {
        CspHooksAside<GameInteractor::OnVanillaBehavior> asideVb;
        CspHooksAside<GameInteractor::OnActorInit> asideInit;
        CspHooksAside<GameInteractor::OnFlagSet> asideFlags;
        CspHooksAside<GameInteractor::OnRandoSetCheckStatus> asideStatus;
        RegisterShuffleTreasureChestGame();
        const HOOK_ID flagHook =
            GameInteractor::Instance->RegisterGameHook<GameInteractor::OnFlagSet>(RandomizerOnFlagSetHandler);

        // H0 control: setting flag while unchecked queues RC_MARKET_TREASURE_CHEST_GAME_SHOPKEEPER.
        Flags_SetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_SHOPKEEPER);
        uint16_t queued[4] = {};
        const int h0Queued = OoT_Rando_TestTakeQueuedChecks(queued, 4);
        CSP_CHECK(h0Queued == 1 && queued[0] == (uint16_t)RC_MARKET_TREASURE_CHEST_GAME_SHOPKEEPER,
                  "H0: setting shopkeeper flag queued %d (first %u)", h0Queued, (unsigned)queued[0]);
        Flags_UnsetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_SHOPKEEPER);
        shopkeeper->SetCheckStatus(RCSHOW_UNCHECKED);

        // H1 first frame: marks collected before setting flag, offers paired seed item, queues 0.
        CspArmTakaraFrame(s, false);
        func_80B17A6C(s.takaraMan, s.play);
        const int h1Queued = OoT_Rando_TestTakeQueuedChecks(queued, 4);
        const bool h1Offered = s.player->interactRangeActor == &s.takaraMan->actor &&
                               s.player->getItemEntry.modIndex == seedEntry.modIndex &&
                               s.player->getItemEntry.itemId == seedEntry.itemId &&
                               s.player->getItemEntry.getItemId == seedEntry.getItemId;
        CSP_CHECK(Flags_GetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_SHOPKEEPER) != 0 &&
                      shopkeeper->HasObtained() && h1Queued == 0 && h1Offered,
                  "H1: paired shopkeeper frame failed (offered=%d queued=%d)", h1Offered ? 1 : 0, h1Queued);

        // H2 second frame before take: continues offering without queueing.
        CspArmTakaraFrame(s, false);
        func_80B17A6C(s.takaraMan, s.play);
        const int h2Queued = OoT_Rando_TestTakeQueuedChecks(queued, 4);
        const bool h2Offered = s.player->interactRangeActor == &s.takaraMan->actor;
        CSP_CHECK(h2Offered && h2Queued == 0, "H2: second frame offered=%d queued=%d", h2Offered ? 1 : 0, h2Queued);

        // H3 after Link takes it: moves on to func_80B17AC4 without offering or queueing.
        CspArmTakaraFrame(s, true);
        func_80B17A6C(s.takaraMan, s.play);
        const int h3Queued = OoT_Rando_TestTakeQueuedChecks(queued, 4);
        CSP_CHECK(s.takaraMan->actionFunc == func_80B17AC4 && s.player->interactRangeActor == nullptr && h3Queued == 0,
                  "H3: after take movedOn=%d queued=%d", s.takaraMan->actionFunc == func_80B17AC4 ? 1 : 0, h3Queued);

        GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnFlagSet>(flagHook);
        world.GetOption(RSK_SHUFFLE_CHEST_MINIGAME).Set(RO_GENERIC_OFF);
        RegisterShuffleTreasureChestGame();
    }
    (void)OoT_Rando_TestTakeQueuedChecks(nullptr, 0);
    world.GetOption(RSK_SHUFFLE_CHEST_MINIGAME).Set(priorOption);
    shopkeeper->SetCheckStatus(priorStatus);
    OoT_gPlayState = priorPlay;
    Context_SetCurrentGame(priorGame);
    std::memcpy(&gSaveContext, savedSave.get(), sizeof(SaveContext));

    free(s.play);
    free(s.player);
    free(s.takaraMan);
}

} // namespace

extern "C" int OoT_ChestSilverPairedTest_Run(void) {
    printf("[TEST] oot-chest-silver-paired: finished paired worlds keep chest-game keys/keyring and silver rupees in "
           "Ocarina of Time across every setting, give the shopkeeper check once, and leave default worlds unchanged "
           "(#1530)\n");
    if (OTRGlobals::Instance == nullptr || OTRGlobals::Instance->gRandoContext == nullptr) {
        printf("[TEST] FAIL: oot-chest-silver-paired needs OTRGlobals' randomizer context\n");
        return 1;
    }
    sCspFailures = 0;

    std::unique_ptr<SaveContext> priorSave = std::make_unique<SaveContext>();
    std::memcpy(priorSave.get(), &gSaveContext, sizeof(SaveContext));
    const GameId priorGame = Context_GetCurrentGame();

    CspSavedKey savedChest;
    CspSavedKey savedKeyRings;
    CspSavedKey savedChestRing;
    CspSavedKey savedSilver;
    savedChest.Save(kChestShuffleKey);
    savedKeyRings.Save(kKeyRingsModeKey);
    savedChestRing.Save(kChestKeyringKey);
    savedSilver.Save(kSilverShuffleKey);

    const CspRun off = CspRunProfile({ "OFF", -1, -1, -1, -1 });
    const CspRun cgKeys = CspRunProfile({ "CG_KEYS", RO_GENERIC_ON, -1, -1, -1 });
    if (cgKeys.crossings >= 0 && Rando::Context::GetInstance() != nullptr) {
        CspCheckPairedShopkeeper(*Rando::Context::GetInstance());
    }
    const CspRun cgRing =
        CspRunProfile({ "CG_RING", RO_GENERIC_ON, RO_KEYRINGS_SELECTION, RO_KEYRING_FOR_DUNGEON_ON, -1 });
    const CspRun srOn = CspRunProfile({ "SR_ON", -1, -1, -1, RO_SHUFFLE_SILVER_ON });
    const CspRun srWallet = CspRunProfile({ "SR_WALLET", -1, -1, -1, RO_SHUFFLE_SILVER_WALLET });
    const CspRun srStart = CspRunProfile({ "SR_START", -1, -1, -1, RO_SHUFFLE_SILVER_STARTWITH });
    const CspRun both = CspRunProfile({ "BOTH", RO_GENERIC_ON, -1, -1, RO_SHUFFLE_SILVER_ON });
    const CspRun offAgain = CspRunProfile({ "OFF_AGAIN", -1, -1, -1, -1 });

    savedChest.Restore();
    savedKeyRings.Restore();
    savedChestRing.Restore();
    savedSilver.Restore();

    CspCheckFinished("OFF", off);
    CspCheckFinished("CG_KEYS", cgKeys);
    CspCheckFinished("CG_RING", cgRing);
    CspCheckFinished("SR_ON", srOn);
    CspCheckFinished("SR_WALLET", srWallet);
    CspCheckFinished("SR_START", srStart);
    CspCheckFinished("BOTH", both);
    CspCheckFinished("OFF_AGAIN", offAgain);

    // OFF baseline checks.
    CSP_CHECK(off.chestLocations == 0 && off.silverLocations == 0 && off.smallKeysOnOoT == 0 && off.keyRingOnOoT == 0 &&
                  off.silverItemsOnOoT == 0 && off.startingSilverItems == 0,
              "OFF: expected zero chest-game/silver locations or items");

    // CG_KEYS checks (6 small keys, 11 locations).
    CSP_CHECK(cgKeys.chestOption == RO_GENERIC_ON && cgKeys.chestLocations == 11 &&
                  cgKeys.locations.size() == off.locations.size() + 11,
              "CG_KEYS: expected 11 chest-game locations (+11 over OFF), got %d (%zu vs %zu)", cgKeys.chestLocations,
              cgKeys.locations.size(), off.locations.size());
    CSP_CHECK(cgKeys.chestEmpty == 0 && cgKeys.chestMixed > 0 && cgKeys.chestAcceptsForeign == 0 &&
                  cgKeys.chestCrossed == 0,
              "CG_KEYS: empty=%d mixed=%d accepts=%d crossed=%d", cgKeys.chestEmpty, cgKeys.chestMixed,
              cgKeys.chestAcceptsForeign, cgKeys.chestCrossed);
    CSP_CHECK(cgKeys.smallKeysOnOoT == 6 && cgKeys.keyRingOnOoT == 0 && cgKeys.silverItemsOnOoT == 0,
              "CG_KEYS: OoT placed keys=%d ring=%d silver=%d, want 6/0/0", cgKeys.smallKeysOnOoT, cgKeys.keyRingOnOoT,
              cgKeys.silverItemsOnOoT);
    CSP_CHECK(cgKeys.confinedRows == off.confinedRows + 6 && cgKeys.homeOnlyRows == off.homeOnlyRows + 6,
              "CG_KEYS: bag confined=%d (want %d) homeOnly=%d (want %d)", cgKeys.confinedRows, off.confinedRows + 6,
              cgKeys.homeOnlyRows, off.homeOnlyRows + 6);

    // CG_RING checks (1 key ring, 11 locations).
    CSP_CHECK(cgRing.chestOption == RO_GENERIC_ON && cgRing.chestKeyringOption == RO_KEYRING_FOR_DUNGEON_ON &&
                  cgRing.chestLocations == 11 && cgRing.chestEmpty == 0 && cgRing.chestAcceptsForeign == 0 &&
                  cgRing.chestCrossed == 0,
              "CG_RING: option=%u ringOpt=%u locs=%d empty=%d", (unsigned)cgRing.chestOption,
              (unsigned)cgRing.chestKeyringOption, cgRing.chestLocations, cgRing.chestEmpty);
    CSP_CHECK(cgRing.smallKeysOnOoT == 0 && cgRing.keyRingOnOoT == 1 && cgRing.silverItemsOnOoT == 0,
              "CG_RING: OoT placed keys=%d ring=%d silver=%d, want 0/1/0", cgRing.smallKeysOnOoT, cgRing.keyRingOnOoT,
              cgRing.silverItemsOnOoT);
    CSP_CHECK(cgRing.confinedRows == off.confinedRows + 1 && cgRing.homeOnlyRows == off.homeOnlyRows + 1,
              "CG_RING: bag confined=%d (want %d) homeOnly=%d (want %d)", cgRing.confinedRows, off.confinedRows + 1,
              cgRing.homeOnlyRows, off.homeOnlyRows + 1);

    // SR_ON checks (80 vanilla silver items, 80 locations).
    CSP_CHECK(srOn.silverOption == RO_SHUFFLE_SILVER_ON && srOn.silverLocations == 80 &&
                  srOn.locations.size() == off.locations.size() + 80,
              "SR_ON: expected 80 silver locations (+80 over OFF), got %d (%zu vs %zu)", srOn.silverLocations,
              srOn.locations.size(), off.locations.size());
    CSP_CHECK(srOn.silverEmpty == 0 && srOn.silverMixed > 0 && srOn.silverAcceptsForeign == 0 &&
                  srOn.silverCrossed == 0,
              "SR_ON: empty=%d mixed=%d accepts=%d crossed=%d", srOn.silverEmpty, srOn.silverMixed,
              srOn.silverAcceptsForeign, srOn.silverCrossed);
    CSP_CHECK(srOn.silverItemsOnOoT == 80 && srOn.mqSilverItemsOnOoT == 0 && srOn.startingSilverItems == 0,
              "SR_ON: OoT placed silver=%d mqSilver=%d startSilver=%d, want 80/0/0", srOn.silverItemsOnOoT,
              srOn.mqSilverItemsOnOoT, srOn.startingSilverItems);
    CSP_CHECK(srOn.confinedRows == off.confinedRows + 80 && srOn.homeOnlyRows == off.homeOnlyRows + 80,
              "SR_ON: bag confined=%d (want %d) homeOnly=%d (want %d)", srOn.confinedRows, off.confinedRows + 80,
              srOn.homeOnlyRows, off.homeOnlyRows + 80);

    // SR_WALLET checks (16 vanilla silver pouches, 80 locations).
    CSP_CHECK(srWallet.silverOption == RO_SHUFFLE_SILVER_WALLET && srWallet.silverLocations == 80 &&
                  srWallet.silverEmpty == 0 && srWallet.silverMixed > 0 && srWallet.silverCrossed == 0,
              "SR_WALLET: option=%u locs=%d empty=%d mixed=%d crossed=%d", (unsigned)srWallet.silverOption,
              srWallet.silverLocations, srWallet.silverEmpty, srWallet.silverMixed, srWallet.silverCrossed);
    CSP_CHECK(srWallet.silverItemsOnOoT == 16 && srWallet.mqSilverItemsOnOoT == 0 && srWallet.startingSilverItems == 0,
              "SR_WALLET: OoT placed silver=%d mqSilver=%d startSilver=%d, want 16/0/0", srWallet.silverItemsOnOoT,
              srWallet.mqSilverItemsOnOoT, srWallet.startingSilverItems);
    CSP_CHECK(srWallet.confinedRows == off.confinedRows + 16 && srWallet.homeOnlyRows == off.homeOnlyRows + 16,
              "SR_WALLET: bag confined=%d (want %d) homeOnly=%d (want %d)", srWallet.confinedRows,
              off.confinedRows + 16, srWallet.homeOnlyRows, off.homeOnlyRows + 16);

    // SR_START checks (0 silver items in pool, 22 in StartingInventory, 80 locations).
    CSP_CHECK(srStart.silverOption == RO_SHUFFLE_SILVER_STARTWITH && srStart.silverLocations == 80 &&
                  srStart.silverEmpty == 0 && srStart.silverMixed > 0 && srStart.silverCrossed == 0,
              "SR_START: option=%u locs=%d empty=%d mixed=%d crossed=%d", (unsigned)srStart.silverOption,
              srStart.silverLocations, srStart.silverEmpty, srStart.silverMixed, srStart.silverCrossed);
    CSP_CHECK(srStart.silverItemsOnOoT == 0 && srStart.startingSilverItems == 22,
              "SR_START: OoT placed silver=%d startSilver=%d, want 0/22", srStart.silverItemsOnOoT,
              srStart.startingSilverItems);
    CSP_CHECK(srStart.confinedRows == off.confinedRows && srStart.homeOnlyRows == off.homeOnlyRows,
              "SR_START: bag confined=%d (want %d) homeOnly=%d (want %d)", srStart.confinedRows, off.confinedRows,
              srStart.homeOnlyRows, off.homeOnlyRows);

    // BOTH checks (6 small keys + 80 silver items, 91 extra locations).
    CSP_CHECK(both.chestOption == RO_GENERIC_ON && both.silverOption == RO_SHUFFLE_SILVER_ON &&
                  both.chestLocations == 11 && both.silverLocations == 80 &&
                  both.locations.size() == off.locations.size() + 91,
              "BOTH: cg=%d sr=%d locs=%zu (want %zu)", both.chestLocations, both.silverLocations, both.locations.size(),
              off.locations.size() + 91);
    CSP_CHECK(both.chestEmpty == 0 && both.silverEmpty == 0 && both.chestCrossed == 0 && both.silverCrossed == 0 &&
                  both.smallKeysOnOoT == 6 && both.silverItemsOnOoT == 80,
              "BOTH: keys=%d silver=%d cgEmpty=%d srEmpty=%d", both.smallKeysOnOoT, both.silverItemsOnOoT,
              both.chestEmpty, both.silverEmpty);
    CSP_CHECK(both.confinedRows == off.confinedRows + 86 && both.homeOnlyRows == off.homeOnlyRows + 86,
              "BOTH: bag confined=%d (want %d) homeOnly=%d (want %d)", both.confinedRows, off.confinedRows + 86,
              both.homeOnlyRows, off.homeOnlyRows + 86);

    // OFF_AGAIN: no residue from the generations above (a self-diff against OFF,
    // not a pin against main; the goldens carry that).
    CSP_CHECK(offAgain.settingsHash == off.settingsHash && offAgain.placementDigest == off.placementDigest &&
                  offAgain.locations == off.locations && offAgain.placedItems == off.placedItems &&
                  offAgain.ootCrossings == off.ootCrossings && offAgain.mmCrossings == off.mmCrossings,
              "OFF_AGAIN: the default world diverged from OFF after the chest-game/silver generations "
              "(hash %08X vs %08X, digest %08X vs %08X)",
              (unsigned)offAgain.settingsHash, (unsigned)off.settingsHash, (unsigned)offAgain.placementDigest,
              (unsigned)off.placementDigest);

    if (auto world = Rando::Context::GetInstance(); world != nullptr) {
        world->GetOption(RSK_SHUFFLE_CHEST_MINIGAME).Set(RO_GENERIC_OFF);
        world->GetOption(RSK_KEYRINGS_CHEST_GAME).Set(RO_KEYRING_FOR_DUNGEON_OFF);
        world->GetOption(RSK_SHUFFLE_SILVER).Set(RO_SHUFFLE_SILVER_OFF);
    }
    Combo_SingleBag_Forget();
    ComboContext_Init();
    Context_SetCurrentGame(priorGame);
    std::memcpy(&gSaveContext, priorSave.get(), sizeof(SaveContext));

    if (sCspFailures != 0) {
        printf("[TEST] FAIL (oot-chest-silver-paired): %d failure(s)\n", sCspFailures);
        return 1;
    }
    printf("[TEST] PASS: oot-chest-silver-paired\n");
    return 0;
}

#else

extern "C" int OoT_ChestSilverPairedTest_Run(void) {
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
