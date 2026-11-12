/**
 * @file mm_warp_songs_mm_test.cpp
 * MM's half of the warp-songs-in-MM locks (#1169 lane XW5; the items are
 * 2s2h/Rando/WarpSongsMM.cpp, the rules RSBS_XITEM2_SONG_MINUET_MM .. in
 * src/common/foreign_items.h, the copies rule and the ownership byte
 * src/common/warp_songs_mm.c).
 *
 * THE HELPERS the redship rows in src/common/tests/test_warp_songs_mm.c drive MM's
 * REAL give, conversion, item table, starting-item list, pool, logic macro and
 * freeze through. They only name MM's ids and put the live save in a known state;
 * every decision is the production code's. They live here because only an MM TU
 * has MM's enums, `gSaveContext`, Rando::StaticData and Logic.h in scope.
 *
 * THE RANDO ROW (`mm-warp-songs-pooled-only-when-rule-on`,
 * MM_WarpSongsPooled_RunHeadless): real paired generations (OoT's Generate, then
 * MM's creation-time half through the real OnSaveInit chain, mm-paired-start's
 * shape) with the rules off, on through the hidden-bit test seam
 * (Combo_ComboItemRules2RevealForTest, XW2's; production never calls it) and with
 * the shared rules on too.
 *
 * THE RECORD ROW (`oot-warp-song-record`, MM_WarpSongRecordWorld_RunHeadless,
 * #1169 XW7): the same creations followed by OoT's side of the tail, under Shuffle
 * Warp Songs with Warp Song Hints on and off, and unshuffled: the creation's
 * warp-song record (gComboCtx.comboOoTWarps) is written there and only there, from
 * the world it created (the real Entrance_Init's table, OoT's own warp prompt's
 * areas), and a package carries it.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include "global.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/Logic/Logic.h"
#include "2s2h/Rando/StaticData/StaticData.h"
#include "2s2h/Rando/WarpSongsMM.h"
#include "2s2h/ShipUtils.h"

#include "combo_logic.h"         // src/common: the coordinator's placement table
#include "combo_settings_view.h" // src/common: the warp-song keys
#include "combo_single_bag.h"    // src/common: the creation's commit (Combo_SingleBag_Forget)
#include "combo_song_pool.h"     // src/common: OoT's Shuffle Songs key (#1542)
#include "context.h"             // src/common: gComboCtx, the frozen states
#include "crossing_store.h"      // src/common: the store a refused creation leaves empty (#1169 XW12)
#include "foreign_items.h"       // src/common: the second rules word and its test seam
#include "mm_warp_song_play.h"   // src/common: the creation's warp-song record (#1169 XW7)
#include "seed_package.h"        // src/common: Generate's completion and its statuses (#1169 XW12)
#include "warp_songs_mm.h"       // src/common: the ownership byte

extern "C" {
#include "variables.h"
#include "functions.h"
// 3drando/menu.cpp: the headless Generate button (freezes the pairing identity and
// the combo records, stops OoT's fill at its general pass).
int Rando_HeadlessSeedTest(const char* seedStr);
int OoT_ComboLogic_GeneralPassDeferred(void);
// mm_rando_gen_test.cpp: MM's creation-time half over that world.
int MM_Rando_HeadlessPairedHalf(void);
}

namespace {

constexpr RandoItemId kSongs[6] = { RI_SONG_MINUET,  RI_SONG_BOLERO,   RI_SONG_SERENADE,
                                    RI_SONG_REQUIEM, RI_SONG_NOCTURNE, RI_SONG_PRELUDE };

} // namespace

/** MM ids by index: 0..5 the six songs (Minuet .. Prelude), 6 RI_MASK_SPOOKY (the
 *  id before them), 7 RI_MAX_TRAP, 8 RI_MAX, 9 RI_WOODFALL_STRAY_FAIRY,
 *  10 RI_SONG_SOARING, 11 RI_JUNK, 12 RI_MASK_GERUDO; -1 past the table. */
extern "C" int MM_WarpSongsTest_Id(int which) {
    static const int kIds[] = { RI_SONG_MINUET,   RI_SONG_BOLERO,          RI_SONG_SERENADE, RI_SONG_REQUIEM,
                                RI_SONG_NOCTURNE, RI_SONG_PRELUDE,         RI_MASK_SPOOKY,   RI_MAX_TRAP,
                                RI_MAX,           RI_WOODFALL_STRAY_FAIRY, RI_SONG_SOARING,  RI_JUNK,
                                RI_MASK_GERUDO };
    return (which >= 0 && which < (int)(sizeof(kIds) / sizeof(kIds[0]))) ? kIds[which] : -1;
}

/** MM's live save as a cross-game MM session runs it: every slot empty, every
 *  RandoSaveInfo byte zero, human, the 0xFF session fileNum. */
extern "C" void MM_WarpSongsTest_ArmLive(void) {
    std::memset(&gSaveContext, 0, sizeof(SaveContext));
    std::memset(gSaveContext.save.saveInfo.inventory.items, ITEM_NONE,
                sizeof(gSaveContext.save.saveInfo.inventory.items));
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.fileNum = 0xFF;
    gSaveContext.flashSaveAvailable = true;
}

