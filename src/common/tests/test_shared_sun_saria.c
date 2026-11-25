/**
 * @file test_shared_sun_saria.c
 * @brief Locks for the Shared Sun's Song and Shared Saria's Song RULES (#1587
 *        stage S1; OoTMM sharedSongSun and sharedSongSarias, data.ts:3080 and
 *        :3280): two bits of the SECOND cross-game rules word, their keys and
 *        their settings entries, hidden until #1587 S7.
 *
 *   shared-sun-saria-rule (CTest SharedSunSariaRule, redship tier, ROM-free;
 *   needs the CVar store, so the runner brings up the display-free shared
 *   context first, as shared-songs-rule does) -- THE RULES. (1) The bits:
 *   0x4000 and 0x8000, allocated, shared-pair families (a second-word member
 *   registers at each), in RSBS_XITEM2_SHARED_SONGS and not warp songs, named
 *   with OoTMM's keys, requiring no second-word bit. (2) The keys: appended after
 *   every earlier id, OoTMM's labels, off by default, exactly 0 or 1, each
 *   authoring its own bit. (3) The resolver: each key reaches only its own bit
 *   and never the first word. (4) The fingerprint: a frozen bit is folded into
 *   it, each differently, and with both off it is today's. (5) HIDDEN: on the
 *   production mask neither rule is offered and Generate's question refuses a
 *   world with either on, by OoTMM's name; the test reveal offers both (#1587 S7
 *   flips this leg). (6) The restore through the API and (7) through a real
 *   .redsave: refused on the production mask, restored into both keys revealed,
 *   the second family word riding the file.
 *
 *   shared-sun-saria-merge (CTest SharedSunSariaMerge, redship tier, ROM-free, no
 *   bring-up, as shared-warp-songs-merge; generates nothing) -- THE MEMBERS (#1587
 *   S2). Both games register their own Sun's Song and Saria's Song at the two
 *   bits; MM's pooled follows the FROZEN rule (so the bag builds both pairs with
 *   MM's own rows at 0); through the real shims a song learnt in either game is
 *   known in the other at arrival and at OoT's load seam, each rule carrying only
 *   its own song and never Epona's; a far copy is credited at pickup; a shared
 *   copy is named untagged; and a Saria's Song MM loses comes back at the next
 *   apply (the pool is OR-only). The rules are frozen directly (no Generate), so
 *   the hidden bits need no reveal.
 *
 * Every key is spelled from the identity prefix rather than as a literal, so
 * the tier-4 classification scan sees only the manifest's spelling.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * like its neighbours; every symbol under test is C-linkage. Helpers carry the
 * Ssr prefix because every included test file shares one translation unit.
 */

#include "../combo_settings_view.h"
#include "../combo_single_bag.h"
#include "../context.h"
#include "../cvar_shared_keys.h"
#include "../foreign_items.h"
#include "../save.h"
#include "../shared_resources.h"
#include "../test_runner.h"
#include "../test_scratch.h"
#include "../warp_song_items.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

static int sSsrFailures;

#define SSR_CHECK(cond, ...)                          \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sSsrFailures++;                           \
        }                                             \
    } while (0)

namespace {

// The identity terms XW2's vectors were computed over (test_combo_item_rules_word2.c).
const uint32_t kSsrSettingsHash = 0xDEADBEEFu;
const uint32_t kSsrProfileDigest = 0x0BADF00Du;
const uint32_t kSsrSeed = 0x0169A0FFu;
const char* const kSsrLoadDir = "rsbs_test_shared_sun_saria_load";

/** One rule: its bit, spelled as a literal (it is .redsave and seed-package
 *  format, so the lock states it rather than reading it back from the header
 *  under test), its key's suffix, its label (OoTMM's option name) and OoTMM's
 *  key. Each game's item (#1587 S2) is not spelled here: this TU sees no RG_* or
 *  RI_*, so the merge row asks the games' bridges (_PoolId, the same index). */
struct SsrSong {
    uint32_t bit;
    const char* keySuffix;
    const char* label;
    const char* rule;
};

const SsrSong kSsrSongs[2] = {
    { 0x00004000u, "SharedSongSun", "Shared Sun's Song", "sharedSongSun" },
    { 0x00008000u, "SharedSongSarias", "Shared Saria's Song", "sharedSongSarias" },
};
const uint32_t kSsrBoth = 0x0000C000u;

std::string SsrKey(int song) {
    return std::string(RSBS::kComboIdentityKeyPrefix) + kSsrSongs[song].keySuffix;
}

/** The authoring id whose key is @p song's rule, or COMBO_SETTING_COUNT. */
ComboSettingId SsrFindSetting(int song) {
    const std::string key = SsrKey(song);
    for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
        // A missing descriptor is a NULL key: never compared, so the leg names it.
        const char* k = Combo_ComboSettingKey((ComboSettingId)i);
        if (k != NULL && key == k) {
            return (ComboSettingId)i;
        }
    }
    return COMBO_SETTING_COUNT;
}

void SsrClearKeys() {
    if (!Combo_ComboSettingStoreAvailable()) {
        return;
    }
    for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
        const char* k = Combo_ComboSettingKey((ComboSettingId)i);
        if (k != NULL) {
            CVarClear(k);
        }
    }
}

/** Publish a paired identity and freeze the shipped combo record, every
 *  first-word rule off and @p rules2 as the second word, the way
 *  Playthrough_Init stamps a world (no Generate: nothing is refused). */
void SsrFreezeWorld(uint32_t rules2) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = kSsrSeed;
    gComboCtx.sharedRandoSettingsHash = kSsrSettingsHash;
    gComboCtx.mmProfileDigest = kSsrProfileDigest;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    Combo_FreezeComboSettingsEx2(&rec, NULL, rules2);
}

void SsrSeedShadows(uint8_t fill) {
    Context_InitFrozenStates();
    std::vector<uint8_t> oot(OOT_SAVE_CONTEXT_SIZE, fill);
    std::vector<uint8_t> mm(MM_SAVE_CONTEXT_SIZE, (uint8_t)(fill ^ 0xFFu));
    Context_UpdateShadowCopy(GAME_OOT, oot.data(), oot.size());
    Context_UpdateShadowCopy(GAME_MM, mm.data(), mm.size());
}

/** The two slots' members of both games, saved so the registration leg puts
 *  the games' own back (from #1587 S2 the bring-up registers real ones). */
struct SsrSavedMembers {
    ComboSharedFamilyMember m[3][2];
    bool had[3][2];
};

void SsrSaveMembers(SsrSavedMembers* s) {
    memset(s, 0, sizeof(*s));
    for (GameId game : { GAME_OOT, GAME_MM }) {
        for (int song = 0; song < 2; song++) {
            s->had[game][song] = Combo_SharedFamily2MemberGet(game, kSsrSongs[song].bit, &s->m[game][song]);
        }
    }
}

void SsrRestoreMembers(const SsrSavedMembers* s) {
    for (GameId game : { GAME_OOT, GAME_MM }) {
        for (int song = 0; song < 2; song++) {
            Combo_RegisterSharedFamily2Member(game, kSsrSongs[song].bit, s->had[game][song] ? &s->m[game][song] : NULL);
        }
    }
}

bool SsrFakeHeld(uint16_t id) {
    (void)id;
    return false;
}
void SsrFakeGive(uint16_t id) {
    (void)id;
}

std::vector<uint8_t> SsrReadBytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

} // namespace

// ============================================================================
// shared-sun-saria-rule
// ============================================================================

