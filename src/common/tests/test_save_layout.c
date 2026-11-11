/**
 * @file test_save_layout.c
 * @brief Save format 6's layout rows (src/common/save_layout_pins.h, save.h).
 *
 * Two rows, both ROM-free and display-free (label "redship"):
 *
 *   SaveLayoutPins ("save-layout-pins"): prints every layout pin's compiled
 *     value, one line each, as "[LAYOUT] <name>=<decimal>", and "[LAYOUT] UNPINNED
 *     <name>" for a pin still at 0 (not yet measured). The orchestrator reads
 *     these lines from the three CI legs and fills save_layout_pins.h; from then
 *     on a nonzero pin that differs from the compiled value fails the BUILD
 *     (RSBS_LAYOUT_PIN_ASSERT). The row passes while pins are 0. What it can fail
 *     on, at run time:
 *       - a nonzero pin whose compiled value differs (unreachable while the
 *         static_assert form works; catches RSBS_LAYOUT_PIN_ASSERT turned into a
 *         no-op for a nonzero pin);
 *       - a measured struct past its blob capacity, or the MM tracker capture not
 *         the MM check capacity (catches the capacity static_asserts deleted);
 *       - a watermark term RsbsSave_BuildWatermark wires to the wrong export
 *         position, or a zero term (catches an export reporting nothing). Both
 *         sides read the same exports, so a wrong VALUE in an export is not
 *         caught here: the capacity static_asserts and the fixture row hold those;
 *       - a term RsbsSave_WatermarkExceedsBuild leaves out or compares the wrong
 *         way: each term is raised by one (must read as newer) and lowered by
 *         one (must not);
 *       - production start-up no longer arming the Tier-2/3 tail checks: every
 *         `--test` row runs disarmed, so the row reads rsbs/src/main.cpp and
 *         requires exactly one live `RsbsSave_ArmTierTailChecks();` after the
 *         `--test` exit and before the integration branch and the save directory
 *         (catches the call deleted, commented out or moved below a slot read).
 *
 *   SaveFormatV6Fixture ("save-format-v6-fixture"): loads the COMMITTED format-6
 *     .redsave (tests/fixtures/save-format-6/redship_slot0.redsave) through the
 *     real SaveManager, with the tail checks armed as production arms them, and
 *     asserts every field the fixture wrote BY NAME: ComboContext's, OoT's
 *     (OoT_SaveLayout_FixtureCheck) and MM's (MM_SaveLayout_FixtureCheck). A
 *     field that moved in a later build reads other bytes and fails by name: the
 *     per-step proof that a backport moved no saved field. A later build that
 *     refuses the file (a capacity change, a watermark this build cannot read)
 *     fails too.
 *     The fixture is generated ONCE, at save format 6's gate, by running this
 *     row with RSBS_WRITE_SAVE_FORMAT_V6_FIXTURE=1 in the environment: it then
 *     authors the file through the production writer, copies it into the source
 *     tree, and verifies it as above. The marker file REQUIRED beside it says the
 *     fixture is committed: with the marker and no fixture the row FAILS; with
 *     neither it prints "[FIXTURE] absent" and passes (the state between this PR
 *     and the fixture's commit).
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++).
 */

#include "../combo_tracker_view.h" // Combo_TrackerFreezeAvailability: the fixture's capture
#include "../context.h"
#include "../crossing_store.h"
#include "../game.h"
#include "../save.h"
#include "../save_layout_pins.h"
#include "../shared_items.h"
#include "../test_runner.h"
#include "../test_scratch.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

extern "C" {
// The games' save format 6 exports (games/oot/soh/GameExports_SingleExe.cpp,
// games/mm/2s2h/GameExports_SingleExe.cpp).
int OoT_SaveLayout_ReportPins(void);
int MM_SaveLayout_ReportPins(void);
uint32_t OoT_SaveLayout_ContextSize(void);
size_t MM_SeedPackage_HalfSize(void);
void OoT_SaveLayout_EnumMaxima(uint16_t* outRcMax, uint16_t* outRandInfMax, uint16_t* outRskMax);
void MM_SaveLayout_EnumMaxima(uint16_t* outRcMax, uint16_t* outRoMax, uint16_t* outRtMax, uint16_t* outRandoInfMax,
                              uint16_t* outReMax);
void OoT_SaveLayout_FixtureWrite(uint8_t* blob, size_t size);
int OoT_SaveLayout_FixtureCheck(const uint8_t* blob, size_t size);
void MM_SaveLayout_FixtureWrite(uint8_t* blob, size_t size);
int MM_SaveLayout_FixtureCheck(const uint8_t* blob, size_t size);
}

