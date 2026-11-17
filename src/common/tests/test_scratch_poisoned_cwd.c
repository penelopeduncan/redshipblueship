/**
 * Save-writing rows in a poisoned working directory (#824, #925).
 *
 * Two CTest rows of one build directory run at the same time (`ctest -j`, and
 * `AllTests` beside the standalone rows it contains) and share that directory
 * as their working directory. #926 moved six rows' .redsave files to a scratch
 * directory per process, but every other save-writing row still spelled its
 * directory as a fixed name relative to the working directory
 * (`rsbs_test_saves_foreign`, `rsbs_test_saves`, ...), and SoH's own
 * `Save/file<N>.sav` is relative to it too. Two such rows overwrite each other's
 * files, and one of them reads the other's: a verdict that is wrong, or no
 * verdict at all when the loser dies mid-write. Neither failure reproduces on
 * demand, which is why #824 (ForeignItemGiveReverse ended with no verdict) and
 * #925 (three slot-0 subtests of AllTests failed together) stayed open.
 *
 * This row makes the working directory hostile instead of waiting for a second
 * process. It plants, in a fresh directory, a plain FILE under every fixed name a
 * row has used there (a row that still writes `<name>/redship_slot0.redsave`
 * cannot create its directory) and DIRECTORIES under `Save/file<N>.sav` and
 * `Save/file<N>.temp` (SoH's .sav of a slot cannot be written or read), then runs
 * three groups of rows in it, each group back to back in ONE process:
 *   - the three slot-0 subtests #925 names, in the order AllTests runs them;
 *   - the rows of #1641 that kept a scratch FOLDER under a fixed name in the
 *     working directory and emptied it themselves (curated-archive-inapp, which
 *     AllTests runs too, and its siblings);
 *   - the foreign-item rows #824 names and the .redsave rows that shared their
 *     working directory.
 * Every group must exit 0. A row that is self-contained never looks at the
 * poison; a row that still shares the working directory fails here, whatever
 * the timing of any other process.
 *
 * A plain file does not catch the #1641 rows: each one removed its folder first,
 * poison and all, and wrote a fresh one. So every group's directory also holds a
 * SENTINEL folder under each of those names, holding one file. After the group
 * the folder must hold exactly that file, unchanged: a row that removed it, or
 * wrote anything beside it, still uses the shared name.
 *
 * The #1641 rows that need staged archives or the rando tier cannot run here, so
 * their move is held by source: each file must still call ScratchDir for its old
 * fixed name (kSpcAnchors).
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++). No display, no
 * ROM archives: every child row is display-free and ROM-free itself.
 */

#include "test_scratch.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

// Every fixed name a save-writing row has taken relative to its working
// directory. Planted as plain files.
const char* const kSpcFixedNames[] = {
    "rsbs_test_saves_crossing_views",
    "rsbs_test_saves_item_view_new_file",
    "rsbs_test_combo_settings",
    "rsbs_test_combo_settings_load",
    "rsbs_test_saves_spoiler_view",
    "rsbs_test_commit_gen",
    "rsbs_test_crossing_commit",
    "rsbs_test_saves_crossings",
    "rsbs_test_saves_crossings_roundtrip",
    "rsbs_test_saves_crossings_capacity",
    "rsbs_test_saves_crossings_redsave",
    "rsbs_test_saves_crossings_spoiler",
    "rsbs_test_crossing_views_world",
    "rsbs_test_saves_foreign",
    "rsbs_test_saves_grants",
    "rsbs_test_refusal_saves",
    "rsbs_test_saves",
    "rsbs_test_session",
    "rsbs_test_triforce_hunt_load",
    "rsbs_test_whole_file_commit",
    "rsbs_test_saves_combo_settings",
    "rsbs_test_saves_creation_new_file",
    "rsbs_test_saves_paired_load_restore",
    "rsbs_test_saves_spoiler_identity",
    "rsbs_test_oot_check_state_load",
};