TestResult SharedSunSariaRule_Run(void) {
    printf("[TEST] shared-sun-saria-rule: Shared Sun's Song and Shared Saria's Song are two second-word Cross-Game "
           "Rules, off by default, hidden (Generate refuses them), frozen into the fingerprint, and restored from "
           "the file at load (#1587)\n");
    sSsrFailures = 0;
    SSR_CHECK(Combo_ComboSettingStoreAvailable(), "no CVar store: every key leg below would pass on the defaults");
    Combo_ComboItemRules2RevealForTest(0);
    ComboContext_Init();
    SsrClearKeys();

    ComboSettingsRecord defaults;
    Combo_ComboSettingsDefaults(&defaults);

    // ---- (1) The bits: allocated, families, named, requiring nothing ------------------
    // Catches: a bit not allocated (ALL_V1), not a family (FAMILY_MASK), outside
    // RSBS_XITEM2_SHARED_SONGS, unnamed, or a Requires widened past the warp songs
    // (0x4000 >> 7 would make the Sun's Song require Shared Soaring).
    {
        SSR_CHECK(RSBS_XITEM2_SHARED_SONG_SUN == kSsrSongs[0].bit && RSBS_XITEM2_SHARED_SONG_SARIAS == kSsrSongs[1].bit,
                  "(1) the bits moved: Sun %08X, Saria's %08X; they are .redsave format (0x4000, 0x8000)",
                  (unsigned)RSBS_XITEM2_SHARED_SONG_SUN, (unsigned)RSBS_XITEM2_SHARED_SONG_SARIAS);
        // #1702 C2 took 0x10000 (the Shared Child's Wallet rule) and 0x80000 (a
        // derived marker), so ALL_V1 is 0009FFFF; neither is a family or a song.
        SSR_CHECK(RSBS_XITEM2_ALL_V1 == 0x0009FFFFu && RSBS_XITEM2_FAMILY_MASK == 0x0000FF80u &&
                      RSBS_XITEM2_SHARED_SONGS == 0x0000FF80u,
                  "(1) ALL_V1 %08X (want 0009FFFF), FAMILY_MASK %08X and SHARED_SONGS %08X (want 0000FF80)",
                  (unsigned)RSBS_XITEM2_ALL_V1, (unsigned)RSBS_XITEM2_FAMILY_MASK, (unsigned)RSBS_XITEM2_SHARED_SONGS);
        for (int song = 0; song < 2; song++) {
            const SsrSong& s = kSsrSongs[song];
            SSR_CHECK((s.bit & (uint32_t)RSBS_XITEM2_ALL_V1) == s.bit &&
                          (s.bit & (uint32_t)RSBS_XITEM2_FAMILY_MASK) == s.bit &&
                          (s.bit & (uint32_t)RSBS_XITEM2_SHARED_SONGS) == s.bit &&
                          (s.bit & (uint32_t)RSBS_XITEM2_WARP_SONG_SHARED) == 0u,
                      "(1) %s (%08X) is not an allocated second-word family outside the warp songs", s.rule,
                      (unsigned)s.bit);
            SSR_CHECK(strcmp(Combo_ComboItemRule2Name(s.bit), s.rule) == 0,
                      "(1) bit %08X is named '%s', OoTMM's key is %s", (unsigned)s.bit, Combo_ComboItemRule2Name(s.bit),
                      s.rule);
            SSR_CHECK(Combo_ComboItemRule2Requires(s.bit) == 0u,
                      "(1) %s requires %08X; its extension is Majora's Mask's own row, never a second-word bit", s.rule,
                      (unsigned)Combo_ComboItemRule2Requires(s.bit));
            SSR_CHECK(Combo_ComboItemRules2Valid(s.bit), "(1) %s alone is not a world this build can create", s.rule);
        }
        SSR_CHECK(Combo_ComboItemRules2Valid(kSsrBoth) && Combo_ComboItemRules2Valid(0x0000FFFFu),
                  "(1) both rules, or every allocated rule, is not a valid world");
        SSR_CHECK(strcmp(Combo_ComboItemRule2Name(kSsrBoth), "(unknown)") == 0,
                  "(1) a mask of the two bits reads as one rule");

        // A second-word member registers at each bit (Family2Index takes it), with
        // the slot's own members saved and put back.
        SsrSavedMembers saved;
        SsrSaveMembers(&saved);
        const ComboSharedFamilyMember fake = { 0x0F0Fu, SsrFakeHeld, SsrFakeGive, NULL };
        int accepted = 0;
        for (int song = 0; song < 2; song++) {
            accepted += Combo_RegisterSharedFamily2Member(GAME_OOT, kSsrSongs[song].bit, &fake) ? 1 : 0;
        }
        SsrRestoreMembers(&saved);
        SSR_CHECK(accepted == 2, "(1) %d of the two bits took a second-word family member", accepted);

        // Armed from the frozen word, as every family is.
        SsrFreezeWorld(kSsrBoth);
        SSR_CHECK(Combo_SharedFamilies2Armed() == kSsrBoth, "(1) a frozen 0000C000 world armed %08X",
                  (unsigned)Combo_SharedFamilies2Armed());
        ComboContext_Init();
    }

    // ---- (2) The keys: appended, OoTMM's names, off by default, exactly 0 or 1 ---------
    // Catches: a reordered enum (an id moved), a missing descriptor (a NULL key),
    // a missing default or value case, and a key that authors another bit.
    ComboSettingId ids[2] = { COMBO_SETTING_COUNT, COMBO_SETTING_COUNT };
    {
        for (int song = 0; song < 2; song++) {
            const SsrSong& s = kSsrSongs[song];
            ids[song] = SsrFindSetting(song);
            SSR_CHECK(ids[song] != COMBO_SETTING_COUNT, "(2) no setting has the key %s", SsrKey(song).c_str());
            if (ids[song] == COMBO_SETTING_COUNT) {
                continue;
            }
            const ComboSettingId want = (ComboSettingId)((int)COMBO_SETTING_SHARED_MASK_SPOOKY + 1 + song);
            SSR_CHECK(ids[song] == want, "(2) %s is setting %d, want %d (appended after #980's ids, so no id moved)",
                      s.rule, (int)ids[song], (int)want);
            SSR_CHECK(strcmp(Combo_ComboSettingLabel(ids[song]), s.label) == 0,
                      "(2) the %s key is labelled '%s', not '%s'", s.rule, Combo_ComboSettingLabel(ids[song]), s.label);
            SSR_CHECK(Combo_ComboSettingDefault(ids[song]) == 0,
                      "(2) %s must ship off (OoTMM's default; no world moves)", s.rule);
            SSR_CHECK(Combo_ComboSettingValueValid(ids[song], 0) && Combo_ComboSettingValueValid(ids[song], 1) &&
                          !Combo_ComboSettingValueValid(ids[song], 2) && !Combo_ComboSettingValueValid(ids[song], -1),
                      "(2) %s is not exactly 0 or 1", s.rule);
            SSR_CHECK(Combo_ComboItemRule2BitFor(ids[song]) == s.bit, "(2) %s authors %08X, not %08X", s.rule,
                      (unsigned)Combo_ComboItemRule2BitFor(ids[song]), (unsigned)s.bit);
        }
        // Every allocated bit but the derived ones (#1702 C2: no key authors them).
        SSR_CHECK(Combo_ComboItemRule2KeyedBits() == (RSBS_XITEM2_ALL_V1 & ~RSBS_XITEM2_DERIVED),
                  "(2) the keyed second-word bits are %08X, expected every allocated non-derived bit (0001FFFF)",
                  (unsigned)Combo_ComboItemRule2KeyedBits());
    }

    // ---- (3) The resolver: each key reaches only its own bit ----------------------------
    // Catches: a key aliasing the other's bit, a key reaching the first word or the
    // record, a value outside 0/1 read as on, and an extension the resolver demands.
    if (ids[0] != COMBO_SETTING_COUNT && ids[1] != COMBO_SETTING_COUNT) {
        SSR_CHECK(Combo_ResolveComboItemRules2() == 0u, "(3) unset keys resolved %08X",
                  (unsigned)Combo_ResolveComboItemRules2());
        uint8_t wantCanon[RSBS_COMBO_SETTINGS_CANONICAL_LEN];
        Combo_ComboSettingsCanonical(&defaults, wantCanon);
        for (int song = 0; song < 2; song++) {
            SsrClearKeys();
            SSR_CHECK(Combo_ComboSettingSet(ids[song], 1) == 1, "(3) the writer refused %s before creation",
                      kSsrSongs[song].rule);
            SSR_CHECK(Combo_ResolveComboItemRules2() == kSsrSongs[song].bit,
                      "(3) %s alone resolved %08X, want exactly %08X (no extension needed)", kSsrSongs[song].rule,
                      (unsigned)Combo_ResolveComboItemRules2(), (unsigned)kSsrSongs[song].bit);
            ComboItemRulesRecord first;
            Combo_ResolveComboItemRules(&first);
            ComboSettingsRecord rec;
            Combo_ResolveComboSettings(&rec);
            uint8_t canon[RSBS_COMBO_SETTINGS_CANONICAL_LEN];
            Combo_ComboSettingsCanonical(&rec, canon);
            SSR_CHECK(first.crossItemFlags == 0u && memcmp(canon, wantCanon, sizeof(canon)) == 0,
                      "(3) %s reached the first word (%08X) or the combo record", kSsrSongs[song].rule,
                      (unsigned)first.crossItemFlags);
            SSR_CHECK(Combo_ComboSettingSet(ids[song], 0) == 1 && Combo_ResolveComboItemRules2() == 0u,
                      "(3) %s explicitly 0 resolved %08X", kSsrSongs[song].rule,
                      (unsigned)Combo_ResolveComboItemRules2());
            // A value nobody chose (set out of band): the shipped default, off.
            CVarSetInteger(SsrKey(song).c_str(), 2);
            SSR_CHECK(Combo_ResolveComboItemRules2() == 0u, "(3) %s stored as 2 resolved %08X, want off",
                      kSsrSongs[song].rule, (unsigned)Combo_ResolveComboItemRules2());
        }
        SsrClearKeys();
        SSR_CHECK(Combo_ComboSettingSet(ids[0], 1) == 1 && Combo_ComboSettingSet(ids[1], 1) == 1,
                  "(3) the writers refused both keys before creation");
        SSR_CHECK(Combo_ResolveComboItemRules2() == kSsrBoth, "(3) both keys resolved %08X, want 0000C000",
                  (unsigned)Combo_ResolveComboItemRules2());
        SsrClearKeys();
    }

    // ---- (4) The fingerprint: folded when set, today's with both off ----------------
    // Catches: a freeze that drops either bit, a fold that cannot tell the two
    // apart, and a world with both off whose fingerprint moved (the v4 pin below;
    // the word-blind Ex form is Ex2 with 0, so comparing the two proves nothing).
    {
        const uint32_t base =
            Combo_ComputeComboSettingsHashForGeneratorEx2(&defaults, NULL, 0u, kSsrSettingsHash, kSsrProfileDigest, 4u);
        SSR_CHECK(base == 0xA06B18F7u, "(4) a rules-off world's v4 fingerprint is %08X, main's is A06B18F7",
                  (unsigned)base);
        const uint32_t sun = Combo_ComputeComboSettingsHashForGeneratorEx2(&defaults, NULL, kSsrSongs[0].bit,
                                                                           kSsrSettingsHash, kSsrProfileDigest, 4u);
        const uint32_t saria = Combo_ComputeComboSettingsHashForGeneratorEx2(&defaults, NULL, kSsrSongs[1].bit,
                                                                             kSsrSettingsHash, kSsrProfileDigest, 4u);
        const uint32_t both = Combo_ComputeComboSettingsHashForGeneratorEx2(&defaults, NULL, kSsrBoth, kSsrSettingsHash,
                                                                            kSsrProfileDigest, 4u);
        SSR_CHECK(sun != base && saria != base && both != base && sun != saria && both != sun && both != saria,
                  "(4) the fingerprints do not tell the worlds apart (base %08X, Sun %08X, Saria's %08X, both %08X)",
                  (unsigned)base, (unsigned)sun, (unsigned)saria, (unsigned)both);

        // A frozen Sun world stamps the fingerprint over its word, which verifies at
        // this generator and not through the word-blind check.
        SsrFreezeWorld(kSsrSongs[0].bit);
        const uint32_t stamp = gComboCtx.comboSettingsHash;
        SSR_CHECK(Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2) == kSsrSongs[0].bit &&
                      Combo_ComboCrossItemRule2(RSBS_XITEM2_SHARED_SONG_SUN) &&
                      !Combo_ComboCrossItemRule2(RSBS_XITEM2_SHARED_SONG_SARIAS),
                  "(4) the freeze stored %08X, not the Sun's bit alone",
                  (unsigned)Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2));
        SSR_CHECK(stamp == Combo_ComputeComboSettingsHashEx2(&gComboCtx.comboSettings, &gComboCtx.comboItemRules,
                                                             kSsrSongs[0].bit, kSsrSettingsHash, kSsrProfileDigest) &&
                      Combo_ComboSettingsHashGeneratorVersionEx2(&gComboCtx.comboSettings, &gComboCtx.comboItemRules,
                                                                 kSsrSongs[0].bit, kSsrSettingsHash, kSsrProfileDigest,
                                                                 stamp) == (int)RSBS_GENERATOR_VERSION,
                  "(4) a frozen Sun world stamped %08X, not the fingerprint over its word", (unsigned)stamp);
        SSR_CHECK(
            Combo_ComboSettingsHashGeneratorVersionEx(&gComboCtx.comboSettings, &gComboCtx.comboItemRules,
                                                      kSsrSettingsHash, kSsrProfileDigest, stamp) < 0,
            "(4) the word-blind check reproduced a fingerprint stamped over the Sun's bit (the bit was not folded)");
        SSR_CHECK((Combo_ComboSettingsDivergence() & RSBS_COMBO_DIVERGE_FINGERPRINT) == 0u,
                  "(4) a freshly frozen Sun world does not reproduce its own fingerprint");

        // With the keys unset, the freeze of the resolved word is today's world.
        SsrClearKeys();
        SsrFreezeWorld(Combo_ResolveComboItemRules2());
        const uint32_t offStamp = gComboCtx.comboSettingsHash;
        SSR_CHECK(Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2) == 0u &&
                      offStamp == Combo_ComputeComboSettingsHashEx(&gComboCtx.comboSettings, &gComboCtx.comboItemRules,
                                                                   kSsrSettingsHash, kSsrProfileDigest) &&
                      offStamp != stamp,
                  "(4) with both keys unset the freeze stored %08X and stamped %08X, not today's world",
                  (unsigned)Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2), (unsigned)offStamp);
        ComboContext_Init();
    }

    // ---- (5) HIDDEN until #1587 S7 (S7 flips this leg) -------------------------------
    // Catches: the bits offered (a row drawn, Generate taking them) before their
    // integration proof exists.
    {
        char names[192];
        // Both inside the mask (#1702 C2 added its own two bits beside them).
        SSR_CHECK((RSBS_XITEM2_HIDDEN & kSsrBoth) == kSsrBoth && (RSBS_XITEM2_HIDDEN & 0x00003FFFu) == 0u,
                  "(5) the hidden mask is %08X, want both 0000C000 inside it until #1587 S7 and no warp song",
                  (unsigned)RSBS_XITEM2_HIDDEN);
        for (int song = 0; song < 2; song++) {
            const SsrSong& s = kSsrSongs[song];
            SSR_CHECK(!Combo_ComboItemRule2Offered(s.bit), "(5) %s is offered before #1587 S7", s.rule);
            const uint32_t refused = Combo_ComboItemRules2Refused(s.bit, names, sizeof(names));
            SSR_CHECK(refused == s.bit && strcmp(names, s.rule) == 0,
                      "(5) Generate's question refused %08X ('%s') for %s, want its bit, by OoTMM's name",
                      (unsigned)refused, names, s.rule);
        }
        SSR_CHECK(Combo_ComboItemRules2Refused(kSsrBoth, names, sizeof(names)) == kSsrBoth &&
                      strcmp(names, "sharedSongSun, sharedSongSarias") == 0,
                  "(5) both rules were refused as '%s'", names);
        // The creation's own question over the session's resolution (Generate asks
        // exactly this before it freezes: combo-item-rules-second-word's source leg).
        if (ids[0] != COMBO_SETTING_COUNT) {
            SSR_CHECK(Combo_ComboSettingSet(ids[0], 1) == 1, "(5) the writer refused the Sun key");
            const uint32_t refused = Combo_ComboItemRules2Refused(Combo_ResolveComboItemRules2(), names, sizeof(names));
            printf("[TEST] (5) Generate with Shared Sun's Song set: resolved %08X, refused %08X (%s)\n",
                   (unsigned)Combo_ResolveComboItemRules2(), (unsigned)refused, names);
            SSR_CHECK(Combo_ResolveComboItemRules2() == kSsrSongs[0].bit && refused == kSsrSongs[0].bit,
                      "GENERATE MUST REFUSE THE HIDDEN SHARED SUN'S SONG: resolved %08X, refused %08X ('%s')",
                      (unsigned)Combo_ResolveComboItemRules2(), (unsigned)refused, names);
            SsrClearKeys();
        }
        // The MM reveal level reads the production mask: still hidden.
        Combo_ComboItemRules2RevealForTest(RSBS_XITEM2_REVEAL_MM_SONGS);
        SSR_CHECK(!Combo_ComboItemRule2Offered(kSsrSongs[0].bit) && !Combo_ComboItemRule2Offered(kSsrSongs[1].bit) &&
                      Combo_ComboItemRules2Refused(kSsrBoth, NULL, 0) == kSsrBoth,
                  "(5) RSBS_XITEM2_REVEAL_MM_SONGS no longer reads the production mask");
        // The full reveal offers both and refuses neither.
        Combo_ComboItemRules2RevealForTest(RSBS_XITEM2_REVEAL_ALL);
        SSR_CHECK(Combo_ComboItemRule2Offered(kSsrSongs[0].bit) && Combo_ComboItemRule2Offered(kSsrSongs[1].bit) &&
                      Combo_ComboItemRules2Refused(kSsrBoth, names, sizeof(names)) == 0u,
                  "(5) revealed, the two rules are not offered, or are refused ('%s')", names);
        Combo_ComboItemRules2RevealForTest(0);
    }

    // ---- (6) The restore through the API ------------------------------------------
    // Catches: a key the restore cannot write (a missing kRestorable row or
    // FrozenValue case), a hidden file restorable on the production mask, and a
    // load that keeps a session's stray key.
    {
        SsrClearKeys();
        SsrFreezeWorld(kSsrBoth);
        const uint32_t bits = Combo_ComboSettingsDivergence();
        const uint32_t frozen2 = Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2);
        SSR_CHECK(bits == RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS &&
                      Combo_ComboSettingsUnrestorableBitsEx2(&gComboCtx.comboItemRules, frozen2, bits) ==
                          RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS &&
                      Combo_ComboSettingsCanRestoreLiveEx2(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, frozen2,
                                                           &gComboCtx.comboTriforce, bits) == 0,
                  "(6) on the production mask a file with both hidden rules diverges %08X and is not refused",
                  (unsigned)bits);

        Combo_ComboItemRules2RevealForTest(1);
        SSR_CHECK(Combo_ComboSettingsCanRestoreLiveEx2(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, frozen2,
                                                       &gComboCtx.comboTriforce, bits) == 1,
                  "(6) revealed, a file with both rules on is not restorable");
        char names[192];
        ComboSettingsKeyUndo undo;
        const int ok = Combo_ComboSettingsRestoreLiveEx2(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, frozen2,
                                                         &gComboCtx.comboTriforce, bits, names, sizeof(names), &undo);
        printf("[TEST] (6) revealed restore: %d (%s)\n", ok, names);
        SSR_CHECK(ok == 1 && strcmp(names, "Shared Sun's Song, Shared Saria's Song") == 0,
                  "(6) the restore wrote '%s' (%d), not the two rows", names, ok);
        if (ids[0] != COMBO_SETTING_COUNT && ids[1] != COMBO_SETTING_COUNT) {
            SSR_CHECK(Combo_ComboSettingResolved(ids[0]) == 1 && Combo_ComboSettingResolved(ids[1]) == 1,
                      "(6) the restore did not put both keys to the file's 1");
        }
        SSR_CHECK(Combo_ComboSettingsDivergence() == 0u, "(6) after the restore the file still diverges by %08X",
                  (unsigned)Combo_ComboSettingsDivergence());
        Combo_ComboSettingsRestoreUndo(&undo);
        if (ids[0] != COMBO_SETTING_COUNT && ids[1] != COMBO_SETTING_COUNT) {
            SSR_CHECK(!Combo_ComboSettingIsExplicit(ids[0]) && !Combo_ComboSettingIsExplicit(ids[1]),
                      "(6) the undo did not put the keys back to unset");
        }
        Combo_ComboItemRules2RevealForTest(0);

        // A session's stray Sun key against a rules-off file, on the production
        // mask: put back to the file's "off" (frozen wins at load, #781).
        if (ids[0] != COMBO_SETTING_COUNT) {
            ComboContext_Init();
            SSR_CHECK(Combo_ComboSettingSet(ids[0], 1) == 1, "(6) the writer refused the stray Sun key");
            SsrFreezeWorld(0u);
            const uint32_t stray = Combo_ComboSettingsDivergence();
            SSR_CHECK(stray == RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS &&
                          Combo_ComboSettingsUnrestorableBitsEx2(&gComboCtx.comboItemRules, 0u, stray) == 0u,
                      "(6) a stray Sun key against a rules-off file must be a restorable divergence (%08X)",
                      (unsigned)stray);
            const int back =
                Combo_ComboSettingsRestoreLiveEx2(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, 0u,
                                                  &gComboCtx.comboTriforce, stray, names, sizeof(names), NULL);
            SSR_CHECK(back == 1 && strcmp(names, "Shared Sun's Song") == 0 && Combo_ComboSettingResolved(ids[0]) == 0 &&
                          Combo_ComboSettingsDivergence() == 0u,
                      "(6) the stray Sun key was not put back to the file's off (%d, '%s')", back, names);
        }
        SsrClearKeys();
        ComboContext_Init();
    }

    // ---- (7) The restore through a real .redsave ------------------------------------
    // Catches: a hidden file that loads on the production mask (or is quarantined
    // or touched by the refusal), and a revealed load that refuses or drops the
    // rules, or the second family word's bits 14-15 (S2's harvest relies on them).
    {
        rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
        const std::string priorSaveDir = std::filesystem::path(mgr.SlotPath(0)).parent_path().string();
        mgr.SetSaveDirectory(rsbs::test::ScratchDir(kSsrLoadDir).string());

        SsrClearKeys();
        RsbsSave_ResetSlotSessionState();
        mgr.DeleteSave(0);
        ComboContext_Init();
        SsrSeedShadows(0x58u);
        SsrFreezeWorld(kSsrBoth);
        SsrSeedShadows(0x58u);
        gComboCtx.sharedFamilyOwned2 = kSsrBoth;
        SSR_CHECK(mgr.Save(0), "(7) Save(0) failed for the Sun's and Saria's file");
        const std::vector<uint8_t> before = SsrReadBytes(mgr.SlotPath(0));

        // The production mask: refused as another build's, intact, not quarantined.
        RsbsSave_ResetSlotSessionState();
        ComboContext_Init();
        const bool hiddenLoaded = mgr.Load(0);
        const std::vector<uint8_t> after = SsrReadBytes(mgr.SlotPath(0));
        printf("[TEST] (7) hidden, keys unset: loaded %d, quarantine %d, file present %d, bytes untouched %d "
               "(expected 0, 0, 1, 1)\n",
               hiddenLoaded ? 1 : 0, RsbsSave_HasQuarantine(0), RsbsSave_HasSave(0),
               (!before.empty() && before == after) ? 1 : 0);
        SSR_CHECK(!hiddenLoaded && RsbsSave_HasQuarantine(0) == 0 && RsbsSave_HasSave(0) == 1 && !before.empty() &&
                      before == after,
                  "(7) A FILE WITH THE HIDDEN RULES LOADED on the production mask, or the refusal quarantined or "
                  "touched it");
        SsrClearKeys();
        ComboContext_Init();

        // Revealed, keys unset: it loads, frozen wins and both keys come back.
        Combo_ComboItemRules2RevealForTest(1);
        RsbsSave_ResetSlotSessionState();
        ComboContext_Init();
        const bool revealedLoaded = mgr.Load(0);
        printf("[TEST] (7) revealed, keys unset: loaded %d, word %08X, owned2 %08X (expected 1, 0000C000, 0000C000)\n",
               revealedLoaded ? 1 : 0, (unsigned)Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2),
               (unsigned)gComboCtx.sharedFamilyOwned2);
        SSR_CHECK(revealedLoaded && Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2) == kSsrBoth &&
                      Combo_ComboSettingsHashGeneratorVersionEx2(
                          &gComboCtx.comboSettings, &gComboCtx.comboItemRules, kSsrBoth,
                          gComboCtx.sharedRandoSettingsHash, gComboCtx.mmProfileDigest,
                          gComboCtx.comboSettingsHash) == (int)RSBS_GENERATOR_VERSION,
                  "(7) revealed, the file did not load with its word and its fingerprint (loaded %d)",
                  revealedLoaded ? 1 : 0);
        if (ids[0] != COMBO_SETTING_COUNT && ids[1] != COMBO_SETTING_COUNT) {
            SSR_CHECK(revealedLoaded && Combo_ComboSettingResolved(ids[0]) == 1 &&
                          Combo_ComboSettingResolved(ids[1]) == 1,
                      "(7) the load did not restore the file's Sun's and Saria's keys");
        }
        SSR_CHECK(revealedLoaded && gComboCtx.sharedFamilyOwned2 == kSsrBoth,
                  "(7) THE SECOND FAMILY WORD did not carry bits 14-15 through Save/Load: owned2 %08X, want 0000C000",
                  (unsigned)gComboCtx.sharedFamilyOwned2);
        Combo_ComboItemRules2RevealForTest(0);

        SsrClearKeys();
        RsbsSave_ResetSlotSessionState();
        mgr.DeleteSave(0);
        mgr.SetSaveDirectory(priorSaveDir);
        ComboContext_Init();
    }

    Combo_ComboItemRules2RevealForTest(0);
    SsrClearKeys();
    ComboContext_Init();
    printf("[TEST] shared-sun-saria-rule: %s\n", sSsrFailures == 0 ? "PASS" : "FAIL");
    return sSsrFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// shared-sun-saria-merge (#1587 S2)