TestResult Test_SaveLayoutPins(void) {
    printf("[TEST] save-layout-pins: every save format 6 layout pin's compiled value, and the capacities they sit "
           "in\n");
    int failures = 0;

    // Tier-1: ComboContext, compiled in this TU.
    int mismatches = 0;
    RSBS_LAYOUT_PIN_REPORT(RSBS_PIN_COMBO_TRACKER_AVAIL_MM, offsetof(ComboContext, trackerAvailMM), mismatches);
    RSBS_LAYOUT_PIN_REPORT(RSBS_PIN_COMBO_SKIP_JOURNAL_COUNT, offsetof(ComboContext, trackerSkipJournalCountOoT),
                           mismatches);
    RSBS_LAYOUT_PIN_REPORT(RSBS_PIN_COMBO_SKIP_JOURNAL, offsetof(ComboContext, trackerSkipJournalOoT), mismatches);
    RSBS_LAYOUT_PIN_REPORT(RSBS_PIN_COMBO_SAVE_WATERMARK, offsetof(ComboContext, saveWatermark), mismatches);
    RSBS_LAYOUT_PIN_REPORT(RSBS_PIN_COMBO_OOT_WARPS, offsetof(ComboContext, comboOoTWarps), mismatches); // XW7
    RSBS_LAYOUT_PIN_REPORT(RSBS_PIN_COMBO_CONTEXT_SIZEOF, sizeof(ComboContext), mismatches);
    // Tiers 2 and 3: the games' own TUs.
    mismatches += OoT_SaveLayout_ReportPins();
    mismatches += MM_SaveLayout_ReportPins();
    if (mismatches != 0) {
        printf("[TEST] FAIL: %d nonzero layout pin(s) differ from the compiled value\n", mismatches);
        failures++;
    }

    // The capacities the measured structs sit in (printed for the PR's table).
    const uint32_t ootSize = OoT_SaveLayout_ContextSize();
    const size_t mmSize = MM_SeedPackage_HalfSize();
    printf("[LAYOUT] capacity Tier-1 %u, Tier-2 %u, Tier-3 %u; slack Tier-1 %u, Tier-2 %u, Tier-3 %u\n",
           (unsigned)RSBS_COMBO_CONTEXT_RECORD_SIZE, (unsigned)OOT_SAVE_CONTEXT_SIZE, (unsigned)MM_SAVE_CONTEXT_SIZE,
           (unsigned)(RSBS_COMBO_CONTEXT_RECORD_SIZE - sizeof(ComboContext)),
           (unsigned)(OOT_SAVE_CONTEXT_SIZE - ootSize), (unsigned)(MM_SAVE_CONTEXT_SIZE - mmSize));
    if (sizeof(ComboContext) > RSBS_COMBO_CONTEXT_RECORD_SIZE || ootSize == 0 || ootSize > OOT_SAVE_CONTEXT_SIZE ||
        mmSize == 0 || mmSize > MM_SAVE_CONTEXT_SIZE) {
        printf("[TEST] FAIL: a measured struct is past its blob capacity (or reported as empty)\n");
        failures++;
    }
    if (RSBS_TRACKER_AVAIL_OOT_CAP != 8192u || RSBS_TRACKER_AVAIL_MM_CAP != 4096u) {
        printf("[TEST] FAIL: the tracker captures are %u / %u, not save format 6's 8192 / 4096\n",
               (unsigned)RSBS_TRACKER_AVAIL_OOT_CAP, (unsigned)RSBS_TRACKER_AVAIL_MM_CAP);
        failures++;
    }

    // The watermark this build stamps is the games' own enum values.
    ComboSaveWatermark build;
    RsbsSave_BuildWatermark(&build);
    uint16_t ootRc = 0, ootInf = 0, ootRsk = 0, mmRc = 0, mmRo = 0, mmRt = 0, mmInf = 0, mmRe = 0;
    OoT_SaveLayout_EnumMaxima(&ootRc, &ootInf, &ootRsk);
    MM_SaveLayout_EnumMaxima(&mmRc, &mmRo, &mmRt, &mmInf, &mmRe);
    printf("[LAYOUT] watermark layoutRevision=%u ootRcMax=%u ootRandInfMax=%u ootRskMax=%u mmRcMax=%u mmRoMax=%u "
           "mmRtMax=%u mmRandoInfMax=%u mmReMax=%u\n",
           (unsigned)build.layoutRevision, (unsigned)build.ootRcMax, (unsigned)build.ootRandInfMax,
           (unsigned)build.ootRskMax, (unsigned)build.mmRcMax, (unsigned)build.mmRoMax, (unsigned)build.mmRtMax,
           (unsigned)build.mmRandoInfMax, (unsigned)build.mmReMax);
    if (build.layoutRevision != RSBS_SAVE_LAYOUT_REVISION || build.ootRcMax != ootRc || build.ootRandInfMax != ootInf ||
        build.ootRskMax != ootRsk || build.mmRcMax != mmRc || build.mmRoMax != mmRo || build.mmRtMax != mmRt ||
        build.mmRandoInfMax != mmInf || build.mmReMax != mmRe || ootRc == 0 || ootInf == 0 || ootRsk == 0 ||
        mmRc == 0 || mmRo == 0 || mmRt == 0 || mmInf == 0 || mmRe == 0) {
        printf("[TEST] FAIL: the watermark this build stamps is not the games' enum values (or a term is zero)\n");
        failures++;
    }
    // Every term of the newer-content compare, one at a time: one above this
    // build's must read as newer, one below must not (the terms are nonzero,
    // checked above, and far below 0xFFFF, so neither step wraps).
    struct WatermarkTerm {
        const char* name;
        uint32_t* u32;
        uint16_t* u16;
    };
    ComboSaveWatermark probe = build;
    const WatermarkTerm terms[] = {
        { "layoutRevision", &probe.layoutRevision, nullptr },
        { "ootRcMax", nullptr, &probe.ootRcMax },
        { "ootRandInfMax", nullptr, &probe.ootRandInfMax },
        { "ootRskMax", nullptr, &probe.ootRskMax },
        { "mmRcMax", nullptr, &probe.mmRcMax },
        { "mmRoMax", nullptr, &probe.mmRoMax },
        { "mmRtMax", nullptr, &probe.mmRtMax },
        { "mmRandoInfMax", nullptr, &probe.mmRandoInfMax },
        { "mmReMax", nullptr, &probe.mmReMax },
    };
    for (const WatermarkTerm& term : terms) {
        for (int step = -1; step <= 1; step += 2) {
            probe = build;
            if (term.u32 != nullptr) {
                *term.u32 = (uint32_t)((int64_t)*term.u32 + step);
            } else {
                *term.u16 = (uint16_t)((int32_t)*term.u16 + step);
            }
            const int exceeds = RsbsSave_WatermarkExceedsBuild(&probe);
            if (exceeds != (step > 0 ? 1 : 0)) {
                printf("[TEST] FAIL: a watermark with %s %s this build's reads as %s\n", term.name,
                       step > 0 ? "one above" : "one below", exceeds != 0 ? "newer" : "not newer");
                failures++;
            }
        }
    }

    // Production arms the Tier-2/3 tail checks in rsbs/src/main.cpp, which no
    // `--test` row reaches (the rows run disarmed): hold the call's place by
    // source, as mm-creation-new-file holds MM_SlotMeta_Register's.
#ifdef RSBS_SOURCE_DIR
    {
        std::ifstream in(std::string(RSBS_SOURCE_DIR) + "/rsbs/src/main.cpp", std::ios::binary);
        std::string text;
        for (std::istreambuf_iterator<char> it(in), end; it != end; ++it) {
            if (*it != '\r') {
                text.push_back(*it);
            }
        }
        const std::string stmt = "RsbsSave_ArmTierTailChecks();";
        const size_t testExit = text.find("_Exit(testResult);");
        const size_t integration = text.find("TestRunner_ParseIntegrationArgs(argc, argv)");
        const size_t saveDir = text.find("SaveManager::Instance().SetSaveDirectory(");
        size_t live = 0;
        size_t at = std::string::npos;
        for (size_t pos = text.find(stmt); pos != std::string::npos; pos = text.find(stmt, pos + 1)) {
            size_t lineStart = text.rfind('\n', pos);
            lineStart = lineStart == std::string::npos ? 0 : lineStart + 1;
            // Its own line, only indentation before it: not a comment, not a mention.
            if (text.find_first_not_of(" \t", lineStart) == pos && text.find('\n', pos) == pos + stmt.size()) {
                at = pos;
                live++;
            }
        }
        const bool placed = live == 1 && testExit != std::string::npos && integration != std::string::npos &&
                            saveDir != std::string::npos && at > testExit && at < integration && at < saveDir;
        printf("[LAYOUT] main.cpp `%s` live statements=%zu, after the --test exit and before the integration branch "
               "and the save directory=%d\n",
               stmt.c_str(), live, placed ? 1 : 0);
        if (text.empty() || !placed) {
            printf("[TEST] FAIL: rsbs/src/main.cpp does not arm the Tier-2/3 tail checks exactly once between the "
                   "`--test` exit and the integration branch: the game would read a newer build's tier tail without "
                   "refusing it, and StageCommit would stop zeroing the tails it writes\n");
            failures++;
        }
    }
#else
    printf("[TEST] FAIL: RSBS_SOURCE_DIR undefined; main.cpp's tail-check arming cannot be checked\n");
    failures++;
#endif

    printf("[TEST] save-layout-pins: %d failure(s)\n", failures);
    return failures == 0 ? TEST_PASS : TEST_FAIL;
}