/** MM's REAL give (Rando::GiveItem) of item `ri`. */
extern "C" void MM_WarpSongsTest_Give(uint16_t ri) {
    Rando::GiveItem((RandoItemId)ri);
}

/** MM's REAL conversion (Rando::ConvertItem) of item `ri`, no check. */
extern "C" int MM_WarpSongsTest_Convert(uint16_t ri) {
    return (int)Rando::ConvertItem((RandoItemId)ri);
}

/** Does MM's REAL give of `ri` change any byte of MM's save? 1 when it does. The
 *  save is put back afterwards; the give's other effects are not. */
extern "C" int MM_WarpSongsTest_GiveTouchesSave(uint16_t ri) {
    auto before = std::make_unique<SaveContext>();
    std::memcpy(before.get(), &gSaveContext, sizeof(SaveContext));
    Rando::GiveItem((RandoItemId)ri);
    const int touched = std::memcmp(before.get(), &gSaveContext, sizeof(SaveContext)) != 0 ? 1 : 0;
    std::memcpy(&gSaveContext, before.get(), sizeof(SaveContext));
    return touched;
}

/**
 * One fact of `ri`'s item-table row, 1 or 0, by `fact`: 0 a RITYPE_MAJOR row (MM's
 * fill and the bag take it as progression), 1 no MM ItemId (ITEM_NONE: it reaches
 * no ItemId-indexed table), 2 no get-item row (GI_NONE), 3 no draw row (GID_NONE:
 * DrawItem's own case draws it), 4 its icon is MM's own song note
 * (MM_gItemIcons[ITEM_SONG_SOARING]). -1 with no row.
 */
extern "C" int MM_WarpSongsTest_RowFact(uint16_t ri, int fact) {
    const auto it = Rando::StaticData::Items.find((RandoItemId)ri);
    if (it == Rando::StaticData::Items.end()) {
        return -1;
    }
    switch (fact) {
        case 0:
            return it->second.randoItemType == RITYPE_MAJOR ? 1 : 0;
        case 1:
            return it->second.itemId == ITEM_NONE ? 1 : 0;
        case 2:
            return it->second.getItemId == GI_NONE ? 1 : 0;
        case 3:
            return it->second.drawId == GID_NONE ? 1 : 0;
        case 4: {
            const char* icon = Rando::StaticData::GetIconTexturePath((RandoItemId)ri);
            return (icon != nullptr && icon == (const char*)MM_gItemIcons[ITEM_SONG_SOARING]) ? 1 : 0;
        }
        default:
            return -1;
    }
}

/** The Items row's display name, or NULL. */
extern "C" const char* MM_WarpSongsTest_Name(uint16_t ri) {
    const auto it = Rando::StaticData::Items.find((RandoItemId)ri);
    return it == Rando::StaticData::Items.end() ? nullptr : it->second.name;
}

/** The name MM's texts and hints use (Rando::StaticData::GetItemName, with its
 *  article). */
extern "C" const char* MM_WarpSongsTest_TextName(uint16_t ri) {
    static std::string sName;
    sName = Rando::StaticData::GetItemName((RandoItemId)ri, true);
    return sName.c_str();
}

/** Is `ri`'s Items row junk (RITYPE_JUNK)? */
extern "C" int MM_WarpSongsTest_IsJunk(int ri) {
    const auto it = Rando::StaticData::Items.find((RandoItemId)ri);
    return (it != Rando::StaticData::Items.end() && it->second.randoItemType == RITYPE_JUNK) ? 1 : 0;
}

/**
 * MM's REAL logic macro CAN_PLAY_OOT_WARP_SONG (Logic.h) for `ri` on the live save
 * with the Ocarina of Time held when `ocarina`, and the ocarina buttons of
 * `buttons` obtained (bit 0 A, 1 C-up, 2 C-down, 3 C-left, 4 C-right; the
 * RANDO_INF flags Ocarina Button Shuffle sets), the rest not. Whatever rule the
 * caller froze and whatever the ownership byte holds. 1 or 0.
 */
extern "C" int MM_WarpSongsTest_CanPlay(uint16_t ri, int ocarina, uint32_t buttons) {
    INV_CONTENT(ITEM_OCARINA_OF_TIME) = ocarina != 0 ? ITEM_OCARINA_OF_TIME : ITEM_NONE;
    const RandoInf kButtons[5] = { RANDO_INF_OBTAINED_OCARINA_BUTTON_A, RANDO_INF_OBTAINED_OCARINA_BUTTON_C_UP,
                                   RANDO_INF_OBTAINED_OCARINA_BUTTON_C_DOWN, RANDO_INF_OBTAINED_OCARINA_BUTTON_C_LEFT,
                                   RANDO_INF_OBTAINED_OCARINA_BUTTON_C_RIGHT };
    // Written straight into the flag words (z_actor.c's Flags_SetRandoInf layout),
    // so no OnFlagSet hook runs for a probe.
    for (int b = 0; b < 5; b++) {
        const s32 flag = (s32)kButtons[b];
        if ((buttons >> b) & 1u) {
            gSaveContext.save.shipSaveInfo.rando.randoInf[flag >> 4] |= (1 << (flag & 0xF));
        } else {
            gSaveContext.save.shipSaveInfo.rando.randoInf[flag >> 4] &= ~(1 << (flag & 0xF));
        }
    }
    const RandoItemId song = (RandoItemId)ri;
    return CAN_PLAY_OOT_WARP_SONG(song) ? 1 : 0;
}

