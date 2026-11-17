/**
 * @file oot_chest_silver_confinement_test.cpp
 * @brief #1530: ROM-free confinement test for Shuffle Chest Minigame (#1364 / PR #1515)
 *        and Shuffle Silver Rupees (#1363 / PR #1512) on the OoT single-bag engine
 *        (games/oot/soh/Enhancements/randomizer/ComboLogicEngineOoT.cpp).
 *
 * Row OoTChestSilverConfinement (CTest label `redship`, included in `--test all`),
 * dispatch `oot-chest-silver-confinement` in src/common/test_runner.cpp.
 *
 *  A  GROUP AND ROW MASKS ACROSS ALL 16 OPTION COMBINATIONS:
 *     Sweeps (Shuffle Chest Minigame in {Off, On}) x (Shuffle Silver Rupees in
 *     {Off, On, Wallet, Start With}) x (Shuffle Songs in {Anywhere, Song Locations}
 *     with pairing active and frozen). Verifies:
 *       - wideGroups answers RSBS_COMBO_GROUP_SONG_LOCATIONS iff Song Locations;
 *       - under Song Locations, the 12 OoT song checks answer ONLY the wide song
 *         group (never mixing wide and local bits);
 *       - every other real OoT check answers local group 0x00000001u iff at least
 *         one of Shuffle Chest Minigame or Shuffle Silver Rupees is on, and 0u when
 *         both are off (leaving default worlds untouched);
 *       - OoT_ComboLogic_PoolRowConfineMasks assigns 0x00000001u to
 *         RG_TREASURE_GAME_SMALL_KEY and RG_TREASURE_GAME_KEY_RING iff Shuffle Chest
 *         Minigame is on, assigns 0x00000001u to all 22 ITEMTYPE_SILVER items iff
 *         Shuffle Silver Rupees is on, assigns RSBS_COMBO_GROUP_SONG_LOCATIONS to all
 *         13 OoT songs iff Song Locations is armed, and assigns 0u to every other
 *         item (including dungeon/fortress small keys and chest-game green rupee /
 *         heart).
 *  B  CLASSIFIER AND FOREIGN-HOST BOUNDARIES:
 *     Both chest-game key items and all 22 silver rupee items classify as
 *     RSBS_FILL_CLASS_PROGRESSION with armedBy == 0u and sharedKind == 0u. All 11
 *     RCTYPE_CHEST_GAME checks and all 154 RCTYPE_SILVER checks refuse foreign MM
 *     items (OoT_ComboLogic_TestHostAcceptsForeign == 0), while
 *     RC_MARKET_TREASURE_CHEST_GAME_REWARD accepts (1). Edge ids (0, RC_MAX, RG_MAX,
 *     0xFFFF) and null/zero-count arguments return 0 cleanly.
 *  C  TRANSIENT CONTEXT HYGIENE:
 *     Leaves no heap Rando::Context behind when none existed on entry, and restores
 *     every gComboCtx field and song-host test override on exit.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/logic.h"

#include "combo_logic.h"
#include "combo_single_bag.h"
#include "combo_song_pool.h"
#include "context.h"
#include "game.h"
#include "shared_items.h"

extern "C" {
int OoT_ComboLogic_TestEnsureLocationTable(void);
int OoT_ComboLogic_TestEnsureItemTableTransient(void);
int OoT_ComboLogic_TestSongCheckIndex(uint16_t rc);
int OoT_ComboLogic_TestHostAcceptsForeign(uint16_t rc);
int OoT_ComboLogic_TestRandoContextExists(void);
const ComboLogicEngine* OoT_ComboLogic_TestEngine(void);
int OoT_ComboLogic_ClassifyItem(uint16_t id, ComboItemClassRow* out);
}

namespace {

constexpr uint32_t kCscLocalGroup = 0x00000001u;

int sCscFailures = 0;

void CscFail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[TEST] oot-chest-silver-confinement: FAIL: ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    sCscFailures++;
}

#define CSC_CHECK(cond, ...)      \
    do {                          \
        if (!(cond)) {            \
            CscFail(__VA_ARGS__); \
        }                         \
    } while (0)

/** Scoped Rando::Context + gComboCtx pairing guard that leaves zero heap context
 *  behind when none existed on entry (matching OoT_ComboLogic_TestHoldSongContext). */
struct CscScopedState {
    std::shared_ptr<Rando::Context> ctx;
    bool createdCtx = false;
    uint8_t priorChestGame = 0;
    uint8_t priorSilver = 0;
    uint8_t priorSongs = 0;
    bool priorSourceIsRando = false;
    uint32_t priorSettingsHash = 0u;
    uint8_t priorFormatVersion = 0u;
    int priorSongOverride = -1;

