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
 * Nothing in production reads the predicates yet; the next stages of #1652 do.
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
    }

    // ---- (4) The compare reads the bits as the file decided them ----------------
    for (const auto& w : kCbsWorlds) {
        CbsFreezeWorld(w.stick, w.nut);
        const uint32_t bits = Combo_ComboSettingsDivergence();
        CBS_CHECK(bits == 0u, "%s: a world met by its own session diverged %08X; the bag bits are the file's", w.what,
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
        RsbsSave_ResetSlotSessionState();
        mgr.DeleteSave(0);
        mgr.SetSaveDirectory(priorSaveDir);
    }

    CbsClearKeys();
    ComboContext_Init();
    Combo_ResetSharedResourceWatermarks();
    printf("[TEST] combo-bag-shuffle-frozen: %s\n", sCbsFailures == 0 ? "PASS" : "FAIL");
    return sCbsFailures == 0 ? TEST_PASS : TEST_FAIL;
}