namespace {

// The fixture's ComboContext fields, set and read by name (indices are literals).
constexpr int32_t kV6FixtureSaveSlot = 0x0006F1F1;
constexpr uint32_t kV6FixtureSharedFlag5 = 0x600DF00Du;
constexpr uint8_t kV6FixtureWarpSongs = 0x05u;
constexpr uint16_t kV6FixtureJournal0 = 0x8123u;

std::filesystem::path V6FixtureDir() {
#ifdef RSBS_SOURCE_DIR
    return std::filesystem::path(RSBS_SOURCE_DIR) / "tests" / "fixtures" / "save-format-6";
#else
    return std::filesystem::path();
#endif
}

/** Authors the fixture through the production writer (SaveManager::Save) in
 *  @p scratch, slot 0; returns the slot file's path, "" on failure. */
std::string V6FixtureAuthor(rsbs::SaveManager& mgr, const std::filesystem::path& scratch) {
    mgr.SetSaveDirectory(scratch.string());
    mgr.ResetSlotSessionState();
    Context_InitFrozenStates();
    ComboContext_Init();
    Combo_Crossings_Clear();
    Combo_ClearSharedItemOutbox();
    // An UNPAIRED record (sourceIsRando 0, no settings digest, no combo record):
    // the fixture locks layout, not pairing, so no identity check can refuse it.
    gComboCtx.saveSlot = kV6FixtureSaveSlot;
    gComboCtx.sharedFlags[5] = kV6FixtureSharedFlag5;
    gComboCtx.mmWarpSongsOwned = kV6FixtureWarpSongs;
    const uint8_t capture[1] = { 0xA5u };
    Combo_TrackerFreezeAvailability((uint8_t)GAME_OOT, capture, 8);
    gComboCtx.trackerSkipJournalCountOoT = 1;
    gComboCtx.trackerSkipJournalOoT[0] = kV6FixtureJournal0;
    gComboCtx.sourceGame = GAME_OOT;
    std::vector<uint8_t> oot((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
    std::vector<uint8_t> mm((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    OoT_SaveLayout_FixtureWrite(oot.data(), oot.size());
    MM_SaveLayout_FixtureWrite(mm.data(), mm.size());
    Context_UpdateShadowCopy(GAME_OOT, oot.data(), oot.size());
    Context_UpdateShadowCopy(GAME_MM, mm.data(), mm.size());
    mgr.DeleteSave(0);
    if (!mgr.Save(0)) {
        return std::string();
    }
    return mgr.SlotPath(0);
}

} // namespace

TestResult Test_SaveFormatV6Fixture(void) {
    printf("[TEST] save-format-v6-fixture: the committed save format 6 .redsave loads and every field it wrote reads "
           "back by name\n");
    const std::filesystem::path dir = V6FixtureDir();
    if (dir.empty()) {
        printf("[TEST] FAIL: save-format-v6-fixture needs RSBS_SOURCE_DIR to find the fixture\n");
        return TEST_FAIL;
    }
    const std::filesystem::path fixture = dir / "redship_slot0.redsave";
    const std::filesystem::path marker = dir / "REQUIRED";
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    const std::filesystem::path scratch = rsbs::test::ScratchDir("rsbs_save_format_v6_fixture");
    std::error_code ec;

    const char* write = std::getenv("RSBS_WRITE_SAVE_FORMAT_V6_FIXTURE");
    if (write != nullptr && write[0] == '1') {
        const std::string authored = V6FixtureAuthor(mgr, scratch / "author");
        if (authored.empty()) {
            printf("[TEST] FAIL: the fixture could not be authored\n");
            return TEST_FAIL;
        }
        std::filesystem::create_directories(dir, ec);
        std::filesystem::copy_file(authored, fixture, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            printf("[TEST] FAIL: could not write the fixture to %s (%s)\n", fixture.string().c_str(),
                   ec.message().c_str());
            return TEST_FAIL;
        }
        printf("[FIXTURE] wrote %s (%llu bytes); commit it with the marker %s\n", fixture.string().c_str(),
               (unsigned long long)std::filesystem::file_size(fixture, ec), marker.string().c_str());
    }

    const bool required = std::filesystem::exists(marker, ec);
    if (!std::filesystem::exists(fixture, ec)) {
        if (required) {
            printf("[TEST] FAIL: the fixture is marked committed (%s) but %s is absent\n", marker.string().c_str(),
                   fixture.string().c_str());
            return TEST_FAIL;
        }
        printf("[FIXTURE] absent\n");
        printf("[TEST] PASS: no fixture is committed yet (generate it with RSBS_WRITE_SAVE_FORMAT_V6_FIXTURE=1)\n");
        return TEST_PASS;
    }

    // Load the committed bytes as slot 0 of a fresh directory, tail checks armed
    // as production arms them (and disarmed on every way out).
    struct TailArm {
        TailArm() {
            RsbsSave_ArmTierTailChecks();
        }
        ~TailArm() {
            RsbsSave_SetTierLayoutSizesForTest(0u, 0u);
        }
    } arm;
    const std::filesystem::path loadDir = scratch / "load";
    std::filesystem::create_directories(loadDir, ec);
    mgr.SetSaveDirectory(loadDir.string());
    mgr.ResetSlotSessionState();
    std::filesystem::copy_file(fixture, mgr.SlotPath(0), std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        printf("[TEST] FAIL: could not stage the fixture (%s)\n", ec.message().c_str());
        return TEST_FAIL;
    }
    Context_InitFrozenStates();
    ComboContext_Init();
    gComboCtx.saveSlot = 0x7FFFFFFF;
    // Scribble XW7's record so its UNSET legs below read the loaded bytes, not
    // ComboContext_Init's zeros: a loader that skipped them would leave 0x5A.
    memset(&gComboCtx.comboOoTWarps, 0x5A, sizeof(gComboCtx.comboOoTWarps));
    const RsbsLoadOutcome outcome = mgr.LoadSlot(0);
    printf("[FIXTURE] load outcome %d, reason %s\n", (int)outcome,
           rsbs::SaveManager::RefuseReasonLabel(mgr.GetSlotRefuseReason(0)));
    if (outcome != RSBS_LOAD_OK) {
        printf("[TEST] FAIL: this build refuses the committed save format 6 fixture\n");
        return TEST_FAIL;
    }

    int bad = 0;
    RSBS_LAYOUT_FIXTURE_FIELD("combo.saveSlot", gComboCtx.saveSlot, kV6FixtureSaveSlot, bad);
    RSBS_LAYOUT_FIXTURE_FIELD("combo.sharedFlags[5]", gComboCtx.sharedFlags[5], kV6FixtureSharedFlag5, bad);
    RSBS_LAYOUT_FIXTURE_FIELD("combo.mmWarpSongsOwned", gComboCtx.mmWarpSongsOwned, kV6FixtureWarpSongs, bad);
    RSBS_LAYOUT_FIXTURE_FIELD("combo.trackerAvailCountOoT", gComboCtx.trackerAvailCountOoT, 8, bad);
    RSBS_LAYOUT_FIXTURE_FIELD("combo.trackerAvailOoT[0]", gComboCtx.trackerAvailOoT[0], 0xA5u, bad);
    RSBS_LAYOUT_FIXTURE_FIELD("combo.trackerSkipJournalCountOoT", gComboCtx.trackerSkipJournalCountOoT, 1, bad);
    RSBS_LAYOUT_FIXTURE_FIELD("combo.trackerSkipJournalOoT[0]", gComboCtx.trackerSkipJournalOoT[0], kV6FixtureJournal0,
                              bad);
    RSBS_LAYOUT_FIXTURE_FIELD("combo.commitGeneration", gComboCtx.commitGeneration, 1u, bad);
    // The watermark the fixture's commit stamped: save format 6's values as P0
    // landed (layout revision 1; OoT 2528 checks, 2064 flags, 234 settings; MM
    // 2311 checks, 54 options, 86 tricks, 78 flags, 83 events). Literals, so a
    // later build's own (higher) values cannot satisfy them.
    RSBS_LAYOUT_FIXTURE_FIELD("combo.saveWatermark.layoutRevision", gComboCtx.saveWatermark.layoutRevision, 1u, bad);
    RSBS_LAYOUT_FIXTURE_FIELD("combo.saveWatermark.ootRcMax", gComboCtx.saveWatermark.ootRcMax, 2528u, bad);
    RSBS_LAYOUT_FIXTURE_FIELD("combo.saveWatermark.ootRandInfMax", gComboCtx.saveWatermark.ootRandInfMax, 2064u, bad);
    RSBS_LAYOUT_FIXTURE_FIELD("combo.saveWatermark.ootRskMax", gComboCtx.saveWatermark.ootRskMax, 234u, bad);
    RSBS_LAYOUT_FIXTURE_FIELD("combo.saveWatermark.mmRcMax", gComboCtx.saveWatermark.mmRcMax, 2311u, bad);
    RSBS_LAYOUT_FIXTURE_FIELD("combo.saveWatermark.mmRoMax", gComboCtx.saveWatermark.mmRoMax, 54u, bad);
    RSBS_LAYOUT_FIXTURE_FIELD("combo.saveWatermark.mmRtMax", gComboCtx.saveWatermark.mmRtMax, 86u, bad);
    RSBS_LAYOUT_FIXTURE_FIELD("combo.saveWatermark.mmRandoInfMax", gComboCtx.saveWatermark.mmRandoInfMax, 78u, bad);
    RSBS_LAYOUT_FIXTURE_FIELD("combo.saveWatermark.mmReMax", gComboCtx.saveWatermark.mmReMax, 83u, bad);
    // #1169 XW7's record, appended after the watermark at layout revision 2: the
    // revision-1 fixture holds zeros where the record now sits (its comboSize is
    // 4096, so these are the file's own bytes, read over the scribble above), so
    // it reads UNSET (growth rule 1's "zero means unset") and every field above
    // stays where it was. The short-record zero-extension itself is proved by
    // SaveComboLegacyRecord's short version-6 leg, not here.
    RSBS_LAYOUT_FIXTURE_FIELD("combo.comboOoTWarps.formatVersion", gComboCtx.comboOoTWarps.formatVersion, 0u, bad);
    RSBS_LAYOUT_FIXTURE_FIELD("combo.comboOoTWarps.flags", gComboCtx.comboOoTWarps.flags, 0u, bad);
    bad += OoT_SaveLayout_FixtureCheck(static_cast<const uint8_t*>(Context_GetOoTSaveContext()),
                                       Context_GetOoTSaveContext() != nullptr ? (size_t)OOT_SAVE_CONTEXT_SIZE : 0u);
    bad += MM_SaveLayout_FixtureCheck(static_cast<const uint8_t*>(Context_GetMMSaveContext()),
                                      Context_GetMMSaveContext() != nullptr ? (size_t)MM_SAVE_CONTEXT_SIZE : 0u);

    mgr.DeleteSave(0);
    mgr.ResetSlotSessionState();
    ComboContext_Init();
    if (bad != 0) {
        printf("[TEST] FAIL: %d field(s) of the save format 6 fixture moved\n", bad);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: the save format 6 fixture loads and every field it wrote reads back by name\n");
    return TEST_PASS;
}