    bool Acquire() {
        priorSourceIsRando = gComboCtx.sourceIsRando;
        priorSettingsHash = gComboCtx.sharedRandoSettingsHash;
        priorFormatVersion = gComboCtx.comboSettings.formatVersion;
        priorSongOverride = Combo_SongHostsTestOverride();

        ctx = Rando::Context::GetInstance();
        if (ctx == nullptr) {
            ctx = Rando::Context::CreateInstance();
            if (ctx == nullptr) {
                return false;
            }
            ctx->GetLogic()->SetContext(nullptr);
            createdCtx = true;
        } else {
            priorChestGame = ctx->GetOption(RSK_SHUFFLE_CHEST_MINIGAME).Get();
            priorSilver = ctx->GetOption(RSK_SHUFFLE_SILVER).Get();
            priorSongs = ctx->GetOption(RSK_SHUFFLE_SONGS).Get();
        }
        return true;
    }

    ~CscScopedState() {
        Combo_SongHostsForceForTest(priorSongOverride);
        gComboCtx.sourceIsRando = priorSourceIsRando;
        gComboCtx.sharedRandoSettingsHash = priorSettingsHash;
        gComboCtx.comboSettings.formatVersion = priorFormatVersion;
        if (ctx != nullptr && !createdCtx) {
            ctx->GetOption(RSK_SHUFFLE_CHEST_MINIGAME).Set(priorChestGame);
            ctx->GetOption(RSK_SHUFFLE_SILVER).Set(priorSilver);
            ctx->GetOption(RSK_SHUFFLE_SONGS).Set(priorSongs);
        }
        ctx.reset();
    }
};

bool CscIsRealCheck(RandomizerCheck rc) {
    if (rc <= RC_UNKNOWN_CHECK || rc >= RC_MAX) {
        return false;
    }
    Rando::Location* loc = Rando::StaticData::GetLocation(rc);
    return loc != nullptr && loc->GetRandomizerCheck() == rc;
}

bool CscIsRealItem(RandomizerGet rg) {
    if (rg <= RG_NONE || rg >= RG_MAX) {
        return false;
    }
    const Rando::Item& item = Rando::StaticData::RetrieveItem(rg);
    return item.GetRandomizerGet() == rg && !item.GetName().GetEnglish().empty();
}

void CscCheckEdgeGuards(const ComboLogicEngine* oot) {
    uint32_t dummyOut[2] = { 0xDEADBEEFu, 0xDEADBEEFu };
    const uint16_t dummyItems[2] = { (uint16_t)RG_TREASURE_GAME_SMALL_KEY, (uint16_t)RG_SHADOW_SILVER_BLADES };
    CSC_CHECK(OoT_ComboLogic_PoolRowConfineMasks(nullptr, 2, dummyOut) == 0, "null items did not return 0");
    CSC_CHECK(OoT_ComboLogic_PoolRowConfineMasks(dummyItems, 2, nullptr) == 0, "null out did not return 0");
    CSC_CHECK(OoT_ComboLogic_PoolRowConfineMasks(dummyItems, 0, dummyOut) == 0, "count 0 did not return 0");

    const uint16_t badItems[3] = { 0u, (uint16_t)RG_MAX, 0xFFFFu };
    uint32_t badMasks[3] = { 9u, 9u, 9u };
    CSC_CHECK(OoT_ComboLogic_PoolRowConfineMasks(badItems, 3, badMasks) == 0 && badMasks[0] == 0u &&
                  badMasks[1] == 0u && badMasks[2] == 0u,
              "out-of-range item ids produced non-zero confine masks");

    CSC_CHECK(oot->hostGroups(oot->self, 0u) == 0u && oot->hostGroups(oot->self, (uint16_t)RC_MAX) == 0u &&
                  oot->hostGroups(oot->self, 0xFFFFu) == 0u,
              "out-of-range check ids produced non-zero hostGroups");
}

