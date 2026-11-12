/**
 * @file test_shared_warp_songs.c
 * @brief Locks for the seven SHARED warp songs (#1169 lane XW10; OoTMM
 *        sharedSongSoaring, sharedSongMinuet .. sharedSongPrelude): the second
 *        family word, its registry and pairs, the players' "held" question, the
 *        runtime merge through both games' real shims, and the one-copy worlds.
 *
 * Every rule stays hidden (RSBS_XITEM2_HIDDEN) until lane XW12. The ROM-free rows
 * freeze the second word directly (Combo_FreezeComboSettingsEx2, no Generate, so
 * no refusal to reveal); the generating rows reveal the hidden rules through
 * Combo_ComboItemRules2RevealForTest and put it back.
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
 *   Songs value, all fourteen rules on: one placement over both worlds, the bag's
 *   SHARED_PAIR row, no orphan copy of either half, on a song check of either
 *   game under Song Locations, Link's Pocket (Advancement under Off and Anywhere)
 *   never a shared warp song, the other songs vanilla under Off, the goal proven,
 *   nothing owned after the creation.
 *
 *   shared-warp-songs-edges (rando) -- the Song of Soaring (OoT) beside one shared
 *   six generates under Dungeon Rewards (12 for 12); under Off the songs family is
 *   armed for a lone Soaring (OoT) with or without a shared six; a started shared
 *   Minuet is no bag row and nothing is placed for it.
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
        ComboLogicSharedPair small[32];
        memset(small, 0xEE, sizeof(small));
        const int refused = Combo_SingleBag_SharedPairs(small, 32);
        SWS_CHECK(refused == -1 && small[0].ootId == 0xEEEEu,
                  "(8) a 32-pair buffer for %d pairs answered %d (first row %04X); want -1 and nothing written, never "
                  "a silent truncation",
                  expected, refused, (unsigned)small[0].ootId);
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
        MM_SharedSwordTest_ArmLive();
        MM_ApplySharedResources();
        printf("[TEST] (1) %s OoT -> MM: pool %08X, MM holds %d of 7 (this one %d)\n", kSwsSongNames[s],
               (unsigned)afterOoT, SwsHeldCount(true), MM_SharedWarpSongsTest_Held(s));
        SWS_CHECK(afterOoT == SwsSharedBitOf(s) && Combo_SharedFamiliesOwned() == 0u,
                  "(1) OoT's harvest put %08X for the %s, want exactly its family and nothing in the first word",
                  (unsigned)afterOoT, kSwsSongNames[s]);
        SWS_CHECK(MM_SharedWarpSongsTest_Held(s) == 1 && SwsHeldCount(true) == 1,
                  "(1) MM's apply did not give MM the %s alone", kSwsSongNames[s]);

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
        SWS_CHECK(MM_CreditSharedPickup(&item) == 1 && MM_SharedWarpSongsTest_Held(1) == 1 &&
                      Combo_MMWarpSongsOwned() == 0x01u,
                  "(8) OoT's Minuet found in Termina was not credited with MM's own Minuet (ownership byte %02X)",
                  (unsigned)Combo_MMWarpSongsOwned());

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
    // THE TEST SEAM: the hidden warp-song rules read as offered, as they will once
    // lane XW12 lands. Production never calls it.
    Combo_ComboItemRules2RevealForTest(1);
    const int rc = Rando_HeadlessSeedTest(seed);
    Combo_ComboItemRules2RevealForTest(0);
    return rc;
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
    SWS_CHECK(bag.compose.sharedPairs >= 7, "the composer made %d shared pair(s), fewer than the seven songs",
              bag.compose.sharedPairs);

    const bool mmVanilla = Combo_PairSongsMmValue((uint32_t)shuffleSongs) == RSBS_MM_SONGS_VANILLA;
    const int shuffledTotal = MM_ComboLogic_TestShuffledItems(nullptr, nullptr, 0);
    std::vector<uint16_t> items((size_t)(shuffledTotal > 0 ? shuffledTotal : 1));
    const int shuffled = MM_ComboLogic_TestShuffledItems(items.data(), nullptr, (int)items.size());
    int sharedRgs[6] = { -1, -1, -1, -1, -1, -1 };
    const int pocketItem = OoT_SharedWarpSongsTest_LinksPocket();
    for (int s = 0; s < 7; s++) {
        uint16_t ootId = 0;
        uint16_t mmId = 0;
        const bool registered = Combo_SharedFamily2MemberId(GAME_OOT, SwsSharedBitOf(s), &ootId) &&
                                Combo_SharedFamily2MemberId(GAME_MM, SwsSharedBitOf(s), &mmId);
        SWS_CHECK(registered, "the shared %s is not registered in both games", kSwsSongNames[s]);
        if (!registered) {
            continue;
        }
        if (s >= 1) {
            sharedRgs[s - 1] = (int)ootId;
        }
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
        SWS_CHECK(placed.pairRows == 1 && placed.orphanRows == 0,
                  "the %s was placed by the bag %d time(s) as the shared pair and %d time(s) without it; want one "
                  "pair row and no orphan copy of either half",
                  kSwsSongNames[s], placed.pairRows, placed.orphanRows);
        if (shuffleSongs == 1) {
            SWS_CHECK(placed.onSongCheck == 1,
                      "under Song Locations the %s's one copy is not on a song check of "
                      "either game",
                      kSwsSongNames[s]);
        }
        SWS_CHECK(pocketItem != (int)ootId, "Link's Pocket holds the shared %s: its one copy is the bag's row",
                  kSwsSongNames[s]);
    }
    if (shuffleSongs == 0) {
        const int expectAtHome = OoT_SharedSongsTest_VanillaSongCount() - 6;
        const int atHome = OoT_SharedSongsTest_OtherVanillaSongsAtHome(sharedRgs, 6);
        SWS_CHECK(atHome == expectAtHome, "under Off %d of the %d unshared OoT songs are at their vanilla checks",
                  atHome, expectAtHome);
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
           "Off the songs family is armed for a lone Soaring (OoT) whatever the shared rules; a started shared "
           "Minuet is no bag row (#1169 XW10)\n");
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
        const int rcAlone = SwsGenerate("RSBSXW10OFF", 0, 0x01u, 0u, false, false);
        const uint32_t armAlone = rcAlone == 0 ? OoT_ComboLogic_ConfinementArmed() : 0u;
        const int rcShared = SwsGenerate("RSBSXW10OFF", 0, 0x03u, 0x02u, false, false);
        const uint32_t armShared = rcShared == 0 ? OoT_ComboLogic_ConfinementArmed() : 0u;
        printf("[TEST] (2) Off: Soaring (OoT) alone generation %d arm %08X; with a shared Minuet %d arm %08X\n",
               rcAlone, (unsigned)armAlone, rcShared, (unsigned)armShared);
        SWS_CHECK(rcAlone == 0 && rcShared == 0, "(2) an Off generation with the Song of Soaring (OoT) failed");
        SWS_CHECK((armAlone & RSBS_FILL_ARM_SONGS_ROAM) != 0u && (armShared & RSBS_FILL_ARM_SONGS_ROAM) != 0u,
                  "(2) under Off the songs family is armed %d alone and %d with a shared Minuet: whether the Song of "
                  "Soaring (OoT) may leave OoT must not depend on an unrelated shared rule",
                  (armAlone & RSBS_FILL_ARM_SONGS_ROAM) != 0u ? 1 : 0,
                  (armShared & RSBS_FILL_ARM_SONGS_ROAM) != 0u ? 1 : 0);
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
            (void)OoT_Creation_FinishPairedHalf(0);
        }
    }

    printf("[TEST] shared-warp-songs-edges: %s\n", sSwsFailures == 0 ? "PASS" : "FAIL");
    return sSwsFailures == 0 ? TEST_PASS : TEST_FAIL;
}
