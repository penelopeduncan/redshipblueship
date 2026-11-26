/**
 * @file test_combo_bag_shuffle.c
 * @brief Lock for the pair's one Deku Stick Bag / Deku Nut Bag shuffle as frozen
 *        world identity (#1652 stage A0; operator ruling 2026-11-15, option A).
 *
 * One CTest row, combo-bag-shuffle-frozen (ComboBagShuffleFrozen, redship tier,
 * ROM-free; it writes a CVar and a real .redsave, so its wrapper brings up the
 * display-free shared context first):
 *
 *   (1) THE BITS: RSBS_COMBO_FLAG_SHUFFLED_STICK_BAG is comboFlags 0x08 and
 *       _NUT_BAG 0x10, both allocated, and no key authors them (the resolver
 *       leaves them clear).
 *   (2) THE CREATION: Combo_ComboSettingsApplyCreationBagShuffles sets each bit
 *       exactly from its own argument and leaves every other bit alone; a world
 *       with both shuffles off keeps the fingerprint of the defaults record, and
 *       each bit on moves it.
 *   (3) THE PREDICATES: nothing frozen (an unpaired file) reads false; a paired
 *       world with the shuffles off reads false; each bit frozen on reads true
 *       for its own bag only; a live change of OoT's option (its CVar) after the
 *       creation changes neither answer.
 *   (4) THE COMPARE: a world frozen with either bit is no divergence against a
 *       session (no key authors them, so the file's bits are the session's).
 *   (5) THE CARRIERS: each bit, on and off, survives the creation snapshot and
 *       rollback, the file-create KEEP invalidation, the seed package's world
 *       record (WorldRecordBytes -> ApplyComboWorld), and a real .redsave
 *       Save/Load.
 *
 * #1702 stage C0 (operator ruling 2026-11-17) adds the pair's one Child's Wallet
 * and Bombchu Bag shuffle the same way, legs (1)-(5) each run again over
 * kCbsWorlds2: RSBS_COMBO_FLAG_SHUFFLED_CHILD_WALLET 0x20 and _BOMBCHU_BAG 0x40,
 * both allocated and creation-derived (exact masks, and a raw difference passed
 * to the compare reported at the byte both ways); the sibling helper
 * (Combo_ComboSettingsApplyCreationWalletChuShuffles) sets only its own two bits
 * and the bag helper leaves them alone; the predicates read the frozen record
 * only. Then:
 *   (6) THE ONE-GAME PIN: a file frozen with neither bit reads false forever,
 *       through OoT's options turned on afterwards and a .redsave round trip.
 *   (7) FROM SOURCE: Playthrough_Init writes the two bits from
 *       RSK_SHUFFLE_CHILD_WALLET and RSK_BOMBCHU_BAG (not None), once, before
 *       the freeze (the creation needs ROMs; this is its ROM-free guard).
 *
 * Nothing in production reads the two #1702 predicates yet; its later stages do.
 *
 * #1702 stage C2 (the redrawn plan: shared when sharing is enabled, split
 * otherwise) adds three legs:
 *   (8) ONE GAME: a world frozen with 0x20 / 0x40 and no new bit (every v0.5.2 and
 *       v0.5.3 file) arms no possession slot and reads MM_Combo_ChildWalletShuffled,
 *       MM_Combo_WalletGated, MM_Combo_ChuBagShared, Combo_ComboBombchuBagShared
 *       and Combo_ComboOwnChildWallets all 0, with MM's flags 80/81 set or clear;
 *       through a real .redsave it loads, keeps 0x20 | 0x40 and the owned
 *       possession bits 0x600, and MM's apply gives no flag.
 *   (9) THE RULE: the Shared Child's Wallet key (appended id, label, default 0,
 *       exactly 0 or 1, bit 0x10000), hidden (not offered, refused by name;
 *       offered under the test reveal), resolved alone, folded into the
 *       fingerprint, arming the wallet slot and the MM predicates when frozen, a
 *       stray key put back, a .redsave refused hidden and restored revealed, and
 *       from source Generate's refusal before the freeze.
 *   (10) THE DERIVED MARKER and THE RESERVED BAG BIT: 0x80000 is keyed by
 *       nothing, never resolved, never offered (even revealed), refused by name,
 *       and a .redsave holding it is refused even revealed; a .redsave with
 *       comboFlags 0x80 is refused at load.
 *
 * #1702 stage C7 (the shared Bombchu Bag tier, armed by 0x80) adds:
 *   (11) THE BIT STAYS UNWRITTEN: the seed package's refusal helper answers
 *       exactly the unallocated bits; from source, no production writer names
 *       0x80 (foreign_items.c holds only the predicate, playthrough.cpp nothing),
 *       OoT's Bombchu Bag mode stand-in is defined once and called by no
 *       production file, and the package's Validate refuses an unallocated flag
 *       as a FORMAT error.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * like its neighbours; every symbol under test is C-linkage. Helpers carry the
 * Cbs prefix because every included test file shares one translation unit.
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

#ifdef __cplusplus
extern "C" {
#endif
// games/oot/soh/Enhancements/randomizer/SeedPackageSingleExe.cpp: the package's
// world-record copy and its apply, end to end.
int OoT_SeedPackage_WorldRecordRoundTripForTest(const ComboContext* live, ComboContext* outRecordBytes);
// #1702 C2, legs (8)-(10): MM's live save and possession flags
// (mm_shared_kokiri_sword_test.cpp, mm_shared_swim_test.cpp), MM's real apply
// shim, and the three MM predicates (2s2h/Rando/WalletChuGate.h, defined in MM's
// GameExports_SingleExe.cpp).
void MM_SharedSwordTest_ArmLive(void);
void MM_ApplySharedResources(void);
void MM_SharedPossessionTest_Set(int wallet, int bag);
int MM_SharedPossessionTest_Held(void);
int MM_Combo_ChildWalletShuffled(void);
int MM_Combo_WalletGated(void);
int MM_Combo_ChuBagShared(void);
#ifdef __cplusplus
}
#endif

static int sCbsFailures;

#define CBS_CHECK(cond, ...)                          \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sCbsFailures++;                           \
        }                                             \
    } while (0)

namespace {

// The bits, spelled as literals: they are .redsave and seed-package format, so the
// lock states them rather than reading them back from the header under test.
const uint8_t kCbsStick = 0x08u;
const uint8_t kCbsNut = 0x10u;
const uint32_t kCbsSettingsHash = 0x1652BA65u;
const uint32_t kCbsProfileDigest = 0x1652D16Eu;
const char* const kCbsLoadDir = "rsbs_test_combo_bag_shuffle_load";
// OoT's own options' keys (CVAR_RANDOMIZER_SETTING): what a player changes after the creation.
const char* const kCbsStickKey = "gRandoSettings.ShuffleDekuStickBag";
const char* const kCbsNutKey = "gRandoSettings.ShuffleDekuNutBag";
// #1702 C0: the Child's Wallet and Bombchu Bag bits, and OoT's two options' keys.
const uint8_t kCbsWallet = 0x20u;
const uint8_t kCbsChu = 0x40u;
const char* const kCbsWalletKey = "gRandoSettings.ShuffleChildWallet";
const char* const kCbsChuKey = "gRandoSettings.BombchuBag";
// #1702 C2: the Shared Child's Wallet rule, the derived own-child-wallets marker,
// the reserved Shared Bombchu Bag comboFlags bit, and the two possession slots.
const uint32_t kCbsRule = 0x00010000u;
const uint32_t kCbsDerived = 0x00080000u;
const uint8_t kCbsSharedChu = 0x80u;
const uint32_t kCbsPossession = 0x00000600u;

/** Freeze a paired world the way the creation event does, with @p stick and
 *  @p nut as OoT's two bag options. */
void CbsFreezeWorld(bool stick, bool nut) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x1652F00Du;
    gComboCtx.sharedRandoSettingsHash = kCbsSettingsHash;
    gComboCtx.mmProfileDigest = kCbsProfileDigest;
    ComboSettingsRecord rec;
    Combo_ResolveComboSettings(&rec);
    Combo_ComboSettingsApplyCreationBagShuffles(&rec, stick, nut);
    Combo_FreezeComboSettingsEx2(&rec, NULL, 0u);
}

bool CbsReads(bool stick, bool nut) {
    return Combo_ComboStickBagShuffled() == stick && Combo_ComboNutBagShuffled() == nut;
}