void CscSweepOptionMatrix(Rando::Context& ctx, const ComboLogicEngine* oot) {
    std::vector<uint16_t> realItems;
    for (int i = 1; i < (int)RG_MAX; ++i) {
        if (CscIsRealItem((RandomizerGet)i)) {
            realItems.push_back((uint16_t)i);
        }
    }
    CSC_CHECK(!realItems.empty(), "A: static item table yielded no real OoT items");

    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSettingsHash = 0x5EED1530u;
    gComboCtx.comboSettings.formatVersion = 1u;
    Combo_SongHostsForceForTest(-1);

    const uint8_t chestModes[2] = { RO_GENERIC_OFF, RO_GENERIC_ON };
    const uint8_t silverModes[4] = {
        RO_SHUFFLE_SILVER_OFF,
        RO_SHUFFLE_SILVER_ON,
        RO_SHUFFLE_SILVER_WALLET,
        RO_SHUFFLE_SILVER_STARTWITH,
    };
    const uint8_t songModes[2] = { (uint8_t)RSBS_OOT_SONGS_ANYWHERE, (uint8_t)RSBS_OOT_SONGS_SONG_LOCATIONS };

    int realCheckCount = 0;
    for (int c = 1; c < (int)RC_MAX; ++c) {
        if (CscIsRealCheck((RandomizerCheck)c)) {
            realCheckCount++;
        }
    }
    CSC_CHECK(realCheckCount > 0, "A: static location table yielded no real OoT checks");

    for (uint8_t chestMode : chestModes) {
        for (uint8_t silverMode : silverModes) {
            for (uint8_t songMode : songModes) {
                ctx.GetOption(RSK_SHUFFLE_CHEST_MINIGAME).Set(chestMode);
                ctx.GetOption(RSK_SHUFFLE_SILVER).Set(silverMode);
                ctx.GetOption(RSK_SHUFFLE_SONGS).Set(songMode);

                const bool chestActive = chestMode != RO_GENERIC_OFF;
                const bool silverActive = silverMode != RO_SHUFFLE_SILVER_OFF;
                const bool homeActive = chestActive || silverActive;
                const bool songUnion = songMode == (uint8_t)RSBS_OOT_SONGS_SONG_LOCATIONS;

                CSC_CHECK(OoT_ComboLogic_SongPoolUnion() == songUnion,
                          "A(cg=%u,sr=%u,sg=%u): SongPoolUnion=%d, want %d", (unsigned)chestMode, (unsigned)silverMode,
                          (unsigned)songMode, OoT_ComboLogic_SongPoolUnion() ? 1 : 0, songUnion ? 1 : 0);
                const uint32_t wantWide = songUnion ? RSBS_COMBO_GROUP_SONG_LOCATIONS : 0u;
                CSC_CHECK(oot->wideGroups(oot->self) == wantWide,
                          "A(cg=%u,sr=%u,sg=%u): wideGroups=0x%08X, want 0x%08X", (unsigned)chestMode,
                          (unsigned)silverMode, (unsigned)songMode, (unsigned)oot->wideGroups(oot->self),
                          (unsigned)wantWide);

                int wideSongHosts = 0;
                int localHosts = 0;
                int mixedNamespaceHosts = 0;
                int wrongHosts = 0;
                for (int c = 1; c < (int)RC_MAX; ++c) {
                    const uint32_t g = oot->hostGroups(oot->self, (uint16_t)c);
                    const bool isReal = CscIsRealCheck((RandomizerCheck)c);
                    const bool isSongCheck = isReal && OoT_ComboLogic_TestSongCheckIndex((uint16_t)c) >= 0;
                    if ((g & RSBS_COMBO_GROUP_WIDE_MASK) != 0u && (g & ~RSBS_COMBO_GROUP_WIDE_MASK) != 0u) {
                        mixedNamespaceHosts++;
                    }
                    uint32_t wantHostGroup = 0u;
                    if (isReal) {
                        if (isSongCheck && songUnion) {
                            wantHostGroup = RSBS_COMBO_GROUP_SONG_LOCATIONS;
                        } else if (homeActive) {
                            wantHostGroup = kCscLocalGroup;
                        }
                    }
                    if (g == RSBS_COMBO_GROUP_SONG_LOCATIONS) {
                        wideSongHosts++;
                    } else if (g == kCscLocalGroup) {
                        localHosts++;
                    }
                    if (g != wantHostGroup) {
                        wrongHosts++;
                    }
                }
                const int wantWideHosts = songUnion ? RSBS_SONG_CHECK_OOT_COUNT : 0;
                const int wantLocalHosts = homeActive ? (realCheckCount - wantWideHosts) : 0;
                CSC_CHECK(mixedNamespaceHosts == 0,
                          "A(cg=%u,sr=%u,sg=%u): %d host(s) answered wide and local bits together", (unsigned)chestMode,
                          (unsigned)silverMode, (unsigned)songMode, mixedNamespaceHosts);
                CSC_CHECK(wrongHosts == 0 && wideSongHosts == wantWideHosts && localHosts == wantLocalHosts,
                          "A(cg=%u,sr=%u,sg=%u): hostGroups wide=%d (want %d), local=%d (want %d), wrong=%d",
                          (unsigned)chestMode, (unsigned)silverMode, (unsigned)songMode, wideSongHosts, wantWideHosts,
                          localHosts, wantLocalHosts, wrongHosts);

                std::vector<uint32_t> masks(realItems.size(), 0xFFFFFFFFu);
                const int confined =
                    OoT_ComboLogic_PoolRowConfineMasks(realItems.data(), (int)realItems.size(), masks.data());
                int expectedConfined = 0;
                int chestKeyMasked = 0;
                int silverMasked = 0;
                int songMasked = 0;
                int wrongMasks = 0;
                for (size_t idx = 0; idx < realItems.size(); ++idx) {
                    const RandomizerGet rg = (RandomizerGet)realItems[idx];
                    const ItemType type = Rando::StaticData::RetrieveItem(rg).GetItemType();
                    const bool isChestKey = rg == RG_TREASURE_GAME_SMALL_KEY || rg == RG_TREASURE_GAME_KEY_RING;
                    const bool isSilver = type == ITEMTYPE_SILVER;
                    const bool isSong = type == ITEMTYPE_SONG;
                    uint32_t wantMask = 0u;
                    if (songUnion && isSong) {
                        wantMask = RSBS_COMBO_GROUP_SONG_LOCATIONS;
                    } else if (chestActive && isChestKey) {
                        wantMask = kCscLocalGroup;
                    } else if (silverActive && isSilver) {
                        wantMask = kCscLocalGroup;
                    }
                    if (wantMask != 0u) {
                        expectedConfined++;
                    }
                    if (isChestKey && masks[idx] == kCscLocalGroup) {
                        chestKeyMasked++;
                    }
                    if (isSilver && masks[idx] == kCscLocalGroup) {
                        silverMasked++;
                    }
                    if (isSong && masks[idx] == RSBS_COMBO_GROUP_SONG_LOCATIONS) {
                        songMasked++;
                    }
                    if (masks[idx] != wantMask) {
                        wrongMasks++;
                    }
                }
                CSC_CHECK(confined == expectedConfined && wrongMasks == 0,
                          "A(cg=%u,sr=%u,sg=%u): PoolRowConfineMasks returned %d (want %d), wrong=%d",
                          (unsigned)chestMode, (unsigned)silverMode, (unsigned)songMode, confined, expectedConfined,
                          wrongMasks);
                CSC_CHECK(chestKeyMasked == (chestActive ? 2 : 0),
                          "A(cg=%u,sr=%u,sg=%u): %d chest-game key items confined, want %d", (unsigned)chestMode,
                          (unsigned)silverMode, (unsigned)songMode, chestKeyMasked, chestActive ? 2 : 0);
                CSC_CHECK(silverMasked == (silverActive ? 22 : 0),
                          "A(cg=%u,sr=%u,sg=%u): %d silver rupee items confined, want %d", (unsigned)chestMode,
                          (unsigned)silverMode, (unsigned)songMode, silverMasked, silverActive ? 22 : 0);
                CSC_CHECK(songMasked == (songUnion ? 13 : 0),
                          "A(cg=%u,sr=%u,sg=%u): %d OoT song items wide-confined, want %d", (unsigned)chestMode,
                          (unsigned)silverMode, (unsigned)songMode, songMasked, songUnion ? 13 : 0);
            }
        }
    }
}

