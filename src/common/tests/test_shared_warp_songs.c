/**
 * @file test_shared_warp_songs.c
 * @brief Locks for the seven SHARED warp songs (#1169 lane XW10; OoTMM
 *        sharedSongSoaring, sharedSongMinuet .. sharedSongPrelude): the second
 *        family word, its registry and pairs, the players' "held" question, the
 *        runtime merge through both games' real shims, and the one-copy worlds.
 *
 * The shared rules are offered while their bits are outside RSBS_XITEM2_HIDDEN
 * (lane XW14). The ROM-free rows
 * freeze the second word directly (Combo_FreezeComboSettingsEx2, no Generate);
 * the generating rows set the rules by their keys on the production mask, with
 * no test seam.
 *
 *   shared-warp-family-storage (redship) -- THE STORAGE AND THE REGISTRY.
 *   gComboCtx.sharedFamilyOwned2 is appended at 2716 (sizeof 2720, layout revision
 *   4); RSBS_XITEM2_FAMILY_MASK is 0x3F80; a second-word member registers only at
 *   a shared bit and never touches the first word's slot at the same bit; the
 *   arming reads the frozen word only; the harvest ORs armed bits and never
 *   clears; the live harvest and apply run the second word in a world that arms
 *   no first-word family; the far-pickup credit fires only for an armed family;
 *   the bag's pairs come word 1 first, a buffer too small refuses (-1) instead of
 *   truncating, and no production caller sizes its buffer RSBS_XITEM_BIT_COUNT.
 *
 *   warp-song-held (redship) -- Combo_WarpSongHeld's truth table over every song,
 *   game, extension, shared rule and holding, with OoTMM's has_song_X as the
 *   oracle, and the MM ownership byte's shift.
 *
 *   shared-warp-songs-merge (redship) -- THE RUNTIME through both games' REAL
 *   shims with every first-word rule off: each game registers its own copy of each
 *   song; a song learnt in either game is known in the other at arrival and after
 *   OoT's .redsave load seam; the extension alone carries nothing; MM's title
 *   screen harvests nothing; Epona's Song and the warp songs never cross-feed;
 *   the matchers' predicates answer from the family while MM's logic predicate
 *   stays own-copy; a far game's copy found here is credited at pickup.
 *
 *   shared-warp-songs-one-copy-{song-locations,dungeon-rewards,anywhere,off}
 *   (rando) -- ONE COPY per shared warp song over a real paired world per Shuffle
 *   Songs value, all fourteen rules on: one placement over both worlds, no orphan
 *   copy of either half, Link's Pocket (Advancement under Off and Anywhere) never a
 *   shared warp song, the goal proven, nothing owned after the creation. Under
 *   Song Locations, Dungeon Rewards and Anywhere that placement is the bag's
 *   SHARED_PAIR row (on a song check of either game under Song Locations). Under
 *   Off (#1169 XW15, the operator's ruling) each one stays on its own default
 *   check: no pair, every OoT song vanilla, MM's Song of Soaring on its unshuffled
 *   Southern Swamp check, seven fixed shared credits, and the exit round credits
 *   the other game (MM plays the Minuet into OoT, OoT the Soaring into MM) until
 *   that check is forbidden.
 *
 *   shared-warp-songs-edges (rando) -- the Song of Soaring (OoT) beside one shared
 *   six generates under Dungeon Rewards (12 for 12); under Off the songs family is
 *   armed for a lone Soaring (OoT) with or without a shared six, and (#1169 XW15)
 *   never for a shared warp song alone; a started shared Minuet is no bag row and
 *   nothing is placed for it.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * like its neighbours; every symbol under test is C-linkage. Every file-local
 * helper is prefixed Sws (the declaration-collision gate).
 */

#include "../combo_logic.h"
#include "../combo_settings_view.h"
#include "../combo_single_bag.h"
#include "../combo_song_pool.h"
#include "../context.h"
#include "../foreign_items.h"
#include "../save.h"
#include "../save_layout_pins.h"
#include "../shared_items.h" // RSBS_FILL_ARM_SONGS_ROAM
#include "../shared_resources.h"
#include "../test_runner.h"
#include "../warp_song_family.h"
#include "../warp_song_items.h" // Combo_SongSpotsLastRefusal
#include "../warp_songs_mm.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

#ifdef __cplusplus
extern "C" {
#endif
void OoT_SharedSwordTest_ArmLive(void);
void MM_SharedSwordTest_ArmLive(void);
void OoT_HarvestSharedResources(void);
void OoT_ApplySharedResources(void);
void MM_HarvestSharedResources(void);
void MM_ApplySharedResources(void);
void OoT_ApplySharedResourcesAtFileLoad(void);
void OoT_SharedSongsTest_SetHeld(int song, int held);
int OoT_SharedSongsTest_Held(int song);
void MM_SharedSongsTest_SetHeld(int song, int held);
int MM_SharedSongsTest_Held(int song);
// games/oot/soh/oot_shared_songs_test.cpp and games/mm/2s2h/mm_shared_songs_test.cpp
// (#1169 XW10): song 0 the Song of Soaring, 1 .. 6 the Minuet .. the Prelude.
void OoT_SharedWarpSongsTest_SetHeld(int song, int held);
int OoT_SharedWarpSongsTest_Held(int song);
int OoT_SharedWarpSongsTest_PoolId(int song);
int OoT_SharedWarpSongsTest_LinksPocket(void);
int OoT_SharedWarpSongsTest_UnsharedSongId(void);
int OoT_Fill_TestSongHeldForBag(uint16_t rg); // fill.cpp, RsbsSongHeldForBag
void MM_SharedWarpSongsTest_SetHeld(int song, int held);
int MM_SharedWarpSongsTest_Held(int song);
int MM_SharedWarpSongsTest_PoolId(int song);
void MM_SharedWarpSongsTest_SetTitleScreen(void);
// SongOfSoaringOoT.cpp / WarpSongsMM.cpp: the matchers' predicates and MM's logic's.
bool OoT_SoaringOoTOwned(void);
uint8_t MM_WarpSongMM_UsableMask(void);
bool MM_WarpSongMM_Usable(uint16_t riId);
// The generating rows (test_shared_songs.c's readers).
int Rando_HeadlessSeedTest(const char* seedStr);
int MM_Rando_HeadlessPairedHalf(void);
int OoT_Creation_FinishPairedHalf(int writeSpoiler);
int MM_ComboLogic_TestShuffledItems(uint16_t* outItems, uint16_t* outChecks, int cap);
int OoT_SharedSongsTest_WorldCopies(int rg);
int OoT_SharedSongsTest_OtherVanillaSongsAtHome(const int* sharedRgs, int sharedCount);
int OoT_SharedSongsTest_VanillaSongCount(void);
int MM_PlacementConfinement_WorldCopies(uint16_t id);
int OoT_ComboLogic_TestSongCheckIndex(uint16_t rc);
int MM_Rando_Foreign_TestSongCheckIndex(uint16_t randoCheckId);
// #1169 XW15: the fixed shared songs' hosts (ComboLogicEngineOoT.cpp,
// ComboLogicEngineSingleExe.cpp).
int OoT_ComboLogic_TestHostsHolding(uint16_t rg, uint16_t* out, int cap);
int MM_ComboLogic_TestFixedCheckItem(uint16_t rc);
int MM_ComboLogic_TestSwampSoaringCheck(void);
// games/mm/2s2h/mm_warp_songs_mm_test.cpp: MM's Items row name (the lookup key) and
// the name MM's texts and hints show (Rando::StaticData::GetItemName, with article).
const char* MM_WarpSongsTest_Name(uint16_t ri);
const char* MM_WarpSongsTest_TextName(uint16_t ri);
#ifdef __cplusplus
}
#endif

static int sSwsFailures;

#define SWS_CHECK(cond, ...)                          \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sSwsFailures++;                           \
        }                                             \
    } while (0)

namespace {

// The rule bits, spelled as XW2 allocated them (.redsave format), not read back
// from the header under test.
const uint32_t kSwsExtensions = 0x0000007Fu;
const uint32_t kSwsShared = 0x00003F80u;
const uint32_t kSwsAll = 0x00003FFFu;
const uint32_t kSwsSharedSoaring = 0x00000080u;
const uint32_t kSwsSharedMinuet = 0x00000100u;
const uint32_t kSwsSharedEponaWord1 = 0x00400000u; // the first word's Shared Epona's Song (#1066)
const char* const kSwsSongNames[7] = { "Song of Soaring",   "Minuet of Forest",  "Bolero of Fire",
                                       "Serenade of Water", "Requiem of Spirit", "Nocturne of Shadow",
                                       "Prelude of Light" };

uint32_t SwsSharedBitOf(int song) {
    return 1u << (unsigned)(song + 7);
}

/** Freeze a paired world with the shipped combo record, first-word rules
 *  @p rules1 and second word @p rules2 (no Generate: nothing is refused). */
void SwsFreeze(uint32_t rules1, uint32_t rules2) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x1169F00Du;
    gComboCtx.sharedRandoSettingsHash = 0xDEADBEEFu;
    gComboCtx.mmProfileDigest = 0x0BADF00Du;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    ComboItemRulesRecord r1;
    memset(&r1, 0, sizeof(r1));
    r1.crossItemFlags = rules1;
    Combo_FreezeComboSettingsEx2(&rec, &r1, rules2);
}

int SwsPopcount(uint32_t v) {
    int n = 0;
    for (; v != 0u; v &= v - 1u) {
        n++;
    }
    return n;
}

// ---- Fake members (the registry rows) ------------------------------------------
// A fake's id is 0x1000 * game + slot, so the id says which game and slot it is.

uint64_t sSwsFakeHeld[3];
int sSwsFakeGives[3];

bool SwsFakeHeld(uint16_t id) {
    const int game = (int)(id >> 12);
    const int slot = (int)(id & 0x0FFFu);
    return game >= 0 && game < 3 && slot < 64 && ((sSwsFakeHeld[game] >> slot) & 1u) != 0u;
}
void SwsFakeGive(uint16_t id) {
    const int game = (int)(id >> 12);
    const int slot = (int)(id & 0x0FFFu);
    if (game >= 0 && game < 3 && slot < 64) {
        sSwsFakeHeld[game] |= (uint64_t)1u << slot;
        sSwsFakeGives[game]++;
    }
}
uint16_t SwsFakeId(GameId game, int slot) {
    return (uint16_t)(0x1000 * (int)game + slot);
}
void SwsFakeReset() {
    memset(sSwsFakeHeld, 0, sizeof(sSwsFakeHeld));
    memset(sSwsFakeGives, 0, sizeof(sSwsFakeGives));
}

/** Every real member of both words, saved so a row that registers fakes puts the
 *  games' own back. */
struct SwsSavedMembers {
    ComboSharedFamilyMember m[3][64];
    bool had[3][64];
};

void SwsSaveMembers(SwsSavedMembers* s) {
    memset(s, 0, sizeof(*s));
    for (GameId game : { GAME_OOT, GAME_MM }) {
        for (int b = 0; b < 32; b++) {
            const uint32_t bit = 1u << b;
            s->had[game][b] = (bit & (uint32_t)RSBS_XFAMILY_SLOTS_ALL) != 0u &&
                              Combo_SharedFamilyMemberGet(game, bit, &s->m[game][b]);
            s->had[game][32 + b] = (bit & (uint32_t)RSBS_XITEM2_FAMILY_MASK) != 0u &&
                                   Combo_SharedFamily2MemberGet(game, bit, &s->m[game][32 + b]);
        }
    }
}

void SwsRestoreMembers(const SwsSavedMembers* s) {
    for (GameId game : { GAME_OOT, GAME_MM }) {
        for (int b = 0; b < 32; b++) {
            const uint32_t bit = 1u << b;
            if ((bit & (uint32_t)RSBS_XFAMILY_SLOTS_ALL) != 0u) {
                Combo_RegisterSharedFamilyMember(game, bit, s->had[game][b] ? &s->m[game][b] : NULL);
            }
            if ((bit & (uint32_t)RSBS_XITEM2_FAMILY_MASK) != 0u) {
                Combo_RegisterSharedFamily2Member(game, bit, s->had[game][32 + b] ? &s->m[game][32 + b] : NULL);
            }
        }
    }
}

/** Register a fake at every second-word family bit (and, with @p word1 too, at
 *  every first-word family slot) for both games. */
void SwsRegisterFakes(bool word1) {
    for (GameId game : { GAME_OOT, GAME_MM }) {
        for (int b = 0; b < 32; b++) {
            const uint32_t bit = 1u << b;
            if (word1 && (bit & (uint32_t)RSBS_XFAMILY_SLOTS_ALL) != 0u) {
                const ComboSharedFamilyMember m = { SwsFakeId(game, b), SwsFakeHeld, SwsFakeGive, NULL };
                Combo_RegisterSharedFamilyMember(game, bit, &m);
            }
            if ((bit & (uint32_t)RSBS_XITEM2_FAMILY_MASK) != 0u) {
                const ComboSharedFamilyMember m = { SwsFakeId(game, 32 + b), SwsFakeHeld, SwsFakeGive, NULL };
                Combo_RegisterSharedFamily2Member(game, bit, &m);
            }
        }
    }
}

// ---- Source text (the pair-buffer lock) ----------------------------------------

std::string SwsReadFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.good()) {
        return std::string();
    }
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

} // namespace