/** #1702 C0: freeze a paired world the way the creation event does, with
 *  @p wallet and @p chu as OoT's Shuffle Child's Wallet and Bombchu Bag (not
 *  None). The bag helper runs first with the stick bag on and the nut bag off,
 *  as the creation calls both, so a wallet helper that writes the whole byte
 *  shows as a lost stick bit. */
void CbsFreezeWorld2(bool wallet, bool chu) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x1702F00Du;
    gComboCtx.sharedRandoSettingsHash = kCbsSettingsHash;
    gComboCtx.mmProfileDigest = kCbsProfileDigest;
    ComboSettingsRecord rec;
    Combo_ResolveComboSettings(&rec);
    Combo_ComboSettingsApplyCreationBagShuffles(&rec, true, false);
    Combo_ComboSettingsApplyCreationWalletChuShuffles(&rec, wallet, chu);
    Combo_FreezeComboSettingsEx2(&rec, NULL, 0u);
}

/** The two #1702 predicates alone. */
bool CbsReadsWalletChu(bool wallet, bool chu) {
    return Combo_ComboChildWalletShuffled() == wallet && Combo_ComboBombchuBagShuffled() == chu;
}

/** A CbsFreezeWorld2 world: its two #1702 bits, and the bag bits it froze beside them. */
bool CbsReads2(bool wallet, bool chu) {
    return CbsReadsWalletChu(wallet, chu) && CbsReads(true, false);
}

/** #1702 C2: freeze a paired world from the resolved record with @p flags OR'd
 *  into its comboFlags and @p rules2 as the second rules word, directly (no
 *  Generate, so nothing is refused). */
void CbsFreezeWorldC2(uint8_t flags, uint32_t rules2) {
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x1702C2F0u;
    gComboCtx.sharedRandoSettingsHash = kCbsSettingsHash;
    gComboCtx.mmProfileDigest = kCbsProfileDigest;
    ComboSettingsRecord rec;
    Combo_ResolveComboSettings(&rec);
    rec.comboFlags = (uint8_t)(rec.comboFlags | flags);
    Combo_FreezeComboSettingsEx2(&rec, NULL, rules2);
}

/** #1702 C2: the three MM predicates as bits: 1 ChildWalletShuffled, 2
 *  WalletGated, 4 ChuBagShared. */
int CbsMMPredicates() {
    return (MM_Combo_ChildWalletShuffled() != 0 ? 1 : 0) | (MM_Combo_WalletGated() != 0 ? 2 : 0) |
           (MM_Combo_ChuBagShared() != 0 ? 4 : 0);
}

/** #1702 C2: every second-word key back to unset (the store directly, so a frozen
 *  record's read-only gate cannot keep one). */
void CbsClearRule2Keys() {
    for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
        const ComboSettingId id = (ComboSettingId)i;
        const char* key = Combo_ComboSettingKey(id);
        if (Combo_ComboItemRule2BitFor(id) != 0u && key != NULL) {
            CVarClear(key);
        }
    }
}

std::string CbsReadSource(const char* rel) {
#ifdef RSBS_SOURCE_DIR
    std::ifstream in(std::string(RSBS_SOURCE_DIR) + "/" + rel, std::ios::binary);
    if (!in.good()) {
        return std::string();
    }
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
#else
    (void)rel;
    return std::string();
#endif
}

/** @p text with every space, tab, carriage return and newline removed. */
std::string CbsSqueeze(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            out.push_back(c);
        }
    }
    return out;
}

void CbsSeedShadows(uint8_t fill) {
    Context_InitFrozenStates();
    std::vector<uint8_t> oot(OOT_SAVE_CONTEXT_SIZE, fill);
    std::vector<uint8_t> mm(MM_SAVE_CONTEXT_SIZE, (uint8_t)(fill ^ 0xFFu));
    Context_UpdateShadowCopy(GAME_OOT, oot.data(), oot.size());
    Context_UpdateShadowCopy(GAME_MM, mm.data(), mm.size());
}

void CbsClearKeys() {
    CVarClear(kCbsStickKey);
    CVarClear(kCbsNutKey);
    CVarClear(kCbsWalletKey);
    CVarClear(kCbsChuKey);
}

/** The four worlds every carrier leg runs: each bit on and off. */
const struct {
    bool stick;
    bool nut;
    const char* what;
} kCbsWorlds[] = {
    { false, false, "both off" },
    { true, false, "stick bag only" },
    { false, true, "nut bag only" },
    { true, true, "both on" },
};

/** #1702 C0: the four wallet / bombchu bag worlds, each bit on and off. */
const struct {
    bool wallet;
    bool chu;
    const char* what;
} kCbsWorlds2[] = {
    { false, false, "wallet and chu bag off" },
    { true, false, "child wallet only" },
    { false, true, "bombchu bag only" },
    { true, true, "wallet and chu bag on" },
};

} // namespace