// ============================================================================

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
int MM_SharedSongsTest_Held(int song);
const char* MM_WarpSongsTest_Name(uint16_t ri);
// games/oot/soh/oot_shared_songs_test.cpp and games/mm/2s2h/mm_shared_songs_test.cpp:
// `song` 0 is the Sun's Song, 1 Saria's Song.
void OoT_SharedSunSariaTest_SetHeld(int song, int held);
int OoT_SharedSunSariaTest_Held(int song);
int OoT_SharedSunSariaTest_PoolId(int song);
uint32_t OoT_SharedSunSariaTest_QuestBit(int song);
uint32_t OoT_SharedSunSariaTest_SongBits(void);
void MM_SharedSunSariaTest_SetHeld(int song, int held);
int MM_SharedSunSariaTest_Held(int song);
int MM_SharedSunSariaTest_PoolId(int song);
uint32_t MM_SharedSunSariaTest_QuestBit(int song);
uint32_t MM_SharedSunSariaTest_SongBits(void);
void MM_SharedSunSariaTest_SetRow(int song, uint32_t value);
#ifdef __cplusplus
}
#endif

namespace {
const char* const kSsrSongNames[2] = { "Sun's Song", "Saria's Song" };
const uint32_t kSsrSharedEponaWord1 = 0x00400000u; // the first word's Shared Epona's Song (#1066)
const uint32_t kSsrAllWarp = 0x00003FFFu;          // every warp-song rule of the second word (#1169)

/** SsrFreezeWorld with a first-word rule word too (@p rules1), for the Epona leg. */
void SsrFreezeWorld2(uint32_t rules1, uint32_t rules2) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = kSsrSeed;
    gComboCtx.sharedRandoSettingsHash = kSsrSettingsHash;
    gComboCtx.mmProfileDigest = kSsrProfileDigest;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    ComboItemRulesRecord r1;
    memset(&r1, 0, sizeof(r1));
    r1.crossItemFlags = rules1;
    Combo_FreezeComboSettingsEx2(&rec, &r1, rules2);
}

