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
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * like its neighbours; every symbol under test is C-linkage. Helpers carry the
 * Cbs prefix because every included test file shares one translation unit.
 */

#include "../context.h"
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
    }
#endif

    CbsClearKeys();
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    printf("[TEST] combo-bag-shuffle-frozen: %s\n", sCbsFailures == 0 ? "PASS" : "FAIL");
    return sCbsFailures == 0 ? TEST_PASS : TEST_FAIL;
}