// #1641: the fixed folder names the rows of the second group used to empty and
// rewrite in the working directory. Planted as sentinel folders, which must come
// through every group untouched.
const char* const kSpcSentinelNames[] = {
    "rsbs_test_806",         "ui-snapshot-image-test", "rsbs_test_loose_705_discovery",
    "rsbs_test_mod_set_706", "rsbs604_array_dispatch",
};
// #1641: the rows that moved to ScratchDir with the ones above but need staged
// archives or the rando tier, so no ROM-free group can run them here. Held by
// source instead: each file must still call the helper for its old fixed name.
struct SpcAnchor {
    const char* file; // relative to the source tree
    const char* call;
};
const SpcAnchor kSpcAnchors[] = {
    { "src/common/tests/test_curated_archive_generator.c", "rsbs::test::ScratchDir(\"rsbs_test_605\")" },
    { "src/common/tests/test_curated_archive_order.c", "rsbs::test::ScratchDir(\"rsbs_test_mod_593_dir\")" },
    { "src/common/tests/test_curated_archive_order.c", "rsbs::test::ScratchDir(\"rsbs_test_mod_1308_alt\")" },
    { "src/common/tests/test_curated_archive_mount.c", "stage.dir = rsbs::test::ScratchDir(dirName);" },
    { "src/common/tests/test_loose_mods_mount.c", "rsbs::test::ScratchDir(\"rsbs_test_loose_705\")" },
    { "src/common/tests/test_mm_mods_mount.c", "rsbs::test::ScratchDir(\"rsbs_test_mods_670\")" },
    { "src/common/tests/test_mm_mods_mount.c", "rsbs::test::ScratchDir(\"rsbs_test_mods_706\")" },
    { "games/mm/2s2h/mm_rando_gen_test.cpp", "rsbs::test::ScratchDirCStr(\"rsbs_test_playerpath_saves\")" },
    { "games/mm/2s2h/mm_rando_gen_test.cpp", "rsbs::test::ScratchDirCStr(\"rsbs_test_pairswitch_saves\")" },
    { "games/mm/2s2h/mm_rando_gen_test.cpp", "rsbs::test::ScratchDirCStr(\"rsbs_test_pairedexhaustion_saves\")" },
    { "games/mm/2s2h/mm_rando_gen_test.cpp", "rsbs::test::ScratchDirCStr(\"rsbs_test_roundbudget_saves\")" },
    { "games/mm/2s2h/mm_rando_gen_test.cpp", "rsbs::test::ScratchDirCStr(\"rsbs_test_goldenrounds_saves\")" },
    { "games/mm/2s2h/mm_rando_gen_test.cpp", "rsbs::test::ScratchDirCStr(\"rsbs_test_critrenew_saves\")" },
    { "games/mm/2s2h/mm_rando_gen_test.cpp", "rsbs::test::ScratchDirCStr(\"rsbs_test_packagepath_saves\")" },
};
const char kSpcSentinelFile[] = "sentinel.txt";
const char kSpcSentinelText[] = "sentinel: a row that removes or writes beside this shares the working directory";

struct SpcGroup {
    const char* why;
    const char* rows; // comma-separated, run back to back in one process
};

const SpcGroup kSpcGroups[] = {
    { "the three slot-0 subtests of #925, in AllTests' order",
      "oot-exit-harvest-gate,mm-death-decline-autosave,oot-file-select-refusal" },
    { "the rows of #1641 that kept a fixed scratch folder in the working directory",
      "curated-archive-inapp,ui-snapshot-image,loose-mods-discovery,mm-mod-set,array-reader-dispatch" },
    { "the foreign-item rows of #824 and the rows that shared their working directory",
      "foreign-item-give,foreign-item-give-reverse,foreign-host-eligibility,foreign-host-gossip-hint,"
      "save-roundtrip-tiers,save-has-delete,save-refused-quarantine,save-write-latch,commit-generation-monotonic,"
      "commit-torn-write,whole-file-commit,whole-file-redeemed-item,session-invalidation,grant-idempotency,"
      "grant-persistence,crossing-commit,crossing-store-roundtrip,crossing-store-capacity,crossing-store-redsave,"
      "crossing-store-spoiler,combo-settings-format,combo-settings-legacy-freeze,"
      "combo-spoiler-view,combo-triforce-hunt" },
};

// Plants the poison in @p cwd; returns how many names it took.
int SpcPlantPoison(const std::filesystem::path& cwd) {
    std::error_code ec;
    int planted = 0;
    for (const char* name : kSpcFixedNames) {
        std::ofstream out(cwd / name, std::ios::binary | std::ios::trunc);
        out << "poison: a row that writes here is not self-contained";
        if (out) {
            planted++;
        }
    }
    const std::filesystem::path save = cwd / "Save";
    std::filesystem::create_directories(save, ec);
    for (int slot = 1; slot <= 3; slot++) {
        for (const char* ext : { ".sav", ".temp" }) {
            const std::filesystem::path trap = save / ("file" + std::to_string(slot) + ext);
            std::filesystem::create_directories(trap, ec);
            std::ofstream inside(trap / "poison.txt");
            inside << "poison";
            if (!ec && std::filesystem::is_directory(trap)) {
                planted++;
            }
        }
        // A dirty Save/ folder: a slot file of the .redsave format's name that no
        // row wrote and no reader can parse.
        std::ofstream dirty(save / ("redship_slot" + std::to_string(slot - 1) + ".redsave"), std::ios::binary);
        dirty << "poison: not a .redsave";
        if (dirty) {
            planted++;
        }
    }
    return planted;
}