TestResult ComboBagShuffleFrozen_Run(void) {
    printf("[TEST] combo-bag-shuffle-frozen: the pair's one Deku Stick Bag / Deku Nut Bag shuffle freezes at the "
           "paired creation and rides every carrier of the record (#1652)\n");
    sCbsFailures = 0;
    ComboContext_Init();
    CbsClearKeys();
    CbsClearRule2Keys();
    Combo_ComboItemRules2RevealForTest(0);

    // ---- (1) The bits: allocated, and authored by no key ------------------------
    {
        CBS_CHECK(RSBS_COMBO_FLAG_SHUFFLED_STICK_BAG == kCbsStick && RSBS_COMBO_FLAG_SHUFFLED_NUT_BAG == kCbsNut &&
                      (RSBS_COMBO_FLAGS_ALL_V1 & (kCbsStick | kCbsNut)) == (kCbsStick | kCbsNut),
                  "the bag bits are not comboFlags 08 and 10, allocated");
        ComboSettingsRecord resolved;
        Combo_ResolveComboSettings(&resolved);
        CBS_CHECK((resolved.comboFlags & (kCbsStick | kCbsNut)) == 0u,
                  "the session resolver set a bag bit (%02X): only the creation may", (unsigned)resolved.comboFlags);
        // #1702 C0: the wallet and bombchu bag bits, allocated and creation-derived.
        CBS_CHECK(RSBS_COMBO_FLAG_SHUFFLED_CHILD_WALLET == kCbsWallet &&
                      RSBS_COMBO_FLAG_SHUFFLED_BOMBCHU_BAG == kCbsChu,
                  "the wallet and bombchu bag bits are not comboFlags 20 and 40");
        CBS_CHECK(RSBS_COMBO_FLAGS_ALL_V1 == 0x7Fu && RSBS_COMBO_FLAGS_CREATION_DERIVED == 0x7Cu &&
                      (RSBS_COMBO_FLAGS_CREATION_DERIVED & (kCbsWallet | kCbsChu)) == (kCbsWallet | kCbsChu),
                  "ALL_V1 %02X / CREATION_DERIVED %02X, expected 7F / 7C", (unsigned)RSBS_COMBO_FLAGS_ALL_V1,
                  (unsigned)RSBS_COMBO_FLAGS_CREATION_DERIVED);
        // A raw difference in either bit, passed straight to the compare, is reported
        // at the byte, in both directions (CREATION_DERIVED is what keeps it there:
        // each bit is inside ALL_V1, so ~ALL_V1 alone would drop it).
        ComboSettingsRecord base;
        Combo_ComboSettingsDefaults(&base);
        const uint8_t kCbsRaw[] = { kCbsWallet, kCbsChu };
        for (const uint8_t bit : kCbsRaw) {
            ComboSettingsRecord set = base;
            set.comboFlags = (uint8_t)(set.comboFlags | bit);
            const uint32_t setFrozen = Combo_ComboSettingsDivergenceBetween(&set, &base);
            const uint32_t setLive = Combo_ComboSettingsDivergenceBetween(&base, &set);
            printf("[TEST] (1) raw comboFlags %02X difference: frozen side %08X, live side %08X\n", (unsigned)bit,
                   (unsigned)setFrozen, (unsigned)setLive);
            CBS_CHECK(setFrozen == RSBS_COMBO_DIVERGE_COMBO_FLAGS && setLive == RSBS_COMBO_DIVERGE_COMBO_FLAGS &&
                          Combo_ComboSettingsDivergenceBetween(&set, &set) == 0u,
                      "a raw comboFlags %02X difference was not reported at the byte in both directions (%08X %08X)",
                      (unsigned)bit, (unsigned)setFrozen, (unsigned)setLive);
        }
        // No key authors them: the resolver leaves both clear with both options on.
        CVarSetInteger(kCbsWalletKey, 1);
        CVarSetInteger(kCbsChuKey, 2);
        Combo_ResolveComboSettings(&resolved);
        CBS_CHECK((resolved.comboFlags & (kCbsWallet | kCbsChu)) == 0u,
                  "the session resolver set a wallet or bombchu bag bit (%02X): only the creation may",
                  (unsigned)resolved.comboFlags);
        CbsClearKeys();
    }

    // ---- (2) The creation's decision, and the fingerprint ------------------------
    {
        ComboSettingsRecord defaults;
        Combo_ComboSettingsDefaults(&defaults);
        for (const auto& w : kCbsWorlds) {
            ComboSettingsRecord rec = defaults;
            rec.comboFlags = (uint8_t)(RSBS_COMBO_FLAG_SHARED_KOKIRI_SWORD | (w.stick ? 0u : kCbsStick) |
                                       (w.nut ? 0u : kCbsNut)); // a stale bit clears, the sword bit stays
            Combo_ComboSettingsApplyCreationBagShuffles(&rec, w.stick, w.nut);
            const uint8_t want =
                (uint8_t)(RSBS_COMBO_FLAG_SHARED_KOKIRI_SWORD | (w.stick ? kCbsStick : 0u) | (w.nut ? kCbsNut : 0u));
            printf("[TEST] (2) %s: flags %02X (want %02X)\n", w.what, (unsigned)rec.comboFlags, (unsigned)want);
            CBS_CHECK(rec.comboFlags == want, "%s: the creation froze flags %02X, expected %02X", w.what,
                      (unsigned)rec.comboFlags, (unsigned)want);
        }
        Combo_ComboSettingsApplyCreationBagShuffles(NULL, true, true); // a no-op, not a crash
        const uint32_t legacy = Combo_ComputeComboSettingsHash(&defaults, kCbsSettingsHash, kCbsProfileDigest);
        ComboSettingsRecord off = defaults;
        Combo_ComboSettingsApplyCreationBagShuffles(&off, false, false);
        CBS_CHECK(Combo_ComputeComboSettingsHash(&off, kCbsSettingsHash, kCbsProfileDigest) == legacy,
                  "a world with both bag shuffles off moved its fingerprint");
        ComboSettingsRecord stick = defaults;
        Combo_ComboSettingsApplyCreationBagShuffles(&stick, true, false);
        ComboSettingsRecord nut = defaults;
        Combo_ComboSettingsApplyCreationBagShuffles(&nut, false, true);
        const uint32_t stickHash = Combo_ComputeComboSettingsHash(&stick, kCbsSettingsHash, kCbsProfileDigest);
        const uint32_t nutHash = Combo_ComputeComboSettingsHash(&nut, kCbsSettingsHash, kCbsProfileDigest);
        CBS_CHECK(stickHash != legacy && nutHash != legacy && stickHash != nutHash,
                  "each bag bit must move the fingerprint, and apart (%08X %08X %08X)", (unsigned)legacy,
                  (unsigned)stickHash, (unsigned)nutHash);

        // #1702 C0: the wallet and bombchu bag helper sets only its own two bits,
        // over a record holding the sword and both bag bits and the stale opposite.
        const uint8_t kept = (uint8_t)(RSBS_COMBO_FLAG_SHARED_KOKIRI_SWORD | kCbsStick | kCbsNut);
        for (const auto& w : kCbsWorlds2) {
            ComboSettingsRecord rec = defaults;
            rec.comboFlags = (uint8_t)(kept | (w.wallet ? 0u : kCbsWallet) | (w.chu ? 0u : kCbsChu));
            Combo_ComboSettingsApplyCreationWalletChuShuffles(&rec, w.wallet, w.chu);
            const uint8_t want = (uint8_t)(kept | (w.wallet ? kCbsWallet : 0u) | (w.chu ? kCbsChu : 0u));
            printf("[TEST] (2) %s: flags %02X (want %02X)\n", w.what, (unsigned)rec.comboFlags, (unsigned)want);
            CBS_CHECK(rec.comboFlags == want, "%s: the creation froze flags %02X, expected %02X", w.what,
                      (unsigned)rec.comboFlags, (unsigned)want);
        }
        // ...and the bag helper leaves the two #1702 bits alone.
        {
            ComboSettingsRecord rec = defaults;
            rec.comboFlags = (uint8_t)(kCbsWallet | kCbsChu);
            Combo_ComboSettingsApplyCreationBagShuffles(&rec, false, false);
            CBS_CHECK(rec.comboFlags == (uint8_t)(kCbsWallet | kCbsChu),
                      "the bag helper cleared a wallet or bombchu bag bit (%02X)", (unsigned)rec.comboFlags);
        }
        Combo_ComboSettingsApplyCreationWalletChuShuffles(NULL, true, true); // a no-op, not a crash
        ComboSettingsRecord off2 = defaults;
        Combo_ComboSettingsApplyCreationWalletChuShuffles(&off2, false, false);
        CBS_CHECK(Combo_ComputeComboSettingsHash(&off2, kCbsSettingsHash, kCbsProfileDigest) == legacy,
                  "a world with the wallet and bombchu bag shuffles off moved its fingerprint");
        ComboSettingsRecord wallet = defaults;
        Combo_ComboSettingsApplyCreationWalletChuShuffles(&wallet, true, false);
        ComboSettingsRecord chu = defaults;
        Combo_ComboSettingsApplyCreationWalletChuShuffles(&chu, false, true);
        const uint32_t walletHash = Combo_ComputeComboSettingsHash(&wallet, kCbsSettingsHash, kCbsProfileDigest);
        const uint32_t chuHash = Combo_ComputeComboSettingsHash(&chu, kCbsSettingsHash, kCbsProfileDigest);
        CBS_CHECK(walletHash != legacy && chuHash != legacy && walletHash != chuHash && walletHash != stickHash &&
                      chuHash != stickHash && walletHash != nutHash && chuHash != nutHash,
                  "each wallet / bombchu bag bit must move the fingerprint, and apart (%08X %08X %08X)",
                  (unsigned)legacy, (unsigned)walletHash, (unsigned)chuHash);
    }

    // ---- (3) The predicates read the frozen record, and nothing else ------------
    {
        ComboContext_Init();
        CBS_CHECK(CbsReads(false, false), "nothing is frozen (an unpaired file), yet a bag reads shuffled");
        // An unpaired file under OoT's options on: still nothing frozen, still false.
        CVarSetInteger(kCbsStickKey, 1);
        CVarSetInteger(kCbsNutKey, 1);
        CBS_CHECK(CbsReads(false, false), "an unpaired file read a bag shuffled from OoT's live option");
        CbsClearKeys();
        // A record nothing froze (formatVersion 0) reads false even with both bits in its bytes.
        gComboCtx.comboSettings.comboFlags = (uint8_t)(kCbsStick | kCbsNut);
        CBS_CHECK(!Combo_ComboSettingsFrozen() && CbsReads(false, false),
                  "an unfrozen record with the bag bits in its bytes read a bag shuffled");
        ComboContext_Init();
        for (const auto& w : kCbsWorlds) {
            CbsFreezeWorld(w.stick, w.nut);
            CBS_CHECK(CbsReads(w.stick, w.nut), "%s: the predicates read stick %d nut %d", w.what,
                      Combo_ComboStickBagShuffled() ? 1 : 0, Combo_ComboNutBagShuffled() ? 1 : 0);
            // The player changes OoT's options after the creation: the world does not move.
            CVarSetInteger(kCbsStickKey, w.stick ? 0 : 1);
            CVarSetInteger(kCbsNutKey, w.nut ? 0 : 1);
            CBS_CHECK(CbsReads(w.stick, w.nut), "%s: a live change of OoT's option changed the frozen answer", w.what);
            CbsClearKeys();
        }

        // #1702 C0: the wallet and bombchu bag predicates, the same way.
        ComboContext_Init();
        CBS_CHECK(CbsReadsWalletChu(false, false), "nothing is frozen, yet the wallet or bombchu bag reads shuffled");
        CVarSetInteger(kCbsWalletKey, 1);
        CVarSetInteger(kCbsChuKey, 2);
        CBS_CHECK(CbsReadsWalletChu(false, false),
                  "an unpaired file read the wallet or bombchu bag shuffled from OoT's live option");
        CbsClearKeys();
        gComboCtx.comboSettings.comboFlags = (uint8_t)(kCbsWallet | kCbsChu);
        CBS_CHECK(!Combo_ComboSettingsFrozen() && CbsReadsWalletChu(false, false),
                  "an unfrozen record with the wallet and bombchu bag bits in its bytes read one shuffled");
        ComboContext_Init();
        for (const auto& w : kCbsWorlds2) {
            CbsFreezeWorld2(w.wallet, w.chu);
            CBS_CHECK(CbsReads2(w.wallet, w.chu), "%s: the predicates read wallet %d chu %d stick %d nut %d", w.what,
                      Combo_ComboChildWalletShuffled() ? 1 : 0, Combo_ComboBombchuBagShuffled() ? 1 : 0,
                      Combo_ComboStickBagShuffled() ? 1 : 0, Combo_ComboNutBagShuffled() ? 1 : 0);
            CVarSetInteger(kCbsWalletKey, w.wallet ? 0 : 1);
            CVarSetInteger(kCbsChuKey, w.chu ? 0 : 2);
            CBS_CHECK(CbsReads2(w.wallet, w.chu), "%s: a live change of OoT's option changed the frozen answer",
                      w.what);
            CbsClearKeys();
        }
    }

    // ---- (4) The compare reads the bits as the file decided them ----------------
    for (const auto& w : kCbsWorlds) {
        CbsFreezeWorld(w.stick, w.nut);
        const uint32_t bits = Combo_ComboSettingsDivergence();
        CBS_CHECK(bits == 0u, "%s: a world met by its own session diverged %08X; the bag bits are the file's", w.what,
                  (unsigned)bits);
    }
    // #1702 C0: likewise (this proves the bits sit inside ALL_V1; leg (1) checks
    // CREATION_DERIVED, which no compare through a session can tell apart).
    for (const auto& w : kCbsWorlds2) {
        CbsFreezeWorld2(w.wallet, w.chu);
        const uint32_t bits = Combo_ComboSettingsDivergence();
        CBS_CHECK(bits == 0u,
                  "%s: a world met by its own session diverged %08X; the wallet and chu bits are the file's", w.what,
                  (unsigned)bits);
    }

    // ---- (5) The carriers ----------------------------------------------------------
    // (5a) The creation snapshot and its rollback, and the KEEP invalidation.
    for (const auto& w : kCbsWorlds) {
        CbsFreezeWorld(w.stick, w.nut);
        ComboCreationRecords prior;
        memset(&prior, 0xEE, sizeof(prior));
        Combo_SnapshotCreationRecords(&prior);
        CbsFreezeWorld(!w.stick, !w.nut); // a failed generation's other world
        Combo_RestoreCreationRecords(&prior);
        CBS_CHECK(CbsReads(w.stick, w.nut) && Combo_ComboSettingsDivergence() == 0u,
                  "%s: the creation rollback did not put the world's bag bits back", w.what);
        Context_InvalidateSessionState(RSBS_SEED_STAMP_KEEP);
        CBS_CHECK(CbsReads(w.stick, w.nut), "%s: the file-create KEEP invalidation dropped a bag bit", w.what);
    }
    for (const auto& w : kCbsWorlds2) {
        CbsFreezeWorld2(w.wallet, w.chu);
        ComboCreationRecords prior;
        memset(&prior, 0xEE, sizeof(prior));
        Combo_SnapshotCreationRecords(&prior);
        CbsFreezeWorld2(!w.wallet, !w.chu); // a failed generation's other world
        Combo_RestoreCreationRecords(&prior);
        CBS_CHECK(CbsReads2(w.wallet, w.chu) && Combo_ComboSettingsDivergence() == 0u,
                  "%s: the creation rollback did not put the world's wallet and chu bits back", w.what);
        Context_InvalidateSessionState(RSBS_SEED_STAMP_KEEP);
        CBS_CHECK(CbsReads2(w.wallet, w.chu), "%s: the file-create KEEP invalidation dropped a wallet or chu bit",
                  w.what);
    }
    // (5b) The seed package's world record.
    for (const auto& w : kCbsWorlds) {
        CbsFreezeWorld(w.stick, w.nut);
        ComboContext live;
        memcpy(&live, &gComboCtx, sizeof(live));
        ComboContext_Init();
        ComboContext* carried = new ComboContext;
        memset(carried, 0xEE, sizeof(*carried));
        const int rt = OoT_SeedPackage_WorldRecordRoundTripForTest(&live, carried);
        const uint8_t want = (uint8_t)((w.stick ? kCbsStick : 0u) | (w.nut ? kCbsNut : 0u));
        printf("[TEST] (5b) %s: package round trip %d, carried flags %02X, resident stick %d nut %d\n", w.what, rt,
               (unsigned)carried->comboSettings.comboFlags, Combo_ComboStickBagShuffled() ? 1 : 0,
               Combo_ComboNutBagShuffled() ? 1 : 0);
        CBS_CHECK(rt == 1 && (carried->comboSettings.comboFlags & (kCbsStick | kCbsNut)) == want,
                  "%s: placement/combo-context.bin carries flags %02X, expected bag bits %02X", w.what,
                  (unsigned)carried->comboSettings.comboFlags, (unsigned)want);
        CBS_CHECK(CbsReads(w.stick, w.nut) && Combo_ComboSettingsDivergence() == 0u,
                  "%s: ApplyComboWorld did not bind the package's bag bits", w.what);
        delete carried;
    }
    for (const auto& w : kCbsWorlds2) {
        CbsFreezeWorld2(w.wallet, w.chu);
        ComboContext live;
        memcpy(&live, &gComboCtx, sizeof(live));
        ComboContext_Init();
        ComboContext* carried = new ComboContext;
        memset(carried, 0xEE, sizeof(*carried));
        const int rt = OoT_SeedPackage_WorldRecordRoundTripForTest(&live, carried);
        const uint8_t want = (uint8_t)(kCbsStick | (w.wallet ? kCbsWallet : 0u) | (w.chu ? kCbsChu : 0u));
        const uint8_t mask = (uint8_t)(kCbsStick | kCbsNut | kCbsWallet | kCbsChu);
        printf("[TEST] (5b) %s: package round trip %d, carried flags %02X, resident wallet %d chu %d\n", w.what, rt,
               (unsigned)carried->comboSettings.comboFlags, Combo_ComboChildWalletShuffled() ? 1 : 0,
               Combo_ComboBombchuBagShuffled() ? 1 : 0);
        CBS_CHECK(rt == 1 && (carried->comboSettings.comboFlags & mask) == want,
                  "%s: placement/combo-context.bin carries flags %02X, expected %02X", w.what,
                  (unsigned)carried->comboSettings.comboFlags, (unsigned)want);
        CBS_CHECK(CbsReads2(w.wallet, w.chu) && Combo_ComboSettingsDivergence() == 0u,
                  "%s: ApplyComboWorld did not bind the package's wallet and chu bits", w.what);
        delete carried;
    }
    // (5c) A real .redsave.
    {
        rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
        const std::string priorSaveDir = std::filesystem::path(mgr.SlotPath(0)).parent_path().string();
        mgr.SetSaveDirectory(rsbs::test::ScratchDir(kCbsLoadDir).string());
        for (const auto& w : kCbsWorlds) {
            RsbsSave_ResetSlotSessionState();
            mgr.DeleteSave(0);
            ComboContext_Init();
            CbsSeedShadows(0x65u);
            CbsFreezeWorld(w.stick, w.nut);
            CbsSeedShadows(0x65u);
            CBS_CHECK(mgr.Save(0), "%s: Save(0) failed", w.what);
            RsbsSave_ResetSlotSessionState();
            ComboContext_Init();
            CBS_CHECK(CbsReads(false, false), "%s: the context still held the world before the load", w.what);
            const bool loaded = mgr.Load(0);
            printf("[TEST] (5c) %s: loaded %d, stick %d nut %d, quarantine %d\n", w.what, loaded ? 1 : 0,
                   Combo_ComboStickBagShuffled() ? 1 : 0, Combo_ComboNutBagShuffled() ? 1 : 0,
                   RsbsSave_HasQuarantine(0));
            CBS_CHECK(loaded && CbsReads(w.stick, w.nut) && Combo_ComboSettingsDivergence() == 0u,
                      "%s: THE .redsave ROUND TRIP lost a bag bit or refused the file (loaded %d)", w.what,
                      loaded ? 1 : 0);
        }
        for (const auto& w : kCbsWorlds2) {
            RsbsSave_ResetSlotSessionState();
            mgr.DeleteSave(0);
            ComboContext_Init();
            CbsSeedShadows(0x65u);
            CbsFreezeWorld2(w.wallet, w.chu);
            CbsSeedShadows(0x65u);
            CBS_CHECK(mgr.Save(0), "%s: Save(0) failed", w.what);
            RsbsSave_ResetSlotSessionState();
            ComboContext_Init();
            CBS_CHECK(CbsReadsWalletChu(false, false), "%s: the context still held the world before the load", w.what);
            const bool loaded = mgr.Load(0);
            printf("[TEST] (5c) %s: loaded %d, wallet %d chu %d, quarantine %d\n", w.what, loaded ? 1 : 0,
                   Combo_ComboChildWalletShuffled() ? 1 : 0, Combo_ComboBombchuBagShuffled() ? 1 : 0,
                   RsbsSave_HasQuarantine(0));
            CBS_CHECK(loaded && CbsReads2(w.wallet, w.chu) && Combo_ComboSettingsDivergence() == 0u,
                      "%s: THE .redsave ROUND TRIP lost a wallet or chu bit or refused the file (loaded %d)", w.what,
                      loaded ? 1 : 0);
        }

        // ---- (6) The one-game pin (#1702): a file frozen with neither bit reads false
        // forever. OoT's options turned on after the creation, and a .redsave round
        // trip under them, change neither answer, and the file is not refused.
        {
            RsbsSave_ResetSlotSessionState();
            mgr.DeleteSave(0);
            ComboContext_Init();
            CbsSeedShadows(0x65u);
            CbsFreezeWorld2(false, false);
            CbsSeedShadows(0x65u);
            CVarSetInteger(kCbsWalletKey, 1);
            CVarSetInteger(kCbsChuKey, 2);
            CBS_CHECK(CbsReads2(false, false), "a world frozen without the bits read one from OoT's later option");
            CBS_CHECK(mgr.Save(0), "(6) Save(0) failed");
            RsbsSave_ResetSlotSessionState();
            ComboContext_Init();
            const bool loaded = mgr.Load(0);
            const uint32_t bits = Combo_ComboSettingsDivergence();
            printf("[TEST] (6) frozen without the bits, options on after: loaded %d, wallet %d chu %d, divergence "
                   "%08X\n",
                   loaded ? 1 : 0, Combo_ComboChildWalletShuffled() ? 1 : 0, Combo_ComboBombchuBagShuffled() ? 1 : 0,
                   (unsigned)bits);
            CBS_CHECK(loaded && CbsReads2(false, false) && bits == 0u && RsbsSave_HasQuarantine(0) == 0,
                      "A FILE FROZEN WITHOUT THE WALLET / CHU BITS changed its answer or was refused under OoT's "
                      "later options (loaded %d, divergence %08X)",
                      loaded ? 1 : 0, (unsigned)bits);
            CbsClearKeys();
        }

        // ---- (8) #1702 C2, ONE GAME: a released file disarms and reads every new
        // predicate 0. CbsFreezeWorld2(true, true) is a v0.5.2 / v0.5.3 file whose
        // OoT shuffled both (0x20 | 0x40, no new bit). The owned bits and MM's flags
        // are written AFTER the freeze, which re-initialises the context.
        // Catches: Combo_SharedPossessionArmed reverted to the C0 bits (armed 600,
        // and the apply after the load gives MM flags 80 / 81: given 3), a
        // predicate keyed on 0x20 / 0x40 (bit 1 or 4 set) or on flag 80 alone
        // (WalletGated 1 with the flag clear), and a load that drops the record's
        // bits or the owned word.
        {
            RsbsSave_ResetSlotSessionState();
            mgr.DeleteSave(0);
            ComboContext_Init();
            CbsSeedShadows(0x65u);
            CbsFreezeWorld2(true, true);
            gComboCtx.sharedFamilyOwned = kCbsPossession;
            MM_SharedSwordTest_ArmLive();
            MM_SharedPossessionTest_Set(1, 1);
            const int predsHeld = CbsMMPredicates();
            MM_SharedPossessionTest_Set(0, 0);
            const int predsClear = CbsMMPredicates();
            const uint32_t armed = Combo_SharedPossessionArmed();
            printf("[TEST] (8) released 20|40 file: armed %08X, MM predicates flags set %d clear %d, bag shared %d, "
                   "own wallets %d (expected 0, 0, 0, 0, 0)\n",
                   (unsigned)armed, predsHeld, predsClear, Combo_ComboBombchuBagShared() ? 1 : 0,
                   Combo_ComboOwnChildWallets() ? 1 : 0);
            CBS_CHECK(armed == 0u && predsHeld == 0 && predsClear == 0 && !Combo_ComboBombchuBagShared() &&
                          !Combo_ComboOwnChildWallets(),
                      "(8) A RELEASED 0x20 | 0x40 FILE armed possession %08X or read a new predicate (MM %d / %d)",
                      (unsigned)armed, predsHeld, predsClear);
            CbsSeedShadows(0x65u);
            CBS_CHECK(mgr.Save(0), "(8) Save(0) failed");
            RsbsSave_ResetSlotSessionState();
            ComboContext_Init();
            const bool loaded = mgr.Load(0);
            const uint8_t flags = gComboCtx.comboSettings.comboFlags;
            const uint32_t owned = gComboCtx.sharedFamilyOwned;
            const uint32_t armedLoaded = Combo_SharedPossessionArmed();
            MM_SharedSwordTest_ArmLive();
            MM_SharedPossessionTest_Set(0, 0);
            const int predsLoaded = CbsMMPredicates();
            MM_ApplySharedResources();
            const int given = MM_SharedPossessionTest_Held();
            printf("[TEST] (8) after the .redsave: loaded %d, flags %02X, owned %08X, armed %08X, MM predicates %d, "
                   "MM given %d (expected 1, 68, 00000600, 0, 0, 0)\n",
                   loaded ? 1 : 0, (unsigned)flags, (unsigned)owned, (unsigned)armedLoaded, predsLoaded, given);
            CBS_CHECK(loaded && (flags & (kCbsWallet | kCbsChu)) == (kCbsWallet | kCbsChu) &&
                          (owned & kCbsPossession) == kCbsPossession,
                      "(8) the released file did not load with its 0x20 | 0x40 and its owned 0x600 (loaded %d, flags "
                      "%02X, owned %08X)",
                      loaded ? 1 : 0, (unsigned)flags, (unsigned)owned);
            CBS_CHECK(armedLoaded == 0u && predsLoaded == 0 && given == 0 && !Combo_ComboBombchuBagShared() &&
                          !Combo_ComboOwnChildWallets(),
                      "(8) AFTER THE LOAD the released file armed %08X, read MM predicates %d, or MM's apply gave "
                      "possession flags %d",
                      (unsigned)armedLoaded, predsLoaded, given);
            MM_SharedSwordTest_ArmLive();
        }

        // ---- (9) #1702 C2, THE SHARED CHILD'S WALLET RULE: a key and a hidden bit ---
        {
            const ComboSettingId id = COMBO_SETTING_SHARED_CHILD_WALLET;
            const std::string wantKey = std::string(RSBS::kComboIdentityKeyPrefix) + "SharedChildWallet";
            const char* key = Combo_ComboSettingKey(id);
            // Catches: an id inserted rather than appended (every later id would
            // move), a missing descriptor, a wrong key or label, a default that
            // ships on, a value space wider than 0/1, a key authoring another bit.
            CBS_CHECK((int)id == (int)COMBO_SETTING_SHARED_SONG_SARIAS + 1 && key != NULL && wantKey == key &&
                          strcmp(Combo_ComboSettingLabel(id), "Shared Child's Wallet") == 0,
                      "(9) the Shared Child's Wallet setting is id %d with key '%s' and label '%s'", (int)id,
                      key != NULL ? key : "(null)", Combo_ComboSettingLabel(id));
            CBS_CHECK(Combo_ComboSettingDefault(id) == 0 && Combo_ComboSettingValueValid(id, 0) &&
                          Combo_ComboSettingValueValid(id, 1) && !Combo_ComboSettingValueValid(id, 2) &&
                          !Combo_ComboSettingValueValid(id, -1),
                      "(9) the Shared Child's Wallet key does not ship off or is not exactly 0 or 1");
            CBS_CHECK(Combo_ComboItemRule2BitFor(id) == kCbsRule && RSBS_XITEM2_SHARED_CHILD_WALLET == kCbsRule &&
                          (Combo_ComboItemRule2KeyedBits() & kCbsRule) == kCbsRule,
                      "(9) the key authors %08X, not 00010000", (unsigned)Combo_ComboItemRule2BitFor(id));
            // HIDDEN. Catches the rule offered (a row drawn, Generate taking it)
            // before stage C5, and a refusal that does not name it.
            char names[192];
            const uint32_t refused = Combo_ComboItemRules2Refused(kCbsRule, names, sizeof(names));
            CBS_CHECK((RSBS_XITEM2_HIDDEN & kCbsRule) == kCbsRule && !Combo_ComboItemRule2Offered(kCbsRule) &&
                          refused == kCbsRule && strcmp(names, "sharedChildWallet") == 0,
                      "(9) the rule is offered, or refused as %08X ('%s'), before #1702 C5", (unsigned)refused, names);
            Combo_ComboItemRules2RevealForTest(RSBS_XITEM2_REVEAL_ALL);
            const bool offeredRevealed = Combo_ComboItemRule2Offered(kCbsRule);
            const uint32_t refusedRevealed = Combo_ComboItemRules2Refused(kCbsRule, NULL, 0);
            Combo_ComboItemRules2RevealForTest(0);
            CBS_CHECK(offeredRevealed && refusedRevealed == 0u,
                      "(9) under the test reveal the rule is not offered (%d) or still refused (%08X)",
                      offeredRevealed ? 1 : 0, (unsigned)refusedRevealed);
            // THE RESOLVER and GENERATE'S QUESTION. Catches the key reaching another
            // bit, and Generate not refusing a hand-set key while the rule is hidden.
            ComboContext_Init();
            CbsClearRule2Keys();
            CBS_CHECK(Combo_ComboSettingSet(id, 1) == 1, "(9) the writer refused the Shared Child's Wallet key");
            const uint32_t resolved = Combo_ResolveComboItemRules2();
            const uint32_t generate = Combo_ComboItemRules2Refused(resolved, names, sizeof(names));
            printf("[TEST] (9) Generate with Shared Child's Wallet set: resolved %08X, refused %08X (%s)\n",
                   (unsigned)resolved, (unsigned)generate, names);
            CBS_CHECK(resolved == kCbsRule && generate == kCbsRule,
                      "(9) GENERATE MUST REFUSE THE HIDDEN SHARED CHILD'S WALLET: resolved %08X, refused %08X",
                      (unsigned)resolved, (unsigned)generate);
            CbsClearRule2Keys();
            CBS_CHECK(Combo_ResolveComboItemRules2() == 0u, "(9) with the key cleared the word resolved %08X",
                      (unsigned)Combo_ResolveComboItemRules2());
            // THE FINGERPRINT. Catches the bit not folded (on == off), and a rule-off
            // world whose stamp moved. Pinned at version 0 so a generator bump
            // re-pins nothing: canonical(defaults) ':' LE32(1652BA65) ':'
            // LE32(1652D16E) [ '%' LE32(00010000) ], FNV-1a 32, computed outside
            // the build (the same formula reproduces CEA3786B, test_cross_item_rules.c).
            ComboSettingsRecord defaults;
            Combo_ComboSettingsDefaults(&defaults);
            const uint32_t off = Combo_ComputeComboSettingsHashForGeneratorEx2(&defaults, NULL, 0u, kCbsSettingsHash,
                                                                               kCbsProfileDigest, 0u);
            const uint32_t on = Combo_ComputeComboSettingsHashForGeneratorEx2(&defaults, NULL, kCbsRule,
                                                                              kCbsSettingsHash, kCbsProfileDigest, 0u);
            CBS_CHECK(on != off && off == 0x24DF9AA0u && on == 0x1EEE1BE6u,
                      "(9) the rule's fingerprint %08X does not differ from the rule-off %08X, or the rule-off world "
                      "moved",
                      (unsigned)on, (unsigned)off);
            // FROZEN ON (directly: no Generate), the rule arms the wallet slot only
            // and the MM predicates follow it. Catches the arming not reading the
            // rule (armed 0), arming the bag too (armed 600), and WalletGated not
            // reading flag 80 (3 with the flag set).
            CbsFreezeWorldC2(0u, kCbsRule);
            const uint32_t armedOn = Combo_SharedPossessionArmed();
            MM_SharedSwordTest_ArmLive();
            MM_SharedPossessionTest_Set(0, 0);
            const int predsUnheld = CbsMMPredicates();
            MM_SharedPossessionTest_Set(1, 0);
            const int predsHeld = CbsMMPredicates();
            MM_SharedSwordTest_ArmLive();
            printf("[TEST] (9) rule frozen on: armed %08X, MM predicates unheld %d held %d (expected 00000200, 3, 1)\n",
                   (unsigned)armedOn, predsUnheld, predsHeld);
            CBS_CHECK(armedOn == (uint32_t)RSBS_XFAMILY_SLOT_CHILD_WALLET && predsUnheld == 3 && predsHeld == 1,
                      "(9) the frozen rule armed %08X (want 00000200) or the MM predicates read %d / %d (want 3 / 1)",
                      (unsigned)armedOn, predsUnheld, predsHeld);
            // A SESSION'S STRAY KEY against a rule-off file: put back to the file's
            // off (frozen wins at load). Catches a missing kRestorable row or
            // FrozenValue case (unrestorable, or not named).
            ComboContext_Init();
            CBS_CHECK(Combo_ComboSettingSet(id, 1) == 1, "(9) the writer refused the stray key");
            CbsFreezeWorldC2(0u, 0u);
            const uint32_t stray = Combo_ComboSettingsDivergence();
            const uint32_t strayUnrestorable =
                Combo_ComboSettingsUnrestorableBitsEx2(&gComboCtx.comboItemRules, 0u, stray);
            const int back =
                Combo_ComboSettingsRestoreLiveEx2(&gComboCtx.comboSettings, &gComboCtx.comboItemRules, 0u,
                                                  &gComboCtx.comboTriforce, stray, names, sizeof(names), NULL);
            printf("[TEST] (9) stray key: divergence %08X, unrestorable %08X, restore %d (%s)\n", (unsigned)stray,
                   (unsigned)strayUnrestorable, back, names);
            CBS_CHECK(stray == RSBS_COMBO_DIVERGE_CROSS_ITEM_FLAGS && strayUnrestorable == 0u && back == 1 &&
                          strcmp(names, "Shared Child's Wallet") == 0 && Combo_ComboSettingResolved(id) == 0 &&
                          Combo_ComboSettingsDivergence() == 0u,
                      "(9) the stray Shared Child's Wallet key was not put back to the file's off");
            CbsClearRule2Keys();
            // THE .redsave: hidden, a file with the rule is refused as another
            // build's; revealed, it loads, its word and key come back, and it arms.
            // Catches a hidden rule that loads, and a revealed load that drops it.
            RsbsSave_ResetSlotSessionState();
            mgr.DeleteSave(0);
            ComboContext_Init();
            CbsSeedShadows(0x66u);
            CbsFreezeWorldC2(0u, kCbsRule);
            CbsSeedShadows(0x66u);
            CBS_CHECK(mgr.Save(0), "(9) Save(0) failed for the Shared Child's Wallet file");
            RsbsSave_ResetSlotSessionState();
            ComboContext_Init();
            const bool hiddenLoaded = mgr.Load(0);
            CbsClearRule2Keys();
            Combo_ComboItemRules2RevealForTest(RSBS_XITEM2_REVEAL_ALL);
            RsbsSave_ResetSlotSessionState();
            ComboContext_Init();
            const bool revealedLoaded = mgr.Load(0);
            const uint32_t word = Combo_ComboItemRules2Word(&gComboCtx.comboItemRules2);
            const int32_t restoredKey = Combo_ComboSettingResolved(id);
            const uint32_t armedLoaded = Combo_SharedPossessionArmed();
            Combo_ComboItemRules2RevealForTest(0);
            CbsClearRule2Keys();
            printf("[TEST] (9) .redsave with the rule: hidden loaded %d, revealed loaded %d, word %08X, key %d, armed "
                   "%08X (expected 0, 1, 00010000, 1, 00000200)\n",
                   hiddenLoaded ? 1 : 0, revealedLoaded ? 1 : 0, (unsigned)word, (int)restoredKey,
                   (unsigned)armedLoaded);
            CBS_CHECK(!hiddenLoaded, "(9) A FILE WITH THE HIDDEN SHARED CHILD'S WALLET LOADED on the production mask");
            CBS_CHECK(revealedLoaded && word == kCbsRule && restoredKey == 1 &&
                          armedLoaded == (uint32_t)RSBS_XFAMILY_SLOT_CHILD_WALLET,
                      "(9) revealed, the file did not load with its rule, its key and its arming");
            ComboContext_Init();
        }

        // ---- (10) #1702 C2, THE DERIVED MARKER and THE RESERVED BAG BIT ------------
        {
            // No key authors the marker. Catches a key added for it (it is
            // creation-derived) and a resolver that produces it.
            uint32_t keyedDerived = 0u;
            for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
                keyedDerived |= Combo_ComboItemRule2BitFor((ComboSettingId)i) & kCbsDerived;
            }
            ComboContext_Init();
            CbsClearRule2Keys();
            int keysSet = 0;
            for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
                if (Combo_ComboItemRule2BitFor((ComboSettingId)i) != 0u) {
                    keysSet += Combo_ComboSettingSet((ComboSettingId)i, 1) == 1 ? 1 : 0;
                }
            }
            const uint32_t resolvedAll = Combo_ResolveComboItemRules2();
            CbsClearRule2Keys();
            printf("[TEST] (10) every second-word key on (%d keys): resolved %08X\n", keysSet, (unsigned)resolvedAll);
            CBS_CHECK(RSBS_XITEM2_OWN_CHILD_WALLETS == kCbsDerived && RSBS_XITEM2_DERIVED == kCbsDerived &&
                          keyedDerived == 0u && (Combo_ComboItemRule2KeyedBits() & kCbsDerived) == 0u &&
                          (resolvedAll & kCbsDerived) == 0u && (resolvedAll & kCbsRule) == kCbsRule,
                      "(10) the derived marker is keyed (%08X) or resolved (%08X)", (unsigned)keyedDerived,
                      (unsigned)resolvedAll);
            // Refused by name, never offered, even revealed. Catches the Offered
            // exclusion of RSBS_XITEM2_DERIVED dropped, and a refusal naming "bit 19".
            char names[192];
            const uint32_t refused = Combo_ComboItemRules2Refused(kCbsDerived, names, sizeof(names));
            Combo_ComboItemRules2RevealForTest(RSBS_XITEM2_REVEAL_ALL);
            const bool offeredRevealed = Combo_ComboItemRule2Offered(kCbsDerived);
            Combo_ComboItemRules2RevealForTest(0);
            CBS_CHECK(refused == kCbsDerived && strcmp(names, "ownChildWallets") == 0 &&
                          !Combo_ComboItemRule2Offered(kCbsDerived) && !offeredRevealed,
                      "(10) the derived marker was refused as %08X ('%s') or offered (revealed %d)", (unsigned)refused,
                      names, offeredRevealed ? 1 : 0);
            // The common predicate reads the frozen marker. Catches it reading the
            // wrong bit (or nothing): true only for the world frozen with it.
            CbsFreezeWorldC2(0u, kCbsDerived);
            const bool ownOn = Combo_ComboOwnChildWallets();
            CbsFreezeWorldC2(0u, kCbsRule);
            const bool ownRule = Combo_ComboOwnChildWallets();
            ComboContext_Init();
            const bool ownUnfrozen = Combo_ComboOwnChildWallets();
            CBS_CHECK(ownOn && !ownRule && !ownUnfrozen,
                      "(10) Combo_ComboOwnChildWallets read %d / %d / %d (marker, rule only, unfrozen; want 1 0 0)",
                      ownOn ? 1 : 0, ownRule ? 1 : 0, ownUnfrozen ? 1 : 0);
            // A .redsave holding the marker is refused, EVEN REVEALED: no key can
            // restore it until stage C3 takes it from the file. Catches the marker
            // made restorable (a key, or a live copy) before its reader exists.
            RsbsSave_ResetSlotSessionState();
            mgr.DeleteSave(0);
            ComboContext_Init();
            CbsSeedShadows(0x67u);
            CbsFreezeWorldC2(0u, kCbsDerived);
            CbsSeedShadows(0x67u);
            CBS_CHECK(mgr.Save(0), "(10) Save(0) failed for the derived-marker file");
            RsbsSave_ResetSlotSessionState();
            ComboContext_Init();
            const bool derivedLoaded = mgr.Load(0);
            Combo_ComboItemRules2RevealForTest(RSBS_XITEM2_REVEAL_ALL);
            RsbsSave_ResetSlotSessionState();
            ComboContext_Init();
            const bool derivedRevealedLoaded = mgr.Load(0);
            Combo_ComboItemRules2RevealForTest(0);
            // A .redsave with comboFlags 0x80, the reserved Shared Bombchu Bag
            // marker, is refused at load: the bit is unallocated until C8. Catches
            // 0x80 joining CREATION_DERIVED or ALL_V1 before C8 (the load would take
            // it and play the bag split), and the predicates not reading it.
            RsbsSave_ResetSlotSessionState();
            mgr.DeleteSave(0);
            ComboContext_Init();
            CbsSeedShadows(0x68u);
            CbsFreezeWorldC2(kCbsSharedChu, 0u);
            const bool chuShared = Combo_ComboBombchuBagShared();
            const int mmChuShared = MM_Combo_ChuBagShared();
            CbsSeedShadows(0x68u);
            CBS_CHECK(mgr.Save(0), "(10) Save(0) failed for the comboFlags 0x80 file");
            RsbsSave_ResetSlotSessionState();
            ComboContext_Init();
            const bool chuLoaded = mgr.Load(0);
            printf("[TEST] (10) .redsave: derived marker loaded %d (revealed %d), comboFlags 80 in memory shared %d / "
                   "MM %d, loaded %d, quarantine %d (expected 0, 0, 1, 1, 0)\n",
                   derivedLoaded ? 1 : 0, derivedRevealedLoaded ? 1 : 0, chuShared ? 1 : 0, mmChuShared,
                   chuLoaded ? 1 : 0, RsbsSave_HasQuarantine(0));
            CBS_CHECK(!derivedLoaded && !derivedRevealedLoaded,
                      "(10) A FILE HOLDING THE DERIVED OWN-CHILD-WALLETS MARKER LOADED (revealed %d)",
                      derivedRevealedLoaded ? 1 : 0);
            CBS_CHECK(chuShared && mmChuShared == 1,
                      "(10) the predicates did not read a frozen comboFlags 0x80 (common %d, MM %d)", chuShared ? 1 : 0,
                      mmChuShared);
            CBS_CHECK(!chuLoaded && !Combo_ComboBombchuBagShared(),
                      "(10) A FILE WITH comboFlags 0x80 LOADED: the reserved bit must be refused until stage C8");
            ComboContext_Init();
        }
        RsbsSave_ResetSlotSessionState();
        mgr.DeleteSave(0);
        mgr.SetSaveDirectory(priorSaveDir);
    }

    // ---- (7) From source: the creation writes the two bits from OoT's own options
    // (#1702 C0). Playthrough_Init needs ROMs, so this is the one ROM-free guard on
    // the call; it is searched INSIDE Playthrough_Init, once, before the freeze.