/**
 * MM's REAL pool (Rando::Logic::GeneratePools) over a fresh save info, every
 * option at its default and Glitchless logic (mm_tree_shuffle_test.cpp's shape),
 * under whatever rules the caller froze: how many copies of `ri` it holds. With
 * `startingItem` nonzero that item is the world's persisted starting list, as a
 * creation persists it. Seeded, so deterministic. -1 when `ri` has no row.
 */
extern "C" int MM_WarpSongsTest_PoolCopies(uint16_t ri, uint16_t startingItem) {
    if (Rando::StaticData::Items.find((RandoItemId)ri) == Rando::StaticData::Items.end()) {
        return -1;
    }
    auto info = std::make_unique<RandoSaveInfo>();
    std::memset(info.get(), 0, sizeof(RandoSaveInfo));
    for (auto& [randoOptionId, row] : Rando::StaticData::Options) {
        info->randoSaveOptions[randoOptionId] = row.defaultValue;
    }
    info->randoSaveOptions[RO_LOGIC] = RO_LOGIC_GLITCHLESS;
    if (startingItem != 0u) {
        std::vector<RandoItemId> start = { (RandoItemId)startingItem };
        Rando::SetStartingItemsInSave(*info, start);
    }
    std::vector<RandoCheckId> checkPool;
    std::vector<RandoItemId> itemPool;
    Ship_Random_Seed(0x1169A5u);
    Rando::Logic::GeneratePools(*info, checkPool, itemPool);
    int copies = 0;
    for (RandoItemId item : itemPool) {
        copies += item == (RandoItemId)ri ? 1 : 0;
    }
    return copies;
}

/**
 * The creation's starting grant for a world whose start list is `ri` alone: the
 * list persisted into the live save's rando info (Rando::SetStartingItemsInSave,
 * as OnFileCreate persists it), then read back and granted item by item exactly as
 * Rando::GrantStartingItems grants it (GiveItem(ConvertItem(item))). 1 when the
 * persisted list read back as `ri` alone.
 */
extern "C" int MM_WarpSongsTest_StartingGrant(uint16_t ri) {
    std::vector<RandoItemId> start = { (RandoItemId)ri };
    Rando::SetStartingItemsInSave(gSaveContext.save.shipSaveInfo.rando, start);
    const std::vector<RandoItemId> persisted = Rando::GetStartingItemsFromSave(gSaveContext.save.shipSaveInfo.rando);
    for (RandoItemId item : persisted) {
        Rando::GiveItem(Rando::ConvertItem(item));
    }
    return (persisted.size() == 1 && persisted[0] == (RandoItemId)ri) ? 1 : 0;
}

/** A departure's freeze of MM's live save and the arrival's restore of it
 *  (Context_FreezeState / Context_RestoreState). 1 when both took. */
extern "C" int MM_WarpSongsTest_FreezeAndRestore(void) {
    Context_FreezeState(GAME_MM, 0, &gSaveContext, sizeof(SaveContext));
    if (!Context_HasFrozenState(GAME_MM)) {
        return 0;
    }
    return Context_RestoreState(GAME_MM, &gSaveContext, sizeof(SaveContext)) != 0 ? 1 : 0;
}

/** #1169 XW12 (D7): the Song of Time's reset, headless: the REAL
 *  Sram_SaveEndOfCycle over a zeroed stand-in PlayState, as
 *  mm_cycle_keep_items_test.cpp's PlaySongOfTime runs it. 1 when it ran. */
extern "C" int MM_WarpSongsTest_EndOfCycle(void) {
    static PlayState sWarpSongsStandIn;
    memset(&sWarpSongsStandIn, 0, sizeof(sWarpSongsStandIn));
    PlayState* const previous = MM_gPlayState;
    MM_gPlayState = &sWarpSongsStandIn;
    Sram_SaveEndOfCycle(&sWarpSongsStandIn);
    MM_gPlayState = previous;
    return 1;
}

// ============================================================================
// mm-warp-songs-pooled-only-when-rule-on (rando tier)
// ============================================================================