/** Both games' live saves armed and empty (no song, MM's own rows at 0). */
void SsrForgetBoth() {
    OoT_SharedSwordTest_ArmLive();
    MM_SharedSwordTest_ArmLive();
}
} // namespace

TestResult SharedSunSariaMerge_Run(void) {
    printf("[TEST] shared-sun-saria-merge: both games register their own Sun's Song and Saria's Song as the shared "
           "families' members; MM's pooled follows the frozen rule, so the pair is built with MM's rows at 0; through "
           "the real shims a song learnt in either game is known in the other at arrival, at the load seam and at "
           "pickup, each rule only its own song (#1587 S2)\n");
    sSsrFailures = 0;

    // ---- (0) The registrations -------------------------------------------------------------
    // Catches: a missing or misplaced registration, and the wrong id.
    for (int song = 0; song < 2; song++) {
        const uint32_t bit = kSsrSongs[song].bit;
        uint16_t ootId = 0;
        uint16_t mmId = 0;
        const bool ootReg = Combo_SharedFamily2MemberId(GAME_OOT, bit, &ootId);
        const bool mmReg = Combo_SharedFamily2MemberId(GAME_MM, bit, &mmId);
        SSR_CHECK(ootReg && (int)ootId == OoT_SharedSunSariaTest_PoolId(song) && mmReg &&
                      (int)mmId == MM_SharedSunSariaTest_PoolId(song),
                  "(0) the shared %s: OoT member %d (id %u, want %d), MM member %d (id %u, want %d)",
                  kSsrSongNames[song], ootReg ? 1 : 0, (unsigned)ootId, OoT_SharedSunSariaTest_PoolId(song),
                  mmReg ? 1 : 0, (unsigned)mmId, MM_SharedSunSariaTest_PoolId(song));
    }

    // ---- (1) pooled follows the frozen rule, never MM's own rows -------------------------------
    // Catches: an MM pooled that is NULL or always true, the two bits swapped, and
    // a pooled that ORs in MM's Shuffle Sun's Song / Shuffle Saria's Song rows.
    {
        SsrForgetBoth();
        for (int song = 0; song < 2; song++) {
            SsrFreezeWorld(kSsrSongs[song].bit);
            const bool mine = Combo_SharedFamily2MemberPooled(GAME_MM, kSsrSongs[song].bit);
            const bool other = Combo_SharedFamily2MemberPooled(GAME_MM, kSsrSongs[1 - song].bit);
            SSR_CHECK(mine && !other,
                      "(1) frozen %08X with MM's rows at 0: MM's %s pooled %d (want 1), MM's %s pooled %d (want 0)",
                      (unsigned)kSsrSongs[song].bit, kSsrSongNames[song], mine ? 1 : 0, kSsrSongNames[1 - song],
                      other ? 1 : 0);
        }
        SsrFreezeWorld(0u);
        MM_SharedSunSariaTest_SetRow(0, 1u);
        MM_SharedSunSariaTest_SetRow(1, 1u);
        const bool sunOn = Combo_SharedFamily2MemberPooled(GAME_MM, kSsrSongs[0].bit);
        const bool sariaOn = Combo_SharedFamily2MemberPooled(GAME_MM, kSsrSongs[1].bit);
        SSR_CHECK(!sunOn && !sariaOn,
                  "(1) both rules off with MM's own rows at 1: MM's members answered pooled (Sun %d, Saria's %d); "
                  "pooled must read the frozen rule only",
                  sunOn ? 1 : 0, sariaOn ? 1 : 0);
        SSR_CHECK(Combo_SharedFamily2MemberPooled(GAME_OOT, kSsrSongs[0].bit) &&
                      Combo_SharedFamily2MemberPooled(GAME_OOT, kSsrSongs[1].bit),
                  "(1) OoT's members answered not pooled (OoT's shape is pooled NULL, always pooled)");
        SsrForgetBoth();
    }

    // ---- (2) The pair is built with MM's rows at 0 (the key leg) -------------------------------
    // Catches: a pooled that reads MM's own options (0 in every paired profile
    // before #1587 S3), which would never build the pair.
    {
        SsrForgetBoth();
        SsrFreezeWorld(kSsrBoth);
        ComboLogicSharedPair pairs[RSBS_XFAMILY_SLOT_COUNT];
        memset(pairs, 0, sizeof(pairs));
        const int n = Combo_SingleBag_SharedPairs(pairs, (int)RSBS_XFAMILY_SLOT_COUNT);
        printf("[TEST] (2) frozen 0000C000, MM's rows 0: %d pairs, [0] %u/%u, [1] %u/%u\n", n, (unsigned)pairs[0].ootId,
               (unsigned)pairs[0].mmId, (unsigned)pairs[1].ootId, (unsigned)pairs[1].mmId);
        SSR_CHECK(n == 2 && (int)pairs[0].ootId == OoT_SharedSunSariaTest_PoolId(0) &&
                      (int)pairs[0].mmId == MM_SharedSunSariaTest_PoolId(0) &&
                      (int)pairs[1].ootId == OoT_SharedSunSariaTest_PoolId(1) &&
                      (int)pairs[1].mmId == MM_SharedSunSariaTest_PoolId(1),
                  "(2) the bag built %d pairs; want exactly the Sun's Song then Saria's Song, each (OoT id, MM id)", n);
        SsrFreezeWorld(0u);
        const int none = Combo_SingleBag_SharedPairs(pairs, (int)RSBS_XFAMILY_SLOT_COUNT);
        SSR_CHECK(none == 0, "(2) with every rule off the bag built %d pairs", none);
    }

    // ---- (3) Learnt in OoT, known in MM; (4) learnt in MM, known in OoT, and the load seam ------
    // Catches: an MM or OoT give that writes nothing, another song's bit or extra
    // songs; a held that reads the wrong bit; and the #1163 load seam skipping them.
    for (int song = 0; song < 2; song++) {
        const uint32_t bit = kSsrSongs[song].bit;
        SsrFreezeWorld(bit);
        SsrForgetBoth();
        OoT_SharedSwordTest_ArmLive();
        OoT_SharedSunSariaTest_SetHeld(song, 1);
        OoT_HarvestSharedResources();
        const uint32_t afterOoT = Combo_SharedFamilies2Owned();
        MM_SharedSwordTest_ArmLive();
        MM_ApplySharedResources();
        printf("[TEST] (3) %s OoT -> MM: pool %08X, MM song bits %08X (want %08X)\n", kSsrSongNames[song],
               (unsigned)afterOoT, (unsigned)MM_SharedSunSariaTest_SongBits(),
               (unsigned)MM_SharedSunSariaTest_QuestBit(song));
        SSR_CHECK(afterOoT == bit && Combo_SharedFamiliesOwned() == 0u,
                  "(3) OoT's harvest put %08X for the %s, want exactly %08X and nothing in the first word",
                  (unsigned)afterOoT, kSsrSongNames[song], (unsigned)bit);
        SSR_CHECK(MM_SharedSunSariaTest_Held(song) == 1 &&
                      MM_SharedSunSariaTest_SongBits() == MM_SharedSunSariaTest_QuestBit(song),
                  "(3) MM's apply did not give MM the %s alone: song bits %08X", kSsrSongNames[song],
                  (unsigned)MM_SharedSunSariaTest_SongBits());
        SSR_CHECK(Combo_SharedFamilies2GivenAtLastApply(GAME_MM) == bit,
                  "(3) MM's give record reads %08X after giving the %s, want %08X",
                  (unsigned)Combo_SharedFamilies2GivenAtLastApply(GAME_MM), kSsrSongNames[song], (unsigned)bit);

        SsrFreezeWorld(bit);
        SsrForgetBoth();
        MM_SharedSwordTest_ArmLive();
        MM_SharedSunSariaTest_SetHeld(song, 1);
        MM_HarvestSharedResources();
        OoT_SharedSwordTest_ArmLive();
        OoT_ApplySharedResources();
        printf("[TEST] (4) %s MM -> OoT: pool %08X, OoT song bits %08X (want %08X)\n", kSsrSongNames[song],
               (unsigned)Combo_SharedFamilies2Owned(), (unsigned)OoT_SharedSunSariaTest_SongBits(),
               (unsigned)OoT_SharedSunSariaTest_QuestBit(song));
        SSR_CHECK(Combo_SharedFamilies2Owned() == bit && OoT_SharedSunSariaTest_Held(song) == 1 &&
                      OoT_SharedSunSariaTest_SongBits() == OoT_SharedSunSariaTest_QuestBit(song),
                  "(4) MM's %s did not reach OoT alone through the shims (pool %08X, OoT song bits %08X)",
                  kSsrSongNames[song], (unsigned)Combo_SharedFamilies2Owned(),
                  (unsigned)OoT_SharedSunSariaTest_SongBits());
        SSR_CHECK(Combo_SharedFamilies2GivenAtLastApply(GAME_OOT) == bit,
                  "(4) OoT's give record reads %08X after giving the %s, want %08X",
                  (unsigned)Combo_SharedFamilies2GivenAtLastApply(GAME_OOT), kSsrSongNames[song], (unsigned)bit);
        // The .redsave load seam (#1163): OoT's half applied before its own save opens.
        OoT_SharedSwordTest_ArmLive();
        OoT_ApplySharedResourcesAtFileLoad();
        SSR_CHECK(OoT_SharedSunSariaTest_Held(song) == 1 &&
                      OoT_SharedSunSariaTest_SongBits() == OoT_SharedSunSariaTest_QuestBit(song),
                  "(4) OoT's load seam did not give the %s alone (song bits %08X)", kSsrSongNames[song],
                  (unsigned)OoT_SharedSunSariaTest_SongBits());
    }

    // ---- (5) Each rule carries only its own song -----------------------------------------------
    // Catches: aliased bits, and a member registered at the other song's bit.
    for (int song = 0; song < 2; song++) {
        const uint32_t bit = kSsrSongs[song].bit;
        SsrFreezeWorld(bit);
        SsrForgetBoth();
        OoT_SharedSwordTest_ArmLive();
        OoT_SharedSunSariaTest_SetHeld(0, 1);
        OoT_SharedSunSariaTest_SetHeld(1, 1);
        OoT_HarvestSharedResources();
        MM_SharedSwordTest_ArmLive();
        MM_ApplySharedResources();
        SSR_CHECK(Combo_SharedFamilies2Owned() == bit && MM_SharedSunSariaTest_Held(song) == 1 &&
                      MM_SharedSunSariaTest_Held(1 - song) == 0,
                  "(5) with only the shared %s frozen and OoT holding both: pool %08X (want %08X), MM holds the %s "
                  "%d (want 1) and the %s %d (want 0)",
                  kSsrSongNames[song], (unsigned)Combo_SharedFamilies2Owned(), (unsigned)bit, kSsrSongNames[song],
                  MM_SharedSunSariaTest_Held(song), kSsrSongNames[1 - song], MM_SharedSunSariaTest_Held(1 - song));
    }

    // ---- (6) Rules off carry nothing -----------------------------------------------------------
    // Catches: an armed mask not driven by the frozen word, and the warp-song rules
    // leaking into these two bits.
    {
        const uint32_t worlds[2] = { 0u, kSsrAllWarp };
        for (int w = 0; w < 2; w++) {
            SsrFreezeWorld(worlds[w]);
            SsrForgetBoth();
            OoT_SharedSwordTest_ArmLive();
            OoT_SharedSunSariaTest_SetHeld(0, 1);
            OoT_SharedSunSariaTest_SetHeld(1, 1);
            OoT_HarvestSharedResources();
            MM_SharedSwordTest_ArmLive();
            MM_ApplySharedResources();
            const uint32_t owned2 = Combo_SharedFamilies2Owned();
            const uint32_t given = Combo_SharedFamilies2GivenAtLastApply(GAME_MM);
            SSR_CHECK((owned2 & kSsrBoth) == 0u && (worlds[w] != 0u || owned2 == 0u) &&
                          MM_SharedSunSariaTest_Held(0) == 0 && MM_SharedSunSariaTest_Held(1) == 0 &&
                          (given & kSsrBoth) == 0u,
                      "(6) frozen %08X with OoT holding both songs: pool %08X, MM holds Sun %d / Saria's %d, MM's "
                      "give record %08X; want none of them carried",
                      (unsigned)worlds[w], (unsigned)owned2, MM_SharedSunSariaTest_Held(0),
                      MM_SharedSunSariaTest_Held(1), (unsigned)given);
        }
    }

    // ---- (7) No crossover with Epona's Song ------------------------------------------------------
    // Catches: helpers built on OoT_SharedSongQuestBit / MM_SharedSongQuestBit (they
    // answer Epona's bit), and a member misregistered in the first word at Epona's slot.
    {
        SsrFreezeWorld2(kSsrSharedEponaWord1, kSsrBoth);
        SsrForgetBoth();
        OoT_SharedSwordTest_ArmLive();
        OoT_SharedSunSariaTest_SetHeld(0, 1); // the Sun's Song only
        OoT_HarvestSharedResources();
        MM_SharedSwordTest_ArmLive();
        MM_ApplySharedResources();
        SSR_CHECK(Combo_SharedFamilies2Owned() == kSsrSongs[0].bit &&
                      (Combo_SharedFamiliesOwned() & kSsrSharedEponaWord1) == 0u && MM_SharedSongsTest_Held(0) == 0 &&
                      MM_SharedSunSariaTest_Held(0) == 1,
                  "(7) OoT holding the Sun's Song alone: second word %08X (want 00004000), first %08X (want no "
                  "Epona), MM holds Epona's Song %d (want 0) and the Sun's Song %d (want 1)",
                  (unsigned)Combo_SharedFamilies2Owned(), (unsigned)Combo_SharedFamiliesOwned(),
                  MM_SharedSongsTest_Held(0), MM_SharedSunSariaTest_Held(0));

        SsrFreezeWorld2(kSsrSharedEponaWord1, kSsrBoth);
        SsrForgetBoth();
        OoT_SharedSwordTest_ArmLive();
        OoT_SharedSongsTest_SetHeld(0, 1); // Epona's Song only
        OoT_HarvestSharedResources();
        MM_SharedSwordTest_ArmLive();
        MM_ApplySharedResources();
        SSR_CHECK(Combo_SharedFamilies2Owned() == 0u && (Combo_SharedFamiliesOwned() & kSsrSharedEponaWord1) != 0u &&
                      MM_SharedSunSariaTest_Held(0) == 0 && MM_SharedSunSariaTest_Held(1) == 0,
                  "(7) OoT holding Epona's Song alone: second word %08X (want 0), first %08X (want Epona), MM holds "
                  "Sun %d / Saria's %d (want 0 / 0)",
                  (unsigned)Combo_SharedFamilies2Owned(), (unsigned)Combo_SharedFamiliesOwned(),
                  MM_SharedSunSariaTest_Held(0), MM_SharedSunSariaTest_Held(1));
    }

    // ---- (8) A far copy found here is credited at pickup, no crossing ----------------------------
    // Catches: a far copy that crosses instead of being credited (a round trip),
    // and an MM member id that is not the one MM's pool places.
    {
        SharedItem item;
        memset(&item, 0, sizeof(item));

        SsrFreezeWorld(kSsrBoth);
        SsrForgetBoth();
        OoT_SharedSwordTest_ArmLive();
        item.originGame = (uint8_t)GAME_MM;
        item.id = (uint16_t)MM_SharedSunSariaTest_PoolId(0);
        const int ootSun = OoT_CreditSharedPickup(&item);
        SSR_CHECK(ootSun == 1 && OoT_SharedSunSariaTest_Held(0) == 1 &&
                      (Combo_SharedFamilies2Owned() & kSsrSongs[0].bit) != 0u,
                  "(8) MM's Sun's Song found in Hyrule: credited %d, OoT holds it %d, pool %08X; want credited at "
                  "once with OoT's own song",
                  ootSun, OoT_SharedSunSariaTest_Held(0), (unsigned)Combo_SharedFamilies2Owned());

        SsrFreezeWorld(kSsrBoth);
        SsrForgetBoth();
        MM_SharedSwordTest_ArmLive();
        item.originGame = (uint8_t)GAME_OOT;
        item.id = (uint16_t)OoT_SharedSunSariaTest_PoolId(1);
        const int mmSaria = MM_CreditSharedPickup(&item);
        SSR_CHECK(mmSaria == 1 && MM_SharedSunSariaTest_Held(1) == 1 &&
                      (Combo_SharedFamilies2Owned() & kSsrSongs[1].bit) != 0u,
                  "(8) OoT's Saria's Song found in Termina: credited %d, MM holds it %d, pool %08X; want credited at "
                  "once with MM's own song",
                  mmSaria, MM_SharedSunSariaTest_Held(1), (unsigned)Combo_SharedFamilies2Owned());

        SsrFreezeWorld(0u);
        SsrForgetBoth();
        OoT_SharedSwordTest_ArmLive();
        item.originGame = (uint8_t)GAME_MM;
        item.id = (uint16_t)MM_SharedSunSariaTest_PoolId(0);
        const int ootOff = OoT_CreditSharedPickup(&item);
        MM_SharedSwordTest_ArmLive();
        item.originGame = (uint8_t)GAME_OOT;
        item.id = (uint16_t)OoT_SharedSunSariaTest_PoolId(1);
        const int mmOff = MM_CreditSharedPickup(&item);
        SSR_CHECK(ootOff == 0 && mmOff == 0 && OoT_SharedSunSariaTest_Held(0) == 0 &&
                      MM_SharedSunSariaTest_Held(1) == 0 && Combo_SharedFamilies2Owned() == 0u,
                  "(8) with both rules off a far copy was credited (OoT %d, MM %d) instead of crossing", ootOff, mmOff);
    }

    // ---- (9) Names: a shared copy is untagged and keeps its own name -----------------------------
    // Catches: a shared far copy that keeps its game tag, and a display name that
    // renames a shared Sun's Song or Saria's Song.
    {
        const uint16_t ootSun = (uint16_t)OoT_SharedSunSariaTest_PoolId(0);
        const uint16_t ootSaria = (uint16_t)OoT_SharedSunSariaTest_PoolId(1);
        const uint16_t mmSun = (uint16_t)MM_SharedSunSariaTest_PoolId(0);
        const uint16_t mmSaria = (uint16_t)MM_SharedSunSariaTest_PoolId(1);
        SsrFreezeWorld(kSsrBoth);
        SSR_CHECK(Combo_SharedFamily2DisplayName((uint8_t)GAME_OOT, ootSun) == NULL &&
                      Combo_SharedFamily2DisplayName((uint8_t)GAME_OOT, ootSaria) == NULL &&
                      Combo_SharedFamily2DisplayName((uint8_t)GAME_MM, mmSun) == NULL &&
                      Combo_SharedFamily2DisplayName((uint8_t)GAME_MM, mmSaria) == NULL,
                  "(9) a shared Sun's Song or Saria's Song was given a display name of its own (renamed)");
        const char* sunName = MM_WarpSongsTest_Name(mmSun);
        const char* sariaName = MM_WarpSongsTest_Name(mmSaria);
        SSR_CHECK(sunName != NULL && strcmp(sunName, "Sun's Song") == 0 && sariaName != NULL &&
                      strcmp(sariaName, "Saria's Song") == 0,
                  "(9) MM names its songs '%s' and '%s', want \"Sun's Song\" and \"Saria's Song\"",
                  sunName != NULL ? sunName : "(null)", sariaName != NULL ? sariaName : "(null)");
        const char* sharedMM = Combo_ForeignItemTag((uint8_t)GAME_MM, mmSun, "Sun's Song");
        const char* sharedOoT = Combo_ForeignItemTag((uint8_t)GAME_OOT, ootSaria, "Saria's Song");
        SSR_CHECK(strcmp(sharedMM, "") == 0 && strcmp(sharedOoT, "") == 0,
                  "(9) shared, MM's Sun's Song is tagged '%s' and OoT's Saria's Song '%s'; want both untagged",
                  sharedMM, sharedOoT);
        SsrFreezeWorld(0u);
        const char* ownMM = Combo_ForeignItemTag((uint8_t)GAME_MM, mmSun, "Sun's Song");
        const char* ownOoT = Combo_ForeignItemTag((uint8_t)GAME_OOT, ootSaria, "Saria's Song");
        SSR_CHECK(strcmp(ownMM, " (MM)") == 0 && strcmp(ownOoT, " (OoT)") == 0,
                  "(9) not shared, MM's Sun's Song is tagged '%s' and OoT's Saria's Song '%s'; want ' (MM)' and "
                  "' (OoT)'",
                  ownMM, ownOoT);
    }

    // ---- (10) Saria's Song stays permanent -------------------------------------------------------
    // Catches: a harvest that assigns instead of ORing (a song MM lost would leave
    // the pool), and a give that only toggles.
    {
        const uint32_t saria = kSsrSongs[1].bit;
        SsrFreezeWorld(saria);
        SsrForgetBoth();
        OoT_SharedSwordTest_ArmLive();
        OoT_SharedSunSariaTest_SetHeld(1, 1);
        OoT_HarvestSharedResources();
        MM_SharedSwordTest_ArmLive();
        MM_ApplySharedResources();
        SSR_CHECK(MM_SharedSunSariaTest_Held(1) == 1, "(10) MM's apply did not give Saria's Song");
        MM_ApplySharedResources();
        SSR_CHECK(Combo_SharedFamilies2GivenAtLastApply(GAME_MM) == 0u && MM_SharedSunSariaTest_Held(1) == 1,
                  "(10) a second MM apply gave %08X (want nothing owed) or took the song away",
                  (unsigned)Combo_SharedFamilies2GivenAtLastApply(GAME_MM));
        MM_SharedSunSariaTest_SetHeld(1, 0); // as 2Ship's one-use removal would
        MM_HarvestSharedResources();
        const uint32_t afterLoss = Combo_SharedFamilies2Owned();
        MM_ApplySharedResources();
        SSR_CHECK((afterLoss & saria) != 0u && MM_SharedSunSariaTest_Held(1) == 1 &&
                      Combo_SharedFamilies2GivenAtLastApply(GAME_MM) == saria,
                  "(10) after MM lost Saria's Song the pool reads %08X and the next apply gave %08X (MM holds it %d); "
                  "want the family kept and the song given back",
                  (unsigned)afterLoss, (unsigned)Combo_SharedFamilies2GivenAtLastApply(GAME_MM),
                  MM_SharedSunSariaTest_Held(1));
    }

    SsrFreezeWorld(0u);
    SsrForgetBoth();
    ComboContext_Init();
    printf("[TEST] shared-sun-saria-merge: %s\n", sSsrFailures == 0 ? "PASS" : "FAIL");
    return sSsrFailures == 0 ? TEST_PASS : TEST_FAIL;
}