// Plants one sentinel folder per kSpcSentinelNames entry in @p cwd; returns how
// many it planted.
int SpcPlantSentinels(const std::filesystem::path& cwd) {
    int planted = 0;
    for (const char* name : kSpcSentinelNames) {
        std::error_code ec;
        std::filesystem::create_directories(cwd / name, ec);
        std::ofstream out(cwd / name / kSpcSentinelFile, std::ios::binary | std::ios::trunc);
        out << kSpcSentinelText;
        out.close();
        if (!ec && out) {
            planted++;
        }
    }
    return planted;
}

// Counts the sentinel folders in @p cwd a row disturbed (removed, rewritten, or
// given another entry), printing each with what was found.
int SpcDisturbedSentinels(const std::filesystem::path& cwd, int group) {
    int disturbed = 0;
    for (const char* name : kSpcSentinelNames) {
        const std::filesystem::path dir = cwd / name;
        std::error_code ec;
        int entries = 0;
        bool onlySentinel = std::filesystem::is_directory(dir, ec);
        if (onlySentinel) {
            for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
                entries++;
                if (entry.path().filename().string() != kSpcSentinelFile) {
                    onlySentinel = false;
                }
            }
        }
        std::string text;
        {
            std::ifstream in(dir / kSpcSentinelFile, std::ios::binary);
            text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        }
        const bool intact = text == kSpcSentinelText;
        if (!onlySentinel || entries != 1 || !intact) {
            printf("[TEST] FAIL: group %d: the sentinel folder %s was disturbed (%d entries, sentinel %s): a row "
                   "still keeps a scratch folder under that fixed name in the working directory (#1641)\n",
                   group, name, entries, intact ? "intact" : "gone or rewritten");
            disturbed++;
        }
    }
    return disturbed;
}

// Counts the kSpcAnchors entries whose file no longer calls the helper (or cannot
// be read), printing each.
int SpcMissingAnchors(void) {
#ifdef RSBS_SOURCE_DIR
    int missing = 0;
    for (const SpcAnchor& a : kSpcAnchors) {
        std::ifstream in(std::string(RSBS_SOURCE_DIR) + "/" + a.file, std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (text.empty() || text.find(a.call) == std::string::npos) {
            printf("[TEST] FAIL: %s %s %s: that row is back on a fixed name every concurrent process shares "
                   "(#1641)\n",
                   a.file, text.empty() ? "could not be read for" : "no longer calls", a.call);
            missing++;
        }
    }
    return missing;
#else
    printf("[TEST] note: RSBS_SOURCE_DIR is not defined; the source anchors of #1641 were not checked\n");
    return 0;
#endif
}

// Prints the lines of a failed child's log that say why it failed.
void SpcPrintFailureLines(const std::filesystem::path& log) {
    std::ifstream in(log);
    std::string line;
    int printed = 0;
    while (std::getline(in, line) && printed < 16) {
        if (line.find("FAIL") != std::string::npos || line.find("NOT saved") != std::string::npos ||
            line.find("ERROR") != std::string::npos || line.find("CRASH") != std::string::npos) {
            printf("[TEST]     | %s\n", line.c_str());
            printed++;
        }
    }
}

// #1641: the leaf ScratchDir hands back is the caller's own name. A loose folder
// that ArchiveManager::AddArchive mounts as a FolderArchive must have no
// extension (mod-survives-switch's rsbs_test_mod_1308_alt), so the process id
// may sit only on the parent; `<name>.<pid>` made it the extension and the mount
// failed. Returns the number of failures.
int SpcScratchShape() {
    const std::string name = "rsbs_test_scratch_shape_1641";
    const std::filesystem::path dir = rsbs::test::ScratchDir(name);
    int failures = 0;
    if (dir.filename().string() != name || !dir.extension().empty()) {
        printf("[TEST] FAIL: ScratchDir(\"%s\") is %s: its leaf must be the name itself, with no extension\n",
               name.c_str(), dir.generic_string().c_str());
        failures++;
    }
    if (dir.parent_path() != rsbs::test::ScratchProcessRoot()) {
        printf("[TEST] FAIL: ScratchDir(\"%s\") is %s, not under this process's own folder %s\n", name.c_str(),
               dir.generic_string().c_str(), rsbs::test::ScratchProcessRoot().generic_string().c_str());
        failures++;
    }
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) {
        printf("[TEST] FAIL: ScratchDir(\"%s\") did not create %s\n", name.c_str(), dir.generic_string().c_str());
        failures++;
    }
    return failures;
}