namespace {

int sWsFailures = 0;

void WsCheck(bool ok, const char* what) {
    if (!ok) {
        sWsFailures++;
        printf("[TEST] FAIL: %s\n", what);
    }
}

const ComboSettingId kExtensionKeys[6] = { COMBO_SETTING_SONG_MINUET_MM,   COMBO_SETTING_SONG_BOLERO_MM,
                                           COMBO_SETTING_SONG_SERENADE_MM, COMBO_SETTING_SONG_REQUIEM_MM,
                                           COMBO_SETTING_SONG_NOCTURNE_MM, COMBO_SETTING_SONG_PRELUDE_MM };
const ComboSettingId kSharedKeys[6] = { COMBO_SETTING_SHARED_SONG_MINUET,   COMBO_SETTING_SHARED_SONG_BOLERO,
                                        COMBO_SETTING_SHARED_SONG_SERENADE, COMBO_SETTING_SHARED_SONG_REQUIEM,
                                        COMBO_SETTING_SHARED_SONG_NOCTURNE, COMBO_SETTING_SHARED_SONG_PRELUDE };
// The rule bits, spelled as XW2 allocated them (.redsave format).
constexpr uint32_t kAllExtensions = 0x0000007Eu; // the six MM songs, not songSoaringOot
constexpr uint32_t kSharedMinuet = 0x00000100u;  // sharedSongMinuet

void ClearKeys() {
    for (int i = 0; i < 6; i++) {
        CVarClear(Combo_ComboSettingKey(kExtensionKeys[i]));
        CVarClear(Combo_ComboSettingKey(kSharedKeys[i]));
    }
    CVarClear(RSBS_OOT_SHUFFLE_SONGS_CVAR);
    Combo_ComboItemRules2RevealForTest(0);
}

/**
 * THE CREATION'S COMMIT (Combo_SingleBag_Forget), which the real creation event
 * makes and MM_Rando_HeadlessPairedHalf does not: that bridge stops before OoT's
 * tail so a row can still read the coordinator's tables, which leaves the world
 * recorded in both engines. A leg's next generation then inherits that record, and
 * its All Locations pre-flight resets the tables: the OoT engine's ClearPlacements
 * writes the previous world's prior items (RG_NONE) over the keys, rewards and songs
 * the new OoT generation has just placed on the same hosts, and the pre-flight
 * refuses dozens of OoT locations "by construction" (the first CI run of this row:
 * the second leg, 56 OoT locations). Forgetting drops the record and restores
 * nothing, as K11FinishPairedOoTHalf and mm-creation-new-file do.
 */
void ForgetCreatedWorld() {
    Combo_SingleBag_Forget();
}

/** Every key a leg set is cleared, the seam closed and the last world forgotten
 *  however the row ends. */
struct WsRestore {
    ~WsRestore() {
        ClearKeys();
        ForgetCreatedWorld();
    }
};

int CountOf(const std::vector<RandoItemId>& items, RandoItemId id) {
    int n = 0;
    for (RandoItemId item : items) {
        n += item == id ? 1 : 0;
    }
    return n;
}

/** Copies of MM's `id` in the finished world: on an MM check, or crossed onto an
 *  OoT host by the single bag (the coordinator's table, still held). */
int PlacedInWorld(RandoItemId id) {
    int placed = 0;
    for (int rc = 0; rc < (int)RC_MAX; ++rc) {
        const RandoSaveCheck& check = RANDO_SAVE_CHECKS[rc];
        placed += (check.shuffled && !check.skipped && check.randoItemId == id) ? 1 : 0;
    }
    ComboLogicPlacement p;
    for (int i = 0; i < Combo_Logic_PlacementCount(GAME_OOT); ++i) {
        if (Combo_Logic_PlacementAt(GAME_OOT, i, &p) && p.item.originGame == (uint8_t)GAME_MM &&
            p.item.id == (uint16_t)id) {
            placed++;
        }
    }
    return placed;
}

/**
 * One leg: a paired creation with the six extension keys on when `extensions`, the
 * Shared Minuet of Forest too when `sharedMinuet`, through the MM seam (the six
 * stay hidden until IntMMWarpSongMinuet, #1169 XW12) or the full one. Checks the
 * frozen word, MM's pool over the half's own persisted input (one copy of each song
 * whose rule is on and whose shared song is off), the world (every pool copy placed)
 * and the ownership byte (nothing owned: the world starts with no warp song, and the
 * bag's simulated rounds gave none to the player). 1 when the creation ran.
 */
int RunLeg(const char* name, const char* seed, bool extensions, bool sharedMinuet) {
    // Before this leg's generation places anything: the previous leg's world is
    // committed, never rolled back over this one (ForgetCreatedWorld).
    ForgetCreatedWorld();
    ComboContext_Init();
    ClearKeys();
    if (extensions) {
        for (int i = 0; i < 6; i++) {
            CVarSetInteger(Combo_ComboSettingKey(kExtensionKeys[i]), 1);
        }
    }
    if (sharedMinuet) {
        CVarSetInteger(Combo_ComboSettingKey(kSharedKeys[0]), 1);
    }
    // THE TEST SEAM: the six extensions stay hidden until IntMMWarpSongMinuet
    // (#1169 XW12), so a leg with them reveals them alone (the shared songs still
    // refused); the Shared Minuet of Forest stays hidden until XW10, so only that
    // leg reads the mask empty.
    Combo_ComboItemRules2RevealForTest(sharedMinuet ? RSBS_XITEM2_REVEAL_ALL
                                       : extensions ? RSBS_XITEM2_REVEAL_MM_SONGS
                                                    : RSBS_XITEM2_REVEAL_NONE);
    // Songs Anywhere (#1542): under the shipped Song Locations the shared song pool
    // takes the six warp songs too, and 12 OoT + 16 MM songs (27 with the Shared
    // Minuet) for the 23 song checks of both games refuse by design, OoTMM's
    // "Not enough locations" (SongPoolOverCapacity (a) locks that refusal). This
    // row is about MM's pool, so it runs where no song is confined, as
    // IntSoaringToOwl does for XW4.
    CVarSetInteger(RSBS_OOT_SHUFFLE_SONGS_CVAR, (int32_t)RSBS_OOT_SONGS_ANYWHERE);
    int rc = Rando_HeadlessSeedTest(seed);
    const uint32_t frozen = Combo_ComboCrossItemFlags2();
    printf("[TEST] mm-warp-songs-pooled-only-when-rule-on: %s: generation %d, frozen second word %08X\n", name, rc,
           (unsigned)frozen);
    if (rc != 0 || !Combo_ForeignPairingActive() || !Combo_ComboSettingsFrozen() ||
        !OoT_ComboLogic_GeneralPassDeferred()) {
        printf("[TEST] FAIL: %s: the paired OoT generation did not leave a frozen paired world at its general pass\n",
               name);
        sWsFailures++;
        ClearKeys();
        return 0;
    }
    const uint32_t wantWord = (extensions ? kAllExtensions : 0u) | (sharedMinuet ? kSharedMinuet : 0u);
    WsCheck(frozen == wantWord, "the creation froze a second rules word other than the leg's keys");
    rc = MM_Rando_HeadlessPairedHalf();
    if (rc != 0) {
        printf("[TEST] FAIL: %s: MM's creation-time half did not produce a rando world (rc=%d)\n", name, rc);
        sWsFailures++;
        ClearKeys();
        return 0;
    }

    auto info = std::make_unique<RandoSaveInfo>();
    std::memcpy(info.get(), &gSaveContext.save.shipSaveInfo.rando, sizeof(RandoSaveInfo));
    std::vector<RandoCheckId> checkPool;
    std::vector<RandoItemId> itemPool;
    Rando::Logic::GeneratePools(*info, checkPool, itemPool);
    for (int i = 0; i < 6; i++) {
        const int copies = CountOf(itemPool, kSongs[i]);
        const int placed = PlacedInWorld(kSongs[i]);
        const int want = (!extensions || (i == 0 && sharedMinuet)) ? 0 : 1;
        printf("[TEST]   %s: %s pool %d, placed %d (expected %d)\n", name, Rando::StaticData::Items[kSongs[i]].name,
               copies, placed, want);
        WsCheck(copies == want, extensions ? (i == 0 && sharedMinuet
                                                  ? "under the Shared Minuet the separate MM copy was pooled; it must "
                                                    "be held back for lane XW10's family"
                                                  : "a song whose rule is on is not in MM's pool exactly once")
                                           : "a song whose rule is off reached MM's pool");
        WsCheck(placed == copies, "a pool copy of an MM warp song was not placed in the world");
    }
    const uint8_t owned = Combo_MMWarpSongsOwned();
    printf("[TEST]   %s: ownership byte after the creation %02X (expected 00)\n", name, (unsigned)owned);
    WsCheck(owned == 0u, "the creation left an MM warp song owned: a simulated pickup (the bag's MM engine, MM's own "
                         "pass) leaked out of its save snapshot, or the start granted one");
    ClearKeys();
    return 1;
}

} // namespace