// ============================================================================
// shared-warp-family-storage
// ============================================================================

TestResult Test_SharedWarpFamilyStorage(void) {
    printf("[TEST] shared-warp-family-storage: the second family word is appended at 2716 (layout revision 4), its "
           "members register at the shared warp-song bits only, it is armed from the frozen word, harvested by OR, "
           "carried by the live shims without a first-word family, credited at a far pickup only when armed, and "
           "its pairs follow the first word's without truncation (#1169 XW10)\n");
    sSwsFailures = 0;
    SwsSavedMembers saved;
    SwsSaveMembers(&saved);

    // ---- (1) The layout and the mask ----------------------------------------------
    SWS_CHECK(offsetof(ComboContext, sharedFamilyOwned2) == 2716u && sizeof(ComboContext) == 2720u &&
                  RSBS_PIN_COMBO_FAMILY_OWNED2 == 2716u && RSBS_PIN_COMBO_CONTEXT_SIZEOF == 2720u,
              "(1) the second family word is at %u (sizeof %u); want 2716 and 2720, appended after comboOoTWarps",
              (unsigned)offsetof(ComboContext, sharedFamilyOwned2), (unsigned)sizeof(ComboContext));
    SWS_CHECK(RSBS_SAVE_LAYOUT_REVISION == 4u, "(1) the layout revision is %u, want 4 (one append)",
              (unsigned)RSBS_SAVE_LAYOUT_REVISION);
    SWS_CHECK(RSBS_XITEM2_FAMILY_MASK == kSwsShared && RSBS_XFAMILY_SLOT_COUNT == 64u,
              "(1) the second-word family mask is %08X (want 00003F80), the slot count %u (want 64)",
              (unsigned)RSBS_XITEM2_FAMILY_MASK, (unsigned)RSBS_XFAMILY_SLOT_COUNT);

    // ---- (2) Registration: the seven shared bits, nothing else ---------------------
    {
        const ComboSharedFamilyMember fake = { 0x0F0Fu, SwsFakeHeld, SwsFakeGive, NULL };
        int accepted = 0;
        for (int s = 0; s < 7; s++) {
            accepted += Combo_RegisterSharedFamily2Member(GAME_OOT, SwsSharedBitOf(s), &fake) ? 1 : 0;
        }
        SWS_CHECK(accepted == 7, "(2) %d of the seven shared warp-song bits took a member", accepted);
        int refused = 0;
        for (int s = 0; s < 7; s++) {
            refused += Combo_RegisterSharedFamily2Member(GAME_OOT, 1u << s, &fake) ? 0 : 1; // the extensions
        }
        refused += Combo_RegisterSharedFamily2Member(GAME_OOT, 0x00004000u, &fake) ? 0 : 1; // past the mask
        refused += Combo_RegisterSharedFamily2Member(GAME_OOT, kSwsSharedSoaring | kSwsSharedMinuet, &fake) ? 0 : 1;
        refused += Combo_RegisterSharedFamily2Member(GAME_OOT, 0u, &fake) ? 0 : 1;
        refused += Combo_RegisterSharedFamily2Member(GAME_NONE, kSwsSharedMinuet, &fake) ? 0 : 1;
        const ComboSharedFamilyMember noHeld = { 0x0F0Fu, NULL, SwsFakeGive, NULL };
        refused += Combo_RegisterSharedFamily2Member(GAME_OOT, kSwsSharedMinuet, &noHeld) ? 0 : 1;
        SWS_CHECK(refused == 12,
                  "(2) %d of 12 bad registrations were refused (extension bits, a bit past the mask, "
                  "two bits, zero, no game, no held())",
                  refused);
        SwsRestoreMembers(&saved);
    }

    // ---- (3) Word isolation: a slot of one word never moves the other's -------------
    {
        int bit = -1;
        for (int b = 7; b <= 13 && bit < 0; b++) {
            if (((1u << b) & (uint32_t)RSBS_XFAMILY_SLOTS_ALL) != 0u) {
                bit = b;
            }
        }
        SWS_CHECK(bit >= 0, "(3) no first-word family slot shares a bit with the second word's mask");
        if (bit >= 0) {
            const uint32_t b = 1u << bit;
            const ComboSharedFamilyMember w1 = { 0x0A01u, SwsFakeHeld, SwsFakeGive, NULL };
            const ComboSharedFamilyMember w2 = { 0x0A02u, SwsFakeHeld, SwsFakeGive, NULL };
            Combo_RegisterSharedFamilyMember(GAME_OOT, b, &w1);
            Combo_RegisterSharedFamily2Member(GAME_OOT, b, &w2);
            uint16_t id1 = 0;
            uint16_t id2 = 0;
            const bool got1 = Combo_SharedFamilyMemberId(GAME_OOT, b, &id1);
            const bool got2 = Combo_SharedFamily2MemberId(GAME_OOT, b, &id2);
            SWS_CHECK(got1 && got2 && id1 == 0x0A01u && id2 == 0x0A02u,
                      "(3) at bit %08X the first word reads %04X and the second %04X; want 0A01 and 0A02 (one slot "
                      "per word)",
                      (unsigned)b, (unsigned)id1, (unsigned)id2);
            Combo_RegisterSharedFamily2Member(GAME_OOT, b, NULL);
            SWS_CHECK(Combo_SharedFamilyMemberId(GAME_OOT, b, &id1) && id1 == 0x0A01u &&
                          !Combo_SharedFamily2MemberId(GAME_OOT, b, &id2),
                      "(3) unregistering the second word's member at %08X moved the first word's", (unsigned)b);
            Combo_RegisterSharedFamily2Member(GAME_OOT, b, &w2);
            Combo_RegisterSharedFamilyMember(GAME_OOT, b, NULL);
            SWS_CHECK(Combo_SharedFamily2MemberId(GAME_OOT, b, &id2) && id2 == 0x0A02u &&
                          !Combo_SharedFamilyMemberId(GAME_OOT, b, &id1),
                      "(3) unregistering the first word's member at %08X moved the second word's", (unsigned)b);
            SwsRestoreMembers(&saved);
        }
    }

    // ---- (4) Arming: the frozen second word only -------------------------------------
    {
        ComboContext_Init();
        Combo_ComboItemRules2Store(&gComboCtx.comboItemRules2, kSwsAll); // stored, not frozen
        SWS_CHECK(Combo_SharedFamilies2Armed() == 0u,
                  "(4) with nothing frozen the second word armed %08X (a word that was never frozen)",
                  (unsigned)Combo_SharedFamilies2Armed());
        SwsFreeze(0u, kSwsAll);
        SWS_CHECK(Combo_SharedFamilies2Armed() == kSwsShared, "(4) all fourteen rules armed %08X, want 00003F80",
                  (unsigned)Combo_SharedFamilies2Armed());
        SwsFreeze(0u, kSwsExtensions);
        SWS_CHECK(Combo_SharedFamilies2Armed() == 0u,
                  "(4) the seven extensions alone armed %08X: an extension is "
                  "never a family",
                  (unsigned)Combo_SharedFamilies2Armed());
    }

    // ---- (5) Harvest is an OR of armed bits; Owed ------------------------------------
    {
        SwsFreeze(0u, kSwsExtensions | kSwsSharedSoaring | kSwsSharedMinuet);
        Combo_SharedFamilies2Harvest(GAME_OOT, kSwsShared);
        SWS_CHECK(Combo_SharedFamilies2Owned() == (kSwsSharedSoaring | kSwsSharedMinuet),
                  "(5) harvesting every family bit owned %08X; want the two armed ones, 00000180",
                  (unsigned)Combo_SharedFamilies2Owned());
        Combo_SharedFamilies2Harvest(GAME_MM, 0u);
        SWS_CHECK(Combo_SharedFamilies2Owned() == (kSwsSharedSoaring | kSwsSharedMinuet),
                  "(5) a harvest holding nothing cleared the pool to %08X: the harvest must OR, never assign",
                  (unsigned)Combo_SharedFamilies2Owned());
        Combo_SharedFamilies2Harvest(GAME_NONE, kSwsShared);
        SWS_CHECK(Combo_SharedFamilies2Owned() == (kSwsSharedSoaring | kSwsSharedMinuet),
                  "(5) a harvest for no game changed the pool");
        SWS_CHECK(Combo_SharedFamilies2Owed(GAME_MM, kSwsSharedSoaring) == kSwsSharedMinuet &&
                      Combo_SharedFamilies2Owed(GAME_MM, kSwsSharedSoaring | kSwsSharedMinuet) == 0u &&
                      Combo_SharedFamilies2Owed(GAME_NONE, 0u) == 0u,
                  "(5) the owed set is not owned & armed & ~held");
        SWS_CHECK(Combo_SharedFamiliesOwned() == 0u, "(5) the second word's harvest wrote the first word (%08X)",
                  (unsigned)Combo_SharedFamiliesOwned());
    }

    // ---- (6) The live shims' calls with NO first-word family armed ---------------------
    {
        SwsRegisterFakes(false);
        SwsFakeReset();
        SwsFreeze(0u, kSwsAll);
        SWS_CHECK(Combo_SharedFamiliesArmed() == 0u, "(6) the leg's first word armed %08X; it must arm nothing",
                  (unsigned)Combo_SharedFamiliesArmed());
        SWS_CHECK(Combo_SharedFamiliesApplyLive(GAME_MM) == 0 && sSwsFakeGives[GAME_MM] == 0,
                  "(6) with nothing owned the apply gave %d copies", sSwsFakeGives[GAME_MM]);
        sSwsFakeHeld[GAME_OOT] = (uint64_t)1u << (32 + 8); // OoT holds its Minuet member only
        Combo_SharedFamiliesHarvestLive(GAME_OOT);
        SWS_CHECK(Combo_SharedFamilies2Owned() == kSwsSharedMinuet && Combo_SharedFamiliesOwned() == 0u,
                  "(6) the live harvest owned %08X (first word %08X); want the Minuet's 00000100 alone: the second "
                  "word must be harvested when the first word arms nothing",
                  (unsigned)Combo_SharedFamilies2Owned(), (unsigned)Combo_SharedFamiliesOwned());
        const int given = Combo_SharedFamiliesApplyLive(GAME_MM);
        SWS_CHECK(given == 1 && SwsFakeHeld(SwsFakeId(GAME_MM, 32 + 8)) && sSwsFakeGives[GAME_MM] == 1,
                  "(6) the live apply gave %d copies (MM's Minuet member held %d); want exactly MM's Minuet: the "
                  "second word must be applied when the first word owns nothing",
                  given, SwsFakeHeld(SwsFakeId(GAME_MM, 32 + 8)) ? 1 : 0);
        SWS_CHECK(Combo_SharedFamiliesApplyLive(GAME_MM) == 0 && sSwsFakeGives[GAME_MM] == 1,
                  "(6) a second apply gave MM's Minuet again (it already holds it)");
        SwsRestoreMembers(&saved);
    }

    // ---- (7) The far-pickup credit: armed families only -------------------------------
    {
        SwsRegisterFakes(false);
        SwsFakeReset();
        SwsFreeze(0u, kSwsAll);
        const bool credited = Combo_SharedFamilies2CreditFarPickup(GAME_OOT, (uint8_t)GAME_MM, SwsFakeId(GAME_MM, 40));
        SWS_CHECK(credited && Combo_SharedFamilies2Owned() == kSwsSharedMinuet &&
                      SwsFakeHeld(SwsFakeId(GAME_OOT, 40)) && sSwsFakeGives[GAME_OOT] == 1,
                  "(7) MM's Minuet found in OoT: credited %d, owned %08X, OoT given %d; want credited, 00000100 and "
                  "OoT's own Minuet at once",
                  credited ? 1 : 0, (unsigned)Combo_SharedFamilies2Owned(), sSwsFakeGives[GAME_OOT]);
        SWS_CHECK(!Combo_SharedFamilies2CreditFarPickup(GAME_OOT, (uint8_t)GAME_OOT, SwsFakeId(GAME_OOT, 40)) &&
                      !Combo_SharedFamilies2CreditFarPickup(GAME_OOT, (uint8_t)GAME_MM, 0x7777u) &&
                      !Combo_SharedFamilies2CreditFarPickup(GAME_NONE, (uint8_t)GAME_MM, SwsFakeId(GAME_MM, 40)),
                  "(7) the credit fired for the running game's own item, an unregistered id or no game");
        SwsFakeReset();
        SwsFreeze(0u, kSwsExtensions);
        SWS_CHECK(!Combo_SharedFamilies2CreditFarPickup(GAME_OOT, (uint8_t)GAME_MM, SwsFakeId(GAME_MM, 40)) &&
                      Combo_SharedFamilies2Owned() == 0u && sSwsFakeGives[GAME_OOT] == 0,
                  "(7) with the extension alone (shared rule off) MM's Minuet was credited instead of crossing "
                  "(owned %08X, OoT given %d)",
                  (unsigned)Combo_SharedFamilies2Owned(), sSwsFakeGives[GAME_OOT]);
        SwsRestoreMembers(&saved);
    }

    // ---- (8) The pairs: word 1 first, never truncated -----------------------------------
    {
        SwsRegisterFakes(true);
        const uint32_t rules1 = (uint32_t)RSBS_XITEM_FAMILY_MASK;
        const uint32_t slots1 = Combo_SharedFamilySlotsArmedBy(rules1);
        const int expected1 = SwsPopcount(slots1);
        const int expected = expected1 + 7;
        SWS_CHECK(expected > 32, "(8) only %d pairs can be armed; the leg cannot tell a 32-pair buffer from 64",
                  expected);
        SwsFreeze(rules1, kSwsAll);
        ComboLogicSharedPair pairs[RSBS_XFAMILY_SLOT_COUNT];
        memset(pairs, 0, sizeof(pairs));
        const int n = Combo_SingleBag_SharedPairs(pairs, (int)RSBS_XFAMILY_SLOT_COUNT);
        bool ordered = n == expected;
        int i = 0;
        for (int b = 0; ordered && b < 32; b++) {
            if ((slots1 & (1u << b)) != 0u) {
                ordered = pairs[i].ootId == SwsFakeId(GAME_OOT, b) && pairs[i].mmId == SwsFakeId(GAME_MM, b);
                i++;
            }
        }
        for (int b = 7; ordered && b <= 13; b++) {
            ordered = pairs[i].ootId == SwsFakeId(GAME_OOT, 32 + b) && pairs[i].mmId == SwsFakeId(GAME_MM, 32 + b);
            i++;
        }
        SWS_CHECK(ordered,
                  "(8) every family armed: %d pairs (want %d), not the first word's slots in order then the "
                  "second word's",
                  n, expected);
        // Not named `small`: the Windows headers define that word as a macro.
        ComboLogicSharedPair tooFew[32];
        memset(tooFew, 0xEE, sizeof(tooFew));
        const int refused = Combo_SingleBag_SharedPairs(tooFew, 32);
        SWS_CHECK(refused == -1 && tooFew[0].ootId == 0xEEEEu,
                  "(8) a 32-pair buffer for %d pairs answered %d (first row %04X); want -1 and nothing written, never "
                  "a silent truncation",
                  expected, refused, (unsigned)tooFew[0].ootId);
        SwsFreeze(rules1, 0u);
        ComboLogicSharedPair word1Only[RSBS_XFAMILY_SLOT_COUNT];
        const int n1 = Combo_SingleBag_SharedPairs(word1Only, (int)RSBS_XFAMILY_SLOT_COUNT);
        bool same = n1 == expected1;
        for (int k = 0; same && k < n1; k++) {
            same = word1Only[k].ootId == pairs[k].ootId && word1Only[k].mmId == pairs[k].mmId;
        }
        SWS_CHECK(same,
                  "(8) with the second word off the pairs moved (%d, want the first word's %d in the same "
                  "order): a world with no warp song pair would change its placement digest",
                  n1, expected1);
        SwsRestoreMembers(&saved);
    }

    // ---- (9) No production caller sizes its pair buffer RSBS_XITEM_BIT_COUNT ------------
    {
#ifdef RSBS_SOURCE_DIR
        const std::filesystem::path root(RSBS_SOURCE_DIR);
        const char* const dirs[3] = { "src/common", "games/oot/soh/Enhancements/randomizer", "games/mm/2s2h/Rando" };
        int calls = 0;
        int narrow = 0;
        std::error_code ec;
        for (const char* dir : dirs) {
            for (auto it = std::filesystem::recursive_directory_iterator(root / dir, ec);
                 !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
                const std::filesystem::path& path = it->path();
                const std::string ext = path.extension().string();
                const std::string generic = path.generic_string();
                if ((ext != ".c" && ext != ".cpp" && ext != ".h") ||
                    generic.find("/src/common/tests/") != std::string::npos) {
                    continue;
                }
                const std::string text = SwsReadFile(path);
                // The public call and the bag's own (the compose and the public body).
                for (const char* token : { "Combo_SingleBag_SharedPairs(", " SingleBagSharedPairs(" }) {
                    const std::string call(token);
                    for (size_t at = text.find(call); at != std::string::npos; at = text.find(call, at + 1)) {
                        if (at >= 4 && text.compare(at - 4, 4, "int ") == 0) {
                            continue; // a declaration or a definition
                        }
                        // The whole argument list, to its matching parenthesis.
                        size_t end = at + call.size();
                        int depth = 1;
                        while (end < text.size() && depth > 0) {
                            depth += text[end] == '(' ? 1 : (text[end] == ')' ? -1 : 0);
                            end++;
                        }
                        const std::string args = text.substr(at, end - at);
                        calls++;
                        if (args.find("RSBS_XITEM_BIT_COUNT") != std::string::npos) {
                            narrow++;
                            printf("[TEST]   %s passes RSBS_XITEM_BIT_COUNT\n", generic.c_str());
                        }
                    }
                }
            }
        }
        SWS_CHECK(calls >= 5 && narrow == 0,
                  "(9) %d production calls of Combo_SingleBag_SharedPairs found (want at least the five), %d sized "
                  "RSBS_XITEM_BIT_COUNT: every buffer must be RSBS_XFAMILY_SLOT_COUNT",
                  calls, narrow);
#else
        SWS_CHECK(false, "(9) RSBS_SOURCE_DIR is not defined; the source lock cannot read the checkout");
#endif
    }

    SwsRestoreMembers(&saved);
    ComboContext_Init();
    printf("[TEST] shared-warp-family-storage: %s\n", sSwsFailures == 0 ? "PASS" : "FAIL");
    return sSwsFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// warp-song-held
// ============================================================================

TestResult Test_WarpSongHeld(void) {
    printf("[TEST] warp-song-held: Combo_WarpSongHeld is OoTMM's has_song_X over every warp song, game, extension, "
           "shared rule and holding (#1169 XW10)\n");
    sSwsFailures = 0;

    // ---- (1) The tables -------------------------------------------------------------------
    SWS_CHECK(Combo_WarpSongHost(0) == (int)GAME_MM && Combo_WarpSongHost(1) == (int)GAME_OOT &&
                  Combo_WarpSongHost(6) == (int)GAME_OOT && Combo_WarpSongHost(7) == (int)GAME_NONE &&
                  Combo_WarpSongHost(-1) == (int)GAME_NONE,
              "(1) the host games are not MM for the Song of Soaring and OoT for the six");
    bool bits = true;
    for (int s = 0; s < 7; s++) {
        bits = bits && Combo_WarpSongExtensionBit(s) == (1u << s) && Combo_WarpSongSharedBit(s) == SwsSharedBitOf(s);
    }
    SWS_CHECK(bits && Combo_WarpSongExtensionBit(7) == 0u && Combo_WarpSongSharedBit(-1) == 0u,
              "(1) a song's rule bits are not its extension bit and that bit << 7");
    SWS_CHECK(Combo_WarpSongHeldMMBits(0x3Fu, true) == 0x7Fu && Combo_WarpSongHeldMMBits(0xFFu, false) == 0x7Eu &&
                  Combo_WarpSongHeldMMBits(0x01u, false) == 0x02u,
              "(1) MM's ownership byte is not shifted up by one (Minuet 0x01 there is song 1 here)");

    // ---- (2) The full table, OoTMM's has_song_X as the oracle --------------------------------
    int wrong = 0;
    for (int s = 0; s < 7; s++) {
        for (int g = 0; g < 2; g++) {
            const int game = g == 0 ? (int)GAME_OOT : (int)GAME_MM;
            const int host = s == 0 ? (int)GAME_MM : (int)GAME_OOT;
            for (int mask = 0; mask < 32; mask++) {
                const bool ext = (mask & 1) != 0;
                const bool shared = (mask & 2) != 0;
                const bool own = (mask & 4) != 0;
                const bool other = (mask & 8) != 0;
                const bool owned = (mask & 16) != 0;
                ComboWarpSongHoldings h;
                h.rules2 = (ext ? (1u << s) : 0u) | (shared ? SwsSharedBitOf(s) : 0u);
                h.owned2 = owned ? SwsSharedBitOf(s) : 0u;
                const uint8_t songBit = (uint8_t)(1u << s);
                h.heldOoT = (uint8_t)((game == (int)GAME_OOT ? own : other) ? songBit : 0u);
                h.heldMM = (uint8_t)((game == (int)GAME_MM ? own : other) ? songBit : 0u);
                // has_song_X = has(SONG_X) || has(SHARED_SONG_X): the game that the
                // extension adds to has no such song without it; a shared song (which
                // needs its extension) is held in both games once either holds it.
                bool want = false;
                if (game != host && !ext) {
                    want = false;
                } else if (own) {
                    want = true;
                } else {
                    want = shared && ext && (owned || other);
                }
                const bool got = Combo_WarpSongHeld(&h, game, s);
                if (got != want) {
                    wrong++;
                    if (wrong <= 8) {
                        printf("[TEST]   %s in %s, ext %d shared %d own %d other %d owned2 %d: got %d, want %d\n",
                               kSwsSongNames[s], g == 0 ? "OoT" : "MM", ext ? 1 : 0, shared ? 1 : 0, own ? 1 : 0,
                               other ? 1 : 0, owned ? 1 : 0, got ? 1 : 0, want ? 1 : 0);
                    }
                }
            }
        }
    }
    SWS_CHECK(wrong == 0, "(2) %d of the 448 cases disagree with OoTMM's has_song_X", wrong);

    // ---- (3) The cases each bug would hit -------------------------------------------------------
    {
        ComboWarpSongHoldings h;
        memset(&h, 0, sizeof(h));
        h.rules2 = 0x01u; // the Soaring extension alone
        h.owned2 = kSwsSharedSoaring;
        SWS_CHECK(!Combo_WarpSongHeld(&h, (int)GAME_OOT, 0),
                  "(3) the family word answered for a song whose shared rule is off");
        h.rules2 = kSwsSharedSoaring; // a shared bit without its extension (never frozen)
        h.heldMM = 0x01u;
        SWS_CHECK(!Combo_WarpSongHeld(&h, (int)GAME_OOT, 0), "(3) OoT holds a Song of Soaring with the extension off");
        h.rules2 = 0u;
        SWS_CHECK(Combo_WarpSongHeld(&h, (int)GAME_MM, 0), "(3) MM's own Song of Soaring is not held with rules off");
        // Only MM's Minuet owned (the ownership byte 0x01).
        h.rules2 = 0x02u | kSwsSharedMinuet;
        h.owned2 = 0u;
        h.heldOoT = 0u;
        h.heldMM = Combo_WarpSongHeldMMBits(0x01u, false);
        SWS_CHECK(Combo_WarpSongHeld(&h, (int)GAME_MM, 1) && !Combo_WarpSongHeld(&h, (int)GAME_MM, 0) &&
                      !Combo_WarpSongHeld(&h, (int)GAME_MM, 2),
                  "(3) MM's ownership byte 0x01 is not the Minuet alone (Soaring %d, Bolero %d)",
                  Combo_WarpSongHeld(&h, (int)GAME_MM, 0) ? 1 : 0, Combo_WarpSongHeld(&h, (int)GAME_MM, 2) ? 1 : 0);
        SWS_CHECK(Combo_WarpSongHeld(&h, (int)GAME_OOT, 1),
                  "(3) under the shared Minuet MM's live copy is not OoT's Minuet before the first harvest");
        SWS_CHECK(!Combo_WarpSongHeld(&h, (int)GAME_OOT, 7) && !Combo_WarpSongHeld(&h, (int)GAME_NONE, 1) &&
                      !Combo_WarpSongHeld(NULL, (int)GAME_OOT, 1),
                  "(3) a bad song, game or holdings answered true");
    }

    printf("[TEST] warp-song-held: %s\n", sSwsFailures == 0 ? "PASS" : "FAIL");
    return sSwsFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// shared-warp-songs-merge
// ============================================================================

namespace {
void SwsForgetAll() {
    OoT_SharedSwordTest_ArmLive();
    for (int s = 0; s < 7; s++) {
        OoT_SharedWarpSongsTest_SetHeld(s, 0);
    }
    OoT_SharedSongsTest_SetHeld(0, 0);
    MM_SharedSwordTest_ArmLive();
    for (int s = 0; s < 7; s++) {
        MM_SharedWarpSongsTest_SetHeld(s, 0);
    }
    MM_SharedSongsTest_SetHeld(0, 0);
}

// #1169 XW14: stub live probes for Combo_SharedFamilies2HeldNow (leg (9)).
int sSwsProbeAnswer[3];
int SwsProbeOoT(void) {
    return sSwsProbeAnswer[GAME_OOT];
}
int SwsProbeMM(void) {
    return sSwsProbeAnswer[GAME_MM];
}
void SwsSetProbes(int oot, int mm) {
    sSwsProbeAnswer[GAME_OOT] = oot;
    sSwsProbeAnswer[GAME_MM] = mm;
}

int SwsHeldCount(bool mm) {
    int n = 0;
    for (int s = 0; s < 7; s++) {
        n += (mm ? MM_SharedWarpSongsTest_Held(s) : OoT_SharedWarpSongsTest_Held(s)) == 1 ? 1 : 0;
    }
    return n;
}
} // namespace

TestResult Test_SharedWarpSongsMerge(void) {
    printf("[TEST] shared-warp-songs-merge: both games register their own copy of each warp song as the shared "
           "family's member, and through the real shims, with every first-word rule off, a song learnt in either "
           "game is known in the other at arrival and at OoT's load seam; the matchers answer from the family and a "
           "far copy is credited at pickup (#1169 XW10)\n");
    sSwsFailures = 0;

    // ---- (0) The registrations ------------------------------------------------------------
    for (int s = 0; s < 7; s++) {
        uint16_t ootId = 0;
        uint16_t mmId = 0;
        const bool ootReg = Combo_SharedFamily2MemberId(GAME_OOT, SwsSharedBitOf(s), &ootId);
        const bool mmReg = Combo_SharedFamily2MemberId(GAME_MM, SwsSharedBitOf(s), &mmId);
        SWS_CHECK(ootReg && (int)ootId == OoT_SharedWarpSongsTest_PoolId(s) && mmReg &&
                      (int)mmId == MM_SharedWarpSongsTest_PoolId(s),
                  "(0) the shared %s: OoT member %d (id %u, want %d), MM member %d (id %u, want %d)", kSwsSongNames[s],
                  ootReg ? 1 : 0, (unsigned)ootId, OoT_SharedWarpSongsTest_PoolId(s), mmReg ? 1 : 0, (unsigned)mmId,
                  MM_SharedWarpSongsTest_PoolId(s));
    }
    SwsFreeze(0u, kSwsAll);
    bool pooledOn = true;
    for (int s = 0; s < 7; s++) {
        pooledOn = pooledOn && Combo_SharedFamily2MemberPooled(GAME_OOT, SwsSharedBitOf(s)) &&
                   Combo_SharedFamily2MemberPooled(GAME_MM, SwsSharedBitOf(s));
    }
    SWS_CHECK(pooledOn, "(0) with every extension frozen on a member answered not pooled: pooled() must ask the "
                        "extension, not the copies (0 under the shared rule)");
    SwsFreeze(0u, 0u);
    SWS_CHECK(!Combo_SharedFamily2MemberPooled(GAME_OOT, kSwsSharedSoaring) &&
                  !Combo_SharedFamily2MemberPooled(GAME_MM, kSwsSharedMinuet),
              "(0) with the extensions off the added copies (Song of Soaring (OoT), Minuet of Forest (MM)) answered "
              "pooled");

    // ---- (1)-(3) Each song, both directions and the load seam ---------------------------------
    for (int s = 0; s < 7; s++) {
        SwsFreeze(0u, kSwsAll);
        SwsForgetAll();
        OoT_SharedSwordTest_ArmLive();
        OoT_SharedWarpSongsTest_SetHeld(s, 1);
        OoT_HarvestSharedResources();
        const uint32_t afterOoT = Combo_SharedFamilies2Owned();
        // OoT's give record belongs to OoT's last apply (an earlier song's leg):
        // MM's apply must leave it exactly as it was.
        const uint32_t ootGivenBefore = Combo_SharedFamilies2GivenAtLastApply(GAME_OOT);
        MM_SharedSwordTest_ArmLive();
        MM_ApplySharedResources();
        printf("[TEST] (1) %s OoT -> MM: pool %08X, MM holds %d of 7 (this one %d)\n", kSwsSongNames[s],
               (unsigned)afterOoT, SwsHeldCount(true), MM_SharedWarpSongsTest_Held(s));
        SWS_CHECK(afterOoT == SwsSharedBitOf(s) && Combo_SharedFamiliesOwned() == 0u,
                  "(1) OoT's harvest put %08X for the %s, want exactly its family and nothing in the first word",
                  (unsigned)afterOoT, kSwsSongNames[s]);
        SWS_CHECK(MM_SharedWarpSongsTest_Held(s) == 1 && SwsHeldCount(true) == 1,
                  "(1) MM's apply did not give MM the %s alone", kSwsSongNames[s]);
        // #1169 XW14: the apply's give record (IntMMWarpSongMinuetShared reads it).
        // Catches a record that is never written, written for a family not given, or
        // written to the other game's record.
        SWS_CHECK(Combo_SharedFamilies2GivenAtLastApply(GAME_MM) == SwsSharedBitOf(s) &&
                      Combo_SharedFamilies2GivenAtLastApply(GAME_OOT) == ootGivenBefore,
                  "(1) after MM's arrival apply gave the %s the give record reads MM %08X, OoT %08X; want MM %08X "
                  "and OoT's left at %08X",
                  kSwsSongNames[s], (unsigned)Combo_SharedFamilies2GivenAtLastApply(GAME_MM),
                  (unsigned)Combo_SharedFamilies2GivenAtLastApply(GAME_OOT), (unsigned)SwsSharedBitOf(s),
                  (unsigned)ootGivenBefore);
        // A second apply owes nothing: the record is cleared at the top of every
        // apply. Catches a record that only accumulates (a stale bit would let the
        // integration row pass on an earlier arrival's give). MM's save is NOT
        // re-armed here: arming zeroes it, which would take MM's own Song of
        // Soaring away again and make this apply owe it a second time.
        MM_ApplySharedResources();
        SWS_CHECK(Combo_SharedFamilies2GivenAtLastApply(GAME_MM) == 0u,
                  "(1) an MM apply that gave nothing left the give record at %08X (not cleared per call)",
                  (unsigned)Combo_SharedFamilies2GivenAtLastApply(GAME_MM));

        SwsFreeze(0u, kSwsAll);
        SwsForgetAll();
        MM_SharedSwordTest_ArmLive();
        MM_SharedWarpSongsTest_SetHeld(s, 1);
        MM_HarvestSharedResources();
        OoT_SharedSwordTest_ArmLive();
        OoT_ApplySharedResources();
        printf("[TEST] (2) %s MM -> OoT: pool %08X, OoT holds %d of 7 (this one %d)\n", kSwsSongNames[s],
               (unsigned)Combo_SharedFamilies2Owned(), SwsHeldCount(false), OoT_SharedWarpSongsTest_Held(s));
        SWS_CHECK(Combo_SharedFamilies2Owned() == SwsSharedBitOf(s) && OoT_SharedWarpSongsTest_Held(s) == 1 &&
                      SwsHeldCount(false) == 1,
                  "(2) MM's %s did not reach OoT alone through the shims", kSwsSongNames[s]);
        SWS_CHECK(Combo_SharedFamilies2GivenAtLastApply(GAME_OOT) == SwsSharedBitOf(s),
                  "(2) after OoT's apply gave the %s the give record reads %08X, want %08X", kSwsSongNames[s],
                  (unsigned)Combo_SharedFamilies2GivenAtLastApply(GAME_OOT), (unsigned)SwsSharedBitOf(s));

        // The .redsave load seam (#1163): OoT's half applied before its own save opens.
        OoT_SharedSwordTest_ArmLive();
        OoT_ApplySharedResourcesAtFileLoad();
        SWS_CHECK(OoT_SharedWarpSongsTest_Held(s) == 1, "(3) OoT's load seam did not give the %s the pool owns",
                  kSwsSongNames[s]);
    }

    // ---- (4) The extensions alone carry nothing ---------------------------------------------------
    SwsFreeze(0u, kSwsExtensions);
    SwsForgetAll();
    OoT_SharedSwordTest_ArmLive();
    for (int s = 0; s < 7; s++) {
        OoT_SharedWarpSongsTest_SetHeld(s, 1);
    }
    OoT_HarvestSharedResources();
    MM_SharedSwordTest_ArmLive();
    MM_ApplySharedResources();
    SWS_CHECK(Combo_SharedFamilies2Owned() == 0u && SwsHeldCount(true) == 0,
              "(4) with the shared rules off OoT's songs reached Termina (pool %08X, MM holds %d)",
              (unsigned)Combo_SharedFamilies2Owned(), SwsHeldCount(true));
    SWS_CHECK(Combo_SharedFamilies2GivenAtLastApply(GAME_MM) == 0u,
              "(4) with the shared rules off MM's apply left the give record at %08X",
              (unsigned)Combo_SharedFamilies2GivenAtLastApply(GAME_MM));

    // ---- (5) MM's title screen harvests nothing (the harvest's gameMode gate) -----------------------
    SwsFreeze(0u, kSwsAll);
    SwsForgetAll();
    MM_SharedSwordTest_ArmLive();
    MM_SharedWarpSongsTest_SetHeld(0, 1);
    MM_SharedWarpSongsTest_SetHeld(1, 1);
    MM_SharedWarpSongsTest_SetTitleScreen();
    MM_HarvestSharedResources();
    SWS_CHECK(Combo_SharedFamilies2Owned() == 0u,
              "(5) MM's title-screen bootstrap save was harvested into the family word (%08X)",
              (unsigned)Combo_SharedFamilies2Owned());

    // ---- (6) Epona's Song and the warp songs never cross-feed ----------------------------------------
    {
        SwsFreeze(kSwsSharedEponaWord1, kSwsAll);
        SwsForgetAll();
        OoT_SharedSwordTest_ArmLive();
        OoT_SharedSongsTest_SetHeld(0, 1); // Epona's Song only
        OoT_HarvestSharedResources();
        SWS_CHECK(Combo_SharedFamilies2Owned() == 0u && (Combo_SharedFamiliesOwned() & kSwsSharedEponaWord1) != 0u,
                  "(6) OoT holding Epona's Song alone owned second word %08X (first %08X)",
                  (unsigned)Combo_SharedFamilies2Owned(), (unsigned)Combo_SharedFamiliesOwned());
        SwsFreeze(kSwsSharedEponaWord1, kSwsAll);
        SwsForgetAll();
        OoT_SharedSwordTest_ArmLive();
        OoT_SharedWarpSongsTest_SetHeld(1, 1); // the Minuet only
        OoT_HarvestSharedResources();
        SWS_CHECK(Combo_SharedFamilies2Owned() == kSwsSharedMinuet && Combo_SharedFamiliesOwned() == 0u,
                  "(6) OoT holding the Minuet alone owned second word %08X and first %08X; want 00000100 and none",
                  (unsigned)Combo_SharedFamilies2Owned(), (unsigned)Combo_SharedFamiliesOwned());
        MM_SharedSwordTest_ArmLive();
        MM_ApplySharedResources();
        SWS_CHECK(MM_SharedSongsTest_Held(0) == 0 && MM_SharedWarpSongsTest_Held(1) == 1,
                  "(6) the Minuet's apply gave MM Epona's Song %d (want 0) and the Minuet %d (want 1)",
                  MM_SharedSongsTest_Held(0), MM_SharedWarpSongsTest_Held(1));
        SwsFreeze(kSwsSharedEponaWord1, kSwsAll);
        SwsForgetAll();
        MM_SharedSwordTest_ArmLive();
        MM_SharedSongsTest_SetHeld(0, 1); // MM's Epona's Song only
        MM_HarvestSharedResources();
        SWS_CHECK(Combo_SharedFamilies2Owned() == 0u, "(6) MM holding Epona's Song alone owned second word %08X",
                  (unsigned)Combo_SharedFamilies2Owned());
        SwsFreeze(kSwsSharedEponaWord1, kSwsAll);
        SwsForgetAll();
        MM_SharedSwordTest_ArmLive();
        MM_SharedWarpSongsTest_SetHeld(0, 1); // MM's Song of Soaring only
        MM_HarvestSharedResources();
        SWS_CHECK(Combo_SharedFamilies2Owned() == kSwsSharedSoaring &&
                      (Combo_SharedFamiliesOwned() & kSwsSharedEponaWord1) == 0u,
                  "(6) MM holding the Song of Soaring alone owned second word %08X and first %08X",
                  (unsigned)Combo_SharedFamilies2Owned(), (unsigned)Combo_SharedFamiliesOwned());
        OoT_SharedSwordTest_ArmLive();
        OoT_ApplySharedResources();
        SWS_CHECK(
            OoT_SharedSongsTest_Held(0) == 0 && OoT_SharedWarpSongsTest_Held(0) == 1,
            "(6) MM's Song of Soaring gave OoT Epona's Song %d (want 0) and the Song of Soaring (OoT) %d (want 1)",
            OoT_SharedSongsTest_Held(0), OoT_SharedWarpSongsTest_Held(0));
    }

    // ---- (7) The matchers' predicates answer from the family; MM's logic does not -------------------
    {
        SwsFreeze(0u, 0x01u | kSwsSharedSoaring);
        SwsForgetAll();
        OoT_SharedSwordTest_ArmLive();
        SWS_CHECK(!OoT_SoaringOoTOwned(), "(7) OoT plays the Song of Soaring with nothing owned");
        gComboCtx.sharedFamilyOwned2 = kSwsSharedSoaring;
        SWS_CHECK(OoT_SoaringOoTOwned(), "(7) under the shared Soaring with the family owned (no OoT flag yet) OoT's "
                                         "matcher predicate answered no: the matcher was not widened");
        SwsFreeze(0u, 0x01u);
        gComboCtx.sharedFamilyOwned2 = kSwsSharedSoaring;
        SWS_CHECK(!OoT_SoaringOoTOwned(), "(7) with the shared rule off the family word made OoT's Soaring playable");
        OoT_SharedWarpSongsTest_SetHeld(0, 1);
        SWS_CHECK(OoT_SoaringOoTOwned(), "(7) OoT's own Song of Soaring (OoT) is not playable under its extension");

        const uint16_t minuetMM = (uint16_t)MM_SharedWarpSongsTest_PoolId(1);
        SwsFreeze(0u, 0x02u | kSwsSharedMinuet);
        SwsForgetAll();
        MM_SharedSwordTest_ArmLive();
        SWS_CHECK(MM_WarpSongMM_UsableMask() == 0u, "(7) MM plays a warp song with nothing owned");
        gComboCtx.sharedFamilyOwned2 = kSwsSharedMinuet;
        const uint8_t mask = MM_WarpSongMM_UsableMask();
        SWS_CHECK(mask == 0x01u,
                  "(7) under the shared Minuet with the family owned MM's usable mask is %02X, want 01: "
                  "the matcher was not widened (or a song other than the Minuet answered)",
                  (unsigned)mask);
        SWS_CHECK(!MM_WarpSongMM_Usable(minuetMM),
                  "(7) MM's LOGIC predicate answered from the family word: it must stay own-copy only");
        SwsFreeze(0u, 0x02u);
        gComboCtx.sharedFamilyOwned2 = kSwsSharedMinuet;
        SWS_CHECK(MM_WarpSongMM_UsableMask() == 0u, "(7) with the shared rule off the family word made MM's Minuet "
                                                    "playable");
        MM_SharedWarpSongsTest_SetHeld(1, 1);
        SWS_CHECK(MM_WarpSongMM_UsableMask() == 0x01u && MM_WarpSongMM_Usable(minuetMM),
                  "(7) MM's own Minuet of Forest (MM) is not playable under its extension");
    }

    // ---- (8) A far copy found here is credited at pickup, no crossing -------------------------------
    {
        const uint16_t minuetMM = (uint16_t)MM_SharedWarpSongsTest_PoolId(1);
        const uint16_t soaringMM = (uint16_t)MM_SharedWarpSongsTest_PoolId(0);
        const uint16_t minuetOoT = (uint16_t)OoT_SharedWarpSongsTest_PoolId(1);
        SharedItem item;
        memset(&item, 0, sizeof(item));

        SwsFreeze(0u, kSwsAll);
        SwsForgetAll();
        OoT_SharedSwordTest_ArmLive();
        item.originGame = (uint8_t)GAME_MM;
        item.id = minuetMM;
        const int ootMinuet = OoT_CreditSharedPickup(&item);
        SWS_CHECK(ootMinuet == 1 && (Combo_SharedFamilies2Owned() & kSwsSharedMinuet) != 0u &&
                      OoT_SharedWarpSongsTest_Held(1) == 1,
                  "(8) MM's Minuet found in Hyrule: credited %d, pool %08X, OoT holds it %d; want credited at once "
                  "with OoT's own Minuet (the #1061 rule), never a round trip",
                  ootMinuet, (unsigned)Combo_SharedFamilies2Owned(), OoT_SharedWarpSongsTest_Held(1));
        item.id = soaringMM;
        SWS_CHECK(OoT_CreditSharedPickup(&item) == 1 && OoT_SharedWarpSongsTest_Held(0) == 1 &&
                      (Combo_SharedFamilies2Owned() & kSwsSharedSoaring) != 0u,
                  "(8) MM's Song of Soaring found in Hyrule was not credited with the Song of Soaring (OoT)");

        SwsFreeze(0u, kSwsAll);
        SwsForgetAll();
        MM_SharedSwordTest_ArmLive();
        item.originGame = (uint8_t)GAME_OOT;
        item.id = minuetOoT;
        // #1169 XW14: the credit gives MM its copy directly, never through the
        // apply, so the apply's give record is left as it was (leg (6)'s MM apply
        // left the Minuet there). Catches a credit that writes or clears it, which
        // would let IntMMWarpSongMinuetShared read a pickup as the arrival's apply.
        const uint32_t recordBefore = Combo_SharedFamilies2GivenAtLastApply(GAME_MM);
        SWS_CHECK(MM_CreditSharedPickup(&item) == 1 && MM_SharedWarpSongsTest_Held(1) == 1 &&
                      Combo_MMWarpSongsOwned() == 0x01u,
                  "(8) OoT's Minuet found in Termina was not credited with MM's own Minuet (ownership byte %02X)",
                  (unsigned)Combo_MMWarpSongsOwned());
        SWS_CHECK(Combo_SharedFamilies2GivenAtLastApply(GAME_MM) == recordBefore && recordBefore == kSwsSharedMinuet,
                  "(8) the far-pickup credit moved the apply's give record (%08X before, %08X after; want leg (6)'s "
                  "00000100 kept)",
                  (unsigned)recordBefore, (unsigned)Combo_SharedFamilies2GivenAtLastApply(GAME_MM));

        SwsFreeze(0u, kSwsExtensions);
        SwsForgetAll();
        OoT_SharedSwordTest_ArmLive();
        item.originGame = (uint8_t)GAME_MM;
        item.id = minuetMM;
        SWS_CHECK(OoT_CreditSharedPickup(&item) == 0 && Combo_SharedFamilies2Owned() == 0u &&
                      OoT_SharedWarpSongsTest_Held(1) == 0,
                  "(8) with the extension alone MM's Minuet found in Hyrule was credited instead of crossing (OoT "
                  "holds its own Minuet %d)",
                  OoT_SharedWarpSongsTest_Held(1));

        SwsFreeze(0u, kSwsAll);
        SwsForgetAll();
        MM_SharedSwordTest_ArmLive();
        MM_SharedWarpSongsTest_SetTitleScreen();
        item.originGame = (uint8_t)GAME_OOT;
        item.id = minuetOoT;
        SWS_CHECK(MM_CreditSharedPickup(&item) == 0 && Combo_SharedFamilies2Owned() == 0u,
                  "(8) MM's title-screen bootstrap save took a pickup credit");
    }

    // ---- (9) #1169 XW14: held here, now (Combo_SharedFamilies2HeldNow) -------------------------
    // Fakes at every second-word slot and stub probes, so each leg sets exactly
    // which game is "running live". The games' own members and probes are put back.
    {
        SwsSavedMembers saved;
        SwsSaveMembers(&saved);
        const ComboSharedLiveProbeFn ootProbe = Combo_SharedLiveProbeFor(GAME_OOT);
        const ComboSharedLiveProbeFn mmProbe = Combo_SharedLiveProbeFor(GAME_MM);
        SWS_CHECK(ootProbe != NULL && mmProbe != NULL,
                  "(9) a game registers no live probe (OoT %d, MM %d): every reader would answer from the family word "
                  "alone and miss the running game's own copy",
                  ootProbe != NULL ? 1 : 0, mmProbe != NULL ? 1 : 0);
        SwsRegisterFakes(false);
        Combo_RegisterSharedLiveProbe(GAME_OOT, SwsProbeOoT);
        Combo_RegisterSharedLiveProbe(GAME_MM, SwsProbeMM);
        const uint64_t ootMinuet = (uint64_t)1u << (32 + 8);
        const uint64_t mmMinuet = (uint64_t)1u << (32 + 8);

        // (a) The rule off, or the extension alone: 0 whatever is owned or held.
        // Catches a helper that skips the arming gate.
        SwsFakeReset();
        sSwsFakeHeld[GAME_OOT] = ootMinuet;
        sSwsFakeHeld[GAME_MM] = mmMinuet;
        SwsSetProbes(1, 1);
        SwsFreeze(0u, 0x02u);
        gComboCtx.sharedFamilyOwned2 = kSwsShared;
        SWS_CHECK(Combo_SharedFamilies2HeldNow() == 0u,
                  "(9a) with the Minuet extension alone the helper answered %08X: a family must be armed",
                  (unsigned)Combo_SharedFamilies2HeldNow());
        SwsFreeze(0u, 0u);
        gComboCtx.sharedFamilyOwned2 = kSwsShared;
        SWS_CHECK(Combo_SharedFamilies2HeldNow() == 0u, "(9a) with every rule off the helper answered %08X",
                  (unsigned)Combo_SharedFamilies2HeldNow());

        // (b) Armed, the family word only, no game live: the word. Catches a helper
        // that reads only the live copies.
        SwsFakeReset();
        SwsSetProbes(0, 0);
        SwsFreeze(0u, 0x02u | kSwsSharedMinuet);
        gComboCtx.sharedFamilyOwned2 = kSwsSharedMinuet;
        SWS_CHECK(Combo_SharedFamilies2HeldNow() == kSwsSharedMinuet,
                  "(9b) armed with the family word owned the helper answered %08X, want 00000100",
                  (unsigned)Combo_SharedFamilies2HeldNow());
        Combo_RegisterSharedLiveProbe(GAME_OOT, NULL);
        Combo_RegisterSharedLiveProbe(GAME_MM, NULL);
        SWS_CHECK(Combo_SharedFamilies2HeldNow() == kSwsSharedMinuet,
                  "(9b) with no probe registered the helper answered %08X, want the family word 00000100",
                  (unsigned)Combo_SharedFamilies2HeldNow());
        Combo_RegisterSharedLiveProbe(GAME_OOT, SwsProbeOoT);
        Combo_RegisterSharedLiveProbe(GAME_MM, SwsProbeMM);

        // (c) Armed, the word 0, OoT live and OoT's member holding it: the bit,
        // before any harvest. Catches a helper that reads the word alone.
        SwsFakeReset();
        sSwsFakeHeld[GAME_OOT] = ootMinuet;
        SwsSetProbes(1, 0);
        SwsFreeze(0u, 0x02u | kSwsSharedMinuet);
        SWS_CHECK(Combo_SharedFamilies2HeldNow() == kSwsSharedMinuet && Combo_SharedFamilies2Owned() == 0u,
                  "(9c) OoT live holding its Minuet: the helper answered %08X (want 00000100) and the word %08X "
                  "(want 0: the helper must not harvest)",
                  (unsigned)Combo_SharedFamilies2HeldNow(), (unsigned)Combo_SharedFamilies2Owned());

        // (d) CRITIC [1]'s CASE: MM is the running game at its title screen (MM's
        // probe 0), OoT is not running (OoT's probe 0), MM's member holds the song
        // and the word is 0: 0. An OoT row drawn from OoT's shadow while MM runs
        // answers from the word only. Catches a helper that asks a member without
        // its own game's probe (MM's bootstrap save would read as held).
        SwsFakeReset();
        sSwsFakeHeld[GAME_MM] = mmMinuet;
        SwsSetProbes(0, 0);
        SwsFreeze(0u, 0x02u | kSwsSharedMinuet);
        SWS_CHECK(Combo_SharedFamilies2HeldNow() == 0u,
                  "(9d) MM's member holds the Minuet but MM is not a live file and OoT is not running: the helper "
                  "answered %08X, want 0",
                  (unsigned)Combo_SharedFamilies2HeldNow());
        // The other probe live does not open MM's member either.
        SwsSetProbes(1, 0);
        SWS_CHECK(Combo_SharedFamilies2HeldNow() == 0u,
                  "(9d) OoT live (its member not holding) made MM's held member count: %08X",
                  (unsigned)Combo_SharedFamilies2HeldNow());
        // (e) As (d) with the word set: the bit, from the word.
        SwsSetProbes(0, 0);
        gComboCtx.sharedFamilyOwned2 = kSwsSharedMinuet;
        SWS_CHECK(Combo_SharedFamilies2HeldNow() == kSwsSharedMinuet,
                  "(9e) as (9d) with the family word owned the helper answered %08X, want 00000100",
                  (unsigned)Combo_SharedFamilies2HeldNow());
        // MM live and holding: the bit (the probe's own game is asked).
        gComboCtx.sharedFamilyOwned2 = 0u;
        SwsSetProbes(0, 1);
        SWS_CHECK(Combo_SharedFamilies2HeldNow() == kSwsSharedMinuet,
                  "(9e) MM live holding its Minuet: the helper answered %08X, want 00000100",
                  (unsigned)Combo_SharedFamilies2HeldNow());
        // Only an armed family's member is asked: the Bolero is held but not armed.
        sSwsFakeHeld[GAME_MM] = mmMinuet | ((uint64_t)1u << (32 + 9));
        SWS_CHECK(Combo_SharedFamilies2HeldNow() == kSwsSharedMinuet,
                  "(9e) an unarmed family (the Bolero) was answered held: %08X",
                  (unsigned)Combo_SharedFamilies2HeldNow());

        Combo_RegisterSharedLiveProbe(GAME_OOT, ootProbe);
        Combo_RegisterSharedLiveProbe(GAME_MM, mmProbe);
        SwsFakeReset();
        SwsRestoreMembers(&saved);
    }

    // ---- (10) #1169 XW14: a shared song is named without its game -------------------------------
    // Over both games' REAL registered members. Each leg would fail if the helper
    // named a song whose shared rule is off (the extension's own "(MM)" / "(OoT)"
    // name must stay), named the wrong song, or if MM's text name ignored it; and
    // the Items row's name, a lookup key, must never change. Only the ADDED copy
    // (the tagged one) is renamed: OoT's Song of Soaring (OoT) and MM's six "(MM)"
    // songs. A game's own song gets NULL, so OoT's six keep their localized
    // names: catches a helper that hands OoT's Minuet the English text.
    {
        SwsFreeze(0u, kSwsAll);
        int wrong = 0;
        for (int s = 0; s < 7; s++) {
            const char* oot =
                Combo_SharedFamily2DisplayName((uint8_t)GAME_OOT, (uint16_t)OoT_SharedWarpSongsTest_PoolId(s));
            const char* mm =
                Combo_SharedFamily2DisplayName((uint8_t)GAME_MM, (uint16_t)MM_SharedWarpSongsTest_PoolId(s));
            const char* added = s == 0 ? oot : mm; // the tagged copy: OoT's for Soaring, MM's for the six
            const char* own = s == 0 ? mm : oot;   // the game's own song, already untagged
            if (added == NULL || strcmp(added, kSwsSongNames[s]) != 0 || own != NULL) {
                wrong++;
                printf("[TEST]   (10) the shared %s is named '%s' in OoT and '%s' in MM\n", kSwsSongNames[s],
                       oot != NULL ? oot : "(null)", mm != NULL ? mm : "(null)");
            }
        }
        SWS_CHECK(wrong == 0,
                  "(10) %d of the seven shared songs: the added copy is not named by the plain name, or the game's "
                  "own copy is renamed",
                  wrong);
        const uint16_t minuetMM = (uint16_t)MM_SharedWarpSongsTest_PoolId(1);
        const char* text = MM_WarpSongsTest_TextName(minuetMM);
        const std::string sharedText = text != NULL ? text : "";
        SWS_CHECK(
            sharedText.find("Minuet of Forest") != std::string::npos && sharedText.find("(MM)") == std::string::npos,
            "(10) under the shared Minuet MM's text name is '%s'; want the song without '(MM)'", sharedText.c_str());
        const char* key = MM_WarpSongsTest_Name(minuetMM);
        SWS_CHECK(key != NULL && strcmp(key, "Minuet of Forest (MM)") == 0,
                  "(10) MM's Items row name moved to '%s': it is a lookup key and must stay 'Minuet of Forest (MM)'",
                  key != NULL ? key : "(null)");
        SWS_CHECK(Combo_SharedFamily2DisplayName((uint8_t)GAME_MM, 0x7777u) == NULL &&
                      Combo_SharedFamily2DisplayName((uint8_t)GAME_NONE, minuetMM) == NULL,
                  "(10) an id that is no member, or no game, got a shared name");

        SwsFreeze(0u, kSwsExtensions);
        int named = 0;
        for (int s = 0; s < 7; s++) {
            named +=
                Combo_SharedFamily2DisplayName((uint8_t)GAME_OOT, (uint16_t)OoT_SharedWarpSongsTest_PoolId(s)) != NULL
                    ? 1
                    : 0;
            named +=
                Combo_SharedFamily2DisplayName((uint8_t)GAME_MM, (uint16_t)MM_SharedWarpSongsTest_PoolId(s)) != NULL
                    ? 1
                    : 0;
        }
        const char* extText = MM_WarpSongsTest_TextName(minuetMM);
        const std::string extensionText = extText != NULL ? extText : "";
        SWS_CHECK(named == 0 && extensionText.find("Minuet of Forest (MM)") != std::string::npos,
                  "(10) with the shared rules off %d copies got a shared name, or MM's text name '%s' lost its '(MM)'",
                  named, extensionText.c_str());

        SwsFreeze(0u, 0x02u | 0x04u | kSwsSharedMinuet);
        SWS_CHECK(
            Combo_SharedFamily2DisplayName((uint8_t)GAME_MM, (uint16_t)MM_SharedWarpSongsTest_PoolId(1)) != NULL &&
                Combo_SharedFamily2DisplayName((uint8_t)GAME_MM, (uint16_t)MM_SharedWarpSongsTest_PoolId(2)) == NULL,
            "(10) the shared Minuet alone named the Bolero (MM) too, or not the Minuet");
    }

    SwsFreeze(0u, 0u);
    SwsForgetAll();
    ComboContext_Init();
    printf("[TEST] shared-warp-songs-merge: %s\n", sSwsFailures == 0 ? "PASS" : "FAIL");
    return sSwsFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// shared-warp-songs-one-copy-* and shared-warp-songs-edges (rando)
// ============================================================================

namespace {
const char* const kSwsShuffleSongsKey = "gRandoSettings.ShuffleSongs";
const char* const kSwsLinksPocketKey = "gRandoSettings.LinksPocket";
const char* const kSwsStartMinuetKey = "gRandoSettings.StartingMinuetOfForest";
const int kSwsLinksPocketAdvancement = 1;
const char* const kSwsShuffleNames[4] = { "Off", "Song Locations", "Dungeon Rewards", "Anywhere" };

const ComboSettingId kSwsExtensionKeys[7] = { COMBO_SETTING_SONG_SOARING_OOT, COMBO_SETTING_SONG_MINUET_MM,
                                              COMBO_SETTING_SONG_BOLERO_MM,   COMBO_SETTING_SONG_SERENADE_MM,
                                              COMBO_SETTING_SONG_REQUIEM_MM,  COMBO_SETTING_SONG_NOCTURNE_MM,
                                              COMBO_SETTING_SONG_PRELUDE_MM };
const ComboSettingId kSwsSharedKeys[7] = { COMBO_SETTING_SHARED_SONG_SOARING, COMBO_SETTING_SHARED_SONG_MINUET,
                                           COMBO_SETTING_SHARED_SONG_BOLERO,  COMBO_SETTING_SHARED_SONG_SERENADE,
                                           COMBO_SETTING_SHARED_SONG_REQUIEM, COMBO_SETTING_SHARED_SONG_NOCTURNE,
                                           COMBO_SETTING_SHARED_SONG_PRELUDE };

void SwsClearKeys() {
    for (int s = 0; s < 7; s++) {
        CVarClear(Combo_ComboSettingKey(kSwsExtensionKeys[s]));
        CVarClear(Combo_ComboSettingKey(kSwsSharedKeys[s]));
    }
    CVarClear(kSwsShuffleSongsKey);
    CVarClear(kSwsLinksPocketKey);
    CVarClear(kSwsStartMinuetKey);
    Combo_ComboItemRules2RevealForTest(0);
}

struct SwsRestoreKeys {
    ~SwsRestoreKeys() {
        SwsClearKeys();
        Combo_SingleBag_Forget();
        ComboContext_Init();
    }
};

/** One generation leg: @p extensions / @p shared are bit masks over the seven songs. */
int SwsGenerate(const char* seed, int shuffleSongs, uint32_t extensions, uint32_t shared, bool pocketAdvancement,
                bool startMinuet) {
    Combo_SingleBag_Forget(); // the previous leg's world is never rolled back over this one
    ComboContext_Init();
    SwsClearKeys();
    for (int s = 0; s < 7; s++) {
        if ((extensions & (1u << s)) != 0u) {
            CVarSetInteger(Combo_ComboSettingKey(kSwsExtensionKeys[s]), 1);
        }
        if ((shared & (1u << s)) != 0u) {
            CVarSetInteger(Combo_ComboSettingKey(kSwsSharedKeys[s]), 1);
        }
    }
    CVarSetInteger(kSwsShuffleSongsKey, shuffleSongs);
    if (pocketAdvancement) {
        CVarSetInteger(kSwsLinksPocketKey, kSwsLinksPocketAdvancement);
    }
    if (startMinuet) {
        CVarSetInteger(kSwsStartMinuetKey, 1);
    }
    // Every rule is offered by the production mask since #1169 XW14: no test seam.
    return Rando_HeadlessSeedTest(seed);
}

/** A shared song's placements by the coordinator, of either half on either host. */
struct SwsPlaced {
    int pairRows;    // carrying the shared pair
    int orphanRows;  // either half placed WITHOUT the pair
    int onSongCheck; // pair rows on a song check of either game
};

SwsPlaced SwsCountPlaced(uint16_t ootId, uint16_t mmId) {
    SwsPlaced r = { 0, 0, 0 };
    for (GameId host : { GAME_OOT, GAME_MM }) {
        for (int i = 0; i < Combo_Logic_PlacementCount(host); i++) {
            ComboLogicPlacement p;
            if (!Combo_Logic_PlacementAt(host, i, &p)) {
                continue;
            }
            const bool half = (p.item.originGame == (uint8_t)GAME_OOT && p.item.id == ootId) ||
                              (p.item.originGame == (uint8_t)GAME_MM && p.item.id == mmId);
            if (!half) {
                continue;
            }
            if (p.pair.originGame == (uint8_t)GAME_NONE) {
                r.orphanRows++;
                continue;
            }
            r.pairRows++;
            const int songIndex = host == GAME_OOT ? OoT_ComboLogic_TestSongCheckIndex(p.hostCheck)
                                                   : MM_Rando_Foreign_TestSongCheckIndex(p.hostCheck);
            r.onSongCheck += songIndex >= 0 ? 1 : 0;
        }
    }
    return r;
}

/**
 * The inputs the single bag builds the Song of Soaring (OoT)'s row from, read at
 * OoT's seam exactly as combo_single_bag.c reads them: the pool row's flags
 * (OoT_ComboLogic_ExportPool source 0), its confinement mask
 * (OoT_ComboLogic_PoolRowConfineMasks) and the composer's disposition under OoT's
 * armed word (ConfinementArmed | Combo_ItemClassArmedFromFrozen). The row's other
 * flag inputs, the direction's HOME_ONLY and the shared-pair flag, do not depend
 * on the songs' rules (the Soaring is no pair in either run of leg (2)).
 */
struct SwsSoaringOoTRow {
    int rows;           // pool rows of the Song of Soaring (OoT)
    uint16_t poolFlags; // RSBS_COMBO_POOL_* of the last one
    uint32_t confine;   // its confinement mask
    int disposition;    // RSBS_COMBO_COMPOSE_*; -1 when no row
};

SwsSoaringOoTRow SwsReadSoaringOoTRow() {
    SwsSoaringOoTRow r = { 0, 0u, 0u, -1 };
    uint16_t soaringId = 0;
    if (!Combo_SharedFamily2MemberId(GAME_OOT, kSwsSharedSoaring, &soaringId)) {
        return r;
    }
    const int total = OoT_ComboLogic_ExportPool(0, nullptr, nullptr, nullptr, 0);
    if (total <= 0) {
        return r;
    }
    std::vector<uint16_t> items((size_t)total, 0);
    std::vector<uint16_t> flags((size_t)total, 0);
    std::vector<uint32_t> confine((size_t)total, 0u);
    const int got = OoT_ComboLogic_ExportPool(0, items.data(), nullptr, flags.data(), total);
    const int n = got < total ? got : total;
    if (n <= 0) {
        return r;
    }
    (void)OoT_ComboLogic_PoolRowConfineMasks(items.data(), n, confine.data());
    const uint32_t armed = OoT_ComboLogic_ConfinementArmed() | Combo_ItemClassArmedFromFrozen((uint8_t)GAME_OOT);
    for (int i = 0; i < n; i++) {
        if (items[(size_t)i] != soaringId) {
            continue;
        }
        SharedItem it;
        memset(&it, 0, sizeof(it));
        it.originGame = (uint8_t)GAME_OOT;
        it.id = soaringId;
        r.rows++;
        r.poolFlags = flags[(size_t)i];
        r.confine = confine[(size_t)i];
        r.disposition = Combo_Logic_ComposeDisposition(it, flags[(size_t)i], armed);
    }
    return r;
}

/** One exit round over the created world (both real engines, nothing assumed),
 *  with @p forbidden (may be NULL) withheld; the fixed credits it delivered go to
 *  @p outDelivered. */
int SwsExitRound(const ComboLogicHostRef* forbidden, int forbiddenCount, ComboLogicRoundResult* out,
                 int* outDelivered) {
    ComboLogicRoundRequest req;
    memset(&req, 0, sizeof(req));
    req.goal = gComboCtx.comboSettings.goal;
    req.forbidden = forbidden;
    req.forbiddenCount = forbiddenCount;
    memset(out, 0, sizeof(*out));
    const int status = Combo_Logic_RunRound(&req, out);
    *outDelivered = Combo_Logic_FixedCreditDeliveredCount();
    return status;
}

/**
 * #1169 XW15, Off only (shared-warp-songs-one-copy-off): THE FIXED SHARED CREDITS in
 * the created world's exit round. Every shared warp song sits on its own check and
 * no bag row carries it, so only the credits make the other game's logic hold it:
 * Majora's Mask holds the Minuet of Forest (MM) once Sheik in Forest is reached
 * (MM's travel fact into OoT), Ocarina of Time the Song of Soaring (OoT) once the
 * Southern Swamp check is reached (OoT's travel fact into MM). Forbidding either
 * host takes that credit away (the forbidden-host skip): its travel fact is gone
 * and fewer credits are delivered. The count is not asserted exactly, because a
 * forbidden host can also cut off other hosts.
 */
void SwsOffExitRounds() {
    ComboLogicRoundResult base;
    int baseDelivered = -1;
    const int st = SwsExitRound(nullptr, 0, &base, &baseDelivered);
    printf("[TEST] (Off) exit round: status %d, goal %d, travel to OoT %08X, to MM %08X, fixed credits delivered %d "
           "of %d\n",
           st, base.goalExpression, (unsigned)base.travelToOoT, (unsigned)base.travelToMM, baseDelivered,
           Combo_Logic_FixedCreditCount());
    SWS_CHECK(st == RSBS_COMBO_LOGIC_OK && base.goalExpression == 1,
              "(Off) the created world's exit round did not run or did not prove the goal (status %d, goal %d)", st,
              base.goalExpression);
    SWS_CHECK(baseDelivered == 7,
              "(Off) the exit round delivered %d of the seven fixed shared credits: a default check of a shared "
              "warp song was unreached in the proved world, or a credit was not delivered",
              baseDelivered);
    SWS_CHECK((base.travelToOoT & RSBS_XITEM2_SONG_MINUET_MM) != 0u,
              "(Off) MM's logic never played the Minuet of Forest into OoT (%08X): the Minuet fixed at Sheik in Forest "
              "was not credited to Majora's Mask",
              (unsigned)base.travelToOoT);
    SWS_CHECK(base.travelToMM == RSBS_COMBO_TRAVEL_INTO_MM,
              "(Off) OoT's logic never played the Song of Soaring into MM (%08X): the Soaring fixed in the Southern "
              "Swamp was not credited to Ocarina of Time",
              (unsigned)base.travelToMM);

    uint16_t minuetMM = 0;
    (void)Combo_SharedFamily2MemberId(GAME_MM, kSwsSharedMinuet, &minuetMM);
    ComboLogicHostRef minuetHost;
    memset(&minuetHost, 0, sizeof(minuetHost));
    for (int c = 0; c < Combo_Logic_FixedCreditCount(); c++) {
        ComboLogicFixedCredit credit;
        if (Combo_Logic_FixedCreditAt(c, &credit) && credit.hostGame == (uint8_t)GAME_OOT &&
            credit.farItem.id == minuetMM) {
            minuetHost.hostGame = credit.hostGame;
            minuetHost.hostCheck = credit.hostCheck;
        }
    }
    SWS_CHECK(minuetHost.hostGame == (uint8_t)GAME_OOT, "(Off) no fixed credit names the Minuet of Forest's host");
    if (minuetHost.hostGame == (uint8_t)GAME_OOT) {
        ComboLogicRoundResult r;
        int delivered = -1;
        const int s = SwsExitRound(&minuetHost, 1, &r, &delivered);
        printf("[TEST] (Off) Sheik's Minuet check %u forbidden: status %d, travel to OoT %08X, delivered %d\n",
               (unsigned)minuetHost.hostCheck, s, (unsigned)r.travelToOoT, delivered);
        SWS_CHECK(s == RSBS_COMBO_LOGIC_OK && (r.travelToOoT & RSBS_XITEM2_SONG_MINUET_MM) == 0u &&
                      delivered < baseDelivered,
                  "(Off) with the Minuet's own check forbidden MM still held the Minuet (travel %08X, delivered %d "
                  "of %d): a forbidden host's credit crossed",
                  (unsigned)r.travelToOoT, delivered, baseDelivered);
    }
    {
        ComboLogicHostRef swamp;
        memset(&swamp, 0, sizeof(swamp));
        swamp.hostGame = (uint8_t)GAME_MM;
        swamp.hostCheck = (uint16_t)MM_ComboLogic_TestSwampSoaringCheck();
        ComboLogicRoundResult r;
        int delivered = -1;
        const int s = SwsExitRound(&swamp, 1, &r, &delivered);
        printf("[TEST] (Off) the Southern Swamp's Soaring check forbidden: status %d, travel to MM %08X, delivered "
               "%d\n",
               s, (unsigned)r.travelToMM, delivered);
        SWS_CHECK(s == RSBS_COMBO_LOGIC_OK && r.travelToMM == 0u && delivered < baseDelivered,
                  "(Off) with the Soaring's own check forbidden OoT still held the Song of Soaring (travel %08X, "
                  "delivered %d of %d)",
                  (unsigned)r.travelToMM, delivered, baseDelivered);
    }
}
} // namespace

TestResult SharedWarpSongsOneCopy_Run(int shuffleSongs) {
    const char* name = (shuffleSongs >= 0 && shuffleSongs < 4) ? kSwsShuffleNames[shuffleSongs] : "?";
    const bool pocket = shuffleSongs == 0 || shuffleSongs == 3; // Off and Anywhere: Link's Pocket may take a song
    printf("[TEST] shared-warp-songs-one-copy (Shuffle Songs %s): with the seven warp-song extensions and the seven "
           "shared songs on, the paired world holds ONE copy of each shared warp song, the bag's shared row, and the "
           "GOAL is still proven (#1169 XW10)\n",
           name);
    sSwsFailures = 0;
    SwsRestoreKeys restore;
    const int gen = SwsGenerate("RSBSXW10ONECOPY", shuffleSongs, 0x7Fu, 0x7Fu, pocket, false);
    SWS_CHECK(gen == 0, "the paired OoT generation failed under Shuffle Songs %s", name);
    SWS_CHECK(Combo_ComboCrossItemFlags2() == kSwsAll, "the creation froze %08X, not all fourteen rules",
              (unsigned)Combo_ComboCrossItemFlags2());
    const int mmHalf = gen == 0 ? MM_Rando_HeadlessPairedHalf() : -1;
    SWS_CHECK(mmHalf == 0, "MM's creation-time half did not produce a rando world");
    if (gen != 0 || mmHalf != 0) {
        printf("[TEST] shared-warp-songs-one-copy (%s): FAIL\n", name);
        return TEST_FAIL;
    }
    const ComboSingleBagReport bag = *Combo_SingleBag_LastReport();
    printf("[TEST] shared-warp-songs-one-copy (%s): bag %d rows, %d shared pair(s), goal proven %d\n", name,
           bag.bagCount, bag.compose.sharedPairs, bag.fill.goalProven ? 1 : 0);
    SWS_CHECK(bag.status == RSBS_COMBO_LOGIC_OK && bag.fill.goalProven, "the single-bag fill did not prove the GOAL");
    // #1169 XW15: under Off every shared warp song stays on its own default check,
    // so neither pool holds a copy and the composer pairs nothing (combo_logic.c
    // pass 2b, "neither pool holds the family"); every other value pairs all seven.
    const bool off = shuffleSongs == 0;
    if (off) {
        SWS_CHECK(bag.compose.sharedPairs == 0,
                  "under Off the composer made %d shared pair(s): a shared warp song left its default check for the "
                  "bag",
                  bag.compose.sharedPairs);
    } else {
        SWS_CHECK(bag.compose.sharedPairs >= 7, "the composer made %d shared pair(s), fewer than the seven songs",
                  bag.compose.sharedPairs);
    }
    // THE FIXED SHARED CREDITS: one per shared warp song under Off (each finder's
    // answer, Combo_SingleBag_Run's wiring), none in any other mode.
    SWS_CHECK(Combo_Logic_FixedCreditCount() == (off ? 7 : 0),
              "the creation left %d fixed shared credit(s) under %s; want %d", Combo_Logic_FixedCreditCount(), name,
              off ? 7 : 0);

    const bool mmVanilla = Combo_PairSongsMmValue((uint32_t)shuffleSongs) == RSBS_MM_SONGS_VANILLA;
    const int shuffledTotal = MM_ComboLogic_TestShuffledItems(nullptr, nullptr, 0);
    std::vector<uint16_t> items((size_t)(shuffledTotal > 0 ? shuffledTotal : 1));
    const int shuffled = MM_ComboLogic_TestShuffledItems(items.data(), nullptr, (int)items.size());
    const int pocketItem = OoT_SharedWarpSongsTest_LinksPocket();
    // The Link's Pocket exclusion itself (fill.cpp RsbsSongHeldForBag), asked per id
    // so the lock does not rest on this seed's one draw: every shared warp song's
    // OoT member is the bag's under Song Locations, Dungeon Rewards and Anywhere
    // (under Off it stays on its own check, #1169 XW15), and an unshared song only
    // where the shared song pool takes every song (Song Locations). Anywhere
    // answers 0 for the seven without the XW10 branch.
    {
        const int unshared = OoT_SharedWarpSongsTest_UnsharedSongId();
        const int wantUnshared = shuffleSongs == 1 ? 1 : 0;
        SWS_CHECK(OoT_Fill_TestSongHeldForBag((uint16_t)unshared) == wantUnshared,
                  "Link's Pocket's song exclusion answers %d for an unshared song under %s; want %d",
                  OoT_Fill_TestSongHeldForBag((uint16_t)unshared), name, wantUnshared);
    }
    for (int s = 0; s < 7; s++) {
        uint16_t ootId = 0;
        uint16_t mmId = 0;
        const bool registered = Combo_SharedFamily2MemberId(GAME_OOT, SwsSharedBitOf(s), &ootId) &&
                                Combo_SharedFamily2MemberId(GAME_MM, SwsSharedBitOf(s), &mmId);
        SWS_CHECK(registered, "the shared %s is not registered in both games", kSwsSongNames[s]);
        if (!registered) {
            continue;
        }
        // Under Off (#1169 XW15) no shared warp song is held back, so the bridge
        // answers 0 (and no song is in the pool for Link's Pocket anyway).
        const int wantHeld = off ? 0 : 1;
        SWS_CHECK(OoT_Fill_TestSongHeldForBag(ootId) == wantHeld,
                  "Link's Pocket's exclusion answers %d for the shared %s under %s; want %d (held back for the bag "
                  "except under Off, where it stays on its own check)",
                  OoT_Fill_TestSongHeldForBag(ootId), kSwsSongNames[s], name, wantHeld);
        const int ootCopies = OoT_SharedSongsTest_WorldCopies((int)ootId);
        int mmCopies = 0;
        if (mmVanilla) {
            mmCopies = MM_PlacementConfinement_WorldCopies(mmId);
        } else {
            for (int i = 0; i < shuffled && i < (int)items.size(); i++) {
                mmCopies += items[(size_t)i] == mmId ? 1 : 0;
            }
        }
        const SwsPlaced placed = SwsCountPlaced(ootId, mmId);
        printf("[TEST]   %s: OoT %d, MM %d, bag pair rows %d (%d on a song check), orphans %d\n", kSwsSongNames[s],
               ootCopies, mmCopies, placed.pairRows, placed.onSongCheck, placed.orphanRows);
        SWS_CHECK(ootCopies >= 0 && ootCopies + mmCopies == 1,
                  "the %s is held by %d OoT and %d MM checks; want ONE over both worlds (the pair's one placement, as "
                  "the hosting game's half)",
                  kSwsSongNames[s], ootCopies, mmCopies);
        const int wantPairRows = off ? 0 : 1;
        SWS_CHECK(placed.pairRows == wantPairRows && placed.orphanRows == 0,
                  "the %s was placed by the bag %d time(s) as the shared pair and %d time(s) without it; want %d "
                  "pair row(s) and no orphan copy of either half",
                  kSwsSongNames[s], placed.pairRows, placed.orphanRows, wantPairRows);
        if (off) {
            // THE DEFAULT CHECK: the six in OoT (their Sheik checks), the Song of
            // Soaring in MM (the Southern Swamp check), and no copy in the other game.
            const int wantOoT = s >= 1 ? 1 : 0;
            SWS_CHECK(ootCopies == wantOoT && mmCopies == 1 - wantOoT,
                      "under Off the %s is held by %d OoT and %d MM checks; want it on its own default check in %s "
                      "only",
                      kSwsSongNames[s], ootCopies, mmCopies, s >= 1 ? "Ocarina of Time" : "Majora's Mask");
            ComboLogicFixedCredit credit;
            bool found = false;
            for (int c = 0; c < Combo_Logic_FixedCreditCount() && !found; c++) {
                found = Combo_Logic_FixedCreditAt(c, &credit) &&
                        ((credit.hostGame == (uint8_t)GAME_OOT && credit.farItem.originGame == (uint8_t)GAME_MM &&
                          credit.farItem.id == mmId) ||
                         (credit.hostGame == (uint8_t)GAME_MM && credit.farItem.originGame == (uint8_t)GAME_OOT &&
                          credit.farItem.id == ootId));
            }
            bool hostRight = false;
            if (found && s >= 1) {
                uint16_t hosts[4] = { 0, 0, 0, 0 };
                hostRight = credit.hostGame == (uint8_t)GAME_OOT &&
                            OoT_ComboLogic_TestHostsHolding(ootId, hosts, 4) == 1 && hosts[0] == credit.hostCheck &&
                            OoT_ComboLogic_TestSongCheckIndex(credit.hostCheck) >= 0;
            } else if (found) {
                hostRight = credit.hostGame == (uint8_t)GAME_MM &&
                            (int)credit.hostCheck == MM_ComboLogic_TestSwampSoaringCheck() &&
                            MM_ComboLogic_TestFixedCheckItem(credit.hostCheck) == (int)mmId;
            }
            printf("[TEST]   %s: fixed credit %s, host %s %u\n", kSwsSongNames[s], found ? "found" : "MISSING",
                   found ? (credit.hostGame == (uint8_t)GAME_OOT ? "OoT" : "MM") : "-",
                   found ? (unsigned)credit.hostCheck : 0u);
            SWS_CHECK(found && hostRight,
                      "under Off the %s has no fixed credit naming its own default check (%s) and the other game's "
                      "member",
                      kSwsSongNames[s], s >= 1 ? "its Sheik check" : "the Southern Swamp song check, unshuffled");
        }
        if (shuffleSongs == 1) {
            SWS_CHECK(placed.onSongCheck == 1,
                      "under Song Locations the %s's one copy is not on a song check of "
                      "either game",
                      kSwsSongNames[s]);
        }
        SWS_CHECK(pocketItem != (int)ootId, "Link's Pocket holds the shared %s: its one copy is the bag's row",
                  kSwsSongNames[s]);
    }
    if (off) {
        // #1169 XW15: every one of OoT's twelve songs is at its vanilla check, the
        // six shared warp songs on their Sheik checks included.
        const int expectAtHome = OoT_SharedSongsTest_VanillaSongCount();
        const int atHome = OoT_SharedSongsTest_OtherVanillaSongsAtHome(nullptr, 0);
        SWS_CHECK(atHome == expectAtHome, "under Off %d of OoT's %d songs are at their vanilla checks", atHome,
                  expectAtHome);
        SwsOffExitRounds();
    }
    SWS_CHECK(Combo_SharedFamilies2Owned() == 0u && Combo_MMWarpSongsOwned() == 0u,
              "the creation left the family word %08X and MM's ownership byte %02X: a simulated pickup leaked",
              (unsigned)Combo_SharedFamilies2Owned(), (unsigned)Combo_MMWarpSongsOwned());
    const int crossings = OoT_Creation_FinishPairedHalf(0);
    SWS_CHECK(crossings >= 0, "OoT's side of the creation tail did not finish the pair");
    printf("[TEST] shared-warp-songs-one-copy (%s): %s\n", name, sSwsFailures == 0 ? "PASS" : "FAIL");
    return sSwsFailures == 0 ? TEST_PASS : TEST_FAIL;
}

TestResult SharedWarpSongsEdges_Run(void) {
    printf("[TEST] shared-warp-songs-edges: the Song of Soaring (OoT) beside a shared six fits Dungeon Rewards; under "
           "Off the songs family is armed for a lone Soaring (OoT) whatever the shared rules and never for a shared "
           "warp song alone (XW15); a started shared Minuet is no bag row (#1169 XW10)\n");
    sSwsFailures = 0;
    SwsRestoreKeys restore;

    // ---- (1) Dungeon Rewards: 12 OoT songs - the shared Minuet + the Soaring (OoT) = 12 -----------
    {
        Combo_SongSpotsRecordRefusal(NULL);
        const int rc = SwsGenerate("RSBSXW10DR", 2, 0x03u, 0x02u, false, false);
        const char* refusal = Combo_SongSpotsLastRefusal();
        printf("[TEST] (1) Dungeon Rewards, Soaring (OoT) + shared Minuet: generation %d, refusal '%s'\n", rc, refusal);
        SWS_CHECK(rc == 0 && refusal[0] == '\0',
                  "(1) the Song of Soaring (OoT) with a shared Minuet was refused under Dungeon Rewards ('%s'): the "
                  "shared Minuet must leave OoT's twelve spots for the bag",
                  refusal);
        const int mmHalf = rc == 0 ? MM_Rando_HeadlessPairedHalf() : -1;
        const ComboSingleBagReport* bag = Combo_SingleBag_LastReport();
        SWS_CHECK(mmHalf == 0 && bag->status == RSBS_COMBO_LOGIC_OK && bag->fill.goalProven,
                  "(1) the Dungeon Rewards pair did not finish with the goal proven");
        if (mmHalf == 0) {
            (void)OoT_Creation_FinishPairedHalf(0);
        }
    }

    // ---- (2) Off: the songs family is armed for a lone Soaring (OoT), with or without a shared six ---
    {
        const SwsSoaringOoTRow none = { 0, 0u, 0u, -1 };
        const int rcAlone = SwsGenerate("RSBSXW10OFF", 0, 0x01u, 0u, false, false);
        const uint32_t armAlone = rcAlone == 0 ? OoT_ComboLogic_ConfinementArmed() : 0u;
        const SwsSoaringOoTRow rowAlone = rcAlone == 0 ? SwsReadSoaringOoTRow() : none;
        const int rcShared = SwsGenerate("RSBSXW10OFF", 0, 0x03u, 0x02u, false, false);
        const uint32_t armShared = rcShared == 0 ? OoT_ComboLogic_ConfinementArmed() : 0u;
        const SwsSoaringOoTRow rowShared = rcShared == 0 ? SwsReadSoaringOoTRow() : none;
        printf("[TEST] (2) Off: Soaring (OoT) alone generation %d arm %08X; with a shared Minuet %d arm %08X\n",
               rcAlone, (unsigned)armAlone, rcShared, (unsigned)armShared);
        SWS_CHECK(rcAlone == 0 && rcShared == 0, "(2) an Off generation with the Song of Soaring (OoT) failed");
        SWS_CHECK((armAlone & RSBS_FILL_ARM_SONGS_ROAM) != 0u && (armShared & RSBS_FILL_ARM_SONGS_ROAM) != 0u,
                  "(2) under Off the songs family is armed %d alone and %d with a shared Minuet: whether the Song of "
                  "Soaring (OoT) may leave OoT must not depend on an unrelated shared rule",
                  (armAlone & RSBS_FILL_ARM_SONGS_ROAM) != 0u ? 1 : 0,
                  (armShared & RSBS_FILL_ARM_SONGS_ROAM) != 0u ? 1 : 0);
        // The arm bit alone does not show the two runs agree for the right reason
        // (before #1169 XW15 a shared six armed it through the hold-back; now only
        // the Soaring (OoT)'s own rule does, leg (2b)): the Soaring (OoT)'s own
        // bag-row inputs must be the same in both, one REQUIRED row with no mask.
        printf("[TEST] (2) Soaring (OoT) pool row: alone %d row(s) flags %04X mask %08X %s; with a shared Minuet %d "
               "row(s) flags %04X mask %08X %s\n",
               rowAlone.rows, (unsigned)rowAlone.poolFlags, (unsigned)rowAlone.confine,
               Combo_Logic_ComposeDispositionName(rowAlone.disposition), rowShared.rows, (unsigned)rowShared.poolFlags,
               (unsigned)rowShared.confine, Combo_Logic_ComposeDispositionName(rowShared.disposition));
        SWS_CHECK(rowAlone.rows == 1 && rowAlone.disposition == RSBS_COMBO_COMPOSE_REQUIRED && rowAlone.confine == 0u,
                  "(2) under Off a lone Song of Soaring (OoT) is %d pool row(s), %s, mask %08X; want one REQUIRED "
                  "row with no mask, the bag's to place",
                  rowAlone.rows, Combo_Logic_ComposeDispositionName(rowAlone.disposition), (unsigned)rowAlone.confine);
        SWS_CHECK(rowShared.rows == rowAlone.rows && rowShared.poolFlags == rowAlone.poolFlags &&
                      rowShared.confine == rowAlone.confine && rowShared.disposition == rowAlone.disposition,
                  "(2) the Song of Soaring (OoT)'s bag row changed with an unrelated shared Minuet (rows %d/%d, "
                  "flags %04X/%04X, mask %08X/%08X, %s/%s)",
                  rowAlone.rows, rowShared.rows, (unsigned)rowAlone.poolFlags, (unsigned)rowShared.poolFlags,
                  (unsigned)rowAlone.confine, (unsigned)rowShared.confine,
                  Combo_Logic_ComposeDispositionName(rowAlone.disposition),
                  Combo_Logic_ComposeDispositionName(rowShared.disposition));
    }

    // ---- (2b) Off, #1169 XW15: a shared warp song alone arms nothing (it stays on its own check) ----
    {
        const int rcMinuet = SwsGenerate("RSBSXW15OFF", 0, 0x02u, 0x02u, false, false);
        const uint32_t armMinuet = rcMinuet == 0 ? OoT_ComboLogic_ConfinementArmed() : 0xFFFFFFFFu;
        const int rcSoaring = SwsGenerate("RSBSXW15OFF", 0, 0x01u, 0x01u, false, false);
        const uint32_t armSoaring = rcSoaring == 0 ? OoT_ComboLogic_ConfinementArmed() : 0xFFFFFFFFu;
        printf("[TEST] (2b) Off: shared Minuet alone generation %d arm %08X; shared Soaring alone %d arm %08X\n",
               rcMinuet, (unsigned)armMinuet, rcSoaring, (unsigned)armSoaring);
        SWS_CHECK(rcMinuet == 0 && rcSoaring == 0, "(2b) an Off generation with one shared warp song failed");
        SWS_CHECK((armMinuet & RSBS_FILL_ARM_SONGS_ROAM) == 0u && (armSoaring & RSBS_FILL_ARM_SONGS_ROAM) == 0u,
                  "(2b) under Off a shared warp song armed OoT's songs to roam (Minuet %d, Soaring %d): it stays on "
                  "its own default check, so nothing of it reaches the bag",
                  (armMinuet & RSBS_FILL_ARM_SONGS_ROAM) != 0u ? 1 : 0,
                  (armSoaring & RSBS_FILL_ARM_SONGS_ROAM) != 0u ? 1 : 0);
    }

    // ---- (3) A started shared Minuet: no bag row, nothing placed ------------------------------------
    {
        const int rc = SwsGenerate("RSBSXW10START", 3, 0x02u, 0x02u, false, true);
        const int mmHalf = rc == 0 ? MM_Rando_HeadlessPairedHalf() : -1;
        SWS_CHECK(rc == 0 && mmHalf == 0, "(3) the started shared Minuet world did not generate");
        if (rc == 0 && mmHalf == 0) {
            uint16_t ootId = 0;
            uint16_t mmId = 0;
            (void)Combo_SharedFamily2MemberId(GAME_OOT, kSwsSharedMinuet, &ootId);
            (void)Combo_SharedFamily2MemberId(GAME_MM, kSwsSharedMinuet, &mmId);
            const SwsPlaced placed = SwsCountPlaced(ootId, mmId);
            const int ootCopies = OoT_SharedSongsTest_WorldCopies((int)ootId);
            printf("[TEST] (3) started Minuet: bag pair rows %d, orphans %d, OoT world copies %d, family word %08X\n",
                   placed.pairRows, placed.orphanRows, ootCopies, (unsigned)Combo_SharedFamilies2Owned());
            SWS_CHECK(placed.pairRows == 0 && placed.orphanRows == 0 && ootCopies == 0,
                      "(3) a started shared Minuet was placed (pair rows %d, orphans %d, OoT copies %d)",
                      placed.pairRows, placed.orphanRows, ootCopies);
            SWS_CHECK(Combo_SharedFamilies2Owned() == 0u,
                      "(3) the creation itself owned the family word (%08X); MM receives the started song at its "
                      "first arrival, through OoT's harvest and MM's apply",
                      (unsigned)Combo_SharedFamilies2Owned());
            // G6, THE KNOWN UNDER-CREDIT (#1552): the created world's exit round over
            // both real engines. A started shared Minuet is OoT's starting item and no
            // bag row, so nothing hands MM's engine its copy and MM's logic plays no
            // Minuet into OoT during generation (OoTMM credits both sides). The
            // conservative answer is recorded here on purpose: the #1552 fix flips
            // the Minuet assertion below, and must change it with that fix.
            ComboLogicRoundRequest req;
            memset(&req, 0, sizeof(req));
            req.goal = gComboCtx.comboSettings.goal;
            ComboLogicRoundResult exitRound;
            memset(&exitRound, 0, sizeof(exitRound));
            const int roundStatus = Combo_Logic_RunRound(&req, &exitRound);
            printf("[TEST] (3) G6 exit round: status %d, goal %d, crossings OoT %d MM %d, travel to OoT %08X, to MM "
                   "%08X\n",
                   roundStatus, exitRound.goalExpression, exitRound.crossingOpenOoT, exitRound.crossingOpenMM,
                   (unsigned)exitRound.travelToOoT, (unsigned)exitRound.travelToMM);
            SWS_CHECK(roundStatus == RSBS_COMBO_LOGIC_OK && exitRound.goalExpression == 1 &&
                          exitRound.crossingOpenOoT == 1,
                      "(3) G6: the started-Minuet world's exit round did not run, prove the goal or open OoT's "
                      "crossing (status %d, goal %d, crossing %d)",
                      roundStatus, exitRound.goalExpression, exitRound.crossingOpenOoT);
            SWS_CHECK((exitRound.travelToOoT & RSBS_XITEM2_SONG_MINUET_MM) == 0u,
                      "(3) G6: MM's logic credited the started shared Minuet as travel into OoT (%08X). If #1552 "
                      "landed this is the intended flip: change this assertion with it",
                      (unsigned)exitRound.travelToOoT);
            (void)OoT_Creation_FinishPairedHalf(0);

            // ... and the started song reaches MM at the first arrival: OoT's suspend
            // harvest of its starting Minuet (simulated: a ROM-free process creates no
            // save, so the hook puts the song where the creation's starting inventory
            // does), then MM's apply, under the world's OWN frozen word.
            const uint32_t frozen2 = Combo_ComboCrossItemFlags2();
            SWS_CHECK(frozen2 == (RSBS_XITEM2_SONG_MINUET_MM | RSBS_XITEM2_SHARED_SONG_MINUET),
                      "(3) G6: the started-Minuet world froze %08X, not the Minuet extension and its shared rule",
                      (unsigned)frozen2);
            SwsForgetAll();
            OoT_SharedSwordTest_ArmLive();
            OoT_SharedWarpSongsTest_SetHeld(1, 1);
            OoT_HarvestSharedResources();
            const uint32_t owned2 = Combo_SharedFamilies2Owned();
            MM_SharedSwordTest_ArmLive();
            MM_ApplySharedResources();
            printf("[TEST] (3) G6 first arrival: family word %08X, MM's ownership byte %02X\n", (unsigned)owned2,
                   (unsigned)Combo_MMWarpSongsOwned());
            SWS_CHECK(owned2 == kSwsSharedMinuet && Combo_MMWarpSongsOwned() == 0x01u,
                      "(3) G6: the started shared Minuet harvested %08X (want 00000100) and MM's apply left the "
                      "ownership byte %02X (want 01, the Minuet alone)",
                      (unsigned)owned2, (unsigned)Combo_MMWarpSongsOwned());
        }
    }

    printf("[TEST] shared-warp-songs-edges: %s\n", sSwsFailures == 0 ? "PASS" : "FAIL");
    return sSwsFailures == 0 ? TEST_PASS : TEST_FAIL;
}
