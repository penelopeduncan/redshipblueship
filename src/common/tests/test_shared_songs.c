/**
 * @file test_shared_songs.c
 * @brief Locks for the Shared Epona's Song and Shared Song of Storms cross-game
 *        rules (#1066; OoTMM sharedSongEpona, sharedSongStorms), two more
 *        families on the #976 mechanism after #1065's Shared Lens of Truth.
 *
 * Three rows, split by what each one needs (test_shared_lens.c's split):
 *
 *   shared-songs-rule (CTest SharedSongsRule, redship tier) -- THE RULES. Each
 *   song's bit is allocated (0x00400000 and 0x00800000, the next free
 *   RSBS_XITEM_* bits after the lens) and is a shared family; each has its own
 *   tier-4 key, labelled for the Cross-Game Rules page, taking exactly 0 or 1;
 *   each key reaches its own bit of the resolved item rules and nothing else;
 *   the creation freezes them and the fingerprint moves with each (and not with
 *   both off); a session that walked away from a songs-on file diverges by name,
 *   and the load puts the file's values back into the keys (frozen wins at
 *   load, #781), through both the restore API and a real .redsave Load.
 *
 *   shared-songs-merge (CTest SharedSongsMerge, redship tier) -- THE RUNTIME.
 *   Both games REGISTER their native copy of each song as that family's member
 *   (the ids the two games place the songs as), and through both games' REAL
 *   harvest and apply shims a song learnt in either game is known, and so
 *   playable on the ocarina, in the other at arrival; each rule carries only
 *   its own song; with the rules off nothing is carried. Under Skip Epona Race,
 *   OoT knowing Epona's Song is OoT having Epona (#1351): the shared song's
 *   arrival apply, the arrival's consume and the load seam each set the flag
 *   the summons read, and nothing else does.
 *
 *   shared-songs-bag (CTest SharedSongsBag, rando tier) -- THE SHARED ROWS. A
 *   real paired generation with both keys on and OoT's songs shuffled anywhere
 *   (so both games' copies are general-pass bag rows): the composer pairs each
 *   song's two copies into ONE row both solvers credit, each is placed exactly
 *   once over both games as a shared pair, MM's own world holds no second copy,
 *   and the GOAL is still proven.
 *
 *   shared-songs-one-copy-off / -song-locations / -dungeon-rewards / -anywhere
 *   (CTest SharedSongsOneCopy*, rando tier, #1085; Off revised by #1561) --
 *   Under Song Locations, Dungeon Rewards and Anywhere, each armed shared song
 *   (Epona's Song, the Song of Storms, the Song of Time) is one single-bag pair
 *   row held by exactly one check over both games' generated worlds. Under Off
 *   (#1561), each armed shared song stays fixed at BOTH games' default checks
 *   (Malon, Windmill, Ocarina of Time in OoT; Romani, Beneath the Graveyard,
 *   Clock Tower Roof in MM), registers TWO fixed credits (OoT->MM and MM->OoT)
 *   so either check alone credits both solvers, and converts whichever check is
 *   reached second into junk at pickup time without promising the song in hints.
 *
 *   shared-songs-off-junk-give (CTest SharedSongsOffJunkGive, redship tier,
 *   #1561) -- ROM-free lock on Combo_SharedWarpSongFixedAtHome and
 *   Combo_SharedSongOffGiveBecomesJunk across all three first-word shared songs,
 *   both games, both pickup orders, live probeHeld vs title screen, unshared
 *   songs, warp songs, non-song families, non-Off Shuffle Songs values, and MM's
 *   real ConvertItem + UncollectedNamedItem.
 *
 *   shared-songs-one-copy-mm-vanilla (CTest SharedSongsOneCopyMMVanilla, rando
 *   tier, #1502; #1542) -- MM'S KEY IS IGNORED: OoT's songs Anywhere with MAJORA'S
 *   MASK's own Shuffle Songs key at Vanilla. The pair has one songs setting, so
 *   MM's half must resolve Anywhere (red if MM's key is read for a paired world),
 *   and the world still holds one copy of each shared song.
 *
 * Every key is spelled from the identity prefix rather than as a literal, so
 * the tier-4 classification scan sees only the manifest's spelling.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * like its neighbours; every symbol under test is C-linkage.
 */

#include "../combo_logic.h"
#include "../combo_settings_view.h"
#include "../combo_single_bag.h"
#include "../combo_song_pool.h" // Combo_PairSongsMmValue (#1542)
#include "../context.h"
#include "../cvar_shared_keys.h"
#include "../foreign_items.h"
#include "../save.h"
#include "../shared_resources.h"
#include "../test_runner.h"
#include "../test_scratch.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
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
void OoT_SharedSongsTest_SetHeld(int song, int held);
int OoT_SharedSongsTest_Held(int song);
int OoT_SharedSongsTest_PoolId(int song);
void MM_SharedSongsTest_SetHeld(int song, int held);
int MM_SharedSongsTest_Held(int song);
int MM_SharedSongsTest_PoolId(int song);
int Rando_HeadlessSeedTest(const char* seedStr);
int MM_Rando_HeadlessPairedHalf(void);
int MM_ComboLogic_TestShuffledItems(uint16_t* outItems, uint16_t* outChecks, int cap);
int OoT_SharedSongsTest_WorldCopies(int rg);
int OoT_SharedSongsTest_OtherVanillaSongsAtHome(const int* sharedRgs, int sharedCount);
int OoT_SharedSongsTest_VanillaSongCount(void);
// games/mm/2s2h/mm_placement_confinement_test.cpp (#1502): every MM check holding
// the id, vanilla (unshuffled) checks included.
int MM_PlacementConfinement_WorldCopies(uint16_t id);
// The same file (#1542): the live MM save's frozen Shuffle Songs.
int MM_PlacementConfinement_FrozenSongs(void);
void OoT_SharedSongsTest_EponaBegin(void);
void OoT_SharedSongsTest_EponaEnd(void);
void OoT_SharedSongsTest_SetEponaFile(int rando, int skip);
int OoT_SharedSongsTest_EponaObtained(void);
void OoT_ConsumeSharedItems(void);
void OoT_ApplySharedResourcesAtFileLoad(void);
void OoT_SharedSongOfTimeTest_SetHeld(int held);
int OoT_SharedSongOfTimeTest_Held(void);
void MM_SharedSongOfTimeTest_SetHeld(int held);
int MM_SharedSongOfTimeTest_Held(void);
int OoT_SharedWarpSongsTest_PoolId(int song);
int OoT_SharedWarpSongsTest_LinksPocket(void);
int OoT_SharedWarpSongsTest_UnsharedSongId(void);
int MM_SharedWarpSongsTest_PoolId(int song);
void MM_SharedWarpSongsTest_SetTitleScreen(void);
int OoT_SharedSongsTest_OffVanillaCheckId(int song);
int OoT_SharedSongsTest_OffVanillaAtHome(int song);
int OoT_SharedSongsTest_OffLastRoundReached(int song);
int OoT_SharedSongsTest_OffGiveIsJunk(int song);
int OoT_SharedSongsTest_OoTHintKey(void);
int OoT_SharedSongsTest_OffHintProbeSlot(int song);
int OoT_SharedSongsTest_MajorCategorySlot(void);
int OoT_SharedSongsTest_JunkCategorySlot(void);
int MM_SharedSongsTest_OffVanillaCheckId(int song);
int MM_SharedSongsTest_OffConvertSong(int song, int mmShuffleSongs);
uint16_t MM_SharedSongsTest_UnsharedSongId(void);
int MM_ComboLogic_TestFixedCheckItem(uint16_t rc);
int OoT_Fill_TestSongHeldForBag(uint16_t rg);
int OoT_Creation_FinishPairedHalf(int writeSpoiler);
int OoT_Combo_SetHintingSuspendedWorld(int on);
int OoT_TrackerAdapter_TestSetDeparted(int rando);
int Rando_TestOoTHintRevealText(char* outText, int textCap, char* outExpected, int expectedCap, int* outNamesSongCheck,
                                int* outCoverNamed);
int RandoTest_HintTrackerSetRead(int hintKey, int read);
int RandoTest_HintTrackerPaneRow(int hintKey, int* outCount, uint32_t* outJournalKey, char* outText, int textCap);
#ifdef __cplusplus
}
#endif

static int sSsFailures;

#define SS_CHECK(cond, ...)                           \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sSsFailures++;                            \
        }                                             \
    } while (0)

namespace {

/** One shared song: its rule bit, spelled as the issue allocates it (#1066,
 *  literals so the lock states the .redsave format rather than reading it back
 *  from the header under test), its key's suffix, its row label, OoTMM's name
 *  for the rule, and each game's display name for its copy. */
struct SsSong {
    uint32_t bit;
    const char* keySuffix;
    const char* label;
    const char* ruleName;
    const char* itemName;
};

const SsSong kSsSongs[2] = {
    { 0x00400000u, "SharedSongEpona", "Shared Epona's Song", "sharedSongEpona", "Epona's Song" },
    { 0x00800000u, "SharedSongStorms", "Shared Song of Storms", "sharedSongStorms", "Song of Storms" },
};
const uint32_t kSsBoth = 0x00C00000u;
/** A bit no build allocates yet: a later build's rule, which no key restores. */
const uint32_t kSsLaterBuildBit = 0x40000000u;
const uint32_t kSsSettingsHash = 0xDEADBEEFu;
const uint32_t kSsProfileDigest = 0x0BADF00Du;
const char* const kSsLoadDir = "rsbs_test_shared_songs_load";

std::string SsKey(int song) {
    return std::string(RSBS::kComboIdentityKeyPrefix) + kSsSongs[song].keySuffix;
}

/** The authoring id whose key is @p song's rule, or COMBO_SETTING_COUNT. */
ComboSettingId SsFindSetting(int song) {
    const std::string key = SsKey(song);
    for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
        if (key == Combo_ComboSettingKey((ComboSettingId)i)) {
            return (ComboSettingId)i;
        }
    }
    return COMBO_SETTING_COUNT;
}

ComboItemRulesRecord SsRules(uint32_t flags) {
    ComboItemRulesRecord r;
    memset(&r, 0, sizeof(r));
    r.crossItemFlags = flags;
    return r;
}

/** Freeze a paired world with the shipped combo record and @p rules, the way
 *  Playthrough_Init stamps one (test_shared_lens.c's SlFreezeWorld). */
void SsFreezeWorld(const ComboItemRulesRecord* rules) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x1066F00Du;
    gComboCtx.sharedRandoSettingsHash = kSsSettingsHash;
    gComboCtx.mmProfileDigest = kSsProfileDigest;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    Combo_FreezeComboSettingsEx(&rec, rules);
}