extern "C" int MM_WarpSongsPooled_RunHeadless(void) {
    printf("[TEST] mm-warp-songs-pooled-only-when-rule-on: real paired creations pool no MM warp song with the rules "
           "off, exactly one of each with them on (through the test seam), none of a song whose shared rule is on, "
           "place every copy and leave none owned (#1169 XW5)\n");
    sWsFailures = 0;
    WsRestore restore;
    RunLeg("rules off", "RSBSXW5SONGS0", false, false);
    RunLeg("six rules on", "RSBSXW5SONGS1", true, false);
    RunLeg("six rules on, Shared Minuet of Forest", "RSBSXW5SONGS2", true, true);
    if (sWsFailures != 0) {
        printf("[TEST] FAIL: %d contract(s) broken\n", sWsFailures);
        return 1;
    }
    printf("[TEST] PASS: mm-warp-songs-pooled-only-when-rule-on\n");
    return 0;
}

// ============================================================================
// oot-warp-song-record (rando tier, #1169 XW7)
// ============================================================================

extern "C" {
// ForeignItemsSingleExe.cpp: OoT's side of the creation tail; the warp-song
// record's writer runs at its end (OoT_Creation_FreezeWarpSongRecord).
int OoT_Creation_FinishPairedHalf(int writeSpoiler);
// WarpSongRecordOoT.cpp: the created world's own answers, read beside the record.
int OoT_WarpSongRecord_TestTableDisagreements(void);
int OoT_WarpSongRecord_TestHintArea(int song);
// ComboLogicEngineOoT.cpp (#1169 XW9): a song's destination as a round reads it
// from the entrance graph.
uint16_t OoT_ComboLogic_TestTravelGraphDest(int song);
// SeedPackageSingleExe.cpp: a package's world-record copy and its apply.
int OoT_SeedPackage_WorldRecordRoundTripForTest(const ComboContext* live, ComboContext* outRecordBytes);
// #1169 XW12 (D3): the freeze's malformed-record seam (WarpSongRecordOoT.cpp), the
// staged generation (ForeignItemsSingleExe.cpp), Generate's completion
// (seed_package.h) and the crossing store's count (crossing_store.h).
void OoT_Creation_TestForceWarpRecordInvalid(int on);
void OoT_Creation_TestStageGeneration(void);
int OoT_Creation_TestClearPairedSpoiler(void);
}