#ifdef RSBS_SOURCE_DIR
    {
        const std::string gen = CbsReadSource("games/oot/soh/Enhancements/randomizer/3drando/playthrough.cpp");
        const size_t fnBegin = gen.find("int Playthrough_Init(");
        const size_t fnEnd =
            (fnBegin == std::string::npos) ? std::string::npos : gen.find("int Playthrough_Repeat(", fnBegin);
        const std::string body = (fnBegin != std::string::npos && fnEnd != std::string::npos)
                                     ? gen.substr(fnBegin, fnEnd - fnBegin)
                                     : std::string();
        const char* const kCall = "Combo_ComboSettingsApplyCreationWalletChuShuffles(";
        const size_t call = body.find(kCall);
        const size_t again = (call == std::string::npos) ? std::string::npos : body.find(kCall, call + 1);
        const size_t callEnd = (call == std::string::npos) ? std::string::npos : body.find(';', call);
        const std::string callText =
            (callEnd != std::string::npos) ? CbsSqueeze(body.substr(call, callEnd - call)) : std::string();
        const std::string wantCall = "Combo_ComboSettingsApplyCreationWalletChuShuffles(&comboSettings,ctx->"
                                     "GetOption(RSK_SHUFFLE_CHILD_WALLET).IsNot(RO_GENERIC_OFF),ctx->GetOption(RSK_"
                                     "BOMBCHU_BAG).IsNot(RO_BOMBCHU_BAG_NONE))";
        const size_t freeze =
            body.find("Combo_FreezeComboSettingsEx2(&comboSettings, &comboItemRules, rsbsItemRules2)");
        printf("[TEST] (7) Playthrough_Init: call at %zu (again %zu), freeze at %zu, text '%s'\n", call, again, freeze,
               callText.c_str());
        CBS_CHECK(!body.empty() && call != std::string::npos && again == std::string::npos &&
                      freeze != std::string::npos && call < freeze && callText == wantCall,
                  "Playthrough_Init does not write the wallet / bombchu bag bits from RSK_SHUFFLE_CHILD_WALLET and "
                  "RSK_BOMBCHU_BAG (not None), once, before the freeze");
        // (9) from source (#1702 C2): Generate asks the hidden-rules question over the
        // very word it freezes, inside Playthrough_Init, and returns before the
        // freeze when it is refused, so a hand-set Shared Child's Wallet key never
        // freezes a world. Catches the gate moved after the freeze, asked of another
        // word, or no longer returning.
        const size_t resolveAt = body.find("const uint32_t rsbsItemRules2 = Combo_ResolveComboItemRules2();");
        const size_t gateAt = (resolveAt == std::string::npos)
                                  ? std::string::npos
                                  : body.find("Combo_ComboItemRules2Refused(rsbsItemRules2,", resolveAt);
        // Only whitespace may lie between the note and its return (a byte bound
        // broke on the indent alone): catches a statement slipped between them.
        static const char kCbsHiddenNote[] = "Combo_GenRefusal_Note(RSBS_GEN_REFUSAL_HIDDEN_RULE);";
        const size_t kCbsHiddenNoteLen = sizeof(kCbsHiddenNote) - 1u;
        const size_t noteAt = (gateAt == std::string::npos) ? std::string::npos : body.find(kCbsHiddenNote, gateAt);
        const size_t returnAt = (noteAt == std::string::npos) ? std::string::npos : body.find("return -1;", noteAt);
        printf("[TEST] (9) Playthrough_Init: resolve at %zu, hidden-rules gate at %zu, its return at %zu, freeze at "
               "%zu\n",
               resolveAt, gateAt, returnAt, freeze);
        const size_t afterNote = (noteAt == std::string::npos)
                                     ? std::string::npos
                                     : body.find_first_not_of(" \t\r\n", noteAt + kCbsHiddenNoteLen);
        CBS_CHECK(resolveAt != std::string::npos && gateAt != std::string::npos && noteAt != std::string::npos &&
                      returnAt != std::string::npos && returnAt == afterNote && freeze != std::string::npos &&
                      returnAt < freeze,
                  "Playthrough_Init does not refuse a hidden second-word rule over the frozen word before the freeze");
    }
