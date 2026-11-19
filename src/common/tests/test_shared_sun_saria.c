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
 * Every key is spelled from the identity prefix rather than as a literal, so
 * the tier-4 classification scan sees only the manifest's spelling.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * like its neighbours; every symbol under test is C-linkage. Helpers carry the
 * Ssr prefix because every included test file shares one translation unit.
 */

#include "../combo_settings_view.h"
#include "../context.h"
#include "../cvar_shared_keys.h"
#include "../foreign_items.h"
#include "../save.h"
#include "../shared_resources.h"
#include "../test_runner.h"
#include "../test_scratch.h"

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
 *  key. S2 extends this table with each game's item. */
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
        SSR_CHECK(RSBS_XITEM2_ALL_V1 == 0x0000FFFFu && RSBS_XITEM2_FAMILY_MASK == 0x0000FF80u &&
                      RSBS_XITEM2_SHARED_SONGS == 0x0000FF80u,
                  "(1) ALL_V1 %08X (want 0000FFFF), FAMILY_MASK %08X and SHARED_SONGS %08X (want 0000FF80)",
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
        SSR_CHECK(Combo_ComboItemRule2KeyedBits() == RSBS_XITEM2_ALL_V1,
                  "(2) the keyed second-word bits are %08X, expected every allocated bit (0000FFFF)",
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
    // apart, and a world with both off whose fingerprint moved.
    {
        for (uint32_t v = 0; v <= RSBS_GENERATOR_VERSION; v++) {
            const uint32_t without =
                Combo_ComputeComboSettingsHashForGeneratorEx(&defaults, NULL, kSsrSettingsHash, kSsrProfileDigest, v);
            const uint32_t withZero = Combo_ComputeComboSettingsHashForGeneratorEx2(
                &defaults, NULL, 0u, kSsrSettingsHash, kSsrProfileDigest, v);
            SSR_CHECK(without == withZero, "(4) at generator %u a 0 second word gives %08X, the word-blind %08X",
                      (unsigned)v, (unsigned)withZero, (unsigned)without);
        }
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
        SSR_CHECK(RSBS_XITEM2_HIDDEN == kSsrBoth, "(5) the hidden mask is %08X, want 0000C000 until #1587 S7",
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