void CscCheckClassifierAndForeignHosts() {
    const RandomizerGet chestKeyIds[2] = { RG_TREASURE_GAME_SMALL_KEY, RG_TREASURE_GAME_KEY_RING };
    for (RandomizerGet rg : chestKeyIds) {
        ComboItemClassRow row = {};
        const int ok = OoT_ComboLogic_ClassifyItem((uint16_t)rg, &row);
        CSC_CHECK(ok == 1 && row.fillClass == RSBS_FILL_CLASS_PROGRESSION && row.armedBy == 0u && row.sharedKind == 0u,
                  "B: chest-game key %d classified ok=%d class=%u armedBy=0x%X sharedKind=%u", (int)rg, ok,
                  (unsigned)row.fillClass, (unsigned)row.armedBy, (unsigned)row.sharedKind);
    }
    int silverItemsChecked = 0;
    for (int rg = (int)RG_SHADOW_SILVER_BLADES; rg <= (int)RG_GANONS_CASTLE_MQ_SILVER_SHADOW; ++rg) {
        ComboItemClassRow row = {};
        const int ok = OoT_ComboLogic_ClassifyItem((uint16_t)rg, &row);
        CSC_CHECK(ok == 1 && row.fillClass == RSBS_FILL_CLASS_PROGRESSION && row.armedBy == 0u && row.sharedKind == 0u,
                  "B: silver item %d classified ok=%d class=%u armedBy=0x%X sharedKind=%u", rg, ok,
                  (unsigned)row.fillClass, (unsigned)row.armedBy, (unsigned)row.sharedKind);
        silverItemsChecked++;
    }
    CSC_CHECK(silverItemsChecked == 22, "B: checked %d silver items, want 22", silverItemsChecked);

    int chestChecks = 0;
    int chestAccepted = 0;
    int silverChecks = 0;
    int silverAccepted = 0;
    for (int c = 1; c < (int)RC_MAX; ++c) {
        const RandomizerCheck rc = (RandomizerCheck)c;
        if (!CscIsRealCheck(rc)) {
            continue;
        }
        const RandomizerCheckType type = Rando::StaticData::GetLocation(rc)->GetRCType();
        if (type == RCTYPE_CHEST_GAME) {
            chestChecks++;
            chestAccepted += OoT_ComboLogic_TestHostAcceptsForeign((uint16_t)rc) != 0 ? 1 : 0;
        } else if (type == RCTYPE_SILVER) {
            silverChecks++;
            silverAccepted += OoT_ComboLogic_TestHostAcceptsForeign((uint16_t)rc) != 0 ? 1 : 0;
        }
    }
    CSC_CHECK(chestChecks == 11 && chestAccepted == 0, "B: %d of %d RCTYPE_CHEST_GAME checks accepted a foreign item",
              chestAccepted, chestChecks);
    CSC_CHECK(silverChecks == 154 && silverAccepted == 0, "B: %d of %d RCTYPE_SILVER checks accepted a foreign item",
              silverAccepted, silverChecks);
    CSC_CHECK(OoT_ComboLogic_TestHostAcceptsForeign((uint16_t)RC_MARKET_TREASURE_CHEST_GAME_REWARD) == 1,
              "B: RC_MARKET_TREASURE_CHEST_GAME_REWARD was rejected as a foreign host");
}

} // namespace