#endif

    // ---- (11) #1702 C7: 0x80 ARMS THE SHARED BOMBCHU BAG TIER, AND NOTHING WRITES IT
    // The seed package's refusal helper: every bit outside ALL_V1 (today 0x80) and
    // nothing inside it. Catches the helper reading the wrong mask (0x7F refused,
    // or 0x80 admitted, which would let a crafted package arm the tier).
    CBS_CHECK(Combo_ComboFlagsUnallocated(0x7Fu) == 0u && Combo_ComboFlagsUnallocated(kCbsSharedChu) == kCbsSharedChu &&
                  Combo_ComboFlagsUnallocated(0xFFu) == kCbsSharedChu,
              "(11) Combo_ComboFlagsUnallocated answers %02X / %02X / %02X for 7F / 80 / FF, want 00 / 80 / 80",
              (unsigned)Combo_ComboFlagsUnallocated(0x7Fu), (unsigned)Combo_ComboFlagsUnallocated(kCbsSharedChu),
              (unsigned)Combo_ComboFlagsUnallocated(0xFFu));
#ifdef RSBS_SOURCE_DIR
    {
        const auto count = [](const std::string& text, const char* needle) {
            int n = 0;
            const std::string want(needle);
            for (size_t at = text.find(want); at != std::string::npos; at = text.find(want, at + 1)) {
                n++;
            }
            return n;
        };
        const std::string common = CbsReadSource("src/common/foreign_items.c");
        const std::string gen = CbsReadSource("games/oot/soh/Enhancements/randomizer/3drando/playthrough.cpp");
        const std::string ootExports = CbsReadSource("games/oot/soh/GameExports_SingleExe.cpp");
        const std::string ootSaves = CbsReadSource("games/oot/soh/SaveManager.cpp");
        const std::string pkg = CbsReadSource("games/oot/soh/Enhancements/randomizer/SeedPackageSingleExe.cpp");
        // No production writer names the bit before C8: foreign_items.c has only the
        // predicate's read, and the creation (playthrough.cpp) none. Catches a
        // writer landing before C8's masks move.
        const int commonUses = count(common, "RSBS_COMBO_FLAG_SHARED_BOMBCHU_BAG");
        const int genUses = count(gen, "RSBS_COMBO_FLAG_SHARED_BOMBCHU_BAG");
        // The mode stand-in is defined once in OoT's exports (its only writer is
        // the setter) and no production file calls it. Catches it leaking into play.
        const int setModeDefs = count(ootExports, "OoT_SharedChuBagTest_SetMode(");
        const int setModeWrites = count(ootExports, "sOoTChuBagModeForTest = ");
        const int setModeProd =
            count(gen, "OoT_SharedChuBagTest_SetMode") + count(ootSaves, "OoT_SharedChuBagTest_SetMode");
        // The seed package's Validate refuses an unallocated flag before anything
        // is applied: searched INSIDE Validate, and the next return after the
        // call is the FORMAT refusal. Catches the check removed or moved out.
        const size_t fnBegin = pkg.find("int Validate(const rsbs::seedpkg::Entries& entries");
        const size_t fnEnd =
            (fnBegin == std::string::npos) ? std::string::npos : pkg.find("void ApplyComboWorld(", fnBegin);
        const std::string body = (fnBegin != std::string::npos && fnEnd != std::string::npos)
                                     ? pkg.substr(fnBegin, fnEnd - fnBegin)
                                     : std::string();
        const size_t callAt = body.find("Combo_ComboFlagsUnallocated(rec.comboSettings.comboFlags) != 0u");
        const size_t returnAt = (callAt == std::string::npos) ? std::string::npos : body.find("return ", callAt);
        const std::string kFormatReturn = "return RSBS_SEEDPKG_ERR_FORMAT;";
        const bool refused =
            returnAt != std::string::npos && body.compare(returnAt, kFormatReturn.size(), kFormatReturn) == 0;
        printf(
            "[TEST] (11) 0x80 named in foreign_items.c %d time(s), playthrough.cpp %d; the mode stand-in defined %d, "
            "written %d, called from production %d; the package's check at %zu, refused %d\n",
            commonUses, genUses, setModeDefs, setModeWrites, setModeProd, callAt, refused ? 1 : 0);
        CBS_CHECK(!common.empty() && !gen.empty() && commonUses == 1 && genUses == 0,
                  "(11) comboFlags 0x80 is named by a production writer before C8 (foreign_items.c %d, playthrough.cpp "
                  "%d; want 1, 0)",
                  commonUses, genUses);
        CBS_CHECK(
            !ootExports.empty() && !ootSaves.empty() && setModeDefs == 1 && setModeWrites == 2 && setModeProd == 0,
            "(11) OoT's Bombchu Bag mode stand-in leaked (defined %d, written %d, production calls %d; want 1, 2, "
            "0)",
            setModeDefs, setModeWrites, setModeProd);
        CBS_CHECK(!body.empty() && callAt != std::string::npos && refused,
                  "(11) the seed package's Validate does not refuse an unallocated combo flag as a FORMAT error");
    }
#endif

    CbsClearKeys();
    CbsClearRule2Keys();                   // #1702 C2: legs (9) and (10) set them
    Combo_ComboItemRules2RevealForTest(0); // and reveal the hidden mask
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    printf("[TEST] combo-bag-shuffle-frozen: %s\n", sCbsFailures == 0 ? "PASS" : "FAIL");
    return sCbsFailures == 0 ? TEST_PASS : TEST_FAIL;
}