void SsSeedShadows(uint8_t fill) {
    Context_InitFrozenStates();
    std::vector<uint8_t> oot(OOT_SAVE_CONTEXT_SIZE, fill);
    std::vector<uint8_t> mm(MM_SAVE_CONTEXT_SIZE, (uint8_t)(fill ^ 0xFFu));
    Context_UpdateShadowCopy(GAME_OOT, oot.data(), oot.size());
    Context_UpdateShadowCopy(GAME_MM, mm.data(), mm.size());
}

void SsClearAllKeys() {
    for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
        CVarClear(Combo_ComboSettingKey((ComboSettingId)i));
    }
    for (int s = 0; s < 2; s++) {
        CVarClear(SsKey(s).c_str());
    }
}

/** Both games forget both songs, then arm their live saves. */
void SsForgetAll() {
    OoT_SharedSwordTest_ArmLive();
    MM_SharedSwordTest_ArmLive();
    for (int s = 0; s < 2; s++) {
        OoT_SharedSongsTest_SetHeld(s, 0);
        MM_SharedSongsTest_SetHeld(s, 0);
    }
}

} // namespace

// ============================================================================
// shared-songs-rule: the bits, the keys, the freeze, the fingerprint, the restore
// ============================================================================

TestResult SharedSongsRule_Run(void) {
    printf("[TEST] shared-songs-rule: Shared Epona's Song and Shared Song of Storms are two Cross-Game Rules, off by "
           "default, frozen at creation into the fingerprint, and restored from the file at load (#1066)\n");
    sSsFailures = 0;
    if (!Combo_ComboSettingStoreAvailable()) {
        printf("[TEST] FAIL: no CVar store: every authoring leg below would pass vacuously on the defaults\n");
        return TEST_FAIL;
    }
    ComboContext_Init();
    SsClearAllKeys();

    // ---- (1) The bits: allocated, families, named, requiring nothing ---------
    for (int s = 0; s < 2; s++) {
        const uint32_t bit = kSsSongs[s].bit;
        SS_CHECK((RSBS_XITEM_ALL_V1 & bit) == bit,
                 "bit %08X is not allocated (RSBS_XITEM_ALL_V1 %08X): a file with the rule on would be refused as a "
                 "later build's",
                 (unsigned)bit, (unsigned)RSBS_XITEM_ALL_V1);
        SS_CHECK((RSBS_XITEM_FAMILY_MASK & bit) == bit,
                 "bit %08X is not a shared family (RSBS_XITEM_FAMILY_MASK %08X): nothing would merge or pair it",
                 (unsigned)bit, (unsigned)RSBS_XITEM_FAMILY_MASK);
        SS_CHECK(strcmp(Combo_ComboItemRuleName(bit), kSsSongs[s].ruleName) == 0,
                 "bit %08X is named '%s', expected OoTMM's '%s'", (unsigned)bit, Combo_ComboItemRuleName(bit),
                 kSsSongs[s].ruleName);
        SS_CHECK(Combo_ComboItemRuleRequires(bit) == 0u, "%s requires no other rule", kSsSongs[s].ruleName);
        const ComboItemRulesRecord on = SsRules(bit);
        SS_CHECK(Combo_ComboItemRulesValid(&on), "a record with only %s on must be a valid world",
                 kSsSongs[s].ruleName);
    }
    {
        const ComboItemRulesRecord both = SsRules(kSsBoth);
        SS_CHECK(Combo_ComboItemRulesValid(&both), "a record with both shared songs on must be a valid world");
    }

    // ---- (2) The keys: one Cross-Game Rules row each, off by default, 0 or 1 -
    ComboSettingId ids[2];
    for (int s = 0; s < 2; s++) {
        ids[s] = SsFindSetting(s);
        if (ids[s] == COMBO_SETTING_COUNT) {
            printf("[TEST] FAIL: no combo setting authors '%s': the rule has no Cross-Game Rules row and no player "
                   "can turn it on (%s:%d)\n",
                   SsKey(s).c_str(), __FILE__, __LINE__);
            printf("[TEST] shared-songs-rule: FAIL\n");
            return TEST_FAIL;
        }
        SS_CHECK(strcmp(Combo_ComboSettingLabel(ids[s]), kSsSongs[s].label) == 0,
                 "the rule's row is labelled '%s', expected '%s'", Combo_ComboSettingLabel(ids[s]), kSsSongs[s].label);
        SS_CHECK(Combo_ComboSettingDefault(ids[s]) == 0, "%s must ship OFF (OoTMM's own default)",
                 kSsSongs[s].ruleName);
        SS_CHECK(Combo_ComboSettingValueValid(ids[s], 0) && Combo_ComboSettingValueValid(ids[s], 1) &&
                     !Combo_ComboSettingValueValid(ids[s], 2) && !Combo_ComboSettingValueValid(ids[s], -1),
                 "%s's value space must be exactly 0 or 1", kSsSongs[s].ruleName);
    }
    SS_CHECK(ids[0] != ids[1], "the two songs must be two rows, not one");

    // ---- (3) The resolver: each key its own bit, and nothing else ------------
    ComboSettingsRecord defaults;
    Combo_ComboSettingsDefaults(&defaults);
    {
        ComboItemRulesRecord r;
        Combo_ResolveComboItemRules(&r);
        SS_CHECK(Combo_ComboItemRulesClear(&r), "with no key set the resolved rules must be every rule off (%08X)",
                 (unsigned)r.crossItemFlags);
        for (int s = 0; s < 2; s++) {
            SS_CHECK(Combo_ComboSettingSet(ids[s], 1) == 1, "%s must be authorable before a world exists",
                     kSsSongs[s].ruleName);
            Combo_ResolveComboItemRules(&r);
            SS_CHECK(r.crossItemFlags == kSsSongs[s].bit && r.blastMaskCooldown == 0u && r.spare[0] == 0u &&
                         r.spare[1] == 0u && r.spare[2] == 0u,
                     "%s's key alone resolves to rules %08X, expected exactly %08X", kSsSongs[s].ruleName,
                     (unsigned)r.crossItemFlags, (unsigned)kSsSongs[s].bit);
            ComboSettingsRecord live;
            Combo_ResolveComboSettings(&live);
            SS_CHECK(memcmp(&live, &defaults, sizeof(live)) == 0,
                     "%s's key moved the combo record itself: it is an item rule, not a combo setting",
                     kSsSongs[s].ruleName);
            SS_CHECK(Combo_ComboSettingSet(ids[s], 0) == 1, "%s must be authorable back off", kSsSongs[s].ruleName);
            Combo_ResolveComboItemRules(&r);
            SS_CHECK(Combo_ComboItemRulesClear(&r), "%s's key explicitly 0 must resolve every rule off (%08X)",
                     kSsSongs[s].ruleName, (unsigned)r.crossItemFlags);
            CVarSetInteger(SsKey(s).c_str(), 2);
            Combo_ResolveComboItemRules(&r);
            SS_CHECK(Combo_ComboItemRulesClear(&r),
                     "an out-of-space stored 2 for %s must resolve to the shipped default (off)", kSsSongs[s].ruleName);
            CVarClear(SsKey(s).c_str());
        }
        SS_CHECK(Combo_ComboSettingSet(ids[0], 1) == 1 && Combo_ComboSettingSet(ids[1], 1) == 1,
                 "both rules must be authorable together");
        Combo_ResolveComboItemRules(&r);
        SS_CHECK(r.crossItemFlags == kSsBoth, "both keys on resolve to rules %08X, expected exactly %08X",
                 (unsigned)r.crossItemFlags, (unsigned)kSsBoth);
        SsClearAllKeys();
    }

    // ---- (4) The creation: frozen, and the fingerprint moves with each -------
    const uint32_t hashOff = Combo_ComputeComboSettingsHashEx(&defaults, NULL, kSsSettingsHash, kSsProfileDigest);
    SS_CHECK(hashOff == Combo_ComputeComboSettingsHash(&defaults, kSsSettingsHash, kSsProfileDigest),
             "the rules off must leave the fingerprint every world had before they existed");
    uint32_t hashOne[2] = { 0u, 0u };
    for (int s = 0; s < 2; s++) {
        const ComboItemRulesRecord on = SsRules(kSsSongs[s].bit);
        hashOne[s] = Combo_ComputeComboSettingsHashEx(&defaults, &on, kSsSettingsHash, kSsProfileDigest);
        SS_CHECK(hashOne[s] != hashOff, "%s did not move the fingerprint: a world rule outside its identity",
                 kSsSongs[s].ruleName);
    }
    SS_CHECK(hashOne[0] != hashOne[1], "the two songs' rules must be two different worlds");
    {
        ComboContext_Init();
        ComboItemRulesRecord resolved;
        Combo_ResolveComboItemRules(&resolved);
        SsFreezeWorld(&resolved);
        SS_CHECK((gComboCtx.comboItemRules.crossItemFlags & kSsBoth) == 0u && gComboCtx.comboSettingsHash == hashOff,
                 "with the keys unset the creation froze rules %08X and fingerprint %08X (expected 0 and %08X)",
                 (unsigned)gComboCtx.comboItemRules.crossItemFlags, (unsigned)gComboCtx.comboSettingsHash,
                 (unsigned)hashOff);
        SS_CHECK((Combo_SharedFamiliesArmed() & kSsBoth) == 0u, "no song family may be armed with the rules off");

        for (int s = 0; s < 2; s++) {
            ComboContext_Init();
            SsClearAllKeys();
            SS_CHECK(Combo_ComboSettingSet(ids[s], 1) == 1, "turn %s on before the creation", kSsSongs[s].ruleName);
            Combo_ResolveComboItemRules(&resolved);
            SsFreezeWorld(&resolved);
            SS_CHECK(Combo_ComboCrossItemRule(kSsSongs[s].bit) && !Combo_ComboCrossItemRule(kSsSongs[1 - s].bit),
                     "the creation froze rules %08X with only %s on", (unsigned)gComboCtx.comboItemRules.crossItemFlags,
                     kSsSongs[s].ruleName);
            SS_CHECK(gComboCtx.comboSettingsHash == hashOne[s],
                     "the stamped fingerprint is %08X, expected %08X: the freeze must fold %s",
                     (unsigned)gComboCtx.comboSettingsHash, (unsigned)hashOne[s], kSsSongs[s].ruleName);
            SS_CHECK((Combo_SharedFamiliesArmed() & kSsBoth) == kSsSongs[s].bit,
                     "the frozen %s arms families %08X, expected only its own", kSsSongs[s].ruleName,
                     (unsigned)(Combo_SharedFamiliesArmed() & kSsBoth));
            SS_CHECK(Combo_ComboSettingsDivergence() == 0u,
                     "a session whose key still says on must compare healthy (diverged %08X)",
                     (unsigned)Combo_ComboSettingsDivergence());
            SS_CHECK(Combo_ComboSettingSet(ids[s], 0) == 0, "%s's writer must refuse while a world is frozen",
                     kSsSongs[s].ruleName);
        }
        SsClearAllKeys();
        ComboContext_Init();
    }

    // ---- (5) Frozen wins at load: the restore API -----------------------------
    {
        const ComboItemRulesRecord bothOn = SsRules(kSsBoth);
        SsFreezeWorld(&bothOn);
        // The session never turned the rules on: both keys resolve off.
        const uint32_t bits = Combo_ComboSettingsDivergence();
        SS_CHECK(bits == RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS,
                 "a songs-on file met by a songs-off session diverged %08X, expected crossItemFlags alone",
                 (unsigned)bits);
        SS_CHECK(Combo_ComboSettingsCanRestoreLive(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, bits) == 1,
                 "the file-select probe would refuse a songs-on file the keys can restore");
        char names[256] = { 0 };
        ComboSettingsKeyUndo undo;
        const int restored = Combo_ComboSettingsRestoreLive(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, bits,
                                                            names, sizeof(names), &undo);
        printf("[TEST] (5) restore: %d, restored '%s', diverged after %08X\n", restored, names,
               (unsigned)Combo_ComboSettingsDivergence());
        SS_CHECK(restored == 1 && strstr(names, kSsSongs[0].label) != NULL && strstr(names, kSsSongs[1].label) != NULL,
                 "the load restore did not put both of the file's song rules back (returned %d, '%s')", restored,
                 names);
        for (int s = 0; s < 2; s++) {
            int32_t stored = 0;
            SS_CHECK(Combo_ComboSettingReadStore(ids[s], &stored) && stored == 1,
                     "%s's restored key reads %d, expected 1", kSsSongs[s].ruleName, (int)stored);
        }
        SS_CHECK(Combo_ComboSettingsDivergence() == 0u, "after the restore the session must agree with the file");
        Combo_ComboSettingsRestoreUndo(&undo);
        SS_CHECK(!Combo_ComboSettingIsExplicit(ids[0]) && !Combo_ComboSettingIsExplicit(ids[1]) &&
                     Combo_ComboSettingsDivergence() == RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS,
                 "the undo must put both keys back to unset");

        // A later build's bit beside the songs is never restored.
        const ComboItemRulesRecord later = SsRules(kSsBoth | kSsLaterBuildBit);
        SS_CHECK(Combo_ComboSettingsCanRestoreLive(&gComboCtx.comboSettings, &later,
                                                   RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS) == 0,
                 "a frozen rule no key authors must not be claimed restorable");
        ComboContext_Init();
    }

    // ---- (6) Frozen wins at load: a real .redsave ------------------------------
    {
        const ComboItemRulesRecord bothOn = SsRules(kSsBoth);
        rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
        const std::string priorSaveDir = std::filesystem::path(mgr.SlotPath(0)).parent_path().string();
        mgr.SetSaveDirectory(rsbs::test::ScratchDir(kSsLoadDir).string());
        RsbsSave_ResetSlotSessionState();
        mgr.DeleteSave(0);
        ComboContext_Init();
        SsSeedShadows(0x66u);
        SsFreezeWorld(&bothOn);
        SsSeedShadows(0x66u);
        SS_CHECK(mgr.Save(0), "Save(0) failed for the songs-on file");
        RsbsSave_ResetSlotSessionState();
        ComboContext_Init();
        SsClearAllKeys();
        const bool loaded = mgr.Load(0);
        int32_t stored[2] = { 0, 0 };
        const bool keySet[2] = { Combo_ComboSettingReadStore(ids[0], &stored[0]),
                                 Combo_ComboSettingReadStore(ids[1], &stored[1]) };
        printf("[TEST] (6) songs-on file under a songs-off session: loaded %d, keys %d/%d, %d/%d, quarantine %d\n",
               loaded ? 1 : 0, keySet[0] ? 1 : 0, (int)stored[0], keySet[1] ? 1 : 0, (int)stored[1],
               RsbsSave_HasQuarantine(0));
        SS_CHECK(loaded, "a songs-on file was refused under a session that only turned the rules off: frozen wins "
                         "at load (#781), the keys go back to the file's values");
        SS_CHECK(keySet[0] && stored[0] == 1 && keySet[1] && stored[1] == 1,
                 "the load did not restore both of the file's song rules into their keys");
        SS_CHECK(!loaded || (Combo_ComboCrossItemRule(kSsSongs[0].bit) && Combo_ComboCrossItemRule(kSsSongs[1].bit)),
                 "the loaded world does not carry both shared songs");
        SS_CHECK(RsbsSave_HasQuarantine(0) == 0 && RsbsSave_HasSave(0) == 1, "a rule divergence is not damage");

        RsbsSave_ResetSlotSessionState();
        mgr.DeleteSave(0);
        mgr.SetSaveDirectory(priorSaveDir);
        ComboContext_Init();
    }

    SsClearAllKeys();
    ComboContext_Init();
    printf("[TEST] shared-songs-rule: %s\n", sSsFailures == 0 ? "PASS" : "FAIL");
    return sSsFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// shared-songs-merge: both games' songs are the families, through the real shims
// ============================================================================

TestResult Test_SharedSongsMerge(void) {
    printf("[TEST] shared-songs-merge: both games register Epona's Song and the Song of Storms as the shared "
           "families' members, and a song learnt in either game is known in the other at arrival through the real "
           "shims (#1066)\n");
    sSsFailures = 0;

    // ---- (0) The registrations: each game's own song, the id it places --------
    for (int s = 0; s < 2; s++) {
        uint16_t ootId = 0;
        uint16_t mmId = 0;
        const bool ootReg = Combo_SharedFamilyMemberId(GAME_OOT, kSsSongs[s].bit, &ootId);
        const bool mmReg = Combo_SharedFamilyMemberId(GAME_MM, kSsSongs[s].bit, &mmId);
        printf("[TEST] (0) %s registered: OoT %d (id %u, pool %d), MM %d (id %u, pool %d)\n", kSsSongs[s].ruleName,
               ootReg ? 1 : 0, (unsigned)ootId, OoT_SharedSongsTest_PoolId(s), mmReg ? 1 : 0, (unsigned)mmId,
               MM_SharedSongsTest_PoolId(s));
        SS_CHECK(ootReg && (int)ootId == OoT_SharedSongsTest_PoolId(s),
                 "OoT's %s is not registered as the %s family's OoT member (the bag pair and the arrival give both "
                 "read this registration)",
                 kSsSongs[s].itemName, kSsSongs[s].ruleName);
        SS_CHECK(mmReg && (int)mmId == MM_SharedSongsTest_PoolId(s),
                 "MM's %s is not registered as the %s family's MM member", kSsSongs[s].itemName, kSsSongs[s].ruleName);
    }

    const ComboItemRulesRecord bothOn = SsRules(kSsBoth);
    for (int s = 0; s < 2; s++) {
        // ---- (1) Learnt in Hyrule, known in Termina --------------------------
        SsFreezeWorld(&bothOn);
        SsForgetAll();
        OoT_SharedSwordTest_ArmLive();
        OoT_SharedSongsTest_SetHeld(s, 1);
        OoT_HarvestSharedResources();
        const uint32_t afterOoT = Combo_SharedFamiliesOwned();
        MM_SharedSwordTest_ArmLive();
        MM_ApplySharedResources();
        printf("[TEST] (1) %s OoT -> MM: pool %08X, MM knows it %d, the other song %d\n", kSsSongs[s].ruleName,
               (unsigned)afterOoT, MM_SharedSongsTest_Held(s), MM_SharedSongsTest_Held(1 - s));
        SS_CHECK((afterOoT & kSsBoth) == kSsSongs[s].bit,
                 "OoT's harvest shim put pool %08X for %s, expected exactly its own family", (unsigned)afterOoT,
                 kSsSongs[s].itemName);
        SS_CHECK(MM_SharedSongsTest_Held(s) == 1, "MM's apply shim did not teach MM %s", kSsSongs[s].itemName);
        SS_CHECK(MM_SharedSongsTest_Held(1 - s) == 0, "learning %s in Hyrule taught MM %s too", kSsSongs[s].itemName,
                 kSsSongs[1 - s].itemName);
        MM_ApplySharedResources();
        SS_CHECK(MM_SharedSongsTest_Held(s) == 1, "a second MM arrival lost %s", kSsSongs[s].itemName);

        // ---- (2) Learnt in Termina, known in Hyrule --------------------------
        SsFreezeWorld(&bothOn);
        SsForgetAll();
        MM_SharedSwordTest_ArmLive();
        MM_SharedSongsTest_SetHeld(s, 1);
        MM_HarvestSharedResources();
        OoT_SharedSwordTest_ArmLive();
        OoT_ApplySharedResources();
        printf("[TEST] (2) %s MM -> OoT: pool %08X, OoT knows it %d, the other song %d\n", kSsSongs[s].ruleName,
               (unsigned)Combo_SharedFamiliesOwned(), OoT_SharedSongsTest_Held(s), OoT_SharedSongsTest_Held(1 - s));
        SS_CHECK((Combo_SharedFamiliesOwned() & kSsBoth) == kSsSongs[s].bit,
                 "MM's harvest shim did not put MM's %s alone into the shared pool", kSsSongs[s].itemName);
        SS_CHECK(OoT_SharedSongsTest_Held(s) == 1, "OoT's apply shim did not teach OoT %s", kSsSongs[s].itemName);
        SS_CHECK(OoT_SharedSongsTest_Held(1 - s) == 0, "learning %s in Termina taught OoT %s too", kSsSongs[s].itemName,
                 kSsSongs[1 - s].itemName);

        // ---- (3) Only this song's rule on: the other song is not carried -----
        const ComboItemRulesRecord otherOnly = SsRules(kSsSongs[1 - s].bit);
        SsFreezeWorld(&otherOnly);
        SsForgetAll();
        OoT_SharedSwordTest_ArmLive();
        OoT_SharedSongsTest_SetHeld(s, 1);
        OoT_HarvestSharedResources();
        MM_SharedSwordTest_ArmLive();
        MM_ApplySharedResources();
        SS_CHECK((Combo_SharedFamiliesOwned() & kSsSongs[s].bit) == 0u && MM_SharedSongsTest_Held(s) == 0,
                 "with only %s's rule on, %s learnt in Hyrule reached Termina (pool %08X)", kSsSongs[1 - s].ruleName,
                 kSsSongs[s].itemName, (unsigned)Combo_SharedFamiliesOwned());
    }

    // ---- (4) The rules off: each game keeps its own songs --------------------
    SsFreezeWorld(NULL);
    SsForgetAll();
    OoT_SharedSwordTest_ArmLive();
    OoT_SharedSongsTest_SetHeld(0, 1);
    OoT_SharedSongsTest_SetHeld(1, 1);
    OoT_HarvestSharedResources();
    MM_SharedSwordTest_ArmLive();
    MM_ApplySharedResources();
    SS_CHECK(Combo_SharedFamiliesOwned() == 0u && MM_SharedSongsTest_Held(0) == 0 && MM_SharedSongsTest_Held(1) == 0,
             "with the rules off a song learnt in Hyrule reached Termina (pool %08X)",
             (unsigned)Combo_SharedFamiliesOwned());

    // ---- (5) Skip Epona Race: knowing Epona's Song is having Epona (#1351) ---
    // Every summon reads EVENTCHKINF_EPONA_OBTAINED. Since #1327 only OoT's
    // item-receive handler set it, and only for a queued check's give, so a song
    // that reached OoT any other way never called Epona.
    OoT_SharedSongsTest_EponaBegin();
    {
        // (5a) Shared Epona's Song learnt in Termina: OoT's arrival apply teaches
        // the song and, through the family's give, frees Epona.
        SsFreezeWorld(&bothOn);
        SsForgetAll();
        MM_SharedSwordTest_ArmLive();
        MM_SharedSongsTest_SetHeld(0, 1);
        MM_HarvestSharedResources();
        OoT_SharedSwordTest_ArmLive();
        OoT_SharedSongsTest_SetEponaFile(1, 1);
        OoT_ApplySharedResources();
        printf("[TEST] (5a) Skip Epona Race, Epona's Song learnt in Termina: OoT knows it %d, Epona %d\n",
               OoT_SharedSongsTest_Held(0), OoT_SharedSongsTest_EponaObtained());
        SS_CHECK(OoT_SharedSongsTest_Held(0) == 1 && OoT_SharedSongsTest_EponaObtained() == 1,
                 "under Skip Epona Race the shared Epona's Song reached OoT without Epona: the song cannot summon her");

        // (5b) The song already known with no family carrying it (Start with
        // Epona's Song, or an OoT Epona's Song redeemed from an MM check): the
        // arrival's consume frees Epona, and so does the load seam.
        SsFreezeWorld(NULL);
        SsForgetAll();
        OoT_SharedSwordTest_ArmLive();
        OoT_SharedSongsTest_SetEponaFile(1, 1);
        OoT_SharedSongsTest_SetHeld(0, 1);
        const int before = OoT_SharedSongsTest_EponaObtained();
        OoT_ConsumeSharedItems();
        const int atArrival = OoT_SharedSongsTest_EponaObtained();
        OoT_SharedSongsTest_SetEponaFile(1, 1);
        OoT_ApplySharedResourcesAtFileLoad();
        const int atLoad = OoT_SharedSongsTest_EponaObtained();
        printf("[TEST] (5b) Skip Epona Race, Epona's Song already known: Epona before %d, after the arrival %d, after "
               "the load %d\n",
               before, atArrival, atLoad);
        SS_CHECK(before == 0, "the leg did not start without Epona: the two checks below would pass vacuously");
        SS_CHECK(atArrival == 1, "an OoT arrival with Epona's Song known under Skip Epona Race did not free Epona");
        SS_CHECK(atLoad == 1, "a file load with Epona's Song known under Skip Epona Race did not free Epona");

        // (5c) Nothing else frees her: Don't Skip with the song, Skip without it,
        // Skip with only the Song of Storms, and a vanilla file with the song.
        struct EponaNotOwed {
            const char* what;
            int rando;
            int skip;
            int song; // -1 none, else OoT_SharedSongsTest_SetHeld's song
        };
        const EponaNotOwed notOwed[] = {
            { "Don't Skip with Epona's Song", 1, 0, 0 },
            { "Skip Epona Race without Epona's Song", 1, 1, -1 },
            { "Skip Epona Race with only the Song of Storms", 1, 1, 1 },
            { "a vanilla file with Epona's Song", 0, 1, 0 },
        };
        for (const EponaNotOwed& c : notOwed) {
            SsFreezeWorld(NULL);
            SsForgetAll();
            OoT_SharedSwordTest_ArmLive();
            OoT_SharedSongsTest_SetEponaFile(c.rando, c.skip);
            if (c.song >= 0) {
                OoT_SharedSongsTest_SetHeld(c.song, 1);
            }
            OoT_ConsumeSharedItems();
            OoT_ApplySharedResourcesAtFileLoad();
            SS_CHECK(OoT_SharedSongsTest_EponaObtained() == 0, "%s freed Epona: only the race does that there", c.what);
        }
    }
    OoT_SharedSongsTest_EponaEnd();

    SsForgetAll();
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    printf("[TEST] shared-songs-merge: %s\n", sSsFailures == 0 ? "PASS" : "FAIL");
    return sSsFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// shared-songs-bag: one row per song both solvers credit, over a real generation
// ============================================================================

namespace {
/** OoT's song shuffle for this row: anywhere (RO_SONG_SHUFFLE_ANYWHERE), so
 *  OoT's songs are general-pass bag rows like MM's and each pair has both
 *  halves. Under every other value OoT holds its shared copy back from its
 *  restricted song placement for the bag (#1085), so the pair has both halves
 *  there too; the shared-songs-one-copy-* rows cover those values. */
const char* const kSsSongShuffleKey = "gRandoSettings.ShuffleSongs";
const int kSsSongShuffleAnywhere = 3;
} // namespace

TestResult SharedSongsBag_Run(void) {
    printf("[TEST] shared-songs-bag: with both rules on the single bag holds ONE Epona's Song and ONE Song of Storms "
           "that both solvers credit, each placed once over both games, and the GOAL is still proven (#1066)\n");
    sSsFailures = 0;
    ComboContext_Init();
    SsClearAllKeys();
    CVarSetInteger(SsKey(0).c_str(), 1);
    CVarSetInteger(SsKey(1).c_str(), 1);
    const int priorShuffle = CVarGetInteger(kSsSongShuffleKey, -1);
    CVarSetInteger(kSsSongShuffleKey, kSsSongShuffleAnywhere);
    struct Restore {
        int prior;
        ~Restore() {
            if (prior < 0) {
                CVarClear(kSsSongShuffleKey);
            } else {
                CVarSetInteger(kSsSongShuffleKey, prior);
            }
            CVarClear(SsKey(0).c_str());
            CVarClear(SsKey(1).c_str());
        }
    } restore{ priorShuffle };

    const int gen = Rando_HeadlessSeedTest("RSBSSHAREDSONGS1");
    SS_CHECK(gen == 0, "the paired OoT generation failed");
    SS_CHECK(Combo_ComboSettingsFrozen() && Combo_ComboCrossItemRule(kSsSongs[0].bit) &&
                 Combo_ComboCrossItemRule(kSsSongs[1].bit),
             "the creation did not freeze both shared songs from the keys (rules %08X)",
             (unsigned)gComboCtx.comboItemRules.crossItemFlags);
    const int mmHalf = (gen == 0) ? MM_Rando_HeadlessPairedHalf() : -1;
    SS_CHECK(mmHalf == 0, "MM's creation-time half did not produce a rando world");
    if (gen != 0 || mmHalf != 0) {
        printf("[TEST] shared-songs-bag: FAIL\n");
        return TEST_FAIL;
    }
    const ComboSingleBagReport bag = *Combo_SingleBag_LastReport();
    printf("[TEST] shared-songs-bag: bag %d rows, %d shared pair(s), %d placed, goal proven %d\n", bag.bagCount,
           bag.compose.sharedPairs, bag.fill.placed, bag.fill.goalProven ? 1 : 0);
    SS_CHECK(bag.status == RSBS_COMBO_LOGIC_OK && bag.fill.goalProven,
             "the single-bag fill did not prove the GOAL with the shared songs");
    SS_CHECK(bag.compose.sharedPairs == 2,
             "the composer made %d shared pair(s), expected exactly the two songs: the games' copies were not paired",
             bag.compose.sharedPairs);

    const int shuffledTotal = MM_ComboLogic_TestShuffledItems(nullptr, nullptr, 0);
    std::vector<uint16_t> items((size_t)(shuffledTotal > 0 ? shuffledTotal : 1));
    const int shuffled = MM_ComboLogic_TestShuffledItems(items.data(), nullptr, (int)items.size());

    for (int s = 0; s < 2; s++) {
        uint16_t ootId = 0;
        uint16_t mmId = 0;
        SS_CHECK(Combo_SharedFamilyMemberId(GAME_OOT, kSsSongs[s].bit, &ootId) &&
                     Combo_SharedFamilyMemberId(GAME_MM, kSsSongs[s].bit, &mmId),
                 "the %s family is not registered in both games", kSsSongs[s].ruleName);
        SharedItem ootSong;
        memset(&ootSong, 0, sizeof(ootSong));
        ootSong.originGame = (uint8_t)GAME_OOT;
        ootSong.id = ootId;
        SharedItem mmSong;
        memset(&mmSong, 0, sizeof(mmSong));
        mmSong.originGame = (uint8_t)GAME_MM;
        mmSong.id = mmId;
        const char* ootName = Combo_DescribeItemName(ootSong);
        const char* mmName = Combo_DescribeItemName(mmSong);
        SS_CHECK(ootName != NULL && strcmp(ootName, kSsSongs[s].itemName) == 0 && mmName != NULL &&
                     strcmp(mmName, kSsSongs[s].itemName) == 0,
                 "the registered pair names '%s' and '%s', expected each game's %s",
                 ootName != NULL ? ootName : "(null)", mmName != NULL ? mmName : "(null)", kSsSongs[s].itemName);

        // Exactly one placement of either copy over both games, and it is the pair.
        int copies = 0;
        int pairedCopies = 0;
        GameId hostOfCopy = GAME_NONE;
        for (GameId host : { GAME_OOT, GAME_MM }) {
            ComboLogicPlacement p;
            for (int i = 0; i < Combo_Logic_PlacementCount(host); i++) {
                if (!Combo_Logic_PlacementAt(host, i, &p)) {
                    continue;
                }
                const bool isOoT = p.item.originGame == (uint8_t)GAME_OOT && p.item.id == ootId;
                const bool isMM = p.item.originGame == (uint8_t)GAME_MM && p.item.id == mmId;
                if (!isOoT && !isMM) {
                    continue;
                }
                copies++;
                hostOfCopy = host;
                const SharedItem farHalf = isOoT ? mmSong : ootSong;
                const bool ownHalf = (host == GAME_OOT) ? isOoT : isMM;
                if (ownHalf && p.pair.originGame == farHalf.originGame && p.pair.id == farHalf.id) {
                    pairedCopies++;
                }
                printf("[TEST] shared-songs-bag: %s on %s check %u (%s half, pair %u:%u)\n", kSsSongs[s].itemName,
                       Game_ToString(host), (unsigned)p.hostCheck, isOoT ? "OoT" : "MM", (unsigned)p.pair.originGame,
                       (unsigned)p.pair.id);
            }
        }
        SS_CHECK(copies == 1 && pairedCopies == 1,
                 "the coordinator placed %d %s(s), %d of them as the shared pair: expected ONE row, the host's own "
                 "half with the other game's beside it",
                 copies, kSsSongs[s].itemName, pairedCopies);

        // MM's own world: a second copy would be the trimmed half back in play.
        int mmChecks = 0;
        for (int i = 0; i < shuffled && i < (int)items.size(); i++) {
            if (items[(size_t)i] == mmId) {
                mmChecks++;
            }
        }
        const int expectMM = (hostOfCopy == GAME_MM) ? 1 : 0;
        printf("[TEST] shared-songs-bag: MM's world holds %d %s check(s) (expected %d)\n", mmChecks,
               kSsSongs[s].itemName, expectMM);
        SS_CHECK(shuffled > 0 && mmChecks == expectMM,
                 "MM's world holds %d %s check(s), expected %d: the trimmed half is still the song", mmChecks,
                 kSsSongs[s].itemName, expectMM);
    }

    printf("[TEST] shared-songs-bag: %s\n", sSsFailures == 0 ? "PASS" : "FAIL");
    return sSsFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// shared-songs-one-copy-*: ONE copy of each shared song, whatever Shuffle Songs
// says (#1085; OoTMM sharedSongEpona, sharedSongStorms, sharedSongTime)
// ============================================================================

namespace {
/** OoT's Shuffle Songs values (RO_SONG_SHUFFLE_*), by the CVar's integer. */
const char* const kSsShuffleNames[4] = { "Off", "Song Locations", "Dungeon Rewards", "Anywhere" };
/** The three shared-song rules: #1066's two and #1069's Song of Time, spelled as
 *  literals like kSsSongs (the key from the identity prefix). */
struct SsOneCopySong {
    uint32_t bit;
    const char* keySuffix;
    const char* itemName;
};
const SsOneCopySong kSsOneCopySongs[3] = {
    { 0x00400000u, "SharedSongEpona", "Epona's Song" },
    { 0x00800000u, "SharedSongStorms", "Song of Storms" },
    { 0x04000000u, "SharedSongTime", "Song of Time" },
};
const uint32_t kSsAllThreeSongsMask = 0x04C00000u;

static std::string SsOneCopyKey(int song) {
    return std::string(RSBS::kComboIdentityKeyPrefix) + kSsOneCopySongs[song].keySuffix;
}

static void SsSetThreeSongHeldOoT(int song, int held) {
    if (song == 2) {
        OoT_SharedSongOfTimeTest_SetHeld(held);
    } else {
        OoT_SharedSongsTest_SetHeld(song, held);
    }
}

static void SsSetThreeSongHeldMM(int song, int held) {
    if (song == 2) {
        MM_SharedSongOfTimeTest_SetHeld(held);
    } else {
        MM_SharedSongsTest_SetHeld(song, held);
    }
}

static void SsResetAllThreeSongsHeld(void) {
    OoT_SharedSwordTest_ArmLive();
    MM_SharedSwordTest_ArmLive();
    for (int s = 0; s < 3; s++) {
        SsSetThreeSongHeldOoT(s, 0);
        SsSetThreeSongHeldMM(s, 0);
    }
    gComboCtx.sharedFamilyOwned = 0u;
    gComboCtx.sharedFamilyOwned2 = 0u;
}

static int SsOffRunRound(const ComboLogicHostRef* forbidden, int forbiddenCount, ComboLogicRoundResult* out,
                         int* outDelivered) {
    ComboLogicRoundRequest req;
    memset(&req, 0, sizeof(req));
    req.goal = gComboCtx.comboSettings.goal;
    req.forbidden = forbidden;
    req.forbiddenCount = forbiddenCount;
    memset(out, 0, sizeof(*out));
    const int st = Combo_Logic_RunRound(&req, out);
    *outDelivered = Combo_Logic_FixedCreditDeliveredCount();
    return st;
}

/** #1561: On the created Off world (with sPlacements intact), verify that every
 *  armed shared song's TWO fixed credits let either default check alone satisfy
 *  both games' logic, while forbidding BOTH default checks cuts off song-gated
 *  checks in BOTH games. */
static void SsOffVerifyExitRounds(const uint16_t ootHosts[3], const uint16_t mmHosts[3]) {
    ComboLogicRoundResult base;
    int baseDelivered = -1;
    const int st = SsOffRunRound(nullptr, 0, &base, &baseDelivered);
    printf("[TEST] (Off) exit round: status %d, goal %d, allHostsReached %d, delivered %d of %d\n", st,
           base.goalExpression, base.allHostsReached, baseDelivered, Combo_Logic_FixedCreditCount());
    SS_CHECK(st == RSBS_COMBO_LOGIC_OK && base.goalExpression == 1 && base.allHostsReached == 1 && baseDelivered == 6,
             "(Off) baseline exit round failed (status %d, goal %d, allHosts %d, delivered %d; want 0, 1, 1, 6)", st,
             base.goalExpression, base.allHostsReached, baseDelivered);
    for (int s = 0; s < 3; s++) {
        SS_CHECK(OoT_SharedSongsTest_OffLastRoundReached(s) == 1,
                 "(Off) baseline exit round did not reach OoT's gated check for %s", kSsOneCopySongs[s].itemName);

        const ComboLogicHostRef forbidOoT[1] = { { (uint8_t)GAME_OOT, ootHosts[s] } };
        ComboLogicRoundResult rOoT;
        int delOoT = -1;
        const int stOoT = SsOffRunRound(forbidOoT, 1, &rOoT, &delOoT);
        SS_CHECK(stOoT == RSBS_COMBO_LOGIC_OK && delOoT == 5 && rOoT.goalExpression == 1 && rOoT.allHostsReached == 1 &&
                     OoT_SharedSongsTest_OffLastRoundReached(s) == 1,
                 "(Off) forbidding OoT's default check %u for %s broke reachability (status %d, delivered %d, goal %d, "
                 "allHosts %d, OoT gated %d): MM's fixed credit did not satisfy OoT",
                 (unsigned)ootHosts[s], kSsOneCopySongs[s].itemName, stOoT, delOoT, rOoT.goalExpression,
                 rOoT.allHostsReached, OoT_SharedSongsTest_OffLastRoundReached(s));

        const ComboLogicHostRef forbidMM[1] = { { (uint8_t)GAME_MM, mmHosts[s] } };
        ComboLogicRoundResult rMM;
        int delMM = -1;
        const int stMM = SsOffRunRound(forbidMM, 1, &rMM, &delMM);
        SS_CHECK(stMM == RSBS_COMBO_LOGIC_OK && delMM == 5 && rMM.goalExpression == 1 && rMM.allHostsReached == 1 &&
                     OoT_SharedSongsTest_OffLastRoundReached(s) == 1,
                 "(Off) forbidding MM's default check %u for %s broke reachability (status %d, delivered %d, goal %d, "
                 "allHosts %d, OoT gated %d): OoT's fixed credit did not satisfy MM",
                 (unsigned)mmHosts[s], kSsOneCopySongs[s].itemName, stMM, delMM, rMM.goalExpression,
                 rMM.allHostsReached, OoT_SharedSongsTest_OffLastRoundReached(s));

        const ComboLogicHostRef forbidBoth[2] = {
            { (uint8_t)GAME_OOT, ootHosts[s] },
            { (uint8_t)GAME_MM, mmHosts[s] },
        };
        ComboLogicRoundResult rBoth;
        int delBoth = -1;
        const int stBoth = SsOffRunRound(forbidBoth, 2, &rBoth, &delBoth);
        const int ootGatedBoth = OoT_SharedSongsTest_OffLastRoundReached(s);
        printf("[TEST] (Off) %s both default checks (%u, %u) forbidden: status %d, delivered %d, allHosts %d, OoT "
               "gated %d\n",
               kSsOneCopySongs[s].itemName, (unsigned)ootHosts[s], (unsigned)mmHosts[s], stBoth, delBoth,
               rBoth.allHostsReached, ootGatedBoth);
        SS_CHECK(stBoth == RSBS_COMBO_LOGIC_OK && delBoth <= 4 && ootGatedBoth == 0 && rBoth.allHostsReached == 0,
                 "(Off) forbidding BOTH default checks for %s still reached song-gated checks (delivered %d, OoT "
                 "gated %d, allHosts %d)",
                 kSsOneCopySongs[s].itemName, delBoth, ootGatedBoth, rBoth.allHostsReached);
    }
}

/** #1561: After OoT_Creation_FinishPairedHalf(0) builds hints on the Off world,
 *  verify runtime junk conversion in BOTH pickup orders and verify that neither
 *  RH_OOT_HINT nor the Hint Tracker promises a song once the other game owns it. */
static void SsOffVerifyRuntimeGiveAndHints(void) {
    const GameId priorGame = Context_GetCurrentGame();
    for (int s = 0; s < 3; s++) {
        SsResetAllThreeSongsHeld();
        Context_SetCurrentGame(GAME_NONE);
        SS_CHECK(OoT_SharedSongsTest_OffGiveIsJunk(s) == 0 && MM_SharedSongsTest_OffConvertSong(s, -1) == 0,
                 "(Off) %s converted to junk before either game learned it", kSsOneCopySongs[s].itemName);

        // Order A: learned in Hyrule first -> Termina's default check converts to junk.
        OoT_SharedSwordTest_ArmLive();
        SsSetThreeSongHeldOoT(s, 1);
        OoT_HarvestSharedResources();
        MM_SharedSwordTest_ArmLive();
        SS_CHECK(MM_SharedSongsTest_OffConvertSong(s, -1) == 1 && OoT_SharedSongsTest_OffGiveIsJunk(s) == 1,
                 "(Off) %s learned in OoT did not convert MM's default check to junk", kSsOneCopySongs[s].itemName);

        // Order B: learned in Termina first -> Hyrule's default check converts to junk.
        SsResetAllThreeSongsHeld();
        MM_SharedSwordTest_ArmLive();
        SsSetThreeSongHeldMM(s, 1);
        MM_HarvestSharedResources();
        OoT_SharedSwordTest_ArmLive();
        SS_CHECK(OoT_SharedSongsTest_OffGiveIsJunk(s) == 1 && MM_SharedSongsTest_OffConvertSong(s, -1) == 1,
                 "(Off) %s learned in MM did not convert OoT's default check to junk", kSsOneCopySongs[s].itemName);
    }

    // Hint & Hint Tracker lock on RC_SONG_FROM_OCARINA_OF_TIME (RH_OOT_HINT).
    SsResetAllThreeSongsHeld();
    const int ootHintKey = OoT_SharedSongsTest_OoTHintKey();
    const int wasSuspended = OoT_Combo_SetHintingSuspendedWorld(1);
    (void)OoT_TrackerAdapter_TestSetDeparted(1);
    const int wasRead = RandoTest_HintTrackerSetRead(ootHintKey, 1);
    Context_SetCurrentGame(GAME_MM);

    char revealBuf[1024] = { 0 };
    char revealExpected[256] = { 0 };
    int namesSongCheck = 0;
    int coverNamed = 0;
    const int enabledBefore =
        Rando_TestOoTHintRevealText(revealBuf, (int)sizeof(revealBuf), revealExpected, (int)sizeof(revealExpected),
                                    &namesSongCheck, &coverNamed);
    int count = 0;
    uint32_t paneKey = 0;
    char pane[1024] = { 0 };
    const int paneBefore = RandoTest_HintTrackerPaneRow(ootHintKey, &count, &paneKey, pane, (int)sizeof(pane));
    const int slotBefore = OoT_SharedSongsTest_OffHintProbeSlot(2);
    const int majorSlot = OoT_SharedSongsTest_MajorCategorySlot();
    const int junkSlot = OoT_SharedSongsTest_JunkCategorySlot();
    SS_CHECK(enabledBefore == 1 && namesSongCheck == 1 && strstr(revealBuf, "Song of Time") != NULL,
             "(Off) RH_OOT_HINT before learning Song of Time did not name Song of Time ('%s')", revealBuf);
    SS_CHECK(paneBefore == 2 && strstr(pane, "Song of Time") != NULL && slotBefore == majorSlot,
             "(Off) Hint Tracker before learning Song of Time gave state %d, slot %d (want %d), text '%s'", paneBefore,
             slotBefore, majorSlot, pane);

    // Learn Song of Time in MM: RH_OOT_HINT and the cached Hint Tracker row must
    // stop promising Song of Time, name Blue Rupee, and rank as Junk Items.
    MM_SharedSwordTest_ArmLive();
    MM_SharedSongOfTimeTest_SetHeld(1);
    MM_HarvestSharedResources();
    const int enabledAfter =
        Rando_TestOoTHintRevealText(revealBuf, (int)sizeof(revealBuf), revealExpected, (int)sizeof(revealExpected),
                                    &namesSongCheck, &coverNamed);
    const int paneAfter = RandoTest_HintTrackerPaneRow(ootHintKey, &count, &paneKey, pane, (int)sizeof(pane));
    const int slotAfter = OoT_SharedSongsTest_OffHintProbeSlot(2);
    printf("[TEST] (Off) RH_OOT_HINT after MM learns Song of Time: '%s'; pane '%s'; slot %d -> %d\n", revealBuf, pane,
           slotBefore, slotAfter);
    SS_CHECK(enabledAfter == 1 && strstr(revealBuf, "Blue Rupee") != NULL && strstr(revealBuf, "Song of Time") == NULL,
             "(Off) RH_OOT_HINT still promised Song of Time after MM learned it: '%s'", revealBuf);
    SS_CHECK(paneAfter == 2 && strstr(pane, "Blue Rupee") != NULL && strstr(pane, "Song of Time") == NULL &&
                 slotAfter == junkSlot,
             "(Off) Hint Tracker cache/rank stale after MM learned Song of Time: state %d, slot %d (want %d), '%s'",
             paneAfter, slotAfter, junkSlot, pane);

    (void)RandoTest_HintTrackerSetRead(ootHintKey, wasRead == 1 ? 1 : 0);
    (void)OoT_TrackerAdapter_TestSetDeparted(-1);
    OoT_Combo_SetHintingSuspendedWorld(wasSuspended);
    Context_SetCurrentGame(priorGame);
    SsResetAllThreeSongsHeld();
}
} // namespace

// ============================================================================
// shared-songs-off-junk-give: ROM-free lock on fixed-at-home and second-check junk
// ============================================================================

TestResult Test_SharedSongsOffJunkGive(void) {
    printf("[TEST] shared-songs-off-junk-give: under Shuffle Songs Off, Combo_SharedWarpSongFixedAtHome covers all "
           "armed shared songs and Combo_SharedSongOffGiveBecomesJunk converts only the second-reached default check "
           "of an armed first-word shared song to junk (#1561)\n");
    sSsFailures = 0;
    const GameId priorGame = Context_GetCurrentGame();

    // ---- (1) Combo_SharedWarpSongFixedAtHome -----------------------------------
    uint16_t ootLens = 0;
    uint16_t mmLens = 0;
    SS_CHECK(Combo_SharedFamilyMemberId(GAME_OOT, RSBS_XITEM_SHARED_LENS, &ootLens) &&
                 Combo_SharedFamilyMemberId(GAME_MM, RSBS_XITEM_SHARED_LENS, &mmLens),
             "(1) Shared Lens registered in both games");
    const uint16_t ootMinuet = (uint16_t)OoT_SharedWarpSongsTest_PoolId(1);
    const uint16_t mmMinuet = (uint16_t)MM_SharedWarpSongsTest_PoolId(1);
    const uint16_t ootUnshared = (uint16_t)OoT_SharedWarpSongsTest_UnsharedSongId();
    const uint16_t mmUnshared = MM_SharedSongsTest_UnsharedSongId();

    for (int s = 0; s < 3; s++) {
        uint16_t ootId = 0;
        uint16_t mmId = 0;
        SS_CHECK(Combo_SharedFamilyMemberId(GAME_OOT, kSsOneCopySongs[s].bit, &ootId) &&
                     Combo_SharedFamilyMemberId(GAME_MM, kSsOneCopySongs[s].bit, &mmId),
                 "(1) %s registered in both games", kSsOneCopySongs[s].itemName);

        ComboContext_Init();
        SS_CHECK(!Combo_SharedWarpSongFixedAtHome(GAME_OOT, ootId, RSBS_SONG_SPOTS_OFF) &&
                     !Combo_SharedWarpSongFixedAtHome(GAME_MM, mmId, RSBS_SONG_SPOTS_OFF),
                 "(1) rules-off world must not arm fixed-at-home for %s under Off", kSsOneCopySongs[s].itemName);

        const ComboItemRulesRecord oneSong = SsRules(kSsOneCopySongs[s].bit);
        SsFreezeWorld(&oneSong);
        SS_CHECK(Combo_SharedWarpSongFixedAtHome(GAME_OOT, ootId, RSBS_SONG_SPOTS_OFF) &&
                     Combo_SharedWarpSongFixedAtHome(GAME_MM, mmId, RSBS_SONG_SPOTS_OFF),
                 "(1) %s on must arm fixed-at-home in both games under Shuffle Songs Off",
                 kSsOneCopySongs[s].itemName);
        SS_CHECK(!Combo_SharedWarpSongFixedAtHome(GAME_OOT, ootUnshared, RSBS_SONG_SPOTS_OFF) &&
                     !Combo_SharedWarpSongFixedAtHome(GAME_MM, mmUnshared, RSBS_SONG_SPOTS_OFF),
                 "(1) unshared song must not match fixed-at-home under Off");
        for (uint32_t mode = 1u; mode <= 3u; mode++) {
            SS_CHECK(!Combo_SharedWarpSongFixedAtHome(GAME_OOT, ootId, mode) &&
                         !Combo_SharedWarpSongFixedAtHome(GAME_MM, mmId, mode),
                     "(1) %s on must NOT arm fixed-at-home under Shuffle Songs %u", kSsOneCopySongs[s].itemName,
                     (unsigned)mode);
        }
    }
    {
        const ComboItemRulesRecord lensOnly = SsRules(RSBS_XITEM_SHARED_LENS);
        SsFreezeWorld(&lensOnly);
        SS_CHECK(!Combo_SharedWarpSongFixedAtHome(GAME_OOT, ootLens, RSBS_SONG_SPOTS_OFF) &&
                     !Combo_SharedWarpSongFixedAtHome(GAME_MM, mmLens, RSBS_SONG_SPOTS_OFF),
                 "(1) a non-song shared family (Lens) must not arm fixed-at-home under Off");
    }
    {
        ComboItemRulesRecord warpRule = SsRules(0u);
        warpRule.crossItemFlags2 = 0x00000080u; // RSBS_XITEM2_SONG_MINUET_SHARED
        SsFreezeWorld(&warpRule);
        SS_CHECK(Combo_SharedWarpSongFixedAtHome(GAME_OOT, ootMinuet, RSBS_SONG_SPOTS_OFF) &&
                     Combo_SharedWarpSongFixedAtHome(GAME_MM, mmMinuet, RSBS_SONG_SPOTS_OFF) &&
                     !Combo_SharedWarpSongFixedAtHome(GAME_OOT, ootMinuet, 1u),
                 "(1) second-word shared warp song still arms fixed-at-home under Off only");
    }

    // ---- (2) Combo_SharedSongOffGiveBecomesJunk & MM ConvertItem ---------------
    const ComboItemRulesRecord allThree = SsRules(kSsAllThreeSongsMask);
    for (int s = 0; s < 3; s++) {
        uint16_t ootId = 0;
        uint16_t mmId = 0;
        SS_CHECK(Combo_SharedFamilyMemberId(GAME_OOT, kSsOneCopySongs[s].bit, &ootId) &&
                     Combo_SharedFamilyMemberId(GAME_MM, kSsOneCopySongs[s].bit, &mmId),
                 "(2) %s registered in both games", kSsOneCopySongs[s].itemName);

        SsFreezeWorld(&allThree);
        SsResetAllThreeSongsHeld();
        Context_SetCurrentGame(GAME_NONE);

        // Unowned in both games -> neither gives junk.
        SS_CHECK(!Combo_SharedSongOffGiveBecomesJunk(GAME_OOT, ootId, RSBS_SONG_SPOTS_OFF, false) &&
                     !Combo_SharedSongOffGiveBecomesJunk(GAME_MM, mmId, RSBS_SONG_SPOTS_OFF, false),
                 "(2) unowned %s became junk under Off", kSsOneCopySongs[s].itemName);
        MM_SharedSwordTest_ArmLive();
        SS_CHECK(MM_SharedSongsTest_OffConvertSong(s, (int)RSBS_MM_SONGS_VANILLA) == 0,
                 "(2) MM ConvertItem converted unowned %s under Vanilla", kSsOneCopySongs[s].itemName);

        // Running game already owns its own copy -> junk under Off only.
        SS_CHECK(Combo_SharedSongOffGiveBecomesJunk(GAME_OOT, ootId, RSBS_SONG_SPOTS_OFF, true) &&
                     Combo_SharedSongOffGiveBecomesJunk(GAME_MM, mmId, RSBS_SONG_SPOTS_OFF, true),
                 "(2) runningGameOwnsCopy did not convert %s to junk under Off", kSsOneCopySongs[s].itemName);
        for (uint32_t mode = 1u; mode <= 3u; mode++) {
            SS_CHECK(!Combo_SharedSongOffGiveBecomesJunk(GAME_OOT, ootId, mode, true) &&
                         !Combo_SharedSongOffGiveBecomesJunk(GAME_MM, mmId, mode, true),
                     "(2) %s converted to junk via helper under non-Off mode %u", kSsOneCopySongs[s].itemName,
                     (unsigned)mode);
        }

        // Harvested from OoT -> MM gives junk (both pure helper and real ConvertItem/UncollectedNamedItem).
        OoT_SharedSwordTest_ArmLive();
        SsSetThreeSongHeldOoT(s, 1);
        OoT_HarvestSharedResources();
        SsSetThreeSongHeldOoT(s, 0);
        MM_SharedSwordTest_ArmLive();
        Context_SetCurrentGame(GAME_MM);
        SS_CHECK(Combo_SharedSongOffGiveBecomesJunk(GAME_MM, mmId, RSBS_SONG_SPOTS_OFF, false) &&
                     MM_SharedSongsTest_OffConvertSong(s, (int)RSBS_MM_SONGS_VANILLA) == 1,
                 "(2) %s harvested from OoT did not convert MM's default check to junk", kSsOneCopySongs[s].itemName);
        // Non-Vanilla MM Shuffle Songs (Song Locations = 0, Anywhere = 3) leaves single-bag song gives intact.
        SS_CHECK(MM_SharedSongsTest_OffConvertSong(s, 0) == 0 && MM_SharedSongsTest_OffConvertSong(s, 3) == 0,
                 "(2) MM ConvertItem converted harvested %s outside Vanilla song shuffle", kSsOneCopySongs[s].itemName);

        // Harvested from MM -> OoT gives junk.
        SsFreezeWorld(&allThree);
        SsResetAllThreeSongsHeld();
        MM_SharedSwordTest_ArmLive();
        SsSetThreeSongHeldMM(s, 1);
        MM_HarvestSharedResources();
        SsSetThreeSongHeldMM(s, 0);
        Context_SetCurrentGame(GAME_OOT);
        SS_CHECK(Combo_SharedSongOffGiveBecomesJunk(GAME_OOT, ootId, RSBS_SONG_SPOTS_OFF, false),
                 "(2) %s harvested from MM did not convert OoT's default check to junk", kSsOneCopySongs[s].itemName);

        // Live probeHeld BEFORE harvest: active game holding the song counts; MM title screen does not.
        SsFreezeWorld(&allThree);
        SsResetAllThreeSongsHeld();
        MM_SharedSwordTest_ArmLive();
        SsSetThreeSongHeldMM(s, 1);
        Context_SetCurrentGame(GAME_MM);
        SS_CHECK(Combo_SharedSongOffGiveBecomesJunk(GAME_OOT, ootId, RSBS_SONG_SPOTS_OFF, false),
                 "(2) live MM probeHeld before harvest was not seen by Combo_SharedSongOffGiveBecomesJunk");
        MM_SharedWarpSongsTest_SetTitleScreen();
        SS_CHECK(!Combo_SharedSongOffGiveBecomesJunk(GAME_OOT, ootId, RSBS_SONG_SPOTS_OFF, false),
                 "(2) MM title-screen bootstrap save falsely triggered Combo_SharedSongOffGiveBecomesJunk");

        // Disarmed rule -> never becomes junk via helper even when runningGameOwnsCopy is true.
        const ComboItemRulesRecord otherTwo = SsRules(kSsAllThreeSongsMask & ~kSsOneCopySongs[s].bit);
        SsFreezeWorld(&otherTwo);
        SsResetAllThreeSongsHeld();
        SS_CHECK(!Combo_SharedSongOffGiveBecomesJunk(GAME_OOT, ootId, RSBS_SONG_SPOTS_OFF, true) &&
                     !Combo_SharedSongOffGiveBecomesJunk(GAME_MM, mmId, RSBS_SONG_SPOTS_OFF, true),
                 "(2) disarmed %s still converted via Combo_SharedSongOffGiveBecomesJunk", kSsOneCopySongs[s].itemName);
    }

    // ---- (3) Negative guards: unshared songs, warp songs, non-song families ----
    {
        ComboItemRulesRecord everything = SsRules(kSsAllThreeSongsMask | RSBS_XITEM_SHARED_LENS);
        everything.crossItemFlags2 = 0x00003F80u;
        SsFreezeWorld(&everything);
        SsResetAllThreeSongsHeld();
        gComboCtx.sharedFamilyOwned = kSsAllThreeSongsMask | RSBS_XITEM_SHARED_LENS;
        gComboCtx.sharedFamilyOwned2 = 0x00003F80u;
        Context_SetCurrentGame(GAME_NONE);

        SS_CHECK(!Combo_SharedSongOffGiveBecomesJunk(GAME_OOT, ootUnshared, RSBS_SONG_SPOTS_OFF, true) &&
                     !Combo_SharedSongOffGiveBecomesJunk(GAME_MM, mmUnshared, RSBS_SONG_SPOTS_OFF, true),
                 "(3) unshared song matched Combo_SharedSongOffGiveBecomesJunk");

        SS_CHECK(!Combo_SharedSongOffGiveBecomesJunk(GAME_OOT, ootMinuet, RSBS_SONG_SPOTS_OFF, true) &&
                     !Combo_SharedSongOffGiveBecomesJunk(GAME_MM, mmMinuet, RSBS_SONG_SPOTS_OFF, true),
                 "(3) second-word shared warp song matched Combo_SharedSongOffGiveBecomesJunk");

        SS_CHECK(!Combo_SharedSongOffGiveBecomesJunk(GAME_OOT, ootLens, RSBS_SONG_SPOTS_OFF, true) &&
                     !Combo_SharedSongOffGiveBecomesJunk(GAME_MM, mmLens, RSBS_SONG_SPOTS_OFF, true),
                 "(3) non-song first-word family (Lens) matched Combo_SharedSongOffGiveBecomesJunk");
    }

    SsResetAllThreeSongsHeld();
    Context_SetCurrentGame(priorGame);
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    printf("[TEST] shared-songs-off-junk-give: %s\n", sSsFailures == 0 ? "PASS" : "FAIL");
    return sSsFailures == 0 ? TEST_PASS : TEST_FAIL;
}

extern "C" {
// #1542 S3 (ComboLogicEngineOoT.cpp, Rando/Foreign.cpp): the song checks of both games.
int OoT_ComboLogic_TestSongCheckIndex(uint16_t rc);
int MM_Rando_Foreign_TestSongCheckIndex(uint16_t randoCheckId);
}

/** MM's Shuffle Songs key (Rando/Menu.cpp) and its Vanilla value (RO_SONG_SHUFFLE_VANILLA). */
const char* const kSsMmSongShuffleKey = "gRando.Options.RO_SHUFFLE_SONGS";
const char* const kSsOoTHintKey = "gRandoSettings.OoTHint";
const int kSsMmSongShuffleVanilla = 2;

TestResult SharedSongsOneCopy_Run(int shuffleSongs, int mmShuffleSongs) {
    // #1542: MM's Shuffle Songs is the mapping of OoT's, whatever MM's key says.
    const uint32_t mmExpected = Combo_PairSongsMmValue((uint32_t)shuffleSongs);
    const bool mmVanilla = mmExpected == RSBS_MM_SONGS_VANILLA;
    const bool off = shuffleSongs == 0;
    const char* shuffleName = mmShuffleSongs == kSsMmSongShuffleVanilla ? "MM key Vanilla, ignored"
                              : (shuffleSongs >= 0 && shuffleSongs < 4) ? kSsShuffleNames[shuffleSongs]
                                                                        : "?";
    printf("[TEST] shared-songs-one-copy (Shuffle Songs %s): with the three shared-song rules on, %s, and the GOAL is "
           "still proven (#1085, #1561)\n",
           shuffleName,
           off ? "both games keep their vanilla copies with two-way fixed credits and second-check junk"
               : "the paired world holds exactly ONE copy of each shared song over both games");
    sSsFailures = 0;
    ComboContext_Init();
    SsClearAllKeys();
    for (int s = 0; s < 3; s++) {
        CVarSetInteger(SsOneCopyKey(s).c_str(), 1);
    }
    const int priorShuffle = CVarGetInteger(kSsSongShuffleKey, -1);
    CVarSetInteger(kSsSongShuffleKey, shuffleSongs);
    const int priorOoTHint = CVarGetInteger(kSsOoTHintKey, -1);
    if (off) {
        CVarSetInteger(kSsOoTHintKey, 1);
    }
    // MM's key is touched only by a row that sets it (the four OoT rows pass -1),
    // and that row puts back what was there.
    const int priorMm = CVarGetInteger(kSsMmSongShuffleKey, -1);
    if (mmShuffleSongs >= 0) {
        CVarSetInteger(kSsMmSongShuffleKey, mmShuffleSongs);
    }
    struct Restore {
        int prior;
        bool setOoTHint;
        int priorOoTHint;
        bool setMm;
        int priorMm;
        ~Restore() {
            if (prior < 0) {
                CVarClear(kSsSongShuffleKey);
            } else {
                CVarSetInteger(kSsSongShuffleKey, prior);
            }
            if (setOoTHint && priorOoTHint < 0) {
                CVarClear(kSsOoTHintKey);
            } else if (setOoTHint) {
                CVarSetInteger(kSsOoTHintKey, priorOoTHint);
            }
            if (setMm && priorMm < 0) {
                CVarClear(kSsMmSongShuffleKey);
            } else if (setMm) {
                CVarSetInteger(kSsMmSongShuffleKey, priorMm);
            }
            for (int s = 0; s < 3; s++) {
                CVarClear(SsOneCopyKey(s).c_str());
            }
        }
    } restore{ priorShuffle, off, priorOoTHint, mmShuffleSongs >= 0, priorMm };

    const int gen = Rando_HeadlessSeedTest("RSBSSONGSONECOPY");
    SS_CHECK(gen == 0, "the paired OoT generation failed under Shuffle Songs %s", shuffleName);
    bool allFrozen = Combo_ComboSettingsFrozen();
    for (int s = 0; s < 3; s++) {
        allFrozen = allFrozen && Combo_ComboCrossItemRule(kSsOneCopySongs[s].bit);
    }
    SS_CHECK(allFrozen, "the creation did not freeze the three shared-song rules from the keys (rules %08X)",
             (unsigned)gComboCtx.comboItemRules.crossItemFlags);
    const int mmHalf = (gen == 0) ? MM_Rando_HeadlessPairedHalf() : -1;
    SS_CHECK(mmHalf == 0, "MM's creation-time half did not produce a rando world");
    if (gen != 0 || mmHalf != 0) {
        printf("[TEST] shared-songs-one-copy (%s): FAIL\n", shuffleName);
        return TEST_FAIL;
    }
    // #1542: the pair's one songs setting. MM's half was made with the mapping of
    // OoT's Shuffle Songs; MM's own key (set by the mm-vanilla row) is not read.
    const int mmResolved = MM_PlacementConfinement_FrozenSongs();
    printf("[TEST] shared-songs-one-copy (%s): MM's Shuffle Songs resolved %d (expected %u)\n", shuffleName, mmResolved,
           (unsigned)mmExpected);
    SS_CHECK(mmResolved == (int)mmExpected,
             "MM's half was made with Shuffle Songs %d, not %u, the mapping of OoT's %d: MM's own key was read for a "
             "paired world (#1542)",
             mmResolved, (unsigned)mmExpected, shuffleSongs);
    const ComboSingleBagReport bag = *Combo_SingleBag_LastReport();
    printf("[TEST] shared-songs-one-copy (%s): bag %d rows, %d shared pair(s), fixed credits %d, %d trimmed OoT / %d "
           "trimmed MM, goal proven %d\n",
           shuffleName, bag.bagCount, bag.compose.sharedPairs, Combo_Logic_FixedCreditCount(), bag.trimmedOoT,
           bag.trimmedMM, bag.fill.goalProven ? 1 : 0);
    SS_CHECK(bag.status == RSBS_COMBO_LOGIC_OK && bag.fill.goalProven,
             "the single-bag fill did not prove the GOAL with the shared songs under Shuffle Songs %s", shuffleName);
    const int wantSharedPairs = off ? 0 : 3;
    const int wantFixedCredits = off ? 6 : 0;
    SS_CHECK(bag.compose.sharedPairs == wantSharedPairs,
             "under Shuffle Songs %s the composer made %d shared pair(s), expected %d", shuffleName,
             bag.compose.sharedPairs, wantSharedPairs);
    SS_CHECK(Combo_Logic_FixedCreditCount() == wantFixedCredits,
             "under Shuffle Songs %s the creation set %d fixed shared credit(s), expected %d", shuffleName,
             Combo_Logic_FixedCreditCount(), wantFixedCredits);
    if (off) {
        // #1069 + #1561: with Shared Song of Time frozen on, MM's starting save does
        // not hold QUEST_SONG_TIME; RC_CLOCK_TOWER_ROOF_SONG_OF_TIME holds MM's copy.
        SS_CHECK(MM_SharedSongOfTimeTest_Held() == 0,
                 "under Off with Shared Song of Time on, MM's starting save already holds Song of Time");
    }

    const int shuffledTotal = MM_ComboLogic_TestShuffledItems(nullptr, nullptr, 0);
    std::vector<uint16_t> items((size_t)(shuffledTotal > 0 ? shuffledTotal : 1));
    const int shuffled = MM_ComboLogic_TestShuffledItems(items.data(), nullptr, (int)items.size());
    SS_CHECK(shuffled > 0, "MM's generated world lists no shuffled items");

    const int pocketItem = OoT_SharedWarpSongsTest_LinksPocket();
    {
        const int unshared = OoT_SharedWarpSongsTest_UnsharedSongId();
        const int wantUnshared = shuffleSongs == 1 ? 1 : 0;
        SS_CHECK(OoT_Fill_TestSongHeldForBag((uint16_t)unshared) == wantUnshared,
                 "Link's Pocket song exclusion answered %d for an unshared song under %s; expected %d",
                 OoT_Fill_TestSongHeldForBag((uint16_t)unshared), shuffleName, wantUnshared);
    }

    int sharedRgs[3] = { -1, -1, -1 };
    uint16_t offOoTHosts[3] = { 0, 0, 0 };
    uint16_t offMMHosts[3] = { 0, 0, 0 };
    for (int s = 0; s < 3; s++) {
        uint16_t ootId = 0;
        uint16_t mmId = 0;
        const bool registered = Combo_SharedFamilyMemberId(GAME_OOT, kSsOneCopySongs[s].bit, &ootId) &&
                                Combo_SharedFamilyMemberId(GAME_MM, kSsOneCopySongs[s].bit, &mmId);
        SS_CHECK(registered, "the %s family is not registered in both games", kSsOneCopySongs[s].itemName);
        if (!registered) {
            continue;
        }
        sharedRgs[s] = (int)ootId;
        const int wantHeldForBag = off ? 0 : 1;
        SS_CHECK(OoT_Fill_TestSongHeldForBag(ootId) == wantHeldForBag,
                 "Link's Pocket song exclusion answered %d for %s under %s; expected %d",
                 OoT_Fill_TestSongHeldForBag(ootId), kSsOneCopySongs[s].itemName, shuffleName, wantHeldForBag);
        SS_CHECK(pocketItem != (int)ootId, "Link's Pocket holds the shared %s", kSsOneCopySongs[s].itemName);

        const int ootCopies = OoT_SharedSongsTest_WorldCopies((int)ootId);
        int mmCopies = 0;
        if (mmVanilla) {
            mmCopies = MM_PlacementConfinement_WorldCopies(mmId);
        } else {
            for (int i = 0; i < shuffled && i < (int)items.size(); i++) {
                if (items[(size_t)i] == mmId) {
                    mmCopies++;
                }
            }
        }
        int placements = 0;
        int onSongChecks = 0;
        for (GameId host : { GAME_OOT, GAME_MM }) {
            for (int i = 0; i < Combo_Logic_PlacementCount(host); i++) {
                ComboLogicPlacement p;
                if (!Combo_Logic_PlacementAt(host, i, &p)) {
                    continue;
                }
                const bool isPair = (p.item.originGame == (uint8_t)GAME_OOT && p.item.id == ootId) ||
                                    (p.item.originGame == (uint8_t)GAME_MM && p.item.id == mmId);
                if (!isPair) {
                    continue;
                }
                placements++;
                onSongChecks += (host == GAME_OOT ? OoT_ComboLogic_TestSongCheckIndex(p.hostCheck)
                                                  : MM_Rando_Foreign_TestSongCheckIndex(p.hostCheck)) >= 0
                                    ? 1
                                    : 0;
            }
        }

        if (off) {
            printf("[TEST] shared-songs-one-copy (Off): %s held by %d OoT and %d MM check(s), bag placements %d "
                   "(expected 1, 1, 0)\n",
                   kSsOneCopySongs[s].itemName, ootCopies, mmCopies, placements);
            SS_CHECK(ootCopies == 1 && mmCopies == 1 && placements == 0,
                     "under Off %s is held by %d OoT and %d MM check(s) with %d bag placement(s); expected 1 at each "
                     "game's default check and 0 bag rows",
                     kSsOneCopySongs[s].itemName, ootCopies, mmCopies, placements);
            const int ootCheck = OoT_SharedSongsTest_OffVanillaCheckId(s);
            const int mmCheck = MM_SharedSongsTest_OffVanillaCheckId(s);
            offOoTHosts[s] = (uint16_t)ootCheck;
            offMMHosts[s] = (uint16_t)mmCheck;
            SS_CHECK(OoT_SharedSongsTest_OffVanillaAtHome(s) == 1,
                     "under Off %s is not fixed hidden on its OoT vanilla check (%d)", kSsOneCopySongs[s].itemName,
                     ootCheck);
            SS_CHECK(mmCheck >= 0 && MM_ComboLogic_TestFixedCheckItem((uint16_t)mmCheck) == (int)mmId,
                     "under Off %s is not fixed unshuffled on its MM vanilla check (%d)", kSsOneCopySongs[s].itemName,
                     mmCheck);

            bool foundOoTCredit = false;
            bool foundMMCredit = false;
            for (int c = 0; c < Combo_Logic_FixedCreditCount(); c++) {
                ComboLogicFixedCredit credit;
                if (!Combo_Logic_FixedCreditAt(c, &credit)) {
                    continue;
                }
                if (credit.hostGame == (uint8_t)GAME_OOT && (int)credit.hostCheck == ootCheck &&
                    credit.farItem.originGame == (uint8_t)GAME_MM && credit.farItem.id == mmId) {
                    foundOoTCredit = true;
                }
                if (credit.hostGame == (uint8_t)GAME_MM && (int)credit.hostCheck == mmCheck &&
                    credit.farItem.originGame == (uint8_t)GAME_OOT && credit.farItem.id == ootId) {
                    foundMMCredit = true;
                }
            }
            SS_CHECK(foundOoTCredit && foundMMCredit,
                     "under Off %s is missing a fixed credit (OoT->MM %d at %d, MM->OoT %d at %d)",
                     kSsOneCopySongs[s].itemName, foundOoTCredit ? 1 : 0, ootCheck, foundMMCredit ? 1 : 0, mmCheck);
        } else {
            printf("[TEST] shared-songs-one-copy (%s): %s held by %d OoT check(s) and %d MM check(s) (expected one in "
                   "all)\n",
                   shuffleName, kSsOneCopySongs[s].itemName, ootCopies, mmCopies);
            SS_CHECK(ootCopies >= 0 && ootCopies + mmCopies == 1,
                     "under Shuffle Songs %s the paired world holds %d copies of %s (OoT %d, MM %d), expected ONE: "
                     "OoT's own copy is still in the world beside the shared row",
                     shuffleName, ootCopies + mmCopies, kSsOneCopySongs[s].itemName, ootCopies, mmCopies);
            if (shuffleSongs == 1) {
                printf("[TEST] shared-songs-one-copy (%s): %s placed %d time(s) by the bag, %d on a song check\n",
                       shuffleName, kSsOneCopySongs[s].itemName, placements, onSongChecks);
                SS_CHECK(placements == 1 && onSongChecks == 1,
                         "under the shared song pool %s was placed %d time(s), %d on a song check: its one copy must "
                         "be the bag's pair row on a song check of either game",
                         kSsOneCopySongs[s].itemName, placements, onSongChecks);
            }
        }
    }

    if (off) {
        // #1561: all twelve OoT songs stay at their vanilla checks under Off, the
        // three shared songs included.
        const int expectAtHome = OoT_SharedSongsTest_VanillaSongCount();
        const int atHome = OoT_SharedSongsTest_OtherVanillaSongsAtHome(nullptr, 0);
        printf("[TEST] shared-songs-one-copy (Off): %d of %d OoT vanilla songs at home\n", atHome, expectAtHome);
        SS_CHECK(atHome == expectAtHome,
                 "under Shuffle Songs Off %d of OoT's %d vanilla songs are at their vanilla checks, expected all",
                 atHome, expectAtHome);
        SsOffVerifyExitRounds(offOoTHosts, offMMHosts);
        const int crossings = OoT_Creation_FinishPairedHalf(0);
        SS_CHECK(crossings >= 0, "OoT_Creation_FinishPairedHalf failed under Shuffle Songs Off (%d)", crossings);
        if (crossings >= 0) {
            SsOffVerifyRuntimeGiveAndHints();
        }
        SS_CHECK(Combo_SharedFamiliesOwned() == 0u, "Off runtime checks leaked sharedFamilyOwned %08X",
                 (unsigned)Combo_SharedFamiliesOwned());
    }

    printf("[TEST] shared-songs-one-copy (%s): %s\n", shuffleName, sSsFailures == 0 ? "PASS" : "FAIL");
    return sSsFailures == 0 ? TEST_PASS : TEST_FAIL;
}