namespace {

int sWsrFailures = 0;

void WsrCheck(bool ok, const char* leg, const char* what) {
    if (!ok) {
        sWsrFailures++;
        printf("[TEST] FAIL (oot-warp-song-record): %s: %s\n", leg, what);
    }
}

bool WsrSameRecord(const ComboOoTWarpRecord& a, const ComboOoTWarpRecord& b) {
    if (a.formatVersion != b.formatVersion || a.flags != b.flags) {
        return false;
    }
    for (size_t i = 0; i < sizeof(a.destLE); i++) {
        if (a.destLE[i] != b.destLE[i]) {
            return false;
        }
    }
    for (size_t i = 0; i < sizeof(a.area); i++) {
        if (a.area[i] != b.area[i]) {
            return false;
        }
    }
    return true;
}

// settings.cpp: RSK_SHUFFLE_WARP_SONGS and RSK_WARP_SONG_HINTS (default on).
const char* const kWsrShuffleKey = "gRandoSettings.ShuffleWarpSongs";
const char* const kWsrHintsKey = "gRandoSettings.WarpSongText";

void WsrClearKeys() {
    CVarClear(kWsrShuffleKey);
    CVarClear(kWsrHintsKey);
}

/** Both keys cleared, the D3 seam and the Minuet key put back, and the last world
 *  forgotten however the row ends. */
struct WsrRestore {
    ~WsrRestore() {
        WsrClearKeys();
        OoT_Creation_TestForceWarpRecordInvalid(0);
        CVarClear(Combo_ComboSettingKey(COMBO_SETTING_SONG_MINUET_MM));
        Combo_ComboItemRules2RevealForTest(0);
        ForgetCreatedWorld();
    }
};

/**
 * #1169 XW12 (D3): THE RECORD INVARIANT AT CREATION. With the freeze's seam forcing
 * a malformed record, three real paired creations, the rule set by its key under the
 * MM seam (the six MM extensions are hidden until IntMMWarpSongMinuet):
 *   (r1) Minuet of Forest (MM) on, OoT's side of the tail alone: it returns -3, the
 *        crossing store is empty and the record UNSET;
 *   (r2) the same through Generate's completion: refused
 *        (RSBS_SEEDPKG_ERR_GENERATION), the identity retracted and nothing frozen;
 *   (r3) the rule off, the same seam: the tail succeeds and the record stays UNSET
 *        (as before XW12, an UNSET record fails nothing when no MM rule is on).
 * Remove the D3 branch in OoT_Creation_FinishPairedHalf and (r1) returns a crossing
 * count, (r2) creates; drop its rule test and (r3) fails.
 */
void WsrRunRefusalLegs() {
    const char* const minuetKey = Combo_ComboSettingKey(COMBO_SETTING_SONG_MINUET_MM);
    // The MM seam: the six extensions stay hidden until IntMMWarpSongMinuet
    // (#1169 XW12), and Generate refuses a hidden rule before the record's tail
    // runs, so without it (r1) and (r2) could not tell D3 from the hidden gate.
    Combo_ComboItemRules2RevealForTest(RSBS_XITEM2_REVEAL_MM_SONGS);
    if (!Combo_ComboItemRule2Offered(RSBS_XITEM2_SONG_MINUET_MM)) {
        WsrCheck(false, "D3", "Minuet of Forest (MM) is not offered under the MM seam");
        Combo_ComboItemRules2RevealForTest(0);
        return;
    }
    for (int leg = 1; leg <= 3; leg++) {
        const char* const name = leg == 1   ? "D3 (r1) Minuet, tail"
                                 : leg == 2 ? "D3 (r2) Minuet, completion"
                                            : "D3 (r3) rules off, tail";
        ForgetCreatedWorld();
        ComboContext_Init();
        WsrClearKeys();
        CVarClear(minuetKey);
        if (leg != 3) {
            CVarSetInteger(minuetKey, 1);
        }
        OoT_Creation_TestForceWarpRecordInvalid(1);
        int rc = Rando_HeadlessSeedTest("RSBSXW12RECORDD3");
        if (rc != 0 || !Combo_ForeignPairingActive() || !OoT_ComboLogic_GeneralPassDeferred()) {
            WsrCheck(false, name, "the paired OoT generation did not leave a paired world at its general pass");
            OoT_Creation_TestForceWarpRecordInvalid(0);
            continue;
        }
        const bool ruleFrozen = Combo_ComboCrossItemRule2(RSBS_XITEM2_SONG_MINUET_MM);
        WsrCheck(ruleFrozen == (leg != 3), name, "the frozen word does not hold the Minuet rule as the key says");
        if (leg == 2) {
            OoT_Creation_TestClearPairedSpoiler();
            OoT_Creation_TestStageGeneration();
            const int completed = OoT_SeedPackage_TestCompleteWorld();
            const int crossingsLeft = Combo_Crossings_Count(GAME_OOT) + Combo_Crossings_Count(GAME_MM);
            printf("[TEST] oot-warp-song-record: %s: completion %d (want %d), pairing active %d, frozen %d, "
                   "crossings left %d, record %d\n",
                   name, completed, (int)RSBS_SEEDPKG_ERR_GENERATION, Combo_ForeignPairingActive() ? 1 : 0,
                   Combo_ComboSettingsFrozen() ? 1 : 0, crossingsLeft, Combo_OoTWarpRecord_Status());
            WsrCheck(completed == RSBS_SEEDPKG_ERR_GENERATION && !Combo_ForeignPairingActive() &&
                         !Combo_ComboSettingsFrozen() && crossingsLeft == 0 &&
                         Combo_OoTWarpRecord_Status() == COMBO_OOT_WARP_REC_UNSET,
                     name,
                     "GENERATE CREATED (or left identity, crossings or a record behind) a world whose Majora's Mask "
                     "warp song has no recorded destination");
        } else {
            rc = MM_Rando_HeadlessPairedHalf();
            if (rc != 0) {
                WsrCheck(false, name, "MM's creation-time half did not produce a rando world");
                OoT_Creation_TestForceWarpRecordInvalid(0);
                continue;
            }
            const int crossings = OoT_Creation_FinishPairedHalf(0);
            const int crossingsLeft = Combo_Crossings_Count(GAME_OOT) + Combo_Crossings_Count(GAME_MM);
            printf("[TEST] oot-warp-song-record: %s: tail %d, crossings left %d, record %d\n", name, crossings,
                   crossingsLeft, Combo_OoTWarpRecord_Status());
            if (leg == 1) {
                WsrCheck(crossings == -3 && crossingsLeft == 0 &&
                             Combo_OoTWarpRecord_Status() == COMBO_OOT_WARP_REC_UNSET,
                         name, "THE TAIL DID NOT FAIL (-3) with the Minuet on and no record, or left crossings behind");
            } else {
                WsrCheck(crossings >= 0 && Combo_OoTWarpRecord_Status() == COMBO_OOT_WARP_REC_UNSET, name,
                         "with every MM warp-song rule off an unrecordable world must still create, record UNSET");
            }
        }
        OoT_Creation_TestForceWarpRecordInvalid(0);
        CVarClear(minuetKey);
    }
    Combo_ComboItemRules2RevealForTest(0);
    ForgetCreatedWorld();
}

/**
 * One leg: a real paired creation (OoT's Generate, MM's creation-time half, OoT's
 * side of the tail, mm-warp-songs-pooled-only-when-rule-on's shape plus the tail)
 * under Shuffle Warp Songs @p shuffled and Warp Song Hints @p hints. The record is
 * unset until the tail, then PRESENT with the world's flags; every destination is
 * what the real Entrance_Init installs for the pad; every area is what OoT's own
 * warp prompt names (none without hints); and a package carries all 20 bytes back.
 */
void WsrRunLeg(const char* leg, const char* seed, bool shuffled, bool hints) {
    ForgetCreatedWorld();
    ComboContext_Init();
    WsrClearKeys();
    if (shuffled) {
        CVarSetInteger(kWsrShuffleKey, 1);
    }
    CVarSetInteger(kWsrHintsKey, hints ? 1 : 0);
    int rc = Rando_HeadlessSeedTest(seed);
    if (rc != 0 || !Combo_ForeignPairingActive() || !OoT_ComboLogic_GeneralPassDeferred()) {
        WsrCheck(false, leg, "the paired OoT generation did not leave a paired world at its general pass");
        return;
    }
    rc = MM_Rando_HeadlessPairedHalf();
    if (rc != 0) {
        WsrCheck(false, leg, "MM's creation-time half did not produce a rando world");
        return;
    }
    const int beforeTail = Combo_OoTWarpRecord_Status();
    const int crossings = OoT_Creation_FinishPairedHalf(0);
    if (crossings < 0) {
        WsrCheck(false, leg, "OoT's side of the creation tail failed");
        return;
    }
    const int status = Combo_OoTWarpRecord_Status();
    const unsigned flags = (unsigned)Combo_OoTWarpRecord_Flags();
    const unsigned wantFlags =
        (shuffled ? RSBS_OOT_WARP_REC_SHUFFLED : 0u) | (shuffled && hints ? RSBS_OOT_WARP_REC_HINTS : 0u);
    int moved = 0;
    int areaMismatches = 0;
    int named = 0;
    for (int i = 0; i < RSBS_MMWP_SONG_COUNT; i++) {
        moved += Combo_OoTWarpRecord_Dest(i) != Combo_MMWarpPlay_PadEntrance(i) ? 1 : 0;
        const int area = (int)Combo_OoTWarpRecord_Area(i);
        const int want = (shuffled && hints) ? OoT_WarpSongRecord_TestHintArea(i) : 0;
        areaMismatches += area != want ? 1 : 0;
        named += area != 0 ? 1 : 0;
    }
    // #1169 XW9: the destination the OoT engine reads from the entrance graph at a
    // round's beginQuery (before any override row exists) is the one this record
    // froze from the override rows, so the logic's MM-warp credit and XW7's play
    // judge the same entrance.
    int graphMismatches = 0;
    for (int i = 0; i < RSBS_MMWP_SONG_COUNT; i++) {
        graphMismatches += OoT_ComboLogic_TestTravelGraphDest(i) != Combo_OoTWarpRecord_Dest(i) ? 1 : 0;
    }
    printf("[TEST] oot-warp-song-record: %s: graph-derived destinations that differ from the record: %d\n", leg,
           graphMismatches);
    WsrCheck(graphMismatches == 0, leg,
             "the OoT engine's graph-derived warp destination is not the recorded one (XW9's credit would judge "
             "another entrance than XW7 lands on)");
    const int disagreements = OoT_WarpSongRecord_TestTableDisagreements();
    printf("[TEST] oot-warp-song-record: %s: crossings %d, status before the tail %d, after %d, flags %u (want %u), "
           "pads moved %d, table disagreements %d, areas named %d, area mismatches %d\n",
           leg, crossings, beforeTail, status, flags, wantFlags, moved, disagreements, named, areaMismatches);
    WsrCheck(beforeTail == COMBO_OOT_WARP_REC_UNSET, leg,
             "a record existed before OoT's side of the tail: something other than its writer wrote it");
    WsrCheck(status == COMBO_OOT_WARP_REC_PRESENT, leg, "the creation did not freeze a PRESENT warp-song record");
    WsrCheck(flags == wantFlags, leg, "the record's flags are not the world's Shuffle Warp Songs / Warp Song Hints");
    WsrCheck(disagreements == 0, leg,
             "a recorded destination is not what Entrance_Init installs for its pad (the writer ran before "
             "the overrides, or read another table)");
    WsrCheck(shuffled ? moved > 0 : moved == 0, leg,
             shuffled ? "no pad moved under Shuffle Warp Songs, so the table check proves nothing"
                      : "an unshuffled world recorded a destination other than a pad");
    WsrCheck(areaMismatches == 0, leg,
             "a recorded area is not what OoT's own warp prompt names (the writer ran before the warp-song texts)");
    WsrCheck((shuffled && hints) ? named > 0 : named == 0, leg,
             (shuffled && hints) ? "no area was recorded with Warp Song Hints on, so the area check proves nothing"
                                 : "an area was recorded with Warp Song Hints off");
    // The operator's ruling on A1, from this real world's frozen record: hints on
    // names the frozen area, shuffled with hints off says OoT's "a mysterious
    // place", unshuffled says OoTMM's fixed text.
    char prompt[128];
    const int form = Combo_MMWarpPlay_PromptText(0, prompt, sizeof(prompt));
    int wantForm = (int)COMBO_MMWP_PROMPT_FIXED;
    if (shuffled) {
        wantForm = hints ? (int)COMBO_MMWP_PROMPT_HINT : (int)COMBO_MMWP_PROMPT_MYSTERIOUS;
    }
    printf("[TEST] oot-warp-song-record: %s: Minuet's MM prompt form %d (want %d)\n", leg, form, wantForm);
    WsrCheck(form == wantForm, leg,
             "MM's warp prompt for this world is not the ruled form (the frozen area with hints on, OoT's \"a "
             "mysterious place\" shuffled with hints off, OoTMM's fixed text unshuffled)");

    // A package carries the record: the copy holds all 20 bytes, and the apply
    // puts them back into a context that lost them.
    const ComboOoTWarpRecord created = gComboCtx.comboOoTWarps;
    std::unique_ptr<ComboContext> live = std::make_unique<ComboContext>();
    std::memcpy(live.get(), &gComboCtx, sizeof(ComboContext));
    std::unique_ptr<ComboContext> carried = std::make_unique<ComboContext>();
    std::memset(carried.get(), 0, sizeof(ComboContext));
    Combo_OoTWarpRecord_Clear(&gComboCtx.comboOoTWarps);
    const int roundTrip = OoT_SeedPackage_WorldRecordRoundTripForTest(live.get(), carried.get());
    WsrCheck(roundTrip == 1 && WsrSameRecord(carried->comboOoTWarps, created), leg,
             "the package's world record does not carry the warp-song record");
    WsrCheck(WsrSameRecord(gComboCtx.comboOoTWarps, created), leg,
             "applying the package did not put the warp-song record back");
}

} // namespace

extern "C" int MM_WarpSongRecordWorld_RunHeadless(void) {
    printf("[TEST] oot-warp-song-record: a real paired creation freezes OoT's warp-song record at the end of OoT's "
           "side of the tail, from the world it created, and a package carries it (#1169 XW7)\n");
    sWsrFailures = 0;
    WsrRestore restore;
    WsrRunLeg("shuffled, hints on", "RSBSXW7RECORD1", true, true);
    WsrRunLeg("shuffled, hints off", "RSBSXW7RECORD2", true, false);
    WsrRunLeg("unshuffled", "RSBSXW7RECORD0", false, false);
    WsrRunRefusalLegs();
    if (sWsrFailures != 0) {
        printf("[TEST] FAIL: oot-warp-song-record: %d contract(s) broken\n", sWsrFailures);
        return 1;
    }
    printf("[TEST] PASS: oot-warp-song-record\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