// #1641: a row that passed but could not delete its folder (on Windows, an
// archive still mounted from it) leaves `<temp>/rsbs_scratch.<pid>` behind when
// its process exits; the next test process's ScratchSweepStale removes it. A root
// a failed row marked as kept stays, and so does a live process's. 999999996 is
// a process id no supported platform hands out (Linux caps at 2^22, macOS at
// 99999, Windows ids stay far below), so its owner is certainly gone. Returns
// the number of failures.
int SpcStaleSweep() {
    const std::filesystem::path base = rsbs::test::ScratchProcessRoot().parent_path();
    const std::filesystem::path stale = base / "rsbs_scratch.999999996";
    const std::filesystem::path kept = base / "rsbs_scratch.999999992";
    std::error_code ec;
    std::filesystem::remove_all(stale, ec);
    std::filesystem::remove_all(kept, ec);
    std::filesystem::create_directories(stale / "leftover", ec);
    std::filesystem::create_directories(kept / "kept_row", ec);
    {
        std::ofstream(stale / "leftover" / "file.txt", std::ios::binary) << "left by a row that passed";
        std::ofstream(kept / "rsbs_scratch_kept", std::ios::binary) << "a-failed-row";
    }
    int failures = 0;
    rsbs::test::ScratchSweepStale();
    if (std::filesystem::exists(stale, ec)) {
        printf("[TEST] FAIL: ScratchSweepStale left %s, the root of a process that is gone, in the temp directory\n",
               stale.generic_string().c_str());
        failures++;
    }
    if (!std::filesystem::exists(kept / "kept_row", ec)) {
        printf("[TEST] FAIL: ScratchSweepStale removed %s, which a failed row kept for forensics\n",
               kept.generic_string().c_str());
        failures++;
    }
    if (!std::filesystem::is_directory(rsbs::test::ScratchProcessRoot(), ec)) {
        printf("[TEST] FAIL: ScratchSweepStale removed this live process's own root %s\n",
               rsbs::test::ScratchProcessRoot().generic_string().c_str());
        failures++;
    }
    std::filesystem::remove_all(stale, ec);
    std::filesystem::remove_all(kept, ec);
    return failures;
}

} // namespace

static TestResult Test_TestScratchPoisonedCwd(void) {
    const std::filesystem::path root = rsbs::test::ScratchDir("rsbs_test_scratch_poisoned_cwd_test");
    int failures = 0;
    int group = 0;
    for (const SpcGroup& g : kSpcGroups) {
        const std::filesystem::path cwd = root / ("cwd" + std::to_string(group));
        const std::filesystem::path log = root / ("group" + std::to_string(group) + ".log");
        std::error_code ec;
        std::filesystem::create_directories(cwd, ec);
        const int planted = SpcPlantPoison(cwd);
        const int wanted = (int)(sizeof(kSpcFixedNames) / sizeof(kSpcFixedNames[0])) + 3 * 3;
        if (planted != wanted) {
            printf("[TEST] FAIL: group %d: planted %d of %d poisoned names (this row would pass vacuously)\n", group,
                   planted, wanted);
            failures++;
            group++;
            continue;
        }
        const int sentinels = SpcPlantSentinels(cwd);
        const int sentinelsWanted = (int)(sizeof(kSpcSentinelNames) / sizeof(kSpcSentinelNames[0]));
        if (sentinels != sentinelsWanted) {
            printf("[TEST] FAIL: group %d: planted %d of %d sentinel folders (this row would pass vacuously)\n", group,
                   sentinels, sentinelsWanted);
            failures++;
            group++;
            continue;
        }
        const int code = rsbs::test::RunRowsInDir(g.rows, cwd, log);
        const bool ok = code == 0;
        printf("[TEST] %s: %s, in a poisoned working directory, one process, exited %d\n", ok ? "ok" : "FAIL", g.why,
               code);
        if (!ok) {
            failures++;
            SpcPrintFailureLines(log);
        }
        failures += SpcDisturbedSentinels(cwd, group);
        group++;
    }
    failures += SpcMissingAnchors();
    failures += SpcScratchShape();
    failures += SpcStaleSweep();
    printf("[TEST] %s: test-scratch-poisoned-cwd, %d failure(s)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? TEST_PASS : TEST_FAIL;
}