extern "C" int OoT_ChestSilverConfinementTest_Run(void) {
    printf("[TEST] oot-chest-silver-confinement: ROM-free hostGroups, PoolRowConfineMasks, classifier and foreign-host "
           "checks for Shuffle Chest Minigame and Shuffle Silver Rupees (#1530)\n");
    sCscFailures = 0;
    const int hadCtxBefore = OoT_ComboLogic_TestRandoContextExists();

    if (OoT_ComboLogic_TestEnsureLocationTable() != 0 || OoT_ComboLogic_TestEnsureItemTableTransient() != 0) {
        printf("[TEST] FAIL: oot-chest-silver-confinement could not initialize static tables\n");
        return 1;
    }

    // The engine itself, not the registry's entry: inside `--test all` the stub rows
    // of combo-logic have unregistered Ocarina of Time's engine by the time this row
    // runs (the row passed alone and failed there at the local gate).
    const ComboLogicEngine* oot = OoT_ComboLogic_TestEngine();
    CSC_CHECK(oot != nullptr && oot->hostGroups != nullptr && oot->wideGroups != nullptr,
              "OoT ComboLogicEngine is missing hostGroups or wideGroups");
    if (oot != nullptr && oot->hostGroups != nullptr && oot->wideGroups != nullptr) {
        CscScopedState scope;
        if (!scope.Acquire()) {
            CscFail("could not acquire scoped Rando::Context");
        } else {
            CscCheckEdgeGuards(oot);
            CscSweepOptionMatrix(*scope.ctx, oot);
            CscCheckClassifierAndForeignHosts();
        }
    }

    const int hasCtxAfter = OoT_ComboLogic_TestRandoContextExists();
    CSC_CHECK(hasCtxAfter == hadCtxBefore, "C: Rando::Context leaked across ROM-free test (before %d, after %d)",
              hadCtxBefore, hasCtxAfter);
    CSC_CHECK(Combo_SongHostsTestOverride() < 0, "C: song-host test override left set (%d)",
              Combo_SongHostsTestOverride());

    if (sCscFailures != 0) {
        printf("[TEST] FAIL (oot-chest-silver-confinement): %d failure(s)\n", sCscFailures);
        return 1;
    }
    printf("[TEST] PASS: oot-chest-silver-confinement\n");
    return 0;
}

#else

extern "C" int OoT_ChestSilverConfinementTest_Run(void) {
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
